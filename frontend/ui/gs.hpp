#pragma once

#include "applet.hpp"

namespace iris {

struct GsDebugger : Applet {
    GsDebugger() {
        id = "gs_debugger";
        title = "GS";
        flags = ImGuiWindowFlags_MenuBar;
    }

    void on_render() override;
};

}
