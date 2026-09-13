#pragma once

#include <cstdint>

namespace iris::profile {

enum Counter : int {
    PS2_CYCLES = 0,
    EE_DISPATCHES,
    EE_LOOKUP_MISSES,
    EE_BLOCKS_COMPILED,
    EE_STORE_INVALIDATIONS,
    EE_DMA_INVALIDATIONS,
    EE_DMA_CODE_INVALIDATIONS,
    EE_TLB_WRITES,
    EE_CACHE_FLUSHES,
    EE_CACHE_PURGES,
    EE_PAGES_DISCARDED,
    IOP_BLOCKS_COMPILED,
    IOP_STORE_INVALIDATIONS,
    IOP_EE_WRITE_INVALIDATIONS,
    IOP_CACHE_FLUSHES,
    IOP_PAGES_DISCARDED,
    VU0_PROGRAMS,
    VU1_PROGRAMS,
    VU0_RESETS,
    VU1_RESETS,
    VU0_BLOCKS_COMPILED_RUN,
    VU0_BLOCKS_INTERPRETED_RUN,
    VU1_BLOCKS_COMPILED_RUN,
    VU1_BLOCKS_INTERPRETED_RUN,
    VU0_ENTRIES_COMPILED_RUN,
    VU0_ENTRIES_INTERPRETED_RUN,
    VU1_ENTRIES_COMPILED_RUN,
    VU1_ENTRIES_INTERPRETED_RUN,
    VU_COMPILE_REQUESTS,
    VU_COMPILES_ADOPTED,
    VU_JIT_BUDGET_FLUSHES,
    VU_MICRO_UPLOADS,
    VU_MICRO_WRITES,
    VIF0_WORDS,
    VIF1_COMMAND_WORDS,
    VIF1_DIRECT_WORDS,
    VIF1_UNPACK_WORDS,
    VIF1_MPG_WORDS,
    VIF1_REGISTER_WORDS,
    VIF1_DMA_KICKS,
    VIF1_DMA_REPEATED_KICKS,
    VIF1_DMA_QWORDS,
    VIF1_DIRECT_BULK_QWORDS,
    VIF1_UNPACK_BULK_QWORDS,
    VIF1_BULK_CHECK_MISMATCHES,
    GIF_PATH1_QWORDS,
    GIF_PATH2_QWORDS,
    GIF_PATH3_QWORDS,
    COUNTER_COUNT
};

inline uint64_t counters[COUNTER_COUNT] = {};

inline void count(Counter counter) {
    counters[counter]++;
}

inline void count(Counter counter, uint64_t amount) {
    counters[counter] += amount;
}

inline void reset_counters() {
    for (uint64_t& counter : counters) {
        counter = 0;
    }
}

inline const char* counter_name(int counter) {
    switch (counter) {
        case PS2_CYCLES: return "ps2 cycle slices";
        case EE_DISPATCHES: return "ee block dispatches";
        case EE_LOOKUP_MISSES: return "ee block lookup misses";
        case EE_BLOCKS_COMPILED: return "ee blocks compiled";
        case EE_STORE_INVALIDATIONS: return "ee code pages dirtied by stores";
        case EE_DMA_INVALIDATIONS: return "ee dma qword writes to ee ram";
        case EE_DMA_CODE_INVALIDATIONS: return "ee code pages dirtied by dma writes";
        case EE_TLB_WRITES: return "ee tlb writes";
        case EE_CACHE_FLUSHES: return "ee whole cache flushes";
        case EE_CACHE_PURGES: return "ee whole cache purges";
        case EE_PAGES_DISCARDED: return "ee code pages discarded";
        case IOP_BLOCKS_COMPILED: return "iop blocks compiled";
        case IOP_STORE_INVALIDATIONS: return "iop code pages dirtied by stores";
        case IOP_EE_WRITE_INVALIDATIONS: return "iop code pages dirtied by ee writes";
        case IOP_CACHE_FLUSHES: return "iop whole cache flushes";
        case IOP_PAGES_DISCARDED: return "iop code pages discarded";
        case VU0_PROGRAMS: return "vu0 programs run";
        case VU1_PROGRAMS: return "vu1 programs run";
        case VU0_RESETS: return "vu0 resets through fbrst";
        case VU1_RESETS: return "vu1 resets through fbrst";
        case VU0_BLOCKS_COMPILED_RUN: return "vu0 blocks run compiled";
        case VU0_BLOCKS_INTERPRETED_RUN: return "vu0 blocks run interpreted";
        case VU1_BLOCKS_COMPILED_RUN: return "vu1 blocks run compiled";
        case VU1_BLOCKS_INTERPRETED_RUN: return "vu1 blocks run interpreted";
        case VU0_ENTRIES_COMPILED_RUN: return "vu0 entries run compiled";
        case VU0_ENTRIES_INTERPRETED_RUN: return "vu0 entries run interpreted";
        case VU1_ENTRIES_COMPILED_RUN: return "vu1 entries run compiled";
        case VU1_ENTRIES_INTERPRETED_RUN: return "vu1 entries run interpreted";
        case VU_COMPILE_REQUESTS: return "vu compile requests";
        case VU_COMPILES_ADOPTED: return "vu compiles served from the code cache";
        case VU_JIT_BUDGET_FLUSHES: return "vu jit flushes from the code budget";
        case VU_MICRO_UPLOADS: return "vu microprogram uploads";
        case VU_MICRO_WRITES: return "vu micro memory writes from the ee";
        case VIF0_WORDS: return "vif0 words";
        case VIF1_COMMAND_WORDS: return "vif1 command words";
        case VIF1_DIRECT_WORDS: return "vif1 direct words";
        case VIF1_UNPACK_WORDS: return "vif1 unpack words";
        case VIF1_MPG_WORDS: return "vif1 mpg words";
        case VIF1_REGISTER_WORDS: return "vif1 register words";
        case VIF1_DMA_KICKS: return "vif1 dma transfer kicks";
        case VIF1_DMA_REPEATED_KICKS: return "vif1 dma kicks at the previous start address";
        case VIF1_DMA_QWORDS: return "vif1 dma qwords";
        case VIF1_DIRECT_BULK_QWORDS: return "vif1 direct qwords taken in bulk";
        case VIF1_UNPACK_BULK_QWORDS: return "vif1 unpack qwords taken in bulk";
        case VIF1_BULK_CHECK_MISMATCHES: return "vif1 bulk unpack check mismatches";
        case GIF_PATH1_QWORDS: return "gif path1 qwords";
        case GIF_PATH2_QWORDS: return "gif path2 qwords";
        case GIF_PATH3_QWORDS: return "gif path3 qwords";
    }

    return "unknown";
}

}
