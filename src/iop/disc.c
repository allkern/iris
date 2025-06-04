#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "disc.h"
#include "disc/iso.h"
#include "disc/bin.h"

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

static const char* disc_extensions[] = {
    "iso",
    "bin",
    "cue",
    NULL
};

static inline int disc_fetch_pvd(struct disc_state* disc) {
    if (disc->pvd_cached)
        return 1;

    if (!disc_read_sector(disc, disc->pvd, 16, DISC_SS_DATA))
        return 0;

    disc->pvd_cached = 1;

    return 1;
}

static inline int disc_fetch_root(struct disc_state* disc) {
    if (disc->root_cached)
        return 1;

    if (!disc_fetch_pvd(disc))
        return 0;

    struct iso9660_pvd pvd = *(struct iso9660_pvd*)disc->pvd;
    struct iso9660_dirent* root = (struct iso9660_dirent*)pvd.root;

    // Root points to unreadable sector
    if (!disc_read_sector(disc, disc->root, root->lba_le, DISC_SS_DATA))
        return 0;

    disc->root_cached = 1;

    return 1;
}

static inline int disc_fetch_system_cnf(struct disc_state* disc) {
    if (disc->system_cnf_cached)
        return 1;

    if (!disc_fetch_root(disc))
        return 0;

    struct iso9660_dirent* dir = (struct iso9660_dirent*)disc->root;

    while (dir->dr_len) {
        if (dir->id_len == 12) {
            if (!strcmp((char*)&dir->id, "SYSTEM.CNF;1")) {
                break;
            }
        }

        uint8_t* ptr = (uint8_t*)dir;

        dir = (struct iso9660_dirent*)(ptr + dir->dr_len);
    }

    // Couldn't find SYSTEM.CNF file, non-playstation disc
    if (!dir->dr_len)
        return 0;

    // SYSTEM.CNF points to unreadable sector, invalid
    if (!disc_read_sector(disc, disc->system_cnf, dir->lba_le, DISC_SS_DATA))
        return 0;

    disc->system_cnf_cached = 1;

    return 1;
}

int disc_get_extension(char* path) {
    if (!path)
        return DISC_EXT_UNSUPPORTED;

    if (!(*path))
        return DISC_EXT_UNSUPPORTED;

    int len = strlen(path);

    char* lc = malloc(len + 1);

    strcpy(lc, path);

    for (int i = 0; i < len; i++)
        lc[i] = tolower(lc[i]);

    path = lc;

    const char* ptr = path + (len - 1);

    while (*ptr != '.') {
        if (ptr == path) {
            free(lc);

            return DISC_EXT_NONE;
        }

        --ptr;
    }

    for (int i = 0; disc_extensions[i]; i++) {
        if (!strcmp(ptr + 1, disc_extensions[i])) {
            free(lc);

            return i;
        }
    }

    free(lc);

    return DISC_EXT_UNSUPPORTED;
}

struct disc_state* disc_open(const char* path) {
    int ext = disc_get_extension(path);

    if (ext == DISC_EXT_UNSUPPORTED)
        return NULL;

    struct disc_state* s = malloc(sizeof(struct disc_state));

    memset(s, 0, sizeof(struct disc_state));

    s->layer2_lba = 0;
    s->ext = ext;

    int r;

    switch (ext) {
        // Standard raw 2048-byte sector ISO 9660 image
        // usually used for DVDs
        case DISC_EXT_ISO: {
            struct disc_iso* iso = iso_create();

            s->udata = iso;
            s->read_sector = iso_read_sector;
            s->get_size = iso_get_size;
            s->get_volume_lba = iso_get_volume_lba;
            s->get_sector_size = iso_get_sector_size;

            // To-do: Check if path exists
            r = iso_init(iso, path);
        } break;

        // Raw 2352-byte sector disc image (CD)
        case DISC_EXT_BIN: {
            struct disc_bin* bin = bin_create();

            s->udata = bin;
            s->read_sector = bin_read_sector;
            s->get_size = bin_get_size;
            s->get_volume_lba = bin_get_volume_lba;
            s->get_sector_size = bin_get_sector_size;

            // To-do: Check if path exists
            r = bin_init(bin, path);
        } break;

        default: {
            free(s);

            return NULL;
        } break;
    }

    if (!r)
        return s;

    free(s);

    return NULL;
}

