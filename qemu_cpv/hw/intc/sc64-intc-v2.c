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
#include "exec/address-spaces.h"
#include "qemu/bitops.h"

#define SC64_INTC_V2_INPUT_LINES    48
#define SC64_INTC_V2_OUTPUT_LINES   6

#define SC64_INTC_SIZE_V2           0x90

#define SC64_INTC_V2_MASK           0x00
#define SC64_INTC_V2_VECTOR         0x08
#define SC64_INTC_V2_PRIOR0         0x10
#define SC64_INTC_V2_PRIOR1         0x18
#define SC64_INTC_V2_PRIOR2         0x20
#define SC64_INTC_V2_PRIOR3         0x28
#define SC64_INTC_V2_PRIOR4         0x30
#define SC64_INTC_V2_PRIOR5         0x38
#define SC64_INTC_V2_MAP0           0x50
#define SC64_INTC_V2_MAP1           0x58
#define SC64_INTC_V2_MAP2           0x60
#define SC64_INTC_V2_MAP3           0x68
#define SC64_INTC_V2_MAP4           0x70
#define SC64_INTC_V2_MAP5           0x78

#define TYPE_SC64_INTC_V2 "sc64-intc-v2"
#define SC64_INTC_V2(obj) OBJECT_CHECK(Sc64IntcStateV2, (obj), TYPE_SC64_INTC_V2)

typedef struct Sc64IntcRegsV2 {
    uint64_t mask;
    uint64_t vector;
    uint64_t prior[SC64_INTC_V2_OUTPUT_LINES];
    uint64_t map[SC64_INTC_V2_OUTPUT_LINES];
} Sc64IntcRegsV2;

typedef struct Sc64IntcStateV2 {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;

    Sc64IntcRegsV2 icregs;
    uint64_t input_irq;

    qemu_irq *irqs;
    qemu_irq *int_irqs;
} Sc64IntcStateV2;

static void sc64_intc_set_outputs(Sc64IntcStateV2 *s);

static inline uint64_t extract64_bytes(uint64_t value, int start, int length)
{
    return extract64(value, 8 * start, 8 * length);
}

static inline uint64_t deposit64_bytes(uint64_t value, int start, int length,
                                       uint64_t fieldval)
{
    return deposit64(value, 8 * start, 8 * length, fieldval);
}

static uint64_t sc64_intc_v2_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64IntcStateV2 *s = opaque;
    uint64_t val;
    uint64_t r_addr;
    uint8_t index;

    r_addr = addr & 0xF8;
    val = 0;

    switch (r_addr) {
    case SC64_INTC_V2_MASK:
        val = extract64_bytes(s->icregs.mask, addr & 0x7, size);
        break;
    case SC64_INTC_V2_VECTOR:
        val = extract64_bytes(s->icregs.vector, addr & 0x7, size);
        break;
    case SC64_INTC_V2_PRIOR0:
    case SC64_INTC_V2_PRIOR1:
    case SC64_INTC_V2_PRIOR2:
    case SC64_INTC_V2_PRIOR3:
    case SC64_INTC_V2_PRIOR4:
    case SC64_INTC_V2_PRIOR5:
        index = (r_addr - 0x10) >> 3;
        val = extract64_bytes(s->icregs.prior[index], addr & 0x7, size);
        break;
    case SC64_INTC_V2_MAP0:
    case SC64_INTC_V2_MAP1:
    case SC64_INTC_V2_MAP2:
    case SC64_INTC_V2_MAP3:
    case SC64_INTC_V2_MAP4:
    case SC64_INTC_V2_MAP5:
        index = (r_addr - 0x50) >> 3;
        val = extract64_bytes(s->icregs.map[index], addr & 0x7, size);
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "SC64-INTC: reading out of range at addr 0x"
                      TARGET_FMT_plx, addr);
    break;
    }

    return val;
}

static void
sc64_intc_v2_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    Sc64IntcStateV2 *s = opaque;
    uint64_t r_addr;
    uint8_t index;
    uint64_t new_val;

    r_addr = addr & 0xF8;

    switch (r_addr) {
    case SC64_INTC_V2_MASK:
        new_val = deposit64_bytes(s->icregs.mask, addr & 0x7, size, val);
        s->icregs.mask = new_val;
        break;
    case SC64_INTC_V2_VECTOR:
        break;
    case SC64_INTC_V2_PRIOR0:
    case SC64_INTC_V2_PRIOR1:
    case SC64_INTC_V2_PRIOR2:
    case SC64_INTC_V2_PRIOR3:
    case SC64_INTC_V2_PRIOR4:
    case SC64_INTC_V2_PRIOR5:
        index = (r_addr - 0x10) >> 3;
        new_val = deposit64_bytes(s->icregs.prior[index], addr & 0x7, size, val);
        s->icregs.prior[index] = new_val;
        break;
    case SC64_INTC_V2_MAP0:
    case SC64_INTC_V2_MAP1:
    case SC64_INTC_V2_MAP2:
    case SC64_INTC_V2_MAP3:
    case SC64_INTC_V2_MAP4:
    case SC64_INTC_V2_MAP5:
        index = (r_addr - 0x50) >> 3;
        new_val = deposit64_bytes(s->icregs.map[index], addr & 0x7, size, val);
        s->icregs.map[index] = new_val;
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "SC64-INTC: writing out of range at addr 0x"
                      TARGET_FMT_plx, addr);
    break;
    }

    sc64_intc_set_outputs(s);
}

