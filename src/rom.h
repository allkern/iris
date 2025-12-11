#ifndef ROM_H
#define ROM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

struct ps2_rom_info {
    char md5hash[33];
    const char* version;
    const char* region;
    const char* model;
    int system;
};

struct ps2_rom_info ps2_rom0_search(uint8_t* rom, size_t size);
struct ps2_rom_info ps2_rom1_search(uint8_t* rom, size_t size);

#ifdef __cplusplus
}
#endif

#endif