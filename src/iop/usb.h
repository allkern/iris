#ifndef USB_H
#define USB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "intc.h"
#include "scheduler.h"
#include "bus_decl.h"

#include "usb/device.h"

#define OHCI_BASE 0x1f801600

// The PS2 OHCI root hub exposes 2 downstream ports
#define OHCI_NUM_PORTS 2

#define USB_HC_REVISION         0x00
#define USB_HC_CONTROL          0x04
#define USB_HC_COMMANDSTATUS    0x08
#define USB_HC_INTERRUPTSTATUS  0x0c
#define USB_HC_INTERRUPTENABLE  0x10
#define USB_HC_INTERRUPTDISABLE 0x14
#define USB_HC_HCCA             0x18
#define USB_HC_PERIODCURRENTED  0x1c
#define USB_HC_CONTROLHEADED    0x20
#define USB_HC_CONTROLCURRENTED 0x24
#define USB_HC_BULKHEADED       0x28
#define USB_HC_BULKCURRENTED    0x2c
#define USB_HC_DONEHEAD         0x30
#define USB_HC_FMINTERVAL       0x34
#define USB_HC_FMREMAINING      0x38
#define USB_HC_FMNUMBER         0x3c
#define USB_HC_PERIODICSTART    0x40
#define USB_HC_LSTHRESHOLD      0x44
#define USB_HC_RHDESCRIPTORA    0x48
#define USB_HC_RHDESCRIPTORB    0x4c
#define USB_HC_RHSTATUS         0x50
#define USB_HC_RHPORTSTATUS     0x54

enum {
    USB_DEVICE_NONE = 0,
    USB_DEVICE_KEYBOARD,
    USB_DEVICE_MOUSE,
    USB_DEVICE_MSD,
    USB_DEVICE_TYPE_COUNT
};

#define USB_MSD_PATH_MAX 512

struct ps2_usb {
    // OHCI operational registers
    uint32_t hc_control;
    uint32_t hc_command_status;
    uint32_t hc_interrupt_status;
    uint32_t hc_interrupt_enable;
    uint32_t hc_hcca;
    uint32_t hc_period_current_ed;
    uint32_t hc_control_head_ed;
    uint32_t hc_control_current_ed;
    uint32_t hc_bulk_head_ed;
    uint32_t hc_bulk_current_ed;
    uint32_t hc_done_head;
    uint32_t hc_fm_interval;
    uint32_t hc_fm_remaining;
    uint32_t hc_fm_number;
    uint32_t hc_periodic_start;
    uint32_t hc_ls_threshold;
    uint32_t hc_rh_descriptor_a;
    uint32_t hc_rh_descriptor_b;
    uint32_t hc_rh_status;
    uint32_t hc_rh_port_status[OHCI_NUM_PORTS];

    uint32_t done_queue;

    struct usb_device device[OHCI_NUM_PORTS];

    int device_type[OHCI_NUM_PORTS];
    char msd_path[OHCI_NUM_PORTS][USB_MSD_PATH_MAX];
    int configured;

    int frame_scheduled;

    struct ps2_iop_intc* intc;
    struct iop_bus* bus;
    struct sched_state* sched;
};

struct ps2_usb* ps2_usb_create(void);
void ps2_usb_init(struct ps2_usb* usb, struct ps2_iop_intc* intc, struct iop_bus* bus, struct sched_state* sched);
void ps2_usb_destroy(struct ps2_usb* usb);
uint64_t ps2_usb_read32(struct ps2_usb* usb, uint32_t addr);
void ps2_usb_write32(struct ps2_usb* usb, uint32_t addr, uint64_t data);
const char* ps2_usb_device_type_name(int type);
int ps2_usb_get_port_device(struct ps2_usb* usb, int port);
void ps2_usb_set_port_device(struct ps2_usb* usb, int port, int type);
void ps2_usb_msd_set_image(struct ps2_usb* usb, int port, const char* path);
void ps2_usb_kbd_key(struct ps2_usb* usb, uint8_t usage, int pressed);
void ps2_usb_mouse_move(struct ps2_usb* usb, int dx, int dy, int dz);
void ps2_usb_mouse_button(struct ps2_usb* usb, int button, int pressed);

#ifdef __cplusplus
}
#endif

#endif
