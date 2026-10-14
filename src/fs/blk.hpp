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

namespace iris::speed::ata {

struct Hdd;

}

namespace iris::iop::disc {

struct Disc;

}

namespace iris::fs {

size_t get_logger_id(logger::Logger* logger);

}

namespace iris::fs::blk {

struct Device {
    int64_t (*read)(void* udata, uint64_t offset, void* buf, uint64_t size);
    uint64_t (*get_size)(void* udata);
    void (*close)(void* udata);

    void* udata;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Device* open_file(logger::Logger* logger, const char* path);
Device* open_memory(logger::Logger* logger, uint8_t* buf, uint64_t size, bool owns);
Device* open_slice(logger::Logger* logger, Device* parent, uint64_t offset, uint64_t size, bool owns_parent);
Device* open_ecc(logger::Logger* logger, Device* parent, uint32_t data_size, uint32_t ecc_size, bool owns_parent);
Device* open_ata(logger::Logger* logger, speed::ata::Hdd* hdd);
Device* open_disc(logger::Logger* logger, iop::disc::Disc* disc);

int64_t read(Device* dev, uint64_t offset, void* buf, uint64_t size);
uint64_t get_size(Device* dev);
void close(Device* dev);

}
