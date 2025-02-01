#ifndef PS2_ISO9660_H
#define PS2_ISO9660_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include "ps2.h"

struct __attribute__((packed)) iso9660_pvd {
	char id[8];
	char system_id[32];
	char volume_id[32];
	char zero[8];
	uint32_t total_sector_le, total_sect_be;
	char zero2[32];
	uint16_t volume_set_size_le, volume_set_size_be;
    uint16_t volume_seq_nr_le, volume_seq_nr_be;
	uint16_t sector_size_le, sector_size_be;
	uint32_t path_table_len_le, path_table_len_be;
	uint32_t path_table_le, path_table_2nd_le;
	uint32_t path_table_be, path_table_2nd_be;
	uint8_t root[34];
	char volume_set_id[128], publisher_id[128], data_preparer_id[128], application_id[128];
	char copyright_file_id[37], abstract_file_id[37], bibliographical_file_id[37];
};

struct __attribute__((packed)) iso9660_dirent {
    uint8_t dr_len;
    uint8_t ext_dr_len;
    uint32_t lba_le, lba_be;
    uint32_t size_le, size_be;
    uint8_t date[7];
    uint8_t flags;
    uint8_t file_unit_size;
    uint8_t interleave_gap_size;
    uint16_t volume_seq_nr_le, volume_seq_nr_be;
    uint8_t id_len;
    uint8_t id;
};

struct iso9660_state {
    char buf[0x800];
    char boot_file[256];
    struct iso9660_pvd pvd;
    FILE* file;
};

struct iso9660_state* iso9660_open(const char* path);
char* iso9660_get_boot_path(struct iso9660_state* iso);
// void iso9660_load_boot_elf(struct iso9660_state* iso, char* buf);
void iso9660_close(struct iso9660_state* iso);

#ifdef __cplusplus
}
#endif

#endif

