#include <string.h>

#include "flash.h"

static unsigned char xor_table[256] = {
	0x00, 0x87, 0x96, 0x11, 0xA5, 0x22, 0x33, 0xB4,
    0xB4, 0x33, 0x22, 0xA5, 0x11, 0x96, 0x87, 0x00,
	0xC3, 0x44, 0x55, 0xD2, 0x66, 0xE1, 0xF0, 0x77,
    0x77, 0xF0, 0xE1, 0x66, 0xD2, 0x55, 0x44, 0xC3,
	0xD2, 0x55, 0x44, 0xC3, 0x77, 0xF0, 0xE1, 0x66,
    0x66, 0xE1, 0xF0, 0x77, 0xC3, 0x44, 0x55, 0xD2,
	0x11, 0x96, 0x87, 0x00, 0xB4, 0x33, 0x22, 0xA5,
    0xA5, 0x22, 0x33, 0xB4, 0x00, 0x87, 0x96, 0x11,
	0xE1, 0x66, 0x77, 0xF0, 0x44, 0xC3, 0xD2, 0x55,
    0x55, 0xD2, 0xC3, 0x44, 0xF0, 0x77, 0x66, 0xE1,
	0x22, 0xA5, 0xB4, 0x33, 0x87, 0x00, 0x11, 0x96,
    0x96, 0x11, 0x00, 0x87, 0x33, 0xB4, 0xA5, 0x22,
	0x33, 0xB4, 0xA5, 0x22, 0x96, 0x11, 0x00, 0x87,
    0x87, 0x00, 0x11, 0x96, 0x22, 0xA5, 0xB4, 0x33,
	0xF0, 0x77, 0x66, 0xE1, 0x55, 0xD2, 0xC3, 0x44,
    0x44, 0xC3, 0xD2, 0x55, 0xE1, 0x66, 0x77, 0xF0,
	0xF0, 0x77, 0x66, 0xE1, 0x55, 0xD2, 0xC3, 0x44,
    0x44, 0xC3, 0xD2, 0x55, 0xE1, 0x66, 0x77, 0xF0,
	0x33, 0xB4, 0xA5, 0x22, 0x96, 0x11, 0x00, 0x87,
    0x87, 0x00, 0x11, 0x96, 0x22, 0xA5, 0xB4, 0x33,
	0x22, 0xA5, 0xB4, 0x33, 0x87, 0x00, 0x11, 0x96,
    0x96, 0x11, 0x00, 0x87, 0x33, 0xB4, 0xA5, 0x22,
	0xE1, 0x66, 0x77, 0xF0, 0x44, 0xC3, 0xD2, 0x55,
    0x55, 0xD2, 0xC3, 0x44, 0xF0, 0x77, 0x66, 0xE1,
	0x11, 0x96, 0x87, 0x00, 0xB4, 0x33, 0x22, 0xA5,
    0xA5, 0x22, 0x33, 0xB4, 0x00, 0x87, 0x96, 0x11,
	0xD2, 0x55, 0x44, 0xC3, 0x77, 0xF0, 0xE1, 0x66,
    0x66, 0xE1, 0xF0, 0x77, 0xC3, 0x44, 0x55, 0xD2,
	0xC3, 0x44, 0x55, 0xD2, 0x66, 0xE1, 0xF0, 0x77,
    0x77, 0xF0, 0xE1, 0x66, 0xD2, 0x55, 0x44, 0xC3,
	0x00, 0x87, 0x96, 0x11, 0xA5, 0x22, 0x33, 0xB4,
    0xB4, 0x33, 0x22, 0xA5, 0x11, 0x96, 0x87, 0x00
};

static void flash_calculate_xor(unsigned char buffer[128], unsigned char blah[4]) {
	unsigned char a = 0, b = 0, c = 0, i;

	for (i = 0; i < 128; i++)
	{
		a ^= xor_table[buffer[i]];
		if (xor_table[buffer[i]] & 0x80)
		{
			b ^= ~i;
			c ^= i;
		}
	}

	blah[0] = (~a) & 0x77;
	blah[1] = (~b) & 0x7F;
	blah[2] = (~c) & 0x7F;
}

static void flash_calculate_ecc(uint8_t page[FLASH_PAGE_SIZE_ECC]) {
	memset(page + FLASH_PAGE_SIZE, 0, FLASH_ECC_SIZE);

	flash_calculate_xor(page + 0 * (FLASH_PAGE_SIZE >> 2), page + FLASH_PAGE_SIZE + 0 * 3); //(ECC_SIZE>>2));
	flash_calculate_xor(page + 1 * (FLASH_PAGE_SIZE >> 2), page + FLASH_PAGE_SIZE + 1 * 3); //(ECC_SIZE>>2));
	flash_calculate_xor(page + 2 * (FLASH_PAGE_SIZE >> 2), page + FLASH_PAGE_SIZE + 2 * 3); //(ECC_SIZE>>2));
	flash_calculate_xor(page + 3 * (FLASH_PAGE_SIZE >> 2), page + FLASH_PAGE_SIZE + 3 * 3); //(ECC_SIZE>>2));
}

