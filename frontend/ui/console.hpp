#pragma once

#include <string>
#include <vector>

#include "applet.hpp"

namespace iris {

struct Console : Applet {
    Console() {
        id = "console";
        title = "Console";
        flags = ImGuiWindowFlags_MenuBar;
        persist = false;
    }

    void on_render() override;

    void rebuild_sources();

    int min_level = 0;
    bool autoscroll = true;
    bool colorize = true;
    bool wrap = false;
    char search[128] = "";
    char source_search[64] = "";

    std::vector <bool> sources = {};

    struct SourceEntry {
        std::string name;
        std::vector <size_t> ids;
        bool frontend;
    };

    std::vector <SourceEntry> source_entries = {};
};

}
