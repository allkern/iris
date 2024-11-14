#include <string>
#include <vector>

#define IM_RGB(r, g, b) ImVec4(((float)r / 255.0f), ((float)g / 255.0f), ((float)b / 255.0f), 1.0)

#include "instance.hpp"

#include "res/IconsMaterialSymbols.h"

#include "ee/ee_dis.h"
#include "iop/iop_dis.h"

#include "ps2_elf.h"

#include "renderer/opengl.hpp"

namespace lunar {

void kputchar_stub(void* udata, char c) {
    putchar(c);
}

struct ee_dis_state g_ee_dis_state;
struct iop_dis_state g_iop_dis_state;

static const ImWchar icon_range[] = { ICON_MIN_MS, ICON_MAX_16_MS, 0 };

void handle_scissor_event(void* udata) {
    lunar::instance* lunar = (lunar::instance*)udata;

    int scax0 = lunar->ps2->gs->scissor_1 & 0x3ff;
    int scay0 = (lunar->ps2->gs->scissor_1 >> 32) & 0x3ff;
    int scax1 = (lunar->ps2->gs->scissor_1 >> 16) & 0x3ff;
    int scay1 = (lunar->ps2->gs->scissor_1 >> 48) & 0x3ff;

    // printf("sca0=(%d,%d) sca1=(%d,%d) frame_1=%x\n",
    //     scax0, scay0,
    //     scax1, scay1,
    //     lunar->ps2->gs->frame_1
    // );
    
    int width = scax1 - scax0;
    int height = scay1 - scay0;

    opengl_set_size(lunar->renderer_state, width, height, 1.5);

    // SDL_SetWindowSize(lunar->window, width, height);
}

lunar::instance* create();

void init(lunar::instance* lunar, int argc, const char* argv[]) {
    lunar->window = SDL_CreateWindow(
        "eegs 0.1",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        960, 720,
        SDL_WINDOW_OPENGL
    );

    lunar->renderer = SDL_CreateRenderer(
        lunar->window,
        -1,
        SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED
    );

    lunar->texture = SDL_CreateTexture(
        lunar->renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        640, 480
    );

    SDL_SetTextureScaleMode(lunar->texture, SDL_ScaleModeLinear);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(lunar->window, lunar->renderer);
    ImGui_ImplSDLRenderer2_Init(lunar->renderer);

    // Init fonts
    io.Fonts->AddFontDefault();

    ImFontConfig config;
    config.MergeMode = true;
    config.GlyphMinAdvanceX = 13.0f;
    config.GlyphOffset = ImVec2(0.0f, 4.0f);

    lunar->font_small_code = io.Fonts->AddFontFromFileTTF("FiraCode-Regular.ttf", 12.0f);
    lunar->font_code    = io.Fonts->AddFontFromFileTTF("FiraCode-Regular.ttf", 16.0f);
    lunar->font_small   = io.Fonts->AddFontFromFileTTF("Roboto-Regular.ttf", 12.0f);
    lunar->font_heading = io.Fonts->AddFontFromFileTTF("Roboto-Regular.ttf", 18.0f);
    lunar->font_body    = io.Fonts->AddFontFromFileTTF("Roboto-Regular.ttf", 14.0f);
    lunar->font_icons   = io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_MSR, 20.0f, &config, icon_range);

    io.FontDefault = lunar->font_body;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding         = ImVec2(8.0, 8.0);
    style.FramePadding          = ImVec2(5.0, 5.0);
    style.ItemSpacing           = ImVec2(8.0, 4.0);
    style.WindowBorderSize      = 0;
    style.ChildBorderSize       = 0;
    style.FrameBorderSize       = 0;
    style.PopupBorderSize       = 0;
    style.TabBorderSize         = 0;
    style.TabBarBorderSize      = 0;
    style.WindowRounding        = 4;
    style.ChildRounding         = 4;
    style.FrameRounding         = 4;
    style.PopupRounding         = 4;
    style.ScrollbarRounding     = 9;
    style.GrabRounding          = 2;
    style.TabRounding           = 4;
    style.WindowTitleAlign      = ImVec2(0.5, 0.5);
    style.DockingSeparatorSize  = 0;

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
    lunar->texture_buf = (uint32_t*)malloc((640 * 480) * sizeof(uint32_t));

    lunar->ps2 = ps2_create();

    ps2_init(lunar->ps2);
    ps2_load_bios(lunar->ps2, argv[1]);
    ps2_init_kputchar(lunar->ps2, kputchar_stub, NULL, kputchar_stub, NULL);
    ps2_gs_init_callback(lunar->ps2->gs, GS_EVENT_VBLANK, (void (*)(void*))update_window, lunar);
    ps2_gs_init_callback(lunar->ps2->gs, GS_EVENT_SCISSOR, handle_scissor_event, lunar);

