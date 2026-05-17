#ifndef ACJV_H
#define ACJV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ACJV_BASE_ADDDR 0x12400000
#define ACJV_RANGE      0x1240
#define ACJV_ADDR_CAP   0x8000 // ACJV.IRX r/w fun will do `(2 * (addr & 0x3FFF)`, meaning available range is `0x12400000` - `0x12407FFE`
#define ACJV_PACKETSIZE 0x300 // seems to be the ammount of bytes that's always read/written by the game

#define ACJV_RDBASE 0x12404000 // acmeme access addr 0x2300
#define ACJV_WRBASE 0x12404600 // acmeme access addr 0x2000

#define ACJV_CTR_START 0x12416002 // set to 0 when ACJV.IRX runs
#define ACJV_CTR_STOP  0x12416000 // set to 0 during: JVFIRM upload begins, ACCORE starts, `acJvModuleStop()` is called

#define ACJV_RDWR_SIZELIMIT 0x4000 // ACJV.IRX read/write functions only consider 14 bits from addr

#define JVS_CMD_SUCCESS 0x1
#define JVS_REVISION 0x30 //Revision 3.0
#define JVS_VERSION 0x10 //Version 1.0
#define JVS_PLAYER_COUNT 2

struct jvs_packet {
    uint8_t sync;
    uint8_t dest;
    uint8_t size;
    uint8_t data[0x2fc];
    uint8_t checksum;
};

struct s2x6_acjv {
    uint16_t wrbuf[ACJV_PACKETSIZE / 2];
    uint16_t rdbuf[ACJV_PACKETSIZE / 2];

    uint32_t dummy;
};

struct s2x6_acjv* s2x6_acjv_create(void);
void s2x6_acjv_init(struct s2x6_acjv* acjv);
void s2x6_acjv_destroy(struct s2x6_acjv* acjv);
uint64_t s2x6_acjv_read16(struct s2x6_acjv* acjv, uint32_t addr);
uint64_t s2x6_acjv_read32(struct s2x6_acjv* acjv, uint32_t addr);
void s2x6_acjv_write16(struct s2x6_acjv* acjv, uint32_t addr, uint64_t data);
void s2x6_acjv_write32(struct s2x6_acjv* acjv, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif