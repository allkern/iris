#ifndef SYSMEM_H
#define SYSMEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../iop.h"
#include "../iop_export.h"

int sysmem_kprintf(struct iop_state* iop);

#ifdef __cplusplus
}
#endif

#endif