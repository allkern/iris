#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "usb.h"
#include "usb/kbd.h"
#include "usb/mouse.h"

#define USB_DEBUG 0

#if USB_DEBUG >= 1
#define usb_log(...) printf("usb: " __VA_ARGS__)
#else
#define usb_log(...) do {} while (0)
#endif

#if USB_DEBUG >= 2
#define usb_logv(...) printf("usb: " __VA_ARGS__)
#else
#define usb_logv(...) do {} while (0)
#endif

// HcInterruptStatus/HcInterruptEnable/HcInterruptDisable
#define OHCI_INTR_SO   0x00000001 // Scheduling overrun
#define OHCI_INTR_WDH  0x00000002 // Writeback done head
#define OHCI_INTR_SF   0x00000004 // Start of frame
#define OHCI_INTR_RD   0x00000008 // Resume detected
#define OHCI_INTR_UE   0x00000010 // Unrecoverable error
#define OHCI_INTR_FNO  0x00000020 // Frame number overflow
#define OHCI_INTR_RHSC 0x00000040 // Root hub status change
#define OHCI_INTR_OC   0x40000000 // Ownership change
#define OHCI_INTR_MIE  0x80000000 // Master interrupt enable

// HcControl
#define OHCI_CTL_CBSR  0x00000003 // Control/bulk service ratio
#define OHCI_CTL_PLE   0x00000004 // Periodic list enable
#define OHCI_CTL_IE    0x00000008 // Isochronous enable
#define OHCI_CTL_CLE   0x00000010 // Control list enable
#define OHCI_CTL_BLE   0x00000020 // Bulk list enable
#define OHCI_CTL_HCFS  0x000000c0 // Host controller functional state
#define OHCI_CTL_IR    0x00000100 // Interrupt routing
#define OHCI_CTL_RWC   0x00000200 // Remote wakeup connected
#define OHCI_CTL_RWE   0x00000400 // Remote wakeup enable

#define OHCI_USB_RESET       0x00
#define OHCI_USB_RESUME      0x40
#define OHCI_USB_OPERATIONAL 0x80
#define OHCI_USB_SUSPEND     0xc0

// HcCommandStatus
#define OHCI_STATUS_HCR 0x00000001 // Host controller reset
#define OHCI_STATUS_CLF 0x00000002 // Control list filled
#define OHCI_STATUS_BLF 0x00000004 // Bulk list filled
#define OHCI_STATUS_OCR 0x00000008 // Ownership change request

// HcRhPortStatus
#define OHCI_PORT_CCS  0x00000001 // Current connect status
#define OHCI_PORT_PES  0x00000002 // Port enable status
#define OHCI_PORT_PSS  0x00000004 // Port suspend status
#define OHCI_PORT_POCI 0x00000008 // Port over-current indicator
#define OHCI_PORT_PRS  0x00000010 // Port reset status
#define OHCI_PORT_PPS  0x00000100 // Port power status
#define OHCI_PORT_LSDA 0x00000200 // Low speed device attached
#define OHCI_PORT_CSC  0x00010000 // Connect status change
#define OHCI_PORT_PESC 0x00020000 // Port enable status change
#define OHCI_PORT_PSSC 0x00040000 // Port suspend status change
#define OHCI_PORT_OCIC 0x00080000 // Over-current indicator change
#define OHCI_PORT_PRSC 0x00100000 // Port reset status change

// Endpoint descriptor fields (16 bytes: control, tailP, headP, nextED)
#define OHCI_ED_K 0x00004000 // Skip
#define OHCI_ED_F 0x00008000 // Format (1 = isochronous)
#define OHCI_ED_H 0x00000001 // Halted (headP bit 0)
#define OHCI_ED_C 0x00000002 // Toggle carry (headP bit 1)

// Transfer descriptor control field (general TD)
#define OHCI_TD_DP_SHIFT 19
#define OHCI_TD_DP_MASK  0x3
#define OHCI_TD_CC_SHIFT 28

// OHCI completion (condition) codes
#define OHCI_CC_NOERROR        0x0
#define OHCI_CC_STALL          0x4
#define OHCI_CC_DEVICENOTRESP  0x5

// A USB frame is 1 ms. The scheduler is clocked at the EE clock (294.912 MHz).
#define OHCI_FRAME_CYCLES 294912

// HCCA layout offsets
#define HCCA_FRAMENUMBER 0x80
#define HCCA_DONEHEAD    0x84

static inline uint32_t ohci_read_dword(struct ps2_usb* usb, uint32_t addr) {
    return iop_bus_read32(usb->bus, addr);
}

static inline void ohci_write_dword(struct ps2_usb* usb, uint32_t addr, uint32_t data) {
    iop_bus_write32(usb->bus, addr, data);
}

static void ohci_read_buf(struct ps2_usb* usb, uint32_t addr, uint8_t* buf, int len) {
    for (int i = 0; i < len; i++)
        buf[i] = iop_bus_read8(usb->bus, addr + i);
}

static void ohci_write_buf(struct ps2_usb* usb, uint32_t addr, const uint8_t* buf, int len) {
    for (int i = 0; i < len; i++)
        iop_bus_write8(usb->bus, addr + i, buf[i]);
}

static void ohci_update_irq(struct ps2_usb* usb) {
    if ((usb->hc_interrupt_enable & OHCI_INTR_MIE) &&
        (usb->hc_interrupt_status & usb->hc_interrupt_enable & 0x7fffffff)) {
        ps2_iop_intc_irq(usb->intc, IOP_INTC_USB);
    }
}

static void ohci_set_interrupt(struct ps2_usb* usb, uint32_t bit) {
    usb->hc_interrupt_status |= bit;
    ohci_update_irq(usb);
}

// Note: Assert RHSC if any root hub port has an unacknowledged change pending. This is
//       needed because a device may already be connected before the driver enables
//       interrupts/goes operational, and a software reset clears HcInterruptStatus.
static void ohci_update_rhsc(struct ps2_usb* usb) {
    uint32_t change_mask = OHCI_PORT_CSC | OHCI_PORT_PESC | OHCI_PORT_PSSC |
                           OHCI_PORT_OCIC | OHCI_PORT_PRSC;

    for (int i = 0; i < OHCI_NUM_PORTS; i++) {
        if (usb->hc_rh_port_status[i] & change_mask) {
            usb_log("root hub change pending on port %d (status=%08x), asserting RHSC\n",
                i, usb->hc_rh_port_status[i]);
            ohci_set_interrupt(usb, OHCI_INTR_RHSC);
            return;
        }
    }
}

static struct usb_device* ohci_find_device(struct ps2_usb* usb, int addr) {
    for (int i = 0; i < OHCI_NUM_PORTS; i++) {
        struct usb_device* dev = &usb->device[i];

        if (dev->connected && dev->address == addr)
            return dev;
    }

    return NULL;
}

