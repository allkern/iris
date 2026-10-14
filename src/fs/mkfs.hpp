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

#include "fs/fs.hpp"

namespace iris::fs::mkfs {

enum : int {
    MKFS_PS2_MCD,
    MKFS_PS1_MCD,
    MKFS_FAT32,
    MKFS_EXFAT
};

struct Params {
    int type = MKFS_FAT32;
    uint64_t size = 0;
    bool partition = false;
    bool ecc = true;

    char label[64] = {};
};

inline constexpr uint32_t PAGE_SIZE = 512;
inline constexpr uint32_t PAGE_ECC_SIZE = 16;

int create_image(logger::Logger* logger, const char* path, uint64_t size, uint8_t fill);
int format(logger::Logger* logger, const char* path, const Params& params);
void page_ecc(const uint8_t* page, uint8_t* out);

}
