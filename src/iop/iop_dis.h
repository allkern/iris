// license:MIT
// copyright-holders:Lisandro Alarcon (Allkern)

/**
 * @file iop_dis.h
 * @brief Disassembler for MIPS R3000A (IOP) compatible code
 * @author Allkern (https://github.com/allkern)
 */

#ifndef IOP_DIS_H
#define IOP_DIS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct iop_dis_state {
    uint32_t addr;
    int print_address;
    int print_opcode;
    int hex_memory_offset;
};

/** @brief Disassemble a single opcode, printing to a buffer and
 *         optionally taking in a pointer to a disassembler state
 *         struct
 *
 *  @param buf pointer to a char buffer
 *  @param opcode opcode to disassemble
 *  @param state optional pointer to disassembler state struct
 *         (pass NULL if not required)
 *  @returns `buf`
 */
char* iop_disassemble(char* buf, uint32_t opcode, struct iop_dis_state* state);

#ifdef __cplusplus
}
#endif

#endif