static const char* flash_get_cmd_name(uint32_t cmd) {
	switch (cmd) {
		case SM_CMD_READ1: return "READ1";
		case SM_CMD_READ2: return "READ2";
		case SM_CMD_READ3: return "READ3";
		case SM_CMD_RESET: return "RESET";
		case SM_CMD_WRITEDATA: return "WRITEDATA";
		case SM_CMD_PROGRAMPAGE: return "PROGRAMPAGE";
		case SM_CMD_ERASEBLOCK: return "ERASEBLOCK";
		case SM_CMD_ERASECONFIRM: return "ERASECONFIRM";
		case SM_CMD_GETSTATUS: return "GETSTATUS";
		case SM_CMD_READID: return "READID";
	}

    return "<unknown>";
}

struct ps2_flash* ps2_flash_create(void) {
    return malloc(sizeof(struct ps2_flash));
}

int ps2_flash_init(struct ps2_flash* flash, const char* path) {
    memset(flash, 0, sizeof(struct ps2_flash));

	flash->id = FLASH_ID_64MBIT;
	flash->counter = 0;
	flash->addrbyte = 0;
	flash->address = 0;
    flash->ctrl = FLASH_CTRL_READY;

	memset(flash->data, 0xff, FLASH_PAGE_SIZE);

	flash_calculate_ecc(flash->data);
    
	FILE* fd = fopen(path, "rb");

    if (!fd) {
        memset(flash->file, 0xff, FLASH_CARD_SIZE_ECC);

        return 0;
    }

    size_t size = fread(flash->file, 1, FLASH_CARD_SIZE_ECC, fd);

    if (size != FLASH_CARD_SIZE_ECC) {
        printf("flash: Flash file size incorrect (%zu bytes)\n", size);

        return 0;
    }

    fclose(fd);

    return 1;
}

void ps2_flash_destroy(struct ps2_flash* flash) {
    // To-do: Flush to file

    free(flash);
}

void ps2_flash_reset(struct ps2_flash* flash) {
    flash->cmd = 0;
    flash->ctrl = FLASH_CTRL_READY;
    flash->counter = 0;
    flash->addrbyte = 0;
    flash->address = 0;

	memset(flash->data, 0xff, FLASH_PAGE_SIZE);

	flash_calculate_ecc(flash->data);
}

uint32_t flash_read_data(struct ps2_flash* flash, int size) {
    uint32_t value, refill;

    memcpy(&value, &flash->data[flash->counter], size);

    flash->counter += size;

    if (flash->cmd == SM_CMD_READ3) {
        if (flash->counter >= FLASH_PAGE_SIZE_ECC) {
            flash->counter = FLASH_PAGE_SIZE;

            refill = 1;
        }
    } else {
        if ((flash->ctrl & FLASH_CTRL_NOECC) && (flash->counter >= FLASH_PAGE_SIZE)) {
            flash->counter %= FLASH_PAGE_SIZE;

            refill = 1;
        } else if (!(flash->ctrl & FLASH_CTRL_NOECC) && (flash->counter >= FLASH_PAGE_SIZE_ECC)) {
            flash->counter %= FLASH_PAGE_SIZE_ECC;

            refill = 1;
        }
    }

    if (refill) {
        flash->address += FLASH_PAGE_SIZE;
        flash->address %= FLASH_CARD_SIZE;

        memcpy(flash->data, flash->file + (flash->address >> FLASH_PAGE_SIZE_BITS) * FLASH_PAGE_SIZE_ECC, FLASH_PAGE_SIZE);

        flash_calculate_ecc(flash->data); // calculate ECC; should be in the file already

        flash->ctrl |= FLASH_CTRL_READY;
    }

    return value;
}

uint32_t flash_read_id(struct ps2_flash* flash) {
    switch (flash->cmd) {
        case SM_CMD_READID: {
            return flash->id;
        } break;

        case SM_CMD_GETSTATUS: {
            return 0x80 | ((flash->ctrl & FLASH_CTRL_READY) ? 0x40 : 0x00);
        } break;
    }

    return 0;
}

void flash_write_data(struct ps2_flash* flash, int size, uint32_t data) {
    memcpy(&flash->data[flash->counter], &flash->data, size);

    flash->counter += size;
    flash->counter %= FLASH_PAGE_SIZE_ECC; //should not get past the last byte, but at the end
}

