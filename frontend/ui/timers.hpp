#pragma once

#include "applet.hpp"

namespace iris {

struct Timers : Applet {
    Timers() {
        id = "timers";
        title = "Timers";
        needs_ps2 = true;
    }

    void on_render() override;
};

}
