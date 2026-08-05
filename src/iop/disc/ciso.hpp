#pragma once

#include "../disc.hpp"


#include <libdeflate.h>
#include <lz4.h>
#include "logger.hpp"

namespace iris::iop::disc::ciso {

struct Header {
    uint32_t magic;
    uint32_t header_size;
    uint64_t uncompressed_size;
    uint32_t block_size;
    uint8_t version;
    uint8_t alignment;
    uint8_t reserved[2];
};

struct Ciso {
    Header header;
    struct libdeflate_decompressor* decompressor;
    FILE* file;
    uint32_t* index_table;
    size_t index_count;
    int compression_type;
    uint8_t* comp_buf;
    size_t comp_buf_cap;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Ciso* create(logger::Logger* logger);
int init(Ciso* ciso, const char* path);
void destroy(Ciso* ciso);

// Disc IF
int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t get_size(void* udata);
int get_sector_size(void* udata);
int get_track_count(void* udata);
int get_track_info(void* udata, int track, TrackInfo* info);
int get_track_number(void* udata, uint64_t lba);

}
