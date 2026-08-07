#pragma once

#include "applet.hpp"

namespace iris {

struct SettingsWindow : Applet {
    SettingsWindow() {
        id = "settings";
        title = "Settings";
        flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking;
        persist = false;
    }

    bool begin() override;
    void end() override;
    void on_render() override;

    int selected = 0;
};

}
