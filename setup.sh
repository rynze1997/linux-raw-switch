#!/bin/bash

# 1. create 4 namespaces (representing 4 PCs)
ip netns add pc1
ip netns add pc2
ip netns add pc3
ip netns add pc4

# 2. Create veth pairs (virtual cables)
# vethX is the hub port, vethX-pc is the PC port
ip link add veth1 type veth peer name veth1-pc
ip link add veth2 type veth peer name veth2-pc
ip link add veth3 type veth peer name veth3-pc
ip link add veth4 type veth peer name veth4-pc

# 3. Move the "pc" ends into the namespaces
ip link set veth1-pc netns pc1
ip link set veth2-pc netns pc2
ip link set veth3-pc netns pc3
ip link set veth4-pc netns pc4

# 4. Bring up the Hub Ports (on your main host)
ip link set veth1 up
ip link set veth2 up
ip link set veth3 up
ip link set veth4 up

# 5. Configure IPs on the PCs and bring them up
ip netns exec pc1 ip addr add 10.0.0.1/24 dev veth1-pc
ip netns exec pc1 ip link set veth1-pc up
ip netns exec pc2 ip addr add 10.0.0.2/24 dev veth2-pc
ip netns exec pc2 ip link set veth2-pc up
ip netns exec pc3 ip addr add 10.0.0.3/24 dev veth3-pc
ip netns exec pc3 ip link set veth3-pc up
ip netns exec pc4 ip addr add 10.0.0.4/24 dev veth4-pc
ip netns exec pc4 ip link set veth4-pc up


echo "Setup Complete. veth1, veth2, veth3, veth4 are your hub ports."