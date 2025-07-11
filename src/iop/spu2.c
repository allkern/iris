#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "spu2.h"

static const int16_t g_spu_gauss_table[] = {
    -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001,
    -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0003, 0x0003,
    0x0003, 0x0004, 0x0004, 0x0005, 0x0005, 0x0006, 0x0007, 0x0007,
    0x0008, 0x0009, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E,
    0x000F, 0x0010, 0x0011, 0x0012, 0x0013, 0x0015, 0x0016, 0x0018,
    0x0019, 0x001B, 0x001C, 0x001E, 0x0020, 0x0021, 0x0023, 0x0025,
    0x0027, 0x0029, 0x002C, 0x002E, 0x0030, 0x0033, 0x0035, 0x0038,
    0x003A, 0x003D, 0x0040, 0x0043, 0x0046, 0x0049, 0x004D, 0x0050,
    0x0054, 0x0057, 0x005B, 0x005F, 0x0063, 0x0067, 0x006B, 0x006F,
    0x0074, 0x0078, 0x007D, 0x0082, 0x0087, 0x008C, 0x0091, 0x0096,
    0x009C, 0x00A1, 0x00A7, 0x00AD, 0x00B3, 0x00BA, 0x00C0, 0x00C7,
    0x00CD, 0x00D4, 0x00DB, 0x00E3, 0x00EA, 0x00F2, 0x00FA, 0x0101,
    0x010A, 0x0112, 0x011B, 0x0123, 0x012C, 0x0135, 0x013F, 0x0148,
    0x0152, 0x015C, 0x0166, 0x0171, 0x017B, 0x0186, 0x0191, 0x019C,
    0x01A8, 0x01B4, 0x01C0, 0x01CC, 0x01D9, 0x01E5, 0x01F2, 0x0200,
    0x020D, 0x021B, 0x0229, 0x0237, 0x0246, 0x0255, 0x0264, 0x0273,
    0x0283, 0x0293, 0x02A3, 0x02B4, 0x02C4, 0x02D6, 0x02E7, 0x02F9,
    0x030B, 0x031D, 0x0330, 0x0343, 0x0356, 0x036A, 0x037E, 0x0392,
    0x03A7, 0x03BC, 0x03D1, 0x03E7, 0x03FC, 0x0413, 0x042A, 0x0441,
    0x0458, 0x0470, 0x0488, 0x04A0, 0x04B9, 0x04D2, 0x04EC, 0x0506,
    0x0520, 0x053B, 0x0556, 0x0572, 0x058E, 0x05AA, 0x05C7, 0x05E4,
    0x0601, 0x061F, 0x063E, 0x065C, 0x067C, 0x069B, 0x06BB, 0x06DC,
    0x06FD, 0x071E, 0x0740, 0x0762, 0x0784, 0x07A7, 0x07CB, 0x07EF,
    0x0813, 0x0838, 0x085D, 0x0883, 0x08A9, 0x08D0, 0x08F7, 0x091E,
    0x0946, 0x096F, 0x0998, 0x09C1, 0x09EB, 0x0A16, 0x0A40, 0x0A6C,
    0x0A98, 0x0AC4, 0x0AF1, 0x0B1E, 0x0B4C, 0x0B7A, 0x0BA9, 0x0BD8,
    0x0C07, 0x0C38, 0x0C68, 0x0C99, 0x0CCB, 0x0CFD, 0x0D30, 0x0D63,
    0x0D97, 0x0DCB, 0x0E00, 0x0E35, 0x0E6B, 0x0EA1, 0x0ED7, 0x0F0F,
    0x0F46, 0x0F7F, 0x0FB7, 0x0FF1, 0x102A, 0x1065, 0x109F, 0x10DB,
    0x1116, 0x1153, 0x118F, 0x11CD, 0x120B, 0x1249, 0x1288, 0x12C7,
    0x1307, 0x1347, 0x1388, 0x13C9, 0x140B, 0x144D, 0x1490, 0x14D4,
    0x1517, 0x155C, 0x15A0, 0x15E6, 0x162C, 0x1672, 0x16B9, 0x1700,
    0x1747, 0x1790, 0x17D8, 0x1821, 0x186B, 0x18B5, 0x1900, 0x194B,
    0x1996, 0x19E2, 0x1A2E, 0x1A7B, 0x1AC8, 0x1B16, 0x1B64, 0x1BB3,
    0x1C02, 0x1C51, 0x1CA1, 0x1CF1, 0x1D42, 0x1D93, 0x1DE5, 0x1E37,
    0x1E89, 0x1EDC, 0x1F2F, 0x1F82, 0x1FD6, 0x202A, 0x207F, 0x20D4,
    0x2129, 0x217F, 0x21D5, 0x222C, 0x2282, 0x22DA, 0x2331, 0x2389,
    0x23E1, 0x2439, 0x2492, 0x24EB, 0x2545, 0x259E, 0x25F8, 0x2653,
    0x26AD, 0x2708, 0x2763, 0x27BE, 0x281A, 0x2876, 0x28D2, 0x292E,
    0x298B, 0x29E7, 0x2A44, 0x2AA1, 0x2AFF, 0x2B5C, 0x2BBA, 0x2C18,
    0x2C76, 0x2CD4, 0x2D33, 0x2D91, 0x2DF0, 0x2E4F, 0x2EAE, 0x2F0D,
    0x2F6C, 0x2FCC, 0x302B, 0x308B, 0x30EA, 0x314A, 0x31AA, 0x3209,
    0x3269, 0x32C9, 0x3329, 0x3389, 0x33E9, 0x3449, 0x34A9, 0x3509,
    0x3569, 0x35C9, 0x3629, 0x3689, 0x36E8, 0x3748, 0x37A8, 0x3807,
    0x3867, 0x38C6, 0x3926, 0x3985, 0x39E4, 0x3A43, 0x3AA2, 0x3B00,
    0x3B5F, 0x3BBD, 0x3C1B, 0x3C79, 0x3CD7, 0x3D35, 0x3D92, 0x3DEF,
    0x3E4C, 0x3EA9, 0x3F05, 0x3F62, 0x3FBD, 0x4019, 0x4074, 0x40D0,
    0x412A, 0x4185, 0x41DF, 0x4239, 0x4292, 0x42EB, 0x4344, 0x439C,
    0x43F4, 0x444C, 0x44A3, 0x44FA, 0x4550, 0x45A6, 0x45FC, 0x4651,
    0x46A6, 0x46FA, 0x474E, 0x47A1, 0x47F4, 0x4846, 0x4898, 0x48E9,
    0x493A, 0x498A, 0x49D9, 0x4A29, 0x4A77, 0x4AC5, 0x4B13, 0x4B5F,
    0x4BAC, 0x4BF7, 0x4C42, 0x4C8D, 0x4CD7, 0x4D20, 0x4D68, 0x4DB0,
    0x4DF7, 0x4E3E, 0x4E84, 0x4EC9, 0x4F0E, 0x4F52, 0x4F95, 0x4FD7,
    0x5019, 0x505A, 0x509A, 0x50DA, 0x5118, 0x5156, 0x5194, 0x51D0,
    0x520C, 0x5247, 0x5281, 0x52BA, 0x52F3, 0x532A, 0x5361, 0x5397,
    0x53CC, 0x5401, 0x5434, 0x5467, 0x5499, 0x54CA, 0x54FA, 0x5529,
    0x5558, 0x5585, 0x55B2, 0x55DE, 0x5609, 0x5632, 0x565B, 0x5684,
    0x56AB, 0x56D1, 0x56F6, 0x571B, 0x573E, 0x5761, 0x5782, 0x57A3,
    0x57C3, 0x57E2, 0x57FF, 0x581C, 0x5838, 0x5853, 0x586D, 0x5886,
    0x589E, 0x58B5, 0x58CB, 0x58E0, 0x58F4, 0x5907, 0x5919, 0x592A,
    0x593A, 0x5949, 0x5958, 0x5965, 0x5971, 0x597C, 0x5986, 0x598F,
    0x5997, 0x599E, 0x59A4, 0x59A9, 0x59AD, 0x59B0, 0x59B2, 0x59B3
};

