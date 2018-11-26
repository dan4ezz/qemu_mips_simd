/*
 * USB UHCI controller emulation
 *
 * Copyright (c) 2005 Fabrice Bellard
 *
 * Copyright (c) 2008 Max Krasnyansky
 *     Magor rewrite of the UHCI data structures parser and frame processor
 *     Support for fully async operation and multiple outstanding transactions
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/usb.h"
#include "hw/usb/uhci-regs.h"
#include "hw/pci/pci.h"
#include "qapi/error.h"
#include "qemu/timer.h"
#include "qemu/iov.h"
#include "sysemu/dma.h"
#include "trace.h"
#include "qemu/main-loop.h"

#include "hcd-uhci.h"

typedef struct UHCIInfo UHCIInfo;
typedef struct UHCIPCIDeviceClass UHCIPCIDeviceClass;

struct UHCIInfo {
    const char *name;
    uint16_t   vendor_id;
    uint16_t   device_id;
    uint8_t    revision;
    uint8_t    irq_pin;
    void       (*realize)(PCIDevice *dev, Error **errp);
    bool       unplug;
};

struct UHCIPCIDeviceClass {
    PCIDeviceClass parent_class;
    UHCIInfo       info;
};

struct UHCIPCIState {
    PCIDevice dev;
    UHCIState uhci;
};

typedef struct UHCIPCIState UHCIPCIState;

#define TYPE_PCI_UHCI "pci-uhci-usb"
#define PCI_UHCI(obj) OBJECT_CHECK(UHCIPCIState, (obj), TYPE_PCI_UHCI)

static void uhci_reset(DeviceState *dev)
{
    PCIDevice *d = PCI_DEVICE(dev);
    UHCIPCIState *s = PCI_UHCI(d);
    uint8_t *pci_conf;

    pci_conf = s->dev.config;

    pci_conf[0x6a] = 0x01; /* usb clock */
    pci_conf[0x6b] = 0x00;

    usb_uhci_reset(&s->uhci);
}

static const VMStateDescription vmstate_uhci_pci = {
    .name = "uhci-pci",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(dev, UHCIPCIState),
        VMSTATE_STRUCT(uhci, UHCIPCIState, 1, vmstate_uhci, UHCIState),
        VMSTATE_END_OF_LIST()
    }
};

static void usb_uhci_common_realize(PCIDevice *dev, Error **errp)
{
    Error *err = NULL;
    PCIDeviceClass *pc = PCI_DEVICE_GET_CLASS(dev);
    UHCIPCIDeviceClass *u = container_of(pc, UHCIPCIDeviceClass, parent_class);
    UHCIPCIState *s = PCI_UHCI(dev);
    uint8_t *pci_conf = s->dev.config;

    pci_conf[PCI_CLASS_PROG] = 0x00;
    /* TODO: reset value should be 0. */
    pci_conf[USB_SBRN] = USB_RELEASE_1; // release number

    pci_config_set_interrupt_pin(pci_conf, u->info.irq_pin + 1);
    s->uhci.uhci_dev_reset = uhci_reset;

    usb_uhci_init(&s->uhci, DEVICE(dev));
    usb_uhci_realize(&s->uhci, DEVICE(dev), &err);
    /* Use region 4 for consistency with real hardware.  BSD guests seem
       to rely on this.  */
    pci_register_bar(dev, 4, PCI_BASE_ADDRESS_SPACE_IO, &s->uhci.mem);
}

static void usb_uhci_vt82c686b_realize(PCIDevice *dev, Error **errp)
{
    UHCIPCIState *s = PCI_UHCI(dev);
    uint8_t *pci_conf = s->dev.config;

    /* USB misc control 1/2 */
    pci_set_long(pci_conf + 0x40,0x00001000);
    /* PM capability */
    pci_set_long(pci_conf + 0x80,0x00020001);
    /* USB legacy support  */
    pci_set_long(pci_conf + 0xc0,0x00002000);

    usb_uhci_common_realize(dev, errp);
}

static void usb_uhci_pci_exit(PCIDevice *dev)
{
    UHCIPCIState *s = PCI_UHCI(dev);

    usb_uhci_exit(&s->uhci);
}

static Property uhci_properties_companion[] = {
    DEFINE_PROP_STRING("masterbus", UHCIPCIState, uhci.masterbus),
    DEFINE_PROP_UINT32("firstport", UHCIPCIState, uhci.firstport, 0),
    DEFINE_PROP_UINT32("bandwidth", UHCIPCIState, uhci.frame_bandwidth, 1280),
    DEFINE_PROP_UINT32("maxframes", UHCIPCIState, uhci.maxframes, 128),
    DEFINE_PROP_END_OF_LIST(),
};

