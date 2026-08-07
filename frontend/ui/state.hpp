#pragma once

#include "applet.hpp"

namespace iris {

struct EeState : Applet {
    EeState() {
        id = "ee_state";
        title = "EE state";
        flags = ImGuiWindowFlags_MenuBar;
    }

    void end() override;
    void on_render() override;
};

struct IopState : Applet {
    IopState() {
        id = "iop_state";
        title = "IOP state";
        flags = ImGuiWindowFlags_MenuBar;
    }

    void end() override;
    void on_render() override;
};

}
