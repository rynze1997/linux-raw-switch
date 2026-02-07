#ifndef MAC_TABLE_H
#define MAC_TABLE_H

#include <stdint.h>

/*------------------------------------------------------------------------------
 * Definitions
 *----------------------------------------------------------------------------*/
#define MAC_ADDR_LEN 6


/*------------------------------------------------------------------------------
 * Function Declarations
 *----------------------------------------------------------------------------*/
/**
 * @brief Update the MAC table with a new MAC address and port.
 *
 * @param src_mac The source MAC address
 * @param port The port number
 */
void mac_table_update(unsigned char *src_mac, int port);

/**
 * @brief Lookup the port number for a given MAC address.
 *
 * @param dst_mac The destination MAC address
 * @return The port number, or -1 if not found
 */
int mac_table_lookup_port(unsigned char *dst_mac);

#endif // MAC_TABLE_H
