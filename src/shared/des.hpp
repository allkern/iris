#pragma once

#include <cstdint>
#include <cstddef>

namespace iris::des {

inline constexpr auto BLOCK_SIZE = 8;
inline constexpr auto KEY_SIZE = 8;
inline constexpr auto DOUBLE_KEY_SIZE = 16;

struct Context {
    uint64_t subkey[16];
};

void init(Context* ctx, const uint8_t* key);
void encrypt_block(const Context* ctx, const uint8_t* in, uint8_t* out);
void decrypt_block(const Context* ctx, const uint8_t* in, uint8_t* out);

void encrypt(const uint8_t* key, uint8_t* data);
void decrypt(const uint8_t* key, uint8_t* data);

void double_encrypt(const uint8_t* key, uint8_t* data);
void double_decrypt(const uint8_t* key, uint8_t* data);

void xor_bytes(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t size);

}
