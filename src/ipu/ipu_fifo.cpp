#include <cstdlib>
#include <cstdio>
#include "ipu_fifo.hpp"

namespace iris::ipu {

void IPU_FIFO::reset()
{
    std::deque<uint128_t> empty;
    f.swap(empty);
    bit_pointer = 0;
    cached_bits = 0;
    bit_cache_dirty = true;
}

void IPU_FIFO::byte_align()
{
    int bits = bit_pointer & 0x7;
    if (bits)
        advance_stream(8 - bits);
}

}
