#pragma once

#include "applet.hpp"

namespace iris {

struct EeInterrupts : Applet {
    EeInterrupts() {
        id = "ee_interrupts";
        title = "EE Interrupts";
    }

    void on_render() override;
};

struct IopInterrupts : Applet {
    IopInterrupts() {
        id = "iop_interrupts";
        title = "IOP Interrupts";
    }

    void on_render() override;
};

}
