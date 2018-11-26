/*
 * Copyright (C) 2017
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
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/ssi/sc64-spi-sysbus.h"
#include "sysemu/dma.h"
#include "hw/block/m25p80.h"

#include "exec/cpu-common.h"

#define TYPE_SC64_SPI "sc64-spi"
#define SC64_SPI(obj) OBJECT_CHECK(Sc64SpiState, (obj), TYPE_SC64_SPI)
#define SC64_SPI_BASE   0x01a60000
#define SC64_SPI_SIZE   0x100

#define SC64_SPI_REG_CTRL               0x00
#define SC64_SPI_REG_MODES              0x04
#define SC64_SPI_REG_CONFIG             0x08
#define SC64_SPI_REG_STATUS             0x0C
# define SC64_SPI_STATUS_IRQ_DONE       (1 << 16)
# define SC64_SPI_STATUS_SPI_FINISHED   (1 << 17)
#define SC64_SPI_REG_CPU_CONFIG         0x10
# define SC64_SPI_PROTECT               (1 << 15)
#define SC64_SPI_REG_TIMINGS            0x14
#define SC64_SPI_REG_IRQ_ENABLE         0x18
#define SC64_SPI_REG_DMA_CONFIG         0x1C
# define SC64_SPI_DMA_CONFIG_CMD_TX     (1 << 31)
#define SC64_SPI_REG_INSTR_MODES        0x20
#define SC64_SPI_REG_INSTR_CS           0x24
#define SC64_SPI_REG_INSTR_LEN          0x28
#define SC64_SPI_REG_INSTR_PARAMS       0x2C
# define SC64_SPI_INSTR_PARAMS_IRQ      (1 << 1)
#define SC64_SPI_REG_INSTR_TX_ADDR_LO   0x30
#define SC64_SPI_REG_INSTR_TX_ADDR_HI   0x34
#define SC64_SPI_REG_INSTR_RX_ADDR_LO   0x38
#define SC64_SPI_REG_INSTR_RX_ADDR_HI   0x3C
#define SC64_SPI_REG_AXI_ID             0x40
#define SC64_SPI_REG_VERSION            0x48
#define SC64_SPI_REG_CPU_TIMINGS        0x4C

#define REG_IDX(x) ((x & 0xfff) >> 2)

#define SC64_SPI_DMA_MAX_LENGTH (64 * 1024)
#define SC64_SPI_SLAVE_COUNT    4
#define SC64_SPI_SLAVE_ACTIVE   0
#define SC64_SPI_SLAVE_INACTIVE 1

struct Sc64SpiRegAccess {
    uint32_t read_mask;
    uint32_t write_mask;
    uint32_t prot_mask;
    uint32_t w1c_mask;
} Sc64SpiRegAccess[] = {
    /* SC64_SPI_REG_CTRL */
    {
        .read_mask  = 0x7FC00F03,
        .write_mask = 0x7FC00F03,
        .prot_mask  = 0x7FC00F00,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_MODES */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_CONFIG */
    {
        .read_mask  = 0xFFFFFFF0,
        .write_mask = 0x0000000F,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_STATUS */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0xFFFFDA00,
    },
    /* SC64_SPI_REG_CPU_CONFIG */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0X07FFFFFF,
        .prot_mask  = 0x07FF7FFF,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_TIMINGS */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_IRQ_ENABLE */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFE00,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_DMA_CONFIG */
    {
        .read_mask  = 0x7FFFFFFE,
        .write_mask = 0x80000002,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000001,
    },
    /* SC64_SPI_REG_INSTR_MODES */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x000007FF,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_INSTR_CS */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000003,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_INSTR_LEN */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x0000FFFF,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_INSTR_PARAMS */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00001FFF,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_INSTR_TX_ADDR_LO */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_INSTR_TX_ADDR_HI */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x0000000F,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_INSTR_RX_ADDR_LO */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_INSTR_RX_ADDR_HI */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x0000000F,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_AXI_ID */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x0000FFFF,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_VERSION */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .prot_mask  = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    /* SC64_SPI_REG_CPU_TIMINGS */
    {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x07FFFF00,
        .prot_mask  = 0x07FFFF00,
        .w1c_mask   = 0x00000000,
    },
};

