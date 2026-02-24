// Standard includes
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <cmath>

// Iris includes
#include "iris.hpp"
#include "config.hpp"
#include "ee/ee_def.hpp"

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
    if (ext == ".iso" || ext == ".bin" || ext == ".cue" ||
        ext == ".chd" || ext == ".cso" || ext == ".zso") {
        if (ps2_cdvd_open(iris->ps2->cdvd, file.c_str(), 0))
            return 1;

        char* boot_file = disc_get_boot_path(iris->ps2->cdvd->disc);

        if (!boot_file)
            return 2;

        elf::load_symbols_from_disc(iris);

        renderer_reset(iris->renderer);

        ps2_boot_file(iris->ps2, boot_file);

        iris->loaded = file;

        return 0;
    }

    elf::load_symbols_from_file(iris, file);

    // Note: We need the trailing whitespaces here because of IOMAN HLE
    // Load executable
    file = "host:  " + file;

    renderer_reset(iris->renderer);

    ps2_boot_file(iris->ps2, file.c_str());

    iris->loaded = file;

    return 0;
}

void update_title(iris::instance* iris) {
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

    std::this_thread::sleep_for(std::chrono::milliseconds(ticks / 2));

    // uint32_t now = SDL_GetTicks();

    // while ((SDL_GetTicks() - now) < ticks) {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(ticks / 4));
    // }
}

