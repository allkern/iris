#ifndef EE_H
#define EE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "shared/ram.h"

#include "u128.h"

#include "vu.h"

struct ee_bus_s {
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

#define EE_SR_CU  0xf0000000
#define EE_SR_DEV 0x08000000
#define EE_SR_BEV 0x04000000
#define EE_SR_CH  0x00040000
#define EE_SR_EDI 0x00020000
#define EE_SR_EIE 0x00010000
#define EE_SR_IM7 0x00008000
#define EE_SR_BEM 0x00001000
#define EE_SR_IM3 0x00000800
#define EE_SR_IM2 0x00000400
#define EE_SR_KSU 0x00000018
#define EE_SR_ERL 0x00000004
#define EE_SR_EXL 0x00000002
#define EE_SR_IE  0x00000001

#define EE_CAUSE_BD   0x80000000
#define EE_CAUSE_BD2  0x40000000
#define EE_CAUSE_CE   0x30000000
#define EE_CAUSE_EXC2 0x00070000
#define EE_CAUSE_IP7  0x00008000
#define EE_CAUSE_IP3  0x00000800
#define EE_CAUSE_IP2  0x00000400
#define EE_CAUSE_EXC  0x0000007c

#define CAUSE_EXC1_INT  (0 << 2)
#define CAUSE_EXC1_MOD  (1 << 2)
#define CAUSE_EXC1_TLBL (2 << 2)
#define CAUSE_EXC1_TLBS (3 << 2)
#define CAUSE_EXC1_ADEL (4 << 2)
#define CAUSE_EXC1_ADES (5 << 2)
#define CAUSE_EXC1_IBE  (6 << 2)
#define CAUSE_EXC1_DBE  (7 << 2)
#define CAUSE_EXC1_SYS  (8 << 2)
#define CAUSE_EXC1_BP   (9 << 2)
#define CAUSE_EXC1_RI   (10 << 2)
#define CAUSE_EXC1_CPU  (11 << 2)
#define CAUSE_EXC1_OV   (12 << 2)
#define CAUSE_EXC1_TR   (13 << 2)

#define CAUSE_EXC2_RES   (0 << 16)
#define CAUSE_EXC2_NMI   (1 << 16)
#define CAUSE_EXC2_PERFC (2 << 16)
#define CAUSE_EXC2_DBG   (3 << 16)

#define EE_VEC_RESET   0xbfc00000
#define EE_VEC_TLBR    0x00000000
#define EE_VEC_COUNTER 0x00000080
#define EE_VEC_DEBUG   0x00000100
#define EE_VEC_COMMON  0x00000180
#define EE_VEC_IRQ     0x00000200

/*
    1       V0 - Even page valid. When not set, the memory referenced in this entry is not mapped.
    2       D0 - Even page dirty. When not set, writes cause an exception.
    3-5     C0 - Even page cache mode.
            2=Uncached
            3=Cached
            7=Uncached accelerated
    6-25    PFN0 - Even page frame number.
    33      V1 - Odd page valid.
    34      D1 - Odd page dirty.
    35      C1 - Odd page cache mode.
    38-57   PFN1 - Odd page frame number.
    63      S - Scratchpad. When set, the virtual mapping goes to scratchpad instead of main memory.
    64-71   ASID - Address Space ID.
    76      G - Global. When set, ASID is ignored.
    77-95   VPN2 - Virtual page number / 2.
            Even pages have a VPN of (VPN2 * 2) and odd pages have a VPN of (VPN2 * 2) + 1
    109-120 MASK - Size of an even/odd page.
*/

struct ee_vtlb_entry {
    int v0;
    int d0;
    int c0;
    int pfn0;
    int v1;
    int d1;
    int c1;
    int pfn1;
    int s;
    int asid;
    int g;
    int vpn2;
    int mask;
};

union ee_fpu_reg {
    float f;
    uint32_t u32;
    int32_t s32;
};

#ifdef _EE_USE_INTRINSICS
#define EE_ALIGNED16 __attribute__((aligned(16)))
#else
#define EE_ALIGNED16
#endif

struct ee_state {
    struct ee_bus_s bus;

    uint128_t r[32] EE_ALIGNED16;
    uint128_t hi EE_ALIGNED16;
    uint128_t lo EE_ALIGNED16;

    uint64_t total_cycles;

    uint32_t prev_pc;
    uint32_t pc;
    uint32_t next_pc;
    uint32_t opcode;
    uint64_t sa;
    int branch, branch_taken, delay_slot;

    struct ps2_ram* scratchpad;

    int cpcond0;

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

    union ee_fpu_reg f[32];
    union ee_fpu_reg a;

    uint32_t fcr;

    struct vu_state* vu0;
    struct vu_state* vu1;

    struct ee_vtlb_entry vtlb[48];
};

struct ee_state* ee_create(void);
void ee_init(struct ee_state* ee, struct vu_state* vu0, struct vu_state* vu1, struct ee_bus_s bus);
void ee_cycle(struct ee_state* ee);
void ee_reset(struct ee_state* ee);
void ee_destroy(struct ee_state* ee);
void ee_set_int0(struct ee_state* ee);
void ee_set_int1(struct ee_state* ee);
void ee_set_cpcond0(struct ee_state* ee, int v);

#undef EE_ALIGNED16

#ifdef __cplusplus
}
#endif

#endif