static Property uhci_properties_standalone[] = {
    DEFINE_PROP_UINT32("bandwidth", UHCIPCIState, uhci.frame_bandwidth, 1280),
    DEFINE_PROP_UINT32("maxframes", UHCIPCIState, uhci.maxframes, 128),
    DEFINE_PROP_END_OF_LIST(),
};

static void uhci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->class_id  = PCI_CLASS_SERIAL_USB;
    dc->vmsd = &vmstate_uhci_pci;
    dc->reset = uhci_reset;
    set_bit(DEVICE_CATEGORY_USB, dc->categories);
}

static const TypeInfo uhci_pci_type_info = {
    .name = TYPE_PCI_UHCI,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(UHCIPCIState),
    .class_size    = sizeof(UHCIPCIDeviceClass),
    .abstract = true,
    .class_init = uhci_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void uhci_data_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);
    UHCIPCIDeviceClass *u = container_of(k, UHCIPCIDeviceClass, parent_class);
    UHCIInfo *info = data;

    k->realize = info->realize ? info->realize : usb_uhci_common_realize;
    k->exit = info->unplug ? usb_uhci_pci_exit : NULL;
    k->vendor_id = info->vendor_id;
    k->device_id = info->device_id;
    k->revision  = info->revision;
    if (!info->unplug) {
        /* uhci controllers in companion setups can't be hotplugged */
        dc->hotpluggable = false;
        dc->props = uhci_properties_companion;
    } else {
        dc->props = uhci_properties_standalone;
    }
    u->info = *info;
}

static UHCIInfo uhci_info[] = {
    {
        .name       = "piix3-usb-uhci",
        .vendor_id = PCI_VENDOR_ID_INTEL,
        .device_id = PCI_DEVICE_ID_INTEL_82371SB_2,
        .revision  = 0x01,
        .irq_pin   = 3,
        .unplug    = true,
    },{
        .name      = "piix4-usb-uhci",
        .vendor_id = PCI_VENDOR_ID_INTEL,
        .device_id = PCI_DEVICE_ID_INTEL_82371AB_2,
        .revision  = 0x01,
        .irq_pin   = 3,
        .unplug    = true,
    },{
        .name      = "vt82c686b-usb-uhci",
        .vendor_id = PCI_VENDOR_ID_VIA,
        .device_id = PCI_DEVICE_ID_VIA_UHCI,
        .revision  = 0x01,
        .irq_pin   = 3,
        .realize   = usb_uhci_vt82c686b_realize,
        .unplug    = true,
    },{
        .name      = "ich9-usb-uhci1", /* 00:1d.0 */
        .vendor_id = PCI_VENDOR_ID_INTEL,
        .device_id = PCI_DEVICE_ID_INTEL_82801I_UHCI1,
        .revision  = 0x03,
        .irq_pin   = 0,
        .unplug    = false,
    },{
        .name      = "ich9-usb-uhci2", /* 00:1d.1 */
        .vendor_id = PCI_VENDOR_ID_INTEL,
        .device_id = PCI_DEVICE_ID_INTEL_82801I_UHCI2,
        .revision  = 0x03,
        .irq_pin   = 1,
        .unplug    = false,
    },{
        .name      = "ich9-usb-uhci3", /* 00:1d.2 */
        .vendor_id = PCI_VENDOR_ID_INTEL,
        .device_id = PCI_DEVICE_ID_INTEL_82801I_UHCI3,
        .revision  = 0x03,
        .irq_pin   = 2,
        .unplug    = false,
    },{
        .name      = "ich9-usb-uhci4", /* 00:1a.0 */
        .vendor_id = PCI_VENDOR_ID_INTEL,
        .device_id = PCI_DEVICE_ID_INTEL_82801I_UHCI4,
        .revision  = 0x03,
        .irq_pin   = 0,
        .unplug    = false,
    },{
        .name      = "ich9-usb-uhci5", /* 00:1a.1 */
        .vendor_id = PCI_VENDOR_ID_INTEL,
        .device_id = PCI_DEVICE_ID_INTEL_82801I_UHCI5,
        .revision  = 0x03,
        .irq_pin   = 1,
        .unplug    = false,
    },{
        .name      = "ich9-usb-uhci6", /* 00:1a.2 */
        .vendor_id = PCI_VENDOR_ID_INTEL,
        .device_id = PCI_DEVICE_ID_INTEL_82801I_UHCI6,
        .revision  = 0x03,
        .irq_pin   = 2,
        .unplug    = false,
    }
};

static void uhci_register_types(void)
{
    TypeInfo uhci_type_info = {
        .parent        = TYPE_PCI_UHCI,
        .class_init    = uhci_data_class_init,
    };
    int i;

    type_register_static(&uhci_pci_type_info);

    for (i = 0; i < ARRAY_SIZE(uhci_info); i++) {
        uhci_type_info.name = uhci_info[i].name;
        uhci_type_info.class_data = uhci_info + i;
        type_register(&uhci_type_info);
    }
}

type_init(uhci_register_types)
