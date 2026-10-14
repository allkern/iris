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
#include <vector>

#include "logger.hpp"
#include "u128.h"

namespace iris::iop::intc { struct Intc; }

namespace iris::sif {

struct Fifo {
    std::vector <uint128_t> data;
    int read_index = 0;
    int write_index = 0;
};

struct Sif {
    uint32_t mscom = 0;
    uint32_t smcom = 0;
    uint32_t msflg = 0;
    uint32_t smflg = 0;
    uint32_t ctrl = 0;
    uint32_t bd6 = 0;

    iop::intc::Intc* iop_intc = nullptr;

    Fifo sif0;
    Fifo sif1;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Sif* create(logger::Logger* logger);
void connect(Sif* sif, iop::intc::Intc* iop_intc);
void destroy(Sif* sif);
uint64_t read32(Sif* sif, uint32_t addr);
void write32(Sif* sif, uint32_t addr, uint64_t data);
void fifo_write(Fifo& fifo, uint128_t data);
uint128_t fifo_read(Fifo& fifo);
void fifo_reset(Fifo& fifo);
void reset(Sif* sif);
bool fifo_is_empty(const Fifo& fifo);

}
