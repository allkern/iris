#include "iris.hpp"

#include "iop/hle/loadcore.h"

#include "res/IconsMaterialSymbols.h"

namespace iris {

void show_iop_modules(iris::instance* iris)
{
    using namespace ImGui;
    struct iop_state* iop = iris->ps2->iop;

    if (Begin("IOP Modules", &iris->show_iop_modules)) {
        if (Button(ICON_MS_REFRESH)) {
            refresh_module_list(iris->ps2->iop);
        }

        if (BeginTable("##iopmodules", 4, ImGuiTableFlags_RowBg)) {
            PushFont(iris->font_small_code);
            TableSetupColumn("Name");
            TableSetupColumn("Text Start");
            TableSetupColumn("Data Start");
            TableSetupColumn("BSS Start");
            TableHeadersRow();
            PopFont();

            for (int i = 0; i < iop->module_count; i++) {
                struct iop_module *mod = &iop->module_list[i];

                TableNextRow();

                TableSetColumnIndex(0);
                Text("%s", mod->name);

                // text section
                TableSetColumnIndex(1);
                uint32_t addr = mod->text_addr;
                Text("0x%08x", addr);

                // data section
                TableSetColumnIndex(2);
                addr += mod->text_size;
                Text("0x%08x", addr);

                // bss section
                TableSetColumnIndex(3);
                addr += mod->data_size;
                Text("0x%08x", iop->module_list[i].text_addr);
            }

            EndTable();
        }
    }
    End();
}

}
