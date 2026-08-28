#pragma once

#include "applet.hpp"

namespace iris {

struct MemoryViewer : Applet {
    MemoryViewer() {
        id = "memory_viewer";
        title = "Memory";
        flags = ImGuiWindowFlags_MenuBar;
        needs_ps2 = true;
    }

    void on_render() override;
};

}
