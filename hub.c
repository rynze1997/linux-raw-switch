#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <net/ethernet.h> // Contains "struct ether_header"
#include <poll.h>

#define MAX_PORTS 4
#define BUFFER_SIZE 2048

// The names of our "physical" ports
const char *interfaces[MAX_PORTS] = {"veth1", "veth2", "veth3", "veth4"};
int sockets[MAX_PORTS];

// Helper to create a raw socket and bind it to a specific interface
int create_socket(const char *iface_name) {
    int sock_fd;
    struct ifreq ifr;           // interface request
    struct sockaddr_ll sll;     // socket address link layer
    struct packet_mreq mr;      // packet membership request

    // 1. Create a RAW socket that listens to ALL protocols (ETH_P_ALL)
    if ((sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // 2. Get the Interface Index (required for binding)
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface_name, IFNAMSIZ - 1);

    /* We pass sock_fd because ioctl needs *any* valid network socket
     * to access the Kernel's networking subsystem. It serves as a "handle"
     * to query interface properties, even though this specific socket isn't bound yet.
     */
    if (ioctl(sock_fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("Getting interface index failed");
        exit(1);
    }
    int if_index = ifr.ifr_ifindex;

    // 3. Bind the socket to the interface
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = if_index;
    sll.sll_protocol = htons(ETH_P_ALL);

    if (bind(sock_fd, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    // 4. Set Promiscuous Mode (so we hear everything)
    memset(&mr, 0, sizeof(mr));
    mr.mr_ifindex = if_index;
    mr.mr_type = PACKET_MR_PROMISC;

    if (setsockopt(sock_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) < 0) {
        perror("Promiscuous mode failed");
        exit(1);
    }

    return sock_fd;
}

int main() {
    struct pollfd fds[MAX_PORTS];
    unsigned char buffer[BUFFER_SIZE];

    printf("Starting Simple Hub on 4 ports...\n");

    // Initialize sockets for all ports
    for (int port = 0; port < MAX_PORTS; port++) {
        sockets[port] = create_socket(interfaces[port]);
        fds[port].fd = sockets[port];
        fds[port].events = POLLIN; // We are interested in Reading (Input)
    }

    /*
     * CONCURRENCY NOTE: Physical Hubs vs. Software Hubs
     *
     * In a real physical hub (Layer 1), if packets arrive on Port 1 and Port 2
     * simultaneously, their electrical signals collide and destroy the data.
     *
     * In this software implementation (Layer 2), the Linux Kernel buffers
     * incoming packets in RAM for each interface.
     *
     * The poll() function below converts "simultaneous" events into a sequential
     * "To-Do List." If two packets arrive at the exact same nanosecond:
     * 1. poll() wakes up indicating both ports have data.
     * 2. We process Port 1's packet first.
     * 3. We process Port 2's packet immediately after.
     *
     * Result: No collisions, just a very fast queue.
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
                int len = recvfrom(sockets[incoming_port_index], buffer, BUFFER_SIZE, 0, NULL, NULL);
                if (len < 0) {
                    perror("Receive failed");
                    continue;
                }

                printf("[Port %d] Received %d bytes. Flooding...\n", incoming_port_index + 1, len);

                // --- HUB LOGIC: FLOOD TO ALL OTHER PORTS ---
                for (int outgoing_port_index = 0; outgoing_port_index < MAX_PORTS; outgoing_port_index++) {
                    // Don't send the packet back out the port it came in!
                    if (incoming_port_index != outgoing_port_index) {
                        if (write(sockets[outgoing_port_index], buffer, len) < 0) {
                            perror("Send failed");
                        } else {
                            printf("[Port %d] Sent %d bytes to port %d\n", incoming_port_index + 1, len, outgoing_port_index + 1);
                        }
                    }
                }
            }
        }
    }

    // Cleanup
    for (int port_index = 0; port_index < MAX_PORTS; port_index++) {
        close(sockets[port_index]);
    }
    return 0;
}