static inline void do_cycle(iris::instance* iris) {
    ps2_cycle(iris->ps2);

    if (iris->step_out) {
        // jr $ra
        if (iris->ps2->ee->opcode == 0x03e00008) {
            iris->step_out = false;
            iris->pause = true;

            // Consume the delay slot
            ps2_cycle(iris->ps2);
        }
    }

    if (iris->step_over) {
        if (iris->ps2->ee->pc == iris->step_over_addr) {
            iris->step_over = false;
            iris->pause = true;
        }
    }

    for (const iris::breakpoint& b : iris->breakpoints) {
        if (b.cpu == iris::BKPT_CPU_EE) {
            if (iris->ps2->ee->pc == b.addr) {
                iris->pause = true;
            }
        } else {
            if (iris->ps2->iop->pc == b.addr) {
                iris->pause = true;
            }
        }
    }
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
    if (SDL_GetWindowFlags(iris->window) & SDL_WINDOW_MINIMIZED) {
        SDL_Delay(1);

        return;
    }

    // Resize swapchain?
    int width, height;

    SDL_GetWindowSize(iris->window, &width, &height);

    if (width > 0 && height > 0 && (iris->swapchain_rebuild || iris->main_window_data.Width != width || iris->main_window_data.Height != height)) {
        ImGui_ImplVulkan_SetMinImageCount(iris->min_image_count);
    
        ImGui_ImplVulkanH_CreateOrResizeWindow(
            iris->instance,
            iris->physical_device,
            iris->device,
            &iris->main_window_data,
            iris->queue_family,
            nullptr,
            width, height,
            iris->min_image_count,
            0
        );

        iris->main_window_data.FrameIndex = 0;
        iris->swapchain_rebuild = false;
    }

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (!iris->fullscreen) {
        show_main_menubar(iris);
    }

    DockSpaceOverViewport(0, GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // Drop file fade animation
    if (iris->drop_file_active) {
        iris->drop_file_alpha += iris->drop_file_alpha_delta;

        if (iris->drop_file_alpha_delta > 0.0f) {
            if (iris->drop_file_alpha >= 1.0f) {
                iris->drop_file_alpha = 1.0f;
                iris->drop_file_alpha_delta = 0.0f;
            }
        } else {
            if (iris->drop_file_alpha <= 0.0f) {
                iris->drop_file_alpha = 0.0f;
                iris->drop_file_alpha_delta = 0.0f;
                iris->drop_file_active = false;
            }
        }

        GetForegroundDrawList()->AddRectFilled(
            ImVec2(0, 0),
            ImVec2(width, height),
            ImColor(0.0f, 0.0f, 0.0f, iris->drop_file_alpha * 0.35f)
        );

        ImVec2 text_size = CalcTextSize("Drop file here to launch");

        PushFont(iris->font_icons_big);

        ImVec2 icon_size = CalcTextSize(ICON_MS_DOWNLOAD);

        ImVec2 total_size = ImVec2(
            std::max(icon_size.x, text_size.x),
            icon_size.y + text_size.y
        );

        GetForegroundDrawList()->AddText(
            ImVec2(width / 2 - icon_size.x / 2, height / 2 - icon_size.y),
            ImColor(1.0f, 1.0f, 1.0f, iris->drop_file_alpha),
            ICON_MS_DOWNLOAD
        );

        PopFont();

        GetForegroundDrawList()->AddText(
            ImVec2(width / 2 - text_size.x / 2, height / 2),
            ImColor(1.0f, 1.0f, 1.0f, iris->drop_file_alpha),
            "Drop file here to launch"
        );
    }

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
    if (iris->show_vu_disassembler) show_vu_disassembler(iris);
    if (iris->show_status_bar && !iris->fullscreen) show_status_bar(iris);
    if (iris->show_breakpoints) show_breakpoints(iris);
    if (iris->show_about_window) show_about_window(iris);
    if (iris->show_settings) show_settings(iris);
    if (iris->show_pad_debugger) show_pad_debugger(iris);
    if (iris->show_symbols) show_symbols(iris);
    if (iris->show_threads) show_threads(iris);
    if (iris->show_sysmem_logs) show_sysmem_logs(iris);
    if (iris->show_memory_card_tool) show_memory_card_tool(iris);
    if (iris->show_memory_search) show_memory_search(iris);
    // if (iris->show_gamelist) show_gamelist(iris);
    if (iris->show_imgui_demo) ShowDemoWindow(&iris->show_imgui_demo);
    if (iris->show_bios_setting_window) show_bios_setting_window(iris);
    if (iris->show_overlay) show_overlay(iris);

    // Display little pause icon in the top right corner
    if (iris->pause) {
        ImVec2 ts = CalcTextSize(ICON_MS_PAUSE);
        ImVec2 offset = ImVec2(10.0f, 10.0f);
        ImVec2 padding = ImVec2(0.0f, 0.0f);

        ts.x -= 1.0f;

        int menubar_offset = 0;

        if (!iris->fullscreen) {
            menubar_offset += iris->menubar_height;
        }

        // GetBackgroundDrawList()->AddRectFilled(
        //     ImVec2(width - ts.x - offset.x - padding.x, menubar_offset + offset.y - padding.y),
        //     ImVec2(width - offset.x + padding.x, menubar_offset + ts.y + offset.y + padding.y),
        //     GetColorU32(GetStyleColorVec4(ImGuiCol_WindowBg)), 8.0f
        // );

        GetBackgroundDrawList(GetMainViewport())->AddText(
            ImVec2(width - ts.x - offset.x, menubar_offset + offset.y),
            GetColorU32(GetStyleColorVec4(ImGuiCol_Text)),
            ICON_MS_PAUSE
        );
    }

    handle_animations(iris);

    // Rendering
    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();

    const bool main_is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

    iris->main_window_data.ClearValue.color.float32[0] = 0.0f;
    iris->main_window_data.ClearValue.color.float32[1] = 0.0f;
    iris->main_window_data.ClearValue.color.float32[2] = 0.0f;
    iris->main_window_data.ClearValue.color.float32[3] = 1.0f;

    if (!main_is_minimized) {
        if (!imgui::render_frame(iris, draw_data)) {
            printf("iris: Failed to render ImGui frame\n");
        }
    }

    iris->frames++;
}

iris::instance* create() {
    return new iris::instance();
}

bool init(iris::instance* iris, int argc, const char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "iris: Failed to init SDL \'%s\'\n", SDL_GetError());

        return false;
    }

    // Create and check window
    iris->main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    // Init preferences path
    if (std::filesystem::exists("portable")) {
        iris->pref_path = "./";
    } else {
        char* pref = SDL_GetPrefPath("Allkern", "Iris");

        iris->pref_path = std::string(pref);

        SDL_free(pref);
    }

    if (!iris::emu::init(iris)) {
        fprintf(stderr, "iris: Failed to initialize emulator state\n");

        return false;
    }

    if (!iris::settings::init(iris, argc, argv)) {
        fprintf(stderr, "iris: Failed to initialize settings\n");

        return false;
    }

    iris->window = SDL_CreateWindow(
        IRIS_TITLE,
        iris->window_width, iris->window_height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN
    );

    if (!iris->window) {
        printf("iris: Failed to create SDL window \'%s\'\n", SDL_GetError());

        return false;
    }

    if (!iris::vulkan::init(iris, iris->vulkan_enable_validation_layers)) {
        fprintf(stderr, "iris: Failed to initialize Vulkan\n");

        return false;
    }

    if (!iris::imgui::init(iris)) {
        fprintf(stderr, "iris: Failed to initialize ImGui\n");

        return false;
    }

    if (!iris::platform::init(iris)) {
        fprintf(stderr, "iris: Failed to initialize platform\n");

        return false;
    }

    if (!iris::audio::init(iris)) {
        fprintf(stderr, "iris: Failed to initialize audio\n");

        return false;
    }

    if (!iris::render::init(iris)) {
        fprintf(stderr, "iris: Failed to initialize render state\n");

        return false;
    }

    if (!iris::input::init(iris)) {
        fprintf(stderr, "iris: Failed to initialize input\n");

        return false;
    }

    for (const std::string& s : iris->shader_passes_pending)
        shaders::push(iris, s);

    iris->shader_passes_pending.clear();

    // Sadly we need to start a frame here to measure menubar height
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    iris->menubar_height = ImGui::GetFrameHeight();

    ImGui::EndFrame();

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    SDL_SetWindowSize(iris->window, iris->window_width, iris->window_height + get_menubar_height(iris));
    SDL_ShowWindow(iris->window);

    return true;
}

