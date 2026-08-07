#pragma once

#include "applet.hpp"

namespace iris {

struct EeLogs : Applet {
    EeLogs() {
        id = "ee_logs";
        title = "EE logs";
    }

    void on_render() override;

    bool follow = true;
};

struct IopLogs : Applet {
    IopLogs() {
        id = "iop_logs";
        title = "IOP logs";
        flags = ImGuiWindowFlags_MenuBar;
    }

    void on_render() override;

    bool follow = true;
};

struct SysmemLogs : Applet {
    SysmemLogs() {
        id = "sysmem_logs";
        title = "SYSMEM logs";
        flags = ImGuiWindowFlags_MenuBar;
    }

    void on_render() override;

    bool follow = true;
};

}
