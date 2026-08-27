#pragma once

#include <cstdint>

#include "logger.hpp"

namespace iris::mg {

enum {
    KEY_STORE_MODE_DEV = 0,
    KEY_STORE_MODE_RETAIL,
    KEY_STORE_MODE_PROTOTYPE,
    KEY_STORE_MODE_ARCADE
};

enum {
    KEY_CHALLENGE_IV = 0,
    KEY_CARD_KEY_STORE,
    KEY_ENCRYPTED_KEY_STORE,
    KEY_STORE_KEY,
    KEY_ARCADE_KELF_KBIT,
    KEY_ARCADE_KELF_KC,
    KEY_FILE_COUNT
};

inline constexpr auto CARD_KEY_STORE_SIZE = 96;
inline constexpr auto ENCRYPTED_KEY_STORE_SIZE = 1024;
inline constexpr auto STORE_KEY_SIZE = 16;
inline constexpr auto CHALLENGE_IV_SIZE = 8;
inline constexpr auto ARCADE_KELF_KEY_SIZE = 16;

#pragma pack(push, 1)

struct KeyStore {
    uint8_t card_key_low[3][8];
    uint8_t card_key_hi[3][8];
    uint8_t card_key2_low[3][8];
    uint8_t card_key2_hi[3][8];
    uint8_t card_iv[3][8];
    uint8_t card_iv2[3][8];
    uint8_t kbit_master_key[16];
    uint8_t kc_master_key[16];
    uint8_t kbit_iv[8];
    uint8_t kc_iv[8];
    uint8_t icvps2_low_key[16];
    uint8_t icvps2_hi_key[16];
    uint8_t icvps2_low_iv[8];
    uint8_t icvps2_hi_iv[8];
    uint8_t signature_master_key[8];
    uint8_t signature_hash_key[8];
    uint8_t root_sig_hash_key[16];
    uint8_t root_sig_master_key[8];
    uint8_t content_iv[8];
    uint8_t content_table_iv[8];
    uint8_t challenge_iv[8];
};

#pragma pack(pop)

struct Keys {
    uint16_t card_key_store[CARD_KEY_STORE_SIZE / 2];
    uint16_t encrypted_key_store[ENCRYPTED_KEY_STORE_SIZE / 2];
    uint8_t store_key[STORE_KEY_SIZE];
    uint8_t challenge_iv[CHALLENGE_IV_SIZE];
    uint8_t arcade_kelf_kbit[ARCADE_KELF_KEY_SIZE];
    uint8_t arcade_kelf_kc[ARCADE_KELF_KEY_SIZE];

    int loaded[KEY_FILE_COUNT];

    KeyStore store;
    uint8_t icvps2_key[16];
    uint8_t console_id[8];
    uint8_t ilink_id[8];

