/*
 * Copyright (C) 2018 Denis Deryugin <deryugin.denis@gmail.com>
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
#include "chardev/char.h"
#include "hw/hw.h"
#include "hw/block/flash.h"
#include "hw/mips/sc64-devicebus.h"
#include "hw/mips/sccr-sysbus.h"
#include "hw/sysbus.h"
#include "qemu/error-report.h"
#include "qemu/timer.h"
#include "qom/cpu.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"
#include "exec/memattrs.h"

#define DEBUG 0
#if DEBUG
#define dbg(...) fprintf(stderr, "DeviceBus: " __VA_ARGS__)
#else
#define dbg(...)
#endif

#define TYPE_DBUS "dbus"
#define DBUS(obj) OBJECT_CHECK(DBus, (obj), TYPE_DBUS)

#define TYPE_SC64_DEVICEBUS "sc64-devicebus"
#define SC64_DEVICEBUS(obj) \
    OBJECT_CHECK(Sc64DeviceBusState, (obj), TYPE_SC64_DEVICEBUS)

#define DBUS_OUTER_WIN_SIZE     (512 * 1024 * 1024) /* 512 MiB */

#define DBUS_ID         0x000
#define SET_CLK_DIV     0x008
#define SET_REG(n)      (0x010 + (n) * 0x8)
#define TYPE_MASK       0xFF
#define CTRL_STATUS     0x050
# define PROG_FLASH     (1ULL << 48)
# define MAP_BOOT       (1ULL << 40)
# define STOP_TIMER     (1ULL << 32)
# define BOOTROM        (1ULL << 30)
# define TIMEOUT        (1ULL << 29)
#define BASE_MASK(n)    (0x058 + (n) * 0x8)

#define REG_IDX(x)      ((x) / 8)
#define REG(s, reg)     ((s)->regs[REG_IDX(reg)])
#define REG_NUM         (0x078 / 8)

#define BASE(s, n)      \
    ((0x1FFF & (REG(s, BASE_MASK((n) / 2)) >> ((n % 2) ? 32 : 0))) << 16)
#define MASK(s, n)      \
    ((0x1FFF & (REG(s, BASE_MASK((n) / 2)) >> ((n % 2) ? 48 : 16))) << 16)

#define TYPE_NOR(t) ((t) == DBUS_TYPE_NOR_SYNC || \
                        (t) == DBUS_TYPE_NOR_ASYNC)
#define TYPE_SRAM(t)  ((t) == DBUS_TYPE_SRAM_ASYNC || \
                        (t) == DBUS_TYPE_SRAM_SYNC_FT || \
                        (t) == DBUS_TYPE_SRAM_PIPE_NOBL || \
                        (t) == DBUS_TYPE_SRAM_PIPE)

#define min(a, b) ((a) < (b) ? (a) : (b))

static const uint64_t sc64_devicebus_reg_write_mask[REG_NUM] = {
    [REG_IDX(DBUS_ID)]                                = 0x0000000000000000ULL,
    [REG_IDX(SET_CLK_DIV)]                            = 0x00000000FFFFFFFFULL,
    [REG_IDX(SET_REG(0)) ... REG_IDX(SET_REG(7))]     = 0x007F3F3F3F3F3FFFULL,
    [REG_IDX(CTRL_STATUS)]                            = 0x0701000000000000ULL,
    [REG_IDX(BASE_MASK(0)) ... REG_IDX(BASE_MASK(3))] = 0x1FFF1FFF1FFF1FFFULL,
};

typedef struct Sc64DeviceBusState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem_regs;
    MemoryRegion iomem_outer;

    BusState *bus;

    uint64_t regs[REG_NUM];

    MemoryRegion boot_mem;

    MemoryRegion device_alias[SC64_DEVICEBUS_DEV_NUM];
    MemoryRegion *device_orig[SC64_DEVICEBUS_DEV_NUM];

    QEMUTimer *watchdog;
    void (*wdog_callback)(void *arg);
    void *wdog_arg;

    void (*bootmap_enable_callback)(MemoryRegion *, bool);
} Sc64DeviceBusState;

static void sc64_devicebus_bootmap_enable(Sc64DeviceBusState *s, bool enable)
{
    assert(s->bootmap_enable_callback);

    s->bootmap_enable_callback(&s->boot_mem, false);

    if (enable) {
        memory_region_init_alias(&s->boot_mem,
                OBJECT(s),
                "dbus device",
                &s->iomem_outer,
                0,
                1024 * 1024);

        s->bootmap_enable_callback(&s->boot_mem, true);
    }
}

