/*
 * Copyright (C) 2017 Denis Deryugin <deryugin.denis@gmail.com>
 * Copyright (C) 2017 Alexander Kalmuk <alexkalmuk@gmail.com>
 * Copyright (C) 2018 Aleksey Kuleshov <rndfax@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/hw.h"
#include "hw/misc/extcom.h"
#include "hw/pci-host/sc64-pci.h"
#include "hw/pci/msix.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci/pci_host.h"
#include "hw/pci/pcie_host.h"
#include "hw/sysbus.h"
#include "qapi/error.h"
#include "qemu/typedefs.h"
#include "qom/object.h"

typedef struct {
    /*< private >*/
    PCIExpressHost parent_obj;
    /*< public >*/

    qemu_irq irq[4];
    ExtCom *extcom;
} SC64PCIEHost;

#define TYPE_SC64_PCIE_HOST "sc64-pcie-host"
#define SC64_PCIE_HOST(obj) \
    OBJECT_CHECK(SC64PCIEHost, (obj), TYPE_SC64_PCIE_HOST)

typedef struct {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    MemoryRegion *msix;
    SC64PCIEHost *host;
} SC64PCIEBridge;

#define TYPE_SC64_PCIE_BRIDGE "sc64-pcie-bridge"
#define SC64_PCIE_BRIDGE(obj) \
    OBJECT_CHECK(SC64PCIEBridge, (obj), TYPE_SC64_PCIE_BRIDGE)

static void sc64_pcie_br_realize(PCIDevice *d, Error **errp)
{
    SC64PCIEBridge *td = SC64_PCIE_BRIDGE(d);
    int rc, i;

    rc = msix_init(d, 32, td->msix, 0, 0x1000, td->msix, 0, 0x1800, 0xa0, NULL);
    assert(rc == 0);

    for (i = 0; i < 32; i++) {
        assert(msix_vector_use(d, i) == 0);
    }

    if (pcie_endpoint_cap_init(d, 0x60) < 0) {
        hw_error("Failed to initialize PCIe capability");
    }
}

static void sc64_pcie_br_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->realize = sc64_pcie_br_realize;
    k->revision = 0;
    k->is_express = 1;
    k->vendor_id = 0x191e;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
}

static const TypeInfo sc64_pcie_br_info = {
    .name          = TYPE_SC64_PCIE_BRIDGE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(SC64PCIEBridge),
    .class_init    = sc64_pcie_br_class_init,
};

enum {
    SC64EP_EXTCOM_BAR_UPDATE,
    SC64EP_EXTCOM_CFG_WRITE,
    SC64EP_EXTCOM_CFG_READ,
    SC64EP_EXTCOM_MEM_WRITE,
    SC64EP_EXTCOM_MEM_READ,
    SC64EP_EXTCOM_IO_WRITE,
    SC64EP_EXTCOM_IO_READ,
};

typedef struct {
    uint8_t type;
    uint8_t bar;
    uint32_t flags;
    uint64_t size;
} SC64EPBarUpdateReq;

static void sc64_pcie_br_handler(void *opaque, void *buf, int size)
{
    SC64PCIEBridge *br = opaque;
    PCIDevice *d = PCI_DEVICE(br);
    ExtCom *extcom = br->host->extcom;
    ExtComWindowReq req;
    uint8_t *type = buf;

    switch (*type) {
    case SC64EP_EXTCOM_MEM_READ:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));

        pci_dma_read(d, req.address, &req.data, req.size);
        extcom_send_ack(extcom, (void *)&req.data, sizeof(req.data));
        break;
    case SC64EP_EXTCOM_MEM_WRITE:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));

        pci_dma_write(d, req.address, &req.data, req.size);
        break;
    case SC64EP_EXTCOM_CFG_WRITE:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));

        pci_default_write_config(d, req.address, req.data, 4);
        break;
    case SC64EP_EXTCOM_CFG_READ:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));

        req.data = pci_default_read_config(d, req.address, req.size);
        extcom_send_ack(extcom, (void *)&req.data, sizeof(req.data));
        break;
    default:
        assert(false);
    }
}

static void sc64_pcie_br_space_updated(void *opaque, int bar, uint64_t size,
                                                                    int flags)
{
    SC64PCIEHost *host = opaque;
    SC64EPBarUpdateReq req = {
        .type = SC64EP_EXTCOM_BAR_UPDATE,
        .bar = bar,
        .flags = flags,
        .size = size,
    };
    extcom_send(host->extcom, (void *)&req, sizeof(req));
}

static uint64_t sc64_extcom_read_mem(void *opaque, hwaddr addr, unsigned size)
{
    SC64PCIEHost *host = opaque;
    return extcom_win_read(host->extcom, SC64EP_EXTCOM_MEM_READ, addr, size, 0);
}

static void sc64_extcom_write_mem(void *opaque, hwaddr addr, uint64_t value,
                                                                unsigned size)
{
    SC64PCIEHost *host = opaque;
    extcom_win_write(host->extcom,
                                SC64EP_EXTCOM_MEM_WRITE, addr, size, 0, value);
}

const MemoryRegionOps sc64_extcom_ops_mem = {
    .read = sc64_extcom_read_mem,
    .write = sc64_extcom_write_mem,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t sc64_extcom_read_io(void *opaque, hwaddr addr, unsigned size)
{
    SC64PCIEHost *host = opaque;
    return extcom_win_read(host->extcom, SC64EP_EXTCOM_IO_READ, addr, size, 0);
}

static void sc64_extcom_write_io(void *opaque, hwaddr addr, uint64_t value,
                                                                unsigned size)
{
    SC64PCIEHost *host = opaque;
    extcom_win_write(host->extcom,
                                SC64EP_EXTCOM_IO_WRITE, addr, size, 0, value);
}

const MemoryRegionOps sc64_extcom_ops_io = {
    .read = sc64_extcom_read_io,
    .write = sc64_extcom_write_io,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static AddressSpace *sc64_pcie_host_set_iommu(PCIBus *bus, void *opaque,
                                             int devfn)
{
    return opaque;
}

static void sc64_pcie_host_set_irq(void *opaque, int irq_num, int level)
{
    SC64PCIEHost *d = opaque;
    qemu_irq *pic = d->irq;

    assert(0 <= irq_num && irq_num < 4);

    qemu_set_irq(pic[irq_num], level);
}

static int sc64_pcie_host_map_irq(PCIDevice *pci_dev, int irq_num)
{
    /* FIXME Interrupt mapping is board specific */
    return 0;
}

static void sc64_pcie_host_cfg_write(void *opaque, hwaddr addr, uint64_t val,
                                                                unsigned size)
{
    PCIHostState *phb = opaque;
    pci_data_write(phb->bus, addr, val, size);
}

static uint64_t sc64_pcie_host_cfg_read(void *opaque, hwaddr addr,
                                                                unsigned size)
{
    PCIHostState *phb = opaque;
    return pci_data_read(phb->bus, addr, size);
}

static const MemoryRegionOps sc64_pcie_host_cfg_ops = {
    .read = sc64_pcie_host_cfg_read,
    .write = sc64_pcie_host_cfg_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static PCIHostState *sc64_pcie_br_register(const struct sc64_pcie_struct *ps,
                                                                bool isdevice)
{
    DeviceState *dev = qdev_create(NULL, TYPE_SC64_PCIE_HOST);
    PCIHostState *phb = PCI_HOST_BRIDGE(dev);
    SC64PCIEHost *d = SC64_PCIE_HOST(dev);
    PCIDevice *pd;
    SC64PCIEBlock *blk;
    SC64PCIEBridge *br;

    if (isdevice) {
        blk = sc64_pcie_block_register_ops(ps,
                                sc64_pcie_br_space_updated, d,
                                &sc64_extcom_ops_mem,
                                &sc64_extcom_ops_io,
                                d);
    } else {
        blk = sc64_pcie_block_register(ps);
    }

    phb->bus = pci_register_bus(dev, ps->name,
                                sc64_pcie_host_set_irq, sc64_pcie_host_map_irq,
                                d,
                                &blk->pci_mem,
                                &blk->pci_io,
                                PCI_DEVFN(0, 0), 4, TYPE_PCIE_BUS);
    qdev_init_nofail(dev);

    pd = pci_create(phb->bus, PCI_DEVFN(1, 0), TYPE_SC64_PCIE_BRIDGE);
    br = SC64_PCIE_BRIDGE(pd);
    br->msix = &blk->pci_reg_host;
    br->host = d;
    qdev_init_nofail(&pd->qdev);

    blk->bridge = pd;

    pci_register_bar(pd, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &blk->in_spaces[0]);

    pci_setup_iommu(phb->bus, sc64_pcie_host_set_iommu, &blk->pci_dma_as);

    d->irq[0] = ps->irq[0];
    d->irq[1] = ps->irq[1];
    d->irq[2] = ps->irq[2];
    d->irq[3] = ps->irq[3];

    if (isdevice) {
        pci_config_set_class(pd->config, PCI_CLASS_PROCESSOR_MIPS);
        pci_config_set_device_id(pd->config, 0x5a41);
        d->extcom = extcom_init(ps->name, sc64_pcie_br_handler, br);
    } else {
        pci_config_set_class(pd->config, PCI_CLASS_BRIDGE_HOST);
        pci_config_set_device_id(pd->config, 0x5a42);

        memory_region_init_io(&phb->conf_mem, OBJECT(dev),
                                            &sc64_pcie_host_cfg_ops, phb,
                                            "sc64-pci-legacy-conf", 0x1000000);
    }

    return phb;
}

void sc64_pcie_host_register(const struct sc64_pcie_struct *ps,
                            hwaddr leg_conf, hwaddr ext_conf, uint32_t ext_size)
{
    PCIHostState *phb = sc64_pcie_br_register(ps, false);

    /* Set up PCI configuration space (legacy) */
    memory_region_add_subregion(get_system_memory(), leg_conf, &phb->conf_mem);

    /* Set up PCIe ECAM */
    pcie_host_mmcfg_map(PCIE_HOST_BRIDGE(phb), ext_conf, ext_size);
}

void sc64_pcie_ep_register(const struct sc64_pcie_struct *ps)
{
    sc64_pcie_br_register(ps, true);
}

static void sc64_pcie_host_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "SC64 PCIe host controller";
}

static const TypeInfo sc64_pcie_host_info = {
    .name          = TYPE_SC64_PCIE_HOST,
    .parent        = TYPE_PCIE_HOST_BRIDGE,
    .instance_size = sizeof(SC64PCIEHost),
    .class_init    = sc64_pcie_host_class_init,
};

static void sc64_pcie_register_types(void)
{
    type_register_static(&sc64_pcie_host_info);
    type_register_static(&sc64_pcie_br_info);
}

type_init(sc64_pcie_register_types)

typedef struct {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    ExtComWindow window[3];

    char *port;
    ExtCom *extcom;

    MemoryRegion msix;
} SC64PCIEEP;

#define TYPE_SC64_PCIE_EP "sc64-pcie-ep"
#define SC64_PCIE_EP(obj) \
    OBJECT_CHECK(SC64PCIEEP, (obj), TYPE_SC64_PCIE_EP)

static void sc64_pcie_ep_handler(void *opaque, void *buf, int size)
{
    SC64PCIEEP *td = opaque;
    PCIDevice *d = &td->parent_obj;
    ExtComWindowReq req;
    SC64EPBarUpdateReq bureq;
    uint8_t type = *(uint8_t *)buf;
    int bar;

    switch (type) {
    case SC64EP_EXTCOM_BAR_UPDATE:
        assert(size == sizeof(bureq));
        memcpy(&bureq, buf, sizeof(bureq));

        bar = bureq.bar;
        assert(bar == 1 || bar == 2);
        memory_region_set_size(&td->window[bar].mr, bureq.size);
        pci_register_bar(&td->parent_obj, bar, bureq.flags,
                                                        &td->window[bar].mr);

        break;
    case SC64EP_EXTCOM_MEM_READ:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));

        pci_dma_read(d, req.address, &req.data, req.size);
        extcom_send_ack(td->extcom, (void *)&req.data, sizeof(req.data));
        break;
    case SC64EP_EXTCOM_MEM_WRITE:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));

        pci_dma_write(d, req.address, &req.data, req.size);
        break;
    case SC64EP_EXTCOM_IO_READ:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));

        address_space_read(&address_space_io, req.address,
                        MEMTXATTRS_UNSPECIFIED, (void *)&req.data, req.size);
        extcom_send_ack(td->extcom, (void *)&req.data, sizeof(req.data));
        break;
    case SC64EP_EXTCOM_IO_WRITE:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));

        address_space_write(&address_space_io, req.address,
                        MEMTXATTRS_UNSPECIFIED, (void *)&req.data, req.size);
        break;
    default:
        assert(false);
    }
}

