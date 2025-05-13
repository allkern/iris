#ifndef DS_H
#define DS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "iop/sio2.h"

#define DS_BT_SELECT   0x0001
#define DS_BT_L3       0x0002
#define DS_BT_R3       0x0004
#define DS_BT_START    0x0008
#define DS_BT_UP       0x0010
#define DS_BT_RIGHT    0x0020
#define DS_BT_DOWN     0x0040
#define DS_BT_LEFT     0x0080
#define DS_BT_L2       0x0100
#define DS_BT_R2       0x0200
#define DS_BT_L1       0x0400
#define DS_BT_R1       0x0800
#define DS_BT_TRIANGLE 0x1000
#define DS_BT_CIRCLE   0x2000
#define DS_BT_CROSS    0x4000
#define DS_BT_SQUARE   0x8000

#define DS_AX_RIGHT_V 0
#define DS_AX_RIGHT_H 1
#define DS_AX_LEFT_V 2
#define DS_AX_LEFT_H 3

struct ds_state {
    int port;
    uint16_t buttons;
    uint8_t ax_right_y;
    uint8_t ax_right_x;
    uint8_t ax_left_y;
    uint8_t ax_left_x;
    int config_mode;
    int act_index;
    int mode_index;
    int mode;
    int vibration[2];
};

struct ds_state* ds_sio2_attach(struct ps2_sio2* sio2, int port);
void ds_button_press(struct ds_state* ds, uint16_t mask);
void ds_button_release(struct ds_state* ds, uint16_t mask);
void ds_analog_change(struct ds_state* ds, int axis, uint8_t value);
void ds_sio2_detach(struct ds_state* ds);

#ifdef __cplusplus
}
#endif

#endif