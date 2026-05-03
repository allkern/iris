#pragma once

#include <cstdint>
#include <asmjit/ujit.h>

#include "shared/ram.h"

#include "u128.h"

#include "vu.h"
#include "vu_def.hpp"

#include <unordered_map>
#include <vector>

#ifdef _EE_USE_INTRINSICS
#define EE_ALIGNED16 alignas(16)
#else
#define EE_ALIGNED16
#endif

#define EE_CYC_DEFAULT 9
#define EE_CYC_BRANCH 11
#define EE_CYC_COP_DEFAULT 7
#define EE_CYC_MULT 2*8
#define EE_CYC_DIV 14*8
#define EE_CYC_MMI_MULT 3*8
#define EE_CYC_MMI_DIV 22*8
#define EE_CYC_MMI_DEFAULT 14
#define EE_CYC_FPU_MULT 4*8
#define EE_CYC_FPU_DIV 6*8
#define EE_CYC_STORE 14
#define EE_CYC_LOAD 14

enum : int {
    EE_I_SLL,
    EE_I_SRL,
    EE_I_SRA,
    EE_I_SLLV,
    EE_I_SRLV,
    EE_I_SRAV,
    EE_I_JR,
    EE_I_JALR,
    EE_I_MOVZ,
    EE_I_MOVN,
    EE_I_SYSCALL,
    EE_I_BREAK,
    EE_I_SYNC,
    EE_I_MFHI,
    EE_I_MTHI,
    EE_I_MFLO,
    EE_I_MTLO,
    EE_I_DSLLV,
    EE_I_DSRLV,
    EE_I_DSRAV,
    EE_I_MULT,
    EE_I_MULTU,
    EE_I_DIV,
    EE_I_DIVU,
    EE_I_ADD,
    EE_I_ADDU,
    EE_I_SUB,
    EE_I_SUBU,
    EE_I_AND,
    EE_I_OR,
    EE_I_XOR,
    EE_I_NOR,
    EE_I_MFSA,
    EE_I_MTSA,
    EE_I_SLT,
    EE_I_SLTU,
    EE_I_DADD,
    EE_I_DADDU,
    EE_I_DSUB,
    EE_I_DSUBU,
    EE_I_TGE,
    EE_I_TGEU,
    EE_I_TLT,
    EE_I_TLTU,
    EE_I_TEQ,
    EE_I_TNE,
    EE_I_DSLL,
    EE_I_DSRL,
    EE_I_DSRA,
    EE_I_DSLL32,
    EE_I_DSRL32,
    EE_I_DSRA32,
    EE_I_BLTZ,
    EE_I_BGEZ,
    EE_I_BLTZL,
    EE_I_BGEZL,
    EE_I_TGEI,
    EE_I_TGEIU,
    EE_I_TLTI,
    EE_I_TLTIU,
    EE_I_TEQI,
    EE_I_TNEI,
    EE_I_BLTZAL,
    EE_I_BGEZAL,
    EE_I_BLTZALL,
    EE_I_BGEZALL,
    EE_I_MTSAB,
    EE_I_MTSAH,
    EE_I_J,
    EE_I_JAL,
    EE_I_BEQ,
    EE_I_BNE,
    EE_I_BLEZ,
    EE_I_BGTZ,
    EE_I_ADDI,
    EE_I_ADDIU,
    EE_I_SLTI,
    EE_I_SLTIU,
    EE_I_ANDI,
    EE_I_ORI,
    EE_I_XORI,
    EE_I_LUI,
    EE_I_MFC0,
    EE_I_MTC0,
    EE_I_BC0F,
    EE_I_BC0T,
    EE_I_BC0FL,
    EE_I_BC0TL,
    EE_I_TLBR,
    EE_I_TLBWI,
    EE_I_TLBWR,
    EE_I_TLBP,
    EE_I_ERET,
    EE_I_EI,
    EE_I_DI,
    EE_I_MFC1,
    EE_I_CFC1,
    EE_I_MTC1,
    EE_I_CTC1,
    EE_I_BC1F,
    EE_I_BC1T,
    EE_I_BC1FL,
    EE_I_BC1TL,
    EE_I_ADDS,
    EE_I_SUBS,
    EE_I_MULS,
    EE_I_DIVS,
    EE_I_SQRTS,
    EE_I_ABSS,
    EE_I_MOVS,
    EE_I_NEGS,
    EE_I_RSQRTS,
    EE_I_ADDAS,
    EE_I_SUBAS,
    EE_I_MULAS,
    EE_I_MADDS,
    EE_I_MSUBS,
    EE_I_MADDAS,
    EE_I_MSUBAS,
    EE_I_CVTW,
    EE_I_MAXS,
    EE_I_MINS,
    EE_I_CF,
    EE_I_CEQ,
    EE_I_CLT,
    EE_I_CLE,
    EE_I_CVTS,
    EE_I_QMFC2,
    EE_I_CFC2,
    EE_I_QMTC2,
    EE_I_CTC2,
    EE_I_BC2F,
    EE_I_BC2T,
    EE_I_BC2FL,
    EE_I_BC2TL,
    EE_I_VADDX,
    EE_I_VADDY,
    EE_I_VADDZ,
    EE_I_VADDW,
    EE_I_VSUBX,
    EE_I_VSUBY,
    EE_I_VSUBZ,
    EE_I_VSUBW,
    EE_I_VMADDX,
    EE_I_VMADDY,
    EE_I_VMADDZ,
    EE_I_VMADDW,
    EE_I_VMSUBX,
    EE_I_VMSUBY,
    EE_I_VMSUBZ,
    EE_I_VMSUBW,
    EE_I_VMAXX,
    EE_I_VMAXY,
    EE_I_VMAXZ,
    EE_I_VMAXW,
    EE_I_VMINIX,
    EE_I_VMINIY,
    EE_I_VMINIZ,
    EE_I_VMINIW,
    EE_I_VMULX,
    EE_I_VMULY,
    EE_I_VMULZ,
    EE_I_VMULW,
    EE_I_VMULQ,
    EE_I_VMAXI,
    EE_I_VMULI,
    EE_I_VMINII,
    EE_I_VADDQ,
    EE_I_VMADDQ,
    EE_I_VADDI,
    EE_I_VMADDI,
    EE_I_VSUBQ,
    EE_I_VMSUBQ,
    EE_I_VSUBI,
    EE_I_VMSUBI,
    EE_I_VADD,
    EE_I_VMADD,
    EE_I_VMUL,
    EE_I_VMAX,
    EE_I_VSUB,
    EE_I_VMSUB,
    EE_I_VOPMSUB,
    EE_I_VMINI,
    EE_I_VIADD,
    EE_I_VISUB,
    EE_I_VIADDI,
    EE_I_VIAND,
    EE_I_VIOR,
    EE_I_VCALLMS,
    EE_I_VCALLMSR,
    EE_I_VADDAX,
    EE_I_VADDAY,
    EE_I_VADDAZ,
    EE_I_VADDAW,
    EE_I_VSUBAX,
    EE_I_VSUBAY,
    EE_I_VSUBAZ,
    EE_I_VSUBAW,
    EE_I_VMADDAX,
    EE_I_VMADDAY,
    EE_I_VMADDAZ,
    EE_I_VMADDAW,
    EE_I_VMSUBAX,
    EE_I_VMSUBAY,
    EE_I_VMSUBAZ,
    EE_I_VMSUBAW,
    EE_I_VITOF0,
    EE_I_VITOF4,
    EE_I_VITOF12,
    EE_I_VITOF15,
    EE_I_VFTOI0,
    EE_I_VFTOI4,
    EE_I_VFTOI12,
    EE_I_VFTOI15,
    EE_I_VMULAX,
    EE_I_VMULAY,
    EE_I_VMULAZ,
    EE_I_VMULAW,
    EE_I_VMULAQ,
    EE_I_VABS,
    EE_I_VMULAI,
    EE_I_VCLIPW,
    EE_I_VADDAQ,
    EE_I_VMADDAQ,
    EE_I_VADDAI,
    EE_I_VMADDAI,
    EE_I_VSUBAQ,
    EE_I_VMSUBAQ,
    EE_I_VSUBAI,
    EE_I_VMSUBAI,
    EE_I_VADDA,
    EE_I_VMADDA,
    EE_I_VMULA,
    EE_I_VSUBA,
    EE_I_VMSUBA,
    EE_I_VOPMULA,
    EE_I_VNOP,
    EE_I_VMOVE,
    EE_I_VMR32,
    EE_I_VLQI,
    EE_I_VSQI,
    EE_I_VLQD,
    EE_I_VSQD,
    EE_I_VDIV,
    EE_I_VSQRT,
    EE_I_VRSQRT,
    EE_I_VWAITQ,
    EE_I_VMTIR,
    EE_I_VMFIR,
    EE_I_VILWR,
    EE_I_VISWR,
    EE_I_VRNEXT,
    EE_I_VRGET,
    EE_I_VRINIT,
    EE_I_VRXOR,
    EE_I_BEQL,
    EE_I_BNEL,
    EE_I_BLEZL,
    EE_I_BGTZL,
    EE_I_DADDI,
    EE_I_DADDIU,
    EE_I_LDL,
    EE_I_LDR,
    EE_I_MADD,
    EE_I_MADDU,
    EE_I_PLZCW,
    EE_I_PADDW,
    EE_I_PSUBW,
    EE_I_PCGTW,
    EE_I_PMAXW,
    EE_I_PADDH,
    EE_I_PSUBH,
    EE_I_PCGTH,
    EE_I_PMAXH,
    EE_I_PADDB,
    EE_I_PSUBB,
    EE_I_PCGTB,
    EE_I_PADDSW,
    EE_I_PSUBSW,
    EE_I_PEXTLW,
    EE_I_PPACW,
    EE_I_PADDSH,
    EE_I_PSUBSH,
    EE_I_PEXTLH,
    EE_I_PPACH,
    EE_I_PADDSB,
    EE_I_PSUBSB,
    EE_I_PEXTLB,
    EE_I_PPACB,
    EE_I_PEXT5,
    EE_I_PPAC5,
    EE_I_PMADDW,
    EE_I_PSLLVW,
    EE_I_PSRLVW,
    EE_I_PMSUBW,
    EE_I_PMFHI,
    EE_I_PMFLO,
    EE_I_PINTH,
    EE_I_PMULTW,
    EE_I_PDIVW,
    EE_I_PCPYLD,
    EE_I_PMADDH,
    EE_I_PHMADH,
    EE_I_PAND,
    EE_I_PXOR,
    EE_I_PMSUBH,
    EE_I_PHMSBH,
    EE_I_PEXEH,
    EE_I_PREVH,
    EE_I_PMULTH,
    EE_I_PDIVBW,
    EE_I_PEXEW,
    EE_I_PROT3W,
    EE_I_MFHI1,
    EE_I_MTHI1,
    EE_I_MFLO1,
    EE_I_MTLO1,
    EE_I_MULT1,
    EE_I_MULTU1,
    EE_I_DIV1,
    EE_I_DIVU1,
    EE_I_MADD1,
    EE_I_MADDU1,
    EE_I_PABSW,
    EE_I_PCEQW,
    EE_I_PMINW,
    EE_I_PADSBH,
    EE_I_PABSH,
    EE_I_PCEQH,
    EE_I_PMINH,
    EE_I_PCEQB,
    EE_I_PADDUW,
    EE_I_PSUBUW,
    EE_I_PEXTUW,
    EE_I_PADDUH,
    EE_I_PSUBUH,
    EE_I_PEXTUH,
    EE_I_PADDUB,
    EE_I_PSUBUB,
    EE_I_PEXTUB,
    EE_I_QFSRV,
    EE_I_PMADDUW,
    EE_I_PSRAVW,
    EE_I_PMTHI,
    EE_I_PMTLO,
    EE_I_PINTEH,
    EE_I_PMULTUW,
    EE_I_PDIVUW,
    EE_I_PCPYUD,
    EE_I_POR,
    EE_I_PNOR,
    EE_I_PEXCH,
    EE_I_PCPYH,
    EE_I_PEXCW,
    EE_I_PMFHLLW,
    EE_I_PMFHLUW,
    EE_I_PMFHLSLW,
    EE_I_PMFHLLH,
    EE_I_PMFHLSH,
    EE_I_PMTHL,
    EE_I_PSLLH,
    EE_I_PSRLH,
    EE_I_PSRAH,
    EE_I_PSLLW,
    EE_I_PSRLW,
    EE_I_PSRAW,
    EE_I_LQ,
    EE_I_SQ,
    EE_I_LB,
    EE_I_LH,
    EE_I_LWL,
    EE_I_LW,
    EE_I_LBU,
    EE_I_LHU,
    EE_I_LWR,
    EE_I_LWU,
    EE_I_SB,
    EE_I_SH,
    EE_I_SWL,
    EE_I_SW,
    EE_I_SDL,
    EE_I_SDR,
    EE_I_SWR,
    EE_I_CACHE,
    EE_I_LWC1,
    EE_I_PREF,
    EE_I_LQC2,
    EE_I_LD,
    EE_I_SWC1,
    EE_I_SQC2,
    EE_I_SD,
    
