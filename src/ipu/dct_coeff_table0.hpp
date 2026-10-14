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
#include "dct_coeff.hpp"

namespace iris::ipu {

class DCT_Coeff_Table0 : public DCT_Coeff
{
    private:
        static VLC_Entry table[];
        static RunLevelPair runlevel_table[];
        static unsigned int index_table[];

        constexpr static int SIZE = 112;
    public:
        DCT_Coeff_Table0();

        bool get_end_of_block(IPU_FIFO &FIFO, uint32_t &result);
        bool get_skip_block(IPU_FIFO &FIFO);
        bool get_runlevel_pair(IPU_FIFO &FIFO, RunLevelPair &pair, bool MPEG1);
        bool get_runlevel_pair_dc(IPU_FIFO &FIFO, RunLevelPair &pair, bool MPEG1);
};

}
