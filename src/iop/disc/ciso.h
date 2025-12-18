#ifndef CISO_H
#define CISO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../disc.h"

#include <stdio.h>
#include <stdint.h>

#include <libdeflate.h>
#include <lz4.h>

struct ciso_header {
    uint32_t magic;
    uint32_t header_size;
    uint64_t uncompressed_size;
    uint32_t block_size;
    uint8_t version;
    uint8_t alignment;
    uint8_t reserved[2];
};

struct disc_ciso {
    struct ciso_header header;
    struct libdeflate_decompressor* decompressor;
    FILE* file;
    uint32_t* index_table;
    size_t index_count;
    int compression_type;
    uint8_t* comp_buf;
    size_t comp_buf_cap;
};

struct disc_ciso* ciso_create(void);
int ciso_init(struct disc_ciso* ciso, const char* path);
void ciso_destroy(struct disc_ciso* ciso);

// Disc IF
int ciso_read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t ciso_get_size(void* udata);
int ciso_get_sector_size(void* udata);
int ciso_get_track_count(void* udata);
int ciso_get_track_info(void* udata, int track, struct track_info* info);
int ciso_get_track_number(void* udata, uint64_t lba);

#ifdef __cplusplus
}
#endif

#endif