#pragma once

#include <cstdint>

namespace iris {

enum class BreakpointCpu : int {
    EE,
    IOP
};

struct Breakpoint {
    uint32_t addr;
    const char* symbol = nullptr;
    BreakpointCpu cpu;
    bool cond_r, cond_w, cond_x;
    int size;
    bool enabled;
};

}
