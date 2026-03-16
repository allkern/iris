#ifndef S14X_SRAM_H
#define S14X_SRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#define S14X_SRAM_SIZE 0x8000

struct s14x_sram {
    const char* path;
    int* write_flag;
    uint8_t buf[S14X_SRAM_SIZE];
};

struct s14x_sram* s14x_sram_create(void);
int s14x_sram_init(struct s14x_sram* sram, int* write_flag);
int s14x_sram_load(struct s14x_sram* sram, const char* path);
uint64_t s14x_sram_read8(struct s14x_sram* sram, uint32_t addr);
uint64_t s14x_sram_read16(struct s14x_sram* sram, uint32_t addr);
uint64_t s14x_sram_read32(struct s14x_sram* sram, uint32_t addr);
void s14x_sram_write8(struct s14x_sram* sram, uint32_t addr, uint64_t data);
void s14x_sram_write16(struct s14x_sram* sram, uint32_t addr, uint64_t data);
void s14x_sram_write32(struct s14x_sram* sram, uint32_t addr, uint64_t data);
void s14x_sram_destroy(struct s14x_sram* sram);

#ifdef __cplusplus
}
#endif

#endif