#include "imgui_internal.h"

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace ImGui {

bool BeginMainStatusBar()
{
    ImGuiContext& g = *GetCurrentContext();
    ImGuiViewportP* viewport = (ImGuiViewportP*)(void*)GetMainViewport();

    // Notify of viewport change so GetFrameHeight() can be accurate in case of DPI change
    SetCurrentViewport(NULL, viewport);

    // For the main menu bar, which cannot be moved, we honor g.Style.DisplaySafeAreaPadding to ensure text can be visible on a TV set.
    // FIXME: This could be generalized as an opt-in way to clamp window->DC.CursorStartPos to avoid SafeArea?
    // FIXME: Consider removing support for safe area down the line... it's messy. Nowadays consoles have support for TV calibration in OS settings.
    g.NextWindowData.MenuBarOffsetMinVal = ImVec2(g.Style.DisplaySafeAreaPadding.x, ImMax(g.Style.DisplaySafeAreaPadding.y - g.Style.FramePadding.y, 0.0f));
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    float height = GetFrameHeight();
    bool is_open = BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, height, window_flags);
    g.NextWindowData.MenuBarOffsetMinVal = ImVec2(0.0f, 0.0f);

    if (is_open)
        BeginMenuBar();
    else
        End();

    return is_open;
}

void EndMainStatusBar()
{
    EndMenuBar();

    // When the user has left the menu layer (typically: closed menus through activation of an item), we restore focus to the previous window
    // FIXME: With this strategy we won't be able to restore a NULL focus.
    ImGuiContext& g = *GImGui;
    if (g.CurrentWindow == g.NavWindow && g.NavLayer == ImGuiNavLayer_Main && !g.NavAnyRequest)
        FocusTopMostWindowUnderOne(g.NavWindow, NULL, NULL, ImGuiFocusRequestFlags_UnlessBelowModal | ImGuiFocusRequestFlags_RestoreFocusedChild);

    End();
}

}

namespace iris {

int get_format_bpp(int fmt) {
    switch (fmt) {
        case GS_PSMCT32: return 32;
        case GS_PSMCT24: return 24;
        case GS_PSMCT16:
        case GS_PSMCT16S: return 16;
    }

    return 0;
}

void show_status_bar(iris::instance* iris) {
    using namespace ImGui;

    if (BeginMainStatusBar()) {
        static const char* const modes[] = {
            "Progressive",
            "Interlaced (Field)",
            "Progressive",
            "Interlaced (Frame)"
        };

        static const char* const renderers[] = {
            "Null",
            "Software",
            "Hardware (Vulkan)"
        };

        int dispfb = 0;

        if (!iris->image.image) {
            Text(ICON_MS_MONITOR " %s | No image | %1.f fps",
                renderers[iris->renderer_backend],
                GetIO().Framerate
            );
        } else {
            Text(ICON_MS_MONITOR " %s | %dx%d | %dx%d | %s | %dbpp | %.1f fps",
                renderers[iris->renderer_backend],
                iris->render_width, iris->render_height,
                iris->image.width, iris->image.height,
                modes[iris->ps2->gs->smode2 & 3],
                32,
                GetIO().Framerate
            );
        }

        // if (vp_w) {
        //     Text(ICON_MS_MONITOR " %s | %dx%d | %dx%d | %s | %dbpp | %.1f fps",
        //         "None", // renderer_get_name(iris->ctx),
        //         disp_w, disp_h,
        //         vp_w, vp_h,
        //         mode == 3 ? "Interlaced" : "Progressive",
        //         get_format_bpp(disp_fmt),
        //         GetIO().Framerate
        //     );

        //     // iris->avg_frames++;
        //     // iris->avg_fps += iris->fps;
        // } else {
        //     Text(ICON_MS_MONITOR " %s | No image",
        //         "None" // renderer_get_name(iris->ctx)
        //     );
        // }

        EndMainStatusBar();
    }
}
    
}