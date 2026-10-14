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
#include <cstdio>

#include "iop/sio2.hpp"
#include "logger.hpp"

namespace iris::dev::ps1_mcd {

inline constexpr uint32_t SECTOR_SIZE = 128;
inline constexpr uint32_t SIZE = 0x20000;

struct Ps1Mcd {
    // 128 KiB
    uint8_t buf[SIZE] = {};
    uint8_t flag = 0;
    int type = 0;

    FILE* file = nullptr;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Ps1Mcd* attach(logger::Logger* logger, sio2::Sio2* sio2, int port, const char* path);
void set_type(Ps1Mcd* mcd, int type);
void reset(void* udata);
void detach(void* udata);

}