    int mode;
    int ready;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

enum {
    MECHA_STATE_0 = 0,
    MECHA_STATE_READY,
    MECHA_STATE_KEY_INDEXES_SET,
    MECHA_STATE_CARD_IV_SEED_SET,
    MECHA_STATE_CARD_NONCE_SET,
    MECHA_STATE_CARD_CHALLENGE_GENERATED,
    MECHA_STATE_CARD_CHALLENGE12_SENT,
    MECHA_STATE_CARD_CHALLENGE23_SENT,
    MECHA_STATE_CARD_RESPONSE12_RECEIVED,
    MECHA_STATE_CARD_RESPONSE3_RECEIVED,
    MECHA_STATE_CARD_VERIFIED,
    MECHA_STATE_KELF_HEADER_PARAMS_SET,
    MECHA_STATE_KELF_HEADER_RECEIVED,
    MECHA_STATE_KELF_HEADER_VERIFIED,
    MECHA_STATE_BIT_LENGTH_SENT,
    MECHA_STATE_KELF_CONTENT_DECRYPT_IN_PROGRESS,
    MECHA_STATE_DATA_IN_LENGTH_SET,
    MECHA_STATE_UNK17,
    MECHA_STATE_DATA_OUT_LENGTH_SET,
    MECHA_STATE_KELF_CONTENT_RECEIVED,
    MECHA_STATE_KELF_CONTENT_DECRYPT_DONE,
    MECHA_STATE_KBIT1_SENT,
    MECHA_STATE_KBIT2_SENT,
    MECHA_STATE_KC1_SENT,
    MECHA_STATE_KC2_SENT
};

enum {
    MECHA_RESULT_0 = 0x000,
    MECHA_RESULT_CARD_CHALLENGE_GENERATED = 0x204,
    MECHA_RESULT_CARD_VERIFIED = 0x209,
    MECHA_RESULT_KELF_HEADER_VERIFIED = 0x20c,
    MECHA_RESULT_KELF_CONTENT_DECRYPTED = 0x213,
    MECHA_RESULT_FAILED = 0x300
};

inline constexpr auto BIT_BLOCK_ENCRYPTED = 1;
inline constexpr auto BIT_BLOCK_SIGNED = 2;

inline constexpr auto DATA_BUFFER_SIZE = 0x80000;
inline constexpr auto MAX_BIT_BLOCKS = 64;
inline constexpr auto CARD_KEY_SLOTS = 16;
inline constexpr auto ARCADE_KELF_APPLICATION_TYPE = 0x07;

#pragma pack(push, 1)

struct KelfHeader {
    uint8_t nonce[16];
    uint32_t content_size;
    uint16_t header_size;
    uint8_t system_type;
    uint8_t application_type;
    uint16_t flags;
    uint16_t ban_count;
    uint32_t region_flags;
};

struct ConsoleBan {
    uint8_t ilink_id[8];
    uint8_t console_id[8];
};

struct BitBlock {
    uint32_t size;
    uint32_t flags;
    uint8_t signature[8];
};

struct BitTable {
    uint32_t header_size;
    uint8_t block_count;
    uint8_t gap[3];

    BitBlock blocks[256];
};

#pragma pack(pop)

struct ProcessedBitBlock {
    uint8_t flags;
    uint32_t size;
    uint8_t signature[8];
};

struct State {
    int state;
    int result;
    uint8_t errorcode;

    uint8_t card_key_slot;
    uint8_t card_key_index;
    uint8_t mode3_key_index;
    uint8_t mode;

    uint8_t memcard_iv[8];
    uint8_t memcard_seed[8];
    uint8_t memcard_nonce[8];
    uint8_t memcard_key[16];
    uint8_t memcard_random[8];
    uint8_t memcard_challenge1[8];
    uint8_t memcard_challenge2[8];
    uint8_t memcard_challenge3[8];
    uint8_t memcard_response1[8];
    uint8_t memcard_response2[8];
    uint8_t memcard_response3[8];

    uint8_t card_key[CARD_KEY_SLOTS][8];

    uint16_t data_size;
    uint16_t data_buffer_offset;
    uint16_t data_out_offset;
    uint8_t* data_out_ptr;

    uint16_t bit_length;
    uint8_t kc[16];

    uint8_t pub_icvps2[8];
    uint8_t pub_kbit[16];
    uint8_t pub_kc[16];

    BitTable* bit_table;
    ProcessedBitBlock bit_blocks[MAX_BIT_BLOCKS];
    int last_bit_table;
    int current_block;
    uint32_t done_blocks;

    KelfHeader verified_header;

    uint8_t content_last_ciphertext[8];
    uint8_t signature_last_ciphertext[8];

    uint32_t random_seed;

    uint8_t data_buffer[DATA_BUFFER_SIZE];
};

void init(Keys* keys, logger::Logger* logger);
void reset(State* state);
int command(Keys* keys, State* state, uint8_t cmd, const uint8_t* params, int param_count, uint8_t* result, int* result_size);
int load_file(Keys* keys, int which, const char* path);
int has_complete_keyset(const Keys* keys);
int derive(Keys* keys, int mode, const uint8_t* console_id, const uint8_t* ilink_id);

}
