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