    // Initialize hardware renderer
    lunar->renderer_state = new opengl_state;

    lunar->ps2->gs->backend.render_point = opengl_render_point;
    lunar->ps2->gs->backend.render_line = opengl_render_line;
    lunar->ps2->gs->backend.render_line_strip = opengl_render_line_strip;
    lunar->ps2->gs->backend.render_triangle = opengl_render_triangle;
    lunar->ps2->gs->backend.render_triangle_strip = opengl_render_triangle_strip;
    lunar->ps2->gs->backend.render_triangle_fan = opengl_render_triangle_fan;
    lunar->ps2->gs->backend.render_sprite = opengl_render_sprite;
    lunar->ps2->gs->backend.render = opengl_render;
    lunar->ps2->gs->backend.udata = lunar->renderer_state;

    opengl_init(lunar->renderer_state);

    lunar->ps2->gs->vqi = 0;
    lunar->ps2->gs->prim = 5;

    int cx = 640 / 2;
    int cy = 480 / 2;
    int r = 200;
    float p = 0.25f * M_PI;

    lunar->ps2->gs->rgbaq = 0xff0000; gs_write_vertex(lunar->ps2->gs, (320 << 4) | (240 << 20));

    int m = 8;

    for (int i = 0; i < m; i++) {
        uint64_t px = (r * sin(p + (float)i * (0.1f * M_PI))) + cx;
        uint64_t py = (r * cos(p + (float)i * (0.1f * M_PI))) + cy;

        uint32_t r = i * (255 / m);
        uint32_t g = 0;
        uint32_t b = 0xff - (i * (255 / m));

        lunar->ps2->gs->rgbaq = (r & 0xff) | (g << 8) | (b << 16);

        gs_write_vertex(lunar->ps2->gs, (px << 4) | (py << 20));
    }

    lunar->ps2->gs->vqi = 0;
    lunar->ps2->gs->prim = 4;

    int sx = 50;
    int sy = 240;

    int dx = 25;
    int dy = 50;

    int q = 9;

    for (int i = 0; i < q; i++) {
        uint32_t r = i * (255 / m);
        uint32_t g = 0;
        uint32_t b = 0xff - (i * (255 / m));

        lunar->ps2->gs->rgbaq = (r & 0xff) | (g << 8) | (b << 16);

        gs_write_vertex(lunar->ps2->gs, (sx << 4) | (sy << 20));

        sx += dx;
        sy += (i & 1) ? -dy : dy;
    }
    
    lunar->pause = 0;

    if (argv[2]) {
        lunar->elf_path = argv[2];

        ps2_elf_load(lunar->ps2, lunar->elf_path);
    }
}

void print_highlighted(const char* buf) {
    using namespace ImGui;

    std::vector <std::string> tokens;

    std::string text;

    while (*buf) {
        text.clear();        

        if (isalpha(*buf)) {
            while (isalpha(*buf) || isdigit(*buf))
                text.push_back(*buf++);
        } else if (isxdigit(*buf) || (*buf == '-')) {
            while (isxdigit(*buf) || (*buf == 'x') || (*buf == '-'))
                text.push_back(*buf++);
        } else if (*buf == '$') {
            while (*buf == '$' || isdigit(*buf) || isalpha(*buf) || *buf == '_')
                text.push_back(*buf++);
        } else if (*buf == ',') {
            while (*buf == ',')
                text.push_back(*buf++);
        } else if (*buf == '(') {
            while (*buf == '(')
                text.push_back(*buf++);
        } else if (*buf == ')') {
            while (*buf == ')')
                text.push_back(*buf++);
        } else if (*buf == '<') {
            while (*buf != '>')
                text.push_back(*buf++);

            text.push_back(*buf++);
        } else if (*buf == '_') {
            text.push_back(*buf++);
        } else if (*buf == '.') {
            text.push_back(*buf++);
        } else {
            printf("unhandled char %c (%d) \"%s\"\n", *buf, *buf, buf);

            exit(1);
        }

        while (isspace(*buf))
            text.push_back(*buf++);

        tokens.push_back(text);
    }

    for (const std::string& t : tokens) {
        if (isalpha(t[0])) {
            TextColored(IM_RGB(211, 167, 30), t.c_str());
        } else if (isdigit(t[0]) || t[0] == '-') {
            TextColored(IM_RGB(138, 143, 226), t.c_str());
        } else if (t[0] == '$') {
            TextColored(IM_RGB(68, 169, 240), t.c_str());
        } else if (t[0] == '<') {
            TextColored(IM_RGB(89, 89, 89), t.c_str());
        } else {
            Text(t.c_str());
        }

        SameLine(0.0f, 0.0f);
    }

    NewLine();
}