static void sc64_devicebus_watchdog(void *opaque)
{
    Sc64DeviceBusState *s = opaque;

    if (REG(s, CTRL_STATUS) & STOP_TIMER) {
        /* Watchset was set off */
        dbg("Watchdog was stopped\n");
        return;
    }

    /* Reset and boot from SPI flash */
    dbg("Failed to boot from DBus, try SPI...\n");
    REG(s, CTRL_STATUS) &= ~MAP_BOOT;
    REG(s, CTRL_STATUS) |= TIMEOUT;

    assert(memory_region_is_mapped(&s->boot_mem));
    sc64_devicebus_bootmap_enable(s, false);

    assert(s->wdog_callback);
    s->wdog_callback(s->wdog_arg);
}

static void sc64_devicebus_cs_mem_update(Sc64DeviceBusState *s, int cs)
{
    MemoryRegion *orig;
    int64_t size, tmp;

    assert(cs >= 0 && cs < SC64_DEVICEBUS_DEV_NUM);

    orig = s->device_orig[cs];
    if (!orig) {
        return;
    }

    if (memory_region_is_mapped(&s->device_alias[cs])) {
        memory_region_del_subregion(&s->iomem_outer, &s->device_alias[cs]);
    }

    if (MASK(s, cs) >= BASE(s, cs)) {
        /* Chip select is done as follows:
         *  Select_chip_0 = ((addr[28:16] & MASK_0) == BASE_0);
         *  Select_chip_i = ((addr[28:16] & MASK_i) == BASE_i) &
         *      ~Select_chip_(i-1); for i = 1..7
         *
         * As we can see, device is active iff MASK_i >= BASE_i,
         * so we map this region only in this case.
         *
         * The `~Select_chip(i-1)' part of expression is handled
         * by selecting appropriate region priority (see below),
         * the first part of the expression is true for all addresses
         * from the range [BASE_i ... BASE_i + 2^k), where `k' is the
         * least significant bit of BASE_i or MASK_i.
         *
         * Also we have to limit the whole region with the maximum
         * address of the outer window as both BASE_i and MASK_i
         * can equal zero.
         *
         * To find 2^{least significant bit of `x'} we can use
         * following formula:
         *      2^k = ((x ^ (x - 1)) + 1) / 2
         *
         * Hence we get the following computations: */
        tmp = BASE(s, cs) | MASK(s, cs) | DBUS_OUTER_WIN_SIZE;
        size = ((tmp ^ (tmp - 1)) + 1) / 2;

        memory_region_init_alias(&s->device_alias[cs],
                OBJECT(s),
                "dbus device",
                orig,
                0,
                size);

        memory_region_add_subregion_overlap(
                &s->iomem_outer, 0, &s->device_alias[cs],
                SC64_DEVICEBUS_DEV_NUM - cs);

        memory_region_set_address(&s->device_alias[cs], BASE(s, cs));
    }
}

static int sc64_devicebus_walker(DeviceState *dev, void *opaque)
{
    Sc64DeviceBusState *s = opaque;
    Object *obj = object_dynamic_cast(OBJECT(dev), TYPE_SYS_BUS_DEVICE);
    SysBusDevice *sbd;
    MemoryRegion *mem;
    int cs;

    if (obj == NULL) {
        error_report("Can't attach to DBus (wrong device type), "
                                "skipping device (%s)", dev->canonical_path);
        return 0;
    }

    if (dev->id == NULL || strlen(dev->id) < 2) {
        error_report("Bad device id. Expected format /^[[:alpha:]]\\d+/, "
                                "skipping device (%s)", dev->canonical_path);
        return 0;
    }

    sbd = SYS_BUS_DEVICE(obj);
    mem = sysbus_mmio_get_region(sbd, 0);

    sscanf(dev->id + 1, "%i", &cs);

    s->device_orig[cs] = mem;

    sc64_devicebus_cs_mem_update(s, cs);

    if (cs == 0 && strstr(dev->id, "bootable")) {
        REG(s, CTRL_STATUS) |= BOOTROM | MAP_BOOT;
        sc64_devicebus_bootmap_enable(s, true);
    }

    return 0;
}

static void sc64_devicebus_realize(DeviceState *dev, Error **errp)
{
    Sc64DeviceBusState *s = SC64_DEVICEBUS(dev);

    s->watchdog = timer_new_ns(QEMU_CLOCK_VIRTUAL, sc64_devicebus_watchdog, s);
    s->bus = qbus_create(TYPE_DBUS, dev, TYPE_DBUS);
}

