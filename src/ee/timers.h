#ifndef EE_TIMERS_H
#define EE_TIMERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "scheduler.h"
#include "intc.h"

struct ee_timer {
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

struct ps2_ee_timers {
    struct ee_timer timer[4];

    struct ps2_intc* intc;
    struct sched_state* sched;
};

struct ps2_ee_timers* ps2_ee_timers_create(void);
void ps2_ee_timers_init(struct ps2_ee_timers* timers, struct ps2_intc* intc, struct sched_state* sched);
void ps2_ee_timers_destroy(struct ps2_ee_timers* timers);
uint64_t ps2_ee_timers_read16(struct ps2_ee_timers* timers, uint32_t addr);
uint64_t ps2_ee_timers_read32(struct ps2_ee_timers* timers, uint32_t addr);
void ps2_ee_timers_write32(struct ps2_ee_timers* timers, uint32_t addr, uint64_t data);
void ps2_ee_timers_write16(struct ps2_ee_timers* timers, uint32_t addr, uint64_t data);
void ps2_ee_timers_tick(struct ps2_ee_timers* timers);
void ps2_ee_timers_handle_hblank(struct ps2_ee_timers* timers);
void ps2_ee_timers_handle_vblank_in(struct ps2_ee_timers* timers);
void ps2_ee_timers_handle_vblank_out(struct ps2_ee_timers* timers);

#ifdef __cplusplus
}
#endif

#endif