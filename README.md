# Virtual Network Switch

A Layer 2 Ethernet Switch implemented in C using Linux raw sockets. The switch learns MAC addresses from incoming traffic and forwards frames only to the correct port (unicast), falling back to flooding when the destination is unknown. Ports are connected to network interfaces dynamically via an interactive CLI.

This project serves as a foundation for understanding Layer 2 networking concepts and is a step towards building a more advanced VLAN-aware Ethernet Switch.

## Overview

The switch runs in two threads:
- **Switch Engine** (background thread): Handles packet reception, MAC learning, forwarding, and flooding.
- **CLI** (main thread): Accepts user commands to connect switch ports to network interfaces at runtime.

Ports are not hardcoded. You connect them at runtime using the `connect` command.

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
- 4 veth pairs connecting each PC to the switch
- IP addresses `10.0.0.1/24` through `10.0.0.4/24`

### Tear down the project

```bash
sudo ./cleanup.sh
```

## Running the Switch

```bash
sudo ./build/sw_switch
```

The switch requires root privileges to create raw sockets and enable promiscuous mode.

### CLI Commands

Once the switch is running, use the interactive CLI to manage ports. Type "help" to list all available commands.

Connect switch ports to virtual interfaces:

```
Switch> connect 1 veth1
Switch> connect 2 veth2
Switch> connect 3 veth3
Switch> connect 4 veth4
```

## Testing

With the switch running and ports connected, open additional terminals to test connectivity:

```bash
# Ping from PC1 to PC3
sudo ip netns exec pc1 ping 10.0.0.3

# Ping from PC2 to PC4
sudo ip netns exec pc2 ping 10.0.0.4
```

You should see the switch logging received packets. On the first packet to an unknown destination, the switch floods to all ports. Once the MAC address is learned, subsequent packets are forwarded only to the correct port.

### Verifying Switch Learning with Packet Capture

To observe the difference from a hub, capture traffic on a PC that is not the destination:

```bash
# Terminal 1: Run the switch and connect ports
sudo ./build/sw_switch

# Terminal 2: Capture traffic on PC2 (not involved in the ping)
sudo ip netns exec pc2 tcpdump -i veth2-pc -n

# Terminal 3: Ping from PC1 to PC3
sudo ip netns exec pc1 ping 10.0.0.3
```

Initially, PC2 will see flooded traffic (ARP requests, first ICMP). Once the switch learns the MAC addresses, PC2 will stop seeing traffic between PC1 and PC3 — demonstrating that the switch forwards unicast frames only to the correct port.

## Documentation

The project includes detailed theory documentation:

| Document | Description |
|----------|-------------|
| [virtual_architecture.md](docs/virtual_architecture.md) | Explains network namespaces, veth pairs, and how this project simulates physical hardware |
| [sockets_concept.md](docs/sockets_concept.md) | Compares standard application sockets (AF_INET) with raw packet sockets (AF_PACKET) |
| [socket_creation.md](docs/socket_creation.md) | Deep dive into raw socket creation, binding, and promiscuous mode |

## License

This project is for educational purposes.
