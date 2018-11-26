/*
 * Copyright (C) 2017 Anton Bondarev <anton.bondarev2310@gmail.com>
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
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "qemu/typedefs.h"
#include "hw/i2c/i2c.h"

#define TYPE_SC64_I2C "sc64_i2c"
#define SC64_I2C(obj) \
    OBJECT_CHECK(Sc64I2CState, (obj), TYPE_SC64_I2C)

typedef struct Sc64I2CState {
    SysBusDevice parent_obj;

    I2CBus *bus;
    char *busname;

    MemoryRegion iomem;
    uint8_t width;
    qemu_irq irq;

    uint8_t addr;
    uint8_t xaddr;
    uint8_t data;
    uint8_t cntr;
    uint8_t stat;
    uint8_t srst;

    uint32_t state;
} Sc64I2CState;

/* I2C registers  */
#define SC64_ADDR     0x0
#define SC64_DATA     0x1
#define SC64_CNTR     0x2
#define SC64_STAT     0x3 /* Read only */
#define SC64_CCR      0x3 /* Write only */
#define SC64_XADDR    0x4
#define SC64_SRST     0x7

/* SC64_CNTR     0x2 */
# define SC64_CNTR_IEN    (1 << 7)
# define SC64_CNTR_ENAB   (1 << 6)
# define SC64_CNTR_STA    (1 << 5)
# define SC64_CNTR_STP    (1 << 4)
# define SC64_CNTR_IFLG   (1 << 3)
# define SC64_CNTR_AAK    (1 << 2)

/* SC64_STAT     0x3 */
# define SC64_STAT_START            0x08
# define SC64_STAT_DOUBLE_START     0x10
# define SC64_STAT_WR_ADDR_ACK      0x18
# define SC64_STAT_WR_ADDR_NOT_ACK  0x20
# define SC64_STAT_WR_DATA_ACK      0x28
# define SC64_STAT_WR_DATA_NOT_ACK  0x30
# define SC64_STAT_ARB_BREAK        0x38
# define SC64_STAT_RD_ADDR_ACK      0x40
# define SC64_STAT_RD_ADDR_NOT_ACK  0x48
# define SC64_STAT_RD_DATA_ACK      0x50
# define SC64_STAT_RD_DATA_NOT_ACK  0x58
# define SC64_STAT_ACK_WR_ADDR      0x60
# define SC64_STAT_SLAVE_ARB_BREAK  0x68
# define SC64_STAT_ACK_BRODCAST     0x70
# define SC64_STAT_MASTER_ARB_BREAK 0x78
# define SC64_STAT_BYTE_ACK         0x80
# define SC64_STAT_BYTE_NOT_ACK     0x88
# define SC64_STAT_BYTE_BR_ACK      0x90
# define SC64_STAT_BYTE_BR_NOT_ACK  0x98
# define SC64_STAT_SLAVE_GET_STOP   0xA0
# define SC64_STAT_GET_ADDR_ACK     0xA8
# define SC64_STAT_ARB_BREAK_ACK    0x90
# define SC64_STAT_SND_BYTE_ACK     0x98
# define SC64_STAT_SND_BYTE_NOT_ACK 0xC0
# define SC64_STAT_SND_LAST_BYTE    0xC8
# define SC64_STAT_WR_ADDR2_ACK     0xD0
# define SC64_STAT_WR_ADDR2_NOT_ACK 0xD8
# define SC64_STAT_WAIT             0xF8

/* I2C states */
#define SC64_STATE_MASTER      0x1
#define SC64_STATE_SEND_ADDR_R 0x2
#define SC64_STATE_SEND_ADDR_W 0x4
#define SC64_STATE_SEND_ADDR   (SC64_STATE_SEND_ADDR_R | SC64_STATE_SEND_ADDR_W)
#define SC64_STATE_SEND_BYTE   0x8
#define SC64_STATE_RECV_BYTE   0x10

#define SC64_CNTR_MASK         (~SC64_CNTR_STA & ~SC64_CNTR_STP)

static inline bool sc64_i2c_is_enabled(Sc64I2CState *s)
{
    return s->cntr & SC64_CNTR_ENAB;
}

static inline bool sc64_i2c_is_master(Sc64I2CState *s)
{
    return s->state & SC64_STATE_MASTER;
}

static void sc64_i2c_update_stat(Sc64I2CState *s, uint8_t new_stat)
{
    s->stat = new_stat;

    if (sc64_i2c_is_enabled(s)) {
        s->cntr |= SC64_CNTR_IFLG;
        if (s->cntr & SC64_CNTR_IEN) {
            qemu_irq_raise(s->irq);
        }
    }
}

static uint64_t sc64_i2c_read(void *opaque, hwaddr offset, unsigned size)
{
    uint8_t value;
    Sc64I2CState *s = SC64_I2C(opaque);

    offset /= s->width;

    switch (offset) {
    case SC64_ADDR:
        value = s->addr;
        break;
    case SC64_DATA:
        value = s->data;
        break;
    case SC64_CNTR:
        value = s->cntr;
        break;
    case SC64_STAT:
        value = s->stat;
        break;
    case SC64_XADDR:
        value = s->xaddr;
        break;
    default:
        value = 0;
        break;
    }
    return value & 0xff;
}

static void sc64_i2c_write(void *opaque, hwaddr offset, uint64_t value,
        unsigned size)
{
    Sc64I2CState *s = SC64_I2C(opaque);

    offset /= s->width;
    value &= 0xff;

    switch (offset) {
    case SC64_ADDR:
        s->addr = value;
        break;
    case SC64_DATA:
        if (!sc64_i2c_is_enabled(s)) {
            break;
        }
        s->data = value;
        if (sc64_i2c_is_master(s)) {
            if (s->stat == SC64_STAT_START
                    || s->stat == SC64_STAT_DOUBLE_START) {
                if (s->data & 1) {
                    s->state |= SC64_STATE_SEND_ADDR_R;
                } else {
                    s->state |= SC64_STATE_SEND_ADDR_W;
                }
            } else {
                s->state |= SC64_STATE_SEND_BYTE;
            }
        }
        break;
    case SC64_CNTR:
        s->cntr = value & SC64_CNTR_MASK;

        if (sc64_i2c_is_master(s)) {
            if (value & SC64_CNTR_STA) {
                sc64_i2c_update_stat(s, SC64_STAT_DOUBLE_START);
            } else if (value & SC64_CNTR_STP) {
                i2c_end_transfer(s->bus);
                s->state = 0;
                sc64_i2c_update_stat(s, SC64_STAT_WAIT);
            }

            if (!(value & SC64_CNTR_IFLG)) {
                qemu_set_irq(s->irq, 0);

                if (s->state & SC64_STATE_SEND_ADDR) {
                    int addr = extract32(s->data, 1, 7);

                    if (i2c_start_transfer(s->bus, addr,
                                    extract32(s->data, 0, 1))) {
                        /* error */
                        sc64_i2c_update_stat(s, SC64_STAT_WR_ADDR_NOT_ACK);
                    } else {
                        /* Ignore the slave if broadcast is not allowed */
                        if (addr == 0x0 && !(s->addr & 1)) {
                            sc64_i2c_update_stat(s, SC64_STAT_WR_ADDR_NOT_ACK);
                        } else if (s->state & SC64_STATE_SEND_ADDR_W) {
                            sc64_i2c_update_stat(s, SC64_STAT_WR_ADDR_ACK);
                        } else {
                            s->state |= SC64_STATE_RECV_BYTE;
                            sc64_i2c_update_stat(s, SC64_STAT_RD_ADDR_ACK);
                        }
                        s->state &= ~SC64_STATE_SEND_ADDR;
                    }
                } else if (s->state & SC64_STATE_SEND_BYTE) {
                    i2c_send(s->bus, s->data);
                    s->state &= ~SC64_STATE_SEND_BYTE;
                    sc64_i2c_update_stat(s, SC64_STAT_WR_DATA_ACK);
                } else if (s->state & SC64_STATE_RECV_BYTE) {
                    int ret = i2c_recv(s->bus);

                    if (ret >= 0) {
                        s->data = ret;
                        sc64_i2c_update_stat(s, value & SC64_CNTR_AAK ?
                            SC64_STAT_RD_DATA_ACK : SC64_STAT_RD_DATA_NOT_ACK);
                    }
                }
            }
        } else {
            if (value & SC64_CNTR_STA) {
                s->state = SC64_STATE_MASTER;
                sc64_i2c_update_stat(s, SC64_STAT_START);
            }
        }
        break;
    case SC64_STAT:
        break;
    case SC64_XADDR:
        s->xaddr = value;
        break;
    case SC64_SRST:
        device_reset(DEVICE(s));
        break;
    default:
        break;
    }
}

static const MemoryRegionOps sc64_i2c_ops = {
    .read = sc64_i2c_read,
    .write = sc64_i2c_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const VMStateDescription sc64_i2c_vmstate = {
    .name = TYPE_SC64_I2C,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(addr, Sc64I2CState),
        VMSTATE_UINT8(xaddr, Sc64I2CState),
        VMSTATE_UINT8(data, Sc64I2CState),
        VMSTATE_UINT8(cntr, Sc64I2CState),
        VMSTATE_UINT8(stat, Sc64I2CState),
        VMSTATE_END_OF_LIST()
    }
};

static void sc64_i2c_reset(DeviceState *dev)
{
    Sc64I2CState *s = SC64_I2C(dev);

    if (s->stat != SC64_STAT_WAIT) {
        i2c_end_transfer(s->bus);
    }
    s->addr = 0;
    s->xaddr = 0;
    s->data = 0;
    s->cntr = 0;
    s->stat = SC64_STAT_WAIT;
    s->state = 0;
}

static void sc64_i2c_realize(DeviceState *dev, Error **errp)
{
    Sc64I2CState *s = SC64_I2C(dev);

    if (s->width == 0 || !((s->width & (s->width - 1)) == 0)) {
        s->width = 1;
    }

    memory_region_init_io(&s->iomem, OBJECT(s), &sc64_i2c_ops, s,
        TYPE_SC64_I2C, 0x8 * s->width);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
    s->bus = i2c_init_bus(dev, s->busname ? s->busname : "i2c");
}

static Property sc64_i2c_props[] = {
    DEFINE_PROP_STRING("busname", Sc64I2CState, busname),
    DEFINE_PROP_UINT8("width", Sc64I2CState, width, 1),
    DEFINE_PROP_END_OF_LIST(),
};

static void sc64_i2c_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &sc64_i2c_vmstate;
    dc->reset = sc64_i2c_reset;
    dc->realize = sc64_i2c_realize;
    dc->props = sc64_i2c_props;
    dc->desc = "SC64 I2C Controller";
}

static const TypeInfo sc64_i2c_info = {
    .name          = TYPE_SC64_I2C,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64I2CState),
    .class_init = sc64_i2c_class_init,
};

static void sc64_i2c_register_types(void)
{
    type_register_static(&sc64_i2c_info);
}

type_init(sc64_i2c_register_types)