SDL_AppResult update(iris::instance* iris) {
    if (iris->pause) {
        iris->step_out = false;
        iris->step_over = false;

        if (iris->step) {
            ps2_step_ee(iris->ps2);

            iris->step = false;
        }

        iris::update_window(iris);

        return SDL_APP_CONTINUE;
    }

    // Execute until VBlank
    while (!ps2_gs_is_vblank(iris->ps2->gs)) {
        do_cycle(iris);

        if (iris->pause) {
            iris::update_window(iris);

            return SDL_APP_CONTINUE;
        }
    }

    // Draw frame
    iris::update_window(iris);
    
    // Execute until vblank is over
    while (ps2_gs_is_vblank(iris->ps2->gs)) {
        do_cycle(iris);

        if (iris->pause) {
            iris::update_window(iris);

            return SDL_APP_CONTINUE;
        }
    }

    // float p = ((float)iris->ps2->ee->eenull_counter / (float)(4920115)) * 100.0f;

    // printf("ee: Time spent idling: %ld cycles (%.2f%%) INTC reads: %d CSR reads: %d (%.1f fps)\n", iris->ps2->ee->eenull_counter, p, iris->ps2->ee->intc_reads, iris->ps2->ee->csr_reads, 1.0f / ImGui::GetIO().DeltaTime);

    iris->ps2->ee->eenull_counter = 0;
    iris->ps2->ee->intc_reads = 0;
    iris->ps2->ee->csr_reads = 0;

    return SDL_APP_CONTINUE;
}

