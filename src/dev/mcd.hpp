#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "iop/sio2.hpp"
#include "logger.hpp"

namespace iris::dev::mcd {

enum Size : uint32_t {
    _8MB = 0x4000,
    _16MB = 0x8000,
    _32MB = 0x10000,
    _64MB = 0x20000
};


inline constexpr uint32_t SECTOR_SIZE = 512 + 16;

enum {
    TERMINATOR_NOT_READY = 0x66,
    TERMINATOR_READY = 0x55
};

enum {
    CARD_KEY_RETAIL = 0,
    CARD_KEY_ARCADE,
    CARD_KEY_ARCADE_CEX,
    CARD_KEY_PROTOTYPE
};

struct Auth {
    uint8_t iv[8];
    uint8_t seed[8];
    uint8_t nonce[8];
    uint8_t challenge1[8];
    uint8_t challenge2[8];
    uint8_t challenge3[8];
    uint8_t response1[8];
    uint8_t response2[8];
    uint8_t response3[8];

    uint8_t crypt_buf[8];
    uint8_t crypt_checksum;

    uint32_t random_seed;
};

struct Mcd {
    int port = 0;
    uint8_t term = 0;
    uint16_t buttons = 0;
    uint8_t ax_right_y = 0;
    uint8_t ax_right_x = 0;
    uint8_t ax_left_y = 0;
    uint8_t ax_left_x = 0;
    int config_mode = 0;
    int act_index = 0;
    int mode_index = 0;
    uint32_t size = 0;
    uint8_t checksum = 0;
    uint32_t addr = 0;
    uint32_t buf_size = 0;
    uint8_t* buf = nullptr;

    FILE* file = nullptr;

    Auth auth;
    int key_source = CARD_KEY_RETAIL;
    int configured_key_source = CARD_KEY_RETAIL;
    int magicgate = 0;
    uint8_t challenge_iv[8] = {};
    uint8_t card_id[8] = { 'M', 'e', 'c', 'h', 'a', 'P', 'w', 'n' };

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Mcd* attach(logger::Logger* logger, sio2::Sio2* sio2, int port, const char* path);
void set_magicgate(Mcd* mcd, int enabled, int key_source, const uint8_t* challenge_iv, const char* card_id_path);
void detach(void* udata);

}