// Process the TD at the head of the given ED. The caller's local `ed` copy is
// updated in place (and written back to RAM) so it can keep looping.
// Returns 1 if a TD was retired (or the ED halted), 0 if the device NAKed.
static int ohci_service_td(struct ps2_usb* usb, uint32_t ed_addr, uint32_t* ed) {
    uint32_t head = ed[2] & 0xfffffff0;
    uint32_t tail = ed[1] & 0xfffffff0;

    if (head == tail)
        return 0;

    uint32_t td_addr = head;
    uint32_t td[4];

    for (int i = 0; i < 4; i++)
        td[i] = ohci_read_dword(usb, td_addr + i * 4);

    int fa  = ed[0] & 0x7f;
    int en  = (ed[0] >> 7) & 0xf;
    int dir = (ed[0] >> 11) & 3;

    int dp = (td[0] >> OHCI_TD_DP_SHIFT) & OHCI_TD_DP_MASK;
    int pid;

    if (dir == 1) pid = USB_PID_OUT;
    else if (dir == 2) pid = USB_PID_IN;
    else pid = dp; // direction taken from TD (0 = SETUP, 1 = OUT, 2 = IN)

    uint32_t cbp = td[1];
    uint32_t be  = td[3];
    uint32_t next_td = td[2] & 0xfffffff0;

    int buf_len = 0;
    if (cbp != 0)
        buf_len = (int)(be - cbp) + 1;
    if (buf_len < 0)
        buf_len = 0;

    uint8_t temp[4096];
    if (buf_len > (int)sizeof(temp))
        buf_len = sizeof(temp);

    struct usb_device* dev = ohci_find_device(usb, fa);

    int result;

    if (!dev) {
        usb_log("TD %08x: no device at address %d (ep=%d pid=%d)\n", td_addr, fa, en, pid);
        result = USB_ACK_NODEV;
    } else if (pid == USB_PID_IN) {
        result = usb_device_transfer(dev, pid, en, temp, buf_len);
    } else {
        if (buf_len)
            ohci_read_buf(usb, cbp, temp, buf_len);

        result = usb_device_transfer(dev, pid, en, temp, buf_len);
    }

    // NAK: leave the TD untouched, the HC retries it on a later frame
    if (result == USB_ACK_NAK)
        return 0;

    int cc = OHCI_CC_NOERROR;
    int actual = 0;

    if (result == USB_ACK_NODEV) {
        cc = OHCI_CC_DEVICENOTRESP;
    } else if (result == USB_ACK_STALL) {
        cc = OHCI_CC_STALL;
    } else {
        actual = result;

        if (pid == USB_PID_IN && actual > 0)
            ohci_write_buf(usb, cbp, temp, actual);
    }

    usb_log("TD %08x ed_fa=%d ep=%d pid=%d len=%d -> cc=%d actual=%d\n",
        td_addr, fa, en, pid, buf_len, cc, actual);

    // Update the transfer descriptor: current buffer pointer + condition code
    if (cc == OHCI_CC_NOERROR) {
        if (actual >= buf_len)
            td[1] = 0;          // whole buffer consumed
        else
            td[1] = cbp + actual;
    }

    td[0] = (td[0] & 0x0fffffff) | ((uint32_t)cc << OHCI_TD_CC_SHIFT);

    // Link the retired TD into the writeback (done) queue. The done queue reuses
    // the TD's NextTD field as its link.
    td[2] = usb->done_queue;
    usb->done_queue = td_addr;

    for (int i = 0; i < 4; i++)
        ohci_write_dword(usb, td_addr + i * 4, td[i]);

    // Advance the endpoint, or halt it on error
    if (cc == OHCI_CC_NOERROR)
        ed[2] = next_td | (ed[2] & OHCI_ED_C);
    else
        ed[2] |= OHCI_ED_H;

    ohci_write_dword(usb, ed_addr + 8, ed[2]);

    return 1;
}

static void ohci_service_ed_list(struct ps2_usb* usb, uint32_t ed_addr) {
    int guard = 0;

    while (ed_addr && guard++ < 64) {
        uint32_t ed[4];

        for (int i = 0; i < 4; i++)
            ed[i] = ohci_read_dword(usb, ed_addr + i * 4);

        uint32_t next_ed = ed[3] & 0xfffffff0;

        // Skip flagged, halted or isochronous endpoints
        if (!(ed[0] & OHCI_ED_K) && !(ed[2] & OHCI_ED_H) && !(ed[0] & OHCI_ED_F)) {
            int td_guard = 0;

            while (td_guard++ < 16) {
                if ((ed[2] & 0xfffffff0) == (ed[1] & 0xfffffff0))
                    break;
                if (ed[2] & OHCI_ED_H)
                    break;
                if (!ohci_service_td(usb, ed_addr, ed))
                    break;
            }
        }

        ed_addr = next_ed;
    }
}

static void ohci_schedule_frame(struct ps2_usb* usb);

