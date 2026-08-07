#pragma once

#include "applet.hpp"

namespace iris {

struct MemoryCardTool : Applet {
    MemoryCardTool() {
        id = "memory_card_tool";
        title = "Create memory card";
        flags = ImGuiWindowFlags_NoCollapse;
        persist = false;
    }

    bool begin() override;
    void on_render() override;
};

}
