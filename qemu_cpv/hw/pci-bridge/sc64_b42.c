/*
 * Copyright (C) 2018 Aleksey Kuleshov <rndfax@yandex.ru>
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
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/pci/msi.h"
#include "hw/pci/pcie_port.h"
#include "hw/pci/sc64_b42_nt_port.h"
#include "hw/register.h"
#include "hw/registerfields.h"
#include "hw/ssi/ssi.h"
#include "hw/i2c/i2c.h"
#include "hw/sysbus.h"
#include "qapi/visitor.h"

#define TYPE_SC64_B42   "sc64-b42"
#define TYPE_SC64_B42_BUS       "sc64-b42-bus"
#define TYPE_SC64_B42_SPI       "sc64-b42-spi"
#define TYPE_SC64_B42_I2CS      "sc64-b42-i2cs"
#define TYPE_SC64_B42_GPIO      "sc64-b42-gpio"

#define REG_ADDR(reg)   A_ ## reg
#define FIELD_MASK(reg, field)  R_ ## reg ## _ ## field ## _MASK

typedef struct {
    /*< private >*/
    PCIEPort parent_obj;
    /*< public >*/

    MemoryRegion bar;

    uint32_t regs[0x1000 / 4];
    RegisterInfo regs_info[0x1000 / 4];

    I2CSlave *i2cs;
    uint8_t i2cs_addr;
    char *i2cs_bus;

    char *i2cm_busname;
    char *spi_busname;

    AddressSpace as;
} SC64B42;

#define SC64_B42(obj)   OBJECT_CHECK(SC64B42, (obj), TYPE_SC64_B42)

REG32(I2CS_ADDR, 0xc20)
    FIELD(I2CS_ADDR, ADDR, 0, 3)

#define TYPE_SC64_B42_DOWNSTREAM    "sc64-b42-downstream"

static void sc64_b42_i2cs_update_addr(SC64B42 *s)
{
    if (s->i2cs) {
        uint32_t addr = ARRAY_FIELD_EX32(s->regs, I2CS_ADDR, ADDR);
        i2c_set_slave_address(s->i2cs, 0x68 | addr);
    }
}

static void sc64_b42_i2cs_addr_post_write(RegisterInfo *reg, uint64_t val)
{
    SC64B42 *s = SC64_B42(reg->opaque);

    ARRAY_FIELD_DP32(s->regs, I2CS_ADDR, ADDR, val);
    sc64_b42_i2cs_update_addr(s);
}

static PCIDevice *sc64_b42_virtual_port(SC64B42 *s)
{
    PCIBus *b = pci_bridge_get_sec_bus(PCI_BRIDGE(s));
    int i;

    for (i = 0; i < 5; i++) {
        PCIDevice *d = b->devices[PCI_DEVFN(i, 0)];
        PCIBus *b = pci_bridge_get_sec_bus(PCI_BRIDGE(d));
        Object *obj = object_dynamic_cast(OBJECT(b->devices[0]),
                                                        TYPE_SC64_B42_NT_PORT);
        if (obj != NULL) {
            return PCI_DEVICE(obj);
        }
    }

    return NULL;
}

static uint64_t sc64_b42_reg_rw(SC64B42 *s, hwaddr addr, uint64_t value,
                                                                    bool write)
{
    PCIBus *b = pci_bridge_get_sec_bus(PCI_BRIDGE(s));
    int idx = addr >> 12;
    hwaddr offset = addr & ((1 << 12) - 1);

    if (idx < 6) {
        PCIDevice *d;
        if (idx == 0) {
            d = PCI_DEVICE(s);
        } else {
            d = b->devices[PCI_DEVFN(idx - 1, 0)];
        }
        if (write) {
            d->config_write(d, offset, value, 4);
            return 0;
        } else {
            return d->config_read(d, offset, 4);
        }
    } else if (idx == 6) {
        PCIDevice *d = sc64_b42_virtual_port(s);
        if (d == NULL) {
            /* FIXME?
             * In real HW all registers are accessible, since Link and Virtual
             * ports are part of the switch.
             */
            return 0;
        }
        if (write) {
            d->config_write(d, offset, value, 4);
            return 0;
        } else {
            return d->config_read(d, offset, 4);
        }
    } else if (idx == 7) {
        PCIDevice *d = sc64_b42_virtual_port(s);
        if (d == NULL) {
            /* FIXME?
             * In real HW all registers are accessible, since Link and Virtual
             * ports are part of the switch.
             */
            return 0;
        }
        return sc64_nt_port_link(SC64_B42_NT_PORT(d), offset, value, write);
    }

    return 0;
}

