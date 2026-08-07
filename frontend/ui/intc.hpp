#pragma once

#include "applet.hpp"

namespace iris {

struct EeInterrupts : Applet {
    EeInterrupts() {
        id = "ee_interrupts";
        title = "EE Interrupts";
        needs_ps2 = true;
    }

    void on_render() override;
};

struct IopInterrupts : Applet {
    IopInterrupts() {
        id = "iop_interrupts";
        title = "IOP Interrupts";
        needs_ps2 = true;
    }

    void on_render() override;
};

}
