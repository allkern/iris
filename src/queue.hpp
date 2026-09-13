#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "u128.h"

// Note: These are in the header because there are files that use queues
//       within hot paths, we don't want them having to pay the cost
//       of a cross-TU call just for some queue stuff

namespace iris::queue {

struct Queue {
    std::vector <uint32_t> buf;
    size_t index = 0;
};

Queue* create();
void destroy(Queue* queue);

inline void push(Queue* queue, uint32_t value) {
    queue->buf.push_back(value);
}

inline void push128(Queue* queue, uint128_t value) {
    uint32_t words[4];

    memcpy(words, &value, sizeof(words));

    queue->buf.push_back(words[0]);
    queue->buf.push_back(words[1]);
    queue->buf.push_back(words[2]);
    queue->buf.push_back(words[3]);
}

inline void push_words(Queue* queue, const uint8_t* data, size_t words) {
    size_t size = queue->buf.size();

    queue->buf.resize(size + words);

    memcpy(queue->buf.data() + size, data, words * sizeof(uint32_t));
}

inline uint32_t pop(Queue* queue) {
    if (queue->index == queue->buf.size())
        return 0;

    return queue->buf[queue->index++];
}

inline uint32_t peek(const Queue* queue) {
    if (queue->index == queue->buf.size())
        return 0;

    return queue->buf[queue->index];
}

inline uint32_t at(const Queue* queue, int index) {
    return queue->buf[queue->index + index];
}

inline bool is_empty(const Queue* queue) {
    return queue->index == queue->buf.size();
}

inline size_t size(const Queue* queue) {
    return queue->buf.size();
}

inline void clear(Queue* queue) {
    queue->buf.clear();

    queue->index = 0;
}

}
