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

#include <atomic>
#include <cstdio>
#include <ctime>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "logger.hpp"
#include "dev/rm.hpp"
#include "dvrfs.hpp"

namespace iris::speed { struct Speed; }

namespace iris::speed::dvrp {

inline constexpr auto INTR_READY = 0x01;
inline constexpr auto INTR_CMD_ACK = 0x02;
inline constexpr auto INTR_CMD_COMP = 0x04;
inline constexpr auto INTR_DMA_ACK = 0x08;
inline constexpr auto INTR_DMA_END = 0x10;

struct Response {
    uint16_t cause = 0;
    uint16_t stat = 0;
    uint16_t words[8] = {};
    int count = 0;
};

struct DvrFile {
    FILE* file = nullptr;
    dvrfs::Volume* volume = nullptr;
    dvrfs::Handle handle;
    bool is_dir = false;
    bool root = false;
    std::vector<std::string> entries;
    size_t index = 0;
    std::string path;
};

struct Dvrp {
    uint16_t cmd = 0;
    uint16_t params[0x10] = {};
    uint8_t param_index = 0;

    uint8_t dma_dir = 0;
    uint8_t reset_reg = 0;

    uint16_t intr_stat = 0;
    uint16_t intr_mask = 0;
    uint16_t intr_cause = 0;

    uint16_t reply[64] = {};
    int reply_count = 0;
    int reply_index = 0;

    std::deque<Response> responses;
    bool delivery_pending = false;

    uint16_t dma_cmd = 0;
    std::vector<uint8_t> dma_in;
    std::vector<uint8_t> dma_out;
    int32_t ret = 0;

    int file_fd = 0;
    uint32_t io_size = 0;
    std::map<int, DvrFile> files;
    std::map<std::string, dvrfs::Volume*> volumes;
    int next_fd = 1;
    std::string root;

    std::map<uint16_t, uint16_t> presets;
    uint8_t tv_channel = 0;
    bool rec_active = false;
    bool rec_paused = false;
    std::time_t rec_start = 0;

    std::atomic <dev::rm::Key> remote_key = dev::rm::Key {};
    std::atomic <uint32_t> remote_reads = 0;
    int remote_phase = 0;

    Speed* speed = nullptr;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Dvrp* create(logger::Logger* logger);
void init(Dvrp* dvrp, Speed* speed);
void destroy(Dvrp* dvrp);
uint64_t read(Dvrp* dvrp, uint32_t addr);
void write(Dvrp* dvrp, uint32_t addr, uint64_t data);
void read_dma(Dvrp* dvrp, uint8_t* buf, uint32_t size);
void write_dma(Dvrp* dvrp, const uint8_t* buf, uint32_t size);
void set_root(Dvrp* dvrp, const char* path);
void set_remote_key(Dvrp* dvrp, dev::rm::Key key);
void release_remote_key(Dvrp* dvrp);

}
