#pragma once

#include <cstdint>
#include <asmjit/ujit.h>

#include "shared/ram.hpp"

#include "u128.h"

#include "ee.hpp"
#include "vu.hpp"
#include "vu_def.hpp"

#include <unordered_map>
#include <vector>

#include "logger.hpp"

namespace iris::ee {

#ifdef _EE_USE_INTRINSICS
#define EE_ALIGNED16 alignas(16)
#else
#define EE_ALIGNED16
#endif

enum Timing : int {
    CYC_DEFAULT = 9,
    CYC_BRANCH = 11,
    CYC_COP_DEFAULT = 7,
    CYC_MULT = 2*8,
    CYC_DIV = 14*8,
    CYC_MMI_MULT = 3*8,
    CYC_MMI_DIV = 22*8,
    CYC_MMI_DEFAULT = 14,
    CYC_FPU_MULT = 4*8,
    CYC_FPU_DIV = 6*8,
    CYC_STORE = 14,
    CYC_LOAD = 14
};

enum : int {
    I_SLL,
    I_SRL,
    I_SRA,
    I_SLLV,
    I_SRLV,
    I_SRAV,
    I_JR,
    I_JALR,
    I_MOVZ,
    I_MOVN,
    I_SYSCALL,
    I_BREAK,
    I_SYNC,
    I_MFHI,
    I_MTHI,
    I_MFLO,
    I_MTLO,
    I_DSLLV,
    I_DSRLV,
    I_DSRAV,
    I_MULT,
    I_MULTU,
    I_DIV,
    I_DIVU,
    I_ADD,
    I_ADDU,
    I_SUB,
    I_SUBU,
    I_AND,
    I_OR,
    I_XOR,
    I_NOR,
    I_MFSA,
    I_MTSA,
    I_SLT,
    I_SLTU,
    I_DADD,
    I_DADDU,
    I_DSUB,
    I_DSUBU,
    I_TGE,
    I_TGEU,
    I_TLT,
    I_TLTU,
    I_TEQ,
    I_TNE,
    I_DSLL,
    I_DSRL,
    I_DSRA,
    I_DSLL32,
    I_DSRL32,
    I_DSRA32,
    I_BLTZ,
    I_BGEZ,
    I_BLTZL,
    I_BGEZL,
    I_TGEI,
    I_TGEIU,
    I_TLTI,
    I_TLTIU,
    I_TEQI,
    I_TNEI,
    I_BLTZAL,
    I_BGEZAL,
    I_BLTZALL,
    I_BGEZALL,
    I_MTSAB,
    I_MTSAH,
    I_J,
    I_JAL,
    I_BEQ,
    I_BNE,
    I_BLEZ,
    I_BGTZ,
    I_ADDI,
    I_ADDIU,
    I_SLTI,
    I_SLTIU,
    I_ANDI,
    I_ORI,
    I_XORI,
    I_LUI,
    I_MFC0,
    I_MTC0,
    I_BC0F,
    I_BC0T,
    I_BC0FL,
    I_BC0TL,
    I_TLBR,
    I_TLBWI,
    I_TLBWR,
    I_TLBP,
    I_ERET,
    I_EI,
    I_DI,
    I_MFC1,
    I_CFC1,
    I_MTC1,
    I_CTC1,
    I_BC1F,
    I_BC1T,
    I_BC1FL,
    I_BC1TL,
    I_ADDS,
    I_SUBS,
    I_MULS,
    I_DIVS,
    I_SQRTS,
    I_ABSS,
    I_MOVS,
    I_NEGS,
    I_RSQRTS,
    I_ADDAS,
    I_SUBAS,
    I_MULAS,
    I_MADDS,
    I_MSUBS,
    I_MADDAS,
    I_MSUBAS,
    I_CVTW,
    I_MAXS,
    I_MINS,
    I_CF,
    I_CEQ,
    I_CLT,
    I_CLE,
    I_CVTS,
    I_QMFC2,
    I_CFC2,
    I_QMTC2,
    I_CTC2,
    I_BC2F,
    I_BC2T,
    I_BC2FL,
    I_BC2TL,
    I_VADDX,
    I_VADDY,
    I_VADDZ,
    I_VADDW,
    I_VSUBX,
    I_VSUBY,
    I_VSUBZ,
    I_VSUBW,
    I_VMADDX,
    I_VMADDY,
    I_VMADDZ,
    I_VMADDW,
    I_VMSUBX,
    I_VMSUBY,
    I_VMSUBZ,
    I_VMSUBW,
    I_VMAXX,
    I_VMAXY,
    I_VMAXZ,
    I_VMAXW,
    I_VMINIX,
    I_VMINIY,
    I_VMINIZ,
    I_VMINIW,
    I_VMULX,
    I_VMULY,
    I_VMULZ,
    I_VMULW,
    I_VMULQ,
    I_VMAXI,
    I_VMULI,
    I_VMINII,
    I_VADDQ,
    I_VMADDQ,
    I_VADDI,
    I_VMADDI,
    I_VSUBQ,
    I_VMSUBQ,
    I_VSUBI,
    I_VMSUBI,
    I_VADD,
    I_VMADD,
    I_VMUL,
    I_VMAX,
    I_VSUB,
    I_VMSUB,
    I_VOPMSUB,
    I_VMINI,
    I_VIADD,
    I_VISUB,
    I_VIADDI,
    I_VIAND,
    I_VIOR,
    I_VCALLMS,
    I_VCALLMSR,
    I_VADDAX,
    I_VADDAY,
    I_VADDAZ,
    I_VADDAW,
    I_VSUBAX,
    I_VSUBAY,
    I_VSUBAZ,
    I_VSUBAW,
    I_VMADDAX,
    I_VMADDAY,
    I_VMADDAZ,
    I_VMADDAW,
    I_VMSUBAX,
    I_VMSUBAY,
    I_VMSUBAZ,
    I_VMSUBAW,
    I_VITOF0,
    I_VITOF4,
    I_VITOF12,
    I_VITOF15,
    I_VFTOI0,
    I_VFTOI4,
    I_VFTOI12,
    I_VFTOI15,
    I_VMULAX,
    I_VMULAY,
    I_VMULAZ,
    I_VMULAW,
    I_VMULAQ,
    I_VABS,
    I_VMULAI,
    I_VCLIPW,
    I_VADDAQ,
    I_VMADDAQ,
    I_VADDAI,
    I_VMADDAI,
    I_VSUBAQ,
    I_VMSUBAQ,
    I_VSUBAI,
    I_VMSUBAI,
    I_VADDA,
    I_VMADDA,
    I_VMULA,
    I_VSUBA,
    I_VMSUBA,
    I_VOPMULA,
    I_VNOP,
    I_VMOVE,
    I_VMR32,
    I_VLQI,
    I_VSQI,
    I_VLQD,
    I_VSQD,
    I_VDIV,
    I_VSQRT,
    I_VRSQRT,
    I_VWAITQ,
    I_VMTIR,
    I_VMFIR,
    I_VILWR,
    I_VISWR,
    I_VRNEXT,
    I_VRGET,
    I_VRINIT,
    I_VRXOR,
    I_BEQL,
    I_BNEL,
    I_BLEZL,
    I_BGTZL,
    I_DADDI,
    I_DADDIU,
    I_LDL,
    I_LDR,
    I_MADD,
    I_MADDU,
    I_PLZCW,
    I_PADDW,
    I_PSUBW,
    I_PCGTW,
    I_PMAXW,
    I_PADDH,
    I_PSUBH,
    I_PCGTH,
    I_PMAXH,
    I_PADDB,
    I_PSUBB,
    I_PCGTB,
    I_PADDSW,
    I_PSUBSW,
    I_PEXTLW,
    I_PPACW,
    I_PADDSH,
    I_PSUBSH,
    I_PEXTLH,
    I_PPACH,
    I_PADDSB,
    I_PSUBSB,
    I_PEXTLB,
    I_PPACB,
    I_PEXT5,
    I_PPAC5,
    I_PMADDW,
    I_PSLLVW,
    I_PSRLVW,
    I_PMSUBW,
    I_PMFHI,
    I_PMFLO,
    I_PINTH,
    I_PMULTW,
    I_PDIVW,
    I_PCPYLD,
    I_PMADDH,
    I_PHMADH,
    I_PAND,
    I_PXOR,
    I_PMSUBH,
    I_PHMSBH,
    I_PEXEH,
    I_PREVH,
    I_PMULTH,
    I_PDIVBW,
    I_PEXEW,
    I_PROT3W,
    I_MFHI1,
    I_MTHI1,
    I_MFLO1,
    I_MTLO1,
    I_MULT1,
    I_MULTU1,
    I_DIV1,
    I_DIVU1,
    I_MADD1,
    I_MADDU1,
    I_PABSW,
    I_PCEQW,
    I_PMINW,
    I_PADSBH,
    I_PABSH,
    I_PCEQH,
    I_PMINH,
    I_PCEQB,
    I_PADDUW,
    I_PSUBUW,
    I_PEXTUW,
    I_PADDUH,
    I_PSUBUH,
    I_PEXTUH,
    I_PADDUB,
    I_PSUBUB,
    I_PEXTUB,
    I_QFSRV,
    I_PMADDUW,
    I_PSRAVW,
    I_PMTHI,
    I_PMTLO,
    I_PINTEH,
    I_PMULTUW,
    I_PDIVUW,
    I_PCPYUD,
    I_POR,
    I_PNOR,
    I_PEXCH,
    I_PCPYH,
    I_PEXCW,
    I_PMFHLLW,
    I_PMFHLUW,
    I_PMFHLSLW,
    I_PMFHLLH,
    I_PMFHLSH,
    I_PMTHL,
    I_PSLLH,
    I_PSRLH,
    I_PSRAH,
    I_PSLLW,
    I_PSRLW,
    I_PSRAW,
    I_LQ,
    I_SQ,
    I_LB,
    I_LH,
    I_LWL,
    I_LW,
    I_LBU,
    I_LHU,
    I_LWR,
    I_LWU,
    I_SB,
    I_SH,
    I_SWL,
    I_SW,
    I_SDL,
    I_SDR,
    I_SWR,
    I_CACHE,
    I_LWC1,
    I_PREF,
    I_LQC2,
    I_LD,
    I_SWC1,
    I_SQC2,
    I_SD,
    
