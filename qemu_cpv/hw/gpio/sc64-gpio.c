/*
 * Copyright (C) 2014 Antony Pavlov <antonynpavlov@gmail.com>
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
#include "qemu/log.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/gpio/sc64-gpio.h"
#include "qemu/typedefs.h"
#include "qapi/visitor.h"

#define TYPE_SC64_GPIO "sc64-gpio"
#define SC64_GPIO(obj) OBJECT_CHECK(Sc64GpioState, (obj), TYPE_SC64_GPIO)

#define SC64_GPIO_PORT_NUM      4
#define SC64_GPIO_LINE_NUM      (SC64_GPIO_PORT_NUM * 8)

#define SC64_OUTER_IRQ_NUM      2
#define SC64_OUTER_INTC_SIZE    4

#define SC64_GPIO_HIGH      0
#define SC64_GPIO_LOW       1
#define SC64_GPIO_RISING    2
#define SC64_GPIO_FALLING   3

typedef enum {
    SC64_GPIO_IRQ_NONE  = -1,
    SC64_GPIO_IRQ_A2    =  0,
    SC64_GPIO_IRQ_A3    =  1,
    SC64_GPIO_IRQ_B2    =  2,
    SC64_GPIO_IRQ_B3    =  3,
    SC64_GPIO_IRQ_INT0  =  4,
    SC64_GPIO_IRQ_INT1  =  5,
    SC64_GPIO_IRQ_INT2  =  6,
    SC64_GPIO_IRQ_INT3  =  7,
    SC64_GPIO_IRQ_INT4  =  8,
    SC64_GPIO_IRQ_INT5  =  9,
    SC64_GPIO_IRQ_INT6  = 10,
    SC64_GPIO_IRQ_INT7  = 11
} Sc64GpioIntNum;

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem_gpio;
    MemoryRegion iomem_outer;
    qemu_irq irq[12];
    qemu_irq handler[SC64_GPIO_LINE_NUM];

    uint8_t dir[SC64_GPIO_PORT_NUM];
    uint8_t in[SC64_GPIO_PORT_NUM];
    uint8_t out[SC64_GPIO_PORT_NUM];
    uint8_t func[SC64_GPIO_PORT_NUM];
    uint8_t interrupt[6];

    int outer_level;
} Sc64GpioState;

/* GPIO registers */
#define REG_SET        0x00
#define REG_PIN        0x00
#define REG_CLR        0x01
#define REG_LATCH      0x01
#define REG_DIR        0x02
#define REG_FUNC       0x03
#define REG_INT        0x04
#define REG_LDIM       0x05

#define SC64_GPIO_IDX_BY_IRQ(n)     ((n) / 2)
#define SC64_GPIO_IRQ_OFFT(n)       ((n) % 2)
#define SC64_GPIO_IRQ_EN_OFFT(n)    (2 + ((n) % 2))
#define SC64_GPIO_IRQ_MODE_OFFT(n)  (4 + 2 * ((n) % 2))

static Sc64GpioIntNum sc64_gpio_int_num(int port, int line)
{
    static const Sc64GpioIntNum num_by_port[2][8] = {
        {
            SC64_GPIO_IRQ_NONE, SC64_GPIO_IRQ_NONE,
            SC64_GPIO_IRQ_A2,   SC64_GPIO_IRQ_A3,
            SC64_GPIO_IRQ_INT2, SC64_GPIO_IRQ_INT3,
            SC64_GPIO_IRQ_INT4, SC64_GPIO_IRQ_INT5
        },
        {
            SC64_GPIO_IRQ_INT7, SC64_GPIO_IRQ_NONE,
            SC64_GPIO_IRQ_B2,   SC64_GPIO_IRQ_B3,
            SC64_GPIO_IRQ_NONE, SC64_GPIO_IRQ_NONE,
            SC64_GPIO_IRQ_INT6, SC64_GPIO_IRQ_NONE,
        }
    };

    if (port >= 0 && port <= 1 && line >= 0 && line <= 8) {
        return num_by_port[port][line];
    } else {
        return SC64_GPIO_IRQ_NONE;
    }
}

