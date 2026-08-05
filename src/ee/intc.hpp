#pragma once



#include "u128.h"
#include "scheduler.hpp"
#include "logger.hpp"

namespace iris::ee { struct Ee; }

namespace iris::ee::intc {

inline constexpr auto GS = 0;
inline constexpr auto SBUS = 1;
inline constexpr auto VBLANK_IN = 2;
inline constexpr auto VBLANK_OUT = 3;
inline constexpr auto VIF0 = 4;
inline constexpr auto VIF1 = 5;
inline constexpr auto VU0 = 6;
inline constexpr auto VU1 = 7;
inline constexpr auto IPU = 8;
inline constexpr auto TIMER0 = 9;
inline constexpr auto TIMER1 = 10;
inline constexpr auto TIMER2 = 11;
inline constexpr auto TIMER3 = 12;
inline constexpr auto SFIFO = 13;
inline constexpr auto VU0_WD = 14;

struct Intc {
    // Wiring. Set by create/connect, preserved across reset.
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