struct ps2_spu2* ps2_spu2_create(void) {
    return (struct ps2_spu2*)malloc(sizeof(struct ps2_spu2));
}

void ps2_spu2_init(struct ps2_spu2* spu2, struct ps2_iop_dma* dma, struct ps2_iop_intc* intc, struct sched_state* sched) {
    memset(spu2, 0, sizeof(struct ps2_spu2));

    spu2->dma = dma;
    spu2->intc = intc;
    spu2->sched = sched;

    // CORE0/1 DMA status (ready)
    spu2->c[0].stat = 0x80;
    spu2->c[1].stat = 0x80;
    spu2->c[0].endx = 0x00ffffff;
    spu2->c[1].endx = 0x00ffffff;
}

void spu2_irq(struct ps2_spu2* spu2, int c) {
    // if (spu2->spdif_irq & (4 << c))
    //     return;

    // spu2->spdif_irq |= 4 << c;

    // printf("spu2: IRQ fired\n");

    ps2_iop_intc_irq(spu2->intc, IOP_INTC_SPU2);
}

void spu2_check_irq(struct ps2_spu2* spu2, uint32_t addr) {
    for (int i = 0; i < 2; i++) {
        if (addr == spu2->c[i].irqa && (spu2->c[i].attr & (1 << 6))) {
            spu2_irq(spu2, i);
        }
    }
}

