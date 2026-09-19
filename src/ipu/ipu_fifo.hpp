#pragma once
#include <cstdint>
#include <queue>
#include <bit>

#include "u128.h"

namespace iris::ipu {

struct IPU_FIFO
{
    std::deque<uint128_t> f;
    int bit_pointer;
    uint64_t cached_bits;
    bool bit_cache_dirty;
    uint32_t fifo_word(int byte_offset) const;
    bool get_bits(uint32_t& data, int bits);
    bool advance_stream(uint8_t amount);

    void reset();
    void byte_align();
};

inline uint32_t IPU_FIFO::fifo_word(int byte_offset) const
{
    const size_t word_index = static_cast<size_t>(byte_offset) >> 4;

    if (word_index >= f.size())
        return 0;

    return std::byteswap(f[word_index].u32[(byte_offset & 0xF) >> 2]);
}

inline bool IPU_FIFO::get_bits(uint32_t &data, int bits)

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

        cached_bits = (static_cast<uint64_t>(fifo_word(index)) << 32) | fifo_word(index + 4);
        bit_cache_dirty = false;
    }
    const int shift = 64 - (bit_pointer % 32) - bits;
    const uint64_t mask = ~0x0ULL >> (64 - bits);
    data = (cached_bits >> shift) & mask;

    return true;
}

inline bool IPU_FIFO::advance_stream(uint8_t amount)
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

}
