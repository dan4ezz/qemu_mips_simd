/*
 * Copyright (C) 2014 Dmitry Smagin <dmitry.s.smagin@gmail.com>
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
#include "exec/address-spaces.h"

#define SC64_NUM_IRQS            24

#define SC64_INTC_SIZE           0x10

#define SC64_INTC_MASK0          0x0
#define SC64_INTC_MASK1          0x1
#define SC64_INTC_MASK2          0x2
#define SC64_INTC_INDEX          0x3
#define SC64_INTC_DATA           0x4
#define SC64_INTC_CODE0          0x5
#define SC64_INTC_CODE1          0x6
#define SC64_INTC_CODE2          0x7
#define SC64_INTC_CODE3          0x8
#define SC64_INTC_CODE4          0x9

#define TYPE_SC64_INTC "sc64-intc"
#define SC64_INTC(obj) OBJECT_CHECK(Sc64IntcState, (obj), TYPE_SC64_INTC)

typedef struct Sc64IntcRegs {
    uint32_t common[15];
    uint8_t map[24];
    uint8_t prior[24];
    uint8_t vector[24];
    int32_t level[24];
} Sc64IntcRegs;

typedef struct Sc64IntcState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    Sc64IntcRegs icregs;
    qemu_irq *irqs;       /* external irqs, up to 24 */
    qemu_irq *int_irqs;  /* internal mips irqs, up to 8 */
} Sc64IntcState;


static uint64_t sc64_intc_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64IntcState *s = opaque;
    uint64_t val;
    (void) size;
    unsigned index;
    unsigned s_addr = addr & 0xFF;

    switch (s_addr) {
    case SC64_INTC_MASK0:
    case SC64_INTC_MASK1:
    case SC64_INTC_MASK2:
    case SC64_INTC_INDEX:
        val = s->icregs.common[s_addr];
    break;

    case SC64_INTC_DATA:
        index = s->icregs.common[SC64_INTC_INDEX];
        if (index < SC64_NUM_IRQS/2) {
            val = s->icregs.map[index];
        } else if (index < 3*SC64_NUM_IRQS/2) {
            val = s->icregs.prior[index - SC64_NUM_IRQS/2];
        } else {
            val = s->icregs.vector[index - 3*SC64_NUM_IRQS/2];
        };
    break;

    case SC64_INTC_CODE0:
    case SC64_INTC_CODE1:
    case SC64_INTC_CODE2:
    case SC64_INTC_CODE3:
    case SC64_INTC_CODE4:
        val = s->icregs.common[s_addr];
    break;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "SC64-INTC: reading out of range at addr 0x"
                      TARGET_FMT_plx, addr);
        val = 0;
    break;
    }

    return val;
}

static void
sc64_intc_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    Sc64IntcState *s = opaque;
    (void) size;
    unsigned index;
    unsigned s_addr = addr & 0xFF;

    switch (s_addr) {
    case SC64_INTC_MASK0:
    case SC64_INTC_MASK1:
    case SC64_INTC_MASK2:
    case SC64_INTC_INDEX:
        s->icregs.common[s_addr] = val;
    break;

    case SC64_INTC_DATA:
        index = s->icregs.common[SC64_INTC_INDEX];
        if (index < SC64_NUM_IRQS/2) {
            s->icregs.map[index] = (char)(val & 0xFF);
        } else if (index < 3*SC64_NUM_IRQS/2) {
            s->icregs.prior[index - SC64_NUM_IRQS/2] = (char)(val & 0x1F);
        } else {
            s->icregs.vector[index - 3*SC64_NUM_IRQS/2] = (char)(val & 0xFF);
        };
    break;

    case SC64_INTC_CODE0:
    case SC64_INTC_CODE1:
    case SC64_INTC_CODE2:
    case SC64_INTC_CODE3:
    case SC64_INTC_CODE4:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "SC64-INTC: writing to read-only memory at 0x"
                      TARGET_FMT_plx, addr);
    break;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "SC64-INTC: writing out of range at addr 0x"
                      TARGET_FMT_plx, addr);
    break;
    }
}

static const MemoryRegionOps intc_mem_ops = {
    .read = sc64_intc_read,
    .write = sc64_intc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void sc64_irq_handler(void *opaque, int n, int level)
{
    Sc64IntcState *s = opaque;
    s->icregs.level[n] = level;
    int tgt = 0xF & (s->icregs.map[n/2] >> (4 * (n%2)));
    int i, nlevel;

    nlevel = 0;
    for (i = 0; i < SC64_NUM_IRQS; ++i) {
        if ((0xF & (s->icregs.map[i/2] >> (4 * (i%2)))) == tgt
                && s->icregs.level[i]
                && !(s->icregs.common[i/8] & (1 << (i%8)))) {
            s->icregs.common[tgt + SC64_INTC_CODE0] = s->icregs.vector[i];
            nlevel = 1;
            break;
        }
    }

    /* INTC has 10 lines, but we can only serve the first 6 of them. */
    if (tgt < 6)
        qemu_set_irq(s->int_irqs[tgt + 2], !!nlevel);

    if (n == -1) {
        printf("Serial #1 set to %u. IRQ=%i, map[6]=0x%x, mask=%i\n",
                nlevel, tgt, s->icregs.map[n/2],
                (!((s->icregs.common[n/8] & (0x1 << (n%8))) == 0)));
    }
}

static void sc64_intc_reset(DeviceState *d)
{
    Sc64IntcState *s = SC64_INTC(d);
    int i;

    for (i = 0; i < SC64_NUM_IRQS; ++i) {
        s->icregs.level[i] = 0;
    }
}

qemu_irq *sc64_intc_register(hwaddr addr, qemu_irq *int_irqs)
{
    Sc64IntcState *s;
    DeviceState *dev;

    dev = qdev_create(NULL, TYPE_SC64_INTC);
    qdev_init_nofail(dev);

    s = SC64_INTC(dev);
    s->int_irqs = int_irqs;
    s->irqs = qemu_allocate_irqs(sc64_irq_handler, s, SC64_NUM_IRQS);

    memory_region_init_io(&s->iomem, OBJECT(dev), &intc_mem_ops, s,
                                  TYPE_SC64_INTC, SC64_INTC_SIZE);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);

    return s->irqs;
}

static int sc64_intc_init(SysBusDevice *dev)
{
    /* Sc64IntcState *s = SC64_INTC(dev); */

    return 0;
}


static const VMStateDescription vmstate_sc64_intc_regs = {
    .name = TYPE_SC64_INTC,
    .version_id = 1,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        /* FIXME: populate later */
        VMSTATE_END_OF_LIST(),
    },
};


static void sc64_intc_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_intc_init;
    dc->desc = "SC64 INTC block";
    dc->reset = sc64_intc_reset;
    dc->vmsd = &vmstate_sc64_intc_regs;
}


static const TypeInfo sc64_intc_sysbus_info = {
    .name          = TYPE_SC64_INTC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64IntcState),
    .class_init    = sc64_intc_sysbus_class_init,
};

static void sc64_intc_register_types(void)
{
    type_register_static(&sc64_intc_sysbus_info);
}

type_init(sc64_intc_register_types);
