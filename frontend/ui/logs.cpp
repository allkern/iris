#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

bool ee_follow = true;
bool iop_follow = true;
bool sysmem_follow = true;

void show_logs(Instance* iris, const std::vector <std::string>& logs, bool follow) {
    using namespace ImGui;

    PushFont(iris->ui.font_code);

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

        if (follow) {
            SetScrollHereY(1.0f);
        }

        EndTable();
    }

    PopFont();
}

void show_ee_logs(Instance* iris) {
    using namespace ImGui;

    if (imgui::BeginEx("EE logs", &iris->ui.show_ee_logs)) {
        if (Button(ICON_MS_DELETE)) {
            iris->debug.ee_log.clear();
        } SameLine();

        if (Button(ICON_MS_CONTENT_COPY)) {
            std::string buf;

            for (const std::string& s : iris->debug.ee_log) {
                buf.append(s);
                buf.push_back('\n');
            }

            SDL_SetClipboardText(buf.c_str());
        }

        if (BeginChild("##eelog")) {
            show_logs(iris, iris->debug.ee_log, ee_follow);
        } EndChild();
    } End();
}

void show_iop_logs(Instance* iris) {
    using namespace ImGui;

    if (imgui::BeginEx("IOP logs", &iris->ui.show_iop_logs, ImGuiWindowFlags_MenuBar)) {
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
            iris->debug.iop_log.clear();
        } SameLine();

        if (Button(ICON_MS_CONTENT_COPY)) {
            std::string buf;

            for (const std::string& s : iris->debug.iop_log) {
                buf.append(s);
                buf.push_back('\n');
            }

            SDL_SetClipboardText(buf.c_str());
        }

        if (BeginChild("##ioplog")) {
            show_logs(iris, iris->debug.iop_log, iop_follow);
        } EndChild();
    } End();
}

void show_sysmem_logs(Instance* iris) {
    using namespace ImGui;

    if (imgui::BeginEx("SYSMEM logs", &iris->ui.show_sysmem_logs, ImGuiWindowFlags_MenuBar)) {
        if (BeginMenuBar()) {
            if (BeginMenu("Settings")) {
                if (MenuItem(sysmem_follow ? ICON_MS_CHECK_BOX " Follow" : ICON_MS_CHECK_BOX_OUTLINE_BLANK " Follow", nullptr)) {
                    sysmem_follow = !sysmem_follow;
                }

                ImGui::EndMenu();
            }

            EndMenuBar();
        }

        if (Button(ICON_MS_DELETE)) {
            iris->debug.sysmem_log.clear();
        } SameLine();

        if (Button(ICON_MS_CONTENT_COPY)) {
            std::string buf;

            for (const std::string& s : iris->debug.sysmem_log) {
                buf.append(s);
                buf.push_back('\n');
            }

            SDL_SetClipboardText(buf.c_str());
        }

        if (BeginChild("##sysmemlog")) {
            show_logs(iris, iris->debug.sysmem_log, sysmem_follow);
        } EndChild();
    } End();
}

}