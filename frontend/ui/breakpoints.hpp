#pragma once

#include "applet.hpp"

namespace iris {

struct Breakpoints : Applet {
    Breakpoints() {
        id = "breakpoints";
        title = "Breakpoints";
        flags = ImGuiWindowFlags_MenuBar;
    }

    void on_render() override;
};

}
