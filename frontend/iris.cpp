// Standard includes
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <cmath>

// Iris includes
#include "iris.hpp"
#include "config.hpp"
#include "ee/ee_def.hpp"
#include "ee/vu_def.hpp"
#include "iop/iop_def.hpp"
#include "net.hpp"
#include "slirp.hpp"

// SDL3 includes
#include <SDL3/SDL.h>

// External includes
#include "res/IconsMaterialSymbols.h"
#include "ps2.hpp"

namespace iris {

void add_recent(Instance* iris, std::string file, RecentType type) {
    auto it = std::find_if(iris->recents.begin(), iris->recents.end(), [file, type](const Recent& a) {
        return a.type == type && a.path == file;
    });

    if (it != iris->recents.end()) {
        iris->recents.erase(it);
        iris->recents.push_front({file, type});

        return;
    }

    iris->recents.push_front({file, type});

    if (iris->recents.size() == 11)
        iris->recents.pop_back();
}

void update_title(Instance* iris) {
    char buf[512];

    std::string base = "";

    if (iris->loaded.size()) {
        base = std::filesystem::path(iris->loaded).filename().string();
    }

    sprintf(buf, base.size() ? IRIS_TITLE " | %s" : IRIS_TITLE,
        base.c_str()
    );

    SDL_SetWindowTitle(iris->window, buf);
}

void update_time(Instance* iris) {
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

void sleep_limiter(Instance* iris) {
    uint32_t ticks = (1.0f / iris->fps_cap) * 1000.0f;

    std::this_thread::sleep_for(std::chrono::milliseconds(ticks / 2));

    // uint32_t now = SDL_GetTicks();

    // while ((SDL_GetTicks() - now) < ticks) {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(ticks / 4));
    // }
}

static void sync_breakpoints(Instance* iris) {
    static_assert(MAX_EXEC_BREAKPOINTS <= ee::EE_MAX_BREAKPOINTS);
    static_assert(MAX_EXEC_BREAKPOINTS <= iop::IOP_MAX_BREAKPOINTS);

    uint32_t ee_addrs[MAX_EXEC_BREAKPOINTS];
    uint32_t iop_addrs[MAX_EXEC_BREAKPOINTS];

    int ee_count = 0;
    int iop_count = 0;

    for (const Breakpoint& b : iris->debug.breakpoints) {
        if (!b.enabled || !b.cond_x)
            continue;

        if (b.cpu == BreakpointCpu::EE) {
            if (ee_count < MAX_EXEC_BREAKPOINTS)
                ee_addrs[ee_count++] = b.addr;
        } else {
            if (iop_count < MAX_EXEC_BREAKPOINTS)
                iop_addrs[iop_count++] = b.addr;
        }
    }

    if (ee_count == iris->debug.pushed_ee_count &&
        iop_count == iris->debug.pushed_iop_count &&
        !memcmp(ee_addrs, iris->debug.pushed_ee, ee_count * sizeof(uint32_t)) &&
        !memcmp(iop_addrs, iris->debug.pushed_iop, iop_count * sizeof(uint32_t)))
        return;

    memcpy(iris->debug.pushed_ee, ee_addrs, ee_count * sizeof(uint32_t));
    memcpy(iris->debug.pushed_iop, iop_addrs, iop_count * sizeof(uint32_t));

    iris->debug.pushed_ee_count = ee_count;
    iris->debug.pushed_iop_count = iop_count;

    ee::set_breakpoints(iris->ps2->ee, ee_addrs, ee_count);
    iop::set_breakpoints(iris->ps2->iop, iop_addrs, iop_count);
}

static void resume_breakpoints(Instance* iris) {
    if (iris->ps2->ee->bp_hit) {
        iris->ps2->ee->bp_hit = false;
        iris->ps2->ee->bp_skip_pc = iris->ps2->ee->pc;
    }

    if (iris->ps2->iop->bp_hit) {
        iris->ps2->iop->bp_hit = false;
        iris->ps2->iop->bp_skip_pc = iris->ps2->iop->pc;
    }
}

static inline void do_cycle(Instance* iris) {
    ps2::cycle(iris->ps2);
    if (iris->debug.step_out) {
        // jr $ra
        if (iris->ps2->ee->opcode == 0x03e00008) {
            iris->debug.step_out = false;
            iris->debug.pause = true;

            // Consume the delay slot
            ps2::cycle(iris->ps2);
        }
    }

    if (iris->debug.step_over) {
        if (iris->ps2->ee->pc == iris->debug.step_over_addr) {
            iris->debug.step_over = false;
            iris->debug.pause = true;
        }
    }

    if (iris->ps2->ee->bp_hit || iris->ps2->iop->bp_hit) {
        iris->debug.pause = true;
    }
}

void update_window(Instance* iris) {
    using namespace ImGui;

    // Limit FPS to 60 only when paused
    if (iris->debug.pause)
        sleep_limiter(iris);

    update_title(iris);
    update_time(iris);

    // Start the Dear ImGui frame
    if (SDL_GetWindowFlags(iris->window) & SDL_WINDOW_MINIMIZED) {
        // SDL_Delay(1);

        return;
    }

    // Resize swapchain?
    int width, height;

    SDL_GetWindowSize(iris->window, &width, &height);

    // Rebuilding starts with vkDeviceWaitIdle, which cannot succeed on a lost device
    if (!iris->vk.device_lost && width > 0 && height > 0 && (iris->vk.swapchain_rebuild || iris->vk.main_window_data.Width != width || iris->vk.main_window_data.Height != height)) {
        VkSurfaceCapabilitiesKHR cap;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(iris->vk.physical_device, iris->vk.main_window_data.Surface, &cap);

        if (cap.currentExtent.width != 0 && cap.currentExtent.height != 0) {
            ImGui_ImplVulkan_SetMinImageCount(iris->vk.min_image_count);
        
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                iris->vk.instance,
                iris->vk.physical_device,
                iris->vk.device,
                &iris->vk.main_window_data,
                iris->vk.queue_family,
                nullptr,
                width, height,
                iris->vk.min_image_count,
                0
            );

            iris->vk.main_window_data.FrameIndex = 0;
            iris->vk.swapchain_rebuild = false;
        }
    }

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    bool usb_mouse_owns_pointer =
        (iris->input.usb_devices[0] == usb::USB_DEVICE_MOUSE || iris->input.usb_devices[1] == usb::USB_DEVICE_MOUSE) &&
        SDL_GetMouseFocus() == iris->window && !iris->debug.pause &&
        !GetIO().WantCaptureMouse;

    if (usb_mouse_owns_pointer)
        SetMouseCursor(ImGuiMouseCursor_None);

    show_main_menubar(iris);

    DockSpaceOverViewport(0, GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Drop file fade animation
    if (iris->ui.dim_active) {
        imgui::render_dim(iris);
    }

    if (iris->ui.drop_file_active) {
        ImDrawList* draw_list = GetForegroundDrawList(GetMainViewport());
        ImVec2 origin = GetMainViewport()->Pos;

        constexpr float panel_w = 400.0f;
        constexpr float panel_h = 112.0f;
        constexpr float rounding = 12.0f;

        ImVec2 local_min = ImVec2(width * 0.5f - panel_w * 0.5f, height * 0.5f - panel_h * 0.5f);
        ImVec2 local_max = ImVec2(local_min.x + panel_w, local_min.y + panel_h);

        float split = local_min.x + panel_w * 0.5f;

        iris->ui.drop_insert_min = ImVec2(split, local_min.y);
        iris->ui.drop_insert_max = local_max;

        bool over_insert =
            iris->ui.drop_pos.x >= split && iris->ui.drop_pos.x <= local_max.x &&
            iris->ui.drop_pos.y >= local_min.y && iris->ui.drop_pos.y <= local_max.y;

        float alpha = iris->ui.dim_current_alpha;

        ImVec4 panel_col = GetStyleColorVec4(ImGuiCol_MenuBarBg);
        ImVec4 text_col = GetStyleColorVec4(ImGuiCol_Text);
        ImVec4 dim_col = GetStyleColorVec4(ImGuiCol_TextDisabled);
        ImVec4 hi_col = GetStyleColorVec4(ImGuiCol_HeaderHovered);

        panel_col.w = alpha;
        text_col.w = alpha;
        dim_col.w = alpha;
        hi_col.w = alpha;

        ImVec2 p0 = ImVec2(origin.x + local_min.x, origin.y + local_min.y);
        ImVec2 p1 = ImVec2(origin.x + local_max.x, origin.y + local_max.y);
        float sx = origin.x + split;

        draw_list->AddRectFilled(p0, p1, GetColorU32(panel_col), rounding);

        draw_list->AddRectFilled(
            over_insert ? ImVec2(sx, p0.y) : p0,
            over_insert ? p1 : ImVec2(sx, p1.y),
            GetColorU32(hi_col), rounding,
            over_insert ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersLeft
        );

        draw_list->AddLine(ImVec2(sx, p0.y + rounding), ImVec2(sx, p1.y - rounding), GetColorU32(dim_col));

        auto draw_zone = [&](float cx, const char* icon, const char* label, bool active) {
            ImU32 col = GetColorU32(active ? text_col : dim_col);
            float cy = (p0.y + p1.y) * 0.5f;

            PushFont(iris->ui.font_icons_big);

            ImVec2 icon_size = CalcTextSize(icon);

            draw_list->AddText(ImVec2(cx - icon_size.x * 0.5f, cy - icon_size.y * 0.6f), col, icon);

            PopFont();

            ImVec2 text_size = CalcTextSize(label);

            draw_list->AddText(ImVec2(cx - text_size.x * 0.5f, cy + icon_size.y * 0.4f), col, label);
        };

        draw_zone((p0.x + sx) * 0.5f, ICON_MS_ROCKET_LAUNCH, "Boot", !over_insert);
        draw_zone((sx + p1.x) * 0.5f, ICON_MS_ALBUM, "Insert disc", over_insert);

        const char* hint = "Drop anywhere to boot";

        ImVec2 hint_size = CalcTextSize(hint);

        draw_list->AddText(
            ImVec2((p0.x + p1.x) * 0.5f - hint_size.x * 0.5f, p1.y + 12.0f),
            GetColorU32(dim_col),
            hint
        );
    }

    if (iris->ui.loading_file_active) {
        OpenPopup("Loading", ImGuiPopupFlags_AnyPopupId);

        ImVec2 center = GetMainViewport()->GetCenter();
        ImVec2 size = GetMainViewport()->Size;

        ImVec4 col1 = GetStyleColorVec4(ImGuiCol_Text);
        ImVec4 col0 = ImVec4(1.0 - col1.x, 1.0 - col1.y, 1.0 - col1.z, 0.0f);

        PushFont(iris->ui.font_heading);

        char buf[512];

        size_t text_len = snprintf(buf, 512, "Loading %s...", iris->ui.loading_target.c_str());
        ImVec2 text_size = CalcTextSize(buf);

        ImVec2 pos = ImVec2((size.x - text_size.x) * 0.5f, (size.y - text_size.y) * 0.5f);

        pos.x += GetMainViewport()->Pos.x;
        pos.y += GetMainViewport()->Pos.y;

        long t = SDL_GetTicks();

        ImVec4 rect_col = GetStyleColorVec4(ImGuiCol_MenuBarBg);

        rect_col.w = iris->ui.dim_current_alpha;

        GetForegroundDrawList(GetMainViewport())->AddRectFilled(
            ImVec2(pos.x - 10, pos.y - 10),
            ImVec2(pos.x + text_size.x + 10, pos.y + text_size.y + 10),
            GetColorU32(rect_col), 10.0f
        );

        for (int i = 0; i < text_len; i++) {
            ImVec4 col;

            float p = (((t*2) - (i*6)) % 2000) / 1000.0f;

            if (p > 0.5) {
                p = 1.0f - p;
            } else if (p > 1.0) {
                p = 0.0f;
            }

            p *= 2.0f;

            col.x = col1.x;
            col.y = col1.y;
            col.z = col1.z;
            col.w = std::lerp(iris->ui.dim_current_alpha, 0.0, p);

            iris->ui.font_heading->RenderChar(GetForegroundDrawList(GetMainViewport()), 20.0F, pos, GetColorU32(col), buf[i]);

            pos.x += iris->ui.font_heading->GetFontBaked(20.0F)->FindGlyph(buf[i])->AdvanceX;
        }

        PopFont();
    }

    applets::render(iris);

    if (iris->ui.show_status_bar && !iris->fullscreen) show_status_bar(iris);
    if (iris->ui.show_imgui_demo) ShowDemoWindow(&iris->ui.show_imgui_demo);
    if (iris->ui.show_overlay) show_overlay(iris);
    if (iris->fatal_error) show_fatal_error(iris);

    iris->ui.show_gamelist = false;

    // if (iris->ui.show_gamelist && !iris->headless) {
    //     ImVec2 pos = GetMainViewport()->Pos;
    //     ImVec2 size = GetMainViewport()->Size;

    //     pos.y += iris->ui.menubar_height;

    //     SetNextWindowPos(pos, ImGuiCond_Always);
    //     SetNextWindowSize(ImVec2((float)width, (float)height - iris->ui.menubar_height), ImGuiCond_Always);
    //     SetNextWindowViewport(GetMainViewport()->ID);

    //     ImGuiWindowFlags flags =
    //         ImGuiWindowFlags_NoDecoration |
    //         ImGuiWindowFlags_NoMove |
    //         ImGuiWindowFlags_NoResize |
    //         ImGuiWindowFlags_NoSavedSettings |
    //         ImGuiWindowFlags_NoDocking;

    //     if (Begin("##GameLibrary", nullptr, flags)) {
    //         show_gamelist(iris);
    //     } End();
    // }

    // Display little pause icon in the top right corner
    if (iris->debug.pause) {
        ImVec2 ts = CalcTextSize(ICON_MS_PAUSE);
        ImVec2 offset = ImVec2(10.0f, 10.0f);
        // ImVec2 padding = ImVec2(0.0f, 0.0f);

        ts.x -= 1.0f;

        int menubar_offset = get_menubar_inset(iris);

        // GetBackgroundDrawList()->AddRectFilled(
        //     ImVec2(width - ts.x - offset.x - padding.x, menubar_offset + offset.y - padding.y),
        //     ImVec2(width - offset.x + padding.x, menubar_offset + ts.y + offset.y + padding.y),
        //     GetColorU32(GetStyleColorVec4(ImGuiCol_WindowBg)), 8.0f
        // );

        ImVec2 pos = ImVec2(width - ts.x - offset.x, menubar_offset + offset.y);

        if (iris->ui.imgui_enable_viewports) {
            ImVec2 window_pos = GetMainViewport()->Pos;

            pos.x = window_pos.x + pos.x;
            pos.y = window_pos.y + pos.y;
        }

        GetBackgroundDrawList(GetMainViewport())->AddText(
            pos,
            GetColorU32(GetStyleColorVec4(ImGuiCol_Text)),
            ICON_MS_PAUSE
        );
    }

    handle_animations(iris);

    // Rendering
    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();

    const bool main_is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

    iris->vk.main_window_data.ClearValue.color.float32[0] = 0.0f;
    iris->vk.main_window_data.ClearValue.color.float32[1] = 0.0f;
    iris->vk.main_window_data.ClearValue.color.float32[2] = 0.0f;
    iris->vk.main_window_data.ClearValue.color.float32[3] = 1.0f;

    if (iris->headless || !main_is_minimized) {
        if (!imgui::render_frame(iris, draw_data)) {
            iris_error(&iris->log.iris, "Failed to render ImGui frame");
        }
    }

    iris->frames++;
}

Instance* create() {
    Instance* iris = new Instance();

    // Before init(), so the earliest failures in there have somewhere to go
    init_logger(iris);

    return iris;
}

bool init(Instance* iris) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
        iris_error(&iris->log.iris, "Failed to init SDL \'{}\'", SDL_GetError());

        return false;
    }

