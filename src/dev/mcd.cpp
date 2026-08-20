#include <string>

#include "mcd.hpp"

namespace iris::dev::mcd {

void flush_block(Mcd* mcd, int addr, int size) {
    fseek(mcd->file, addr, SEEK_SET);
    fwrite(&mcd->buf[addr], 1, size, mcd->file);
}

void cmd_probe(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_probe");

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}
void cmd_unk_12(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_unk_12");

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}
void cmd_start_erase(sio2::Sio2* sio2, Mcd* mcd) {
    uint32_t lba =
        (sio2->in->buf[sio2->in->index + 2]) |
        (sio2->in->buf[sio2->in->index + 3] << 8) |
        (sio2->in->buf[sio2->in->index + 4] << 16) |
        (sio2->in->buf[sio2->in->index + 5] << 24);

    iris_debug(mcd, "cmd_start_erase({:08x})", lba);

    mcd->addr = lba * SECTOR_SIZE;

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}
void cmd_start_write(sio2::Sio2* sio2, Mcd* mcd) {
    uint32_t lba =
        (sio2->in->buf[sio2->in->index + 2]) |
        (sio2->in->buf[sio2->in->index + 3] << 8) |
        (sio2->in->buf[sio2->in->index + 4] << 16) |
        (sio2->in->buf[sio2->in->index + 5] << 24);

    iris_debug(mcd, "cmd_start_write({:08x})", lba);

    mcd->addr = lba * SECTOR_SIZE;

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}
void cmd_start_read(sio2::Sio2* sio2, Mcd* mcd) {
    uint32_t lba =
        (sio2->in->buf[sio2->in->index + 2]) |
        (sio2->in->buf[sio2->in->index + 3] << 8) |
        (sio2->in->buf[sio2->in->index + 4] << 16) |
        (sio2->in->buf[sio2->in->index + 5] << 24);

    iris_debug(mcd, "cmd_start_read({:08x})", lba);

    mcd->addr = lba * SECTOR_SIZE;

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}
void cmd_get_specs(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_get_specs", sio2->in->buf[2]);

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);

    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, 0x00); // Sector size (2-byte)
    queue::push(sio2->out, 0x02);
    queue::push(sio2->out, 0x10); // Erase block size (2-byte)
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, (mcd->size >> 0) & 0xff); // Sector count (4-byte)
    queue::push(sio2->out, (mcd->size >> 8) & 0xff);
    queue::push(sio2->out, (mcd->size >> 16) & 0xff);
    queue::push(sio2->out, (mcd->size >> 24) & 0xff);
    queue::push(sio2->out, mcd->checksum); // Checksum
    queue::push(sio2->out, mcd->term);
}
void cmd_set_terminator(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_set_terminator({:02x})", sio2->in->buf[2]);

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);

    mcd->term = sio2->in->buf[2];
}
void cmd_get_terminator(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_get_terminator");

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
    queue::push(sio2->out, 0x55);
}
void cmd_write_data(sio2::Sio2* sio2, Mcd* mcd) {
    uint8_t size = queue::at(sio2->in, 2);

    iris_debug(mcd, "cmd_write_data({:02x})", size);

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);

    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);

    uint32_t addr = mcd->addr;

    for (int i = 0; i < size; i++) {
        mcd->buf[mcd->addr++ % mcd->buf_size] = queue::at(sio2->in, 3 + i);

        queue::push(sio2->out, 0);
    }

    flush_block(mcd, addr, size);

    queue::push(sio2->out, 0);
    queue::push(sio2->out, mcd->term);
}
void cmd_read_data(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_read_data({:02x})", queue::at(sio2->in, 2));

    // assert(queue::at(sio2->in, 2) == 0x80);

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);

    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);

    uint8_t checksum = 0;

    for (int i = 0; i < queue::at(sio2->in, 2); i++) {
        uint8_t data = mcd->buf[mcd->addr++ % mcd->buf_size];

        checksum ^= data;

        queue::push(sio2->out, data);
    }

    queue::push(sio2->out, checksum); // XOR checksum
    queue::push(sio2->out, mcd->term);
}
void cmd_rw_end(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_rw_end");

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}
void cmd_erase_block(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_erase_block");

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);

    uint32_t addr = mcd->addr;

    for (int i = 0; i < SECTOR_SIZE * 16; i++) {
        mcd->buf[mcd->addr++ % mcd->buf_size] = 0xff;
    }

    flush_block(mcd, addr, SECTOR_SIZE * 16);
}
void cmd_auth_f0(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_auth_f0");

    uint8_t param = sio2->in->buf[2];

    switch (param) {
        case 0x01:
        case 0x02:
        case 0x04:
        case 0x0f:
        case 0x11:
        case 0x13: {
            // Handle checksum
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);

            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x2b);

            uint8_t checksum = 0;

            for (int i = 0; i < 8; i++) {
                checksum ^= sio2->in->buf[i+3];

                queue::push(sio2->out, 0x00);
            }

            queue::push(sio2->out, checksum);
            queue::push(sio2->out, mcd->term);
        } break;

        case 0x06:
        case 0x07:
        case 0x0b: {
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);

            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x2b);
            queue::push(sio2->out, mcd->term);
        } break;

        default: {
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);

            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x2b);
            queue::push(sio2->out, mcd->term);
        } break;
    }
}
void cmd_auth_f1(sio2::Sio2* sio2, Mcd* mcd) {
    std::string params;

    for (int i = 0; i < 16; i++)
        params += fmt::format("{:02x} ", sio2->in->buf[2 + i]);

    iris_fatal_error(mcd, "cmd_auth_f1 params={}", params);

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}
void cmd_auth_f3(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_auth_f3");

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}
void cmd_auth_f7(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_auth_f7");

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}
void cmd_unk_bf(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_unk_bf");

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}

