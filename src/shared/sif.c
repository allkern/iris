#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sif.h"

struct ps2_sif* ps2_sif_create(void) {
    return malloc(sizeof(struct ps2_sif));
}

void ps2_sif_init(struct ps2_sif* sif, int side) {
    memset(sif, 0, sizeof(struct ps2_sif));

    sif->side = side;
    sif->ctrl = 0xf0000012;
}

void ps2_sif_destroy(struct ps2_sif* sif) {
    free(sif);
}

uint64_t ps2_sif_read32(struct ps2_sif* sif, uint32_t addr) {
    switch (addr) {
        case 0x0000: return sif->mscom;
        case 0x0010: return sif->smcom;
        case 0x0020: return sif->msflg;
        case 0x0030: return sif->smflg;
        case 0x0040: return sif->ctrl;
        case 0x0060: return sif->bd6;
    }

    return 0;
}

void ps2_sif_write32(struct ps2_sif* sif, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x0000: sif->mscom = data; return;
        case 0x0010: sif->smcom = data; return;
        case 0x0020: sif->msflg = data; return;
        case 0x0030: sif->smflg = data; return;
        case 0x0040: sif->ctrl = data; return;
        case 0x0060: sif->bd6 = data; return;
    }
}