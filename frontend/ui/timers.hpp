#pragma once

#include "applet.hpp"

namespace iris {

struct Timers : Applet {
    Timers() {
        id = "timers";
        title = "Timers";
    }

    void on_render() override;
};

}
