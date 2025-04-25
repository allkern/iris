#include "mcd.h"

#include <stdlib.h>
#include <string.h>

void mcd_cmd_probe(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_probe\n");

    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}
void mcd_cmd_auth_f3(struct ps2_sio2* sio2, struct mcd_state* mcd) {
    printf("mcd: mcd_cmd_auth_f3\n");

    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x2b);
    queue_push(sio2->out, mcd->term);
}

void mcd_handle_command(struct ps2_sio2* sio2, void* udata) {
    struct mcd_state* mcd = (struct mcd_state*)udata;

    uint8_t cmd = sio2->in->buf[1];

    switch (cmd) {
        case 0x11: mcd_cmd_probe(sio2, mcd); return;
        case 0xf3: mcd_cmd_auth_f3(sio2, mcd); return;
    }

    printf("mcd: Unhandled command %02x\n", cmd);

    exit(1);
}

struct mcd_state* mcd_sio2_attach(struct ps2_sio2* sio2, int port) {
    struct mcd_state* mcd = malloc(sizeof(struct mcd_state));
    struct sio2_device dev;

    mcd->term = 0x55;

    dev.handle_command = mcd_handle_command;
    dev.udata = mcd;

    memset(mcd, 0, sizeof(struct mcd_state));

    ps2_sio2_attach_device(sio2, dev, port);

    return mcd;
}