#pragma once

namespace iris {

struct Instance;

namespace settings {
    bool init(Instance* iris, int argc, const char* argv[]);
    bool check_for_quick_exit(int argc, const char* argv[]);
    void close(Instance* iris);
    void apply_device_maps(Instance* iris);
}

}
