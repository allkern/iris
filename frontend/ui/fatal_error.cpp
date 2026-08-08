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

            ps2::reset(iris->ps2);
        }
    } End();
}

}
