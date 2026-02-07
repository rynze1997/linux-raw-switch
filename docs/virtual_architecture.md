# Virtual Architecture: Network Namespaces & Veth Pairs

This document explains the Linux networking concepts used to simulate a physical network environment for testing a custom C-based Ethernet Hub.

## 1. Core Concepts

### Network Namespaces (`netns`)
**Analogy: A Virtual PC**

In Linux, the main computer has one "network stack" (one routing table, one list of IP addresses, one set of firewall rules). A **Network Namespace** creates a completely isolated copy of that stack.

* **Isolation:** A process running inside a namespace (like `pc1`) thinks it is the only thing on the computer. It cannot see the network cards of the main host or other namespaces.
* **Independence:** It has its own `localhost`, its own IP address, and its own routing table.

In this project, `pc1`, `pc2`, `pc3`, and `pc4` act as four tiny, separate Linux computers running inside the main machine.

### Virtual Ethernet (`veth`)
**Analogy: A Virtual Patch Cable**

A physical patch cable has two ends. If you plug one end into a switch and the other into a PC, electrons flow between them. A **veth pair** is the software equivalent. It always comes in pairs.

* **Behavior:** Anything injected into `End-A` immediately pops out of `End-B`, and vice versa.
* **The Setup:**
    * **End A (`veth1`):** Stays in the main OS (Host). Your C program attaches to this. It acts as the **Hub Port**.
    * **End B (`veth1-pc`):** Is moved inside the `pc1` namespace. It acts as the **PC's Network Card (NIC)**.

## 2. The Setup Script Explained

Here is what happens line-by-line when running the setup script.

### Step 1: Create the Virtual PCs
```bash
ip netns add pc1
```

* **Action:** Creates a new, empty network namespace named `pc1`.
* **State:** `pc1` is currently a "computer" with no network cards (except a down loopback). It is completely disconnected.

### Step 2: Create the Virtual Cable

```bash
ip link add veth1 type veth peer name veth1-pc
```

* **Action:** Creates a pair of connected virtual interfaces.
* **State:** Both ends (`veth1` and `veth1-pc`) are currently sitting in the main host OS. It is like a cable lying on a desk, not plugged in yet.

### Step 3: Plug the Cable into the PC

```bash
ip link set veth1-pc netns pc1
```

* **Action:** Takes the interface `veth1-pc` and moves it *into* the `pc1` namespace.
**State:**
* `veth1` remains visible to the main OS (and the C program).
* `veth1-pc` **disappears** from the main OS. It is now only visible inside `pc1`.
* We have successfully "wired" the hub port to the PC.



### Step 4: Turn the Hub Port On

```bash
ip link set veth1 up
```

* **Action:** Powers up the interface on the host side.
* **Why:** By default, interfaces are "down" (off). The C program cannot read from a down interface.

### Step 5: Configure the PC

```bash
ip netns exec pc1 ip addr add 10.0.0.1/24 dev veth1-pc
ip netns exec pc1 ip link set veth1-pc up
```

* **`ip netns exec pc1 ...`**: This wrapper executes the command *inside* the environment of `pc1`.
* **`ip addr add`**: Assigns the IP address to the PC's network card.
* **`ip link set ... up`**: Turns on the network card inside the PC.
---

## 3. Data Flow Visualization

When `pc1` pings `pc2`, this is the physical path the data takes:

1. **Inside `pc1`:** The ping command generates an ICMP packet. It sends it out `veth1-pc`.
2. **The Tunnel:** The Linux kernel instantly transports that packet to the other end of the pair: `veth1`.
3. **The Host (Switch):** The packet arrives at `veth1`. The C program (Hub) reads it using `recvfrom`.
4. **The Flooding:** The C program writes that packet to `veth2`.
5. **The Tunnel:** The packet travels from `veth2` to `veth2-pc`.
6. **Inside `pc2`:** The packet pops out of the network card `veth2-pc`, where the OS receives it.