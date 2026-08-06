#pragma once



#include "u128.h"
#include "scheduler.hpp"
#include "logger.hpp"

namespace iris::ee { struct Ee; }

namespace iris::ee::intc {

enum Source {
    GS,
    SBUS,
    VBLANK_IN,
    VBLANK_OUT,
    VIF0,
    VIF1,
    VU0,
    VU1,
    IPU,
    TIMER0,
    TIMER1,
    TIMER2,
    TIMER3,
    SFIFO,
    VU0_WD
};

struct Intc {
    struct {
        ee::Ee* ee;
        scheduler::Scheduler* sched;
    } hw;

    uint32_t stat;
    uint32_t mask;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Intc* create(logger::Logger* logger, scheduler::Scheduler* sched);
void connect(Intc* intc, ee::Ee* ee);
void reset(Intc* intc);
void destroy(Intc* intc);
uint64_t read32(Intc* intc, uint32_t addr);
void write8(Intc* intc, uint32_t addr, uint64_t data);
void write16(Intc* intc, uint32_t addr, uint64_t data);
void write32(Intc* intc, uint32_t addr, uint64_t data);
void write64(Intc* intc, uint32_t addr, uint64_t data);
void irq(Intc* intc, int dev);

}
