#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

#include "mybridge.h"

#define FDB_MAX_SIZE 100
#define TOO_OLD 300

static inline bool same_mac (mac_addr_t mac1, mac_addr_t mac2){
	return memcmp (mac1.mac, mac2.mac, 6) == 0;
}

typedef struct Fdb{
	FdbEntry data[FDB_MAX_SIZE];
	pthread_mutex_t lock;
} Fdb;

bool is_valid_fdbentry (FdbEntry *ent){
	return ent->lastseen != 0;
}

Fdb *make_fdb (void) {
	Fdb *fdb = malloc (sizeof (Fdb));
	
	if (fdb) {
		int ret =  pthread_mutex_init (&fdb->lock, NULL);
		if (ret != 0) {
			free (fdb);
			return NULL;
		}

		for (int i = 0; i < FDB_MAX_SIZE; i ++){
			memset (&(fdb->data[i]), 0, sizeof (FdbEntry));
		}
		return fdb;
	}
	return NULL;
}

void destroy_fdb (Fdb *fdb){
	if (!fdb) return;
	pthread_mutex_destroy (&fdb->lock);
	free (fdb);
}

void fdb_add_mac (Fdb *fdb, mac_addr_t dst, Port port){
	time_t oldest_time = time (NULL);
	int index = 0;
	FdbEntry *data = fdb->data;
	pthread_mutex_lock (&fdb->lock);

	for (int i = 0; i < FDB_MAX_SIZE; i ++){
		if (same_mac (dst, data[i].dst)){
			index = i;
			break;
		}
		if (oldest_time > data[i].lastseen){
			oldest_time = data[i].lastseen;
			index = i;
		};
	}
	(data[index]).dst = dst;
	(data[index]).port = port;
	(data[index]).lastseen = time (NULL);
	pthread_mutex_unlock (&fdb->lock);
}

FdbEntry fdb_resolve_mac (Fdb *fdb, mac_addr_t mac){
	FdbEntry *data = fdb->data;
	FdbEntry ret;
	ret.lastseen = 0;
	time_t t = time (NULL);
	pthread_mutex_lock (&fdb->lock);

	for (int i = 0; i < FDB_MAX_SIZE; i ++){
		if (t - data[i].lastseen > TOO_OLD){
			memset (&data[i], 0, sizeof (data[i]));
		}

		if (same_mac (mac, data[i].dst)) {
			ret = data[i];
			break;
		}
	}
	pthread_mutex_unlock (&fdb->lock);
	return ret;
}

bool fdb_should_relay (Fdb *fdb, mac_addr_t dst, Port port){
	FdbEntry *data = fdb->data;
	bool ret = false;
	pthread_mutex_lock (&fdb->lock);

	for (int i = 0; i < FDB_MAX_SIZE; i ++){
		if (same_mac (dst, data[i].dst) &&
				data[i].port.fd == port.fd && 
				data[i].port.status == PORT_STATUS_ACTIVE){
			ret = true;
			break;
		}
	}
	pthread_mutex_unlock (&fdb->lock);
	return ret;
}

FdbEntry **fdb_to_arry (Fdb *fdb, int *arry_size){
	FdbEntry *data = fdb->data;
	FdbEntry **ret = malloc (sizeof (FdbEntry *) * FDB_MAX_SIZE);

	if (!ret){
		*arry_size = 0;
		return NULL;
	}

	int size = 0;

	pthread_mutex_lock (&fdb->lock);
	for (int i = 0; i < FDB_MAX_SIZE; i ++){
		if (is_valid_fdbentry (&data[i])){
			ret[size ++] = &data[i];
		}
	}
	*arry_size = size;
	pthread_mutex_unlock (&fdb->lock);

	return ret;
}
