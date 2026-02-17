#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <poll.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#include "switch.h"
#include "mac_table.h"
#include "net/socket.h"

/*------------------------------------------------------------------------------
 * Definitions
 *----------------------------------------------------------------------------*/
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
    bool is_active;          // 1 = UP, 0 = DOWN

    bool request_connect;         // 1 = CLI wants to connect this port
    char pending_name[IFNAMSIZ]; // The name CLI wants to connect to
    bool request_disconnect;      // 1 = CLI wants to disconnect this port
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
static pthread_t switch_thread_id;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/*------------------------------------------------------------------------------
 * Static Function Declarations
 *----------------------------------------------------------------------------*/
/**
 * @brief Print a MAC address.
 *        Used for debugging purposes.
 *
 * @param mac The MAC address to print
 */
static void print_mac(unsigned char *mac);

/**
 * @brief Flood a packet to all active ports.
 *
 * @param incoming_port_index The port the packet came in on
 * @param frame_buffer The frame buffer to send
 * @param len The length of the frame buffer
 */
static void flood_packet(uint8_t incoming_port_index, unsigned char *frame_buffer, size_t len);

/**
 * @brief The main function for the switch thread.
 *
 */
static void *switch_thread_func();

/**
 * @brief Process any pending port connect requests.
 *
 * @param fds The pollfd struct to update
 */
static void process_pending_port_requests(struct pollfd *fds);

/**
 * @brief Process an incoming frame.
 *
 * @param incoming_port_index The index of the incoming port (0-based)
 */
static void process_incoming_frame(int incoming_port_index);

/**
 * @brief Disconnect a port.
 *
 * @param port The port info struct
 * @param port_index The index of the port (0-based)
 */
static void disconnect_port(switch_port_info_t *port, int port_index);

/**
 * @brief Connect a port.
 *
 * @param port The port info struct
 * @param port_index The index of the port (0-based)
 */
static void connect_port(switch_port_info_t *port, int port_index);

/*------------------------------------------------------------------------------
 * Static Functions
 *----------------------------------------------------------------------------*/
static void print_mac(unsigned char *mac) {
    printf("%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void flood_packet(uint8_t incoming_port_index, unsigned char *frame_buffer, size_t len) {
    printf("Flooding...\n");
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

static void disconnect_port(switch_port_info_t *port, int port_index) {
    socket_close(port->socket_fd);
    port->socket_fd = -1;
    port->is_active = false;
    mac_table_flush_port(port_index);
    printf("[Switch Engine] Port %d disconnected.\n", port_index + 1);
}

static void connect_port(switch_port_info_t *port, int port_index) {
    int new_sock = create_socket(port->pending_name);
    if (new_sock >= 0) {
        port->socket_fd = new_sock;
        strncpy(port->if_name, port->pending_name, IFNAMSIZ);
        port->is_active = true;
        printf("[Switch Engine] Port %d connected to %s and is UP.\n", port_index + 1, port->pending_name);
    }
}

static void process_pending_port_requests(struct pollfd *fds) {
    for (int i = 0; i < MAX_PORTS; i++) {
        // Check if CLI asked to connect a port
        if (switch_inst.port[i].request_connect) {
            // Close old socket if it was open
            if (switch_inst.port[i].socket_fd != -1) {
                disconnect_port(&switch_inst.port[i], i);
            }
            connect_port(&switch_inst.port[i], i);
            switch_inst.port[i].request_connect = false; // Request handled
        } else if (switch_inst.port[i].request_disconnect) {
            disconnect_port(&switch_inst.port[i], i);
            switch_inst.port[i].request_disconnect = false; // Request handled
        }

        // Update poll struct
        fds[i].fd = switch_inst.port[i].socket_fd;
        fds[i].events = POLLIN;
    }
}

static void process_incoming_frame(int incoming_port_index) {

    int len = recvfrom(switch_inst.port[incoming_port_index].socket_fd,
                        &switch_inst.frame_buffer, sizeof(switch_inst.frame_buffer), 0, NULL, NULL);
    ethernet_header_t *header = (ethernet_header_t *)switch_inst.frame_buffer;

    if (len < 0) {
        perror("Receive failed");
        return;
    }

    // Ignore ipv6
    if (ntohs(header->ether_type) == ETH_TYPE_IPV6) {
        return;
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
    if (outgoing_port_index == -1 || !switch_inst.port[outgoing_port_index].is_active) {
        flood_packet(incoming_port_index, switch_inst.frame_buffer, len);
    } else {
        printf("Sending to Port %d\n", outgoing_port_index + 1);
        if (write(switch_inst.port[outgoing_port_index].socket_fd, switch_inst.frame_buffer, len) < 0) {
            perror("Send failed");
        } else {
            printf("[Port %d] Sent %d bytes to port %d\n", incoming_port_index + 1, len, outgoing_port_index + 1);
        }
    }
    printf("--------------------------------\n");
}

static void *switch_thread_func() {
    struct pollfd fds[MAX_PORTS];

    /*
     * The poll() function below converts "simultaneous" events into a sequential
     * "To-Do List." If two packets arrive at the exact same nanosecond:
     * 1. poll() wakes up indicating both ports have data.
     * 2. We process Port 1's packet first.
     * 3. We process Port 2's packet immediately after.
     */
     while (!switch_inst.shutdown) {

        pthread_mutex_lock(&lock); // Grab the key
        process_pending_port_requests(fds);
        pthread_mutex_unlock(&lock); // Release the key

        /* poll() blocks until data arrives on ANY of the ports
         * Timeout = 1000ms. If no packets arrive, wake up anyway to check for CLI commands.
         */
        int ret = poll(fds, MAX_PORTS, 1000);

        if (ret <= 0) {
            continue;
        }

        // Check which port has data
        for (int incoming_port_index = 0; incoming_port_index < MAX_PORTS; incoming_port_index++) {
            if (fds[incoming_port_index].revents & POLLIN) {
                process_incoming_frame(incoming_port_index);
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

/*------------------------------------------------------------------------------
 * Public Functions
 *----------------------------------------------------------------------------*/
void switch_init(void) {
    memset(&switch_inst, 0, sizeof(switch_inst));
    for (int i = 0; i < MAX_PORTS; i++) {
        switch_inst.port[i].socket_fd = -1;
    }
}

void switch_start(void) {
    pthread_create(&switch_thread_id, NULL, switch_thread_func, NULL);
}

void switch_stop(void) {
    switch_inst.shutdown = true;
    pthread_join(switch_thread_id, NULL);
}

int switch_connect_port(int port, const char *iface_name) {
    int port_idx = port - 1; // Convert from 1-based to 0-based

    if (port_idx < 0 || port_idx >= MAX_PORTS) {
        return -1;
    }

    pthread_mutex_lock(&lock);
    strncpy(switch_inst.port[port_idx].pending_name, iface_name, IFNAMSIZ);
    switch_inst.port[port_idx].request_connect = true;
    pthread_mutex_unlock(&lock);

    return 0;
}

int switch_disconnect_port(int port) {
    int port_idx = port - 1; // Convert from 1-based to 0-based

    if (port_idx < 0 || port_idx >= MAX_PORTS) {
        return -1;
    }

    pthread_mutex_lock(&lock);
    switch_inst.port[port_idx].request_disconnect = true;
    pthread_mutex_unlock(&lock);

    return 0;
}
