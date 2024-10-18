#ifndef EE_H
#define EE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "u128.h"
#include "ram.h"

struct ee_bus {
    void* udata;
    uint64_t (*read8)(void* udata, uint32_t addr);
    uint64_t (*read16)(void* udata, uint32_t addr);
    uint64_t (*read32)(void* udata, uint32_t addr);
    uint64_t (*read64)(void* udata, uint32_t addr);
    uint128_t (*read128)(void* udata, uint32_t addr);
    void (*write8)(void* udata, uint32_t addr, uint64_t data);
    void (*write16)(void* udata, uint32_t addr, uint64_t data);
    void (*write32)(void* udata, uint32_t addr, uint64_t data);
    void (*write64)(void* udata, uint32_t addr, uint64_t data);
    void (*write128)(void* udata, uint32_t addr, uint128_t data);
};

struct ee_state {
    struct ee_bus bus;

    uint128_t r[32];
    uint32_t pc;
    uint32_t next_pc;
    uint32_t opcode;
    uint64_t hi;
    uint64_t lo;
    uint64_t hi1;
    uint64_t lo1;
    uint32_t sa;
    int branch, branch_taken, delay_slot;
    struct ps2_ram* scratchpad;

    union {
        uint32_t cop0_r[32];
    
        struct {
            uint32_t index;
            uint32_t random;
            uint32_t entrylo0;
            uint32_t entrylo1;
            uint32_t context;
            uint32_t pagemask;
            uint32_t wired;
            uint32_t unused7;
            uint32_t badvaddr;
            uint32_t count;
            uint32_t entryhi;
            uint32_t compare;
            uint32_t status;
            uint32_t cause;
            uint32_t epc;
            uint32_t prid;
            uint32_t config;
            uint32_t unused16;
            uint32_t unused17;
            uint32_t unused18;
            uint32_t unused19;
            uint32_t unused20;
            uint32_t unused21;
            uint32_t badpaddr;
            uint32_t debug;
            uint32_t perf;
            uint32_t unused25;
            uint32_t unused26;
            uint32_t taglo;
            uint32_t taghi;
            uint32_t errorepc;
            uint32_t unused30;
            uint32_t unused31;
        };
    };
};

struct ee_state* ee_create(void);
void ee_init(struct ee_state* ee, struct ee_bus bus);
void ee_cycle(struct ee_state* ee);
void ee_destroy(struct ee_state* ee);

#ifdef __cplusplus
}
#endif

#endif