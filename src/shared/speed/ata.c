#include <string.h>
#include <stdio.h>

#include "ata.h"

#include "shared/ata/isif.h"
#include "shared/ata/raw.h"

#define printf(fmt, ...)(0)

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

void ata_create_identify(uint8_t* buf, uint64_t sectors);

void ata_init_security_data(struct ps2_ata* ata) {
    // Note: Taken from PCSX2
    memcpy(ata->sce_security_data, "Sony Computer Entertainment Inc.", 32); // Always this magic header.
    memcpy(ata->sce_security_data + 0x20, "SCPH-20401", 10); // sometimes this matches HDD model, the rest 6 bytes filles with zeroes, or sometimes with spaces
    memcpy(ata->sce_security_data + 0x30, " 120", 4); // or " 120" for PSX DESR, reference for ps2 area size. The rest bytes filled with zeroes

    ata->sce_security_data[0x40] = 0; // 0x40 - 0x43 - 4-byte HDD internal SCE serial, does not match real HDD serial, currently hardcoded to 0x1000000
    ata->sce_security_data[0x41] = 0;
    ata->sce_security_data[0x42] = 0;
    ata->sce_security_data[0x43] = 0x01;

    // purpose of next 12 bytes is unknown
    ata->sce_security_data[0x44] = 0; // always zero
    ata->sce_security_data[0x45] = 0; // always zero
    ata->sce_security_data[0x46] = 0x1a;
    ata->sce_security_data[0x47] = 0x01;
    ata->sce_security_data[0x48] = 0x02;
    ata->sce_security_data[0x49] = 0x20;
    ata->sce_security_data[0x4a] = 0; // always zero
    ata->sce_security_data[0x4b] = 0; // always zero
    // next 4 bytes always these values
    ata->sce_security_data[0x4c] = 0x01;
    ata->sce_security_data[0x4d] = 0x03;
    ata->sce_security_data[0x4e] = 0x11;
    ata->sce_security_data[0x4f] = 0x01;
}

void ps2_ata_init(struct ps2_ata* ata, struct ps2_speed* speed, struct sched_state* sched) {
    memset(ata, 0, sizeof(struct ps2_ata));

    ata->speed = speed;
    ata->sched = sched;

    // Note: See atad ata_device_probe
    ata->nsector = 1;
    ata->sector = 1;

    ata->status = ATA_STAT_READY | ATA_STAT_SEEK;

    ata_init_security_data(ata);
}

