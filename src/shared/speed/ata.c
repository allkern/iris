#include <string.h>
#include <stdio.h>

#include "ata.h"

struct ps2_ata* ps2_ata_create(void) {
    return malloc(sizeof(struct ps2_ata));
}

const char* ata_get_register_name(uint32_t addr, int rw) {
    switch (addr) {
        case 0x0040: return "DATA";
        case 0x0042: return rw ? "FEATURE" : "ERROR";
        case 0x0044: return "NSECTOR";
        case 0x0046: return "SECTOR";
        case 0x0048: return "LCYL";
        case 0x004a: return "HCYL";
        case 0x004c: return "SELECT";
        case 0x004e: return rw ? "COMMAND" : "STATUS";
        case 0x005c: return "CONTROL";
    }

    return "UNKNOWN";
}

void ps2_ata_init(struct ps2_ata* ata, struct ps2_speed* speed) {
    memset(ata, 0, sizeof(struct ps2_ata));

    ata->speed = speed;

    // Note: See atad ata_device_probe
    ata->nsector = 1;
    ata->sector = 1;

    ata->status = 0x40; // RDY | DRQ
    ata->control = 0x40;
}

void ata_create_identify(uint8_t* buf, uint64_t sectors) {
	memset(buf, 0, ATA_SECTOR_SIZE);

    struct ata_identify* identify = (struct ata_identify*)buf;

	// Default CHS translation
    const uint16_t heads = 16;
    const uint16_t sectors_per_track = 63;

	uint16_t cyls = sectors / heads / sectors_per_track;

    identify->general_configuration = 0x0040; // Non-removable
    identify->num_cylinders = cyls;
    identify->specific_configuration = 0xc837; // taken from a real PS2 HDD
    identify->num_heads = ATA_NUM_HEADS;
    identify->bytes_per_track = ATA_SECTOR_SIZE * ATA_SECTORS_PER_TRACK;
    identify->bytes_per_sector = ATA_SECTOR_SIZE;
    identify->num_sectors_per_track = ATA_SECTORS_PER_TRACK;

    // Generate a fake serial number (20 ASCII characters)
    for (int i = 0; i < 19; i++) {
        identify->serial_number[i] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"[i % 36];
    }

    identify->serial_number[19] = '\0';

    strncpy(identify->firmware_revision, "FIRM100", 8);
    strncpy(identify->model_number, "Iris ATA HDD", 40);

    identify->max_block_transfer = 0x8080;

    // IORDY supported, LBA supported, DMA supported
    identify->capabilities = 0x40000b00;
    identify->obsolete_words51[0] = ATA_PIO_MODE << 8;

    // CHS, PIO/MWDMA, UDMA fields valid
    identify->translation_fields_free_fall = 7;
    identify->num_current_cylinders = cyls;
    identify->num_current_heads = ATA_NUM_HEADS;
    identify->num_current_sectors_per_track = ATA_SECTORS_PER_TRACK;
    identify->current_sector_capacity = cyls * ATA_NUM_HEADS * ATA_SECTORS_PER_TRACK;

    // Multi-sector setting is valid
    identify->multi_sector_capabilities = 0x0080;
    identify->user_addressable_sectors = sectors;

    // MWDMA modes supported (0,1,2) and active mode (2)
    identify->mwdma_support_active = 0x0407; // or 0x0007

    // PIO 3,4 supported
    identify->pio_support_active = 0x0003;

    identify->minimum_mw_xfer_cycle_time = 120;
    identify->recommended_mw_xfer_cycle_time = 120;
    identify->minimum_pio_cycle_time = 120;
    identify->minimum_pio_cycle_time_iordy = 120;

    identify->major_revision = 0x0070;
    identify->minor_revision = 0x0018;

    // SMART, Write cache, NOP, FLUSH CACHE, FLUSH CACHE EXT
    // SMART error logging
    // SMART self-test
    identify->feature_sets_supported[0] = 0x4021;
    identify->feature_sets_supported[1] = 0x7000;
    identify->feature_sets_supported[2] = 0x0003;
    identify->feature_sets_active[0] = 0x4021;
    identify->feature_sets_active[1] = 0x7000;
    identify->feature_sets_active[2] = 0x0003;

    identify->udma_support_active = 0x007f;

    // if (lba48_supported)
    //    identify->feature_sets_supported |= (1 << 26); // LBA48

    // Drive 0 passed diagnostic
    identify->hardware_reset_result = 0x4009;

    // if (lba48_supported) {
    //     identify->max_48bit_lba[0] = sectors & 0xffffffff;
    //     identify->max_48bit_lba[1] = (sectors >> 32) & 0xffffffff;
    // }

    identify->physical_logical_sector_size = 0x4000;

    identify->signature = 0xa5;
    identify->checksum = 0;

    for (int i = 0; i < 511; i++) {
        identify->checksum += ((uint8_t*)buf)[i];
    }
}

