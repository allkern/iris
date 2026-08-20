#include <new>

#include "acuart.hpp"

namespace iris::s2x6::acuart {

static void status_event(void* udata, int overshoot);

static void schedule_status(Acuart* acuart) {
    scheduler::Event event;

    event.callback = status_event;
    event.cycles = STATUS_INTERVAL;
    event.name = "ACUART drive board status";
    event.udata = acuart;

    scheduler::schedule(acuart->sched, event);
}

static void status_event(void* udata, int overshoot) {
    Acuart* acuart = (Acuart*)udata;

    schedule_status(acuart);

    if (acuart->device != DEVICE_DRIVE_BOARD)
        return;

    if (!acuart->rx_size) {
        static const char status[] = "C01";

        for (const char* c = status; *c; c++)
            acuart->rx[acuart->rx_size++] = *c;

        acuart->rx_head = 0;
    }

    accore::irq(acuart->accore, accore::IRQ_UART);
}

void reset(Acuart* acuart) {
    schedule_status(acuart);
}

Acuart* create(logger::Logger* logger, accore::Accore* accore, scheduler::Scheduler* sched) {
    Acuart* acuart = new Acuart();

    acuart->logger = logger;
    acuart->logger_id = logger::register_source(logger, "acuart");

    acuart->accore = accore;
    acuart->sched = sched;

    schedule_status(acuart);

    return acuart;
}

void set_device(Acuart* acuart, int device) {
    acuart->device = device;
}

static uint8_t receive(Acuart* acuart) {
    if (!acuart->rx_size)
        return 0;

    uint8_t value = acuart->rx[acuart->rx_head];

    acuart->rx_head = (acuart->rx_head + 1) % RX_MAX;
    acuart->rx_size--;

    return value;
}

static void flush_line(Acuart* acuart) {
    if (!acuart->line_size)
        return;

    acuart->line[acuart->line_size] = '\0';
    acuart->line_size = 0;

    iris_info(acuart, "{}", acuart->line);
}

void destroy(Acuart* acuart) {
    flush_line(acuart);

    delete acuart;
}

static void transmit(Acuart* acuart, uint8_t value) {
    if (acuart->device != DEVICE_NONE)
        return;

    if (value == '\n' || acuart->line_size == LINE_MAX - 1) {
        flush_line(acuart);

        if (value == '\n')
            return;
    }

    if (value == '\r')
        return;

    acuart->line[acuart->line_size++] = value;
}

uint64_t read16(Acuart* acuart, uint32_t addr) {
    switch (addr & (ADDR_SIZE - 1)) {
        case R_DATA: {
            if (acuart->line_control & LINE_CONTROL_DIVISOR)
                return acuart->divisor_low;

            return receive(acuart);
        }

        case R_INTERRUPT_ENABLE: {
            if (acuart->line_control & LINE_CONTROL_DIVISOR)
                return acuart->divisor_high;

            return acuart->interrupt_enable;
        }

        case R_INTERRUPT_ID: return INTERRUPT_NONE;
        case R_LINE_CONTROL: return acuart->line_control;
        case R_MODEM_CONTROL: return acuart->modem_control;
        case R_LINE_STATUS: return LINE_STATUS_TX_IDLE | (acuart->rx_size ? LINE_STATUS_RX_READY : 0);
        case R_MODEM_STATUS: return 0;
        case R_SCRATCH: return acuart->scratch;
    }

    return 0;
}

void write16(Acuart* acuart, uint32_t addr, uint64_t data) {
    switch (addr & (ADDR_SIZE - 1)) {
        case R_DATA: {
            if (acuart->line_control & LINE_CONTROL_DIVISOR) {
                acuart->divisor_low = data;
            } else {
                transmit(acuart, data & 0xff);
            }
        } return;

        case R_INTERRUPT_ENABLE: {
            if (acuart->line_control & LINE_CONTROL_DIVISOR) {
                acuart->divisor_high = data;

                return;
            }

            acuart->interrupt_enable = data;
        } return;

        case R_INTERRUPT_ID: return;

        case R_LINE_CONTROL: acuart->line_control = data; return;
        case R_MODEM_CONTROL: acuart->modem_control = data; return;
        case R_SCRATCH: acuart->scratch = data; return;
    }
}

}
