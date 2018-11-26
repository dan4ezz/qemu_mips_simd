/*
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
#include "qemu/error-report.h"
#include "hw/pci/msi.h"
#include "hw/pci/pcie_port.h"
#include "hw/pci/sc64_b42_nt_port.h"
#include "hw/misc/extcom.h"

#define NT_PORT_WIN_SETUP       0x0d0
#define NT_PORT_IR_SET          0xc4c
#define NT_PORT_IR_CLEAR        0xc50
#define NT_PORT_IM_SET          0xc54
#define NT_PORT_IM_CLEAR        0xc58
#define NT_PORT_SCRATCH         0xc6c

#define NT_PORT(td, virt, off)      (((td)->link) ? (virt) + (off) : (virt))

struct SC64B42NTPort {
    /*< private >*/
    PCIDevice parent_obj;

    uint32_t *regs_setup;
    uint32_t *regs_trans;

    /* BAR1, BAR2-BAR5 */
    ExtComWindow window[5];

    char *port;
    ExtCom *extcom;

    bool link;

    uint16_t irq;
};

enum {
    NT_EXTCOM_SCRATCH,
    NT_EXTCOM_WRITE,
    NT_EXTCOM_READ,
};

static bool sc64_nt_port_win_enabled(SC64B42NTPort *td, unsigned win)
{
    if (win == 0) {
        return true;
    }
    assert(win < ARRAY_SIZE(td->window));
    return !!(td->regs_setup[win] & (1 << 31));
}

static bool sc64_nt_port_win_is64bit(SC64B42NTPort *td, unsigned win)
{
    assert(win < ARRAY_SIZE(td->window));
    if (win == 1 || win == 3) {
        return !!((td->regs_setup[win] >> 1) & 0b10);
    }
    return false;
}

static bool sc64_nt_port_win_ispref(SC64B42NTPort *td, unsigned win)
{
    assert(win < ARRAY_SIZE(td->window));
    if (win == 0) {
        return !!(td->regs_setup[0] & (1 << 2));
    }
    return !!(td->regs_setup[win] & (1 << 3));
}

static int sc64_nt_port_win_type(SC64B42NTPort *td, unsigned win)
{
    assert(win < ARRAY_SIZE(td->window));
    if (win == 0) {
        return td->regs_setup[0] & 3;
    }
    return (td->regs_setup[win] >> 1) & 3;
}

static uint64_t sc64_nt_port_win_size(SC64B42NTPort *td, unsigned win,
                                                                    bool bit64)
{
    uint64_t size;

    assert(win < ARRAY_SIZE(td->window));

    if (bit64) {
        assert(win + 1 < ARRAY_SIZE(td->window));
        size = (uint64_t)1 << (31 + 12);
        size -= ((td->regs_setup[win + 1] & 0x7fffffff) << 12) |
                                        ((td->regs_setup[win] >> 20) & 0xfff);
    } else {
        size = (1 << 11) - ((td->regs_setup[win] >> 20) & 0x7ff);
    }
    return size * 1024 * 1024;
}

