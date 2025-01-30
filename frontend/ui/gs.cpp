#include <vector>
#include <string>
#include <cctype>

#include "instance.hpp"

#include "res/IconsMaterialSymbols.h"

namespace lunar {

static const char* gs_internal_reg_names[] = {
    "prim",
    "rgbaq",
    "st",
    "uv",
    "xyzf2",
    "xyz2",
    "fog",
    "xyzf3",
    "xyz3",
    "prmodecont",
    "prmode",
    "texclut",
    "scanmsk",
    "texa",
    "fogcol",
    "texflush",
    "dimx",
    "dthe",
    "colclamp",
    "pabe",
    "bitbltbuf",
    "trxpos",
    "trxreg",
    "trxdir",
    "hwreg",
    "signal",
    "finish",
    "label"
};

static const char* gs_gp_reg_names[] = {
    "frame",
    "zbuf",
    "tex0",
    "tex1",
    "tex2",
    "miptbp1",
    "miptbp2",
    "clamp",
    "test",
    "alpha",
    "xyoffset",
    "scissor",
    "fba"
};

static const char* gs_privileged_reg_names[] = {
    "pmode",
    "smode1",
    "smode2",
    "srfsh",
    "synch1",
    "synch2",
    "syncv",
    "dispfb1",
    "display1",
    "dispfb2",
    "display2",
    "extbuf",
    "extdata",
    "extwrite",
    "bgcolor",
    "csr",
    "imr",
    "busdir",
    "siglblid"
};

static const char* gs_debug_names[] = {
    "Registers",
    "Buffers",
    "Textures",
    "Queue",
    "Memory"
};

static const char* sizing_combo_items[] = {
    ICON_MS_FIT_WIDTH " Fixed fit",
    ICON_MS_FULLSCREEN " Fixed same",
    ICON_MS_FULLSCREEN " Stretch prop",
    ICON_MS_FULLSCREEN " Stretch same"
};

static ImGuiTableFlags table_sizing_flags[] = {
    ImGuiTableFlags_SizingFixedFit,
    ImGuiTableFlags_SizingFixedSame,
    ImGuiTableFlags_SizingStretchProp,
    ImGuiTableFlags_SizingStretchSame
};

static int gs_table_sizing = 3;
static int gs_sizing_index = 0;
static int gs_debug_index = 0;

static const char* gs_context_names[] = {
    "Privileged",
    "Context 1",
    "Context 2",
    "Internal"
};

static int gs_context = 0;

void show_privileged_registers(lunar::instance* lunar) {
    using namespace ImGui;

    struct ps2_gs* gs = lunar->ps2->gs;

    PushFont(lunar->font_code);

    uint64_t* regs = &gs->pmode;

    if (BeginTable("table6", 2, ImGuiTableFlags_RowBg)) {
        PushFont(lunar->font_small_code);
        TableSetupColumn("Register");
        TableSetupColumn("Value");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 19; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text(gs_privileged_reg_names[i]);

            TableSetColumnIndex(1);

            Text("%08x%08x", regs[i] >> 32, regs[i] & 0xffffffff);
        }

        EndTable();
    }

    PopFont();
}

void show_context_registers(lunar::instance* lunar, int ctx) {
    using namespace ImGui;

    struct ps2_gs* gs = lunar->ps2->gs;

    PushFont(lunar->font_code);

    uint64_t* regs = &gs->context[ctx].frame;

    if (BeginTable("table9", 2, ImGuiTableFlags_RowBg)) {
        PushFont(lunar->font_small_code);
        TableSetupColumn("Register");
        TableSetupColumn("Value");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 13; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text(gs_gp_reg_names[i]);

            TableSetColumnIndex(1);

            Text("%08x%08x", regs[i] >> 32, regs[i] & 0xffffffff);
        }

        EndTable();
    }

    PopFont();
}

