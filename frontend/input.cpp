#include "iris.hpp"

namespace iris {

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

void handle_keydown_event(iris::instance* iris, SDL_KeyboardEvent& key) {
    switch (key.keysym.sym) {
        case SDLK_SPACE: iris->pause = !iris->pause; break;
        case SDLK_F11: {
            iris->fullscreen = !iris->fullscreen;

            SDL_SetWindowFullscreen(iris->window, iris->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        } break;
        case SDLK_m: {
            ps2_iop_intc_irq(iris->ps2->iop_intc, IOP_INTC_USB);
        } break;
    }

    uint16_t mask = map_button(key.keysym.sym);

    ds_button_press(iris->ds, mask);
}

void handle_keyup_event(iris::instance* iris, SDL_KeyboardEvent& key) {
    uint16_t mask = map_button(key.keysym.sym);

    ds_button_release(iris->ds, mask);
}

}