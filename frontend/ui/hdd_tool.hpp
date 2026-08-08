#pragma once

#include "applet.hpp"

namespace iris {

enum ImageFormat : int {
    RAW,
    ISIF
};

struct HddTool : Applet {
    HddTool() {
        id = "hdd_tool";
        title = "HDD Tool";
        flags = ImGuiWindowFlags_AlwaysAutoResize;
        persist = false;
    }

    void on_render() override;

    int image_format = ImageFormat::ISIF;
    int size_add = 0;
    bool assign = true;
};

}
