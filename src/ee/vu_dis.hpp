#pragma once
#include <cstdint>

namespace iris::vu::dis {

struct Dis {
    int print_address;
    int print_opcode;
    uint32_t addr;
};

char* disassemble_upper(char* buf, uint64_t opcode, Dis* s);
char* disassemble_lower(char* buf, uint64_t opcode, Dis* s, int ibit);

}
