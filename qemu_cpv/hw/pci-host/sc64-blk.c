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
#include "hw/pci/msix.h"
#include "hw/pci-host/sc64-pci.h"

#define TYPE_SC64_PCIE_BLOCK "sc64-pcie-block"
#define SC64_PCIE_BLOCK(obj) \
    OBJECT_CHECK(SC64PCIEBlock, (obj), TYPE_SC64_PCIE_BLOCK)

#define REG_ID                      0x0800
#define PCI_REG_HOST_CTRL           0x0804
#define PCI_HOST_TIMEOUT_MWR        0x0810
#define PCI_HOST_TIMEOUT_MWR_STATUS 0x0814
#define PCI_HOST_TIMEOUT_MRD        0x0818
#define PCI_HOST_TIMEOUT_MRD_STATUS 0x081C
#define PCI_HOST_TIMEOUT_SWR        0x0820
#define PCI_HOST_TIMEOUT_SWR_STATUS 0x0824
#define PCI_HOST_TIMEOUT_SRD        0x0828
#define PCI_HOST_TIMEOUT_SRD_STATUS 0x082C
#define DOORBELL_IN                 0x0900
#define DOORBELL_IN_END             0x097C
#define PCI_INT_FLAG                0x0980
#define PCI_INT_MASK                0x0984
#define DOORBELL_OUT                0x0990
#define MSIPENDING_CLR              0x0998
#define MSIXTBL                     0x09A0
#define MSIXTBLBIR                  0x09A4
#define MSIXPBABIR                  0x09A8
#define HOST_INT_FLAG               0x0B00
#define HOST_INT_EN                 0x0B04
#define HOSTOUTREG                  0x0C00 /* ADDRL, ADDRH, BAR, ATTR */
#define HOSTOUTADDRL(n)             (HOSTOUTREG + (n) * 0x10)
#define HOSTOUTADDRH(n)             (HOSTOUTREG + (n) * 0x10 + 4)
#define HOSTOUTBAR(n)               (HOSTOUTREG + (n) * 0x10 + 8)
#define HOSTOUTATTR(n)              (HOSTOUTREG + (n) * 0x10 + 12)
#define HOSTOUTATTR_EN              (1 << 31)
#define WIN_TYPE(n)                 (((n) >> 20) & 0x3)
#define HOSTOUTREG_END              0x0C7C
#define HOSTINREG                   0x0D00
#define HOSTINADDR(n)               (HOSTINREG + (n) * 0x10)
#define HOSTINBAR(n)                (HOSTINREG + (n) * 0x10 + 8)
#define HOSTINATTR(n)               (HOSTINREG + (n) * 0x10 + 12)
#define HOSTINATTR_EN               (1 << 31)
#define HOSTINATTR_BASE             (1 << 30)
#define HOSTINATTR_PREF             (1 << 29)
#define HOSTINREG_END               0x0D84
#define HOSTINATTR_COM              0x0D8C
#define HOSTINATTR_COM_EN           (1 << 31)
#define HOSTINATTR_COM_BASE         (1 << 30)
#define HOSTINATTR_COM_PREF         (1 << 29)
#define MSI_IRQ_TBL                 0x1000
#define MSI_IRQ_TBL_END             0x11FC
#define MSIPENDING                  0x1800

/* Bits 0-19 in HOSTINATTRx, HOSTOUTATTRx, HOSTINATTR_COM */
#define HOSTWINDOW_SIZE             0xFFFFF

#define WINDOW(addr)    (((addr) >> 4) & 0xF)

#define REG(blk, reg)   ((blk)->regs[(reg) / 4])

/*
 * attr is HOSTINATTRx, HOSTOUTATTRx, HOSTINATTR_COM
 *
 * Bits 0-19 contain size of the window (VAL):
 * FFFFF - 4KB
 * FFFFE - 8KB
 * FFFFC - 16KB
 *      ...
 * 00000 - 4Gb
 * One can see that:
 * 0x100000 - 0xFFFFF = 2^0
 * 0x100000 - 0xFFFFE = 2^1
 * 0x100000 - 0xFFFFC = 2^2
 *         ...
 * The resulting formula is:
 * (0x100000 - VAL) * 4KB
 */
