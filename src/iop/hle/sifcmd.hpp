#pragma once

#include "../iop.hpp"
#include "logger.hpp"

namespace iris::iop::hle::sifcmd {

enum {
    HOOK_NONE,
    HOOK_REGISTER_RPC
};

void register_exports(iop::Iop* iop, uint32_t library);
int hook_for_target(iop::Iop* iop, uint32_t target);
int run_hook(iop::Iop* iop, int hook);

}