static void ohci_frame(void* udata, int overshoot) {
    struct ps2_usb* usb = (struct ps2_usb*)udata;

    (void)overshoot;

    ohci_schedule_frame(usb);

    if ((usb->hc_control & OHCI_CTL_HCFS) != OHCI_USB_OPERATIONAL)
        return;

    usb->hc_fm_number = (usb->hc_fm_number + 1) & 0xffff;
    usb->hc_fm_remaining = usb->hc_fm_interval & 0x3fff;

    if (usb->hc_hcca)
        ohci_write_dword(usb, usb->hc_hcca + HCCA_FRAMENUMBER, usb->hc_fm_number & 0xffff);

    ohci_set_interrupt(usb, OHCI_INTR_SF);

    // Periodic (interrupt) list for this frame
    if (usb->hc_control & OHCI_CTL_PLE) {
        uint32_t intr_ed = 0;

        if (usb->hc_hcca)
            intr_ed = ohci_read_dword(usb, usb->hc_hcca + ((usb->hc_fm_number & 0x1f) * 4));

        if (intr_ed) {
            usb_logv("frame %u: periodic ED list head %08x\n", usb->hc_fm_number, intr_ed);
            ohci_service_ed_list(usb, intr_ed);
        }
    }

    // Control list
    if (usb->hc_control & OHCI_CTL_CLE) {
        ohci_service_ed_list(usb, usb->hc_control_head_ed);
        usb->hc_command_status &= ~OHCI_STATUS_CLF;
    }

    // Bulk list
    if (usb->hc_control & OHCI_CTL_BLE) {
        ohci_service_ed_list(usb, usb->hc_bulk_head_ed);
        usb->hc_command_status &= ~OHCI_STATUS_BLF;
    }

    // Write back the done queue once the driver has acknowledged the previous one
    if (usb->done_queue && !(usb->hc_interrupt_status & OHCI_INTR_WDH)) {
        uint32_t done = usb->done_queue;

        // Low bit signals other unmasked interrupts are pending
        if (usb->hc_interrupt_status & usb->hc_interrupt_enable & 0x7fffffff & ~OHCI_INTR_WDH)
            done |= 1;

        if (usb->hc_hcca)
            ohci_write_dword(usb, usb->hc_hcca + HCCA_DONEHEAD, done);

        usb->hc_done_head = usb->done_queue;
        usb->done_queue = 0;

        usb_log("writeback done head %08x, raising WDH (intr_enable=%08x)\n",
            done & ~0xf, usb->hc_interrupt_enable);

        ohci_set_interrupt(usb, OHCI_INTR_WDH);
    }
}

static void ohci_schedule_frame(struct ps2_usb* usb) {
    struct sched_event event;

    event.callback = ohci_frame;
    event.cycles = OHCI_FRAME_CYCLES;
    event.name = "USB OHCI frame";
    event.udata = usb;

    sched_schedule(usb->sched, event);
}

static void ohci_soft_reset(struct ps2_usb* usb) {
    // Software reset (HcCommandStatus.HCR). Operational registers return to
    // their defaults, but the root hub/device connection state is preserved.
    usb->hc_control = OHCI_USB_SUSPEND;
    usb->hc_command_status = 0;
    usb->hc_interrupt_status = 0;
    usb->hc_interrupt_enable = 0;
    usb->hc_hcca = 0;
    usb->hc_period_current_ed = 0;
    usb->hc_control_head_ed = 0;
    usb->hc_control_current_ed = 0;
    usb->hc_bulk_head_ed = 0;
    usb->hc_bulk_current_ed = 0;
    usb->hc_done_head = 0;
    usb->hc_fm_remaining = 0;
    usb->done_queue = 0;

    usb_log("host controller software reset\n");
}

static void ohci_port_write(struct ps2_usb* usb, int port, uint32_t data) {
    uint32_t* ps = &usb->hc_rh_port_status[port];

    usb_log("write RhPortStatus[%d] = %08x (status was %08x)\n", port, data, *ps);

    if (data & OHCI_PORT_CCS)  // ClearPortEnable
        *ps &= ~OHCI_PORT_PES;

    if ((data & OHCI_PORT_PES) && (*ps & OHCI_PORT_CCS)) // SetPortEnable
        *ps |= OHCI_PORT_PES;

    if ((data & OHCI_PORT_PSS) && (*ps & OHCI_PORT_CCS)) // SetPortSuspend
        *ps |= OHCI_PORT_PSS;

    if (data & OHCI_PORT_POCI) // ClearSuspendStatus
        *ps &= ~OHCI_PORT_PSS;

    if ((data & OHCI_PORT_PRS) && (*ps & OHCI_PORT_CCS)) {
        // SetPortReset: a port reset returns the device to the default address
        usb_device_reset(&usb->device[port]);

        *ps &= ~OHCI_PORT_PRS;
        *ps |= OHCI_PORT_PES | OHCI_PORT_PRSC;

        ohci_set_interrupt(usb, OHCI_INTR_RHSC);

        usb_log("port %d reset\n", port);
    }

    if (data & OHCI_PORT_PPS)  // SetPortPower
        *ps |= OHCI_PORT_PPS;

    if (data & OHCI_PORT_LSDA) // ClearPortPower
        *ps &= ~OHCI_PORT_PPS;

    // Write-1-to-clear change bits
    *ps &= ~(data & (OHCI_PORT_CSC | OHCI_PORT_PESC | OHCI_PORT_PSSC |
                     OHCI_PORT_OCIC | OHCI_PORT_PRSC));
}

