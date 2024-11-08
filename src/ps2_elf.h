#ifndef PS2_ELF_H
#define PS2_ELF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <elf.h>

#include "ps2.h"

int ps2_elf_load(struct ps2_state* ps2, const char* path);

#ifdef __cplusplus
}
#endif

#endif