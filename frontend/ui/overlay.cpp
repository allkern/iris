#include <vector>
#include <cmath>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

#include "implot.h"

#define MAX_SAMPLES 100

namespace iris {

std::vector <float> fps_history = { 0 };
std::vector <float> fps_history_avg = { 0 };

ImVec2 pos = ImVec2(10, 10);
const ImVec2 padding = ImVec2(5, 5);
const ImVec2 size = ImVec2(250, 100);
const float opacity = 0.75f;

float max = 0.0;

void update_overlay(iris::instance* iris) {
    // if (fps_history.size() == MAX_SAMPLES) {
    //     if (fps_history.front() >= max) {
    //         max = 0.0;

    //         for (int i = 1; i < MAX_SAMPLES; i++) {
    //             if (fps_history[i] > max) {
    //                 max = fps_history[i];
    //             }
    //         }
    //     }

    //     fps_history.pop_front();
    // }

    float sample = 1.0 / ImGui::GetIO().DeltaTime;

    if (!iris->pause) {
        if (fps_history.size() == MAX_SAMPLES)
            fps_history.erase(fps_history.begin());

        fps_history.push_back(sample);

        if (fps_history_avg.size() == MAX_SAMPLES)
            fps_history_avg.erase(fps_history_avg.begin());

        fps_history_avg.push_back(std::roundf(ImGui::GetIO().Framerate));
    }
}

void show_overlay(iris::instance* iris) {
    using namespace ImGui;
    using namespace ImPlot;

    SetNextWindowBgAlpha(0.5f);
    SetNextWindowPos(ImVec2(10.0, 10.0 + iris->menubar_height), ImGuiCond_Always);

    if (Begin("Overlay", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoDocking)) {
        update_overlay(iris);

        ImPlotFlags flags =
            ImPlotFlags_NoTitle |
            ImPlotFlags_NoLegend |
            ImPlotFlags_NoMouseText |
            ImPlotFlags_NoBoxSelect |
            ImPlotFlags_NoFrame |
            ImPlotFlags_NoMenus |
            ImPlotFlags_CanvasOnly |
            ImPlotFlags_NoInputs;

        ImPlotAxisFlags axis_flags =
            ImPlotAxisFlags_NoTickLabels |
            ImPlotAxisFlags_NoGridLines;

        if (BeginPlot("##overlay_plot", ImVec2(0, 0), flags)) {
            SetupAxes(nullptr, nullptr, axis_flags, axis_flags);
            SetupAxesLimits(0, (double)MAX_SAMPLES - 1, 0, 60, ImGuiCond_Always);
            PlotLine("FPS", fps_history.data(), (int)fps_history.size());

            EndPlot();
        }

        renderer_stats* stats; // = renderer_get_debug_stats(iris->ctx);

        PushFont(iris->font_black);
        Text("%d fps", (int)std::roundf(1.0 / ImGui::GetIO().DeltaTime));
        PopFont();
        // Text("Primitives: %d", stats->primitives);
        // Text("Texture uploads: %d", stats->texture_uploads);
        // Text("Texture blits: %d", stats->texture_blits);
    } End();
}

}