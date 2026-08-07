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
        needs_ps2 = true;
    }

    bool begin() override;
    void end() override;
    void on_open() override;
    void on_render() override;

    void sync_paths();
    bool paths_dirty() const;

    int selected = 0;

    char bios_buf[512] = "";
    char rom1_buf[512] = "";
    char rom2_buf[512] = "";
    char nvram_buf[512] = "";
    char hdd_buf[512] = "";
    char flash_buf[512] = "";
};

}
