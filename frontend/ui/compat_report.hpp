#pragma once

#include <string>

#include "applet.hpp"

namespace iris {

struct CompatReport : Applet {
    CompatReport() {
        id = "compat_report";
        title = "Report compatibility";
        flags = ImGuiWindowFlags_AlwaysAutoResize;
        persist = false;
    }

    void on_open() override;
    void on_render() override;

    int rating = 3;
    char comment[1024] = "";
    std::string serial = "";
};

}
