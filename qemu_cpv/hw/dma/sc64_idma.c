/*
 * Copyright (C) 2017 Denis Deryugin <deryugin.denis@gmail.com>
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
#include "hw/dma/sc64_idma.h"
#include "hw/mips/sccr-sysbus.h"
#include "hw/sysbus.h"
#include "sysemu/dma.h"
#include "qemu/typedefs.h"

#ifndef SC64_IDMA_DEBUG
#define SC64_IDMA_DEBUG 0
#endif

#define DPRINT(...) do { \
    if (SC64_IDMA_DEBUG > 1) { \
        fprintf(stderr,  ": %s: ", __func__); \
        fprintf(stderr, ## __VA_ARGS__); \
    } \
} while (0);

#define EPRINT(...) do { \
    if (SC64_IDMA_DEBUG > 0) { \
        fprintf(stderr,  ": %s: ", __func__); \
        fprintf(stderr, ## __VA_ARGS__); \
    } \
} while (0);

#define TYPE_SC64_IDMA "sc64-idma"
#define SC64_IDMA(obj) OBJECT_CHECK(Sc64IdmaState, (obj), TYPE_SC64_IDMA)

#define SC64_REG_SIZE                   0x40

#define SC64_IDMA_SRC                   0
#define SC64_IDMA_DST                   1
#define SC64_IDMA_SIZE_CTRL             2
# define SC64_IDMA_CTRL_SRC             (1 <<  0)
# define SC64_IDMA_PCI_SRC              (1 <<  1)
# define SC64_IDMA_CTRL_DST             (1 <<  4)
# define SC64_IDMA_PCI_DST              (1 <<  5)
# define SC64_IDMA_IRQ_EN               (1 <<  8)
# define SC64_IDMA_IRQ                  (1 <<  9)
# define SC64_IDMA_BYTE_SWAP_OFFT       10
# define SC64_IDMA_SIZE_OFFT            32
#define SC64_IDMA_COUNT_CTRL            3
# define SC64_IDMA_START                (1 <<  0)
# define SC64_IDMA_RERROR               (1 <<  1)
# define SC64_IDMA_M                    (1 <<  2)
# define SC64_IDMA_3D                   (1 <<  3)
# define SC64_IDMA_WERROR               (1 <<  4)
# define SC64_IDMA_AXI_SIZE_OFFT        8
# define SC64_IDMA_DESC_COUNT_OFFT      32
# define SC64_IDMA_DESC_COUNT_MASK      0xFFFF00000000
#define SC64_IDMA_ERROR                 (SC64_IDMA_RERROR | SC64_IDMA_WERROR)
#define SC64_IDMA_DESCR                 4
#define SC64_IDMA_ZYX                   5
# define SC64_IDMA_SRC_Y_OFFT           0
# define SC64_IDMA_DST_Y_OFFT           16
# define SC64_IDMA_SRC_Z_OFFT           32
# define SC64_IDMA_DST_Z_OFFT           48
#define SC64_IDMA_STEP                  6
#define SC64_IDMA_STEPX                 7

static const uint64_t sc64_idma_zero_mask[] = {
    /* Unused bits always set to zero */
    0x0000000000000000,
    0x0000000000000000,
    0x00000000FFFFF088,
    0xFFFF0000FFFFFC20,
    0xFFFFFFF000000007,
    0xFFFF000000000000,
    0x0000000000000000,
    0xFFFFFFFFFFFFFFFF,
};

#define REG_IDX(x) ((x & 0xfff) >> 2)

#define SC64_IDMA_LINEAR_DESC_SZ    (sizeof(uint64_t) * 4)
#define SC64_IDMA_3D_DESC_SZ        (sizeof(uint64_t) * 6)

struct sc64_idma_desc {
    uint64_t src;
    uint64_t dst;
    uint64_t size_ctrl;
    /* Following fields are not used
     * in linear descriptors */
    uint64_t zyx;
    uint64_t step;
    uint64_t stepx;
};

typedef struct Sc64IdmaState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint64_t regs[SC64_REG_SIZE >> 3];

    AddressSpace *dma_as;
    AddressSpace pci0_as;
    AddressSpace pci1_as;

    qemu_irq irq;
} Sc64IdmaState;