static hwaddr sc64_pcie_ep_addr(SC64PCIEEP *td, hwaddr addr, int n)
{
    return addr + pci_get_bar_addr(PCI_DEVICE(td), n);
}

static uint64_t sc64_pcie_ep_read_mem(void *opaque, hwaddr addr, unsigned size)
{
    ExtComWindow *ew = opaque;
    SC64PCIEEP *td = ew->owner;
    addr = sc64_pcie_ep_addr(td, addr, ew->n);
    return extcom_win_read(td->extcom,
                                    SC64EP_EXTCOM_MEM_READ, addr, size, ew->n);
}

static void sc64_pcie_ep_write_mem(void *opaque, hwaddr addr,
                                                uint64_t value, unsigned size)
{
    ExtComWindow *ew = opaque;
    SC64PCIEEP *td = ew->owner;
    addr = sc64_pcie_ep_addr(td, addr, ew->n);
    extcom_win_write(td->extcom,
                            SC64EP_EXTCOM_MEM_WRITE, addr, size, ew->n, value);
}

const MemoryRegionOps sc64_pcie_ep_ops_mem = {
    .read = sc64_pcie_ep_read_mem,
    .write = sc64_pcie_ep_write_mem,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void sc64_pcie_ep_realize(PCIDevice *d, Error **errp)
{
    SC64PCIEEP *td = SC64_PCIE_EP(d);
    int i;

    for (i = 0; i < 3; i++) {
        td->window[i].owner = td;
        td->window[i].n = i;
    }

    memory_region_init_io(&td->window[0].mr, OBJECT(td),
                                        &sc64_pcie_ep_ops_mem, &td->window[0],
                                        "sc64-pcie-br-bar0", 0x2000);
    memory_region_init_io(&td->window[1].mr, OBJECT(td),
                                        &sc64_pcie_ep_ops_mem, &td->window[1],
                                        "sc64-pcie-br-bar1", 0x10000000ULL);
    memory_region_init_io(&td->window[2].mr, OBJECT(td),
                                        &sc64_pcie_ep_ops_mem, &td->window[2],
                                        "sc64-pcie-br-bar2", 0x10000000ULL);

    memory_region_init(&td->msix, OBJECT(td), "msix", 0x2000);

    pci_register_bar(&td->parent_obj, 0,
                            PCI_BASE_ADDRESS_SPACE_MEMORY, &td->window[0].mr);

    td->extcom = extcom_init(td->port, sc64_pcie_ep_handler, td);
}

static Property sc64_pcie_ep_properties[] = {
    DEFINE_PROP_STRING("port", SC64PCIEEP, port),
    DEFINE_PROP_END_OF_LIST(),
};

static void sc64_pcie_ep_cfg_write(PCIDevice *d,
                                        uint32_t addr, uint32_t data, int len)
{
    SC64PCIEEP *td = SC64_PCIE_EP(d);

    extcom_win_write(td->extcom, SC64EP_EXTCOM_CFG_WRITE, addr, len, 0, data);
    pci_default_write_config(d, addr, data, len);
}

static uint32_t sc64_pcie_ep_cfg_read(PCIDevice *d, uint32_t addr, int len)
{
    SC64PCIEEP *td = SC64_PCIE_EP(d);

    return extcom_win_read(td->extcom, SC64EP_EXTCOM_CFG_READ, addr, len, 0);
}

static void sc64_pcie_ep_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->config_write = sc64_pcie_ep_cfg_write;
    k->config_read = sc64_pcie_ep_cfg_read;
    k->realize = sc64_pcie_ep_realize;
    k->revision = 0;
    k->is_express = 1;
    dc->props = sc64_pcie_ep_properties;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
}

static const TypeInfo sc64_pcie_ep_info = {
    .name          = TYPE_SC64_PCIE_EP,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(SC64PCIEEP),
    .class_init    = sc64_pcie_ep_class_init,
};

static void sc64_pcie_ep_register_types(void)
{
    type_register_static(&sc64_pcie_ep_info);
}

type_init(sc64_pcie_ep_register_types)
