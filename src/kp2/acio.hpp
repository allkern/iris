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

#include <cstdint>

#include "logger.hpp"

namespace iris::kp2::acio {

inline constexpr auto SYNC = 0xaa;
inline constexpr auto ESCAPE = 0xff;

inline constexpr auto BROADCAST_ADDRESS = 0x00;
inline constexpr auto RESPONSE_FLAG = 0x80;
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

    int sync_run;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

void init(Port* port, logger::Logger* logger, size_t logger_id);
void reset(Port* port);
void register_node(Port* port, node_handler handler, void* udata, const char* name);

void write(Port* port, const uint8_t* data, int size);
int read(Port* port, uint8_t* data, int size);
int pending(Port* port);
void queue(Port* port, const uint8_t* data, int size);

}
