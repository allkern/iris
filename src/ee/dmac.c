#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dmac.h"

// #define printf(fmt, ...)(0)

static inline uint128_t dmac_read_qword(struct ps2_dmac* dmac, uint32_t addr, int mem) {
    int spr = mem || (addr & 0x80000000);

    if (!spr)
        return ee_bus_read128(dmac->bus, addr & 0xfffffff0);

    return ps2_ram_read128(dmac->spr, addr & 0x3ff0);
}

static inline void dmac_write_qword(struct ps2_dmac* dmac, uint32_t addr, int mem, uint128_t value) {
    int spr = mem || (addr & 0x80000000);

    if (!spr) {
        ee_bus_write128(dmac->bus, addr & 0xfffffff0, value);

        return;
    }

    ps2_ram_write128(dmac->spr, addr & 0x3ff0, value);
}

struct ps2_dmac* ps2_dmac_create(void) {
    return malloc(sizeof(struct ps2_dmac));
}

void ps2_dmac_init(struct ps2_dmac* dmac, struct ps2_sif* sif, struct ps2_iop_dma* iop_dma, struct ps2_ram* spr, struct ee_state* ee, struct sched_state* sched, struct ee_bus* bus) {
    memset(dmac, 0, sizeof(struct ps2_dmac));

    dmac->bus = bus;
    dmac->sif = sif;
    dmac->spr = spr;
    dmac->iop_dma = iop_dma;
    dmac->ee = ee;
    dmac->sched = sched;

    // v2+ BIOSes need this value on boot (smh...)
    dmac->enable = 0x1201;
}

void ps2_dmac_destroy(struct ps2_dmac* dmac) {
    free(dmac);
}

static inline struct dmac_channel* dmac_get_channel(struct ps2_dmac* dmac, uint32_t addr) {
    switch (addr & 0xff00) {
        case 0x8000: return &dmac->vif0;
        case 0x9000: return &dmac->vif1;
        case 0xA000: return &dmac->gif;
        case 0xB000: return &dmac->ipu_from;
        case 0xB400: return &dmac->ipu_to;
        case 0xC000: return &dmac->sif0;
        case 0xC400: return &dmac->sif1;
        case 0xC800: return &dmac->sif2;
        case 0xD000: return &dmac->spr_from;
        case 0xD400: return &dmac->spr_to;
    }

    return NULL;
}

static inline const char* dmac_get_channel_name(struct ps2_dmac* dmac, uint32_t addr) {
    switch (addr & 0xff00) {
        case 0x8000: return "vif0";
        case 0x9000: return "vif1";
        case 0xA000: return "gif";
        case 0xB000: return "ipu_from";
        case 0xB400: return "ipu_to";
        case 0xC000: return "sif0";
        case 0xC400: return "sif1";
        case 0xC800: return "sif2";
        case 0xD000: return "spr_from";
        case 0xD400: return "spr_to";
    }

    return NULL;
}

static inline int channel_is_done(struct dmac_channel* ch) {
    return ch->tag.end || (ch->tag.irq && (ch->chcr & 0x80));
}

uint64_t ps2_dmac_read32(struct ps2_dmac* dmac, uint32_t addr) {
    struct dmac_channel* c = dmac_get_channel(dmac, addr);

    if (c) {
        switch (addr & 0xff) {
            case 0x00: if (c == &dmac->ipu_to) printf("dmac: Read channel %s chcr=%08x\n", dmac_get_channel_name(dmac, addr), c->chcr); return c->chcr;
            case 0x10: if (c == &dmac->ipu_to) printf("dmac: Read channel %s madr=%08x\n", dmac_get_channel_name(dmac, addr), c->madr); return c->madr;
            case 0x20: if (c == &dmac->ipu_to) printf("dmac: Read channel %s qwc=%08x\n", dmac_get_channel_name(dmac, addr), c->qwc); return c->qwc;
            case 0x30: return c->tadr;
            case 0x40: return c->asr0;
            case 0x50: return c->asr1;
            case 0x80: return c->sadr;
        }

        // printf("dmac: Unknown channel register %02x\n", addr & 0xff);

        return 0;
    }

    switch (addr) {
        case 0x1000E000: return dmac->ctrl;
        case 0x1000E010: return dmac->stat;
        case 0x1000E020: return dmac->pcr;
        case 0x1000E030: return dmac->sqwc;
        case 0x1000E040: return dmac->rbsr;
        case 0x1000E050: return dmac->rbor;
        case 0x1000F520: return dmac->enable;
        case 0x1000F590: break; // ENABLEW (W)
    }

    return 0;
}

