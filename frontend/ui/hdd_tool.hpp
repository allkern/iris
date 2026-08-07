#pragma once

#include "applet.hpp"

namespace iris {

enum : int {
    IMAGE_FMT_RAW,
    IMAGE_FMT_ISIF
};

struct HddTool : Applet {
    HddTool() {
        id = "hdd_tool";
        title = "HDD Tool";
        persist = false;
    }

    void on_render() override;

    int image_format = IMAGE_FMT_ISIF;
    int size_add = 0;
    bool assign = true;
};

}
