#include <stdlib.h>

#include "iso.h"

#ifdef _MSC_VER
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#elif defined(_WIN32)
#define fseek64 fseeko64
#define ftell64 ftello64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

struct disc_iso* iso_create(void) {
    return malloc(sizeof(struct disc_iso));
}

int iso_init(struct disc_iso* iso, const char* path) {
    iso->file = fopen(path, "rb");

    if (!iso->file) {
        free(iso);

        return 1;
    }

    return 0;
}

void iso_destroy(struct disc_iso* iso) {
    fclose(iso->file);
    free(iso);
}

// Disc IF
int iso_read_sector(void* udata, unsigned char* buf, uint64_t lba, int size) {
    struct disc_iso* iso = (struct disc_iso*)udata;

    int s, r;

    s = fseek64(iso->file, lba * 0x800, SEEK_SET);
    r = fread(buf, 1, 0x800, iso->file);

    return r && !s;
}

uint64_t iso_get_size(void* udata) {
    struct disc_iso* iso = (struct disc_iso*)udata;

    fseek64(iso->file, 0, SEEK_END);

    return ftell64(iso->file);
}

uint64_t iso_get_volume_lba(void* udata) {
    return 0;
}

int iso_get_sector_size(void* udata) {
    return 2048;
}

#undef fseek64
#undef ftell64