void spu2_decode_adpcm_block(struct ps2_spu2* spu2, struct spu2_voice* v);
void adsr_load_attack(struct ps2_spu2* spu2, struct spu2_core* c, struct spu2_voice* v);
void adsr_load_release(struct ps2_spu2* spu2, struct spu2_core* c, struct spu2_voice* v, int i);

void spu2_write_kon(struct ps2_spu2* spu2, int c, int h, uint64_t data) {
    // printf("spu2: core%d kon%c %04x\n", c, h ? 'h' : 'l', data);

    for (int i = 0; i < 16; i++) {
        if (!(data & (1 << i)))
            continue;

        int idx = i+h*16;

        if (idx >= 24)
            break;

        struct spu2_voice* v = &spu2->c[c].v[idx];
        struct spu2_core* cr = &spu2->c[c];

        // Make sure to clear the internal state of a voice
        // before playing
        v->playing = 1;
        v->counter = 0;
        v->h[0] = 0;
        v->h[1] = 0;

        for (int i = 0; i < 28; i++)
            v->buf[i] = 0;

        v->loop_start = 0;
        v->loop = 0;
        v->loop_end = 0;
        v->prev_sample_index = 0;

        for (int i = 0; i < 4; i++)
            v->s[i] = 0;

        v->adsr_cycles_left = 0;
        v->adsr_phase = 0;
        v->adsr_cycles_reload = 0;
        v->adsr_cycles = 0;
        v->adsr_mode = 0;
        v->adsr_dir = 0;
        v->adsr_shift = 0;
        v->adsr_step = 0;
        v->adsr_pending_step = 0;
        v->adsr_sustain_level = 0;

        v->playing = 1;
        v->nax = v->ssa;
        v->adsr_sustain_level = ((v->adsr1 & 0xf) + 1) * 0x800;

        cr->endx &= ~(1u << idx);

        // printf("spu2: CORE%d Voice %d playing, ssa=%08x lsax=%08x nax=%08x voll=%04x volr=%04x\n", c, i+h*16, v->ssa, v->lsax, v->nax, v->voll, v->volr);

        adsr_load_attack(spu2, cr, v);
        spu2_decode_adpcm_block(spu2, v);
    }
}

void spu2_write_koff(struct ps2_spu2* spu2, int c, int h, uint64_t data) {
    // printf("spu2: core%d koff%c %04x\n", c, h ? 'h' : 'l', data);

    for (int i = 0; i < 16; i++) {
        if (!(data & (1 << i)))
            continue;

        int v = i+h*16;

        if (v >= 24)
            break;

        // spu2->c[c].v[i+h*16].playing = 0;

        // To-do: Enter ADSR release
        // printf("spu2: voice %d koff\n", v);
        adsr_load_release(spu2, &spu2->c[c], &spu2->c[c].v[v], v);
    }
}

