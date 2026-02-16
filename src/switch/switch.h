#ifndef SWITCH_H
#define SWITCH_H

#define MAX_PORTS 4

/**
 * @brief Initialize the switch instance.
 */
void switch_init(void);

/**
 * @brief Start the switch engine in a background thread.
 */
void switch_start(void);

/**
 * @brief Signal the switch engine to stop and wait for it to finish.
 */
void switch_stop(void);

/**
 * @brief Request the switch engine to connect a port to an interface.
 *
 * @param port Port number (1-based, 1 to MAX_PORTS)
 * @param iface_name Name of the network interface (e.g., "veth1")
 * @return 0 on success, -1 on invalid port
 */
int switch_connect_port(int port, const char *iface_name);

#endif // SWITCH_H
