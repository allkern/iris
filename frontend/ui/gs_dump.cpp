#include <string>
#include <cstring>
#include <algorithm>

#include "iris.hpp"

#include "gs/gs_dump.hpp"

#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"
#include "ps2.hpp"

namespace iris {

static std::string gsdump_detect_serial(Instance* iris) {
    if (!iris->ps2 || !iris->ps2->cdvd || !iris->ps2->cdvd->disc)
        return "";

    char buf[128];

    if (!iop::disc::get_serial(iris->ps2->cdvd->disc, buf))
        return "";

    std::string serial = buf;

    std::replace(serial.begin(), serial.end(), '_', '-');
    serial.erase(std::remove(serial.begin(), serial.end(), '.'), serial.end());

    return serial;
}

void GsDumpTool::on_open() {
    serial = gsdump_detect_serial(iris);
    frames = 1;
    delay = 0;

    std::string def = (serial.empty() ? std::string("dump") : serial) + ".gs";

    strncpy(filename, def.c_str(), sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
}

void GsDumpTool::on_render() {
    using namespace ImGui;

    bool capturing = iris->debug.gsdump_armed ||
        (iris->debug.gsdump && gs::dump::is_active(iris->debug.gsdump));

    if (iris->renderer_backend != gs::renderer::BACKEND_HARDWARE) {
        TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
            ICON_MS_WARNING " GS dumps require the hardware (Vulkan) renderer.");
        Spacing();
    }

    Text("Serial");
    if (serial.empty()) {
        TextDisabled("No serial detected");
    } else {
        Text("%s", serial.c_str());
    }

    Spacing();

    Text("Output file");
    SetNextItemWidth(360.0f);
    InputText("##gsdump_file", filename, sizeof(filename));
    SameLine();
    if (Button(ICON_MS_FOLDER_OPEN "##gsdump_browse")) {
        audio::mute(iris);

        auto f = pfd::save_file("Save GS dump", filename, {
            "GS dump (*.gs)", "*.gs",
            "All Files (*.*)", "*"
        });

        while (!f.ready());

        audio::unmute(iris);

        if (f.result().size()) {
            strncpy(filename, f.result().c_str(), sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';
        }
    }

    Spacing();

    SetNextItemWidth(140.0f);
    if (InputInt("Frames to capture", &frames)) {
        if (frames < 1) frames = 1;
    }

    SetNextItemWidth(140.0f);
    if (InputInt("Frames to wait first", &delay)) {
        if (delay < 0) delay = 0;
    }

    Spacing();
    Separator();
    Spacing();

    if (capturing) {
        if (iris->debug.gsdump_armed) {
            Text(ICON_MS_HOURGLASS_TOP " Waiting %d frame(s) before capture...",
                iris->debug.gsdump_delay_remaining);
        } else {
            Text(ICON_MS_FIBER_MANUAL_RECORD " Capturing... %d frame(s) left",
                iris->debug.gsdump_frames_remaining);
        }

        if (Button("Close")) {
            iris->applets.gs_dump_tool.open = false;
        }
    } else {
        BeginDisabled(filename[0] == '\0' ||
            iris->renderer_backend != gs::renderer::BACKEND_HARDWARE);

        if (Button("Start capture")) {
            iris->debug.pause = false;

            render::gs_dump_start(iris, filename, frames, delay, serial);

            iris->applets.gs_dump_tool.open = false;
        }

        EndDisabled();

        SameLine();

        if (Button("Cancel")) {
            iris->debug.pause = iris->debug.gsdump_prev_pause;

            iris->applets.gs_dump_tool.open = false;
        }
    }
}

}
