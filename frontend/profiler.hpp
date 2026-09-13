#pragma once

#include <cstdint>
#include <cstdio>

namespace iris::profiler {

void start();
void stop_and_report(FILE* out, int top, uint64_t frames);
bool is_running();

}