#define IDMA_BUF_SZ 512

static uint8_t idma_write_buffer[IDMA_BUF_SZ];

static uint16_t swap_hword(uint16_t hw)
{
    return ((hw & 0x0ff) << 8) | ((hw >> 8) & 0x0ff);
}

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
sc64_idma_raw_transfer(AddressSpace *src_as, AddressSpace *dst_as,
        uint64_t src_addr, uint64_t dst_addr, uint64_t len, int sw_type)
{
    uint64_t write_len = sizeof(idma_write_buffer);
    int i;

    while (len > 0) {
        if (len < write_len) {
            write_len = len;
        }

        dma_memory_read(src_as, src_addr, idma_write_buffer, write_len);

        for (i = 0; i < write_len; i++) {
            if (SC64_IDMA_DEBUG > 2) {
                printf("%02x ", idma_write_buffer[i]);

                if ((i % 32 == 0) || (i == write_len - 1)) {
                    printf("\n");
                }
            }
        }

        switch (sw_type) {
        case 1:
            for (i = 0; i < IDMA_BUF_SZ / 8; i++) {
                ((uint64_t *)idma_write_buffer)[i] =
                    swap_dword(((uint64_t *)idma_write_buffer)[i]);
            }
            break;
        case 2:
            for (i = 0; i < IDMA_BUF_SZ / 4; i++) {
                ((uint32_t *)idma_write_buffer)[i] =
                    swap_word(((uint32_t *)idma_write_buffer)[i]);
            }
            break;
        case 3:
            for (i = 0; i < IDMA_BUF_SZ / 2; i++) {
                ((uint16_t *)idma_write_buffer)[i] =
                    swap_hword(((uint16_t *)idma_write_buffer)[i]);
            }
            break;
        default:
            break;
        }

        dma_memory_write(dst_as, dst_addr, idma_write_buffer, write_len);

        src_addr += write_len;
        dst_addr += write_len;
        len -= write_len;
    }
}

static void
sc64_idma_desc_transfer(Sc64IdmaState *s, struct sc64_idma_desc *desc)
{
    uint64_t src_addr, dst_addr, len;
    uint64_t x_sz, y_sz, z_sz;
    uint64_t dst_z_step, dst_y_step, dst_x_step;
    uint64_t src_z_step, src_y_step, src_x_step;

    int i, j, sw_type = (desc->size_ctrl >> 10) & 3;

    AddressSpace *src_as;
    AddressSpace *dst_as;

    DPRINT("DMA transfer len=%ld; swap type=%ld\n",
            len, (desc->size_ctrl >> 10) & 0x3);

    if (desc->size_ctrl & SC64_IDMA_CTRL_SRC) {
        if (desc->size_ctrl & SC64_IDMA_PCI_SRC) {
            src_as = &s->pci1_as;
            DPRINT("source address space:      PCI1\n");
        } else {
            src_as = &s->pci0_as;
            DPRINT("source address space:      PCI0\n");
        }
    } else {
        src_as = s->dma_as;
        DPRINT("source address space:      RAM\n");
    }

    if (desc->size_ctrl & SC64_IDMA_CTRL_DST) {
        if (desc->size_ctrl & SC64_IDMA_PCI_DST) {
            dst_as = &s->pci1_as;
            DPRINT("destination address space: PCI1\n");
        } else {
            dst_as = &s->pci0_as;
            DPRINT("destination address space: PCI0\n");
        }
    } else {
        dst_as = s->dma_as;
        DPRINT("destination address space: RAM\n");
    }

    src_addr = desc->src;
    dst_addr = desc->dst;

    DPRINT("src_addr = %08lx; dst_addr = %08lx\n", src_addr, dst_addr);

    if (s->regs[SC64_IDMA_COUNT_CTRL] & SC64_IDMA_3D) {
        /* 3D DMA transfer */
        x_sz = 1 + ((desc->zyx >>  0) & 0xFFFF);
        y_sz = 1 + ((desc->zyx >> 16) & 0xFFFF);
        z_sz = 1 + ((desc->zyx >> 32) & 0xFFFF);

        dst_z_step = (desc->step  >> 48) & 0xFFFF;
        dst_y_step = (desc->step  >> 32) & 0xFFFF;
        dst_x_step = (desc->stepx >> 32) & 0xFFFF;
        src_z_step = (desc->step  >> 16) & 0xFFFF;
        src_y_step = (desc->step  >>  0) & 0xFFFF;
        src_x_step = (desc->stepx >>  0) & 0xFFFF;

        DPRINT("3d transfer, sz=(%ld,%ld,%ld), step=(%ld,%ld;%ld,%ld)\n",
                x_sz, y_sz, z_sz,
                dst_y_step, dst_z_step, src_y_step, src_z_step);

        src_addr += src_x_step;
        dst_addr += dst_x_step;

        for (i = 0; i < z_sz; i++) {
            for (j = 0; j < y_sz; j++) {
                sc64_idma_raw_transfer(
                            src_as, dst_as,
                            src_addr, dst_addr,
                            x_sz, sw_type);

                src_addr += src_y_step + src_x_step;
                dst_addr += dst_y_step + dst_x_step;
            }

            src_addr -= src_y_step;
            dst_addr -= dst_y_step;

            src_addr += src_z_step;
            dst_addr += src_z_step;
        }
    } else {
        /* Linear DMA transfer */
        len = 1 + (desc->size_ctrl >> SC64_IDMA_SIZE_OFFT);
        sc64_idma_raw_transfer(
                src_as, dst_as,
                src_addr, dst_addr,
                len, sw_type);
    }
}

