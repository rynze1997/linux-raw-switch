#!/bin/bash

echo "Cleaning up virtual network..."

# 1. Delete the network namespaces
# When you delete a namespace, the virtual interfaces inside it (vethX-pc)
# are destroyed. Because veth interfaces are pairs, destroying one end
# automatically destroys the other end (the hub ports veth1-4).
echo "Deleting Namespaces..."
ip netns del pc1 2>/dev/null
ip netns del pc2 2>/dev/null
ip netns del pc3 2>/dev/null
ip netns del pc4 2>/dev/null

# 2. Cleanup host-side interfaces (Just in case)
# If the previous script failed halfway through, some veths might still 
# exist on the host. We attempt to delete them to be safe.
echo "Cleaning up residual interfaces..."
ip link del veth1 2>/dev/null
ip link del veth2 2>/dev/null
ip link del veth3 2>/dev/null
ip link del veth4 2>/dev/null

echo "Cleanup Complete. Virtual cables and PCs are gone."