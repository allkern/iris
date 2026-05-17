#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "acata.h"

struct s2x6_acata* s2x6_acata_create(void) {
    return malloc(sizeof(struct s2x6_acata));
}

static const uint16_t ATA_R_IDENTIFY_PACKET_DEVICE[256] = {
    0x8500,
    0,0,0,0,0,0,0,0,0,
    /* serial [10]*/
    0x2020,0x2020,0x2020,0x2020,0x2020,
    /* firmware [15]*/
    0x312E,0x3030,0x2020,0x2020,
    /* model [19]*/
    0x4E41,0x4D43,0x4F20,0x4456,//[22
    0x442D,0x524F,0x4D20,0x4452,//[26
    0x4956,0x4520,0x2020,0x2020,//[30
    0x2020,0x2020,0x2020,0x2020,//[34
    0x2020,0x2020,0x2020,0x2020,//[38
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //[50
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,// [61
    0x0007, //Word 62: Single-word DMA
    0x0007, //Word 63: Multiword DMA
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,//[80]
    0,0,0,0,0,0,0,//[87]
    0x0000,//Word 88 — Ultra DMA
};

void acata_reset_busy_event(void* udata, int overshoot) {
    struct s2x6_acata* acata = (struct s2x6_acata*)udata;

    acata->status &= ~ATA_STAT_BUSY;
}

void s2x6_acata_init(struct s2x6_acata* acata, struct ps2_iop_intc* intc, struct sched_state* sched) {
    memset(acata, 0, sizeof(struct s2x6_acata));

    acata->intc = intc;
    acata->sched = sched;

    acata->nsector = 1;
    acata->sector = 1;

    acata->status = ATA_STAT_READY | ATA_STAT_SEEK;

    acata->dvd.file = fopen("NM00007 SC21 DVD0D (DVD-ROM).iso", "rb");

    fseek(acata->dvd.file, 0, SEEK_END);
    acata->dvd.num_sectors = ftell(acata->dvd.file) / ATAPI_DVD_SECTOR_SIZE;
    fseek(acata->dvd.file, 0, SEEK_SET);
}

const char* acata_get_register_name(uint32_t addr, int rw) {
    switch (addr) {
        case 0: return "DATA";
        case 1: return rw ? "FEATURE" : "ERROR";
        case 2: return "NSECTOR";
        case 3: return "SECTOR";
        case 4: return "LCYL";
        case 5: return "HCYL";
        case 6: return "SELECT";
        case 7: return rw ? "COMMAND" : "STATUS";
        case 22: return "CONTROL";
    }

    return "UNKNOWN";
}

