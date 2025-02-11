// license:MIT
// copyright-holders:Lisandro Alarcon (Allkern)

/**
 * @file r3000d.c
 * @brief Disassembler for MIPS R3000A compatible code
 * @author Allkern (https://github.com/allkern)
 */

#include <stdint.h>
#include <stdio.h>

#include "iop_dis.h"

#define OP ((opcode >> 26) & 0x3f)
#define S ((opcode >> 21) & 0x1f)
#define T ((opcode >> 16) & 0x1f)
#define D ((opcode >> 11) & 0x1f)
#define IMM5 ((opcode >> 6) & 0x1f)
#define CMT ((opcode >> 6) & 0xfffff)
#define SOP (opcode & 0x3f)
#define IMM26 (opcode & 0x3ffffff)
#define IMM16 (opcode & 0xffff)
#define IMM16S ((int32_t)((int16_t)IMM16))

const char* invalid = "";

static const char* r3000_secondary_table[] = {
    "sll"    , "invalid", "srl"    , "sra"    ,
    "sllv"   , "invalid", "srlv"   , "srav"   ,
    "jr"     , "jalr"   , "invalid", "invalid",
    "syscall", "break"  , "invalid", "invalid",
    "mfhi"   , "mthi"   , "mflo"   , "mtlo"   ,
    "invalid", "invalid", "invalid", "invalid",
    "mult"   , "multu"  , "div"    , "divu"   ,
    "invalid", "invalid", "invalid", "invalid",
    "add"    , "addu"   , "sub"    , "subu"   ,
    "and"    , "or"     , "xor"    , "nor"    ,
    "invalid", "invalid", "slt"    , "sltu"   ,
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid"
};

static const char* r3000_primary_table[] = {
    "special", "bxx"    , "j"      , "jal"    ,
    "beq"    , "bne"    , "blez"   , "bgtz"   ,
    "addi"   , "addiu"  , "slti"   , "sltiu"  ,
    "andi"   , "ori"    , "xori"   , "lui"    ,
    "cop0"   , "cop1"   , "cop2"   , "cop3"   ,
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "lb"     , "lh"     , "lwl"    , "lw"     ,
    "lbu"    , "lhu"    , "lwr"    , "invalid",
    "sb"     , "sh"     , "swl"    , "sw"     ,
    "invalid", "invalid", "swr"    , "invalid",
    "lwc0"   , "lwc1"   , "lwc2"   , "lwc3"   ,
    "invalid", "invalid", "invalid", "invalid",
    "swc0"   , "swc1"   , "swc2"   , "swc3"   ,
    "invalid", "invalid", "invalid", "invalid"
};

static const char* r3000_cop0_table[] = {
    "mfc0"   , "invalid", "invalid", "invalid",
    "mtc0"   , "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "rfe"    , "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid"
};

// static const char* r3000_cop2_table[] = {
//     "mfc2"   , "invalid", "cfc2"   , "invalid",
//     "mtc2"   , "invalid", "ctc2"   , "invalid",
//     "invalid", "invalid", "invalid", "invalid",
//     "invalid", "invalid", "invalid", "invalid",
//     "gte"    , "gte"    , "gte"    , "gte"    ,
//     "gte"    , "gte"    , "gte"    , "gte"    ,
//     "gte"    , "gte"    , "gte"    , "gte"    ,
//     "gte"    , "gte"    , "gte"    , "gte"
// };

static const char* r3000_bxx_table[] = {
    "bltz"   , "bgez"   , "bltz"   , "bgez"   ,
    "bltz"   , "bgez"   , "bltz"   , "bgez"   ,
    "bltz"   , "bgez"   , "bltz"   , "bgez"   ,
    "bltz"   , "bgez"   , "bltz"   , "bgez"   ,
    "bltzal" , "bgezal" , "bltz"   , "bgez"   ,
    "bltz"   , "bgez"   , "bltz"   , "bgez"   ,
    "bltz"   , "bgez"   , "bltz"   , "bgez"   ,
    "bltz"   , "bgez"   , "bltz"   , "bgez"
};