static inline void dmac_process_source_tag(struct ps2_dmac* dmac, struct dmac_channel* c, uint128_t tag) {
    // Set CHCR TAG bytes
    c->chcr &= 0xffff;
    c->chcr |= tag.u32[0] & 0xffff0000;

    c->tag.qwc = TAG_QWC(tag);
    c->tag.pct = TAG_PCT(tag);
    c->tag.id = TAG_ID(tag);
    c->tag.irq = TAG_IRQ(tag);
    c->tag.addr = TAG_ADDR(tag);
    c->tag.mem = TAG_MEM(tag);
    c->tag.data = TAG_DATA(tag);

    // printf("ee: dmac tag %016lx %016lx qwc=%08x id=%d irq=%d addr=%08x mem=%d data=%016lx\n",
    //     tag.u64[1], tag.u64[0],
    //     c->tag.qwc,
    //     c->tag.id,
    //     c->tag.irq,
    //     c->tag.addr,
    //     c->tag.mem,
    //     c->tag.data
    // );

    c->tag.end = 0;
    c->qwc = c->tag.qwc;

    switch (c->tag.id) {
        case 0: { // REFE tag
            c->madr = c->tag.addr;
            c->tadr += 16;
            c->tag.end = 1;
        } break;

        case 1: {
            c->madr = c->tadr + 16;
            c->tadr = c->madr;
        } break;

        case 2: {
            c->madr = c->tadr + 16;
            c->tadr = c->tag.addr;
        } break;

        case 3: {
            c->madr = c->tag.addr;
            c->tadr += 16;
        } break;

        case 4: {
            c->madr = c->tag.addr;
            c->tadr += 16;
        } break;

        case 5: {
            c->madr = c->tadr + 16;

            int asp = (c->chcr >> 4) & 3;

            if (!asp) {
                c->asr0 = c->madr + (c->tag.qwc * 16);
            } else if (asp == 1) {
                c->asr1 = c->madr + (c->tag.qwc * 16);
            }

            c->tadr = c->tag.addr;
            c->chcr += 0x10;
        } break;

        case 6: {
            c->madr = c->tadr + 16;

            int asp = (c->chcr >> 4) & 3;

            if (asp == 2) {
                c->tadr = c->asr1;
                c->chcr -= 0x10;
            } else if (asp == 1) {
                c->tadr = c->asr0;
                c->chcr -= 0x10;
            } else {
                c->tag.end = 1;
            }
        } break;
  
        case 7: {
            c->madr = c->tadr + 16;
            c->tag.end = 1;
        } break;
    }

    // If TIE and TAG.IRQ are set, then end transfer
    if ((c->chcr & 0x80) && c->tag.irq)
        c->tag.end = 1;
}

static inline void dmac_process_dest_tag(struct ps2_dmac* dmac, struct dmac_channel* c, uint128_t tag) {
    // Set CHCR TAG bytes
    c->chcr &= 0xffff;
    c->chcr |= tag.u32[0] & 0xffff0000;

    c->tag.qwc = TAG_QWC(tag);
    c->tag.pct = TAG_PCT(tag);
    c->tag.id = TAG_ID(tag);
    c->tag.irq = TAG_IRQ(tag);
    c->tag.addr = TAG_ADDR(tag);
    c->tag.mem = TAG_MEM(tag);
    c->tag.data = TAG_DATA(tag);

    c->qwc = c->tag.qwc;

    c->tag.end = dmac->sif0.tag.irq && (dmac->sif0.chcr & 0x80);

    switch (c->tag.id) {
        case 7:
            c->tag.end = 1;
        case 0:
        case 1:
            c->madr = c->tag.addr;
    }
}

static inline void dmac_test_cpcond0(struct ps2_dmac* dmac) {
    ee_set_cpcond0(dmac->ee, (((~dmac->pcr) | dmac->stat) & 0x3ff) == 0x3ff);
}

static inline void dmac_test_irq(struct ps2_dmac* dmac) {
    dmac_test_cpcond0(dmac);

    int meis = ((dmac->stat >> 14) & 1) & ((dmac->stat >> 30) & 1);
    int chirq = (dmac->stat & 0x3ff) & ((dmac->stat >> 16) & 0x3ff);

    ee_set_int1(dmac->ee, chirq || meis);
}

static inline void dmac_set_irq(struct ps2_dmac* dmac, int ch) {
    dmac->stat |= 1 << ch;

    // printf("dmac: channel=%d flag=%08x mask=%08x irq=%08x\n", ch, dmac->stat & 0x3ff, (dmac->stat >> 16) & 0x3ff, (dmac->stat & 0x3ff) & ((dmac->stat >> 16) & 0x3ff));

    dmac_test_irq(dmac);
}

void dmac_handle_vif0_transfer(struct ps2_dmac* dmac) {
    // printf("ee: VIF0 DMA dir=%d mode=%d tte=%d tie=%d qwc=%d madr=%08x tadr=%08x\n",
    //     dmac->vif0.chcr & 1,
    //     (dmac->vif0.chcr >> 2) & 3,
    //     (dmac->vif0.chcr >> 6) & 1,
    //     (dmac->vif0.chcr >> 7) & 1,
    //     dmac->vif0.qwc,
    //     dmac->vif0.madr,
    //     dmac->vif0.tadr
    // );

    int mode = (dmac->vif0.chcr >> 2) & 3;

    for (int i = 0; i < dmac->vif0.qwc; i++) {
        uint128_t q = dmac_read_qword(dmac, dmac->vif0.madr, 0);

        // VIF0 FIFO address
        ee_bus_write128(dmac->bus, 0x10004000, q);

        dmac->vif0.madr += 16;
    }

    if (mode == 0) {
        dmac->vif0.chcr &= ~0x100;
        dmac->vif0.qwc = 0;

        dmac_set_irq(dmac, DMAC_VIF0);

        return;
    }

    // Chain mode
    do {
        uint128_t tag = dmac_read_qword(dmac, dmac->vif0.tadr, 0);

        dmac_process_source_tag(dmac, &dmac->vif0, tag);

        // printf("ee: vif0 tag qwc=%08x madr=%08x tadr=%08x id=%d addr=%08x\n",
        //     dmac->vif0.tag.qwc,
        //     dmac->vif0.madr,
        //     dmac->vif0.tadr,
        //     dmac->vif0.tag.id,
        //     dmac->vif0.tag.addr
        // );

        // CHCR.TTE: Transfer tag DATA field
        if ((dmac->vif0.chcr >> 6) & 1) {
            ee_bus_write32(dmac->bus, 0x10004000, dmac->vif0.tag.data & 0xffffffff);
            ee_bus_write32(dmac->bus, 0x10004000, dmac->vif0.tag.data >> 32);
        }

        for (int i = 0; i < dmac->vif0.qwc; i++) {
            uint128_t q = dmac_read_qword(dmac, dmac->vif0.madr, dmac->vif0.tag.mem);

            // printf("ee: Sending %016lx%016lx from %08x to VIF0 FIFO\n",
            //     q.u64[1], q.u64[0],
            //     dmac->vif0.madr
            // );

            ee_bus_write128(dmac->bus, 0x10004000, q);

            dmac->vif0.madr += 16;
        }

        if (dmac->vif0.tag.id == 1) {
            dmac->vif0.tadr = dmac->vif0.madr;
        }
    } while (!channel_is_done(&dmac->vif0));

    dmac->vif0.chcr &= ~0x100;
    dmac->vif0.qwc = 0;

    dmac_set_irq(dmac, DMAC_VIF0);
}

void dmac_send_vif1_irq(void* udata, int overshoot) {
    struct ps2_dmac* dmac = (struct ps2_dmac*)udata;

    dmac->vif1.chcr &= ~0x100;
    dmac->vif1.qwc = 0;

    // printf("dmac: VIF1 interrupt\n");

    dmac_set_irq(dmac, DMAC_VIF1);
}

void dmac_handle_vif1_transfer(struct ps2_dmac* dmac) {
    // printf("dmac: VIF1 DMA dir=%d mode=%d tte=%d tie=%d qwc=%d madr=%08x tadr=%08x rbor=%08x rbsr=%08x sprfrom.madr=%08x\n",
    //     dmac->vif1.chcr & 1,
    //     (dmac->vif1.chcr >> 2) & 3,
    //     (dmac->vif1.chcr >> 6) & 1,
    //     (dmac->vif1.chcr >> 7) & 1,
    //     dmac->vif1.qwc,
    //     dmac->vif1.madr,
    //     dmac->vif1.tadr,
    //     dmac->rbor,
    //     dmac->rbsr,
    //     dmac->spr_from.madr
    // );

    uint32_t spr_from_madr = dmac->spr_from.madr;
    int mfifo_drain = (dmac->ctrl >> 2) & 3;

    if (mfifo_drain == 2) {
        if (dmac->vif1.tadr == dmac->spr_from.madr) {
            dmac_set_irq(dmac, DMAC_MEIS);

            return;
        }
    }

    int tte = (dmac->vif1.chcr >> 6) & 1;
    int mode = (dmac->vif1.chcr >> 2) & 3;

    struct sched_event event;

    event.name = "VIF1 DMA IRQ";
    event.udata = dmac;
    event.callback = dmac_send_vif1_irq;
    event.cycles = 1000;

    // We don't handle VIF1 reads
    if ((dmac->vif1.chcr & 1) == 0) {
        // Gran Turismo 3 sends a VIF1 read with QWC=0, presumably to
        // wait until the GIF FIFO is actually full, so we shouldn't send
        // and interrupt there.
        if (dmac->vif1.qwc == 0)
            return;

        sched_schedule(dmac->sched, event);

        return;
    }

    for (int i = 0; i < dmac->vif1.qwc; i++) {
        uint128_t q = dmac_read_qword(dmac, dmac->vif1.madr, 0);

        // VIF1 FIFO address
        ee_bus_write32(dmac->bus, 0x10005000, q.u32[0]);
        ee_bus_write32(dmac->bus, 0x10005000, q.u32[1]);
        ee_bus_write32(dmac->bus, 0x10005000, q.u32[2]);
        ee_bus_write32(dmac->bus, 0x10005000, q.u32[3]);

        dmac->vif1.madr += 16;
    }

    if (mode == 0) {
        sched_schedule(dmac->sched, event);

        return;
    }

    // Chain mode
    do {
        uint128_t tag = dmac_read_qword(dmac, dmac->vif1.tadr, 0);

        dmac_process_source_tag(dmac, &dmac->vif1, tag);

        // printf("ee: vif1 tag qwc=%08x madr=%08x tadr=%08x id=%d addr=%08x\n",
        //     dmac->vif1.tag.qwc,
        //     dmac->vif1.madr,
        //     dmac->vif1.tadr,
        //     dmac->vif1.tag.id,
        //     dmac->vif1.tag.addr
        // );

        // CHCR.TTE: Transfer tag DATA field
        if ((dmac->vif1.chcr >> 6) & 1) {
            ee_bus_write32(dmac->bus, 0x10005000, dmac->vif1.tag.data & 0xffffffff);
            ee_bus_write32(dmac->bus, 0x10005000, dmac->vif1.tag.data >> 32);
        }

        for (int i = 0; i < dmac->vif1.qwc; i++) {
            uint128_t q = dmac_read_qword(dmac, dmac->vif1.madr, dmac->vif1.tag.mem);

            // printf("ee: Sending %016lx%016lx from %08x to VIF1 FIFO\n",
            //     q.u64[1], q.u64[0],
            //     dmac->vif1.madr
            // );

            ee_bus_write32(dmac->bus, 0x10005000, q.u32[0]);
            ee_bus_write32(dmac->bus, 0x10005000, q.u32[1]);
            ee_bus_write32(dmac->bus, 0x10005000, q.u32[2]);
            ee_bus_write32(dmac->bus, 0x10005000, q.u32[3]);

            dmac->vif1.madr += 16;
        }

        if (dmac->vif1.tag.id == 1) {
            dmac->vif1.tadr = dmac->vif1.madr;
        }
    } while (!channel_is_done(&dmac->vif1));

    sched_schedule(dmac->sched, event);
}

void dmac_send_gif_irq(void* udata, int overshoot) {
    struct ps2_dmac* dmac = (struct ps2_dmac*)udata;

    dmac_set_irq(dmac, DMAC_GIF);

    dmac->gif.chcr &= ~0x100;
    dmac->gif.qwc = 0;
}

void dmac_handle_gif_transfer(struct ps2_dmac* dmac) {
    struct sched_event event;

    int mode = (dmac->gif.chcr >> 2) & 3;

    event.name = "GIF DMA IRQ";
    event.udata = dmac;
    event.callback = dmac_send_gif_irq;
    event.cycles = 1000;

    sched_schedule(dmac->sched, event);

    // printf("ee: GIF DMA dir=%d mode=%d tte=%d tie=%d qwc=%d madr=%08x tadr=%08x\n",
    //     dmac->gif.chcr & 1,
    //     (dmac->gif.chcr >> 2) & 3,
    //     (dmac->gif.chcr >> 6) & 1,
    //     (dmac->gif.chcr >> 7) & 1,
    //     dmac->gif.qwc,
    //     dmac->gif.madr,
    //     dmac->gif.tadr
    // );

    for (int i = 0; i < dmac->gif.qwc; i++) {
        uint128_t q = dmac_read_qword(dmac, dmac->gif.madr, 0);

        // fprintf(file, "ee: Sending %016lx%016lx from %08x to GIF FIFO (burst)\n",
        //     q.u64[1], q.u64[0],
        //     dmac->gif.madr
        // );

        // GIF FIFO address
        ee_bus_write128(dmac->bus, 0x10006000, q);

        dmac->gif.madr += 16;
    }

    if (mode == 0) {
        return;
    }

    // Chain mode
    do {
        uint128_t tag = dmac_read_qword(dmac, dmac->gif.tadr, 0);

        dmac_process_source_tag(dmac, &dmac->gif, tag);

        // printf("ee: gif tag qwc=%08x madr=%08x tadr=%08x\n", dmac->gif.tag.qwc, dmac->gif.madr, dmac->gif.tadr);

        for (int i = 0; i < dmac->gif.qwc; i++) {
            uint128_t q = dmac_read_qword(dmac, dmac->gif.madr, dmac->gif.tag.mem);

            // fprintf(file, "ee: Sending %016lx%016lx from %08x to GIF FIFO (chain)\n",
            //     q.u64[1], q.u64[0],
            //     dmac->gif.madr
            // );

            ee_bus_write128(dmac->bus, 0x10006000, q);

            dmac->gif.madr += 16;
        }

        if (dmac->gif.tag.id == 1) {
            dmac->gif.tadr = dmac->gif.madr;
        }
    } while (!channel_is_done(&dmac->gif));
}
void dmac_handle_ipu_from_transfer(struct ps2_dmac* dmac) {
    if ((dmac->ipu_from.chcr & 0x100) == 0) {
        // printf("dmac: ipu_from channel not started\n");

        return;
    }

    int mode = (dmac->ipu_from.chcr >> 2) & 3;

    // printf("dmac: ipu_from start data=%08x dir=%d mod=%d tte=%d madr=%08x qwc=%08x tadr=%08x dreq=%d\n",
    //     dmac->ipu_from.chcr,
    //     dmac->ipu_from.chcr & 1,
    //     (dmac->ipu_from.chcr >> 2) & 3,
    //     !!(dmac->ipu_from.chcr & 0x40),
    //     dmac->ipu_from.madr,
    //     dmac->ipu_from.qwc,
    //     dmac->ipu_from.tadr,
    //     dmac->ipu_from.dreq
    // );

    if (mode != 0) {
        printf("dmac: ipu_from mode %d not supported\n", mode);

        exit(1);

        return;
    }

    while (dmac->ipu_from.dreq && dmac->ipu_from.qwc) {
        uint128_t q = ee_bus_read128(dmac->bus, 0x10007000);

        dmac_write_qword(dmac, dmac->ipu_from.madr, 0, q);

        dmac->ipu_from.madr += 16;
        dmac->ipu_from.qwc--;
    }

    if (dmac->ipu_from.qwc == 0) {
        dmac_set_irq(dmac, DMAC_IPU_FROM);

        dmac->ipu_from.chcr &= ~0x100;
        dmac->ipu_from.qwc = 0;

        // printf("dmac: ipu_from channel done\n");
    }
}

