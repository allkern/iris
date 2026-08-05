#pragma once

#include <cstdint>

namespace iris::ee::dis {

struct Dis {
    int print_address;
    int print_opcode;
    int pseudo_instructions;
    uint32_t pc;
};

char* disassemble(char* buf, uint32_t opcode, Dis* dis_state);

}
