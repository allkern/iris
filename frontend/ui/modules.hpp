#pragma once

#include "applet.hpp"

namespace iris {

struct IopModules : Applet {
    IopModules() {
        id = "iop_modules";
        title = "IOP Modules";
        flags = ImGuiWindowFlags_MenuBar;
        needs_ps2 = true;
    }

    void on_render() override;
};

}
