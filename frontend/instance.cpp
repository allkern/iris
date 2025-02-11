#include <csignal>
#include <chrono>
#include <string>
#include <vector>

#include "instance.hpp"

#include "ps2_elf.h"
#include "ps2_iso9660.h"

#include "ee/ee_dis.h"
#include "iop/iop_dis.h"

#include "res/IconsMaterialSymbols.h"
#include "tfd/tinyfiledialogs.h"

#include "ee/renderer/software.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <stdio.h>
#include <SDL_filesystem.h>
#include <SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

namespace lunar {

uint32_t map_button(SDL_Keycode k) {
    if (k == SDLK_x     ) return DS_BT_CROSS;
    if (k == SDLK_a     ) return DS_BT_SQUARE;
    if (k == SDLK_w     ) return DS_BT_TRIANGLE;
    if (k == SDLK_d     ) return DS_BT_CIRCLE;
    if (k == SDLK_RETURN) return DS_BT_START;
    if (k == SDLK_s     ) return DS_BT_SELECT;
    if (k == SDLK_UP    ) return DS_BT_UP;
    if (k == SDLK_DOWN  ) return DS_BT_DOWN;
    if (k == SDLK_LEFT  ) return DS_BT_LEFT;
    if (k == SDLK_RIGHT ) return DS_BT_RIGHT;
    if (k == SDLK_q     ) return DS_BT_L1;
    if (k == SDLK_e     ) return DS_BT_R1;
    if (k == SDLK_1     ) return DS_BT_L2;
    if (k == SDLK_3     ) return DS_BT_R2;
    if (k == SDLK_z     ) return DS_BT_L3;
    if (k == SDLK_c     ) return DS_BT_R3;

    return 0;
}

void kputchar_stub(void* udata, char c) {
    putchar(c);
}

void ee_kputchar(void* udata, char c) {
    lunar::instance* lunar = (lunar::instance*)udata;
    
    if (c == '\r')
        return;

    if (c == '\n') {
        lunar->ee_log.push_back("");
    } else {
        lunar->ee_log.back().push_back(c);
    }
}

void iop_kputchar(void* udata, char c) {
    lunar::instance* lunar = (lunar::instance*)udata;
    
    if (c == '\r')
        return;

    if (c == '\n') {
        lunar->iop_log.push_back("");
    } else {
        lunar->iop_log.back().push_back(c);
    }
}

static const ImWchar icon_range[] = { ICON_MIN_MS, ICON_MAX_16_MS, 0 };

void handle_scissor_event(void* udata) {
    lunar::instance* lunar = (lunar::instance*)udata;

    software_set_size(lunar->ctx, 0, 0);
}

lunar::instance* create();

lunar::instance* g_lunar = nullptr;

static const char *ee_cc_r2[] = {
    "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

void sigint_handler(int signal);

void atexit_handler() {
    sigint_handler(0);
}

void sigint_handler(int signal) {
    char buf[128];
    struct ee_dis_state ee_ds;
    struct iop_dis_state iop_ds;

    ee_ds.print_address = 1;
    ee_ds.print_opcode = 1;
    iop_ds.print_address = 1;
    iop_ds.print_opcode = 1;

    struct ee_state* ee = g_lunar->ps2->ee;

    FILE* file = fopen("ram.dump", "wb");

    fwrite(g_lunar->ps2->ee_ram->buf, 1, RAM_SIZE_32MB, file);
    fclose(file);

    for (int i = 0; i < 32; i++) {
        printf("%s: %08x %08x %08x %08x\n",
            ee_cc_r2[i],
            ee->r[i].u32[3],
            ee->r[i].u32[2],
            ee->r[i].u32[1],
            ee->r[i].u32[0]
        );
    }

    printf("hi: %08x %08x %08x %08x\n",
        ee->hi.u32[3],
        ee->hi.u32[2],
        ee->hi.u32[1],
        ee->hi.u32[0]
    );
    printf("lo: %08x %08x %08x %08x\n",
        ee->hi.u32[3],
        ee->hi.u32[2],
        ee->hi.u32[1],
        ee->hi.u32[0]
    );
    printf("pc: %08x\n", ee->pc);

    for (int i = -8; i <= 8; i++) {
        ee_ds.pc = ee->pc + (i * 4);

        if (ee->pc == ee_ds.pc) {
            printf("--> ");
        } else {
            printf("    ");
        }

        puts(ee_disassemble(buf, ee_bus_read32(ee->bus.udata, ee_ds.pc & 0x1fffffff), &ee_ds));
    }

    printf("intc: stat=%08x mask=%08x\n",
        g_lunar->ps2->ee_intc->stat,
        g_lunar->ps2->ee_intc->mask
    );

    printf("dmac: stat=%08x mask=%08x\n",
        g_lunar->ps2->ee_dma->stat & 0x3ff,
        (g_lunar->ps2->ee_dma->stat >> 16) & 0x3ff
    );

    printf("epc: %08x\n", ee->epc);

    printf("IOP:\n");

    struct iop_state* iop = g_lunar->ps2->iop;

    printf("r0=%08x at=%08x v0=%08x v1=%08x\n", iop->r[0] , iop->r[1] , iop->r[2] , iop->r[3] );
    printf("a0=%08x a1=%08x a2=%08x a3=%08x\n", iop->r[4] , iop->r[5] , iop->r[6] , iop->r[7] );
    printf("t0=%08x t1=%08x t2=%08x t3=%08x\n", iop->r[8] , iop->r[9] , iop->r[10], iop->r[11]);
    printf("t4=%08x t5=%08x t6=%08x t7=%08x\n", iop->r[12], iop->r[13], iop->r[14], iop->r[15]);
    printf("s0=%08x s1=%08x s2=%08x s3=%08x\n", iop->r[16], iop->r[17], iop->r[18], iop->r[19]);
    printf("s4=%08x s5=%08x s6=%08x s7=%08x\n", iop->r[20], iop->r[21], iop->r[22], iop->r[23]);
    printf("t8=%08x t9=%08x k0=%08x k1=%08x\n", iop->r[24], iop->r[25], iop->r[26], iop->r[27]);
    printf("gp=%08x sp=%08x fp=%08x ra=%08x\n", iop->r[28], iop->r[29], iop->r[30], iop->r[31]);
    printf("pc=%08x hi=%08x lo=%08x ep=%08x\n", iop->pc, iop->hi, iop->lo, iop->cop0_r[COP0_EPC]);

    for (int i = -8; i <= 8; i++) {
        iop_ds.addr = iop->pc + (i * 4);

        if (iop->pc == iop_ds.addr) {
            printf("--> ");
        } else {
            printf("    ");
        }

        puts(iop_disassemble(buf, iop_bus_read32(iop->bus.udata, iop_ds.addr & 0x1fffffff), &iop_ds));
    }

    printf("intc: stat=%08x mask=%08x ctrl=%08x\n",
        g_lunar->ps2->iop_intc->stat,
        g_lunar->ps2->iop_intc->mask,
        g_lunar->ps2->iop_intc->ctrl
    );

    exit(1);
}

void update_title(lunar::instance* lunar) {
    char buf[128];

    const char* base = lunar->loaded.size() ? basename(lunar->loaded.c_str()) : NULL;

    sprintf(buf, base ? "Iris (eegs 0.1) | %s" : "Iris (eegs 0.1)",
        base
    );

    SDL_SetWindowTitle(lunar->window, buf);
}

int init_settings(lunar::instance* lunar) {
    lunar->settings_path = SDL_GetPrefPath("Allkern", "Iris");
    lunar->settings_path.append("settings.toml");

    toml::parse_result result = toml::parse_file(lunar->settings_path);

    if (!result) {
        std::string desc(result.error().description());

        printf("iris: Couldn't parse settings file: %s\n", desc.c_str());

        return 1;
    }

    toml::table& tbl = result.table();

    auto paths = tbl["paths"];
    lunar->bios_path = paths["bios_path"].value_or("");

    auto window = tbl["window"];
    lunar->window_width = window["window_width"].value_or(960);
    lunar->window_height = window["window_height"].value_or(720);
    lunar->fullscreen = window["fullscreen"].value_or(false);
    // lunar->fullscreen_mode = tbl["fullscreen_mode"].value_or(0);

    auto display = tbl["display"];
    lunar->aspect_mode = display["aspect_mode"].value_or(0);
    lunar->bilinear = display["bilinear"].value_or(true);
    lunar->integer_scaling = display["integer_scaling"].value_or(false);
    lunar->scale = display["scale"].value_or(1.5f);

    auto debugger = tbl["debugger"];
    lunar->show_ee_control = debugger["show_ee_control"].value_or(false);
    lunar->show_ee_state = debugger["show_ee_state"].value_or(false);
    lunar->show_ee_logs = debugger["show_ee_logs"].value_or(false);
    lunar->show_ee_interrupts = debugger["show_ee_interrupts"].value_or(false);
    lunar->show_ee_dmac = debugger["show_ee_dmac"].value_or(false);
    lunar->show_iop_control = debugger["show_iop_control"].value_or(false);
    lunar->show_iop_state = debugger["show_iop_state"].value_or(false);
    lunar->show_iop_logs = debugger["show_iop_logs"].value_or(false);
    lunar->show_iop_interrupts = debugger["show_iop_interrupts"].value_or(false);
    lunar->show_iop_dma = debugger["show_iop_dma"].value_or(false);
    lunar->show_gs_debugger = debugger["show_gs_debugger"].value_or(false);
    lunar->show_memory_viewer = debugger["show_memory_viewer"].value_or(false);
    lunar->show_status_bar = debugger["show_status_bar"].value_or(true);
    lunar->show_breakpoints = debugger["show_breakpoints"].value_or(false);
    lunar->show_imgui_demo = debugger["show_imgui_demo"].value_or(false);

    software_set_aspect_mode(lunar->ctx, lunar->aspect_mode);
    software_set_bilinear(lunar->ctx, lunar->bilinear);
    software_set_integer_scaling(lunar->ctx, lunar->integer_scaling);
    software_set_scale(lunar->ctx, lunar->scale);

    return 0;
}

void audio_update(void* ud, uint8_t* buf, int size) {
    lunar::instance* lunar = (lunar::instance*)ud;

    memset(buf, 0, size);

    if (lunar->mute || lunar->pause)
        return;

    for (int i = 0; i < (size >> 2); i++) {
        struct spu2_sample sample = ps2_spu2_get_sample(lunar->ps2->spu2);

        *(int16_t*)(&buf[(i << 2) + 0]) = sample.u16[0];
        *(int16_t*)(&buf[(i << 2) + 2]) = sample.u16[1];
    }
}

void init_audio(lunar::instance* lunar) {
    SDL_AudioDeviceID dev;
    SDL_AudioSpec obtained, desired;

    desired.freq     = 48000;
    desired.format   = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples  = 512;
    desired.callback = &audio_update;
    desired.userdata = lunar;

    dev = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);

    if (dev)
        SDL_PauseAudioDevice(dev, 0);
}

void init(lunar::instance* lunar, int argc, const char* argv[]) {
    g_lunar = lunar;

    std::signal(SIGINT, sigint_handler);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    lunar->window = SDL_CreateWindow(
        "Iris (eegs 0.1)",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        960, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    init_audio(lunar);

    lunar->gl_context = SDL_GL_CreateContext(lunar->window);
    SDL_GL_MakeCurrent(lunar->window, lunar->gl_context);
    SDL_GL_SetSwapInterval(0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(lunar->window, lunar->gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Init fonts
    io.Fonts->AddFontDefault();

    ImFontConfig config;
    config.MergeMode = true;
    config.GlyphMinAdvanceX = 13.0f;
    config.GlyphOffset = ImVec2(0.0f, 4.0f);

    lunar->font_small_code = io.Fonts->AddFontFromFileTTF("res/FiraCode-Regular.ttf", 12.0f);
    lunar->font_code       = io.Fonts->AddFontFromFileTTF("res/FiraCode-Regular.ttf", 16.0f);
    lunar->font_small      = io.Fonts->AddFontFromFileTTF("res/Roboto-Regular.ttf", 12.0f);
    lunar->font_heading    = io.Fonts->AddFontFromFileTTF("res/Roboto-Regular.ttf", 20.0f);
    lunar->font_body       = io.Fonts->AddFontFromFileTTF("res/Roboto-Regular.ttf", 16.0f);
    lunar->font_icons      = io.Fonts->AddFontFromFileTTF("res/" FONT_ICON_FILE_NAME_MSR, 20.0f, &config, icon_range);

    io.FontDefault = lunar->font_body;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding           = ImVec2(8.0, 8.0);
    style.FramePadding            = ImVec2(5.0, 5.0);
    style.ItemSpacing             = ImVec2(8.0, 4.0);
    style.WindowBorderSize        = 0;
    style.ChildBorderSize         = 0;
    style.FrameBorderSize         = 0;
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

    // Init theme
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.29f, 0.35f, 0.39f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.07f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_Border]                 = ImVec4(0.10f, 0.12f, 0.13f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.10f, 0.12f, 0.13f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.24f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.29f, 0.35f, 0.39f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.20f, 0.24f, 0.26f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.39f, 0.47f, 0.52f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.49f, 0.59f, 0.65f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.13f, 0.16f, 0.17f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.20f, 0.24f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.29f, 0.35f, 0.39f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.13f, 0.16f, 0.17f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.20f, 0.24f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.29f, 0.35f, 0.39f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.23f, 0.28f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.33f, 0.39f, 0.43f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.38f, 0.46f, 0.51f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.00f, 0.30f, 0.25f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.00f, 0.39f, 0.32f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.20f, 0.24f, 0.26f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.23f, 0.28f, 0.30f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.26f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.10f, 0.12f, 0.13f, 1.00f);
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
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.15f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    lunar->open = true;
    lunar->ps2 = ps2_create();

    ps2_init(lunar->ps2);
    ps2_init_kputchar(lunar->ps2, ee_kputchar, lunar, iop_kputchar, lunar);
    ps2_gs_init_callback(lunar->ps2->gs, GS_EVENT_VBLANK, (void (*)(void*))update_window, lunar);
    ps2_gs_init_callback(lunar->ps2->gs, GS_EVENT_SCISSOR, handle_scissor_event, lunar);

    lunar->ds = ds_sio2_attach(lunar->ps2->sio2, 0);

    // To-do: Implement backend constructor and destructor
    lunar->ctx = new software_state;
    lunar->ps2->gs->backend.udata = lunar->ctx;
    lunar->ps2->gs->backend.render_point = software_render_point;
    lunar->ps2->gs->backend.render_line = software_render_line;
    lunar->ps2->gs->backend.render_triangle = software_render_triangle;
    lunar->ps2->gs->backend.render_sprite = software_render_sprite;
    lunar->ps2->gs->backend.render = software_render;
    lunar->ps2->gs->backend.transfer_start = software_transfer_start;
    lunar->ps2->gs->backend.transfer_write = software_transfer_write;
    lunar->ps2->gs->backend.transfer_read = software_transfer_read;

    software_init(lunar->ctx, lunar->ps2->gs, lunar->window);

    lunar->ticks = SDL_GetTicks();
    lunar->pause = true;

    init_settings(lunar);

    std::string bios_path;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];

        if (a == "-x") {
            lunar->elf_path = argv[i+1];

            ++i;
        } else if (a == "-d") {
            lunar->boot_path = argv[i+1];

            ++i;
        } else if (a == "-b") {
            bios_path = argv[i+1];

            ++i;
        } else if (a == "-i") {
            lunar->disc_path = argv[i+1];

            ++i;
        } else {
            lunar->disc_path = argv[i];
        }
    }

    if (bios_path.size()) {
        ps2_load_bios(lunar->ps2, bios_path.c_str());
    } else {
        if (lunar->bios_path.size()) {
            ps2_load_bios(lunar->ps2, lunar->bios_path.c_str());
        } else {
            lunar->show_bios_setting_window = true;
        }
    }

    if (lunar->elf_path.size()) {
        ps2_elf_load(lunar->ps2, lunar->elf_path.c_str());

        lunar->loaded = lunar->elf_path;
    }

    if (lunar->boot_path.size()) {
        ps2_boot_file(lunar->ps2, lunar->boot_path.c_str());

        lunar->loaded = lunar->boot_path;
    }

    if (lunar->disc_path.size()) {
        struct iso9660_state* iso = iso9660_open(lunar->disc_path.c_str());

        if (!iso) {
            printf("lunar: Couldn't open disc image \"%s\"\n", lunar->disc_path.c_str());

            exit(1);

            return;
        }

        char* boot_file = iso9660_get_boot_path(iso);

        if (!boot_file)
            return;

        ps2_boot_file(lunar->ps2, boot_file);
        ps2_cdvd_open(lunar->ps2->cdvd, lunar->disc_path.c_str());

        iso9660_close(iso);

        lunar->loaded = lunar->disc_path;
    }
}

void destroy(lunar::instance* lunar);

void update(lunar::instance* lunar) {
    if (!lunar->pause) {
        ps2_cycle(lunar->ps2);

        for (const breakpoint& b : lunar->breakpoints) {
            if (lunar->ps2->ee->pc == b.addr)
                lunar->pause = true;
        }
    } else {
        if (lunar->step) {
            ps2_cycle(lunar->ps2);

            lunar->step = false;
        }

        update_window(lunar);
    }
}

void update_time(lunar::instance* lunar) {
    int t = SDL_GetTicks() - lunar->ticks;

    if (t < 500)
        return;

    if (lunar->fps == 0.0f) {
        lunar->fps = (float)lunar->frames;
    } else {
        lunar->fps += (float)lunar->frames;
        lunar->fps /= 2.0f;
    }

    lunar->ticks = SDL_GetTicks();
    lunar->frames = 0;
}

void show_bios_setting_window(lunar::instance* lunar) {
    using namespace ImGui;

    OpenPopup("Welcome");

    // Always center this window when appearing
    ImVec2 center = GetMainViewport()->GetCenter();

    SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    static char buf[512];

    if (BeginPopupModal("Welcome", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        PushFont(lunar->font_heading);
        Text("Welcome to Iris!");
        PopFont();
        Separator();
        Text(
            "Iris requires a PlayStation 2 BIOS file in order to run.\n\n"
            "Please select one using the box below or provide one using the\n"
            "command line arguments."
        );

        InputTextWithHint("##BIOS", "e.g. scph10000.bin", buf, 512, ImGuiInputTextFlags_EscapeClearsAll);
        SameLine();

        if (Button(ICON_MS_FOLDER)) {
            const char* patterns[1] = { "*.bin" };

            const char* file = tinyfd_openFileDialog(
                "Select BIOS file",
                "",
                1,
                patterns,
                "BIOS files",
                0
            );

            if (file) {
                strncpy(buf, file, 512);
            }
        }

        Separator();

        BeginDisabled(!buf[0]);

        if (Button("Done")) {
            lunar->bios_path = buf;
            lunar->dump_to_file = true;

            ps2_load_bios(lunar->ps2, lunar->bios_path.c_str());

            CloseCurrentPopup();

            lunar->show_bios_setting_window = false;
        }

        EndDisabled();

        EndPopup();
    }
}

void update_window(lunar::instance* lunar) {
    using namespace ImGui;

    update_title(lunar);
    update_time(lunar);

    ImGuiIO& io = ImGui::GetIO();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    NewFrame();

    show_main_menubar(lunar);

    DockSpaceOverViewport(0, GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    if (lunar->show_ee_control) show_ee_control(lunar);
    if (lunar->show_ee_state) show_ee_state(lunar);
    if (lunar->show_ee_logs) show_ee_logs(lunar);
    if (lunar->show_ee_interrupts) show_ee_interrupts(lunar);
    if (lunar->show_ee_dmac) show_ee_dmac(lunar);
    if (lunar->show_iop_control) show_iop_control(lunar);
    if (lunar->show_iop_state) show_iop_state(lunar);
    if (lunar->show_iop_logs) show_iop_logs(lunar);
    if (lunar->show_iop_interrupts) show_iop_interrupts(lunar);
    if (lunar->show_iop_dma) show_iop_dma(lunar);
    if (lunar->show_gs_debugger) show_gs_debugger(lunar);
    if (lunar->show_memory_viewer) show_memory_viewer(lunar);
    if (lunar->show_status_bar) show_status_bar(lunar);
    if (lunar->show_breakpoints) show_breakpoints(lunar);
    if (lunar->show_about_window) show_about_window(lunar);
    if (lunar->show_imgui_demo) ShowDemoWindow(&lunar->show_imgui_demo);
    if (lunar->show_bios_setting_window) show_bios_setting_window(lunar);

    if (lunar->pause) {
        int width, height;

        SDL_GetWindowSize(lunar->window, &width, &height);

        auto ts = CalcTextSize(ICON_MS_PAUSE);

        GetBackgroundDrawList()->AddText(
            ImVec2(
                width - ts.x * 1.5f,
                lunar->menubar_height + ts.x * 0.5f
            ),
            0xffffffff,
            ICON_MS_PAUSE
        );
    }

    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(0.11, 0.11, 0.11, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render display texture
    software_render(lunar->ps2->gs, lunar->ps2->gs->backend.udata);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }

    SDL_GL_SwapWindow(lunar->window);

    lunar->frames++;

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
            case SDL_QUIT: {
                lunar->open = false;
            } break;

            case SDL_KEYDOWN: {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    lunar->pause = !lunar->pause;
                }

                if (event.key.keysym.sym == SDLK_0) {
                    ps2_iop_intc_irq(lunar->ps2->iop_intc, IOP_INTC_SPU2);

                    break;
                }
                uint16_t mask = map_button(event.key.keysym.sym);

                ds_button_press(lunar->ds, mask);
            } break;

            case SDL_KEYUP: {
                uint16_t mask = map_button(event.key.keysym.sym);

                ds_button_release(lunar->ds, mask);
            } break;
        }
    }
}

