#pragma once

#include <vector>
#include <asmjit/ujit.h>

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
    int dst = 0;
    int src1 = 0;
    int src2 = 0;

    // 0 - no branch
    // 1 - normal branch
    // 2 - immediate branch
    int branch = 0;

    void (*func)(struct iop_state*, iop_instruction&) = nullptr;
};

typedef void (*iop_compiled_block)(struct iop_state*);

struct iop_block {
    std::vector <iop_instruction> instructions;
    uint32_t cycles = 0;
    uint32_t start_pc = 0;
    uint32_t end_pc = 0;
    iop_compiled_block func = nullptr;
};

struct iop_cache_page {
    iop_block* blocks;
    uint32_t min_code_addr;
    uint32_t max_code_addr;
    bool valid;
    bool dirty;
};

#ifndef _IOP_CACHE_PAGESIZE
#define _IOP_CACHE_PAGESIZE 512
#endif

#define IOP_CACHE_PAGECOUNT (0x20000000u / _IOP_CACHE_PAGESIZE)

struct iop_cached_reg {
    asmjit::ujit::Gp reg;
    bool valid = false;
    bool dirty = false;
    bool constant = false;
    uint32_t value = 0;
};

struct iop_state {
    uint32_t r[32] = { 0 };
    uint32_t hi = 0, lo = 0;
    uint32_t opcode = 0;
    uint32_t pc = 0, next_pc = 0, saved_pc = 0;
    uint32_t load_d = 0, load_v = 0;
    uint32_t last_cycles = 0;
    uint64_t total_cycles = 0;
    uint32_t biu_config = 0;
    int branch = 0, delay_slot = 0, branch_taken = 0;
    uint32_t cop0_r[16] = { 0 };

    struct iop_bus_s bus = { nullptr };

    iop_cache_page block_cache[IOP_CACHE_PAGECOUNT] = { nullptr };

    iop_block* last_cached_block = nullptr;
    uint32_t last_cached_block_pc = 0;
    uint32_t executing_cache_page = 0xffffffff;
    uint32_t deferred_invalidate_page = 0xffffffff;

    void (*kputchar)(void*, char) = nullptr;
    void* kputchar_udata = nullptr;
    void (*sm_putchar)(void*, char) = nullptr;
    void* sm_putchar_udata = nullptr;

    // ASMJIT stuff
    asmjit::JitRuntime rt;
    asmjit::CodeHolder code;
    asmjit::FileLogger* logger;
    asmjit::ujit::BackendCompiler* bc;
    asmjit::ujit::UniCompiler* uc;
    asmjit::ujit::Gp iop_ptr;
    iop_cached_reg reg_cache[32];
    bool load_pending = false;
    bool load_pending_reg_known = false;
    int load_pending_reg = 0;

    uint32_t module_list_addr = 0;
    uint32_t thread_list_addr = 0;
    
    /* cache module list */
    int module_count = 0;
    struct iop_module* module_list = nullptr;
};

#define TSW_IOP_NONE 0x0
#define TSW_IOP_SLEEP 0x1
#define TSW_IOP_DELAY 0x2
#define TSW_IOP_SEMA 0x3
#define TSW_IOP_EVENTFLAG 0x4
#define TSW_IOP_MBX 0x5
#define TSW_IOP_VPL 0x6
#define TSW_IOP_FPL 0x7

struct iop_thread_ctx {
    uint32_t unk;
    uint32_t at;
    uint32_t v0;
    uint32_t v1;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t t7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t t8;
    uint32_t t9;
    uint32_t unk68;
    uint32_t unk6c;
    uint32_t gp;
    uint32_t sp;
    uint32_t fp;
    uint32_t ra;
    uint32_t hi;
    uint32_t lo;
    uint32_t sr;
    uint32_t pc;
    uint32_t I_CTRL;
    uint32_t unk2;
};

struct iop_thread {
    /* Links for priority list */
    uint32_t next;
    uint32_t prev;

    uint16_t tag;
    uint16_t id;
    uint8_t status;
    uint16_t priority;
    uint32_t reg_storage; // iop_thread_ctx*
    int unk14;
    int unk18;
    uint16_t wait_type;
    uint16_t wakeup_count;
    uint32_t wait_id; // ptr to wait object
    uint32_t next_thread; // iop_thread*
    uint32_t event_bits;
    uint16_t event_mode;
    uint16_t init_prio;
    uint32_t run_clocks_hi;
    uint32_t run_clocks_lo;
    uint32_t entry_point;
    uint32_t stack_memory;
    uint32_t stack_size;
    uint32_t gp_reg;
    uint32_t attr;
    uint32_t option;
    uint32_t wait_return;
    uint32_t reason_counter;
    uint32_t irq_preempt_count;
    uint32_t thread_preempt_count;
    uint32_t release_count;
};
