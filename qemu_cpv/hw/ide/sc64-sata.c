#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "sysemu/block-backend.h"
#include "sysemu/dma.h"

#include "hw/ide/internal.h"
#include "hw/ide/ahci_internal.h"

#include "hw/ide/pci.h"
#include "hw/ide/sc64-sata.h"

#define TYPE_SC64_SATA "sc64-sata"
#define SC64SATA(obj) OBJECT_CHECK(SC64SataState, (obj), TYPE_SC64_SATA)

typedef struct SC64SataIDE {
    IDEBus bus;
    BMDMAState bmdma;
    MemoryRegion sata_reg;

    uint32_t scr_ctl;
} SC64SataIDE;

typedef struct SC64SataIDEState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    SC64SataIDE channel[2];

    qemu_irq irq;
    MemoryRegion iomem;

    /* Memory access */
    AddressSpace *dma_as;

    MemoryRegion conf_io;
} SC64SataState;

static void sc64_sata_ide_reset(DeviceState *dev)
{
    SC64SataState *s = SC64SATA(dev);
    int i;

    for (i = 0; i < 2; i++) {
        ide_bus_reset(&s->channel[i].bus);
    }
}

static uint64_t bmdma_read(void *opaque, hwaddr addr, unsigned size)
{
    BMDMAState *bm = opaque;
    uint32_t val;

    if (size != 1) {
        return ((uint64_t)1 << (size * 8)) - 1;
    }

    switch (addr & 3) {
    case 0:
        val = bm->cmd;
        break;
    case 2:
        val = bm->status;
        break;
    default:
        val = 0xff;
        break;
    }
#ifdef DEBUG_IDE
    printf("sc64-sata-bmdma: readb 0x%02x : 0x%02x\n", (uint8_t)addr, val);
#endif
    return val;
}

static void bmdma_write(void *opaque, hwaddr addr,
                        uint64_t val, unsigned size)
{
    BMDMAState *bm = opaque;

    if (size != 1) {
        return;
    }

#ifdef DEBUG_IDE
    printf("sc64-sata-bmdma: writeb 0x%02x : 0x%02x\n", addr, val);
#endif

    switch (addr & 3) {
    case 0:
        bmdma_cmd_writeb(bm, val);
        break;
    case 2:
        bm->status = (val & 0x60) | (bm->status & 1)
                                  | (bm->status & ~val & 0x06);
        break;
    }
}

static const MemoryRegionOps sc64_sata_bmdma_ops = {
    .read = bmdma_read,
    .write = bmdma_write,
};

static uint64_t sc64_sata_read(void *opaque, hwaddr addr, unsigned size)
{
    SC64SataIDE *s = opaque;
    IDEBus *bus = &s->bus;
    uint32_t val;

    switch (addr & 3) {
    case PORT_SCR_STAT:
        if (bus->ifs[0].blk) {
            val = SATA_SCR_SSTATUS_DET_DEV_PRESENT_PHY_UP |
                  SATA_SCR_SSTATUS_SPD_GEN1 | SATA_SCR_SSTATUS_IPM_ACTIVE;
        } else {
            val = SATA_SCR_SSTATUS_DET_NODEV;
        }
        break;
    case PORT_SCR_CTL:
        val = s->scr_ctl;
        break;
    case PORT_SCR_ERR:
        /* Always success */
        val = 0;
        break;
    default:
        val = 0;
    }

    return val;
}

static void sc64_sata_reset(SC64SataIDE *s)
{
    s->bmdma.dma.ops->reset(&s->bmdma.dma);
    s->scr_ctl = 0;
}

static void sc64_sata_write(void *opaque, hwaddr addr,
                        uint64_t val, unsigned size)
{
    SC64SataIDE *s = opaque;

    switch (addr & 3) {
    case PORT_SCR_STAT:
        /* Read Only */
        break;
    case PORT_SCR_CTL:
        if (((s->scr_ctl & AHCI_SCR_SCTL_DET) == 1) &&
            ((val & AHCI_SCR_SCTL_DET) == 0)) {
            sc64_sata_reset(s);
        }
        s->scr_ctl = val;
        break;
    case PORT_SCR_ERR:
        /* Read Only, always success */
        break;
    }
}

static const MemoryRegionOps sc64_sata_ops = {
    .read = sc64_sata_read,
    .write = sc64_sata_write,
};

static uint64_t sc64_sata_conf_read(void *opaque, hwaddr addr, unsigned size)
{
    uint32_t val;

    addr &= ~3;

    switch (addr & 0xFF) {
    case PCI_VENDOR_ID:
        val = 0x3351 << 16 | 0x191e ;
        break;
    case PCI_REVISION_ID:
        val = 0x01 << 24 | 0x01 << 16 | 0x85 << 8 | 0x01;
        break;
    case PCI_CACHE_LINE_SIZE:
        val = 0x00 << 24 | 0x80 << 16 | 0x00 << 8 | 0x00;
        break;
    case PCI_COMMAND:
        val = 0;
        break;
    case SC64_SATA_PCMD_BAR:
        val = 0x00;
        break;
    case SC64_SATA_PCNL_BAR:
        val = 0x20;
        break;
    case SC64_SATA_SCMD_BAR:
        val = 0x40;
        break;
    case SC64_SATA_SCNL_BAR:
        val = 0x60;
        break;
    case SC64_SATA_BAR:
        val = 0x80;
        break;
    case SC64_SATA_SIDPBA_BAR:
        val = 0xA0;
        break;
    default:
        val = 0;
    }
#ifdef DEBUG_IDE
    printf("sc64_sata_conf_read:%d bytes 0x%02x:0x%02x\n", size, addr, val);
#endif
    return val;
}