static void sc64_devicebus_reset(DeviceState *ds)
{
    Sc64DeviceBusState *s = SC64_DEVICEBUS(ds);
    int i;

    memset(s->regs, 0, REG_NUM * sizeof(s->regs[0]));

    REG(s, DBUS_ID)     = 0x0000000000000F00ULL;
    REG(s, SET_CLK_DIV) = 0x0000000002020202ULL;
    REG(s, CTRL_STATUS) = 0x0000010000000000ULL;

    for (i = 0; i < SC64_DEVICEBUS_DEV_NUM; i++) {
        REG(s, SET_REG(i)) = 0x1000400070304ULL;
    }

    REG(s, BASE_MASK(0)) = 0x1FFF1FFF00000000ULL;
    for (i = 1; i < SC64_DEVICEBUS_DEV_NUM / 2; i++) {
        REG(s, BASE_MASK(i)) = 0x1FFF1FFF1FFF1FFFULL;
    }

    qbus_walk_children(s->bus, sc64_devicebus_walker, NULL, NULL, NULL, s);

    if (memory_region_is_mapped(&s->boot_mem)) {
        int64_t tmp = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        timer_mod(s->watchdog, tmp + NANOSECONDS_PER_SECOND / 2);
    }
}

static void sc64_devicebus_update_windows(Sc64DeviceBusState *s)
{
    int i;
    for (i = 0; i < SC64_DEVICEBUS_DEV_NUM; i++) {
        sc64_devicebus_cs_mem_update(s, i);
    }
}

static void sc64_devicebus_write(void *opaque, hwaddr addr, uint64_t val,
        unsigned size)
{
    Sc64DeviceBusState *s = opaque;
    int saddr = REG_IDX(addr);

    if (saddr > REG_NUM) {
        printf("DeviceBus: write memory out of range!\n");
        return;
    }

    dbg("write %016lx->%03lx size %d\n", val, addr, size);
    dbg("saddr= %d, opaque = %p\n", saddr, s);
    if (size == 4) {
        val &= 0xFFFFFFFF;
        if (addr & 0x4) {
            val <<= 32;
            val |= s->regs[saddr] & 0xFFFFFFFFULL;
        } else {
            val |= s->regs[saddr] & 0xFFFFFFFF00000000ULL;
        }
    }

    s->regs[saddr] = val & sc64_devicebus_reg_write_mask[saddr];

    addr &= ~0x7;

    switch (addr) {
    case CTRL_STATUS:
        /* STOP_TIMER and MAP_BOOT are not accessible
         * if DBus flash is not a bootable device */
        if (REG(s, CTRL_STATUS) & BOOTROM) {
            if ((!(val & MAP_BOOT)) && (REG(s, CTRL_STATUS) & STOP_TIMER)) {
                /* MAP_BOOT can be discarded only after STOP_TIMER */
                REG(s, CTRL_STATUS) &= ~MAP_BOOT;
            }

            if ((val & STOP_TIMER) &&
                !(REG(s, CTRL_STATUS) & TIMEOUT)) {
                /* Bit can be written only during boot */
                REG(s, CTRL_STATUS) |= STOP_TIMER;
            }
        }

        if (!(REG(s, CTRL_STATUS) & BOOTROM) ||
            (REG(s, CTRL_STATUS) & (TIMEOUT | STOP_TIMER))) {
            /* PROG_FLASH is writable only if timer is stopped or
             * if DBus flash is not bootable device */
            if (val & PROG_FLASH) {
                REG(s, CTRL_STATUS) |= PROG_FLASH;
            } else {
                REG(s, CTRL_STATUS) &= ~PROG_FLASH;
            }
        }

        sc64_devicebus_bootmap_enable(s,
                REG(s, CTRL_STATUS) & (PROG_FLASH | MAP_BOOT));

        break;
    case BASE_MASK(0) ... BASE_MASK(3):
        sc64_devicebus_update_windows(s);
        break;
    default:
        break;
        /* Do nothing */
    }
}

static uint64_t sc64_devicebus_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64DeviceBusState *s = opaque;
    int saddr = REG_IDX(addr);
    uint64_t retval;

    if (saddr > REG_NUM) {
        printf("DeviceBus: read memory out of range!\n");
        return -1;
    }

    retval = s->regs[saddr];

    if (size == 4) {
        if (addr & 0x4) {
            retval >>= 32;
        }

        retval &= 0xFFFFFFFF;
    }

    dbg("read  %016lx<-%03lx size %d\n", retval, addr, size);

    return retval;
}