    // Pseudo instructions
    I_INVALID,
    I_LI,
    I_NOP,
    I_LWFIX,
    I_SWFIX,
    I_MAX
};

struct Instruction {
    uint32_t opcode;

    struct {
        int32_t r;
        bool constant;
        uint64_t value;
    } rs, rt, rd;

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

    void (*func)(Ee*, const Instruction&);
};

typedef void (*CompiledBlock)(Ee*);

struct Block {
    std::vector <Instruction> instructions;
    uint32_t cycles = 0;
    uint32_t start_pc = 0;
    uint32_t end_pc = 0;
    CompiledBlock func;
    uint64_t hits;
};

enum BlockTerm {
    TERM_FALLTHROUGH,
    TERM_BRANCH,
    TERM_LIKELY,
    TERM_EXCEPT
};

struct SubBlock {
    uint32_t start_pc;
    uint32_t end_pc;
    uint32_t cycles;
    uint32_t first;
    uint32_t count;
    BlockTerm term;

    int32_t branch_idx;

    uint32_t succ_pc[2];
    bool has_succ[2];
    int32_t succ[2];

    bool back_edge_target;
};

inline constexpr auto BLOCK_MAX_INSTRS = 256;
inline constexpr auto REGION_MAX_INSTRS = 512;
inline constexpr auto REGION_MAX_BLOCKS = 16;

struct Page {
    uint32_t pfn;
    int valid;
    int dirty;
    int spr;
    int global;
};

struct CachedReg {
    asmjit::ujit::Gp reg;
    bool valid = false;
    bool constant = false;
    uint64_t value = 0;
};

struct CachedVec {
    asmjit::ujit::Vec vec;

