#pragma once

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "iris.hpp"

namespace iris {

void load_elf_symbols_from_disc(iris::instance* iris);
void load_elf_symbols_from_file(iris::instance* iris, std::string path);

}