static uint64_t sc64_b42_read(void *opaque, hwaddr addr, unsigned size)
{
    assert(size == 4);
    return sc64_b42_reg_rw(opaque, addr, 0, false);
}

static void sc64_b42_write(void *opaque, hwaddr addr, uint64_t value,
                                                                unsigned size)
{
    assert(size == 4);
    sc64_b42_reg_rw(opaque, addr, value, true);
}

static const MemoryRegionOps sc64_b42_ops = {
    .read = sc64_b42_read,
    .write = sc64_b42_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const RegisterAccessInfo sc64_b42_regs_info[] = {
    {
        .name = "I2CS_ADDR",
        .addr = REG_ADDR(I2CS_ADDR),
        .reset = 0,
        .post_write = sc64_b42_i2cs_addr_post_write,
    },
};

static const MemoryRegionOps sc64_b42_reg_ops = {
    .read = register_read_memory,
    .write = register_write_memory,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    }
};

static BusState *sc64_b42_find_bus(BusState *bus, const char *id)
{
    BusChild *kid;
    BusState *ret;
    BusState *child;

    if (id == NULL) {
        return NULL;
    }

    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        DeviceState *dev = kid->child;

        ret = qdev_get_child_bus(dev, id);
        if (ret) {
            return ret;
        }

        QLIST_FOREACH(child, &dev->child_bus, sibling) {
            ret = sc64_b42_find_bus(child, id);
            if (ret) {
                return ret;
            }
        }
    }
    return NULL;
}

static void sc64_b42_i2cs_init(SC64B42 *s)
{
    DeviceState *i2cs;
    I2CBus *bus;

    bus = (I2CBus *)sc64_b42_find_bus(sysbus_get_default(), s->i2cs_bus);
    if (bus == NULL) {
        return;
    }

    i2cs = i2c_create_slave(bus, TYPE_SC64_B42_I2CS, 0);
    qdev_prop_set_ptr(i2cs, "address_space", &s->as);
    s->i2cs = I2C_SLAVE(i2cs);
}

static void sc64_b42_int_handler(PCIDevice *d, int level, int group)
{
    if (msi_enabled(d)) {
        if (level) {
            msi_notify(d, group);
        }
    } else {
        pci_set_irq(d, level);
    }
}

static void sc64_b42_i2cm_int_handler(void *opaque, int n, int level)
{
    SC64B42 *s = opaque;

    sc64_b42_int_handler(PCI_DEVICE(s), level, 0);
}

static void sc64_b42_i2cm_init(SC64B42 *s, BusState *parent, MemoryRegion *mr)
{
    DeviceState *dev;
    const char *busname;

    busname = s->i2cm_busname ? s->i2cm_busname : "sc64-b42-i2cm-bus";

    dev = qdev_create(parent, "sc64_i2c");
    qdev_prop_set_string(dev, "busname", busname);
    qdev_prop_set_uint8(dev, "width", 4);
    qdev_init_nofail(dev);

    memory_region_add_subregion(mr, 0xca0,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0));
    qdev_connect_gpio_out_named(dev, SYSBUS_DEVICE_GPIO_IRQ, 0,
                            qemu_allocate_irq(sc64_b42_i2cm_int_handler, s, 0));
}

static void sc64_b42_gpio_int_handler(void *opaque, int n, int level)
{
    SC64B42 *s = opaque;

    sc64_b42_int_handler(PCI_DEVICE(s), level, 1);
}

static void sc64_b42_gpio_init(SC64B42 *s, BusState *parent, MemoryRegion *mr)
{
    DeviceState *dev;

    dev = qdev_create(parent, TYPE_SC64_B42_GPIO);
    qdev_init_nofail(dev);

    memory_region_add_subregion(mr, 0xc50,
                            sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0));
    qdev_connect_gpio_out_named(dev, SYSBUS_DEVICE_GPIO_IRQ, 0,
                            qemu_allocate_irq(sc64_b42_gpio_int_handler, s, 0));
}

static void sc64_b42_spi_init(SC64B42 *s, BusState *parent, MemoryRegion *mr)
{
    DeviceState *dev;
    const char *busname = s->spi_busname ? s->spi_busname : "sc64-b42-spi-bus";

    dev = qdev_create(parent, TYPE_SC64_B42_SPI);
    qdev_prop_set_ptr(dev, "address_space", &s->as);
    qdev_prop_set_string(dev, "busname", busname);
    qdev_init_nofail(dev);

    memory_region_add_subregion(mr, 0xc10,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0));
}