static const struct {
    const char* name;
    void (*create)(struct usb_device* dev);
} usb_device_types[USB_DEVICE_TYPE_COUNT] = {
    [USB_DEVICE_NONE]     = { "None", NULL },
    [USB_DEVICE_KEYBOARD] = { "Keyboard", usb_kbd_create },
    [USB_DEVICE_MOUSE]    = { "Mouse", usb_mouse_create },
};

const char* ps2_usb_device_type_name(int type) {
    if (type < 0 || type >= USB_DEVICE_TYPE_COUNT)
        return NULL;

    return usb_device_types[type].name;
}

int ps2_usb_get_port_device(struct ps2_usb* usb, int port) {
    if (port < 0 || port >= OHCI_NUM_PORTS)
        return USB_DEVICE_NONE;

    return usb->device_type[port];
}

void ps2_usb_set_port_device(struct ps2_usb* usb, int port, int type) {
    if (port < 0 || port >= OHCI_NUM_PORTS)
        return;

    if (type < 0 || type >= USB_DEVICE_TYPE_COUNT)
        return;

    struct usb_device* dev = &usb->device[port];

    // Tear down whatever is currently connected
    usb_device_free(dev);

    usb->device_type[port] = type;

    if (usb_device_types[type].create) {
        usb_device_types[type].create(dev);

        // Device present and powered; flag the connect status change
        usb->hc_rh_port_status[port] = OHCI_PORT_CCS | OHCI_PORT_PPS | OHCI_PORT_CSC;
    } else {
        // Port emptied; clear connect status and flag the change
        usb->hc_rh_port_status[port] = OHCI_PORT_PPS | OHCI_PORT_CSC;
    }

    ohci_set_interrupt(usb, OHCI_INTR_RHSC);

    usb_log("port %d device set to %s (status=%08x)\n",
        port, usb_device_types[type].name, usb->hc_rh_port_status[port]);
}

struct ps2_usb* ps2_usb_create(void) {
    return calloc(1, sizeof(struct ps2_usb));
}

void ps2_usb_init(struct ps2_usb* usb, struct ps2_iop_intc* intc, struct iop_bus* bus, struct sched_state* sched) {
    // The port device selection survives a reset
    int configured = usb->configured;
    int device_type[OHCI_NUM_PORTS];

    for (int i = 0; i < OHCI_NUM_PORTS; i++)
        device_type[i] = usb->device_type[i];

    memset(usb, 0, sizeof(struct ps2_usb));

    usb->intc = intc;
    usb->bus = bus;
    usb->sched = sched;

    usb->hc_control = OHCI_USB_RESET;
    usb->hc_fm_interval = 0x2edf;
    usb->hc_ls_threshold = 0x628;

    // NDP = OHCI_NUM_PORTS, NPS (no power switching, ports always on), POTPGT = 1
    usb->hc_rh_descriptor_a = (1u << 24) | (1u << 9) | OHCI_NUM_PORTS;
    usb->hc_rh_descriptor_b = 0;
    usb->hc_rh_status = 0;

    if (!configured) {
        // Default configuration: a keyboard on the first port
        device_type[0] = USB_DEVICE_KEYBOARD;

        for (int i = 1; i < OHCI_NUM_PORTS; i++)
            device_type[i] = USB_DEVICE_NONE;
    }

    usb->configured = 1;

    for (int i = 0; i < OHCI_NUM_PORTS; i++)
        ps2_usb_set_port_device(usb, i, device_type[i]);

    ohci_schedule_frame(usb);
}

void ps2_usb_destroy(struct ps2_usb* usb) {
    for (int i = 0; i < OHCI_NUM_PORTS; i++)
        usb_device_free(&usb->device[i]);

    free(usb);
}

