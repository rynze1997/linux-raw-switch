#ifndef SOCKET_H
#define SOCKET_H

// Helper to create a raw socket and bind it to a specific interface
int create_socket(const char *iface_name);

#endif // SOCKET_H