    if (!net::init(&iris->log.net)) {
        iris_error(&iris->log.iris, "Failed to initialize net");

        return false;
    }

    // Create and check window
    iris->vk.main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    // Init preferences path
    if (iris->cli.portable || std::filesystem::exists("portable") || std::filesystem::exists("portable.txt")) {
        iris->paths.pref_path = "./";
    } else {
        char* pref = SDL_GetPrefPath("Allkern", "Iris");

        iris->paths.pref_path = std::string(pref);

        SDL_free(pref);
    }

    if (!emu::init(iris)) {
        iris_error(&iris->log.iris, "Failed to initialize emulator state");

        return false;
    }

    applets::create(iris);

    if (!settings::init(iris)) {
        iris_error(&iris->log.iris, "Failed to initialize settings");

        return false;
    }

    log_apply_settings(iris);

    iris->window = SDL_CreateWindow(
        IRIS_TITLE,
        iris->window_width, iris->window_height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN
    );

    if (!iris->window) {
        iris_error(&iris->log.iris, "Failed to create SDL window \'{}\'", SDL_GetError());

        return false;
    }

    if (!vulkan::init(iris, iris->vk.vulkan_enable_validation_layers)) {
        iris_error(&iris->log.iris, "Failed to initialize Vulkan");

        return false;
    }

