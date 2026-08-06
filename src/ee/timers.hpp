#pragma once



#include "scheduler.hpp"
#include "intc.hpp"
#include "logger.hpp"

namespace iris::ee::timers {

struct Timer {
    uint32_t counter;
    uint16_t mode;
    uint32_t compare;
    uint16_t hold;
    
    // Internal state
    int id;
    uint32_t internal;
    uint32_t delta;
    uint32_t delta_reload;
    uint32_t check_reload;
    int cycles_until_compare;
    int cycles_until_overflow;
    int cycles_until_check;
    int check_enabled;
    
    // Lazy evaluation
    uint64_t last_sync_cycle;
    
    // Mode fields
    int clks;
    int gate;
    int gats;
    int gatm;
    int zret;
    int cue;
    int cmpe;
    int ovfe;
};

struct Timers {
    struct {
        ee::intc::Intc* intc;
        scheduler::Scheduler* sched;
    } hw;

    Timer timer[4];
    uint8_t active_mask;
    uint8_t ee_cycle_phase;
    
    // Global cycle tracking for lazy evaluation
    uint64_t current_cycle;
    uint64_t scheduler_advanced_cycles;
    int irq_event_pending;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Timers* create(logger::Logger* logger, scheduler::Scheduler* sched);
void connect(Timers* timers, ee::intc::Intc* intc);
void reset(Timers* timers);
void destroy(Timers* timers);
uint64_t read16(Timers* timers, uint32_t addr);
uint64_t read32(Timers* timers, uint32_t addr);
void write32(Timers* timers, uint32_t addr, uint64_t data);
void write16(Timers* timers, uint32_t addr, uint64_t data);
void tick(Timers* timers);
void tick_cycles(Timers* timers, uint32_t cycles);
void handle_hblank(Timers* timers);
void handle_vblank_in(Timers* timers);
void handle_vblank_out(Timers* timers);

}
