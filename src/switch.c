#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <poll.h>

#include "net/socket.h"

#define MAX_PORTS 4
#define BUFFER_SIZE 2048

// The names of our "physical" ports
const char *interfaces[MAX_PORTS] = {"veth1", "veth2", "veth3", "veth4"};
int sockets[MAX_PORTS];

int main() {
    struct pollfd fds[MAX_PORTS];
    unsigned char buffer[BUFFER_SIZE];

    printf("Starting Simple Switch on 4 ports...\n");

    // Initialize sockets for all ports
    for (int port = 0; port < MAX_PORTS; port++) {
        sockets[port] = create_socket(interfaces[port]);
        fds[port].fd = sockets[port];
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
                int len = recvfrom(sockets[incoming_port_index], buffer, BUFFER_SIZE, 0, NULL, NULL);
                if (len < 0) {
                    perror("Receive failed");
                    continue;
                }

                printf("[Port %d] Received %d bytes. Flooding...\n", incoming_port_index + 1, len);

                // --- SWITCH LOGIC: FLOOD TO ALL OTHER PORTS ---
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
