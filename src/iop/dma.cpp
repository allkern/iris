#include <new>
#include <cctype>
#include <signal.h>

#include "cdvd.hpp"
#include "sio2.hpp"
#include "spu2.hpp"
#include "bus.hpp"
#include "iop_def.hpp"
#include "dma.hpp"
#include "rpc.hpp"

namespace iris::iop::dma {

static inline void set_dicr(Dma* dma, uint32_t v) {
    dma->dicr &= ~0xffffff;
    dma->dicr |= v & 0xffffff;
    dma->dicr &= ~(v & 0x7f000000);
}

static inline void set_dicr2(Dma* dma, uint32_t v) {
    dma->dicr2 &= ~0xffffff;
    dma->dicr2 |= v & 0xffffff;
    dma->dicr2 &= ~(v & 0x3f000000);
}

inline static void set_dicr_flag(Dma* dma, uint32_t ch) {
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

inline static void check_irq(Dma* dma) {
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
        iop::intc::irq(dma->hw.intc, iop::intc::DMA);
    }
}

Dma* create(logger::Logger* logger, iop::intc::Intc* intc, sif::Sif* sif, speed::Speed* speed, scheduler::Scheduler* sched, iop::Iop* iop, bus::Bus* bus) {
    Dma* dma = new Dma();

    dma->logger = logger;
    dma->logger_id = logger::register_source(logger, "iop_dma");

    dma->hw.intc = intc;
    dma->hw.sif = sif;
    dma->hw.speed = speed;
    dma->hw.sched = sched;
    dma->hw.iop = iop;
    dma->hw.bus = bus;

    reset(dma);

    return dma;
}

void connect(Dma* dma, cdvd::Cdvd* cdvd, ee::dmac::Dmac* ee_dma, sio2::Sio2* sio2, spu2::Spu2* spu2, s2x6::acata::Acata* s2x6_acata, s2x6::acram::Acram* s2x6_acram) {
    dma->hw.cdvd = cdvd;
    dma->hw.ee_dma = ee_dma;
    dma->hw.sio2 = sio2;
    dma->hw.spu2 = spu2;
    dma->hw.s2x6_acata = s2x6_acata;
    dma->hw.s2x6_acram = s2x6_acram;
}

void reset(Dma* dma) {
    auto hw = dma->hw;

    logger::Logger* logger = dma->logger;
    size_t logger_id = dma->logger_id;

    new (dma) Dma();

    dma->logger = logger;
    dma->logger_id = logger_id;

    dma->hw = hw;

    dma->dmacinten = 0x01;
}

void destroy(Dma* dma) {
    delete dma;
}

static inline Channel* get_channel(Dma* dma, uint32_t addr) {
    switch (addr & 0xff0) {
        case 0x080: return &dma->channels[MDEC_IN];
        case 0x090: return &dma->channels[MDEC_OUT];
        case 0x0a0: return &dma->channels[SIF2];
        case 0x0b0: return &dma->channels[CDVD];
        case 0x0c0: return &dma->channels[SPU1];
        case 0x0d0: return &dma->channels[PIO];
        case 0x0e0: return &dma->channels[OTC];
        case 0x500: return &dma->channels[SPU2];
        case 0x510: return &dma->channels[DEV9];
        case 0x520: return &dma->channels[SIF0];
        case 0x530: return &dma->channels[SIF1];
        case 0x540: return &dma->channels[SIO2_IN];
        case 0x550: return &dma->channels[SIO2_OUT];
    }

    return NULL;
}

static inline const char* get_channel_name(uint32_t addr) {
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

static inline void dma_fetch_tag(Dma* dma, Channel* c) {
    c->tag = bus::read32(dma->hw.bus, c->tadr);
    c->tag |= (uint64_t)bus::read32(dma->hw.bus, c->tadr + 4) << 32;

    c->addr = c->tag & 0x7fffff;
    c->size = (c->tag >> 32) & 0xffffff;
    c->irq = !!(c->tag & 0x40000000);
    c->eot = !!(c->tag & 0x80000000);
    c->extra = !!(c->chcr & 0x100);

    // Round to 8 words
    c->size = (c->size + 3) & ~3;
}

void handle_mdec_in_transfer(Dma* dma) {
    iris_fatal_error(dma, "iop: MDEC in channel unimplemented");
}
void handle_mdec_out_transfer(Dma* dma) {
    iris_fatal_error(dma, "iop: MDEC out channel unimplemented");
}
void handle_sif2_transfer(Dma* dma) {
    iris_fatal_error(dma, "iop: SIF2 channel unimplemented");
}
void handle_cdvd_transfer(Dma* dma) {
    // No data in CDVD buffer yet
    if (!dma->hw.cdvd->buf_size)
        return;

    // Channel not yet started
    if (!(dma->channels[CDVD].chcr & 0x1000000)) {
        iris_debug(dma, "iop: CDVD transfer incoming, channel not yet started ({:08x})", dma->channels[CDVD].chcr);

        // exit(1);

        return;
    }

    // iris_debug(dma, "iop: Writing {} bytes of sector data to {:08x} ({:08x})", dma->hw.cdvd->buf_size, dma->channels[CDVD].madr, dma->channels[CDVD].bcr);

    // uint32_t addr = dma->channels[CDVD].madr;

    int i = 0;

    while (dma->channels[CDVD].transfer_size && dma->hw.cdvd->buf_size) {
        iop::invalidate_cache_page(dma->hw.iop, dma->channels[CDVD].madr);
        bus::write8(dma->hw.bus, dma->channels[CDVD].madr++, dma->hw.cdvd->buf[i++]);

        dma->hw.cdvd->buf_size--;
        dma->channels[CDVD].transfer_size--;
    }

    // iris_debug(dma, "buf_size={} transfer_size={}", //     dma->hw.cdvd->buf_size,
    //     dma->channels[CDVD].transfer_size
    //);

    // int size = dma->hw.cdvd->buf_size;

    // while (size > 0) {
    //     iris_debug(dma, "{:08x}:", addr);

    //     for (int i = 0; i < 16; i++) {
    //         iris_debug(dma, "{:02x}", bus::read8(dma->hw.bus, addr + i));
    //     }

    //     putchar('|');

    //     for (int i = 0; i < 16; i++) {
    //         uint8_t b = bus::read8(dma->hw.bus, addr + i);

    //         iris_debug(dma, "{}", isprint(b) ? b : '.');
    //     }

    //     puts("|");

    //     addr += 16;
    //     size -= 16;
    // }

    // Only end the transfer when there aren't any
    // blocks left to copy
    if (dma->channels[CDVD].transfer_size)
        return;

    // iris_debug(dma, "Sending IRQ to IOP");
    set_dicr_flag(dma, CDVD);
    check_irq(dma);

    // iris_debug(dma, "cdvd: Ending transfer");
    dma->channels[CDVD].chcr &= ~0x1000000;
    dma->channels[CDVD].bcr = 0;
}

/* Notes on SPU ADMA timing:

   IOP runs at 36.864 MHz, that is, 36864000 cycles per second. The SPU outputs at
   48 KHz, 36864000 / 48000 = 768 cycles per *stereo* sample. Each stereo sample is
   4 bytes, so that's 192 "cycles per byte", we use this metric to calculate how long
   a transfer should take based on the number of bytes being transferred.

   Our scheduler works in terms of EE cycles, so after we get the number of IOP cycles
   a transfer should take, we multiply it by 8 to get the number of EE cycles.

   e.g. Marvel vs. Capcom 2 sends 2048 bytes per transfer, that's 1024 mono samples
        or 512 stereo samples. The transfer should take 2048 * 192 = 393216 IOP cycles
        or 393216 * 8 = 3145728 EE cycles.
*/

void spu1_dma_irq_event_handler(void* udata, int overshoot) {
    Dma* dma = (Dma*)udata;

    set_dicr_flag(dma, SPU1);
    check_irq(dma);

    dma->channels[SPU1].chcr &= ~0x1000000;
}

#define ADMA_MADR_STEP 0x100

static void spu1_adma_advance_handler(void* udata, int overshoot);
static void spu2_adma_advance_handler(void* udata, int overshoot);

static void spu_adma_advance(Dma* dma, int ch, void (*cb)(void*, int)) {
    Channel* c = &dma->channels[ch];

    if (!(c->chcr & 0x1000000)) {
        c->adma_remaining = 0;

        return;
    }

    int step = (c->adma_remaining < ADMA_MADR_STEP) ? c->adma_remaining : ADMA_MADR_STEP;

    c->madr += step;
    c->adma_remaining -= step;

    if (c->adma_remaining <= 0) {
        set_dicr_flag(dma, ch);
        check_irq(dma);

        c->chcr &= ~0x1000000;

        return;
    }

    int next = (c->adma_remaining < ADMA_MADR_STEP) ? c->adma_remaining : ADMA_MADR_STEP;

    scheduler::Event event;

    event.callback = cb;
    event.cycles = (long)next * c->adma_cpb;
    event.name = "SPU ADMA MADR advance";
    event.udata = dma;

    scheduler::schedule(dma->hw.sched, event);
}

static void spu1_adma_advance_handler(void* udata, int overshoot) {
    spu_adma_advance((Dma*)udata, SPU1, spu1_adma_advance_handler);
}

static void spu2_adma_advance_handler(void* udata, int overshoot) {
    spu_adma_advance((Dma*)udata, SPU2, spu2_adma_advance_handler);
}

static void spu_adma_start_tracking(Dma* dma, int ch, uint32_t madr_start, uint32_t size, int cpb, void (*cb)(void*, int)) {
    Channel* c = &dma->channels[ch];

    c->madr = madr_start;
    c->adma_remaining = size;
    c->adma_cpb = cpb;

    int first = (size < ADMA_MADR_STEP) ? size : ADMA_MADR_STEP;

    scheduler::Event event;

    event.callback = cb;
    event.cycles = (long)first * cpb;
    event.name = "SPU ADMA MADR advance";
    event.udata = dma;

    scheduler::schedule(dma->hw.sched, event);
}

void handle_spu1_transfer(Dma* dma) {
    // If ADMA is off, then transfer all the data at once and trigger
    // an IRQ event to signal the end of the transfer
    if (!(dma->hw.spu2->c[0].admas & 1)) {
        unsigned int size = (dma->channels[SPU1].bcr & 0xffff) * (dma->channels[SPU1].bcr >> 16);

        int write = dma->channels[SPU1].chcr & 1;

        for (int i = 0; i < size; i++) {
            if (write) {
                uint32_t d = bus::read32(dma->hw.bus, dma->channels[SPU1].madr);

                bus::write16(dma->hw.bus, 0x1f9001ac, d & 0xffff);
                bus::write16(dma->hw.bus, 0x1f9001ac, d >> 16);
            } else {
                uint16_t lo = spu2::read_data(dma->hw.spu2, 0);
                uint16_t hi = spu2::read_data(dma->hw.spu2, 0);

                bus::write32(dma->hw.bus, dma->channels[SPU1].madr, lo | ((uint32_t)hi << 16));
            }

            dma->channels[SPU1].madr += 4;
        }

        scheduler::Event spu1_dma_irq_event;

        spu1_dma_irq_event.callback = spu1_dma_irq_event_handler;
        spu1_dma_irq_event.cycles = 10000;
        spu1_dma_irq_event.name = "SPU1 DMA IRQ event";
        spu1_dma_irq_event.udata = dma;

        scheduler::schedule(dma->hw.sched, spu1_dma_irq_event);

        return;
    }

    if ((dma->channels[SPU1].chcr & 0x01000000) == 0)
        return;

    uint32_t size = dma->channels[SPU1].transfer_size;
    uint32_t madr_start = dma->channels[SPU1].madr;
    uint16_t* buf = (uint16_t *)malloc(size);

    // iris_debug(dma, "CORE0 ADMA transfer size={} bytes cycles={}", size, size * 192);

    int index = 0;

    while (dma->channels[SPU1].transfer_size) {
        uint32_t d = bus::read32(dma->hw.bus, dma->channels[SPU1].madr);

        buf[index++] = d & 0xffff;
        buf[index++] = d >> 16;

        dma->channels[SPU1].madr += 4;
        dma->channels[SPU1].transfer_size -= 4;
    }

    spu2::adma_write(dma->hw.spu2, 0, buf, size);

    free(buf);

    int cpb = spu2::adma_is_bitstream(dma->hw.spu2) ? (96 * 8) : (192 * 8);

    spu_adma_start_tracking(dma, SPU1, madr_start, size, cpb, spu1_adma_advance_handler);
}
void handle_pio_transfer(Dma* dma) {
    iris_fatal_error(dma, "iop: PIO channel unimplemented");
}
void handle_otc_transfer(Dma* dma) {
    iris_fatal_error(dma, "iop: OTC channel unimplemented");
}

void spu2_dma_irq_event_handler(void* udata, int overshoot) {
    Dma* dma = (Dma*)udata;

    set_dicr_flag(dma, SPU2);
    check_irq(dma);

    dma->channels[SPU2].chcr &= ~0x1000000;
}

void handle_spu2_transfer(Dma* dma) {
    if ((dma->channels[SPU2].chcr & 0x1000000) == 0)
        return;

    // iris_debug(dma, "spu2 core1: chcr={:08x} madr={:08x} bcr={:08x} bytes={} ({:08x}) adma={}", dma->channels[SPU2].chcr, dma->channels[SPU2].madr, dma->channels[SPU2].bcr,
    //     (dma->channels[SPU2].bcr & 0xffff) * (dma->channels[SPU2].bcr >> 16) * 4, (dma->channels[SPU2].bcr & 0xffff) * (dma->channels[SPU2].bcr >> 16) * 4, dma->hw.spu2->c[1].admas
    //);

    if (!dma->channels[SPU2].transfer_size)
        return;

    // If ADMA is off, then transfer all the data at once and trigger
    // an IRQ event to signal the end of the transfer
    if (!(dma->hw.spu2->c[1].admas & 2)) {
        unsigned int size = (dma->channels[SPU2].bcr & 0xffff) * (dma->channels[SPU2].bcr >> 16);

        // Note: Grand THeft Auto - Vice City depends on a number of things:
        //       - SPU readback
        //       - MEMOUT (mix to RAM)
        //       - Noise generator
        int write = dma->channels[SPU2].chcr & 1;

        for (int i = 0; i < size; i++) {
            if (write) {
                uint32_t d = bus::read32(dma->hw.bus, dma->channels[SPU2].madr);

                bus::write16(dma->hw.bus, 0x1f9005ac, d & 0xffff);
                bus::write16(dma->hw.bus, 0x1f9005ac, d >> 16);
            } else {
                uint16_t lo = spu2::read_data(dma->hw.spu2, 1);
                uint16_t hi = spu2::read_data(dma->hw.spu2, 1);

                bus::write32(dma->hw.bus, dma->channels[SPU2].madr, lo | ((uint32_t)hi << 16));
            }

            dma->channels[SPU2].madr += 4;
        }

        scheduler::Event spu2_dma_irq_event;

        spu2_dma_irq_event.callback = spu2_dma_irq_event_handler;
        spu2_dma_irq_event.cycles = dma->channels[SPU2].transfer_size * 768;
        spu2_dma_irq_event.name = "SPU2 DMA IRQ event";
        spu2_dma_irq_event.udata = dma;

        scheduler::schedule(dma->hw.sched, spu2_dma_irq_event);

        return;
    }

    uint32_t size = dma->channels[SPU2].transfer_size;
    uint32_t madr_start = dma->channels[SPU2].madr;
    uint16_t* buf = (uint16_t *)malloc(size);

    // iris_debug(dma, "CORE1 ADMA transfer size={} bytes cycles={}", size, size * 192);

    int index = 0;

    for (int i = 0; i < size; i += 4) {
        uint32_t d = bus::read32(dma->hw.bus, dma->channels[SPU2].madr);

        buf[index++] = d & 0xffff;
        buf[index++] = d >> 16;

        dma->channels[SPU2].madr += 4;
        dma->channels[SPU2].transfer_size -= 4;
    }

    spu2::adma_write(dma->hw.spu2, 1, buf, size);

    free(buf);

    // Core 1 has no bitstream-bypass double-rate mode. Track MADR at the normal
    // 48 kHz rate
    spu_adma_start_tracking(dma, SPU2, madr_start, size, 192 * 8, spu2_adma_advance_handler);
}

void handle_dev9_ata_transfer(Dma* dma) {
    // iris_debug(dma, "iop: dev9 ata madr={:08x} bcr={:08x} chcr={:08x} tadr={:08x} size={} ({:08x})", //     dma->channels[DEV9].madr,
    //     dma->channels[DEV9].bcr,
    //     dma->channels[DEV9].chcr,
    //     dma->channels[DEV9].tadr,
    //     dma->channels[DEV9].transfer_size,
    //     dma->channels[DEV9].transfer_size
    //);

    int dir = dma->channels[DEV9].chcr & 1;

    if (dir) {
        while (dma->channels[DEV9].transfer_size) {
            uint16_t d = bus::read16(dma->hw.bus, dma->channels[DEV9].madr);

            // ATA DATA register
            ata_write(dma->hw.speed->ata, 0x40, d);

            dma->channels[DEV9].madr += 2;
            dma->channels[DEV9].transfer_size -= 2;
        }
    } else {
        while (dma->channels[DEV9].transfer_size) {
            // ATA DATA register
            uint32_t d = ata_read(dma->hw.speed->ata, 0x40);

            iop::invalidate_cache_page(dma->hw.iop, dma->channels[DEV9].madr);
            bus::write16(dma->hw.bus, dma->channels[DEV9].madr, d);

            dma->channels[DEV9].madr += 2;
            dma->channels[DEV9].transfer_size -= 2;
        }
    }

    set_dicr_flag(dma, DEV9);
    check_irq(dma);

    dma->channels[DEV9].chcr &= ~0x1000000;
}

void handle_dev9_nand_transfer(Dma* dma) {
    while (dma->channels[DEV9].transfer_size) {
        uint32_t d = bus::read8(dma->hw.bus, 0x14000008);

        iop::invalidate_cache_page(dma->hw.iop, dma->channels[DEV9].madr);
        bus::write8(dma->hw.bus, dma->channels[DEV9].madr++, d);

        dma->channels[DEV9].transfer_size--;
    }

    set_dicr_flag(dma, DEV9);
    check_irq(dma);

    dma->channels[DEV9].chcr &= ~0x1000000;
}

void handle_dev9_acata_transfer(Dma* dma) {
    int dir = dma->channels[DEV9].chcr & 1;

    iris_debug(dma, "iop: dev9 acata madr={:08x} bcr={:08x} chcr={:08x} tadr={:08x} size={} ({:08x}) dir={}", dma->channels[DEV9].madr,
        dma->channels[DEV9].bcr,
        dma->channels[DEV9].chcr,
        dma->channels[DEV9].tadr,
        dma->channels[DEV9].transfer_size,
        dma->channels[DEV9].transfer_size,
        dir);

    if (dir) {
        while (dma->channels[DEV9].transfer_size) {
            uint16_t d = bus::read16(dma->hw.bus, dma->channels[DEV9].madr);

            // ATA DATA register
            s2x6::acata::write(dma->hw.s2x6_acata, 0, d);

            dma->channels[DEV9].madr += 2;
            dma->channels[DEV9].transfer_size -= 2;
        }
    } else {
        while (dma->channels[DEV9].transfer_size) {
            // ATA DATA register
            uint32_t d = s2x6::acata::read(dma->hw.s2x6_acata, 0);
    
            iop::invalidate_cache_page(dma->hw.iop, dma->channels[DEV9].madr);
            bus::write16(dma->hw.bus, dma->channels[DEV9].madr, d);

            dma->channels[DEV9].madr += 2;
            dma->channels[DEV9].transfer_size -= 2;
        }
    }

    set_dicr_flag(dma, DEV9);
    check_irq(dma);

    dma->channels[DEV9].chcr &= ~0x1000000;
}

void handle_dev9_acram_transfer(Dma* dma) {
    int dir = dma->channels[DEV9].chcr & 1;
    int bank = s2x6::acram::bank_from_dma_target(dma->dev9_dma_target);

    if (dir) {
        while (dma->channels[DEV9].transfer_size) {
            uint32_t d = bus::read32(dma->hw.bus, dma->channels[DEV9].madr);

            s2x6::acram::dma_write32(dma->hw.s2x6_acram, bank, d);

            dma->channels[DEV9].madr += 4;
            dma->channels[DEV9].transfer_size -= 4;
        }
    } else {
        while (dma->channels[DEV9].transfer_size) {
            uint32_t d = s2x6::acram::dma_read32(dma->hw.s2x6_acram, bank);

            iop::invalidate_cache_page(dma->hw.iop, dma->channels[DEV9].madr);
            bus::write32(dma->hw.bus, dma->channels[DEV9].madr, d);

            dma->channels[DEV9].madr += 4;
            dma->channels[DEV9].transfer_size -= 4;
        }
    }

    set_dicr_flag(dma, DEV9);
    check_irq(dma);

    dma->channels[DEV9].chcr &= ~0x1000000;
}

void handle_dev9_smap_transfer(Dma* dma) {
    int dir = dma->channels[DEV9].chcr & 1;

    if (dir) {
        // Memory -> SMAP TX FIFO. The frame is discarded (no host backend).
        while (dma->channels[DEV9].transfer_size) {
            uint32_t d = bus::read32(dma->hw.bus, dma->channels[DEV9].madr);

            speed::smap::fifo_write(dma->hw.speed->smap, d);

            dma->channels[DEV9].madr += 4;
            dma->channels[DEV9].transfer_size -= 4;
        }
    } else {
        // SMAP RX FIFO -> memory. Nothing is ever queued, so this drains zeroes.
        while (dma->channels[DEV9].transfer_size) {
            uint32_t d = speed::smap::fifo_read(dma->hw.speed->smap);

            iop::invalidate_cache_page(dma->hw.iop, dma->channels[DEV9].madr);
            bus::write32(dma->hw.bus, dma->channels[DEV9].madr, d);

            dma->channels[DEV9].madr += 4;
            dma->channels[DEV9].transfer_size -= 4;
        }
    }

    speed::smap::dma_complete(dma->hw.speed->smap);

    set_dicr_flag(dma, DEV9);
    check_irq(dma);

    dma->channels[DEV9].chcr &= ~0x1000000;
}

void handle_dev9_transfer(Dma* dma) {
    // Note: DEV9 DMA serves different purposes based on the system.

    // On retail hardware, DEV9 DMA is used to transfer data in and out
    // of the HDD. On Namco System 147/148 arcade hardware, DEV9 DMA
    // is used to transfer data to and from the Samsung NAND flash
    // storage chip. On Namco System 246/256 arcade hardware, DEV9 DMA is
    // used to transfer data to and from the ACATA device, which is
    // either an ATA HDD or ATAPI DVD drive.
    // SMAP shares the DEV9 channel with ATA. the active FIFO DMAEN bit
    // marks the transfer as belonging to the Ethernet block.

    if (speed::smap::dma_pending(dma->hw.speed->smap)) {
        handle_dev9_smap_transfer(dma);

        return;
    }

    if (dma->dev9_mode == DEV9_MODE_RETAIL) {
        handle_dev9_ata_transfer(dma);
    } else if (dma->dev9_mode == DEV9_MODE_ACATA) {
        if (!s2x6::acata::dma_pending(dma->hw.s2x6_acata) && s2x6::acram::is_dma_target(dma->dev9_dma_target)) {
            handle_dev9_acram_transfer(dma);
        } else {
            handle_dev9_acata_transfer(dma);
        }
    } else {
        handle_dev9_nand_transfer(dma);
    }
}
void handle_sif0_transfer(Dma* dma) {
    // if (!sif::fifo_is_empty(dma->hw.sif->sif0)) {
    //     iris_debug(dma, "iopdma: SIF FIFO not empty");

    //     exit(1);
    // }

    // sif::fifo_reset(dma->hw.sif->sif0);

    dma->channels[SIF0].eot = 0;

    do {
        dma_fetch_tag(dma, &dma->channels[SIF0]);

        // iris_debug(dma, "iop: SIF0 tag at {:08x} extra={} addr={:08x} size={:08x} irq={} eot={}", //     dma->channels[SIF0].tadr, dma->channels[SIF0].extra, dma->channels[SIF0].addr, dma->channels[SIF0].size, dma->channels[SIF0].irq, dma->channels[SIF0].eot
        //);

        uint128_t q;

        if (dma->channels[SIF0].extra) {
            q.u32[0] = bus::read32(dma->hw.bus, dma->channels[SIF0].tadr + 8);
            q.u32[1] = bus::read32(dma->hw.bus, dma->channels[SIF0].tadr + 12);
            q.u32[2] = bus::read32(dma->hw.bus, dma->channels[SIF0].tadr + 0);
            q.u32[3] = bus::read32(dma->hw.bus, dma->channels[SIF0].tadr + 4);

            sif::fifo_write(dma->hw.sif->sif0, q);
        }

        while (dma->channels[SIF0].size) {
            q.u32[0] = bus::read32(dma->hw.bus, dma->channels[SIF0].addr);
            q.u32[1] = bus::read32(dma->hw.bus, dma->channels[SIF0].addr + 4);
            q.u32[2] = bus::read32(dma->hw.bus, dma->channels[SIF0].addr + 8);
            q.u32[3] = bus::read32(dma->hw.bus, dma->channels[SIF0].addr + 12);

            sif::fifo_write(dma->hw.sif->sif0, q);

            dma->channels[SIF0].addr += 16;
            dma->channels[SIF0].size -= 4;
        }

        dma->channels[SIF0].tadr += (dma->channels[SIF0].extra ? 4 : 2) * 4;
    } while (!dma->channels[SIF0].eot);

    set_dicr_flag(dma, SIF0);
    check_irq(dma);

    ee::dmac::handle_sif0_transfer(dma->hw.ee_dma);

    dma->channels[SIF0].tadr += dma->channels[SIF0].extra ? 4 : 2;
    dma->channels[SIF0].chcr &= ~0x1000000;
}


void handle_sif1_transfer(Dma* dma) {
    int madr_increment = ((dma->channels[SIF1].chcr >> 1) & 1) ? -4 : 4;

    // No data in the SIF FIFO yet
    if (sif::fifo_is_empty(dma->hw.sif->sif1))
        return;

    // Data ready but channel isn't ready yet, keep waiting
    if (!(dma->channels[SIF1].chcr & 0x1000000)) {
        // iris_debug(dma, "iop: EE sent SIF1 but channel isn't ready");

        return;
    }

    // Data ready and channel is started, do transfer
    int eot;

    do {
        uint128_t q = sif::fifo_read(dma->hw.sif->sif1);

        uint64_t tag = q.u64[0];

        uint32_t addr = tag & 0x7fffff;
        int size = (tag >> 32) & 0xffffff;
        int irq = !!(tag & 0x40000000);
        eot = !!(tag & 0x80000000);

        // iris_debug(dma, "iop: SIF1 tag read_index=%", dma->hw.sif->sif1.read_index);

        char buf[128];

        if (iop::rpc::decode_packet(dma->logger, dma->logger_id, dma->hw.intc->hw.iop, buf, (uint32_t*)(((uint8_t*)dma->hw.sif->sif1.data.data()) + (dma->hw.sif->sif1.read_index * 16)))) {
            // iris_debug(dma, "{}", buf);
        }

        while (size) {
            uint128_t q = sif::fifo_read(dma->hw.sif->sif1);

            for (int i = 0; i < 4; i++) {
                iop::invalidate_cache_page(dma->hw.iop, addr);
                bus::write32(dma->hw.bus, addr, q.u32[i]);

                addr += madr_increment;
                --size;
            }
        }

        // Send interrupt on tag IRQ (regardless of channel IRQ enable)
        if ((dma->dicr2 & 0x400) && irq) {
            iop::intc::irq(dma->hw.intc, iop::intc::DMA);
        }

        if (sif::fifo_is_empty(dma->hw.sif->sif1))
            break;
    } while (!eot);

    set_dicr_flag(dma, SIF1);
    check_irq(dma);

    // sif::fifo_reset(dma->hw.sif->sif1);

    dma->channels[SIF1].chcr &= ~0x1000000;
}
void handle_sio2_in_transfer(Dma* dma) {
    uint32_t size = (dma->channels[SIO2_IN].bcr & 0xffff) * (dma->channels[SIO2_IN].bcr >> 16);

    // iris_debug(dma, "SIO2 in transfer size={}", size);

    sio2::dma_reset(dma->hw.sio2);

    for (int i = 0; i < size; i++) {
        uint32_t w = bus::read32(dma->hw.bus, dma->channels[SIO2_IN].madr);

        bus::write8(dma->hw.bus, 0x1F808260, (w >> 0) & 0xff);
        bus::write8(dma->hw.bus, 0x1F808260, (w >> 8) & 0xff);
        bus::write8(dma->hw.bus, 0x1F808260, (w >> 16) & 0xff);
        bus::write8(dma->hw.bus, 0x1F808260, (w >> 24) & 0xff);

        // iris_debug(dma, "{:02x} {:02x} {:02x} {:02x}", //     (w >> 0) & 0xff,
        //     (w >> 8) & 0xff,
        //     (w >> 16) & 0xff,
        //     (w >> 24) & 0xff
        //);

        dma->channels[SIO2_IN].madr += 4;
    }

    set_dicr_flag(dma, SIO2_IN);
    check_irq(dma);

    dma->channels[SIO2_IN].chcr &= ~0x1000000;
}

void dma_handle_sio2_out_irq_event(void* udata, int overshoot) {
    Dma* dma = (Dma*)udata;

    set_dicr_flag(dma, SIO2_OUT);
    check_irq(dma);

    dma->channels[SIO2_OUT].chcr &= ~0x1000000;
}
void handle_sio2_out_transfer(Dma* dma) {
    if ((dma->channels[SIO2_OUT].chcr & 0x1000000) == 0) {
        iris_fatal_error(dma, "SIO2_out not requested");

        return;
    }
    
    if (queue::is_empty(dma->hw.sio2->out)) {
        // iris_debug(dma, "SIO2_out waiting size={} bcr={:08x} madr={:08x}", queue::size(dma->hw.sio2->out), dma->channels[SIO2_OUT].bcr, dma->channels[SIO2_OUT].madr);
        
        return;
    }

    iris_fatal_error(dma, "WHAT? Doing SIO2 out transfer size={} bcr={:08x} madr={:08x}", queue::size(dma->hw.sio2->out), dma->channels[SIO2_OUT].bcr, dma->channels[SIO2_OUT].madr);

    for (int b = 0; b < (dma->channels[SIO2_OUT].bcr >> 16); b++) {
        for (int i = 0; i < (dma->channels[SIO2_OUT].bcr & 0xffff); i++) {
            for (int j = 0; j < 4; j++) {
                uint8_t b = bus::read8(dma->hw.bus, 0x1F808264);

                iop::invalidate_cache_page(dma->hw.iop, dma->channels[SIO2_OUT].madr);
                bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, b);    
            } 

            // bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, queue::pop(dma->hw.sio2->out));
            // bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, queue::pop(dma->hw.sio2->out));
            // bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, queue::pop(dma->hw.sio2->out));
            // bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, queue::pop(dma->hw.sio2->out));
        }
    }
    // for (int b = 0; b < (dma->channels[SIO2_OUT].bcr >> 16); b++) {
    //     for (int i = 0; i < (dma->channels[SIO2_OUT].bcr & 0xffff); i++) {
    //         bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, 0);
    //         bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, 0);
    //         bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, 0);
    //         bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, 0);
    //     }
    // }

    // scheduler::Event event;

    // event.callback = dma_handle_sio2_out_irq_event;
    // event.cycles = 10000;
    // event.name = "SIO2 out DMA IRQ";
    // event.udata = dma;

    // scheduler::schedule(dma->hw.sched, event);

    set_dicr_flag(dma, SIO2_OUT);
    check_irq(dma);

    dma->channels[SIO2_OUT].chcr &= ~0x1000000;
}

void end_sio2_out_transfer(Dma* dma) {
    if ((dma->channels[SIO2_OUT].chcr & 0x1000000) == 0) {
        // iris_debug(dma, "SIO2_out not requested");

        return;
    }
    
    if (queue::is_empty(dma->hw.sio2->out)) {
        // iris_debug(dma, "SIO2 command put nothing in the fifo, ending transfer");

        set_dicr_flag(dma, SIO2_OUT);
        check_irq(dma);

        dma->channels[SIO2_OUT].chcr &= ~0x1000000;
        // iris_debug(dma, "SIO2_out waiting size={} bcr={:08x} madr={:08x}", queue::size(dma->hw.sio2->out), dma->channels[SIO2_OUT].bcr, dma->channels[SIO2_OUT].madr);
        
        return;
    }

    // iris_debug(dma, "Doing SIO2 out transfer size={} bcr={:08x} madr={:08x}", queue::size(dma->hw.sio2->out), dma->channels[SIO2_OUT].bcr, dma->channels[SIO2_OUT].madr);

    for (int b = 0; b < (dma->channels[SIO2_OUT].bcr >> 16); b++) {
        for (int i = 0; i < (dma->channels[SIO2_OUT].bcr & 0xffff); i++) {
            for (int j = 0; j < 4; j++) {
                uint8_t b = bus::read8(dma->hw.bus, 0x1F808264);

                iop::invalidate_cache_page(dma->hw.iop, dma->channels[SIO2_OUT].madr);
                bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, b);    
            } 

            // bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, queue::pop(dma->hw.sio2->out));
            // bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, queue::pop(dma->hw.sio2->out));
            // bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, queue::pop(dma->hw.sio2->out));
            // bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, queue::pop(dma->hw.sio2->out));
        }
    }
    // for (int b = 0; b < (dma->channels[SIO2_OUT].bcr >> 16); b++) {
    //     for (int i = 0; i < (dma->channels[SIO2_OUT].bcr & 0xffff); i++) {
    //         bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, 0);
    //         bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, 0);
    //         bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, 0);
    //         bus::write8(dma->hw.bus, dma->channels[SIO2_OUT].madr++, 0);
    //     }
    // }

    // scheduler::Event event;

    // event.callback = dma_handle_sio2_out_irq_event;
    // event.cycles = 10000;
    // event.name = "SIO2 out DMA IRQ";
    // event.udata = dma;

    // scheduler::schedule(dma->hw.sched, event);

    set_dicr_flag(dma, SIO2_OUT);
    check_irq(dma);

    dma->channels[SIO2_OUT].chcr &= ~0x1000000;
}

uint64_t read32(Dma* dma, uint32_t addr) {
    Channel* c = get_channel(dma, addr);

    if (c) {
        switch (addr & 0xf) {
            case 0x0: return c->madr;
            case 0x4: return c->bcr;
            case 0x8: return c->chcr;
            case 0xc: return c->tadr;
        }

        const char* name = get_channel_name(addr);

        iris_debug(dma, "iop_dma: Unknown {} register read {:08x}", name, addr);

        return 0;
    }

    switch (addr) {
        case 0x1f801410: return dma->dev9_dma_target;
        case 0x1f8010f0: return dma->dpcr;
        case 0x1f801570: return dma->dpcr2;
        case 0x1f8010f4: return dma->dicr;
        case 0x1f801574: return dma->dicr2;
        case 0x1f801578: return dma->dmacen;
        case 0x1f80157c: return dma->dmacinten;
    }

    iris_debug(dma, "iop_dma: Unknown DMA register read {:08x}", addr);

    return 0;
}

void write32(Dma* dma, uint32_t addr, uint64_t data) {
    Channel* c = get_channel(dma, addr);

    if (c) {
        switch (addr & 0xf) {
            case 0x0: c->madr = data; return;
            case 0x4: c->bcr = data; return;
            case 0x8: {
                if ((c->chcr & 0x1000000) && (data & 0x1000000)) {
                    iris_debug(dma, "iop: SPU2 channel already started, ignoring");

                    return;
                }

                c->chcr = data;

                if (!(c->chcr & 0x1000000)) {
                    return;
                }

                c->transfer_size = (c->bcr >> 16) * ((c->bcr & 0xffff) * 4);

                // if ((addr & 0xff0) == 0x0b0) {
                //     FILE* file = fopen("cdvd.dump", "a");
                //     fprintf(file, "iop: Starting %s channel with chcr=%08x madr=%08x bcr=%08x tadr=%08x\n", get_channel_name(addr), data, c->madr, c->bcr, c->tadr);
                //     fclose(file);
                // }

                // if ((addr & 0xff0) == 0x0b0)
                // iris_debug(dma, "iop: Starting {} channel with chcr={:08x} madr={:08x} bcr={:08x} tadr={:08x}", get_channel_name(addr), data, c->madr, c->bcr, c->tadr);

                // iris_debug(dma, "iop: Starting {} channel with chcr={:08x} madr={:08x} bcr={:08x} tadr={:08x}", get_channel_name(addr), data, c->madr, c->bcr, c->tadr);

                // Check negative MADR increments
                // if ((c->chcr & 2) == 0) {
                //     iris_debug(dma, "iop: Negative MADR increments not supported on IOP DMA channels");

                //     // exit(1);
                // }

                // // Check for burst transfers
                // if (((c->chcr >> 9) & 3) != 0) {
                //     iris_debug(dma, "iop: Burst transfers not supported on IOP DMA channels");

                //     // exit(1);
                // }

                // // Check for 0-sized blocks
                // if ((c->bcr & 0xffff) == 0) {
                //     iris_debug(dma, "iop: 0-sized blocks not supported on IOP DMA channels");

                //     exit(1);
                // }

                switch (addr & 0xff0) {
                    case 0x080: handle_mdec_in_transfer(dma); break;
                    case 0x090: handle_mdec_out_transfer(dma); break;
                    case 0x0a0: handle_sif2_transfer(dma); break;
                    case 0x0b0: handle_cdvd_transfer(dma); break;
                    case 0x0c0: handle_spu1_transfer(dma); break;
                    case 0x0d0: handle_pio_transfer(dma); break;
                    case 0x0e0: handle_otc_transfer(dma); break;
                    case 0x500: handle_spu2_transfer(dma); break;
                    case 0x510: handle_dev9_transfer(dma); break;
                    case 0x520: handle_sif0_transfer(dma); break;
                    case 0x530: handle_sif1_transfer(dma); break;
                    case 0x540: handle_sio2_in_transfer(dma); break;
                    case 0x550: handle_sio2_out_transfer(dma); break;
                }

                return;
            }
            case 0xc: c->tadr = data; return;
        }

        const char* name = get_channel_name(addr);

        iris_debug(dma, "iop_dma: Unknown {} register write {:08x} {:08x}", name, addr, data);

        return;
    }

    switch (addr) {
        case 0x1f801410: dma->dev9_dma_target = data; return;
        case 0x1f8010f0: dma->dpcr = data; return;
        case 0x1f801570: dma->dpcr2 = data; return;
        case 0x1f8010f4: set_dicr(dma, data); check_irq(dma); return;
        case 0x1f801574: set_dicr2(dma, data); check_irq(dma); return;
        case 0x1f801578: dma->dmacen = data; return;
        case 0x1f80157c: dma->dmacinten = data; check_irq(dma); return;
    }

    iris_debug(dma, "iop_dma: Unknown DMA register write {:08x} {:08x}", addr, data);
}

uint64_t read16(Dma* dma, uint32_t addr) {
    Channel* c = get_channel(dma, addr);

    if (c) {
        switch (addr & 0xf) {
            case 0x0: return c->madr;
            case 0x4: return c->bcr;
            case 0x8: return c->chcr;
            case 0xc: return c->tadr;
        }

        const char* name = get_channel_name(addr);

        iris_debug(dma, "iop_dma: Unknown {} register read {:08x}", name, addr);

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

    iris_debug(dma, "iop_dma: Unknown DMA register read {:08x}", addr);

    return 0;
}

void write16(Dma* dma, uint32_t addr, uint64_t data) {
    Channel* c = get_channel(dma, addr);

    if (c) {
        switch (addr & 0xf) {
            case 0x4: if ((addr & 0xff0) == 0x0b0) iris_debug(dma, "iop: bcrlo write16 {:08x}", data); c->bcr &= ~0xffff; c->bcr |= data; return;
            case 0x6: if ((addr & 0xff0) == 0x0b0) iris_debug(dma, "iop: bcrhi write16 {:08x}", data); c->bcr &= 0xffff; c->bcr |= data << 16; return;
        }

        const char* name = get_channel_name(addr);

        iris_fatal_error(dma, "iop_dma: Unknown 16-bit {} register write {:08x} {:08x}", name, addr, data);

        return;
    }

    iris_fatal_error(dma, "iop_dma: Unknown DMA register write {:08x} {:08x}", addr, data);
}

void end_spu1_transfer(Dma* dma) {
    set_dicr_flag(dma, SPU1);
    check_irq(dma);

    dma->channels[SPU1].chcr &= ~0x1000000;
}

void end_spu2_transfer(Dma* dma) {
    set_dicr_flag(dma, SPU2);
    check_irq(dma);

    dma->channels[SPU2].chcr &= ~0x1000000;
}

void handle_spu1_adma(Dma* dma) {
    if (!dma->channels[SPU1].transfer_size) {
        // If we have no more data to transfer, then we can end the transfer
        // and trigger an IRQ event
        set_dicr_flag(dma, SPU1);
        check_irq(dma);

        dma->channels[SPU1].chcr &= ~0x1000000;

        // iris_debug(dma, "spu2 core0: transfer done (chcr={:08x})", dma->channels[SPU1].chcr);

        return;
    }

    // iris_debug(dma, "spu2 core0: transfer update ({} bytes pending)", dma->channels[SPU1].transfer_size);

    // Transfer data as long as the spu1 isn't streaming ADMA
    // samples and we still have data to transfer
    while (dma->channels[SPU1].transfer_size && !spu2::is_adma_active(dma->hw.spu2, 0)) {
        uint32_t d = bus::read32(dma->hw.bus, dma->channels[SPU1].madr);

        bus::write16(dma->hw.bus, 0x1f9001ac, d & 0xffff);
        bus::write16(dma->hw.bus, 0x1f9001ac, d >> 16);

        dma->channels[SPU1].madr += 4;
        dma->channels[SPU1].transfer_size -= 4;
    }
}
void handle_spu2_adma(Dma* dma) {
    if (!dma->channels[SPU2].transfer_size) {
        // If we have no more data to transfer, then we can end the transfer
        // and trigger an IRQ event
        set_dicr_flag(dma, SPU2);
        check_irq(dma);

        dma->channels[SPU2].chcr &= ~0x1000000;

        // iris_debug(dma, "spu2 core1: transfer done (chcr={:08x})", dma->channels[SPU2].chcr);

        return;
    }
    
    // iris_debug(dma, "spu2 core1: transfer update ({} bytes pending)", dma->channels[SPU2].transfer_size);

    // Transfer data as long as the SPU2 isn't streaming ADMA
    // samples and we still have data to transfer
    while (dma->channels[SPU2].transfer_size && !spu2::is_adma_active(dma->hw.spu2, 1)) {
        uint32_t d = bus::read32(dma->hw.bus, dma->channels[SPU2].madr);

        bus::write16(dma->hw.bus, 0x1f9005ac, d & 0xffff);
        bus::write16(dma->hw.bus, 0x1f9005ac, d >> 16);

        dma->channels[SPU2].madr += 4;
        dma->channels[SPU2].transfer_size -= 4;
    }
}

void set_dev9_mode(Dma* dma, int mode) {
    dma->dev9_mode = mode;
}

}