static const MemoryRegionOps intc_mem_ops = {
    .read = sc64_intc_v2_read,
    .write = sc64_intc_v2_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static inline uint8_t sc64_intc_get_prio(Sc64IntcStateV2 *s, int in_irq)
{
    return s->icregs.prior[in_irq / 8] & (0xFF << (in_irq % 8));
}

static inline bool sc64_intc_irq_masked(Sc64IntcStateV2 *s, int in_irq)
{
    return s->icregs.mask & (((uint64_t) 1) << in_irq);
}

/* Calculates output interrupt number */
static int sc64_intc_v2_get_output_irq(Sc64IntcStateV2 *s, int in_irq)
{
    int res;
    int i = in_irq / 8;
    int j = in_irq % 8;
    uint64_t out_irq;

    out_irq = extract64(s->icregs.map[i], 8 * j, SC64_INTC_V2_OUTPUT_LINES);
    res = find_first_bit(&out_irq, SC64_INTC_V2_OUTPUT_LINES);

    return res == SC64_INTC_V2_OUTPUT_LINES ? -1 : res;
}

static void sc64_intc_set_outputs(Sc64IntcStateV2 *s)
{
    int vector[SC64_INTC_V2_OUTPUT_LINES];
    int out_irq;
    int i;

    for (i = 0; i < SC64_INTC_V2_OUTPUT_LINES; i++) {
        vector[i] = -1;
    }

    for (i = 0; i < SC64_INTC_V2_INPUT_LINES; i++) {
        out_irq = sc64_intc_v2_get_output_irq(s, i);
        if (out_irq == -1 || test_bit(i, &s->input_irq) == 0) {
            continue;
        }
        if (sc64_intc_irq_masked(s, i)) {
            continue;
        }
        if (vector[out_irq] == -1) {
            vector[out_irq] = i;
            continue;
        }
        if (sc64_intc_get_prio(s, vector[out_irq])
                        < sc64_intc_get_prio(s, i)) {
            vector[out_irq] = i;
        }
    }

    for (i = 0; i < SC64_INTC_V2_OUTPUT_LINES; i++) {
        if (vector[i] == -1) {
            qemu_irq_lower(s->int_irqs[i + 2]);
        } else {
            s->icregs.vector = deposit64(s->icregs.vector, 8 * i,
                SC64_INTC_V2_OUTPUT_LINES, vector[i]);
            qemu_irq_raise(s->int_irqs[i + 2]);
        }
    }
}

static void sc64_intc_v2_irq_handler(void *opaque, int in_irq, int level)
{
    Sc64IntcStateV2 *s = opaque;
    s->input_irq = deposit64(s->input_irq, in_irq, 1, level);
    if (!sc64_intc_irq_masked(s, in_irq)) {
        sc64_intc_set_outputs(s);
    }
}

static void sc64_intc_v2_reset(DeviceState *d)
{
    Sc64IntcStateV2 *s = SC64_INTC_V2(d);
    int i;

    s->icregs.mask = 0;
    s->icregs.vector = 0;

    for (i = 0; i < SC64_INTC_V2_OUTPUT_LINES; ++i) {
        s->icregs.prior[i] = 0;
        s->icregs.map[i] = 0;
    }
    s->input_irq = 0;
}

qemu_irq *sc64_intc_v2_register(hwaddr addr, qemu_irq *int_irqs)
{
    Sc64IntcStateV2 *s;
    DeviceState *dev;

    dev = qdev_create(NULL, TYPE_SC64_INTC_V2);
    qdev_init_nofail(dev);

    s = SC64_INTC_V2(dev);
    s->int_irqs = int_irqs;
    s->irqs = qemu_allocate_irqs(sc64_intc_v2_irq_handler, s,
                                    SC64_INTC_V2_INPUT_LINES);

    memory_region_init_io(&s->iomem, OBJECT(dev), &intc_mem_ops, s,
                                  TYPE_SC64_INTC_V2, SC64_INTC_SIZE_V2);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);

    return s->irqs;
}

static int sc64_intc_v2_init(SysBusDevice *dev)
{
    return 0;
}

static const VMStateDescription vmstate_sc64_intc_v2_regs = {
    .name = TYPE_SC64_INTC_V2,
    .version_id = 1,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        /* FIXME: populate later */
        VMSTATE_END_OF_LIST(),
    },
};

static void sc64_intc_v2_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_intc_v2_init;
    dc->desc = "SC64 INTC block";
    dc->reset = sc64_intc_v2_reset;
    dc->vmsd = &vmstate_sc64_intc_v2_regs;
}

static const TypeInfo sc64_intc_v2_sysbus_info = {
    .name          = TYPE_SC64_INTC_V2,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64IntcStateV2),
    .class_init    = sc64_intc_v2_sysbus_class_init,
};

static void sc64_intc_v2_register_types(void)
{
    type_register_static(&sc64_intc_v2_sysbus_info);
}

type_init(sc64_intc_v2_register_types);
