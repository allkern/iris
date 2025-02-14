#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

void show_ee_dmac(iris::instance* iris) {
    using namespace ImGui;

    if (Begin("EE DMAC", &iris->show_ee_dmac)) {

    } End();
}

void show_iop_dma(iris::instance* iris) {
    using namespace ImGui;

    if (Begin("IOP DMA", &iris->show_iop_dma)) {

    } End();
}

}