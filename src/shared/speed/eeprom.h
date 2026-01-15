#ifndef EEPROM_H
#define EEPROM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#define PP_DOUT 4
#define PP_DIN 5
#define PP_SCLK 6
#define PP_CSEL 7
#define PP_OP_READ 2
#define PP_OP_WRITE 1
#define PP_OP_EWEN 0
#define PP_OP_EWDS 0

enum {
    EEPROM_S_CMD_START,
    EEPROM_S_CMD_READ,
    EEPROM_S_ADDR_READ,
    EEPROM_S_TRANSMIT
};

struct ps2_eeprom {
    int state;

    // Pins
    uint8_t clk;
    uint8_t din;
    uint8_t dout;

    uint8_t cmd;
    uint8_t sequence;
    uint8_t addr;

    uint16_t buf[32];
};

struct ps2_eeprom* ps2_eeprom_create(void);
void ps2_eeprom_init(struct ps2_eeprom* eeprom);
void ps2_eeprom_load(struct ps2_eeprom* eeprom, const uint16_t* data);
void ps2_eeprom_destroy(struct ps2_eeprom* eeprom);
uint64_t ps2_eeprom_read(struct ps2_eeprom* eeprom);
void ps2_eeprom_write(struct ps2_eeprom* eeprom, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif