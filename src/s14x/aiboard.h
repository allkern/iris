#ifndef S14X_AIBOARD_H
#define S14X_AIBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include "link.h"

struct s14x_aiboard {
    uint16_t version;
};

struct s14x_aiboard* s14x_aiboard_create(void);
int s14x_aiboard_init(struct s14x_aiboard* aiboard);
void s14x_aiboard_destroy(struct s14x_aiboard* aiboard);

void s14x_aiboard_handle_packet(void* udata, struct link_packet* in, struct link_packet* out);

#ifdef __cplusplus
}
#endif

#endif