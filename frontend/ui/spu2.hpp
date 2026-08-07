#pragma once

#include "applet.hpp"

namespace iris {

struct Spu2Debugger : Applet {
    Spu2Debugger() {
        id = "spu2_debugger";
        title = "SPU2";
    }

    void on_render() override;
};

}
