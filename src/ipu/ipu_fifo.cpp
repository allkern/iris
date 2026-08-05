#include <cstdlib>
#include <cstdio>
#include "ipu_fifo.hpp"

namespace iris::ipu {

bool IPU_FIFO::get_bits(uint32_t &data, int bits)
{
    const int fifo_bits = static_cast<int>(f.size()) * 128;
    const int bits_available = fifo_bits - bit_pointer;

    if (bits_available < bits || bits_available == 0)
    {
        data = 0;
        return false;
    }

    if (bit_cache_dirty)
    {
        const int index = (bit_pointer & ~0x1F) / 8;
        const int fifo_words = static_cast<int>(f.size());
        uint64_t cache = 0;

        // MPEG bitstreams are big-endian. Build the cache byte-by-byte so we
        // do not depend on temporary stack buffers or aliasing tricks here.
        for (int i = 0; i < 8; ++i)
        {
            const int fifo_index = index + i;
            const int word_index = fifo_index >> 4;
            const int byte_index = fifo_index & 0xF;
            uint8_t byte = 0;

            if (word_index < fifo_words)
                byte = f[word_index].u8[byte_index];

            cache = (cache << 8) | byte;
        }
        bit_cache_dirty = false;
        cached_bits = cache;
    }
    const int shift = 64 - (bit_pointer % 32) - bits;
    const uint64_t mask = ~0x0ULL >> (64 - bits);
    data = (cached_bits >> shift) & mask;

    return true;
}

bool IPU_FIFO::advance_stream(uint8_t amount)
{
    if (amount > 32)
        amount = 32;
    
    //printf("Advance stream: %d + %d = %d\n", bit_pointer - amount, amount, bit_pointer);

    const int fifo_bits = static_cast<int>(f.size()) * 128;
    if ((bit_pointer + amount) > fifo_bits)
    {
        return false;
    }

    const uint32_t old_words = bit_pointer / 32;
    bit_pointer += amount;
    const uint32_t new_words = bit_pointer / 32;
    bit_cache_dirty |= (old_words != new_words);

    while (bit_pointer >= 128)
    {
        bit_pointer -= 128;
        f.pop_front();
        bit_cache_dirty = true;
    }
    return true;
}

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