static uint64_t sc64_gpio_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64GpioState *s = (Sc64GpioState *) opaque;
    int port = addr / 8;

    switch (addr & 0x7) {
    case REG_PIN:
        return s->in[port] & s->dir[port];
    case REG_LATCH:
        return s->out[port];
    case REG_DIR:
        return s->dir[port];
    case REG_FUNC:
        return s->func[port];
    case REG_INT:
        return port < 2 ? s->interrupt[port] : 0;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-gpio: read access to unknown register 0x"
                      TARGET_FMT_plx, addr);
    }

    return 0;
}

static int sc64_gpio_irq_mode(Sc64GpioState *s, Sc64GpioIntNum num)
{
    int idx = SC64_GPIO_IDX_BY_IRQ(num);
    int offt = SC64_GPIO_IRQ_MODE_OFFT(num);

    if (num == SC64_GPIO_IRQ_NONE) {
        return -1;
    }

    return (s->interrupt[idx] >> offt) & 0x3;
}

static int sc64_gpio_irq_enabled(Sc64GpioState *s, Sc64GpioIntNum num)
{
    int idx = SC64_GPIO_IDX_BY_IRQ(num);
    int offt = SC64_GPIO_IRQ_EN_OFFT(num);

    if (num == SC64_GPIO_IRQ_NONE) {
        return -1;
    }

    return (s->interrupt[idx] >> offt) & 1;
}

static void sc64_gpio_upd_irq(Sc64GpioState *s, Sc64GpioIntNum num,
        int old_level, int new_level)
{
    int irq_level = 0;
    int irq_mode;
    int idx = SC64_GPIO_IDX_BY_IRQ(num);
    int irq_offt = SC64_GPIO_IRQ_OFFT(num);

    if (num == SC64_GPIO_IRQ_NONE) {
        return;
    }

    if (!sc64_gpio_irq_enabled(s, num)) {
        qemu_irq_lower(s->irq[num]);
        s->interrupt[idx] &= ~(1 << irq_offt);
        return;
    }

    irq_mode = sc64_gpio_irq_mode(s, num);

    switch (irq_mode) {
    case SC64_GPIO_HIGH:
        if (new_level == 1) {
            irq_level = 1;
        }
        break;
    case SC64_GPIO_LOW:
        if (new_level == 0) {
            irq_level = 1;
        }
        break;
    case SC64_GPIO_RISING:
        if (old_level == 0 && new_level == 1) {
            irq_level = 1;
        }
        break;
    case SC64_GPIO_FALLING:
        if (old_level == 1 && new_level == 0) {
            irq_level = 1;
        }
        break;
    }

    if (irq_level == 1) {
        s->interrupt[idx] |= 1 << irq_offt;
    } else {
        s->interrupt[idx] &= ~(1 << irq_offt);
    }

    qemu_set_irq(s->irq[num], irq_level);
}

static void sc64_gpio_upd_irq_by_line(Sc64GpioState *s, int port, int line)
{
    int level;
    Sc64GpioIntNum num;

    if (s->func[port] & (1 << line)) {
        /* Pin is configured as input with special function.
         * In current implementation we only support interrupts */
        level = s->in[port] & (1 << line) ? 1 : 0;
        num = sc64_gpio_int_num(port, line);
        sc64_gpio_upd_irq(s, num, level, level);
    } else {
        if (s->dir[port] & (1 << line)) {
            /* Pin is configured as output */
            level = s->out[port] & (1 << line) ? 1 : 0;
            qemu_set_irq(s->handler[port * 8 + line], level);
        }
    }
}

static void sc64_gpio_upd_irq_by_port(Sc64GpioState *s, int port)
{
    int line;
    for (line = 0; line < 8; line++) {
        sc64_gpio_upd_irq_by_line(s, port, line);
    }
}

static void sc64_gpio_write(void *opaque, hwaddr addr,
                        uint64_t value, unsigned size)
{
    Sc64GpioState *s = (Sc64GpioState *) opaque;
    int port = addr / 8;

    value &= 0xff;

    switch (addr & 0x7) {
    case REG_SET:
        s->out[port] |= value;
        break;
    case REG_CLR:
        s->out[port] &= ~value;
        break;
    case REG_LDIM:
        s->out[port] = value;
        break;
    case REG_DIR:
        s->dir[port] |= value;
        break;
    case REG_FUNC:
        s->func[port] = value;
        break;
    case REG_INT:
        if (port > 1) {
            /* Ports C and D have no interrupt registers */
            break;
        }

        if (value & 0x3) {
            s->interrupt[port] &= ~(value & 0x3);
        }
        s->interrupt[port] = (s->interrupt[port] & 0x3) | (value & ~0x3);

        /* Ports A and B has interrupts related to lines 2 and 3 */
        sc64_gpio_upd_irq_by_line(s, port, 2);
        sc64_gpio_upd_irq_by_line(s, port, 3);
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-gpio: read access to unknown register 0x"
                      TARGET_FMT_plx, addr);
    }

    sc64_gpio_upd_irq_by_port(s, port);
}

