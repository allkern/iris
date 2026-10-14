/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include <regex>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "ps2.hpp"

namespace iris {

void show_ee_timers(Instance* iris) {
    using namespace ImGui;

    for (int i = 0; i < 4; i++) {
        auto& t = iris->ps2->ee_timers->timer[i];

        if (TreeNode((std::string("Timer ") + std::to_string(i)).c_str())) {
            Text("Counter: %04x", (uint32_t)t.counter);
            Text("Mode: %04x", t.mode);
            Text("Compare: %04x", t.compare);
            Text("Hold: %04x", t.hold);

            TreePop();
        }
    }
}

void show_iop_timers(Instance* iris) {
    using namespace ImGui;

    for (int i = 0; i < 6; i++) {
        auto& t = iris->ps2->iop_timers->timer[i];

        if (TreeNode((std::string("Timer ") + std::to_string(i)).c_str())) {
            Text("Counter: %04x", (uint32_t)t.counter);
            Text("Mode: %04x", t.mode);

            TreePop();
        }
    }
}

void show_scheduler(Instance* iris) {
    using namespace ImGui;

}

void Timers::on_render() {
    using namespace ImGui;

    if (BeginTabBar("##timers_tab_bar")) {
        if (imgui::BeginTabItem("EE timers")) {
            show_ee_timers(iris);

            EndTabItem();
        }

        if (imgui::BeginTabItem("IOP timers")) {
            show_iop_timers(iris);

            EndTabItem();
        }

        if (imgui::BeginTabItem("Scheduler")) {
            show_scheduler(iris);

            EndTabItem();
        }
    }

    EndTabBar();
}

}