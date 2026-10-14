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

#include <string>
#include "logger.hpp"

namespace iris::s14x::sram {

inline constexpr auto SIZE = 0x8000;

struct Sram {
    std::string path;
    int* write_flag;
    uint8_t buf[SIZE];

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Sram* create(logger::Logger* logger, int* write_flag);
int load(Sram* sram, const char* path);
uint64_t read8(Sram* sram, uint32_t addr);
uint64_t read16(Sram* sram, uint32_t addr);
uint64_t read32(Sram* sram, uint32_t addr);
void write8(Sram* sram, uint32_t addr, uint64_t data);
void write16(Sram* sram, uint32_t addr, uint64_t data);
void write32(Sram* sram, uint32_t addr, uint64_t data);
void destroy(Sram* sram);

}