    bool valid = false;
    bool dirty = false;
};

inline constexpr auto VIRT_SIZE = 0x100000000ull;
inline constexpr auto MIN_PAGESIZE = 0x1000;

struct CachePage {
    Block* blocks;
    uint32_t min_code_addr;
    uint32_t max_code_addr;
    bool valid;
    bool dirty;
};

inline constexpr auto BLOCK_LUT_SIZE = 65536;
inline constexpr auto BLOCK_LUT_MASK = (BLOCK_LUT_SIZE - 1);

struct BlockLutEntry {
    uint32_t pc;
    uint32_t gen;
    Block* block;
};

struct Ee {
    EE_ALIGNED16 uint128_t r[32];
    EE_ALIGNED16 uint8_t qfsrv_buf[32];

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

    union FpuReg f[32];
    union FpuReg a;

    uint32_t fcr;

    int32_t cycles_left;
    int32_t exit_req;

    int fmv_skip;
    uint32_t prev_pc;
    uint32_t pc;
    uint32_t next_pc;
    uint32_t opcode;
    uint64_t sa;
    int branch, branch_taken, delay_slot;

    int cpcond0;

    vu::Vu* vu0;
    vu::Vu* vu1;

    BusInterface bus;

    Page pagetable[VIRT_SIZE / MIN_PAGESIZE];