static void
sc64_idma_start(Sc64IdmaState *s)
{
    uint64_t count;
    uint64_t desc_size;
    void *desc_ptr;
    struct sc64_idma_desc desc;

    if (s->regs[SC64_IDMA_COUNT_CTRL] & SC64_IDMA_M) {
        /* Read DMA config from descriptors */
        count = ((s->regs[SC64_IDMA_COUNT_CTRL]) >> 32) & 0xFFFF;
        desc_ptr = (void *) s->regs[SC64_IDMA_DESCR];
        if (s->regs[SC64_IDMA_COUNT_CTRL] & SC64_IDMA_3D) {
            desc_size = SC64_IDMA_3D_DESC_SZ;
        } else {
            desc_size = SC64_IDMA_LINEAR_DESC_SZ;
        }

        while (count--) {
            dma_memory_read(s->dma_as, (hwaddr) desc_ptr, &desc, desc_size);
            sc64_idma_desc_transfer(s, &desc);
            s->regs[SC64_IDMA_COUNT_CTRL] &=
                ~((uint64_t)SC64_IDMA_DESC_COUNT_MASK);
            s->regs[SC64_IDMA_COUNT_CTRL] |=
                count << SC64_IDMA_DESC_COUNT_OFFT;
            desc_ptr += desc_size;
        }

        s->regs[SC64_IDMA_COUNT_CTRL] &= ~((uint64_t)SC64_IDMA_DESC_COUNT_MASK);
    } else {
        /* Read DMA config from registers */
        desc.src       = s->regs[SC64_IDMA_SRC];
        desc.dst       = s->regs[SC64_IDMA_DST];
        desc.size_ctrl = s->regs[SC64_IDMA_SIZE_CTRL];

        if (s->regs[SC64_IDMA_COUNT_CTRL] & SC64_IDMA_3D) {
            desc.zyx       = s->regs[SC64_IDMA_ZYX];
            desc.step      = s->regs[SC64_IDMA_STEP];
            desc.stepx     = s->regs[SC64_IDMA_STEPX];
        }

        sc64_idma_desc_transfer(s, &desc);
    }

    if (s->regs[SC64_IDMA_SIZE_CTRL] & SC64_IDMA_IRQ_EN) {
        s->regs[SC64_IDMA_SIZE_CTRL] |= SC64_IDMA_IRQ;
        DPRINT("raise IRQ\n");
        qemu_irq_raise(s->irq);
    }
}

