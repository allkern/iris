#include "queue.hpp"

namespace iris::queue {

Queue* create() {
    Queue* queue = new Queue();

    queue->buf.reserve(256);

    return queue;
}

void destroy(Queue* queue) {
    delete queue;
}

}
