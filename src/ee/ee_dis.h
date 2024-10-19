#ifndef EE_DIS_H
#define EE_DIS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct ee_dis_state {
    int print_address;
    int print_opcode;
    uint32_t pc;
};

char* ee_disassemble(char* buf, uint32_t opcode, struct ee_dis_state* dis_state);

#ifdef __cplusplus
}
#endif

#endif