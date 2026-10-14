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

namespace iris::s14x::nand {

inline constexpr auto CMD_READ = 0x30;
inline constexpr auto CMD_ERASE = 0x60;
inline constexpr auto CMD_WRITE = 0x80;
inline constexpr auto CMD_READID = 0x90;

inline constexpr auto PAGE_SIZE_NOECC = 0x800;
inline constexpr auto PAGE_SIZE_ECC = 0x840;
inline constexpr auto PAGES_PER_BLOCK = 0x40;

inline constexpr auto STATE_READ_BYTE0 = 0;
inline constexpr auto STATE_READ_BYTE1 = 1;
inline constexpr auto STATE_READ_PAGE0 = 2;
inline constexpr auto STATE_READ_PAGE1 = 3;
inline constexpr auto STATE_READ_PAGE2 = 4;

inline constexpr auto REG_WAITFLAG = 0;
inline constexpr auto REG_ENABLE = 1;
inline constexpr auto REG_CMD = 2;
inline constexpr auto REG_OFFSET = 3;
inline constexpr auto REG_WRITE_UNLOCK = 4;
inline constexpr auto REG_OUTBYTE = 8;

struct Nand {
    FILE* file;
    int enable;
    uint8_t cmd;
    uint8_t* buf;
    int index;
    int size;

    uint16_t byte_offset;
    uint32_t page_offset;
    int state;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Nand* create(logger::Logger* logger);
int load(Nand* nand, const char* path);
uint64_t read8(Nand* nand, uint32_t addr);
void write8(Nand* nand, uint32_t addr, uint64_t data);
void destroy(Nand* nand);

}