    // Pseudo instructions
    EE_I_INVALID,
    EE_I_LI,
    EE_I_MAX
};

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
    int cycles;
    int id;

    void (*func)(struct ee_state*, const ee_instruction&); 
};

typedef void (*ee_compiled_block)(struct ee_state*);

struct ee_block {
    std::vector <ee_instruction> instructions;
    uint32_t cycles = 0;
    uint32_t start_pc = 0;
    uint32_t end_pc = 0;
    ee_compiled_block func;
    uint64_t hits;
};

struct ee_page {
    uint32_t pfn;
    int valid;
    int dirty;
    int spr;
    int global;
};

#define EE_VIRT_SIZE 0x100000000ull
#define EE_MIN_PAGESIZE 0x1000

struct ee_cache_page {
    ee_block* blocks;
    uint32_t min_code_addr;
    uint32_t max_code_addr;
    bool valid;
    bool dirty;
};

struct ee_state {
    EE_ALIGNED16 uint128_t r[32];
    EE_ALIGNED16 uint128_t hi;
    EE_ALIGNED16 uint128_t lo;

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

    struct ee_bus_s bus;

    ee_page pagetable[EE_VIRT_SIZE / EE_MIN_PAGESIZE];

    uint32_t block_pc;

    std::vector <ee_cache_page> block_cache;
    
