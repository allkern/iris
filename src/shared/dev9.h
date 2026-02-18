#ifndef DEV9_H
#define DEV9_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#define DEV9_TYPE_PCMCIA 0x20 // CXD9566
#define DEV9_TYPE_EXPBAY 0x30 // CXD9611

struct ps2_dev9 {
    uint16_t r_1460;
    uint16_t r_1462;
    uint16_t r_1464;
    uint16_t r_1466;
    uint16_t r_1468;
    uint16_t r_146a;
    uint16_t power;
    uint16_t rev;
    uint16_t r_1470;
    uint16_t r_1472;
    uint16_t r_1474;
    uint16_t r_1476;
    uint16_t r_1478;
    uint16_t r_147a;
    uint16_t r_147c;
    uint16_t r_147e;
};

struct ps2_dev9* ps2_dev9_create(void);
void ps2_dev9_init(struct ps2_dev9* dev9, int model);
void ps2_dev9_destroy(struct ps2_dev9* dev9);
uint64_t ps2_dev9_read8(struct ps2_dev9* dev9, uint32_t addr);
uint64_t ps2_dev9_read16(struct ps2_dev9* dev9, uint32_t addr);
uint64_t ps2_dev9_read32(struct ps2_dev9* dev9, uint32_t addr);
void ps2_dev9_write8(struct ps2_dev9* dev9, uint32_t addr, uint64_t data);
void ps2_dev9_write16(struct ps2_dev9* dev9, uint32_t addr, uint64_t data);
void ps2_dev9_write32(struct ps2_dev9* dev9, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif