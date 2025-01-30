#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

    // v2+ BIOSes need this value on boot (smh...)
    dmac->enabler = 0x1201;
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

    const char* name = dmac_get_channel_name(dmac, addr);

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

static inline void dmac_process_source_tag(struct ps2_dmac* dmac, struct dmac_channel* c, uint128_t tag) {
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
            printf("dmac: Unsupported %d tag ID\n", c->tag.id);

            exit(1);
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

    // If TIE is set, then end transfer
    if ((c->chcr & 0x80) && c->tag.irq)
        c->tag.end = 1;
}

static inline void dmac_process_dest_tag(struct ps2_dmac* dmac, struct dmac_channel* c, uint128_t tag) {
    c->tag.qwc = TAG_QWC(tag);
    c->tag.pct = TAG_PCT(tag);
    c->tag.id = TAG_ID(tag);
    c->tag.irq = TAG_IRQ(tag);
    c->tag.addr = TAG_ADDR(tag);
    c->tag.mem = TAG_MEM(tag);
    c->tag.data = TAG_DATA(tag);

    c->tag.end = dmac->sif0.tag.irq && (dmac->sif0.chcr & 0x80);

    switch (c->tag.id) {
        case 7:
            c->tag.end = 1;
        case 0:
        case 1:
            c->madr = c->tag.addr;
    }
}

static inline void dmac_set_irq(struct ps2_dmac* dmac, int ch) {
    dmac->stat |= 1 << ch;

    // printf("dmac: channel=%d flag=%08x mask=%08x irq=%08x\n", ch, dmac->stat & 0x3ff, (dmac->stat >> 16) & 0x3ff, (dmac->stat & 0x3ff) & ((dmac->stat >> 16) & 0x3ff));

    if ((dmac->stat & 0x3ff) & ((dmac->stat >> 16) & 0x3ff)) {
        ee_set_int1(dmac->ee);
    } else {
        // Reset INT1
        dmac->ee->cause &= ~EE_CAUSE_IP3;
    }
}

void dmac_handle_vif0_transfer(struct ps2_dmac* dmac) {
    printf("ee: VIF0 channel unimplemented\n");

    dmac_set_irq(dmac, DMAC_VIF0);

    dmac->vif0.chcr &= ~0x100;
}
void dmac_handle_vif1_transfer(struct ps2_dmac* dmac) {
    // printf("ee: VIF1 DMA dir=%d mode=%d tte=%d tie=%d qwc=%d madr=%08x tadr=%08x\n",
    //     dmac->vif1.chcr & 1,
    //     (dmac->vif1.chcr >> 2) & 3,
    //     (dmac->vif1.chcr >> 6) & 1,
    //     (dmac->vif1.chcr >> 7) & 1,
    //     dmac->vif1.qwc,
    //     dmac->vif1.madr,
    //     dmac->vif1.tadr
    // );

    for (int i = 0; i < dmac->vif1.qwc; i++) {
        uint128_t q;

        if (dmac->vif1.madr & 0x80000000) {
            q = ps2_ram_read128(dmac->ee->scratchpad, dmac->vif1.madr & 0x3fff);
        } else {
            q = ee_bus_read128(dmac->bus, dmac->vif1.madr);
        }

        // VIF1 FIFO address
        ee_bus_write32(dmac->bus, 0x10005000, q.u32[0]);
        ee_bus_write32(dmac->bus, 0x10005000, q.u32[1]);
        ee_bus_write32(dmac->bus, 0x10005000, q.u32[2]);
        ee_bus_write32(dmac->bus, 0x10005000, q.u32[3]);

        dmac->vif1.madr += 16;
    }

    if (((dmac->vif1.chcr >> 2) & 7) != 1) {
        dmac->vif1.chcr &= ~0x100;

        dmac_set_irq(dmac, DMAC_VIF1);

        return;
    }

    int loop = 0;
    // Chain mode
    do {
        uint128_t tag = ee_bus_read128(dmac->bus, dmac->vif1.tadr);

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

        for (int i = 0; i < dmac->vif1.tag.qwc; i++) {
            uint128_t q;

            if (dmac->vif1.madr & 0x80000000) {
                q = ps2_ram_read128(dmac->ee->scratchpad, dmac->vif1.madr & 0x3fff);
            } else {
                q = ee_bus_read128(dmac->bus, dmac->vif1.madr);
            }

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

    dmac->vif1.chcr &= ~0x100;

    dmac_set_irq(dmac, DMAC_VIF1);
}
void dmac_handle_gif_transfer(struct ps2_dmac* dmac) {
    dmac_set_irq(dmac, DMAC_GIF);

    dmac->gif.chcr &= ~0x100;

    // printf("dmac: GIF transfer mode=%d madr=%08x qwc=%d\n", (dmac->gif.chcr >> 2) & 7, dmac->gif.madr, dmac->gif.qwc);
    for (int i = 0; i < dmac->gif.qwc; i++) {
        uint128_t q;

        if (dmac->gif.madr & 0x80000000) {
            q = ps2_ram_read128(dmac->ee->scratchpad, dmac->gif.madr & 0x3fff);
        } else {
            q = ee_bus_read128(dmac->bus, dmac->gif.madr);
        }

        // GIF FIFO address
        ee_bus_write128(dmac->bus, 0x10006000, q);

        dmac->gif.madr += 16;
    }

    if (((dmac->gif.chcr >> 2) & 7) != 1)
        return;

    // Chain mode

    do {
        uint128_t tag = ee_bus_read128(dmac->bus, dmac->gif.tadr);

        dmac_process_source_tag(dmac, &dmac->gif, tag);

        // printf("ee: gif qwc=%08x madr=%08x tadr=%08x\n", dmac->gif.tag.qwc, dmac->gif.madr, dmac->gif.tadr);

        for (int i = 0; i < dmac->gif.tag.qwc; i++) {
            uint128_t q;

            if (dmac->gif.madr & 0x80000000) {
                q = ps2_ram_read128(dmac->ee->scratchpad, dmac->gif.madr & 0x3fff);
            } else {
                q = ee_bus_read128(dmac->bus, dmac->gif.madr);
            }

            // printf("ee: Sending %016lx%016lx from %08x to GIF FIFO\n",
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
    printf("ee: IPU from channel unimplemented\n");

    dmac_set_irq(dmac, DMAC_IPU_FROM);

    dmac->ipu_from.chcr &= ~0x100;
}
void dmac_handle_ipu_to_transfer(struct ps2_dmac* dmac) {
    printf("ee: IPU to channel unimplemented\n");

    dmac_set_irq(dmac, DMAC_IPU_TO);

    dmac->ipu_to.chcr &= ~0x100;
}
void dmac_handle_sif0_transfer(struct ps2_dmac* dmac) {
    // SIF FIFO is empty, keep waiting
    if (ps2_sif_fifo_is_empty(dmac->sif)) {
        return;
    }

    // Data ready but channel isn't ready yet, keep waiting
    if (!(dmac->sif0.chcr & 0x100)) {
        return;
    }

    while (!ps2_sif_fifo_is_empty(dmac->sif)) {
        uint128_t tag = ps2_sif_fifo_read(dmac->sif);

        dmac_process_dest_tag(dmac, &dmac->sif0, tag);

        // printf("ee: sif0 tag qwc=%08lx madr=%08lx id=%ld irq=%ld addr=%08lx mem=%ld data=%016lx tte=%d\n",
        //     dmac->sif0.tag.qwc,
        //     madr,
        //     dmac->sif0.tag.id,
        //     dmac->sif0.tag.irq,
        //     dmac->sif0.tag.addr,
        //     dmac->sif0.tag.mem,
        //     dmac->sif0.tag.data,
        //     dmac->sif0.chcr
        // );

        for (int i = 0; i < dmac->sif0.tag.qwc; i++) {
            uint128_t q = ps2_sif_fifo_read(dmac->sif);

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

            ee_bus_write128(dmac->bus, dmac->sif0.madr, q);

            dmac->sif0.madr += 16;
        }

        if (channel_is_done(&dmac->sif0)) {
            dmac->sif0.chcr &= ~0x100;

            dmac_set_irq(dmac, DMAC_SIF0);

            ps2_sif_fifo_reset(dmac->sif);

            return;
        }
    }

    // We shouldn't send an interrupt if tag end or irq/tie weren't
    // set
}
void dmac_handle_sif1_transfer(struct ps2_dmac* dmac) {
    assert(!dmac->sif1.qwc);

    if (!ps2_sif_fifo_is_empty(dmac->sif)) {
        printf("dmac: WARNING!!! SIF FIFO not empty\n");
    }

    int end = 0;

    do {
        uint128_t tag = ee_bus_read128(dmac->bus, dmac->sif1.tadr);

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

        for (int i = 0; i < dmac->sif1.tag.qwc; i++) {
            uint128_t q = ee_bus_read128(dmac->bus, dmac->sif1.madr);

            // printf("%08x: ", dmac->sif1.madr);

            // for (int i = 0; i < 16; i++) {
            //     printf("%02x ", q.u8[i]);
            // }

            // putchar('|');

            // for (int i = 0; i < 16; i++) {
            //     printf("%c", isprint(q.u8[i]) ? q.u8[i] : '.');
            // }

            // puts("|");

            ps2_sif_fifo_write(dmac->sif, q);

            dmac->sif1.madr += 16;
        }
    } while (!channel_is_done(&dmac->sif1));

    iop_dma_handle_sif1_transfer(dmac->iop_dma);

    dmac_set_irq(dmac, DMAC_SIF1);

    dmac->sif1.chcr &= ~0x100;
}
void dmac_handle_sif2_transfer(struct ps2_dmac* dmac) {
    printf("ee: SIF2 channel unimplemented\n"); exit(1);
}
void dmac_handle_spr_from_transfer(struct ps2_dmac* dmac) {
    printf("ee: SPR from channel unimplemented\n"); exit(1);
}
void dmac_handle_spr_to_transfer(struct ps2_dmac* dmac) {
    printf("ee: SPR to channel unimplemented\n"); exit(1);
}

static inline void dmac_handle_channel_start(struct ps2_dmac* dmac, uint32_t addr) {
    struct dmac_channel* c = dmac_get_channel(dmac, addr);

    // printf("ee: %s start data=%08x dir=%d mod=%d tte=%d madr=%08x qwc=%08x tadr=%08x\n",
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
    uint32_t istat = data & 0x0000ffff;
    uint32_t imask = data & 0xffff0000;

    dmac->stat &= ~istat;
    dmac->stat ^= imask;

    if ((dmac->stat & 0x3ff) & ((dmac->stat >> 16) & 0x3ff)) {
        ee_set_int1(dmac->ee);
    } else {
        // Reset INT1
        dmac->ee->cause &= ~EE_CAUSE_IP3;
    }
}

void ps2_dmac_write32(struct ps2_dmac* dmac, uint32_t addr, uint64_t data) {
    struct dmac_channel* c = dmac_get_channel(dmac, addr);

    const char* name = dmac_get_channel_name(dmac, addr);

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

    if (c) {
        switch (addr & 0xff) {
            case 0x00: {
                c->chcr = data;

                // printf("ee: Set %s chcr <- %08x %08x\n", name, data, dmac->sif1.chcr);

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
}