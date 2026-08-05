#include "ee/intc.hpp"
#include "iop/intc.hpp"

#include "scheduler.hpp"
#include "sbus.hpp"

namespace iris::sbus {

Sbus* create(logger::Logger* logger, ee::intc::Intc* ee_intc, iop::intc::Intc* iop_intc, scheduler::Scheduler* sched) {
    Sbus* sbus = new Sbus();

    sbus->logger = logger;
    sbus->logger_id = logger::register_source(logger, "sbus");

    sbus->ee_intc = ee_intc;
    sbus->iop_intc = iop_intc;
    sbus->sched = sched;

    return sbus;
}

// Sbus holds no mutable state of its own
void reset(Sbus* sbus) {}

void destroy(Sbus* sbus) {
    delete sbus;
}

uint64_t read8(Sbus* sbus, uint32_t addr) {
    iris_fatal_error(sbus, "Unhandled 8-bit read at {:08x}", addr);

    return 0;
}

uint64_t read16(Sbus* sbus, uint32_t addr) {
    iris_fatal_error(sbus, "Unhandled 16-bit read at {:08x}", addr);

    return 0;
}

void write8(Sbus* sbus, uint32_t addr, uint64_t data) {
    iris_fatal_error(sbus, "Unhandled 8-bit write at {:08x} <- {:02x}", addr, data);
}

void write16(Sbus* sbus, uint32_t addr, uint64_t data) {
    iris_fatal_error(sbus, "Unhandled 16-bit write at {:08x} <- {:04x}", addr, data);
}

void write32(Sbus* sbus, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1f801450: {
            if (data & 2)
                ee::intc::irq(sbus->ee_intc, ee::intc::SBUS);
        } return;
    }
}

}
