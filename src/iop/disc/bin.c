#include <stdlib.h>

#include "../disc.h"
#include "bin.h"

#ifdef _WIN32
#define fseek64 fseeko64
#define ftell64 ftello64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

struct disc_bin* bin_create(void) {
    return malloc(sizeof(struct disc_bin));
}

int bin_init(struct disc_bin* bin, const char* path) {
    bin->file = fopen(path, "rb");

    if (!bin->file) {
        free(bin);

        return 1;
    }

    return 0;
}

void bin_destroy(struct disc_bin* bin) {
    fclose(bin->file);
    free(bin);
}

// Disc IF
int bin_read_sector(void* udata, unsigned char* buf, uint64_t lba, int size) {
    struct disc_bin* bin = (struct disc_bin*)udata;

    int s, r;

    if (size == DISC_SS_DATA) {
        s = fseek64(bin->file, (lba * 2352) + 0x18, SEEK_SET);
        r = fread(buf, 1, 2048, bin->file);
    } else {
        s = fseek64(bin->file, lba * 2352, SEEK_SET);
        r = fread(buf, 1, 2352, bin->file);
    }

    return r && !s;
}

uint64_t bin_get_size(void* udata) {
    struct disc_bin* bin = (struct disc_bin*)udata;

    fseek64(bin->file, 0, SEEK_END);

    return ftell64(bin->file);
}

uint64_t bin_get_volume_lba(void* udata) {
    return 0;
}

int bin_get_sector_size(void* udata) {
    return 2352;
}

#undef fseek64
#undef ftell64