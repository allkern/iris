#pragma once

#include "applet.hpp"

namespace iris {

struct IopModules : Applet {
    IopModules() {
        id = "iop_modules";
        title = "IOP Modules";
        flags = ImGuiWindowFlags_MenuBar;
    }

    void on_render() override;
};

}
