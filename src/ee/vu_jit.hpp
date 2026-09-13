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
uint64_t blocks_compiled(Jit* jit);
uint64_t blocks_failed(Jit* jit);

}
