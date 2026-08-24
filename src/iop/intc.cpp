#include <new>

#include "intc.hpp"

namespace iris::iop::intc {

Intc* create(logger::Logger* logger, iop::Iop* iop) {
    Intc* intc = new Intc();

    intc->logger = logger;
    intc->logger_id = logger::register_source(logger, "iop_intc");

    intc->hw.iop = iop;

    reset(intc);

    return intc;
}

void reset(Intc* intc) {
    auto hw = intc->hw;

    logger::Logger* logger = intc->logger;
    size_t logger_id = intc->logger_id;

    new (intc) Intc();

    intc->logger = logger;
    intc->logger_id = logger_id;

    intc->hw = hw;

    intc->ctrl = 1;
}

void irq(Intc* intc, int dev) {
    intc->stat |= dev;

    iop::set_irq_pending(intc->hw.iop, intc->ctrl && (intc->stat & intc->mask));
}

void destroy(Intc* intc) {
    delete intc;
}

uint64_t read8(Intc* intc, uint32_t addr) {
    iris_fatal_error(intc, "IOP intc 8-bit read from address {:08x}", addr);

    return 0;
}

uint64_t read16(Intc* intc, uint32_t addr) {
    iris_fatal_error(intc, "IOP intc 16-bit read from address {:08x}", addr);

    return 0;
}

uint64_t read32(Intc* intc, uint32_t addr) {
    uint32_t ctrl = intc->ctrl;

    switch (addr) {
        case 0x1f801070: return intc->stat;
        case 0x1f801074: return intc->mask;
        case 0x1f801078: intc->ctrl = 0; break;
    }

    iop::set_irq_pending(intc->hw.iop, intc->ctrl && (intc->stat & intc->mask));

    return ctrl;
}

void write8(Intc* intc, uint32_t addr, uint64_t data) {
    iris_fatal_error(intc, "iop: IOP INTC 8-bit write to address {:08x} ({:02x})", addr, data);
}

void write16(Intc* intc, uint32_t addr, uint64_t data) {
    iris_fatal_error(intc, "iop: IOP INTC 8-bit write to address {:08x} ({:04x})", addr, data);
}

void write32(Intc* intc, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1f801070: intc->stat &= data; break;
        case 0x1f801074: intc->mask = data; break;
        case 0x1f801078: intc->ctrl = data; break;
    }

    iop::set_irq_pending(intc->hw.iop, intc->ctrl && (intc->stat & intc->mask));
}

}