static void sc64_b42_realize(PCIDevice *d, Error **errp)
{
    PCIEPort *p = PCIE_PORT(d);
    PCIBridge *b = PCI_BRIDGE(d);
    SC64B42 *s = SC64_B42(d);
    RegisterInfoArray *reg_array;
    MemoryRegion *mr;
    BusState *device_bus;
    static int chassis;
    int i;

    reg_array = register_init_block32(DEVICE(d), sc64_b42_regs_info,
                              ARRAY_SIZE(sc64_b42_regs_info),
                              s->regs_info, s->regs,
                              &sc64_b42_reg_ops,
                              0,
                              sizeof(s->regs));

    pci_bridge_initfn(d, TYPE_PCIE_BUS);
    pcie_port_init_reg(d);

    memory_region_init_io(&s->bar, OBJECT(d), &sc64_b42_ops, s,
                                                "sc64-b42-regs", 0x10000);
    memory_region_add_subregion(&s->bar,
                                0x9000,
                                &reg_array->mem);

    address_space_init(&s->as, &s->bar, "sc64-b42-regs-as");

    /* Prevent s->bar changing its address by using alias */
    mr = g_new(MemoryRegion, 1);
    memory_region_init_alias(mr, OBJECT(d), "sc64-b42-bar0", &s->bar, 0,
                                                                    0x10000);
    pci_register_bar(d, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, mr);

    assert(pcie_cap_init(d, 0x68, PCI_EXP_TYPE_UPSTREAM, p->port, NULL) >= 0);
    pcie_cap_flr_init(d);
    pcie_cap_deverr_init(d);

    assert(msi_init(d, 0x48, 2, true, true, NULL) == 0);

    for (i = 0; i < 5; i++) {
        PCIDevice *sd = pci_create(&b->sec_bus,
                                    PCI_DEVFN(i, 0), TYPE_SC64_B42_DOWNSTREAM);
        qdev_prop_set_uint8(&sd->qdev, "chassis", chassis);
        qdev_prop_set_uint8(&sd->qdev, "slot", i);
        qdev_init_nofail(&sd->qdev);
    }

    chassis++;

    device_bus = qbus_create(TYPE_SC64_B42_BUS, DEVICE(s), TYPE_SC64_B42_BUS);

    sc64_b42_i2cs_init(s);
    sc64_b42_i2cm_init(s, device_bus, &reg_array->mem);
    sc64_b42_gpio_init(s, device_bus, &reg_array->mem);
    sc64_b42_spi_init(s, device_bus, &reg_array->mem);
}

static void sc64_b42_reset(DeviceState *qdev)
{
    PCIDevice *d = PCI_DEVICE(qdev);
    SC64B42 *s = SC64_B42(d);

    pci_bridge_reset(qdev);
    pcie_cap_deverr_reset(d);

    ARRAY_FIELD_DP32(s->regs, I2CS_ADDR, ADDR, s->i2cs_addr);
    sc64_b42_i2cs_update_addr(s);
}

static void sc64_b42_write_config(PCIDevice *d, uint32_t address,
                                                        uint32_t val, int len)
{
    pci_bridge_write_config(d, address, val, len);
    pcie_cap_flr_write_config(d, address, val, len);
    pcie_cap_slot_write_config(d, address, val, len);
}

static Property sc64_b42_props[] = {
    DEFINE_PROP_UINT8("i2cs_addr", SC64B42, i2cs_addr, 0),
    DEFINE_PROP_STRING("i2cs_bus", SC64B42, i2cs_bus),
    DEFINE_PROP_STRING("i2cm_busname", SC64B42, i2cm_busname),
    DEFINE_PROP_STRING("spi_busname", SC64B42, spi_busname),
    DEFINE_PROP_END_OF_LIST(),
};

static void sc64_b42_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = sc64_b42_reset;
    dc->props = sc64_b42_props;
    k->config_write = sc64_b42_write_config;
    k->realize = sc64_b42_realize;
    k->vendor_id = 0x191e;
    k->device_id = 0x5a52;
    k->revision = 0;
    k->is_bridge = 1,
    k->is_express = 1;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
}

static const TypeInfo sc64_b42 = {
    .name          = TYPE_SC64_B42,
    .parent        = TYPE_PCIE_PORT,
    .instance_size = sizeof(SC64B42),
    .class_init    = sc64_b42_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void sc64_b42_d_realize(PCIDevice *d, Error **errp)
{
    PCIEPort *p = PCIE_PORT(d);
    PCIESlot *s = PCIE_SLOT(d);

    pci_bridge_initfn(d, TYPE_PCIE_BUS);
    pcie_port_init_reg(d);

    assert(pcie_cap_init(d, 0x68, PCI_EXP_TYPE_DOWNSTREAM, p->port, NULL) >= 0);
    pcie_cap_flr_init(d);
    pcie_cap_deverr_init(d);
    pcie_cap_slot_init(d, s->slot);

    pcie_chassis_create(s->chassis);
    assert(pcie_chassis_add_slot(s) >= 0);
}

static void sc64_b42_d_reset(DeviceState *qdev)
{
    PCIDevice *d = PCI_DEVICE(qdev);

    pcie_cap_deverr_reset(d);
    pcie_cap_slot_reset(d);
    pci_bridge_reset(qdev);
}

static void sc64_b42_d_write_config(PCIDevice *d, uint32_t address,
                                                        uint32_t val, int len)
{
    pci_bridge_write_config(d, address, val, len);
    pcie_cap_flr_write_config(d, address, val, len);
}

static void sc64_b42_d_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = sc64_b42_d_reset;
    k->config_write = sc64_b42_d_write_config;
    k->realize = sc64_b42_d_realize;
    k->vendor_id = 0x191e;
    k->device_id = 0x5a52;
    k->revision = 0;
    k->is_bridge = 1,
    k->is_express = 1;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
}

