#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <linux/if.h>
#include <assert.h>
#include <pthread.h>

#include "mybridge.h"

#define MAX_NUMBER_OF_PORTS 10

typedef struct EthPorts {
	Port arr[MAX_NUMBER_OF_PORTS];
} EthPorts;

struct sockaddr_ll get_addr (const char *pname, int fd, int *err);
int register_port (const char *pname, Port *port);

struct sockaddr_ll get_addr (const char *pname, int fd, int *err){
	struct ifreq interface;
	struct sockaddr_ll addr;
	memset (&interface, 0, sizeof (struct ifreq));
	memset (&addr, 0, sizeof (struct sockaddr_ll));

	strcpy (interface.ifr_name, pname);

	if ((*err = ioctl (fd, SIOCGIFINDEX, &interface)) != 0){
		fprintf (stderr, "failed to get interface mapping dev:%s\n", pname);
	}
	addr.sll_ifindex = interface.ifr_ifindex;
	if ((*err = ioctl (fd, SIOCGIFHWADDR, &interface)) != 0){
		fprintf (stderr, "failed to get hw interface addr dev:%s\n", pname);
	}

	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons (ETH_P_ALL);
	addr.sll_halen = 6;
	memcpy (addr.sll_addr, interface.ifr_hwaddr.sa_data, 6);

	return addr;
}

int register_port (const char *pname, Port *port){
	int fd = socket (AF_PACKET, SOCK_RAW | SOCK_NONBLOCK, htons (ETH_P_ALL));
	int ret;

	if (strlen (pname) + 1 > IFNAMSIZ) {
		fprintf (stderr, "error, interface name can't be longer than %d\n", IFNAMSIZ - 1);
		goto fail;
	}
	if (fd < 0) {
		fprintf (stderr, "failed to create a socket: %s\n", strerror (errno));
		goto fail;
	}

	port->addr = get_addr (pname, fd, &ret);
	port->fd = fd;
	strcpy (port->name, pname);
	if (ret != 0) goto fail_close;

	struct packet_mreq mreq;

	mreq.mr_ifindex = port->addr.sll_ifindex;
	mreq.mr_type = PACKET_MR_PROMISC;
	mreq.mr_alen = 6;
	memcpy (mreq.mr_address, port->addr.sll_addr, 6);

	ret = setsockopt (fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof (mreq));
	if (ret != 0) {
		fprintf (stderr, "failed to setsockopt: %s\n", strerror (errno));
		goto fail_close;
	}

	ret = bind (fd, (struct sockaddr *) &(port->addr), sizeof (struct sockaddr_ll));
	if (ret != 0){
		fprintf (stderr, "failed to bind socket to interface: %s. error: %s\n", pname, strerror (errno));
		goto fail_close;
	}

	return fd;
fail_close:
	close (fd);
fail:
	port->fd = -1;
	return -1;
}

int find_port (Bridge *bridge, char *pname){
	EthPorts *ports = bridge->ethports;
	for (int i = 0; i < MAX_NUMBER_OF_PORTS; i ++){
		if (strcmp (pname, ports->arr[i].name) == 0) return i;
	}
	return -1;
}

EthPorts *make_ethports (void){
	EthPorts *ports = malloc (sizeof (EthPorts));
	if (ports) {
		for (int i = 0; i < MAX_NUMBER_OF_PORTS; i ++){
			memset (&ports->arr[i], 0, sizeof (ports->arr[i]));
			ports->arr[i].name[0] = '\0';
		}
		return ports;
	}
	return NULL;
}

int add_port (Bridge *bridge, char *pname){
	EthPorts *ports = bridge->ethports;
	int index = find_port (bridge, "");
	if (index == -1){
		fprintf (stderr, "error too many interfaces\n");
		return -1;
	}
	int t  = find_port (bridge, pname);
	if (t != -1){
		fprintf (stderr, "interface %s was already added\n", pname);
		return -1;
	}

	int ret = register_port (pname, &ports->arr[index]);
	ports->arr[index].status = PORT_STATUS_ACTIVE;
	ports->arr[index].bridge = bridge;
	if (ret < 0){
		memset (&ports->arr[index], 0, sizeof (ports->arr[index]));
		ports->arr[index].name[0] = '\0';
		ports->arr[index].status = PORT_STATUS_DISABLED;
		return -1;
	}
	return 0;
}

int remove_port (Bridge *bridge, char *pname){
	EthPorts *ports = bridge->ethports;
	int index = find_port (bridge, pname);
	if (index == -1){
		fprintf (stderr, "could not find interface %s\n", pname);
		return -1;
	}
	close (ports->arr[index].fd);
	memset (&ports->arr[index], 0, sizeof (ports->arr[index]));
	ports->arr[index].name[0] = '\0';

	return 0;
}

Port **to_portarry (Bridge *bridge, int *arr_size){
	EthPorts *ports = bridge->ethports;
	int size = 0;
	for (int i = 0; i < MAX_NUMBER_OF_PORTS; i ++){
		if (ports->arr[i].name[0] != '\0') size ++;
	}

	Port **arr = malloc (sizeof (Port *) * size);
	int j = 0;

	if (arr){
		for (int i = 0; i < MAX_NUMBER_OF_PORTS; i++){
			if (ports->arr[i].name[0] != '\0'){
				arr[j ++] = &ports->arr[i];
			}
		}
		*arr_size = size;
		return arr;
	}
	*arr_size = 0;
	return NULL;
}

void destroy_ethports (Bridge *bridge){
	free (bridge->ethports);
}

