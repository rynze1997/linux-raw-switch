#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "mac_table.h"
/*------------------------------------------------------------------------------
 * Definitions
 *----------------------------------------------------------------------------*/
#define TABLE_SIZE 1024

/*------------------------------------------------------------------------------
 * Types
 *----------------------------------------------------------------------------*/
typedef struct mac_entry_st {
    unsigned char mac[MAC_ADDR_LEN];
    uint8_t port_index;
} mac_entry_t;

typedef struct mac_table_st {
    mac_entry_t entries[TABLE_SIZE];
    uint16_t count;
} mac_table_t;

/*------------------------------------------------------------------------------
 * Static Variables
 *----------------------------------------------------------------------------*/
static mac_table_t mac_table;

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

/*------------------------------------------------------------------------------
 * Static Functions Definitions
 *----------------------------------------------------------------------------*/
static void print_mac(unsigned char *mac) {
    printf("%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/*------------------------------------------------------------------------------
 * Public Functions Definitions
 *----------------------------------------------------------------------------*/
void mac_table_update(unsigned char *src_mac, uint8_t port) {

    // Check if we already know this MAC
    for (int i = 0; i < mac_table.count; i++) {
        if (memcmp(mac_table.entries[i].mac, src_mac, MAC_ADDR_LEN) == 0) {
            // Found it! Update timestamp and port (in case it moved)
            if (mac_table.entries[i].port_index != port) {
                printf("MAC moved! ");
                print_mac(src_mac);
                printf(" moved from Port %d to Port %d\n", mac_table.entries[i].port_index, port + 1);
                mac_table.entries[i].port_index = port;
            }
            return; // Done
        }
    }

    // If not found, add new entry
    if (mac_table.count < TABLE_SIZE) {
        memcpy(mac_table.entries[mac_table.count].mac, src_mac, MAC_ADDR_LEN);
        mac_table.entries[mac_table.count].port_index = port;
        mac_table.count++;

        printf("LEARNED: ");
        print_mac(src_mac);
        printf(" is on Port %d\n", port + 1);
    } else {
        printf("Table full! Cannot learn new MAC.\n");
    }
}

int mac_table_lookup_port(unsigned char *dst_mac) {
    // Broadcast address (FF:FF:FF:FF:FF:FF) must ALWAYS be flooded
    unsigned char broadcast[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    if (memcmp(dst_mac, broadcast, MAC_ADDR_LEN) == 0) {
        return -1; // -1 means "Flood"
    }

    // Search the table
    for (int i = 0; i < mac_table.count; i++) {
        if (memcmp(mac_table.entries[i].mac, dst_mac, MAC_ADDR_LEN) == 0) {
            return mac_table.entries[i].port_index; // Found the specific port!
        }
    }

    return -1; // Not found -> Flood
}

void mac_table_flush_port(uint8_t port) {
    for (int i = 0; i < mac_table.count; i++) {
        if (mac_table.entries[i].port_index == port) {
            // Swap with last entry and shrink table
            mac_table.entries[i] = mac_table.entries[mac_table.count - 1];
            mac_table.count--;
            i--; // Re-check this index since we swapped in a new entry
        }
    }
}
