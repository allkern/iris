#include "raw.h"

#include <stdlib.h>
#include <string.h>

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

struct raw_state* raw_open(const char* path) {
    struct raw_state* state = malloc(sizeof(struct raw_state));

    if (!state) {
        return NULL;
    }

    state->file = fopen(path, "r+b");

    if (!state->file) {
        fprintf(stderr, "raw: Unable to open file\n");

        free(state);

        return NULL;
    }

    fseek64(state->file, 0, SEEK_END);

    state->sector_count = ftell64(state->file) / 512;

    return state;
}

uint64_t raw_get_sector_count(struct raw_state* state) {
    return state->sector_count;
}

void raw_read_sector(struct raw_state* state, uint64_t lba, uint8_t* buf) {
    fseek64(state->file, lba * 512, SEEK_SET);
    fread(buf, 512, 1, state->file);
}

void raw_write_sector(struct raw_state* state, uint64_t lba, const uint8_t* buf) {
    fseek64(state->file, lba * 512, SEEK_SET);
    fwrite(buf, 512, 1, state->file);
}

int raw_get_identify(struct raw_state* state, uint8_t* buf) {
    // We can't really get identify data from a raw image, so we'll just return zero
    memset(buf, 0, 512);

    return 0;
}

void raw_close(struct raw_state* state) {
    if (state->file) {
        fclose(state->file);
    }

    free(state);
}

void ata_raw_read_sector(void* udata, uint64_t lba, uint8_t* buf) {
    struct raw_state* state = (struct raw_state*)udata;

    raw_read_sector(state, lba, buf);

}
void ata_raw_write_sector(void* udata, uint64_t lba, const uint8_t* buf) {
    struct raw_state* state = (struct raw_state*)udata;

    raw_write_sector(state, lba, buf);
}

int ata_raw_get_identify(void* udata, uint8_t* buf) {
    struct raw_state* state = (struct raw_state*)udata;

    return raw_get_identify(state, buf);
}

uint64_t ata_raw_get_sector_count(void* udata) {
    struct raw_state* state = (struct raw_state*)udata;

    return raw_get_sector_count(state);
}

void ata_raw_close(void* udata) {
    struct raw_state* state = (struct raw_state*)udata;

    raw_close(state);
}