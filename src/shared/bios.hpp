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

#include "logger.hpp"
#include "u128.h"

namespace iris::bios {

struct Bios {
    uint8_t* buf = nullptr;

    // Really a mask, not a length: reads are addr & size
    size_t size = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Bios* create(logger::Logger* logger);
bool load(Bios* bios, const char* path);
void destroy(Bios* bios);

uint64_t read8(Bios* bios, uint32_t addr);
uint64_t read16(Bios* bios, uint32_t addr);
uint64_t read32(Bios* bios, uint32_t addr);
uint64_t read64(Bios* bios, uint32_t addr);
uint128_t read128(Bios* bios, uint32_t addr);
void write8(Bios* bios, uint32_t addr, uint64_t data);
void write16(Bios* bios, uint32_t addr, uint64_t data);
void write32(Bios* bios, uint32_t addr, uint64_t data);
void write64(Bios* bios, uint32_t addr, uint64_t data);
void write128(Bios* bios, uint32_t addr, uint128_t data);

}