static void sc64_sata_conf_write(void *opaque, hwaddr addr,
                        uint64_t val, unsigned size)
{
#ifdef DEBUG_IDE
    printf("sc64_sata_conf_write:%d bytes 0x%02x:0x%02x\n", size, addr, val);
#endif
}

static const MemoryRegionOps sc64_sata_conf_ops = {
    .read = sc64_sata_conf_read,
    .write = sc64_sata_conf_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* NOTE: Not tested */
static const VMStateDescription vmstate_ide_sc64_sata = {
    .name = "sc64-sata",
    .version_id = 1,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_IDE_BUS(channel[0].bus, SC64SataState),
        VMSTATE_IDE_DRIVES(channel[0].bus.ifs, SC64SataState),
        VMSTATE_IDE_BUS(channel[1].bus, SC64SataState),
        VMSTATE_IDE_DRIVES(channel[1].bus.ifs, SC64SataState),
        VMSTATE_END_OF_LIST()
    }
};

static void sc64_sata_channel_init(SysBusDevice *d, int ch,
                                   const char *bmdma_host_name,
                                   const char *bmdma_name,
                                   const char *sata_name,
                                   const char *ide_name)
{
    SC64SataState *s = SC64SATA(d);
    SC64SataIDE *si = &s->channel[ch];
    BMDMAState *bm = &si->bmdma;

    memory_region_init_io(&bm->extra_io, OBJECT(d), &sc64_sata_bmdma_ops,
                          bm, bmdma_host_name, 4);
    memory_region_add_subregion(&s->iomem, 0x80 + ch * 8, &bm->extra_io);
    memory_region_init_io(&bm->addr_ioport, OBJECT(d),
                          &bmdma_addr_ioport_ops, bm, bmdma_name, 4);
    memory_region_add_subregion(&s->iomem, 0x80 + ch * 8 + 4,
                                &bm->addr_ioport);
    memory_region_init_io(&si->sata_reg, OBJECT(d),
                          &sc64_sata_ops, si, sata_name, 0xc);
    memory_region_add_subregion(&s->iomem, 0x100 + ch * 0x10,
                                &si->sata_reg);

    ide_bus_new(&si->bus, sizeof(si->bus), DEVICE(d), ch, 1);
    ide_init_ioport_mr(&si->bus, &s->iomem, ch * 0x40, 0x22 + ch * 0x40,
                       ide_name);
}

static int sc64_sata_ide_init(SysBusDevice *d)
{
    SC64SataState *s = SC64SATA(d);

    memory_region_init(&s->iomem, OBJECT(s), "sc64-sata",
                                              0x1000);

    sc64_sata_channel_init(d, 0, "host-bmdma", "bmdma", "sata", "ide");
    sc64_sata_channel_init(d, 1, "host-bmdma#2", "bmdma#2", "sata#2", "ide#2");

    memory_region_init_io(&s->conf_io, OBJECT(d), &sc64_sata_conf_ops, s,
                              "pci-conf", 0x100);
    memory_region_add_subregion(&s->iomem, 0x300, &s->conf_io);

    sysbus_init_mmio(d, &s->iomem);
    sysbus_init_irq(d, &s->irq);

    return 0;
}

void sc64_init_disks(DeviceState *ds,  AddressSpace *dma_as,
                         DriveInfo *hds[SC64_SATA_CHANNELS_QUANTITY])
{
    SC64SataState *s = SC64SATA(ds);
    int i;

    s->dma_as = dma_as;

    for (i = 0; i < SC64_SATA_CHANNELS_QUANTITY; i++) {
        SC64SataIDE *si = &s->channel[i];

        ide_init2(&si->bus, s->irq);

        bmdma_init_sysbus(&si->bus, &si->bmdma, ds, s->dma_as);
        si->bmdma.bus = &si->bus;
        ide_register_restart_cb(&si->bus);

        if (hds[i]) {
            printf("ide %d created\n", i);
            ide_create_drive(&si->bus, 0, hds[i]);
        }
    }
}

static Property sc64_sata_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void sc64_sata_ide_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_sata_ide_init;
    dc->reset = sc64_sata_ide_reset;
    dc->vmsd = &vmstate_ide_sc64_sata;
    dc->props = sc64_sata_properties;
}

static const TypeInfo sc64_sata_ide_type_info = {
    .name = TYPE_SC64_SATA,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SC64SataState),
    .class_init = sc64_sata_ide_class_init,
};

static void sc64_sata_ide_register_types(void)
{
    type_register_static(&sc64_sata_ide_type_info);
}

type_init(sc64_sata_ide_register_types)
