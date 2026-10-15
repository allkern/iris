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
#include <string>
#include <vector>

namespace iris::speed::ata { struct Ata; }

namespace iris::speed::dvrfs {

struct Volume;

struct Stat {
    uint16_t mode = 0;
    uint16_t attr = 0;
    uint64_t size = 0;
    uint8_t ctime[8] = {};
    uint8_t atime[8] = {};
    uint8_t mtime[8] = {};
    uint32_t uid = 0;
    uint32_t gid = 0;
    uint32_t blocks = 0;
    uint32_t data = 0;
};

struct Handle {
    uint32_t inode = 0;
    uint64_t position = 0;
    uint32_t flags = 0;
    bool directory = false;
    std::vector <std::string> names;
    std::vector <uint32_t> inodes;
    size_t index = 0;
};

int format_area(ata::Ata* ata);
int format_pfs(ata::Ata* ata, const std::string& partition, uint32_t zone_size, uint32_t fragment);

Volume* mount(ata::Ata* ata, const std::string& partition);
void unmount(Volume* volume);
const std::string& partition_of(Volume* volume);

int open(Volume* volume, const std::string& path, uint32_t flags, uint16_t mode, Handle* out);
int close(Volume* volume, Handle* handle);
int64_t read(Volume* volume, Handle* handle, uint8_t* buf, uint32_t size);
int64_t write(Volume* volume, Handle* handle, const uint8_t* buf, uint32_t size);
int64_t seek(Volume* volume, Handle* handle, int64_t offset, int whence);
int dopen(Volume* volume, const std::string& path, Handle* out);
int dread(Volume* volume, Handle* handle, std::string* name, Stat* stat);
int getstat(Volume* volume, const std::string& path, Stat* stat);
int mkdir(Volume* volume, const std::string& path, uint16_t mode);
int remove(Volume* volume, const std::string& path);
int rmdir(Volume* volume, const std::string& path);
int rename(Volume* volume, const std::string& from, const std::string& to);
uint32_t zone_size(Volume* volume);
uint32_t free_zones(Volume* volume);

}
