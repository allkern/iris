#ifndef PS2_ISO9660_H
#define PS2_ISO9660_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include "ps2.h"

struct iso9660_pvd {
    uint8_t   type;
    char      magic[5];
    uint8_t   version;
    char      pad0[1];
    char      system_id[32];
    char      volume_id[32];
    char      pad1[8];
    uint32_t  volume_space_size[2];
    char      pad2[32];
    uint16_t  volume_set_size[2];
    uint16_t  volume_seq_number[2];
    uint16_t  logical_block_size[2];
    uint32_t  path_table_size[2];
    uint32_t  path_table_lba[4];
};

struct iso9660_state {
    struct iso9660_pvd pvd;
    FILE* file;
};

void iso9660_read(struct iso9660_state* iso, uint32_t lba);
void iso9660_load_boot_elf(struct iso9660_state* iso, char* buf);

#ifdef __cplusplus
}
#endif

#endif

