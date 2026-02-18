#include <string.h>
#include <stdio.h>

#include "eeprom.h"

uint16_t default_data[32] = {
    0x6D76, 0x6361, 0x3130, 0x0207,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x1000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x0010, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
    0x1000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

struct ps2_eeprom* ps2_eeprom_create(void) {
    return malloc(sizeof(struct ps2_eeprom));
}

void ps2_eeprom_init(struct ps2_eeprom* eeprom) {
    memset(eeprom, 0, sizeof(struct ps2_eeprom));

    memcpy(eeprom->buf, default_data, 32 * sizeof(uint16_t));
}

void ps2_eeprom_load(struct ps2_eeprom* eeprom, const uint16_t* data) {
    memcpy(eeprom->buf, data, 32 * sizeof(uint16_t));
}

void ps2_eeprom_destroy(struct ps2_eeprom* eeprom) {
    free(eeprom);
}

uint64_t ps2_eeprom_read(struct ps2_eeprom* eeprom) {
    return eeprom->dout << PP_DOUT;
}

void eeprom_step(struct ps2_eeprom* eeprom) {
    switch (eeprom->state) {
        case EEPROM_S_CMD_START: {
            eeprom->sequence = 0;

            if (eeprom->din) {
                eeprom->state = EEPROM_S_CMD_READ;
            }
        } break;

        case EEPROM_S_CMD_READ: {
            eeprom->cmd |= eeprom->din << (1 - eeprom->sequence);
            eeprom->sequence++;

            if (eeprom->sequence == 2) {
                eeprom->sequence = 0;
                eeprom->state = EEPROM_S_ADDR_READ;
            }
        } break;

        case EEPROM_S_ADDR_READ: {
            // Address read in from highest bit to lowest bit
            eeprom->addr |= eeprom->din << (5 - eeprom->sequence);

            // Don't know what the behaviour would be but lets prevent oob.
            eeprom->addr = eeprom->addr & 31;

            eeprom->sequence++;
            if (eeprom->sequence == 6)
            {
                eeprom->state = EEPROM_S_TRANSMIT;
                eeprom->sequence = 0;
            }
        } break;

        // fire away, bit position increments every pulse
        case EEPROM_S_TRANSMIT: {
            if (eeprom->cmd == PP_OP_READ) {
                eeprom->dout = (eeprom->buf[eeprom->addr] >> (15 - eeprom->sequence)) & 1;
            }

            if (eeprom->cmd == PP_OP_WRITE) {
                eeprom->buf[eeprom->addr] = eeprom->buf[eeprom->addr] | (eeprom->din << (15 - eeprom->sequence));
            }

            eeprom->sequence++;

            if (eeprom->sequence == 16) {
                eeprom->sequence = 0;

                // Let's prevent oob here too
                eeprom->addr = (eeprom->addr + 1) & 31;
            }
        } break;
    }
    return;
}

void ps2_eeprom_write(struct ps2_eeprom* eeprom, uint64_t data) {
    uint8_t csel = (data >> PP_CSEL) & 1;
    uint8_t sclk = (data >> PP_SCLK) & 1;
    uint8_t din = (data >> PP_DIN) & 1;

    if (!csel) {
        eeprom->sequence = 0;
        eeprom->addr = 0;
        eeprom->state = EEPROM_S_CMD_START;
        eeprom->clk = 0;

        return;
    }

    eeprom->din = din;

    if (sclk && !eeprom->clk) {
        eeprom_step(eeprom);
    }

    eeprom->clk = sclk;
}