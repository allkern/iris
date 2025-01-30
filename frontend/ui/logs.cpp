#include <vector>
#include <string>
#include <cctype>

#include "instance.hpp"

#include "res/IconsMaterialSymbols.h"

namespace lunar {

void show_logs(lunar::instance* lunar, const std::vector <std::string>& logs) {
    using namespace ImGui;

    PushFont(lunar->font_code);

    if (BeginTable("##logstable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        for (int i = 0; i < logs.size(); i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text("  %-3d ", i+1);

            TableSetColumnIndex(1);

            char buf[16]; sprintf(buf, "##l%d", i);

            if (Selectable(buf, false, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns)) {
                // Do something with text
            } SameLine(0.0, 0.0);

            Text(logs[i].c_str());
        }

        EndTable();
    }

    PopFont();
}

void show_ee_logs(lunar::instance* lunar) {
    using namespace ImGui;

    if (Begin("EE logs", &lunar->show_ee_logs)) {
        if (Button(ICON_MS_DELETE)) {
            lunar->ee_log.clear();
        } SameLine();

        if (Button(ICON_MS_CONTENT_COPY)) {
            std::string buf;

            for (const std::string& s : lunar->ee_log) {
                buf.append(s);
                buf.push_back('\n');
            }

            SDL_SetClipboardText(buf.c_str());
        }

        if (BeginChild("##eelog")) {
            show_logs(lunar, lunar->ee_log);
        } EndChild();
    } End();
}

void show_iop_logs(lunar::instance* lunar) {
    using namespace ImGui;

    if (Begin("IOP logs", &lunar->show_iop_logs)) {
        if (Button(ICON_MS_DELETE)) {
            lunar->iop_log.clear();
        } SameLine();

        if (Button(ICON_MS_CONTENT_COPY)) {
            std::string buf;

            for (const std::string& s : lunar->iop_log) {
                buf.append(s);
                buf.push_back('\n');
            }

            SDL_SetClipboardText(buf.c_str());
        }

        if (BeginChild("##ioplog")) {
            show_logs(lunar, lunar->iop_log);
        } EndChild();
    } End();
}

}