static void sc64_nt_port_handler(void *opaque, void *buf, int size)
{
    SC64B42NTPort *td = opaque;
    PCIDevice *d = &td->parent_obj;
    ExtComWindowReq req;
    uint8_t *type = buf;

    switch (*type) {
    case NT_EXTCOM_SCRATCH:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));
        pci_default_write_config(d, req.address, req.data, req.size);
        break;
    case NT_EXTCOM_WRITE:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));

        if (req.n > 0) {
            pci_dma_write(d, req.address, &req.data, req.size);
        } else {
            if (td->link) {
                d->config_write(d, req.address, req.data, 4);
            } else {
                PCIDevice *p = pci_bridge_get_device(d->bus);
                p = pci_bridge_get_device(p->bus);
                MemoryRegion *mr = p->io_regions[0].memory;

                req.data = bswap32(req.data);
                memory_region_dispatch_write(mr,
                            req.address, req.data, 4, MEMTXATTRS_UNSPECIFIED);
            }
        }
        break;
    case NT_EXTCOM_READ:
        assert(size == sizeof(req));
        memcpy(&req, buf, sizeof(req));

        if (req.n > 0) {
            pci_dma_read(d, req.address, &req.data, req.size);
        } else {
            if (td->link) {
                req.data = d->config_read(d, req.address, 4);
            } else {
                PCIDevice *p = pci_bridge_get_device(d->bus);
                p = pci_bridge_get_device(p->bus);
                MemoryRegion *mr = p->io_regions[0].memory;

                memory_region_dispatch_read(mr,
                            req.address, &req.data, 4, MEMTXATTRS_UNSPECIFIED);
                req.data = bswap32(req.data);
            }
        }

        extcom_send_ack(td->extcom, (void *)&req.data, sizeof(req.data));
        break;
    default:
        assert(false);
    }
}

static uint64_t sc64_nt_port_trans(SC64B42NTPort *td, int win, uint64_t addr)
{
    assert(win < ARRAY_SIZE(td->window));

    if (win == 0) {
        /* No translation for win 0 */
        return addr;
    }

    addr += td->regs_trans[win - 1];

    if (sc64_nt_port_win_is64bit(td, win)) {
        addr += td->regs_trans[win];
    }

    return addr;
}

uint64_t sc64_nt_port_link(SC64B42NTPort *td, hwaddr addr, uint64_t value,
                                                                    bool write)
{
    if (write) {
        extcom_win_write(td->extcom, NT_EXTCOM_WRITE, addr, 4, 0, value);
        return 0;
    } else {
        return extcom_win_read(td->extcom, NT_EXTCOM_READ, addr, 4, 0);
    }
    return 0;
}

static uint64_t sc64_nt_port_read_mem(void *opaque, hwaddr addr, unsigned size)
{
    ExtComWindow *ew = opaque;
    SC64B42NTPort *td = ew->owner;

    addr = sc64_nt_port_trans(td, ew->n, addr);

    if (ew->n == 0 && !td->link) {
        /* No BAR0/1 for virtual port */
        return -1;
    }

    return extcom_win_read(td->extcom, NT_EXTCOM_READ, addr, size, ew->n);
}

static void sc64_nt_port_write_mem(void *opaque, hwaddr addr,
                                                uint64_t value, unsigned size)
{
    ExtComWindow *ew = opaque;
    SC64B42NTPort *td = ew->owner;

    addr = sc64_nt_port_trans(td, ew->n, addr);

    if (ew->n == 0 && !td->link) {
        /* No BAR0/1 for virtual port */
        return;
    }

    extcom_win_write(td->extcom, NT_EXTCOM_WRITE, addr, size, ew->n, value);
}

