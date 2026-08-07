#pragma once

#include "applet.hpp"

namespace iris {

struct About : Applet {
    About() {
        id = "about_window";
        title = "About";
        flags =
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_AlwaysAutoResize;
        persist = false;
    }

    bool begin() override;
    void on_render() override;
};

}
