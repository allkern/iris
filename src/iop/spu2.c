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
    spu2->c[0].stat = 0x80;
    spu2->c[1].stat = 0x80;
}

void spu2_write_kon(struct ps2_spu2* spu2, int c, int h, uint64_t data) {
    for (int i = 0; i < 16; i++) {
        if (!(data & (1 << i)))
            continue;

        spu2->c[c].v[i+h*16].playing = 1;
    }
}

void spu2_write_koff(struct ps2_spu2* spu2, int c, int h, uint64_t data) {
    for (int i = 0; i < 16; i++) {
        if (!(data & (1 << i)))
            continue;

        spu2->c[c].v[i+h*16].playing = 0;

        // To-do: Enter ADSR release
    }
}

void spu2_write_data(struct ps2_spu2* spu2, int c, uint64_t data) {
    // printf("spu2: core%d data=%04x tsa=%08x\n", c, data, spu2->c[c].tsa);
    spu2->ram[spu2->c[c].tsa++] = data;
}

uint64_t ps2_spu2_read16(struct ps2_spu2* spu2, uint32_t addr) {
    addr &= 0x7ff;

    if ((addr >= 0x000 && addr <= 0x17f) || (addr >= 0x400 && addr <= 0x57f)) {
        int core = (addr >> 10) & 1;
        int voice = (addr >> 4) & 0x1f;

        switch (addr & 0xf) {
            case 0x0: return spu2->c[core].v[voice].voll;
            case 0x2: return spu2->c[core].v[voice].volr;
            case 0x4: return spu2->c[core].v[voice].pitch;
            case 0x6: return spu2->c[core].v[voice].adsr1;
            case 0x8: return spu2->c[core].v[voice].adsr2;
            case 0xA: return spu2->c[core].v[voice].envx;
            case 0xC: return spu2->c[core].v[voice].volxl;
            case 0xE: return spu2->c[core].v[voice].volxr;
        }
    }

    if ((addr >= 0x180 && addr <= 0x1bf) || (addr >= 0x580 && addr <= 0x5bf)) {
        int core = (addr >> 10) & 1;

        switch (addr & 0x3ff) {
            case 0x180: return spu2->c[core].pmon & 0xffff;
            case 0x182: return spu2->c[core].pmon >> 16;
            case 0x184: return spu2->c[core].non & 0xffff;
            case 0x186: return spu2->c[core].non >> 16;
            case 0x188: return spu2->c[core].vmixl & 0xffff;
            case 0x18A: return spu2->c[core].vmixl >> 16;
            case 0x18C: return spu2->c[core].vmixel & 0xffff;
            case 0x18E: return spu2->c[core].vmixel >> 16;
            case 0x190: return spu2->c[core].vmixr & 0xffff;
            case 0x192: return spu2->c[core].vmixr >> 16;
            case 0x194: return spu2->c[core].vmixer & 0xffff;
            case 0x196: return spu2->c[core].vmixer >> 16;
            case 0x198: return spu2->c[core].mmix;
            case 0x19A: return spu2->c[core].attr;
            case 0x19C: return spu2->c[core].irqa >> 16;
            case 0x19E: return spu2->c[core].irqa & 0xffff;
            case 0x1A0: return spu2->c[core].kon & 0xffff;
            case 0x1A2: return spu2->c[core].kon >> 16;
            case 0x1A4: return spu2->c[core].koff & 0xffff;
            case 0x1A6: return spu2->c[core].koff >> 16;
            case 0x1A8: return spu2->c[core].tsa >> 16;
            case 0x1AA: return spu2->c[core].tsa & 0xffff;
            case 0x1AC: return spu2->c[core].data;
            case 0x1AE: return spu2->c[core].ctrl;
            case 0x1B0: return spu2->c[core].admas;
            case 0x2E0: return spu2->c[core].esa >> 16;
            case 0x2E2: return spu2->c[core].esa & 0xffff;
        }
    }

    if ((addr >= 0x1c0 && addr <= 0x2df) || (addr >= 0x5c0 && addr <= 0x6df)) {
        int core = (addr >> 10) & 1;
        int voice = (addr - (0x1c0 + 0x400 * core)) / 12;

        switch (addr % 0xc) {
            case 0x0: return spu2->c[core].v[voice].ssa >> 16;
            case 0x2: return spu2->c[core].v[voice].ssa & 0xffff;
            case 0x4: return spu2->c[core].v[voice].lsax >> 16;
            case 0x6: return spu2->c[core].v[voice].lsax & 0xffff;
            case 0x8: return spu2->c[core].v[voice].nax >> 16;
            case 0xA: return spu2->c[core].v[voice].nax & 0xffff;
        }
    }

    if ((addr >= 0x2e0 && addr <= 0x347) || (addr >= 0x6e0 && addr <= 0x747)) {
        int core = (addr >> 10) & 1;

        switch (addr & 0x3ff) {
            case 0x2E0: return spu2->c[core].esa >> 16;
            case 0x2E2: return spu2->c[core].esa & 0xffff;
            case 0x2E4: return spu2->c[core].fb_src_a & 0xffff;
            case 0x2E6: return spu2->c[core].fb_src_a >> 16;
            case 0x2E8: return spu2->c[core].fb_src_b & 0xffff;
            case 0x2EA: return spu2->c[core].fb_src_b >> 16;
            case 0x2EC: return spu2->c[core].iir_dest_a0 & 0xffff;
            case 0x2EE: return spu2->c[core].iir_dest_a0 >> 16;
            case 0x2F0: return spu2->c[core].iir_dest_a1 & 0xffff;
            case 0x2F2: return spu2->c[core].iir_dest_a1 >> 16;
            case 0x2F4: return spu2->c[core].acc_src_a0 & 0xffff;
            case 0x2F6: return spu2->c[core].acc_src_a0 >> 16;
            case 0x2F8: return spu2->c[core].acc_src_a1 & 0xffff;
            case 0x2FA: return spu2->c[core].acc_src_a1 >> 16;
            case 0x2FC: return spu2->c[core].acc_src_b0 & 0xffff;
            case 0x2FE: return spu2->c[core].acc_src_b0 >> 16;
            case 0x300: return spu2->c[core].acc_src_b1 & 0xffff;
            case 0x302: return spu2->c[core].acc_src_b1 >> 16;
            case 0x304: return spu2->c[core].iir_src_a0 & 0xffff;
            case 0x306: return spu2->c[core].iir_src_a0 >> 16;
            case 0x308: return spu2->c[core].iir_src_a1 & 0xffff;
            case 0x30A: return spu2->c[core].iir_src_a1 >> 16;
            case 0x30C: return spu2->c[core].iir_dest_b0 & 0xffff;
            case 0x30E: return spu2->c[core].iir_dest_b0 >> 16;
            case 0x310: return spu2->c[core].iir_dest_b1 & 0xffff;
            case 0x312: return spu2->c[core].iir_dest_b1 >> 16;
            case 0x314: return spu2->c[core].acc_src_c0 & 0xffff;
            case 0x316: return spu2->c[core].acc_src_c0 >> 16;
            case 0x318: return spu2->c[core].acc_src_c1 & 0xffff;
            case 0x31A: return spu2->c[core].acc_src_c1 >> 16;
            case 0x31C: return spu2->c[core].acc_src_d0 & 0xffff;
            case 0x31E: return spu2->c[core].acc_src_d0 >> 16;
            case 0x320: return spu2->c[core].acc_src_d1 & 0xffff;
            case 0x322: return spu2->c[core].acc_src_d1 >> 16;
            case 0x324: return spu2->c[core].iir_src_b1 & 0xffff;
            case 0x326: return spu2->c[core].iir_src_b1 >> 16;
            case 0x328: return spu2->c[core].iir_src_b0 & 0xffff;
            case 0x32A: return spu2->c[core].iir_src_b0 >> 16;
            case 0x32C: return spu2->c[core].mix_dest_a0 & 0xffff;
            case 0x32E: return spu2->c[core].mix_dest_a0 >> 16;
            case 0x330: return spu2->c[core].mix_dest_a1 & 0xffff;
            case 0x332: return spu2->c[core].mix_dest_a1 >> 16;
            case 0x334: return spu2->c[core].mix_dest_b0 & 0xffff;
            case 0x336: return spu2->c[core].mix_dest_b0 >> 16;
            case 0x338: return spu2->c[core].mix_dest_b1 & 0xffff;
            case 0x33A: return spu2->c[core].mix_dest_b1 >> 16;
            case 0x33C: return spu2->c[core].eea >> 16;
            case 0x33E: return spu2->c[core].eea & 0xffff;
            case 0x340: return spu2->c[core].endx & 0xffff;
            case 0x342: return spu2->c[core].endx >> 16;
            case 0x344: return spu2->c[core].stat;
            case 0x346: return spu2->c[core].ends;
        }
    }

    // Misc.
    switch (addr & 0x7ff) {
        case 0x760: return spu2->c[0].mvoll;
        case 0x762: return spu2->c[0].mvolr;
        case 0x764: return spu2->c[0].evoll;
        case 0x766: return spu2->c[0].evolr;
        case 0x768: return spu2->c[0].avoll;
        case 0x76A: return spu2->c[0].avolr;
        case 0x76C: return spu2->c[0].bvoll;
        case 0x76E: return spu2->c[0].bvolr;
        case 0x770: return spu2->c[0].mvolxl;
        case 0x772: return spu2->c[0].mvolxr;
        case 0x774: return spu2->c[0].iir_alpha;
        case 0x776: return spu2->c[0].acc_coef_a;
        case 0x778: return spu2->c[0].acc_coef_b;
        case 0x77A: return spu2->c[0].acc_coef_c;
        case 0x77C: return spu2->c[0].acc_coef_d;
        case 0x77E: return spu2->c[0].iir_coef;
        case 0x780: return spu2->c[0].fb_alpha;
        case 0x782: return spu2->c[0].fb_x;
        case 0x784: return spu2->c[0].in_coef_l;
        case 0x786: return spu2->c[0].in_coef_r;
        case 0x788: return spu2->c[1].mvoll;
        case 0x78a: return spu2->c[1].mvolr;
        case 0x78c: return spu2->c[1].evoll;
        case 0x78e: return spu2->c[1].evolr;
        case 0x790: return spu2->c[1].avoll;
        case 0x792: return spu2->c[1].avolr;
        case 0x794: return spu2->c[1].bvoll;
        case 0x796: return spu2->c[1].bvolr;
        case 0x798: return spu2->c[1].mvolxl;
        case 0x79a: return spu2->c[1].mvolxr;
        case 0x79c: return spu2->c[1].iir_alpha;
        case 0x79e: return spu2->c[1].acc_coef_a;
        case 0x7a0: return spu2->c[1].acc_coef_b;
        case 0x7a2: return spu2->c[1].acc_coef_c;
        case 0x7a4: return spu2->c[1].acc_coef_d;
        case 0x7a6: return spu2->c[1].iir_coef;
        case 0x7a8: return spu2->c[1].fb_alpha;
        case 0x7aa: return spu2->c[1].fb_x;
        case 0x7ac: return spu2->c[1].in_coef_l;
        case 0x7ae: return spu2->c[1].in_coef_r;
        case 0x7C0: return spu2->spdif_out;
        case 0x7C6: return spu2->spdif_mode;
        case 0x7C8: return spu2->spdif_media;
        case 0x7CA: return spu2->spdif_copy;
    }

    printf("spu2: Unhandled register %x read\n", addr & 0x7ff);

    exit(1);
}

