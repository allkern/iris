#pragma once

#include "applet.hpp"

namespace iris {

struct EeControl : Applet {
    EeControl() {
        id = "ee_control";
        title = "EE (R5900)";
    }

    bool begin() override;
    void end() override;
    void on_render() override;
};

struct IopControl : Applet {
    IopControl() {
        id = "iop_control";
        title = "IOP (R3000)";
    }

    bool begin() override;
    void end() override;
    void on_render() override;
};

}
