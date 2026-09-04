#include <string>

#include "mcd.hpp"

#ifdef IRIS_ENABLE_MAGICGATE
#include "shared/des.hpp"
#endif

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
static const uint8_t card_keys[4][16] = {
    { 0x06, 0x46, 0x7A, 0x6C, 0x5B, 0x9B, 0x82, 0x77, 0x0D, 0xDF, 0xE9, 0x7E, 0x24, 0x5B, 0x9F, 0xCA },
    { 0xCE, 0xC2, 0x18, 0x1C, 0x03, 0x6B, 0x0A, 0x9B, 0x87, 0x9F, 0x65, 0x6B, 0x43, 0x28, 0x94, 0xCB },
    { 0xA9, 0xFB, 0x27, 0x2A, 0x63, 0xCF, 0xED, 0x6F, 0xD0, 0x28, 0xA2, 0x4A, 0x98, 0x11, 0xB8, 0x2E },
    { 0x8C, 0x4B, 0xEF, 0xA6, 0xF4, 0x9A, 0x23, 0xA0, 0x9C, 0xF1, 0x46, 0xAA, 0x17, 0x1C, 0xFE, 0x75 }
};

static const uint8_t key_sources[4][8] = {
    { 0xF5, 0x80, 0x95, 0x3C, 0x4C, 0x84, 0xA9, 0xC0 },
    { 0x03, 0x13, 0xE4, 0x19, 0x27, 0x01, 0xB9, 0x52 },
    { 0xF5, 0x80, 0x95, 0x3C, 0x4C, 0x84, 0xA9, 0xC0 },
    { 0xF5, 0x80, 0x95, 0x3C, 0x4C, 0x84, 0xA9, 0xC0 }
};

void set_magicgate(Mcd* mcd, int enabled, int key_source, const uint8_t* challenge_iv, const char* card_id_path) {
    mcd->magicgate = enabled;
    mcd->key_source = key_source;
    mcd->configured_key_source = key_source;

    if (challenge_iv)
        memcpy(mcd->challenge_iv, challenge_iv, 8);

    if (card_id_path && card_id_path[0]) {
        FILE* file = fopen(card_id_path, "rb");

        if (file) {
            if (fread(mcd->card_id, 1, 8, file) != 8) {
                iris_error(mcd, "Card ID \"{}\" is too short", card_id_path);
            } else {
                iris_info(mcd, "Loaded card ID from \"{}\"", card_id_path);
            }

            fclose(file);
        } else {
            iris_error(mcd, "Failed to open card ID \"{}\"", card_id_path);
        }
    }

    mcd->auth.random_seed = 0x1234abcd;
}

static uint8_t next_random(Mcd* mcd) {
    mcd->auth.random_seed = (mcd->auth.random_seed * 1103515245u) + 12345u;

    return (uint8_t)(mcd->auth.random_seed >> 16);
}

static int key_index(Mcd* mcd) {
    int source = mcd->key_source;

    if (source < 0 || source > CARD_KEY_PROTOTYPE) {
        source = CARD_KEY_RETAIL;
    }

    return source;
}

static const uint8_t* card_key(Mcd* mcd) {
    return card_keys[key_index(mcd)];
}

static void generate_iv_seed_nonce(Mcd* mcd) {
    const uint8_t* source = key_sources[key_index(mcd)];

    for (int i = 0; i < 8; i++) {
        mcd->auth.iv[i] = next_random(mcd);
        mcd->auth.seed[i] = source[i] ^ mcd->auth.iv[i];
        mcd->auth.nonce[i] = next_random(mcd);
    }
}

#ifdef IRIS_ENABLE_MAGICGATE
static void generate_response(Mcd* mcd) {
    const uint8_t* key = card_key(mcd);

    des::double_decrypt(key, mcd->auth.challenge1);

    uint8_t random[8];

    des::xor_bytes(mcd->auth.challenge1, mcd->challenge_iv, random, 8);

    des::xor_bytes(mcd->auth.nonce, mcd->challenge_iv, mcd->auth.response1, 8);
    des::double_encrypt(key, mcd->auth.response1);

    des::xor_bytes(random, mcd->auth.response1, mcd->auth.response2, 8);
    des::double_encrypt(key, mcd->auth.response2);

    des::xor_bytes(mcd->card_id, mcd->auth.response2, mcd->auth.response3, 8);
    des::double_encrypt(key, mcd->auth.response3);
}

static void push_auth_bytes(sio2::Sio2* sio2, Mcd* mcd, const uint8_t* data) {
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x00);

    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, 0x2b);

    uint8_t checksum = 0;

    for (int i = 0; i < 8; i++) {
        uint8_t value = data[7 - i];

        checksum ^= value;

        queue::push(sio2->out, value);
    }

    queue::push(sio2->out, checksum);
    queue::push(sio2->out, mcd->term);
}

#endif