void adma_write_data(struct ps2_spu2* spu2, int c, uint64_t data) {
    if (spu2->c[c].memin_write_addr < 0x100) {
        spu2->ram[(c ? 0x2400 : 0x2000) + ((spu2->c[c].memin_write_addr++) & 0xff)] = data;
    } else if (spu2->c[c].memin_write_addr < 0x200) {
        spu2->ram[(c ? 0x2600 : 0x2200) + ((spu2->c[c].memin_write_addr++) & 0xff)] = data;
    }
    // } else if (spu2->c[c].memin_write_addr < 0x300) {
    //     spu2->ram[(c ? 0x2500 : 0x2100) + ((spu2->c[c].memin_write_addr++) & 0xff)] = data;
    // } else if (spu2->c[c].memin_write_addr < 0x400) {
    //     spu2->ram[(c ? 0x2700 : 0x2300) + ((spu2->c[c].memin_write_addr++) & 0xff)] = data;
    // }
}

void spu2_write_data(struct ps2_spu2* spu2, int c, uint64_t data) {
    // printf("spu2: core%d data=%04x tsa=%08x\n", c, data, spu2->c[c].tsa);
    if (spu2->c[c].admas & (1 << c)) {
        adma_write_data(spu2, c, data);

        return;
    }

    spu2_check_irq(spu2, spu2->c[c].tsa);

    spu2->ram[spu2->c[c].tsa++] = data;

    spu2->c[c].tsa &= 0xfffff;
}

void spu2_core0_reset_handler(void* udata, int overshoot) {
    struct ps2_spu2* spu2 = (struct ps2_spu2*)udata;

    printf("spu2: core0 Reset\n");

    spu2->c[0].stat |= 0x80;
}

void spu2_core1_reset_handler(void* udata, int overshoot) {
    struct ps2_spu2* spu2 = (struct ps2_spu2*)udata;

    printf("spu2: core1 Reset\n");

    spu2->c[1].stat |= 0x80;
}

void spu2_write_attr(struct ps2_spu2* spu2, int c, uint64_t data) {
    // if (spu2->c[c].attr & (1 << 6)) {
    //     if (!(data & (1 << 6))) {
    //         spu2->spdif_irq &= ~(4 << c);
    //     }
    // }

    spu2->c[c].attr = data & 0x7fff;

    if (data & 0x8000) {
        struct sched_event event;

        printf("spu2: Resetting core%d\n", c);

        event.callback = c ? spu2_core1_reset_handler : spu2_core0_reset_handler;
        event.cycles = 10000;
        event.name = "SPU2 Reset";
        event.udata = spu2;

        sched_schedule(spu2->sched, event);

        spu2->c[c].stat = 0;
    }
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
            case 0x340: return spu2->c[core].endx >> 16;
            case 0x342: return spu2->c[core].endx & 0xffff;
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
        // case 0x7C2: return spu2->spdif_irq;
        case 0x7C6: return spu2->spdif_mode;
        case 0x7C8: return spu2->spdif_media;
        case 0x7CA: return spu2->spdif_copy;
    }

    printf("spu2: Unhandled register %x read\n", addr & 0x7ff);

    return 0;
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
            case 0x19A: spu2_write_attr(spu2, core, data); return;
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

        addr -= (core ? 0x5c0 : 0x1c0);

        int voice = addr / 12;

        switch (addr % 12) {
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
            case 0x340: SPU2_WRITEH(core, endx); return;
            case 0x342: SPU2_WRITEL(core, endx); return;
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
        // case 0x7C2: spu2->spdif_irq = data; return;
        case 0x7C6: spu2->spdif_mode = data; return;
        case 0x7C8: spu2->spdif_media = data; return;
        case 0x7CA: spu2->spdif_copy = data; return;
    }

    printf("spu2: Unhandled register %x write (%04lx)\n", addr & 0x7ff, data);
}

void ps2_spu2_destroy(struct ps2_spu2* spu2) {
    free(spu2);
}

static const struct spu2_sample silence = {
    .u32 = 0
};

static const int ps_adpcm_coefs_i[5][2] = {
    {   0 ,   0 },
    {  60 ,   0 },
    { 115 , -52 },
    {  98 , -55 },
    { 122 , -60 },
};

