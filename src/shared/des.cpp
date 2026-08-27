#include "des.hpp"

namespace iris::des {

static const uint8_t initial_permutation[64] = {
    58, 50, 42, 34, 26, 18, 10,  2, 60, 52, 44, 36, 28, 20, 12,  4,
    62, 54, 46, 38, 30, 22, 14,  6, 64, 56, 48, 40, 32, 24, 16,  8,
    57, 49, 41, 33, 25, 17,  9,  1, 59, 51, 43, 35, 27, 19, 11,  3,
    61, 53, 45, 37, 29, 21, 13,  5, 63, 55, 47, 39, 31, 23, 15,  7
};

static const uint8_t final_permutation[64] = {
    40,  8, 48, 16, 56, 24, 64, 32, 39,  7, 47, 15, 55, 23, 63, 31,
    38,  6, 46, 14, 54, 22, 62, 30, 37,  5, 45, 13, 53, 21, 61, 29,
    36,  4, 44, 12, 52, 20, 60, 28, 35,  3, 43, 11, 51, 19, 59, 27,
    34,  2, 42, 10, 50, 18, 58, 26, 33,  1, 41,  9, 49, 17, 57, 25
};

static const uint8_t expansion[48] = {
    32,  1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,
     8,  9, 10, 11, 12, 13, 12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21, 20, 21, 22, 23, 24, 25,
    24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32,  1
};

static const uint8_t permutation[32] = {
    16,  7, 20, 21, 29, 12, 28, 17,  1, 15, 23, 26,  5, 18, 31, 10,
     2,  8, 24, 14, 32, 27,  3,  9, 19, 13, 30,  6, 22, 11,  4, 25
};

static const uint8_t permuted_choice1[56] = {
    57, 49, 41, 33, 25, 17,  9,  1, 58, 50, 42, 34, 26, 18,
    10,  2, 59, 51, 43, 35, 27, 19, 11,  3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15,  7, 62, 54, 46, 38, 30, 22,
    14,  6, 61, 53, 45, 37, 29, 21, 13,  5, 28, 20, 12,  4
};

static const uint8_t permuted_choice2[48] = {
    14, 17, 11, 24,  1,  5,  3, 28, 15,  6, 21, 10,
    23, 19, 12,  4, 26,  8, 16,  7, 27, 20, 13,  2,
    41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32
};

static const uint8_t rotations[16] = {
    1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

static const uint8_t sbox[8][64] = {
    {
        14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
         0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,
         4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
        15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13
    },
    {
        15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
         3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,
         0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
        13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9
    },
    {
        10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
        13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,
        13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
         1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12
    },
    {
         7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
        13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,
        10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
         3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14
    },
    {
         2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
        14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,
         4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
        11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3
    },
    {
        12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
        10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,
         9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
         4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13
    },
    {
         4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
        13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,
         1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
         6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12
    },
    {
        13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
         1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,
         7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
         2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11
    }
};

static uint64_t permute(uint64_t value, const uint8_t* table, int in_bits, int out_bits) {
    uint64_t result = 0;

    for (int i = 0; i < out_bits; i++) {
        uint64_t bit = (value >> (in_bits - table[i])) & 1;

        result |= bit << (out_bits - 1 - i);
    }

    return result;
}

static uint64_t pack(const uint8_t* in) {
    uint64_t value = 0;

    for (int i = 0; i < 8; i++) {
        value = (value << 8) | in[i];
    }

    return value;
}

static void unpack(uint64_t value, uint8_t* out) {
    for (int i = 0; i < 8; i++) {
        out[i] = (uint8_t)(value >> (56 - (i * 8)));
    }
}

static uint32_t feistel(uint32_t half, uint64_t subkey) {
    uint64_t expanded = permute(half, expansion, 32, 48) ^ subkey;

    uint32_t result = 0;

    for (int i = 0; i < 8; i++) {
        uint8_t chunk = (uint8_t)((expanded >> (42 - (i * 6))) & 0x3f);

        uint8_t row = (uint8_t)(((chunk & 0x20) >> 4) | (chunk & 1));
        uint8_t column = (uint8_t)((chunk >> 1) & 0xf);

        result = (result << 4) | sbox[i][(row * 16) + column];
    }

    return (uint32_t)permute(result, permutation, 32, 32);
}

void init(Context* ctx, const uint8_t* key) {
    uint64_t permuted = permute(pack(key), permuted_choice1, 64, 56);

    uint32_t left = (uint32_t)((permuted >> 28) & 0x0fffffff);
    uint32_t right = (uint32_t)(permuted & 0x0fffffff);

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < rotations[i]; j++) {
            left = ((left << 1) | (left >> 27)) & 0x0fffffff;
            right = ((right << 1) | (right >> 27)) & 0x0fffffff;
        }

        uint64_t merged = ((uint64_t)left << 28) | right;

        ctx->subkey[i] = permute(merged, permuted_choice2, 56, 48);
    }
}

static void process(const Context* ctx, const uint8_t* in, uint8_t* out, int reverse) {
    uint64_t block = permute(pack(in), initial_permutation, 64, 64);

    uint32_t left = (uint32_t)(block >> 32);
    uint32_t right = (uint32_t)block;

    for (int i = 0; i < 16; i++) {
        uint64_t subkey = ctx->subkey[reverse ? 15 - i : i];

        uint32_t next = left ^ feistel(right, subkey);

        left = right;
        right = next;
    }

    uint64_t merged = ((uint64_t)right << 32) | left;

    unpack(permute(merged, final_permutation, 64, 64), out);
}

void encrypt_block(const Context* ctx, const uint8_t* in, uint8_t* out) {
    process(ctx, in, out, 0);
}

void decrypt_block(const Context* ctx, const uint8_t* in, uint8_t* out) {
    process(ctx, in, out, 1);
}

void encrypt(const uint8_t* key, uint8_t* data) {
    Context ctx;

    init(&ctx, key);
    encrypt_block(&ctx, data, data);
}

void decrypt(const uint8_t* key, uint8_t* data) {
    Context ctx;

    init(&ctx, key);
    decrypt_block(&ctx, data, data);
}

void double_encrypt(const uint8_t* key, uint8_t* data) {
    encrypt(key, data);
    decrypt(key + 8, data);
    encrypt(key, data);
}

void double_decrypt(const uint8_t* key, uint8_t* data) {
    decrypt(key, data);
    encrypt(key + 8, data);
    decrypt(key, data);
}

void xor_bytes(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t size) {
    for (size_t i = 0; i < size; i++) {
        out[i] = a[i] ^ b[i];
    }
}

}
