#ifndef PS2_H
#define PS2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ee/bus.h"
#include "ee/ee.h"
#include "iop/bus.h"
#include "iop/iop.h"

#include "shared/bios.h"
#include "shared/ram.h"
#include "shared/sif.h"

struct ps2_state {
    struct ee_state* ee;
    struct iop_state* iop;
    struct iop_bus* iop_bus;
    struct ee_bus* ee_bus;
    struct ps2_ram* iop_ram;
    struct ps2_bios* bios;
    struct ps2_sif* sif;
};

struct ps2_state* ps2_create(void);
void ps2_init(struct ps2_state* ps2);
void ps2_load_bios(struct ps2_state* ps2, const char* path);
void ps2_cycle(struct ps2_state* ps2);
void ps2_destroy(struct ps2_state* ps2);

#ifdef __cplusplus
}
#endif

#endif