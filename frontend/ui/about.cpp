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

    if (BeginChild("##iconchild", ImVec2(100.0, 0.0), ImGuiChildFlags_AutoResizeY)) {
        Image((ImTextureID)(intptr_t)iris->ui.iris_icon.descriptor_set, ImVec2(100.0, 100.0));
    } EndChild(); SameLine(0.0, 8.0);

    if (BeginChild("##textchild", ImVec2(420.0, 0.0), ImGuiChildFlags_AutoResizeY)) {
        PushFont(iris->ui.font_heading);
        Text(IRIS_TITLE);
        PopFont();

        Separator();

        Text("Sony PlayStation 2 emulator for Windows, Linux and macOS.");
        Spacing();
        Text("Available at "); SameLine(0.0, 0.0);
        TextLinkOpenURL("https://github.com/allkern/iris", "https://github.com/allkern/iris");
        Spacing();
        TextWrapped(
            "Special thanks to: The emudev Discord server, Ziemas, "
            "refraction, ncarrillo, cakehonolulu, Layle, el_isra, "
            "uyjulian, slimpuggamer, DiscoStarSlayer, PSI, the PCSX2 "
            "and ARMSX2 teams for their kind support."
        );
        Spacing();
        Text("Please file any issues to "); SameLine(0.0, 0.0);
        TextLinkOpenURL("our GitHub issues page", "https://github.com/allkern/iris/issues"); SameLine(0.0, 0.0);
        Text(".");

        Separator();
        Text("Built with " IRIS_COMPILER_NAME "-" IRIS_COMPILER_VERSION " on " STR(_IRIS_OSVERSION) " (" STR(_IRIS_PROCESSOR) ")");
    } EndChild();
}

}