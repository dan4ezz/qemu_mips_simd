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
#include "hw/hw.h"
#include "hw/dma/sc64_dma.h"
#include "hw/mips/sccr-sysbus.h"
#include "hw/sysbus.h"
#include "sysemu/dma.h"
#include "qemu/typedefs.h"

#define SC64_SYS_TEST1          0x158
#define SC64_SYS_TEST2          0x15c
#define SC64_SYS_DMA_PCI_ADDR   0x160
#define SC64_SYS_DMA_MEM_ADDR   0x164
#define SC64_SYS_DMA_LEN        0x168
#define SC64_SYS_DMA_CTRL       0x16c
# define DMA_CTRL_START         (1 << 0)
# define DMA_CTRL_CONST         (1 << 3)
# define DMA_CTRL_CONST_INC     (1 << 5)

#define SC64_SYS_DMA_INC_LOW    0x3a0
#define SC64_SYS_DMA_INC_HI     0x3a4
#define SC64_SYS_DMA_ADDR_HI    0x3a8

/* Registers 0x158, 0x15с, and 0x160 - 0x16c */
#define SC64_DMA_REGS_OFFSET1   0x158
/* Registers 0x3a0 - 0x3a8 */
#define SC64_DMA_REGS_OFFSET2   0x3a0

#define REG_IDX(x) ((x & 0xfff) >> 2)

#define TYPE_SC64_DMA "sc64-dma"
#define SC64_DMA(obj) OBJECT_CHECK(Sc64DmaState, (obj), TYPE_SC64_DMA)

#ifdef DEBUG
#define DPRINTF(fmt, ...) \
    do { printf("sc64-dma: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

#define SC64_DMA_SIZE          0x1000

typedef struct Sc64DmaState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem[2];
    uint32_t regs[SC64_DMA_SIZE << 2];

    AddressSpace *dma_as;
} Sc64DmaState;

#define SCCR_BUF_SZ 512

static uint8_t sccr_write_buffer[SCCR_BUF_SZ];

static uint32_t swap_word(uint32_t w)
{
    return ((w & 0x0ff) << 24) | (((w >> 8) & 0x0ff) << 16) |
        (((w >> 16) & 0x0ff) << 8) | ((w >> 24) & 0x0ff);
}

static uint64_t swap_dword(uint64_t d)
{
    uint32_t wl, wr;
    wr = d;
    wr = swap_word(wr);
    wl = d >> 32;
    wl = swap_word(wl);

    return (((uint64_t)wr) << 32) | wl;
}

static void
sc64_dma_const_start(AddressSpace *as, uint64_t dest, uint64_t src, size_t len,
    uint64_t inc)
{
    uint64_t data;

    while (len >= 8) {
        data = swap_dword(src);
        dma_memory_write(as, dest, &data, 8);

        src += inc;
        dest += 8;
        len -= 8;
    }

    data = swap_dword(src);
    dma_memory_write(as, dest, &data, len);
}

static void
sc64_dma_start(Sc64DmaState *s)
{
    uint64_t full_src;
    uint64_t full_dest;
    size_t write_len;
    size_t full_len;
    uint32_t addr_hi;
    uint32_t ctrl;

    ctrl = s->regs[REG_IDX(SC64_SYS_DMA_CTRL)];

    if (!(ctrl & DMA_CTRL_START)) {
        printf("ret %x & %x\n", ctrl, DMA_CTRL_START);
        return;
    }

    addr_hi = s->regs[REG_IDX(SC64_SYS_DMA_ADDR_HI)];
    full_dest = (uint64_t)(addr_hi & 0xF0) << 28;
    full_dest |= s->regs[REG_IDX(SC64_SYS_DMA_MEM_ADDR)];

    full_len = s->regs[REG_IDX(SC64_SYS_DMA_LEN)];

    if (ctrl & DMA_CTRL_CONST) {
        uint64_t src;
        uint64_t inc = 0;

        src = (uint64_t)(s->regs[REG_IDX(SC64_SYS_TEST2)]) << 32;
        src |= (uint64_t)(s->regs[REG_IDX(SC64_SYS_TEST1)]);

        if (ctrl & DMA_CTRL_CONST_INC) {
            inc = (uint64_t)(s->regs[REG_IDX(SC64_SYS_DMA_INC_HI)]) << 32;
            inc |= (uint64_t)(s->regs[REG_IDX(SC64_SYS_DMA_INC_LOW)]);
        }

        sc64_dma_const_start(s->dma_as, full_dest, src, full_len, inc);
    } else {
        full_src = (uint64_t)(addr_hi & 0x0F) << 32;
        full_src |= s->regs[REG_IDX(SC64_SYS_DMA_PCI_ADDR)];

        write_len = sizeof(sccr_write_buffer);

        while (full_len > 0) {
            if (full_len < write_len) {
                write_len = full_len;
            }

            dma_memory_read(s->dma_as, full_src, sccr_write_buffer, write_len);
            dma_memory_write(s->dma_as, full_dest, sccr_write_buffer, write_len);

            full_len -= write_len;
            full_src += write_len;
            full_dest += write_len;
        }
    }
}

static void sc64_dma_write(void *opaque, hwaddr addr,
    uint64_t val, unsigned size)
{
    Sc64DmaState *s = opaque;
    int raddr = (addr & 0xfff) >> 2;

    DPRINTF("addr=%02lx; val=%016lx; size=%d\n", addr, val, size);

    switch (addr) {
    case SC64_SYS_TEST1:
    case SC64_SYS_TEST2:
    case SC64_SYS_DMA_PCI_ADDR:
    case SC64_SYS_DMA_MEM_ADDR:
    case SC64_SYS_DMA_LEN:
    case SC64_SYS_DMA_INC_HI:
    case SC64_SYS_DMA_INC_LOW:
    case SC64_SYS_DMA_ADDR_HI:
        s->regs[raddr] = val;
    case SC64_SYS_DMA_CTRL:
        s->regs[raddr] = val;
        if (val & DMA_CTRL_START) {
            sc64_dma_start(s);
            s->regs[raddr] &= ~DMA_CTRL_START;
        }
    break;
    default:
        printf("Bad register idx 0x%x\n", raddr);
        break;
    }
}

static uint64_t sc64_dma_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64DmaState *s = opaque;
    int raddr = (addr & 0xfff) >> 2;
    uint64_t val;

    switch (addr) {
    case SC64_SYS_TEST1:
    case SC64_SYS_TEST2:
    case SC64_SYS_DMA_PCI_ADDR:
    case SC64_SYS_DMA_MEM_ADDR:
    case SC64_SYS_DMA_LEN:
    case SC64_SYS_DMA_CTRL:
    case SC64_SYS_DMA_INC_HI:
    case SC64_SYS_DMA_INC_LOW:
    case SC64_SYS_DMA_ADDR_HI:
        val = s->regs[raddr];
        break;
    default:
        printf("Bad register idx 0x%x\n", raddr);
        break;
    }

    DPRINTF("addr=%02lx; val=%016lx; size=%d\n", addr, val, size);

    return val;
}

