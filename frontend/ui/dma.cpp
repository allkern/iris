#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

void show_ee_dmac(Instance* iris) {
    using namespace ImGui;

    if (imgui::BeginEx("EE DMAC", &iris->ui.show_ee_dmac)) {

    } End();
}

void show_iop_dma(Instance* iris) {
    using namespace ImGui;

    if (imgui::BeginEx("IOP DMA", &iris->ui.show_iop_dma)) {

    } End();
}

}