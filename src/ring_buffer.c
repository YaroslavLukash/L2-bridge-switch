#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <stdbool.h>

typedef struct RingBuffer {
	size_t consumer_ptr;
	size_t publisher_ptr;

	size_t slot_size;
	size_t number_of_slots;

	pthread_mutex_t lock;
	sem_t empty;
	sem_t full;
	
	char *buffer;

	bool shutdown;
} RingBuffer;

RingBuffer *make_RingBuffer (size_t slot_size, size_t number_of_slots){
	struct RingBuffer *rbuffer = malloc (sizeof (RingBuffer));
	if (!rbuffer) goto fail;

	slot_size += sizeof (size_t);

	rbuffer->buffer = malloc (slot_size * number_of_slots);
	if (!rbuffer->buffer) goto fail_cleanup_rbuffer;

	if (sem_init (&rbuffer->empty, 0, number_of_slots) != 0) goto fail_cleanup_buffer;
	if (sem_init (&rbuffer->full, 0, 0) != 0) goto fail_destroy_empty;
	if (pthread_mutex_init (&rbuffer->lock, NULL) != 0) goto fail_destroy_full;

	rbuffer->consumer_ptr = 0;
	rbuffer->publisher_ptr = 0;

	rbuffer->slot_size = slot_size;
	rbuffer->number_of_slots = number_of_slots;

	rbuffer->shutdown = false;

	return rbuffer;

fail_destroy_full:
	sem_destroy (&rbuffer->full);
fail_destroy_empty:
	sem_destroy (&rbuffer->empty);
fail_cleanup_buffer:
	free (rbuffer->buffer);
fail_cleanup_rbuffer:
	free (rbuffer);
fail:
	return NULL;
}

void shutdown_RingBuffer (RingBuffer *rbuffer){
	if (!rbuffer) return;

	while (pthread_mutex_lock (&rbuffer->lock) != 0);
	rbuffer->shutdown = true;
	pthread_mutex_unlock (&rbuffer->lock);

	sem_post (&rbuffer->full);
	sem_post (&rbuffer->empty);
}

void destroy_RingBuffer (RingBuffer *rbuffer){
	if (!rbuffer) return;

	pthread_mutex_destroy (&rbuffer->lock);
	sem_destroy (&rbuffer->empty);
	sem_destroy (&rbuffer->full);

	free (rbuffer->buffer);
	free (rbuffer);
}

void flush_RingBuffer (RingBuffer *rbuffer){
	sem_destroy (&rbuffer->full);
	sem_destroy (&rbuffer->empty);

	rbuffer->shutdown = false;
	rbuffer->consumer_ptr = 0;
	rbuffer->publisher_ptr = 0;

	assert (sem_init (&rbuffer->full, 0, 0) == 0 &&
			sem_init (&rbuffer->empty, 0, rbuffer->number_of_slots) == 0);
}

void RingBuffer_publish (RingBuffer *rbuffer, void *msg, size_t size){
	assert (rbuffer && msg && size <= rbuffer->slot_size - sizeof (size_t) && "rbuffer is null or msg or msg size is to big");

	while (sem_wait (&rbuffer->empty) != 0);

	if (rbuffer->shutdown){
		sem_post (&rbuffer->empty);
		return;
	}

	while (pthread_mutex_lock (&rbuffer->lock) != 0);

	char *buffer = rbuffer->buffer + (rbuffer->publisher_ptr * rbuffer->slot_size);
	*(size_t *)buffer = size;
	buffer += sizeof (size_t);

	memcpy (buffer, msg, size);
	rbuffer->publisher_ptr = (rbuffer->publisher_ptr + 1) % rbuffer->number_of_slots;

	pthread_mutex_unlock (&rbuffer->lock);
	sem_post (&rbuffer->full);
}

void RingBuffer_consume (RingBuffer *rbuffer, void *dst){
	assert (rbuffer && dst && "rbuffer is null or dst is null");

	while (sem_wait (&rbuffer->full) != 0);

	if (rbuffer->shutdown){
		sem_post (&rbuffer->full);
		return;
	}

	while (pthread_mutex_lock (&rbuffer->lock) != 0);

	char *buffer = rbuffer->buffer + (rbuffer->consumer_ptr * rbuffer->slot_size);
	size_t size = *(size_t *)buffer;
	buffer += sizeof (size_t);

	memcpy (dst, buffer, size);
	rbuffer->consumer_ptr = (rbuffer->consumer_ptr + 1) % rbuffer->number_of_slots;

	pthread_mutex_unlock (&rbuffer->lock);
	sem_post (&rbuffer->empty);
}
