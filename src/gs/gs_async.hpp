#pragma once

#include <cstddef>
#include <cstdint>
#include <fenv.h>

namespace iris::gs::async {

struct Async;

using TransferFunc = void (*)(void* udata, int path, const void* data, size_t size, uint32_t flags);

Async* create();
void start(Async* async, const fenv_t& fp_env, int rounding);
void destroy(Async* async);

void set_backend(Async* async, void* udata, TransferFunc transfer);
void transfer(Async* async, int path, const void* data, size_t size, uint32_t flags);

bool is_idle(Async* async);
void sync(Async* async);

}
