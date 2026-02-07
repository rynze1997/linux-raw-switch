# Virtual Network Switch

A simple Ethernet Hub implemented in C using Linux raw sockets. This project creates a Layer 2 hub that floods incoming traffic to all connected ports, simulating how a physical network hub operates.

This project serves as a foundation for understanding Layer 2 networking concepts and is a first step before building a more advanced Ethernet Switch that learns MAC addresses and is VLAN-aware.

## Overview

The hub listens on 4 virtual network interfaces (`veth1`-`veth4`) and forwards any packet received on one port to all other ports.

## Prerequisites

- Linux operating system
- Root privileges (for raw sockets and network namespace management)
- GCC compiler

## Building

```bash
make
```

To clean up build artifacts:

```bash
make clean
```

## Setting Up the Virtual Environment

The project includes scripts to create a virtual network environment with 4 simulated PCs.

### Create the project

```bash
sudo ./setup.sh
```

This creates:
- 4 network namespaces (`pc1`-`pc4`) acting as virtual PCs
- 4 veth pairs connecting each PC to the hub
- IP addresses `10.0.0.1/24` through `10.0.0.4/24`

### Tear down the project

```bash
sudo ./cleanup.sh
```

## Running the Hub

```bash
sudo ./build/hub
```

The hub requires root privileges to create raw sockets and enable promiscuous mode.

## Testing

With the hub running in one terminal, open additional terminals to test connectivity:

```bash
# Ping from PC1 to PC3
sudo ip netns exec pc1 ping 10.0.0.3

# Ping from PC2 to PC4
sudo ip netns exec pc2 ping 10.0.0.4
```

You should see the hub logging received packets and flooding them to all other ports.

### Verifying Hub Flooding with Packet Capture

To prove the hub floods to all ports, you can capture traffic on a PC that is not the destination. Use `tcpdump` on PC2 while pinging between PC1 and PC3:

```bash
# Terminal 1: Run the hub
sudo ./build/hub

# Terminal 2: Capture traffic on PC2 (not involved in the ping)
sudo ip netns exec pc2 tcpdump -i veth2-pc -n

# Terminal 3: Ping from PC1 to PC3
sudo ip netns exec pc1 ping 10.0.0.3
```

You will see PC2 receiving traffic meant for PC3:

```
ARP, Request who-has 10.0.0.3 tell 10.0.0.1
IP 10.0.0.1 > 10.0.0.3: ICMP echo request
IP 10.0.0.3 > 10.0.0.1: ICMP echo reply
```

This demonstrates classic hub behavior — all ports receive all traffic, even if they are not the intended destination. PC2's OS receives these packets but drops them at the IP layer since they are not addressed to it.

## Project Structure

```
hub/
├── hub.c                      # Main hub implementation
├── Makefile                   # Build configuration
├── setup.sh                   # Creates virtual network
├── cleanup.sh                 # Tears down virtual network
├── README.md                  # This file
└── docs/
    ├── virtual_architecture.md       # Network namespaces & veth pairs
    ├── sockets_concept.md            # Socket theory: application vs infrastructure
    └── socket_creation.md            # Raw socket implementation details
```

## Documentation

The project includes detailed theory documentation:

| Document | Description |
|----------|-------------|
| [virtual_architecture.md](virtual_architecture.md) | Explains network namespaces, veth pairs, and how this project simulates physical hardware |
| [sockets_concept.md](sockets_concept.md) | Compares standard application sockets (AF_INET) with raw packet sockets (AF_PACKET) |
| [socket_creation.md](socket_creation.md) | Deep dive into raw socket creation, binding, and promiscuous mode |

## How It Works

1. **Socket Creation**: Creates raw `AF_PACKET` sockets bound to each virtual interface
2. **Promiscuous Mode**: Enables receiving all packets, not just those addressed to the interface
3. **Poll Loop**: Uses `poll()` to efficiently wait for packets on any port
4. **Flooding**: When a packet arrives, it is forwarded to all other ports (classic hub behavior)

## License

This project is for educational purposes.
