#pragma once

namespace iris {

struct Instance;

#define IRIS_TITLEBAR_DEFAULT 0
#define IRIS_TITLEBAR_SEAMLESS 1

#if defined(_WIN32) || defined(__APPLE__)
#define IRIS_HAS_DARK_TITLEBAR
#endif

namespace platform {
    void init_console();
    bool init(Instance* iris);
    bool apply_settings(Instance* iris);
    void destroy(Instance* iris);
}

}
