#pragma once

#include "../iop.hpp"
#include "logger.hpp"

namespace iris::iop::hle::loadcore {

struct Module {
    char name[64];
    uint16_t version;
    uint32_t text_addr;
    uint32_t entry;
    uint32_t gp;
    uint32_t text_size;
    uint32_t data_size;
    uint32_t bss_size;
};

int reg_lib_ent(iop::Iop* iop);
void refresh_module_list(iop::Iop* iop);



}