int dmac_transfer_ipu_to_qword(struct ps2_dmac* dmac) {
    if ((dmac->ipu_to.chcr & 0x100) == 0) {
        // printf("dmac: ipu_to channel not started\n");

        return 0;
    }

    if (!dmac->ipu_to.dreq) {
        // printf("dmac: ipu_to dreq cleared\n");

        return 0;
    }

    if (dmac->ipu_to.qwc) {
        uint128_t q = dmac_read_qword(dmac, dmac->ipu_to.madr, dmac->ipu_to.tag.mem);

        ee_bus_write128(dmac->bus, 0x10007010, q);

        dmac->ipu_to.madr += 16;
        dmac->ipu_to.qwc--;

        return 1;
    }

    if (channel_is_done(&dmac->ipu_to)) {
        printf("dmac: ipu_to channel done\n");

        dmac_set_irq(dmac, DMAC_IPU_TO);

        dmac->ipu_to.chcr &= ~0x100;
        dmac->ipu_to.qwc = 0;

        return 0;
    }

    uint128_t tag = dmac_read_qword(dmac, dmac->ipu_to.tadr, 0);

    dmac_process_source_tag(dmac, &dmac->ipu_to, tag);

    // printf("dmac: ipu_to tag tag.qwc=%08lx qwc=%08lx id=%ld irq=%ld addr=%08lx mem=%ld data=%016lx end=%d tte=%d\n",
    //     dmac->ipu_to.tag.qwc,
    //     dmac->ipu_to.qwc,
    //     dmac->ipu_to.tag.id,
    //     dmac->ipu_to.tag.irq,
    //     dmac->ipu_to.tag.addr,
    //     dmac->ipu_to.tag.mem,
    //     dmac->ipu_to.tag.data,
    //     dmac->ipu_to.tag.end,
    //     (dmac->ipu_to.chcr >> 7) & 1
    // );

    return 1;
}
void dmac_handle_ipu_to_transfer(struct ps2_dmac* dmac) {
    if ((dmac->ipu_to.chcr & 0x100) == 0) {
        // printf("dmac: ipu_to channel not started\n");

        return;
    }

    // printf("dmac: ipu_to start data=%08x dir=%d mod=%d tte=%d madr=%08x qwc=%08x tadr=%08x\n",
    //     dmac->ipu_to.chcr,
    //     dmac->ipu_to.chcr & 1,
    //     (dmac->ipu_to.chcr >> 2) & 3,
    //     !!(dmac->ipu_to.chcr & 0x40),
    //     dmac->ipu_to.madr,
    //     dmac->ipu_to.qwc,
    //     dmac->ipu_to.tadr
    // );

    while (dmac_transfer_ipu_to_qword(dmac)) {
        // Keep transferring until we run out of QWC or DREQ is cleared
    }

    // while (dmac->ipu_to.dreq && dmac->ipu_to.qwc) {
    //     uint128_t q = dmac_read_qword(dmac, dmac->ipu_to.madr, dmac->ipu_to.tag.mem);

    //     ee_bus_write128(dmac->bus, 0x10007010, q);

    //     dmac->ipu_to.madr += 16;
    //     dmac->ipu_to.tag.qwc--;
    // }

    // if (dmac->ipu_to.tag.qwc == 0) {
    //     if (channel_is_done(&dmac->ipu_to)) {
    //         dmac_set_irq(dmac, DMAC_IPU_TO);

    //         dmac->ipu_to.chcr &= ~0x100;
    //         dmac->ipu_to.qwc = 0;

    //         printf("dmac: ipu_to channel done\n");

    //         return;
    //     }

    //     uint128_t tag = dmac_read_qword(dmac, dmac->ipu_to.tadr, 0);

    //     dmac_process_source_tag(dmac, &dmac->ipu_to, tag);
    // }

    // if (!dmac->ipu_to.dreq) {
    //     return;
    // }
}
void dmac_handle_sif0_transfer(struct ps2_dmac* dmac) {
    // SIF FIFO is empty, keep waiting
    if (ps2_sif0_is_empty(dmac->sif)) {
        return;
    }

    // Data ready but channel isn't ready yet, keep waiting
    if (!(dmac->sif0.chcr & 0x100)) {
        return;
    }

    while (!ps2_sif0_is_empty(dmac->sif)) {
        uint128_t tag = ps2_sif0_read(dmac->sif);

        dmac_process_dest_tag(dmac, &dmac->sif0, tag);

        // printf("ee: sif0 tag qwc=%08lx madr=%08lx id=%ld irq=%ld addr=%08lx mem=%ld data=%016lx tte=%d\n",
        //     dmac->sif0.tag.qwc,
        //     dmac->sif0.madr,
        //     dmac->sif0.tag.id,
        //     dmac->sif0.tag.irq,
        //     dmac->sif0.tag.addr,
        //     dmac->sif0.tag.mem,
        //     dmac->sif0.tag.data,
        //     dmac->sif0.chcr
        // );

        for (int i = 0; i < dmac->sif0.qwc; i++) {
            if (ps2_sif0_is_empty(dmac->sif)) {
                printf("dmac: qwc != 0 FIFO empty\n");

                if (channel_is_done(&dmac->sif0)) {
                    printf("dmac: qwc != 0 FIFO empty\n");

                    dmac->sif0.chcr &= ~0x100;
                    dmac->sif0.qwc = 0;
        
                    dmac_set_irq(dmac, DMAC_SIF0);
        
                    return;
                }
            }

            uint128_t q = ps2_sif0_read(dmac->sif);

            // printf("%08x: ", dmac->sif0.madr);

            // for (int i = 0; i < 16; i++) {
            //     printf("%02x ", q.u8[i]);
            // }

            // putchar('|');

            // for (int i = 0; i < 16; i++) {
            //     printf("%c", isprint(q.u8[i]) ? q.u8[i] : '.');
            // }

            // puts("|");

            // printf("ee: Writing %016lx %016lx to %08x\n", q.u64[1], q.u64[0], dmac->sif0.madr);

            dmac_write_qword(dmac, dmac->sif0.madr, dmac->sif0.tag.mem, q);

            dmac->sif0.madr += 16;
        }

        if (channel_is_done(&dmac->sif0)) {
            dmac->sif0.chcr &= ~0x100;
            dmac->sif0.qwc = 0;

            dmac_set_irq(dmac, DMAC_SIF0);

            // ps2_sif_fifo_reset(dmac->sif);

            return;
        }
    }

    // dmac->sif0.chcr &= ~0x100;

    // dmac_set_irq(dmac, DMAC_SIF0);

    // ps2_sif_fifo_reset(dmac->sif);

    // We shouldn't send an interrupt if tag end or irq/tie weren't
    // set
}
void dmac_handle_sif1_transfer(struct ps2_dmac* dmac) {
    assert(!dmac->sif1.qwc);

    // This should be ok?
    // if (!ps2_sif_fifo_is_empty(dmac->sif)) {
    //     printf("dmac: WARNING!!! SIF FIFO not empty\n");
    // }

    do {
        uint128_t tag = dmac_read_qword(dmac, dmac->sif1.tadr, 0);

        dmac_process_source_tag(dmac, &dmac->sif1, tag);

        // printf("ee: sif1 tag qwc=%08lx id=%ld irq=%ld addr=%08lx mem=%ld data=%016lx end=%d tte=%d\n",
        //     dmac->sif1.tag.qwc,
        //     dmac->sif1.tag.id,
        //     dmac->sif1.tag.irq,
        //     dmac->sif1.tag.addr,
        //     dmac->sif1.tag.mem,
        //     dmac->sif1.tag.data,
        //     dmac->sif1.tag.end,
        //     (dmac->sif1.chcr >> 7) & 1
        // );
        // printf("ee: SIF1 tag madr=%08x\n", dmac->sif1.madr);

        for (int i = 0; i < dmac->sif1.qwc; i++) {
            uint128_t q = dmac_read_qword(dmac, dmac->sif1.madr, dmac->sif1.tag.mem);

            // printf("%08x: ", dmac->sif1.madr);

            // for (int i = 0; i < 16; i++) {
            //     printf("%02x ", q.u8[i]);
            // }

            // putchar('|');

            // for (int i = 0; i < 16; i++) {
            //     printf("%c", isprint(q.u8[i]) ? q.u8[i] : '.');
            // }

            // puts("|");

            ps2_sif1_write(dmac->sif, q);

            dmac->sif1.madr += 16;
        }
    } while (!channel_is_done(&dmac->sif1));

    iop_dma_handle_sif1_transfer(dmac->iop_dma);

    dmac_set_irq(dmac, DMAC_SIF1);

    dmac->sif1.chcr &= ~0x100;
    dmac->sif1.qwc = 0;
}
void dmac_handle_sif2_transfer(struct ps2_dmac* dmac) {
    printf("ee: SIF2 channel unimplemented\n"); exit(1);
}

