#pragma once

#include "applet.hpp"

namespace iris {

struct VuDisassembler : Applet {
    VuDisassembler() {
        id = "vu_disassembler";
        title = "VU disassembler";
        flags = ImGuiWindowFlags_MenuBar;
        needs_ps2 = true;
    }

    bool begin() override;
    void end() override;
    void on_render() override;
};

}
