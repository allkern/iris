#ifndef BIN_H
#define BIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

#include "../disc.h"

struct disc_bin {
    FILE* file;
};

struct disc_bin* bin_create(void);
int bin_init(struct disc_bin* bin, const char* path);
void bin_destroy(struct disc_bin* bin);

// Disc IF
int bin_read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t bin_get_size(void* udata);
uint64_t bin_get_volume_lba(void* udata);
int bin_get_sector_size(void* udata);
int bin_get_track_count(void* udata);
int bin_get_track_info(void* udata, int track, struct track_info* info);
int bin_get_track_number(void* udata, uint64_t lba);

#ifdef __cplusplus
}
#endif

#endif