static void push_terminator(sio2::Sio2* sio2, Mcd* mcd, int length) {
    for (int i = 0; i < length - 2; i++) {
        queue::push(sio2->out, 0x00);
    }

    queue::push(sio2->out, 0x2b);
    queue::push(sio2->out, mcd->term);
}

static void take_auth_bytes(sio2::Sio2* sio2, uint8_t* data) {
    for (int i = 0; i < 8; i++) {
        data[7 - i] = sio2->in->buf[i + 3];
    }
}

#ifdef IRIS_ENABLE_MAGICGATE
static int cmd_auth_f0_magicgate(sio2::Sio2* sio2, Mcd* mcd, uint8_t param) {
    switch (param) {
        case 0x01: {
            generate_iv_seed_nonce(mcd);

            push_auth_bytes(sio2, mcd, mcd->auth.iv);
        } return 1;

        case 0x02: {
            push_auth_bytes(sio2, mcd, mcd->auth.seed);
        } return 1;

        case 0x04: {
            push_auth_bytes(sio2, mcd, mcd->auth.nonce);
        } return 1;

        case 0x0f: {
            generate_response(mcd);

            push_auth_bytes(sio2, mcd, mcd->auth.response1);
        } return 1;

        case 0x11: {
            push_auth_bytes(sio2, mcd, mcd->auth.response2);
        } return 1;

        case 0x13: {
            push_auth_bytes(sio2, mcd, mcd->auth.response3);
        } return 1;

        case 0x06: {
            take_auth_bytes(sio2, mcd->auth.challenge3);
        } return 0;

        case 0x07: {
            take_auth_bytes(sio2, mcd->auth.challenge2);
        } return 0;

        case 0x0b: {
            take_auth_bytes(sio2, mcd->auth.challenge1);
        } return 0;
    }

    return 0;
}

#endif

void cmd_auth_f0(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_auth_f0");

    uint8_t param = sio2->in->buf[2];

#ifdef IRIS_ENABLE_MAGICGATE
    if (mcd->magicgate && cmd_auth_f0_magicgate(sio2, mcd, param)) {
        return;
    }
#endif

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
static int cmd_auth_crypt(sio2::Sio2* sio2, Mcd* mcd, uint8_t param) {
    switch (param) {
        case 0x40:
        case 0x42:
        case 0x50:
        case 0x52: {
            push_terminator(sio2, mcd, 5);
        } return 1;

        case 0x41:
        case 0x51: {
            mcd->auth.crypt_checksum = 0;

            for (int i = 0; i < 8; i++) {
                uint8_t value = sio2->in->buf[i + 3];

                mcd->auth.crypt_checksum ^= value;
                mcd->auth.crypt_buf[i] = value;
            }

            push_terminator(sio2, mcd, 14);
        } return 1;

        case 0x43:
        case 0x53: {
            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x00);

            queue::push(sio2->out, 0x00);
            queue::push(sio2->out, 0x2b);

            for (int i = 0; i < 8; i++) {
                queue::push(sio2->out, mcd->auth.crypt_buf[i]);
            }

            queue::push(sio2->out, mcd->auth.crypt_checksum);
            queue::push(sio2->out, mcd->term);
        } return 1;
    }

    return 0;
}

void cmd_auth_f1(sio2::Sio2* sio2, Mcd* mcd) {
    uint8_t param = sio2->in->buf[2];

    iris_debug(mcd, "cmd_auth_f1({:02x})", param);

    if (cmd_auth_crypt(sio2, mcd, param))
        return;

    iris_warning(mcd, "Unhandled auth crypt parameter {:02x}", param);

    push_terminator(sio2, mcd, 5);
}
void cmd_auth_f3(sio2::Sio2* sio2, Mcd* mcd) {
    iris_debug(mcd, "cmd_auth_f3");

    if (mcd->magicgate) {
        mcd->term = TERMINATOR_READY;
        mcd->key_source = mcd->configured_key_source;
    }

    push_terminator(sio2, mcd, 5);
}
void cmd_auth_f7(sio2::Sio2* sio2, Mcd* mcd) {
    uint8_t param = sio2->in->buf[2];

    iris_debug(mcd, "cmd_auth_f7({:02x})", param);

    if (mcd->magicgate && param == 0x01)
        mcd->key_source = CARD_KEY_RETAIL;

    push_terminator(sio2, mcd, 5);
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
        case 0xf2: cmd_auth_f1(sio2, mcd); return;
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

    mcd->port = port;
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
    dev.reset = reset;
    dev.handle_command = handle_command;
    dev.udata = mcd;

    sio2::attach_device(sio2, dev, port);

    return mcd;
}

void reset(void* udata) {
    Mcd* mcd = (Mcd*)udata;

    mcd->term = 0x55;
    mcd->config_mode = 0;
    mcd->act_index = 0;
    mcd->mode_index = 0;
    mcd->checksum = 0;
    mcd->addr = 0;

    mcd->auth = {};
    mcd->key_source = mcd->configured_key_source;
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
