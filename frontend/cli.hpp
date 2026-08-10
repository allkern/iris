#pragma once

#include <functional>
#include <string>
#include <vector>

namespace iris {

struct Instance;

namespace cli {

struct State {
    std::vector <std::function <void(Instance*)>> pending = {};
    std::vector <std::function <void(bool)>> overrides = {};
    std::vector <std::string> shaders = {};
    std::string open_path = "";
    bool portable = false;
    bool reset_settings = false;
};

bool quick_exit(int argc, const char* argv[]);
bool parse(Instance* iris, int argc, const char* argv[]);
void apply(Instance* iris);
void unapply(Instance* iris);
void reapply(Instance* iris);
void boot(Instance* iris);

void print_help();
void print_version();

}

}
