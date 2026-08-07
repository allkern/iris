#pragma once

#include "applet.hpp"

namespace iris {

struct BiosSetting : Applet {
    BiosSetting() {
        id = "bios_setting_window";
        title = "Welcome";
        persist = false;
    }

    bool begin() override;
    void end() override;
    void on_render() override;

    void show_bios_stage();
    void show_memory_card_stage();

    int stage = 0;
    bool begun = false;

    bool bios_checked = false;
    int bios_valid = false;
    bool open_settings = true;
    char buf[512] = "";
};

}