static const char* r3000_register_names[] = {
    "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

static const char* r3000_cop0_register_names[] = {
    "Index",
    "Random",
    "EntryLo",
    "BPC",
    "Context",
    "BDA",
    "JUMPDEST",
    "DCIC",
    "BadVAddr",
    "BDAM",
    "EntryHi",
    "BPCM",
    "SR",
    "Cause",
    "EPC",
    "PRId",
    "r16",
    "r17",
    "r18",
    "r19",
    "r20",
    "r21",
    "r22",
    "r23",
    "r24",
    "r25",
    "r26",
    "r27",
    "r28",
    "r29",
    "r30",
    "r31",
    "r32",
    "r33",
    "r34",
    "r35",
    "r36",
    "r37",
    "r38",
    "r39",
    "r40",
    "r41",
    "r42",
    "r43",
    "r44",
    "r45",
    "r46",
    "r47"
};

void disassemble_secondary(char* buf, uint32_t opcode, struct iop_dis_state* state) {
    char* ptr = buf;

    int sop = SOP;

    switch (sop) {
        case 0:
        case 2:
        case 3:
            sprintf(ptr,
                "%-8s $%s, $%s, %u",
                r3000_secondary_table[sop],
                r3000_register_names[D],
                r3000_register_names[T],
                IMM5
            );
        break;
        case 4:
        case 6:
        case 7:
            sprintf(ptr,
                "%-8s $%s, $%s, $%s",
                r3000_secondary_table[sop],
                r3000_register_names[D],
                r3000_register_names[T],
                r3000_register_names[S]
            );
        break;
        case 9:
            sprintf(ptr,
                "%-8s $%s, $%s",
                r3000_secondary_table[sop],
                r3000_register_names[S],
                r3000_register_names[D]
            );
        break;
        case 12:
        case 13:
            sprintf(ptr,
                "%-8s 0x%x",
                r3000_secondary_table[sop],
                CMT
            );
        break;
        case 16:
            sprintf(ptr,
                "%-8s 0x%x",
                r3000_cop0_table[sop],
                CMT
            );
        break;
        case 18:
            sprintf(ptr,
                "%-8s $%s",
                r3000_secondary_table[sop],
                r3000_register_names[D]
            );
        break;
        case 8:
        case 17:
        case 19:
            sprintf(ptr,
                "%-8s $%s",
                r3000_secondary_table[sop],
                r3000_register_names[S]
            );
        break;
        case 24:
        case 25:
        case 26:
        case 27:
            sprintf(ptr,
                "%-8s $%s, $%s",
                r3000_secondary_table[sop],
                r3000_register_names[S],
                r3000_register_names[T]
            );
        break;
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        case 39:
        case 42:
        case 43:
            sprintf(ptr,
                "%-8s $%s, $%s, $%s",
                r3000_secondary_table[sop],
                r3000_register_names[D],
                r3000_register_names[S],
                r3000_register_names[T]
            );
        break;
        default:
            sprintf(ptr, "<invalid>");
        break;
    }
}

char* iop_disassemble(char* buf, uint32_t opcode, struct iop_dis_state* state) {
    char* ptr = buf;

    if (state) if (state->print_address)
        ptr += sprintf(ptr, "%08x: ", state->addr);

    if (state) if (state->print_opcode)
        ptr += sprintf(ptr, "%08x ", opcode);

    int op = OP;

    switch (op) {
        case 0:
            disassemble_secondary(ptr, opcode, state);
        break;
        case 1:
            sprintf(ptr,
                (IMM16S >= 0) ? "%-8s $%s, 0x%04x" : "%-8s $%s, -0x%04x",
                r3000_bxx_table[T],
                r3000_register_names[S],
                (IMM16S >= 0) ? IMM16S : ((~IMM16S) + 1)
            );
        break;
        case 2:
        case 3:
            sprintf(ptr, "%-8s 0x%08x",
                r3000_primary_table[op],
                (state ? (state->addr & 0xf0000000) : 0) |
                (IMM26 << 2)
            );
        break;
        case 4:
        case 5:
            sprintf(ptr,
                (IMM16S >= 0) ? "%-8s $%s, $%s, 0x%04x" : "%-8s $%s, $%s, -0x%04x",
                r3000_primary_table[op],
                r3000_register_names[S],
                r3000_register_names[T],
                (IMM16S >= 0 ? IMM16S : ~IMM16S) << 2
            );
        break;
        case 6:
        case 7:
            sprintf(ptr,
                (IMM16S >= 0) ? "%-8s $%s, 0x%04x" : "%-8s $%s, -0x%04x",
                r3000_primary_table[op],
                r3000_register_names[S],
                (IMM16S >= 0) ? IMM16S : ((~IMM16S) + 1)
            );
        break;
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
            sprintf(ptr, "%-8s $%s, $%s, 0x%x",
                r3000_primary_table[op],
                r3000_register_names[T],
                r3000_register_names[S],
                IMM16S
            );
        break;
        case 15:
            sprintf(ptr, "%-8s $%s, 0x%04x",
                r3000_primary_table[op],
                r3000_register_names[T],
                IMM16
            );
        break;
        case 16: {
            int s = S;

            sprintf(ptr,
                s == 16 ? "%-8s" : "%-8s $%s, $Cop0_%s",
                r3000_cop0_table[s],
                r3000_register_names[T],
                r3000_cop0_register_names[D]
            );
        } break;

        /* To-do: case 18 (COP2, GTE) */
        case 18:
            sprintf(ptr, "<gte-unimplemented>");
        break;

        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        case 40:
        case 41:
        case 42:
        case 43:
        case 46:
        case 48:
        case 49:
        case 50:
        case 51:
        case 56:
        case 57:
        case 58:
        case 59: {
            const char* fmt = "%-8s $%s, %i($%s)";

            if (state) if (state->hex_memory_offset)
                fmt = "%-8s $%s, 0x%04x($%s)";

            sprintf(ptr, fmt,
                r3000_primary_table[op],
                r3000_register_names[T],
                IMM16S,
                r3000_register_names[S]
            );
        } break;
        default:
            sprintf(ptr, "<invalid>");
        break;
    }

    return buf;
}