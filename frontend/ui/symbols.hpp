#pragma once

#include <vector>

#include "applet.hpp"
#include "elf.hpp"

namespace iris {

struct Symbols : Applet {
    Symbols() {
        id = "symbols";
        title = "Symbols";
        flags = ImGuiWindowFlags_MenuBar;
        persist = false;
    }

    void on_render() override;

    void filter(const char* text);

    std::vector <elf::Symbol> list = {};

    char search[512] = "";
    bool use_regex = false;
    bool case_sensitive = false;
    bool autosearch = true;

    int table_sizing_combo = 0;
    ImGuiTableFlags table_sizing = ImGuiTableFlags_SizingStretchProp;
};

}
