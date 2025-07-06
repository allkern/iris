#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

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

void show_privileged_registers(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_gs* gs = iris->ps2->gs;

    PushFont(iris->font_code);

    uint64_t* regs = &gs->pmode;

    if (BeginTable("table6", 2, ImGuiTableFlags_RowBg)) {
        PushFont(iris->font_small_code);
        TableSetupColumn("Register");
        TableSetupColumn("Value");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 19; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text("%s", gs_privileged_reg_names[i]);

            TableSetColumnIndex(1);

            Text("%08lx%08lx", regs[i] >> 32, regs[i] & 0xffffffff);
        }

        EndTable();
    }

    PopFont();
}

void show_context_registers(iris::instance* iris, int ctx) {
    using namespace ImGui;

    struct ps2_gs* gs = iris->ps2->gs;

    PushFont(iris->font_code);

    uint64_t* regs = &gs->context[ctx].frame;

    if (BeginTable("table9", 2, ImGuiTableFlags_RowBg)) {
        PushFont(iris->font_small_code);
        TableSetupColumn("Register");
        TableSetupColumn("Value");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 13; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text("%s", gs_gp_reg_names[i]);

            TableSetColumnIndex(1);

            Text("%08lx%08lx", regs[i] >> 32, regs[i] & 0xffffffff);
        }

        EndTable();
    }

    PopFont();
}

void show_internal_registers(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_gs* gs = iris->ps2->gs;

    PushFont(iris->font_code);

    uint64_t* regs = &gs->prim;

    if (BeginTable("table6", 2, ImGuiTableFlags_RowBg)) {
        PushFont(iris->font_small_code);
        TableSetupColumn("Register");
        TableSetupColumn("Value");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 28; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text("%s", gs_internal_reg_names[i]);

            TableSetColumnIndex(1);

            Text("%08lx%08lx", regs[i] >> 32, regs[i] & 0xffffffff);
        }

        EndTable();
    }

    PopFont();
}

static inline void show_gs_vertex_xy(iris::instance* iris, const gs_vertex* vtx) {
    using namespace ImGui;

    TableSetColumnIndex(0);

    Text("Position (XY)");

    TableSetColumnIndex(1);

    PushFont(iris->font_code);

    Text("0x%04lx, 0x%04lx", vtx->xyz & 0xffff, (vtx->xyz >> 16) & 0xffff);

    TableSetColumnIndex(2);

    Text("%d.%04ld, %d.%04ld", vtx->x, (vtx->xyz & 0xf) * 625, vtx->y, ((vtx->xyz >> 16) & 0xf) * 625);

    PopFont();

    TableNextRow();
}

static inline void show_gs_vertex_z(iris::instance* iris, const gs_vertex* vtx) {
    using namespace ImGui;

    TableSetColumnIndex(0);

    Text("Depth (Z)");

    TableSetColumnIndex(1);

    PushFont(iris->font_code);

    Text("0x%08x", vtx->z);

    TableSetColumnIndex(2);

    Text("%d", vtx->z);

    PopFont();

    TableNextRow();
}

static inline void show_gs_vertex_stq(iris::instance* iris, const gs_vertex* vtx) {
    using namespace ImGui;

    TableSetColumnIndex(0);

    Text("STQ");

    TableSetColumnIndex(1);

    PushFont(iris->font_code);

    Text("0x%08lx, 0x%08lx, 0x%08lx", vtx->st & 0xffffffff, vtx->st >> 32, vtx->rgbaq >> 32);

    TableSetColumnIndex(2);

    Text("%f, %f, %f", vtx->s, vtx->t, vtx->q);

    PopFont();

    TableNextRow();
}

static inline void show_gs_vertex_uv(iris::instance* iris, const gs_vertex* vtx) {
    using namespace ImGui;

    TableSetColumnIndex(0);

    Text("UV");

    TableSetColumnIndex(1);

    PushFont(iris->font_code);

    Text("0x%08lx, 0x%08lx",
        vtx->uv & 0x3fff,
        (vtx->uv >> 16) & 0x3fff
    );

    TableSetColumnIndex(2);

    Text("%d.%04ld, %d.%04ld",
        vtx->u, (vtx->uv & 0xf) * 625,
        vtx->v, ((vtx->uv >> 16) & 0xf) * 625
    );

    PopFont();

    TableNextRow();
}

static inline void show_gs_vertex_rgba(iris::instance* iris, const gs_vertex* vtx) {
    using namespace ImGui;

    TableSetColumnIndex(0);

    Text("Color (RGBA)");

    TableSetColumnIndex(1);

    PushFont(iris->font_code);

    Text("0x%08lx", vtx->rgbaq & 0xffffffff);

    TableSetColumnIndex(2);

    Text("0x%02lx, 0x%02lx, 0x%02lx, 0x%02lx",
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

void show_gs_vertex(iris::instance* iris, const gs_vertex* vtx) {
    using namespace ImGui;

    if (BeginTable("table10", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        PushFont(iris->font_small_code);
        TableSetupColumn("Attribute");
        TableSetupColumn("Raw");
        TableSetupColumn("Value");
        TableHeadersRow();
        PopFont();

        TableNextRow();

        show_gs_vertex_xy(iris, vtx);
        show_gs_vertex_z(iris, vtx);
        show_gs_vertex_stq(iris, vtx);
        show_gs_vertex_uv(iris, vtx);
        show_gs_vertex_rgba(iris, vtx);

        EndTable();
    }
}

void show_gs_queue(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_gs* gs = iris->ps2->gs;

    for (unsigned int i = 0; i < gs->vqi; i++) {
        char buf[32]; sprintf(buf, "Vertex %d", i+1);

        if (TreeNode(buf)) {
            show_gs_vertex(iris, &gs->vq[i]);

            TreePop();
        }
    }

    if (TreeNode("Uninitialized")) {
        for (int i = gs->vqi; i < 4; i++) {
            char buf[32]; sprintf(buf, "Vertex %d", i+1);

            if (TreeNode(buf)) {
                show_gs_vertex(iris, &gs->vq[i]);

                TreePop();
            }
        }

        TreePop();
    }
}

void show_gs_registers(iris::instance* iris) {
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
                show_privileged_registers(iris);
            } break;

            case 1: {
                show_context_registers(iris, 0);
            } break;

            case 2: {
                show_context_registers(iris, 1);
            } break;

            case 3: {
                show_internal_registers(iris);
            } break;
        }
    } EndChild();
}

