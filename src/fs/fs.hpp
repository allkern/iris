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

#include "fs/blk.hpp"
#include "logger.hpp"

namespace iris::fs {

enum : int {
    FS_NONE,
    FS_PS2_MCD,
    FS_PS1_MCD,
    FS_FAT,
    FS_EXFAT,
    FS_ISO9660,
    FS_APA,
    FS_PFS
};

enum : int {
    FS_OK = 0,
    FS_ERR_NOT_FOUND = -1,
    FS_ERR_NOT_DIRECTORY = -2,
    FS_ERR_IS_DIRECTORY = -3,
    FS_ERR_IO = -4,
    FS_ERR_CORRUPT = -5,
    FS_ERR_UNSUPPORTED = -6
};

enum : uint32_t {
    ENTRY_DIRECTORY = 1u << 0,
    ENTRY_READ_ONLY = 1u << 1,
    ENTRY_HIDDEN = 1u << 2,
    ENTRY_SYSTEM = 1u << 3,
    ENTRY_PROTECTED = 1u << 4,
    ENTRY_PSX_SAVE = 1u << 5,
    ENTRY_POCKETSTATION = 1u << 6,
    ENTRY_DELETED = 1u << 7,
    ENTRY_TRUNCATED = 1u << 8
};

inline constexpr size_t MAX_DIR_ENTRIES = 65536;

struct Time {
    bool valid = false;
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
};

struct Entry {
    char name[256] = {};
    uint64_t size = 0;
    uint32_t flags = 0;
    Time created;
    Time modified;

    // Whatever it needs to find the entry again
    uint64_t cookie = 0;
};

struct Handle;

struct Fs {
    int (*list)(void* udata, const char* path, std::vector <Entry>* out);
    int (*stat)(void* udata, const char* path, Entry* out);
    int (*open)(void* udata, const char* path, Handle** out);
    int64_t (*read)(void* udata, Handle* handle, uint64_t offset, void* buf, uint64_t size);
    void (*close_handle)(void* udata, Handle* handle);
    void (*close)(void* udata);

    void* udata;

    blk::Device* dev;
    bool owns_dev;

    int type = FS_NONE;

    char variant[16] = {};
    char label[64] = {};
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;

    bool truncated = false;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Fs* probe(logger::Logger* logger, blk::Device* dev, bool take_ownership);

int list(Fs* fs, const char* path, std::vector <Entry>* out);
int stat(Fs* fs, const char* path, Entry* out);
int open(Fs* fs, const char* path, Handle** out);
int64_t read(Fs* fs, Handle* handle, uint64_t offset, void* buf, uint64_t size);
void close_handle(Fs* fs, Handle* handle);
void close(Fs* fs);

const char* type_name(int type);
const char* type_name(const Fs* fs);
const char* error_name(int error);

std::string path_join(const std::string& dir, const std::string& name);
std::string path_parent(const std::string& path);
std::vector <std::string> path_split(const std::string& path);
const char* path_basename(const char* path);

void utf16_to_utf8(const uint16_t* in, size_t count, char* out, size_t out_size, bool* truncated);
bool sanitize_name(const char* in, std::string* out);
bool path_is_inside(const std::string& root, const std::string& path);

}
