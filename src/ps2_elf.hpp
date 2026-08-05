#pragma once

#ifdef __linux__
#include <elf.h>
#else
#include "elf.h"
#endif

#include "ps2.hpp"

namespace iris::elf {

bool load(ps2::Ps2* ps2, const char* path);

}
