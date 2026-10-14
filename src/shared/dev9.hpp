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

namespace iris::dev9 {

enum class Model : uint16_t {
    PCMCIA = 0x20, // CXD9566
    EXPBAY = 0x32  // CXD9611
};

inline constexpr auto REG_BASE = 0x1f801460;
inline constexpr auto REG_COUNT = 16;

struct Dev9 {
    uint16_t regs[REG_COUNT] = { 0 };
    uint16_t rev = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Dev9* create(logger::Logger* logger, Model model);
void destroy(Dev9* dev9);

uint64_t read8(Dev9* dev9, uint32_t addr);
uint64_t read16(Dev9* dev9, uint32_t addr);
uint64_t read32(Dev9* dev9, uint32_t addr);
void write8(Dev9* dev9, uint32_t addr, uint64_t data);
void write16(Dev9* dev9, uint32_t addr, uint64_t data);
void write32(Dev9* dev9, uint32_t addr, uint64_t data);

}