void show_internal_registers(lunar::instance* lunar) {
    using namespace ImGui;

    struct ps2_gs* gs = lunar->ps2->gs;

    PushFont(lunar->font_code);

    uint64_t* regs = &gs->prim;

    if (BeginTable("table6", 2, ImGuiTableFlags_RowBg)) {
        PushFont(lunar->font_small_code);
        TableSetupColumn("Register");
        TableSetupColumn("Value");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 28; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text(gs_internal_reg_names[i]);

            TableSetColumnIndex(1);

            Text("%08x%08x", regs[i] >> 32, regs[i] & 0xffffffff);
        }

        EndTable();
    }

    PopFont();
}

static inline void show_gs_vertex_xy(lunar::instance* lunar, const gs_vertex* vtx) {
    using namespace ImGui;

    TableSetColumnIndex(0);

    Text("Position (XY)");

    TableSetColumnIndex(1);

    PushFont(lunar->font_code);

    Text("0x%04x, 0x%04x", vtx->xyz & 0xffff, (vtx->xyz >> 16) & 0xffff);

    TableSetColumnIndex(2);

    Text("%d.%04d, %d.%04d", vtx->x, (vtx->xyz & 0xf) * 625, vtx->y, ((vtx->xyz >> 16) & 0xf) * 625);

    PopFont();

    TableNextRow();
}

static inline void show_gs_vertex_z(lunar::instance* lunar, const gs_vertex* vtx) {
    using namespace ImGui;

    TableSetColumnIndex(0);

    Text("Depth (Z)");

    TableSetColumnIndex(1);

    PushFont(lunar->font_code);

    Text("0x%08x", vtx->z);

    TableSetColumnIndex(2);

    Text("%d", vtx->z);

    PopFont();

    TableNextRow();
}

static inline void show_gs_vertex_stq(lunar::instance* lunar, const gs_vertex* vtx) {
    using namespace ImGui;

    TableSetColumnIndex(0);

    Text("STQ");

    TableSetColumnIndex(1);

    PushFont(lunar->font_code);

    Text("0x%08x, 0x%08x, 0x%08x", vtx->st & 0xffffffff, vtx->st >> 32, vtx->rgbaq >> 32);

    TableSetColumnIndex(2);

    Text("%f, %f, %f", vtx->s, vtx->t, vtx->q);

    PopFont();

    TableNextRow();
}

static inline void show_gs_vertex_uv(lunar::instance* lunar, const gs_vertex* vtx) {
    using namespace ImGui;

    TableSetColumnIndex(0);

    Text("UV");

    TableSetColumnIndex(1);

    PushFont(lunar->font_code);

    Text("0x%08x, 0x%08x",
        vtx->uv & 0x3fff,
        (vtx->uv >> 16) & 0x3fff
    );

    TableSetColumnIndex(2);

    Text("%d.%04d, %d.%04d",
        vtx->u, (vtx->uv & 0xf) * 625,
        vtx->v, ((vtx->uv >> 16) & 0xf) * 625
    );

    PopFont();

    TableNextRow();
}

static inline void show_gs_vertex_rgba(lunar::instance* lunar, const gs_vertex* vtx) {
    using namespace ImGui;

    TableSetColumnIndex(0);

    Text("Color (RGBA)");

    TableSetColumnIndex(1);

    PushFont(lunar->font_code);

    Text("0x%08x", vtx->rgbaq & 0xffffffff);

    TableSetColumnIndex(2);

    Text("0x%02x, 0x%02x, 0x%02x, 0x%02x",
        vtx->rgbaq & 0xff,
        (vtx->rgbaq >> 8) & 0xff,
        (vtx->rgbaq >> 16) & 0xff,
        (vtx->rgbaq >> 24) & 0xff
    ); SameLine();

    ImVec4 color = ImVec4(
        (float)(vtx->rgbaq & 0xff) / 255.0f,
        (float)((vtx->rgbaq >> 8) & 0xff) / 255.0f,
        (float)((vtx->rgbaq >> 16) & 0xff) / 255.0f,
        (float)((vtx->rgbaq >> 24) & 0xff) / 255.0f
    );

    ColorEdit4("MyColor##3", (float*)&color,
        ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoLabel |
        ImGuiColorEditFlags_NoPicker |
        ImGuiColorEditFlags_AlphaPreviewHalf
    );

    PopFont();

    TableNextRow();
}

