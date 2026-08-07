#pragma once

#include <string>

#include "applet.hpp"

namespace iris {

struct GsDumpTool : Applet {
    GsDumpTool() {
        id = "gs_dump_tool";
        title = "Capture GS dump";
        flags = ImGuiWindowFlags_AlwaysAutoResize;
        persist = false;
    }

    void on_open() override;
    void on_render() override;

    char filename[1024] = "";
    int frames = 1;
    int delay = 0;
    std::string serial = "";
};

}