uint64_t ps2_usb_read32(struct ps2_usb* usb, uint32_t addr) {
    addr &= 0xff;

    switch (addr) {
        case USB_HC_REVISION:         return 0x10;
        case USB_HC_CONTROL:          return usb->hc_control;
        case USB_HC_COMMANDSTATUS:    return usb->hc_command_status;
        case USB_HC_INTERRUPTSTATUS:  return usb->hc_interrupt_status;
        case USB_HC_INTERRUPTENABLE:  return usb->hc_interrupt_enable;
        case USB_HC_INTERRUPTDISABLE: return usb->hc_interrupt_enable;
        case USB_HC_HCCA:             return usb->hc_hcca;
        case USB_HC_PERIODCURRENTED:  return usb->hc_period_current_ed;
        case USB_HC_CONTROLHEADED:    return usb->hc_control_head_ed;
        case USB_HC_CONTROLCURRENTED: return usb->hc_control_current_ed;
        case USB_HC_BULKHEADED:       return usb->hc_bulk_head_ed;
        case USB_HC_BULKCURRENTED:    return usb->hc_bulk_current_ed;
        case USB_HC_DONEHEAD:         return usb->hc_done_head;
        case USB_HC_FMINTERVAL:       return usb->hc_fm_interval;
        case USB_HC_FMREMAINING:      return usb->hc_fm_remaining;
        case USB_HC_FMNUMBER:         return usb->hc_fm_number;
        case USB_HC_PERIODICSTART:    return usb->hc_periodic_start;
        case USB_HC_LSTHRESHOLD:      return usb->hc_ls_threshold;
        case USB_HC_RHDESCRIPTORA:    return usb->hc_rh_descriptor_a;
        case USB_HC_RHDESCRIPTORB:    return usb->hc_rh_descriptor_b;
        case USB_HC_RHSTATUS:         return usb->hc_rh_status;
    }

    if (addr >= USB_HC_RHPORTSTATUS && addr < USB_HC_RHPORTSTATUS + OHCI_NUM_PORTS * 4) {
        int port = (addr - USB_HC_RHPORTSTATUS) >> 2;

        usb_log("read RhPortStatus[%d] = %08x\n", port, usb->hc_rh_port_status[port]);

        return usb->hc_rh_port_status[port];
    }

    usb_log("unhandled read at %02x\n", addr);

    return 0;
}

