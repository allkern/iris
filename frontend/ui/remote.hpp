#pragma once

#include <cstdint>

#include "applet.hpp"

namespace iris {

struct DvdRemote : Applet {
    DvdRemote() {
        id = "dvd_remote";
        title = "DVD Remote";
        flags =
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_AlwaysAutoResize;
        needs_ps2 = true;
    }

    void on_render() override;
    void on_close() override;

    void show_status();
    void show_remote();
    void set_held(uint32_t action);

    uint32_t held = 0xffffffff;
    uint32_t last_reads = 0;
    uint64_t last_read_tick = 0;
    uint32_t last_dvrp_reads = 0;
    uint64_t last_dvrp_tick = 0;
    uint32_t last_polls[2] = {};
    uint64_t last_poll_tick[2] = {};
};

}
