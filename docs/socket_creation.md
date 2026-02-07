# Socket Creation: The Bridge to the Kernel

This document details the theory and mechanics behind using Raw Sockets (`AF_PACKET`) in Linux C programming. This is the core mechanism that allows a userspace program to act as an Ethernet Hub (and later, a Switch).

## 1. The Mental Model: The "Magic File"

In Linux, the core philosophy is **"Everything is a file."**

A **Socket** is simply a way to make the Network Interface Card (NIC) look like a file so your C program can `read()` and `write()` to it.

* **Standard Files:** You `open()` a file on a disk, get an ID (File Descriptor), and `read()` bytes from the hard drive.
* **Sockets:** You `socket()` a connection, get an ID (File Descriptor), and `read()` bytes that just arrived on the wire.

**The Crucial Difference:**
* **Web Programming (TCP/UDP):** Usually uses "Stream Sockets". The OS acts as a polite secretary—handling headers, handshakes, and re-ordering packets for you. You only see the final data payload.
* **Hub/Switch Programming (Raw Sockets):** We cannot be polite. We need the raw, messy digital signal, including the Ethernet Headers (MAC addresses). We use **Raw Sockets** to bypass the OS's networking logic.



---

## 2. The Code Breakdown: `socket()`

The entry point to the kernel's networking subsystem is the `socket` system call.

```c
int sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
```

This single line configures our "portal" to the kernel using three specific arguments:

### Argument 1: The Domain (`AF_PACKET`)

* **Standard (`AF_INET`):** Tells the kernel "I want to talk to the Internet (IP addresses)." The Kernel automatically strips off Ethernet headers before giving you the data.
* **Our Choice (`AF_PACKET`):** Tells the kernel "I want to talk to the Network Card driver directly."
* **Result:** We receive the raw Layer 2 frame (Header + Payload).

### Argument 2: The Type (`SOCK_RAW`)

* **Standard (`SOCK_STREAM`):** "Give me a clean, ordered stream of bytes (TCP)."
* **Our Choice (`SOCK_RAW`):** "Give me the packet exactly as it appeared on the wire."
* **Result:** If the packet has a VLAN tag, we see it. If the packet is malformed, we might still see it. We have full control.

### Argument 3: The Protocol (`htons(ETH_P_ALL)`)

* **Meaning:** "Which specific packets should be sent to this socket?"
* `ETH_P_IP`: Only IPv4 packets.
* `ETH_P_ARP`: Only ARP packets.
* `ETH_P_ALL`: **Every single packet.**


* *Note: `htons` (Host to Network Short) ensures the byte-order is correct for the networking hardware.*

---

## 3. The Binding (`bind`)

Running `socket()` creates a generic socket in memory, but it isn't "plugged in" to any specific port. It is floating. If left unbound, it might try to listen to every interface on the device (WiFi, Loopback, Ethernet).

We must "glue" the socket to a specific physical interface (e.g., `veth1`) using `bind()`:

```c
struct sockaddr_ll sll;
sll.sll_family = AF_PACKET;
sll.sll_ifindex = index_of_veth1; // <--- The critical identifier
bind(sock_fd, (struct sockaddr *)&sll, sizeof(sll));
```

**Result:** Now, when we call `read(sock_fd)`, we only receive electricity/data that physically hit the `veth1` interface.

---

## 4. Promiscuous Mode ("Spy Mode")

This step is specific to Hubs, Switches, and Packet Sniffers (like Wireshark).

**The Problem:**
By default, a Network Card (NIC) hardware is programmed to be efficient. If a packet arrives with a Destination MAC of `AA:BB:CC`, but the NIC's actual MAC is `11:22:33`, the hardware drops the packet immediately. The CPU never sees it.

**The Hub/Switch Requirement:**
A Hub or Switch's job is to forward traffic meant for *other people*. If `PC A` sends a message to `PC B`, the Hub must "hear" it to forward it, even though the Hub isn't the final destination.

**The Solution:**
We use `setsockopt` to enable **Promiscuous Mode**.

```c
setsockopt(..., PACKET_ADD_MEMBERSHIP, ... PACKET_MR_PROMISC ...);

```

**Translation:** "Hey Network Card, turn off your hardware filter. Pass **every single electrical signal** that hits the wire up to the CPU, regardless of who it is addressed to."

---

## Summary of Data Flow

1. **Hardware (`veth1`):** Frame arrives on the wire.
2. **Driver:** Sees "Promiscuous Mode" is on. Passes the raw frame to the Kernel.
3. **Kernel:** Sees an `AF_PACKET` socket bound to this interface.
4. **Socket Buffer:** Kernel copies the frame into the socket's memory buffer.
5. **User Space (C Program):** `recvfrom(sock_fd)` wakes up, reads the data, and stores it in our `unsigned char buffer[]`.

We now hold the raw Ethernet Frame (Bytes) in a C array, ready for inspection.