static const MemoryRegionOps sc64_gpio_ops = {
    .read = sc64_gpio_read,
    .write = sc64_gpio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static uint64_t sc64_outer_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64GpioState *s = (Sc64GpioState *) opaque;
    int port = addr + 2;

    return s->interrupt[port];
}

static void sc64_outer_write(void *opaque, hwaddr addr,
        uint64_t value, unsigned size)
{
    Sc64GpioState *s = (Sc64GpioState *) opaque;
    int lvl;
    int port = addr + 2;

    if (value & 0x3) {
        s->interrupt[port] &= ~(value & 0x3);
    }

    s->interrupt[port] = (s->interrupt[port] & 0x3) | (value & ~0x3);

    switch (addr) {
    case 0:
        lvl = s->outer_level & 0x1;
        sc64_gpio_upd_irq(s, SC64_GPIO_IRQ_INT0, lvl, lvl);
        lvl = (s->outer_level >> 1) & 0x1;
        sc64_gpio_upd_irq(s, SC64_GPIO_IRQ_INT1, lvl, lvl);
        break;
    case 1:
        sc64_gpio_upd_irq_by_line(s, 0, 4); /* INT2 */
        sc64_gpio_upd_irq_by_line(s, 0, 5); /* INT3 */
        break;
    case 2:
        sc64_gpio_upd_irq_by_line(s, 0, 6); /* INT4 */
        sc64_gpio_upd_irq_by_line(s, 0, 7); /* INT5 */
        break;
    case 3:
        sc64_gpio_upd_irq_by_line(s, 1, 6); /* INT6 */
        sc64_gpio_upd_irq_by_line(s, 1, 0); /* INT7 */
        break;
    }
}

static const MemoryRegionOps sc64_outer_ops = {
    .read = sc64_outer_read,
    .write = sc64_outer_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void sc64_gpio_set(void *opaque, int line, int level)
{
    Sc64GpioState *s = (Sc64GpioState *) opaque;
    int old_level;
    int port = line / 8;
    Sc64GpioIntNum num;

    level = !!level; /* Make sure it's 0 or 1 */

    if (line >= SC64_GPIO_LINE_NUM) {
        /* INT0 and INT1 are not attached to any GPIO lines,
         * so we handle it manually */
        line %= 2;
        Sc64GpioIntNum num = line ? SC64_GPIO_IRQ_INT1 : SC64_GPIO_IRQ_INT0;
        sc64_gpio_upd_irq(s, num, (s->outer_level >> line) & 1, level);

        s->outer_level &= ~(1 << line);
        s->outer_level |= (level << line);

        return;
    }

    line %= 8;
    old_level =  (s->in[port] >> line) & 1;
    old_level &= (s->dir[port] >> line) & 1;
    if (level) {
        s->in[port] |= (1 << line);
    } else {
        s->in[port] &= ~(1 << line);
    }

    num = sc64_gpio_int_num(port, line);
    sc64_gpio_upd_irq(s, num, old_level, level);
}

DeviceState *sc64_gpio_register(hwaddr gpio_base, hwaddr outer_base,
        qemu_irq irq_a2, qemu_irq irq_a3,
        qemu_irq irq_b2, qemu_irq irq_b3,
        qemu_irq irq_outer0, qemu_irq irq_outer1)
{
    DeviceState *dev;
    Sc64GpioState *s;
    SysBusDevice *sbd;

    dev = qdev_create(NULL, TYPE_SC64_GPIO);
    qdev_init_nofail(dev);

    s = SC64_GPIO(dev);
    s->irq[0] = irq_a2;
    s->irq[1] = irq_a3;
    s->irq[2] = irq_b2;
    s->irq[3] = irq_b3;
    s->irq[4] = s->irq[6] = s->irq[8] = s->irq[10] = irq_outer0;
    s->irq[5] = s->irq[7] = s->irq[9] = s->irq[11] = irq_outer1;

    sbd = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(sbd, 0, gpio_base);
    sysbus_mmio_map(sbd, 1, outer_base);

    return dev;
}

static void sc64_set_gpio(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    Error *local_err = NULL;
    int64_t level;
    int gpio = (uint64_t)opaque;

    visit_type_int(v, name, &level, &local_err);

    qemu_set_irq(qdev_get_gpio_in(DEVICE(obj), gpio), level);
}

static const VMStateDescription vmstate_sc64_gpio_regs = {
    .name = "sc64-gpio",
    .version_id = 1,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8_ARRAY(dir, Sc64GpioState, SC64_GPIO_PORT_NUM),
        VMSTATE_UINT8_ARRAY(in, Sc64GpioState, SC64_GPIO_PORT_NUM),
        VMSTATE_UINT8_ARRAY(out, Sc64GpioState, SC64_GPIO_PORT_NUM),
        VMSTATE_UINT8_ARRAY(func, Sc64GpioState, SC64_GPIO_PORT_NUM),
        VMSTATE_UINT8_ARRAY(interrupt, Sc64GpioState, 6),
        VMSTATE_END_OF_LIST(),
    },
};

