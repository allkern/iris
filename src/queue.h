#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct queue_state {
    uint32_t* buf;

    unsigned int cap;
    unsigned int size;
    unsigned int index;
};

struct queue_state* queue_create(void);
void queue_init(struct queue_state* queue);
void queue_push(struct queue_state* queue, uint32_t value);
uint32_t queue_pop(struct queue_state* queue);
uint32_t queue_peek(struct queue_state* queue);
uint32_t queue_at(struct queue_state* queue, int idx);
int queue_is_empty(struct queue_state* queue);
int queue_size(struct queue_state* queue);
void queue_clear(struct queue_state* queue);
void queue_destroy(struct queue_state* queue);

#ifdef __cplusplus
}
#endif

#endif