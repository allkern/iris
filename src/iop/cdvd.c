#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cdvd.h"

struct ps2_cdvd* ps2_cdvd_create(void) {
    return malloc(sizeof(struct ps2_cdvd));
}

void ps2_cdvd_init(struct ps2_cdvd* cdvd) {
    memset(cdvd, 0, sizeof(struct ps2_cdvd));
}

void ps2_cdvd_destroy(struct ps2_cdvd* cdvd) {
    free(cdvd->s_fifo);
    free(cdvd);
}

// To-do: Disc images
void ps2_cdvd_open(struct ps2_cdvd* cdvd, const char* path);
void ps2_cdvd_close(struct ps2_cdvd* cdvd);

uint64_t ps2_cdvd_read8(struct ps2_cdvd* cdvd, uint32_t addr) {
    return 0;
}

void ps2_cdvd_write8(struct ps2_cdvd* cdvd, uint32_t addr, uint64_t data) {
    return;
}