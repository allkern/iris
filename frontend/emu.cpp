#include "iris.hpp"

namespace iris::emu {

bool init(iris::instance* iris) {
    // Initialize our emulator state
    iris->ps2 = ps2_create();

    ps2_init(iris->ps2);
    ps2_init_kputchar(iris->ps2, iris::handle_ee_tty_event, iris, iris::handle_iop_tty_event, iris);

    iris->ds[0] = ds_attach(iris->ps2->sio2, 0);

    return true;
}

void destroy(iris::instance* iris) {
    ps2_destroy(iris->ps2);
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
        ps1_mcd_attach(iris->ps2->sio2, slot+2, path);

        iris->mcd_slot_type[slot] = 2;

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