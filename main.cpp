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
#include "ee/ee_def.hpp"

// ImGui includes
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

// SDL3 includes
#include <SDL3/SDL.h>

// External includes
#include "res/IconsMaterialSymbols.h"

// INCBIN stuff
#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE

#include "incbin.h"

INCBIN(roboto, "../res/Roboto-Regular.ttf");
INCBIN(symbols, "../res/MaterialSymbolsRounded.ttf");
INCBIN(firacode, "../res/FiraCode-Regular.ttf");
INCBIN(ps1_memory_card_icon, "../res/ps1_mcd.png");
INCBIN(ps2_memory_card_icon, "../res/ps2_mcd.png");
INCBIN(pocketstation_icon, "../res/pocketstation.png");
INCBIN(iris_icon, "../res/iris.png");

// INCBIN_EXTERN(roboto);
// INCBIN_EXTERN(symbols);
// INCBIN_EXTERN(firacode);
// INCBIN_EXTERN(ps1_memory_card_icon);
// INCBIN_EXTERN(ps2_memory_card_icon);
// INCBIN_EXTERN(pocketstation_icon);
// INCBIN_EXTERN(iris_icon);

// stb_image stuff
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

// Icon range (for our fonts)
static const ImWchar g_icon_range[] = { ICON_MIN_MS, ICON_MAX_16_MS, 0 };

