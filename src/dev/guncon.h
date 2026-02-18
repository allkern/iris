#ifndef GUNCON_H
#define GUNCON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "iop/sio2.h"

#define GUNCON_BT_START  0x0008
#define GUNCON_BT_CIRCLE 0x2000
#define GUNCON_BT_CROSS  0x4000

#define GUNCON_AX_X 0
#define GUNCON_AX_Y 1

struct guncon_state {
    int port;
    uint16_t buttons;
    uint16_t x;
    uint16_t y;
    int config_mode;
};

struct guncon_state* guncon_attach(struct ps2_sio2* sio2, int port);
void guncon_button_press(struct guncon_state* guncon, uint16_t mask);
void guncon_button_release(struct guncon_state* guncon, uint16_t mask);
void guncon_analog_change(struct guncon_state* guncon, int axis, uint8_t value);
void guncon_detach(void* udata);

#ifdef __cplusplus
}
#endif

#endif