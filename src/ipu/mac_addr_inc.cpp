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

#include "mac_addr_inc.hpp"

namespace iris::ipu {

VLC_Entry MacroblockAddrInc::table[] =
{
    {0x1, 0x10001, 1},
    {0x3, 0x30002, 3},
    {0x2, 0x30003, 3},
    {0x3, 0x40004, 4},

    {0x2, 0x40005, 4},
    {0x3, 0x50006, 5},
    {0x2, 0x50007, 5},
    {0x7, 0x70008, 7},

    {0x6, 0x70009, 7},
    {0xB, 0x8000A, 8},
    {0xA, 0x8000B, 8},
    {0x9, 0x8000C, 8},

    {0x8, 0x8000D, 8},
    {0x7, 0x8000E, 8},
    {0x6, 0x8000F, 8},
    {0x17, 0xA0010, 10},

    //16
    {0x16, 0xA0011, 10},
    {0x15, 0xA0012, 10},
    {0x14, 0xA0013, 10},
    {0x13, 0xA0014, 10},

    //20
    {0x12, 0xA0015, 10},
    {0x23, 0xB0016, 11},
    {0x22, 0xB0017, 11},
    {0x21, 0xB0018, 11},

    //24
    {0x20, 0xB0019, 11},
    {0x1F, 0xB001A, 11},
    {0x1E, 0xB001B, 11},
    {0x1D, 0xB001C, 11},

    //28
    {0x1C, 0xB001D, 11},
    {0x1B, 0xB001E, 11},
    {0x1A, 0xB001F, 11},
    {0x19, 0xB0020, 11},

    {0x18, 0xB0021, 11},
    {0xF, 0xB0022, 11},
    {0x8, 0xB0023, 11}
};

unsigned int MacroblockAddrInc::index_table[11] =
{
    0,
    1,
    1,
    3,
    5,
    5,
    7,
    9,
    9,
    15,
    21,
};

MacroblockAddrInc::MacroblockAddrInc() : VLC_Table(table, SIZE, 11, index_table)
{

}

}
