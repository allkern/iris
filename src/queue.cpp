#include "queue.hpp"

namespace iris::queue {

Queue* create() {
    Queue* queue = new Queue();

    queue->buf.reserve(4);

    return queue;
}

void destroy(Queue* queue) {
    delete queue;
}

void push(Queue* queue, uint32_t value) {
    queue->buf.push_back(value);
}

uint32_t pop(Queue* queue) {
    if (queue->index == queue->buf.size())
        return 0;

    return queue->buf[queue->index++];
}

uint32_t peek(const Queue* queue) {
    if (queue->index == queue->buf.size())
        return 0;

    return queue->buf[queue->index];
}

uint32_t at(const Queue* queue, int index) {
    return queue->buf[queue->index + index];
}

bool is_empty(const Queue* queue) {
    return queue->index == queue->buf.size();
}

size_t size(const Queue* queue) {
    return queue->buf.size();
}

void clear(Queue* queue) {
    queue->buf.clear();

    queue->index = 0;
}

}