static void show_ee_disassembly_view(lunar::instance* lunar) {
    using namespace ImGui;

    PushFont(lunar->font_code);

    if (BeginTable("table1", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        TableSetupColumn("a", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("b", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("c", ImGuiTableColumnFlags_NoResize);

        for (int row = -64; row < 64; row++) {
            g_ee_dis_state.pc = lunar->ps2->ee->pc + (row * 4);

            TableNextRow();
            TableSetColumnIndex(0);

            PushFont(lunar->font_icons);

            TableSetColumnIndex(1);

            if (g_ee_dis_state.pc == lunar->ps2->ee->pc)
                Text(ICON_MS_CHEVRON_RIGHT " ");

            PopFont();

            TableSetColumnIndex(2);

            uint32_t opcode = ee_bus_read32(lunar->ps2->ee_bus, g_ee_dis_state.pc & 0x1fffffff);

            char buf[128];

            char addr_str[9]; sprintf(addr_str, "%08x", g_ee_dis_state.pc);
            char opcode_str[9]; sprintf(opcode_str, "%08x", opcode);
            char* disassembly = ee_disassemble(buf, opcode, &g_ee_dis_state);

            Text("%s ", addr_str); SameLine();
            TextDisabled("%s ", opcode_str); SameLine();

            char id[16];

            sprintf(id, "##%d", row);

            if (Selectable(id, false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns)) {
            } SameLine();

            if (BeginPopupContextItem()) {
                PushFont(lunar->font_small_code);
                TextDisabled("0x%08x", g_ee_dis_state.pc);
                PopFont();

                PushFont(lunar->font_body);

                if (BeginMenu(ICON_MS_CONTENT_COPY "  Copy")) {
                    if (Selectable(ICON_MS_SORT "  Address")) {
                        SDL_SetClipboardText(addr_str);
                    }

                    if (Selectable(ICON_MS_SORT "  Opcode")) {
                        SDL_SetClipboardText(opcode_str);
                    }

                    if (Selectable(ICON_MS_SORT "  Disassembly")) {
                        SDL_SetClipboardText(disassembly);
                    }

                    ImGui::EndMenu();
                }

                PopFont();
                EndPopup();
            }

            if (true) {
                print_highlighted(disassembly);
            } else {
                Text(disassembly);
            }

            if (g_ee_dis_state.pc == lunar->ps2->ee->pc)
                SetScrollHereY(0.5f);
        }

        EndTable();
    }

    PopFont();
}

static void show_iop_disassembly_view(lunar::instance* lunar) {
    using namespace ImGui;

    PushFont(lunar->font_code);

    if (BeginTable("table2", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        TableSetupColumn("a", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("b", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("c", ImGuiTableColumnFlags_NoResize);

        for (int row = -64; row < 64; row++) {
            g_iop_dis_state.addr = lunar->ps2->iop->pc + (row * 4);

            TableNextRow();
            TableSetColumnIndex(0);

            PushFont(lunar->font_icons);

            TableSetColumnIndex(1);

            if (g_iop_dis_state.addr == lunar->ps2->iop->pc)
                Text(ICON_MS_CHEVRON_RIGHT " ");

            PopFont();

            TableSetColumnIndex(2);

            uint32_t opcode = iop_bus_read32(lunar->ps2->iop_bus, g_iop_dis_state.addr & 0x1fffffff);

            char buf[128];

            char addr_str[9]; sprintf(addr_str, "%08x", g_iop_dis_state.addr);
            char opcode_str[9]; sprintf(opcode_str, "%08x", opcode);
            char* disassembly = iop_disassemble(buf, opcode, &g_iop_dis_state);

            Text("%s ", addr_str); SameLine();
            TextDisabled("%s ", opcode_str); SameLine();

            char id[16];

            sprintf(id, "##%d", row);

            if (Selectable(id, false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns)) {
            } SameLine();

            if (BeginPopupContextItem()) {
                PushFont(lunar->font_small_code);
                TextDisabled("0x%08x", g_iop_dis_state.addr);
                PopFont();

                PushFont(lunar->font_body);

                if (BeginMenu(ICON_MS_CONTENT_COPY "  Copy")) {
                    if (Selectable(ICON_MS_SORT "  Address")) {
                        SDL_SetClipboardText(addr_str);
                    }

                    if (Selectable(ICON_MS_SORT "  Opcode")) {
                        SDL_SetClipboardText(opcode_str);
                    }

                    if (Selectable(ICON_MS_SORT "  Disassembly")) {
                        SDL_SetClipboardText(disassembly);
                    }

                    ImGui::EndMenu();
                }

                PopFont();
                EndPopup();
            }

            if (true) {
                print_highlighted(disassembly);
            } else {
                Text(disassembly);
            }

            if (g_iop_dis_state.addr == lunar->ps2->iop->pc)
                SetScrollHereY(0.5f);
        }

        EndTable();
    }

    PopFont();
}

void destroy(lunar::instance* lunar);

void update(lunar::instance* lunar) {
    if (!lunar->pause) {
        ps2_cycle(lunar->ps2);
    } else {
        update_window(lunar);
    }
}

void update_window(lunar::instance* lunar) {
    using namespace ImGui;

    // ImGuiIO& io = ImGui::GetIO();

    // ImGui_ImplSDLRenderer2_NewFrame();
    // ImGui_ImplSDL2_NewFrame();
    // NewFrame();

    // DockSpaceOverViewport(0, GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // PushFont(lunar->font_body);

    // if (Begin("Logs")) {
    //     if (BeginTabBar("logs_tabs")) {
    //         if (BeginTabItem("EE")) {
    //             Text("EE logs");

    //             EndTabItem();
    //         }

    //         if (BeginTabItem("IOP")) {
    //             Text("IOP logs");

    //             EndTabItem();
    //         }

    //         EndTabBar();
    //     }
    // } End();

    // if (Begin("EE control")) {
    //     if (Button(ICON_MS_STEP)) {
    //         if (!lunar->pause) {
    //             lunar->pause = true;
    //         } else {
    //             tick_ee(lunar);
    //         }
    //     } SameLine();

    //     if (Button(lunar->pause ? ICON_MS_PLAY_ARROW : ICON_MS_PAUSE)) {
    //         lunar->pause = !lunar->pause;
    //     }
    // } End();

    // if (Begin("IOP control")) {
    //     if (Button(ICON_MS_STEP)) {
    //         if (!lunar->pause) {
    //             lunar->pause = true;
    //         } else {
    //             tick_iop(lunar);
    //         }
    //     }
    // } End();

    // if (Begin("EE disassembly")) {
    //     show_ee_disassembly_view(lunar);
    // } End();

    // if (Begin("IOP disassembly")) {
    //     show_iop_disassembly_view(lunar);
    // } End();

    // PopFont();

    // Render();

    // // SDL_GL_SwapWindow(lunar->window);

    // SDL_RenderSetScale(lunar->renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

    // SDL_SetRenderDrawColor(lunar->renderer, 0, 0, 0, 0xff);
    // SDL_RenderClear(lunar->renderer);

    // int width = ((lunar->ps2->gs->display2 >> 32) & 0xfff) + 1;
    // width /= ((lunar->ps2->gs->display2 >> 23) & 0xf) + 1;
    // int height = ((lunar->ps2->gs->display2 >> 44) & 0x7ff) + 1;
    // // int stride = ((lunar->ps2->gs->frame_1 >> 16) & 0x3f) * 64;
    // uint64_t base = ((lunar->ps2->gs->frame_1 & 0x1f) * 2048);

    // SDL_DestroyTexture(lunar->texture);
    // lunar->texture = SDL_CreateTexture(
    //     lunar->renderer,
    //     SDL_PIXELFORMAT_ABGR8888,
    //     SDL_TEXTUREACCESS_STREAMING,
    //     width, height
    // );
    // SDL_SetTextureScaleMode(lunar->texture, SDL_ScaleModeLinear);
    // SDL_UpdateTexture(lunar->texture, NULL, lunar->ps2->gs->vram + base, width * 4);
    // SDL_RenderCopy(lunar->renderer, lunar->texture, NULL, NULL);

    // ImGui_ImplSDLRenderer2_RenderDrawData(GetDrawData(), lunar->renderer);

    // SDL_RenderPresent(lunar->renderer);

    SDL_GL_SwapWindow(lunar->window);

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
            case SDL_QUIT: {
                lunar->open = false;
            } break;
        }
    }
}

bool is_open(lunar::instance* lunar) {
    return lunar->open;
}

void close(lunar::instance* lunar) {
    using namespace ImGui;

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    DestroyContext();

    SDL_DestroyRenderer(lunar->renderer);
    SDL_DestroyWindow(lunar->window);
    SDL_Quit();
}

}