typedef struct Sc64SpiState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint32_t regs[SC64_SPI_SIZE >> 2];

    AddressSpace *dma_as;

    uint32_t bus_buf;

    qemu_irq irq;

    SSIBus *ssi;

    qemu_irq *cs_lines;

    MemoryRegion *cs[2], mr_block[2];
} Sc64SpiState;

uint8_t sc64_spi_buf[SC64_SPI_DMA_MAX_LENGTH];

static void sc64_spi_chip_select(Sc64SpiState *s, int num, int state)
{
    if (num < 0 || num >= SC64_SPI_SLAVE_COUNT) {
        printf("Wrong SPI slave number!\n");
        return;
    }

    qemu_set_irq(s->cs_lines[num], state);
}

static void sc64_spi_push_cmd(Sc64SpiState *s)
{
    hwaddr src =
        ((uint64_t) s->regs[REG_IDX(SC64_SPI_REG_INSTR_TX_ADDR_HI)] << 32) |
        s->regs[REG_IDX(SC64_SPI_REG_INSTR_TX_ADDR_LO)];

    hwaddr dst =
        ((uint64_t)s->regs[REG_IDX(SC64_SPI_REG_INSTR_RX_ADDR_HI)]) << 32 |
        s->regs[REG_IDX(SC64_SPI_REG_INSTR_RX_ADDR_LO)];

    int len = s->regs[REG_IDX(SC64_SPI_REG_INSTR_LEN)];
    if (len == 0) {
        len = SC64_SPI_DMA_MAX_LENGTH;
    }

    int slave_id = s->regs[REG_IDX(SC64_SPI_REG_INSTR_CS)];

    sc64_spi_chip_select(s, slave_id, SC64_SPI_SLAVE_ACTIVE);

    if (s->regs[REG_IDX(SC64_SPI_REG_INSTR_PARAMS)] & (1 << 2)) {
        int i;
        dma_memory_read(s->dma_as, src, sc64_spi_buf, len);

        for (i = 0; i < len; i++) {
            ssi_transfer(s->ssi, sc64_spi_buf[i]);
        }
    }

    if (s->regs[REG_IDX(SC64_SPI_REG_INSTR_PARAMS)] & (1 << 3)) {
        int i;
        for (i = 0; i < len; i++) {
            sc64_spi_buf[i] = ssi_transfer(s->ssi, 0); /* NOP */
        }

        dma_memory_write(s->dma_as, dst, sc64_spi_buf, len);
    }

    if (s->regs[REG_IDX(SC64_SPI_REG_INSTR_PARAMS)] &
            SC64_SPI_INSTR_PARAMS_IRQ) {
        s->regs[REG_IDX(SC64_SPI_REG_STATUS)] |= SC64_SPI_STATUS_IRQ_DONE;
    }

    if (!(s->regs[REG_IDX(SC64_SPI_REG_INSTR_PARAMS)] & 0x1)) {
        sc64_spi_chip_select(s, slave_id, SC64_SPI_SLAVE_INACTIVE);
    }

    if (s->regs[REG_IDX(SC64_SPI_REG_IRQ_ENABLE)]) {
        s->regs[REG_IDX(SC64_SPI_REG_STATUS)] |= SC64_SPI_STATUS_IRQ_DONE;
        qemu_irq_raise(s->irq);
    }

    s->regs[REG_IDX(SC64_SPI_REG_STATUS)] |= SC64_SPI_STATUS_SPI_FINISHED;
}