int disc_detect_media(struct disc_state* disc) {
    uint64_t size = disc_get_size(disc);

    disc_fetch_pvd(disc);

    uint64_t sector_size = disc_get_sector_size(disc);
    uint64_t volume_size = *(uint32_t*)&disc->pvd[0x50];
    uint64_t path_table_lba = *(uint32_t*)&disc->pvd[0x8c];

    // printf("sector_size=%lx volume_size=%lx (%ld) in bytes=%lx (%ld) path_table_lba=%lx (%ld) size=%lx (%ld)\n",
    //     sector_size,
    //     volume_size, volume_size,
    //     volume_size * sector_size, volume_size * sector_size,
    //     path_table_lba, path_table_lba,
    //     size, size
    // );

    // DVD is dual-layer
    if ((volume_size * sector_size) < size) {
        disc->layer2_lba = volume_size;
    
        return DISC_MEDIA_DVD;
    }

    if ((volume_size * sector_size) <= 681574400 || path_table_lba != 257) {
        return DISC_MEDIA_CD;
    }

    return DISC_MEDIA_DVD;
}

static inline int disc_detect_type(struct disc_state* disc) {
    if (!disc_fetch_pvd(disc))
        return DISC_TYPE_INVALID;

    struct iso9660_pvd pvd = *(struct iso9660_pvd*)disc->pvd;

    // Not ISO 9660 format disc
    if (strncmp(pvd.id, "\1CD001\1", 8))
        return DISC_TYPE_INVALID;

    // Check for the "PLAYSTATION" string at PVD offset 08h
    // Patch 20 byte so comparison is done correctly
    disc->pvd[0x13] = 0;

    if (strncmp(&disc->pvd[0x8], "PLAYSTATION", 12))
        return DISC_TYPE_CDDA;

    return DISC_TYPE_GAME;
}

int disc_get_type(struct disc_state* disc) {
    int media = disc_detect_media(disc);
    int type = disc_detect_type(disc);

    if (type == DISC_TYPE_INVALID) {
        return CDVD_DISC_INVALID;
    }

    // Start final detection
    char buf[2048];

    if (!disc_read_sector(disc, buf, 16, DISC_SS_DATA)) {
        return CDVD_DISC_INVALID;
    }

    struct iso9660_pvd pvd = *(struct iso9660_pvd*)buf;
    struct iso9660_dirent* root = (struct iso9660_dirent*)pvd.root;

    // Root points to unreadable sector
    if (!disc_read_sector(disc, buf, root->lba_le, DISC_SS_DATA)) {
        return CDVD_DISC_INVALID;
    }

    struct iso9660_dirent* dir = (struct iso9660_dirent*)buf;

    while (dir->dr_len) {
        if (dir->id_len == 12) {
            if (!strcmp((char*)&dir->id, "SYSTEM.CNF;1")) {
                break;
            }
        }

        // printf("dir=\'%s\'\n", &dir->id);

        uint8_t* ptr = (uint8_t*)dir;

        dir = (struct iso9660_dirent*)(ptr + dir->dr_len);
    }

    // Couldn't find SYSTEM.CNF file, non-playstation disc
    if (!dir->dr_len) {
        if (media == DISC_MEDIA_CD)
            return CDVD_DISC_CDDA;

        // Might be a DVD Video disc
        // Look for VIDEO_TS in the root dir
        dir = (struct iso9660_dirent*)buf;

        while (dir->dr_len) {
            // Search directories
            if ((dir->flags & 2) == 0) goto next;
            if (dir->id_len != 8) goto next;

            if (!strncmp((char*)&dir->id, "VIDEO_TS", dir->id_len)) {
                return CDVD_DISC_DVD_VIDEO;
            }

            // printf("dir=\'%s\' (%d)\n", (char*)&dir->id, dir->id_len);

            next:;

            uint8_t* ptr = (uint8_t*)dir;

            dir = (struct iso9660_dirent*)(ptr + dir->dr_len);
        }

        return CDVD_DISC_PS2_DVD;
    }

    // SYSTEM.CNF points to unreadable sector, invalid
    if (!disc_read_sector(disc, buf, dir->lba_le, DISC_SS_DATA)) {
        return CDVD_DISC_INVALID;
    }

    // Parse SYSTEM.CNF
    char* p = buf;
    char key[64];
    
    while (*p) {
        char* kptr = key;

        while (isspace(*p))
            ++p;

        while (isalnum(*p))
            *kptr++ = *p++;

        *kptr = '\0';

        // printf("key: %s\n", key);

        // BOOT entry found, PlayStation CD
        if (!strncmp(key, "BOOT", 64))
            return CDVD_DISC_PSX_CD;

        // BOOT2 entry found, PlayStation 2 disc
        if (!strncmp(key, "BOOT2", 64)) {
            return media == DISC_MEDIA_CD ? CDVD_DISC_PS2_CD : CDVD_DISC_PS2_DVD;
        }
    }

    // Couldn't find BOOT or BOOT2 entry, invalid/bootleg PlayStation disc?
    return DISC_TYPE_INVALID;
}

