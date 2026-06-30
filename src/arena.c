#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "arena.h"

struct arena_state* arena_create() {
    return (struct arena_state*)malloc(sizeof(struct arena_state));
}

int arena_init(struct arena_state* arena, size_t size) {
    arena->base = (uint8_t*)malloc(size);

    if (!arena->base) {
        return 0;
    }

    arena->size = size;
    arena->offset = 0;

    return 1;
}

void* arena_alloc(struct arena_state* arena, size_t size) {
    if (arena->offset + size > arena->size) {
        arena->offset = 0;

        return NULL;
    }

    void* ptr = arena->base + arena->offset;

    arena->offset += size;

    return ptr;
}

void arena_destroy(struct arena_state* arena) {
    free(arena->base);
    free(arena);
}