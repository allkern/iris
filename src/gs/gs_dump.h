#ifndef GS_DUMP_H
#define GS_DUMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#include "gs/gs.h"
#include "ee/gif.h"

struct gs_dump;

struct gs_dump* gs_dump_create(void);
void gs_dump_destroy(struct gs_dump* dump);
int gs_dump_begin(struct gs_dump* dump, const char* path, struct ps2_gs* gs, struct ps2_gif* gif, const void* vram, uint32_t vram_size, const char* serial, uint32_t crc);
int gs_dump_is_active(struct gs_dump* dump);
void gs_dump_transfer(struct gs_dump* dump, int path, const void* data, size_t size);
void gs_dump_vsync(struct gs_dump* dump, struct ps2_gs* gs);
void gs_dump_readfifo(struct gs_dump* dump, uint32_t size);
void gs_dump_end(struct gs_dump* dump);

#ifdef __cplusplus
}
#endif

#endif
