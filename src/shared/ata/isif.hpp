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
#include <cstdio>

#include "logger.hpp"

namespace iris::ata::isif {

inline constexpr auto MAGIC = 0x46495349;// "ISIF"
inline constexpr auto BLOCK_MODE_UNCOMPRESSED_32BIT = 0;
inline constexpr auto BLOCK_MODE_UNCOMPRESSED_64BIT = 1;
inline constexpr auto BLOCK_MODE_COMPRESSED_32BIT = 2;
inline constexpr auto BLOCK_MODE_COMPRESSED_64BIT = 3;

struct Header {
    uint32_t magic;
    uint32_t version;
    uint64_t block_count;
    uint32_t block_size;
    uint16_t block_mode;
    uint16_t block_compression;
    uint64_t bat_offset;
    uint64_t extension_offset;
    uint64_t data_offset;
    uint64_t bat_block_count;
    uint64_t reserved;
};

struct Isif {
    FILE* file = nullptr;

    Header hdr;
    void* bat = nullptr;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Isif* open(logger::Logger* logger, const char* path);
uint32_t get_version(Isif* isif);
uint64_t get_block_count(Isif* isif);
uint32_t get_block_size(Isif* isif);
uint16_t get_block_mode(Isif* isif);
uint16_t get_block_compression(Isif* isif);
uint64_t get_total_size(Isif* isif);
uint64_t get_allocated_size(Isif* isif);
void read_extension(Isif* isif, void* buffer);
void read_block(Isif* isif, uint64_t index, void* buffer);
void write_block(Isif* isif, uint64_t index, const void* buffer);
void close(Isif* isif);

// File utility functions
int create_image(logger::Logger* logger, const char* path, uint64_t block_count, uint32_t block_size, uint16_t block_mode, uint16_t block_compression, void* ext, uint64_t ext_size);

// ATA adapters
void ata_read_sector(void* udata, uint64_t lba, uint8_t* buf);
void ata_write_sector(void* udata, uint64_t lba, const uint8_t* buf);
int ata_get_identify(void* udata, uint8_t* buf);
uint64_t ata_get_sector_count(void* udata);
void ata_close(void* udata);

}
