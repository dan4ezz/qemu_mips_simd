#include "qemu/osdep.h"

#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "hw/mips/mips.h"
#include "hw/pci/pci_host.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"
#include "hw/pci-host/sc64-pci.h"

//#define DEBUG

#ifdef DEBUG
#define DPRINTF(fmt, ...) fprintf(stderr, "%s: " fmt, __FUNCTION__, ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...)
#endif

#define TYPE_SC64_PCI_HOST_BRIDGE "sc64-pci"

#define SC64_PCI_HOST_BRIDGE(obj) \
    OBJECT_CHECK(SC64PciState, (obj), TYPE_SC64_PCI_HOST_BRIDGE)

#define SC64_PCI_REGS            128

#define SC64_PCI_MEM_TRANS_REG   0x40
#define SC64_PCI_IO_TRANS_REG    0x44

typedef struct SC64PciState {
    PCIHostState parent_obj;

    PCIDevice *pci_dev;

    MemoryRegion pci_mem; /* 4G pci mem */
    MemoryRegion pci_mem_main; /* 128Mb window of the main memory */
    MemoryRegion pci_mem_second; /* 32Mb window of the secondary memory */
    MemoryRegion pci_io;
    MemoryRegion pci_io_win;
} SC64PciState;

static bool sc64_is_pci_controller(hwaddr addr)
{
    uint8_t busid = extract32(addr, 16, 8);
    uint8_t devid = extract32(addr, 11, 4);
    uint8_t devfn = extract32(addr, 8, 3);

    return !busid && !devid && !devfn;
}

static void sc64_pci_init_mem_region(SC64PciState *s, MemoryRegion *m,
    MemoryRegion *parent, const char *name, uint32_t trans_reg,
    hwaddr addr, int size)
{
    uint32_t offset;

    offset = pci_default_read_config(s->pci_dev, trans_reg, 4);
    memory_region_init_alias(m, OBJECT(s), name, &s->pci_io, offset, size);
    memory_region_add_subregion(get_system_memory(), addr, m);
}

static void sc64_pci_destroy_mem_region(MemoryRegion *m)
{
    memory_region_del_subregion(get_system_memory(), m);
    object_unparent(OBJECT(m));
}

static void sc64_pci_io_mapping_set(SC64PciState *s)
{
    sc64_pci_init_mem_region(s, &s->pci_io_win, &s->pci_io, "pci-io-win",
        SC64_PCI_IO_TRANS_REG, 0x1a000000, 0x400000);
}

static void sc64_pci_mem_mapping_set(SC64PciState *s)
{
    sc64_pci_init_mem_region(s, &s->pci_mem_main, &s->pci_mem,
        "pci-mem-main", SC64_PCI_MEM_TRANS_REG, 0x10000000, 0x8000000);
    sc64_pci_init_mem_region(s, &s->pci_mem_second, &s->pci_mem,
        "pci-mem-second", SC64_PCI_MEM_TRANS_REG, 0x18000000, 0x2000000);
}

static void sc64_pci_io_mapping_update(SC64PciState *s)
{
    sc64_pci_destroy_mem_region(&s->pci_io_win);
    sc64_pci_io_mapping_set(s);
}

static void sc64_pci_mem_mapping_update(SC64PciState *s)
{
    sc64_pci_destroy_mem_region(&s->pci_mem_main);
    sc64_pci_destroy_mem_region(&s->pci_mem_second);
    sc64_pci_mem_mapping_set(s);
}

static const VMStateDescription vmstate_sc64 = {
    .name = "sc64-pci",
    .version_id = 1,
    .minimum_version_id = 1
};

static void sc64_write(void *opaque, hwaddr addr,
                          uint64_t val, unsigned size)
{
    SC64PciState *s = opaque;
    PCIHostState *phb = PCI_HOST_BRIDGE(s);
    uint32_t saddr;

    if (sc64_is_pci_controller(addr)) {
        saddr = (addr & 0xFF);
        switch (saddr) {
        case SC64_PCI_MEM_TRANS_REG:
            pci_default_write_config(s->pci_dev, SC64_PCI_MEM_TRANS_REG,
                val, size);
            sc64_pci_mem_mapping_update(s);
            return;
        case SC64_PCI_IO_TRANS_REG:
            pci_default_write_config(s->pci_dev, SC64_PCI_IO_TRANS_REG,
                val, size);
            sc64_pci_io_mapping_update(s);
            return;
        default:
            DPRINTF("sc64_write unsupported reg "TARGET_FMT_plx" %x\n", saddr);
            break;
        }
    }

    DPRINTF("sc64_write "TARGET_FMT_plx" val %x\n", addr, val);
    pci_data_write(phb->bus, addr, val, size);
}

