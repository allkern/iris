#ifndef ARENA_H
#define ARENA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

struct arena_state {
    uint8_t* base;
    size_t size;
    size_t offset;
};

struct arena_state* arena_create();
int arena_init(struct arena_state* arena, size_t size);
void arena_reset(struct arena_state* arena);
void* arena_alloc(struct arena_state* arena, size_t size);
void arena_destroy(struct arena_state* arena);

#ifdef __cplusplus
}
#endif

#endif