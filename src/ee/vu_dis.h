#ifndef VU_DIS_H
#define VU_DIS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct vu_dis_state {
    int print_address;
    int print_opcode;
    uint32_t addr;
};

char* vu_disassemble_upper(char* buf, uint64_t opcode, struct vu_dis_state* s);
char* vu_disassemble_lower(char* buf, uint64_t opcode, struct vu_dis_state* s);

#ifdef __cplusplus
}
#endif

#endif