#define SC64_DMA_READ_FUNC(x, offset) \
    static uint64_t sc64_dma_read##x(void *opaque, hwaddr addr, unsigned size) \
    { \
        return sc64_dma_read(opaque, addr + offset, size); \
    }

#define SC64_DMA_WRITE_FUNC(x, offset) \
    static void sc64_dma_write##x(void *opaque, hwaddr addr, \
        uint64_t val, unsigned size) \
    { \
        sc64_dma_write(opaque, addr + offset, val, size); \
    }

SC64_DMA_READ_FUNC(0, SC64_DMA_REGS_OFFSET1)
SC64_DMA_WRITE_FUNC(0, SC64_DMA_REGS_OFFSET1)
SC64_DMA_READ_FUNC(1, SC64_DMA_REGS_OFFSET2)
SC64_DMA_WRITE_FUNC(1, SC64_DMA_REGS_OFFSET2)

static const MemoryRegionOps dma_mem_ops0 = {
    .read = sc64_dma_read0,
    .write = sc64_dma_write0,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static const MemoryRegionOps dma_mem_ops1 = {
    .read = sc64_dma_read1,
    .write = sc64_dma_write1,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static void sc64_dma_reset(DeviceState *dev)
{
    Sc64DmaState *s = SC64_DMA(dev);
    /* All registers are zero by default */
    memset(s->regs, 0, SC64_DMA_SIZE);
}

void sysbus_dma_register(hwaddr addr, AddressSpace *dma_as)
{
    DeviceState *dev;
    SysBusDevice *sbd;
    Sc64DmaState *s;

    dev = qdev_create(NULL, TYPE_SC64_DMA);
    s = SC64_DMA(dev);
    s->dma_as = dma_as;
    qdev_init_nofail(dev);

    sbd = SYS_BUS_DEVICE(dev);
    if (addr != (hwaddr)-1) {
        sysbus_mmio_map(sbd, 0, addr + SC64_DMA_REGS_OFFSET1);
        sysbus_mmio_map(sbd, 1, addr + SC64_DMA_REGS_OFFSET2);
    }
}

static int sc64_dma_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    Sc64DmaState *s = SC64_DMA(dev);

    /* Map registers 0x158, 0x15с, and 0x160 - 0x16c */
    memory_region_init_io(&s->iomem[0], OBJECT(s),
                          &dma_mem_ops0, s, TYPE_SC64_DMA, 0x4 * 6);
    sysbus_init_mmio(sbd, &s->iomem[0]);

    /* Map registers 0x3a0 - 0x3a8 */
    memory_region_init_io(&s->iomem[1], OBJECT(s),
                          &dma_mem_ops1, s, TYPE_SC64_DMA, 0x4 * 3);
    sysbus_init_mmio(sbd, &s->iomem[1]);

    return 0;
}

static void sc64_dma_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_dma_init;
    dc->desc = "SC64 DMA block";
    dc->reset = sc64_dma_reset;
}

static const TypeInfo sc64_dma_sysbus_info = {
    .name          = TYPE_SC64_DMA,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64DmaState),
    .class_init    = sc64_dma_sysbus_class_init,
};

static void sc64_dma_register_types(void)
{
    type_register_static(&sc64_dma_sysbus_info);
}

type_init(sc64_dma_register_types)
