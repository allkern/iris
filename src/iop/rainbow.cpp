/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <new>
#include <cstring>
#include <algorithm>

#include "rainbow.hpp"

namespace iris::rainbow {

static const char* const g_vendor = "SONY    ";
static const char* const g_product = "DVD+-RW DW-DX02A";
static const char* const g_revision = "1.00";

static inline void update_irq(Rainbow* rainbow) {
    int line = (rainbow->intr_stat & rainbow->intr_mask) != 0;

    if (line && !rainbow->irq_line) {
        iop::intc::irq(rainbow->hw.intc, iop::intc::CDVD);
    }

    rainbow->irq_line = line;
}

static inline void assert_irq(Rainbow* rainbow) {
    if (rainbow->control & CONTROL_NIEN)
        return;

    rainbow->intr_stat |= INTR_ATA0;

    update_irq(rainbow);
}

static inline void clear_irq(Rainbow* rainbow) {
    rainbow->intr_stat &= ~INTR_ATA0;

    update_irq(rainbow);
}

static void set_signature(Rainbow* rainbow) {
    rainbow->error = 0x01;
    rainbow->nsector = 0x01;
    rainbow->sector = 0x01;
    rainbow->lcyl = 0x14;
    rainbow->hcyl = 0xeb;
    rainbow->select &= 0xb0;
    rainbow->status = 0x00;
    rainbow->phase = PHASE_IDLE;
    rainbow->buf_size = 0;
    rainbow->buf_index = 0;
}

static inline int device1_selected(Rainbow* rainbow) {
    return (rainbow->select >> 4) & 1;
}

static inline bool media_present(Rainbow* rainbow) {
    cdvd::Cdvd* cdvd = rainbow->hw.cdvd;

    return cdvd->disc && !(cdvd->status & cdvd::STATUS_TRAY_OPEN_BIT);
}

static inline bool media_is_dvd(Rainbow* rainbow) {
    int type = rainbow->hw.cdvd->detected_disc_type;

    return type == iop::disc::CDVD_DISC_PS2_DVD || type == iop::disc::CDVD_DISC_DVD_VIDEO;
}

static void check_media_change(Rainbow* rainbow) {
    void* disc = rainbow->hw.cdvd->disc;

    if (disc != rainbow->last_disc) {
        rainbow->last_disc = disc;
        rainbow->unit_attention = 1;
        rainbow->media_event = disc ? 2 : 3;
    }
}

static void complete_event(void* udata, int overshoot) {
    Rainbow* rainbow = (Rainbow*)udata;

    if (rainbow->sense_key) {
        rainbow->status = STATUS_DRDY | STATUS_DSC | STATUS_ERR;
        rainbow->error = (rainbow->sense_key << 4) | ERROR_ABRT;
    } else {
        rainbow->status = STATUS_DRDY | STATUS_DSC;
        rainbow->error = 0;
    }

    rainbow->nsector = IREASON_IO | IREASON_COD;
    rainbow->phase = PHASE_IDLE;

    assert_irq(rainbow);
}

static void schedule_complete(Rainbow* rainbow) {
    rainbow->status = STATUS_BSY;

    scheduler::Event event;

    event.callback = complete_event;
    event.udata = rainbow;
    event.cycles = 5000;
    event.name = "rainbow complete";

    scheduler::schedule(rainbow->hw.sched, event);
}

static void start_chunk(Rainbow* rainbow) {
    uint32_t remaining = rainbow->buf_size - rainbow->buf_index;
    uint32_t chunk = std::min(remaining, rainbow->byte_limit);

    rainbow->chunk_end = rainbow->buf_index + chunk;
    rainbow->lcyl = chunk & 0xff;
    rainbow->hcyl = (chunk >> 8) & 0xff;
    rainbow->nsector = IREASON_IO;
    rainbow->status = STATUS_DRDY | STATUS_DSC | STATUS_DRQ;
    rainbow->phase = PHASE_ATAPI_DATA_IN;

    assert_irq(rainbow);
}

static void data_in_event(void* udata, int overshoot) {
    start_chunk((Rainbow*)udata);
}

static void schedule_data_in(Rainbow* rainbow, uint32_t size) {
    rainbow->buf_size = size;
    rainbow->buf_index = 0;
    rainbow->status = STATUS_BSY;

    if (!size) {
        schedule_complete(rainbow);

        return;
    }

    scheduler::Event event;

    event.callback = data_in_event;
    event.udata = rainbow;
    event.cycles = 5000;
    event.name = "rainbow data in";

    scheduler::schedule(rainbow->hw.sched, event);
}

static void set_sense(Rainbow* rainbow, uint8_t key, uint8_t asc, uint8_t ascq) {
    rainbow->sense_key = key;
    rainbow->asc = asc;
    rainbow->ascq = ascq;
}

static void fail(Rainbow* rainbow, uint8_t key, uint8_t asc, uint8_t ascq) {
    set_sense(rainbow, key, asc, ascq);
    schedule_complete(rainbow);
}

static inline void put_string(uint8_t* dst, const char* src, size_t size) {
    size_t len = strlen(src);

    for (size_t i = 0; i < size; i++) {
        dst[i] = i < len ? src[i] : ' ';
    }
}

static inline uint32_t alloc_length_8(const uint8_t* pkt) {
    return pkt[4];
}

static inline uint32_t alloc_length_16(const uint8_t* pkt) {
    return (pkt[7] << 8) | pkt[8];
}

static bool check_ready(Rainbow* rainbow) {
    check_media_change(rainbow);

    if (!media_present(rainbow)) {
        fail(rainbow, SENSE_NOT_READY, 0x3a, 0x00);

        return false;
    }

    if (rainbow->unit_attention) {
        rainbow->unit_attention = 0;

        fail(rainbow, SENSE_UNIT_ATTENTION, 0x28, 0x00);

        return false;
    }

    return true;
}

static void cmd_test_unit_ready(Rainbow* rainbow) {
    if (!check_ready(rainbow))
        return;

    schedule_complete(rainbow);
}

static void cmd_request_sense(Rainbow* rainbow) {
    uint32_t size = alloc_length_8(rainbow->packet);

    memset(rainbow->buf, 0, size);

    uint8_t data[18] = {};

    data[0] = 0x70;
    data[2] = rainbow->sense_key;
    data[7] = 10;
    data[12] = rainbow->asc;
    data[13] = rainbow->ascq;

    memcpy(rainbow->buf, data, std::min<uint32_t>(size, sizeof(data)));

    set_sense(rainbow, SENSE_NONE, 0, 0);
    schedule_data_in(rainbow, size);
}

static void cmd_inquiry(Rainbow* rainbow) {
    uint32_t size = alloc_length_8(rainbow->packet);

    memset(rainbow->buf, 0, std::max<uint32_t>(size, 56));

    rainbow->buf[0] = 0x05;
    rainbow->buf[1] = 0x80;
    rainbow->buf[2] = 0x00;
    rainbow->buf[3] = 0x32;
    rainbow->buf[4] = 51;

    put_string(&rainbow->buf[8], g_vendor, 8);
    put_string(&rainbow->buf[16], g_product, 16);
    put_string(&rainbow->buf[32], g_revision, 4);

    set_sense(rainbow, SENSE_NONE, 0, 0);
    schedule_data_in(rainbow, size);
}

static void cmd_read_capacity(Rainbow* rainbow) {
    if (!check_ready(rainbow))
        return;

    uint64_t sectors = iop::disc::get_size(rainbow->hw.cdvd->disc) / 2048;
    uint32_t last = sectors ? (uint32_t)(sectors - 1) : 0;

    rainbow->buf[0] = (last >> 24) & 0xff;
    rainbow->buf[1] = (last >> 16) & 0xff;
    rainbow->buf[2] = (last >> 8) & 0xff;
    rainbow->buf[3] = last & 0xff;
    rainbow->buf[4] = 0x00;
    rainbow->buf[5] = 0x00;
    rainbow->buf[6] = 0x08;
    rainbow->buf[7] = 0x00;

    schedule_data_in(rainbow, 8);
}

static void cmd_read10(Rainbow* rainbow) {
    if (!check_ready(rainbow))
        return;

    const uint8_t* pkt = rainbow->packet;

    uint32_t lba = (pkt[2] << 24) | (pkt[3] << 16) | (pkt[4] << 8) | pkt[5];
    uint32_t count = (pkt[7] << 8) | pkt[8];

    if (count * 2048 > sizeof(rainbow->buf)) {
        iris_warning(rainbow, "READ(10) of {} sectors is too large", count);

        fail(rainbow, SENSE_ILLEGAL_REQUEST, 0x24, 0x00);

        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        iop::disc::read_sector(rainbow->hw.cdvd->disc, &rainbow->buf[i * 2048], lba + i, 2048);
    }

    iris_debug(rainbow, "READ(10) lba {:x} count {} dma {}", lba, count, rainbow->feature & 1);

    if (rainbow->feature & 1) {
        rainbow->buf_size = count * 2048;
        rainbow->buf_index = 0;
        rainbow->dma_active = count != 0;
        rainbow->status = STATUS_DRDY | STATUS_DSC | STATUS_DRQ;

        if (!count) {
            schedule_complete(rainbow);
        }

        return;
    }

    schedule_data_in(rainbow, count * 2048);
}

static void cmd_read_disc_info(Rainbow* rainbow) {
    if (!check_ready(rainbow))
        return;

    uint32_t size = alloc_length_16(rainbow->packet);
    uint8_t data[34] = {};

    data[1] = 32;
    data[2] = 0x0e;
    data[3] = 1;
    data[4] = 1;
    data[5] = 1;
    data[6] = 1;
    data[7] = 0x20;
    data[20] = 0xff;
    data[21] = 0xff;
    data[22] = 0xff;
    data[23] = 0xff;

    memset(rainbow->buf, 0, std::max<uint32_t>(size, sizeof(data)));
    memcpy(rainbow->buf, data, std::min<uint32_t>(size, sizeof(data)));

    schedule_data_in(rainbow, size);
}

static void cmd_get_event_status(Rainbow* rainbow) {
    const uint8_t* pkt = rainbow->packet;
    uint32_t size = alloc_length_16(pkt);

    check_media_change(rainbow);

    memset(rainbow->buf, 0, std::max<uint32_t>(size, 8));

    if (!(pkt[1] & 1) || !(pkt[4] & 0x10)) {
        rainbow->buf[1] = 2;
        rainbow->buf[2] = 0x80;
        rainbow->buf[3] = 0x10;

        set_sense(rainbow, SENSE_NONE, 0, 0);
        schedule_data_in(rainbow, std::min<uint32_t>(size, 4));

        return;
    }

    bool tray_open = rainbow->hw.cdvd->status & cdvd::STATUS_TRAY_OPEN_BIT;

    rainbow->buf[1] = 6;
    rainbow->buf[2] = 0x04;
    rainbow->buf[3] = 0x10;
    rainbow->buf[4] = rainbow->media_event;
    rainbow->buf[5] = (media_present(rainbow) ? 0x02 : 0x00) | (tray_open ? 0x01 : 0x00);

    iris_debug(rainbow, "GET EVENT STATUS -> event {} status {:02x}", rainbow->media_event, rainbow->buf[5]);

    rainbow->media_event = 0;

    set_sense(rainbow, SENSE_NONE, 0, 0);
    schedule_data_in(rainbow, std::min<uint32_t>(size, 8));
}

static void cmd_get_configuration(Rainbow* rainbow) {
    uint32_t size = alloc_length_16(rainbow->packet);
    uint16_t profile = 0;

    check_media_change(rainbow);

    if (media_present(rainbow)) {
        profile = media_is_dvd(rainbow) ? 0x0010 : 0x0008;
    }

    uint8_t data[20] = {};

    data[3] = 16;
    data[6] = profile >> 8;
    data[7] = profile & 0xff;
    data[10] = 0x03;
    data[11] = 8;
    data[13] = 0x10;
    data[14] = profile == 0x0010 ? 0x01 : 0x00;
    data[17] = 0x08;
    data[18] = profile == 0x0008 ? 0x01 : 0x00;

    memset(rainbow->buf, 0, std::max<uint32_t>(size, sizeof(data)));
    memcpy(rainbow->buf, data, std::min<uint32_t>(size, sizeof(data)));

    iris_debug(rainbow, "GET CONFIGURATION -> profile {:04x}", profile);

    set_sense(rainbow, SENSE_NONE, 0, 0);
    schedule_data_in(rainbow, size);
}

static void cmd_start_stop_unit(Rainbow* rainbow) {
    iris_debug(rainbow, "START STOP UNIT {:02x}", rainbow->packet[4]);

    schedule_complete(rainbow);
}

static void cmd_vendor_f6(Rainbow* rainbow) {
    const uint8_t* pkt = rainbow->packet;
    uint32_t size = alloc_length_16(pkt);

    check_media_change(rainbow);

    memset(rainbow->buf, 0, std::max<uint32_t>(size, 16));

    switch (pkt[2]) {
        case 0xa0: {
            if (media_present(rainbow)) {
                rainbow->buf[3] = 0x10 | 0x02;
            }
        } break;

        case 0xa2: {
            if (media_present(rainbow) && media_is_dvd(rainbow)) {
                rainbow->buf[5] = 0x20;
            }
        } break;

        case 0xb0: break;

        case 0xb1: {
            rainbow->buf[4] = 0x81;
        } break;

        default: {
            iris_warning(rainbow, "Unhandled vendor command F6 {:02x}", pkt[2]);

            fail(rainbow, SENSE_ILLEGAL_REQUEST, 0x24, 0x00);
        } return;
    }

    iris_debug(rainbow, "F6 {:02x} -> {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}", pkt[2],
        rainbow->buf[0], rainbow->buf[1], rainbow->buf[2],
        rainbow->buf[3], rainbow->buf[4], rainbow->buf[5]
    );

    set_sense(rainbow, SENSE_NONE, 0, 0);
    schedule_data_in(rainbow, size);
}

static void cmd_vendor_f9(Rainbow* rainbow) {
    switch (rainbow->packet[2]) {
        case 0xb1: {
            iris_debug(rainbow, "Spindle released to the mechacon");

            rainbow->hw.cdvd->atapi_owner = 0;

            schedule_complete(rainbow);
        } return;

        case 0xb2: {
            iris_debug(rainbow, "Spindle taken by the ATAPI drive");

            rainbow->hw.cdvd->atapi_owner = 1;

            schedule_complete(rainbow);
        } return;
    }

    iris_warning(rainbow, "Unhandled vendor command F9 {:02x}", rainbow->packet[2]);

    fail(rainbow, SENSE_ILLEGAL_REQUEST, 0x24, 0x00);
}

static void execute_packet(Rainbow* rainbow) {
    const uint8_t* pkt = rainbow->packet;

    rainbow->byte_limit = rainbow->lcyl | (rainbow->hcyl << 8);

    if (!rainbow->byte_limit) {
        rainbow->byte_limit = 0xfffe;
    }

    iris_debug(rainbow, "Packet {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} (dma {})",
        pkt[0], pkt[1], pkt[2], pkt[3], pkt[4], pkt[5],
        pkt[6], pkt[7], pkt[8], pkt[9], pkt[10], pkt[11],
        rainbow->feature & 1
    );

    if (pkt[0] != 0x03) {
        set_sense(rainbow, SENSE_NONE, 0, 0);
    }

    switch (pkt[0]) {
        case 0x00: cmd_test_unit_ready(rainbow); return;
        case 0x03: cmd_request_sense(rainbow); return;
        case 0x12: cmd_inquiry(rainbow); return;
        case 0x1b: cmd_start_stop_unit(rainbow); return;
        case 0x1e: schedule_complete(rainbow); return;
        case 0x25: cmd_read_capacity(rainbow); return;
        case 0x28: cmd_read10(rainbow); return;
        case 0x46: cmd_get_configuration(rainbow); return;
        case 0x51: cmd_read_disc_info(rainbow); return;
        case 0x4a: cmd_get_event_status(rainbow); return;
        case 0xf6: cmd_vendor_f6(rainbow); return;
        case 0xf9: cmd_vendor_f9(rainbow); return;
    }

    iris_warning(rainbow, "Unhandled packet {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
        pkt[0], pkt[1], pkt[2], pkt[3], pkt[4], pkt[5],
        pkt[6], pkt[7], pkt[8], pkt[9], pkt[10], pkt[11]
    );

    fail(rainbow, SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
}

static void build_identify(Rainbow* rainbow) {
    uint16_t id[256] = {};

    id[0] = 0x85c0;
    id[49] = 0x0f00;
    id[53] = 0x0006;
    id[63] = 0x0407;
    id[64] = 0x0003;
    id[65] = 0x0078;
    id[66] = 0x0078;
    id[67] = 0x00b4;
    id[68] = 0x0078;
    id[80] = 0x0070;
    id[88] = 0x0407;

    char model[40];
    char firmware[8];
    char serial[20];

    put_string((uint8_t*)model, "SONY DVD+-RW DW-DX02A", sizeof(model));
    put_string((uint8_t*)firmware, g_revision, sizeof(firmware));
    put_string((uint8_t*)serial, "0", sizeof(serial));

    for (int i = 0; i < 10; i++) {
        id[10 + i] = (serial[i * 2] << 8) | serial[i * 2 + 1];
    }

    for (int i = 0; i < 4; i++) {
        id[23 + i] = (firmware[i * 2] << 8) | firmware[i * 2 + 1];
    }

    for (int i = 0; i < 20; i++) {
        id[27 + i] = (model[i * 2] << 8) | model[i * 2 + 1];
    }

    memcpy(rainbow->buf, id, sizeof(id));
}

static void ata_data_in(Rainbow* rainbow, uint32_t size) {
    rainbow->buf_size = size;
    rainbow->buf_index = 0;
    rainbow->status = STATUS_DRDY | STATUS_DSC | STATUS_DRQ;
    rainbow->phase = PHASE_ATA_DATA_IN;

    assert_irq(rainbow);
}

static void ata_complete(Rainbow* rainbow) {
    rainbow->status = STATUS_DRDY | STATUS_DSC;
    rainbow->error = 0;
    rainbow->phase = PHASE_IDLE;

    assert_irq(rainbow);
}

static void ata_abort(Rainbow* rainbow) {
    rainbow->status = STATUS_DRDY | STATUS_ERR;
    rainbow->error = ERROR_ABRT;
    rainbow->phase = PHASE_IDLE;

    assert_irq(rainbow);
}

static void handle_command(Rainbow* rainbow, uint8_t cmd) {
    if (device1_selected(rainbow))
        return;

    iris_debug(rainbow, "Command {:02x} feature {:02x} nsector {:02x}", cmd, rainbow->feature, rainbow->nsector);

    switch (cmd) {
        case 0x08: {
            set_signature(rainbow);
        } break;

        case 0x90: {
            set_signature(rainbow);
            assert_irq(rainbow);
        } break;

        case 0xa0: {
            rainbow->packet_index = 0;
            rainbow->phase = PHASE_PACKET;
            rainbow->nsector = IREASON_COD;
            rainbow->status = STATUS_DRDY | STATUS_DRQ;
        } break;

        case 0xa1: {
            build_identify(rainbow);
            ata_data_in(rainbow, 512);
        } break;

        case 0xef: {
            iris_debug(rainbow, "SET FEATURES {:02x} {:02x}", rainbow->feature, rainbow->nsector);

            ata_complete(rainbow);
        } break;

        case 0xec: {
            rainbow->lcyl = 0x14;
            rainbow->hcyl = 0xeb;

            ata_abort(rainbow);
        } break;

        default: {
            iris_warning(rainbow, "Unhandled ATA command {:02x}", cmd);

            ata_abort(rainbow);
        } break;
    }
}

static uint16_t read_data(Rainbow* rainbow) {
    if (rainbow->phase == PHASE_ATA_DATA_IN) {
        uint16_t value = rainbow->buf[rainbow->buf_index] | (rainbow->buf[rainbow->buf_index + 1] << 8);

        rainbow->buf_index += 2;

        if (rainbow->buf_index >= rainbow->buf_size) {
            rainbow->status = STATUS_DRDY | STATUS_DSC;
            rainbow->phase = PHASE_IDLE;
        }

        return value;
    }

    if (rainbow->phase == PHASE_ATAPI_DATA_IN) {
        uint16_t value = rainbow->buf[rainbow->buf_index];

        if (rainbow->buf_index + 1 < rainbow->chunk_end) {
            value |= rainbow->buf[rainbow->buf_index + 1] << 8;
        }

        rainbow->buf_index = std::min(rainbow->buf_index + 2, rainbow->chunk_end);

        if (rainbow->buf_index >= rainbow->chunk_end) {
            if (rainbow->buf_index >= rainbow->buf_size) {
                schedule_complete(rainbow);
            } else {
                rainbow->status = STATUS_BSY;

                scheduler::Event event;

                event.callback = data_in_event;
                event.udata = rainbow;
                event.cycles = 1000;
                event.name = "rainbow data in";

                scheduler::schedule(rainbow->hw.sched, event);
            }
        }

        return value;
    }

    return 0;
}

static void write_data(Rainbow* rainbow, uint16_t data) {
    if (rainbow->phase != PHASE_PACKET)
        return;

    rainbow->packet[rainbow->packet_index++] = data & 0xff;
    rainbow->packet[rainbow->packet_index++] = data >> 8;

    if (rainbow->packet_index >= 12) {
        rainbow->status = STATUS_BSY;
        rainbow->phase = PHASE_IDLE;

        execute_packet(rainbow);
    }
}

static void write_control(Rainbow* rainbow, uint8_t data) {
    int srst_falling = (rainbow->control & CONTROL_SRST) && !(data & CONTROL_SRST);

    rainbow->control = data;

    if (data & CONTROL_SRST) {
        rainbow->status = STATUS_BSY;
    }

    if (srst_falling) {
        set_signature(rainbow);
    }
}

static void write_if_ctrl(Rainbow* rainbow, uint16_t data) {
    if (data & 0x80) {
        set_signature(rainbow);
        clear_irq(rainbow);
    }

    rainbow->if_ctrl = data & ~0x80;
}

static uint16_t read_reg(Rainbow* rainbow, uint32_t offset) {
    if (offset >= 0x4000) {
        return 0;
    }

    if (offset >= 0x40 && offset < 0x60 && offset != 0x4c && device1_selected(rainbow)) {
        return 0;
    }

    switch (offset) {
        case 0x02: return 0x0011;
        case 0x04: return 0x0002;
        case 0x0e: return 0x0002;
        case 0x24: return rainbow->dma_ctrl;
        case 0x28: return rainbow->intr_stat;
        case 0x2a: return rainbow->intr_mask;
        case 0x32: return rainbow->xfr_ctrl;
        case 0x38: {
            if (!rainbow->dma_active)
                return 0;

            return std::min<uint32_t>((rainbow->buf_size - rainbow->buf_index) / 512, 31);
        }
        case 0x40: return read_data(rainbow);
        case 0x42: return rainbow->error;
        case 0x44: return rainbow->nsector;
        case 0x46: return rainbow->sector;
        case 0x48: return rainbow->lcyl;
        case 0x4a: return rainbow->hcyl;
        case 0x4c: return rainbow->select;
        case 0x4e: {
            clear_irq(rainbow);

            return rainbow->status;
        }
        case 0x5c: return rainbow->status;
        case 0x64: return rainbow->if_ctrl;
        case 0x70: return rainbow->pio_mode;
        case 0x72: return rainbow->mwdma_mode;
        case 0x74: return rainbow->udma_mode;
    }

    iris_debug(rainbow, "Unhandled read from {:04x}", offset);

    return 0;
}

static void write_reg(Rainbow* rainbow, uint32_t offset, uint16_t data) {
    if (offset >= 0x4000) {
        return;
    }

    if (offset >= 0x40 && offset < 0x5c && offset != 0x4c && device1_selected(rainbow)) {
        return;
    }

    switch (offset) {
        case 0x24: rainbow->dma_ctrl = data; return;
        case 0x2e: rainbow->led = data; return;
        case 0x28: {
            rainbow->intr_stat &= ~data;

            update_irq(rainbow);
        } return;
        case 0x2a: {
            rainbow->intr_mask = data;

            update_irq(rainbow);
        } return;
        case 0x32: rainbow->xfr_ctrl = data; return;
        case 0x40: write_data(rainbow, data); return;
        case 0x42: rainbow->feature = data; return;
        case 0x44: rainbow->nsector = data; return;
        case 0x46: rainbow->sector = data; return;
        case 0x48: rainbow->lcyl = data; return;
        case 0x4a: rainbow->hcyl = data; return;
        case 0x4c: rainbow->select = data; return;
        case 0x4e: handle_command(rainbow, data); return;
        case 0x5c: write_control(rainbow, data); return;
        case 0x64: write_if_ctrl(rainbow, data); return;
        case 0x70: rainbow->pio_mode = data; return;
        case 0x72: rainbow->mwdma_mode = data; return;
        case 0x74: rainbow->udma_mode = data; return;
    }

    iris_debug(rainbow, "Unhandled write to {:04x} ({:04x})", offset, data);
}

Rainbow* create(logger::Logger* logger, iop::intc::Intc* intc, scheduler::Scheduler* sched, cdvd::Cdvd* cdvd) {
    Rainbow* rainbow = new Rainbow();

    rainbow->logger = logger;
    rainbow->logger_id = logger::register_source(logger, "rainbow");

    rainbow->hw.intc = intc;
    rainbow->hw.sched = sched;
    rainbow->hw.cdvd = cdvd;

    reset(rainbow);

    return rainbow;
}

void reset(Rainbow* rainbow) {
    auto hw = rainbow->hw;
    auto logger = rainbow->logger;
    auto logger_id = rainbow->logger_id;
    int enabled = rainbow->enabled;

    new (rainbow) Rainbow();

    rainbow->hw = hw;
    rainbow->logger = logger;
    rainbow->logger_id = logger_id;
    rainbow->enabled = enabled;

    set_signature(rainbow);

    rainbow->hw.cdvd->atapi_owner = 0;
}

void destroy(Rainbow* rainbow) {
    delete rainbow;
}

void set_enabled(Rainbow* rainbow, int enabled) {
    rainbow->enabled = enabled;
}

int dma_active(Rainbow* rainbow) {
    return rainbow->enabled && rainbow->dma_active;
}

void dma_read(Rainbow* rainbow, uint8_t* buf, uint32_t size) {
    uint32_t available = rainbow->buf_size - rainbow->buf_index;
    uint32_t n = std::min(size, available);

    memcpy(buf, &rainbow->buf[rainbow->buf_index], n);
    memset(buf + n, 0, size - n);

    rainbow->buf_index += n;

    if (rainbow->buf_index >= rainbow->buf_size) {
        rainbow->dma_active = 0;

        schedule_complete(rainbow);
    }
}

uint64_t read8(Rainbow* rainbow, uint32_t addr) {
    if (!rainbow->enabled)
        return 0;

    uint16_t value = read_reg(rainbow, (addr & 0xffff) & ~1);

    return (addr & 1) ? (value >> 8) : (value & 0xff);
}

uint64_t read16(Rainbow* rainbow, uint32_t addr) {
    if (!rainbow->enabled)
        return 0;

    return read_reg(rainbow, addr & 0xffff);
}

uint64_t read32(Rainbow* rainbow, uint32_t addr) {
    if (!rainbow->enabled)
        return 0;

    return read_reg(rainbow, addr & 0xffff);
}

void write8(Rainbow* rainbow, uint32_t addr, uint64_t data) {
    if (!rainbow->enabled)
        return;

    write_reg(rainbow, addr & 0xffff, data & 0xff);
}

void write16(Rainbow* rainbow, uint32_t addr, uint64_t data) {
    if (!rainbow->enabled)
        return;

    write_reg(rainbow, addr & 0xffff, data & 0xffff);
}

void write32(Rainbow* rainbow, uint32_t addr, uint64_t data) {
    if (!rainbow->enabled)
        return;

    write_reg(rainbow, addr & 0xffff, data & 0xffff);
}

}