static uint64_t sc64_get_window_size(uint32_t attr)
{
    return ((uint64_t)(0x100000 - (attr & HOSTWINDOW_SIZE))) * 0x1000;
}

/* n is a number of OUT window - 0:7 */
static void sc64_pcie_update_outwindow(SC64PCIEBlock *blk, int n)
{
    assert(n < SC64_PCIE_WINDOWS_CNT);

    uint32_t attrs = REG(blk, HOSTOUTATTR(n));
    MemoryRegion *win = WIN_TYPE(attrs) == 2 ?
                                &blk->out_windows_io[n] : &blk->out_windows[n];
    uint64_t size = sc64_get_window_size(attrs);
    uint64_t addr = (((uint64_t) REG(blk, HOSTOUTADDRH(n))) << 32) |
                    REG(blk, HOSTOUTADDRL(n));
    uint32_t bar = REG(blk, HOSTOUTBAR(n));
    int s_nr = n >= SC64_PCIE_SPACES_CNT ? SC64_PCIE_SPACES_CNT - 1 : n;

    assert(s_nr < SC64_PCIE_SPACES_CNT);

    memory_region_set_enabled(&blk->out_windows_io[n], false);
    memory_region_set_enabled(&blk->out_windows[n], false);

    memory_region_set_enabled(win, !!(attrs & HOSTOUTATTR_EN));
    memory_region_set_size(win, size);
    memory_region_set_address(win, bar);
    memory_region_set_alias_offset(win, addr);
}

/* n is a number of IN window - 0:7 */
static void sc64_pcie_update_inwindow(SC64PCIEBlock *blk, unsigned n)
{
    assert(n < SC64_PCIE_WINDOWS_CNT);

    MemoryRegion *win = &blk->in_windows[n];
    uint32_t addr = REG(blk, HOSTINADDR(n));
    uint32_t bar = n >= 2 ? REG(blk, HOSTINBAR(n)) : 0;
    uint32_t attrs = REG(blk, HOSTINATTR(n));
    uint64_t size = sc64_get_window_size(attrs);
    int s_nr = n >= SC64_PCIE_SPACES_CNT ? SC64_PCIE_SPACES_CNT - 1 : n;

    assert(s_nr < SC64_PCIE_SPACES_CNT);

    memory_region_set_enabled(win, !!(attrs & HOSTOUTATTR_EN));
    memory_region_set_size(win, size);
    memory_region_set_address(win, bar);
    memory_region_set_alias_offset(win, addr);
}

static uint64_t sc64_pcie_block_read(void *opaque, hwaddr addr, unsigned size)
{
    SC64PCIEBlock *blk = opaque;

    switch (addr) {
    case REG_ID:
    case PCI_REG_HOST_CTRL:
    case PCI_HOST_TIMEOUT_MWR:
    case PCI_HOST_TIMEOUT_MWR_STATUS:
    case PCI_HOST_TIMEOUT_MRD:
    case PCI_HOST_TIMEOUT_MRD_STATUS:
    case PCI_HOST_TIMEOUT_SWR:
    case PCI_HOST_TIMEOUT_SWR_STATUS:
    case PCI_HOST_TIMEOUT_SRD:
    case PCI_HOST_TIMEOUT_SRD_STATUS:
    case DOORBELL_IN ... DOORBELL_IN_END:
        return REG(blk, addr);
    case PCI_INT_FLAG:
        return REG(blk, addr) & ~REG(blk, PCI_INT_MASK);
    case PCI_INT_MASK:
    case DOORBELL_OUT:
    case MSIPENDING_CLR:
    case MSIXTBL:
    case MSIXTBLBIR:
    case MSIXPBABIR:
    case HOST_INT_FLAG:
    case HOST_INT_EN:
    case HOSTOUTREG ... HOSTOUTREG_END:
    case HOSTINREG ... HOSTINREG_END:
    case HOSTINATTR_COM:
        return REG(blk, addr);
    case MSI_IRQ_TBL ... MSI_IRQ_TBL_END:
        /* Must not hit, since msix MemoryRegion must handle it by itself */
        assert(false);
        return 0;
    case MSIPENDING:
        /* Must not hit, since msix MemoryRegion must handle it by itself */
        assert(false);
        return 0;
    }

    qemu_log_mask(LOG_UNIMP,
                      "sc64-pcie: read access to unknown register 0x"
                      TARGET_FMT_plx, addr);
    return 0;
}

