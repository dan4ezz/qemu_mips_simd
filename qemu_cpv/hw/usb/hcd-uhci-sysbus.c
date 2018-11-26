/*
 * USB UHCI controller emulation (as SYSBUS device)
 *
 * Copyright (C) 2017 Anton Bondarev <anton.bondarev2310@gmail.com>
 * Copyright (C) 2017 Alexander Kalmuk <alexkalmuk@gmail.com>
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
#include "hw/sysbus.h"
#include "qapi/error.h"
#include "qemu/timer.h"
#include "qemu/iov.h"
#include "sysemu/dma.h"
#include "trace.h"
#include "qemu/main-loop.h"

#include "hcd-uhci.h"

typedef struct UHCISysBusState UHCISysBusState;

typedef struct UHCISysBusState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/
    UHCIState uhci;
} UHCISysBusState;

#define TYPE_SYS_BUS_UHCI "sysbus-uhci-usb"
#define SYS_BUS_UHCI(obj) OBJECT_CHECK(UHCISysBusState, (obj), TYPE_SYS_BUS_UHCI)

const VMStateDescription vmstate_uhci_sysbus = {
    .name = "uhci-sysbus",
    .version_id = 3,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT(uhci, UHCISysBusState, 2, vmstate_uhci, UHCIState),
        VMSTATE_END_OF_LIST()
    }
};

void usb_uhci_sysbus_init(DeviceState *uhci_dev, AddressSpace *dma_as)
{
    UHCIState *s = &SYS_BUS_UHCI(uhci_dev)->uhci;
    s->dma_as = dma_as;
}

static void uhci_sysbus_reset(DeviceState *dev)
{
    UHCISysBusState *d = SYS_BUS_UHCI(dev);
    UHCIState *s = &d->uhci;

    usb_uhci_reset(s);
}

static Property uhci_properties_companion[] = {
    DEFINE_PROP_STRING("masterbus", UHCISysBusState, uhci.masterbus),
    DEFINE_PROP_UINT32("firstport", UHCISysBusState, uhci.firstport, 0),
    DEFINE_PROP_UINT32("bandwidth", UHCISysBusState, uhci.frame_bandwidth, 1280),
    DEFINE_PROP_UINT32("maxframes", UHCISysBusState, uhci.maxframes, 128),
    DEFINE_PROP_END_OF_LIST(),
};

static void uhci_reset(DeviceState *dev)
{
    SysBusDevice *d = SYS_BUS_DEVICE(dev);
    UHCISysBusState *s = SYS_BUS_UHCI(d);

    usb_uhci_reset(&s->uhci);
}

static void uhci_sysbus_init(Object *obj)
{
    UHCISysBusState *i = SYS_BUS_UHCI(obj);
    UHCIState *s = &i->uhci;

    s->uhci_dev_reset = uhci_reset;

    usb_uhci_init(s, DEVICE(obj));
}

static void usb_uhci_sysbus_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *d = SYS_BUS_DEVICE(dev);
    UHCISysBusState *i = SYS_BUS_UHCI(dev);
    UHCIState *s = &i->uhci;

    usb_uhci_realize(s, dev, errp);
    sysbus_init_mmio(d, &s->mem);

    sysbus_init_irq(d, &s->irq);
}

static void uhci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = usb_uhci_sysbus_realize;
    dc->vmsd = &vmstate_uhci_sysbus;
    dc->reset = uhci_sysbus_reset;
    set_bit(DEVICE_CATEGORY_USB, dc->categories);
}

static const TypeInfo uhci_sysbus_type_info = {
    .name = TYPE_SYS_BUS_UHCI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(UHCISysBusState),
    .instance_init = uhci_sysbus_init,
    .class_size    = sizeof(SysBusDeviceClass),
    .abstract = true,
    .class_init = uhci_class_init,
};

static void uhci_sysbus_data_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->hotpluggable = false;
    dc->props = uhci_properties_companion;
}

#define TYPE_SRISA_UHCI "sc64-uhci"

#define SRISA_UHCI(obj) \
    OBJECT_CHECK(SRISAUHCIState, (obj), TYPE_SRISA_UHCI)

typedef struct SRISAUHCIState {
    /*< private >*/
    UHCISysBusState parent_obj;
    /*< public >*/

    MemoryRegion mem_vendor;
    AddressSpace dma_as;
} SRISAUHCIState;

static uint64_t srisa_uhci_read(void *opaque, hwaddr addr, unsigned size)
{
    switch (addr & 0xf) {
    case 0:
        return 0x3380191e;
    case 4:
        return 0x0c030000;
    case 0xc:
        return 0x11072800;
    }
    return 0;
}

static const MemoryRegionOps srisa_uhci_mmio_ops = {
    .read = srisa_uhci_read,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void srisa_uhci_init(Object *obj)
{
    UHCISysBusState *i = SYS_BUS_UHCI(obj);
    UHCIState *s = &i->uhci;
    SRISAUHCIState *f = SRISA_UHCI(obj);

    memory_region_set_size(&s->mem, 0x200);
    memory_region_init_io(&f->mem_vendor, obj, &srisa_uhci_mmio_ops, s, "srisa", 0x10);
    memory_region_add_subregion(&s->mem, 0xF0, &f->mem_vendor);
}

static TypeInfo uhci_sc64_type_info = {
    .name = TYPE_SRISA_UHCI,
    .parent = TYPE_SYS_BUS_UHCI,
    .instance_init = srisa_uhci_init,
    .instance_size = sizeof(SRISAUHCIState),
    .class_init = uhci_sysbus_data_class_init,
};

static void uhci_register_types(void)
{
    type_register_static(&uhci_sysbus_type_info);
    type_register_static(&uhci_sc64_type_info);
}

type_init(uhci_register_types)
