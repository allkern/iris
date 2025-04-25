#ifndef MCD_H
#define MCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "iop/sio2.h"

struct mcd_state {
    int port;
    uint8_t term;
    uint16_t buttons;
    uint8_t ax_right_y;
    uint8_t ax_right_x;
    uint8_t ax_left_y;
    uint8_t ax_left_x;
    int config_mode;
    int act_index;
    int mode_index;
};

struct mcd_state* mcd_sio2_attach(struct ps2_sio2* sio2, int port);
void mcd_sio2_detach(struct mcd_state* mcd);

#ifdef __cplusplus
}
#endif

#endif