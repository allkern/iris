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

#include "lumtable.hpp"

namespace iris::ipu {

VLC_Entry LumTable::table[] =
{
    {0x0000, 1, 2},
    {0x0001, 2, 2},
    {0x0004, 0, 3},
    {0x0005, 3, 3},
    {0x0006, 4, 3},
    {0x000E, 5, 4},
    {0x001E, 6, 5},
    {0x003E, 7, 6},
    {0x007E, 8, 7},
    {0x00FE, 9, 8},
    {0x01FE, 10, 9},
    {0x01FF, 11, 9}
};

unsigned int LumTable::index_table[9] =
{
    0,
    0,
    2,
    5,
    6,
    7,
    8,
    9,
    10,
};

LumTable::LumTable() : VLC_Table(table, SIZE, 9, index_table)
{

}

}
