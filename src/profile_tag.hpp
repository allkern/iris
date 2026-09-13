#pragma once

namespace iris::profile {

enum Jit : int {
    JIT_NONE = 0,
    JIT_EE,
    JIT_IOP,
    JIT_VU0,
    JIT_VU1
};

inline volatile int active_jit = JIT_NONE;

}