void acata_create_identify(uint8_t* buf, uint64_t sectors) {
	memset(buf, 0, ATA_SECTOR_SIZE);

    struct ata_identify* identify = (struct ata_identify*)buf;

	// Default CHS translation
	uint16_t default_cyls = (sectors > 16514064 ? 16514064 : sectors) / ATA_NUM_HEADS / ATA_SECTORS_PER_TRACK;
    uint16_t current_cyls = default_cyls;

    printf("acata: Creating IDENTIFY data for %lu sectors (CHS %d/%d/%d)\n", sectors, default_cyls, ATA_NUM_HEADS, ATA_SECTORS_PER_TRACK);

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

int s2x6_acata_load(struct s2x6_acata* acata, const char* path) {
    struct isif_state* isif = isif_open(path);

    if (!isif) {
        struct raw_state* raw = raw_open(path);

        if (!raw) {
            fprintf(stderr, "acata: Failed to open HDD image\n");

            return 0;
        }

        acata->hdd.udata = (void*)raw;

        acata->hdd.read_sector = ata_raw_read_sector;
        acata->hdd.write_sector = ata_raw_write_sector;
        acata->hdd.get_identify = ata_raw_get_identify;
        acata->hdd.get_sector_count = ata_raw_get_sector_count;
        acata->hdd.close = ata_raw_close;
    } else {
        acata->hdd.udata = (void*)isif;

        acata->hdd.read_sector = ata_isif_read_sector;
        acata->hdd.write_sector = ata_isif_write_sector;
        acata->hdd.get_identify = ata_isif_get_identify;
        acata->hdd.get_sector_count = ata_isif_get_sector_count;
        acata->hdd.close = ata_isif_close;
    }

    if (acata->hdd.get_identify(acata->hdd.udata, acata->identify) == 0) {
        fprintf(stderr, "acata: Failed to get identify data\n");

        // Create new identify data based on the size of the image
        // if the image doesn't provide valid identify data.
        acata_create_identify(acata->identify, acata->hdd.get_sector_count(acata->hdd.udata));
    }

    return 1;
}

void s2x6_acata_destroy(struct s2x6_acata* acata) {
    if (acata->hdd.close) {
        acata->hdd.close(acata->hdd.udata);
    }

    free(acata);
}

int acata_get_drive(struct s2x6_acata* acata) {
    return (acata->select >> 4) & 1;
}

void acata_init_response(struct s2x6_acata* acata, int size) {
    acata->status |= ATA_STAT_DRQ;

    acata->buf_index = 0;
    acata->buf_size = size;
    acata->buf = malloc(size);

    memset(acata->buf, 0, acata->buf_size);
}

void atapi_packet_response_event(void* udata, int cycles) {
    struct s2x6_acata* acata = (struct s2x6_acata*)udata;

    // printf("acata: ATAPI packet response event\n");

    ps2_iop_intc_irq(acata->intc, IOP_INTC_DEV9);
}

struct atapi_packet atapi_process_packet(struct s2x6_acata* acata) {
    struct atapi_packet packet;

    packet.cmd = acata->buf[0];
    packet.lba = (acata->buf[2] << 24) | (acata->buf[3] << 16) | (acata->buf[4] << 8) | acata->buf[5];
    packet.len = (acata->buf[7] << 8) | acata->buf[8];

    return packet;
}

void atapi_read_dvd(struct atapi_dvd* dvd, uint64_t lba, uint64_t count, uint8_t* buf) {
    fseek(dvd->file, lba * ATAPI_DVD_SECTOR_SIZE, SEEK_SET);
    fread(buf, 1, count * ATAPI_DVD_SECTOR_SIZE, dvd->file);
}

void atapi_handle_command(struct s2x6_acata* acata, struct atapi_packet* packet) {
    // printf("acata: Handling ATAPI packet command %02x\n", packet->cmd);

    switch (packet->cmd) {
        // TEST UNIT READY
        case 0x00: {
            printf("acata: ATAPI TEST UNIT READY\n");
        } break;

        // READ
        case 0x28: {
            printf("acata: ATAPI READ (LBA %d COUNT %d)\n", packet->lba, packet->len);

            if (packet->len == 0) {
                printf("acata: ATAPI READ with length 0, treating as 65536\n");

                exit(1);
            }

            acata->atapi_response = 1;

            acata_init_response(acata, packet->len * ATAPI_DVD_SECTOR_SIZE);

            atapi_read_dvd(&acata->dvd, packet->lba, packet->len, acata->buf);
        } break;

        default: {
            printf("acata: Unhandled ATAPI packet command %02x\n", packet->cmd);
        } break;
    }
}

void acata_handle_data_overflow(struct s2x6_acata* acata) {
    // printf("acata: Data overflow (pending sectors %lu) command=%02x\n", acata->pending_sectors, acata->command);

    switch (acata->command) {
        case ATA_C_IDENTIFY_PACKET_DEVICE:
        case ATA_C_IDENTIFY_DEVICE: {
            acata->status &= ~ATA_STAT_DRQ;
            printf("acata: DRQ cleared\n");
        } break;

        case ATA_C_WRITE_DMA: {
            acata->hdd.write_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);

            acata->pending_sectors--;

            if (acata->pending_sectors == 0) {
                acata->status &= ~ATA_STAT_DRQ;
                printf("acata: DRQ cleared\n");
            } else {
                acata->buf_index = 0;
                acata->buf_size = 512;
            }
        } break;

        case ATA_C_READ_DMA:
        case ATA_C_READ_SECTOR: {
            if (acata->pending_sectors == 0) {
                acata->status &= ~ATA_STAT_DRQ;
                printf("acata: DRQ cleared\n");

                return;
            }

            acata->hdd.read_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);

            acata->buf_index = 0;
            acata->buf_size = 512;

            acata->pending_sectors--;
        } break;

        case ATA_C_PACKET: {
            acata->status &= ~ATA_STAT_DRQ;
            acata->status |= ATA_STAT_READY;
            printf("acata: DRQ cleared\n");

            if (acata->atapi_response) {
                acata->atapi_response = 0;

                return;
            }

            struct atapi_packet packet = atapi_process_packet(acata);

            // printf("acata: ATAPI packet command %02x lba %08x len %08x\n", packet.cmd, packet.lba, packet.len);

            atapi_handle_command(acata, &packet);

            struct sched_event event;

            event.callback = atapi_packet_response_event;
            event.cycles = 1000;
            event.name = "ATAPI packet response";
            event.udata = acata;

            sched_schedule(acata->sched, event);
        } break;

        case ATA_C_SCE_SECURITY_CONTROL: {
            acata->status &= ~ATA_STAT_DRQ;
            printf("acata: DRQ cleared\n");
        } break;
    }
}

