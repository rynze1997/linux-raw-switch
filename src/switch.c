#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <poll.h>
#include <arpa/inet.h>

#include "net/socket.h"
#include "switch/mac_table.h"

/*------------------------------------------------------------------------------
 * Definitions
 *----------------------------------------------------------------------------*/
#define MAX_PORTS 4
#define ETH_PAYLOAD_MAX 1500
#define ETH_TYPE_IPV6 0x86dd
/*------------------------------------------------------------------------------
 * Types
 *----------------------------------------------------------------------------*/
 typedef struct ethernet_header_st {
    unsigned char dst_mac[MAC_ADDR_LEN];
    unsigned char src_mac[MAC_ADDR_LEN];
    /* Indicates the protocol of the encapsulated data
     *(e.g., 0x0800 for IPv4, 0x0806 for ARP) or the length of the payload. */
    uint16_t ether_type;

 } ethernet_header_t;

typedef struct switch_st {
    int socket_fds[MAX_PORTS];
    char *interfaces[MAX_PORTS];
    unsigned char frame_buffer[ETH_PAYLOAD_MAX + sizeof(ethernet_header_t)];
} switch_t;

/*------------------------------------------------------------------------------
 * Static Variables
 *----------------------------------------------------------------------------*/
static switch_t switch_inst = {
    .socket_fds = {0},
    .interfaces = {"veth1", "veth2", "veth3", "veth4"},
};

/*------------------------------------------------------------------------------
 * Main Function
 *----------------------------------------------------------------------------*/
static void print_mac(unsigned char *mac) {
    printf("%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int main() {
    struct pollfd fds[MAX_PORTS];

    printf("Starting Simple Switch on 4 ports...\n");

    // Initialize sockets for all ports
    for (int port = 0; port < MAX_PORTS; port++) {
        switch_inst.socket_fds[port] = create_socket(switch_inst.interfaces[port]);
        fds[port].fd = switch_inst.socket_fds[port];
        fds[port].events = POLLIN; // We are interested in Reading (Input)
    }

    /*
     * The poll() function below converts "simultaneous" events into a sequential
     * "To-Do List." If two packets arrive at the exact same nanosecond:
     * 1. poll() wakes up indicating both ports have data.
     * 2. We process Port 1's packet first.
     * 3. We process Port 2's packet immediately after.
     */

    while (1) {
        // poll() blocks until data arrives on ANY of the ports
        int ret = poll(fds, MAX_PORTS, -1);
        if (ret < 0) {
            perror("Poll failed");
            break;
        }

        // Check which port has data
        for (int incoming_port_index = 0; incoming_port_index < MAX_PORTS; incoming_port_index++) {
            if (fds[incoming_port_index].revents & POLLIN) {

                // Data is ready on port [incoming_port_index]
                int len = recvfrom(switch_inst.socket_fds[incoming_port_index],
                                    &switch_inst.frame_buffer, sizeof(switch_inst.frame_buffer), 0, NULL, NULL);
                ethernet_header_t *header = (ethernet_header_t *)switch_inst.frame_buffer;

                if (len < 0) {
                    perror("Receive failed");
                    continue;
                }

                // Ignore ipv6
                if (ntohs(header->ether_type) == ETH_TYPE_IPV6) {
                    continue;
                }

                printf("PORT %d:\n", incoming_port_index + 1);
                printf("Source MAC: ");
                print_mac(header->src_mac);

                printf(" Destination MAC: ");
                print_mac(header->dst_mac);
                printf("\n");

                printf("Ether Type: 0x%04x", ntohs(header->ether_type));

                mac_table_update(header->src_mac, incoming_port_index);

                int8_t outgoing_port_index = mac_table_lookup_port(header->dst_mac);
                if (outgoing_port_index == -1) {
                    printf("Flooding...\n");
                    for (int port = 0; port < MAX_PORTS; port++) {
                        // Don't send the packet back out the port it came in!
                        if (port != incoming_port_index) {
                            if (write(switch_inst.socket_fds[port], switch_inst.frame_buffer, len) < 0) {
                                perror("Send failed");
                            } else {
                                printf("[Port %d] Sent %d bytes to port %d\n", incoming_port_index + 1, len, port + 1);
                            }
                        }
                    }
                } else {
                    printf("Sending to Port %d\n", outgoing_port_index + 1);
                    if (write(switch_inst.socket_fds[outgoing_port_index], switch_inst.frame_buffer, len) < 0) {
                        perror("Send failed");
                    } else {
                        printf("[Port %d] Sent %d bytes to port %d\n", incoming_port_index + 1, len, outgoing_port_index + 1);
                    }
                }
                printf("--------------------------------\n");

            }
        }
    }

    // Cleanup
    for (int port_index = 0; port_index < MAX_PORTS; port_index++) {
        close(switch_inst.socket_fds[port_index]);
    }
    return 0;
}
