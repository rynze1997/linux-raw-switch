#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <poll.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
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

typedef struct switch_port_info_st {
    int socket_fd;          // The actual file descriptor (or -1 if down)
    char if_name[IFNAMSIZ]; // Name of the interface (e.g., "veth1")
    int is_active;          // 1 = UP, 0 = DOWN

    // Request flags for the Switch Engine
    int request_connect;         // 1 = CLI wants to connect this port
    char pending_name[IFNAMSIZ]; // The name CLI wants to connect to
} switch_port_info_t;

typedef struct switch_st {
    unsigned char frame_buffer[ETH_PAYLOAD_MAX + sizeof(ethernet_header_t)];
    switch_port_info_t port[MAX_PORTS]; // Shared state between CLI and Switch Engine
    bool shutdown;
} switch_t;
/*------------------------------------------------------------------------------
 * Static Variables
 *----------------------------------------------------------------------------*/
static switch_t switch_inst;

// Mutex: The "Key" to the shared memory.
// Only one thread can hold this at a time.
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/*------------------------------------------------------------------------------
 * Main Function
 *----------------------------------------------------------------------------*/
static void print_mac(unsigned char *mac) {
    printf("%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void flood_packet(uint8_t incoming_port_index, unsigned char *frame_buffer, size_t len) {
    printf("Flooding... ");
    for (int port = 0; port < MAX_PORTS; port++) {
        if (port != incoming_port_index && switch_inst.port[port].is_active) {
            if (write(switch_inst.port[port].socket_fd, frame_buffer, len) < 0) {
                perror("Send on port failed");
            } else {
                printf("[Port %d] Sent %zu bytes to port %d\n", incoming_port_index + 1, len, port + 1);
            }
        }
    }
}

static void *switch_thread() {
    struct pollfd fds[MAX_PORTS];

    /*
     * The poll() function below converts "simultaneous" events into a sequential
     * "To-Do List." If two packets arrive at the exact same nanosecond:
     * 1. poll() wakes up indicating both ports have data.
     * 2. We process Port 1's packet first.
     * 3. We process Port 2's packet immediately after.
     */
     while (!switch_inst.shutdown) {

        pthread_mutex_lock(&lock); // Grab the key!

        for (int i = 0; i < MAX_PORTS; i++) {
            // Check if CLI asked to connect a port
            if (switch_inst.port[i].request_connect) {
                // Close old socket if it was open
                if (switch_inst.port[i].socket_fd != -1) {
                    socket_close(switch_inst.port[i].socket_fd);
                    switch_inst.port[i].socket_fd = -1;
                    switch_inst.port[i].is_active = 0;
                    mac_table_flush_port(i);
                }

                // Open new socket
                printf("[Switch Engine] Connecting Port %d to %s...\n", i+1, switch_inst.port[i].pending_name);
                int new_sock = create_socket(switch_inst.port[i].pending_name);

                if (new_sock >= 0) {
                    switch_inst.port[i].socket_fd = new_sock;
                    strncpy(switch_inst.port[i].if_name, switch_inst.port[i].pending_name, IFNAMSIZ);
                    switch_inst.port[i].is_active = 1;
                    printf("[Data Plane] Port %d is UP.\n", i+1);
                }

                switch_inst.port[i].request_connect = 0; // Request handled
            }

            // Update poll struct
            fds[i].fd = switch_inst.port[i].socket_fd;
            fds[i].events = POLLIN;
        }

        pthread_mutex_unlock(&lock); // Release the key!

        /* poll() blocks until data arrives on ANY of the ports
         * Timeout = 1000ms. If no packets arrive, wake up anyway to check for CLI commands.
         */
        int ret = poll(fds, MAX_PORTS, 1000);

        if (ret > 0) {
            // Check which port has data
            for (int incoming_port_index = 0; incoming_port_index < MAX_PORTS; incoming_port_index++) {
                if (fds[incoming_port_index].revents & POLLIN) {

                    // Data is ready on port [incoming_port_index]
                    int len = recvfrom(switch_inst.port[incoming_port_index].socket_fd,
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

                    printf("Ether Type: 0x%04x\n", ntohs(header->ether_type));

                    mac_table_update(header->src_mac, incoming_port_index);

                    int8_t outgoing_port_index = mac_table_lookup_port(header->dst_mac);
                    if (outgoing_port_index == -1) {
                        flood_packet(incoming_port_index, switch_inst.frame_buffer, len);
                    } else {
                        if (!switch_inst.port[outgoing_port_index].is_active) {
                            flood_packet(incoming_port_index, switch_inst.frame_buffer, len);
                            continue; // Skip sending to this port
                        }

                        printf("Sending to Port %d\n", outgoing_port_index + 1);
                        if (write(switch_inst.port[outgoing_port_index].socket_fd, switch_inst.frame_buffer, len) < 0) {
                            perror("Send failed");
                        } else {
                            printf("[Port %d] Sent %d bytes to port %d\n", incoming_port_index + 1, len, outgoing_port_index + 1);
                        }
                    }
                    printf("--------------------------------\n");

                }
            }
        }
    }

    // Clean up
    for (int port = 0; port < MAX_PORTS; port++) {
        if (switch_inst.port[port].socket_fd != -1)
            socket_close(switch_inst.port[port].socket_fd);
    }

    return NULL;
}

int main() {

    printf("Starting Simple Switch on 4 ports...\n");

    // Initialize switch instance
    memset(&switch_inst, 0, sizeof(switch_inst));
    for (int i = 0; i < MAX_PORTS; i++) {
        switch_inst.port[i].socket_fd = -1;
    }

    pthread_t switch_thread_id;
    char cmd_buffer[200];
    char arg1[100], arg2[IFNAMSIZ];

    // Create switch thread and start it in the background
    pthread_create(&switch_thread_id, NULL, switch_thread, NULL);

    // Main thread: CLI
    while (1) {
        printf("Switch> ");
        if (fgets(cmd_buffer, sizeof(cmd_buffer), stdin) == NULL) {
            break;
        }

        // Remove trailing newline
        cmd_buffer[strcspn(cmd_buffer, "\n")] = '\0';
        // If only pressed enter, continue
        if (strcmp(cmd_buffer, "") == 0) {
            continue;
        }

        if (strcmp(cmd_buffer, "exit") == 0) {
            switch_inst.shutdown = true;
            break;
        }

        if (sscanf(cmd_buffer, "connect %s %s", arg1, arg2) == 2) {
            int port_idx = atoi(arg1) - 1;

            if (port_idx >= 0 && port_idx < MAX_PORTS) {
                // LOCK critical section
                pthread_mutex_lock(&lock);

                strncpy(switch_inst.port[port_idx].pending_name, arg2, IFNAMSIZ);
                switch_inst.port[port_idx].request_connect = 1; // Signal the engine

                pthread_mutex_unlock(&lock);
                printf("Command sent: Connect Port %d to %s\n", port_idx + 1, arg2);
            } else {
                printf("Error: Invalid port number. Use 1-4.\n");
            }
        }
    }

    printf("Exiting...\n");

    pthread_join(switch_thread_id, NULL);

    return 0;
}
