#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>

#include "socket.h"

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
