#include <new>

#include "iop/dma.hpp"
#include "dmac.hpp"
#include "gif.hpp"
#include "vif.hpp"
#include "bus.hpp"
#include <cassert>

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
    int chirq = (dmac->stat & 0x3ff) & ((dmac->stat >> 16) & 0x3ff);

    ee::set_int1(dmac->hw.ee, chirq || meis);
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

    if (dmac->channels[VIF0].tag.id == 1) {
        dmac->channels[VIF0].tadr = dmac->channels[VIF0].madr;

        // iris_debug(dmac, "vif0 tag id=1, setting tadr to {:08x}", dmac->channels[VIF0].tadr);
        // exit(1);
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

inline constexpr auto MFIFO_STEP_LIMIT = 0x10000;

static inline uint32_t mfifo_wrap(Dmac* dmac, uint32_t addr) {
    return dmac->rbor | (addr & dmac->rbsr);
}

static inline int mfifo_tag_is_ref(int id) {
    return id == 0 || id == 3 || id == 4;
}

static uint32_t mfifo_available(Dmac* dmac, uint32_t drain) {
    uint32_t write = dmac->channels[SPR_FROM].madr;

    if (drain <= write)
        return (write - drain) >> 4;

    uint32_t limit = dmac->rbor + dmac->rbsr + 16;

    return ((write - dmac->rbor) + (limit - drain)) >> 4;
}

static void mfifo_push_qword(Dmac* dmac, Channel* c, uint128_t q) {
    if (c != &dmac->channels[VIF1]) {
        gif::fifo_write(dmac->hw.gif, q, gif::PATH3);

        return;
    }

    for (int i = 0; i < 4; i++) {
        vif::fifo_write(dmac->hw.vif1, q.u32[i]);
    }
}

static void mfifo_push_tag(Dmac* dmac, Channel* c) {
    if (c != &dmac->channels[VIF1]) {
        uint128_t q = { 0 };

        q.u64[0] = c->tag.data;

        gif::fifo_write(dmac->hw.gif, q, gif::PATH3);

        return;
    }

    vif::fifo_write(dmac->hw.vif1, c->tag.data & 0xffffffff);
    vif::fifo_write(dmac->hw.vif1, c->tag.data >> 32);
}

static void mfifo_end(Dmac* dmac, Channel* c) {
    set_irq(dmac, c == &dmac->channels[VIF1] ? VIF1 : GIF);

    c->chcr &= ~0x100;
    c->qwc = 0;
}

static int mfifo_step(Dmac* dmac) {
    Channel* c = dmac->mfifo_drain;

    if (!c || !(c->chcr & 0x100))
        return 0;

    if (c->qwc) {
        int in_ring = !mfifo_tag_is_ref(c->tag.id);

        if (in_ring && !mfifo_available(dmac, c->madr))
            return 0;

        mfifo_push_qword(dmac, c, read_qword(dmac, c->madr));

        c->madr += 16;

        if (in_ring)
            c->madr = mfifo_wrap(dmac, c->madr);

        c->qwc--;

        if (c->qwc)
            return 1;
    }

    if (channel_is_done(c)) {
        mfifo_end(dmac, c);

        return 0;
    }

    if (c->tag.id == 1)
        c->tadr = mfifo_wrap(dmac, c->madr);

    if (!mfifo_available(dmac, c->tadr)) {
        set_irq(dmac, MEIS);

        return 0;
    }

    process_source_tag(dmac, c, read_qword(dmac, c->tadr));

    if ((c->chcr >> 6) & 1)
        mfifo_push_tag(dmac, c);

    c->tadr = mfifo_wrap(dmac, c->tadr);

    if (!mfifo_tag_is_ref(c->tag.id))
        c->madr = mfifo_wrap(dmac, c->madr);

    return 1;
}

void mfifo_run(Dmac* dmac) {
    int guard = MFIFO_STEP_LIMIT;

    while (guard-- && mfifo_step(dmac));

    if (guard <= 0)
        iris_warning(dmac, "MFIFO drain hit the step limit, tag chain may be looping");
}

void send_vif1_read_irq(void* udata, int overshoot) {
    Dmac* dmac = (Dmac*)udata;

    end_transfer(dmac, VIF1);
}

void handle_vif1_read_transfer(Dmac* dmac) {
    // Gran Turismo 3 sends a VIF1 read with QWC=0, presumably to
    // wait until the GIF FIFO is actually full, so we shouldn't send
    // an interrupt there.
    if (dmac->channels[VIF1].qwc == 0)
        return;

    iris_debug(dmac, "Handling VIF1 read transfer with QWC={} MADR={:08x}", dmac->channels[VIF1].qwc, dmac->channels[VIF1].madr);

    // uint32_t qwc = dmac->channels[VIF1].qwc;

    // dmac->channels[VIF1].chcr &= ~0x100;
    // dmac->channels[VIF1].madr += dmac->channels[VIF1].qwc * 16;
    // dmac->channels[VIF1].qwc = 0;

    // Note: Huge Gran Turismo 4 hack, it sends a VIF1 read transfer and crashes if an interrupt is sent!
    //       Works for Gran Turismo 4, Armored Core 2/3 and Ibara.
    if (dmac->channels[VIF1].qwc == 32773) {
        dmac->channels[VIF1].chcr &= ~0x100;
        dmac->channels[VIF1].madr += dmac->channels[VIF1].qwc * 16;
        dmac->channels[VIF1].qwc = 0;

        return;
    }

    // if (qwc >= 0x4000 && qwc < 0xe000)
    //     return;

    // Trash GS readback implementation, whatever...
    // uint128_t* buf = (uint128_t*)malloc(qwc * 16);

    // dmac->hw.gif->readback(dmac->hw.gif, buf, qwc * 16);

    // for (int i = 0; i < qwc; i++) {
    //     uint128_t q = { 0 };

    //     write_qword(dmac, dmac->channels[VIF1].madr, 0, q);

    //     dmac->channels[VIF1].madr += 16;
    // }

    scheduler::Event event;

    event.name = "vif1_read_transfer_end";
    event.callback = send_vif1_read_irq;
    event.cycles = dmac->channels[VIF1].qwc * 2;
    event.udata = dmac;

    scheduler::schedule(dmac->hw.sched, event);

    end_transfer(dmac, VIF1);
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
        uint32_t w = read_word(dmac, dmac->channels[VIF1].madr);

        vif::fifo_write(dmac->hw.bus->vif1, w);

        dmac->channels[VIF1].madr += 4;
        dmac->channels[VIF1].index++;

        if (dmac->channels[VIF1].index == 4) {
            dmac->channels[VIF1].index = 0;
            dmac->channels[VIF1].qwc--;
        }

        return 1;
    }

    if (channel_is_done(&dmac->channels[VIF1])) {
        end_transfer(dmac, VIF1);

        iris_warning(dmac, "vif1 transfer done");

        return 0;
    }

    if (dmac->channels[VIF1].tag.id == 1) {
        dmac->channels[VIF1].tadr = dmac->channels[VIF1].madr;

        // iris_fatal_error(dmac, "vif1 tag id=1, setting tadr to {:08x}", dmac->channels[VIF1].tadr);
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

void handle_vif1_transfer(Dmac* dmac) {
    if ((dmac->channels[VIF1].chcr & 0x100) == 0)
        return;

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
        mfifo_run(dmac);

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

    while (transfer_vif1_word(dmac)) {
        // Transfer words until we run out of data or DREQ is cleared
    }
}

void send_gif_irq(void* udata, int overshoot) {
    Dmac* dmac = (Dmac*)udata;

    set_irq(dmac, GIF);

    dmac->channels[GIF].chcr &= ~0x100;
    dmac->channels[GIF].qwc = 0;
}

void handle_gif_transfer(Dmac* dmac) {
    scheduler::Event event;

    int mode = (dmac->channels[GIF].chcr >> 2) & 3;

    // iris_debug(dmac, "GIF DMA dir={} mode={} tte={} tie={} qwc={} madr={:08x} tadr={:08x}", //     dmac->channels[GIF].chcr & 1,
    //     (dmac->channels[GIF].chcr >> 2) & 3,
    //     (dmac->channels[GIF].chcr >> 6) & 1,
    //     (dmac->channels[GIF].chcr >> 7) & 1,
    //     dmac->channels[GIF].qwc,
    //     dmac->channels[GIF].madr,
    //     dmac->channels[GIF].tadr,
    //     dmac->rbor,
    //     dmac->rbsr,
    //     dmac->channels[SPR_FROM].madr
    //);

    int mfifo_drain = (dmac->ctrl >> 2) & 3;

    if (mfifo_drain == 3) {
        mfifo_run(dmac);

        return;
    }

    // iris_debug(dmac, "ee: GIF DMA dir={} mode={} tte={} tie={} qwc={} madr={:08x} tadr={:08x}", //     dmac->channels[GIF].chcr & 1,
    //     (dmac->channels[GIF].chcr >> 2) & 3,
    //     (dmac->channels[GIF].chcr >> 6) & 1,
    //     (dmac->channels[GIF].chcr >> 7) & 1,
    //     dmac->channels[GIF].qwc,
    //     dmac->channels[GIF].madr,
    //     dmac->channels[GIF].tadr
    //);

    for (int i = 0; i < dmac->channels[GIF].qwc; i++) {
        uint128_t q = read_qword(dmac, dmac->channels[GIF].madr);

        // fprintf(file, "ee: Sending %016lx%016lx from %08x to GIF FIFO (burst)\n",
        //     q.u64[1], q.u64[0],
        //     dmac->channels[GIF].madr
        // );

        // GIF FIFO address
        gif::fifo_write(dmac->hw.bus->gif, q, gif::PATH3);

        dmac->channels[GIF].madr += 16;
    }

    if (dmac->channels[GIF].tag.end) {
        end_transfer(dmac, GIF);

        return;
    }

    // int id = (dmac->channels[GIF].chcr >> 28) & 7;

    // if ((mode == 1) && (id == 0 || id == 7) && dmac->channels[GIF].qwc) {
    //     return;
    // }

    // Chain mode
    do {
        uint128_t tag = read_qword(dmac, dmac->channels[GIF].tadr);

        process_source_tag(dmac, &dmac->channels[GIF], tag);

        // iris_debug(dmac, "ee: gif tag qwc={:08x} madr={:08x} tadr={:08x} mem={}", dmac->channels[GIF].qwc, dmac->channels[GIF].madr, dmac->channels[GIF].tadr, dmac->channels[GIF].tag.mem);

        for (int i = 0; i < dmac->channels[GIF].qwc; i++) {
            uint128_t q = read_qword(dmac, dmac->channels[GIF].madr);

            // fprintf(file, "ee: Sending %016lx%016lx from %08x to GIF FIFO (chain)\n",
            //     q.u64[1], q.u64[0],
            //     dmac->channels[GIF].madr
            // );

            gif::fifo_write(dmac->hw.bus->gif, q, gif::PATH3);

            dmac->channels[GIF].madr += 16;
        }

        if (dmac->channels[GIF].tag.id == 1) {
            dmac->channels[GIF].tadr = dmac->channels[GIF].madr;
        }
    } while (!channel_is_done(&dmac->channels[GIF]));

    end_transfer(dmac, GIF);
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

            spr->madr = mfifo_wrap(dmac, spr->madr + 0x10);
            spr->sadr = (spr->sadr + 0x10) & 0x3ff0;
        }

        spr->qwc = 0;

        mfifo_run(dmac);

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
        case 0xA000: handle_gif_transfer(dmac); return;
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

void write32(Dmac* dmac, uint32_t addr, uint64_t data) {
    Channel* c = get_channel(dmac, addr);

    switch (addr) {
        case 0x1000E000: {
            dmac->ctrl = data;

            int mfifo_drain = (dmac->ctrl >> 2) & 3;
            int stall_ctrl = (dmac->ctrl >> 4) & 3;
            int stall_drain = (dmac->ctrl >> 6) & 3;

            if (mfifo_drain || stall_ctrl || stall_drain) {
                // iris_debug(dmac, "32-bit mfifo_drain={} stall_ctrl={} stall_drain={}", //     mfifo_drain, stall_ctrl, stall_drain
                //);

                switch (mfifo_drain) {
                    case 0: dmac->mfifo_drain = NULL; break;
                    case 2: dmac->mfifo_drain = &dmac->channels[VIF1]; break;
                    case 3: dmac->mfifo_drain = &dmac->channels[GIF]; break;
                    default: iris_fatal_error(dmac, "Invalid MFIFO drain channel {}", mfifo_drain);
                }
            }
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

            int mfifo_drain = (dmac->ctrl >> 2) & 3;
            int stall_ctrl = (dmac->ctrl >> 4) & 3;
            int stall_drain = (dmac->ctrl >> 6) & 3;

            if (mfifo_drain || stall_ctrl || stall_drain) {
                // iris_debug(dmac, "8-bit mfifo_drain={} stall_ctrl={} stall_drain={}", //     mfifo_drain, stall_ctrl, stall_drain
                //);

                switch (mfifo_drain) {
                    case 0: dmac->mfifo_drain = NULL; break;
                    case 2: dmac->mfifo_drain = &dmac->channels[VIF1]; break;
                    case 3: dmac->mfifo_drain = &dmac->channels[GIF]; break;
                    default: iris_fatal_error(dmac, "Invalid MFIFO drain channel {}", mfifo_drain);
                }
            }
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
