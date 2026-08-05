#include <new>

#include "intc.hpp"
#include "ee.hpp"
#include "scheduler.hpp"

namespace iris::ee::intc {

static inline void intc_check_irq(Intc* intc) {
    ee::set_int0(intc->hw.ee, intc->stat & intc->mask);
}

Intc* create(logger::Logger* logger, scheduler::Scheduler* sched) {
    Intc* intc = new Intc();

    intc->logger = logger;
    intc->logger_id = logger::register_source(logger, "ee_intc");

    intc->hw.sched = sched;

    return intc;
}

void connect(Intc* intc, ee::Ee* ee) {
    intc->hw.ee = ee;
}

void reset(Intc* intc) {
    auto hw = intc->hw;

    new (intc) Intc();

    intc->hw = hw;
}

void destroy(Intc* intc) {
    delete intc;
}

uint64_t read32(Intc* intc, uint32_t addr) {
    switch (addr) {
        case 0x1000f000: return intc->stat;
        case 0x1000f010: return intc->mask;

        default: iris_fatal_error(intc, "Unhandled INTC read {:08x}", addr);
    }

    return 0;
}

void write8(Intc* intc, uint32_t addr, uint64_t data) {
    iris_fatal_error(intc, "write8 {:04x}", data);

    switch (addr) {
        case 0x1000f000: intc->stat &= ~data; break;
        case 0x1000f010: intc->mask ^= data; break;

        default: iris_fatal_error(intc, "Unhandled INTC write {:08x} {:08x}", addr, data);
    }

    intc_check_irq(intc);
}

void write16(Intc* intc, uint32_t addr, uint64_t data) {
    iris_fatal_error(intc, "write16 {:04x}", data);

    switch (addr) {
        case 0x1000f000: intc->stat &= ~data; break;
        case 0x1000f010: intc->mask ^= data; break;

        default: iris_fatal_error(intc, "Unhandled INTC write {:08x} {:08x}", addr, data);
    }

    intc_check_irq(intc);
}

void intc_check_irq_event(void* udata, int overshoot) {
    Intc* intc = (Intc*)udata;

    intc_check_irq(intc);
}

void write32(Intc* intc, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1000f000: intc->stat &= ~data; break;
        case 0x1000f010: intc->mask ^= data; break;

        default: iris_fatal_error(intc, "Unhandled INTC write {:08x} {:08x}", addr, data);
    }

    // scheduler::Event event;

    // event.callback = intc_check_irq_event;
    // event.cycles = 16;
    // event.name = "INTC IRQ check";
    // event.udata = intc;

    // scheduler::schedule(intc->hw.sched, event);
    ee::reset_intc_reads(intc->hw.ee);

    ee::set_int0(intc->hw.ee, intc->stat & intc->mask);
}

void write64(Intc* intc, uint32_t addr, uint64_t data) {
    iris_fatal_error(intc, "write64 {:016x}", data);

    switch (addr) {
        case 0x1000f000: intc->stat &= ~data; break;
        case 0x1000f010: intc->mask ^= data; break;

        default: iris_fatal_error(intc, "Unhandled INTC write {:08x} {:08x}", addr, data);
    }

    intc_check_irq(intc);
}

void irq(Intc* intc, int dev) {
    intc->stat |= 1 << dev;

    static const char* dev_names[] = {
        "GS",
        "SBUS",
        "VBLANK_IN",
        "VBLANK_OUT",
        "VIF0",
        "VIF1",
        "VU0",
        "VU1",
        "IPU",
        "TIMER0",
        "TIMER1",
        "TIMER2",
        "TIMER3",
        "SFIFO",
        "VU0_WD"
    };

    ee::reset_intc_reads(intc->hw.ee);

    ee::set_int0(intc->hw.ee, intc->stat & intc->mask);
}

}
