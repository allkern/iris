#pragma once

#include <string>

namespace iris {

struct Instance;

namespace patches {

std::string get_serial(const char* boot_path);
void apply(Instance* iris, const char* boot_path);
void reapply(Instance* iris);
void insert(Instance* iris);
void clear(Instance* iris);

}

}
