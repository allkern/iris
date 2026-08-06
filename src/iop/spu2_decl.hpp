#pragma once

#include <cstdint>

// Sample is what the host audio path passes around, and it needs nothing but
// <cstdint>. Keeping it here lets the frontend hold a std::vector <Sample>
// without pulling in the scheduler, intc and dma headers spu2.hpp needs.

namespace iris::spu2 {

struct Spu2;

struct Sample {
    union {
        uint32_t u32;
        uint16_t u16[2];
        int16_t s16[2];
    };
};

}
