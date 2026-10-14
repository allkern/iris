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

namespace iris {

static bool autoscroll = true;

static const char* const log_names[LOG_COUNT] = { "EE", "IOP", "SYSMEM" };

static std::vector <std::string>& log_for(Instance* iris, int source) {
    switch (source) {
        case LOG_IOP: return iris->debug.iop_log;
        case LOG_SYSMEM: return iris->debug.sysmem_log;
    }

    return iris->debug.ee_log;
}

static void show_logs(Instance* iris, const std::vector <std::string>& logs, bool tail) {
    using namespace ImGui;

    PushFont(iris->ui.font_code);

    if (BeginTable("##logstable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        for (unsigned int i = 0; i < logs.size(); i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            TextDisabled("  %-3d ", i+1);

            TableSetColumnIndex(1);

            char buf[16]; sprintf(buf, "##l%d", i);

            if (Selectable(buf, false, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns)) {
                // Do something with text
            } SameLine(0.0, 0.0);

            Text("%s", logs[i].c_str());
        }

        if (tail) {
            SetScrollHereY(1.0f);
        }

        EndTable();
    }

    PopFont();
}

static void copy_log(const std::vector <std::string>& log) {
    std::string buf;

    for (const std::string& s : log) {
        buf.append(s);
        buf.push_back('\n');
    }

    SDL_SetClipboardText(buf.c_str());
}

void show_log_view(Instance* iris, int source) {
    using namespace ImGui;

    std::vector <std::string>& log = log_for(iris, source);

    if (Button(ICON_MS_DELETE))
        log.clear();

    SameLine();

    if (Button(ICON_MS_CONTENT_COPY))
        copy_log(log);

    SameLine();

    AlignTextToFramePadding();
    TextDisabled("%zu lines", log.size());

    if (BeginChild("##view")) {
        show_logs(iris, log, autoscroll);
    } EndChild();
}

void Logs::on_render() {
    using namespace ImGui;

    if (BeginMenuBar()) {
        if (imgui::BeginMenu("View")) {
            imgui::MenuItem(ICON_MS_ARROW_DOWNWARD " Autoscroll", nullptr, &autoscroll);

            ImGui::EndMenu();
        }

        EndMenuBar();
    }

    imgui::segmented("##logsource", &source, log_names, LOG_COUNT, 76.0f);

    SameLine();

    show_log_view(iris, source);
}

}