bool is_open(lunar::instance* lunar) {
    return lunar->open;
}

void close(lunar::instance* lunar) {
    using namespace ImGui;

    if (lunar->dump_to_file) {
        std::ofstream file(lunar->settings_path);

        file << "# File auto-generated by Iris 0.1\n\n";

        auto tbl = toml::table {
            { "debugger", toml::table {
                { "show_ee_control", lunar->show_ee_control },
                { "show_ee_state", lunar->show_ee_state },
                { "show_ee_logs", lunar->show_ee_logs },
                { "show_ee_interrupts", lunar->show_ee_interrupts },
                { "show_ee_dmac", lunar->show_ee_dmac },
                { "show_iop_control", lunar->show_iop_control },
                { "show_iop_state", lunar->show_iop_state },
                { "show_iop_logs", lunar->show_iop_logs },
                { "show_iop_interrupts", lunar->show_iop_interrupts },
                { "show_iop_dma", lunar->show_iop_dma },
                { "show_gs_debugger", lunar->show_gs_debugger },
                { "show_memory_viewer", lunar->show_memory_viewer },
                { "show_status_bar", lunar->show_status_bar },
                { "show_breakpoints", lunar->show_breakpoints },
                { "show_imgui_demo", lunar->show_imgui_demo }
            } },
            { "display", toml::table {
                { "scale", lunar->ctx->scale },
                { "aspect_mode", lunar->ctx->aspect_mode },
                { "integer_scaling", lunar->ctx->integer_scaling },
                { "bilinear", lunar->ctx->bilinear }
            } },
            { "paths", toml::table {
                { "bios_path", lunar->bios_path }
            } }
        };

        file << tbl;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    DestroyContext();

    SDL_GL_DeleteContext(lunar->gl_context);
    SDL_DestroyWindow(lunar->window);
    SDL_Quit();

    ps2_destroy(lunar->ps2);
}

}