static void sc64_gpio_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    Sc64GpioState *s = SC64_GPIO(dev);
    char buf[16];
    int i;

    for (i = 0; i < SC64_GPIO_LINE_NUM; i++) {
        snprintf(buf, 16, "%i:%i", (i & ~7) >> 3, i & 7);
        object_property_add(OBJECT(sbd), buf, "int",
                            NULL,
                            sc64_set_gpio,
                            NULL, (void *)(uint64_t)i, NULL);
    }

    for (i = 0; i < SC64_OUTER_IRQ_NUM; i++) {
        snprintf(buf, 16, "INT%d", i);

        object_property_add(OBJECT(sbd), buf, "int",
                            NULL, sc64_set_gpio, NULL,
                            (void *)(uint64_t)(i + SC64_GPIO_LINE_NUM), NULL);
    }

    qdev_init_gpio_in(dev, sc64_gpio_set,
            SC64_GPIO_LINE_NUM + SC64_OUTER_IRQ_NUM);
    qdev_init_gpio_out(dev, s->handler, SC64_GPIO_LINE_NUM);

    memory_region_init_io(&s->iomem_gpio, OBJECT(s), &sc64_gpio_ops,
                            s, "sc64-gpio", 8 * SC64_GPIO_PORT_NUM);

    memory_region_init_io(&s->iomem_outer, OBJECT(s), &sc64_outer_ops,
                            s, "sc64-outer-intc", 4);

    for (i = 0; i < ARRAY_SIZE(s->irq); i++) {
        sysbus_init_irq(sbd, &s->irq[i]);
    }

    sysbus_init_mmio(sbd, &s->iomem_gpio);
    sysbus_init_mmio(sbd, &s->iomem_outer);
}

static void sc64_gpio_reset(DeviceState *dev)
{
    Sc64GpioState *s = SC64_GPIO(dev);
    int i;

    /* Default values */
    for (i = 0; i < SC64_GPIO_PORT_NUM; i++) {
        s->dir[i]  = 0xff;
        s->in[i]   = 0x02; /* pmon2000 techno mode */
        s->out[i]  = 0x00;
        s->func[i] = 0x00;
    }

    for (i = 0; i < ARRAY_SIZE(s->interrupt); i++) {
        s->interrupt[i] = 0;
    }
}

static void sc64_gpio_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "SC64 GPIO block";
    dc->vmsd = &vmstate_sc64_gpio_regs;
    dc->realize = sc64_gpio_realize;
    dc->reset = sc64_gpio_reset;
}

static const TypeInfo sc64_gpio_sysbus_info = {
    .name          = TYPE_SC64_GPIO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64GpioState),
    .class_init    = sc64_gpio_sysbus_class_init,
};

static void sc64_gpio_register_types(void)
{
    type_register_static(&sc64_gpio_sysbus_info);
}

type_init(sc64_gpio_register_types)
