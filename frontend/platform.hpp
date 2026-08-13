#pragma once

#include "menu.hpp"

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

    // Hands the menubar description to the OS. A no-op wherever ImGui draws it
    void set_menubar(Instance* iris, const std::vector <menu::Node>& nodes);
    void destroy(Instance* iris);
}

}