    // Single-entry block cache for fast lookup (avoid hash computation)
    // Exploits temporal locality since we execute the same block repeatedly
    uint32_t last_block_lookup_pc;
    struct ee_block* last_block_ptr;

    // ASMJIT stuff
    asmjit::JitRuntime rt;
    asmjit::CodeHolder code;
    asmjit::FileLogger* logger;
    asmjit::ujit::BackendCompiler* bc;
    asmjit::ujit::UniCompiler* uc;
    asmjit::ujit::Gp ee_ptr;
    asmjit::ujit::Gp reg_cache[32];
    bool reg_is_cached[32] = { false };

    uint64_t total_cycles;

    int exception;

    int fmv_skip;
    uint32_t prev_pc;
    uint32_t pc;
    uint32_t next_pc;
    uint32_t opcode;
    uint64_t sa;
    int branch, branch_taken, delay_slot;

    struct ps2_ram* spr;

    int cpcond0;

    struct vu_state* vu0;
    struct vu_state* vu1;

    struct ee_vtlb_entry vtlb[48];
    struct ee_osd_config osd_config;

    int eenull_counter;
    int csr_reads;
    int intc_reads;
    int ram_size;

    uint32_t thread_list_base;

    // Stats
    uint64_t cache_misses;
    uint64_t cache_hits;
    uint64_t idle_skips;
};

#define THS_RUN 0x01
#define THS_READY 0x02
#define THS_WAIT 0x04
#define THS_SUSPEND 0x08
#define THS_WAITSUSPEND 0x0C // THS_WAIT | THS_SUSPEND
#define THS_DORMANT 0x10

struct ee_thread {
    uint32_t prev; // TCB*
    uint32_t next; // TCB*
    int status;
    uint32_t func; // void*
    uint32_t current_stack; // void*
    uint32_t gp_reg; // void*
    short current_priority;
    short init_priority;
    int wait_type; //0=not waiting, 1=sleeping, 2=waiting on semaphore
    int sema_id;
    int wakeup_count;
    int attr;
    int option;
    uint32_t func_; // void* ??? 
    int argc;
    uint32_t argv; // char**
    uint32_t initial_stack; // void*
    int stack_size;
    uint32_t root; // int* function to return to when exiting thread? 
    uint32_t heap_base; // void*
};