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

#include <cctype>
#include <cstring>
#include <string>

#include "disc.hpp"
#include "disc/iso.hpp"
#include "disc/cue.hpp"
#include "disc/chd.hpp"
#include "disc/ciso.hpp"
#include "disc/bin.hpp"
#include "disc/mds.hpp"
#include "ps2_iso9660.hpp"

namespace iris::iop::disc {

static const char* extensions[] = {
    "iso",
    "bin",
    "cue",
    "chd",
    "cso",
    "zso",
    "mds",
    "mdf",
    NULL
};

static inline int fetch_pvd(Disc* disc) {
    if (disc->pvd_cached)
        return 1;

    if (!read_sector(disc, (unsigned char*)disc->pvd, 16, DISC_SS_DATA))
        return 0;

    disc->pvd_cached = 1;

    return 1;
}

static inline int fetch_root(Disc* disc) {
    if (disc->root_cached)
        return 1;

    if (!fetch_pvd(disc))
        return 0;

    iso9660::Pvd pvd = *(iso9660::Pvd*)disc->pvd;
    iso9660::Dirent* root = (iso9660::Dirent*)pvd.root;

    // Root points to unreadable sector
    if (!read_sector(disc, (unsigned char*)disc->root, root->lba_le, DISC_SS_DATA))
        return 0;

    disc->root_cached = 1;

    return 1;
}

static inline int fetch_system_cnf(Disc* disc) {
    if (disc->system_cnf_cached)
        return 1;

    if (!fetch_root(disc))
        return 0;

    iso9660::Dirent* dir = (iso9660::Dirent*)disc->root;

    while (dir->dr_len) {
        if (dir->id_len == 12) {
            if (!strcmp((char*)&dir->id, "SYSTEM.CNF;1")) {
                break;
            }
        }

        uint8_t* ptr = (uint8_t*)dir;

        dir = (iso9660::Dirent*)(ptr + dir->dr_len);
    }

    // Couldn't find SYSTEM.CNF file, non-playstation disc
    if (!dir->dr_len)
        return 0;

    // SYSTEM.CNF points to unreadable sector, invalid
    if (!read_sector(disc, (unsigned char*)disc->system_cnf, dir->lba_le, DISC_SS_DATA))
        return 0;

    disc->system_cnf_cached = 1;

    return 1;
}

int get_extension(const char* path) {
    if (!path)
        return DISC_EXT_UNSUPPORTED;

    const char* ptr = strrchr(path, '.');

    if (!ptr) {
        return DISC_EXT_NONE;
    }

    std::string ext(ptr + 1);

    // tolower is only defined for unsigned char values, so a path with any
    // byte above 0x7f would otherwise be undefined
    for (char& c : ext)
        c = (char)std::tolower((unsigned char)c);

    for (int i = 0; extensions[i]; i++) {
        if (ext == extensions[i])
            return i;
    }

    return DISC_EXT_UNSUPPORTED;
}

static constexpr auto LAYOUT_UNKNOWN = 0;
static constexpr auto LAYOUT_COOKED = 1;
static constexpr auto LAYOUT_RAW_MODE1 = 2;
static constexpr auto LAYOUT_RAW_MODE2 = 3;

static bool has_pvd_signature(FILE* file, uint64_t offset) {
    char id[5];

    if (fseek64(file, offset + 1, SEEK_SET))
        return false;

    if (fread(id, 1, sizeof(id), file) != sizeof(id))
        return false;

    return !memcmp(id, "CD001", sizeof(id));
}

static int probe_layout(const char* path) {
    FILE* file = fopen(path, "rb");

    if (!file)
        return LAYOUT_UNKNOWN;

    int layout = LAYOUT_UNKNOWN;

    if (has_pvd_signature(file, 16 * 2048)) {
        layout = LAYOUT_COOKED;
    } else if (has_pvd_signature(file, (16 * 2352) + bin::BIN_MODE2_DATA_OFFSET)) {
        layout = LAYOUT_RAW_MODE2;
    } else if (has_pvd_signature(file, (16 * 2352) + bin::BIN_MODE1_DATA_OFFSET)) {
        layout = LAYOUT_RAW_MODE1;
    }

    fclose(file);

    return layout;
}

static int open_bin(Disc* s, const char* path, int data_offset) {
    bin::Bin* bin = bin::create(s->logger);

    bin->data_offset = data_offset;

    s->ext = DISC_EXT_BIN;
    s->udata = bin;
    s->read_sector = bin::read_sector;
    s->get_size = bin::get_size;
    s->get_sector_size = bin::get_sector_size;
    s->get_track_count = bin::get_track_count;
    s->get_track_info = bin::get_track_info;
    s->get_track_number = bin::get_track_number;

    return bin::init(bin, path);
}

Disc* open(logger::Logger* logger, const char* path) {
    int ext = get_extension(path);

    if (ext == DISC_EXT_UNSUPPORTED)
        return NULL;

    Disc* s = new Disc();

    s->logger = logger;
    s->logger_id = logger::register_source(logger, "disc");
    s->layer2_lba = 0;
    s->ext = ext;

    int r;

    switch (ext) {
        // Standard raw 2048-byte sector ISO 9660 image
        // usually used for DVDs
        case DISC_EXT_ISO: {
            int layout = probe_layout(path);

            if (layout == LAYOUT_RAW_MODE1) {
                r = open_bin(s, path, bin::BIN_MODE1_DATA_OFFSET);
            } else if (layout == LAYOUT_RAW_MODE2) {
                r = open_bin(s, path, bin::BIN_MODE2_DATA_OFFSET);
            } else {
                iso::Iso* iso = iso::create(s->logger);

                s->udata = iso;
                s->read_sector = iso::read_sector;
                s->get_size = iso::get_size;
                s->get_sector_size = iso::get_sector_size;
                s->get_track_count = iso::get_track_count;
                s->get_track_info = iso::get_track_info;
                s->get_track_number = iso::get_track_number;

                // To-do: Check if path exists
                r = iso::init(iso, path);
            }
        } break;

        // Raw 2352-byte sector disc image (CD)
        case DISC_EXT_NONE:
        case DISC_EXT_BIN: {
            int layout = probe_layout(path);

            if (layout == LAYOUT_RAW_MODE1) {
                r = open_bin(s, path, bin::BIN_MODE1_DATA_OFFSET);
            } else {
                r = open_bin(s, path, bin::BIN_MODE2_DATA_OFFSET);
            }
        } break;

        // CUE+BIN disc image (contains track information)
        case DISC_EXT_CUE: {
            cue::Cue* cue = cue::create(s->logger);

            s->udata = cue;
            s->read_sector = cue::read_sector;
            s->get_size = cue::get_size;
            s->get_sector_size = cue::get_sector_size;
            s->get_track_count = cue::get_track_count;
            s->get_track_info = cue::get_track_info;
            s->get_track_number = cue::get_track_number;

            r = cue::init(cue, path);
        } break;

        // MAME CHD disc image (Compressed Hunks of Data)
        case DISC_EXT_CHD: {
            chd::Chd* chd = chd::create(s->logger);

            s->udata = chd;
            s->read_sector = chd::read_sector;
            s->get_size = chd::get_size;
            s->get_sector_size = chd::get_sector_size;
            s->get_track_count = chd::get_track_count;
            s->get_track_info = chd::get_track_info;
            s->get_track_number = chd::get_track_number;

            r = chd::init(chd, path);
        } break;

        case DISC_EXT_CSO:
        case DISC_EXT_ZSO: {
            ciso::Ciso* ciso = ciso::create(s->logger);

            s->udata = ciso;
            s->read_sector = ciso::read_sector;
            s->get_size = ciso::get_size;
            s->get_sector_size = ciso::get_sector_size;
            s->get_track_count = ciso::get_track_count;
            s->get_track_info = ciso::get_track_info;
            s->get_track_number = ciso::get_track_number;

            r = ciso::init(ciso, path);
        } break;

        case DISC_EXT_MDS:
        case DISC_EXT_MDF: {
            mds::Mds* mds = mds::create(s->logger);

            s->udata = mds;
            s->read_sector = mds::read_sector;
            s->get_size = mds::get_size;
            s->get_sector_size = mds::get_sector_size;
            s->get_track_count = mds::get_track_count;
            s->get_track_info = mds::get_track_info;
            s->get_track_number = mds::get_track_number;

            r = mds::init(mds, path);

            if (r) {
                s->layer2_lba = mds->layer_break;
            }
        } break;

        default: {
            delete s;

            return NULL;
        } break;
    }

    if (!r) {
        delete s;

        return NULL;
    }

    return s;
}

#define CD_EXTRA_SIZE 800000000
#define CDX_MAX_SIZE 734003200
#define CD_MAX_SIZE 681574400

int detect_media(Disc* disc) {
    uint64_t size = get_size(disc);

    fetch_pvd(disc);

    uint64_t sector_size = get_sector_size(disc);
    uint64_t volume_size = *(uint32_t*)&disc->pvd[0x50];
    uint64_t path_table_lba = *(uint32_t*)&disc->pvd[0x8c];

    iris_debug(disc, "sector_size={:x} volume_size={:x} ({}) in bytes={:x} ({}) path_table_lba={:x} ({}) size={:x} ({})", sector_size,
        volume_size, volume_size,
        volume_size * sector_size, volume_size * sector_size,
        path_table_lba, path_table_lba,
        size, size);

    if (sector_size != 2048) {
        return DISC_MEDIA_CD;
    }

    // DVD is dual-layer
    if ((volume_size * sector_size) < size) {
        if (!disc->layer2_lba) {
            disc->layer2_lba = volume_size;
        }

        return DISC_MEDIA_DVD;
    }

    if (((volume_size * sector_size) <= CD_EXTRA_SIZE) && (path_table_lba != 257)) {
        return DISC_MEDIA_CD;
    }

    return DISC_MEDIA_DVD;
}

static inline int detect_type(Disc* disc) {
    if (!fetch_pvd(disc))
        return DISC_TYPE_INVALID;

    iso9660::Pvd pvd = *(iso9660::Pvd*)disc->pvd;

    // Not ISO 9660 format disc
    if (strncmp(pvd.id, "\1CD001\1", 8)) {
        // If the disc doesn't contain an ISO filesystem
        // and it's a CUE image, it's probably a CD audio image
        bool has_tracks = (disc->ext == DISC_EXT_CUE) ||
                          (disc->ext == DISC_EXT_MDS) ||
                          (disc->ext == DISC_EXT_MDF);

        if (has_tracks) {
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

int get_type(Disc* disc) {
    int media = detect_media(disc);
    int type = detect_type(disc);

    if (type == DISC_TYPE_INVALID) {
        return iop::disc::CDVD_DISC_INVALID;
    }

    if (type == DISC_TYPE_CDDA) {
        return iop::disc::CDVD_DISC_CDDA;
    }

    // Start final detection
    char buf[2048];

    if (!read_sector(disc, (unsigned char*)buf, 16, DISC_SS_DATA)) {
        return iop::disc::CDVD_DISC_INVALID;
    }

    iso9660::Pvd pvd = *(iso9660::Pvd*)buf;
    iso9660::Dirent* root = (iso9660::Dirent*)pvd.root;

    // Root points to unreadable sector
    if (!read_sector(disc, (unsigned char*)buf, root->lba_le, DISC_SS_DATA)) {
        return iop::disc::CDVD_DISC_INVALID;
    }

    iso9660::Dirent* dir = (iso9660::Dirent*)buf;

    while (dir->dr_len) {
        if (dir->id_len == 12) {
            if (!strcmp((char*)&dir->id, "SYSTEM.CNF;1")) {
                break;
            }
        }

        // iris_debug(disc, "dir=\'{}\'", &dir->id);

        uint8_t* ptr = (uint8_t*)dir;

        dir = (iso9660::Dirent*)(ptr + dir->dr_len);
    }

    // Couldn't find SYSTEM.CNF file, non-playstation disc
    if (!dir->dr_len) {
        // Might be a DVD Video disc
        // Look for VIDEO_TS in the root dir
        dir = (iso9660::Dirent*)buf;

        while (dir->dr_len) {
            // Search directories
            if ((dir->flags & 2) == 0) goto next;
            if (dir->id_len != 8) goto next;

            if (!strncmp((char*)&dir->id, "VIDEO_TS", dir->id_len)) {
                return iop::disc::CDVD_DISC_DVD_VIDEO;
            }

            // iris_debug(disc, "dir=\'{}\' ({})", (char*)&dir->id, dir->id_len);

            next:;

            uint8_t* ptr = (uint8_t*)dir;

            dir = (iso9660::Dirent*)(ptr + dir->dr_len);
        }

        // SYSTEM.CNF not found and VIDEO_TS not found
        // If the PLAYSTATION string is in the PVD, then this might actually
        // be part of a multi-disc set, return as a PlayStation disc
        // Otherwise it's probably something else entirely (like an Xbox disc)
        // The PS2 wouldn't handle it anyways, return as invalid.

        // The Linux for PlayStation 2 install disc is an example of this.
        // Disc 2 contains a valid ISO filesystem and the PLAYSTATION string,
        // but no SYSTEM.CNF file.
        if (media == DISC_MEDIA_DVD) {
            return type == DISC_TYPE_GAME ? iop::disc::CDVD_DISC_PS2_DVD : iop::disc::CDVD_DISC_INVALID;
        }

        return type == DISC_TYPE_GAME ? iop::disc::CDVD_DISC_PS2_CD : iop::disc::CDVD_DISC_INVALID;
    }

    // SYSTEM.CNF points to unreadable sector, invalid
    if (!read_sector(disc, (unsigned char*)buf, dir->lba_le, DISC_SS_DATA)) {
        return iop::disc::CDVD_DISC_INVALID;
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

        // iris_debug(disc, "key: {}", key);

        // BOOT entry found, PlayStation CD
        if (!strncmp(key, "BOOT", 64))
            return iop::disc::CDVD_DISC_PSX_CD;

        // BOOT2 entry found, PlayStation 2 disc
        if (!strncmp(key, "BOOT2", 64)) {
            return media == DISC_MEDIA_CD ? iop::disc::CDVD_DISC_PS2_CD : iop::disc::CDVD_DISC_PS2_DVD;
        }
    }

    // Couldn't find BOOT or BOOT2 entry, invalid/bootleg PlayStation disc?
    return DISC_TYPE_INVALID;
}

int read_sector(Disc* disc, unsigned char* buf, uint64_t lba, int size) {
    if (!disc)
        return 0;

    if (!disc->read_sector)
        return 0;

    return disc->read_sector(disc->udata, buf, lba, size);
}

uint64_t get_size(Disc* disc) {
    if (!disc)
        return 0;

    if (!disc->read_sector)
        return 0;

    return disc->get_size(disc->udata);
}

uint64_t get_volume_lba(Disc* disc, int vol) {
    if (!disc)
        return 0;

    if (!disc->read_sector)
        return 0;

    if (!vol)
        return 0;

    if (!disc->layer2_lba) {
        detect_media(disc);
    }

    return disc->layer2_lba;
}

int get_sector_size(Disc* disc) {
    if (!disc)
        return 0;

    if (!disc->get_sector_size)
        return 0;

    return disc->get_sector_size(disc->udata);
}

int get_track_count(Disc* disc) {
    if (!disc)
        return 0;

    if (!disc->get_track_count)
        return 0;

    return disc->get_track_count(disc->udata);
}

int get_track_info(Disc* disc, int track, TrackInfo* info) {
    if (!disc)
        return 0;

    if (!disc->get_track_info)
        return 0;

    return disc->get_track_info(disc->udata, track, info);
}

int get_track_number(Disc* disc, uint64_t lba) {
    if (!disc)
        return 0;

    if (!disc->get_track_number)
        return 0;

    return disc->get_track_number(disc->udata, lba);
}

void close(Disc* disc) {
    switch (disc->ext) {
        // Standard raw 2048-byte sector ISO 9660 image
        // usually used for DVDs
        case DISC_EXT_ISO: {
            iso::destroy((iso::Iso*)disc->udata);
        } break;

        case DISC_EXT_CUE: {
            cue::destroy((cue::Cue*)disc->udata);
        } break;

        // Raw 2352-byte sector disc image (CD). open() routes an unknown
        // extension here too, so both cases have to be freed
        case DISC_EXT_NONE:
        case DISC_EXT_BIN: {
            bin::destroy((bin::Bin*)disc->udata);
        } break;

        case DISC_EXT_CHD: {
            chd::destroy((chd::Chd*)disc->udata);
        } break;

        case DISC_EXT_CSO:
        case DISC_EXT_ZSO: {
            ciso::destroy((ciso::Ciso*)disc->udata);
        } break;

        case DISC_EXT_MDS:
        case DISC_EXT_MDF: {
            mds::destroy((mds::Mds*)disc->udata);
        } break;
    }

    delete disc;
}

char* get_serial(Disc* disc, char* serial) {
    if (!disc)
        return NULL;

    // No game serial
    if (!fetch_system_cnf(disc))
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
                iris_debug(disc, "iso: Expected =");

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

    iris_debug(disc, "iso: Couldn't find BOOT2 entry in SYSTEM.CNF (PlayStation disc?)");

    return NULL;
}

char* get_boot_path(Disc* disc) {
    if (disc->boot_path_cached)
        return disc->boot_path;

    // No boot path
    if (!fetch_system_cnf(disc))
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

        // iris_debug(disc, "key: {}", key);

        if (!strncmp(key, "BOOT2", 64)) {
            while (isspace(*p)) ++p;

            if (*p != '=') {
                iris_debug(disc, "iso: Expected =");

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

    iris_debug(disc, "iso: Couldn't find BOOT2 entry in SYSTEM.CNF (PlayStation disc?)");

    return NULL;
}

char* read_boot_elf(Disc* disc, int s) {
    char* boot_path = get_boot_path(disc);

    iris_debug(disc, "iso: Reading boot ELF from disc at path \'{}\'...", boot_path);

    if (!boot_path) {
        iris_debug(disc, "iso: No boot path found in SYSTEM.CNF");

        return NULL;
    }

    if (!fetch_root(disc)) {
        iris_debug(disc, "iso: Couldn't fetch root directory");

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

    iso9660::Dirent* dir = (iso9660::Dirent*)disc->root;

    while (dir->dr_len) {
        if (!strncmp((char*)&dir->id, path, dir->id_len)) {
            int32_t size = dir->size_le;
            uint32_t lba = dir->lba_le;

            char* buf = (char *)malloc(((size >> 11) + 2) << 11);
            char* ptr = buf;

            iris_debug(disc, "iso: Boot ELF found at lba={:08x} size={:08x}", lba, dir->size_le);

            while (size > 0) {
                if (!read_sector(disc, (unsigned char*)ptr, lba++, DISC_SS_DATA)) {
                    iris_debug(disc, "iso: Couldn't read boot ELF sector {}", lba - 1);

                    return NULL;
                }

                ptr += 2048;
                size -= 2048;
            }

            if (size != 0) {
                // Read the last sector, which might be smaller than 2048 bytes
                if (!read_sector(disc, (unsigned char*)ptr, lba, DISC_SS_DATA)) {
                    iris_debug(disc, "iso: Couldn't read boot ELF sector {}", lba);

                    return NULL;
                }
            }

            return buf;
        }

        uint8_t* ptr = (uint8_t*)dir;

        dir = (iso9660::Dirent*)(ptr + dir->dr_len);
    }

    // Couldn't find the boot ELF file in the root directory
    return NULL;
}

#undef PACKED

}
