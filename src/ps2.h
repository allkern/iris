#ifndef PS2_H
#define PS2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ee/bus.h"
#include "ee/ee.h"
#include "ee/gif.h"
#include "ee/vif.h"
#include "ee/dmac.h"
#include "ee/intc.h"
#include "ee/timers.h"
#include "ee/vu.h"
#include "iop/bus.h"
#include "iop/bus_decl.h"
#include "iop/iop.h"
#include "iop/dma.h"
#include "iop/intc.h"
#include "iop/timers.h"
#include "iop/cdvd.h"
#include "iop/sio2.h"
#include "iop/spu2.h"
#include "iop/usb.h"
#include "iop/fw.h"
#include "shared/bios.h"
#include "shared/ram.h"
#include "shared/sif.h"
#include "shared/sbus.h"
#include "gs/gs.h"
#include "ipu/ipu.h"

// SIO2 devices (controllers, memory cards, etc.)
#include "dev/ds.h"
#include "dev/mcd.h"
#include "dev/ps1_mcd.h"
#include "dev/mtap.h"

#include "scheduler.h"

struct ps2_elf_function {
    char* name;
    uint32_t addr;
};

struct ps2_state {
    // CPUs
    struct ee_state* ee;
    struct iop_state* iop;
    struct vu_state* vu0;
    struct vu_state* vu1;

    // EE-only
    struct ee_bus* ee_bus;
    struct ps2_gif* gif;
    struct ps2_vif* vif0;
    struct ps2_vif* vif1;
    struct ps2_gs* gs;
    struct ps2_ipu* ipu;
    struct ps2_dmac* ee_dma;
    struct ps2_ram* ee_ram;
    struct ps2_intc* ee_intc;
    struct ps2_ee_timers* ee_timers;

    // IOP-only
    struct iop_bus* iop_bus;
    struct ps2_ram* iop_spr;
    struct ps2_iop_dma* iop_dma;
    struct ps2_iop_intc* iop_intc;
    struct ps2_iop_timers* iop_timers;
    struct ps2_sio2* sio2;
    struct ps2_spu2* spu2;
    struct ps2_fw* fw;
    
    // Shared
    struct ps2_ram* iop_ram;
    struct ps2_bios* bios;
    struct ps2_bios* rom1; // Mapped to 1E000000-1E3FFFFF (DVD firmware)
    struct ps2_bios* rom2; // Mapped to 1E400000-1E7FFFFF (Chinese exts)
    struct ps2_cdvd* cdvd;
    struct ps2_sif* sif;
    struct ps2_usb* usb;
    struct ps2_sbus* sbus;

    struct sched_state* sched;

    int ee_cycles;
    int timescale;

    // Debug
    struct ps2_elf_function* func;
    unsigned int nfuncs;
    char* strtab;
};

struct ps2_state* ps2_create(void);
void ps2_init(struct ps2_state* ps2);
void ps2_init_kputchar(struct ps2_state* ps2, void (*ee_kputchar)(void*, char), void*, void (*iop_kputchar)(void*, char), void*);
void ps2_boot_file(struct ps2_state* ps2, const char* path);
void ps2_reset(struct ps2_state* ps2);
void ps2_load_bios(struct ps2_state* ps2, const char* path);
void ps2_load_rom1(struct ps2_state* ps2, const char* path);
void ps2_load_rom2(struct ps2_state* ps2, const char* path);
void ps2_cycle(struct ps2_state* ps2);
void ps2_set_timescale(struct ps2_state* ps2, int timescale);
void ps2_iop_cycle(struct ps2_state* ps2);
void ps2_destroy(struct ps2_state* ps2);

#ifdef __cplusplus
}
#endif

#endif