void ata_create_identify(uint8_t* buf, uint64_t sectors) {
	memset(buf, 0, ATA_SECTOR_SIZE);

    struct ata_identify* identify = (struct ata_identify*)buf;

	// Default CHS translation
	uint16_t default_cyls = (sectors > 16514064 ? 16514064 : sectors) / ATA_NUM_HEADS / ATA_SECTORS_PER_TRACK;
    uint16_t current_cyls = default_cyls;

    printf("ata: Creating IDENTIFY data for %lu sectors (CHS %d/%d/%d)\n", sectors, default_cyls, ATA_NUM_HEADS, ATA_SECTORS_PER_TRACK);

    identify->general_configuration = 0x0040; // Non-removable
    identify->num_cylinders = default_cyls;
    identify->specific_configuration = 0xc837; // taken from a real PS2 HDD
    identify->num_heads = ATA_NUM_HEADS;
    identify->bytes_per_track = ATA_SECTOR_SIZE * ATA_SECTORS_PER_TRACK;
    identify->bytes_per_sector = ATA_SECTOR_SIZE;
    identify->num_sectors_per_track = ATA_SECTORS_PER_TRACK;

    // Generate a fake serial number (20 ASCII characters)
    for (int i = 0; i < 19; i++) {
        identify->serial_number[i] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"[rand() % 36];
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
    identify->num_current_cylinders = current_cyls;
    identify->num_current_heads = ATA_NUM_HEADS;
    identify->num_current_sectors_per_track = ATA_SECTORS_PER_TRACK;
    identify->current_sector_capacity = current_cyls * ATA_NUM_HEADS * ATA_SECTORS_PER_TRACK;

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
    identify->feature_sets_active[1] = 0x3000;
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
    struct isif_state* isif = isif_open(path);

    if (!isif) {
        struct raw_state* raw = raw_open(path);

        if (!raw) {
            fprintf(stderr, "ata: Failed to open HDD image\n");

            return 0;
        }

        ata->hdd.udata = (void*)raw;

        ata->hdd.read_sector = ata_raw_read_sector;
        ata->hdd.write_sector = ata_raw_write_sector;
        ata->hdd.get_identify = ata_raw_get_identify;
        ata->hdd.get_sector_count = ata_raw_get_sector_count;
        ata->hdd.close = ata_raw_close;
    } else {
        ata->hdd.udata = (void*)isif;

        ata->hdd.read_sector = ata_isif_read_sector;
        ata->hdd.write_sector = ata_isif_write_sector;
        ata->hdd.get_identify = ata_isif_get_identify;
        ata->hdd.get_sector_count = ata_isif_get_sector_count;
        ata->hdd.close = ata_isif_close;
    }

    if (ata->hdd.get_identify(ata->hdd.udata, ata->identify) == 0) {
        fprintf(stderr, "ata: Failed to get identify data\n");

        // Create new identify data based on the size of the image
        // if the image doesn't provide valid identify data.
        ata_create_identify(ata->identify, ata->hdd.get_sector_count(ata->hdd.udata));
    }

    return 1;
}

void ps2_ata_destroy(struct ps2_ata* ata) {
    if (ata->hdd.close) {
        ata->hdd.close(ata->hdd.udata);
    }

    free(ata);
}

int ata_get_drive(struct ps2_ata* ata) {
    return (ata->select >> 4) & 1;
}

void ata_init_response(struct ps2_ata* ata, int size) {
    ata->status |= ATA_STAT_DRQ;

    ata->buf_index = 0;
    ata->buf_size = size;
    ata->buf = malloc(size);
}

void ata_handle_data_overflow(struct ps2_ata* ata) {
    // printf("ata: Data overflow (pending sectors %lu) command=%02x\n", ata->pending_sectors, ata->command);

    switch (ata->command) {
        case ATA_C_IDENTIFY_DEVICE: {
            ata->status &= ~ATA_STAT_DRQ;
        } break;

        case ATA_C_WRITE_DMA: {
            ata->hdd.write_sector(ata->hdd.udata, ata->pending_lba++, ata->buf);

            ata->pending_sectors--;

            if (ata->pending_sectors == 0) {
                ata->status &= ~ATA_STAT_DRQ;
            } else {
                ata->buf_index = 0;
                ata->buf_size = 512;
            }
        } break;

        case ATA_C_READ_DMA:
        case ATA_C_READ_SECTOR: {
            if (ata->pending_sectors == 0) {
                ata->status &= ~ATA_STAT_DRQ;

                return;
            }

            ata->hdd.read_sector(ata->hdd.udata, ata->pending_lba++, ata->buf);

            ata->buf_index = 0;
            ata->buf_size = 512;

            ata->pending_sectors--;
        } break;

        case ATA_C_SCE_SECURITY_CONTROL: {
            ata->status &= ~ATA_STAT_DRQ;
        } break;
    }
}

uint64_t ata_get_lba(struct ps2_ata* ata) {
    return ata->sector | (ata->lcyl << 8) | (ata->hcyl << 16) | ((ata->select & 0x0f) << 24);
}

uint64_t ata_get_nsectors(struct ps2_ata* ata) {
    if (ata->nsector == 0) {
        return 0x100;
    }

    return ata->nsector;
}

void ata_handle_command(struct ps2_ata* ata, uint16_t cmd) {
    switch (cmd) {
        case ATA_C_IDENTIFY_DEVICE: {
            printf("ata: IDENTIFY DEVICE\n");

            ata_init_response(ata, 512);

            memcpy(ata->buf, ata->identify, ATA_SECTOR_SIZE);
        } break;

        case ATA_C_READ_DMA: {
            printf("ata: READ DMA (LBA %d COUNT %d)\n", ata->sector, ata->nsector);

            ata->pending_sectors = ata_get_nsectors(ata) - 1;
            ata->pending_lba = ata_get_lba(ata);

            ata->status |= ATA_STAT_DRQ;
            ata->buf_size = 512;
            ata->buf_index = 0;

            ata->hdd.read_sector(ata->hdd.udata, ata->pending_lba++, ata->buf);
        } break;

        case ATA_C_WRITE_DMA: {
            printf("ata: WRITE DMA (LBA %d COUNT %d)\n", ata->sector, ata->nsector);

            ata->pending_sectors = ata_get_nsectors(ata);
            ata->pending_lba = ata_get_lba(ata);

            ata->status |= ATA_STAT_DRQ;
            ata->buf_size = 512;
            ata->buf_index = 0;
        } break;

        case ATA_C_READ_SECTOR: {
            printf("ata: READ SECTOR (LBA %d COUNT %d)\n", ata->sector, ata->nsector);

            ata_init_response(ata, 512);

            ata->pending_sectors = ata_get_nsectors(ata) - 1;
            ata->pending_lba = ata_get_lba(ata);

            ata->status |= ATA_STAT_DRQ;
            ata->buf_size = 512;
            ata->buf_index = 0;

            ata->hdd.read_sector(ata->hdd.udata, ata->pending_lba++, ata->buf);
        } break;

        case ATA_C_IDLE: {
            printf("ata: IDLE\n");
        } break;

        case ATA_C_SMART: {
            printf("ata: SMART command subcommand %d\n", ata->feature);
        } break;

        case ATA_C_FLUSH_CACHE: {
            printf("ata: FLUSH CACHE\n");
        } break;

        case ATA_C_SET_FEATURES: {
            printf("ata: SET FEATURES subcommand %d\n", ata->feature);
        } break;

        case ATA_C_SCE_SECURITY_CONTROL: {
            printf("ata: SCE SECURITY CONTROL command subcommand %02x\n", ata->feature);

            if (ata->feature == 0xec) {
                printf("ata: SCE SECURITY CONTROL - Get security data\n");

                ata_init_response(ata, 512);

                memcpy(ata->buf, ata->sce_security_data, 512);
            } break;
        } break;

        default: {
            printf("ata: Unhandled command %02x\n", cmd);

            // exit(1);
        } break;
    }
}

uint16_t ata_handle_data_read(struct ps2_ata* ata) {
    if (ata->buf_index >= ata->buf_size) {
        return 0;
    }

    uint16_t value = ata->buf[ata->buf_index] | (ata->buf[ata->buf_index + 1] << 8);

    // printf("ata: Data read %04x (index %d lba %08lx)\n", value, ata->buf_index, ata->pending_lba);

    ata->buf_index += 2;

    if (ata->buf_index >= ata->buf_size) {
        ata_handle_data_overflow(ata);
    }

    return value;
}

void ata_handle_data_write(struct ps2_ata* ata, uint16_t value) {
    if (ata->buf_index >= ata->buf_size) {
        return;
    }

    ata->buf[ata->buf_index] = value & 0xff;
    ata->buf[ata->buf_index + 1] = (value >> 8) & 0xff;

    ata->buf_index += 2;

    if (ata->buf_index >= ata->buf_size) {
        ata_handle_data_overflow(ata);
    }
}

uint16_t ata_read(struct ps2_ata* ata, uint32_t addr) {
    // printf("ata: Read %s (drive %d, status %02x, control %02x)\n", ata_get_register_name(addr, 0), ata_get_drive(ata), ata->status, ata->control);
    if (!ata->hdd.udata)
        return 0;

    // Only allow reads from the SELECT reg when slave is selected
    if (ata_get_drive(ata) && addr != 0x4c) return 0;

    switch (addr) {
        case 0x40: return ata_handle_data_read(ata);
        case 0x42: /* printf("ata: error read %04x\n", ata->error); */ return ata->error;
        case 0x44: /* printf("ata: nsector read %04x\n", ata->nsector); */ return ata->nsector;
        case 0x46: /* printf("ata: sector read %04x\n", ata->sector); */ return ata->sector;
        case 0x48: /* printf("ata: lcyl read %04x\n", ata->lcyl); */ return ata->lcyl;
        case 0x4a: /* printf("ata: hcyl read %04x\n", ata->hcyl); */ return ata->hcyl;
        case 0x4c: /* printf("ata: select read %04x\n", ata->select); */ return ata->select;

        // Note: This is the status reg offset, reading from this reg
        //       clears the interrupt flags
        //       Reg 5C reads the same status reg as 4e but without
        //       clearing the interrupt flags
        case 0x4e: { /* printf("ata: status read %04x\n", ata->status); */
            ata->speed->intr_stat &= SPD_INTR_ATA0;
            
            return ata->status;
        }

        case 0x5c: /* printf("ata: status alt read %04x\n", ata->status); */ return ata->status;
    }

    printf("ata: read from unknown register %08x\n", addr);

    return 0;
}

void ata_reset_busy_event(void* udata, int overshoot) {
    struct ps2_ata* ata = (struct ps2_ata*)udata;

    ata->status &= ~ATA_STAT_BUSY;

    ps2_speed_send_irq(ata->speed, SPD_INTR_ATA0);
}

void ata_write(struct ps2_ata* ata, uint32_t addr, uint64_t data) {
    if (!ata->hdd.udata)
        return;

    // printf("ata: Write %s %08lx (drive %d)\n", ata_get_register_name(addr, 1), data, ata_get_drive(ata));

    if (ata_get_drive(ata) && (addr != 0x4c && addr != 0x5c))
        return;

    switch (addr) {
        case 0x40: ata_handle_data_write(ata, data); return;
        case 0x42: ata->feature = data; return;
        case 0x44: ata->nsector = data; return;
        case 0x46: ata->sector = data; return;
        case 0x48: ata->lcyl = data; return;
        case 0x4a: ata->hcyl = data; return;
        case 0x4c: ata->select = data; return;
        case 0x4e: {
            ata->command = data;

            ata->status |= ATA_STAT_BUSY;

            struct sched_event event;

            event.callback = ata_reset_busy_event;
            event.udata = ata;
            event.name = "ata reset busy";
            event.cycles = 1000;

            sched_schedule(ata->sched, event);

            ata_handle_command(ata, ata->command);

            return;
        } break;
        case 0x005c: {
            if (data & 2 || data & 4) {
                printf("ata: Software reset\n");

                ata->status = ATA_STAT_READY | ATA_STAT_SEEK;
                ata->sector = 1;
                ata->nsector = 1;
            }

            return;
        } break;
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