#include <new>
#include <cstring>

#include "../speed.hpp"
#include "dvrp.hpp"

namespace iris::speed::dvrp {

Dvrp* create(logger::Logger* logger) {
    Dvrp* dvrp = new Dvrp();

    dvrp->logger = logger;
    dvrp->logger_id = logger::register_source(logger, "dvrp");

    return dvrp;
}

void init(Dvrp* dvrp, Speed* speed) {
    logger::Logger* logger = dvrp->logger;
    size_t logger_id = dvrp->logger_id;

    new (dvrp) Dvrp();

    dvrp->logger = logger;
    dvrp->logger_id = logger_id;

    dvrp->speed = speed;

    // bit 5 - IOMAN task
    // bit 4 - DVR task
    // bit 3 - AV task
    // bit 2 - DVR/MISC task
    // bit 1 - Busy
    dvrp->status = 0x3E;
}

void destroy(Dvrp* dvrp) {
    delete dvrp;
}

void dvrp_send_irq(Dvrp* dvrp, uint16_t irq) {
    dvrp->intr_stat = irq;
    dvrp->intr_cause = dvrp->cmd;

    if (dvrp->intr_stat & dvrp->intr_mask) {
        send_irq(dvrp->speed, INTR_DVR);
    }
}

void dvrp_send_intr_cmd_ack(void* udata, int overshoot) {
    Dvrp* dvrp = (Dvrp*)udata;

    dvrp_send_irq(dvrp, INTR_CMD_ACK);
}

void dvrp_send_intr_cmd_comp(void* udata, int overshoot) {
    Dvrp* dvrp = (Dvrp*)udata;

    dvrp_send_irq(dvrp, INTR_CMD_COMP);
}

static void dvrp_get_sircs(Dvrp* dvrp) {
    uint32_t code = dev::rm::next_code(dvrp->remote_key.load(), dvrp->remote_phase);

    dvrp->remote_reads++;

    dvrp->reply[1] = code ? 0x14 : 0x00;
    dvrp->reply[2] = code & 0xff;
    dvrp->reply[3] = (code >> 8) & 0xff;
    dvrp->reply[4] = (code >> 16) & 0xff;
}

void dvrp_handle_command(Dvrp* dvrp, uint16_t cmd) {
    // iris_debug(dvrp, "Handle command {:04x} params={}", cmd, dvrp->param_index);

    dvrp->cmd = cmd;
    dvrp->param_index = 0;

    memset(dvrp->reply, 0, sizeof(dvrp->reply));

    dvrp->reply_index = 0;

    if (cmd == 0x5107) {
        dvrp_get_sircs(dvrp);
    }

    scheduler::Event event;

    event.callback = dvrp_send_intr_cmd_ack;
    event.udata = dvrp;
    event.cycles = 10000;
    event.name = "dvrp cmd ack";

    scheduler::schedule(dvrp->speed->sched, event);

    // 210e - dvrioctl2_rec_prohibit
    // 3109 - avioctl2_set_d_audio_sel
}

uint64_t read(Dvrp* dvrp, uint32_t addr) {
    // iris_debug(dvrp, "read16 {:08x}", addr);

    switch (addr) {
        case 0x4200: return dvrp->intr_stat;
        case 0x4208: return dvrp->intr_mask;
        case 0x4210: return dvrp->cmd;
        case 0x4214: return dvrp->params[dvrp->param_index];
        case 0x4218: return dvrp->status2;
        case 0x4220: return dvrp->intr_cause;
        case 0x4224: return dvrp->reply_index < 64 ? dvrp->reply[dvrp->reply_index++] : 0;
        case 0x4228: return 1; // ?
        case 0x4230: return dvrp->status;

        // ??
        case 0x4234: return 1;
        case 0x4238: return dvrp->status;
        case 0x423c: return dvrp->status;
    }

    return 0;
}

void dvrp_clear_speed_dvr_intr(Dvrp* dvrp) {
    if ((dvrp->intr_stat & dvrp->intr_mask) == 0) {
        dvrp->speed->intr_stat &= ~INTR_DVR;
    }
}

void set_remote_key(Dvrp* dvrp, dev::rm::Key key) {
    dvrp->remote_key.store(key);
}

void release_remote_key(Dvrp* dvrp) {
    dvrp->remote_key.store(dev::rm::Key {});
}

void write(Dvrp* dvrp, uint32_t addr, uint64_t data) {
    // iris_debug(dvrp, "write16 {:08x} {:08x}", addr, data);

    switch (addr) {
        case 0x4204: dvrp->intr_stat &= ~data; dvrp_clear_speed_dvr_intr(dvrp); break;
        case 0x4208: dvrp->intr_mask = data; dvrp_clear_speed_dvr_intr(dvrp); break;
        case 0x4210: dvrp_handle_command(dvrp, data); break;
        case 0x4214: dvrp->params[dvrp->param_index++] = data; break;
        // case 0x4218: dvrp->status2 = data; break;
        case 0x4230: dvrp->status = data; break;
    }
}

}
