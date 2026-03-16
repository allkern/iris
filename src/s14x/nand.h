#ifndef S14X_NAND_H
#define S14X_NAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#define S14X_NAND_CMD_READ   0x30
#define S14X_NAND_CMD_ERASE  0x60
#define S14X_NAND_CMD_WRITE  0x80
#define S14X_NAND_CMD_READID 0x90

#define S14X_NAND_PAGE_SIZE_NOECC 0x800
#define S14X_NAND_PAGE_SIZE_ECC 0x840
#define S14X_NAND_PAGES_PER_BLOCK 0x40

#define S14X_NAND_STATE_READ_BYTE0 0
#define S14X_NAND_STATE_READ_BYTE1 1
#define S14X_NAND_STATE_READ_PAGE0 2
#define S14X_NAND_STATE_READ_PAGE1 3
#define S14X_NAND_STATE_READ_PAGE2 4

#define S14X_NAND_REG_WAITFLAG 0
#define S14X_NAND_REG_ENABLE 1
#define S14X_NAND_REG_CMD 2
#define S14X_NAND_REG_OFFSET 3
#define S14X_NAND_REG_WRITE_UNLOCK 4
#define S14X_NAND_REG_OUTBYTE 8

struct s14x_nand {
    FILE* file;
    int enable;
    uint8_t cmd;
    uint8_t* buf;
    int index;
    int size;

    uint16_t byte_offset;
    uint32_t page_offset;
    int state;
};

struct s14x_nand* s14x_nand_create(void);
int s14x_nand_init(struct s14x_nand* nand);
int s14x_nand_load(struct s14x_nand* nand, const char* path);
uint64_t s14x_nand_read(struct s14x_nand* nand, uint32_t addr);
void s14x_nand_write(struct s14x_nand* nand, uint32_t addr, uint64_t data);
void s14x_nand_destroy(struct s14x_nand* nand);

#ifdef __cplusplus
}
#endif

#endif