static const TypeInfo sc64_b42_d = {
    .name          = TYPE_SC64_B42_DOWNSTREAM,
    .parent        = TYPE_PCIE_SLOT,
    .class_init    = sc64_b42_d_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

typedef struct {
    /*< private >*/
    I2CSlave i2c;
    /*< public >*/
    void *address_space;

    enum {
        CMD,
        ADDRESS_HI,
        ADDRESS_LO,
        DATA_1,
        DATA_2,
        DATA_3,
        DATA_4,
        DONE,
    } state;

    uint8_t state_command;
    uint16_t state_address;

    uint32_t reg;
} SC64B42I2CS;

#define SC64_B42_I2CS(obj) \
                    OBJECT_CHECK(SC64B42I2CS, (obj), TYPE_SC64_B42_I2CS)

static int sc64_b42_i2cs_event(I2CSlave *i2c, enum i2c_event event)
{
    SC64B42I2CS *s = SC64_B42_I2CS(i2c);

    if (event == I2C_FINISH) {
        s->state = CMD;
    }

    return 0;
}

static uint32_t sc64_b42_i2cs_rw(I2CSlave *i2c, uint32_t data, bool write)
{
    SC64B42I2CS *s = SC64_B42_I2CS(i2c);
    address_space_rw(s->address_space, s->state_address & ~3,
                            MEMTXATTRS_UNSPECIFIED, (uint8_t *)&data, 4, write);
    return data;
}

static int sc64_b42_i2cs_rx(I2CSlave *i2c)
{
    SC64B42I2CS *s = SC64_B42_I2CS(i2c);
    uint8_t data;

    if (s->state_command & (1 << 4)) {
        return 0;
    }

    switch (s->state) {
    case DATA_1:
        s->reg = sc64_b42_i2cs_rw(i2c, 0, false);
        /* Fall through */
    case DATA_2 ... DATA_4:
        data = (s->reg >> (24 - 8 * (s->state - DATA_1))) & 0xff;
        s->state++;
        break;
    default:
        assert(0);
    }

    return data;
}

static int sc64_b42_i2cs_tx(I2CSlave *i2c, uint8_t data)
{
    SC64B42I2CS *s = SC64_B42_I2CS(i2c);
    int idx;

    switch (s->state) {
    case CMD:
        s->state_command = data;
        s->state = ADDRESS_HI;
        break;
    case ADDRESS_HI:
        s->state_address = data << 8;
        s->state = ADDRESS_LO;
        break;
    case ADDRESS_LO:
        s->state_address |= data;
        s->state = DATA_1;
        break;
    case DATA_1:
        s->reg = sc64_b42_i2cs_rw(i2c, 0, false);
        /* Fall through */
    case DATA_2 ... DATA_4:
        idx = s->state - DATA_1;
        if (s->state_command & (1 << (3 - idx))) {
            int shift = 24 - 8 * idx;
            s->reg &= ~(0xff << shift);
            s->reg |= data << shift;
        }
        if (s->state == DATA_4) {
            sc64_b42_i2cs_rw(i2c, s->reg, true);
        }
        s->state++;
        break;
    default:
        assert(0);
    }

    return 1;
}

static Property sc64_b42_i2cs_props[] = {
    DEFINE_PROP_PTR("address_space", SC64B42I2CS, address_space),
    DEFINE_PROP_END_OF_LIST(),
};

static void sc64_b42_i2cs_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *k = DEVICE_CLASS(oc);
    I2CSlaveClass *isc = I2C_SLAVE_CLASS(oc);

    isc->event = sc64_b42_i2cs_event;
    isc->recv = sc64_b42_i2cs_rx;
    isc->send = sc64_b42_i2cs_tx;
    k->props = sc64_b42_i2cs_props;
}