void ps2_usb_write32(struct ps2_usb* usb, uint32_t addr, uint64_t data) {
    addr &= 0xff;

    uint32_t v = (uint32_t)data;

    switch (addr) {
        case USB_HC_REVISION:
            return;
        case USB_HC_CONTROL: {
            uint32_t old_state = usb->hc_control & OHCI_CTL_HCFS;
            usb->hc_control = v;
            uint32_t new_state = v & OHCI_CTL_HCFS;

            if (new_state != old_state) {
                const char* names[] = { "RESET", "RESUME", "OPERATIONAL", "SUSPEND" };
                usb_log("HcControl=%08x state->%s (PLE=%d CLE=%d BLE=%d IE=%d)\n",
                    v, names[new_state >> 6],
                    !!(v & OHCI_CTL_PLE), !!(v & OHCI_CTL_CLE),
                    !!(v & OHCI_CTL_BLE), !!(v & OHCI_CTL_IE));

                // A device may already be attached when the driver starts the
                // controller; let it know via the root hub status change interrupt.
                if (new_state == OHCI_USB_OPERATIONAL)
                    ohci_update_rhsc(usb);
            }
            return;
        }
        case USB_HC_COMMANDSTATUS:
            if (v & OHCI_STATUS_HCR)
                ohci_soft_reset(usb);

            usb->hc_command_status |= v & (OHCI_STATUS_CLF | OHCI_STATUS_BLF | OHCI_STATUS_OCR);

            if (v & OHCI_STATUS_OCR)
                ohci_set_interrupt(usb, OHCI_INTR_OC);

            usb_logv("HcCommandStatus write %08x\n", v);
            return;
        case USB_HC_INTERRUPTSTATUS:
            usb->hc_interrupt_status &= ~v; // write 1 to clear
            ohci_update_irq(usb);
            usb_logv("clear interrupt status %08x -> %08x\n", v, usb->hc_interrupt_status);
            return;
        case USB_HC_INTERRUPTENABLE:
            usb->hc_interrupt_enable |= v;
            ohci_update_irq(usb);
            usb_log("HcInterruptEnable |= %08x -> %08x\n", v, usb->hc_interrupt_enable);
            return;
        case USB_HC_INTERRUPTDISABLE:
            usb->hc_interrupt_enable &= ~v;
            ohci_update_irq(usb);
            usb_log("HcInterruptDisable %08x -> enable %08x\n", v, usb->hc_interrupt_enable);
            return;
        case USB_HC_HCCA:
            usb->hc_hcca = v & 0xffffff00;
            usb_log("HcHCCA = %08x\n", usb->hc_hcca);
            return;
        case USB_HC_PERIODCURRENTED:
            usb->hc_period_current_ed = v & 0xfffffff0;
            return;
        case USB_HC_CONTROLHEADED:
            usb->hc_control_head_ed = v & 0xfffffff0;
            usb_log("HcControlHeadED = %08x\n", usb->hc_control_head_ed);
            return;
        case USB_HC_CONTROLCURRENTED:
            usb->hc_control_current_ed = v & 0xfffffff0;
            return;
        case USB_HC_BULKHEADED:
            usb->hc_bulk_head_ed = v & 0xfffffff0;
            usb_log("HcBulkHeadED = %08x\n", usb->hc_bulk_head_ed);
            return;
        case USB_HC_BULKCURRENTED:
            usb->hc_bulk_current_ed = v & 0xfffffff0;
            return;
        case USB_HC_DONEHEAD:
            usb->hc_done_head = v;
            return;
        case USB_HC_FMINTERVAL:
            usb->hc_fm_interval = v;
            return;
        case USB_HC_FMREMAINING:
            return;
        case USB_HC_FMNUMBER:
            return;
        case USB_HC_PERIODICSTART:
            usb->hc_periodic_start = v;
            return;
        case USB_HC_LSTHRESHOLD:
            usb->hc_ls_threshold = v;
            return;
        case USB_HC_RHDESCRIPTORA:
            // Keep the number of downstream ports fixed
            usb->hc_rh_descriptor_a = (v & 0xffffff00) | OHCI_NUM_PORTS;
            return;
        case USB_HC_RHDESCRIPTORB:
            usb->hc_rh_descriptor_b = v;
            return;
        case USB_HC_RHSTATUS:
            if (v & 0x00010000) { // SetGlobalPower
                for (int i = 0; i < OHCI_NUM_PORTS; i++)
                    usb->hc_rh_port_status[i] |= OHCI_PORT_PPS;
            }
            if (v & 0x00000001) { // ClearGlobalPower
                for (int i = 0; i < OHCI_NUM_PORTS; i++)
                    usb->hc_rh_port_status[i] &= ~OHCI_PORT_PPS;
            }
            if (v & 0x00008000) // SetRemoteWakeupEnable
                usb->hc_rh_status |= 0x00008000;
            if (v & 0x80000000) // ClearRemoteWakeupEnable
                usb->hc_rh_status &= ~0x00008000;
            return;
    }

    if (addr >= USB_HC_RHPORTSTATUS && addr < USB_HC_RHPORTSTATUS + OHCI_NUM_PORTS * 4) {
        int port = (addr - USB_HC_RHPORTSTATUS) >> 2;

        ohci_port_write(usb, port, v);

        return;
    }

    usb_log("unhandled write %08x at %02x\n", v, addr);
}

void ps2_usb_kbd_key(struct ps2_usb* usb, uint8_t usage, int pressed) {
    for (int i = 0; i < OHCI_NUM_PORTS; i++) {
        if (usb->device_type[i] == USB_DEVICE_KEYBOARD)
            usb_kbd_key(&usb->device[i], usage, pressed);
    }
}

void ps2_usb_mouse_move(struct ps2_usb* usb, int dx, int dy, int dz) {
    for (int i = 0; i < OHCI_NUM_PORTS; i++) {
        if (usb->device_type[i] == USB_DEVICE_MOUSE)
            usb_mouse_move(&usb->device[i], dx, dy, dz);
    }
}

void ps2_usb_mouse_button(struct ps2_usb* usb, int button, int pressed) {
    for (int i = 0; i < OHCI_NUM_PORTS; i++) {
        if (usb->device_type[i] == USB_DEVICE_MOUSE)
            usb_mouse_button(&usb->device[i], button, pressed);
    }
}