    uint32_t block_pc;

    std::vector <CachePage> block_cache;
    bool pending_purge = false;

    std::vector <SubBlock> sub_blocks;

    // JIT stuff
    uint32_t last_block_lookup_pc;
    Block* last_block_ptr;
    BlockLutEntry block_lut[BLOCK_LUT_SIZE];
    uint32_t block_lut_gen;

    void** vfast_r;

    // ASMJIT stuff
    asmjit::JitRuntime rt;
    asmjit::CodeHolder code;
    asmjit::FileLogger* jit_logger;
    asmjit::ujit::BackendCompiler* bc;
    asmjit::ujit::UniCompiler* uc;
    asmjit::ujit::Gp state_ptr;
    CachedReg reg_cache[32];
    CachedVec vec_cache[32];

    uint64_t total_cycles;

    int exception;

    ram::Ram* spr;

    VtlbEntry vtlb[48];
    OsdConfig osd_config;

    int eenull_counter;
    int csr_reads;
    int intc_reads;
    int ram_size;

    uint32_t thread_list_base;

    // Stats
    uint64_t cache_misses;
    uint64_t cache_hits;
    uint64_t idle_skips;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

enum ThreadStatus : int {
    RUN = 0x01,
    READY = 0x02,
    WAIT = 0x04,
    SUSPEND = 0x08,
    WAITSUSPEND = 0x0C, // THS_WAIT | THS_SUSPEND
    DORMANT = 0x10
};

enum ThreadWaitType : int {
    NONE = 0,
    SLEEP = 1,
    SEMA = 2
};

struct ThreadCtx {
    uint32_t sa;
    uint32_t fcsr;
    uint32_t float_thing;
    uint32_t unk;
    // gpr excluding $zero
    // k0/k1 contains hi, hi1, lo, lo1
    uint128_t gpr[31];
    float fpr[32];
};

struct Thread {
    uint32_t prev; // TCB*
    uint32_t next; // TCB*
    int status;
    uint32_t resume_addr; // void*
    uint32_t register_storage; // ThreadCtx*
    uint32_t gp_reg; // void*
    short init_priority;
    short current_priority;
    int wait_type; // 0=not waiting, 1=sleeping, 2=waiting on semaphore
    int sema_id;
    int wakeup_count;
    int attr;
    int option;
    uint32_t entry_point; // void* ???
    int argc;
    uint32_t argv; // char**
    uint32_t stack_memory; // void*
    int stack_size;
    uint32_t root; // int* function to return to when exiting thread?
    uint32_t heap_base; // void*
};

}
