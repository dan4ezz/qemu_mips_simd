/*
 * Copyright (C) 2017 Alexander Kalmuk <alexkalmuk@gmail.com>
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
#include "hw/sysbus.h"
#include "hw/mips/mips.h"
#include <sysemu/dma.h>
#include "exec/address-spaces.h"
#include "qemu/typedefs.h"
#include "qemu/bitops.h"
#include "hw/mips/sc64-mpu.h"

#define DEBUG

#define SC64_MPU_WINDOWS 11

#define SC64_MPU_CONFIG  0x00
# define SC64_MPU_CONFIG_HRL_OFFSET      24
# define SC64_MPU_CONFIG_NRGD_OFFSET     16
# define SC64_MPU_CONFIG_MPU_LOCK_OFFSET 15
#define SC64_MPU_CONFIG_SPERR_MASK 0x7FF
#define SC64_MPU_EADR    0x04
#define SC64_MPU_ESTATUS 0x08
#define SC64_MPU_RGi_START(i)  (0x0C + (i) * 0xC)
#define SC64_MPU_RGi_END(i)    (0x10 + (i) * 0xC)
#define SC64_MPU_RGi_ACCESS(i) (0x14 + (i) * 0xC)

# define SC64_MPU_ACCESS_READ_OFFSET 31
# define SC64_MPU_ACCESS_WRITE_OFFSET 30

#define SC64_MPU_WINDOW_REGS_START 0x0C
#define SC64_MPU_WINDOW_REGS_END   0x8C

#define PAGE_SHIFT 12

#define MPU_WINDOW_REG(s, reg) \
    (s->regs.window_regs[((reg) - 0xC) / 4].val)

#define MPU_WINDOW_REG_WMASK(s, reg) \
    (s->regs.window_regs[((reg) - 0xC) / 4].wmask)

#define MPU_WINDOW_START(s, dev_nr) \
    (s->regs.window_regs[dev_nr * 3].val)

#define MPU_WINDOW_END(s, dev_nr) \
    (s->regs.window_regs[dev_nr * 3 + 1].val)

#define MPU_WINDOW_ACCESS(s, dev_nr) \
    (s->regs.window_regs[dev_nr * 3 + 2].val)

#define TYPE_SC64_MPU "sc64-mpu"
#define SC64_MPU(obj) OBJECT_CHECK(Sc64MPUState, (obj), TYPE_SC64_MPU)

#ifdef DEBUG
#define DPRINTF(fmt, ...) \
    do { printf("sc64-mpu: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

typedef struct Sc64MPUWindowReg {
    uint32_t val;
    uint32_t wmask;
} Sc64MPUWindowReg;

typedef struct Sc64MPURegs {
    uint32_t config;
    uint32_t eadr;
    uint32_t estatus;
    /* START, END, ACCESS */
    Sc64MPUWindowReg window_regs[SC64_MPU_WINDOWS * 3];
} Sc64MPURegs;

typedef struct Sc64MPUState Sc64MPUState;

typedef struct Sc64MPUDevInfo {
    Sc64MPUState *mpu_state;
    enum MpuDeviceType dev_type;
} Sc64MPUDevInfo;

typedef struct Sc64MPUState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion dev_mem[SC64_MPU_WINDOWS];
    AddressSpace dev_mem_as[SC64_MPU_WINDOWS];
    MemoryRegion host_mem;

    AddressSpace *dma_as;

    Sc64MPURegs regs;

    Sc64MPUDevInfo devices[SC64_MPU_WINDOWS];

    qemu_irq irq;
} Sc64MPUState;

enum MpuIO {
    MPU_READ = 0,
    MPU_WRITE,
};

static inline bool sc64_mpu_enabled(Sc64MPUState *s)
{
    return s->regs.config & (1 << SC64_MPU_CONFIG_MPU_LOCK_OFFSET);
}

static void sc64_mpu_set_error(Sc64MPUState *s, enum MpuDeviceType device_type,
    enum MpuIO type, dma_addr_t addr)
{
    s->regs.config |= 1 << device_type;
    s->regs.eadr |= addr >> 12;
    s->regs.estatus |= device_type;
    s->regs.estatus |= (type == MPU_WRITE ? 1 : 0) << 7;

    qemu_irq_raise(s->irq);
}