void dmac_spr_from_interleave(struct ps2_dmac* dmac) {
    uint32_t sqwc = dmac->sqwc & 0xff;
    uint32_t tqwc = (dmac->sqwc >> 16) & 0xff;

    // Note: When TQWC=0, it is set to QWC instead (undocumented)
    if (tqwc == 0)
        tqwc = dmac->spr_from.qwc;

    while (dmac->spr_from.qwc) {
        for (int i = 0; i < tqwc && dmac->spr_from.qwc; i++) {
            uint128_t q = ps2_ram_read128(dmac->spr, dmac->spr_from.sadr);

            ee_bus_write128(dmac->bus, dmac->spr_from.madr, q);

            dmac->spr_from.madr += 0x10;
            dmac->spr_from.sadr += 0x10;
            dmac->spr_from.sadr &= 0x3ff0;
            dmac->spr_from.qwc--;
        }

        dmac->spr_from.madr += sqwc * 16;
    }
}
void dmac_handle_spr_from_transfer(struct ps2_dmac* dmac) {
    dmac_set_irq(dmac, DMAC_SPR_FROM);

    dmac->spr_from.chcr &= ~0x100;

    // printf("dmac: spr_from start data=%08x dir=%d mod=%d tte=%d madr=%08x qwc=%08x tadr=%08x sadr=%08x rbor=%08x rbsr=%08x\n",
    //     dmac->spr_from.chcr,
    //     dmac->spr_from.chcr & 1,
    //     (dmac->spr_from.chcr >> 2) & 3,
    //     !!(dmac->spr_from.chcr & 0x40),
    //     dmac->spr_from.madr,
    //     dmac->spr_from.qwc,
    //     dmac->spr_from.tadr,
    //     dmac->spr_from.sadr,
    //     dmac->rbor,
    //     dmac->rbsr
    // );

    // int mfifo_enabled = (dmac->ctrl >> 2) & 3;

    // if (mfifo_enabled) {
    //     dmac->spr_from.madr |= dmac->rbor & dmac->rbsr;
    //     for (int i = 0; i < dmac->spr_from.qwc; i++) {
    //         uint128_t q = dmac_read_qword(dmac, dmac->spr_from.sadr, 1);

    //         ee_bus_write128(dmac->bus, dmac->spr_from.madr, q);

    //         dmac->spr_from.madr += 0x10;
    //         dmac->spr_from.sadr += 0x10;
    //         dmac->spr_from.sadr &= 0x3ff0;
    //     }
    // }

    int mode = (dmac->spr_from.chcr >> 2) & 3;

    if (mode == 2) {
        dmac_spr_from_interleave(dmac);

        return;
    }

    for (int i = 0; i < dmac->spr_from.qwc; i++) {
        uint128_t q = dmac_read_qword(dmac, dmac->spr_from.sadr, 1);

        ee_bus_write128(dmac->bus, dmac->spr_from.madr, q);

        dmac->spr_from.madr += 0x10;
        dmac->spr_from.sadr += 0x10;
        dmac->spr_from.sadr &= 0x3ff0;
    }

    dmac->spr_from.qwc = 0;

    if (mode == 0) {
        return;
    }

    // Chain mode
    int loop = 0;
    do {
        uint128_t tag = dmac_read_qword(dmac, dmac->spr_from.sadr, 1);

        dmac->spr_from.sadr += 0x10;
        dmac->spr_from.sadr &= 0x3ff0;

        dmac->spr_from.qwc = tag.u32[0] & 0xffff;
        dmac->spr_from.tag.id = (tag.u32[0] >> 28) & 0x7;
        dmac->spr_from.tag.irq = tag.u32[0] & 0x80000000;
        dmac->spr_from.tag.end = dmac->spr_from.tag.id == 7;
        dmac->spr_from.madr = tag.u32[1];

        // printf("ee: spr_from tag qwc=%08lx madr=%08lx sadr=%08x tadr=%08lx id=%ld addr=%08lx mem=%ld data=%016lx irq=%d end=%d tte=%d\n",
        //     dmac->spr_from.tag.qwc,
        //     dmac->spr_from.madr,
        //     dmac->spr_from.sadr,
        //     dmac->spr_from.tadr,
        //     dmac->spr_from.tag.id,
        //     dmac->spr_from.tag.addr,
        //     dmac->spr_from.tag.mem,
        //     dmac->spr_from.tag.data,
        //     dmac->spr_from.tag.irq,
        //     dmac->spr_from.tag.end,
        //     (dmac->spr_from.chcr >> 7) & 1
        // );

        for (int i = 0; i < dmac->spr_from.qwc; i++) {
            uint128_t q = dmac_read_qword(dmac, dmac->spr_from.sadr, 1);

            ee_bus_write128(dmac->bus, dmac->spr_from.madr, q);

            dmac->spr_from.madr += 0x10;
            dmac->spr_from.sadr += 0x10;
            dmac->spr_from.sadr &= 0x3ff0;
        }
    } while (!channel_is_done(&dmac->spr_from));
}