    if (!imgui::init(iris)) {
        iris_error(&iris->log.iris, "Failed to initialize ImGui");

        return false;
    }

    if (!platform::init(iris)) {
        iris_error(&iris->log.iris, "Failed to initialize platform");

        return false;
    }

    if (!audio::init(iris)) {
        iris_error(&iris->log.iris, "Failed to initialize audio");

        return false;
    }

    if (!render::init(iris)) {
        iris_error(&iris->log.iris, "Failed to initialize render state");

        return false;
    }

    if (!input::init(iris)) {
        iris_error(&iris->log.iris, "Failed to initialize input");

        return false;
    }

    if (!gamelist::init(iris)) {
        iris_error(&iris->log.iris, "Failed to initialize gamelist");

        return false;
    }

    for (const std::string& s : iris->vk.shader_passes_pending)
        shaders::push(iris, s);

    iris->vk.shader_passes_pending.clear();

    applets::init(iris);

    // Sadly we need to start a frame here to measure menubar height
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    iris->ui.menubar_height = ImGui::GetFrameHeight();

    ImGui::EndFrame();

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    SDL_SetWindowSize(iris->window, iris->window_width, iris->window_height + get_menubar_height(iris));

    if (!iris->headless) {
        SDL_ShowWindow(iris->window);
    }

