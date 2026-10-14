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

#include "mac_b_pic.hpp"

namespace iris::ipu {

VLC_Entry Macroblock_BPic::table[] =
{
    {0x2, 0x2000C, 2},
    {0x3, 0x2000E, 2},
    {0x2, 0x30004, 3},
    {0x3, 0x30006, 3},

    {0x2, 0x40008, 4},
    {0x3, 0x4000A, 4},
    {0x3, 0x50001, 5},
    {0x2, 0x5001E, 5},

    {0x3, 0x6001A, 6},
    {0x2, 0x60016, 6},
    {0x1, 0x60011, 6}
};

unsigned int Macroblock_BPic::index_table[6] =
{
    0,
    0,
    2,
    4,
    6,
    8,
};

Macroblock_BPic::Macroblock_BPic() : VLC_Table(table, SIZE, 6, index_table)
{

}

}
