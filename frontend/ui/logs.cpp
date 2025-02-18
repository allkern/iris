#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

bool ee_follow = true;
bool iop_follow = true;

void show_logs(iris::instance* iris, const std::vector <std::string>& logs) {
    using namespace ImGui;

    PushFont(iris->font_code);

    if (BeginTable("##logstable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        for (unsigned int i = 0; i < logs.size(); i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text("  %-3d ", i+1);

            TableSetColumnIndex(1);

            char buf[16]; sprintf(buf, "##l%d", i);

            if (Selectable(buf, false, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns)) {
                // Do something with text
            } SameLine(0.0, 0.0);

            Text("%s", logs[i].c_str());
        }

        if (iop_follow) {
            SetScrollHereY(1.0f);
        }

        EndTable();
    }

    PopFont();
}

void show_ee_logs(iris::instance* iris) {
    using namespace ImGui;

    if (Begin("EE logs", &iris->show_ee_logs)) {
        if (Button(ICON_MS_DELETE)) {
            iris->ee_log.clear();
        } SameLine();

        if (Button(ICON_MS_CONTENT_COPY)) {
            std::string buf;

            for (const std::string& s : iris->ee_log) {
                buf.append(s);
                buf.push_back('\n');
            }

            SDL_SetClipboardText(buf.c_str());
        }

        if (BeginChild("##eelog")) {
            show_logs(iris, iris->ee_log);
        } EndChild();
    } End();
}

void show_iop_logs(iris::instance* iris) {
    using namespace ImGui;

    if (Begin("IOP logs", &iris->show_iop_logs, ImGuiWindowFlags_MenuBar)) {
        if (BeginMenuBar()) {
            if (BeginMenu("Settings")) {
                if (MenuItem(iop_follow ? ICON_MS_CHECK_BOX " Follow" : ICON_MS_CHECK_BOX_OUTLINE_BLANK " Follow", nullptr)) {
                    iop_follow = !iop_follow;
                }

                ImGui::EndMenu();
            }

            EndMenuBar();
        }

        if (Button(ICON_MS_DELETE)) {
            iris->iop_log.clear();
        } SameLine();

        if (Button(ICON_MS_CONTENT_COPY)) {
            std::string buf;

            for (const std::string& s : iris->iop_log) {
                buf.append(s);
                buf.push_back('\n');
            }

            SDL_SetClipboardText(buf.c_str());
        }

        if (BeginChild("##ioplog")) {
            show_logs(iris, iris->iop_log);
        } EndChild();
    } End();
}

}