static TypeInfo sc64_b42_i2cs_info = {
    .name = TYPE_SC64_B42_I2CS,
    .parent = TYPE_I2C_SLAVE,
    .instance_size = sizeof(SC64B42I2CS),
    .class_init = sc64_b42_i2cs_class_init
};

#define SC64_B42_GPIO_NUM       16

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    uint32_t regs[8];
    RegisterInfo regs_info[8];

    qemu_irq irq;
    qemu_irq handler[SC64_B42_GPIO_NUM];

    bool level[SC64_B42_GPIO_NUM];
} SC64B42GPIO;

#define SC64_B42_GPIO(obj) \
                    OBJECT_CHECK(SC64B42GPIO, (obj), TYPE_SC64_B42_GPIO)

REG32(GPIO_CTRL_90, 0x0)
REG32(GPIO_CTRL_1510, 0x4)
REG32(GPIO_HYSTERESIS, 0x8)
REG32(GPIO_INPUT_DATA, 0xc)
REG32(GPIO_OUTPUT_DATA, 0x10)
REG32(GPIO_INT_POL, 0x14)
REG32(GPIO_INT_STATUS, 0x18)
REG32(GPIO_INT_MASK, 0x1c)

static uint8_t sc64_b42_gpio_settings(SC64B42GPIO *s, int i)
{
    uint32_t reg = R_GPIO_CTRL_90;

    if (i > 9) {
        i -= 10;
        reg = R_GPIO_CTRL_1510;
    }

    return (s->regs[reg] >> (3 * i)) & 7;
}

static bool sc64_b42_gpio_dir_in(SC64B42GPIO *s, int i)
{
    return (sc64_b42_gpio_settings(s, i) >> 2) == 0;
}

static bool sc64_b42_gpio_in_int(SC64B42GPIO *s, int i)
{
    return (sc64_b42_gpio_settings(s, i) & 3) == 0b01;
}

static void sc64_b42_gpio_update_int(SC64B42GPIO *s, int i)
{
    bool polarity = !!((s->regs[R_GPIO_INT_POL] >> i) & 1);

    s->regs[R_GPIO_INT_STATUS] &= ~BIT(i);

    if ((polarity ^ s->level[i]) == 0) {
        s->regs[R_GPIO_INT_STATUS] |= BIT(i);
    }
}

static void sc64_b42_gpio_update_input(SC64B42GPIO *s, int i)
{
    s->regs[R_GPIO_INPUT_DATA] &= ~BIT(i);

    if (s->level[i]) {
        s->regs[R_GPIO_INPUT_DATA] |= BIT(i);
    }
}

static void sc64_b42_gpio_update_output(SC64B42GPIO *s, int i)
{
    int level = (s->regs[R_GPIO_OUTPUT_DATA] >> i) & 1;

    s->regs[R_GPIO_INPUT_DATA] &= ~BIT(i);

    qemu_set_irq(s->handler[i], level);
}

static void sc64_b42_gpio_update(SC64B42GPIO *s)
{
    int i;

    for (i = 0; i < SC64_B42_GPIO_NUM; i++) {
        if (sc64_b42_gpio_dir_in(s, i)) {
            if (sc64_b42_gpio_in_int(s, i)) {
                sc64_b42_gpio_update_int(s, i);
            } else {
                sc64_b42_gpio_update_input(s, i);
            }
        } else {
            sc64_b42_gpio_update_output(s, i);
        }
    }

    for (i = 0; i < SC64_B42_GPIO_NUM; i++) {
        if (!sc64_b42_gpio_dir_in(s, i)) {
            continue;
        }
        if (!sc64_b42_gpio_in_int(s, i)) {
            continue;
        }
        if (s->regs[R_GPIO_INT_MASK] & BIT(i)) {
            continue;
        }
        if (((s->regs[R_GPIO_INT_STATUS] >> i) & 1) == 0) {
            continue;
        }
        qemu_irq_raise(s->irq);
        break;
    }
    if (i == SC64_B42_GPIO_NUM) {
        qemu_irq_lower(s->irq);
    }
}

static void sc64_b42_gpio_post_write(RegisterInfo *reg, uint64_t val)
{
    SC64B42GPIO *s = SC64_B42_GPIO(reg->opaque);
    sc64_b42_gpio_update(s);
}

