#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace iris::queue {

// Reading advances an index instead of removing, so at() can address entries
// relative to the read position. clear() is what actually reclaims storage.
struct Queue {
    std::vector <uint32_t> buf;
    size_t index = 0;
};

Queue* create();
void destroy(Queue* queue);

void push(Queue* queue, uint32_t value);
uint32_t pop(Queue* queue);
uint32_t peek(const Queue* queue);
uint32_t at(const Queue* queue, int index);
bool is_empty(const Queue* queue);
size_t size(const Queue* queue);
void clear(Queue* queue);

}
