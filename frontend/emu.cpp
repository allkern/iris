#include "iris.hpp"

namespace iris::emu {

bool init(iris::instance* iris) {
    // Initialize our emulator state
    iris->ps2 = ps2_create();

    ps2_init(iris->ps2);
    ps2_init_kputchar(iris->ps2, iris::handle_ee_tty_event, iris, iris::handle_iop_tty_event, iris);

    iris->ds[0] = ds_attach(iris->ps2->sio2, 0);
    iris->mcd[0] = mcd_attach(iris->ps2->sio2, 2, "slot1.mcd");

    // Apply settings loaded from file/CLI
    ps2_set_timescale(iris->ps2, iris->timescale);

    ee_set_fmv_skip(iris->ps2->ee, iris->skip_fmv);

    return true;
}

void destroy(iris::instance* iris) {
    ps2_destroy(iris->ps2);
}

}