#define SPU2_WRITEL(cr, r) { spu2->c[cr].r &= 0xffff0000; spu2->c[cr].r |= data; }
#define SPU2_WRITEH(cr, r) { spu2->c[cr].r &= 0x0000ffff; spu2->c[cr].r |= data << 16; }
#define SPU2_WRITEL_V(cr, vc, r) { spu2->c[cr].v[vc].r &= 0xffff0000; spu2->c[cr].v[vc].r |= data; }
#define SPU2_WRITEH_V(cr, vc, r) { spu2->c[cr].v[vc].r &= 0x0000ffff; spu2->c[cr].v[vc].r |= data << 16; }

void ps2_spu2_write16(struct ps2_spu2* spu2, uint32_t addr, uint64_t data) {
    addr &= 0x7ff;

    if ((addr >= 0x000 && addr <= 0x17f) || (addr >= 0x400 && addr <= 0x57f)) {
        int core = (addr >> 10) & 1;
        int voice = (addr >> 4) & 0x1f;

        switch (addr & 0xf) {
            case 0x0: spu2->c[core].v[voice].voll = data; return;
            case 0x2: spu2->c[core].v[voice].volr = data; return;
            case 0x4: spu2->c[core].v[voice].pitch = data; return;
            case 0x6: spu2->c[core].v[voice].adsr1 = data; return;
            case 0x8: spu2->c[core].v[voice].adsr2 = data; return;
            case 0xA: spu2->c[core].v[voice].envx = data; return;
            case 0xC: spu2->c[core].v[voice].volxl = data; return;
            case 0xE: spu2->c[core].v[voice].volxr = data; return;
        }
    }

    if ((addr >= 0x180 && addr <= 0x1bf) || (addr >= 0x580 && addr <= 0x5bf)) {
        int core = (addr >> 10) & 1;

        switch (addr & 0x3ff) {
            case 0x180: SPU2_WRITEL(core, pmon); return;
            case 0x182: SPU2_WRITEH(core, pmon); return;
            case 0x184: SPU2_WRITEL(core, non); return;
            case 0x186: SPU2_WRITEH(core, non); return;
            case 0x188: SPU2_WRITEL(core, vmixl); return;
            case 0x18A: SPU2_WRITEH(core, vmixl); return;
            case 0x18C: SPU2_WRITEL(core, vmixel); return;
            case 0x18E: SPU2_WRITEH(core, vmixel); return;
            case 0x190: SPU2_WRITEL(core, vmixr); return;
            case 0x192: SPU2_WRITEH(core, vmixr); return;
            case 0x194: SPU2_WRITEL(core, vmixer); return;
            case 0x196: SPU2_WRITEH(core, vmixer); return;
            case 0x198: spu2->c[core].mmix = data; return;
            case 0x19A: spu2->c[core].attr = data; return;
            case 0x19C: SPU2_WRITEH(core, irqa); return;
            case 0x19E: SPU2_WRITEL(core, irqa); return;
            case 0x1A0: spu2_write_kon(spu2, core, 0, data); return;
            case 0x1A2: spu2_write_kon(spu2, core, 1, data); return;
            case 0x1A4: spu2_write_koff(spu2, core, 0, data); return;
            case 0x1A6: spu2_write_koff(spu2, core, 1, data); return;
            case 0x1A8: SPU2_WRITEH(core, tsa); return;
            case 0x1AA: SPU2_WRITEL(core, tsa); return;
            case 0x1AC: spu2_write_data(spu2, core, data); return;
            case 0x1AE: spu2->c[core].ctrl = data; return;
            case 0x1B0: spu2->c[core].admas = data; return;
        }
    }

    if ((addr >= 0x1c0 && addr <= 0x2df) || (addr >= 0x5c0 && addr <= 0x6df)) {
        int core = (addr >> 10) & 1;
        int voice = (addr - (0x1c0 + 0x400 * core)) / 12;

        switch (addr % 0xc) {
            case 0x0: SPU2_WRITEH_V(core, voice, ssa); return;
            case 0x2: SPU2_WRITEL_V(core, voice, ssa); return;
            case 0x4: SPU2_WRITEH_V(core, voice, lsax); return;
            case 0x6: SPU2_WRITEL_V(core, voice, lsax); return;
            case 0x8: SPU2_WRITEH_V(core, voice, nax); return;
            case 0xA: SPU2_WRITEL_V(core, voice, nax); return;
        }
    }

    if ((addr >= 0x2e0 && addr <= 0x347) || (addr >= 0x6e0 && addr <= 0x747)) {
        int core = (addr >> 10) & 1;

        switch (addr & 0x3ff) {
            case 0x2E0: SPU2_WRITEH(core, esa); return;
            case 0x2E2: SPU2_WRITEL(core, esa); return;
            case 0x2E4: SPU2_WRITEL(core, fb_src_a); return;
            case 0x2E6: SPU2_WRITEH(core, fb_src_a); return;
            case 0x2E8: SPU2_WRITEL(core, fb_src_b); return;
            case 0x2EA: SPU2_WRITEH(core, fb_src_b); return;
            case 0x2EC: SPU2_WRITEL(core, iir_dest_a0); return;
            case 0x2EE: SPU2_WRITEH(core, iir_dest_a0); return;
            case 0x2F0: SPU2_WRITEL(core, iir_dest_a1); return;
            case 0x2F2: SPU2_WRITEH(core, iir_dest_a1); return;
            case 0x2F4: SPU2_WRITEL(core, acc_src_a0); return;
            case 0x2F6: SPU2_WRITEH(core, acc_src_a0); return;
            case 0x2F8: SPU2_WRITEL(core, acc_src_a1); return;
            case 0x2FA: SPU2_WRITEH(core, acc_src_a1); return;
            case 0x2FC: SPU2_WRITEL(core, acc_src_b0); return;
            case 0x2FE: SPU2_WRITEH(core, acc_src_b0); return;
            case 0x300: SPU2_WRITEL(core, acc_src_b1); return;
            case 0x302: SPU2_WRITEH(core, acc_src_b1); return;
            case 0x304: SPU2_WRITEL(core, iir_src_a0); return;
            case 0x306: SPU2_WRITEH(core, iir_src_a0); return;
            case 0x308: SPU2_WRITEL(core, iir_src_a1); return;
            case 0x30A: SPU2_WRITEH(core, iir_src_a1); return;
            case 0x30C: SPU2_WRITEL(core, iir_dest_b0); return;
            case 0x30E: SPU2_WRITEH(core, iir_dest_b0); return;
            case 0x310: SPU2_WRITEL(core, iir_dest_b1); return;
            case 0x312: SPU2_WRITEH(core, iir_dest_b1); return;
            case 0x314: SPU2_WRITEL(core, acc_src_c0); return;
            case 0x316: SPU2_WRITEH(core, acc_src_c0); return;
            case 0x318: SPU2_WRITEL(core, acc_src_c1); return;
            case 0x31A: SPU2_WRITEH(core, acc_src_c1); return;
            case 0x31C: SPU2_WRITEL(core, acc_src_d0); return;
            case 0x31E: SPU2_WRITEH(core, acc_src_d0); return;
            case 0x320: SPU2_WRITEL(core, acc_src_d1); return;
            case 0x322: SPU2_WRITEH(core, acc_src_d1); return;
            case 0x324: SPU2_WRITEL(core, iir_src_b1); return;
            case 0x326: SPU2_WRITEH(core, iir_src_b1); return;
            case 0x328: SPU2_WRITEL(core, iir_src_b0); return;
            case 0x32A: SPU2_WRITEH(core, iir_src_b0); return;
            case 0x32C: SPU2_WRITEL(core, mix_dest_a0); return;
            case 0x32E: SPU2_WRITEH(core, mix_dest_a0); return;
            case 0x330: SPU2_WRITEL(core, mix_dest_a1); return;
            case 0x332: SPU2_WRITEH(core, mix_dest_a1); return;
            case 0x334: SPU2_WRITEL(core, mix_dest_b0); return;
            case 0x336: SPU2_WRITEH(core, mix_dest_b0); return;
            case 0x338: SPU2_WRITEL(core, mix_dest_b1); return;
            case 0x33A: SPU2_WRITEH(core, mix_dest_b1); return;
            case 0x33C: SPU2_WRITEH(core, eea); return;
            case 0x33E: SPU2_WRITEL(core, eea); return;
            case 0x340: SPU2_WRITEL(core, endx); return;
            case 0x342: SPU2_WRITEH(core, endx); return;
            case 0x344: spu2->c[core].stat = data; return;
            case 0x346: spu2->c[core].ends = data; return;
        }
    }

    // Misc.
    switch (addr & 0x7ff) {
        case 0x760: spu2->c[0].mvoll = data; return;
        case 0x762: spu2->c[0].mvolr = data; return;
        case 0x764: spu2->c[0].evoll = data; return;
        case 0x766: spu2->c[0].evolr = data; return;
        case 0x768: spu2->c[0].avoll = data; return;
        case 0x76A: spu2->c[0].avolr = data; return;
        case 0x76C: spu2->c[0].bvoll = data; return;
        case 0x76E: spu2->c[0].bvolr = data; return;
        case 0x770: spu2->c[0].mvolxl = data; return;
        case 0x772: spu2->c[0].mvolxr = data; return;
        case 0x774: spu2->c[0].iir_alpha = data; return;
        case 0x776: spu2->c[0].acc_coef_a = data; return;
        case 0x778: spu2->c[0].acc_coef_b = data; return;
        case 0x77A: spu2->c[0].acc_coef_c = data; return;
        case 0x77C: spu2->c[0].acc_coef_d = data; return;
        case 0x77E: spu2->c[0].iir_coef = data; return;
        case 0x780: spu2->c[0].fb_alpha = data; return;
        case 0x782: spu2->c[0].fb_x = data; return;
        case 0x784: spu2->c[0].in_coef_l = data; return;
        case 0x786: spu2->c[0].in_coef_r = data; return;
        case 0x788: spu2->c[1].mvoll = data; return;
        case 0x78a: spu2->c[1].mvolr = data; return;
        case 0x78c: spu2->c[1].evoll = data; return;
        case 0x78e: spu2->c[1].evolr = data; return;
        case 0x790: spu2->c[1].avoll = data; return;
        case 0x792: spu2->c[1].avolr = data; return;
        case 0x794: spu2->c[1].bvoll = data; return;
        case 0x796: spu2->c[1].bvolr = data; return;
        case 0x798: spu2->c[1].mvolxl = data; return;
        case 0x79a: spu2->c[1].mvolxr = data; return;
        case 0x79c: spu2->c[1].iir_alpha = data; return;
        case 0x79e: spu2->c[1].acc_coef_a = data; return;
        case 0x7a0: spu2->c[1].acc_coef_b = data; return;
        case 0x7a2: spu2->c[1].acc_coef_c = data; return;
        case 0x7a4: spu2->c[1].acc_coef_d = data; return;
        case 0x7a6: spu2->c[1].iir_coef = data; return;
        case 0x7a8: spu2->c[1].fb_alpha = data; return;
        case 0x7aa: spu2->c[1].fb_x = data; return;
        case 0x7ac: spu2->c[1].in_coef_l = data; return;
        case 0x7ae: spu2->c[1].in_coef_r = data; return;
        case 0x7C0: spu2->spdif_out = data; return;
        case 0x7C6: spu2->spdif_mode = data; return;
        case 0x7C8: spu2->spdif_media = data; return;
        case 0x7CA: spu2->spdif_copy = data; return;
    }

    printf("spu2: Unhandled register %x write (%04x)\n", addr & 0x7ff, data);
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