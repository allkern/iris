#ifndef LOADCORE_H_
#define LOADCORE_H_

#include "../iop.h"

#ifdef __cplusplus
extern "C" {
#endif

struct iop_module {
    char name[64];
    uint16_t version;
    uint32_t text_addr;
    uint32_t entry;
    uint32_t gp;
    uint32_t text_size;
    uint32_t data_size;
    uint32_t bss_size;
};

int loadcore_reg_lib_ent(struct iop_state* iop);
void refresh_module_list(struct iop_state* iop);

#ifdef __cplusplus
}
#endif

#endif // LOADCORE_H_