void spu2_decode_adpcm_block(struct ps2_spu2* spu2, struct spu2_voice* v) {
    uint16_t hdr = spu2->ram[v->nax];

    // if (v->nax == spu2->c[0].irqa || v->nax == spu2->c[1].irqa)
    //     ps2_iop_intc_irq(spu2->intc, IOP_INTC_SPU2);

    v->loop_end = (hdr >> 8) & 1;
    v->loop = (hdr >> 9) & 1;
    v->loop_start = (hdr >> 10) & 1;

    // printf("spu2: start=%d loop=%d end=%d\n", v->loop_start, v->loop, v->loop_end);

    int shift_factor = (hdr & 0xf);
    int coef_index = ((hdr >> 4) & 0xF);

    int32_t f0 = ps_adpcm_coefs_i[coef_index][0];
    int32_t f1 = ps_adpcm_coefs_i[coef_index][1];

    for (int i = 0; i < 28; i++) {
        int sh = (i & 3) * 4;

        uint32_t addr = v->nax + 1 + (i >> 2);
        uint16_t n = (spu2->ram[addr] & (0xf << sh)) >> sh;

        // Don't send SPU2 IRQs
        // if (addr == spu2->c[0].irqa || addr == spu2->c[1].irqa)
        //     ps2_iop_intc_irq(spu2->intc, IOP_INTC_SPU2);

        // Sign extend t
        int32_t t = (int16_t)((n << 12) & 0xf000) >> shift_factor;

        t += (f0 * v->h[0] + f1 * v->h[1]) >> 6;
        t = (t < INT16_MIN) ? INT16_MIN : ((t > INT16_MAX) ? INT16_MAX : t);

        v->h[1] = v->h[0];
        v->h[0] = t;
        v->buf[i] = t;

        // printf("spu2: buf[%d]=%04x n=%04x nax=%08x\n", i, s, n, v->nax);
    }
}

#define PHASE v->adsr_phase
#define CYCLES v->adsr_cycles
#define EXPONENTIAL v->adsr_mode
#define DECREASE v->adsr_dir
#define SHIFT v->adsr_shift
#define STEP v->adsr_step
#define LEVEL_STEP v->adsr_pending_step
#define LEVEL v->envx
#define CLAMP(v, l, h) (((v) <= (l)) ? (l) : (((v) >= (h)) ? (h) : (v)))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

enum {
    ADSR_ATTACK,
    ADSR_DECAY,
    ADSR_SUSTAIN,
    ADSR_RELEASE,
    ADSR_END
};

void adsr_calculate_values(struct ps2_spu2* spu2, struct spu2_voice* v) {
    CYCLES = 1 << MAX(0, SHIFT - 11);
    LEVEL_STEP = STEP << MAX(0, 11 - SHIFT);

    if (EXPONENTIAL && (LEVEL > 0x6000) && !DECREASE)
        CYCLES *= 4;

    if (EXPONENTIAL && DECREASE)
        LEVEL_STEP = (LEVEL_STEP * LEVEL) >> 15;

    v->adsr_cycles_reload = CYCLES;
}

void adsr_load_attack(struct ps2_spu2* spu2, struct spu2_core* c, struct spu2_voice* v) {
    v->adsr_phase = ADSR_ATTACK;
    v->adsr_mode  = (v->adsr1 >> 15) & 1;
    v->adsr_shift = (v->adsr1 >> 10) & 0x1f;
    v->adsr_step  = 7 - ((v->adsr1 >> 8) & 3);
    v->adsr_dir   = 0;
    v->envx       = 0;

    adsr_calculate_values(spu2, v);

    // printf("adsr: attack mode=%d shift=%d step=%d dir=%d envx=%d\n",
    //     v->adsr_mode,
    //     v->adsr_shift,
    //     v->adsr_step,
    //     v->adsr_dir,
    //     v->envx
    // );
}

void adsr_load_decay(struct ps2_spu2* spu2, struct spu2_core* c, struct spu2_voice* v) {
    v->adsr_phase = ADSR_DECAY;
    v->adsr_mode  = 1;
    v->adsr_dir   = 1;
    v->adsr_shift = (v->adsr1 >> 4) & 0xf;
    v->adsr_step  = -8;
    v->envx       = 0x7fff;

    adsr_calculate_values(spu2, v);
}

