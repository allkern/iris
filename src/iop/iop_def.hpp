#pragma once

#include <vector>

#include "iop.h"

enum {
    IOP_I_INVALID,
    IOP_I_BLTZ,
    IOP_I_BGEZ,
    IOP_I_BLTZAL,
    IOP_I_BGEZAL,
    IOP_I_J,
    IOP_I_JAL,
    IOP_I_BEQ,
    IOP_I_BNE,
    IOP_I_BLEZ,
    IOP_I_BGTZ,
    IOP_I_ADDI,
    IOP_I_ADDIU,
    IOP_I_SLTI,
    IOP_I_SLTIU,
    IOP_I_ANDI,
    IOP_I_ORI,
    IOP_I_XORI,
    IOP_I_LUI,
    IOP_I_LB,
    IOP_I_LH,
    IOP_I_LWL,
    IOP_I_LW,
    IOP_I_LBU,
    IOP_I_LHU,
    IOP_I_LWR,
    IOP_I_SB,
    IOP_I_SH,
    IOP_I_SWL,
    IOP_I_SW,
    IOP_I_SWR,
    IOP_I_LWC0,
    IOP_I_LWC1,
    IOP_I_LWC2,
    IOP_I_LWC3,
    IOP_I_SWC0,
    IOP_I_SWC1,
    IOP_I_SWC2,
    IOP_I_SWC3,
    IOP_I_SLL,
    IOP_I_SRL,
    IOP_I_SRA,
    IOP_I_SLLV,
    IOP_I_SRLV,
    IOP_I_SRAV,
    IOP_I_JR,
    IOP_I_JALR,
    IOP_I_SYSCALL,
    IOP_I_BREAK,
    IOP_I_MFHI,
    IOP_I_MTHI,
    IOP_I_MFLO,
    IOP_I_MTLO,
    IOP_I_MULT,
    IOP_I_MULTU,
    IOP_I_DIV,
    IOP_I_DIVU,
    IOP_I_ADD,
    IOP_I_ADDU,
    IOP_I_SUB,
    IOP_I_SUBU,
    IOP_I_AND,
    IOP_I_OR,
    IOP_I_XOR,
    IOP_I_NOR,
    IOP_I_SLT,
    IOP_I_SLTU,
    IOP_I_MFC0,
    IOP_I_MTC0,
    IOP_I_RFE,
    IOP_I_MAX
};

struct iop_instruction {
    uint32_t opcode = 0;
    uint32_t id = 0;
    uint32_t rs = 0;
    uint32_t rt = 0;
    uint32_t rd = 0;
    uint32_t imm5 = 0;
    uint32_t imm16 = 0;
    int32_t imm16s = 0;
    uint32_t imm26 = 0;

    // 0 - no branch
    // 1 - normal branch
    // 2 - immediate branch
    int branch = 0;

    void (*func)(struct iop_state*, iop_instruction&) = nullptr;
};

struct iop_block {
    std::vector <iop_instruction> instructions;
    uint32_t start_pc = 0;
    uint32_t end_pc = 0;

    union {
        uint32_t cycles = 0;
        uint32_t valid;
    };
};

struct iop_state {
    struct iop_bus_s bus = { nullptr };

    std::vector <iop_block*> block_cache;
    std::vector <int> block_cache_dirty;

    iop_block* last_cached_block = nullptr;
    uint32_t last_cached_block_pc = 0;
    uint32_t executing_cache_page = 0xffffffff;
    uint32_t deferred_invalidate_page = 0xffffffff;
    
    uint32_t r[32] = { 0 };
    uint32_t opcode = 0;
    uint32_t pc = 0, next_pc = 0, saved_pc = 0;
    uint32_t hi = 0, lo = 0;
    uint32_t load_d = 0, load_v = 0;
    uint32_t last_cycles = 0;
    uint64_t total_cycles = 0;
    uint32_t biu_config = 0;
    int branch = 0, delay_slot = 0, branch_taken = 0;

    void (*kputchar)(void*, char) = nullptr;
    void* kputchar_udata = nullptr;
    void (*sm_putchar)(void*, char) = nullptr;
    void* sm_putchar_udata = nullptr;
    
    uint32_t cop0_r[16] = { 0 };

    uint32_t module_list_addr = 0;
    
    /* cache module list */
    int module_count = 0;
    struct iop_module* module_list = nullptr;
};
