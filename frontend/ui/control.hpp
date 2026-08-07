#pragma once

#include "applet.hpp"

namespace iris {

void show_ee_disassembly_view(Instance* iris);
void show_iop_disassembly_view(Instance* iris);

struct EeControl : Applet {
    EeControl() {
        id = "ee_control";
        title = "EE (R5900)";
        needs_ps2 = true;
    }

    bool begin() override;
    void end() override;
    void on_render() override;
};

struct IopControl : Applet {
    IopControl() {
        id = "iop_control";
        title = "IOP (R3000)";
        needs_ps2 = true;
    }

    bool begin() override;
    void end() override;
    void on_render() override;
};

}
