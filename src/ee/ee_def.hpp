#pragma once

#include <cstdint>

#include "shared/ram.h"

#include "u128.h"

#include "vu.h"

#include <unordered_map>
#include <vector>

#ifdef _EE_USE_INTRINSICS
#ifdef _MSC_VER
#define EE_ALIGNED16 __declspec(align(16))
#else
#define EE_ALIGNED16 __attribute__((aligned(16)))
#endif
#else
#define EE_ALIGNED16
#endif

struct ee_instruction {
    uint32_t opcode;
    int32_t rs;
    int32_t rt;
    int32_t rd;
    int32_t sa;
    int32_t i15;
    int32_t i16;
    int32_t i26;

    // 0 - no branch
    // 1 - delayed branch
    // 2 - immediate branch
    // 3 - likely branch
    // 4 - conditional exception
    int branch;

    void (*func)(struct ee_state*, const ee_instruction&); 
};

struct ee_state {
    struct ee_bus_s bus;

    uint32_t block_pc;

    std::unordered_map <uint32_t, std::vector<ee_instruction>> block_cache;

    uint128_t r[32] EE_ALIGNED16;
    uint128_t hi EE_ALIGNED16;
    uint128_t lo EE_ALIGNED16;

    uint64_t total_cycles;

    int exception;

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