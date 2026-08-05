#pragma once

#include "u128.h"
#include "logger.hpp"
#include <cstdint>

namespace iris::gif { struct Gif; }
namespace iris::vif { struct Vif; }

namespace iris::vu {

struct Vu;

inline constexpr uint32_t D_FLD = 0x01e00000;
inline constexpr uint32_t D_X   = 0x01000000;
inline constexpr uint32_t D_Y   = 0x00800000;
inline constexpr uint32_t D_Z   = 0x00400000;
inline constexpr uint32_t D_W   = 0x00200000;

struct Reg128 {
    union {
        uint128_t u128;
        uint64_t u64[2];
        uint32_t u32[4];
        int32_t s32[4];
        float f[4];

        // Named fields
        struct {
            float x;
            float y;
            float z;
            float w;
        };
    };
};

struct Reg32 {
    union {
        uint32_t u32;
        int32_t s32;
        float f;
        uint16_t u16[2];
        int16_t s16[2];
        uint8_t u8[4];
        int8_t s8[4];
    };
};

#define REG_R 32
#define REG_I 33
#define REG_Q 34
#define REG_P 35

struct Instruction {
    uint32_t ld_di[4];
    uint32_t ld_d;
    uint32_t ld_s;
    uint32_t ld_t;
    uint32_t ld_sf;
    uint32_t ld_tf;
    int32_t ld_imm5;
    int32_t ld_imm11;
    uint32_t ld_imm12;
    uint32_t ld_imm15;
    uint32_t ld_imm24;
    uint32_t ud_di[4];
    uint32_t ud_d;
    uint32_t ud_s;
    uint32_t ud_t;
    uint32_t opcode;
    int branch;

    struct {
        int reg;
        int field;
    } dst, src[2];

    int vi_dst;
    int vi_src[2];

    void (*func)(Vu* vu, const Instruction* i);
};

Vu* create(logger::Logger* logger, int id);
void connect(Vu* vu, gif::Gif* gif, vif::Vif* vif, Vu* vu1);
void destroy(Vu* vu);

// VU mem bus interface
uint64_t read8(Vu* vu, uint32_t addr);
uint64_t read16(Vu* vu, uint32_t addr);
uint64_t read32(Vu* vu, uint32_t addr);
uint64_t read64(Vu* vu, uint32_t addr);
uint128_t read128(Vu* vu, uint32_t addr);
void write8(Vu* vu, uint32_t addr, uint64_t data);
void write16(Vu* vu, uint32_t addr, uint64_t data);
void write32(Vu* vu, uint32_t addr, uint64_t data);
void write64(Vu* vu, uint32_t addr, uint64_t data);
void write128(Vu* vu, uint32_t addr, uint128_t data);
void write_vi(Vu* vu, int index, uint32_t value);
uint32_t read_vi(Vu* vu, int index);
void reset(Vu* vu);
void decode_upper(Vu* vu, uint32_t opcode);
void decode_lower(Vu* vu, uint32_t opcode);
void execute_lower(Vu* vu, uint32_t opcode);
void execute_upper(Vu* vu, uint32_t opcode);

void cycle(Vu* vu);
void execute_program(Vu* vu, uint32_t addr);
void execute_program_tpc(Vu* vu);
uint128_t* get_vu_mem_ptr(Vu* vu, uint32_t addr);
uint64_t* get_micro_mem_ptr(Vu* vu, uint32_t addr);
uint32_t get_tpc(Vu* vu);
void clear_block_cache(Vu* vu);
void invalidate_range(Vu* vu, uint32_t addr, uint32_t size);
int is_interlocked(Vu* vu);

}
