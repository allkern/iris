#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include <regex>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

void show_ee_timers(iris::instance* iris) {
    using namespace ImGui;

    for (int i = 0; i < 4; i++) {
        auto& t = iris->ps2->ee_timers->timer[i];

        if (TreeNode((std::string("Timer ") + std::to_string(i)).c_str())) {
            Text("Counter: %04x", t.counter);
            Text("Mode: %04x", t.mode);
            Text("Compare: %04x", t.compare);
            Text("Hold: %04x", t.hold);

            TreePop();
        }
    }
}

void show_iop_timers(iris::instance* iris) {
    using namespace ImGui;

    for (int i = 0; i < 6; i++) {
        auto& t = iris->ps2->iop_timers->timer[i];

        if (TreeNode((std::string("Timer ") + std::to_string(i)).c_str())) {
            Text("Counter: %04x", t.counter);
            Text("Mode: %04x", t.mode);

            TreePop();
        }
    }
}

void show_scheduler(iris::instance* iris) {
    using namespace ImGui;

}

void show_timers(iris::instance* iris) {
    using namespace ImGui;

    if (imgui::BeginEx("Timers", &iris->show_timers)) {
        if (BeginTabBar("##timers_tab_bar")) {
            if (BeginTabItem("EE timers")) {
                show_ee_timers(iris);

                EndTabItem();
            }

            if (BeginTabItem("IOP timers")) {
                show_iop_timers(iris);

                EndTabItem();
            }

            if (BeginTabItem("Scheduler")) {
                show_scheduler(iris);

                EndTabItem();
            }
        }

        EndTabBar();
    } End();
}

}