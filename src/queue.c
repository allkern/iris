#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include "queue.h"

struct queue_state* queue_create(void) {
    return malloc(sizeof(struct queue_state));
}

void queue_init(struct queue_state* queue) {
    memset(queue, 0, sizeof(struct queue_state));

    queue->cap = 2;
    queue->buf = malloc(queue->cap * sizeof(uint32_t));
}

void queue_push(struct queue_state* queue, uint32_t value) {
    if (queue->size == queue->cap) {
        queue->cap *= 2;
        
        uint32_t* buf = realloc(queue->buf, queue->cap * sizeof(uint32_t));

        if (!buf) {
            printf("queue: Couldn't allocate memory\n");

            exit(1);
        }

        queue->buf = buf;
    }

    queue->buf[queue->size++] = value;
}

uint32_t queue_pop(struct queue_state* queue) {
    if (queue->index == queue->size)
        return 0;

    return queue->buf[queue->index++];
}

uint32_t queue_peek(struct queue_state* queue) {
    if (queue->index == queue->size)
        return 0;

    return queue->buf[queue->index];
}

uint32_t queue_at(struct queue_state* queue, int idx) {
    return queue->buf[queue->index + idx];
}

int queue_is_empty(struct queue_state* queue) {
    return queue->index == queue->size;
}

int queue_size(struct queue_state* queue) {
    return queue->size;
}

void queue_clear(struct queue_state* queue) {
    queue->size = 0;
    queue->index = 0;
}

void queue_destroy(struct queue_state* queue) {
    free(queue->buf);
    free(queue);
}