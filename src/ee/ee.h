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

#define FPU_FLG_C 0x00800000
#define FPU_FLG_I 0x00020000
#define FPU_FLG_D 0x00010000
#define FPU_FLG_O 0x00008000
#define FPU_FLG_U 0x00004000
#define FPU_FLG_SI 0x00000040
#define FPU_FLG_SD 0x00000020
#define FPU_FLG_SO 0x00000010
#define FPU_FLG_SU 0x00000008

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

struct ee_state;

struct ee_state* ee_create(void);
void ee_init(struct ee_state* ee, struct vu_state* vu0, struct vu_state* vu1, struct ee_bus_s bus);
void ee_reset(struct ee_state* ee);
void ee_destroy(struct ee_state* ee);
void ee_set_int0(struct ee_state* ee, int v);
void ee_set_int1(struct ee_state* ee, int v);
void ee_set_cpcond0(struct ee_state* ee, int v);
uint32_t ee_get_pc(struct ee_state* ee);
struct ps2_ram* ee_get_spr(struct ee_state* ee);
int ee_run_block(struct ee_state* ee, int cycles);
void ee_set_fmv_skip(struct ee_state* ee, int v);
void ee_reset_intc_reads(struct ee_state* ee);
void ee_reset_csr_reads(struct ee_state* ee);

#undef EE_ALIGNED16

#ifdef __cplusplus
}
#endif

#endif