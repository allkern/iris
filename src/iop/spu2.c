#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "spu2.h"

struct ps2_spu2* ps2_spu2_create(void) {
    return (struct ps2_spu2*)malloc(sizeof(struct ps2_spu2));
}

void ps2_spu2_init(struct ps2_spu2* spu2) {
    memset(spu2, 0, sizeof(struct ps2_spu2));

    // CORE0/1 DMA status (ready)
    P_STAT(0) = 0x80;
    P_STAT(1) = 0x80;
}

void spu2_kon(struct ps2_spu2* spu2, int core, uint32_t v) {
    printf("spu2: core%d kon %04x\n", core, v);
    for (int i = 0; i < 1; i++) {
        if (!(v & (1 << i)))
            continue;

        spu2->c[core].v[i].playing = 1;
    }
}

void spu2_data(struct ps2_spu2* spu2, int core, uint16_t data) {
    printf("spu2: core%d tsa=%08x data=%04x\n", core, A_TSA(core), data);

    spu2->ram[A_TSA(core)++] = data;
}

int spu2_handle_write(struct ps2_spu2* spu2, uint32_t addr, uint64_t data) {
    int high = 8 * (addr & 2);
    int core = (addr >> 10) & 1;

    switch (addr & 0x7ff) {
        case 0x1a0: case 0x1a2: case 0x5a0: case 0x5a2: {
            spu2_kon(spu2, core, data << high);
        } return 1;

        case 0x1a8: case 0x1aa: case 0x5a8: case 0x5aa: {
            printf("addr=%x %08x tsa=%x core=%d\n", addr, data, A_TSA(core), core);
        } return 1;

        case 0x1ac: case 0x5ac: {
            printf("spu2: abcd\n");
            spu2_data(spu2, core, data);
        } return 1;
    }

    return 0;
}

uint64_t ps2_spu2_read16(struct ps2_spu2* spu2, uint32_t addr) {
    if (addr == 0x1f900344) {
        P_STAT(0) ^= 0x80;
    }

    if (addr == 0x1f900744) {
        P_STAT(1) ^= 0x80;
    }

    return *(uint16_t*)(&spu2->r[addr & 0x7ff]);
}

void ps2_spu2_write16(struct ps2_spu2* spu2, uint32_t addr, uint64_t data) {
    if (spu2_handle_write(spu2, addr, data))
        return;

    *(uint16_t*)(&spu2->r[addr & 0x7ff]) = data;
}

void ps2_spu2_destroy(struct ps2_spu2* spu2) {
    free(spu2);
}

struct spu2_sample ps2_spu2_get_sample(struct ps2_spu2* spu2) {
    struct spu2_sample s;

    s.u32 = 0;

    for (int i = 0; i < 24; i++) {
        if (!spu2->c[0].v[i].playing)
            continue;
    }

    return s;
}