    return true;
}

SDL_AppResult update(Instance* iris) {
    if (iris->input.double_click_counter) {
        iris->input.double_click_counter--;
    }

    if (iris->ui.loading_file_active &&
        iris->load_ready.exchange(false, std::memory_order_acquire)) {
        emu::finalize_load(iris);
    }

    // int iop_count = iris->ps2->iop->total_cycles;
    // int ee_count = iris->ps2->ee->total_cycles;

    if (iris->debug.pause) {
        sync_breakpoints(iris);

        emu::start_pending_load(iris);

        iris->debug.step_out = false;
        iris->debug.step_over = false;

        if (iris->debug.step && !iris->ui.loading_file_active) {
            ps2::step_ee(iris->ps2);

            iris->debug.step = false;
        }

        update_window(iris);

        return SDL_APP_CONTINUE;
    }

    sync_breakpoints(iris);
    resume_breakpoints(iris);

    if (iris->ps2 && iris->ps2->speed)
        slirp::pump(iris->ps2->speed->smap);

    // Execute until VBlank
    while (!gs::is_vblank(iris->ps2->gs)) {
        do_cycle(iris);

        if (iris->debug.pause) {
            update_window(iris);

            return SDL_APP_CONTINUE;
        }
    }

    // Record a GS dump frame boundary
    render::gs_dump_tick(iris);

    // Draw frame
    update_window(iris);

    // Execute until vblank is over
    while (gs::is_vblank(iris->ps2->gs)) {
        do_cycle(iris);

        if (iris->debug.pause) {
            update_window(iris);

            return SDL_APP_CONTINUE;
        }
    }

    // printf("ee_stats: cache: hits=%d misses=%d idle skips=%d\n", iris->ps2->ee->cache_hits, iris->ps2->ee->cache_misses, iris->ps2->ee->idle_skips);

    iris->ps2->ee->cache_hits = 0;
    iris->ps2->ee->cache_misses = 0;
    iris->ps2->ee->idle_skips = 0;

    // printf("ee: %ld cycles, iop: %ld cycles\n", iris->ps2->ee->total_cycles - ee_count, iris->ps2->iop->total_cycles - iop_count);

    // float p = ((float)iris->ps2->ee->eenull_counter / (float)(4920115)) * 100.0f;

    // printf("ee: Time spent idling: %ld cycles (%.2f%%) INTC reads: %d CSR reads: %d (%.1f fps)\n", iris->ps2->ee->eenull_counter, p, iris->ps2->ee->intc_reads, iris->ps2->ee->csr_reads, 1.0f / ImGui::GetIO().DeltaTime);

    iris->ps2->ee->eenull_counter = 0;
    iris->ps2->ee->intc_reads = 0;
    iris->ps2->ee->csr_reads = 0;

    switch (iris->present_mode) {
        case render::FPS_30:
        case render::FPS_60: {
            using namespace std::chrono;

            float framerate = iris->present_mode == render::FPS_30 ? 30.0f : 60.0f;

            auto target = nanoseconds((int64_t)(1000000000.0 / framerate));
            auto now = high_resolution_clock::now();

            if (iris->frame_deadline.time_since_epoch().count() == 0 || iris->frame_deadline < now - 2 * target)
                iris->frame_deadline = now;

            iris->frame_deadline += target;

            constexpr int64_t SPIN_MARGIN_NS = 1000000; // 1 ms

            int64_t wait_ns = duration_cast<nanoseconds>(iris->frame_deadline - now).count();

            if (wait_ns > SPIN_MARGIN_NS)
                SDL_DelayNS((Uint64)(wait_ns - SPIN_MARGIN_NS));

            while (high_resolution_clock::now() < iris->frame_deadline)
                SDL_CPUPauseInstruction();
        } break;
    }

    return SDL_APP_CONTINUE;
}