SDL_GPUTexture* load_texture(iris::instance* iris, SDL_GPUCopyPass* cp, void* buf, size_t size, uint32_t* width, uint32_t* height) {
    SDL_GPUTexture* texture = nullptr;

    stbi_uc* data = stbi_load_from_memory((stbi_uc*)buf, size, (int*)width, (int*)height, nullptr, 4);

    if (!data) {
        printf("Error: stbi_load_from_memory() failed: %s\n", stbi_failure_reason());

        return texture;
    }

    SDL_GPUTextureCreateInfo tci = {};

    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.width = *width;
    tci.height = *height;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUSamplerCreateInfo sci = {};

    sci.min_filter = SDL_GPU_FILTER_LINEAR;
    sci.mag_filter = SDL_GPU_FILTER_LINEAR;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    texture = SDL_CreateGPUTexture(iris->device, &tci);

    // Transfer the texture
    SDL_GPUTransferBufferCreateInfo ttbci = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = ((*width) * 4) * (*height)
    };

    SDL_GPUTransferBuffer* ttb = SDL_CreateGPUTransferBuffer(iris->device, &ttbci);

    // fill the transfer buffer
    void* ptr = SDL_MapGPUTransferBuffer(iris->device, ttb, false);

    SDL_memcpy(ptr, data, ((*width) * 4) * (*height));

    SDL_UnmapGPUTransferBuffer(iris->device, ttb);

    SDL_GPUTextureTransferInfo tti = {
        .transfer_buffer = ttb,
        .offset = 0,
    };

    SDL_GPUTextureRegion tr = {
        .texture = texture,
        .w = *width,
        .h = *height,
        .d = 1,
    };

    SDL_UploadToGPUTexture(cp, &tti, &tr, false);

    stbi_image_free(data);

    return texture;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    SDL_SetAppMetadata("Iris", STR(_IRIS_VERSION), "com.allkern.iris");

    iris::instance* iris = new iris::instance;

    // Check if we got --help or --version in the commandline args
    // if so, don't do anything else.
    cli_check_for_help_version(iris, argc, (const char**)argv);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());

        return SDL_APP_FAILURE;
    }

    // Create and check window
    iris->window = SDL_CreateWindow(
        IRIS_TITLE,
        iris->window_width, iris->window_height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN
    );

    if (!iris->window) {
        printf("iris: SDL_CreateWindow() failed \'%s\'\n", SDL_GetError());

        return SDL_APP_FAILURE;
    }

    // Create, check and setup GPU device
    iris->device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV |
        SDL_GPU_SHADERFORMAT_DXIL |
        SDL_GPU_SHADERFORMAT_METALLIB,
        true,
        nullptr
    );

    if (!iris->device) {
        printf("iris: SDL_CreateGPUDevice() failed \'%s\'\n", SDL_GetError());

        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(iris->device, iris->window)) {
        printf("iris: SDL_ClaimWindowForGPUDevice() failed \'%s\'\n", SDL_GetError());

        return SDL_APP_FAILURE;
    }

    SDL_ShowWindow(iris->window);

    SDL_SetGPUSwapchainParameters(
        iris->device, iris->window,
        SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
        SDL_GPU_PRESENTMODE_MAILBOX
    );

    // Init audio
    SDL_AudioSpec spec;

    spec.channels = 2;
    spec.format = SDL_AUDIO_S16;
    spec.freq = 48000;

    iris->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, iris::audio_update, iris);

    if (!iris->stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());

        return SDL_APP_FAILURE;
    }

    /* SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to start! */
    SDL_ResumeAudioStreamDevice(iris->stream);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable docking

    ImGui_ImplSDL3_InitForSDLGPU(iris->window);
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = iris->device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(iris->device, iris->window);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&init_info);

    // Init fonts
    io.Fonts->AddFontDefault();

    ImFontConfig config;
    config.MergeMode = true;
    config.GlyphMinAdvanceX = 13.0f;
    config.GlyphOffset = ImVec2(0.0f, 4.0f);
    config.FontDataOwnedByAtlas = false;

    ImFontConfig config_no_own;
    config_no_own.FontDataOwnedByAtlas = false;

    iris->font_small_code = io.Fonts->AddFontFromMemoryTTF((void*)g_firacode_data, g_firacode_size, 12.0F, &config_no_own);
    iris->font_code       = io.Fonts->AddFontFromMemoryTTF((void*)g_firacode_data, g_firacode_size, 16.0F, &config_no_own);
    iris->font_small      = io.Fonts->AddFontFromMemoryTTF((void*)g_roboto_data, g_roboto_size, 12.0F, &config_no_own);
    iris->font_heading    = io.Fonts->AddFontFromMemoryTTF((void*)g_roboto_data, g_roboto_size, 20.0F, &config_no_own);
    iris->font_body       = io.Fonts->AddFontFromMemoryTTF((void*)g_roboto_data, g_roboto_size, 16.0F, &config_no_own);
    iris->font_icons      = io.Fonts->AddFontFromMemoryTTF((void*)g_symbols_data, g_symbols_size, 20.0F, &config, g_icon_range);
    iris->font_icons_big  = io.Fonts->AddFontFromMemoryTTF((void*)g_symbols_data, g_symbols_size, 50.0F, &config_no_own, g_icon_range);

    IM_ASSERT(iris->font_small_code != nullptr);
    IM_ASSERT(iris->font_code != nullptr);
    IM_ASSERT(iris->font_small != nullptr);
    IM_ASSERT(iris->font_heading != nullptr);
    IM_ASSERT(iris->font_body != nullptr);
    IM_ASSERT(iris->font_icons != nullptr);
    IM_ASSERT(iris->font_icons_big != nullptr);

    io.FontDefault = iris->font_body;

    // Init backends
    init_platform(iris);
    init_audio(iris);

    // Init preferences stuff
    if (std::filesystem::exists("portable")) {
        iris->pref_path = "./";
    } else {
        char* pref = SDL_GetPrefPath("Allkern", "Iris");

        iris->pref_path = std::string(pref);

        SDL_free(pref);
    }

    iris->ini_path = iris->pref_path + "imgui.ini";

    io.IniFilename = iris->ini_path.c_str();

    // Init 'Granite' theme
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding           = ImVec2(8.0, 8.0);
    style.FramePadding            = ImVec2(5.0, 5.0);
    style.ItemSpacing             = ImVec2(8.0, 4.0);
    style.WindowBorderSize        = 0;
    style.ChildBorderSize         = 0;
    style.FrameBorderSize         = 1;
    style.PopupBorderSize         = 0;
    style.TabBorderSize           = 0;
    style.TabBarBorderSize        = 0;
    style.WindowRounding          = 6;
    style.ChildRounding           = 4;
    style.FrameRounding           = 4;
    style.PopupRounding           = 4;
    style.ScrollbarRounding       = 9;
    style.GrabRounding            = 2;
    style.TabRounding             = 4;
    style.WindowTitleAlign        = ImVec2(0.5, 0.5);
    style.DockingSeparatorSize    = 0;
    style.SeparatorTextBorderSize = 1;
    style.SeparatorTextPadding    = ImVec2(20, 0);

    // Use ImGui's default dark style as a base for our own style
    ImGui::StyleColorsDark();

    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.07f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_Border]                 = ImVec4(0.10f, 0.12f, 0.13f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.10f, 0.12f, 0.13f, 0.50f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.24f, 0.26f, 0.50f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.29f, 0.35f, 0.39f, 0.50f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.39f, 0.47f, 0.52f, 0.50f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.49f, 0.59f, 0.65f, 0.50f);
    colors[ImGuiCol_Button]                 = ImVec4(0.13f, 0.16f, 0.17f, 0.25f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.20f, 0.24f, 0.26f, 0.50f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.29f, 0.35f, 0.39f, 0.50f);
    colors[ImGuiCol_Header]                 = ImVec4(0.13f, 0.16f, 0.17f, 0.50f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.20f, 0.24f, 0.26f, 0.50f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.29f, 0.35f, 0.39f, 0.50f);
    colors[ImGuiCol_Separator]              = ImVec4(0.23f, 0.28f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.33f, 0.39f, 0.43f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.38f, 0.46f, 0.51f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.00f, 0.30f, 0.25f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.00f, 0.39f, 0.32f, 1.00f);
    colors[ImGuiCol_InputTextCursor]        = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.23f, 0.28f, 0.30f, 0.59f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.20f, 0.24f, 0.26f, 0.59f);
    colors[ImGuiCol_TabSelected]            = ImVec4(0.26f, 0.31f, 0.35f, 0.59f);
    colors[ImGuiCol_TabSelectedOverline]    = ImVec4(0.00f, 0.39f, 0.32f, 1.00f);
    colors[ImGuiCol_TabDimmed]              = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabDimmedSelected]      = ImVec4(0.10f, 0.12f, 0.13f, 1.00f);
    colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink]               = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavCursor]              = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);

    iris->open = true;

    // Initialize our emulator state
    iris->ps2 = ps2_create();

    ps2_init(iris->ps2);
    ps2_init_kputchar(iris->ps2, iris::handle_ee_tty_event, iris, iris::handle_iop_tty_event, iris);

    // Note: Important change, we are NOT going to be using the Vblank callback anymore.
    //       Frame timing will be controlled entirely by the renderer/frontend from now on
    // ps2_gs_init_callback(iris->ps2->gs, GS_EVENT_VBLANK, (void (*)(void*))iris::update_window, iris);
    ps2_gs_init_callback(iris->ps2->gs, GS_EVENT_SCISSOR, iris::handle_scissor_event, iris);

    iris->ds[0] = ds_sio2_attach(iris->ps2->sio2, 0);
    iris->mcd[0] = mcd_sio2_attach(iris->ps2->sio2, 2, "slot1.mcd");

    ps1_mcd_sio2_attach(iris->ps2->sio2, 3, "slot1_ps1.mcd");

    iris->ctx = renderer_create();

    renderer_init_backend(iris->ctx, iris->ps2->gs, iris->window, iris->device, RENDERER_NULL);

    iris->ticks = SDL_GetTicks();
    iris->pause = true;
    
    init_settings(iris, argc, (const char**)argv);
    
    renderer_init_backend(iris->ctx, iris->ps2->gs, iris->window, iris->device, iris->renderer_backend);
    renderer_set_scale(iris->ctx, iris->scale);
    renderer_set_aspect_mode(iris->ctx, iris->aspect_mode);
    renderer_set_bilinear(iris->ctx, iris->bilinear);
    renderer_set_integer_scaling(iris->ctx, iris->integer_scaling);

    // Initialize assets
    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(iris->device);
    SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cb);

    iris->ps2_memory_card_icon_tex = load_texture(
        iris, cp,
        (void*)g_ps2_memory_card_icon_data,
        g_ps2_memory_card_icon_size,
        &iris->ps2_memory_card_icon_width,
        &iris->ps2_memory_card_icon_height
    );

    iris->ps1_memory_card_icon_tex = load_texture(
        iris, cp,
        (void*)g_ps1_memory_card_icon_data,
        g_ps1_memory_card_icon_size,
        &iris->ps1_memory_card_icon_width,
        &iris->ps1_memory_card_icon_height
    );

    iris->pocketstation_icon_tex = load_texture(
        iris, cp,
        (void*)g_pocketstation_icon_data,
        g_pocketstation_icon_size,
        &iris->pocketstation_icon_width,
        &iris->pocketstation_icon_height
    );

    iris->iris_icon_tex = load_texture(
        iris, cp,
        (void*)g_iris_icon_data,
        g_iris_icon_size,
        &iris->iris_icon_width,
        &iris->iris_icon_height
    );

    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cb);

    // Initialize appstate
    *appstate = iris;

    return SDL_APP_CONTINUE;
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