static int sc64_mpu_check_access(Sc64MPUState *s,
    enum MpuDeviceType device_type, enum MpuIO type,
    dma_addr_t addr, dma_addr_t len)
{
    uint64_t start, end;
    uint32_t access;

    start = (uint64_t) MPU_WINDOW_START(s, device_type) << PAGE_SHIFT;
    end = ((uint64_t) MPU_WINDOW_END(s, device_type) << PAGE_SHIFT) - 1;
    access = MPU_WINDOW_ACCESS(s, device_type);

    if ((addr < start) || (addr + len) > end) {
        DPRINTF("Invalid address %" PRIx64 "\n", addr);
        return -1;
    }

    switch (type) {
    case MPU_READ:
        if (!(access & (1 << SC64_MPU_ACCESS_READ_OFFSET))) {
            DPRINTF("Invalid access read %" PRIx64 "\n", addr);
            return -1;
        }
        break;
    case MPU_WRITE:
        if (!(access & (1 << SC64_MPU_ACCESS_WRITE_OFFSET))) {
            DPRINTF("Invalid access write %" PRIx64 "\n", addr);
            return -1;
        }
        break;
    }

    return 0;
}

static uint64_t sc64_mpu_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64MPUState *s = opaque;
    uint32_t val;
    uint8_t r_addr;

    r_addr = addr & 0xFC;
    val = 0;

    switch (r_addr) {
    case SC64_MPU_CONFIG:
        val = s->regs.config;
        break;
    case SC64_MPU_EADR:
        val = s->regs.eadr;
        break;
    case SC64_MPU_ESTATUS:
        val = s->regs.estatus;
        break;
    case SC64_MPU_WINDOW_REGS_START ... SC64_MPU_WINDOW_REGS_END:
        if (r_addr % 4 != 0) {
            goto unimp;
        }
        val = MPU_WINDOW_REG(s, r_addr);
        break;
    default:
unimp:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-mpu: reading at bad addr"
                      TARGET_FMT_plx, addr);
        DPRINTF("sc64-mpu: reading at bad addr %" PRIx64 "\n", addr);
        break;
    }

    DPRINTF("%s addr=%" PRIx64 ", reg=%x, val=%x\n", __func__, addr, r_addr, val);

    return val;
}

static void sc64_mpu_write(void *opaque, hwaddr addr, uint64_t val,
    unsigned size)
{
    Sc64MPUState *s = opaque;
    uint32_t mpu_lock;
    uint8_t r_addr;

    r_addr = addr & 0xFC;

    DPRINTF("%s addr=%" PRIx64 ", reg=%x, val=%" PRIx64 "\n", __func__, addr, r_addr, val);

    /* After MPU_LOCK is set, all writings are ignored
     * except interrupt clearing. */
    if (sc64_mpu_enabled(s)) {
        if (r_addr == SC64_MPU_CONFIG) {
            s->regs.config &= ~SC64_MPU_CONFIG_SPERR_MASK;
            s->regs.eadr = 0;
            s->regs.estatus = 0;
            qemu_irq_lower(s->irq);
        }
        return;
    }

    switch (r_addr) {
    case SC64_MPU_CONFIG:
        mpu_lock = val & (1 << SC64_MPU_CONFIG_MPU_LOCK_OFFSET);
        s->regs.config |= mpu_lock;
        break;
    case SC64_MPU_WINDOW_REGS_START ... SC64_MPU_WINDOW_REGS_END:
        if (r_addr % 4 != 0) {
            goto unimp;
        }
        MPU_WINDOW_REG(s, r_addr) = val & MPU_WINDOW_REG_WMASK(s, r_addr);
        break;
    default:
unimp:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-mpu: writing in read-only addr="
                      TARGET_FMT_plx, addr);
        DPRINTF("sc64-mpu: writing in read-only addr=%" PRIx64 "\n", addr);
        break;
    }
}

static const MemoryRegionOps sc64_mpu_host_ops = {
    .read = sc64_mpu_read,
    .write = sc64_mpu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static MemTxResult sc64_mpu_dev_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64MPUDevInfo *info = (Sc64MPUDevInfo *)opaque;
    Sc64MPUState *s = info->mpu_state;

    if (sc64_mpu_enabled(s)) {
        if (0 != sc64_mpu_check_access(s, info->dev_type,
                                       MPU_READ, addr, size)) {
            sc64_mpu_set_error(s, info->dev_type, MPU_READ, addr);
            return MEMTX_DECODE_ERROR;
        }
    }

    return MEMTX_OK;
}

static MemTxResult sc64_mpu_dev_write(void *opaque, hwaddr addr, unsigned size)
{
    Sc64MPUDevInfo *info = (Sc64MPUDevInfo *)opaque;
    Sc64MPUState *s = info->mpu_state;

    if (sc64_mpu_enabled(s)) {
        if (0 != sc64_mpu_check_access(s, info->dev_type,
                                       MPU_WRITE, addr, size)) {
            sc64_mpu_set_error(s, info->dev_type, MPU_WRITE, addr);
            return MEMTX_DECODE_ERROR;
        }
    }

    return MEMTX_OK;
}

