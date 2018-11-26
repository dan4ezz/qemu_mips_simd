/*
 * QEMU DEC 21143 (Tulip) emulation -- sysbus adaptation (embedded Tulip)
 *
 * Copyright (C) 2012 Antony Pavlov, Mikhail Khropov
 * Copyright (C) 2013, 2014 Dmitry Smagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/qdev.h"
#include "hw/sysbus.h"
#include "net/net.h"
#include "sysemu/sysemu.h"
#include "sysemu/dma.h"
#include "qemu/timer.h"

#include "hw/nvram/eeprom93xx.h"
#include "hw/net/vg15-sysbus.h"
#include "vg15.h"
#include "tulip_mdio.h"

#define TYPE_SYSBUS_TULIP "sysbus-vg15"

#define SYSBUS_TULIP(obj) \
    OBJECT_CHECK(SysBusTulipState, (obj), TYPE_SYSBUS_TULIP)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    VG15State state;

    /* Memory access */
    AddressSpace *dma_as;
} SysBusTulipState;

void sysbus_vg15_register(enum VG15_Type type,
        NICInfo *nd, hwaddr addr, qemu_irq irq, AddressSpace *dma_as)
{
    SysBusDevice *sbd;
    DeviceState *dev;
    uint32_t devid;
    SysBusTulipState *s;

    devid = type == VG15e ? VG15E_ID : VG15M_ID;

    nd->model = (char *)TYPE_SYSBUS_TULIP;
    qemu_check_nic_model(nd, TYPE_SYSBUS_TULIP);

    dev = qdev_create(NULL, TYPE_SYSBUS_TULIP);
    qdev_set_nic_properties(dev, nd);
    s = SYSBUS_TULIP(dev);
    s->dma_as = dma_as;
    s->state.devid = devid;
    qdev_init_nofail(dev);

    sbd = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(sbd, 0, addr);
    sysbus_connect_irq(sbd, 0, irq);
}

static const VMStateDescription vmstate_etulip = {
    .name = "vg15",
    .version_id = 2,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_TIMER_PTR(timer, VG15State),
        VMSTATE_UINT32_ARRAY(mac_reg, VG15State, (VG15M_REGION_SIZE >> 2)),
        VMSTATE_UINT64(cur_tx_desc, VG15State),
        VMSTATE_UINT64(cur_rx_desc, VG15State),
        VMSTATE_INT32(tx_polling, VG15State),
        VMSTATE_END_OF_LIST()
     }
};

static uint64_t vg15_sysbus_read(void *opaque, hwaddr addr, unsigned size)
{
    unsigned int index = addr & (VG15M_REGION_SIZE - 1);
    VG15State *s = opaque;
    uint64_t ret;

    switch (index) {
    case 0x04:
        ret = s->devid;
        break;
    case 0x0c:
        ret = 0x02000021; /* PCI class (Network controller) & revision id */
        break;
    case 0x14:
        ret = 0x33107777; /* PCI subsystem vendor & device ids */
        break;
    default:
        switch (s->devid) {
        case VG15E_ID:
            ret = vg15e_read(opaque, addr, size);
            break;
        case VG15M_ID:
            ret = vg15m_read(opaque, addr, size);
            break;
        }
    }

    return ret;
}

static void vg15_sysbus_write(void *opaque, hwaddr addr, uint64_t val,
                              unsigned size)
{
    VG15State *s = opaque;

    switch (s->devid) {
    case VG15E_ID:
        vg15e_write(opaque, addr, val, size);
        break;
    case VG15M_ID:
        vg15m_write(opaque, addr, val, size);
        break;
    }
}

static const MemoryRegionOps sysbus_tulip_mmio_ops = {
    .read = vg15_sysbus_read,
    .write = vg15_sysbus_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void sysbus_physical_memory_write(void *dma_opaque, hwaddr addr,
                                      uint8_t *buf, int len)
{
    dma_memory_write(dma_opaque, addr, buf, len);
}

static void sysbus_physical_memory_read(void *dma_opaque, hwaddr addr,
                                     uint8_t *buf, int len)
{
    dma_memory_read(dma_opaque, addr, buf, len);
}

#if 0
/* FIXME: Move it to tulip.c ? */
static void sysbus_tulip_uninit(SysBusDevice *dev)
{
    SysBusTulipState *d = SYSBUS_TULIP(dev);

    qemu_free_irq(d->state.irq);
    timer_del(d->state.timer);
    timer_free(d->state.timer);
    /* eeprom93xx_free(&dev->qdev, d->state.eeprom); */
    qemu_del_nic(d->state.nic);
}
#endif

static NetClientInfo net_tulip_info = {
    .type                = NET_CLIENT_DRIVER_NIC,
    .size                = sizeof(NICState),
    .receive             = vg15_receive,
};

static const VMStateDescription vmstate_tulip_sysbus = {
    .name = "vg15",
    .version_id = 2,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_STRUCT(state, SysBusTulipState, 0, vmstate_etulip, VG15State),
        VMSTATE_END_OF_LIST()
    }
};

static int sysbus_tulip_init(SysBusDevice *dev)
{
    SysBusTulipState *d = SYSBUS_TULIP(dev);
    VG15State *s = &d->state;
    uint32_t region_size;

    region_size = s->devid == VG15E_ID ? VG15E_REGION_SIZE
                                       : VG15M_REGION_SIZE;

    memory_region_init_io(&s->mmio, OBJECT(d), &sysbus_tulip_mmio_ops, s,
                         "vg15-mmio", region_size);

    sysbus_init_mmio(dev, &s->mmio);

    sysbus_init_irq(dev, &s->irq);

    s->phys_mem_read = sysbus_physical_memory_read;
    s->phys_mem_write = sysbus_physical_memory_write;
    s->dma_opaque = d->dma_as;

    return vg15_init(DEVICE(dev), s, &net_tulip_info);
}

static void sysbus_tulip_reset(DeviceState *dev)
{
    SysBusTulipState *d = SYSBUS_TULIP(dev);
    vg15_reset(&d->state);
}

static Property sysbus_tulip_properties[] = {
    DEFINE_NIC_PROPERTIES(SysBusTulipState, state.conf),
    DEFINE_PROP_END_OF_LIST(),
};

static void sysbus_tulip_class_init(ObjectClass *cl, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(cl);
    SysBusDeviceClass *c = SYS_BUS_DEVICE_CLASS(cl);

    c->init = sysbus_tulip_init;
    dc->fw_name = "ethernet";
    dc->desc = "VG15";
    dc->reset = sysbus_tulip_reset;
    dc->vmsd  = &vmstate_tulip_sysbus;
    dc->props = sysbus_tulip_properties;
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
}

static const TypeInfo etulip_info = {
    .name          = TYPE_SYSBUS_TULIP,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SysBusTulipState),
    .class_init    = sysbus_tulip_class_init,
};

static void etulip_register(void)
{
    type_register_static(&etulip_info);
}

type_init(etulip_register);