static bool is_input_event(SDL_Event* event) {
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: return true;
    }

    return false;
}

static bool guest_owns_pointer(Instance* iris) {
    if (iris->input.usb_devices[0] == usb::USB_DEVICE_MOUSE ||
        iris->input.usb_devices[1] == usb::USB_DEVICE_MOUSE)
        return true;

    if (iris->ps2 && iris->ps2->s2x6_acjv &&
        iris->ps2->s2x6_acjv->mode == s2x6::acjv::MODE_LIGHTGUN)
        return true;

    return false;
}

static void handle_lightgun_event(Instance* iris, SDL_Event* event) {
    s2x6::acjv::Acjv* acjv = iris->ps2 ? iris->ps2->s2x6_acjv : nullptr;

    if (!acjv || acjv->mode != s2x6::acjv::MODE_LIGHTGUN)
        return;

    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION: {
            if (!iris->render_width || !iris->render_height)
                return;

            s2x6::acjv::set_gun_position(acjv, 0,
                (event->motion.x - iris->render_x) / (float)iris->render_width,
                (event->motion.y - iris->render_y) / (float)iris->render_height);
        } break;

        case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
            s2x6::acjv::set_gun_off_screen(acjv, 0);
        } break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int pressed = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;

            if (event->button.button == SDL_BUTTON_LEFT)
                s2x6::acjv::set_gun_trigger(acjv, 0, pressed);

            if (event->button.button == SDL_BUTTON_RIGHT)
                s2x6::acjv::set_gun_pedal(acjv, 0, pressed);
        } break;
    }
}

