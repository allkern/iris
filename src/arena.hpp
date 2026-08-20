#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace iris::arena {

struct Arena {
    std::vector <uint8_t> buf;
    size_t offset = 0;
};

Arena* create(size_t size);
void destroy(Arena* arena);

void reset(Arena* arena);
void* alloc(Arena* arena, size_t size);

}