void dmac_spr_to_interleave(struct ps2_dmac* dmac) {
    uint32_t sqwc = dmac->sqwc & 0xff;
    uint32_t tqwc = (dmac->sqwc >> 16) & 0xff;

    // Note: When TQWC=0, it is set to QWC instead (undocumented)
    if (tqwc == 0)
        tqwc = dmac->spr_to.qwc;

    while (dmac->spr_to.qwc) {
        for (int i = 0; i < tqwc && dmac->spr_to.qwc; i++) {
            uint128_t q = dmac_read_qword(dmac, dmac->spr_to.madr, 0);

            ps2_ram_write128(dmac->spr, dmac->spr_to.sadr, q);

            dmac->spr_to.madr += 0x10;
            dmac->spr_to.sadr += 0x10;
            dmac->spr_to.sadr &= 0x3ff0;
            dmac->spr_to.qwc--;
        }

        dmac->spr_to.madr += sqwc * 16;
    }
}
void dmac_handle_spr_to_transfer(struct ps2_dmac* dmac) {
    dmac_set_irq(dmac, DMAC_SPR_TO);

    dmac->spr_to.chcr &= ~0x100;

    int mode = (dmac->spr_to.chcr >> 2) & 3;

    // printf("ee: spr_to start data=%08x dir=%d mod=%d tte=%d madr=%08x qwc=%08x tadr=%08x sadr=%08x\n",
    //     dmac->spr_to.chcr,
    //     dmac->spr_to.chcr & 1,
    //     (dmac->spr_to.chcr >> 2) & 3,
    //     !!(dmac->spr_to.chcr & 0x40),
    //     dmac->spr_to.madr,
    //     dmac->spr_to.qwc,
    //     dmac->spr_to.tadr,
    //     dmac->spr_to.sadr
    // );

    if (mode == 2) {
        dmac_spr_to_interleave(dmac);

        return;
    }

    for (int i = 0; i < dmac->spr_to.qwc; i++) {
        uint128_t q = dmac_read_qword(dmac, dmac->spr_to.madr, 0);

        ps2_ram_write128(dmac->spr, dmac->spr_to.sadr, q);

        dmac->spr_to.madr += 0x10;
        dmac->spr_to.sadr += 0x10;
        dmac->spr_to.sadr &= 0x3ff0;
    }

    dmac->spr_to.qwc = 0;

    // We're done
    if (mode == 0) {
        return;
    }

    // Chain mode
    do {
        uint128_t tag = dmac_read_qword(dmac, dmac->spr_to.tadr, 0);

        dmac_process_source_tag(dmac, &dmac->spr_to, tag);

        if ((dmac->spr_to.chcr >> 6) & 1) {
            ps2_ram_write128(dmac->spr, dmac->spr_to.sadr, tag);

            dmac->spr_to.madr += 0x10;
        }

        // printf("ee: spr_to tag qwc=%08lx madr=%08lx tadr=%08lx id=%ld addr=%08lx mem=%ld data=%016lx end=%d tte=%d\n",
        //     dmac->spr_to.tag.qwc,
        //     dmac->spr_to.madr,
        //     dmac->spr_to.tadr,
        //     dmac->spr_to.tag.id,
        //     dmac->spr_to.tag.addr,
        //     dmac->spr_to.tag.mem,
        //     dmac->spr_to.tag.data,
        //     dmac->spr_to.tag.end,
        //     (dmac->spr_to.chcr >> 7) & 1
        // );

        for (int i = 0; i < dmac->spr_to.qwc; i++) {
            uint128_t q = dmac_read_qword(dmac, dmac->spr_to.madr, dmac->spr_to.tag.mem);

            ps2_ram_write128(dmac->spr, dmac->spr_to.sadr, q);

            dmac->spr_to.madr += 0x10;
            dmac->spr_to.sadr += 0x10;
            dmac->spr_to.sadr &= 0x3ff0;
        }

        if (dmac->spr_to.tag.id == 1) {
            dmac->spr_to.tadr = dmac->spr_to.madr;
        }
    } while (!channel_is_done(&dmac->spr_to));
}

static inline void dmac_handle_channel_start(struct ps2_dmac* dmac, uint32_t addr) {
    struct dmac_channel* c = dmac_get_channel(dmac, addr);

    // printf("dmac: %s start data=%08x dir=%d mod=%d tte=%d madr=%08x qwc=%08x tadr=%08x rbsr=%08x rbor=%08x\n",
    //     dmac_get_channel_name(dmac, addr),
    //     c->chcr,
    //     c->chcr & 1,
    //     (c->chcr >> 2) & 3,
    //     !!(c->chcr & 0x40),
    //     c->madr,
    //     c->qwc,
    //     c->tadr,
    //     dmac->rbsr,
    //     dmac->rbor
    // );

    // if (c == &dmac->ipu_to && c->qwc != 0) {
    //     int mode = (c->chcr >> 2) & 3;

    //     if (mode == 1) {
    //         uint128_t tag;

    //         tag.u32[0] = (c->chcr & 0xffff0000) | (c->qwc & 0xffff);

    //         dmac_process_source_tag(dmac, c, tag);
    //     } else {
    //         c->tag.end = 1;
    //     }
    // }

    switch (addr & 0xff00) {
        case 0x8000: dmac_handle_vif0_transfer(dmac); return;
        case 0x9000: dmac_handle_vif1_transfer(dmac); return;
        case 0xA000: dmac_handle_gif_transfer(dmac); return;
        case 0xB000: dmac_handle_ipu_from_transfer(dmac); return;
        case 0xB400: dmac->ipu_to.dreq = 1; dmac_handle_ipu_to_transfer(dmac); return;
        case 0xC000: dmac_handle_sif0_transfer(dmac); return;
        case 0xC400: dmac_handle_sif1_transfer(dmac); return;
        case 0xC800: dmac_handle_sif2_transfer(dmac); return;
        case 0xD000: dmac_handle_spr_from_transfer(dmac); return;
        case 0xD400: dmac_handle_spr_to_transfer(dmac); return;
    }
}

void dmac_write_stat(struct ps2_dmac* dmac, uint32_t data) {
    uint32_t istat = data & 0x0000ffff;
    uint32_t imask = data & 0xffff0000;

    dmac->stat &= ~istat;
    dmac->stat ^= imask;

    // printf("dmac: stat=%08x istat=%08x imask=%08x\n", dmac->stat, istat, imask);

    dmac_test_irq(dmac);
}

void ps2_dmac_write32(struct ps2_dmac* dmac, uint32_t addr, uint64_t data) {
    struct dmac_channel* c = dmac_get_channel(dmac, addr);

    switch (addr) {
        case 0x1000E000: {
            dmac->ctrl = data;

            int mfifo_drain = (dmac->ctrl >> 2) & 3;
            int stall_ctrl = (dmac->ctrl >> 4) & 3;
            int stall_drain = (dmac->ctrl >> 6) & 3;

            if (mfifo_drain || stall_ctrl || stall_drain) {
                printf("dmac: mfifo_drain=%d stall_ctrl=%d stall_drain=%d\n",
                    mfifo_drain, stall_ctrl, stall_drain
                );

                // exit(1);
            }
        } return;
        case 0x1000E010: dmac_write_stat(dmac, data); return;
        case 0x1000E020: dmac->pcr = data; dmac_test_cpcond0(dmac); return;
        case 0x1000E030: dmac->sqwc = data; return;
        case 0x1000E040: dmac->rbsr = data; return;
        case 0x1000E050: dmac->rbor = data; return;
        case 0x1000F520: return; // ENABLER (R)
        case 0x1000F590: dmac->enable = data; return;
    }

    if (c) {
        switch (addr & 0xff) {
            case 0x00: {
                c->chcr = data;

                if (data & 0x100) {
                    dmac_handle_channel_start(dmac, addr);
                }

                // Behavior required for IPU FMVs to work
                // if ((c->chcr & 0x100) == 0) {
                //     c->chcr = data;

                //     if (data & 0x100) {
                //         dmac_handle_channel_start(dmac, addr);
                //     }
                // } else {
                //     printf("dmac: channel %s value=%08x chcr=%08x\n", dmac_get_channel_name(dmac, addr), data, c->chcr);
                //     c->chcr &= (data & 0x100) | 0xfffffeff;
                // }
            } return;
            case 0x10: {
                c->madr = data;

                // Clear MADR's MSB on SPR channels
                if (c == &dmac->spr_to || c == &dmac->spr_from) {
                    c->madr &= 0x7fffffff;
                }
            } return;
            case 0x20: c->qwc = data & 0xffff; return;
            case 0x30: c->tadr = data; return;
            case 0x40: c->asr0 = data; return;
            case 0x50: c->asr1 = data; return;
            case 0x80: c->sadr = data & 0x3ff0; return;
        }

        // printf("dmac: Unknown channel register %02x\n", addr & 0xff);

        return;
    }
}

uint64_t ps2_dmac_read8(struct ps2_dmac* dmac, uint32_t addr) {
    if (addr == 0x10009000) {
        // printf("dmac: 8-bit read from chcr (%08x)\n", dmac->vif1.chcr & 0xff);

        return dmac->vif1.chcr & 0xff;
    }

    int shift = (addr & 0x3) * 8;

    return (ps2_dmac_read32(dmac, addr & ~0x3) >> shift) & 0xff;

    // struct dmac_channel* c = dmac_get_channel(dmac, addr & ~3);

    // if (!c) {
    //     switch (addr) {
    //         case 0x1000e000: {
    //             return dmac->ctrl & 0xff;
    //         } break;
    //     }

    //     printf("dmac: Unknown channel read8 at %08x\n", addr);

    //     return 0;
    // }

    // switch (addr) {
    //     case 0x10009000:
    //     case 0x1000a000:
    //     case 0x10008000: {
    //         return c->chcr & 0xff;
    //     }

    //     case 0x10008001:
    //     case 0x10009001:
    //     case 0x1000a001: {
    //         return (c->chcr >> 8) & 0xff;
    //     }
    // }

    // printf("dmac: Unhandled 8-bit read from %08x\n", addr);

    // exit(1);

    // return 0;
}

void ps2_dmac_write8(struct ps2_dmac* dmac, uint32_t addr, uint64_t data) {
    struct dmac_channel* c = dmac_get_channel(dmac, addr & ~3);

    switch (addr) {
        case 0x10008000:
        case 0x10009000:
        case 0x1000a000:
        case 0x1000b000:
        case 0x1000b400: {
            c->chcr &= 0xffffff00;
            c->chcr |= data & 0xff;

            return;
        } break;

        case 0x10008001:
        case 0x10009001: {
            ps2_dmac_write32(dmac, addr & ~0x3, (ps2_dmac_read32(dmac, addr & ~0x3) & 0xffff00ff) | ((data & 0xff) << 8));
            // c->chcr &= 0xffff00ff;
            // c->chcr |= (data & 0xff) << 8;

            // if (c->chcr & 0x100) {
            //     dmac_handle_channel_start(dmac, addr);
            // }

            return;
        } break;

        case 0x1000e000: {
            dmac->ctrl &= 0xffffff00;
            dmac->ctrl |= data;
        } return;
    }

    printf("dmac: 8-bit write to %08x (%02x)\n", addr, data);

    exit(1);

    return;
}

uint64_t ps2_dmac_read16(struct ps2_dmac* dmac, uint32_t addr) {
    printf("dmac: 16-bit read from %08x\n", addr);

    exit(1);
}

void ps2_dmac_write16(struct ps2_dmac* dmac, uint32_t addr, uint64_t data) {
    printf("dmac: 16-bit write to %08x (%04x)\n", addr, data & 0xffff);

    exit(1);
}