void adsr_load_sustain(struct ps2_spu2* spu2, struct spu2_core* c, struct spu2_voice* v) {
    v->adsr_phase = ADSR_SUSTAIN;
    v->adsr_mode  = (v->adsr2 >> 15) & 1;
    v->adsr_dir   = (v->adsr2 >> 14) & 1;
    v->adsr_shift = (v->adsr2 >> 8) & 0x1f;
    v->adsr_step  = (v->adsr2 >> 6) & 3;
    v->adsr_step  = v->adsr_dir ? (-8 + v->adsr_step) : (7 - v->adsr_step);
    v->envx       = v->adsr_sustain_level;

    adsr_calculate_values(spu2, v);
}

void adsr_load_release(struct ps2_spu2* spu2, struct spu2_core* c, struct spu2_voice* v, int i) {
    v->adsr_phase = ADSR_RELEASE;
    v->adsr_mode  = (v->adsr2 >> 5) & 1;
    v->adsr_dir   = 1;
    v->adsr_shift = v->adsr2 & 0x1f;
    v->adsr_step  = -8;

    c->endx |= 1u << i;

    adsr_calculate_values(spu2, v);
}

void spu2_handle_adsr(struct ps2_spu2* spu2, struct spu2_core* c, struct spu2_voice* v) {
    if (CYCLES) {
        CYCLES -= 1;

        return;
    }

    adsr_calculate_values(spu2, v);

    int level = v->envx;

    level += LEVEL_STEP;

    switch (v->adsr_phase) {
        case ADSR_ATTACK: {
            level = CLAMP(level, 0x0000, 0x7fff);

            if (level == 0x7fff)
                adsr_load_decay(spu2, c, v);
        } break;

        case ADSR_DECAY: {
            level = CLAMP(level, 0x0000, 0x7fff);

            if (level <= v->adsr_sustain_level)
                adsr_load_sustain(spu2, c, v);
        } break;

        case ADSR_SUSTAIN: {
            level = CLAMP(level, 0x0000, 0x7fff);

            /* Not stopped automatically, need to KOFF */
        } break;

        case ADSR_RELEASE: {
            level = CLAMP(level, 0x0000, 0x7fff);

            if (!level) {
                PHASE = ADSR_END;
                CYCLES = 0;

                v->envx = 0;
                v->playing = 0;
            }
        } break;

        case ADSR_END: {
            v->envx = 0;
            v->playing = 0;
        } break;
    }

    v->envx = level;

    CYCLES = v->adsr_cycles_reload;
}

#undef PHASE
#undef CYCLES
#undef MODE
#undef DIR
#undef SHIFT
#undef STEP
#undef PENDING_STEP
#undef CLAMP
#undef MAX

struct spu2_sample spu2_get_voice_sample(struct ps2_spu2* spu2, int cr, int vc) {
    if (!spu2->c[cr].v[vc].playing)
        return silence;

    struct spu2_core* c = &spu2->c[cr];
    struct spu2_voice* v = &c->v[vc];
    struct spu2_sample s;

    int sample_index = v->counter >> 12;

    spu2_handle_adsr(spu2, c, v);

    if (sample_index > 27) {
        sample_index -= 28;

        v->counter &= 0xfff;
        v->counter |= sample_index << 12;

        // if (v->loop_end) {
        //     c->endx |= 1 << vc;

        //     if (!v->loop) {
        //         adsr_load_release(spu2, c, v, vc);
        //     }
        // } else if (v->loop_start) {
        //     v->lsax = v->nax;
        // } else {
        //     v->nax += 8;
        // }

        if (v->loop_start) {
            spu2_check_irq(spu2, v->nax);

            v->lsax = v->nax;

            v->nax += 8;
        } else if (v->loop_end) {
            // printf("spu2: Voice %d loop end at 0x%08x (lsax=%08x ssa=%08x) loop=%d\n", vc, v->nax, v->lsax, v->ssa, v->loop);
            v->nax = v->lsax;

            spu2_check_irq(spu2, v->nax);

            if (!v->loop) {
                adsr_load_release(spu2, c, v, vc);

                v->envx = 0;
                v->playing = 0;
            }
        } else {
            spu2_check_irq(spu2, v->nax);

            v->nax += 8;
        }

        spu2_decode_adpcm_block(spu2, v);
    }

