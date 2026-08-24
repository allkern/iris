#pragma once

#include <cstdint>

#include "logger.hpp"

namespace iris::kp2::acio {

inline constexpr auto SYNC = 0xaa;
inline constexpr auto ESCAPE = 0xff;

inline constexpr auto BROADCAST_ADDRESS = 0x00;
inline constexpr auto CODE_ASSIGN_ADDRESS = 0x0001;

inline constexpr auto PAYLOAD_MAX = 256;
inline constexpr auto FRAME_MAX = 512;
inline constexpr auto NODE_MAX = 8;

struct Request {
    uint8_t address;
    uint16_t code;
    uint8_t sequence;

    uint8_t payload[PAYLOAD_MAX];
    uint8_t length;
};

struct Response {
    uint8_t payload[PAYLOAD_MAX];
    uint8_t length;
};

typedef bool (*node_handler)(void* udata, const Request* request, Response* response);

struct Node {
    node_handler handler;
    void* udata;

    const char* name;
};

struct Port {
    Node nodes[NODE_MAX];
    int node_count;

    uint8_t rx[FRAME_MAX];
    int rx_size;

    uint8_t tx[FRAME_MAX];
    int tx_size;
    int tx_read;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

void init(Port* port, logger::Logger* logger, size_t logger_id);
void reset(Port* port);
void register_node(Port* port, node_handler handler, void* udata, const char* name);

void write(Port* port, const uint8_t* data, int size);
int read(Port* port, uint8_t* data, int size);
int pending(Port* port);

}