static const RegisterAccessInfo sc64_b42_gpio_regs_info[] = {
    {
        .name = "GPIO_CTRL_90",
        .addr = REG_ADDR(GPIO_CTRL_90),
        .reset = 0,
        .ro = 0xc0000000,
        .post_write = sc64_b42_gpio_post_write,
    },
    {
        .name = "GPIO_CTRL_1510",
        .addr = REG_ADDR(GPIO_CTRL_1510),
        .reset = 0,
        .ro = 0x0ffc0000,
        .post_write = sc64_b42_gpio_post_write,
    },
    {
        .name = "GPIO_INPUT_DATA",
        .addr = REG_ADDR(GPIO_INPUT_DATA),
        .ro = 0xffffffff,
    },
    {
        .name = "GPIO_OUTPUT_DATA",
        .addr = REG_ADDR(GPIO_OUTPUT_DATA),
        .ro = 0xffff0000,
        .post_write = sc64_b42_gpio_post_write,
    },
    {
        .name = "GPIO_INT_POL",
        .addr = REG_ADDR(GPIO_INT_POL),
        .reset = 0,
        .ro = 0xffff0000,
        .post_write = sc64_b42_gpio_post_write,
    },
    {
        .name = "GPIO_INT_STATUS",
        .addr = REG_ADDR(GPIO_INT_STATUS),
        .reset = 0,
        .ro = 0xffffffff,
        .post_write = sc64_b42_gpio_post_write,
    },
    {
        .name = "GPIO_INT_MASK",
        .addr = REG_ADDR(GPIO_INT_MASK),
        .reset = 0,
        .ro = 0xffff0000,
        .post_write = sc64_b42_gpio_post_write,
    },
};

static const MemoryRegionOps sc64_b42_gpio_ops = {
    .read = register_read_memory,
    .write = register_write_memory,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    }
};

static void sc64_b42_gpio_set(void *opaque, int line, int level)
{
    SC64B42GPIO *s = opaque;

    assert(line < SC64_B42_GPIO_NUM);

    if (level) {
        level = 1;
    }

    if (s->level[line] != level) {
        s->level[line] = level;
        sc64_b42_gpio_update(s);
    }
}

static void sc64_b42_set_gpio(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    Error *local_err = NULL;
    int64_t level;
    int gpio = (uint64_t)opaque;

    visit_type_int(v, name, &level, &local_err);

    qemu_set_irq(qdev_get_gpio_in(DEVICE(obj), gpio), level);
}

static void sc64_b42_gpio_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    SC64B42GPIO *s = SC64_B42_GPIO(dev);
    RegisterInfoArray *reg_array;
    int i;

    for (i = 0; i < SC64_B42_GPIO_NUM; i++) {
        char buf[16];
        snprintf(buf, 16, "%i", i);
        object_property_add(OBJECT(sbd), buf, "int",
                            NULL,
                            sc64_b42_set_gpio,
                            NULL, (void *)(uint64_t)i, NULL);
    }

    qdev_init_gpio_in(dev, sc64_b42_gpio_set, SC64_B42_GPIO_NUM);
    qdev_init_gpio_out(dev, s->handler, SC64_B42_GPIO_NUM);

    reg_array = register_init_block32(dev, sc64_b42_gpio_regs_info,
                              ARRAY_SIZE(sc64_b42_gpio_regs_info),
                              s->regs_info, s->regs,
                              &sc64_b42_gpio_ops,
                              0,
                              sizeof(s->regs));

    sysbus_init_irq(sbd, &s->irq);
    sysbus_init_mmio(sbd, &reg_array->mem);
}

static void sc64_b42_gpio_reset(DeviceState *dev)
{
    SC64B42GPIO *s = SC64_B42_GPIO(dev);

    memset(s->level, 0, sizeof(s->level));
}

static void sc64_b42_gpio_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->desc = "SC64 B42 GPIO block";
    dc->realize = sc64_b42_gpio_realize;
    dc->reset = sc64_b42_gpio_reset;
}

static TypeInfo sc64_b42_gpio_info = {
    .name = TYPE_SC64_B42_GPIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SC64B42GPIO),
    .class_init = sc64_b42_gpio_class_init
};

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    uint32_t regs[4];
    RegisterInfo regs_info[4];

    char *busname;
    void *address_space;

    SSIBus *ssi;
    qemu_irq cs;
} SC64B42SPI;

#define SC64_B42_SPI(obj) \
                    OBJECT_CHECK(SC64B42SPI, (obj), TYPE_SC64_B42_SPI)

REG32(EEPROM_CTRL, 0x0)
    FIELD(EEPROM_CTRL, EepSR, 24, 8)
    FIELD(EEPROM_CTRL, EepBlkAddrUpperBit, 20, 1)
    FIELD(EEPROM_CTRL, EepCmdStatus, 18, 1)
    FIELD(EEPROM_CTRL, EepPrsnt, 16, 2)
#define EepPrsnt_NOT_FOUND 0b00
#define EepPrsnt_FOUND_GOOD 0b01
#define EepPrsnt_FOUND_BAD 0b11
    FIELD(EEPROM_CTRL, EepCmd, 13, 3)