SDL_AppResult handle_events(iris::instance* iris, SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);

    switch (event->type) {
        case SDL_EVENT_QUIT: {
            return SDL_APP_SUCCESS;
        } break;

        case SDL_EVENT_GAMEPAD_ADDED: {
            SDL_Gamepad* gamepad = SDL_OpenGamepad(event->gdevice.which);

            if (!gamepad) {
                SDL_Log("Failed to open gamepad ID %u: %s", (unsigned int) event->gdevice.which, SDL_GetError());
            }

            if (iris->ds[0] && ((iris->input_devices[0] == nullptr) || (iris->input_devices[0]->get_type() == 0))) {
                if (iris->input_devices[0]) delete iris->input_devices[0];

                iris->input_devices[0] = new iris::gamepad_device(event->gdevice.which);
                iris->input_devices[0]->set_slot(0);

                if (iris->input_map[0] <= 1) {
                    iris->input_map[0] = 1;
                }

                push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' connected to slot 1");
            } else if (iris->ds[1] && ((iris->input_devices[1] == nullptr) || (iris->input_devices[1]->get_type() == 0))) {
                if (iris->input_devices[1]) delete iris->input_devices[1];

                iris->input_devices[1] = new iris::gamepad_device(event->gdevice.which);
                iris->input_devices[1]->set_slot(1);

                if (iris->input_map[1] <= 1) {
                    iris->input_map[1] = 1;
                }

                push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' connected to slot 2");
            } else {
                push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' connected");
            }

            iris->gamepads[event->gdevice.which] = gamepad;
        } break;

        case SDL_EVENT_GAMEPAD_REMOVED: {
            SDL_Gamepad* gamepad = iris->gamepads[event->gdevice.which];

            for (int i = 0; i < 2; i++) {
                if (iris->input_devices[i] && iris->input_devices[i]->get_type() == 1) {
                    iris::gamepad_device* gp = static_cast<iris::gamepad_device*>(iris->input_devices[i]);

                    if (gp->get_id() == event->gdevice.which) {
                        delete iris->input_devices[i];
                        iris->input_devices[i] = new iris::keyboard_device();

                        if (iris->input_map[i] <= 1) {
                            iris->input_map[i] = 0;
                        }

                        push_info(iris, "\'" + std::string(SDL_GetGamepadName(gamepad)) + "\' in slot " + std::to_string(i + 1) + " disconnected");
                    }
                }
            }

            if (gamepad) {
                SDL_CloseGamepad(gamepad);

                iris->gamepads.erase(event->gdevice.which);
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
            iris->last_input_event_read = false;
            iris->last_input_event = input::sdl_event_to_input_event(event);

            if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                iris->last_input_event_value = fabs(event->gaxis.value / 32767.0f);
            } else {
                iris->last_input_event_value = 1.0f;
            }

            if (iris->input_devices[0]) iris->input_devices[0]->handle_event(iris, event);
            if (iris->input_devices[1]) iris->input_devices[1]->handle_event(iris, event);
        } break;

        case SDL_EVENT_KEY_DOWN: {
            input::handle_keydown_event(iris, event);
        } break;

        case SDL_EVENT_DROP_BEGIN: {
            iris->drop_file_active = true;
            iris->drop_file_alpha = 0.0f;
            iris->drop_file_alpha_delta = 1.0f / 10.0f;
            iris->drop_file_alpha_target = 1.0f;
        } break;
        
        case SDL_EVENT_DROP_COMPLETE: {
            iris->drop_file_active = true;
            iris->drop_file_alpha = iris->drop_file_alpha_target;
            iris->drop_file_alpha_delta = -(1.0f / 10.0f);
            iris->drop_file_alpha_target = 0.0f;
        } break;

        case SDL_EVENT_DROP_FILE: {
            if (!event->drop.data)
                break;

            std::string path(event->drop.data);

            if (open_file(iris, path)) {
                push_info(iris, "Failed to open file: " + path);
            } else {
                add_recent(iris, path);
            }

            // Maybe not needed anymore?
            // SDL_free(event->drop.data);
        } break;
    }

    return SDL_APP_CONTINUE;
}

int get_menubar_height(iris::instance* iris) {
    if (iris->show_status_bar) {
        return iris->menubar_height * 2;
    }

    return iris->menubar_height;
}

void destroy(iris::instance* iris) {
    for (int i = 0; i < 2; i++) {
        if (iris->input_devices[i]) {
            delete iris->input_devices[i];
            iris->input_devices[i] = nullptr;
        }
    }

    if (iris->imgui_enable_viewports) {
        iris->show_ee_control = false;
        iris->show_ee_state = false;
        iris->show_ee_logs = false;
        iris->show_ee_interrupts = false;
        iris->show_ee_dmac = false;
        iris->show_iop_control = false;
        iris->show_iop_state = false;
        iris->show_iop_logs = false;
        iris->show_iop_interrupts = false;
        iris->show_iop_modules = false;
        iris->show_iop_dma = false;
        iris->show_gs_debugger = false;
        iris->show_spu2_debugger = false;
        iris->show_memory_viewer = false;
        iris->show_memory_search = false;
        iris->show_vu_disassembler = false;
        iris->show_status_bar = false;
        iris->show_breakpoints = false;
        iris->show_threads = false;
        iris->show_sysmem_logs = false;
        iris->show_imgui_demo = false;
        iris->show_overlay = false;
    }

    if (iris->window) SDL_HideWindow(iris->window);

    iris::imgui::cleanup(iris);
    iris::audio::close(iris);
    iris::settings::close(iris);
    iris::render::destroy(iris);
    iris::vulkan::cleanup(iris);
    iris::platform::destroy(iris);
    iris::emu::destroy(iris);

    if (iris->window) SDL_DestroyWindow(iris->window);

    SDL_Quit();

    delete iris;
}

}