static uint32_t addr = 0, width = 0, height = 0;
static float scale = 1.0f;

const char* format_names[] = {
    "32-bit/24-bit",
    "16-bit"
};

int format = 0;
static SDL_GPUTexture* tex = nullptr;
static SDL_GPUSampler* sampler = nullptr;
static SDL_GPUTextureSamplerBinding tsb;

void show_gs_memory(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_gs* gs = iris->ps2->gs;

    static char buf0[16];
    static char buf1[16];
    static char buf2[16];
    static char buf3[16];

    if (InputText("Address##", buf0, 9, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (buf0[0]) {
            addr = strtoul(buf0, NULL, 16);
        }
    } 

    if (InputText("Width##", buf1, 9, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (buf1[0]) {
            width = strtoul(buf1, NULL, 0);
        }
    }

    if (InputText("Height##", buf2, 9, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (buf2[0]) {
            height = strtoul(buf2, NULL, 0);
        }
    }

    sprintf(buf3, "%.1f", scale);

    if (BeginCombo("Scale", buf3, ImGuiComboFlags_HeightSmall)) {
        char buf4[16];

        for (int i = 0; i < 6; i++) {
            sprintf(buf4, "%.1f", 1.0f + (0.5f * i));

            if (Selectable(buf4, scale == 1.0f + (0.5f * i))) {
                scale = 1.0f + (0.5f * i);
            }
        }
        
        EndCombo();
    }

    if (BeginCombo("Format", format_names[format], ImGuiComboFlags_HeightSmall)) {
        for (int i = 0; i < 2; i++) {
            if (Selectable(format_names[i], i == format)) {
                format = i;
            }
        }

        EndCombo();
    }

    SDL_GPUTextureFormat fmt;
    int stride = 0;

    switch (format) {
        case GS_PSMCT32:
        case GS_PSMCT24: {
            fmt = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
            stride = 4;
        } break;

        case GS_PSMCT16:
        case GS_PSMCT16S: {
            fmt = SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM;
            stride = 2;
        } break;
    }

    if (Button("View")) {
        addr = strtoul(buf0, NULL, 16);
        width = strtoul(buf1, NULL, 0);
        height = strtoul(buf2, NULL, 0);

        if (tex) {
            SDL_ReleaseGPUTexture(iris->device, tex);
        }

        if (sampler) {
            SDL_ReleaseGPUSampler(iris->device, sampler);
        }

        SDL_GPUTextureCreateInfo tci = {};

        tci.format = fmt;
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = width;
        tci.height = height;
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        tci.sample_count = SDL_GPU_SAMPLECOUNT_1;

        tex = SDL_CreateGPUTexture(iris->device, &tci);

        SDL_GPUSamplerCreateInfo sci = {
            .min_filter = SDL_GPU_FILTER_LINEAR,
            .mag_filter = SDL_GPU_FILTER_LINEAR,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        };

        sampler = SDL_CreateGPUSampler(iris->device, &sci);

        tsb.texture = tex;
        tsb.sampler = sampler;
    }

    if (!tex)
        return;

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(iris->device);

    SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cb);

    SDL_GPUTransferBufferCreateInfo ttbci = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = width * height * stride
    };

    SDL_GPUTransferBuffer* texture_tb = SDL_CreateGPUTransferBuffer(iris->device, &ttbci);

    // fill the transfer buffer
    void* data = SDL_MapGPUTransferBuffer(iris->device, texture_tb, false);

    void* ptr = ((uint8_t*)gs->vram) + addr;

    SDL_memcpy(data, ptr, (width * stride) * height);

    SDL_UnmapGPUTransferBuffer(iris->device, texture_tb);

    SDL_GPUTextureTransferInfo tti = {
        .transfer_buffer = texture_tb,
        .offset = 0,
    };

    SDL_GPUTextureRegion tr = {
        .texture = tex,
        .w = width,
        .h = height,
        .d = 1,
    };

    SDL_UploadToGPUTexture(cp, &tti, &tr, false);

    // end the copy pass
    SDL_EndGPUCopyPass(cp);

    ImageWithBg((ImTextureID)(intptr_t)&tsb, ImVec2(width*scale, height*scale), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1));
}

void show_gs_debugger(iris::instance* iris) {
    using namespace ImGui;

    if (Begin("GS", &iris->show_gs_debugger, ImGuiWindowFlags_MenuBar)) {
        if (BeginMenuBar()) {
            if (BeginMenu("Settings")) {
                if (BeginMenu(ICON_MS_CROP " Sizing")) {
                    for (int i = 0; i < 4; i++) {
                        if (Selectable(sizing_combo_items[i], i == gs_sizing_index)) {
                            gs_table_sizing = table_sizing_flags[i];
                            gs_sizing_index = i;
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
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
                    show_gs_registers(iris);
                } break;

                case 3: {
                    show_gs_queue(iris);
                } break;

                case 4: {
                    show_gs_memory(iris);
                } break;
            }
        } EndChild();
    } End();
}

}