const MemoryRegionOps sc64_nt_port_ops_mem = {
    .read = sc64_nt_port_read_mem,
    .write = sc64_nt_port_write_mem,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void sc64_nt_port_realize(PCIDevice *d, Error **errp)
{
    SC64B42NTPort *td = SC64_B42_NT_PORT(d);
    int mask = PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
    uint32_t setup_wmask[] = {
        0x00000003,
        0xfff0000e,
        0xffffffff,
        0xfff0000e,
        0xffffffff,
    };
    uint32_t trans_wmask[] = {
        0xfff00000,
        0xffffffff,
        0xfff00000,
        0xffffffff,
    };
    uint32_t irq_wmask[] = {
        0x0000ffff,
        0x0000ffff,
        0x0000ffff,
        0x0000ffff,
    };
    uint32_t irq_vals[] = {
        0x00000000,
        0x00000000,
        0x0000ffff,
        0x0000ffff,
    };
    int i;

    for (i = 0; i < ARRAY_SIZE(td->window); i++) {
        char buf[64];

        td->window[i].owner = td;
        td->window[i].n = i;

        snprintf(buf, sizeof(buf), "sc64-pcie-nt-bar%i", i);

        memory_region_init_io(&td->window[i].mr, OBJECT(td),
                                        &sc64_nt_port_ops_mem, &td->window[i],
                                        buf, 0);
        memory_region_set_enabled(&td->window[i].mr, false);
    }

    if (td->link) {
        pci_config_set_prog_interface(d->config, 1);
    }

    d->config[PCI_COMMAND] = mask;
    d->wmask[PCI_COMMAND] &= ~mask;

    if (pci_add_capability(d, PCI_CAP_ID_VNDR, 0xc8, 0x38, errp) < 0) {
        hw_error("Failed to initialize PCIe capability");
    }

    if (pcie_endpoint_cap_v1_init(d, 0x68) < 0) {
        hw_error("Failed to initialize PCIe capability");
    }

    assert(msi_init(d, 0x48, 2, true, true, NULL) == 0);

    /*
     * pcie_add_capability expects something at the beginning
     * of the extended cfg space (see hw/vfio/pci.c vfio_add_ext_cap too).
     */
    pci_set_long(d->config + PCI_CONFIG_SPACE_SIZE, PCI_EXT_CAP(0xFFFF, 0, 0));
    pci_set_long(d->wmask + PCI_CONFIG_SPACE_SIZE, 0);

    pcie_add_capability(d, PCI_EXT_CAP_ID_DSN, 1, 0x100, 8);
    pcie_add_capability(d, PCI_EXT_CAP_ID_VNDR, 1, 0xc34, 0x58);

    pci_set_long(d->config + 0xcc, 0x03800002);

    pci_set_long(d->config + 0xc38, 0x0580003);

    memcpy(&d->wmask[0xd0], setup_wmask, sizeof(setup_wmask));
    memcpy(&d->wmask[0xe4], setup_wmask, sizeof(setup_wmask));

    memcpy(&d->wmask[0xc3c], trans_wmask, sizeof(trans_wmask));

    td->regs_setup = (uint32_t *)&d->config[td->link ? 0xe4 : 0xd0];
    td->regs_trans = (uint32_t *)&d->config[0xc3c];

    memset(&d->wmask[NT_PORT_IR_SET], 0, 16 * 2);
    memcpy(&d->wmask[NT_PORT(td, NT_PORT_IR_SET, 16)],
                                                irq_wmask, sizeof(irq_wmask));
    memcpy(&d->config[NT_PORT(td, NT_PORT_IR_SET, 16)],
                                                irq_vals, sizeof(irq_vals));

    /* Scratch registers */
    memset(&d->wmask[0xc6c], 0xff, 8 * 4);

    /* INTx */
    memset(&d->wmask[0x3c], 0, 4);
    memset(&d->wmask[0x3d], 0xff, 1);
    d->config[0x3d] = 1;

    td->extcom = extcom_init(td->port, sc64_nt_port_handler, td);
}

static void sc64_nt_port_do_update(SC64B42NTPort *td,
                unsigned win, int bar, bool enabled, uint64_t size, int flags)
{
    assert(win < ARRAY_SIZE(td->window));

    MemoryRegion *w = &td->window[win].mr;
    PCIDevice *d = PCI_DEVICE(td);

    memory_region_set_enabled(w, enabled);
    memory_region_set_size(w, size);

    if (w->container) {
        memory_region_del_subregion(w->container, w);
    }

    pci_register_bar(d, bar, flags, w);
}

static void sc64_nt_port_update_bar01(SC64B42NTPort *td, unsigned win)
{
    int flags = PCI_BASE_ADDRESS_SPACE_MEMORY;
    uint64_t size = 128 * 1024;
    bool enabled = true;
    int bar = 0;

    assert(win == 0);

    if (sc64_nt_port_win_ispref(td, win)) {
        flags |= PCI_BASE_ADDRESS_MEM_PREFETCH;
    }

    switch (sc64_nt_port_win_type(td, win)) {
    case 0b01:
        error_report("Setting reserved bit");
    case 0b00:
        enabled = false;
        size = 0;
        break;
    case 0b10:
        flags |= PCI_BASE_ADDRESS_MEM_TYPE_32;
        break;
    case 0b11:
        flags |= PCI_BASE_ADDRESS_MEM_TYPE_64;
        break;
    }

    sc64_nt_port_do_update(td, win, bar, enabled, size, flags);
}

static void sc64_nt_port_update_bar_base(SC64B42NTPort *td, unsigned win)
{
    int flags = PCI_BASE_ADDRESS_SPACE_MEMORY;
    uint64_t size = 0;
    bool enabled = false;
    int bar = win == 1 ? 2 : 4;

    assert(win == 1 || win == 3);
    assert(win < ARRAY_SIZE(td->window));

    if (sc64_nt_port_win_ispref(td, win)) {
        flags |= PCI_BASE_ADDRESS_MEM_PREFETCH;
    }

    switch (sc64_nt_port_win_type(td, win)) {
    case 0b00:
        flags |= PCI_BASE_ADDRESS_MEM_TYPE_32;
        enabled = sc64_nt_port_win_enabled(td, win);
        if (enabled) {
            size = sc64_nt_port_win_size(td, win, false);
        }
        break;
    case 0b10:
        flags |= PCI_BASE_ADDRESS_MEM_TYPE_64;
        enabled = sc64_nt_port_win_enabled(td, win + 1);
        if (enabled) {
            size = sc64_nt_port_win_size(td, win, true);
        }
        break;
    case 0b01:
    case 0b11:
        error_report("Setting reserved bit");
        enabled = false;
        size = 0;
        break;
    }

    sc64_nt_port_do_update(td, win, bar, enabled, size, flags);
}

static void sc64_nt_port_update_bar_ext(SC64B42NTPort *td, unsigned win)
{
    int flags = PCI_BASE_ADDRESS_SPACE_MEMORY;
    uint64_t size = 0;
    bool enabled = false;
    int bar = win == 2 ? 3 : 5;

    assert(win == 2 || win == 4);
    assert(win < ARRAY_SIZE(td->window));

    if (sc64_nt_port_win_ispref(td, win)) {
        flags |= PCI_BASE_ADDRESS_MEM_PREFETCH;
    }

    switch (sc64_nt_port_win_type(td, win)) {
    case 0b00:
        flags |= PCI_BASE_ADDRESS_MEM_TYPE_32;
        if (sc64_nt_port_win_type(td, win - 1) == 0b00) {
            enabled = sc64_nt_port_win_enabled(td, win);
        }
        if (enabled) {
            size = sc64_nt_port_win_size(td, win, false);
        }
        break;
    case 0b10:
    case 0b01:
    case 0b11:
        error_report("Setting reserved bit");
        enabled = false;
        size = 0;
        break;
    }

    sc64_nt_port_do_update(td, win, bar, enabled, size, flags);
}

static void sc64_nt_port_update_win(SC64B42NTPort *td, int win)
{
    switch (win) {
    case 0:
        sc64_nt_port_update_bar01(td, win);
        break;
    case 1:
    case 3:
        sc64_nt_port_update_bar_base(td, win);
        break;
    case 2:
    case 4:
        sc64_nt_port_update_bar_ext(td, win);
        break;
    default:
        assert(false);
    }
}

static Property sc64_nt_port_properties[] = {
    DEFINE_PROP_STRING("port", SC64B42NTPort, port),
    DEFINE_PROP_BOOL("link", SC64B42NTPort, link, false),
    DEFINE_PROP_END_OF_LIST(),
};

static inline int pci_irq_disabled(PCIDevice *d)
{
    return pci_get_word(d->config + PCI_COMMAND) & PCI_COMMAND_INTX_DISABLE;
}

static void sc64_nt_port_update_irq(PCIDevice *d)
{
    SC64B42NTPort *td = SC64_B42_NT_PORT(d);
    uint16_t irq, mask, newirq;
    bool risen;

    irq = pci_get_word(d->config + NT_PORT(td, NT_PORT_IR_SET, 16));
    mask = pci_get_word(d->config + NT_PORT(td, NT_PORT_IM_SET, 16));
    newirq = irq & ~mask;
    risen = (td->irq | newirq) != td->irq;

    if (msi_enabled(d)) {
        if (risen) {
            msi_notify(d, msi_nr_vectors_allocated(d) - 1);
        }
    } else {
        pci_set_irq(d, newirq ? 1 : 0);
    }

    td->irq = newirq;
}

static void sc64_nt_port_ireg_set(PCIDevice *d, uint32_t address, uint32_t data)
{
    SC64B42NTPort *td = SC64_B42_NT_PORT(d);
    uint16_t val = pci_get_word(d->config + address);
    bool set = false;

    if (address == NT_PORT(td, NT_PORT_IR_SET, 16)) {
        set = true;
    } else if (address == NT_PORT(td, NT_PORT_IR_CLEAR, 16)) {
        address = NT_PORT(td, NT_PORT_IR_SET, 16);
    } else if (address == NT_PORT(td, NT_PORT_IM_SET, 16)) {
        set = true;
    } else if (address == NT_PORT(td, NT_PORT_IM_CLEAR, 16)) {
        address = NT_PORT(td, NT_PORT_IM_SET, 16);
    }

    if (set) {
        val |= data;
    } else {
        val &= ~data;
    }

    pci_set_word(d->config + address, val);
    pci_set_word(d->config + address + 4, val);

    sc64_nt_port_update_irq(d);
}

static void sc64_nt_port_cfg_write(PCIDevice *d, uint32_t address,
                                                        uint32_t data, int len)
{
    SC64B42NTPort *td = SC64_B42_NT_PORT(d);
    int i;

#define IN_RANGE(l, s, v)       ((l) <= (v) && (v) < (l) + (s))
    if (IN_RANGE(NT_PORT(td, NT_PORT_WIN_SETUP, 20), 20, address)) {
        pci_default_write_config(d, address, data, len);
        for (i = 0; i < ARRAY_SIZE(td->window); i++) {
            sc64_nt_port_update_win(td, i);
        }
    } else if (IN_RANGE(NT_PORT(td, NT_PORT_IR_SET, 16), 16, address)) {
        sc64_nt_port_ireg_set(d, address, data);
    } else if (IN_RANGE(NT_PORT_SCRATCH, 32, address)) {
        pci_default_write_config(d, address, data, len);
        extcom_win_write(td->extcom, NT_EXTCOM_SCRATCH, address, len, 0, data);
    } else {
        pci_default_write_config(d, address, data, len);
    }
#undef RANGE
}

static void sc64_nt_port_reset(DeviceState *dev)
{
    PCIDevice *d = PCI_DEVICE(dev);
    SC64B42NTPort *td = SC64_B42_NT_PORT(d);

    d->config_write(d, NT_PORT(td, NT_PORT_WIN_SETUP, 20), 2, 4);

    td->irq = 0;
}

static void sc64_nt_port_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->config_write = sc64_nt_port_cfg_write;
    k->realize = sc64_nt_port_realize;
    k->revision = 0;
    k->is_express = 1;
    k->vendor_id = 0x191e;
    k->device_id = 0x5a52;
    k->class_id = PCI_CLASS_BRIDGE_OTHER;
    dc->props = sc64_nt_port_properties;
    dc->reset = sc64_nt_port_reset;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
}

static const TypeInfo sc64_nt_port = {
    .name          = TYPE_SC64_B42_NT_PORT,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(SC64B42NTPort),
    .class_init    = sc64_nt_port_class_init,
};

static void sc64_nt_port_register_types(void)
{
    type_register_static(&sc64_nt_port);
}

type_init(sc64_nt_port_register_types)
