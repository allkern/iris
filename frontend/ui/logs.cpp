#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

static void show_logs(Instance* iris, const std::vector <std::string>& logs, bool follow) {
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

        if (follow) {
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

void EeLogs::on_render() {
    using namespace ImGui;

    if (Button(ICON_MS_DELETE)) {
        iris->debug.ee_log.clear();
    } SameLine();

    if (Button(ICON_MS_CONTENT_COPY)) {
        copy_log(iris->debug.ee_log);
    }

    if (BeginChild("##eelog")) {
        show_logs(iris, iris->debug.ee_log, follow);
    } EndChild();
}

void IopLogs::on_render() {
    using namespace ImGui;

    if (BeginMenuBar()) {
        if (imgui::BeginMenu("Settings")) {
            if (imgui::MenuItem(follow ? ICON_MS_CHECK_BOX " Follow" : ICON_MS_CHECK_BOX_OUTLINE_BLANK " Follow", nullptr)) {
                follow = !follow;
            }

            ImGui::EndMenu();
        }

        EndMenuBar();
    }

    if (Button(ICON_MS_DELETE)) {
        iris->debug.iop_log.clear();
    } SameLine();

    if (Button(ICON_MS_CONTENT_COPY)) {
        copy_log(iris->debug.iop_log);
    }

    if (BeginChild("##ioplog")) {
        show_logs(iris, iris->debug.iop_log, follow);
    } EndChild();
}

void SysmemLogs::on_render() {
    using namespace ImGui;

    if (BeginMenuBar()) {
        if (imgui::BeginMenu("Settings")) {
            if (imgui::MenuItem(follow ? ICON_MS_CHECK_BOX " Follow" : ICON_MS_CHECK_BOX_OUTLINE_BLANK " Follow", nullptr)) {
                follow = !follow;
            }

            ImGui::EndMenu();
        }

        EndMenuBar();
    }

    if (Button(ICON_MS_DELETE)) {
        iris->debug.sysmem_log.clear();
    } SameLine();

    if (Button(ICON_MS_CONTENT_COPY)) {
        copy_log(iris->debug.sysmem_log);
    }

    if (BeginChild("##sysmemlog")) {
        show_logs(iris, iris->debug.sysmem_log, follow);
    } EndChild();
}

}
