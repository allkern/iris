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

#include "motioncode.hpp"

namespace iris::ipu {

VLC_Entry MotionCode::table[] =
{
    {0x1, 0x10000, 1},
    {0x2, 0x30001, 3},
    {0x3, 0x3FFFF, 3},
    {0x2, 0x40002, 4},

    {0x3, 0x4FFFE, 4},
    {0x2, 0x50003, 5},
    {0x3, 0x5FFFD, 5},
    {0x6, 0x70004, 7},

    {0x7, 0x7FFFC, 7},
    {0xA, 0x80005, 8},
    {0xB, 0x8FFFB, 8},
    {0x8, 0x80006, 8},

    {0x9, 0x8FFFA, 8},
    {0x6, 0x80007, 8},
    {0x7, 0x8FFF9, 8},
    {0x16, 0xA0008, 10},

    {0x17, 0xAFFF8, 10},
    {0x14, 0xA0009, 10},
    {0x15, 0xAFFF7, 10},
    {0x12, 0xA000A, 10},

    {0x13, 0xAFFF6, 10},
    {0x22, 0xB000B, 11},
    {0x23, 0xBFFF5, 11},
    {0x20, 0xB000C, 11},

    {0x21, 0xBFFF4, 11},
    {0x1E, 0xB000D, 11},
    {0x1F, 0xBFFF3, 11},
    {0x1C, 0xB000E, 11},

    {0x1D, 0xBFFF2, 11},
    {0x1A, 0xB000F, 11},
    {0x1B, 0xBFFF1, 11},
    {0x18, 0xB0010, 11},

    {0x19, 0xBFFF0, 11}
};

unsigned int MotionCode::index_table[11] =
{
    0,
    1,
    1,
    3,
    5,
    7,
    7,
    9,
    15,
    15,
    21,
};

MotionCode::MotionCode() :
    VLC_Table(table, SIZE, 11, index_table)
{

}

}
