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

namespace iris::iso9660 {

#ifdef _MSC_VER
#pragma pack(push, 1)
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif

struct PACKED Pvd {
    char id[8];
    char system_id[32];
    char volume_id[32];
    char zero[8];
    uint32_t total_sector_le, total_sect_be;
    char zero2[32];
    uint16_t volume_set_size_le, volume_set_size_be;
    uint16_t volume_seq_nr_le, volume_seq_nr_be;
    uint16_t sector_size_le, sector_size_be;
    uint32_t path_table_len_le, path_table_len_be;
    uint32_t path_table_le, path_table_2nd_le;
    uint32_t path_table_be, path_table_2nd_be;
    uint8_t root[34];
    char volume_set_id[128], publisher_id[128], data_preparer_id[128], application_id[128];
    char copyright_file_id[37], abstract_file_id[37], bibliographical_file_id[37];
};

struct PACKED Dirent {
    uint8_t dr_len;
    uint8_t ext_dr_len;
    uint32_t lba_le, lba_be;
    uint32_t size_le, size_be;
    uint8_t date[7];
    uint8_t flags;
    uint8_t file_unit_size;
    uint8_t interleave_gap_size;
    uint16_t volume_seq_nr_le, volume_seq_nr_be;
    uint8_t id_len;
    uint8_t id;
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#undef PACKED

}
