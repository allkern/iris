#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sif.h"

struct ps2_sif* ps2_sif_create(void) {
    return malloc(sizeof(struct ps2_sif));
}

void ps2_sif_init(struct ps2_sif* sif) {
    memset(sif, 0, sizeof(struct ps2_sif));

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

void ps2_sif_fifo_write(struct ps2_sif* sif, uint128_t data) {
    printf("writing %016lx %016lx to SIF FIFO\n", data.u64[1], data.u64[0]);

    sif->fifo.data[sif->fifo.write_index++] = data;
}

uint128_t ps2_sif_fifo_read(struct ps2_sif* sif) {
    if (sif->fifo.read_index == sif->fifo.write_index)
        return (uint128_t)0;

    uint128_t q = sif->fifo.data[sif->fifo.read_index++];

    return q;
}

void ps2_sif_fifo_reset(struct ps2_sif* sif) {
    sif->fifo.read_index = 0;
    sif->fifo.write_index = 0;
}

int ps2_sif_fifo_is_empty(struct ps2_sif* sif) {
    return sif->fifo.read_index == sif->fifo.write_index;
}