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

namespace iris::vu {

struct Vu;
struct Block;

}

namespace iris::vu::jit {

struct Jit;

Jit* create();
void destroy(Jit* jit);
void flush_blocks(Vu* vu);
void release_block(Jit* jit, Block* block);
void compile_block(Vu* vu, Block* block);
void flush_if_needed(Vu* vu);
void drain_blocks(Vu* vu);
void stash_block(Vu* vu, Block* block);
bool adopt_block(Vu* vu, Block* block);
void save_runs(Vu* vu, Block* block);
void restore_runs(Vu* vu, Block* block);
uint32_t victim_lengths(Vu* vu, uint32_t tpc, uint32_t* lengths, uint32_t capacity);
uint64_t blocks_compiled(Jit* jit);
uint64_t blocks_failed(Jit* jit);
void enter_block(Vu* vu, Block* block);
bool can_dispatch(Vu* vu);
bool dispatch(Vu* vu);

}
