#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/if_ether.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <assert.h>
#include <arpa/inet.h>

#include "mybridge.h"

struct flood {
	Port **arr;
	size_t len;
	Port recv_port;
	char *buffer;
	size_t size;
};	

void flood (struct flood args){
	struct ethhdr *header = (struct ethhdr *) (args.buffer);
	for (int i = 0; i < args.len; i ++){
		if (args.recv_port.fd != args.arr[i]->fd){
			if (args.arr[i]->fd == -1) continue;

			struct sockaddr_ll addr = args.arr[i]->addr;
			
			sendto (args.arr[i]->fd, args.buffer, args.size, 0,
					(struct sockaddr *) &addr,
					sizeof (args.arr[i]->addr));

			mac_addr_t dst;
			mac_addr_t src;

			memcpy (dst.mac, header->h_dest, 6);
			memcpy (src.mac, header->h_source, 6);


		}
	}
	return;
}

void *mac_relay (void *args){
	Bridge *bridge = (Bridge *)args;

	RingBuffer *rbuffer = bridge->rbuffer;
	Fdb *fdb = bridge->fdb;

	int size;
	Port **arr = to_portarry (bridge, &size);

	to_mac_relay msg;
	char *buffer = msg.buffer;

	struct flood flood_args;
	flood_args.arr = arr;
	flood_args.len = size;
	flood_args.buffer = buffer;
	
	while (bridge->is_running){
		RingBuffer_consume (rbuffer, &msg);
		struct ethhdr *header = (struct ethhdr *) buffer;
		mac_addr_t src;
		mac_addr_t dst;
		memcpy (src.mac, header->h_source, 6);
		memcpy (dst.mac, header->h_dest, 6);

		fdb_add_mac (fdb, src, msg.eth);
		if (memcmp (msg.eth.addr.sll_addr, dst.mac, 6) == 0) continue;
		if (msg.type != user_data_frame) continue;

		FdbEntry entry = fdb_resolve_mac (fdb, dst);
		if (!is_valid_fdbentry (&entry)){
			flood_args.recv_port = msg.eth;
			flood_args.size = msg.size;
			flood (flood_args);
			continue;
		}
		if (entry.port.status != PORT_STATUS_ACTIVE) continue;

		sendto (entry.port.fd, buffer, msg.size, 0, (struct sockaddr *) &entry.port.addr, sizeof (entry.port.addr));
	}
	free (arr);
	return NULL;
}
