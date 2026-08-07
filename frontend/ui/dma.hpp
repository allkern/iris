#pragma once

#include "applet.hpp"

namespace iris {

struct EeDmac : Applet {
    EeDmac() {
        id = "ee_dmac";
        title = "EE DMAC";
        needs_ps2 = true;
    }

    void on_render() override;
};

struct IopDma : Applet {
    IopDma() {
        id = "iop_dma";
        title = "IOP DMA";
        needs_ps2 = true;
    }

    void on_render() override;
};

}
