#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/hw.h"
#include "sysemu/sysemu.h"
#include "sysemu/blockdev.h"
#include "hw/sysbus.h"
#include "hw/block/flash.h"
#include "hw/block/sc64-nand.h"
#include "hw/devices.h"

#if 0
#define DNAND(x) x
#else
#define DNAND(x)
#endif

static uint64_t sc64_nand_read(void *opaque, hwaddr addr, unsigned size)
{
    SC64NANDState *s = (SC64NANDState *) opaque;
    uint32_t r;
    int rdy;

    r = 0;

    switch (addr) {
    case 0x00:
        r = nand_getio(s->nand);
        if (size == 1) {
            break;
        }
        r |= (nand_getio(s->nand) << 8);
        if (size == 2) {
            break;
        }
        r |= (nand_getio(s->nand) << 16);
        r |= (nand_getio(s->nand) << 24);
        break;

    case 0x0c:
        nand_getpins(s->nand, &rdy);
        s->rdy = rdy;

        r = ((rdy) & 1) << 2;
        break;
    }

    DNAND(printf("%s addr=%" PRIx64 " r=%" PRIx32 " size=%d\n", __func__,
                 addr, r, size));

    return r;
}

static void sc64_nand_write(void *opaque, hwaddr addr,
                     uint64_t value, unsigned size)
{
    SC64NANDState *s = (SC64NANDState *) opaque;
    int rdy;

    DNAND(printf("%s addr=%" PRIx64 " v=%" PRIx64 " size=%d\n", __func__, addr, value, size));

    s->ce = 0; /* active low */

    switch (addr) {
    case 0:
    case 0xc:
        break;
    case 8:
        s->ale = 1;
        s->cle = 0;
        break;
    case 9:
        s->ale = 0;
        s->cle = 1;
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-nand: write access to unknown register 0x"
                      TARGET_FMT_plx, addr);
    }

    nand_setpins(s->nand, s->cle, s->ale, s->ce, 1, 0);
    switch (size) {
    case 1:
        nand_setio(s->nand, value & 0xff);
        break;
    case 2:
        nand_setio(s->nand, value & 0xff);
        nand_setio(s->nand, (value >> 8) & 0xff);
        break;
    case 4:
    default:
        nand_setio(s->nand, value & 0xff);
        nand_setio(s->nand, (value >> 8) & 0xff);
        nand_setio(s->nand, (value >> 16) & 0xff);
        nand_setio(s->nand, (value >> 24) & 0xff);
        break;
    }
    nand_getpins(s->nand, &rdy);
    s->rdy = rdy;

    s->ale = 0;
    s->cle = 0;
}

static const MemoryRegionOps sc64_nand_ops = {
    .read = sc64_nand_read,
    .write = sc64_nand_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

void sc64_nand_flash_register(hwaddr base)
{
    DeviceState *dev;

    dev = qdev_create(NULL, TYPE_SC64_NAND);

    qdev_prop_set_uint8(dev, "manf_id", NAND_MFR_SAMSUNG);
    qdev_prop_set_uint8(dev, "chip_id", 0xd3);

    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, base);
}

static int sc64_nand_init(SysBusDevice *dev)
{
    SC64NANDState *s = SC64_NAND(dev);
    DriveInfo *nand;

    nand = drive_get(IF_MTD, 0, 0);
    s->nand = nand_init(nand ? blk_by_legacy_dinfo(nand) : NULL,
                        s->manf_id, s->chip_id);

    memory_region_init_io(&s->iomem, OBJECT(s),
                            &sc64_nand_ops, s, "sc64-nand", 0x10);
    sysbus_init_mmio(dev, &s->iomem);

    return 0;
}

static VMStateDescription vmstate_sc64_nand_info = {
    .name = "sc64-nand",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_END_OF_LIST(),
    },
};

static Property sc64_nand_properties[] = {
    DEFINE_PROP_UINT8("manf_id", SC64NANDState, manf_id, NAND_MFR_SAMSUNG),
    DEFINE_PROP_UINT8("chip_id", SC64NANDState, chip_id, 0xf1),
    DEFINE_PROP_END_OF_LIST(),
};

static void sc64_nand_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_nand_init;
    dc->vmsd = &vmstate_sc64_nand_info;
    dc->props = sc64_nand_properties;
}

static const TypeInfo sc64_nand_info = {
    .name          = TYPE_SC64_NAND,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SC64NANDState),
    .class_init    = sc64_nand_class_init,
};

static void sc64_nand_register_types(void)
{
    type_register_static(&sc64_nand_info);
}

type_init(sc64_nand_register_types)