static const MemoryRegionOps sc64_devicebus_mem_ops = {
    .read = sc64_devicebus_read,
    .write = sc64_devicebus_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void sc64_devicebus_outer_write(void *opaque, hwaddr addr,
        uint64_t val, unsigned size)
{
    dbg("outer write %016lx->%08lx, not taken by any device\n", val, addr);
}

static uint64_t sc64_devicebus_outer_read(void *opaque, hwaddr addr,
        unsigned size)
{
    dbg("outer read %016lx, not taken by any device, return zero\n",
            addr);
    return 0;
}

static const MemoryRegionOps sc64_devicebus_outer_mem_ops = {
    .read = sc64_devicebus_outer_read,
    .write = sc64_devicebus_outer_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

BusState *sc64_devicebus_register(hwaddr base,
        hwaddr outer_base,
        void (*wdog_callback)(void *),
        void *wdog_arg,
        void (*bootmap_enable_callback)(MemoryRegion *, bool))
{
    DeviceState *dev = qdev_create(NULL, TYPE_SC64_DEVICEBUS);
    Sc64DeviceBusState *s = SC64_DEVICEBUS(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem_regs, OBJECT(s),
            &sc64_devicebus_mem_ops, s,
            "sc64-devicebus-regs", REG_NUM * sizeof(s->regs[0]));
    sysbus_init_mmio(sbd, &s->iomem_regs);
    sysbus_mmio_map(sbd, 0, base);

    memory_region_init_io(&s->iomem_outer, OBJECT(s),
            &sc64_devicebus_outer_mem_ops, s,
            "sc64-devicebus-outer", DBUS_OUTER_WIN_SIZE);
    sysbus_init_mmio(sbd, &s->iomem_outer);
    sysbus_mmio_map(sbd, 1, outer_base);

    s->wdog_callback = wdog_callback;
    s->wdog_arg = wdog_arg;
    s->bootmap_enable_callback = bootmap_enable_callback;

    qdev_init_nofail(dev);

    return s->bus;
}

static const VMStateDescription vmstate_sc64_devicebus_regs = {
    .name = TYPE_SC64_DEVICEBUS,
    .version_id = 1,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_UINT64_ARRAY(regs, Sc64DeviceBusState, REG_NUM),
        VMSTATE_END_OF_LIST(),
    },
};

static void sc64_devicebus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset   = sc64_devicebus_reset;
    dc->realize = sc64_devicebus_realize;
    dc->desc    = "SC64 DeviceBus block";
    dc->vmsd    = &vmstate_sc64_devicebus_regs;
}

static const TypeInfo sc64_devicebus_info = {
    .name          = TYPE_SC64_DEVICEBUS,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64DeviceBusState),
    .class_init    = sc64_devicebus_class_init,
};

static const TypeInfo dbus_info = {
    .name = TYPE_DBUS,
    .parent = TYPE_BUS,
};

#define TYPE_SC64_SRAM "sc64-sram"
#define SC64_SRAM(obj) OBJECT_CHECK(sc64_sram_t, (obj), TYPE_SC64_SRAM)

typedef struct sc64_sram_t {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    uint64_t size;
    char *name;
    MemoryRegion mem;
} sc64_sram_t;

static void sc64_sram_realize(DeviceState *dev, Error **errp)
{
    sc64_sram_t *sram = SC64_SRAM(dev);

    memory_region_init_ram_nomigrate(&sram->mem, OBJECT(dev),
            sram->name, sram->size, errp);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &sram->mem);
}

static Property sc64_sram_properties[] = {
    DEFINE_PROP_UINT64("size", struct sc64_sram_t, size, 0x1000),
    DEFINE_PROP_STRING("name", struct sc64_sram_t, name),
    DEFINE_PROP_END_OF_LIST(),
};

static void sc64_sram_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = sc64_sram_realize;
    dc->props = sc64_sram_properties;
    dc->user_creatable = true;
    dc->bus_type = TYPE_DBUS;
    set_bit(DEVICE_CATEGORY_STORAGE, dc->categories);
}

static const TypeInfo sc64_sram_info = {
    .name           = TYPE_SC64_SRAM,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(struct sc64_sram_t),
    .class_init     = sc64_sram_class_init,
};

static void sc64_devicebus_register_types(void)
{
    type_register_static(&dbus_info);
    type_register_static(&sc64_devicebus_info);
    type_register_static(&sc64_sram_info);
}

type_init(sc64_devicebus_register_types)