SDL_AppResult SDL_AppIterate(void* appstate) {
    iris::instance* iris = (iris::instance*)appstate;

    if (iris->pause) {
        iris->step_out = false;
        iris->step_over = false;

        if (iris->step) {
            ps2_cycle(iris->ps2);

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

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    iris::instance* iris = (iris::instance*)appstate;

    ImGui_ImplSDL3_ProcessEvent(event);

    switch (event->type) {
        case SDL_EVENT_QUIT: {
            return SDL_APP_SUCCESS;
        } break;

        case SDL_EVENT_KEY_DOWN: {
            handle_keydown_event(iris, event->key);
        } break;

        case SDL_EVENT_KEY_UP: {
            handle_keyup_event(iris, event->key);
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

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    iris::instance* iris = (iris::instance*)appstate;

    // Iris-specific cleanup
    renderer_destroy(iris->ctx);
    destroy_platform(iris);
    close_settings(iris);

    // Cleanup
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    SDL_WaitForGPUIdle(iris->device);
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();

    // Release resources
    SDL_ReleaseGPUTexture(iris->device, iris->ps2_memory_card_icon_tex);
    SDL_ReleaseGPUTexture(iris->device, iris->ps1_memory_card_icon_tex);
    SDL_ReleaseGPUTexture(iris->device, iris->pocketstation_icon_tex);
    SDL_ReleaseGPUTexture(iris->device, iris->iris_icon_tex);

    SDL_ReleaseWindowFromGPUDevice(iris->device, iris->window);
    SDL_DestroyGPUDevice(iris->device);
    SDL_DestroyWindow(iris->window);

    ps2_destroy(iris->ps2);

    delete iris;
}