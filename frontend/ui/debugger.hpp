#pragma once

#include "applet.hpp"

namespace iris {

struct Debugger : Applet {
    Debugger() {
        id = "debugger";
        title = "Debugger";
        flags = ImGuiWindowFlags_MenuBar;
        needs_ps2 = true;
        persist = false;
    }

    bool begin() override;
    void end() override;
    void on_render() override;

    int cpu = 0;
    int log_source = 0;

    float left_width = 300.0f;
    float right_width = 460.0f;
    float disasm_height = 330.0f;
    float memory_height = 220.0f;
    bool show_left = true;
    bool show_memory = true;
    bool show_logs = true;
    bool memory_open = true;
    bool logs_open = true;
};

}
