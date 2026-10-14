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

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "ps2.hpp"

namespace iris {

void PadDebugger::on_render() {
    using namespace ImGui;

    if (BeginTabBar("##padtabbar")) {
        if (imgui::BeginTabItem("Slot 1")) {
            dev::ds::Ds* ds = iris->input.ds[0];

            if (!ds) {
                Text("No controller connected");
            } else {
                Text("Buttons: %04x", ds->buttons);
                Text("AxisRH: %04x", ds->ax_right_x);
                Text("AxisLH: %04x", ds->ax_left_x);
                Text("AxisRV: %04x", ds->ax_right_y);
                Text("AxisLV: %04x", ds->ax_left_y);
                Text("Config Mode: %d", ds->config_mode);
                Text("Active Index: %d", ds->act_index);
                Text("Mode Index: %d", ds->mode_index);
                Text("Mode: %d", ds->mode);
                Text("Vibration 0: %d", ds->vibration[0]);
                Text("Vibration 1: %d", ds->vibration[1]);
                Text("Mask 0: %d", ds->mask[0]);
                Text("Mask 1: %d", ds->mask[1]);
                Text("Lock: %d", ds->lock);
            }

            EndTabItem();
        }

        if (imgui::BeginTabItem("Slot 2")) {
            dev::ds::Ds* ds = iris->input.ds[1];

            if (!ds) {
                Text("No controller connected");
            } else {
                Text("Buttons: %04x", ds->buttons);
                Text("AxisRH: %04x", ds->ax_right_x);
                Text("AxisLH: %04x", ds->ax_left_x);
                Text("AxisRV: %04x", ds->ax_right_y);
                Text("AxisLV: %04x", ds->ax_left_y);
                Text("Config Mode: %d", ds->config_mode);
                Text("Active Index: %d", ds->act_index);
                Text("Mode Index: %d", ds->mode_index);
                Text("Mode: %d", ds->mode);
                Text("Vibration 0: %d", ds->vibration[0]);
                Text("Vibration 1: %d", ds->vibration[1]);
                Text("Mask 0: %d", ds->mask[0]);
                Text("Mask 1: %d", ds->mask[1]);
                Text("Lock: %d", ds->lock);
            }

            EndTabItem();
        }

        EndTabBar();
    }
}

}