    if (v->prev_sample_index != sample_index) {
        v->s[3] = v->s[2];
        v->s[2] = v->s[1];
        v->s[1] = v->s[0];
    }

    v->s[0] = v->buf[sample_index];

    // Apply 4-point Gaussian interpolation
    uint8_t gauss_index = (v->counter >> 4) & 0xff;
    int32_t g0 = g_spu_gauss_table[0x0ff - gauss_index];
    int32_t g1 = g_spu_gauss_table[0x1ff - gauss_index];
    int32_t g2 = g_spu_gauss_table[0x100 + gauss_index];
    int32_t g3 = g_spu_gauss_table[0x000 + gauss_index];
    int32_t out;

    out  = (g0 * v->s[3]) >> 15;
    out += (g1 * v->s[2]) >> 15;
    out += (g2 * v->s[1]) >> 15;
    out += (g3 * v->s[0]) >> 15;

    s.s16[0] = (out * v->voll) >> 15;
    s.s16[1] = (out * v->volr) >> 15;
    s.s16[0] = ((int32_t)s.s16[0] * v->envx) >> 15;
    s.s16[1] = ((int32_t)s.s16[1] * v->envx) >> 15;

    v->counter += v->pitch;

    v->prev_sample_index = sample_index;

    return s;
}

static inline struct spu2_sample spu2_get_adma_sample(struct ps2_spu2* spu2, int c) {
    if (spu2->c[c].memin_write_addr < 0x200)
        return silence;

    spu2->c[c].adma_playing = 1;

    struct spu2_sample s = silence;

    s.s16[0] = spu2->ram[(c ? 0x2400 : 0x2000) + spu2->c[c].memin_read_addr];
    s.s16[1] = spu2->ram[(c ? 0x2600 : 0x2200) + spu2->c[c].memin_read_addr];

    spu2->c[c].memin_read_addr++;

    if (spu2->c[c].memin_read_addr == 0x100) {
        spu2->c[c].memin_read_addr = 0;
        spu2->c[c].memin_write_addr = 0;
        spu2->c[c].adma_playing = 0;

        // Request more data
        if (c == 0) {
            iop_dma_handle_spu1_adma(spu2->dma);
        } else {
            iop_dma_handle_spu2_adma(spu2->dma);
        }
    }

    return s;
}

struct spu2_sample ps2_spu2_get_adma_sample(struct ps2_spu2* spu2, int c) {
    return spu2_get_adma_sample(spu2, c);
}

struct spu2_sample ps2_spu2_get_sample(struct ps2_spu2* spu2) {
    struct spu2_sample s = silence;

    s.u16[0] = 0;
    s.u16[1] = 0;

    // ADMA
    struct spu2_sample c0_adma = spu2_get_adma_sample(spu2, 0);
    struct spu2_sample c1_adma = spu2_get_adma_sample(spu2, 1);

    s.s16[0] += c0_adma.s16[0];
    s.s16[1] += c0_adma.s16[1];
    s.s16[0] += c1_adma.s16[0];
    s.s16[1] += c1_adma.s16[1];

    for (int i = 0; i < 24; i++) {
        struct spu2_sample c0 = spu2_get_voice_sample(spu2, 0, i);
        struct spu2_sample c1 = spu2_get_voice_sample(spu2, 1, i);

        s.s16[0] += c0.s16[0];
        s.s16[1] += c0.s16[1];
        s.s16[0] += c1.s16[0];
        s.s16[1] += c1.s16[1];
    }

    return s;
}

struct spu2_sample ps2_spu2_get_voice_sample(struct ps2_spu2* spu2, int c, int v) {
    return spu2_get_voice_sample(spu2, c, v);
}

int spu2_is_adma_active(struct ps2_spu2* spu2, int c) {
    return spu2->c[c].memin_write_addr >= 0x200;
}

void spu2_start_adma(struct ps2_spu2* spu2, int c) {
    spu2->c[c].adma_playing = 0;
    spu2->c[c].memin_read_addr = 0;
    spu2->c[c].memin_write_addr = 0;
}