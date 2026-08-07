#pragma once

#include "applet.hpp"

namespace iris {

struct EeThreads : Applet {
    EeThreads() {
        id = "ee_threads";
        title = "EE Threads";
        needs_ps2 = true;
    }

    void on_render() override;
};

struct IopThreads : Applet {
    IopThreads() {
        id = "iop_threads";
        title = "IOP Threads";
        needs_ps2 = true;
    }

    void on_render() override;
};

}
