#include "nand.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

struct s14x_nand* s14x_nand_create(void) {
    return malloc(sizeof(struct s14x_nand));
}

int s14x_nand_init(struct s14x_nand* nand) {
    memset(nand, 0, sizeof(struct s14x_nand));

    nand->buf = malloc(S14X_NAND_PAGE_SIZE_ECC);
    nand->state = S14X_NAND_STATE_READ_BYTE0;

    return 1;
}

int s14x_nand_load(struct s14x_nand* nand, const char* path) {
    nand->file = fopen(path, "rb");

    if (!nand->file) {
        return 0;
    }

    return 1;
}

void nand_handle_offset_write(struct s14x_nand* nand, uint8_t data) {
    switch (nand->state) {
        case S14X_NAND_STATE_READ_BYTE0: {
            nand->byte_offset = (nand->byte_offset & 0xff00) | data;
            nand->state++;
        } break;
        case S14X_NAND_STATE_READ_BYTE1: {
            nand->byte_offset = (nand->byte_offset & 0x00ff) | (data << 8);
            nand->state++;
        } break;
        case S14X_NAND_STATE_READ_PAGE0: {
            nand->page_offset = (nand->page_offset & 0xffff00) | data;
            nand->state++;
        } break;
        case S14X_NAND_STATE_READ_PAGE1: {
            nand->page_offset = (nand->page_offset & 0xff00ff) | (data << 8);
            nand->state++;
        } break;
        case S14X_NAND_STATE_READ_PAGE2: {
            nand->page_offset = (nand->page_offset & 0x00ffff) | ((uint32_t)data << 16);
            nand->state = S14X_NAND_STATE_READ_BYTE0;
        } break;
    }
}

void nand_handle_cmd_read(struct s14x_nand* nand) {
    nand->size = S14X_NAND_PAGE_SIZE_ECC;
    nand->index = nand->byte_offset;

    if (!nand->file) {
        memset(nand->buf, 0, S14X_NAND_PAGE_SIZE_ECC);

        return;
    }

    fseek(nand->file, nand->page_offset * S14X_NAND_PAGE_SIZE_ECC, SEEK_SET);
    fread(nand->buf, 1, S14X_NAND_PAGE_SIZE_ECC, nand->file);
}

uint64_t s14x_nand_read(struct s14x_nand* nand, uint32_t addr) {
    switch (addr) {
        case S14X_NAND_REG_OUTBYTE: {
            int index = nand->index++ % nand->size;

            return nand->buf[index];
        } break;

        default: {
            // printf("s14x_nand: Unhandled register read %02x\n", addr);
        } break;
    }
}

void s14x_nand_write(struct s14x_nand* nand, uint32_t addr, uint64_t data) {
    switch (addr) {
        case S14X_NAND_REG_CMD: {
            nand->cmd = data;

            switch (nand->cmd) {
                case 0: {
                    // NOP
                } break;

                case S14X_NAND_CMD_READ: {
                    nand_handle_cmd_read(nand);
                } break;

                default: {
                    printf("s14x_nand: Unhandled command %02x\n", nand->cmd);

                    exit(1);
                } break;
            }
        } break;

        case S14X_NAND_REG_OFFSET: {
            nand_handle_offset_write(nand, data);
        } break;

        default: {
            // printf("s14x_nand: Unhandled register write %02x = %02x\n", addr, (uint8_t)data);
        } break;
    }
}

void s14x_nand_destroy(struct s14x_nand* nand) {
    if (nand->buf) free(nand->buf);

    free(nand);
}