int ps2_ata_load(struct ps2_ata* ata, const char* path) {
    // To-do: Load HDD image

    // A standard 40GB disk would have 7812500 sectors
    ata_create_identify(ata->identify, 7812500);

    return 1;
}

void ps2_ata_destroy(struct ps2_ata* ata) {
    free(ata);
}

int ata_get_drive(struct ps2_ata* ata) {
    return (ata->select >> 4) & 1;
}

void ata_init_response(struct ps2_ata* ata, int size) {
    ata->status = 0x48; // RDY | DRQ
    ata->control = 0x40;

    ata->buf_index = 0;
    ata->buf_size = size;
    ata->buf = malloc(size);
}

void ata_handle_command(struct ps2_ata* ata, uint16_t cmd) {
    switch (cmd) {
        case ATA_C_IDENTIFY_DEVICE: {
            printf("ata: IDENTIFY DEVICE\n");

            ata_init_response(ata, 512);

            ata_create_identify(ata->identify, 7812500);

            memcpy(ata->buf, ata->identify, ATA_SECTOR_SIZE);
        } break;
    }
}

uint16_t ata_handle_data_read(struct ps2_ata* ata) {
    if (ata->buf_index >= ata->buf_size) {
        return 0;
    }

    uint16_t value = ata->buf[ata->buf_index] | (ata->buf[ata->buf_index + 1] << 8);

    ata->buf_index += 2;

    printf("ata: Read data %04x\n", value);

    return value;
}

uint16_t ata_read(struct ps2_ata* ata, uint32_t addr) {
    printf("ata: Read %s (drive %d, status %02x, control %02x)\n", ata_get_register_name(addr, 0), ata_get_drive(ata), ata->status, ata->control);

    // Only allow reads from the SELECT reg when slave is selected
    if (ata_get_drive(ata) && addr != 0x4c) return 0;

    switch (addr) {
        case 0x0040: return ata_handle_data_read(ata);
        case 0x0042: return ata->error;
        case 0x0044: return ata->nsector;
        case 0x0046: return ata->sector;
        case 0x0048: return ata->lcyl;
        case 0x004a: return ata->hcyl;
        case 0x004c: return ata->select;
        case 0x004e: printf("ata: Reading status register %02x\n", ata->status); return ata->status;
        case 0x005c: printf("ata: Reading control register %02x\n", ata->control); return ata->control;
    }

    printf("ata: read from unknown register %08x\n", addr);

    return 0;
}

void ata_write(struct ps2_ata* ata, uint32_t addr, uint64_t data) {
    printf("ata: Write %s %08lx (drive %d)\n", ata_get_register_name(addr, 1), data, ata_get_drive(ata));

    if (ata_get_drive(ata) && (addr != 0x4c && addr != 0x5c))
        return;

    switch (addr) {
        // case 0x0040: ata->data = data; return;
        // case 0x0042: ata->feature = data; return;
        // case 0x0044: ata->nsector = data; return;
        // case 0x0046: ata->sector = data; return;
        case 0x0048: ata->lcyl = data; return;
        case 0x004a: ata->hcyl = data; return;
        case 0x004c: ata->select = data; return;
        case 0x004e: {
            ata_handle_command(ata, data);

            ps2_speed_send_irq(ata->speed, SPD_INTR_ATA0);

            return;
        } break;
        // case 0x005c: ata->control = data; return;
    }

    printf("ata: write to unknown register %08x\n", addr);
}

uint64_t ps2_ata_read16(struct ps2_ata* ata, uint32_t addr) {
    return ata_read(ata, addr);
}

uint64_t ps2_ata_read32(struct ps2_ata* ata, uint32_t addr) {
    return ata_read(ata, addr);
}

void ps2_ata_write16(struct ps2_ata* ata, uint32_t addr, uint64_t data) {
    ata_write(ata, addr, data);
}

void ps2_ata_write32(struct ps2_ata* ata, uint32_t addr, uint64_t data) {
    ata_write(ata, addr, data);
}