#include <stdlib.h>
#include <string.h>

#include "mtap.h"

void mtap_cmd_probe(struct ps2_sio2* sio2, struct mtap_state* mtap) {
    printf("mtap: mtap_cmd_probe\n");

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0x80);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x04);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, 0x5a);
}

void mtap_handle_command(struct ps2_sio2* sio2, void* udata, int cmd) {
    struct mtap_state* mtap = (struct mtap_state*)udata;

    switch (cmd) {
        case 0x12: mtap_cmd_probe(sio2, mtap); return;
    }

    printf("mtap: Unhandled command %02x\n", cmd);

    exit(1);
}

struct mtap_state* mtap_sio2_attach(struct ps2_sio2* sio2, int port) {
    struct mtap_state* mtap = malloc(sizeof(struct mtap_state));
    struct sio2_device dev;

    dev.detach = mtap_sio2_detach;
    dev.handle_command = mtap_handle_command;
    dev.udata = mtap;

    memset(mtap, 0, sizeof(struct mtap_state));

    ps2_sio2_attach_device(sio2, dev, port);

    return mtap;
}

void mtap_sio2_detach(void* udata) {
    struct mtap_state* mtap = (struct mtap_state*)udata;

    free(mtap);
}