static MemTxResult sc64_mpu_dev_check_access(void *opaque,
                                        hwaddr addr, int len, bool is_write)
{
    if (is_write) {
        return sc64_mpu_dev_write(opaque, addr, len);
    } else {
        return sc64_mpu_dev_read(opaque, addr, len);
    }
}

static uint64_t sc64_mpu_dma_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64MPUDevInfo *info = (Sc64MPUDevInfo *)opaque;
    Sc64MPUState *s = info->mpu_state;
    uint64_t buf;

    dma_memory_read(s->dma_as, addr, &buf, size);

    return buf;
}

static void sc64_mpu_dma_write(void *opaque, hwaddr addr, uint64_t val,
    unsigned size)
{
    Sc64MPUDevInfo *info = (Sc64MPUDevInfo *)opaque;
    Sc64MPUState *s = info->mpu_state;

    dma_memory_write(s->dma_as, addr, &val, size);
}

static const MemoryRegionOps sc64_mpu_dev_ops = {
    .check_access = sc64_mpu_dev_check_access,
    .read = sc64_mpu_dma_read,
    .write = sc64_mpu_dma_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static void sc64_mpu_reset(DeviceState *d)
{
    Sc64MPUState *s = SC64_MPU(d);
    int i;

    s->regs.config = (1 << SC64_MPU_CONFIG_HRL_OFFSET) |
                     (10 << SC64_MPU_CONFIG_NRGD_OFFSET);
    s->regs.eadr = 0;
    s->regs.estatus = 0;

    for (i = 0; i < SC64_MPU_WINDOWS; i++) {
        MPU_WINDOW_REG(s, SC64_MPU_RGi_START(i)) = 0;
        MPU_WINDOW_REG(s, SC64_MPU_RGi_END(i)) = 0xFFFFFFFF;
        MPU_WINDOW_REG(s, SC64_MPU_RGi_ACCESS(i)) = 0;

        MPU_WINDOW_REG_WMASK(s, SC64_MPU_RGi_START(i)) = 0xFFFFFFFF;
        MPU_WINDOW_REG_WMASK(s, SC64_MPU_RGi_END(i)) = 0xFFFFFFFF;
        MPU_WINDOW_REG_WMASK(s, SC64_MPU_RGi_ACCESS(i)) = 0xC0000000;
    }
}

static int sc64_mpu_init(SysBusDevice *dev)
{
    Sc64MPUState *s = SC64_MPU(dev);

    sysbus_init_irq(dev, &s->irq);

    memory_region_init_io(&s->host_mem, OBJECT(s), &sc64_mpu_host_ops, s,
                          TYPE_SC64_MPU, 0x100);
    sysbus_init_mmio(dev, &s->host_mem);

    return 0;
}

AddressSpace *sc64_mpu_register(hwaddr addr, qemu_irq irq, AddressSpace *dma_as)
{
    DeviceState *dev;
    Sc64MPUState *s;
    uint64_t dma_as_size = memory_region_size(dma_as->root);
    int i;

    dev = sysbus_create_simple(TYPE_SC64_MPU, addr, irq);
    s = SC64_MPU(dev);

    for (i = 0; i < SC64_MPU_WINDOWS; i++) {
        s->devices[i].mpu_state = s;
        s->devices[i].dev_type = i;

        memory_region_init_io(&s->dev_mem[i], OBJECT(s),
                    &sc64_mpu_dev_ops, &s->devices[i], "dev-mem", dma_as_size);
        address_space_init(&s->dev_mem_as[i], &s->dev_mem[i], "sc64-mpu");
    }

    s->dma_as = dma_as;

    return s->dev_mem_as;
}

static const VMStateDescription vmstate_sc64_mpu_regs = {
    .name = TYPE_SC64_MPU,
    .version_id = 1,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        /* FIXME: populate later */
        VMSTATE_END_OF_LIST(),
    },
};

static void sc64_mpu_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_mpu_init;
    dc->desc = "SC64 MPU";
    dc->reset = sc64_mpu_reset;
    dc->vmsd = &vmstate_sc64_mpu_regs;
}

static const TypeInfo sc64_mpu_sysbus_info = {
    .name          = TYPE_SC64_MPU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64MPUState),
    .class_init    = sc64_mpu_sysbus_class_init,
};

static void sc64_mpu_register_types(void)
{
    type_register_static(&sc64_mpu_sysbus_info);
}

type_init(sc64_mpu_register_types);
