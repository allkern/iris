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
#include <queue>
#include "ipu_fifo.hpp"

namespace iris::ipu {

struct VLC_Entry
{
    uint32_t key;
    uint32_t value;
    uint8_t bits;
};

class VLC_Table
{
    private:
        static constexpr int LUT_BITS = 10;

        VLC_Entry* table;
        int table_size, max_bits;
        unsigned int* index_table;
        int lut_bits;
        int16_t lut[1 << LUT_BITS];

        int search(uint32_t window, int window_bits, int first, int last) const;
    protected:
        VLC_Table(VLC_Entry* table, int table_size, int max_bits, unsigned int* index_table);
    public:
        bool peek_symbol(IPU_FIFO& FIFO, VLC_Entry& entry, bool* invalid = nullptr);
        bool get_symbol(IPU_FIFO& FIFO, uint32_t& result, bool* invalid = nullptr);
};

}