uint64_t acata_get_lba(struct s2x6_acata* acata) {
    return acata->sector | (acata->lcyl << 8) | (acata->hcyl << 16) | ((acata->select & 0x0f) << 24);
}

uint64_t acata_get_nsectors(struct s2x6_acata* acata) {
    if (acata->nsector == 0) {
        return 0x100;
    }

    return acata->nsector;
}

void acata_handle_command(struct s2x6_acata* acata, uint16_t cmd) {
    switch (cmd) {
        case ATA_C_NOP: {
            printf("acata: NOP\n");
        } break;

        case ATA_C_IDENTIFY_DEVICE: {
            printf("acata: IDENTIFY DEVICE\n");

            acata_init_response(acata, 512);

            memcpy(acata->buf, acata->identify, ATA_SECTOR_SIZE);
        } break;

        case ATA_C_READ_DMA: {
            printf("acata: READ DMA (LBA %d COUNT %d)\n", acata->sector, acata->nsector);

            acata->pending_sectors = acata_get_nsectors(acata) - 1;
            acata->pending_lba = acata_get_lba(acata);

            acata->status |= ATA_STAT_DRQ;
            acata->buf_size = 512;
            acata->buf_index = 0;

            acata->hdd.read_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);
        } break;

        case ATA_C_WRITE_DMA: {
            printf("acata: WRITE DMA (LBA %d COUNT %d)\n", acata->sector, acata->nsector);

            acata->pending_sectors = acata_get_nsectors(acata);
            acata->pending_lba = acata_get_lba(acata);

            acata->status |= ATA_STAT_DRQ;
            acata->buf_size = 512;
            acata->buf_index = 0;
        } break;

        case ATA_C_READ_SECTOR: {
            printf("acata: READ SECTOR (LBA %d COUNT %d)\n", acata->sector, acata->nsector);

            acata_init_response(acata, 512);

            acata->pending_sectors = acata_get_nsectors(acata) - 1;
            acata->pending_lba = acata_get_lba(acata);

            acata->status |= ATA_STAT_DRQ;
            acata->buf_size = 512;
            acata->buf_index = 0;

            acata->hdd.read_sector(acata->hdd.udata, acata->pending_lba++, acata->buf);
        } break;

        case ATA_C_IDLE: {
            printf("acata: IDLE\n");
        } break;

        case ATA_C_SMART: {
            printf("acata: SMART command subcommand %d\n", acata->feature);
        } break;

        case ATA_C_FLUSH_CACHE: {
            printf("acata: FLUSH CACHE\n");
        } break;

        case ATA_C_SET_FEATURES: {
            printf("acata: SET FEATURES subcommand %d\n", acata->feature);

            return;
        } break;

        case ATA_C_PACKET: {
            printf("acata: PACKET\n");

            acata->status |= ATA_STAT_DRQ;
            acata->buf_size = 12;
            acata->buf_index = 0;
        } break;

        case ATA_C_IDENTIFY_PACKET_DEVICE: {
            printf("acata: IDENTIFY PACKET DEVICE\n");

            acata_init_response(acata, 512);

            memcpy(acata->buf, ATA_R_IDENTIFY_PACKET_DEVICE, ATA_SECTOR_SIZE);
        } break;

        case ATA_C_SCE_SECURITY_CONTROL: {
            printf("acata: SCE SECURITY CONTROL command subcommand %02x\n", acata->feature);

            // if (acata->feature == 0xec) {
            //     printf("acata: SCE SECURITY CONTROL - Get security data\n");

            //     acata_init_response(acata, 512);

            //     memcpy(acata->buf, acata->sce_security_data, 512);
            // } break;
        } break;

        default: {
            printf("acata: Unhandled command %02x\n", cmd);

            exit(1);
        } break;
    }

    acata->status |= ATA_STAT_BUSY;

    struct sched_event event;

    event.callback = acata_reset_busy_event;
    event.udata = acata;
    event.name = "acata reset busy";
    event.cycles = 1000;

    sched_schedule(acata->sched, event);
}

