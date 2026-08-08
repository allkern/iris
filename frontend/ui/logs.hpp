#pragma once

#include "applet.hpp"

namespace iris {

enum {
    LOG_EE,
    LOG_IOP,
    LOG_SYSMEM,
    LOG_COUNT
};

void show_log_view(Instance* iris, int source);

struct Logs : Applet {
    Logs() {
        id = "logs";
        title = "Logs";
        flags = ImGuiWindowFlags_MenuBar;
    }

    void on_render() override;

    int source = LOG_EE;
};

}
