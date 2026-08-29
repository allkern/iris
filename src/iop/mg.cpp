#include <cstdio>
#include <cstring>
#include <string>

#include <fmt/format.h>

#include "mg.hpp"
#include "shared/des.hpp"

namespace iris::mg {

static const uint16_t memory_card_key_indexes[72] = {
    0x0018, 0xFFFF, 0xFFFF, 0x001C, 0xFFFF, 0xFFFF, 0x0020, 0xFFFF, 0xFFFF,
    0x0024, 0xFFFF, 0xFFFF, 0x0028, 0xFFFF, 0xFFFF, 0x002C, 0xFFFF, 0xFFFF,
    0x0030, 0x0048, 0x0060, 0x0034, 0x004C, 0x0064, 0x0038, 0x0050, 0x0068,
    0x003C, 0x0054, 0x006C, 0x0040, 0x0058, 0x0070, 0x0044, 0x005C, 0x0074,
    0x0000, 0x1000, 0x1001, 0x0004, 0x1002, 0x1003, 0x0008, 0x1004, 0x1005,
    0x000C, 0x1006, 0x1007, 0x0010, 0x1008, 0x1009, 0x0014, 0x100A, 0x100B,
    0x0090, 0x00A8, 0x00A8, 0x0094, 0x00AC, 0x00AC, 0x0098, 0x00B0, 0x00B0,
    0x009C, 0x00B4, 0x00B4, 0x00A0, 0x00B8, 0x00B8, 0x00A4, 0x00BC, 0x00BC
};

static const uint16_t kelf_keys_index[4] = { 0x110, 0x110, 0x00C4, 0x015C };

static const struct {
    const char* name;
    uint32_t size;
} key_files[KEY_FILE_COUNT] = {
    { "challenge IV", CHALLENGE_IV_SIZE },
    { "card key store", CARD_KEY_STORE_SIZE },
    { "encrypted key store", ENCRYPTED_KEY_STORE_SIZE },
    { "key store key", STORE_KEY_SIZE },
    { "arcade KELF KBIT", ARCADE_KELF_KEY_SIZE },
    { "arcade KELF KC", ARCADE_KELF_KEY_SIZE }
};

static uint8_t* key_buffer(Keys* keys, int which) {
    switch (which) {
        case KEY_CHALLENGE_IV: return keys->challenge_iv;
        case KEY_CARD_KEY_STORE: return (uint8_t*)keys->card_key_store;
        case KEY_ENCRYPTED_KEY_STORE: return (uint8_t*)keys->encrypted_key_store;
        case KEY_STORE_KEY: return keys->store_key;
        case KEY_ARCADE_KELF_KBIT: return keys->arcade_kelf_kbit;
        case KEY_ARCADE_KELF_KC: return keys->arcade_kelf_kc;
    }

    return NULL;
}

void init(Keys* keys, logger::Logger* logger) {
    memset(keys, 0, sizeof(Keys));

    keys->logger = logger;
    keys->logger_id = logger::register_source(logger, "mecha");
}

int load_file(Keys* keys, int which, const char* path) {
    if (which < 0 || which >= KEY_FILE_COUNT)
        return 0;

    keys->loaded[which] = 0;

    if (!path || !path[0])
        return 0;

    FILE* file = fopen(path, "rb");

    if (!file) {
        iris_error(keys, "Failed to open {} file \"{}\"", key_files[which].name, path);

        return 0;
    }

    uint8_t* buffer = key_buffer(keys, which);

    size_t read = fread(buffer, 1, key_files[which].size, file);

    fclose(file);

    if (read != key_files[which].size) {
        iris_error(keys, "{} file \"{}\" is {} bytes, expected {}",
            key_files[which].name, path, read, key_files[which].size);

        return 0;
    }

    keys->loaded[which] = 1;

    iris_info(keys, "Loaded {} from \"{}\"", key_files[which].name, path);

    return 1;
}

int has_complete_keyset(const Keys* keys) {
    return keys->loaded[KEY_CARD_KEY_STORE] && keys->loaded[KEY_ENCRYPTED_KEY_STORE] && keys->loaded[KEY_STORE_KEY];
}

static void read_key_store(Keys* keys, int mode) {
    uint16_t* store = (uint16_t*)&keys->store;

    uint32_t at = 0;
    uint32_t index = 0;

    for (int i = 0; i < 18; i++) {
        uint16_t key_index = memory_card_key_indexes[(18 * mode) + i];

        if (key_index >= 0x200) {
            if (key_index == 0xFFFF) {
                for (int j = 0; j < 4; j++) {
                    store[at++] = 0;
                }
            } else {
                for (int j = 0; j < 4; j++) {
                    store[at++] = keys->card_key_store[(4 * (uint8_t)key_index) + j];
                }
            }
        } else {
            index = key_index;

            for (int j = 0; j < 4; j++) {
                store[at++] = keys->encrypted_key_store[index++];
            }
        }
    }

    index = kelf_keys_index[mode];

    for (int i = 0; i < 19; i++) {
        for (int j = 0; j < 4; j++) {
            store[at++] = keys->encrypted_key_store[index++];
        }
    }

    index = 192;

    for (int j = 0; j < 4; j++) {
        store[at++] = keys->encrypted_key_store[index++];
    }
}

int derive(Keys* keys, int mode, const uint8_t* console_id, const uint8_t* ilink_id) {
    keys->ready = 0;

    if (!has_complete_keyset(keys)) {
        return 0;
    }

    if (mode < KEY_STORE_MODE_DEV || mode > KEY_STORE_MODE_ARCADE) {
        return 0;
    }

    keys->mode = mode;

    read_key_store(keys, mode);

    uint8_t* store = (uint8_t*)&keys->store;

    for (int i = 0; i < (int)(sizeof(KeyStore) / 8); i++) {
        des::double_decrypt(keys->store_key, store + (i * 8));
    }

    uint8_t seed[8];
    uint8_t low[8];
    uint8_t hi[8];

    des::xor_bytes(ilink_id, console_id, seed, 8);
    des::xor_bytes(seed, keys->store.icvps2_low_iv, low, 8);
    des::xor_bytes(seed, keys->store.icvps2_hi_iv, hi, 8);

    des::double_encrypt(keys->store.icvps2_low_key, low);
    des::double_encrypt(keys->store.icvps2_hi_key, hi);

    memcpy(keys->icvps2_key, low, 8);
    memcpy(keys->icvps2_key + 8, hi, 8);

    memcpy(keys->console_id, console_id, 8);
    memcpy(keys->ilink_id, ilink_id, 8);

    keys->ready = 1;

    return 1;
}

void reset(State* state) {
    memset(state, 0, sizeof(State));

    state->state = MECHA_STATE_READY;
    state->result = MECHA_RESULT_0;
    state->random_seed = 0x12345678;
}

static uint32_t next_random(State* state) {
    state->random_seed = (state->random_seed * 1103515245u) + 12345u;

    return (state->random_seed >> 16) & 0xff;
}

static int generate_card_challenge(Keys* keys, State* state) {
    uint8_t seed[8];

    des::xor_bytes(state->memcard_iv, state->memcard_seed, seed, 8);

    des::xor_bytes(keys->store.card_iv[state->card_key_index], seed, state->memcard_key, 8);
    des::xor_bytes(keys->store.card_iv2[state->card_key_index], seed, state->memcard_key + 8, 8);

    uint8_t key1[16];
    uint8_t key2[16];

    memcpy(key1, keys->store.card_key_low[state->card_key_index], 8);
    memcpy(key1 + 8, keys->store.card_key_hi[state->card_key_index], 8);
    memcpy(key2, keys->store.card_key2_low[state->card_key_index], 8);
    memcpy(key2 + 8, keys->store.card_key2_hi[state->card_key_index], 8);

    des::double_encrypt(key1, state->memcard_key);
    des::double_encrypt(key2, state->memcard_key + 8);

    for (int i = 0; i < 8; i++) {
        state->memcard_random[i] = (uint8_t)next_random(state);
    }

    des::xor_bytes(keys->store.challenge_iv, state->memcard_random, state->memcard_challenge1, 8);
    des::double_encrypt(state->memcard_key, state->memcard_challenge1);

    des::xor_bytes(state->memcard_nonce, state->memcard_challenge1, state->memcard_challenge2, 8);
    des::double_encrypt(state->memcard_key, state->memcard_challenge2);

    des::xor_bytes(state->memcard_iv, state->memcard_challenge2, state->memcard_challenge3, 8);
    des::double_encrypt(state->memcard_key, state->memcard_challenge3);

    return MECHA_RESULT_CARD_CHALLENGE_GENERATED;
}

static int verify_card_challenge(Keys* keys, State* state) {
    uint8_t response[8];

    memcpy(response, state->memcard_response1, 8);

    des::double_decrypt(state->memcard_key, response);
    des::xor_bytes(response, keys->store.challenge_iv, response, 8);

    if (memcmp(state->memcard_nonce, response, 8) != 0) {
        iris_error(keys, "Invalid card response 1 (key store mode {}, card key index {})",
            keys->mode, state->card_key_index);

        return MECHA_RESULT_FAILED;
    }

    memcpy(response, state->memcard_response2, 8);

    des::double_decrypt(state->memcard_key, response);
    des::xor_bytes(response, state->memcard_response1, response, 8);

    if (memcmp(state->memcard_random, response, 8) != 0) {
        iris_error(keys, "Invalid card response 2 (key store mode {}, card key index {})",
            keys->mode, state->card_key_index);

        return MECHA_RESULT_FAILED;
    }

    memcpy(response, state->memcard_response3, 8);

    des::double_decrypt(state->memcard_key, response);
    des::xor_bytes(state->memcard_response2, response, state->card_key[state->card_key_slot], 8);

    return MECHA_RESULT_CARD_VERIFIED;
}

static int fail_at(Keys* keys, State* state, uint8_t code, int line) {
    // iris_warning(keys, "KELF rejected at mg.cpp: {} (error {:02x}, state {}, mode {})", line, code, state->state, state->mode);

    state->errorcode = code;

    return MECHA_RESULT_FAILED;
}

#define fail(state, code) fail_at(keys, state, code, __LINE__)

static int decrypt_kelf_header(Keys* keys, State* state) {
    KelfHeader* header = (KelfHeader*)state->data_buffer;

    if (state->data_size < sizeof(KelfHeader)) {
        return fail(state, 0x81);
    }

    // iris_debug(keys, "KELF header: mode {}, {} bytes, system {:02x}, app {:02x}, flags {:04x}, {} ban(s)",
    //     state->mode, state->data_size, header->system_type, header->application_type,
    //     header->flags, header->ban_count
    // );

    uint32_t header_size = (uint32_t)(sizeof(KelfHeader) + (sizeof(ConsoleBan) * header->ban_count));

    if (header_size > state->data_size) {
        return fail(state, 0x81);
    }

    if (header->flags & 1) {
        if (header_size >= state->data_size) {
            return fail(state, 0x81);
        }

        header_size += state->data_buffer[header_size] + 1;

        if (header_size > state->data_size) {
            return fail(state, 0x81);
        }
    }

    uint8_t signature[8];

    if (header_size > state->data_size - sizeof(signature))
        return fail(state, 0x81);

    memset(signature, 0, sizeof(signature));

    for (uint32_t i = 0; i < (header_size & 0xfffffff8); i += 8) {
        des::xor_bytes(state->data_buffer + i, signature, signature, 8);
        des::encrypt(keys->store.signature_master_key, signature);
    }

    des::decrypt(keys->store.signature_hash_key, signature);
    des::encrypt(keys->store.signature_master_key, signature);

    if (memcmp(signature, state->data_buffer + header_size, 8) != 0) {
        iris_error(keys, "Invalid KELF header signature");

        return fail(state, 0x84);
    }

    if (header->header_size != state->data_size) {
        return fail(state, 0x81);
    }

    if (state->mode == 3 && !(header->flags & 4) && !(header->flags & 8)) {
        return fail(state, 0x82);
    }

    ConsoleBan* bans = (ConsoleBan*)(state->data_buffer + sizeof(KelfHeader));

    for (int i = 0; i < header->ban_count; i++) {
        if (memcmp(bans[i].ilink_id, keys->ilink_id, 8) == 0 && memcmp(bans[i].console_id, keys->console_id, 8) == 0) {
            return fail(state, 0x85);
        }
    }

    uint32_t offset = header_size + (uint32_t)sizeof(signature);

    uint8_t kbit[16];

    if (offset > state->data_size - 32u)
        return fail(state, 0x81);

    if (state->mode == 1 || state->mode == 3) {
        memcpy(kbit, state->data_buffer + offset, 16);
        offset += 16;

        memcpy(state->kc, state->data_buffer + offset, 16);
        offset += 16;

        des::decrypt(state->card_key[state->card_key_slot], kbit);
        des::decrypt(state->card_key[state->card_key_slot], kbit + 8);
        des::decrypt(state->card_key[state->card_key_slot], state->kc);
        des::decrypt(state->card_key[state->card_key_slot], state->kc + 8);
    } else {
        uint8_t nonce[8];

        des::xor_bytes(state->data_buffer, state->data_buffer + 8, nonce, 8);

        uint8_t kek[16];

        des::xor_bytes(keys->store.kbit_iv, nonce, kek, 8);
        des::double_encrypt(keys->store.kbit_master_key, kek);
        des::xor_bytes(keys->store.kc_iv, nonce, kek + 8, 8);
        des::double_encrypt(keys->store.kc_master_key, kek + 8);

        memcpy(kbit, state->data_buffer + offset, 16);
        offset += 16;
    
        memcpy(state->kc, state->data_buffer + offset, 16);
        offset += 16;

        des::double_decrypt(kek, kbit);
        des::double_decrypt(kek, kbit + 8);
        des::double_decrypt(kek, state->kc);
        des::double_decrypt(kek, state->kc + 8);
    }

    if (header->application_type == ARCADE_KELF_APPLICATION_TYPE && keys->mode == KEY_STORE_MODE_ARCADE) {
        if (!keys->loaded[KEY_ARCADE_KELF_KBIT] || !keys->loaded[KEY_ARCADE_KELF_KC]) {
            iris_error(keys, "Arcade KELF override keys are required for application type {:02x}", ARCADE_KELF_APPLICATION_TYPE);

            return fail(state, 0x81);
        }

        memcpy(kbit, keys->arcade_kelf_kbit, 16);
        memcpy(state->kc, keys->arcade_kelf_kc, 16);
    }

    if (offset > state->data_size - 8u)
        return fail(state, 0x81);

    state->bit_table = (BitTable*)(state->data_buffer + offset);

    uint8_t even[8];

    memcpy(even, state->bit_table, 8);

    des::double_decrypt(kbit, (uint8_t*)state->bit_table);
    des::xor_bytes(keys->store.content_table_iv, (uint8_t*)state->bit_table, (uint8_t*)state->bit_table, 8);

    uint32_t table_length = (uint32_t)(sizeof(state->bit_table->header_size) + sizeof(state->bit_table->block_count) +
        sizeof(state->bit_table->gap) + (sizeof(BitBlock) * state->bit_table->block_count));

    uint32_t remaining = state->data_size - offset;

    if (table_length > remaining || remaining - table_length < 16)
        return fail(state, 0x81);

    state->last_bit_table = 0;

    int signed_blocks = 0;
    uint32_t table_total = 0;

    for (int i = 0; i < state->bit_table->block_count; i++) {
        BitBlock* block = &state->bit_table->blocks[i];

        uint8_t odd[8];

        memcpy(odd, block, 8);
    
        des::double_decrypt(kbit, (uint8_t*)block);
        des::xor_bytes(even, (uint8_t*)block, (uint8_t*)block, 8);

        memcpy(even, block->signature, 8);
    
        des::double_decrypt(kbit, block->signature);
        des::xor_bytes(odd, block->signature, block->signature, 8);

        table_total += block->size;

        if (block->flags & (BIT_BLOCK_SIGNED | BIT_BLOCK_ENCRYPTED)) {
            if (state->last_bit_table >= MAX_BIT_BLOCKS)
                return fail(state, 0x81);

            state->bit_blocks[state->last_bit_table].flags = (uint8_t)block->flags;
            state->bit_blocks[state->last_bit_table].size = block->size;

            memcpy(state->bit_blocks[state->last_bit_table].signature, block->signature, 8);

            state->last_bit_table++;

            if (block->flags & BIT_BLOCK_SIGNED)
                signed_blocks++;
        }
    }

    if (!signed_blocks)
        return fail(state, 0x81);

    uint8_t table_signature[8];

    memcpy(table_signature, kbit, 8);

    if (memcmp(kbit, kbit + 8, 8) != 0)
        des::xor_bytes(kbit + 8, table_signature, table_signature, 8);

    des::xor_bytes(state->kc, table_signature, table_signature, 8);

    if (memcmp(state->kc, state->kc + 8, 8) != 0)
        des::xor_bytes(state->kc + 8, table_signature, table_signature, 8);

    for (int i = 0; i < (state->bit_table->block_count * 2) + 1; i++) {
        des::xor_bytes(((uint8_t*)state->bit_table) + (i * 8), table_signature, table_signature, 8);
    }

    uint8_t master_hash_key[16];

    memcpy(master_hash_key, keys->store.signature_master_key, 8);
    memcpy(master_hash_key + 8, keys->store.signature_hash_key, 8);

    des::double_encrypt(master_hash_key, table_signature);

    if (memcmp(state->data_buffer + offset + 8 + (state->bit_table->block_count * 16), table_signature, 8) != 0) {
        iris_error(keys, "Invalid BIT table signature");

        return fail(state, 0x84);
    }

    state->bit_length = (uint16_t)((16 * state->bit_table->block_count) + 8);

    uint8_t root[8];

    memcpy(root, signature, 8);

    des::encrypt(keys->store.root_sig_master_key, root);
    des::xor_bytes(table_signature, root, root, 8);
    des::encrypt(keys->store.root_sig_master_key, root);

    for (int i = 0; i < state->last_bit_table; i++) {
        if (state->bit_blocks[i].flags & BIT_BLOCK_SIGNED) {
            des::xor_bytes(state->bit_blocks[i].signature, root, root, 8);
            des::encrypt(keys->store.root_sig_master_key, root);
        }
    }

    uint8_t root_source[8];

    memcpy(root_source, root, 8);

    if ((state->mode == 1 || state->mode == 3) && (header->flags & 2)) {
        des::double_decrypt(keys->icvps2_key, root);
    } else {
        des::double_decrypt(keys->store.root_sig_hash_key, root);
    }

    if (memcmp(state->data_buffer + offset + 8 + (state->bit_table->block_count * 16) + 8, root, 8) != 0) {
        iris_error(keys, "Invalid root signature");

        if ((state->mode == 1 || state->mode == 3) && (header->flags & 2))
            return fail(state, 0x83);

        return fail(state, 0x84);
    }

    if (state->mode == 2 && (header->flags & 2)) {
        memcpy(state->pub_icvps2, root_source, 8);

        des::double_decrypt(keys->icvps2_key, state->pub_icvps2);
    }

    if (state->mode == 2 || state->mode == 3) {
        uint8_t slot = state->mode == 2 ? state->card_key_slot : state->mode3_key_index;

        memcpy(state->pub_kbit, kbit, 16);
        memcpy(state->pub_kc, state->kc, 16);

        des::encrypt(state->card_key[slot], state->pub_kbit);
        des::encrypt(state->card_key[slot], state->pub_kbit + 8);
        des::encrypt(state->card_key[slot], state->pub_kc);
        des::encrypt(state->card_key[slot], state->pub_kc + 8);
    }

    // iris_debug(keys, "KELF bit table: {} of {} block(s) usable, {} signed, blocks total {} bytes, header says {}",
    //     state->last_bit_table, state->bit_table->block_count, signed_blocks,
    //     table_total, header->content_size);

    memcpy(&state->verified_header, state->data_buffer, sizeof(KelfHeader));

    state->done_blocks = 0;
    state->current_block = 0;

    if (state->mode == 2 || state->mode == 3) {
        while (state->current_block < state->last_bit_table &&
            !(state->bit_blocks[state->current_block].flags & BIT_BLOCK_SIGNED)) {
            state->current_block++;
        }
    }

    return MECHA_RESULT_KELF_HEADER_VERIFIED;
}

static int decrypt_kelf_content(Keys* keys, State* state) {
    uint8_t* buffer = state->data_buffer;

    if (!state->done_blocks) {
        memcpy(state->content_last_ciphertext, keys->store.content_iv, 8);
        memset(state->signature_last_ciphertext, 0, 8);
    }

    int done = 0;

    if (state->bit_blocks[state->current_block].flags & BIT_BLOCK_ENCRYPTED) {
        while (done < state->data_size && done < state->data_buffer_offset) {
            int crypto = (state->verified_header.flags >> 8) & 0xf;
            int keys_used = (state->verified_header.flags >> 4) & 0xf;

            if (crypto == 1) {
                if (keys_used == 1) {
                    des::decrypt(state->kc, buffer);
                } else if (keys_used == 2) {
                    des::double_decrypt(state->kc, buffer);
                }
            } else if (crypto == 2) {
                uint8_t temp[8];

                memcpy(temp, buffer, 8);

                if (keys_used == 1) {
                    des::decrypt(state->kc, temp);
                } else if (keys_used == 2) {
                    des::double_decrypt(state->kc, temp);
                }

                des::xor_bytes(state->content_last_ciphertext, temp, temp, 8);

                memcpy(state->content_last_ciphertext, buffer, 8);
                memcpy(buffer, temp, 8);
            } else {
                return fail(state, 0x81);
            }

            if (state->bit_blocks[state->current_block].flags & BIT_BLOCK_SIGNED)
                des::xor_bytes(buffer, state->signature_last_ciphertext, state->signature_last_ciphertext, 8);

            done += 8;
            buffer += 8;
        }
    } else {
        while (done < state->data_size && done < state->data_buffer_offset) {
            des::xor_bytes(buffer, state->signature_last_ciphertext, state->signature_last_ciphertext, 8);
            des::encrypt(keys->store.signature_master_key, state->signature_last_ciphertext);

            done += 8;
            buffer += 8;
        }
    }

    state->done_blocks += done;

    if (state->bit_blocks[state->current_block].size <= state->done_blocks) {
        state->done_blocks = 0;

        if (state->bit_blocks[state->current_block].flags & BIT_BLOCK_ENCRYPTED) {
            uint8_t master_hash_key[16];

            memcpy(master_hash_key, keys->store.signature_master_key, 8);
            memcpy(master_hash_key + 8, keys->store.signature_hash_key, 8);

            des::double_encrypt(master_hash_key, state->signature_last_ciphertext);
        } else {
            des::decrypt(keys->store.signature_hash_key, state->signature_last_ciphertext);
            des::encrypt(keys->store.signature_master_key, state->signature_last_ciphertext);
        }

        if (state->bit_blocks[state->current_block].flags & BIT_BLOCK_SIGNED) {
            if (memcmp(state->bit_blocks[state->current_block].signature, state->signature_last_ciphertext, 8) != 0) {
                iris_error(keys, "KELF content signature error");

                return fail(state, 0x84);
            }
        }

        if (state->mode == 2 || state->mode == 3) {
            do {
                state->current_block++;
            } while (state->current_block < state->last_bit_table &&
                !(state->bit_blocks[state->current_block].flags & BIT_BLOCK_SIGNED));
        } else {
            state->current_block++;
        }
    }

    // iris_debug(keys, "block {} of {}, {} bytes done", state->current_block, state->last_bit_table, state->done_blocks);

    return MECHA_RESULT_KELF_CONTENT_DECRYPTED;
}

static void execute_handler(Keys* keys, State* state) {
    switch (state->state) {
        case MECHA_STATE_CARD_NONCE_SET: {
            state->result = generate_card_challenge(keys, state);
        } break;

        case MECHA_STATE_CARD_RESPONSE3_RECEIVED: {
            memset(state->card_key[state->card_key_slot], 0xaa, 8);

            state->result = verify_card_challenge(keys, state);
        } break;

        case MECHA_STATE_KELF_HEADER_RECEIVED: {
            state->result = decrypt_kelf_header(keys, state);
        } break;

        case MECHA_STATE_DATA_IN_LENGTH_SET:
        case MECHA_STATE_KELF_CONTENT_RECEIVED: {
            state->result = decrypt_kelf_content(keys, state);
        } break;
    }
}

int command(Keys* keys, State* state, uint8_t cmd, const uint8_t* params, int param_count, uint8_t* result, int* result_size) {
    if (cmd < 0x80 || cmd > 0x98)
        return 0;

    if (!keys->ready)
        return 0;

    memset(result, 0, 16);

    *result_size = 1;
    result[0] = 0x80;

    switch (cmd) {
        case 0x80: {
            if (state->state && param_count == 1) {
                state->state = MECHA_STATE_READY;
                state->result = MECHA_RESULT_0;
                state->errorcode = 0;

                if (params[0] < 0x10)
                    result[0] = 0;
            }
        } break;

        case 0x81: {
            if (state->state && param_count == 1) {
                state->state = MECHA_STATE_READY;

                uint8_t slot = params[0] & 0x3f;
                uint8_t index = (params[0] >> 6) & 3;

                if (slot < 0x10 && index != 3) {
                    state->card_key_slot = slot;
                    state->card_key_index = index;
                    state->state = MECHA_STATE_KEY_INDEXES_SET;

                    result[0] = 0;
                }
            }
        } break;

        case 0x82: {
            if (state->state == MECHA_STATE_KEY_INDEXES_SET && param_count == 16) {
                memcpy(state->memcard_iv, params, 8);
                memcpy(state->memcard_seed, params + 8, 8);

                state->state = MECHA_STATE_CARD_IV_SEED_SET;

                result[0] = 0;
            } else {
                state->state = MECHA_STATE_READY;
            }
        } break;

        case 0x83: {
            if (state->state == MECHA_STATE_CARD_IV_SEED_SET && param_count == 8) {
                memcpy(state->memcard_nonce, params, 8);

                state->state = MECHA_STATE_CARD_NONCE_SET;

                execute_handler(keys, state);

                result[0] = 0;
            } else {
                state->state = MECHA_STATE_READY;
            }
        } break;

        case 0x84: {
            if (state->state == MECHA_STATE_CARD_CHALLENGE_GENERATED && param_count == 0) {
                *result_size = 1 + 8 + 4;

                result[0] = 0;

                memcpy(result + 1, state->memcard_challenge1, 8);
                memcpy(result + 9, state->memcard_challenge2, 4);

                state->state = MECHA_STATE_CARD_CHALLENGE12_SENT;
            } else {
                state->state = MECHA_STATE_READY;
            }
        } break;

        case 0x85: {
            if (state->state == MECHA_STATE_CARD_CHALLENGE12_SENT && param_count == 0) {
                *result_size = 1 + 4 + 8;

                result[0] = 0;

                memcpy(result + 1, state->memcard_challenge2 + 4, 4);
                memcpy(result + 5, state->memcard_challenge3, 8);

                state->state = MECHA_STATE_CARD_CHALLENGE23_SENT;
            }
        } break;

        case 0x86: {
            if (state->state == MECHA_STATE_CARD_CHALLENGE23_SENT && param_count == 16) {
                memcpy(state->memcard_response1, params, 8);
                memcpy(state->memcard_response2, params + 8, 8);

                state->state = MECHA_STATE_CARD_RESPONSE12_RECEIVED;

                result[0] = 0;
            } else {
                state->state = MECHA_STATE_READY;
            }
        } break;

        case 0x87: {
            if (state->state == MECHA_STATE_CARD_RESPONSE12_RECEIVED && param_count == 8) {
                memcpy(state->memcard_response3, params, 8);

                state->state = MECHA_STATE_CARD_RESPONSE3_RECEIVED;

                execute_handler(keys, state);

                result[0] = 0;
            } else {
                state->state = MECHA_STATE_READY;
            }
        } break;

        case 0x88: {
            if (state->state == MECHA_STATE_CARD_VERIFIED && param_count == 0)
                result[0] = 0;
        } break;

        case 0x8c: {
            execute_handler(keys, state);

            state->result = MECHA_RESULT_0;
            state->state = MECHA_STATE_READY;

            result[0] = 0;
        } break;

        case 0x8d: {
            int writing = state->state == MECHA_STATE_KELF_HEADER_PARAMS_SET ||
                state->state == MECHA_STATE_DATA_IN_LENGTH_SET;

            if (param_count && writing && state->data_buffer_offset + param_count <= 0x800) {
                memcpy(state->data_buffer + state->data_buffer_offset, params, param_count);

                state->data_buffer_offset += (uint16_t)param_count;

                if (state->data_size <= state->data_buffer_offset) {
                    if (state->state == MECHA_STATE_KELF_HEADER_PARAMS_SET) {
                        state->state = MECHA_STATE_KELF_HEADER_RECEIVED;
                    } else {
                        state->state = MECHA_STATE_KELF_CONTENT_RECEIVED;
                    }

                    execute_handler(keys, state);
                }

                result[0] = 0;
            }
        } break;

        case 0x8e: {
            int reading = state->state == MECHA_STATE_BIT_LENGTH_SENT ||
                state->state == MECHA_STATE_DATA_OUT_LENGTH_SET;

            if (param_count == 0 && reading && state->data_out_offset <= state->data_size) {
                uint16_t length = state->data_size - state->data_out_offset;

                if (length > 0x10)
                    length = 0x10;

                *result_size = length;

                for (uint16_t i = 0; i < length; i++) {
                    result[i] = *state->data_out_ptr++;
                }

                state->data_out_offset += length;

                if (state->data_size <= state->data_out_offset) {
                    if (state->state == MECHA_STATE_BIT_LENGTH_SENT) {
                        state->state = MECHA_STATE_KELF_CONTENT_DECRYPT_IN_PROGRESS;
                    } else if (state->current_block >= state->last_bit_table) {
                        state->state = MECHA_STATE_READY;
                    } else {
                        state->state = MECHA_STATE_KELF_CONTENT_DECRYPT_IN_PROGRESS;
                    }
                }
            } else {
                *result_size = 0;

                state->state = MECHA_STATE_READY;
            }
        } break;

        case 0x8f: {
            if (param_count)
                break;

            switch (state->state) {
                case MECHA_STATE_CARD_NONCE_SET:
                case MECHA_STATE_CARD_CHALLENGE_GENERATED: {
                    if (state->result == MECHA_RESULT_CARD_CHALLENGE_GENERATED) {
                        state->state = MECHA_STATE_CARD_CHALLENGE_GENERATED;

                        result[0] = 0;
                    }
                } break;

                case MECHA_STATE_CARD_RESPONSE3_RECEIVED:
                case MECHA_STATE_CARD_VERIFIED: {
                    if (state->result == MECHA_RESULT_CARD_VERIFIED) {
                        state->state = MECHA_STATE_CARD_VERIFIED;

                        result[0] = 0;
                    }
                } break;

                case MECHA_STATE_KELF_HEADER_RECEIVED:
                case MECHA_STATE_KELF_HEADER_VERIFIED: {
                    if (state->result == MECHA_RESULT_KELF_HEADER_VERIFIED) {
                        state->state = MECHA_STATE_KELF_HEADER_VERIFIED;

                        result[0] = 0;
                    } else if (state->result == MECHA_RESULT_FAILED) {
                        state->state = MECHA_STATE_READY;

                        result[0] = state->errorcode;
                    }
                } break;

                case MECHA_STATE_DATA_IN_LENGTH_SET:
                case MECHA_STATE_UNK17:
                case MECHA_STATE_KELF_CONTENT_RECEIVED: {
                    if (state->result == MECHA_RESULT_KELF_CONTENT_DECRYPTED) {
                        if (state->mode == 2 || state->mode == 3) {
                            if (state->current_block >= state->last_bit_table) {
                                state->state = MECHA_STATE_KELF_CONTENT_DECRYPT_DONE;
                            } else {
                                state->state = MECHA_STATE_KELF_CONTENT_DECRYPT_IN_PROGRESS;
                            }
                        } else {
                            state->state = MECHA_STATE_UNK17;
                        }

                        result[0] = 0;
                    } else if (state->result == MECHA_RESULT_FAILED) {
                        state->state = MECHA_STATE_READY;

                        result[0] = state->errorcode;
                    }
                } break;
            }
        } break;

        case 0x90: {
            if (state->state && param_count == 5) {
                state->mode = params[0];
                state->data_size = (uint16_t)(params[1] | (params[2] << 8));
                state->card_key_slot = params[3];
                state->mode3_key_index = params[4];
                state->data_buffer_offset = 0;
                state->state = MECHA_STATE_READY;

                if (state->mode <= 3 && state->data_size <= 0x800) {
                    if (state->mode == 0 ||
                        (state->card_key_slot <= 0x10 &&
                            ((state->mode == 1 || state->mode == 2) || state->mode3_key_index < 0x10))) {
                        state->state = MECHA_STATE_KELF_HEADER_PARAMS_SET;

                        result[0] = 0;
                    }
                }
            } else {
                state->state = MECHA_STATE_READY;
            }
        } break;

        case 0x91: {
            if (state->state == MECHA_STATE_KELF_HEADER_VERIFIED && param_count == 0) {
                *result_size = 3;

                result[0] = 0;
                result[1] = (uint8_t)state->bit_length;
                result[2] = (uint8_t)(state->bit_length >> 8);

                state->data_size = state->bit_length;
                state->data_out_offset = 0;
                state->data_out_ptr = (uint8_t*)state->bit_table;

                state->state = MECHA_STATE_BIT_LENGTH_SENT;
            } else {
                state->state = MECHA_STATE_READY;
            }
        } break;

        case 0x92: {
            if (state->state == MECHA_STATE_KELF_CONTENT_DECRYPT_IN_PROGRESS && param_count == 2) {
                state->data_size = (uint16_t)(params[0] | (params[1] << 8));

                uint32_t remaining = state->bit_blocks[state->current_block].size - state->done_blocks;

                if (remaining > 0x800)
                    remaining = 0x800;

                if (state->data_size == remaining) {
                    state->data_buffer_offset = 0;
                    state->state = MECHA_STATE_DATA_IN_LENGTH_SET;

                    result[0] = 0;
                }
            } else {
                state->state = MECHA_STATE_READY;
            }
        } break;

        case 0x93: {
            if (state->state == MECHA_STATE_UNK17 && param_count == 2) {
                uint16_t length = (uint16_t)(params[0] | (params[1] << 8));

                if (length == state->data_size) {
                    state->data_out_offset = 0;
                    state->data_out_ptr = state->data_buffer;
                    state->state = MECHA_STATE_DATA_OUT_LENGTH_SET;

                    result[0] = 0;
                }
            }
        } break;

        case 0x94:
        case 0x95:
        case 0x96:
        case 0x97:
        case 0x98: {
            static const struct {
                int required;
                int next;
            } steps[5] = {
                { MECHA_STATE_KELF_CONTENT_DECRYPT_DONE, MECHA_STATE_KBIT1_SENT },
                { MECHA_STATE_KBIT1_SENT, MECHA_STATE_KBIT2_SENT },
                { MECHA_STATE_KBIT2_SENT, MECHA_STATE_KC1_SENT },
                { MECHA_STATE_KC1_SENT, MECHA_STATE_KC2_SENT },
                { MECHA_STATE_KC2_SENT, MECHA_STATE_READY }
            };

            int step = cmd - 0x94;

            if (state->state == steps[step].required && param_count == 0) {
                const uint8_t* source = state->pub_kbit;

                if (cmd == 0x95) {
                    source = state->pub_kbit + 8;
                }

                if (cmd == 0x96) {
                    source = state->pub_kc;
                }

                if (cmd == 0x97) {
                    source = state->pub_kc + 8;
                }

                if (cmd == 0x98) {
                    source = state->pub_icvps2;
                }

                *result_size = 1 + 8;

                result[0] = 0;

                memcpy(result + 1, source, 8);

                state->state = steps[step].next;

                if (cmd == 0x97 && !(state->mode == 2 && (state->verified_header.flags & 2))) {
                    state->state = MECHA_STATE_READY;
                }
            } else {
                state->state = MECHA_STATE_READY;
            }
        } break;
    }

    return 1;
}

static std::string hex(const uint8_t* data, int size) {
    std::string out;

    for (int i = 0; i < size; i++) {
        out += fmt::format("{:02X}", data[i]);
    }

    return out;
}

}
