#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

void show_about_window(iris::instance* iris) {
    using namespace ImGui;

    static const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (Begin("About", &iris->show_about_window, flags)) {
        if (BeginChild("##iconchild", ImVec2(100.0, 210.0), ImGuiChildFlags_AutoResizeY)) {
            Image(iris->iris_icon_tex, ImVec2(100.0, 100.0));
        } EndChild(); SameLine(0.0, 10.0);

        if (BeginChild("##textchild", ImVec2(350.0, 0.0))) {
            PushFont(iris->font_heading);
            Text("Iris");
            PopFont();

            Separator();

            Text("Experimental PlayStation 2 emulator");
            Text("");
            Text("Available at "); SameLine(0.0, 0.0);
            TextLinkOpenURL("https://github.com/allkern/eegs", "https://github.com/allkern/eegs");
            Text("");
            TextWrapped(
                "Special thanks to: The emudev Discord server, Ziemas, "
                "ncarrillo, cakehonolulu, and the PCSX2 team "
                "for their kind support."
            );
            Text("");
            Text("Please file any issues to "); SameLine(0.0, 0.0);
            TextLinkOpenURL("our GitHub issues page", "https://github.com/allkern/eegs/issues"); SameLine(0.0, 0.0);
            Text(".");
        } EndChild();
    } End();
}

}