#ifndef MTAP_H
#define MTAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "iop/sio2.h"

struct mtap_state {
    struct sio2_device port[8];
};

struct mtap_state* mtap_attach(struct ps2_sio2* sio2, int port);
void mtap_detach(void* udata);

#ifdef __cplusplus
}
#endif

#endif