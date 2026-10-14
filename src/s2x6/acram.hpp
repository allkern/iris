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

#include "logger.hpp"

namespace iris::s2x6::acram {

inline constexpr auto BASE_ADDR = 0x14000000;

inline constexpr auto BANK_SIZE = 0x2000000;
inline constexpr auto NUM_BANKS = 4;
inline constexpr auto SIZE = BANK_SIZE * NUM_BANKS;

inline constexpr auto BANK_SHIFT = 21;
inline constexpr auto BANK_MASK = 0x1fffff;

inline constexpr auto R_READ_POINTER = 0x60000;
inline constexpr auto R_WRITE_POINTER = 0x70000;
inline constexpr auto R_POINTER_END = 0x80000;
inline constexpr auto R_CONFIG = 0x20000;
inline constexpr auto R_STATUS_END = 0x20;

inline constexpr auto STATUS_READY = 0x50;

struct Bank {
    uint32_t read_addr;
    uint32_t write_addr;
};

struct Acram {
    uint8_t* buf;

    Bank banks[NUM_BANKS];

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Acram* create(logger::Logger* logger, int has_ram);
void destroy(Acram* acram);
bool is_dma_target(uint32_t target);
int bank_from_dma_target(uint32_t target);
uint32_t dma_read32(Acram* acram, int bank);
void dma_write32(Acram* acram, int bank, uint32_t data);
uint64_t read16(Acram* acram, uint32_t addr);
void write16(Acram* acram, uint32_t addr, uint64_t data);
}
