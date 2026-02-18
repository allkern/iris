#include "mcd.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define printf(fmt,...)(0)

void mcd_flush_block(struct mcd_state* mcd, int addr, int size) {
    fseek(mcd->file, addr, SEEK_SET);
    fwrite(&mcd->buf[addr], 1, size, mcd->file);
}

void mcd_cmd_probe(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_probe\n");

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_unk_12(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_unk_12\n");

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_start_erase(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    uint32_t lba =
        (sio2->in->buf[sio2->in->index + 2]) |
        (sio2->in->buf[sio2->in->index + 3] << 8) |
        (sio2->in->buf[sio2->in->index + 4] << 16) |
        (sio2->in->buf[sio2->in->index + 5] << 24);

    printf("mcd: mcd_cmd_start_erase(%08x)\n", lba);

    mcd->addr = lba * MCD_SECTOR_SIZE;

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_start_write(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    uint32_t lba =
        (sio2->in->buf[sio2->in->index + 2]) |
        (sio2->in->buf[sio2->in->index + 3] << 8) |
        (sio2->in->buf[sio2->in->index + 4] << 16) |
        (sio2->in->buf[sio2->in->index + 5] << 24);

    printf("mcd: mcd_cmd_start_write(%08x)\n", lba);

    mcd->addr = lba * MCD_SECTOR_SIZE;

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_start_read(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    uint32_t lba =
        (sio2->in->buf[sio2->in->index + 2]) |
        (sio2->in->buf[sio2->in->index + 3] << 8) |
        (sio2->in->buf[sio2->in->index + 4] << 16) |
        (sio2->in->buf[sio2->in->index + 5] << 24);

    printf("mcd: mcd_cmd_start_read(%08x)\n", lba);

    mcd->addr = lba * MCD_SECTOR_SIZE;

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_get_specs(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_get_specs\n", sio2->in->buf[2]);

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);

    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, 0x00); // Sector size (2-byte)
    queue_push(sio2->out, 0x02);
    queue_push(sio2->out, 0x10); // Erase block size (2-byte)
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, (mcd->size >> 0) & 0xff); // Sector count (4-byte)
    queue_push(sio2->out, (mcd->size >> 8) & 0xff);
    queue_push(sio2->out, (mcd->size >> 16) & 0xff);
    queue_push(sio2->out, (mcd->size >> 24) & 0xff);
    queue_push(sio2->out, mcd->checksum); // Checksum
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_set_terminator(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_set_terminator(%02x)\n", sio2->in->buf[2]);

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);

    mcd->term = sio2->in->buf[2];
}
void mcd_cmd_get_terminator(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_get_terminator\n");

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
    queue_push(sio2->out, 0x55);
}
void mcd_cmd_write_data(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    uint8_t size = queue_at(sio2->in, 2);

    printf("mcd: mcd_cmd_write_data(%02x)\n", size);

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);

    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);

    uint32_t addr = mcd->addr;

    for (int i = 0; i < size; i++) {
        mcd->buf[mcd->addr++] = queue_at(sio2->in, 3 + i);

        queue_push(sio2->out, 0);
    }

    mcd_flush_block(mcd, addr, size);

    queue_push(sio2->out, 0);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_read_data(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_read_data(%02x)\n", queue_at(sio2->in, 2));

    // assert(queue_at(sio2->in, 2) == 0x80);

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);

    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);

    uint8_t checksum = 0;

    for (int i = 0; i < queue_at(sio2->in, 2); i++) {
        uint8_t data = mcd->buf[mcd->addr++];

        checksum ^= data;

        queue_push(sio2->out, data);
    }

    queue_push(sio2->out, checksum); // XOR checksum
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_rw_end(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_rw_end\n");

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_erase_block(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_erase_block\n");

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);

    uint32_t addr = mcd->addr;

    for (int i = 0; i < MCD_SECTOR_SIZE * 16; i++) {
        mcd->buf[mcd->addr++] = 0xff;
    }

    mcd_flush_block(mcd, addr, MCD_SECTOR_SIZE * 16);
}
void mcd_cmd_auth_f0(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_auth_f0\n");

    uint8_t param = sio2->in->buf[2];

    switch (param) {
        case 0x01:
        case 0x02:
        case 0x04:
        case 0x0f:
        case 0x11:
        case 0x13: {
            // Handle checksum
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);

            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x2b);

            uint8_t checksum = 0;

            for (int i = 0; i < 8; i++) {
                checksum ^= sio2->in->buf[i+3];

                queue_push(sio2->out, 0x00);
            }

            queue_push(sio2->out, checksum);
            queue_push(sio2->out, mcd->term);
        } break;

        case 0x06:
        case 0x07:
        case 0x0b: {
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);

            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x2b);
            queue_push(sio2->out, mcd->term);
        } break;

        default: {
            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x00);

            queue_push(sio2->out, 0x00);
            queue_push(sio2->out, 0x2b);
            queue_push(sio2->out, mcd->term);
        } break;
    }
}
void mcd_cmd_auth_f1(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    fprintf(stderr, "mcd: mcd_cmd_auth_f1\n");
    fprintf(stderr, "mcd: params=");

    for (int i = 0; i < 16; i++) {
        fprintf(stderr, "%02x ", sio2->in->buf[2 + i]);
    }

    fprintf(stderr, "\n");

    exit(1);

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_auth_f3(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_auth_f3\n");

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_auth_f7(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_auth_f7\n");

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_unk_bf(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_unk_bf\n");

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}

void mcd_handle_command(struct ps2_sio2* sio2, void* udata, int cmd) {
    struct mcd_state* mcd = (struct mcd_state*)udata;

    switch (cmd) {
        case 0x11: mcd_cmd_probe(sio2, mcd); return;
        case 0x12: mcd_cmd_unk_12(sio2, mcd); return;
        case 0x21: mcd_cmd_start_erase(sio2, mcd); return;
        case 0x22: mcd_cmd_start_write(sio2, mcd); return;
        case 0x23: mcd_cmd_start_read(sio2, mcd); return;
        case 0x26: mcd_cmd_get_specs(sio2, mcd); return;
        case 0x27: mcd_cmd_set_terminator(sio2, mcd); return;
        case 0x28: mcd_cmd_get_terminator(sio2, mcd); return;
        case 0x42: mcd_cmd_write_data(sio2, mcd); return;
        case 0x43: mcd_cmd_read_data(sio2, mcd); return;
        // case 0x52: mcd_cmd_ps1_read(sio2, mcd); return;
        case 0x81: mcd_cmd_rw_end(sio2, mcd); return;
        case 0x82: mcd_cmd_erase_block(sio2, mcd); return;
        case 0xf0: mcd_cmd_auth_f0(sio2, mcd); return;
        case 0xf1: mcd_cmd_auth_f1(sio2, mcd); return;
        case 0xf3: mcd_cmd_auth_f3(sio2, mcd); return;
        case 0xf7: mcd_cmd_auth_f7(sio2, mcd); return;
        case 0xbf: mcd_cmd_unk_bf(sio2, mcd); return;
    }

    printf("mcd: Unhandled command %02x\n", cmd);

    exit(1);
}

struct mcd_state* mcd_attach(struct ps2_sio2* sio2, int port, const char* path) {
    FILE* file = fopen(path, "r+b");

    if (!file)
        return NULL;

    struct mcd_state* mcd = malloc(sizeof(struct mcd_state));
    struct sio2_device dev;

    memset(mcd, 0, sizeof(struct mcd_state));

    // Get memcard size
    fseek(file, 0, SEEK_END);

    mcd->buf_size = ftell(file);

    fseek(file, 0, SEEK_SET);

    mcd->buf = (uint8_t*)malloc(mcd->buf_size);

    fread(mcd->buf, 1, mcd->buf_size, file);

    // Init card state
    mcd->term = 0x55;
    mcd->file = file;
    mcd->size = (1 << (31 - __builtin_clz(mcd->buf_size))) >> 9;

    mcd->checksum = 0x02 ^ 0x10;

    for (int i = 0; i < 4; i++)
        mcd->checksum ^= (mcd->size >> (i * 8)) & 0xff;

    printf("mcd: Memory card at \'%s\' initialized.\n\tTotal size: %x (%d)\n\tSize (in sectors): %x (%d)\n\tChecksum: %02x\n",
        path, mcd->buf_size, mcd->buf_size,
        mcd->size, mcd->size,
        mcd->checksum
    );

    dev.detach = mcd_detach;
    dev.handle_command = mcd_handle_command;
    dev.udata = mcd;

    ps2_sio2_attach_device(sio2, dev, port);

    return mcd;
}

void mcd_detach(void* udata) {
    struct mcd_state* mcd = (struct mcd_state*)udata;

    // Flush buffer back to file
    fseek(mcd->file, 0, SEEK_SET);
    fwrite(mcd->buf, 1, mcd->buf_size, mcd->file);

    fclose(mcd->file);
    free(mcd->buf);
    free(mcd);
}