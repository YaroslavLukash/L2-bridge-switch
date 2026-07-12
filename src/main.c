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

int main_run (Bridge *bridge);
int main_exit (Bridge *bridge);
int main_add (Bridge *bridge, char **args);
int main_remove (Bridge *bridge, char **args);
int main_stop (Bridge *bridge);
void main_help (void);
int main_show (Bridge *bridge, char **args);

int main (int argc, char *argv[]){
	Bridge bridge;

	bridge.fdb = make_fdb ();
	if (!bridge.fdb) {
		fprintf (stderr, "could not allocate fdb\n");
		goto fail;
	}

	bridge.rbuffer = make_RingBuffer (sizeof (to_mac_relay), 50);
	if (!bridge.rbuffer) {
		fprintf (stderr, "could not allocate ringbuffer\n");
	   	goto fail_free_fdb;
	}

	bridge.ethports = make_ethports ();
	if (!bridge.ethports) {
		fprintf (stderr, "could not allocate ethports\n");
	   	goto fail_free_rbuffer;
	}
	bridge.is_running = false;

	if (argc > 1){
		for (int i = 1; i < argc; i++){
			int ret = add_port (&bridge, argv[i]);
			if (ret == 0) printf ("interface %s was added\n", argv[i]);
		}
	}

	char line[128];
	while (1){
		printf ("bridge > ");
		if (fgets (line, sizeof (line), stdin) == NULL) continue;
		char *rest = line;
		char *word = strtok_r (line, " \n", &rest);

		if (word == NULL) continue;

		else if (strcmp (word, "run") == 0){
			main_run (&bridge);
		}
		else if (strcmp (word, "stop") == 0){
			main_stop (&bridge);
		}
		else if (strcmp (word, "exit") == 0){
			main_exit (&bridge);
		}
		else if (strcmp (word, "add") == 0){
			main_add (&bridge, &rest);	
		}
		else if (strcmp (word, "remove") == 0 ||
				strcmp (word, "rm") == 0){
			main_remove (&bridge, &rest);
		}
		else if (strcmp (word, "help") == 0 ||
				strcmp (word, "?") == 0){
			main_help ();
		}
		else if (strcmp (word, "show") == 0){
			main_show (&bridge, &rest);
		}
		else {
			fprintf (stderr, "unknown command %s\n", word);
		}
	}
	return 0;
fail_free_rbuffer:
	destroy_RingBuffer (bridge.rbuffer);
fail_free_fdb:
	destroy_fdb (bridge.fdb);
fail:
	return 0;
}

void main_help (void) {
    printf("\n==================== Available Commands ====================\n");
    printf("  run               - Starts the bridge processing engine.\n");
    printf("  stop              - Stops the running bridge engine.\n");
    printf("  add <iface...>    - Adds one or more network interfaces (e.g., add eth0 eth1).\n");
    printf("  remove/rm <iface> - Removes network interfaces from the configuration.\n");
    printf("  help / ?          - Displays this help menu.\n");
    printf("  exit              - Stops the engine and gracefully terminates the program.\n");
    printf("  show fdb [n]      - Shows n entitys of fdb\n");
    printf("============================================================\n\n");
}

