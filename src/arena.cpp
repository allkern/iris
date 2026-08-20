#include "arena.hpp"

namespace iris::arena {

Arena* create(size_t size) {
    Arena* arena = new Arena();

    arena->buf.resize(size);

    return arena;
}

void destroy(Arena* arena) {
    delete arena;
}

void reset(Arena* arena) {
    arena->offset = 0;
}

void* alloc(Arena* arena, size_t size) {
    if (arena->offset + size > arena->buf.size()) {
        arena->offset = 0;

        return nullptr;
    }

    void* ptr = arena->buf.data() + arena->offset;

    arena->offset += size;

    return ptr;
}

}
