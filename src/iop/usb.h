#ifndef USB_H
#define USB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define OHCI_BASE 0x1f801600
#define OHCI_MAX_PORTS 15

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

struct ps2_usb {
    int dummy;
};

struct ps2_usb* ps2_usb_create(void);
void ps2_usb_init(struct ps2_usb* usb);
void ps2_usb_destroy(struct ps2_usb* usb);
uint64_t ps2_usb_read32(struct ps2_usb* usb, uint32_t addr);
void ps2_usb_write32(struct ps2_usb* usb, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif