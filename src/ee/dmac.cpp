#include <new>

#include "iop/dma.hpp"
#include "dmac.hpp"
#include "gif.hpp"
#include "vif.hpp"
#include "../gs/gs.hpp"
#include "bus.hpp"
#include "profile_counters.hpp"
#include <cassert>
#include <cstring>
#include <cstdlib>

namespace iris::ee::dmac {

static inline uint128_t read_qword(Dmac* dmac, uint32_t addr) {
    int spr = addr & 0x80000000;

    if (!spr)
        return ee::bus::read128(dmac->hw.bus, addr & 0xfffffff0);

    return ram::read128(dmac->hw.spr, addr & 0x3ff0);
}

static inline uint32_t read_word(Dmac* dmac, uint32_t addr) {
    int spr = addr & 0x80000000;

    if (!spr)
        return ee::bus::read32(dmac->hw.bus, addr & 0xffffffff);

    return ram::read32(dmac->hw.spr, addr & 0x3fff);
}

static inline void write_qword(Dmac* dmac, uint32_t addr, int mem, uint128_t value) {
    int spr = mem || (addr & 0x80000000);

    if (!spr) {
        ee::invalidate_block(dmac->hw.ee, addr & 0xfffffff0);

        ee::bus::write128(dmac->hw.bus, addr & 0xfffffff0, value);

        return;
    }

    ram::write128(dmac->hw.spr, addr & 0x3ff0, value);
}

Dmac* create(logger::Logger* logger, scheduler::Scheduler* sched, ee::bus::Bus* bus, sif::Sif* sif) {
    Dmac* dmac = new Dmac();

    dmac->logger = logger;
    dmac->logger_id = logger::register_source(logger, "ee_dmac");

    dmac->hw.sched = sched;
    dmac->hw.bus = bus;
    dmac->hw.sif = sif;

    reset(dmac);

    return dmac;
}

void connect(Dmac* dmac, gif::Gif* gif, vif::Vif* vif0, vif::Vif* vif1, ipu::Ipu* ipu, iop::dma::Dma* iop_dma, ee::Ee* ee) {
    dmac->hw.gif = gif;
    dmac->hw.vif0 = vif0;
    dmac->hw.vif1 = vif1;
    dmac->hw.ipu = ipu;
    dmac->hw.iop_dma = iop_dma;
    dmac->hw.ee = ee;

    dmac->hw.spr = ee::get_spr(ee);
}

void reset(Dmac* dmac) {
    auto hw = dmac->hw;

    logger::Logger* logger = dmac->logger;
    size_t logger_id = dmac->logger_id;

    new (dmac) Dmac();

    dmac->logger = logger;
    dmac->logger_id = logger_id;

    dmac->hw = hw;

    // v2+ BIOSes need this value on boot (smh...)
    dmac->enable = 0x1201;
}

void destroy(Dmac* dmac) {
    delete dmac;
}

static inline Channel* get_channel(Dmac* dmac, uint32_t addr) {
    switch (addr & 0xff00) {
        case 0x8000: return &dmac->channels[VIF0];
        case 0x9000: return &dmac->channels[VIF1];
        case 0xA000: return &dmac->channels[GIF];
        case 0xB000: return &dmac->channels[IPU_FROM];
        case 0xB400: return &dmac->channels[IPU_TO];
        case 0xC000: return &dmac->channels[SIF0];
        case 0xC400: return &dmac->channels[SIF1];
        case 0xC800: return &dmac->channels[SIF2];
        case 0xD000: return &dmac->channels[SPR_FROM];
        case 0xD400: return &dmac->channels[SPR_TO];
    }

    return NULL;
}

static inline const char* get_channel_name(Dmac* dmac, uint32_t addr) {
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

static inline int channel_is_done(Channel* ch) {
    return ch->tag.end || (ch->tag.irq && (ch->chcr & 0x80));
}

uint64_t read32(Dmac* dmac, uint32_t addr) {
    Channel* c = get_channel(dmac, addr);

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

        // iris_debug(dmac, "Unknown channel register {:02x}", addr & 0xff);

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

static inline void process_source_tag(Dmac* dmac, Channel* c, uint128_t tag) {
    // Set CHCR TAG bytes
    c->chcr &= 0xffff;
    c->chcr |= tag.u32[0] & 0xffff0000;

    c->tag.qwc = tag_qwc(tag);
    c->tag.pct = tag_pct(tag);
    c->tag.id = tag_id(tag);
    c->tag.irq = tag_irq(tag);
    c->tag.addr = tag_addr(tag);
    c->tag.data = tag_data(tag);

    // if (dmac->mfifo_drain)
    // iris_debug(dmac, "ee: dmac tag {:016x} {:016x} qwc={:08x} id={} irq={} addr={:08x} mem={} data={:016x}", //     tag.u64[1], tag.u64[0],
    //     c->tag.qwc,
    //     c->tag.id,
    //     c->tag.irq,
    //     c->tag.addr,
    //     c->tag.mem,
    //     c->tag.data
    //);

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
            c->tadr = c->madr + c->qwc * 16;
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

static inline void process_dest_tag(Dmac* dmac, Channel* c, uint128_t tag) {
    // Set CHCR TAG bytes
    c->chcr &= 0xffff;
    c->chcr |= tag.u32[0] & 0xffff0000;

    c->tag.qwc = tag_qwc(tag);
    c->tag.pct = tag_pct(tag);
    c->tag.id = tag_id(tag);
    c->tag.irq = tag_irq(tag);
    c->tag.addr = tag_addr(tag);
    c->tag.data = tag_data(tag);

    c->qwc = c->tag.qwc;

    c->tag.end = dmac->channels[SIF0].tag.irq && (dmac->channels[SIF0].chcr & 0x80);

    switch (c->tag.id) {
        case 7:
            c->tag.end = 1;
        case 0:
        case 1:
            c->madr = c->tag.addr;
    }
}

static inline void test_cpcond0(Dmac* dmac) {
    ee::set_cpcond0(dmac->hw.ee, (((~dmac->pcr) | dmac->stat) & 0x3ff) == 0x3ff);
}

static inline void test_irq(Dmac* dmac) {
    test_cpcond0(dmac);

    int meis = ((dmac->stat >> 14) & 1) & ((dmac->stat >> 30) & 1);
    int beis = (dmac->stat >> 15) & 1;
    int chirq = (dmac->stat & 0x3ff) & ((dmac->stat >> 16) & 0x3ff);

    ee::set_int1(dmac->hw.ee, chirq || meis || beis);
}

static inline void set_irq(Dmac* dmac, int ch) {
    dmac->stat |= 1 << ch;

    // iris_debug(dmac, "channel={} flag={:08x} mask={:08x} irq={:08x}", ch, dmac->stat & 0x3ff, (dmac->stat >> 16) & 0x3ff, (dmac->stat & 0x3ff) & ((dmac->stat >> 16) & 0x3ff));

    test_irq(dmac);
}

static inline void end_transfer(Dmac* dmac, int id) {
    set_irq(dmac, id);

    dmac->channels[id].tag.end = 0;
    dmac->channels[id].tag.irq = 0;
    dmac->channels[id].chcr &= ~0x100;
    dmac->channels[id].qwc = 0;
}

int transfer_vif0_word(Dmac* dmac) {
    if ((dmac->channels[VIF0].chcr & 0x100) == 0) {
        iris_debug(dmac, "vif0 channel not started");

        return 0;
    }

    if (!vif::get_dreq(dmac->hw.bus->vif0)) {
        // iris_debug(dmac, "vif0 dreq cleared");

        return 0;
    }

    if (dmac->channels[VIF0].qwc) {
        uint32_t w = read_word(dmac, dmac->channels[VIF0].madr);

        vif::fifo_write(dmac->hw.bus->vif0, w);

        dmac->channels[VIF0].madr += 4;
        dmac->channels[VIF0].index++;

        if (dmac->channels[VIF0].index == 4) {
            dmac->channels[VIF0].index = 0;
            dmac->channels[VIF0].qwc--;
        }

        return 1;
    }

    if (channel_is_done(&dmac->channels[VIF0])) {
        end_transfer(dmac, VIF0);

        // iris_debug(dmac, "vif0 transfer done");

        return 0;
    }

    uint128_t tag = read_qword(dmac, dmac->channels[VIF0].tadr);

    process_source_tag(dmac, &dmac->channels[VIF0], tag);

    // iris_debug(dmac, "vif0 tag tag.qwc={:08x} qwc={:08x} id={} irq={} addr={:08x} data={:016x} end={} tte={}", //     dmac->channels[VIF0].tag.qwc,
    //     dmac->channels[VIF0].qwc,
    //     dmac->channels[VIF0].tag.id,
    //     dmac->channels[VIF0].tag.irq,
    //     dmac->channels[VIF0].tag.addr,
    //     dmac->channels[VIF0].tag.data,
    //     dmac->channels[VIF0].tag.end,
    //     (dmac->channels[VIF0].chcr >> 7) & 1
    //);

    if ((dmac->channels[VIF0].chcr >> 6) & 1) {
        vif::fifo_write(dmac->hw.bus->vif0, dmac->channels[VIF0].tag.data & 0xffffffff);

        if (!vif::get_dreq(dmac->hw.bus->vif0)) {
            // iris_debug(dmac, "vif0 dreq cleared while writing tag data");

            // exit(1);
        }

        vif::fifo_write(dmac->hw.bus->vif0, dmac->channels[VIF0].tag.data >> 32);

        if (!vif::get_dreq(dmac->hw.bus->vif0)) {
            // iris_debug(dmac, "vif0 dreq cleared while writing tag data");

            // exit(1);
        }
    }

    return 1;
}
void handle_vif0_transfer(Dmac* dmac) {
    if ((dmac->channels[VIF0].chcr & 0x100) == 0)
        return;

    // iris_debug(dmac, "VIF0 DMA dir={} mode={} tte={} tie={} qwc={} madr={:08x} tadr={:08x} end={} dreq={}", //     dmac->channels[VIF0].chcr & 1,
    //     (dmac->channels[VIF0].chcr >> 2) & 3,
    //     (dmac->channels[VIF0].chcr >> 6) & 1,
    //     (dmac->channels[VIF0].chcr >> 7) & 1,
    //     dmac->channels[VIF0].qwc,
    //     dmac->channels[VIF0].madr,
    //     dmac->channels[VIF0].tadr,
    //     dmac->channels[VIF0].tag.end,
    //     vif::get_dreq(dmac->hw.bus->vif0)
    //);

    int tte = (dmac->channels[VIF0].chcr >> 6) & 1;
    int mode = (dmac->channels[VIF0].chcr >> 2) & 3;

    if (mode == 3)
        mode = 1;

    while (transfer_vif0_word(dmac)) {
        // Transfer words until we run out of data or DREQ is cleared
    }
}

static inline uint32_t mfifo_wrap(Dmac* dmac, uint32_t addr) {
    return dmac->rbor | (addr & dmac->rbsr);
}

static inline uint32_t transfer_vif1_qwords(Dmac* dmac);

void mfifo_handle_ref_tag(Dmac* dmac) {
    Channel* c = dmac->mfifo_drain;

    while (c->qwc) {
        if (c == &dmac->channels[VIF1] && transfer_vif1_qwords(dmac)) {
            continue;
        }

        uint128_t q = read_qword(dmac, c->madr);

        if (c == &dmac->channels[VIF1]) {
            // VIF1 FIFO
            vif::fifo_write(dmac->hw.bus->vif1, q.u32[0]);
            vif::fifo_write(dmac->hw.bus->vif1, q.u32[1]);
            vif::fifo_write(dmac->hw.bus->vif1, q.u32[2]);
            vif::fifo_write(dmac->hw.bus->vif1, q.u32[3]);
        } else {
            // GIF FIFO
            gif::fifo_write(dmac->hw.bus->gif, q, gif::PATH3);
        }

        c->madr += 16;
        c->qwc--;
    }

    if (channel_is_done(c)) {
        // iris_debug(dmac, "mfifo channel done end={} tte-irq={}", c->tag.end, c->tag.irq && (c->chcr & 0x80));
        set_irq(dmac, c == &dmac->channels[VIF1] ? VIF1 : GIF);

        c->chcr &= ~0x100;
        c->qwc = 0;

        return;
    }

    if (c->tag.id == 1) {
        c->tadr = dmac->rbor | (c->madr & dmac->rbsr);
    }
}

void mfifo_write_qword(Dmac* dmac, uint128_t q) {
    Channel* c = dmac->mfifo_drain;

    if (c->qwc) {
        uint128_t q = read_qword(dmac, c->madr);

        if (c == &dmac->channels[VIF1]) {
            // VIF1 FIFO
            vif::fifo_write(dmac->hw.vif1, q.u32[0]);
            vif::fifo_write(dmac->hw.vif1, q.u32[1]);
            vif::fifo_write(dmac->hw.vif1, q.u32[2]);
            vif::fifo_write(dmac->hw.vif1, q.u32[3]);
        } else {
            // GIF FIFO
            gif::fifo_write(dmac->hw.bus->gif, q, gif::PATH3);
        }

        c->madr += 16;
        c->qwc--;

        // iris_debug(dmac, "mfifo channel qwc={}", c->qwc);

        if (c->qwc == 0) {
            if (channel_is_done(c)) {
                // iris_debug(dmac, "mfifo channel done end={} tte-irq={}", c->tag.end, c->tag.irq && (c->chcr & 0x80));
                set_irq(dmac, c == &dmac->channels[VIF1] ? VIF1 : GIF);

                c->chcr &= ~0x100;
                c->qwc = 0;

                return;
            }

            if (c->tag.id == 1) {
                c->tadr = dmac->rbor | (c->madr & dmac->rbsr);
            }
        }

        return;
    }

    uint128_t tag = read_qword(dmac, c->tadr);

    process_source_tag(dmac, c, tag);

    if ((c->chcr >> 6) & 1) {
        vif::fifo_write(dmac->hw.vif1, c->tag.data & 0xffffffff);
        vif::fifo_write(dmac->hw.vif1, c->tag.data >> 32);
    }

    c->tadr = dmac->rbor | (c->tadr & dmac->rbsr);

    // iris_debug(dmac, "tadr={:08x} madr={:08x} qwc={} tagid={} end={}", c->tadr, c->madr, c->qwc, c->tag.id, c->tag.end);

    switch (c->tag.id) {
        case 1:
        case 2:
        case 5:
        case 6:
        case 7: {
            c->madr = dmac->rbor | (c->madr & dmac->rbsr);
        } break;

        default: {
            mfifo_handle_ref_tag(dmac);

            if (c->tadr == dmac->channels[SPR_FROM].madr) {
                // iris_debug(dmac, "MFIFO empty");

                set_irq(dmac, MEIS);
            }
        } return;
    }

    if (c->qwc == 0) {
        if (channel_is_done(c)) {
            // iris_debug(dmac, "mfifo channel done end={} tte-irq={}", c->tag.end, c->tag.irq && (c->chcr & 0x80));
            set_irq(dmac, c == &dmac->channels[VIF1] ? VIF1 : GIF);

            c->chcr &= ~0x100;
            c->qwc = 0;

            return;
        }
    }

    if (c->tadr == dmac->channels[SPR_FROM].madr) {
        // iris_debug(dmac, "MFIFO empty");

        set_irq(dmac, MEIS);
    }
}

static inline bool vif1_reading(Dmac* dmac) {
    Channel* c = &dmac->channels[VIF1];

    return (c->chcr & 0x100) && (c->chcr & 1) == 0;
}

static void finish_vif1_read(void* udata, int overshoot) {
    Dmac* dmac = (Dmac*)udata;

    dmac->vif1_read_pending = false;

    if (!vif1_reading(dmac)) {
        return;
    }

    end_transfer(dmac, VIF1);
}

enum DmaTarget {
    TARGET_RAM,
    TARGET_SPR,
    TARGET_SINK,
    TARGET_ERROR
};

static DmaTarget dma_write_target(Dmac* dmac, uint32_t addr) {
    if (addr & 0x80000000) {
        return TARGET_SPR;
    }

    uint32_t phys = addr & 0x1ffffff0;

    if (phys < dmac->hw.bus->ee_ram->size) {
        return TARGET_RAM;
    }

    if (phys < 0x10000000) {
        return TARGET_SINK;
    }

    if (phys < 0x10004000) {
        return TARGET_SPR;
    }

    return TARGET_ERROR;
}

static void dma_write_qword(Dmac* dmac, uint32_t addr, uint128_t value) {
    switch (dma_write_target(dmac, addr)) {
        case TARGET_RAM: {
            write_qword(dmac, addr & 0x1ffffff0, 0, value);
        } break;

        case TARGET_SPR: {
            write_qword(dmac, addr & 0x3ff0, 1, value);
        } break;

        default: {
        } break;
    }
}

static void pump_vif1_read(Dmac* dmac) {
    Channel* c = &dmac->channels[VIF1];
    gif::Gif* gif = dmac->hw.bus->gif;

    if (!vif1_reading(dmac) || !c->qwc || dmac->vif1_read_pending) {
        return;
    }

    if (dma_write_target(dmac, c->madr) == TARGET_ERROR) {
        dmac->stat |= 1 << 15;
        c->qwc = 0;

        end_transfer(dmac, VIF1);

        return;
    }

    uint32_t size = gif::download_remaining(gif);

    if (size > c->qwc) {
        size = c->qwc;
    }

    uint32_t left = size;

    while (left) {
        uint128_t chunk[64];

        uint32_t count = left < 64 ? left : 64;

        gif::read_download(gif, chunk, count);

        for (uint32_t index = 0; index < count; index++) {
            dma_write_qword(dmac, c->madr, chunk[index]);

            c->madr += 16;
        }

        left -= count;
    }

    c->qwc -= size;

    scheduler::Event event;

    event.name = "vif1_read_end";
    event.callback = finish_vif1_read;
    event.cycles = size ? (int64_t)size * 2 : 4;
    event.udata = dmac;

    dmac->vif1_read_pending = true;

    scheduler::schedule(dmac->hw.sched, event);
}

void vif1_download_ready(Dmac* dmac) {
    pump_vif1_read(dmac);
}

void vif1_read_abort(Dmac* dmac) {
    if (!vif1_reading(dmac) || dmac->vif1_read_pending) {
        return;
    }

    dmac->channels[VIF1].qwc = 0;

    end_transfer(dmac, VIF1);
}

void handle_vif1_read_transfer(Dmac* dmac) {
    pump_vif1_read(dmac);
}

static inline const uint8_t* dma_source_span(Dmac* dmac, uint32_t addr, uint32_t qwords) {
    uint64_t bytes = (uint64_t)qwords * 16;

    if (addr & 0x80000000) {
        uint64_t offset = addr & 0x3ff0;

        if (offset + bytes > dmac->hw.spr->size) {
            return nullptr;
        }

        return dmac->hw.spr->buf + offset;
    }

    ram::Ram* ram = dmac->hw.bus->ee_ram;

    if ((uint64_t)addr + bytes > ram->size) {
        return nullptr;
    }

    return ram->buf + addr;
}

static inline uint32_t transfer_vif1_unpack_qwords(Dmac* dmac, Channel* c, vif::Vif* vif, uint32_t pending) {
    uint32_t count = pending < c->qwc ? pending : c->qwc;

    const uint8_t* source = dma_source_span(dmac, c->madr, count);

    if (source) {
        vif::unpack_words(vif, source, count * 4);
    } else {
        for (uint32_t index = 0; index < count; index++) {
            uint128_t qword = read_qword(dmac, c->madr + index * 16);

            vif::unpack_words(vif, (const uint8_t*)&qword, 4);
        }
    }

    c->madr += count * 16;
    c->qwc -= count;
    c->qword_valid = false;

    profile::count(profile::VIF1_DMA_QWORDS, count);
    profile::count(profile::VIF1_UNPACK_BULK_QWORDS, count);

    return count;
}

static inline uint32_t transfer_vif1_mpg_qwords(Dmac* dmac, Channel* c, vif::Vif* vif, uint32_t pending) {
    uint32_t count = pending < c->qwc ? pending : c->qwc;

    const uint8_t* source = dma_source_span(dmac, c->madr, count);

    if (source) {
        vif::upload_micro_qwords(vif, source, count);
    } else {
        for (uint32_t index = 0; index < count; index++) {
            uint128_t qword = read_qword(dmac, c->madr + index * 16);

            vif::upload_micro_qwords(vif, (const uint8_t*)&qword, 1);
        }
    }

    c->madr += count * 16;
    c->qwc -= count;
    c->qword_valid = false;

    profile::count(profile::VIF1_DMA_QWORDS, count);

    return count;
}

static inline uint32_t transfer_vif1_direct_qwords(Dmac* dmac, Channel* c, vif::Vif* vif, uint32_t pending);

static inline uint32_t transfer_vif1_qwords(Dmac* dmac) {
    Channel* c = &dmac->channels[VIF1];

    if ((c->chcr & 0x100) == 0 || !c->qwc || c->index || (c->madr & 0xf)) {
        return 0;
    }

    vif::Vif* vif = dmac->hw.bus->vif1;

    uint32_t direct = vif::direct_qwords_pending(vif);

    if (direct) {
        return transfer_vif1_direct_qwords(dmac, c, vif, direct);
    }

    uint32_t unpack = vif::unpack_words_pending(vif) / 4;

    if (unpack) {
        return transfer_vif1_unpack_qwords(dmac, c, vif, unpack);
    }

    uint32_t mpg = vif::mpg_qwords_pending(vif);

    if (mpg) {
        return transfer_vif1_mpg_qwords(dmac, c, vif, mpg);
    }

    return 0;
}

static inline uint32_t transfer_vif1_direct_qwords(Dmac* dmac, Channel* c, vif::Vif* vif, uint32_t pending) {
    uint32_t count = pending < c->qwc ? pending : c->qwc;

    const uint8_t* source = dma_source_span(dmac, c->madr, count);

    if (source) {
        vif::write_direct_qwords(vif, source, count);
    } else {
        for (uint32_t index = 0; index < count; index++) {
            uint128_t qword = read_qword(dmac, c->madr + index * 16);

            vif::write_direct_qwords(vif, (const uint8_t*)&qword, 1);
        }
    }

    c->madr += count * 16;
    c->qwc -= count;
    c->qword_valid = false;

    profile::count(profile::VIF1_DMA_QWORDS, count);
    profile::count(profile::VIF1_DIRECT_BULK_QWORDS, count);

    return count;
}

int transfer_vif1_word(Dmac* dmac) {
    if ((dmac->channels[VIF1].chcr & 0x100) == 0) {
        iris_debug(dmac, "vif1 channel not started");

        return 0;
    }

    if (!vif::get_dreq(dmac->hw.bus->vif1)) {
        // iris_debug(dmac, "vif1 dreq cleared");

        return 0;
    }

    if (dmac->channels[VIF1].qwc) {
        Channel* c = &dmac->channels[VIF1];

        uint32_t base = c->madr & ~0xfu;

        if (!c->qword_valid || c->qword_addr != base) {
            c->qword = read_qword(dmac, base);
            c->qword_addr = base;
            c->qword_valid = true;
        }

        vif::fifo_write(dmac->hw.bus->vif1, c->qword.u32[(c->madr >> 2) & 3]);

        c->madr += 4;
        c->index++;

        if (c->index == 4) {
            c->index = 0;
            c->qwc--;

            profile::count(profile::VIF1_DMA_QWORDS);
        }

        return 1;
    }

    if (channel_is_done(&dmac->channels[VIF1])) {
        end_transfer(dmac, VIF1);

        iris_debug(dmac, "vif1 transfer done");

        return 0;
    }

    uint128_t tag = read_qword(dmac, dmac->channels[VIF1].tadr);

    process_source_tag(dmac, &dmac->channels[VIF1], tag);

    // iris_warning(dmac, "vif1 tag tag.qwc={:08x} qwc={:08x} id={} irq={} addr={:08x} data={:016x} end={} tte={}",
    //     dmac->channels[VIF1].tag.qwc,
    //     dmac->channels[VIF1].qwc,
    //     dmac->channels[VIF1].tag.id,
    //     dmac->channels[VIF1].tag.irq,
    //     dmac->channels[VIF1].tag.addr,
    //     dmac->channels[VIF1].tag.data,
    //     dmac->channels[VIF1].tag.end,
    //     (dmac->channels[VIF1].chcr >> 7) & 1
    // );

    if ((dmac->channels[VIF1].chcr >> 6) & 1) {
        vif::fifo_write(dmac->hw.bus->vif1, dmac->channels[VIF1].tag.data & 0xffffffff);

        if (!vif::get_dreq(dmac->hw.bus->vif1)) {
            // iris_fatal_error(dmac, "vif1 dreq cleared while writing tag data");

            // exit(1);
        }

        vif::fifo_write(dmac->hw.bus->vif1, dmac->channels[VIF1].tag.data >> 32);

        if (!vif::get_dreq(dmac->hw.bus->vif1)) {
            // iris_fatal_error(dmac, "vif1 dreq cleared while writing tag data");

            // exit(1);
        }
    }

    return 1;
}

void send_vif1_irq(void* udata, int overshoot) {
    Dmac* dmac = (Dmac*)udata;

    end_transfer(dmac, VIF1);
}

static int64_t dma_pace() {
    static int64_t pace = -1;

    if (pace < 0) {
        const char* setting = getenv("IRIS_DMA_PACE");

        pace = setting ? atoll(setting) : 0;

        if (pace < 0) {
            pace = 0;
        }
    }

    return pace;
}

constexpr int64_t VIF1_PACE_CHUNK_QWORDS = 128;
constexpr int64_t VIF1_PATH3_WAIT_CYCLES = 128;

static void run_vif1_transfer(Dmac* dmac);

static void resume_vif1_transfer(void* udata, int overshoot) {
    Dmac* dmac = (Dmac*)udata;

    dmac->vif1_pace_pending = false;

    if ((dmac->channels[VIF1].chcr & 0x100) == 0) {
        return;
    }

    if ((dmac->channels[VIF1].chcr & 1) == 0) {
        handle_vif1_read_transfer(dmac);

        return;
    }

    run_vif1_transfer(dmac);
}

static void schedule_vif1_resume(Dmac* dmac, int64_t cycles) {
    scheduler::Event event;

    event.name = "vif1_pace";
    event.callback = resume_vif1_transfer;
    event.cycles = cycles;
    event.udata = dmac;

    dmac->vif1_pace_pending = true;

    scheduler::schedule(dmac->hw.sched, event);
}

static void credit_vif1_budget(Dmac* dmac) {
    int64_t pace = dma_pace();
    int64_t qwords = (dmac->hw.sched->now - dmac->vif1_credit_time) / pace;

    if (qwords <= 0) {
        return;
    }

    dmac->vif1_budget += qwords * 4;
    dmac->vif1_credit_time += qwords * pace;
}

static bool vif1_word_needs_gif(uint32_t word) {
    switch ((word >> 24) & 0x7f) {
        case 0x14:
        case 0x15:
        case 0x17:
        case 0x50:
        case 0x51: {
            return true;
        }
    }

    return false;
}

static uint32_t peek_vif1_word(Dmac* dmac) {
    Channel* c = &dmac->channels[VIF1];

    uint32_t base = c->madr & ~0xfu;

    if (!c->qword_valid || c->qword_addr != base) {
        c->qword = read_qword(dmac, base);
        c->qword_addr = base;
        c->qword_valid = true;
    }

    return c->qword.u32[(c->madr >> 2) & 3];
}

static bool vif1_word_slices_path3(uint32_t word) {
    switch ((word >> 24) & 0x7f) {
        case 0x14:
        case 0x15:
        case 0x17:
        case 0x50: {
            return true;
        }
    }

    return false;
}

static bool vif1_word_is_flusha(uint32_t word) {
    return ((word >> 24) & 0x7f) == 0x13;
}

static bool vif1_next_command_matches(Dmac* dmac, bool (*match)(uint32_t)) {
    Channel* c = &dmac->channels[VIF1];

    if (c->qwc) {
        return match(peek_vif1_word(dmac));
    }

    if (channel_is_done(c) || ((c->chcr >> 6) & 1) == 0) {
        return false;
    }

    uint128_t tag = read_qword(dmac, c->tadr);

    return match(tag.u32[2]) || match(tag.u32[3]);
}

static bool path3_busy(Dmac* dmac) {
    gif::Gif* gif = dmac->hw.bus->gif;

    if (gif::path3_packet_started(gif)) {
        return true;
    }

    if ((dmac->channels[GIF].chcr & 0x100) == 0) {
        return false;
    }

    if (((dmac->ctrl >> 2) & 3) == 3) {
        return false;
    }

    if (gs::signal_stalled(dmac->hw.bus->gs)) {
        return false;
    }

    return !gif::path3_refusal(gif);
}

bool gif_path3_active(Dmac* dmac) {
    if (!dma_pace()) {
        return false;
    }

    if (path3_busy(dmac)) {
        return true;
    }

    return dmac->hw.sched->now < dmac->gif_busy_until;
}

void note_path3_output(Dmac* dmac, uint32_t qwords) {
    int64_t pace = dma_pace();

    if (!pace) {
        return;
    }

    int64_t now = dmac->hw.sched->now;

    if (dmac->gif_busy_until < now) {
        dmac->gif_busy_until = now;
    }

    dmac->gif_busy_until += (int64_t)qwords * pace;
}

static bool vif1_must_wait(Dmac* dmac) {
    int64_t pace = dma_pace();

    if (!pace) {
        return false;
    }

    if (dmac->hw.bus->vif1->state != vif::VIF_IDLE) {
        return false;
    }

    Channel* c = &dmac->channels[VIF1];

    if (!c->qwc && channel_is_done(c)) {
        return false;
    }

    if (gs::signal_stalled(dmac->hw.bus->gs)) {
        schedule_vif1_resume(dmac, VIF1_PATH3_WAIT_CYCLES);

        return true;
    }

    if (gif::path3_packet_started(dmac->hw.bus->gif) && vif1_next_command_matches(dmac, vif1_word_needs_gif)) {
        bool slice = gif::path3_image_slice(dmac->hw.bus->gif) && vif1_next_command_matches(dmac, vif1_word_slices_path3);

        if (!slice) {
            schedule_vif1_resume(dmac, VIF1_PATH3_WAIT_CYCLES);

            return true;
        }
    }

    if (vif1_next_command_matches(dmac, vif1_word_is_flusha) && path3_busy(dmac)) {
        dmac->vif1_flusha_wait = true;
        dmac->vif1_pace_pending = true;

        return true;
    }

    if (dmac->vif1_budget > 0) {
        return false;
    }

    credit_vif1_budget(dmac);

    if (dmac->vif1_budget > 0) {
        return false;
    }

    schedule_vif1_resume(dmac, VIF1_PACE_CHUNK_QWORDS * pace);

    return true;
}

static void run_vif1_transfer(Dmac* dmac) {
    while (true) {
        uint32_t qwords = transfer_vif1_qwords(dmac);

        if (qwords) {
            dmac->vif1_budget -= (int64_t)qwords * 4;

            continue;
        }

        if (vif1_must_wait(dmac)) {
            return;
        }

        if (!transfer_vif1_word(dmac)) {
            break;
        }

        dmac->vif1_budget--;
    }
}


void handle_vif1_transfer(Dmac* dmac) {
    if ((dmac->channels[VIF1].chcr & 0x100) == 0)
        return;

    if ((dmac->channels[VIF1].chcr & 1) == 0) {
        handle_vif1_read_transfer(dmac);

        return;
    }

    if (dmac->vif1_pace_pending) {
        return;
    }

    int mfifo_drain = (dmac->ctrl >> 2) & 3;

    // iris_warning(dmac, "VIF1 DMA dir={} mode={} tte={} tie={} qwc={} madr={:08x} tadr={:08x} end={} dreq={} mfifo_drain={}",
    //     dmac->channels[VIF1].chcr & 1,
    //     (dmac->channels[VIF1].chcr >> 2) & 3,
    //     (dmac->channels[VIF1].chcr >> 6) & 1,
    //     (dmac->channels[VIF1].chcr >> 7) & 1,
    //     dmac->channels[VIF1].qwc,
    //     dmac->channels[VIF1].madr,
    //     dmac->channels[VIF1].tadr,
    //     dmac->channels[VIF1].tag.end,
    //     vif::get_dreq(dmac->hw.bus->vif1),
    //     mfifo_drain
    // );

    if (mfifo_drain == 2) {
        return;
    }

    // Note: MGS3 will not boot unless VIF1 DMA IRQs are delayed by a few cycles for some reason.
    //       My guess is that the game expects some VIF command within the transfer to stall and thus
    //       delay the transfer, but I haven't been able to confirm this.
    //       In order to debug: Checkout commit d58ca88

    // scheduler::Event event;

    // event.name = "vif1_transfer_end";
    // event.cycles = 4096;
    // event.udata = dmac;
    // event.callback = send_vif1_irq;

    // scheduler::schedule(dmac->hw.sched, event);

    int tte = (dmac->channels[VIF1].chcr >> 6) & 1;
    int mode = (dmac->channels[VIF1].chcr >> 2) & 3;

    if (mode == 3)
        mode = 1;

    if ((dmac->channels[VIF1].chcr & 1) == 0) {
        handle_vif1_read_transfer(dmac);

        return;
    }

    Channel* channel = &dmac->channels[VIF1];

    uint32_t kick_address = mode ? channel->tadr : channel->madr;

    profile::count(profile::VIF1_DMA_KICKS);

    if (kick_address == channel->kick_address) {
        profile::count(profile::VIF1_DMA_REPEATED_KICKS);
    }

    channel->kick_address = kick_address;
    channel->qword_valid = false;

    dmac->vif1_budget = VIF1_PACE_CHUNK_QWORDS * 4;
    dmac->vif1_credit_time = dmac->hw.sched->now;

    run_vif1_transfer(dmac);
}

void send_gif_irq(void* udata, int overshoot) {
    Dmac* dmac = (Dmac*)udata;

    set_irq(dmac, GIF);

    dmac->channels[GIF].chcr &= ~0x100;
    dmac->channels[GIF].qwc = 0;
}

void handle_gif_transfer(Dmac* dmac);

static bool gif_yield_at_packet_end(Dmac* dmac, int64_t spent);

static inline void transfer_gif_qwords(Dmac* dmac, Channel* c) {
    gif::Gif* gif = dmac->hw.bus->gif;

    if (!c->qwc) {
        return;
    }

    if (gif->p3_refuse) {
        int64_t spent = 0;

        while (c->qwc) {
            if (gif::path3_refusal(gif) && (gif::path3_fifo_holding(gif) || !gif::path3_packet_open(gif))) {
                uint32_t space = gif::path3_fifo_space(gif);

                if (space > c->qwc) {
                    space = c->qwc;
                }

                for (uint32_t index = 0; index < space; index++) {
                    uint128_t qword = read_qword(dmac, c->madr);

                    gif::path3_fifo_push(gif, (const uint8_t*)&qword, 1);

                    c->madr += 16;
                    c->qwc--;
                }

                return;
            }

            if (!gif::path3_packet_open(gif)) {
                dmac->gif_unmask_run = false;
            }

            uint64_t count = gif::path3_packet_qwords(gif);

            if (!count) {
                count = 1;
            }

            if (count > c->qwc) {
                count = c->qwc;
            }

            const uint8_t* source = dma_source_span(dmac, c->madr, (uint32_t)count);

            if (source) {
                gif::fifo_write_qwords(gif, source, (uint32_t)count, gif::PATH3);
            } else {
                for (uint64_t index = 0; index < count; index++) {
                    gif::fifo_write(gif, read_qword(dmac, c->madr + (uint32_t)index * 16), gif::PATH3);
                }
            }

            c->madr += (uint32_t)count * 16;
            c->qwc -= (uint32_t)count;

            spent += (int64_t)count;

            if (c->qwc && !gif::path3_packet_open(gif) && gif_yield_at_packet_end(dmac, spent)) {
                return;
            }
        }

        return;
    }

    if (!gif::path3_stall_enabled(gif)) {
        const uint8_t* source = dma_source_span(dmac, c->madr, c->qwc);

        if (source) {
            gif::fifo_write_qwords(gif, source, c->qwc, gif::PATH3);

            c->madr += c->qwc * 16;
            c->qwc = 0;

            return;
        }
    }

    uint32_t sent = 0;

    while (sent < c->qwc) {
        if (!gif::can_accept(gif, gif::PATH3)) {
            break;
        }

        uint128_t q = read_qword(dmac, c->madr);

        gif::fifo_write(gif, q, gif::PATH3);

        c->madr += 16;

        sent++;
    }

    c->qwc -= sent;
}

static void run_gif_transfer(Dmac* dmac);

void resume_gif(Dmac* dmac) {
    if ((dmac->channels[GIF].chcr & 0x100) == 0) {
        return;
    }

    if (dmac->hw.bus->gif->p3_refuse && ((dmac->ctrl >> 2) & 3) != 3) {
        dmac->gif_unmask_run = !gif::path3_packet_open(dmac->hw.bus->gif);
        dmac->gif_budget = 0;
        dmac->gif_credit_time = dmac->hw.sched->now;

        run_gif_transfer(dmac);

        dmac->gif_unmask_run = false;

        return;
    }

    handle_gif_transfer(dmac);
}

static void resume_gif_transfer(void* udata, int overshoot) {
    Dmac* dmac = (Dmac*)udata;

    dmac->gif_pace_pending = false;

    if ((dmac->channels[GIF].chcr & 0x100) == 0) {
        return;
    }

    handle_gif_transfer(dmac);
}

static void schedule_gif_resume(Dmac* dmac, int64_t cycles) {
    if (dmac->gif_pace_pending) {
        return;
    }

    scheduler::Event event;

    event.name = "gif_pace";
    event.callback = resume_gif_transfer;
    event.cycles = cycles;
    event.udata = dmac;

    dmac->gif_pace_pending = true;

    scheduler::schedule(dmac->hw.sched, event);
}

static bool gif_yield_at_packet_end(Dmac* dmac, int64_t spent) {
    int64_t pace = dma_pace();

    if (!pace) {
        return false;
    }

    int64_t qwords = (dmac->hw.sched->now - dmac->gif_credit_time) / pace;

    if (qwords > 0) {
        dmac->gif_budget += qwords;
        dmac->gif_credit_time += qwords * pace;
    }

    if (dmac->gif_budget - spent > 0) {
        return false;
    }

    schedule_gif_resume(dmac, VIF1_PACE_CHUNK_QWORDS * pace);

    return true;
}

static bool gif_must_wait(Dmac* dmac) {
    int64_t pace = dma_pace();

    if (!pace) {
        return false;
    }

    if (gs::signal_stalled(dmac->hw.bus->gs)) {
        schedule_gif_resume(dmac, VIF1_PATH3_WAIT_CYCLES);

        return true;
    }

    gif::Gif* gif = dmac->hw.bus->gif;

    if (gif::path3_packet_open(gif) && !gif::path3_image_slice(gif)) {
        return false;
    }

    if (gif::path2_packet_open(gif)) {
        schedule_gif_resume(dmac, VIF1_PATH3_WAIT_CYCLES);

        return true;
    }

    if (dmac->gif_unmask_run) {
        return false;
    }

    if (dmac->gif_budget > 0) {
        return false;
    }

    int64_t qwords = (dmac->hw.sched->now - dmac->gif_credit_time) / pace;

    if (qwords > 0) {
        dmac->gif_budget += qwords;
        dmac->gif_credit_time += qwords * pace;
    }

    if (dmac->gif_budget > 0) {
        return false;
    }

    schedule_gif_resume(dmac, VIF1_PACE_CHUNK_QWORDS * pace);

    return true;
}

static inline bool transfer_gif_payload(Dmac* dmac, Channel* c) {
    uint32_t before = c->qwc;

    transfer_gif_qwords(dmac, c);

    dmac->gif_budget -= before - c->qwc;

    return c->qwc == 0;
}

static void wake_vif1_flusha(Dmac* dmac) {
    if (!dmac->vif1_flusha_wait || path3_busy(dmac)) {
        return;
    }

    dmac->vif1_flusha_wait = false;

    schedule_vif1_resume(dmac, 1);
}

static void step_gif_transfer(Dmac* dmac);

static void run_gif_transfer(Dmac* dmac) {
    step_gif_transfer(dmac);

    wake_vif1_flusha(dmac);
}

static void step_gif_transfer(Dmac* dmac) {
    Channel* c = &dmac->channels[GIF];
    gif::Gif* gif = dmac->hw.bus->gif;

    bool path3_yields = !gif::path3_packet_open(gif) || gif::path3_image_slice(gif);

    if (dma_pace() && c->qwc && path3_yields && gif::path2_packet_open(gif)) {
        schedule_gif_resume(dmac, VIF1_PATH3_WAIT_CYCLES);

        return;
    }

    if (!transfer_gif_payload(dmac, c)) {
        return;
    }

    if (c->tag.end) {
        end_transfer(dmac, GIF);

        return;
    }

    do {
        if (gif_must_wait(dmac)) {
            return;
        }

        uint128_t tag = read_qword(dmac, c->tadr);

        process_source_tag(dmac, c, tag);

        if (c->tag.id == 1) {
            c->tadr = c->madr + c->qwc * 16;
        }

        dmac->gif_budget--;

        if (!transfer_gif_payload(dmac, c)) {
            return;
        }
    } while (!channel_is_done(c));

    end_transfer(dmac, GIF);
}

void handle_gif_transfer(Dmac* dmac) {
    if (dmac->gif_pace_pending) {
        return;
    }

    int mfifo_drain = (dmac->ctrl >> 2) & 3;

    if (mfifo_drain == 3) {
        return;
    }

    run_gif_transfer(dmac);
}


void handle_ipu_from_transfer(Dmac* dmac) {
    if ((dmac->channels[IPU_FROM].chcr & 0x100) == 0) {
        // iris_debug(dmac, "ipu_from channel not started");

        return;
    }

    int mode = (dmac->channels[IPU_FROM].chcr >> 2) & 3;

    // iris_debug(dmac, "ipu_from start data={:08x} dir={} mod={} tte={} madr={:08x} qwc={:08x} tadr={:08x} dreq={}", //     dmac->channels[IPU_FROM].chcr,
    //     dmac->channels[IPU_FROM].chcr & 1,
    //     (dmac->channels[IPU_FROM].chcr >> 2) & 3,
    //     !!(dmac->channels[IPU_FROM].chcr & 0x40),
    //     dmac->channels[IPU_FROM].madr,
    //     dmac->channels[IPU_FROM].qwc,
    //     dmac->channels[IPU_FROM].tadr,
    //     dmac->channels[IPU_FROM].dreq
    //);

    if (mode != 0) {
        iris_fatal_error(dmac, "ipu_from mode {} not supported", mode);

        return;
    }

    while (dmac->channels[IPU_FROM].dreq && dmac->channels[IPU_FROM].qwc) {
        uint128_t q = ipu::fifo_read(dmac->hw.ipu);

        write_qword(dmac, dmac->channels[IPU_FROM].madr, 0, q);

        dmac->channels[IPU_FROM].madr += 16;
        dmac->channels[IPU_FROM].qwc--;
    }

    if (dmac->channels[IPU_FROM].qwc == 0) {
        set_irq(dmac, IPU_FROM);

        dmac->channels[IPU_FROM].chcr &= ~0x100;
        dmac->channels[IPU_FROM].qwc = 0;
    }
}

int transfer_ipu_to_qword(Dmac* dmac) {
    if ((dmac->channels[IPU_TO].chcr & 0x100) == 0) {
        // iris_debug(dmac, "ipu_to channel not started");

        return 0;
    }

    if (!dmac->channels[IPU_TO].dreq) {
        // iris_debug(dmac, "ipu_to dreq cleared");

        return 0;
    }

    if (dmac->channels[IPU_TO].qwc) {
        uint128_t q = read_qword(dmac, dmac->channels[IPU_TO].madr);

        ipu::fifo_write(dmac->hw.ipu, q);

        dmac->channels[IPU_TO].madr += 16;
        dmac->channels[IPU_TO].qwc--;

        return 1;
    }

    if (channel_is_done(&dmac->channels[IPU_TO])) {
        set_irq(dmac, IPU_TO);

        dmac->channels[IPU_TO].chcr &= ~0x100;
        dmac->channels[IPU_TO].qwc = 0;

        return 0;
    }

    if (dmac->channels[IPU_TO].tag.id == 1) {
        dmac->channels[IPU_TO].tadr = dmac->channels[IPU_TO].madr;
        iris_fatal_error(dmac, "ipu_to tag id=1, setting tadr to {:08x}", dmac->channels[IPU_TO].tadr);
    }

    uint128_t tag = read_qword(dmac, dmac->channels[IPU_TO].tadr);

    process_source_tag(dmac, &dmac->channels[IPU_TO], tag);

    // iris_debug(dmac, "ipu_to tag tag.qwc={:08x} qwc={:08x} id={} irq={} addr={:08x} mem={} data={:016x} end={} tte={}", //     dmac->channels[IPU_TO].tag.qwc,
    //     dmac->channels[IPU_TO].qwc,
    //     dmac->channels[IPU_TO].tag.id,
    //     dmac->channels[IPU_TO].tag.irq,
    //     dmac->channels[IPU_TO].tag.addr,
    //     dmac->channels[IPU_TO].tag.mem,
    //     dmac->channels[IPU_TO].tag.data,
    //     dmac->channels[IPU_TO].tag.end,
    //     (dmac->channels[IPU_TO].chcr >> 7) & 1
    //);

    return 1;
}
void handle_ipu_to_transfer(Dmac* dmac) {
    if ((dmac->channels[IPU_TO].chcr & 0x100) == 0) {
        // iris_debug(dmac, "ipu_to channel not started");

        return;
    }

    // iris_debug(dmac, "ipu_to start data={:08x} dir={} mod={} tte={} madr={:08x} qwc={:08x} tadr={:08x}", //     dmac->channels[IPU_TO].chcr,
    //     dmac->channels[IPU_TO].chcr & 1,
    //     (dmac->channels[IPU_TO].chcr >> 2) & 3,
    //     !!(dmac->channels[IPU_TO].chcr & 0x40),
    //     dmac->channels[IPU_TO].madr,
    //     dmac->channels[IPU_TO].qwc,
    //     dmac->channels[IPU_TO].tadr
    //);

    while (transfer_ipu_to_qword(dmac)) {
        // Keep transferring until we run out of QWC or DREQ is cleared
    }
}
void handle_sif0_transfer(Dmac* dmac) {
    // SIF FIFO is empty, keep waiting
    if (sif::fifo_is_empty(dmac->hw.sif->sif0)) {
        return;
    }

    // Data ready but channel isn't ready yet, keep waiting
    if (!(dmac->channels[SIF0].chcr & 0x100)) {
        return;
    }
    // iris_debug(dmac, "sif0 start data={:08x} dir={} mod={} tte={} madr={:08x} qwc={:08x} tadr={:08x}", //     dmac->channels[SIF0].chcr,
    //     dmac->channels[SIF0].chcr & 1,
    //     (dmac->channels[SIF0].chcr >> 2) & 3,
    //     !!(dmac->channels[SIF0].chcr & 0x40),
    //     dmac->channels[SIF0].madr,
    //     dmac->channels[SIF0].qwc,
    //     dmac->channels[SIF0].tadr
    //);

    while (!sif::fifo_is_empty(dmac->hw.sif->sif0)) {
        uint128_t tag = sif::fifo_read(dmac->hw.sif->sif0);

        process_dest_tag(dmac, &dmac->channels[SIF0], tag);

        // iris_debug(dmac, "ee: sif0 tag qwc={:08x} madr={:08x} id={} irq={} addr={:08x} mem={} data={:016x} tte={}", //     dmac->channels[SIF0].tag.qwc,
        //     dmac->channels[SIF0].madr,
        //     dmac->channels[SIF0].tag.id,
        //     dmac->channels[SIF0].tag.irq,
        //     dmac->channels[SIF0].tag.addr,
        //     dmac->channels[SIF0].tag.mem,
        //     dmac->channels[SIF0].tag.data,
        //     dmac->channels[SIF0].chcr
        //);

        for (int i = 0; i < dmac->channels[SIF0].qwc; i++) {
            if (sif::fifo_is_empty(dmac->hw.sif->sif0)) {
                iris_debug(dmac, "qwc != 0 FIFO empty");

                if (channel_is_done(&dmac->channels[SIF0])) {
                    iris_debug(dmac, "qwc != 0 FIFO empty");

                    dmac->channels[SIF0].chcr &= ~0x100;
                    dmac->channels[SIF0].qwc = 0;
        
                    set_irq(dmac, SIF0);
        
                    return;
                }
            }

            uint128_t q = sif::fifo_read(dmac->hw.sif->sif0);

            // iris_debug(dmac, "{:08x}:", dmac->channels[SIF0].madr);

            // for (int i = 0; i < 16; i++) {
            //     iris_debug(dmac, "{:02x}", q.u8[i]);
            // }

            // putchar('|');

            // for (int i = 0; i < 16; i++) {
            //     iris_debug(dmac, "{}", isprint(q.u8[i]) ? q.u8[i] : '.');
            // }

            // puts("|");

            // iris_debug(dmac, "ee: Writing {:016x} {:016x} to {:08x}", q.u64[1], q.u64[0], dmac->channels[SIF0].madr);

            write_qword(dmac, dmac->channels[SIF0].madr, 0, q);

            dmac->channels[SIF0].madr += 16;
        }

        if (channel_is_done(&dmac->channels[SIF0])) {
            dmac->channels[SIF0].chcr &= ~0x100;
            dmac->channels[SIF0].qwc = 0;

            set_irq(dmac, SIF0);

            // sif::fifo_reset(dmac->hw.sif);

            return;
        }
    }

    // dmac->channels[SIF0].chcr &= ~0x100;

    // set_irq(dmac, SIF0);

    // sif::fifo_reset(dmac->hw.sif);

    // We shouldn't send an interrupt if tag end or irq/tie weren't
    // set
}
void handle_sif1_transfer(Dmac* dmac) {
    assert(!dmac->channels[SIF1].qwc);
    assert(((dmac->channels[SIF1].chcr >> 2) & 3) == 1);

    // This should be ok?
    // if (!sif::fifo_is_empty(dmac->hw.sif)) {
    //     iris_debug(dmac, "WARNING!!! SIF FIFO not empty");
    // }

    do {
        uint128_t tag = read_qword(dmac, dmac->channels[SIF1].tadr);

        process_source_tag(dmac, &dmac->channels[SIF1], tag);

        // iris_debug(dmac, "ee: sif1 tag qwc={:08x} id={} irq={} addr={:08x} mem={} data={:016x} end={} tte={}", //     dmac->channels[SIF1].tag.qwc,
        //     dmac->channels[SIF1].tag.id,
        //     dmac->channels[SIF1].tag.irq,
        //     dmac->channels[SIF1].tag.addr,
        //     dmac->channels[SIF1].tag.mem,
        //     dmac->channels[SIF1].tag.data,
        //     dmac->channels[SIF1].tag.end,
        //     (dmac->channels[SIF1].chcr >> 7) & 1
        //);
        // iris_debug(dmac, "ee: SIF1 tag madr={:08x}", dmac->channels[SIF1].madr);

        for (int i = 0; i < dmac->channels[SIF1].qwc; i++) {
            uint128_t q = read_qword(dmac, dmac->channels[SIF1].madr);

            // iris_debug(dmac, "{:08x}:", dmac->channels[SIF1].madr);

            // for (int i = 0; i < 16; i++) {
            //     iris_debug(dmac, "{:02x}", q.u8[i]);
            // }

            // putchar('|');

            // for (int i = 0; i < 16; i++) {
            //     iris_debug(dmac, "{}", isprint(q.u8[i]) ? q.u8[i] : '.');
            // }

            // puts("|");

            sif::fifo_write(dmac->hw.sif->sif1, q);

            dmac->channels[SIF1].madr += 16;
        }

        if (dmac->channels[SIF1].tag.id == 1) {
            dmac->channels[SIF1].tadr = dmac->channels[SIF1].madr;

            iris_fatal_error(dmac, "SIF1 tag id=1, setting TADR to MADR={:08x}", dmac->channels[SIF1].madr);
        }
    } while (!channel_is_done(&dmac->channels[SIF1]));

    iop::dma::handle_sif1_transfer(dmac->hw.iop_dma);

    set_irq(dmac, SIF1);

    dmac->channels[SIF1].chcr &= ~0x100;
    dmac->channels[SIF1].qwc = 0;
}
void handle_sif2_transfer(Dmac* dmac) {
    iris_fatal_error(dmac, "ee: SIF2 channel unimplemented");
}

void spr_from_interleave(Dmac* dmac) {
    uint32_t sqwc = dmac->sqwc & 0xff;
    uint32_t tqwc = (dmac->sqwc >> 16) & 0xff;

    // Note: When TQWC=0, it is set to QWC instead (undocumented)
    if (tqwc == 0)
        tqwc = dmac->channels[SPR_FROM].qwc;

    while (dmac->channels[SPR_FROM].qwc) {
        for (int i = 0; i < tqwc && dmac->channels[SPR_FROM].qwc; i++) {
            uint128_t q = ram::read128(dmac->hw.spr, dmac->channels[SPR_FROM].sadr);

            ee::bus::write128(dmac->hw.bus, dmac->channels[SPR_FROM].madr, q);

            dmac->channels[SPR_FROM].madr += 0x10;
            dmac->channels[SPR_FROM].sadr += 0x10;
            dmac->channels[SPR_FROM].sadr &= 0x3ff0;
            dmac->channels[SPR_FROM].qwc--;
        }

        dmac->channels[SPR_FROM].madr += sqwc * 16;
    }
}
void handle_spr_from_transfer(Dmac* dmac) {
    set_irq(dmac, SPR_FROM);

    dmac->channels[SPR_FROM].chcr &= ~0x100;

    // iris_debug(dmac, "spr_from start data={:08x} dir={} mod={} tte={} madr={:08x} qwc={:08x} tadr={:08x} sadr={:08x} rbor={:08x} rbsr={:08x}", //     dmac->channels[SPR_FROM].chcr,
    //     dmac->channels[SPR_FROM].chcr & 1,
    //     (dmac->channels[SPR_FROM].chcr >> 2) & 3,
    //     !!(dmac->channels[SPR_FROM].chcr & 0x40),
    //     dmac->channels[SPR_FROM].madr,
    //     dmac->channels[SPR_FROM].qwc,
    //     dmac->channels[SPR_FROM].tadr,
    //     dmac->channels[SPR_FROM].sadr,
    //     dmac->rbor,
    //     dmac->rbsr
    //);

    // exit(1);

    int mode = (dmac->channels[SPR_FROM].chcr >> 2) & 3;

    if (dmac->mfifo_drain) {
        assert(mode == 0);

        Channel* spr = &dmac->channels[SPR_FROM];

        spr->madr = mfifo_wrap(dmac, spr->madr);

        for (int i = 0; i < spr->qwc; i++) {
            uint128_t q = ram::read128(dmac->hw.spr, spr->sadr & 0x3ff0);

            ee::bus::write128(dmac->hw.bus, spr->madr, q);

            mfifo_write_qword(dmac, q);

            spr->madr = mfifo_wrap(dmac, spr->madr + 0x10);
            spr->sadr = (spr->sadr + 0x10) & 0x3ff0;
        }

        spr->qwc = 0;

        return;
    }

    if (mode == 2) {
        spr_from_interleave(dmac);

        return;
    }

    for (int i = 0; i < dmac->channels[SPR_FROM].qwc; i++) {
        uint128_t q = ram::read128(dmac->hw.spr, dmac->channels[SPR_FROM].sadr & 0x3ff0);

        ee::bus::write128(dmac->hw.bus, dmac->channels[SPR_FROM].madr, q);

        dmac->channels[SPR_FROM].madr += 0x10;
        dmac->channels[SPR_FROM].sadr += 0x10;
        dmac->channels[SPR_FROM].sadr &= 0x3ff0;
    }

    dmac->channels[SPR_FROM].qwc = 0;

    if (dmac->channels[SPR_FROM].tag.end) {
        return;
    }

    // Chain mode
    do {
        uint128_t tag = ram::read128(dmac->hw.spr, dmac->channels[SPR_FROM].sadr & 0x3ff0);

        dmac->channels[SPR_FROM].sadr += 0x10;
        dmac->channels[SPR_FROM].sadr &= 0x3ff0;

        dmac->channels[SPR_FROM].qwc = tag.u32[0] & 0xffff;
        dmac->channels[SPR_FROM].tag.id = (tag.u32[0] >> 28) & 0x7;
        dmac->channels[SPR_FROM].tag.irq = tag.u32[0] & 0x80000000;
        dmac->channels[SPR_FROM].tag.end = dmac->channels[SPR_FROM].tag.id == 0 || dmac->channels[SPR_FROM].tag.id == 7;
        dmac->channels[SPR_FROM].madr = tag.u32[1];

        // iris_debug(dmac, "ee: spr_from tag qwc={:08x} madr={:08x} sadr={:08x} tadr={:08x} id={} addr={:08x} mem={} data={:016x} irq={} end={} tte={}", //     dmac->channels[SPR_FROM].tag.qwc,
        //     dmac->channels[SPR_FROM].madr,
        //     dmac->channels[SPR_FROM].sadr,
        //     dmac->channels[SPR_FROM].tadr,
        //     dmac->channels[SPR_FROM].tag.id,
        //     dmac->channels[SPR_FROM].tag.addr,
        //     dmac->channels[SPR_FROM].tag.mem,
        //     dmac->channels[SPR_FROM].tag.data,
        //     dmac->channels[SPR_FROM].tag.irq,
        //     dmac->channels[SPR_FROM].tag.end,
        //     (dmac->channels[SPR_FROM].chcr >> 7) & 1
        //);

        for (int i = 0; i < dmac->channels[SPR_FROM].qwc; i++) {
            uint128_t q = ram::read128(dmac->hw.spr, dmac->channels[SPR_FROM].sadr & 0x3ff0);

            ee::bus::write128(dmac->hw.bus, dmac->channels[SPR_FROM].madr, q);

            dmac->channels[SPR_FROM].madr += 0x10;
            dmac->channels[SPR_FROM].sadr += 0x10;
            dmac->channels[SPR_FROM].sadr &= 0x3ff0;
        }
    } while (!channel_is_done(&dmac->channels[SPR_FROM]));
}

void spr_to_interleave(Dmac* dmac) {
    uint32_t sqwc = dmac->sqwc & 0xff;
    uint32_t tqwc = (dmac->sqwc >> 16) & 0xff;

    // Note: When TQWC=0, it is set to QWC instead (undocumented)
    if (tqwc == 0)
        tqwc = dmac->channels[SPR_TO].qwc;

    while (dmac->channels[SPR_TO].qwc) {
        for (int i = 0; i < tqwc && dmac->channels[SPR_TO].qwc; i++) {
            uint128_t q = read_qword(dmac, dmac->channels[SPR_TO].madr);

            ram::write128(dmac->hw.spr, dmac->channels[SPR_TO].sadr, q);

            dmac->channels[SPR_TO].madr += 0x10;
            dmac->channels[SPR_TO].sadr += 0x10;
            dmac->channels[SPR_TO].sadr &= 0x3ff0;
            dmac->channels[SPR_TO].qwc--;
        }

        dmac->channels[SPR_TO].madr += sqwc * 16;
    }
}
void handle_spr_to_transfer(Dmac* dmac) {
    set_irq(dmac, SPR_TO);

    dmac->channels[SPR_TO].chcr &= ~0x100;

    int mode = (dmac->channels[SPR_TO].chcr >> 2) & 3;

    // iris_debug(dmac, "ee: spr_to start data={:08x} dir={} mod={} tte={} madr={:08x} qwc={:08x} tadr={:08x} sadr={:08x}", //     dmac->channels[SPR_TO].chcr,
    //     dmac->channels[SPR_TO].chcr & 1,
    //     (dmac->channels[SPR_TO].chcr >> 2) & 3,
    //     !!(dmac->channels[SPR_TO].chcr & 0x40),
    //     dmac->channels[SPR_TO].madr,
    //     dmac->channels[SPR_TO].qwc,
    //     dmac->channels[SPR_TO].tadr,
    //     dmac->channels[SPR_TO].sadr
    //);

    if (mode == 2) {
        spr_to_interleave(dmac);

        return;
    }

    for (int i = 0; i < dmac->channels[SPR_TO].qwc; i++) {
        uint128_t q = read_qword(dmac, dmac->channels[SPR_TO].madr);

        ram::write128(dmac->hw.spr, dmac->channels[SPR_TO].sadr, q);

        dmac->channels[SPR_TO].madr += 0x10;
        dmac->channels[SPR_TO].sadr += 0x10;
        dmac->channels[SPR_TO].sadr &= 0x3ff0;
    }

    dmac->channels[SPR_TO].qwc = 0;

    // We're done
    if (dmac->channels[SPR_TO].tag.end)
        return;

    // Chain mode
    do {
        uint128_t tag = read_qword(dmac, dmac->channels[SPR_TO].tadr);

        if ((dmac->channels[SPR_TO].chcr >> 6) & 1) {
            ram::write128(dmac->hw.spr, dmac->channels[SPR_TO].sadr, tag);

            dmac->channels[SPR_TO].sadr += 0x10;
        }

        process_source_tag(dmac, &dmac->channels[SPR_TO], tag);
        
        // iris_debug(dmac, "ee: spr_to tag qwc={:08x} madr={:08x} tadr={:08x} id={} addr={:08x} mem={} end={} data={:08x}{:08x}", //     dmac->channels[SPR_TO].tag.qwc,
        //     dmac->channels[SPR_TO].madr,
        //     dmac->channels[SPR_TO].tadr,
        //     dmac->channels[SPR_TO].tag.id,
        //     dmac->channels[SPR_TO].tag.addr,
        //     dmac->channels[SPR_TO].tag.mem,
        //     dmac->channels[SPR_TO].tag.end,
        //     tag.u32[1], tag.u32[0]
        //);

        for (int i = 0; i < dmac->channels[SPR_TO].qwc; i++) {
            uint128_t q = read_qword(dmac, dmac->channels[SPR_TO].madr);

            ram::write128(dmac->hw.spr, dmac->channels[SPR_TO].sadr, q);

            dmac->channels[SPR_TO].madr += 0x10;
            dmac->channels[SPR_TO].sadr += 0x10;
            dmac->channels[SPR_TO].sadr &= 0x3ff0;
        }

        if (dmac->channels[SPR_TO].tag.id == 1) {
            dmac->channels[SPR_TO].tadr = dmac->channels[SPR_TO].madr;
        }
    } while (!channel_is_done(&dmac->channels[SPR_TO]));
}
static inline void handle_channel_start(Dmac* dmac, uint32_t addr) {
    Channel* c = get_channel(dmac, addr);

    // if (c == &dmac->channels[IPU_TO] || c == &dmac->channels[IPU_FROM])
    // iris_debug(dmac, "{} start data={:08x} dir={} mod={} tte={} madr={:08x} qwc={:08x} tadr={:08x} rbsr={:08x} rbor={:08x}", //     get_channel_name(dmac, addr),
    //     c->chcr,
    //     c->chcr & 1,
    //     (c->chcr >> 2) & 3,
    //     !!(c->chcr & 0x40),
    //     c->madr,
    //     c->qwc,
    //     c->tadr,
    //     dmac->rbsr,
    //     dmac->rbor
    //);

    // if (c == &dmac->channels[IPU_TO] && c->qwc != 0) {
    //     int mode = (c->chcr >> 2) & 3;

    //     if (mode == 1) {
    //         uint128_t tag;

    //         tag.u32[0] = (c->chcr & 0xffff0000) | (c->qwc & 0xffff);

    //         process_source_tag(dmac, c, tag);
    //     } else {
    //         c->tag.end = 1;
    //     }
    // }

    int mode = (c->chcr >> 2) & 3;

    // Modes 1 and 3 are chain modes
    if ((mode & 1) == 0) {
        c->tag.end = 1;
    } else if (c->qwc != 0) {
        int id = c->chcr >> 28 & 7;
        int tie = (c->chcr >> 7) & 1;
        int irq = c->chcr & 0x80000000;

        c->tag.end = (id == 0 || id == 7) || (tie && irq);

        // iris_debug(dmac, "{} qwc != 0, madr={:08x} tadr={:08x} qwc={:08x} tag={:08x} end={}", get_channel_name(dmac, addr),
        //     c->madr, c->tadr, c->qwc, c->chcr >> 16, c->tag.end
        //);
    } else {
        c->tag.end = 0;
    }

    switch (addr & 0xff00) {
        case 0x8000: handle_vif0_transfer(dmac); return;
        case 0x9000: handle_vif1_transfer(dmac); return;
        case 0xA000: {
            dmac->gif_budget = VIF1_PACE_CHUNK_QWORDS;
            dmac->gif_credit_time = dmac->hw.sched->now;

            handle_gif_transfer(dmac);
        } return;
        case 0xB000: handle_ipu_from_transfer(dmac); return;
        case 0xB400: handle_ipu_to_transfer(dmac); return;
        case 0xC000: handle_sif0_transfer(dmac); return;
        case 0xC400: handle_sif1_transfer(dmac); return;
        case 0xC800: handle_sif2_transfer(dmac); return;
        case 0xD000: handle_spr_from_transfer(dmac); return;
        case 0xD400: handle_spr_to_transfer(dmac); return;
    }
}

void write_stat(Dmac* dmac, uint32_t data) {
    uint32_t istat = data & 0x0000ffff;
    uint32_t imask = data & 0xffff0000;

    dmac->stat &= ~istat;
    dmac->stat ^= imask;

    // iris_debug(dmac, "stat={:08x} istat={:08x} imask={:08x}", dmac->stat, istat, imask);

    test_irq(dmac);
}

static void update_mfifo_drain(Dmac* dmac) {
    int mfifo_drain = (dmac->ctrl >> 2) & 3;

    switch (mfifo_drain) {
        case 0: {
            dmac->mfifo_drain = NULL;
        } break;

        case 2: {
            dmac->mfifo_drain = &dmac->channels[VIF1];
        } break;

        case 3: {
            dmac->mfifo_drain = &dmac->channels[GIF];
        } break;

        default: {
            iris_fatal_error(dmac, "Invalid MFIFO drain channel {}", mfifo_drain);
        } break;
    }
}

void write32(Dmac* dmac, uint32_t addr, uint64_t data) {
    Channel* c = get_channel(dmac, addr);

    switch (addr) {
        case 0x1000E000: {
            dmac->ctrl = data;

            update_mfifo_drain(dmac);
        } return;
        case 0x1000E010: write_stat(dmac, data); return;
        case 0x1000E020: dmac->pcr = data; test_cpcond0(dmac); return;
        case 0x1000E030: dmac->sqwc = data; return;
        case 0x1000E040: dmac->rbsr = data; return;
        case 0x1000E050: dmac->rbor = data; return;
        case 0x1000F520: return; // ENABLER (R)
        case 0x1000F590: dmac->enable = data; return;
    }

    if (!c)
        return;

    switch (addr & 0xff) {
        case 0x00: {
            // Behavior required for IPU FMVs to work
            if ((c->chcr & 0x100) == 0) {
                c->chcr = data;

                if (data & 0x100) {
                    handle_channel_start(dmac, addr);
                }
            } else {
                // if (c == &dmac->channels[VIF1]) {
                //     iris_fatal_error(dmac, "channel {} value={:08x} chcr={:08x}", get_channel_name(dmac, addr), data, c->chcr);
                // }

                c->chcr &= (data & 0x100) | 0xfffffeff;
            }
        } return;

        case 0x10: {
            c->madr = data;

            // Clear MADR's MSB on SPR channels
            if (c == &dmac->channels[SPR_TO] || c == &dmac->channels[SPR_FROM]) {
                c->madr &= 0x7fffffff;
            }
        } return;

        // Note: This right here is pretty much a hack. Crash Tag Team Racing requires
        //       TADR to be writable during VIF1 transfers.
        //       BUT, Atelier Iris requires QWC to NOT be writable during IPU transfers,
        //       otherwise it will increase QWC mid-transfer, which causes the IPU to
        //       starve of data, ultimately causing the transfer to never end.
        case 0x20: if ((c->chcr & 0x100) == 0) c->qwc = data & 0xffff; return;
        case 0x30: c->tadr = data; return;
        case 0x40: c->asr0 = data; return;
        case 0x50: c->asr1 = data; return;
        case 0x80: c->sadr = data & 0x3ff0; return;
    }

    // iris_debug(dmac, "Unknown channel register {:02x}", addr & 0xff);

    return;
}

uint64_t read8(Dmac* dmac, uint32_t addr) {
    if (addr == 0x10009000) {
        // iris_debug(dmac, "8-bit read from chcr ({:08x})", dmac->channels[VIF1].chcr & 0xff);

        return dmac->channels[VIF1].chcr & 0xff;
    }

    int shift = (addr & 0x3) * 8;

    return (read32(dmac, addr & ~0x3) >> shift) & 0xff;

    // Channel* c = get_channel(dmac, addr & ~3);

    // if (!c) {
    //     switch (addr) {
    //         case 0x1000e000: {
    //             return dmac->ctrl & 0xff;
    //         } break;
    //     }

    //     iris_debug(dmac, "Unknown channel read8 at {:08x}", addr);

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

    // iris_debug(dmac, "Unhandled 8-bit read from {:08x}", addr);

    // exit(1);

    // return 0;
}

void write8(Dmac* dmac, uint32_t addr, uint64_t data) {
    Channel* c = get_channel(dmac, addr & ~3);

    switch (addr) {
        case 0x10008000:
        case 0x10009000:
        case 0x1000a000:
        case 0x1000b000:
        case 0x1000d000:
        case 0x1000b400:
        case 0x1000d400: {
            c->chcr &= 0xffffff00;
            c->chcr |= data & 0xff;

            return;
        } break;

        case 0x10008001:
        case 0x1000d001:
        case 0x1000d401:
        case 0x10009001: {
            write32(dmac, addr & ~0x3, (read32(dmac, addr & ~0x3) & 0xffff00ff) | ((data & 0xff) << 8));
            // c->chcr &= 0xffff00ff;
            // c->chcr |= (data & 0xff) << 8;

            // if (c->chcr & 0x100) {
            //     handle_channel_start(dmac, addr);
            // }

            return;
        } break;

        case 0x1000e000: {
            dmac->ctrl &= 0xffffff00;
            dmac->ctrl |= data;

            update_mfifo_drain(dmac);
        } return;

        // ENABLEW (byte 2)
        case 0x1000f592: {
            dmac->enable &= 0xff00ffff;
            dmac->enable |= (data & 0xff) << 16;
        } return;
    }

    iris_debug(dmac, "8-bit write to {:08x} ({:02x})", addr, data);

    // exit(1);

    return;
}

uint64_t read16(Dmac* dmac, uint32_t addr) {
    int shift = (addr & 2) * 16;
    addr = addr & ~3;

    return (read32(dmac, addr) >> shift) & 0xffff;
}

void write16(Dmac* dmac, uint32_t addr, uint64_t data) {
    Channel* c = get_channel(dmac, addr & ~3);

    switch (addr) {
        case 0x10008000:
        case 0x1000a000:
        case 0x1000d000:
        case 0x1000d400:
        case 0x1000d800:
        case 0x10009000: {
            if ((c->chcr & 0x100) == 0) {
                c->chcr &= 0xffff0000;
                c->chcr |= data & 0xffff;

                if (data & 0x100) {
                    handle_channel_start(dmac, addr);
                }
            } else {
                // iris_debug(dmac, "channel {} value={:08x} chcr={:08x}", get_channel_name(dmac, addr), data, c->chcr);
                c->chcr &= (data & 0x100) | 0xfffffeff;
            }
        } return;
    }

    iris_fatal_error(dmac, "16-bit write to {:08x} ({:04x})", addr, data & 0xffff);
}

}
