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
    free(sif->sif0.data);
    free(sif->sif1.data);
    free(sif);
}

uint64_t ps2_sif_read32(struct ps2_sif* sif, uint32_t addr) {
    // IOP read
    switch (addr) {
        case 0x1d000000: return sif->mscom;
        case 0x1d000010: return sif->smcom;
        case 0x1d000020: return sif->msflg;
        case 0x1d000030: return sif->smflg;
        case 0x1d000040: return sif->ctrl | 0xf0000001;
        case 0x1d000060: return sif->bd6;
    }

    // EE read
    switch (addr) {
        case 0x1000f200: return sif->mscom;
        case 0x1000f210: return sif->smcom;
        case 0x1000f220: return sif->msflg;
        case 0x1000f230: return sif->smflg;
        case 0x1000f240: return sif->ctrl | 0xf0000101;
        case 0x1000f260: return sif->bd6;
    }

    return 0;
}

void ps2_sif_write32(struct ps2_sif* sif, uint32_t addr, uint64_t data) {
    // IOP write
    switch (addr) {
        case 0x1d000000: sif->mscom = data; return;
        case 0x1d000010: sif->smcom = data; return;
        case 0x1d000020: sif->msflg &= ~data; return;
        case 0x1d000030: sif->smflg |= data; return;
        case 0x1d000040: sif->ctrl = data; return;
        case 0x1d000060: sif->bd6 = data; return;
    }

    // EE write
    switch (addr) {
        case 0x1000f200: sif->mscom = data; return;
        case 0x1000f210: sif->smcom = data; return;
        case 0x1000f220: sif->msflg |= data; return;
        case 0x1000f230: sif->smflg &= ~data; return;
        case 0x1000f240: sif->ctrl = data; return;
        case 0x1000f260: sif->bd6 = data; return;
    }
}

void ps2_sif0_write(struct ps2_sif* sif, uint128_t data) {
    // printf("writing %016lx %016lx to SIF FIFO\n", data.u64[1], data.u64[0]);

    if (!sif->sif0.capacity) {
        sif->sif0.capacity = 4;
        sif->sif0.data = malloc(sizeof(uint128_t) * 4);
    } else if (sif->sif0.write_index == sif->sif0.capacity) {
        sif->sif0.capacity *= 2;

        uint128_t* ptr = realloc(sif->sif0.data, sizeof(uint128_t) * sif->sif0.capacity);
        
        if (!ptr) {
            printf("sif: Couldn't resize SIF0 buffer\n");

            exit(1);
        }

        sif->sif0.data = ptr;
    }

    sif->sif0.data[sif->sif0.write_index++] = data;
}

uint128_t ps2_sif0_read(struct ps2_sif* sif) {
    // If EE requests more data than the IOP produced, then return the last
    // QW that was actually transferred.
    // This happens during SIF initialization. The IOP triggers a SIF0 transfer
    // that sends 8 words of data (2 QW), the first QW is an EE tag that starts
    // a 2 QW transfer from the SIF FIFO, but the IOP only ever wrote 2 QWs.
    if (sif->sif0.read_index == sif->sif0.write_index)
        return sif->sif0.data[sif->sif0.read_index - 1];

    uint128_t q = sif->sif0.data[sif->sif0.read_index++];

    return q;
}

void ps2_sif0_reset(struct ps2_sif* sif) {
    sif->sif0.read_index = 0;
    sif->sif0.write_index = 0;
}

int ps2_sif0_is_empty(struct ps2_sif* sif) {
    return sif->sif0.read_index == sif->sif0.write_index;
}

void ps2_sif1_write(struct ps2_sif* sif, uint128_t data) {
    // printf("writing %016lx %016lx to SIF FIFO\n", data.u64[1], data.u64[0]);

    if (!sif->sif1.capacity) {
        sif->sif1.capacity = 4;
        sif->sif1.data = malloc(sizeof(uint128_t) * 4);
    } else if (sif->sif1.write_index == sif->sif1.capacity) {
        sif->sif1.capacity *= 2;

        uint128_t* ptr = realloc(sif->sif1.data, sizeof(uint128_t) * sif->sif1.capacity);
        
        if (!ptr) {
            printf("sif: Couldn't resize SIF1 buffer\n");

            exit(1);
        }

        sif->sif1.data = ptr;
    }

    sif->sif1.data[sif->sif1.write_index++] = data;
}

uint128_t ps2_sif1_read(struct ps2_sif* sif) {
    // If EE requests more data than the IOP produced, then return the last
    // QW that was actually transferred.
    // This happens during SIF initialization. The IOP triggers a SIF0 transfer
    // that sends 8 words of data (2 QW), the first QW is an EE tag that starts
    // a 2 QW transfer from the SIF FIFO, but the IOP only ever wrote 2 QWs.
    if (sif->sif1.read_index == sif->sif1.write_index)
        return sif->sif1.data[sif->sif1.read_index - 1];

    uint128_t q = sif->sif1.data[sif->sif1.read_index++];

    return q;
}

void ps2_sif1_reset(struct ps2_sif* sif) {
    sif->sif1.read_index = 0;
    sif->sif1.write_index = 0;
}

int ps2_sif1_is_empty(struct ps2_sif* sif) {
    return sif->sif1.read_index == sif->sif1.write_index;
}