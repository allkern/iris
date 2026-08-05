#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace iris::arena {

// Bump allocator over one fixed block. Individual allocations are never
// released; reset() reclaims the whole arena at once.
struct Arena {
    std::vector <uint8_t> buf;
    size_t offset = 0;
};

Arena* create(size_t size);
void destroy(Arena* arena);

void reset(Arena* arena);
void* alloc(Arena* arena, size_t size);

}