uint16_t acata_handle_data_read(struct s2x6_acata* acata) {
     if (acata->buf_index >= acata->buf_size) {
        return 0;
    }

    uint16_t value = acata->buf[acata->buf_index] | (acata->buf[acata->buf_index + 1] << 8);

    // printf("acata: Data read %04x (index %d lba %08lx)\n", value, acata->buf_index, acata->pending_lba);

    acata->buf_index += 2;

    // printf("acata: Data read %04x (index %d lba %08lx)\n", value, acata->buf_index, acata->pending_lba);

    if (acata->buf_index >= acata->buf_size) {
        acata_handle_data_overflow(acata);
    }

    return value;
}

void acata_handle_data_write(struct s2x6_acata* acata, uint16_t value) {
    if (acata->buf_index >= acata->buf_size) {
        return;
    }

    acata->buf[acata->buf_index] = value & 0xff;
    acata->buf[acata->buf_index + 1] = (value >> 8) & 0xff;

    acata->buf_index += 2;

    if (acata->buf_index >= acata->buf_size) {
        acata_handle_data_overflow(acata);
    }
}

uint16_t acata_read(struct s2x6_acata* acata, uint32_t addr) {
    printf("acata: Read %s (drive %d, status %02x, control %02x)\n", acata_get_register_name(addr, 0), acata_get_drive(acata), acata->status, acata->control);

    // if (!acata->hdd.udata)
    //     return 0;

    // Only allow reads from the SELECT reg when slave is selected
    if (acata_get_drive(acata) && addr != 6) return 0;

    switch (addr) {
        case 0: return acata_handle_data_read(acata);
        case 1: /* printf("acata: error read %04x\n", acata->error); */ return acata->error;
        case 2: /* printf("acata: nsector read %04x\n", acata->nsector); */ return acata->nsector & 0xff;
        case 3: /* printf("acata: sector read %04x\n", acata->sector); */ return acata->sector;
        case 4: /* printf("acata: lcyl read %04x\n", acata->lcyl); */ return acata->lcyl;
        case 5: /* printf("acata: hcyl read %04x\n", acata->hcyl); */ return acata->hcyl;
        case 6: /* printf("acata: select read %04x\n", acata->select); */ return acata->select;

        // Note: This is the status reg offset, reading from this reg
        //       clears the interrupt flags
        //       Reg 5C reads the same status reg as 4e but without
        //       clearing the interrupt flags
        case 7: { /* printf("acata: status read %04x\n", acata->status); */
            // acata->speed->intr_stat &= SPD_INTR_ATA0;
            
            return acata->status;
        }

        case 22: /* printf("acata: status alt read %04x\n", acata->status); */ return acata->status;
    }

    printf("acata: read from unknown register %08x\n", addr);

    return 0;
}

void acata_write(struct s2x6_acata* acata, uint32_t addr, uint64_t data) {
    // if (!acata->hdd.udata)
    //     return;

    // printf("acata: Write %s %08lx (drive %d)\n", acata_get_register_name(addr, 1), data, acata_get_drive(acata));

    if (acata_get_drive(acata) && (addr != 6 && addr != 22))
        return;

    switch (addr) {
        case 0: acata_handle_data_write(acata, data); return;
        case 1: acata->feature = data; return;
        case 2: acata->nsector = data & 0xff; return;
        case 3: acata->sector = data; return;
        case 4: acata->lcyl = data; return;
        case 5: acata->hcyl = data; return;
        case 6: acata->select = data; return;
        case 7: {
            acata->command = data;

            acata_handle_command(acata, acata->command);

            return;
        } break;
        case 22: {
            if (data & 2 || data & 4) {
                printf("acata: Software reset\n");

                acata->status = ATA_STAT_READY | ATA_STAT_SEEK;
                acata->sector = 1;
                acata->nsector = 1;
            }

            return;
        } break;
    }

    printf("acata: write to unknown register %08x\n", addr);
}

uint64_t s2x6_acata_read16(struct s2x6_acata* acata, uint32_t addr) {
    return acata_read(acata, (addr >> 16) & 0x3f);
}

void s2x6_acata_write16(struct s2x6_acata* acata, uint32_t addr, uint64_t data) {
    acata_write(acata, (addr >> 16) & 0x3f, data);
}