void cmd_ps1_probe(sio2::Sio2* sio2, Mcd* mcd) {
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);
}

void handle_command(sio2::Sio2* sio2, void* udata, int cmd) {
    Mcd* mcd = (Mcd*)udata;

    switch (cmd) {
        case 0x11: cmd_probe(sio2, mcd); return;
        case 0x12: cmd_unk_12(sio2, mcd); return;
        case 0x21: cmd_start_erase(sio2, mcd); return;
        case 0x22: cmd_start_write(sio2, mcd); return;
        case 0x23: cmd_start_read(sio2, mcd); return;
        case 0x26: cmd_get_specs(sio2, mcd); return;
        case 0x27: cmd_set_terminator(sio2, mcd); return;
        case 0x28: cmd_get_terminator(sio2, mcd); return;
        case 0x42: cmd_write_data(sio2, mcd); return;
        case 0x43: cmd_read_data(sio2, mcd); return;

        // Reply with zeroes to signal that this card is a PS2 card
        case 0x52: case 0x53: case 0x57: case 0x58: {
            cmd_ps1_probe(sio2, mcd);
        } return;

        case 0x81: cmd_rw_end(sio2, mcd); return;
        case 0x82: cmd_erase_block(sio2, mcd); return;
        case 0xf0: cmd_auth_f0(sio2, mcd); return;
        case 0xf1: cmd_auth_f1(sio2, mcd); return;
        case 0xf3: cmd_auth_f3(sio2, mcd); return;
        case 0xf7: cmd_auth_f7(sio2, mcd); return;
        case 0xbf: cmd_unk_bf(sio2, mcd); return;
    }

    iris_fatal_error(mcd, "Unhandled command {:02x}", cmd);
}

Mcd* attach(logger::Logger* logger, sio2::Sio2* sio2, int port, const char* path) {
    FILE* file = fopen(path, "r+b");

    if (!file)
        return nullptr;

    Mcd* mcd = new Mcd();

    mcd->logger = logger;
    mcd->logger_id = logger::register_source(logger, "mcd");
    sio2::Device dev;

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

    iris_debug(mcd, "Memory card at \'{}\' initialized.\n\tTotal size: {:x} ({})\n\tSize (in sectors): {:x} ({})\n\tChecksum: {:02x}", path, mcd->buf_size, mcd->buf_size,
        mcd->size, mcd->size,
        mcd->checksum);

    dev.detach = detach;
    dev.handle_command = handle_command;
    dev.udata = mcd;

    sio2::attach_device(sio2, dev, port);

    return mcd;
}

void detach(void* udata) {
    Mcd* mcd = (Mcd*)udata;

    // Flush buffer back to file
    fseek(mcd->file, 0, SEEK_SET);
    fwrite(mcd->buf, 1, mcd->buf_size, mcd->file);

    fclose(mcd->file);
    free(mcd->buf);
    delete mcd;
}

}
