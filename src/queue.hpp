/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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
    const uint32_t* first = (const uint32_t*)data;

    queue->buf.insert(queue->buf.end(), first, first + words);
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