static void sc64_spi_write(void *opaque, hwaddr addr, uint64_t val,
        unsigned size)
{
    Sc64SpiState *s = opaque;
    int saddr;
    int prot;
    uint32_t write, w1c;

    if (addr > SC64_SPI_SIZE) {
        printf("SPI: write memory out of range!\n");
        return;
    }

    if (addr & 0x3) {
        val = val << (8 * (addr & 0x3));
        addr &= ~0x3;
    }

    saddr = REG_IDX(addr);
    prot = s->regs[REG_IDX(SC64_SPI_REG_CPU_CONFIG)] & SC64_SPI_PROTECT;

    w1c = val & Sc64SpiRegAccess[saddr].w1c_mask;
    s->regs[saddr] &= ~w1c;

    write = Sc64SpiRegAccess[saddr].write_mask;

    if (!prot) {
        write &= ~Sc64SpiRegAccess[saddr].prot_mask;
    }

    s->regs[saddr] &= ~write;
    s->regs[saddr] |= val & write;

    switch (addr) {
    case SC64_SPI_REG_CTRL:
    case SC64_SPI_REG_MODES:
        break;
    case SC64_SPI_REG_CONFIG:
        if (val & (1 << 0)) {
            sc64_spi_chip_select(s, 0, SC64_SPI_SLAVE_INACTIVE);
        }
        if (val & (1 << 1)) {
            sc64_spi_chip_select(s, 1, SC64_SPI_SLAVE_INACTIVE);
        }
        if (val & (1 << 2)) {
            sc64_spi_chip_select(s, 2, SC64_SPI_SLAVE_INACTIVE);
        }
        if (val & (1 << 3)) {
            sc64_spi_chip_select(s, 3, SC64_SPI_SLAVE_INACTIVE);
        }

        break;
    case SC64_SPI_REG_STATUS:
        if (val & (SC64_SPI_STATUS_IRQ_DONE | SC64_SPI_STATUS_SPI_FINISHED)) {
            qemu_irq_lower(s->irq);
        }
        break;
    case SC64_SPI_REG_CPU_CONFIG:
    case SC64_SPI_REG_TIMINGS:
    case SC64_SPI_REG_IRQ_ENABLE:
        break;
    case SC64_SPI_REG_DMA_CONFIG:
        if (val & SC64_SPI_DMA_CONFIG_CMD_TX) {
            /* Transmit instruction into buffer */
            sc64_spi_push_cmd(s);
        }
        break;
    case SC64_SPI_REG_INSTR_MODES:
    case SC64_SPI_REG_INSTR_CS:
    case SC64_SPI_REG_INSTR_LEN:
    case SC64_SPI_REG_INSTR_PARAMS:
    case SC64_SPI_REG_INSTR_TX_ADDR_LO:
    case SC64_SPI_REG_INSTR_TX_ADDR_HI:
    case SC64_SPI_REG_INSTR_RX_ADDR_LO:
    case SC64_SPI_REG_INSTR_RX_ADDR_HI:
    case SC64_SPI_REG_AXI_ID:
    case SC64_SPI_REG_VERSION:
    case SC64_SPI_REG_CPU_TIMINGS:
        break;
    default:
        printf("SPI: Bad register offset 0x%x\n", (int) addr);
        break;
    }
}

static uint64_t sc64_spi_read(void *opaque, hwaddr addr, unsigned size)
{
    if (addr > SC64_SPI_SIZE) {
        printf("SPI: read memory out of range!\n");
        return 0;
    }

    Sc64SpiState *s = opaque;

    int saddr = REG_IDX(addr);

    switch (addr) {
    case SC64_SPI_REG_CTRL:
    case SC64_SPI_REG_MODES:
    case SC64_SPI_REG_CONFIG:
    case SC64_SPI_REG_STATUS:
    case SC64_SPI_REG_CPU_CONFIG:
    case SC64_SPI_REG_TIMINGS:
    case SC64_SPI_REG_IRQ_ENABLE:
    case SC64_SPI_REG_DMA_CONFIG:
    case SC64_SPI_REG_INSTR_MODES:
    case SC64_SPI_REG_INSTR_CS:
    case SC64_SPI_REG_INSTR_LEN:
    case SC64_SPI_REG_INSTR_PARAMS:
    case SC64_SPI_REG_INSTR_TX_ADDR_LO:
    case SC64_SPI_REG_INSTR_TX_ADDR_HI:
    case SC64_SPI_REG_INSTR_RX_ADDR_LO:
    case SC64_SPI_REG_INSTR_RX_ADDR_HI:
    case SC64_SPI_REG_AXI_ID:
    case SC64_SPI_REG_VERSION:
    case SC64_SPI_REG_CPU_TIMINGS:
        return s->regs[saddr] & Sc64SpiRegAccess[saddr].read_mask;
    default:
        printf("SPI: Bad register offset 0x%x\n", (int) addr);
        break;
    }

    return 0;
}

