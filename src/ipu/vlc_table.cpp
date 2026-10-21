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

#include <cstdlib>
#include <cstdio>
#include "vlc_table.hpp"

namespace iris::ipu {

VLC_Table::VLC_Table(VLC_Entry* table, int table_size, int max_bits, unsigned int* index_table) :
    table(table), table_size(table_size), max_bits(max_bits), index_table(index_table)
{
    lut_bits = max_bits < LUT_BITS ? max_bits : LUT_BITS;

    for (uint32_t prefix = 0; prefix < (1u << lut_bits); prefix++)
        lut[prefix] = (int16_t)search(prefix, lut_bits, 0, lut_bits);
}

int VLC_Table::search(uint32_t window, int window_bits, int first, int last) const
{
    for (int i = first; i < last; i++)
    {
        const int bits = i + 1;
        const uint32_t key = window >> (window_bits - bits);

        for (int j = index_table[i]; j < table_size; j++)
        {
            if (bits != table[j].bits)
                break;

            if (key == table[j].key)
                return j;
        }
    }

    return -1;
}

bool VLC_Table::peek_symbol(IPU_FIFO &FIFO, VLC_Entry &entry, bool* invalid)
{
    uint32_t window;

    if (invalid)
        *invalid = false;

    if (FIFO.get_bits(window, max_bits))
    {
        int j = lut[window >> (max_bits - lut_bits)];

        if (j < 0)
            j = search(window, max_bits, lut_bits, max_bits);

        if (j >= 0)
        {
            entry = table[j];
            return true;
        }

        FIFO.vlc_error = true;

        if (invalid)
            *invalid = true;

        return false;
    }

    uint32_t key;

    for (int i = 0; i < max_bits; i++)
    {
        int bits = i + 1;
        if (!FIFO.get_bits(key, bits))
            return false;
        for (int j = index_table[i]; j < table_size; j++)
        {
            if (bits != table[j].bits)
                break;

            if (key == table[j].key)
            {
                entry = table[j];
                return true;
            }
        }
    }
    // throw VLC_Error("VLC symbol not found");
    return false;
}

bool VLC_Table::get_symbol(IPU_FIFO& FIFO, uint32_t &result, bool* invalid)
{
    VLC_Entry entry;
    if (!peek_symbol(FIFO, entry, invalid))
        return false;

    FIFO.advance_stream(entry.bits);
    result = entry.value;
    return true;
}

}
