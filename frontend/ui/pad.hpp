#pragma once

#include "applet.hpp"

namespace iris {

struct PadDebugger : Applet {
    PadDebugger() {
        id = "pad_debugger";
        title = "DualShock 2";
    }

    void on_render() override;
};

}