#define EepCmd_WRITE_SR 0b001
#define EepCmd_WRITE_DATA 0b010
#define EepCmd_READ_DATA 0b011
#define EepCmd_RESET_WE 0b100
#define EepCmd_READ_SR 0b101
#define EepCmd_SET_WE 0b110
#define EepCmd_FLASH_CMD 0b111
    FIELD(EEPROM_CTRL, EepBlkAddr, 0, 13)

REG32(EEPROM_DATA_BUF, 0x4)
    FIELD(EEPROM_DATA_BUF, EepBuf, 0, 32)

REG32(EEPROM_3BYTE_ADDR, 0xc)
    FIELD(EEPROM_3BYTE_ADDR, START_CFG_FROM_EEPROM, 17, 1)
    FIELD(EEPROM_3BYTE_ADDR, FLASH_CMD, 8, 8)
    FIELD(EEPROM_3BYTE_ADDR, EEP_3DBYTE_ADDR, 0, 8)

static void sc64_b42_spi_set_addr_raw(SC64B42SPI *s, uint32_t addr)
{
    ssi_transfer(s->ssi, (addr >> 16) & 0xff);
    ssi_transfer(s->ssi, (addr >> 8) & 0xff);
    ssi_transfer(s->ssi, (addr >> 0) & 0xff);
}

static uint32_t sc64_b42_spi_data(SC64B42SPI *s, uint32_t data)
{
    uint32_t ret = 0;
    ret |= ssi_transfer(s->ssi, (data >> 0) & 0xff) << 0;
    ret |= ssi_transfer(s->ssi, (data >> 8) & 0xff) << 8;
    ret |= ssi_transfer(s->ssi, (data >> 16) & 0xff) << 16;
    ret |= ssi_transfer(s->ssi, (data >> 24) & 0xff) << 24;
    return ret;
}

static void sc64_b42_spi_set_addr(SC64B42SPI *s)
{
    uint32_t ctrl = s->regs[R_EEPROM_CTRL];
    uint32_t addr;

    addr = FIELD_EX32(ctrl, EEPROM_CTRL, EepBlkAddr) << 2;
    addr |= FIELD_EX32(ctrl, EEPROM_CTRL, EepBlkAddrUpperBit) << 15;
    addr |= ARRAY_FIELD_EX32(s->regs, EEPROM_3BYTE_ADDR, EEP_3DBYTE_ADDR);

    sc64_b42_spi_set_addr_raw(s, addr);
}

static void sc64_b42_spi_ctrl_post_write(RegisterInfo *reg, uint64_t val)
{
    SC64B42SPI *s = SC64_B42_SPI(reg->opaque);
    uint32_t cmd = FIELD_EX32(val, EEPROM_CTRL, EepCmd);

    qemu_irq_lower(s->cs);
    if (cmd == EepCmd_FLASH_CMD) {
        cmd = ARRAY_FIELD_EX32(s->regs, EEPROM_3BYTE_ADDR, FLASH_CMD);
        ssi_transfer(s->ssi, cmd);
    } else {
        ssi_transfer(s->ssi, cmd);

        switch (cmd) {
        case EepCmd_READ_DATA:
            sc64_b42_spi_set_addr(s);
            ARRAY_FIELD_DP32(s->regs,
                        EEPROM_DATA_BUF, EepBuf, sc64_b42_spi_data(s, 0));
            break;
        case EepCmd_READ_SR:
            ARRAY_FIELD_DP32(s->regs,
                        EEPROM_CTRL, EepSR, ssi_transfer(s->ssi, 0));
            break;
        case EepCmd_WRITE_DATA:
            sc64_b42_spi_set_addr(s);
            sc64_b42_spi_data(s,
                        ARRAY_FIELD_EX32(s->regs, EEPROM_DATA_BUF, EepBuf));
            break;
        }
    }

    qemu_irq_raise(s->cs);
}

static void sc64_b42_spi_load(SC64B42SPI *s)
{
#define READ1(s) ssi_transfer(s->ssi, 0)
#define READ2(s) (READ1(s) | (READ1(s) << 8))
#define READ4(s) (READ2(s) | (READ2(s) << 16))
    uint32_t size;
    uint8_t magic;
    int rc = 0;

    if (!ARRAY_FIELD_EX32(s->regs, EEPROM_CTRL, EepPrsnt)) {
        return;
    }

    qemu_irq_lower(s->cs);
    ssi_transfer(s->ssi, EepCmd_READ_DATA);
    sc64_b42_spi_set_addr_raw(s, 0);

    magic = READ1(s);
    if (magic != 0x5a) {
        warn_report(TYPE_SC64_B42 ": Wrong magic number in EEPROM image. "
                                            "Expected 0x5a, got %02x", magic);
        rc = -1;
        goto out;
    }
    /* Skip reserved field */
    READ1(s);

    size = READ1(s) << 8;
    size = READ1(s);

    while (size >= 6) {
        uint16_t addr = READ2(s);
        uint32_t data = READ4(s);

        addr <<= 2;

        address_space_rw(s->address_space,
                    addr, MEMTXATTRS_UNSPECIFIED, (uint8_t *)&data, 4, true);

        size -= 6;
    }

out:
    qemu_irq_raise(s->cs);

    ARRAY_FIELD_DP32(s->regs, EEPROM_CTRL, EepPrsnt,
                            rc == 0 ? EepPrsnt_FOUND_GOOD : EepPrsnt_FOUND_BAD);
#undef READ1
#undef READ2
#undef READ4
}