SDL_AppResult handle_events(Instance* iris, SDL_Event* event) {
    bool skip_events = iris->ui.dim_active && (event->window.windowID == SDL_GetWindowID(iris->window));

    if (!skip_events) {
        ImGui_ImplSDL3_ProcessEvent(event);
    }

    if (iris->ui.loading_file_active && is_input_event(event)) {
        return SDL_APP_CONTINUE;
    }

    const bool window_focused = SDL_GetWindowFlags(iris->window) & SDL_WINDOW_INPUT_FOCUS;
    const bool key_event = event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP;

    if (key_event && iris->ps2 && iris->ps2->usb && window_focused && !ImGui::GetIO().WantCaptureKeyboard) {
        if (event->key.scancode <= 0xE7) {
            usb::kbd_key(iris->ps2->usb, (uint8_t)event->key.scancode, event->type == SDL_EVENT_KEY_DOWN);
        }
    }

    if (window_focused && !ImGui::GetIO().WantCaptureMouse) {
        handle_lightgun_event(iris, event);
    }

    if (iris->ps2 && iris->ps2->usb && window_focused && !ImGui::GetIO().WantCaptureMouse) {
        switch (event->type) {
            case SDL_EVENT_MOUSE_MOTION: {
                usb::mouse_move(iris->ps2->usb, (int)event->motion.xrel, (int)event->motion.yrel, 0);
            } break;

            case SDL_EVENT_MOUSE_WHEEL: {
                usb::mouse_move(iris->ps2->usb, 0, 0, (int)event->wheel.y);
            } break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                int button = -1;

                switch (event->button.button) {
                    case SDL_BUTTON_LEFT:   button = 0; break;
                    case SDL_BUTTON_RIGHT:  button = 1; break;
                    case SDL_BUTTON_MIDDLE: button = 2; break;
                }

                if (button >= 0) {
                    usb::mouse_button(iris->ps2->usb, button, event->type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                }
            } break;
        }
    }

    switch (event->type) {
        case SDL_EVENT_QUIT: {
            return SDL_APP_SUCCESS;
        } break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (ImGui::GetIO().WantCaptureMouse) {
                break;
            }

            if (guest_owns_pointer(iris)) {
                break;
            }

            if (event->button.button == SDL_BUTTON_LEFT && event->button.windowID == SDL_GetWindowID(iris->window)) {
                if (iris->input.double_click_counter) {
                    if ((SDL_GetTicks() - iris->input.double_click_counter) > iris->input.double_click_interval) {
                        iris->input.double_click_counter = SDL_GetTicks();
                    } else {
                        iris->fullscreen = !iris->fullscreen;

                        SDL_SetWindowFullscreen(iris->window, iris->fullscreen);
                    }
                } else {
                    iris->input.double_click_counter = SDL_GetTicks();
                }
            }
        } break;

        case SDL_EVENT_GAMEPAD_ADDED: {
            SDL_Gamepad* gamepad = SDL_OpenGamepad(event->gdevice.which);

            if (!gamepad) {
                SDL_Log("Failed to open gamepad ID %u: %s", (unsigned int) event->gdevice.which, SDL_GetError());
            }

            if (iris->input.ds[0] && ((iris->input.input_devices[0] == nullptr) || (iris->input.input_devices[0]->get_type() == 0))) {
                if (iris->input.input_devices[0]) delete iris->input.input_devices[0];

                iris->input.input_devices[0] = new GamepadDevice(event->gdevice.which);
                iris->input.input_devices[0]->set_slot(0);

                if (iris->input.input_map[0] <= 1) {
                    iris->input.input_map[0] = 1;
                }

                push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' connected to slot 1");
            } else if (iris->input.ds[1] && ((iris->input.input_devices[1] == nullptr) || (iris->input.input_devices[1]->get_type() == 0))) {
                if (iris->input.input_devices[1]) delete iris->input.input_devices[1];

                iris->input.input_devices[1] = new GamepadDevice(event->gdevice.which);
                iris->input.input_devices[1]->set_slot(1);

                if (iris->input.input_map[1] <= 1) {
                    iris->input.input_map[1] = 1;
                }

                push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' connected to slot 2");
            } else {
                push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' connected");
            }

            iris->input.gamepads[event->gdevice.which] = gamepad;
        } break;

        case SDL_EVENT_GAMEPAD_REMOVED: {
            SDL_Gamepad* gamepad = iris->input.gamepads[event->gdevice.which];

            for (int i = 0; i < 2; i++) {
                if (iris->input.input_devices[i] && iris->input.input_devices[i]->get_type() == 1) {
                    GamepadDevice* gp = static_cast<GamepadDevice*>(iris->input.input_devices[i]);

                    if (gp->get_id() == event->gdevice.which) {
                        delete iris->input.input_devices[i];
                        iris->input.input_devices[i] = new KeyboardDevice();

                        if (iris->input.input_map[i] <= 1) {
                            iris->input.input_map[i] = 0;
                        }

                        push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' in slot " + std::to_string(i + 1) + " disconnected");
                    }
                }
            }

            if (gamepad) {
                SDL_CloseGamepad(gamepad);

                iris->input.gamepads.erase(event->gdevice.which);
            }
        } break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            if (event->window.windowID == SDL_GetWindowID(iris->window)) {
                return SDL_APP_SUCCESS;
            }
        } break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        case SDL_EVENT_KEY_UP: {
            iris->input.last_input_event_read = false;
            iris->input.last_input_event = input::sdl_event_to_input_event(event);

            if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                iris->input.last_input_event_value = fabs(event->gaxis.value / 32767.0f);
            } else {
                iris->input.last_input_event_value = 1.0f;
            }

            if (window_focused) {
                if (iris->input.input_devices[0]) iris->input.input_devices[0]->handle_event(iris, event);
                if (iris->input.input_devices[1]) iris->input.input_devices[1]->handle_event(iris, event);
            }
        } break;

        case SDL_EVENT_KEY_DOWN: {
            input::handle_keydown_event(iris, event);
        } break;

        case SDL_EVENT_DROP_BEGIN: {
            if (iris->ui.dim_active)
                break;

            iris->ui.drop_file_active = true;
            iris->ui.drop_pos = ImVec2(-1.0f, -1.0f);

            imgui::start_dim(iris, 0.35f, 100);
        } break;

        case SDL_EVENT_DROP_POSITION: {
            if (event->drop.windowID != SDL_GetWindowID(iris->window))
                break;

            iris->ui.drop_pos = ImVec2(event->drop.x, event->drop.y);
        } break;
        
        case SDL_EVENT_DROP_COMPLETE: {
            iris->ui.drop_file_active = false;
            iris->ui.drop_pos = ImVec2(-1.0f, -1.0f);

            if (!iris->ui.loading_file_active) {
                imgui::end_dim(iris);
            }
        } break;

        case SDL_EVENT_DROP_FILE: {
            if (iris->ui.loading_file_active || !event->drop.data)
                break;

            std::string path(event->drop.data);

            std::filesystem::path p(path);

            float dx = event->drop.x;
            float dy = event->drop.y;

            bool on_insert =
                event->drop.windowID == SDL_GetWindowID(iris->window) &&
                dx >= iris->ui.drop_insert_min.x && dx <= iris->ui.drop_insert_max.x &&
                dy >= iris->ui.drop_insert_min.y && dy <= iris->ui.drop_insert_max.y;

            if (on_insert) {
                if (!emu::is_disc_image(path)) {
                    push_info(iris, "Only disc images can be inserted");

                    break;
                }

                if (emu::insert_disc(iris, path)) {
                    push_info(iris, "Failed to insert disc: " + path);
                } else {
                    add_recent(iris, path, RecentType::PS2);
                }

                break;
            }

            if (std::filesystem::is_regular_file(p)) {
                if (emu::open_file(iris, path)) {
                    push_info(iris, "Failed to open file: " + path);
                } else {
                    add_recent(iris, path, RecentType::PS2);
                }
            } else {
                if (emu::load_arcade(iris, path)) {
                    add_recent(iris, path, RecentType::ARCADE);
                } else {
                    push_info(iris, "Failed to boot arcade: " + path);
                }
            }

            // Maybe not needed anymore?
            // SDL_free(event->drop.data);
        } break;
    }

    return SDL_APP_CONTINUE;
}