void flash_write_cmd(struct ps2_flash* flash, uint16_t value) {
    if (!(flash->ctrl & FLASH_CTRL_READY)) {
        if ((value != SM_CMD_GETSTATUS) && (value != SM_CMD_RESET)) {
            return;
        }
    } if (flash->cmd == SM_CMD_WRITEDATA) {
        if ((value != SM_CMD_PROGRAMPAGE) && (value != SM_CMD_RESET)) {
            flash->ctrl &= ~FLASH_CTRL_READY; //go busy, reset is needed
        }
    }

    switch (value) { // A8 bit is encoded in READ cmd;)
        case SM_CMD_READ1: {
            flash->counter = 0;

            if (flash->cmd != SM_CMD_GETSTATUS)
                flash->address = flash->counter;

            flash->addrbyte = 0;
        } break;

        case SM_CMD_READ2: {
            flash->counter = FLASH_PAGE_SIZE / 2;

            if (flash->cmd != SM_CMD_GETSTATUS)
                flash->address = flash->counter;

            flash->addrbyte = 0;
        } break;

        case SM_CMD_READ3: {
            flash->counter = FLASH_PAGE_SIZE;

            if (flash->cmd != SM_CMD_GETSTATUS)
                flash->address = flash->counter;

            flash->addrbyte = 0;
        } break;

        case SM_CMD_RESET: {
            ps2_flash_reset(flash);
        } break;

        case SM_CMD_WRITEDATA: {
            flash->counter = 0;
            flash->address = flash->counter;
            flash->addrbyte = 0;
        } break;

        case SM_CMD_ERASEBLOCK: {
            flash->counter = 0;

            memset(flash->data, 0xff, FLASH_PAGE_SIZE);

            flash->address = flash->counter;
            flash->addrbyte = 1;
        } break;
            
        case SM_CMD_PROGRAMPAGE: //fall
        case SM_CMD_ERASECONFIRM: {
            flash->ctrl &= ~FLASH_CTRL_READY;

            flash_calculate_ecc(flash->data);

            memcpy(flash->file + (flash->address / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE_ECC, flash->data, FLASH_PAGE_SIZE_ECC);

            /*write2file*/
            flash->ctrl |= FLASH_CTRL_READY;
        } break;

        case SM_CMD_GETSTATUS: break;
        case SM_CMD_READID: {
            flash->counter = 0;
            flash->address = flash->counter;
            flash->addrbyte = 0;
        } break;

        default: {
            flash->ctrl &= ~FLASH_CTRL_READY;

            return;
        } break;
    }

    flash->cmd = value;
}

void flash_write_addr(struct ps2_flash* flash, uint16_t value) {
    flash->address |= (value & 0xff) << (flash->addrbyte == 0 ? 0 : (1 + 8 * flash->addrbyte));
    flash->addrbyte++;

    if (!(value & 0x100)) { // address is complete
        if ((flash->cmd == SM_CMD_READ1) || (flash->cmd == SM_CMD_READ2) || (flash->cmd == SM_CMD_READ3)) {
            memcpy(flash->data, flash->file + (flash->address >> FLASH_PAGE_SIZE_BITS) * FLASH_PAGE_SIZE_ECC, FLASH_PAGE_SIZE);

            flash_calculate_ecc(flash->data); // calculate ECC; should be in the file already

            flash->ctrl |= FLASH_CTRL_READY;
        }

        flash->addrbyte = 0; // address reset
    }
}

void flash_write_ctrl(struct ps2_flash* flash, uint16_t value) {
    flash->ctrl = (flash->ctrl & FLASH_CTRL_READY) | (value & ~FLASH_CTRL_READY);
}

uint64_t ps2_flash_read16(struct ps2_flash* flash, uint32_t addr) {
    switch (addr) {
        case 0x4800: return flash_read_data(flash, 2);
        case 0x480c: return flash->ctrl;
        case 0x4814: return flash_read_id(flash);
    }

    printf("flash: Unknown 16-bit read at address %08x\n", addr);

    return 0;
}

uint64_t ps2_flash_read32(struct ps2_flash* flash, uint32_t addr) {
    switch (addr) {
        case 0x4800: return flash_read_data(flash, 4);
        case 0x480c: return flash->ctrl;
        case 0x4814: return flash_read_id(flash);
    }

    printf("flash: Unknown 16-bit read at address %08x\n", addr);

    return 0;
}

void ps2_flash_write16(struct ps2_flash* flash, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x4800: flash_write_data(flash, 2, data); return;
        case 0x4804: flash_write_cmd(flash, data); return;
        case 0x4808: flash_write_addr(flash, data); return;
        case 0x480c: flash_write_ctrl(flash, data); return;
    }

    printf("flash: Unknown 16-bit write at address %08x (%08lx)\n", addr, data);
}

void ps2_flash_write32(struct ps2_flash* flash, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x4800: flash_write_data(flash, 4, data); return;
        case 0x4804: flash_write_cmd(flash, data); return;
        case 0x4808: flash_write_addr(flash, data); return;
        case 0x480c: flash_write_ctrl(flash, data); return;
    }

    printf("flash: Unknown 32-bit write at address %08x (%08lx)\n", addr, data);
}