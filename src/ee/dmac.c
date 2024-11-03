#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dmac.h"

struct ps2_dmac* ps2_dmac_create(void) {
    return malloc(sizeof(struct ps2_dmac));
}

void ps2_dmac_init(struct ps2_dmac* dmac, struct ps2_sif* sif, struct ps2_iop_dma* iop_dma, struct ee_state* ee, struct ee_bus* bus) {
    memset(dmac, 0, sizeof(struct ps2_dmac));

    dmac->bus = bus;
    dmac->sif = sif;
    dmac->iop_dma = iop_dma;
    dmac->ee = ee;
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

uint64_t ps2_dmac_read32(struct ps2_dmac* dmac, uint32_t addr) {
    struct dmac_channel* c = dmac_get_channel(dmac, addr);

    if (c) {
        switch (addr & 0xff) {
            case 0x00: return c->chcr;
            case 0x10: return c->madr;
            case 0x20: return c->qwc;
            case 0x30: return c->tadr;
            case 0x40: return c->asr0;
            case 0x50: return c->asr1;
            case 0x80: return c->sadr;
        }

        printf("dmac: Unknown channel register %02x\n", addr & 0xff);

        return 0;
    }

    switch (addr) {
        case 0x1000E000: return dmac->ctrl;
        case 0x1000E010: return dmac->stat;
        case 0x1000E020: return dmac->pcr;
        case 0x1000E030: return dmac->sqwc;
        case 0x1000E040: return dmac->rbsr;
        case 0x1000E050: return dmac->rbor;
        case 0x1000F520: return dmac->enabler;
        case 0x1000F590: return dmac->enablew;
    }

    return 0;
}

static inline void dmac_fetch_tag(struct ps2_dmac* dmac, struct dmac_channel* c) {
    uint128_t tag = ee_bus_read128(dmac->bus, c->tadr);

    c->tag.qwc = TAG_QWC(tag);
    c->tag.pct = TAG_PCT(tag);
    c->tag.id = TAG_ID(tag);
    c->tag.irq = TAG_IRQ(tag);
    c->tag.addr = TAG_ADDR(tag);
    c->tag.mem = TAG_MEM(tag);
    c->tag.data = TAG_DATA(tag);

    c->tag.end = 0;

    switch (c->tag.id) {
        case 0: // REFE tag
            c->madr = c->tag.addr;
            c->tadr += 16;
            c->tag.end = 1;
        break;

        case 1:
            c->madr = c->tadr + 16;
            c->tadr = c->madr;
        break;

        case 2:
            c->madr = c->tadr + 16;
            c->tadr = c->tag.addr;
        break;

        case 3:
            c->madr = c->tag.addr;
            c->tadr += 16;
        break;

        case 7:
            c->madr = c->tadr + 16;
            c->tag.end = 1;
        break;
    }
}

static inline void dmac_set_irq(struct ps2_dmac* dmac, int ch) {
    dmac->stat |= 1 << ch;

    // printf("dmac: flag=%08x imask=%08x\n", 1 << ch, (dmac->stat >> 16) & 0x3ff);

    if ((dmac->stat & 0x3ff) & ((dmac->stat >> 16) & 0x3ff)) {
        ee_set_int1(dmac->ee);
    }
}

static inline void dmac_handle_vif0_transfer(struct ps2_dmac* dmac) {}
static inline void dmac_handle_vif1_transfer(struct ps2_dmac* dmac) {}
static inline void dmac_handle_gif_transfer(struct ps2_dmac* dmac) {
    for (int i = 0; i < dmac->gif.qwc; i++) {
        uint128_t q = ee_bus_read128(dmac->bus, dmac->gif.madr);

        // GIF FIFO address
        ee_bus_write128(dmac->bus, 0x10006000, q);

        dmac->gif.madr += 16;
    }

    dmac_set_irq(dmac, DMAC_GIF);
}

static inline void dmac_handle_ipu_from_transfer(struct ps2_dmac* dmac) {}
static inline void dmac_handle_ipu_to_transfer(struct ps2_dmac* dmac) {}
static inline void dmac_handle_sif0_transfer(struct ps2_dmac* dmac) {}
static inline void dmac_handle_sif2_transfer(struct ps2_dmac* dmac) {}
static inline void dmac_handle_spr_from_transfer(struct ps2_dmac* dmac) {}
static inline void dmac_handle_spr_to_transfer(struct ps2_dmac* dmac) {}

static inline void dmac_handle_sif1_transfer(struct ps2_dmac* dmac) {
    do {
        dmac_fetch_tag(dmac, &dmac->sif1);

        for (int i = 0; i < dmac->sif1.tag.qwc; i++) {
            uint128_t q = ee_bus_read128(dmac->bus, dmac->sif1.madr);

            ps2_sif_fifo_write(dmac->sif, q);

            dmac->sif1.madr += 16;
        }
    } while (!dmac->sif1.tag.end);

    ps2_iop_dma_start_sif1_transfer(dmac->iop_dma);

    dmac_set_irq(dmac, DMAC_SIF1);
}

static inline void dmac_handle_channel_start(struct ps2_dmac* dmac, uint32_t addr) {
    struct dmac_channel* c = dmac_get_channel(dmac, addr);

    // printf("%s start: data=%08x dir=%d mod=%d tte=%d madr=%08x qwc=%08x tadr=%08x\n",
    //     dmac_get_channel_name(dmac, addr),
    //     c->chcr,
    //     c->chcr & 1,
    //     (c->chcr >> 2) & 3,
    //     !!(c->chcr & 0x40),
    //     c->madr,
    //     c->qwc,
    //     c->tadr
    // );

    switch (addr & 0xff00) {
        case 0x8000: dmac_handle_vif0_transfer(dmac); return;
        case 0x9000: dmac_handle_vif1_transfer(dmac); return;
        case 0xA000: dmac_handle_gif_transfer(dmac); return;
        case 0xB000: dmac_handle_ipu_from_transfer(dmac); return;
        case 0xB400: dmac_handle_ipu_to_transfer(dmac); return;
        case 0xC000: dmac_handle_sif0_transfer(dmac); return;
        case 0xC400: dmac_handle_sif1_transfer(dmac); return;
        case 0xC800: dmac_handle_sif2_transfer(dmac); return;
        case 0xD000: dmac_handle_spr_from_transfer(dmac); return;
        case 0xD400: dmac_handle_spr_to_transfer(dmac); return;
    }
}

void dmac_write_stat(struct ps2_dmac* dmac, uint32_t data) {
    uint32_t istat = data & 0x000003ff;
    uint32_t imask = data & 0x03ff0000;

    dmac->stat &= ~istat;
    dmac->stat ^= imask;
    dmac->stat &= 0x03ff03ff;
    dmac->stat |= data & (~0x03ff03ff);

    if ((dmac->stat & 0x3ff) & ((dmac->stat >> 16) & 0x3ff)) {
        ee_set_int1(dmac->ee);
    } else {
        // Reset INT1
        dmac->ee->cause &= ~EE_CAUSE_IP3;
    }
}

void ps2_dmac_write32(struct ps2_dmac* dmac, uint32_t addr, uint64_t data) {
    struct dmac_channel* c = dmac_get_channel(dmac, addr);

    if (c) {
        switch (addr & 0xff) {
            case 0x00: {
                if (data & 0x100)
                    dmac_handle_channel_start(dmac, addr);
            } return;
            case 0x10: c->madr = data; return;
            case 0x20: c->qwc = data; return;
            case 0x30: c->tadr = data; return;
            case 0x40: c->asr0 = data; return;
            case 0x50: c->asr1 = data; return;
            case 0x80: c->sadr = data; return;
        }

        printf("dmac: Unknown channel register %02x\n", addr & 0xff);

        return;
    }

    switch (addr) {
        case 0x1000E000: dmac->ctrl = data; return;
        case 0x1000E010: dmac_write_stat(dmac, data); return;
        case 0x1000E020: dmac->pcr = data; return;
        case 0x1000E030: dmac->sqwc = data; return;
        case 0x1000E040: dmac->rbsr = data; return;
        case 0x1000E050: dmac->rbor = data; return;
        case 0x1000F520: dmac->enabler = data; return;
        case 0x1000F590: dmac->enablew = data; return;
    }
}

void ps2_dmac_start_sif0_transfer(struct ps2_dmac* dmac) {
    uint128_t tag = ps2_sif_fifo_read(dmac->sif);

    dmac->sif0.tag.qwc = TAG_QWC(tag);
    dmac->sif0.tag.pct = TAG_PCT(tag);
    dmac->sif0.tag.id = TAG_ID(tag);
    dmac->sif0.tag.irq = TAG_IRQ(tag);
    dmac->sif0.tag.addr = TAG_ADDR(tag);
    dmac->sif0.tag.mem = TAG_MEM(tag);
    dmac->sif0.tag.data = TAG_DATA(tag);

    switch (dmac->sif0.tag.id) {
        case 7:
            dmac->sif0.tag.end = 1;
        case 0:
        case 1:
            dmac->sif0.madr = dmac->sif0.tag.addr;
    }

    for (int i = 0; i < dmac->sif0.tag.qwc; i++) {
        uint128_t q = ps2_sif_fifo_read(dmac->sif);

        printf("writing %016lx %016lx to %08x\n", q.u64[1], q.u64[0], dmac->sif0.madr);

        ee_bus_write128(dmac->bus, dmac->sif0.madr, q);

        dmac->sif0.madr += 16;
    }

    // printf("eedmac: sif0 tag qwc=%08lx id=%ld irq=%ld addr=%08lx mem=%ld data=%016lx tte=%d\n",
    //     dmac->sif0.tag.qwc,
    //     dmac->sif0.tag.id,
    //     dmac->sif0.tag.irq,
    //     dmac->sif0.tag.addr,
    //     dmac->sif0.tag.mem,
    //     dmac->sif0.tag.data,
    //     dmac->sif0.chcr
    // );

    ps2_sif_fifo_reset(dmac->sif);

    dmac_set_irq(dmac, DMAC_SIF0);
}