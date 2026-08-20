#pragma once

#include <cstdint>

#include "logger.hpp"
#include "scheduler.hpp"

namespace iris::ee::intc { struct Intc; }
namespace iris::iop::intc { struct Intc; }

namespace iris::sbus {

struct Sbus {
    ee::intc::Intc* ee_intc = nullptr;
    iop::intc::Intc* iop_intc = nullptr;
    scheduler::Scheduler* sched = nullptr;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Sbus* create(logger::Logger* logger, ee::intc::Intc* ee_intc, iop::intc::Intc* iop_intc, scheduler::Scheduler* sched);
void reset(Sbus* sbus);
void destroy(Sbus* sbus);

uint64_t read8(Sbus* sbus, uint32_t addr);
uint64_t read16(Sbus* sbus, uint32_t addr);
void write8(Sbus* sbus, uint32_t addr, uint64_t data);
void write16(Sbus* sbus, uint32_t addr, uint64_t data);
void write32(Sbus* sbus, uint32_t addr, uint64_t data);

}