int disc_read_sector(struct disc_state* disc, unsigned char* buf, uint64_t lba, int size) {
    if (!disc)
        return 0;

    if (!disc->read_sector)
        return 0;

    return disc->read_sector(disc->udata, buf, lba, size);
}

uint64_t disc_get_size(struct disc_state* disc) {
    if (!disc)
        return 0;

    if (!disc->read_sector)
        return 0;

    return disc->get_size(disc->udata);
}

uint64_t disc_get_volume_lba(struct disc_state* disc, int vol) {
    if (!disc)
        return 0;

    if (!disc->read_sector)
        return 0;

    if (!vol)
        return 0;

    if (!disc->layer2_lba) {
        disc_detect_media(disc);
    }

    return disc->layer2_lba;
}

int disc_get_sector_size(struct disc_state* disc) {
    if (!disc)
        return 0;

    if (!disc->get_sector_size)
        return 0;

    return disc->get_sector_size(disc->udata);
}

void disc_close(struct disc_state* disc) {
    switch (disc->ext) {
        // Standard raw 2048-byte sector ISO 9660 image
        // usually used for DVDs
        case DISC_EXT_ISO: {
            iso_destroy(disc->udata);
        } break;

        // Raw 2352-byte sector disc image (CD)
        case DISC_EXT_BIN: {
            bin_destroy(disc->udata);
        } break;
    }

    free(disc);
}

char* disc_get_serial(struct disc_state* disc, char* serial) {
    if (!disc)
        return NULL;

    // No game serial
    if (!disc_fetch_system_cnf(disc))
        return NULL;

    // Parse SYSTEM.CNF
    char* p = disc->system_cnf;
    char key[64];
    
    while (*p) {
        char* kptr = key;

        while (isspace(*p))
            ++p;

        while (isalnum(*p))
            *kptr++ = *p++;

        *kptr = '\0';

        if (!strncmp(key, "BOOT2", 64)) {
            while (isspace(*p)) ++p;

            if (*p != '=') {
                printf("iso: Expected =\n");

                return NULL;
            }

            ++p;

            while (isspace(*p)) ++p;

            while (*p != ':') ++p;

            ++p;

            if (*p == '\\' || *p == '/')
                ++p;

            int i;

            for (i = 0; i < 16; i++) {
                if (*p == ';' || *p == '\n' || *p == '\r')
                    break;

                serial[i] = *p++;
            }

            serial[i] = '\0';

            return serial;
        } else {
            while ((*p != '\n') && (*p != '\0') && (*p != '\r')) ++p;
            while ((*p == '\n') || (*p == '\r')) ++p;
        }
    }

    printf("iso: Couldn't find BOOT2 entry in SYSTEM.CNF (PlayStation disc?)\n");

    return NULL;
}

char* disc_get_boot_path(struct disc_state* disc) {
    // No boot path
    if (!disc_fetch_system_cnf(disc))
        return NULL;

    // Parse SYSTEM.CNF
    char* p = disc->system_cnf;
    char key[64];
    
    while (*p) {
        char* kptr = key;

        while (isspace(*p))
            ++p;

        while (isalnum(*p))
            *kptr++ = *p++;

        *kptr = '\0';

        // printf("key: %s\n", key);

        if (!strncmp(key, "BOOT2", 64)) {
            while (isspace(*p)) ++p;

            if (*p != '=') {
                printf("iso: Expected =\n");

                return NULL;
            }

            ++p;

            while (isspace(*p)) ++p;

            int i;

            for (i = 0; i < 255; i++) {
                if (*p == '\n' || *p == '\r')
                    break;

                disc->boot_path[i] = *p++;
            }

            disc->boot_path[i] = '\0';

            return disc->boot_path;
        } else {
            while ((*p != '\n') && (*p != '\0') && (*p != '\r')) ++p;
            while ((*p == '\n') || (*p == '\r')) ++p;
        }
    }

    printf("iso: Couldn't find BOOT2 entry in SYSTEM.CNF (PlayStation disc?)\n");

    return NULL;
}