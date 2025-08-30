#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

void show_pad_debugger(iris::instance* iris) {
    using namespace ImGui;

    if (Begin("DualShock 2", &iris->show_pad_debugger)) {
        if (BeginTabBar("##padtabbar")) {
            if (BeginTabItem("Slot 1")) {
                struct ds_state* ds = (struct ds_state*)iris->ps2->sio2->port[0].udata;

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

            if (BeginTabItem("Slot 2")) {
                struct ds_state* ds = (struct ds_state*)iris->ps2->sio2->port[1].udata;

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
    } End();
}

}