#pragma once

namespace iris {

struct Instance;

#define IRIS_TITLEBAR_DEFAULT 0
#define IRIS_TITLEBAR_SEAMLESS 1

namespace platform {
    void init_console();
    bool init(Instance* iris);
    bool apply_settings(Instance* iris);
    void destroy(Instance* iris);
}

}
