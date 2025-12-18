#include "iris.hpp"

namespace iris::emu {

bool init(iris::instance* iris) {
    // Initialize our emulator state
    iris->ps2 = ps2_create();

    ps2_init(iris->ps2);
    ps2_init_tty_handler(iris->ps2, PS2_TTY_EE, iris::handle_ee_tty_event, iris);
    ps2_init_tty_handler(iris->ps2, PS2_TTY_IOP, iris::handle_iop_tty_event, iris);
    ps2_init_tty_handler(iris->ps2, PS2_TTY_SYSMEM, iris::handle_sysmem_tty_event, iris);

    iris->ds[0] = ds_attach(iris->ps2->sio2, 0);

    return true;
}

void destroy(iris::instance* iris) {
    ps2_destroy(iris->ps2);
}

const char* get_extension(const char* path) {
    const char* dot = strrchr(path, '.');

    if (!dot || dot == path)
        return nullptr;

    return dot + 1;
}

int attach_memory_card(iris::instance* iris, int slot, const char* path) {
    detach_memory_card(iris, slot);

    FILE* file = fopen(path, "rb");

    if (!file) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fclose(file);

    if (size < 0x800000) {
        struct ps1_mcd_state* mcd = ps1_mcd_attach(iris->ps2->sio2, slot+2, path);

        std::string ext = get_extension(path);

        if (ext == "psm" || ext == "pocket") {
            ps1_mcd_set_type(mcd, 1);

            iris->mcd_slot_type[slot] = 3;
        } else {
            ps1_mcd_set_type(mcd, 0);

            iris->mcd_slot_type[slot] = 2;
        }

        return 1;
    }

    mcd_attach(iris->ps2->sio2, slot+2, path);

    iris->mcd_slot_type[slot] = 1;

    return 1;
}

void detach_memory_card(iris::instance* iris, int slot) {
    iris->mcd_slot_type[slot] = 0;

    ps2_sio2_detach_device(iris->ps2->sio2, slot+2);
}

}