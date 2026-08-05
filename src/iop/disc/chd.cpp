#include <cctype>

#include "chd.hpp"

namespace iris::iop::disc::chd {

int get_sector_type_size(const char* type) {
    if (strncmp(type, "MODE1", 64) == 0) {
        return 2048;
    } else if (strncmp(type, "MODE1_RAW", 64) == 0) {
        return 2352;
    } else if (strncmp(type, "MODE2", 64) == 0) {
        return 2336;
    } else if (strncmp(type, "MODE2_FORM1", 64) == 0) {
        return 2048;
    } else if (strncmp(type, "MODE2_FORM2", 64) == 0) {
        return 2324;
    } else if (strncmp(type, "MODE2_FORM_MIX", 64) == 0) {
        return 2336;
    } else if (strncmp(type, "MODE2_RAW", 64) == 0) {
        return 2352;
    }

    return 0;
}

Chd* create(logger::Logger* logger) {
    Chd* chd = new Chd();

    chd->logger = logger;
    chd->logger_id = logger::register_source(logger, "chd");

    return chd;
}

int init(Chd* chd, const char* path) {
    if (chd_open(path, CHD_OPEN_READ, NULL, &chd->file)) {
        chd_close(chd->file);

        return 0;
    }

    chd->header = chd_get_header(chd->file);
    chd->buffer = (uint8_t *)malloc(chd->header->hunkbytes);

    // Get sector size from metadata
    char buf[512], type[64];
    uint32_t len, tag;
    uint8_t flags;

    if (chd_get_metadata(chd->file, CDROM_TRACK_METADATA2_TAG, 0, buf, 512, &len, &tag, &flags)) {
        iris_debug(chd, "Failed to get metadata");

        return 0;
    }

    if (tag != CDROM_TRACK_METADATA2_TAG) {
        iris_debug(chd, "Failed to get track metadata");

        return 0;
    }

    // Parse track type
    char* c = buf;
    char* t = type;

    while (*c != ':') c++;

    c++;

    while (*c != ':') c++;

    c++;

    while (((t - type) < 64) && !isspace(*c)) {
        *t++ = *c++;
    }

    *t = '\0';

    chd->sector_size = get_sector_type_size(type);

    if (!chd->sector_size) {
        iris_debug(chd, "Unsupported sector type \'{}\'", type);

        return 0;
    }

    return 1;
}

void destroy(Chd* chd) {
    chd_close(chd->file);
    
    free(chd->buffer);
    delete chd;
}

static inline size_t find_hunk_number(Chd* chd, uint64_t offset) {
    return offset / chd->header->hunkbytes;
}

// Disc IF
int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size) {
    Chd* chd = (Chd*)udata;

    uint64_t offset = lba * chd->header->unitbytes;

    size_t hunknum = find_hunk_number(chd, offset);

    if (chd->cached_hunknum != hunknum) {
        chd->cached_hunknum = hunknum;

        // Cache hunk
        chd_read(chd->file, hunknum, chd->buffer);
    } else {
        uint8_t* base = chd->buffer + (offset % chd->header->hunkbytes);

        // Hunk is cached
        if (chd->sector_size == 2048) {
            memcpy(buf, base, 2048);
        } else {
            if (size == DISC_SS_DATA) {
                memcpy(buf, base + 0x18, 2048);
            } else {
                memcpy(buf, base, 2352);
            }
        }

        return 1;
    }

    // Read cached hunk
    return read_sector(udata, buf, lba, size);
}

uint64_t get_size(void* udata) {
    Chd* chd = (Chd*)udata;

    return chd->sector_size * chd->header->hunkcount;
}

int get_sector_size(void* udata) {
    Chd* chd = (Chd*)udata;

    return chd->sector_size;
}

int get_track_count(void* udata) {
    return 1;
}

int get_track_info(void* udata, int track, TrackInfo* info) {
    return 0;
}

int get_track_number(void* udata, uint64_t lba) {
    return 1;
}

#undef fseek64
#undef ftell64

}
