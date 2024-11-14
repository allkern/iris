#ifndef CDVD_H
#define CDVD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct ps2_cdvd {
    uint8_t n_cmd;
    uint8_t n_stat;
    uint8_t error;
    uint8_t i_stat;
    uint8_t status;
    uint8_t sticky_status;
    uint8_t disc_type;
    uint8_t s_cmd;
    uint8_t s_stat;
    uint8_t n_params[16];
    uint8_t s_params[16];
    uint8_t* s_fifo;
    int n_param_index;
    int s_param_index;
    int s_fifo_index;
    int s_fifo_size;
};

struct ps2_cdvd* ps2_cdvd_create(void);
void ps2_cdvd_init(struct ps2_cdvd* cdvd);
void ps2_cdvd_destroy(struct ps2_cdvd* cdvd);
void ps2_cdvd_open(struct ps2_cdvd* cdvd, const char* path);
void ps2_cdvd_close(struct ps2_cdvd* cdvd);
uint64_t ps2_cdvd_read8(struct ps2_cdvd* cdvd, uint32_t addr);
void ps2_cdvd_write8(struct ps2_cdvd* cdvd, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif