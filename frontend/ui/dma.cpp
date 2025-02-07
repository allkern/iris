#include <vector>
#include <string>
#include <cctype>

#include "instance.hpp"

#include "res/IconsMaterialSymbols.h"

namespace lunar {

void show_ee_dmac(lunar::instance* lunar) {
    using namespace ImGui;

    if (Begin("EE DMAC", &lunar->show_ee_dmac)) {

    } End();
}

void show_iop_dma(lunar::instance* lunar) {
    using namespace ImGui;

    if (Begin("IOP DMA", &lunar->show_iop_dma)) {

    } End();
}

}