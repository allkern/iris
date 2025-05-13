#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>

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
    int cinten = !!(dma->dmacinten & 1);
    int minten = !(dma->dmacinten & 2);

    int mif = be || (mcien && (dicr_flags || dicr2_flags));

    mif = mif && cinten;

    dma->dicr &= 0x7fffffff;
    dma->dicr |= mif ? 0x80000000 : 0;

    if (mif && minten) {
        ps2_iop_intc_irq(dma->intc, IOP_INTC_DMA);
    }
}

struct ps2_iop_dma* ps2_iop_dma_create(void) {
    return malloc(sizeof(struct ps2_iop_dma));
}

void ps2_iop_dma_init(struct ps2_iop_dma* dma, struct ps2_iop_intc* intc, struct ps2_sif* sif, struct ps2_cdvd* cdvd, struct ps2_dmac* ee_dma, struct ps2_sio2* sio2, struct ps2_spu2* spu, struct sched_state* sched, struct iop_bus* bus) {
    memset(dma, 0, sizeof(struct ps2_iop_dma));

    dma->intc = intc;
    dma->bus = bus;
    dma->sif = sif;
    dma->drive = cdvd;
    dma->sched = sched;
    dma->ee_dma = ee_dma;
    dma->sio2 = sio2;
    dma->spu = spu;

    dma->dmacinten = 0x01;
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

static inline void dma_fetch_tag(struct ps2_iop_dma* dma, struct iop_dma_channel* c) {
    c->tag = iop_bus_read32(dma->bus, c->tadr);
    c->tag |= (uint64_t)iop_bus_read32(dma->bus, c->tadr + 4) << 32;

    c->addr = c->tag & 0x7fffff;
    c->size = (c->tag >> 32) & 0xffffff;
    c->irq = !!(c->tag & 0x40000000);
    c->eot = !!(c->tag & 0x80000000);
    c->extra = !!(c->chcr & 0x100);

    // Round to 8 words
    c->size = (c->size + 3) & ~3;
}

void iop_dma_handle_mdec_in_transfer(struct ps2_iop_dma* dma) {
    printf("iop: MDEC in channel unimplemented\n"); exit(1);
}
void iop_dma_handle_mdec_out_transfer(struct ps2_iop_dma* dma) {
    printf("iop: MDEC out channel unimplemented\n"); exit(1);
}
void iop_dma_handle_sif2_transfer(struct ps2_iop_dma* dma) {
    printf("iop: SIF2 channel unimplemented\n"); exit(1);
}
void iop_dma_handle_cdvd_transfer(struct ps2_iop_dma* dma) {
    // No data in CDVD buffer yet
    if (!dma->drive->buf_size)
        return;

    // Channel not yet started
    if (!(dma->cdvd.chcr & 0x1000000)) {
        printf("iop: CDVD transfer incoming, channel not yet started (%08x)\n", dma->cdvd.chcr);

        // exit(1);

        return;
    }

    // printf("iop: Writing %d bytes of sector data to %08x (%08x)\n", dma->drive->buf_size, dma->cdvd.madr, dma->cdvd.bcr);

    // uint32_t addr = dma->cdvd.madr;

    int i = 0;

    while (dma->cdvd.transfer_size && dma->drive->buf_size) {
        iop_bus_write8(dma->bus, dma->cdvd.madr++, dma->drive->buf[i++]);

        dma->drive->buf_size--;
        dma->cdvd.transfer_size--;
    }

    // printf("dma: buf_size=%d transfer_size=%d\n",
    //     dma->drive->buf_size,
    //     dma->cdvd.transfer_size
    // );

    // int size = dma->drive->buf_size;

    // while (size > 0) {
    //     printf("%08x: ", addr);

    //     for (int i = 0; i < 16; i++) {
    //         printf("%02x ", iop_bus_read8(dma->bus, addr + i));
    //     }

    //     putchar('|');

    //     for (int i = 0; i < 16; i++) {
    //         uint8_t b = iop_bus_read8(dma->bus, addr + i);

    //         printf("%c", isprint(b) ? b : '.');
    //     }

    //     puts("|");

    //     addr += 16;
    //     size -= 16;
    // }

    // Only end the transfer when there aren't any
    // blocks left to copy
    if (dma->cdvd.transfer_size)
        return;

    // printf("dma: Sending IRQ to IOP\n");
    iop_dma_set_dicr_flag(dma, IOP_DMA_CDVD);
    iop_dma_check_irq(dma);

    // printf("cdvd: Ending transfer\n");
    dma->cdvd.chcr &= ~0x1000000;
    dma->cdvd.bcr = 0;
}

void spu1_dma_irq_event_handler(void* udata, int overshoot) {
    struct ps2_iop_dma* dma = (struct ps2_iop_dma*)udata;

    iop_dma_set_dicr_flag(dma, IOP_DMA_SPU1);
    iop_dma_check_irq(dma);

    dma->spu1.chcr &= ~0x1000000;
}

static FILE* file;

void iop_dma_handle_spu1_transfer(struct ps2_iop_dma* dma) {
    // printf("spu2 core0: chcr=%08x madr=%08x bcr=%08x adma=%d\n", dma->spu1.chcr, dma->spu1.madr, dma->spu1.bcr, dma->spu->c[0].admas);

    unsigned int size = (dma->spu1.bcr & 0xffff) * (dma->spu1.bcr >> 16);

    for (int i = 0; i < size; i++) {
        uint32_t d = iop_bus_read32(dma->bus, dma->spu1.madr);

        iop_bus_write16(dma->bus, 0x1f9001ac, d & 0xffff);
        iop_bus_write16(dma->bus, 0x1f9001ac, d >> 16);

        dma->spu1.madr += 4;
    }

    // If ADMA is on, then the transfer will be ended by the SPU2
    // whenever all samples were played
    if (dma->spu->c[0].admas & 1)
        return;

    // Else we need to end the transfer ourselves
    struct sched_event spu1_dma_irq_event;

    spu1_dma_irq_event.callback = spu1_dma_irq_event_handler;
    spu1_dma_irq_event.cycles = 1000000;
    spu1_dma_irq_event.name = "SPU1 DMA IRQ event";
    spu1_dma_irq_event.udata = dma;

    sched_schedule(dma->sched, spu1_dma_irq_event);
}
void iop_dma_handle_pio_transfer(struct ps2_iop_dma* dma) {
    printf("iop: PIO channel unimplemented\n"); exit(1);
}
void iop_dma_handle_otc_transfer(struct ps2_iop_dma* dma) {
    printf("iop: OTC channel unimplemented\n"); exit(1);
}

void spu2_dma_irq_event_handler(void* udata, int overshoot) {
    struct ps2_iop_dma* dma = (struct ps2_iop_dma*)udata;

    iop_dma_set_dicr_flag(dma, IOP_DMA_SPU2);
    iop_dma_check_irq(dma);

    dma->spu2.chcr &= ~0x1000000;
}

void iop_dma_handle_spu2_transfer(struct ps2_iop_dma* dma) {
    // printf("spu2 core1: chcr=%08x madr=%08x bcr=%08x bytes=%d (%08x) adma=%d\n", dma->spu2.chcr, dma->spu2.madr, dma->spu2.bcr,
    //     (dma->spu2.bcr & 0xffff) * (dma->spu2.bcr >> 16) * 4, (dma->spu2.bcr & 0xffff) * (dma->spu2.bcr >> 16) * 4, dma->spu->c[1].admas
    // );

    unsigned int size = (dma->spu2.bcr & 0xffff) * (dma->spu2.bcr >> 16);

    // file = fopen("core1.adma", "ab");

    // uint16_t buf[0x400];
    // uint16_t* ptr = buf;

    for (int i = 0; i < size; i++) {
        uint32_t d = iop_bus_read32(dma->bus, dma->spu2.madr);

        // *ptr++ = d & 0xffff;
        // *ptr++ = d >> 16;

        iop_bus_write16(dma->bus, 0x1f9005ac, d & 0xffff);
        iop_bus_write16(dma->bus, 0x1f9005ac, d >> 16);

        dma->spu2.madr += 4;
    }

    // for (int i = 0; i < 0x100; i++) {
    //     uint16_t s0 = buf[i+0x000];
    //     uint16_t s1 = buf[i+0x100];

    //     iop_bus_write16(dma->bus, 0x1f9005ac, s0);
    //     iop_bus_write16(dma->bus, 0x1f9005ac, s1);

    //     fwrite(&s0, sizeof(uint16_t), 1, file);
    //     fwrite(&s1, sizeof(uint16_t), 1, file);
    // }

    // for (int i = 0; i < 0x100; i++) {
    //     uint16_t s0 = buf[i+0x200];
    //     uint16_t s1 = buf[i+0x300];

    //     fwrite(&s0, sizeof(uint16_t), 1, file);
    //     fwrite(&s1, sizeof(uint16_t), 1, file);
    // }

    // fclose(file);

    // If ADMA is on, then the transfer will be ended by the SPU2
    // whenever all samples were played
    if (dma->spu->c[1].admas & 2)
        return;

    // Else we need to end the transfer ourselves
    struct sched_event spu2_dma_irq_event;

    spu2_dma_irq_event.callback = spu2_dma_irq_event_handler;
    spu2_dma_irq_event.cycles = 1000000;
    spu2_dma_irq_event.name = "SPU2 DMA IRQ event";
    spu2_dma_irq_event.udata = dma;

    sched_schedule(dma->sched, spu2_dma_irq_event);
}
void iop_dma_handle_dev9_transfer(struct ps2_iop_dma* dma) {
    printf("iop: DEV9 channel unimplemented\n"); exit(1);
}
void iop_dma_handle_sif0_transfer(struct ps2_iop_dma* dma) {
    // if (!ps2_sif0_is_empty(dma->sif)) {
    //     printf("iopdma: SIF FIFO not empty\n");

    //     exit(1);
    // }

    // ps2_sif0_reset(dma->sif);

    dma->sif0.eot = 0;

    do {
        dma_fetch_tag(dma, &dma->sif0);

        // printf("iop: SIF0 tag at %08x extra=%u addr=%08x size=%08x irq=%d eot=%d\n",
        //     dma->sif0.tadr, dma->sif0.extra, dma->sif0.addr, dma->sif0.size, dma->sif0.irq, dma->sif0.eot
        // );

        uint128_t q;

        if (dma->sif0.extra) {
            q.u32[0] = iop_bus_read32(dma->bus, dma->sif0.tadr + 8);
            q.u32[1] = iop_bus_read32(dma->bus, dma->sif0.tadr + 12);
            q.u32[2] = 0;
            q.u32[3] = 0;

            ps2_sif0_write(dma->sif, q);
        }

        while (dma->sif0.size) {
            q.u32[0] = iop_bus_read32(dma->bus, dma->sif0.addr);
            q.u32[1] = iop_bus_read32(dma->bus, dma->sif0.addr + 4);
            q.u32[2] = iop_bus_read32(dma->bus, dma->sif0.addr + 8);
            q.u32[3] = iop_bus_read32(dma->bus, dma->sif0.addr + 12);

            ps2_sif0_write(dma->sif, q);

            dma->sif0.addr += 16;
            dma->sif0.size -= 4;
        }

        dma->sif0.tadr += (dma->sif0.extra ? 4 : 2) * 4;
    } while (!dma->sif0.eot);

    iop_dma_set_dicr_flag(dma, IOP_DMA_SIF0);
    iop_dma_check_irq(dma);

    dmac_handle_sif0_transfer(dma->ee_dma);

    dma->sif0.tadr += dma->sif0.extra ? 4 : 2;
    dma->sif0.chcr &= ~0x1000000;
}

#include "rpc.h"

void iop_dma_handle_sif1_transfer(struct ps2_iop_dma* dma) {
    // No data in the SIF FIFO yet
    if (ps2_sif1_is_empty(dma->sif))
        return;

    // Data ready but channel isn't ready yet, keep waiting
    if (!(dma->sif1.chcr & 0x1000000)) {
        // printf("iop: EE sent SIF1 but channel isn't ready\n");

        return;
    }

    // Data ready and channel is started, do transfer
    int eot;

    do {
        uint128_t q = ps2_sif1_read(dma->sif);

        uint64_t tag = q.u64[0];

        uint32_t addr = tag & 0x7fffff;
        int size = (tag >> 32) & 0xffffff;
        int irq = !!(tag & 0x40000000);
        eot = !!(tag & 0x80000000);

        // printf("iop: SIF1 tag addr=%08x size=%08x irq=%d eot=%d\n", addr, size, irq, eot);

        // char buf[128];

        // puts(rpc_decode_packet(buf, ((void*)dma->sif->fifo.data) + (dma->sif->fifo.read_index * 16)));

        while (size) {
            uint128_t q = ps2_sif1_read(dma->sif);

            for (int i = 0; i < 4; i++) {
                iop_bus_write32(dma->bus, addr, q.u32[i]);

                addr += 4;
                --size;
            }
        }

        // Send interrupt on tag IRQ (regardless of channel IRQ enable)
        if ((dma->dicr2 & 0x400) && irq) {
            ps2_iop_intc_irq(dma->intc, IOP_INTC_DMA);
        }

        if (ps2_sif1_is_empty(dma->sif))
            break;
    } while (!eot);

    iop_dma_set_dicr_flag(dma, IOP_DMA_SIF1);
    iop_dma_check_irq(dma);

    // ps2_sif1_reset(dma->sif);

    dma->sif1.chcr &= ~0x1000000;
}
void iop_dma_handle_sio2_in_transfer(struct ps2_iop_dma* dma) {
    uint32_t size = (dma->sio2_in.bcr & 0xffff) * (dma->sio2_in.bcr >> 16);

    // printf("dma: SIO2 in transfer size=%d\n", size);

    // sio2_dma_reset(dma->sio2);

    // for (int i = 0; i < size; i++) {
    //     uint32_t w = iop_bus_read32(dma->bus, dma->sio2_in.madr);

    //     queue_push(dma->sio2->in, (w >> 0) & 0xff);
    //     queue_push(dma->sio2->in, (w >> 8) & 0xff);
    //     queue_push(dma->sio2->in, (w >> 16) & 0xff);
    //     queue_push(dma->sio2->in, (w >> 24) & 0xff);

    //     printf("%02x %02x %02x %02x\n",
    //         (w >> 0) & 0xff,
    //         (w >> 8) & 0xff,
    //         (w >> 16) & 0xff,
    //         (w >> 24) & 0xff
    //     );

    //     dma->sio2_in.madr += 4;
    // }

    iop_dma_set_dicr_flag(dma, IOP_DMA_SIO2_IN);
    iop_dma_check_irq(dma);

    dma->sio2_in.chcr &= ~0x1000000;
}
void iop_dma_handle_sio2_out_transfer(struct ps2_iop_dma* dma) {
    // if (queue_is_empty(dma->sio2->out)) {
    //     // printf("dma: Waiting for SIO2 out\n");

    //     return;
    // }

    // // printf("dma: Doing SIO2 out transfer size=%d bcr=%08x\n", queue_size(dma->sio2->out), dma->sio2_out.bcr);

    // iop_dma_set_dicr_flag(dma, IOP_DMA_SIO2_OUT);
    // iop_dma_check_irq(dma);

    // for (int b = 0; b < (dma->sio2_out.bcr >> 16); b++) {
    //     for (int i = 0; i < (dma->sio2_out.bcr & 0xffff); i++) {
    //         iop_bus_write8(dma->bus, dma->sio2_out.madr++, queue_pop(dma->sio2->out));
    //         iop_bus_write8(dma->bus, dma->sio2_out.madr++, queue_pop(dma->sio2->out));
    //         iop_bus_write8(dma->bus, dma->sio2_out.madr++, queue_pop(dma->sio2->out));
    //         iop_bus_write8(dma->bus, dma->sio2_out.madr++, queue_pop(dma->sio2->out));
    //     }
    // }
    for (int b = 0; b < (dma->sio2_out.bcr >> 16); b++) {
        for (int i = 0; i < (dma->sio2_out.bcr & 0xffff); i++) {
            iop_bus_write8(dma->bus, dma->sio2_out.madr++, 0);
            iop_bus_write8(dma->bus, dma->sio2_out.madr++, 0);
            iop_bus_write8(dma->bus, dma->sio2_out.madr++, 0);
            iop_bus_write8(dma->bus, dma->sio2_out.madr++, 0);
        }
    }

    iop_dma_set_dicr_flag(dma, IOP_DMA_SIO2_OUT);
    iop_dma_check_irq(dma);

    dma->sio2_out.chcr &= ~0x1000000;
}

uint64_t ps2_iop_dma_read32(struct ps2_iop_dma* dma, uint32_t addr) {
    struct iop_dma_channel* c = iop_dma_get_channel(dma, addr);

    if (c) {
        switch (addr & 0xf) {
            case 0x0: return c->madr;
            case 0x4: return c->bcr;
            case 0x8: return c->chcr;
            case 0xc: return c->tadr;
        }

        const char* name = iop_dma_get_channel_name(addr);

        printf("iop_dma: Unknown %s register read %08x\n", name, addr);

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

    printf("iop_dma: Unknown DMA register read %08x\n", addr);

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

                if (!(c->chcr & 0x1000000)) {
                    return;
                }

                c->transfer_size = (c->bcr >> 16) * ((c->bcr & 0xffff) * 4);

                // if ((addr & 0xff0) == 0x0b0) {
                //     FILE* file = fopen("cdvd.dump", "a");
                //     fprintf(file, "iop: Starting %s channel with chcr=%08x madr=%08x bcr=%08x tadr=%08x\n", iop_dma_get_channel_name(addr), data, c->madr, c->bcr, c->tadr);
                //     fclose(file);
                // }

                // if ((addr & 0xff0) == 0x0b0)
                // printf("iop: Starting %s channel with chcr=%08x madr=%08x bcr=%08x tadr=%08x\n", iop_dma_get_channel_name(addr), data, c->madr, c->bcr, c->tadr);

                // printf("iop: Starting %s channel with chcr=%08x madr=%08x bcr=%08x tadr=%08x\n", iop_dma_get_channel_name(addr), data, c->madr, c->bcr, c->tadr);

                // Check negative MADR increments
                assert((c->chcr & 2) == 0);

                // Check for burst transfers
                assert(((c->chcr >> 9) & 3) != 0);

                // Check for 0-sized blocks
                assert((c->bcr & 0xffff) != 0);

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

        const char* name = iop_dma_get_channel_name(addr);

        printf("iop_dma: Unknown %s register write %08x %08lx\n", name, addr, data);

        return;
    }

    switch (addr) {
        case 0x1f8010f0: dma->dpcr = data; return;
        case 0x1f801570: dma->dpcr2 = data; return;
        case 0x1f8010f4: iop_dma_set_dicr(dma, data); iop_dma_check_irq(dma); return;
        case 0x1f801574: iop_dma_set_dicr2(dma, data); iop_dma_check_irq(dma); return;
        case 0x1f801578: dma->dmacen = data; return;
        case 0x1f80157c: dma->dmacinten = data; iop_dma_check_irq(dma); return;
    }

    printf("iop_dma: Unknown DMA register write %08x %08lx\n", addr, data);
}

uint64_t ps2_iop_dma_read16(struct ps2_iop_dma* dma, uint32_t addr) {
    struct iop_dma_channel* c = iop_dma_get_channel(dma, addr);

    if (c) {
        switch (addr & 0xf) {
            case 0x0: return c->madr;
            case 0x4: return c->bcr;
            case 0x8: return c->chcr;
            case 0xc: return c->tadr;
        }

        const char* name = iop_dma_get_channel_name(addr);

        printf("iop_dma: Unknown %s register read %08x\n", name, addr);

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

    printf("iop_dma: Unknown DMA register read %08x\n", addr);

    return 0;
}

void ps2_iop_dma_write16(struct ps2_iop_dma* dma, uint32_t addr, uint64_t data) {
    struct iop_dma_channel* c = iop_dma_get_channel(dma, addr);

    if (c) {
        switch (addr & 0xf) {
            case 0x4: if ((addr & 0xff0) == 0x0b0) printf("iop: bcrlo write16 %08x\n", data); c->bcr &= ~0xffff; c->bcr |= data; return;
            case 0x6: if ((addr & 0xff0) == 0x0b0) printf("iop: bcrhi write16 %08x\n", data); c->bcr &= 0xffff; c->bcr |= data << 16; return;
        }

        const char* name = iop_dma_get_channel_name(addr);

        printf("iop_dma: Unknown 16-bit %s register write %08x %08lx\n", name, addr, data);

        exit(1);

        return;
    }

    printf("iop_dma: Unknown DMA register write %08x %08lx\n", addr, data);

    exit(1);
}

void iop_dma_end_spu1_transfer(struct ps2_iop_dma* dma) {
    iop_dma_set_dicr_flag(dma, IOP_DMA_SPU1);
    iop_dma_check_irq(dma);

    dma->spu1.chcr &= ~0x1000000;
}

void iop_dma_end_spu2_transfer(struct ps2_iop_dma* dma) {
    iop_dma_set_dicr_flag(dma, IOP_DMA_SPU2);
    iop_dma_check_irq(dma);

    dma->spu2.chcr &= ~0x1000000;
}