int get_menubar_inset(Instance* iris) {
    if (menu::native() || iris->fullscreen)
        return 0;

    return iris->ui.menubar_height;
}

int get_menubar_height(Instance* iris) {
    int height = menu::native() ? 0 : iris->ui.menubar_height;

    if (iris->ui.show_status_bar)
        height += iris->ui.menubar_height;

    return height;
}

void destroy(Instance* iris) {
    if (!iris)
        return;

    log_close_file(iris);

    if (iris->snap_on_exit) {
        input::save_screenshot(iris);
    }

    for (int i = 0; i < 2; i++) {
        if (iris->input.input_devices[i]) {
            delete iris->input.input_devices[i];
            iris->input.input_devices[i] = nullptr;
        }
    }

    if (iris->ui.imgui_enable_viewports) {
        iris->applets.ee_control.open = false;
        iris->applets.ee_state.open = false;
        iris->applets.ee_interrupts.open = false;
        iris->applets.ee_dmac.open = false;
        iris->applets.iop_control.open = false;
        iris->applets.iop_state.open = false;
        iris->applets.iop_interrupts.open = false;
        iris->applets.iop_modules.open = false;
        iris->applets.iop_dma.open = false;
        iris->applets.gs_debugger.open = false;
        iris->applets.spu2_debugger.open = false;
        iris->applets.memory_viewer.open = false;
        iris->applets.memory_search.open = false;
        iris->applets.vu_disassembler.open = false;
        iris->applets.breakpoints.open = false;
        iris->applets.ee_threads.open = false;
        iris->applets.iop_threads.open = false;
        iris->applets.timers.open = false;
        iris->applets.logs.open = false;
        iris->ui.show_imgui_demo = false;
        iris->ui.show_overlay = false;
    }

    if (iris->window) SDL_HideWindow(iris->window);

    imgui::cleanup(iris);
    audio::close(iris);
    settings::close(iris);
    render::destroy(iris);
    vulkan::cleanup(iris);
    platform::destroy(iris);
    emu::destroy(iris);
    gamelist::destroy(iris);
    net::cleanup();

    if (iris->window) SDL_DestroyWindow(iris->window);

    SDL_Quit();

    if (iris->logger) logger::destroy(iris->logger);

    delete iris;
}

}