// Standard includes
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstdio>

// Iris includes
#include "iris.hpp"
#include "elf.hpp"
#include "config.hpp"
#include "platform.hpp"

// ImGui includes
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

// SDL3 includes
#include <SDL3/SDL.h>

// External includes
#include "res/IconsMaterialSymbols.h"

namespace iris {

void add_recent(iris::instance* iris, std::string file) {
    auto it = std::find(iris->recents.begin(), iris->recents.end(), file);

    if (it != iris->recents.end()) {
        iris->recents.erase(it);
        iris->recents.push_front(file);

        return;
    }

    iris->recents.push_front(file);

    if (iris->recents.size() == 11)
        iris->recents.pop_back();
}

int open_file(iris::instance* iris, std::string file) {
    std::filesystem::path path(file);
    std::string ext = path.extension().string();

    for (char& c : ext)
        c = tolower(c);

    // Load disc image
    if (ext == ".iso" || ext == ".bin" || ext == ".cue") {
        if (ps2_cdvd_open(iris->ps2->cdvd, file.c_str()))
            return 1;

        char* boot_file = disc_get_boot_path(iris->ps2->cdvd->disc);

        if (!boot_file)
            return 2;

        load_elf_symbols_from_disc(iris);

        // Temporarily disable window updates
        struct gs_callback cb = *ps2_gs_get_callback(iris->ps2->gs, GS_EVENT_VBLANK);

        ps2_gs_remove_callback(iris->ps2->gs, GS_EVENT_VBLANK);
        ps2_boot_file(iris->ps2, boot_file);

        // Re-enable window updates
        ps2_gs_init_callback(iris->ps2->gs, GS_EVENT_VBLANK, cb.func, cb.udata);

        iris->loaded = file;

        return 0;
    }

    load_elf_symbols_from_file(iris, file);

    // Note: We need the trailing whitespaces here because of IOMAN HLE
    // Load executable
    file = "host:  " + file;

    // Temporarily disable window updates
    struct gs_callback cb = *ps2_gs_get_callback(iris->ps2->gs, GS_EVENT_VBLANK);

    ps2_gs_remove_callback(iris->ps2->gs, GS_EVENT_VBLANK);

    ps2_boot_file(iris->ps2, file.c_str());

    // ps2_elf_load(iris->ps2, file);

    // Re-enable window updates
    ps2_gs_init_callback(iris->ps2->gs, GS_EVENT_VBLANK, cb.func, cb.udata);

    iris->loaded = file;

    return 0;
}

void update_title(iris::instance* iris) {
    char buf[128];

    std::string base = "";

    if (iris->loaded.size()) {
        base = std::filesystem::path(iris->loaded).filename().string();
    }

    sprintf(buf, base.size() ? IRIS_TITLE " | %s" : IRIS_TITLE,
        base.c_str()
    );

    SDL_SetWindowTitle(iris->window, buf);
}

void update_time(iris::instance* iris) {
    int t = SDL_GetTicks() - iris->ticks;

    if (t < 500)
        return;

    if (iris->fps == 0.0f) {
        iris->fps = (float)iris->frames;
    } else {
        iris->fps += (float)iris->frames;
        iris->fps /= 2.0f;
    }

    iris->ticks = SDL_GetTicks();
    iris->frames = 0;
}

void sleep_limiter(iris::instance* iris) {
    uint32_t ticks = (1.0f / iris->fps_cap) * 1000.0f;
    uint32_t now = SDL_GetTicks();

    while ((SDL_GetTicks() - now) < ticks);
}

void update_window(iris::instance* iris) {
    using namespace ImGui;

    // Limit FPS to 60 only when paused
    if (iris->limit_fps && iris->pause)
        sleep_limiter(iris);

    update_title(iris);
    update_time(iris);

    ImGuiIO& io = ImGui::GetIO();

    // Start the Dear ImGui frame
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (!iris->fullscreen) {
        show_main_menubar(iris);
    }

    int w, h;

    SDL_GetWindowSize(iris->window, &w, &h);

    h -= iris->menubar_height;

    // To-do: Optionally hide main menubar
    if (iris->show_status_bar)
        h -= iris->menubar_height;

    DockSpaceOverViewport(0, GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    if (iris->show_ee_control) show_ee_control(iris);
    if (iris->show_ee_state) show_ee_state(iris);
    if (iris->show_ee_logs) show_ee_logs(iris);
    if (iris->show_ee_interrupts) show_ee_interrupts(iris);
    if (iris->show_ee_dmac) show_ee_dmac(iris);
    if (iris->show_iop_control) show_iop_control(iris);
    if (iris->show_iop_state) show_iop_state(iris);
    if (iris->show_iop_logs) show_iop_logs(iris);
    if (iris->show_iop_interrupts) show_iop_interrupts(iris);
    if (iris->show_iop_modules) show_iop_modules(iris);
    if (iris->show_iop_dma) show_iop_dma(iris);
    if (iris->show_gs_debugger) show_gs_debugger(iris);
    if (iris->show_spu2_debugger) show_spu2_debugger(iris);
    if (iris->show_memory_viewer) show_memory_viewer(iris);
    if (iris->show_status_bar && !iris->fullscreen) show_status_bar(iris);
    if (iris->show_breakpoints) show_breakpoints(iris);
    if (iris->show_about_window) show_about_window(iris);
    if (iris->show_settings) show_settings(iris);
    if (iris->show_memory_card_tool) show_memory_card_tool(iris);
    // if (iris->show_gamelist) show_gamelist(iris);
    if (iris->show_imgui_demo) ShowDemoWindow(&iris->show_imgui_demo);
    if (iris->show_bios_setting_window) show_bios_setting_window(iris);

    // Display little pause icon in the top right corner
    if (iris->pause) {
        int width, height;

        SDL_GetWindowSize(iris->window, &width, &height);

        auto ts = CalcTextSize(ICON_MS_PAUSE);

        int offset = 0;

        if (!iris->fullscreen) {
            offset += iris->menubar_height;
        }

        GetBackgroundDrawList()->AddText(
            ImVec2(
                width - ts.x * 1.5f,
                offset + ts.x * 0.5f
            ),
            0xffffffff,
            ICON_MS_PAUSE
        );
    }

    handle_animations(iris);

    // Rendering
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(iris->gpu_device); // Acquire a GPU command buffer

    SDL_GPUTexture* swapchain_texture;
    SDL_AcquireGPUSwapchainTexture(command_buffer, iris->window, &swapchain_texture, nullptr, nullptr); // Acquire a swapchain texture

    if (swapchain_texture != nullptr && !is_minimized)
    {
        // This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index buffer!
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

        // Setup and start a render pass
        SDL_GPUColorTargetInfo target_info = {};
        target_info.texture = swapchain_texture;
        target_info.clear_color = SDL_FColor { 0.11, 0.11, 0.11, 1.0 };
        target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
        target_info.mip_level = 0;
        target_info.layer_or_depth_plane = 0;
        target_info.cycle = false;
        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

        // Render ImGui
        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

        SDL_EndGPURenderPass(render_pass);
    }

    // Submit the command buffer
    SDL_SubmitGPUCommandBuffer(command_buffer);

    iris->frames++;
}

}