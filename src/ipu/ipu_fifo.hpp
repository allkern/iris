#pragma once
#include <cstdint>
#include <queue>

#include "u128.h"

namespace iris::ipu {

struct IPU_FIFO
{
    std::deque<uint128_t> f;
    int bit_pointer;
    uint64_t cached_bits;
    bool bit_cache_dirty;
    bool get_bits(uint32_t& data, int bits);
    bool advance_stream(uint8_t amount);

    void reset();
    void byte_align();
};

}