void show_gs_vertex(lunar::instance* lunar, const gs_vertex* vtx) {
    using namespace ImGui;

    struct ps2_gs* gs = lunar->ps2->gs;

    if (BeginTable("table10", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        PushFont(lunar->font_small_code);
        TableSetupColumn("Attribute");
        TableSetupColumn("Raw");
        TableSetupColumn("Value");
        TableHeadersRow();
        PopFont();

        TableNextRow();

        show_gs_vertex_xy(lunar, vtx);
        show_gs_vertex_z(lunar, vtx);
        show_gs_vertex_stq(lunar, vtx);
        show_gs_vertex_uv(lunar, vtx);
        show_gs_vertex_rgba(lunar, vtx);

        EndTable();
    }
}

void show_gs_queue(lunar::instance* lunar) {
    using namespace ImGui;

    struct ps2_gs* gs = lunar->ps2->gs;

    for (int i = 0; i < gs->vqi; i++) {
        char buf[16]; sprintf(buf, "Vertex %d", i+1);

        if (TreeNode(buf)) {
            show_gs_vertex(lunar, &gs->vq[i]);

            TreePop();
        }
    }

    if (TreeNode("Uninitialized")) {
        for (int i = gs->vqi; i < 4; i++) {
            char buf[16]; sprintf(buf, "Vertex %d", i+1);

            if (TreeNode(buf)) {
                show_gs_vertex(lunar, &gs->vq[i]);

                TreePop();
            }
        }

        TreePop();
    }
}

void show_gs_registers(lunar::instance* lunar) {
    using namespace ImGui;

    if (BeginTable("table6", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
        TableNextRow();

        for (int i = 0; i < 4; i++) {
            TableSetColumnIndex(i);

            if (Selectable(gs_context_names[i], gs_context == i)) {
                gs_context = i;
            }
        }

        EndTable();
    }

    if (BeginChild("##gsr")) {
        switch (gs_context) {
            case 0: {
                show_privileged_registers(lunar);
            } break;

            case 1: {
                show_context_registers(lunar, 0);
            } break;

            case 2: {
                show_context_registers(lunar, 1);
            } break;

            case 3: {
                show_internal_registers(lunar);
            } break;
        }
    } EndChild();
}

void show_gs_debugger(lunar::instance* lunar) {
    using namespace ImGui;

    if (Begin("GS", &lunar->show_gs_debugger, ImGuiWindowFlags_MenuBar)) {
        if (BeginMenuBar()) {
            if (BeginMenu("Settings")) {
                if (BeginMenu(ICON_MS_CROP " Sizing")) {
                    for (int i = 0; i < 4; i++) {
                        if (Selectable(sizing_combo_items[i], i == gs_sizing_index)) {
                            gs_table_sizing = table_sizing_flags[i];
                            gs_sizing_index = i;
                        }
                    }

                    EndMenu();
                }

                EndMenu();
            }

            EndMenuBar();
        }

        if (BeginTable("table5", 5, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            TableNextRow();

            for (int i = 0; i < 5; i++) {
                TableSetColumnIndex(i);

                if (Selectable(gs_debug_names[i], gs_debug_index == i)) {
                    gs_debug_index = i;
                }
            }

            EndTable();
        }

        if (BeginChild("##gschild")) {
            switch (gs_debug_index) {
                case 0: {
                    show_gs_registers(lunar);
                } break;

                case 3: {
                    show_gs_queue(lunar);
                } break;
            }
        } EndChild();
    } End();
}

}