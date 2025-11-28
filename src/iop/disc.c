#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "disc.h"
#include "disc/iso.h"
#include "disc/cue.h"
#include "disc/chd.h"
#include "disc/bin.h"

#ifdef _MSC_VER
__pragma(pack(push, 1))

#define PACKED
#else
#define PACKED __attribute__((packed))
#endif

struct PACKED iso9660_pvd {
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

struct PACKED iso9660_dirent {
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

#ifdef _MSC_VER
__pragma(pack(pop))
#endif

static const char* disc_extensions[] = {
    "iso",
    "bin",
    "cue",
    "chd",
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

int disc_get_extension(const char* path) {
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
            s->get_sector_size = iso_get_sector_size;
            s->get_track_count = iso_get_track_count;
            s->get_track_info = iso_get_track_info;
            s->get_track_number = iso_get_track_number;

            // To-do: Check if path exists
            r = iso_init(iso, path);
        } break;

        // Raw 2352-byte sector disc image (CD)
        case DISC_EXT_NONE:
        case DISC_EXT_BIN: {
            struct disc_bin* bin = bin_create();

            s->udata = bin;
            s->read_sector = bin_read_sector;
            s->get_size = bin_get_size;
            s->get_sector_size = bin_get_sector_size;
            s->get_track_count = bin_get_track_count;
            s->get_track_info = bin_get_track_info;
            s->get_track_number = bin_get_track_number;

            r = bin_init(bin, path);
        } break;

        // CUE+BIN disc image (contains track information)
        case DISC_EXT_CUE: {
            struct disc_cue* cue = cue_create();

            s->udata = cue;
            s->read_sector = cue_read_sector;
            s->get_size = cue_get_size;
            s->get_sector_size = cue_get_sector_size;
            s->get_track_count = cue_get_track_count;
            s->get_track_info = cue_get_track_info;
            s->get_track_number = cue_get_track_number;

            r = cue_init(cue, path);
        } break;

        // MAME CHD disc image (Compressed Hunks of Data)
        case DISC_EXT_CHD: {
            struct disc_chd* chd = chd_create();

            s->udata = chd;
            s->read_sector = chd_read_sector;
            s->get_size = chd_get_size;
            s->get_sector_size = chd_get_sector_size;
            s->get_track_count = chd_get_track_count;
            s->get_track_info = chd_get_track_info;
            s->get_track_number = chd_get_track_number;

            r = chd_init(chd, path);
        } break;

        default: {
            free(s);

            return NULL;
        } break;
    }

    if (!r) {
        free(s);

        return NULL;
    }

