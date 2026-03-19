struct vu_state;

#ifndef VU_H
#define VU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "u128.h"
#include "vif.h"
#include "gif.h"

struct vu_reg128 {
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

struct vu_reg32 {
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

#define VU_REG_R 32
#define VU_REG_I 33
#define VU_REG_Q 34
#define VU_REG_P 35

struct vu_instruction {
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

    struct {
        int reg;
        int field;
    } dst, src[2];

    int vi_dst;
    int vi_src[2];

    void (*func)(struct vu_state* vu, const struct vu_instruction* i);
};

struct vu_state;

struct vu_state* vu_create(void);
void vu_init(struct vu_state* vu, int id, struct ps2_gif* gif, struct ps2_vif* vif, struct vu_state* vu1);
void vu_destroy(struct vu_state* vu);

// VU mem bus interface
uint64_t ps2_vu_read8(struct vu_state* vu, uint32_t addr);
uint64_t ps2_vu_read16(struct vu_state* vu, uint32_t addr);
uint64_t ps2_vu_read32(struct vu_state* vu, uint32_t addr);
uint64_t ps2_vu_read64(struct vu_state* vu, uint32_t addr);
uint128_t ps2_vu_read128(struct vu_state* vu, uint32_t addr);
void ps2_vu_write8(struct vu_state* vu, uint32_t addr, uint64_t data);
void ps2_vu_write16(struct vu_state* vu, uint32_t addr, uint64_t data);
void ps2_vu_write32(struct vu_state* vu, uint32_t addr, uint64_t data);
void ps2_vu_write64(struct vu_state* vu, uint32_t addr, uint64_t data);
void ps2_vu_write128(struct vu_state* vu, uint32_t addr, uint128_t data);
void ps2_vu_write_vi(struct vu_state* vu, int index, uint32_t value);
uint32_t ps2_vu_read_vi(struct vu_state* vu, int index);
void ps2_vu_reset(struct vu_state* vu);
void ps2_vu_decode_upper(struct vu_state* vu, uint32_t opcode);
void ps2_vu_decode_lower(struct vu_state* vu, uint32_t opcode);
void ps2_vu_execute_lower(struct vu_state* vu, uint32_t opcode);
void ps2_vu_execute_upper(struct vu_state* vu, uint32_t opcode);

void vu_cycle(struct vu_state* vu);
void vu_execute_program(struct vu_state* vu, uint32_t addr);
void vu_execute_program_tpc(struct vu_state* vu);
uint128_t* vu_get_vu_mem_ptr(struct vu_state* vu, uint32_t addr);
uint64_t* vu_get_micro_mem_ptr(struct vu_state* vu, uint32_t addr);
uint32_t vu_get_tpc(struct vu_state* vu);

#ifdef __cplusplus
}
#endif

#endif