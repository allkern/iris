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
#include "vlc_table.hpp"

namespace iris::ipu {

struct RunLevelPair
{
    int run, level;
};

class DCT_Coeff : public VLC_Table
{
    protected:
        constexpr static int RUN_ESCAPE = 102;
    public:
        DCT_Coeff(VLC_Entry* table, int table_size, int max_bits, unsigned int* index_table);

        virtual bool get_end_of_block(IPU_FIFO& FIFO, uint32_t& result) = 0;
        virtual bool get_skip_block(IPU_FIFO& FIFO) = 0;
        virtual bool get_runlevel_pair(IPU_FIFO& FIFO, RunLevelPair& pair, bool MPEG1) = 0;
        virtual bool get_runlevel_pair_dc(IPU_FIFO& FIFO, RunLevelPair& pair, bool MPEG1) = 0;

        bool peek_value(IPU_FIFO& FIFO, int bits, int& bit_count, uint32_t& result);
};

}
