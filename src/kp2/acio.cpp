#include <cstring>

#include "acio.hpp"

namespace iris::kp2::acio {

static uint8_t checksum(const uint8_t* data, int size) {
    uint8_t sum = 0;

    for (int i = 0; i < size; i++) {
        sum += data[i];
    }

    return sum;
}

static void push(Port* port, uint8_t value) {
    if (port->tx_size >= FRAME_MAX)
        return;

    port->tx[port->tx_size++] = value;
}

static void push_escaped(Port* port, uint8_t value) {
    if (value == SYNC || value == ESCAPE) {
        push(port, ESCAPE);
        push(port, value ^ 0xff);

        return;
    }

    push(port, value);
}

static void send(Port* port, const Request* request, const Response* response) {
    uint8_t header[5] = {
        request->address,
        (uint8_t)(request->code >> 8),
        (uint8_t)(request->code & 0xff),
        request->sequence,
        response->length
    };

    push(port, SYNC);

    for (int i = 0; i < 5; i++) {
        push_escaped(port, header[i]);
    }

    for (int i = 0; i < response->length; i++) {
        push_escaped(port, response->payload[i]);
    }

    uint8_t sum = checksum(header, sizeof(header)) + checksum(response->payload, response->length);

    push_escaped(port, sum);
}

static void handle_request(Port* port, const Request* request) {
    Response response = {};

    if (request->address == BROADCAST_ADDRESS && request->code == CODE_ASSIGN_ADDRESS) {
        response.payload[0] = (uint8_t)port->node_count;
        response.length = 1;

        send(port, request, &response);

        return;
    }

    int index = request->address - 1;

    if (index < 0 || index >= port->node_count) {
        // iris_debug(port, "Request for unknown node {:02x} (code {:04x})", request->address, request->code);

        return;
    }

    const Node* node = &port->nodes[index];

    if (!node->handler(node->udata, request, &response)) {
        // iris_debug(port, "Node \"{}\" rejected code {:04x}", node->name, request->code);

        return;
    }

    send(port, request, &response);
}

static void consume_frame(Port* port) {
    uint8_t frame[FRAME_MAX];
    int size = 0;

    for (int i = 1; i < port->rx_size; i++) {
        uint8_t value = port->rx[i];

        if (value == ESCAPE) {
            if (++i >= port->rx_size)
                return;

            value = port->rx[i] ^ 0xff;
        }

        frame[size++] = value;
    }

    if (size < 6)
        return;

    Request request = {};

    request.address = frame[0];
    request.code = (frame[1] << 8) | frame[2];
    request.sequence = frame[3];
    request.length = frame[4];

    if (size < 5 + request.length + 1)
        return;

    memcpy(request.payload, frame + 5, request.length);

    handle_request(port, &request);
}

void init(Port* port, logger::Logger* logger, size_t logger_id) {
    port->logger = logger;
    port->logger_id = logger_id;

    port->node_count = 0;

    reset(port);
}

void reset(Port* port) {
    port->rx_size = 0;
    port->tx_size = 0;
    port->tx_read = 0;
}

void register_node(Port* port, node_handler handler, void* udata, const char* name) {
    if (port->node_count >= NODE_MAX)
        return;

    Node* node = &port->nodes[port->node_count++];

    node->handler = handler;
    node->udata = udata;
    node->name = name;
}

void write(Port* port, const uint8_t* data, int size) {
    for (int i = 0; i < size; i++) {
        uint8_t value = data[i];

        if (value == SYNC) {
            if (port->rx_size > 1) {
                consume_frame(port);
            }

            port->rx_size = 0;
        }

        if (port->rx_size >= FRAME_MAX) {
            port->rx_size = 0;

            continue;
        }

        port->rx[port->rx_size++] = value;
    }

    if (port->rx_size > 1) {
        consume_frame(port);

        port->rx_size = 0;
    }
}

int read(Port* port, uint8_t* data, int size) {
    int available = port->tx_size - port->tx_read;

    if (available <= 0) {
        port->tx_size = 0;
        port->tx_read = 0;

        return 0;
    }

    if (available > size)
        available = size;

    memcpy(data, port->tx + port->tx_read, available);

    port->tx_read += available;

    if (port->tx_read >= port->tx_size) {
        port->tx_size = 0;
        port->tx_read = 0;
    }

    return available;
}

int pending(Port* port) {
    return port->tx_size - port->tx_read;
}

}
