/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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