static void
sc64_idma_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    DPRINT("addr=%02lx; val=%016lx; size=%d\n", addr, val, size);

    Sc64IdmaState *s = opaque;
    int reg_idx = addr / 8;

    switch (reg_idx) {
    case SC64_IDMA_SRC:
    case SC64_IDMA_DST:
    case SC64_IDMA_DESCR:
    case SC64_IDMA_ZYX:
    case SC64_IDMA_STEP:
        val &= ~sc64_idma_zero_mask[reg_idx];
        s->regs[reg_idx] = val;

        break;
    case SC64_IDMA_SIZE_CTRL:
        val &= ~sc64_idma_zero_mask[reg_idx];
        if (val & SC64_IDMA_IRQ) {
            val &= ~SC64_IDMA_IRQ;
            DPRINT("lower IRQ\n");
            qemu_irq_lower(s->irq);
        }

        s->regs[reg_idx] = val;
        break;
    case SC64_IDMA_COUNT_CTRL:
        val &= ~sc64_idma_zero_mask[reg_idx];
        s->regs[reg_idx] = val;

        if (val & SC64_IDMA_START) {
            sc64_idma_start(s);
        }

        s->regs[reg_idx] &= ~(SC64_IDMA_START | SC64_IDMA_ERROR);

        break;
    default:
        printf("Bad register idx 0x%x\n", (int)reg_idx);
        break;
    }
}

static uint64_t sc64_idma_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64IdmaState *s = opaque;
    int reg_idx = addr / 8;
    uint64_t val;

    switch (reg_idx) {
    case SC64_IDMA_SRC:
    case SC64_IDMA_DST:
    case SC64_IDMA_SIZE_CTRL:
    case SC64_IDMA_COUNT_CTRL:
    case SC64_IDMA_DESCR:
    case SC64_IDMA_ZYX:
    case SC64_IDMA_STEP:
        val = s->regs[reg_idx];
        break;
    default:
        printf("Bad register idx 0x%x\n", (int)reg_idx);
        break;
    }

    DPRINT("addr=%02lx; val=%016lx; size=%d\n", addr, val, size);

    return val;
}

static const MemoryRegionOps idma_mem_ops = {
    .read = sc64_idma_read,
    .write = sc64_idma_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 8,
        .max_access_size = 8,
    },
};

static void sc64_idma_reset(DeviceState *dev)
{
    Sc64IdmaState *s = SC64_IDMA(dev);
    /* All registers are zero by default */
    memset(s->regs, 0, SC64_REG_SIZE);
}

void sysbus_idma_register(const char *name, hwaddr addr, AddressSpace *dma_as,
        MemoryRegion *pci0_mr, MemoryRegion *pci1_mr, qemu_irq irq)
{
    DeviceState *dev;
    SysBusDevice *sbd;
    Sc64IdmaState *s;

    dev = qdev_create(NULL, name);
    s = SC64_IDMA(dev);
    s->dma_as = dma_as;
    if (pci0_mr) {
        address_space_init(&s->pci0_as, pci0_mr, "idma_pci0");
    }
    if (pci1_mr) {
        address_space_init(&s->pci1_as, pci1_mr, "idma_pci1");
    }
    qdev_init_nofail(dev);

    sbd = SYS_BUS_DEVICE(dev);
    if (addr != (hwaddr)-1) {
        sysbus_mmio_map(sbd, 0, addr);
    }
    sysbus_connect_irq(sbd, 0, irq);
}

static int sc64_idma_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    Sc64IdmaState *s = SC64_IDMA(dev);

    memory_region_init_io(&s->iomem, OBJECT(s),
                            &idma_mem_ops, s, TYPE_SC64_IDMA, SC64_REG_SIZE);

    sysbus_init_mmio(sbd, &s->iomem);

    sysbus_init_irq(sbd, &s->irq);

    return 0;
}

static void sc64_idma_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_idma_init;
    dc->desc = "SC64 IDMA block";
    dc->reset = sc64_idma_reset;
}

static const TypeInfo sc64_idma_sysbus_info = {
    .name          = TYPE_SC64_IDMA,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64IdmaState),
    .class_init    = sc64_idma_sysbus_class_init,
};

static void sc64_idma_register_types(void)
{
    type_register_static(&sc64_idma_sysbus_info);
}

type_init(sc64_idma_register_types)