static uint64_t sc64_read(void *opaque, hwaddr addr,
                             unsigned size)
{
    SC64PciState *s = opaque;
    PCIHostState *phb = PCI_HOST_BRIDGE(s);
    uint32_t saddr;

    if (sc64_is_pci_controller(addr)) {
        saddr = (addr & 0xFF);
        switch (saddr) {
        case SC64_PCI_MEM_TRANS_REG:
            return pci_default_read_config(s->pci_dev,
                        SC64_PCI_MEM_TRANS_REG, size);
        case SC64_PCI_IO_TRANS_REG:
            return pci_default_read_config(s->pci_dev,
                        SC64_PCI_IO_TRANS_REG, size);
        default:
            DPRINTF("sc64_read unsupported reg "TARGET_FMT_plx" %x\n", saddr);
            break;
        }
    }

    DPRINTF("sc64_read "TARGET_FMT_plx"\n", addr);
    return pci_data_read(phb->bus, addr, size);
}

static const MemoryRegionOps sc64_ops = {
    .read = sc64_read,
    .write = sc64_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void sc64_pci_set_irq(void *opaque, int irq_num, int level)
{
    qemu_irq *pic = opaque;

    qemu_set_irq(pic[irq_num], level);
}

static int sc64_pci_map_irq(PCIDevice *pci_dev, int irq_num)
{
    /* FIXME Interrupt mapping is board specific */
    return 0;
}

static AddressSpace *sc64_pci_set_iommu(PCIBus *bus, void *opaque,
                                             int devfn)
{
    return opaque;
}

void sc64_pci_register(qemu_irq *pic, AddressSpace *dma_as)
{
    DeviceState *dev;
    SC64PciState *d;
    SysBusDevice *sbd;
    PCIHostState *phb;

    dev = qdev_create(NULL, TYPE_SC64_PCI_HOST_BRIDGE);

    d = SC64_PCI_HOST_BRIDGE(dev);
    sbd = SYS_BUS_DEVICE(dev);
    phb = PCI_HOST_BRIDGE(dev);

    /* Set up pci configuration space */
    memory_region_init_io(&phb->conf_mem, OBJECT(dev), &sc64_ops, d,
                          "sc64-pci-conf", 0x1000000);
    sysbus_init_mmio(sbd, &phb->conf_mem);
    sysbus_mmio_map(sbd, 0, 0x1e000000);

    /* Set up pci memory region 4G */
    memory_region_init(&d->pci_mem, OBJECT(sbd), "pci-mem",
        0x100000000ULL);
    memory_region_add_subregion(get_system_memory(), 0xd00000000ULL,
        &d->pci_mem);

    /* Set up pci I/O region 4G */
    memory_region_init(&d->pci_io, OBJECT(sbd), "pci-io", 0x100000000ULL);

    phb->bus = pci_register_bus(dev, "pci",
                                sc64_pci_set_irq, sc64_pci_map_irq,
                                pic,
                                &d->pci_mem,
                                &d->pci_io,
                                PCI_DEVFN(0, 0), 4, TYPE_PCI_BUS);

    qdev_init_nofail(dev);
    pci_create_simple(phb->bus, PCI_DEVFN(0, 0), "sc64_pci");

    pci_setup_iommu(phb->bus, sc64_pci_set_iommu, dma_as);
}

static void sc64_pci_realize(PCIDevice *d, Error **errp)
{
}

static void sc64_pci_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->realize = sc64_pci_realize;
    k->vendor_id = 0x191e;
    k->device_id = 0xc0fe;
    k->revision = 0;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    /*
     * PCI-facing part of the host bridge, not usable without the
     * host-facing part, which can't be device_add'ed, yet.
     */
    dc->user_creatable = false;
}

static const TypeInfo sc64_pci_info = {
    .name          = "sc64_pci",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIDevice),
    .class_init    = sc64_pci_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void sc64_pci_reset(void *opaque)
{
    SC64PciState *s = opaque;
    PCIHostState *phb = PCI_HOST_BRIDGE(s);
    PCIDevice *this = pci_find_device(phb->bus, 0, 0);

    assert(this);
    s->pci_dev = this;

    pci_default_write_config(s->pci_dev, SC64_PCI_MEM_TRANS_REG, 0, 4);
    pci_default_write_config(s->pci_dev, SC64_PCI_IO_TRANS_REG, 0, 4);
    sc64_pci_mem_mapping_set(s);
    sc64_pci_io_mapping_set(s);
}

static int sc64_init(SysBusDevice *dev)
{
    SC64PciState *s;

    s = SC64_PCI_HOST_BRIDGE(dev);

    qemu_register_reset(sc64_pci_reset, s);

    return 0;
}

static void sc64_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = sc64_init;
    dc->vmsd = &vmstate_sc64;
}

static const TypeInfo sc64_info = {
    .name          = TYPE_SC64_PCI_HOST_BRIDGE,
    .parent        = TYPE_PCI_HOST_BRIDGE,
    .instance_size = sizeof(SC64PciState),
    .class_init    = sc64_class_init,
};

static void sc64_pci_register_types(void)
{
    type_register_static(&sc64_info);
    type_register_static(&sc64_pci_info);
}

type_init(sc64_pci_register_types)
