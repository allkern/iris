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

#include "chromtable.hpp"

namespace iris::ipu {

VLC_Entry ChromTable::table[] =
{
    {0x0, 0, 2},
    {0x1, 1, 2},
    {0x2, 2, 2},
    {0x6, 3, 3},
    {0xE, 4, 4},
    {0x1E, 5, 5},
    {0x3E, 6, 6},
    {0x7E, 7, 7},
    {0xFE, 8, 8},
    {0x1FE, 9, 9},
    {0x3FE, 10, 10},
    {0x3FF, 11, 10}
};

unsigned int ChromTable::index_table[10] =
{
    0,
    0,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
};

ChromTable::ChromTable() : VLC_Table(table, SIZE, 10, index_table)
{

}

}
