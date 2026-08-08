#pragma once

#include "shared/ram.hpp"

#include "u128.h"
#include "logger.hpp"

#include "vu.hpp"

namespace iris::ee {

struct Ee;

struct BusInterface {
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

inline constexpr auto SR_CU = 0xf0000000;
inline constexpr auto SR_DEV = 0x08000000;
inline constexpr auto SR_BEV = 0x04000000;
inline constexpr auto SR_CH = 0x00040000;
inline constexpr auto SR_EDI = 0x00020000;
inline constexpr auto SR_EIE = 0x00010000;
inline constexpr auto SR_IM7 = 0x00008000;
inline constexpr auto SR_BEM = 0x00001000;
inline constexpr auto SR_IM3 = 0x00000800;
inline constexpr auto SR_IM2 = 0x00000400;
inline constexpr auto SR_KSU = 0x00000018;
inline constexpr auto SR_ERL = 0x00000004;
inline constexpr auto SR_EXL = 0x00000002;
inline constexpr auto SR_IE = 0x00000001;

inline constexpr auto CAUSE_BD = 0x80000000;
inline constexpr auto CAUSE_BD2 = 0x40000000;
inline constexpr auto CAUSE_CE = 0x30000000;
inline constexpr auto CAUSE_EXC2 = 0x00070000;
inline constexpr auto CAUSE_IP7 = 0x00008000;
inline constexpr auto CAUSE_IP3 = 0x00000800;
inline constexpr auto CAUSE_IP2 = 0x00000400;
inline constexpr auto CAUSE_EXC = 0x0000007c;

inline constexpr auto CAUSE_EXC1_INT = (0 << 2);
inline constexpr auto CAUSE_EXC1_MOD = (1 << 2);
inline constexpr auto CAUSE_EXC1_TLBL = (2 << 2);
inline constexpr auto CAUSE_EXC1_TLBS = (3 << 2);
inline constexpr auto CAUSE_EXC1_ADEL = (4 << 2);
inline constexpr auto CAUSE_EXC1_ADES = (5 << 2);
inline constexpr auto CAUSE_EXC1_IBE = (6 << 2);
inline constexpr auto CAUSE_EXC1_DBE = (7 << 2);
inline constexpr auto CAUSE_EXC1_SYS = (8 << 2);
inline constexpr auto CAUSE_EXC1_BP = (9 << 2);
inline constexpr auto CAUSE_EXC1_RI = (10 << 2);
inline constexpr auto CAUSE_EXC1_CPU = (11 << 2);
inline constexpr auto CAUSE_EXC1_OV = (12 << 2);
inline constexpr auto CAUSE_EXC1_TR = (13 << 2);
inline constexpr auto CAUSE_EXC1_TLBIL = (2 << 0);
inline constexpr auto CAUSE_EXC1_TLBIS = (3 << 0);

inline constexpr auto CAUSE_EXC2_RES = (0 << 16);
inline constexpr auto CAUSE_EXC2_NMI = (1 << 16);
inline constexpr auto CAUSE_EXC2_PERFC = (2 << 16);
inline constexpr auto CAUSE_EXC2_DBG = (3 << 16);

inline constexpr auto VEC_RESET = 0xbfc00000;
inline constexpr auto VEC_TLB = 0x00000000;
inline constexpr auto VEC_COUNTER = 0x00000080;
inline constexpr auto VEC_DEBUG = 0x00000100;
inline constexpr auto VEC_COMMON = 0x00000180;
inline constexpr auto VEC_IRQ = 0x00000200;

inline constexpr auto FPU_FLG_C = 0x00800000;
inline constexpr auto FPU_FLG_I = 0x00020000;
inline constexpr auto FPU_FLG_D = 0x00010000;
inline constexpr auto FPU_FLG_O = 0x00008000;
inline constexpr auto FPU_FLG_U = 0x00004000;
inline constexpr auto FPU_FLG_SI = 0x00000040;
inline constexpr auto FPU_FLG_SD = 0x00000020;
inline constexpr auto FPU_FLG_SO = 0x00000010;
inline constexpr auto FPU_FLG_SU = 0x00000008;

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

struct VtlbEntry {
    int v0;
    int d0;
    int c0;
    uint32_t pfn0;
    int v1;
    int d1;
    int c1;
    uint32_t pfn1;
    int s;
    int asid;
    int g;
    uint32_t vpn2;
    uint32_t mask;
};

// Taken from PCSX2
struct OsdConfig {
    /** 0=enabled, 1=disabled */
    uint32_t spdif_mode : 1; /*00*/
    /** 0=4:3, 1=fullscreen, 2=16:9 */
    uint32_t screen_type : 2; /*01*/
    /** 0=rgb(scart), 1=component */
    uint32_t video_output : 1; /*03*/
    /** 0=japanese, 1=english(non-japanese) */
    /*04*/uint32_t jap_language : 1;
    /** Playstation driver settings. */
    uint32_t ps1drv_config : 8; /*05*/
    /** 0 = early Japanese OSD, 1 = OSD2, 2 = OSD2 with extended languages.
     * Early kernels cannot retain the value set in this field (Hence always 0). */
    uint32_t version : 3; /*13*/
    /** LANGUAGE_??? value */
    uint32_t language : 5; /*16*/
    /** timezone minutes offset from gmt */
    uint32_t timezone_offset : 11; /*21*/
};

union FpuReg {
    float f;
    uint32_t u32;
    int32_t s32;
};

Ee* create(logger::Logger* logger, int ram_size);
void connect(Ee* ee, vu::Vu* vu0, vu::Vu* vu1, BusInterface bus);
void reset(Ee* ee);
void destroy(Ee* ee);
void set_int0(Ee* ee, int v);
void set_int1(Ee* ee, int v);
void set_cpcond0(Ee* ee, int v);
uint32_t get_pc(Ee* ee);
ram::Ram* get_spr(Ee* ee);
int run_block(Ee* ee, int cycles);
int step(Ee* ee);
void set_fmv_skip(Ee* ee, int v);
void reset_intc_reads(Ee* ee);
void reset_csr_reads(Ee* ee);
void invalidate_block(Ee* ee, uint32_t addr);
void invalidate_range(Ee* ee, uint32_t addr, uint32_t size);
void flush_cache(Ee* ee);
void set_breakpoints(Ee* ee, const uint32_t* addrs, int count);
bool breakpoint_hit(Ee* ee);
void set_ram_size(Ee* ee, int ram_size);
void set_osd_config(Ee* ee, OsdConfig config);
OsdConfig get_osd_config(Ee* ee);

#undef EE_ALIGNED16

}
