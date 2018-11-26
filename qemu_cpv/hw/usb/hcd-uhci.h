#ifndef HW_USB_HCD_UHCI_H
#define HW_USB_HCD_UHCI_H

#include "hw/hw.h"
#include "qemu/timer.h"
#include "hw/usb.h"
#include "sysemu/dma.h"
#include "sysemu/sysemu.h"
#include "hw/pci/pci.h"
#include "hw/sysbus.h"

#define UHCI_NB_PORTS     2

typedef struct UHCIAsync UHCIAsync;
typedef struct UHCIQueue UHCIQueue;
typedef struct UHCIState UHCIState;

/*
 * Pending async transaction.
 * 'packet' must be the first field because completion
 * handler does "(UHCIAsync *) pkt" cast.
 */

struct UHCIAsync {
    USBPacket packet;
    uint8_t   static_buf[64]; /* 64 bytes is enough, except for isoc packets */
    uint8_t   *buf;
    UHCIQueue *queue;
    QTAILQ_ENTRY(UHCIAsync) next;
    uint32_t  td_addr;
    uint8_t   done;
};

struct UHCIQueue {
    uint32_t  qh_addr;
    uint32_t  token;
    UHCIState *uhci;
    USBEndpoint *ep;
    QTAILQ_ENTRY(UHCIQueue) next;
    QTAILQ_HEAD(asyncs_head, UHCIAsync) asyncs;
    int8_t    valid;
};

typedef struct UHCIPort {
    USBPort port;
    uint16_t ctrl;
} UHCIPort;

struct UHCIState {
    USBBus bus; /* Note unused when we're a companion controller */
    DeviceState *device;
    AddressSpace *dma_as;
    qemu_irq irq;
    MemoryRegion mem;
    MemoryRegion io_bar;

    uint16_t cmd; /* cmd register */
    uint16_t status;
    uint16_t intr; /* interrupt enable register */
    uint16_t frnum; /* frame number */
    uint32_t fl_base_addr; /* frame list base address */
    uint8_t sof_timing;
    uint8_t status2; /* bit 0 and 1 are used to generate UHCI_STS_USBINT */
    int64_t expire_time;
    QEMUTimer *frame_timer;
    QEMUBH *bh;
    uint32_t frame_bytes;
    uint32_t frame_bandwidth;
    bool completions_only;
    UHCIPort ports[UHCI_NB_PORTS];

    /* Interrupts that should be raised at the end of the current frame.  */
    uint32_t pending_int_mask;

    /* Active packets */
    QTAILQ_HEAD(, UHCIQueue) queues;
    uint8_t num_ports_vmstate;

    /* Properties */
    char *masterbus;
    uint32_t firstport;
    uint32_t maxframes;

    void (*uhci_dev_reset)(DeviceState *dev);
};

extern const VMStateDescription vmstate_uhci;

extern void usb_uhci_init(UHCIState *s, DeviceState *dev);
extern void usb_uhci_realize(UHCIState *s, DeviceState *dev, Error **errp);
extern void usb_uhci_unrealize(UHCIState *s, DeviceState *dev, Error **errp);
extern void usb_uhci_reset(UHCIState *s);
extern void usb_uhci_exit(UHCIState *s);

extern void usb_uhci_sysbus_init(DeviceState *uhci_dev, AddressSpace *dma_as);

#endif
