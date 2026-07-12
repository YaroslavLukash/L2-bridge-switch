#include <stdbool.h>
#include <stdint.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <sys/socket.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>

#ifndef MYBRIDGE_H
#define MYBRIDGE_H

#define fatale_error(msg) \
	{fprintf (stderr, "%s\n", msg); exit (EXIT_FAILURE);}

#define BUFFER_SIZE 9216

#define PORT_STATUS_DISABLED 0
#define PORT_STATUS_ACTIVE 1

typedef struct {
	uint8_t mac[6];
} mac_addr_t;

//main.c

typedef struct Bridge Bridge;

typedef struct {
	char name[IFNAMSIZ];
	Bridge *bridge;
	int status;
	int fd;
	struct sockaddr_ll addr;
} Port;

// FDB

typedef struct {
	mac_addr_t dst;
	Port port;
	time_t lastseen;
} FdbEntry;

typedef struct Fdb Fdb;

Fdb *make_fdb (void);
void destroy_fdb (Fdb *fdb);
void fdb_add_mac (Fdb *fdb, mac_addr_t dst, Port port);
FdbEntry fdb_resolve_mac (Fdb *fdb, mac_addr_t mac);
bool fdb_should_relay (Fdb *fdb, mac_addr_t mac, Port port);
bool is_valid_fdbentry (FdbEntry *entry); 
FdbEntry **fdb_to_arry (Fdb *fdb, int *arry_size);

// mac_entity

void *mac_entity (void *args);

typedef enum {
	user_data_frame,
	mac_specific_frame,
	mac_reserved_frame
} frame_type_t;

typedef struct RingBuffer RingBuffer;

// ring_buffer
typedef struct RingBuffer RingBuffer;
RingBuffer *make_RingBuffer (size_t slot_size, size_t number_of_slots);
void destroy_RingBuffer (RingBuffer *rbuffer);
void RingBuffer_publish (RingBuffer *rbuffer, void *msg, size_t size);
void RingBuffer_consume (RingBuffer *rbuffer, void *dst);
void flush_RingBuffer (RingBuffer *rbuffer);
void shutdown_RingBuffer (RingBuffer *rbuffer);

// mac_relay

typedef struct {
	frame_type_t type;
	Port eth;
	size_t size;
	char buffer[BUFFER_SIZE];
} to_mac_relay;

void *mac_relay (void *);

// bridge

typedef struct EthPorts EthPorts;
Port **to_portarry (Bridge *bridge, int *arr_size);

typedef struct Bridge {
	RingBuffer *rbuffer;
	Fdb *fdb;
	EthPorts *ethports;

	bool is_running;
	pthread_t *threads;
	int size;
} Bridge;

int find_port (Bridge *bridge, char *pname);
EthPorts *make_ethports (void);
int add_port (Bridge *bridge, char *pname);
int remove_port (Bridge *bridge, char *pname);
void destroy_ethports (Bridge *bridge);

#endif