static void sc64_pcie_block_update_irq(SC64PCIEBlock *blk)
{
    uint32_t irqs = REG(blk, PCI_INT_FLAG) & ~REG(blk, PCI_INT_MASK);
    qemu_set_irq(blk->irq, irqs == 0 ? 0 : 1);
}

static void sc64_pcie_block_space_updated(SC64PCIEBlock *blk, int bar,
                                                    uint64_t size, int flags)
{
    assert(bar == 1 || bar == 2);
    memory_region_set_size(&blk->in_spaces[bar], size);
    pci_register_bar(PCI_DEVICE(blk->bridge), bar, flags, &blk->in_spaces[bar]);
    if (blk->space_updated) {
        blk->space_updated(blk->opaque, bar, size, flags);
    }
}

static void sc64_pcie_block_write(void *opaque, hwaddr addr,
                        uint64_t value, unsigned size)
{
    SC64PCIEBlock *blk = opaque;
    int flags = PCI_BASE_ADDRESS_SPACE_MEMORY;
    uint32_t vector;

    switch (addr) {
    case PCI_REG_HOST_CTRL:
    case PCI_HOST_TIMEOUT_MWR:
    case PCI_HOST_TIMEOUT_MWR_STATUS:
    case PCI_HOST_TIMEOUT_MRD:
    case PCI_HOST_TIMEOUT_MRD_STATUS:
    case PCI_HOST_TIMEOUT_SWR:
    case PCI_HOST_TIMEOUT_SWR_STATUS:
    case PCI_HOST_TIMEOUT_SRD:
    case PCI_HOST_TIMEOUT_SRD_STATUS:
        REG(blk, addr) = value;
        break;
    case PCI_INT_FLAG:
        REG(blk, addr) &= ~(value & ~REG(blk, PCI_INT_MASK));
        sc64_pcie_block_update_irq(blk);
        break;
    case PCI_INT_MASK:
        REG(blk, addr) = value;
        sc64_pcie_block_update_irq(blk);
        break;
    case DOORBELL_OUT:
        msix_notify(blk->bridge, value);
        break;
    case MSIPENDING_CLR:
    case MSIXTBL:
    case MSIXTBLBIR:
    case MSIXPBABIR:
    case HOST_INT_FLAG:
    case HOST_INT_EN:
    case HOSTOUTREG ... HOSTOUTREG_END:
    case DOORBELL_IN ... DOORBELL_IN_END:
        REG(blk, addr) = value;
        break;
    case HOSTINREG ... HOSTINREG_END:
        /* FIXME Do not write to read-only fields */
    case HOSTINATTR_COM:
        REG(blk, addr) = value;
        break;
    case MSI_IRQ_TBL ... MSI_IRQ_TBL_END:
        /* Must not hit, since msix MemoryRegion must handle it by itself */
        assert(false);
        break;
    case MSIPENDING:
        /* Must not hit, since msix MemoryRegion must handle it by itself */
        assert(false);
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-pcie: write access to unknown register 0x"
                      TARGET_FMT_plx, addr);
        return;
    }

    switch (addr) {
    case DOORBELL_IN ... DOORBELL_IN_END:
        vector = (addr - DOORBELL_IN) / 4;
        if (!(REG(blk, PCI_INT_MASK) & (1 << vector))) {
            REG(blk, PCI_INT_FLAG) |= 1 << vector;
            sc64_pcie_block_update_irq(blk);
        }
        break;
    case HOSTOUTREG ... HOSTOUTREG_END:
        sc64_pcie_update_outwindow(blk, WINDOW(addr));
        break;
    case HOSTINREG ... HOSTINREG_END:
        switch (addr) {
        case HOSTINATTR(1):
            if (!(value & HOSTINATTR_EN)) {
                break;
            }
            sc64_pcie_block_space_updated(blk, 1, sc64_get_window_size(value),
                                                                        flags);
            break;
        }
        sc64_pcie_update_inwindow(blk, WINDOW(addr));
        break;
    case HOSTINATTR_COM:
        REG(blk, addr) = value;

        if (!(value & HOSTINATTR_COM_EN)) {
            break;
        }
        if (value & HOSTINATTR_COM_BASE) {
            flags |= PCI_BASE_ADDRESS_MEM_TYPE_64;
        }
        if (value & HOSTINATTR_COM_PREF) {
            flags |= PCI_BASE_ADDRESS_MEM_PREFETCH;
        }
        sc64_pcie_block_space_updated(blk, 2, sc64_get_window_size(value),
                                                                        flags);
        break;
    }
}