int main_run (Bridge *bridge){
	if (bridge->is_running) {
		fprintf (stderr, "bridge is already running\n");
		return -1;
	}

	int size = 0;
	Port **arr = to_portarry (bridge, &size);

	int created_threads = 0;
	pthread_t *threads = malloc (sizeof (pthread_t) * (size + 1));

	if (!threads){
		fprintf (stderr, "could not allocate threads\n");
		goto fail_free_arr;
	}

	if (size <= 1){
		fprintf (stderr, "can't start bridge only with %d ports\n", size);
		goto fail_free_threads;
	}

	bridge->is_running = true;
	for (int i = 0; i < size; i ++){
		int ret = pthread_create (&threads[i], NULL, mac_entity, arr[i]);
		if (ret != 0){
			fprintf (stderr, "could not create thread\n");
			goto fail_close_mac_entity;
		}
		created_threads ++;
	}
	int ret = pthread_create (&threads[size], NULL, mac_relay, bridge);
	if (ret != 0){
		fprintf (stderr, "could not create thread\n");
		goto fail_close_mac_entity;
	}
	created_threads ++;

	bridge->threads = threads;
	bridge->size = created_threads;
	return 0;

fail_close_mac_entity:
	bridge->is_running = false;
	for (int i = 0; i < created_threads; i ++){
		pthread_join (threads[i], NULL);
	}
	
fail_free_threads:
	free (threads);
	
fail_free_arr:
	free (arr);

return -1;
	
}
int main_exit (Bridge *bridge){
	if (bridge->is_running) main_stop (bridge);

	int size = 0;
	Port **arr = to_portarry (bridge, &size);

	for (int i = 0; i < size; i ++){
		close (arr[i]->fd);
	}
	free (arr);

	destroy_fdb (bridge->fdb);
	destroy_RingBuffer (bridge->rbuffer);
	destroy_ethports (bridge);

	exit (EXIT_SUCCESS);
	return 0;
}

int main_add (Bridge *bridge, char **args){
	char *word;
	if (bridge->is_running){
		fprintf (stderr, "can't add interface to a running engine\n");
		return -1;
	}

	while ((word = strtok_r (NULL, " \n", args))){
		int ret = add_port (bridge, word);
		if (ret == 0) printf ("interface %s was added\n", word);
	}
	return 0;
}

int main_remove (Bridge *bridge, char **args){
	char *word;
	if (bridge->is_running){
		fprintf (stderr, "can't remove interface to a running engine\n");
		return -1;
	}

	while ((word = strtok_r (NULL, " \n", args))){
		int ret = remove_port (bridge, word);
		if (ret == 0) printf ("interface %s was removed\n", word);
	}
	return 0;
}

int main_stop (Bridge *bridge){
	if (!bridge->is_running){
		fprintf (stderr, "bridge was not running\n");
		return -1;
	}
	if (!bridge->threads) return -1;

	bridge->is_running = false;
	shutdown_RingBuffer (bridge->rbuffer);

	for (int i = 0; i < bridge->size; i ++){
		pthread_join (bridge->threads[i], NULL);
	}

	free (bridge->threads);
	bridge->threads = NULL;
	bridge->size = 0;
	flush_RingBuffer (bridge->rbuffer); 

	return 0;
}


#define SHOW_DEFAULT_NUMBER 10

void print_fdb_entry (FdbEntry *entry){
	printf ("%02x %02x %02x %02x %02x %02x ",entry->dst.mac[0], entry->dst.mac[1], entry->dst.mac[2], entry->dst.mac[3], entry->dst.mac[4], entry->dst.mac[5]);

	printf ("%s ", entry->port.name);
	printf ("%d\n", time (NULL) - entry->lastseen);
}

int main_show (Bridge *bridge, char **args){
	char *operand = strtok_r (NULL, " \n", args);
	if (operand == NULL) {
		fprintf (stderr, "wrong use of show command.\n");
		return -1;
	}

	char *number = strtok_r (NULL, " \n", args);
	bool is_numeric = true;

	if (number != NULL){
		for (int i = 0; number[i] != '\0'; i++){
			if (!(number[i] >= '0' && number[i] <= '9')){
				is_numeric = false;
				fprintf (stderr, "show: %s is not a number default value is used instead\n", number);
				break;
			}
		}
	}

	int show = 10;
	if (number != NULL && is_numeric) show = atoi (number);

	if (strcmp (operand, "fdb") == 0){
		int size;
		FdbEntry **arr = fdb_to_arry(bridge->fdb, &size);

		if (arr == NULL){
			fprintf (stderr, "could not allocate memmory for show. please try again\n");
			return -1;
		}
		printf ("MAC address       Interface Lastseen\n");
		for (int i = 0; i < size; i ++){
			print_fdb_entry (arr[i]);
			if (i == show) break;
		}

		free (arr);
		return 0;
	}
	else {
		fprintf (stderr, "show is not supported for %s\n", operand);
	}
}
