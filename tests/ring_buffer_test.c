#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "mybridge.h"

#define TEST_SLOTS 5
#define MSG_SIZE 256
#define ITERATIONS_PER_THREAD 50
#define PRODUCERS 3
#define CONSUMERS 3

const char *TEST_MSG = "Hello Ring Buffer!";

typedef struct {
    RingBuffer *rb;
    int thread_id;
	bool *done;
} thread_arg_t;

// 1. Producer Worker
void *stress_producer(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    char msg[MSG_SIZE];

    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        snprintf(msg, MSG_SIZE, "%s From Prod %d (Seq %d)", TEST_MSG, targ->thread_id, i);
        RingBuffer_publish(targ->rb, msg, strlen(msg) + 1);
    }
    return NULL;
}

// 2. Consumer Worker
void *stress_consumer(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    char target_buffer[MSG_SIZE];
    int received_count = 0;

    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        RingBuffer_consume(targ->rb, target_buffer);
        
        if (*targ->done) {
            break;
        }

        assert(strncmp(target_buffer, TEST_MSG, strlen(TEST_MSG)) == 0);
        received_count++;
    }
    
    printf("  [Consumer %d] Got %d messages.\n", targ->thread_id, received_count);
    return NULL;
}

// 3. Blocked Consumer Worker (Shutdown test)
void *blocked_consumer(void *arg) {
    RingBuffer *rb = (RingBuffer *)arg;
    char target_buffer[MSG_SIZE];
    
    printf("  [Shutdown Thread] Waiting on empty buffer...\n");
    RingBuffer_consume(rb, target_buffer);
    printf("  [Shutdown Thread] Unblocked successfully!\n");
    
    return NULL;
}

int main() {
	bool done = false;

    printf("Running RingBuffer Tests\n");

    // TEST 1: Wrap-Around Mechanics
    printf("[Test 1] Checking array wrap-around...\n");
    RingBuffer *rb = make_RingBuffer(MSG_SIZE, TEST_SLOTS);
    assert(rb != NULL);

    char dump_buf[MSG_SIZE];
    for (int cycle = 0; cycle < 2; cycle++) {
        for (int i = 0; i < TEST_SLOTS; i++) {
            RingBuffer_publish(rb, (void *)TEST_MSG, strlen(TEST_MSG) + 1);
        }
        for (int i = 0; i < TEST_SLOTS; i++) {
            RingBuffer_consume(rb, dump_buf);
            assert(strcmp(dump_buf, TEST_MSG) == 0);
        }
    }
    destroy_RingBuffer(rb);
    printf("-> Wrap-around OK.\n");

    // TEST 2: Multi-threaded Stress Test
    printf("[Test 2] Running multi-thread stress test (%d prods / %d cons)...\n", PRODUCERS, CONSUMERS);
    rb = make_RingBuffer(MSG_SIZE, TEST_SLOTS);
    
    pthread_t producers[PRODUCERS];
    pthread_t consumers[CONSUMERS];
    thread_arg_t args[PRODUCERS > CONSUMERS ? PRODUCERS : CONSUMERS];

    for (int i = 0; i < CONSUMERS; i++) {
        args[i].rb = rb;
        args[i].thread_id = i + 1;
		args[i].done = &done;
        pthread_create(&consumers[i], NULL, stress_consumer, &args[i]);
    }

    for (int i = 0; i < PRODUCERS; i++) {
        args[i].rb = rb;
        args[i].thread_id = i + 1;
		args[i].done = &done;
        pthread_create(&producers[i], NULL, stress_producer, &args[i]);
    }

    for (int i = 0; i < PRODUCERS; i++) pthread_join(producers[i], NULL);
    for (int i = 0; i < CONSUMERS; i++) pthread_join(consumers[i], NULL);
    
    destroy_RingBuffer(rb);
    printf("-> Concurrency OK.\n");

    // TEST 3: Shutdown Unblocking
    printf("[Test 3] Testing shutdown unblocking...\n");
    rb = make_RingBuffer(MSG_SIZE, TEST_SLOTS);
    
    pthread_t test_consumer;
    pthread_create(&test_consumer, NULL, blocked_consumer, rb);

    usleep(100000); 

    printf("[Main] Triggering shutdown...\n");
    shutdown_RingBuffer(rb);

    pthread_join(test_consumer, NULL);
    destroy_RingBuffer(rb);
    printf("-> Shutdown OK.\n");
    return 0;
}