static const MemoryRegionOps sc64_pcie_block_ops = {
    .read = sc64_pcie_block_read,
    .write = sc64_pcie_block_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void sc64_pcie_setup_outspace(SC64PCIEBlock *blk, unsigned nr,
                                                const char *name, uint64_t size)
{
    assert(nr < SC64_PCIE_SPACES_CNT);
    memory_region_init(&blk->out_spaces[nr], OBJECT(blk), name, size);
    sysbus_init_mmio(SYS_BUS_DEVICE(blk), &blk->out_spaces[nr]);
}

static void sysbus_sc64_pcie_block_init(DeviceState *qdev, Error **errp)
{
    SysBusDevice *dev = SYS_BUS_DEVICE(qdev);
    SC64PCIEBlock *blk = SC64_PCIE_BLOCK(dev);
    int i;

    memory_region_init_io(&blk->pci_reg_host, OBJECT(dev), &sc64_pcie_block_ops,
                                            blk, "sc64-pcie-reg-host", 0x2000);

    MemoryRegion *temp = g_new(MemoryRegion, 1);
    memory_region_init_alias(temp, OBJECT(blk), "sc64-pcie-reg-host-mmio",
                                            &blk->pci_reg_host, 0, 0x2000);
    sysbus_init_mmio(dev, temp);

    sc64_pcie_setup_outspace(blk, 0, "pcie-out-space-0", 0x8000000ULL);
    sc64_pcie_setup_outspace(blk, 1, "pcie-out-space-1", 0x400000ULL);
    sc64_pcie_setup_outspace(blk, 2, "pcie-out-space-2", 0x100000000ULL);

    memory_region_init(&blk->in_spaces[0], OBJECT(dev), "pcie-in-space-0",
                                                                0x2000);
    memory_region_init(&blk->in_spaces[1], OBJECT(dev), "pcie-in-space-1",
                                                                0x10000000ULL);
    memory_region_init(&blk->in_spaces[2], OBJECT(dev), "pcie-in-space-2",
                                                                0x10000000ULL);

    address_space_init(&blk->pci_dma_as, &blk->pci_mem, "pcie-mem-inbound");

    for (i = 0; i < SC64_PCIE_WINDOWS_CNT; i++) {
        MemoryRegion *base;
        char name[64];
        int s_nr = i >= SC64_PCIE_SPACES_CNT ? SC64_PCIE_SPACES_CNT - 1 : i;

        snprintf(name, sizeof(name), "pcie-out-window-%d", i);

        memory_region_init_alias(&blk->out_windows[i], OBJECT(blk), name,
                                                    &blk->pci_mem, 0, 0);
        memory_region_set_enabled(&blk->out_windows[i], false);
        memory_region_add_subregion(&blk->out_spaces[s_nr],
                                                    0, &blk->out_windows[i]);

        memory_region_init_alias(&blk->out_windows_io[i], OBJECT(blk), name,
                                                    get_system_io(), 0, 0);
        memory_region_set_enabled(&blk->out_windows_io[i], false);
        memory_region_add_subregion(&blk->out_spaces[s_nr],
                                                    0, &blk->out_windows_io[i]);

        snprintf(name, sizeof(name), "pcie-in-window-%d", i);

        base = i == 0 ? &blk->pci_reg_host : blk->dma_as->root;
        memory_region_init_alias(&blk->in_windows[i], OBJECT(blk), name,
                                                                base, 0, 0);
        memory_region_set_enabled(&blk->in_windows[i], false);
        memory_region_add_subregion(&blk->in_spaces[s_nr],
                                                    0, &blk->in_windows[i]);
    }
}

static void sysbus_sc64_pcie_block_reset(DeviceState *dev)
{
    SC64PCIEBlock *blk = SC64_PCIE_BLOCK(dev);
    int i;

    REG(blk, REG_ID) = 0x5A41ACDC;
    REG(blk, MSIXTBL) = 0x001F0000;
    REG(blk, MSIXTBLBIR) = 0x00001000;
    REG(blk, MSIXPBABIR) = 0x00001800;

    /* OUT */
    REG(blk, HOSTOUTATTR(0)) = 0x801F8000;

    REG(blk, HOSTOUTATTR(1)) = 0x801FFC00;

    REG(blk, HOSTOUTATTR(2)) = 0x801C0000;

    REG(blk, HOSTOUTADDRL(3)) = 0x40000000;
    REG(blk, HOSTOUTBAR(3)) = 0x40000000;
    REG(blk, HOSTOUTATTR(3)) = 0x801C0000;

    for (i = 4; i < SC64_PCIE_WINDOWS_CNT; i++) {
        REG(blk, HOSTOUTADDRL(i)) = 0x80000000 + 0x20000000 * (i - 4);
        REG(blk, HOSTOUTBAR(i)) = 0x80000000;
        REG(blk, HOSTOUTATTR(i)) = 0x801E0000;
    }

    /* IN */
    REG(blk, HOSTINATTR(0)) = 0x801FFFFE;

    for (i = 1; i < SC64_PCIE_WINDOWS_CNT; i++) {
        REG(blk, HOSTINATTR(i)) = 0x001F0000;
    }

    for (i = 0; i < SC64_PCIE_WINDOWS_CNT; i++) {
        sc64_pcie_update_outwindow(blk, i);
        sc64_pcie_update_inwindow(blk, i);
    }
}

static void sysbus_sc64_pcie_block_class_init(ObjectClass *cl, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(cl);

    dc->realize = sysbus_sc64_pcie_block_init;
    dc->desc = "SC64 PCIE Block";
    dc->reset = sysbus_sc64_pcie_block_reset;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo sc64_pcie_block_info = {
    .name          = TYPE_SC64_PCIE_BLOCK,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SC64PCIEBlock),
    .class_init    = sysbus_sc64_pcie_block_class_init,
};

SC64PCIEBlock *sc64_pcie_block_register(const struct sc64_pcie_struct *ps)
{
    return sc64_pcie_block_register_ops(ps, NULL, NULL, NULL, NULL, NULL);
}

SC64PCIEBlock *sc64_pcie_block_register_ops(const struct sc64_pcie_struct *ps,
        void (*space_updated)(void *opaque, int bar, uint64_t size, int flags),
        void *opaque,
        const MemoryRegionOps *pci_mem_ops,
        const MemoryRegionOps *pci_io_ops,
        void *ops_opaque)
{
    DeviceState *dblk = qdev_create(NULL, TYPE_SC64_PCIE_BLOCK);
    SysBusDevice *sblk = SYS_BUS_DEVICE(dblk);
    SC64PCIEBlock *blk = SC64_PCIE_BLOCK(sblk);

    blk->irq = ps->msi;
    blk->space_updated = space_updated;
    blk->opaque = opaque ? opaque : blk;
    blk->dma_as = ps->dma_as;

    if (pci_mem_ops) {
        memory_region_init_io(&blk->pci_mem, OBJECT(dblk),
                                                pci_mem_ops, ops_opaque,
                                                "pcie-mem", UINT64_MAX);
    } else {
        memory_region_init(&blk->pci_mem, OBJECT(dblk), "pcie-mem", UINT64_MAX);
    }

    if (pci_io_ops) {
        memory_region_init_io(&blk->pci_io, OBJECT(dblk),
                                                pci_io_ops, ops_opaque,
                                                "pcie-io", UINT64_MAX);
    } else {
        memory_region_init(&blk->pci_io, OBJECT(dblk), "pcie-io", UINT64_MAX);
    }

    memory_region_add_subregion(get_system_io(), 0, &blk->pci_io);

    qdev_init_nofail(dblk);

    sysbus_mmio_map(sblk, 0, ps->self_conf);
    sysbus_mmio_map(sblk, 1, ps->out1);
    sysbus_mmio_map(sblk, 2, ps->out2);
    sysbus_mmio_map(sblk, 3, ps->out3);

    return blk;
}

static void sc64_blk_register_types(void)
{
    type_register_static(&sc64_pcie_block_info);
}

type_init(sc64_blk_register_types)
