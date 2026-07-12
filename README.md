# Lv2-bridge — A Userspace Ethernet Bridge Prototype

Lv2-bridge is a small userspace Ethernet bridge/switch implementation for Linux. It captures and forwards raw Ethernet frames between network interfaces using Linux `AF_PACKET` sockets.

The bridge implements basic MAC address learning through a forwarding database (FDB). Frames with known destination MAC addresses are forwarded to the corresponding port, while frames with unknown destinations are flooded to the other active ports.

Lv2-bridge is an experimental and educational project. It does not implement the Spanning Tree Algorithm (STA), meaning that network loops may cause broadcast storms and other forwarding problems. It is not intended to replace the Linux bridge implementation or production network hardware.

The project was inspired by the bridging concepts described in IEEE 802.1D.

## Features

* Userspace Ethernet frame forwarding
* Raw `AF_PACKET` sockets
* MAC address learning
* Forwarding database (FDB)
* FDB entry aging
* Unknown-destination frame flooding
* Promiscuous interface mode
* Multiple bridge ports
* Multithreaded frame processing
* Thread-safe producer/consumer ring buffer
* Interactive command-line interface

## Requirements

* Linux
* A modern C compiler with C17 support or later
* POSIX threads
* `make`
* Root privileges or the required network capabilities

## Installation

Clone the source code and build the project using `make`.

## Usage

Start the userspace bridge with one or more interfaces:
sudo ./bridge <interface1> <interface2> [interface3 ...]

Example:
sudo ./bridge eth0 eth1

While running, use the interactive CLI:

run                 Start frame forwarding
stop                Stop frame forwarding
add <iface...>      Add network interfaces
remove/rm <iface>   Remove network interfaces
show fdb [n]        Display learned MAC addresses
help/?              Show available commands
exit                Shutdown lv2-bridge

## Structure

Lv2-bridge consists of the following componets:

## Threads

### MAC Entity

A MAC Entity is created for each bridge interface. It listens for Ethernet frames on its assigned interface, classifies incoming frames, and forwards them to the MAC Relay Entity through a thread-safe RingBuffer.

### MAC Relay Entity

The MAC Relay Entity receives frames from MAC Entities through the thread-safe RingBuffer.

For each received frame:

1. The source MAC address is learned and stored in the FDB.
2. The destination MAC address is searched in the FDB.
3. If the destination address exists, the frame is forwarded to the corresponding port.
4. If the destination address is unknown, the frame is flooded to all other active ports.

## Important Data Structures

### FDB (Forwarding Database)

The Forwarding Database stores learned MAC address mappings.

Structure:

The FDB is used to determine the outgoing interface for frames with known destination addresses.

### RingBuffer

A thread-safe producer/consumer queue used for communication between MAC Entities and the MAC Relay Entity.

It allows packet reception and packet forwarding to run independently without unsafe concurrent access.

## Important Structures

### Port

A logical representation of an active network interface.

A Port is provided as an input parameter when creating a MAC Entity.

### Bridge

A logical representation of bridge resources and runtime state.

The Bridge contains the global state required for forwarding operations, including ports, FDB state, and shared resources.

## Limitations

* No Spanning Tree Algorithm (STA) support
* Network loops are not detected or prevented
* Intended for educational and experimental use
* Linux-specific due to the use of `AF_PACKET`

## License

This project is licensed under the GNU General Public License (GPL).
