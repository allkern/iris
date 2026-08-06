#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace iris {

struct Instance;

namespace elf {

struct Symbol {
    char* name;
    uint32_t addr;
    uint32_t size;
};

bool load_symbols_from_disc(Instance* iris);
bool load_symbols_from_file(Instance* iris, std::string path);

}

}
