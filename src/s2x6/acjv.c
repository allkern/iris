#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "acjv.h"

struct s2x6_acjv* s2x6_acjv_create(void) {
    return malloc(sizeof(struct s2x6_acjv));
}

void s2x6_acjv_init(struct s2x6_acjv* acjv) {
    memset(acjv, 0, sizeof(struct s2x6_acjv));
}

void s2x6_acjv_destroy(struct s2x6_acjv* acjv) {
    free(acjv);
}

void acjv_handle_packet(struct s2x6_acjv* acjv, const uint8_t* in, uint8_t* out) {
    struct jvs_packet* packet = (struct jvs_packet*)in;

    if (packet->sync != 0x0e) {
        printf("acjv: Invalid packet sync byte %02x\n", packet->sync);

        return;
    }
}

uint64_t s2x6_acjv_read16(struct s2x6_acjv* acjv, uint32_t addr) {
    if (addr >= ACJV_RDBASE && addr < 0x124045FE) {
        int x = (addr - ACJV_RDBASE) / 2;
		if (/*CurrentCMD == NONE &&*/ (x == 2 || x == 3 || x == 4)) return acjv->rdbuf[x] | 1;
        return (uint16_t)acjv->rdbuf[x];
    } else if ((addr == 0x124045FE)) {
		return (uint16_t)acjv->rdbuf[(addr - ACJV_RDBASE)/2];
	}

    printf("Reading 16-bit value from ACJV at address 0x%08X\n", addr);

    return 0;
}

uint64_t s2x6_acjv_read32(struct s2x6_acjv* acjv, uint32_t addr) {
    printf("Reading 32-bit value from ACJV at address 0x%08X\n", addr);

    exit(1);

    return 0;
}

void s2x6_acjv_write16(struct s2x6_acjv* acjv, uint32_t addr, uint64_t data) {
    printf("Writing 16-bit value to ACJV at address 0x%08X\n", addr);

    if (addr >= ACJV_WRBASE && addr < 0x12404BFE) { //0x124048FE
        uint32_t x = (addr - ACJV_WRBASE) / 2;
        acjv->wrbuf[x] = (uint16_t)data;
    } else if (addr == 0x12404BFE) {
        acjv->wrbuf[(addr - ACJV_WRBASE) / 2] = (uint16_t)data;
    }
}

void s2x6_acjv_write32(struct s2x6_acjv* acjv, uint32_t addr, uint64_t data) {
    printf("Writing 32-bit value to ACJV at address 0x%08X\n", addr);
}