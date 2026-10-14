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

#include <string>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

void show_fatal_error(Instance* iris) {
    using namespace ImGui;

    static ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoDecoration;

    ImVec2 center = GetMainViewport()->GetCenter();

    SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5, 0.5));

    if (Begin("Emulation halted", nullptr, flags)) {
        TextUnformatted("The virtual machine encountered an error it can't continue past.");

        Spacing();
        Separator();
        Spacing();

        PushTextWrapPos(GetCursorPos().x + 480.0);
        TextUnformatted(iris->fatal_error_text.c_str());
        PopTextWrapPos();

        Spacing();
        Separator();
        Spacing();

        if (Button("Dismiss", ImVec2(120.0, 0.0)))
            iris->fatal_error = false;

        SameLine();

        if (Button("Reset", ImVec2(120.0, 0.0))) {
            iris->fatal_error = false;

            patches::reapply(iris);

            ps2::reset(iris->ps2);
        }
    } End();
}

}