static const MemoryRegionOps spi_mem_ops = {
    .read = sc64_spi_read,
    .write = sc64_spi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static int sc64_spi_walker(DeviceState *dev, void *opaque)
{
    Sc64SpiState *s = opaque;
    qemu_irq cs_line = qdev_get_gpio_in_named(dev, SSI_GPIO_CS, 0);
    int idlen = dev->id ? strlen(dev->id) : 0;
    unsigned int i;
    int rc;

    if (idlen < 3) {
        fprintf(stderr, "Disabling SPI device due to missing id with 'cs'\n");
        qemu_irq_raise(cs_line);
        return 0;
    }

    rc = sscanf(dev->id + idlen - 3, "cs%u", &i);

    if (i >= SC64_SPI_SLAVE_COUNT || rc != 1) {
        fprintf(stderr, "Disabling SPI device due to unsopported 'cs': %s\n",
                                                                    dev->id);
        qemu_irq_raise(cs_line);
        return 0;
    }

    sysbus_connect_irq(SYS_BUS_DEVICE(s), i + 1, cs_line);

    if (i < 2) {
        m25p80_init_mr(SSI_SLAVE(dev), &s->mr_block[i]);
        memory_region_add_subregion(s->cs[i], 0, &s->mr_block[i]);
    }

    return 0;
}

static void sc64_spi_reset(DeviceState *dev)
{
    Sc64SpiState *s = SC64_SPI(dev);

    memset(s->regs, 0, sizeof(s->regs));
    s->regs[REG_IDX(SC64_SPI_REG_VERSION)]
        = 0x10120343;
    s->regs[REG_IDX(SC64_SPI_REG_STATUS)]
        = (1 << 6) | (1 << 2) | (1 << 1);
    s->regs[REG_IDX(SC64_SPI_REG_CPU_CONFIG)]
        = (0x11 << 16) | (3 << 8) | (3 << 0);
    s->regs[REG_IDX(SC64_SPI_REG_TIMINGS)]
        = (7 << 20) | (1 << 14) | (1 << 8) | (0x11 << 0);
    s->regs[REG_IDX(SC64_SPI_REG_CPU_TIMINGS)]
        = (7 << 20) | (1 << 14) | (1 << 8);

    qbus_walk_children(BUS(s->ssi), sc64_spi_walker, NULL, NULL, NULL, s);
}

void sysbus_spi_register(const char *name, hwaddr addr, qemu_irq irq,
        AddressSpace *dma_as, MemoryRegion *cs0, MemoryRegion *cs1)
{
    DeviceState *dev;
    SysBusDevice *sbd;
    Sc64SpiState *p;

    dev = qdev_create(NULL, name);
    p = SC64_SPI(dev);
    p->dma_as = dma_as;
    qdev_init_nofail(dev);

    sbd = SYS_BUS_DEVICE(dev);
    if (addr != (hwaddr)-1) {
        sysbus_mmio_map(sbd, 0, addr);
    }
    sysbus_connect_irq(sbd, 0, irq);

    p->cs[0] = cs0;
    p->cs[1] = cs1;
}

static int sc64_spi_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    Sc64SpiState *s = SC64_SPI(dev);
    int i;

    s->ssi = ssi_create_bus(dev, "spi");
    sysbus_init_irq(sbd, &s->irq);

    memory_region_init_io(&s->iomem, OBJECT(s),
                            &spi_mem_ops, s, TYPE_SC64_SPI, SC64_SPI_SIZE);

    sysbus_init_mmio(sbd, &s->iomem);

    s->cs_lines = g_new0(qemu_irq, SC64_SPI_SLAVE_COUNT);
    for (i = 0; i < SC64_SPI_SLAVE_COUNT; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->cs_lines[i]);
    }

    return 0;
}

static void sc64_spi_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_spi_init;
    dc->desc = "SC64 SPI block";
    dc->reset = sc64_spi_reset;
}

static const TypeInfo sc64_spi_sysbus_info = {
    .name          = TYPE_SC64_SPI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64SpiState),
    .class_init    = sc64_spi_sysbus_class_init,
};

static void sc64_spi_register_types(void)
{
    type_register_static(&sc64_spi_sysbus_info);
}

type_init(sc64_spi_register_types)
