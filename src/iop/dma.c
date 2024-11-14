#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dma.h"

static inline void iop_dma_set_dicr(struct ps2_iop_dma* dma, uint32_t v) {
    dma->dicr &= ~0xffffff;
    dma->dicr |= v & 0xffffff;
    dma->dicr &= ~(v & 0x7f000000);
}

static inline void iop_dma_set_dicr2(struct ps2_iop_dma* dma, uint32_t v) {
    dma->dicr2 &= ~0xffffff;
    dma->dicr2 |= v & 0xffffff;
    dma->dicr2 &= ~(v & 0x3f000000);
}

inline static void iop_dma_set_dicr_flag(struct ps2_iop_dma* dma, uint32_t ch) {
    if (ch < 7) {
        uint32_t m = 0x10000 << ch;

        if (dma->dicr & m)
            dma->dicr |= 0x1000000 << ch;

        return;
    }

    uint32_t m = 0x10000 << (ch - 7);

    if (dma->dicr2 & m)
        dma->dicr2 |= 0x1000000 << (ch - 7);
}

inline static void iop_dma_check_irq(struct ps2_iop_dma* dma) {
    int be = dma->dicr & 0x8000;
    int mcien = dma->dicr & 0x800000;
    uint32_t dicr_flags = dma->dicr & 0x7f000000;
    uint32_t dicr2_flags = dma->dicr2 & 0x3f000000;

    int mif = be || (mcien && (dicr_flags || dicr2_flags));

    dma->dicr &= 0x7fffffff;
    dma->dicr |= mif ? 0x80000000 : 0;

    if (mif) {
        ps2_iop_intc_irq(dma->intc, IOP_INTC_DMA);
    }
}

struct ps2_iop_dma* ps2_iop_dma_create(void) {
    return malloc(sizeof(struct ps2_iop_dma));
}

void ps2_iop_dma_init(struct ps2_iop_dma* dma, struct ps2_iop_intc* intc, struct ps2_sif* sif, struct ps2_dmac* ee_dma, struct iop_bus* bus) {
    memset(dma, 0, sizeof(struct ps2_iop_dma));

    dma->intc = intc;
    dma->bus = bus;
    dma->sif = sif;
    dma->ee_dma = ee_dma;
}

void ps2_iop_dma_destroy(struct ps2_iop_dma* dma) {
    free(dma);
}

static inline struct iop_dma_channel* iop_dma_get_channel(struct ps2_iop_dma* dma, uint32_t addr) {
    switch (addr & 0xff0) {
        case 0x080: return &dma->mdec_in;
        case 0x090: return &dma->mdec_out;
        case 0x0a0: return &dma->sif2;
        case 0x0b0: return &dma->cdvd;
        case 0x0c0: return &dma->spu1;
        case 0x0d0: return &dma->pio;
        case 0x0e0: return &dma->otc;
        case 0x500: return &dma->spu2;
        case 0x510: return &dma->dev9;
        case 0x520: return &dma->sif0;
        case 0x530: return &dma->sif1;
        case 0x540: return &dma->sio2_in;
        case 0x550: return &dma->sio2_out;
    }

    return NULL;
}

static inline const char* iop_dma_get_channel_name(uint32_t addr) {
    switch (addr & 0xff0) {
        case 0x080: return "mdec_in";
        case 0x090: return "mdec_out";
        case 0x0a0: return "sif2";
        case 0x0b0: return "cdvd";
        case 0x0c0: return "spu1";
        case 0x0d0: return "pio";
        case 0x0e0: return "otc";
        case 0x500: return "spu2";
        case 0x510: return "dev9";
        case 0x520: return "sif0";
        case 0x530: return "sif1";
        case 0x540: return "sio2_in";
        case 0x550: return "sio2_out";
    }

    return NULL;
}
void iop_dma_handle_mdec_in_transfer(struct ps2_iop_dma* dma) {}
void iop_dma_handle_mdec_out_transfer(struct ps2_iop_dma* dma) {}
void iop_dma_handle_sif2_transfer(struct ps2_iop_dma* dma) {}
void iop_dma_handle_cdvd_transfer(struct ps2_iop_dma* dma) {}
void iop_dma_handle_spu1_transfer(struct ps2_iop_dma* dma) {}
void iop_dma_handle_pio_transfer(struct ps2_iop_dma* dma) {}
void iop_dma_handle_otc_transfer(struct ps2_iop_dma* dma) {}
void iop_dma_handle_spu2_transfer(struct ps2_iop_dma* dma) {}
void iop_dma_handle_dev9_transfer(struct ps2_iop_dma* dma) {}
void iop_dma_handle_sif0_transfer(struct ps2_iop_dma* dma) {
    ps2_sif_fifo_reset(dma->sif);

    int extra = !!(dma->sif0.chcr & 0x100);

    uint64_t tag = iop_bus_read32(dma->bus, dma->sif0.tadr);
    tag |= (uint64_t)iop_bus_read32(dma->bus, dma->sif0.tadr + 4) << 32;

    uint32_t addr = tag & 0x7fffff;
    uint32_t size = (tag >> 32) & 0xffffff;
    int irq = !!(tag & 0x40000000);
    int eot = !!(tag & 0x80000000);

    // printf("iop: SIF0 tag at %08x extra=%u addr=%08x size=%08x irq=%d eot=%d\n", dma->sif0.tadr, extra, addr, size, irq, eot);

    size += extra * 2;
    size = (size + 3) & ~3;

    uint128_t q;

    if (extra) {
        q.u32[0] = iop_bus_read32(dma->bus, dma->sif0.tadr + 8);
        q.u32[1] = iop_bus_read32(dma->bus, dma->sif0.tadr + 12);
        q.u32[2] = 0;
        q.u32[3] = 0;

        ps2_sif_fifo_write(dma->sif, q);
    }

    while (size) {
        q.u32[0] = iop_bus_read32(dma->bus, addr);
        q.u32[1] = iop_bus_read32(dma->bus, addr + 4);
        q.u32[2] = iop_bus_read32(dma->bus, addr + 8);
        q.u32[3] = iop_bus_read32(dma->bus, addr + 12);

        ps2_sif_fifo_write(dma->sif, q);

        addr += 16;
        size -= 4;
    }

    ps2_dmac_start_sif0_transfer(dma->ee_dma);

    dma->sif0.chcr &= ~0x1000000;
}

#include "rpc.h"

void iop_dma_handle_sif1_transfer(struct ps2_iop_dma* dma) {
    // No data in the SIF FIFO yet
    if (ps2_sif_fifo_is_empty(dma->sif))
        return;

    // Data ready but channel isn't ready yet, keep waiting
    if (!(dma->sif1.chcr & 0x1000000))
        return;

    // Data ready and channel is started, do transfer
    uint128_t q = ps2_sif_fifo_read(dma->sif);

    uint64_t tag = q.u64[0];

    uint32_t addr = tag & 0x7fffff;
    uint32_t size = (tag >> 32) & 0xffffff;
    int irq = !!(tag & 0x40000000);
    int eot = !!(tag & 0x80000000);

    // printf("iop: SIF1 tag addr=%08x size=%08x irq=%d eot=%d\n", addr, size, irq, eot);

    uint32_t* ptr = (uint32_t*)&dma->sif->fifo.data;

    ptr += 4;

    char buf[128];

    puts(rpc_decode_packet(buf, ptr));

    while (size) {
        // printf("writing %08x to %08x\n", *ptr, addr);
        iop_bus_write32(dma->bus, addr, *ptr++);

        addr += 4;
        --size;
    }

    iop_dma_set_dicr_flag(dma, IOP_DMA_SIF1);
    iop_dma_check_irq(dma);

    ps2_sif_fifo_reset(dma->sif);

    dma->sif1.chcr &= ~0x1000000;
}
void iop_dma_handle_sio2_in_transfer(struct ps2_iop_dma* dma) {}
void iop_dma_handle_sio2_out_transfer(struct ps2_iop_dma* dma) {}

uint64_t ps2_iop_dma_read32(struct ps2_iop_dma* dma, uint32_t addr) {
    struct iop_dma_channel* c = iop_dma_get_channel(dma, addr);

    const char* name = iop_dma_get_channel_name(addr);

    if (c) {
        switch (addr & 0xf) {
            case 0x0: return c->madr;
            case 0x4: return c->bcr;
            case 0x8: return c->chcr;
            case 0xc: return c->tadr;
        }

        printf("iop_dma: Unknown channel register %08x\n", addr);

        return 0;
    }

    switch (addr) {
        case 0x1f8010f0: return dma->dpcr;
        case 0x1f801570: return dma->dpcr2;
        case 0x1f8010f4: return dma->dicr;
        case 0x1f801574: return dma->dicr2;
        case 0x1f801578: return dma->dmacen;
        case 0x1f80157c: return dma->dmacinten;
    }

    printf("iop_dma: Unknown DMA register %08x\n", addr);

    return 0;
}

void ps2_iop_dma_write32(struct ps2_iop_dma* dma, uint32_t addr, uint64_t data) {
    struct iop_dma_channel* c = iop_dma_get_channel(dma, addr);

    if (c) {
        switch (addr & 0xf) {
            case 0x0: c->madr = data; return;
            case 0x4: c->bcr = data; return;
            case 0x8: {
                c->chcr = data;

                if (!(c->chcr & 0x1000000))
                    return;

                // printf("iop: Starting %s channel with chcr=%08x\n", iop_dma_get_channel_name(addr), data);

                switch (addr & 0xff0) {
                    case 0x080: iop_dma_handle_mdec_in_transfer(dma); break;
                    case 0x090: iop_dma_handle_mdec_out_transfer(dma); break;
                    case 0x0a0: iop_dma_handle_sif2_transfer(dma); break;
                    case 0x0b0: iop_dma_handle_cdvd_transfer(dma); break;
                    case 0x0c0: iop_dma_handle_spu1_transfer(dma); break;
                    case 0x0d0: iop_dma_handle_pio_transfer(dma); break;
                    case 0x0e0: iop_dma_handle_otc_transfer(dma); break;
                    case 0x500: iop_dma_handle_spu2_transfer(dma); break;
                    case 0x510: iop_dma_handle_dev9_transfer(dma); break;
                    case 0x520: iop_dma_handle_sif0_transfer(dma); break;
                    case 0x530: iop_dma_handle_sif1_transfer(dma); break;
                    case 0x540: iop_dma_handle_sio2_in_transfer(dma); break;
                    case 0x550: iop_dma_handle_sio2_out_transfer(dma); break;
                }

                return;
            }
            case 0xc: c->tadr = data; return;
        }

        printf("iop_dma: Unknown channel register %08x\n", addr);

        return;
    }

    switch (addr) {
        case 0x1f8010f0: dma->dpcr = data; return;
        case 0x1f801570: dma->dpcr2 = data; return;
        case 0x1f8010f4: iop_dma_set_dicr(dma, data); iop_dma_check_irq(dma); return;
        case 0x1f801574: iop_dma_set_dicr2(dma, data); iop_dma_check_irq(dma); return;
        case 0x1f801578: dma->dmacen = data; return;
        case 0x1f80157c: dma->dmacinten = data; return;
    }

    printf("iop_dma: Unknown DMA register %08x\n", addr);
}