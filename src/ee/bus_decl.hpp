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

#include "u128.h"
#include "logger.hpp"

namespace iris::ee::bus {

struct Bus;

Bus* create(logger::Logger* logger);
void destroy(Bus* bus);

uint64_t read8(void* udata, uint32_t addr);
uint64_t read16(void* udata, uint32_t addr);
uint64_t read32(void* udata, uint32_t addr);
uint64_t read64(void* udata, uint32_t addr);
uint128_t read128(void* udata, uint32_t addr);
void write8(void* udata, uint32_t addr, uint64_t data);
void write16(void* udata, uint32_t addr, uint64_t data);
void write32(void* udata, uint32_t addr, uint64_t data);
void write64(void* udata, uint32_t addr, uint64_t data);
void write128(void* udata, uint32_t addr, uint128_t data);

}
