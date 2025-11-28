#ifndef ISO_H
#define ISO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../disc.h"

#include <stdio.h>
#include <stdint.h>

struct disc_iso {
    FILE* file;
};

struct disc_iso* iso_create(void);
int iso_init(struct disc_iso* iso, const char* path);
void iso_destroy(struct disc_iso* iso);

// Disc IF
int iso_read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t iso_get_size(void* udata);
uint64_t iso_get_volume_lba(void* udata);
int iso_get_sector_size(void* udata);
int iso_get_track_count(void* udata);
int iso_get_track_info(void* udata, int track, struct track_info* info);
int iso_get_track_number(void* udata, uint64_t lba);

#ifdef __cplusplus
}
#endif

#endif