#pragma once

#include "applet.hpp"

namespace iris {

struct Spu2Debugger : Applet {
    Spu2Debugger() {
        id = "spu2_debugger";
        title = "SPU2";
        needs_ps2 = true;
    }

    void on_render() override;
};

}
