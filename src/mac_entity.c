#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/if_ether.h>
#include <poll.h>

#include "mybridge.h"

#define EFC 0x8808

static inline bool is_bridge_group_addr (mac_addr_t mac1){
	uint8_t *mac = mac1.mac;
	return mac[0] == 0x01 &&
		mac[1] == 0x80 &&
		mac[2] == 0xC2 &&
		mac[3] == 0x00 &&
		mac[4] == 0x00 &&
		mac[5] >= 0x00 &&
		mac[5] <= 0x0f;
}

void *mac_entity (void *args){
	Port port = *(Port *)(args);
	Bridge *bridge = port.bridge;

	RingBuffer *rbuffer = bridge->rbuffer;

	to_mac_relay msg;
	char *buffer = msg.buffer;
	msg.eth = port;
	size_t len;

	struct pollfd fd;
	fd.fd = port.fd;
	fd.events = POLLIN;

	while (bridge->is_running){

		struct sockaddr_ll paddr;
		socklen_t paddrlen = sizeof (paddr);

		while (poll (&fd, 1, 1000) == 0 && bridge->is_running);
		if (!bridge->is_running) return NULL;

		len = recvfrom (port.fd, buffer, BUFFER_SIZE, 0,
				(struct sockaddr *) &paddr, &paddrlen);

		if (paddrlen <= 0) continue;

		if (paddr.sll_pkttype == PACKET_OUTGOING) continue;

		struct ethhdr *header = (struct ethhdr *) buffer;
		if (len < sizeof (struct ethhdr)) continue;

		mac_addr_t dst;
		mac_addr_t src;
		memcpy (dst.mac, header->h_dest, 6);
		memcpy (src.mac, header->h_source, 6);
		frame_type_t frame_type;

		if (is_bridge_group_addr (dst)) frame_type = mac_specific_frame;
		else if (ntohs (header->h_proto) == EFC) frame_type = mac_reserved_frame;
		else frame_type = user_data_frame;

		msg.type = frame_type;
		msg.size = len;

		RingBuffer_publish (rbuffer, &msg, len + sizeof (to_mac_relay) - BUFFER_SIZE);
	}
	return NULL;
}
