#ifndef PS2_H
#define PS2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ee/bus.h"
#include "ee/ee.h"
#include "ee/gif.h"
#include "ee/gs.h"
#include "ee/dmac.h"
#include "ee/intc.h"
#include "ee/timers.h"
#include "iop/bus.h"
#include "iop/bus_decl.h"
#include "iop/iop.h"
#include "iop/dma.h"
#include "iop/intc.h"
#include "iop/timers.h"
#include "iop/cdvd.h"
#include "shared/bios.h"
#include "shared/ram.h"
#include "shared/sif.h"

#include "sched.h"

struct ps2_elf_function {
    char* name;
    uint32_t addr;
};

struct ps2_state {
    // CPUs
    struct ee_state* ee;
    struct iop_state* iop;

    // EE-only
    struct ee_bus* ee_bus;
    struct ps2_gif* gif;
    struct ps2_gs* gs;
    struct ps2_dmac* ee_dma;
    struct ps2_ram* ee_ram;
    struct ps2_intc* ee_intc;
    struct ps2_ee_timers* ee_timers;

    // IOP-only
    struct iop_bus* iop_bus;
    struct ps2_iop_dma* iop_dma;
    struct ps2_iop_intc* iop_intc;
    struct ps2_iop_timers* iop_timers;
    struct ps2_cdvd* cdvd;

    // Shared
    struct ps2_ram* iop_ram;
    struct ps2_bios* bios;
    struct ps2_sif* sif;

    struct sched_state* sched;

    int ee_cycles;

    // Debug
    struct ps2_elf_function* func;
    unsigned int nfuncs;
    char* strtab;

};

struct ps2_state* ps2_create(void);
void ps2_init(struct ps2_state* ps2);
void ps2_init_kputchar(struct ps2_state* ps2, void (*ee_kputchar)(void*, char), void*, void (*iop_kputchar)(void*, char), void*);
void ps2_reset(struct ps2_state* ps2);
void ps2_load_bios(struct ps2_state* ps2, const char* path);
void ps2_cycle(struct ps2_state* ps2);
void ps2_iop_cycle(struct ps2_state* ps2);
void ps2_destroy(struct ps2_state* ps2);

#ifdef __cplusplus
}
#endif

#endif