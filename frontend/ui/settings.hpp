#pragma once

#include <string>

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

    std::string bios_buf;
    std::string arcade_bios_bufs[emu::ARCADE_BIOS_COUNT];
    std::string rom1_buf;
    std::string rom2_buf;
    std::string nvram_buf;
    std::string hdd_buf;
    std::string flash_buf;
};

}