static void sc64_b42_spi_3byte_addr_post_write(RegisterInfo *reg,
                                                                uint64_t val)
{
    SC64B42SPI *s = SC64_B42_SPI(reg->opaque);

    if (FIELD_EX32(val, EEPROM_3BYTE_ADDR, START_CFG_FROM_EEPROM)) {
        sc64_b42_spi_load(s);
    }
}

static const RegisterAccessInfo sc64_b42_spi_regs_info[] = {
    {
        .name = "EEPROM_CTRL",
        .addr = REG_ADDR(EEPROM_CTRL),
        .reset = 0,
        .ro = FIELD_MASK(EEPROM_CTRL, EepCmdStatus) |
                                            FIELD_MASK(EEPROM_CTRL, EepPrsnt),
        .post_write = sc64_b42_spi_ctrl_post_write,
    },
    {
        .name = "EEPROM_DATA_BUF",
        .addr = REG_ADDR(EEPROM_DATA_BUF),
        .reset = 0,
    },
    {
        .name = "EEPROM_3BYTE_ADDR",
        .addr = REG_ADDR(EEPROM_3BYTE_ADDR),
        .reset = 0,
        .post_write = sc64_b42_spi_3byte_addr_post_write,
    },
};

static const MemoryRegionOps sc64_b42_spi_ops = {
    .read = register_read_memory,
    .write = register_write_memory,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    }
};

static void sc64_b42_spi_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    SC64B42SPI *s = SC64_B42_SPI(dev);
    RegisterInfoArray *reg_array;

    s->ssi = ssi_create_bus(DEVICE(s), s->busname);

    reg_array = register_init_block32(dev, sc64_b42_spi_regs_info,
                              ARRAY_SIZE(sc64_b42_spi_regs_info),
                              s->regs_info, s->regs,
                              &sc64_b42_spi_ops,
                              0,
                              sizeof(s->regs));

    sysbus_init_mmio(sbd, &reg_array->mem);
}

static void sc64_b42_spi_reset(DeviceState *dev)
{
    SC64B42SPI *s = SC64_B42_SPI(dev);
    BusChild *bchild = QTAILQ_FIRST(&BUS(s->ssi)->children);
    DeviceState *child = bchild == NULL ? NULL : bchild->child;

    qbus_reset_all(BUS(s->ssi));

    if (child != NULL) {
        s->cs = qdev_get_gpio_in_named(child, SSI_GPIO_CS, 0);
    }

    ARRAY_FIELD_DP32(s->regs, EEPROM_CTRL, EepPrsnt,
                    child == NULL ? EepPrsnt_NOT_FOUND : EepPrsnt_FOUND_BAD);

    sc64_b42_spi_load(s);
}

static Property sc64_b42_spi_props[] = {
    DEFINE_PROP_PTR("address_space", SC64B42SPI, address_space),
    DEFINE_PROP_STRING("busname", SC64B42SPI, busname),
    DEFINE_PROP_END_OF_LIST(),
};

static void sc64_b42_spi_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->desc = "SC64 B42 SPI block";
    dc->realize = sc64_b42_spi_realize;
    dc->reset = sc64_b42_spi_reset;
    dc->props = sc64_b42_spi_props;
}

static TypeInfo sc64_b42_spi_info = {
    .name = TYPE_SC64_B42_SPI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SC64B42SPI),
    .class_init = sc64_b42_spi_class_init
};

static const TypeInfo sc64_b42_bus_info = {
    .name = TYPE_SC64_B42_BUS,
    .parent = TYPE_BUS,
};

static void sc64_b42_register_types(void)
{
    type_register_static(&sc64_b42);
    type_register_static(&sc64_b42_d);
    type_register_static(&sc64_b42_i2cs_info);
    type_register_static(&sc64_b42_gpio_info);
    type_register_static(&sc64_b42_spi_info);
    type_register_static(&sc64_b42_bus_info);
}

type_init(sc64_b42_register_types)
