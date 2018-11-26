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
#include "hw/net/tulip-sysbus.h"
#include "tulip.h"
#include "tulip_mdio.h"

#define TYPE_SYSBUS_TULIP "sysbus-tulip"

#define SYSBUS_TULIP(obj) \
    OBJECT_CHECK(SysBusTulipState, (obj), TYPE_SYSBUS_TULIP)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    TulipState state;

    /* Memory access */
    AddressSpace *dma_as;
} SysBusTulipState;

void sysbus_tulip_register(hwaddr addr, qemu_irq irq, AddressSpace *dma_as)
{
    NICInfo *nd = &nd_table[0];
    SysBusDevice *sbd;
    DeviceState *dev;
    SysBusTulipState *s;

    nd->model = (char *)TYPE_SYSBUS_TULIP;
    qemu_check_nic_model(nd, TYPE_SYSBUS_TULIP);

    dev = qdev_create(NULL, TYPE_SYSBUS_TULIP);
    qdev_set_nic_properties(dev, nd);
    s = SYSBUS_TULIP(dev);
    s->dma_as = dma_as;
    qdev_init_nofail(dev);

    sbd = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(sbd, 0, addr);
    sysbus_connect_irq(sbd, 0, irq);
}

static const VMStateDescription vmstate_etulip = {
    .name = "tulip",
    .version_id = 2,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_TIMER_PTR(timer, TulipState),
        VMSTATE_UINT32_ARRAY(mac_reg, TulipState, (TULIP_CSR_REGION_SIZE >> 2)),
        VMSTATE_UINT32(cur_tx_desc, TulipState),
        VMSTATE_UINT32(cur_rx_desc, TulipState),
        VMSTATE_INT32(tx_polling, TulipState),
        VMSTATE_END_OF_LIST()
     }
};

static uint64_t tulip_sysbus_csr_read(void *opaque, hwaddr addr, unsigned size)
{
    unsigned int index = addr & (TULIP_CSR_REGION_SIZE - 1);
    uint64_t ret;

    switch (index) {
    case 0x04:
        ret =    0x191e         /* PCI vendor id */
              | (0x3311 << 16); /* PCI device id */
        break;
    case 0x0c:
        ret = 0x02000021; /* PCI class (Network controller) & revision id */
        break;
    case 0x14:
        ret = 0x33107777; /* PCI subsystem vendor & device ids */
        break;
    default:
        ret = tulip_csr_read(opaque, addr, size);
    }

    return ret;
}


static const MemoryRegionOps sysbus_tulip_mmio_ops = {
    .read = tulip_sysbus_csr_read,
    .write = tulip_csr_write,
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


static void sysbus_tulip_set_link_status(NetClientState *nc)
{
    (void) nc;
#if 0
    TulipState *s = DO_UPCAST(NICState, nc, nc)->opaque;
    uint32_t old_status = s->mac_reg[STATUS];

    if (nc->link_down) {
        s->mac_reg[STATUS] &= ~ETULIP_STATUS_LU;
    } else {
        s->mac_reg[STATUS] |= ETULIP_STATUS_LU;
    }

    if (s->mac_reg[STATUS] != old_status) {
        set_ics(s, 0, ETULIP_ICR_LSC);
    }
#endif
}

static NetClientInfo net_tulip_info = {
    .type                = NET_CLIENT_DRIVER_NIC,
    .size                = sizeof(NICState),
    .receive             = tulip_receive,
    .link_status_changed = sysbus_tulip_set_link_status,
};

static const VMStateDescription vmstate_tulip_sysbus = {
    .name = "tulip",
    .version_id = 2,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_STRUCT(state, SysBusTulipState, 0, vmstate_etulip, TulipState),
        VMSTATE_END_OF_LIST()
    }
};

static int sysbus_tulip_init(SysBusDevice *dev)
{
    SysBusTulipState *d = SYSBUS_TULIP(dev);
    TulipState *s = &d->state;

    memory_region_init_io(&s->mmio, OBJECT(d), &sysbus_tulip_mmio_ops, s,
                         "tulip-mmio", TULIP_CSR_REGION_SIZE);

    sysbus_init_mmio(dev, &s->mmio);

    sysbus_init_irq(dev, &s->irq);

    s->phys_mem_read = sysbus_physical_memory_read;
    s->phys_mem_write = sysbus_physical_memory_write;
    s->dma_opaque = d->dma_as;

    tulip_init(DEVICE(dev), s, &net_tulip_info);
    return 0;
}

static void sysbus_tulip_reset(DeviceState *dev)
{
    SysBusTulipState *d = SYSBUS_TULIP(dev);
    tulip_reset(&d->state);
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
    dc->desc = "DEC 21143 Tulip";
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