    return s;
}

#define CD_EXTRA_SIZE 800000000
#define CDX_MAX_SIZE 734003200
#define CD_MAX_SIZE 681574400

int disc_detect_media(struct disc_state* disc) {
    uint64_t size = disc_get_size(disc);

    disc_fetch_pvd(disc);

    uint64_t sector_size = disc_get_sector_size(disc);
    uint64_t volume_size = *(uint32_t*)&disc->pvd[0x50];
    uint64_t path_table_lba = *(uint32_t*)&disc->pvd[0x8c];

    printf("sector_size=%lx volume_size=%lx (%ld) in bytes=%lx (%ld) path_table_lba=%lx (%ld) size=%lx (%ld)\n",
        sector_size,
        volume_size, volume_size,
        volume_size * sector_size, volume_size * sector_size,
        path_table_lba, path_table_lba,
        size, size
    );

    // DVD is dual-layer
    if ((volume_size * sector_size) < size) {
        disc->layer2_lba = volume_size;
    
        return DISC_MEDIA_DVD;
    }

    if (((volume_size * sector_size) <= CD_EXTRA_SIZE) && (path_table_lba != 257)) {
        return DISC_MEDIA_CD;
    }

    return DISC_MEDIA_DVD;
}

static inline int disc_detect_type(struct disc_state* disc) {
    if (!disc_fetch_pvd(disc))
        return DISC_TYPE_INVALID;

    struct iso9660_pvd pvd = *(struct iso9660_pvd*)disc->pvd;

    // Not ISO 9660 format disc
    if (strncmp(pvd.id, "\1CD001\1", 8)) {
        // If the disc doesn't contain an ISO filesystem
        // and it's a CUE image, it's probably a CD audio image
        if (disc->ext == DISC_EXT_CUE) {
            return DISC_TYPE_CDDA;
        }

        // Otherwise it's invalid
        return DISC_TYPE_INVALID;
    }

    // Check for the "PLAYSTATION" string at PVD offset 08h
    // Patch 20 byte so comparison is done correctly
    disc->pvd[0x13] = 0;

    // Disc contains a "valid" ISO filesystem, but it's not a
    // PlayStation disc, it might be a DVD video disc or something
    // else entirely, either way don't outright reject it just yet
    if (strncmp(&disc->pvd[0x8], "PLAYSTATION", 12))
        return DISC_TYPE_UNKNOWN;

    // Disc contains a valid ISO filesystem and the PlayStation string
    // is present, so this is most likely a game disc.
    return DISC_TYPE_GAME;
}

int disc_get_type(struct disc_state* disc) {
    int media = disc_detect_media(disc);
    int type = disc_detect_type(disc);

    if (type == DISC_TYPE_INVALID) {
        return CDVD_DISC_INVALID;
    }

    if (type == DISC_TYPE_CDDA) {
        return CDVD_DISC_CDDA;
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

        // SYSTEM.CNF not found and VIDEO_TS not found
        // This is probably something else entirely (like an Xbox disc)
        // The PS2 wouldn't handle it anyways
        return CDVD_DISC_INVALID;
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

int disc_get_track_count(struct disc_state* disc) {
    if (!disc)
        return 0;

    if (!disc->get_track_count)
        return 0;

    return disc->get_track_count(disc->udata);
}

int disc_get_track_info(struct disc_state* disc, int track, struct track_info* info) {
    if (!disc)
        return 0;

    if (!disc->get_track_info)
        return 0;

    return disc->get_track_info(disc->udata, track, info);
}

int disc_get_track_number(struct disc_state* disc, uint64_t lba) {
    if (!disc)
        return 0;

    if (!disc->get_track_number)
        return 0;

    return disc->get_track_number(disc->udata, lba);
}

void disc_close(struct disc_state* disc) {
    switch (disc->ext) {
        // Standard raw 2048-byte sector ISO 9660 image
        // usually used for DVDs
        case DISC_EXT_ISO: {
            iso_destroy(disc->udata);
        } break;

        case DISC_EXT_CUE: {
            cue_destroy(disc->udata);
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
    if (disc->boot_path_cached)
        return disc->boot_path;

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

char* disc_read_boot_elf(struct disc_state* disc, int s) {
    char* boot_path = disc_get_boot_path(disc);

    printf("iso: Reading boot ELF from disc at path \'%s\'...\n", boot_path);

    if (!boot_path) {
        printf("iso: No boot path found in SYSTEM.CNF\n");

        return NULL;
    }

    if (!disc_fetch_root(disc)) {
        printf("iso: Couldn't fetch root directory\n");

        return NULL;
    }

    char path[256];

    path[0] = '\0';

    char* ptr = boot_path;

    // Go to end of boot path
    while (*ptr++);

    // Reverse search for a path separator
    while (*ptr != '/' && *ptr != '\\' && *ptr != ':') {
        if (ptr == boot_path)
            break;

        --ptr;
    }

    // Skip the path separator
    ptr += 1;

    // Copy the path to our buffer
    int i;

    for (i = 0; *ptr; i++) {
        path[i] = *ptr++;
    }

    path[i] = '\0';

    struct iso9660_dirent* dir = (struct iso9660_dirent*)disc->root;

    while (dir->dr_len) {
        if (!strncmp((char*)&dir->id, path, dir->id_len)) {
            int32_t size = dir->size_le;
            uint32_t lba = dir->lba_le;

            char* buf = malloc(((size >> 11) + 2) << 11);
            char* ptr = buf;

            printf("iso: Boot ELF found at lba=%08x size=%08x\n", lba, dir->size_le, dir->size_le);

            while (size > 0) {
                if (!disc_read_sector(disc, ptr, lba++, DISC_SS_DATA)) {
                    printf("iso: Couldn't read boot ELF sector %d\n", lba - 1);

                    return NULL;
                }

                ptr += 2048;
                size -= 2048;
            }

            if (size != 0) {
                // Read the last sector, which might be smaller than 2048 bytes
                if (!disc_read_sector(disc, ptr, lba, DISC_SS_DATA)) {
                    printf("iso: Couldn't read boot ELF sector %d\n", lba);

                    return NULL;
                }
            }

            return buf;
        }

        uint8_t* ptr = (uint8_t*)dir;

        dir = (struct iso9660_dirent*)(ptr + dir->dr_len);
    }

    // Couldn't find the boot ELF file in the root directory
    return NULL;
}

#undef PACKED