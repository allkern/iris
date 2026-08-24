#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"
#include "config.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

bool About::begin() {
    using namespace ImGui;

    if (GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable && !GetIO().ConfigViewportsNoDecoration)
        flags |= ImGuiWindowFlags_NoTitleBar;

    ImVec2 padding = GetStyle().WindowPadding;

    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding.x, padding.y + 8.0));

    bool visible = Begin(title, &open, flags);

    PopStyleVar();

    return visible;
}

void About::on_render() {
    using namespace ImGui;

    if (BeginChild("##iconchild", ImVec2(100.0, 250.0), ImGuiChildFlags_AutoResizeY)) {
        Image((ImTextureID)(intptr_t)iris->ui.iris_icon.descriptor_set, ImVec2(100.0, 100.0));
    } EndChild(); SameLine(0.0, 10.0);

    if (BeginChild("##textchild", ImVec2(420.0, 0.0), ImGuiChildFlags_AutoResizeY)) {
        PushFont(iris->ui.font_heading);
        Text(IRIS_TITLE);
        PopFont();

        Separator();

        Text("Experimental PlayStation 2 emulator");
        Text("");
        Text("Available at "); SameLine(0.0, 0.0);
        TextLinkOpenURL("https://github.com/allkern/iris", "https://github.com/allkern/iris");
        Text("");
        TextWrapped(
            "Special thanks to: The emudev Discord server, Ziemas, "
            "refraction, ncarrillo, cakehonolulu, Layle, el_isra, "
            "uyjulian, slimpuggamer, DiscoStarSlayer, PSI, and "
            "the PCSX2 team for their kind support."
        );
        Text("");
        Text("Please file any issues to "); SameLine(0.0, 0.0);
        TextLinkOpenURL("our GitHub issues page", "https://github.com/allkern/iris/issues"); SameLine(0.0, 0.0);
        Text(".");

        Separator();
        Text("Built with " IRIS_COMPILER_NAME "-" IRIS_COMPILER_VERSION " on " STR(_IRIS_OSVERSION) " (" STR(_IRIS_PROCESSOR) ")");
    } EndChild();
}

}