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
#include "hw/hw.h"
#include "hw/block/sc64-nand.h"
#include "hw/block/sc64-onfi.h"
#include "hw/can/sc64-can.h"
#include "hw/display/vdc35-vgafb.h"
#include "hw/mips/sccr-sysbus.h"
#include "hw/sysbus.h"
#include "qemu/typedefs.h"

#define TYPE_SC64_SCCR "sc64-sccr"
#define SC64_SCCR(obj) OBJECT_CHECK(Sc64SccrState, (obj), TYPE_SC64_SCCR)

#define SC64_SCCR_BASE          0x1b000000
#define SC64_SCCR_SIZE          0x1000

#define SC64_SYS_DEV            0x140
# define PIC48_BIT              (1 << 6)
#define SC64_SYS_RAM_FREQ       0x148
#define SC64_SYS_NAND           0x14c
#define SC64_SYS_CPU            0x154
#define SC64_MEM_CTRL           0x174
#define SC64_ERROR_ADDR         0x178
#define SC64_MISC               0x17c
# define SC64_MISC_ARBITER      (1 << 17)
# define SC64_MISC_NAND         (1 << 27)
#define SC64_MEM_FREQ           0x180
#define SC64_CPU_FREQ           0x184
#define SC64_PIX_FREQ           0x188
#define SC64_CONFIG_CTRL        0x18c
#define SC64_AXI_FREQ           0x190
#define SC64_SPC_DBE            0x1a4 /* FIXME */
#define SC64_VDC_CONF           0x28c
#define SC64_CORE_MASK          0x3ac
#define SC64_CAN_CONF           0x420

#define REG_IDX(x) ((x & 0xfff) >> 2)

#define IRQ_MAP(x, pic24, pic48)    [(x) * 2] = pic24, pic48,

static const int duna_irq_mapping[] = {
    IRQ_MAP(SC64_TIMER_V1_0_IRQ, DUNA_TIMER0_IRQ, -1)
    IRQ_MAP(SC64_TIMER_V1_1_IRQ, DUNA_TIMER1_IRQ, -1)
    IRQ_MAP(SC64_TIMER_V1_2_IRQ, DUNA_TIMER2_IRQ, -1)
    IRQ_MAP(SC64_TIMER_V1_3_IRQ, DUNA_TIMER3_IRQ, -1)
    IRQ_MAP(SC64_TIMER_V1_4_IRQ, DUNA_TIMER4_IRQ, -1)

    IRQ_MAP(SC64_TIMER_V2_0_IRQ, -1, 8)
    IRQ_MAP(SC64_TIMER_V2_1_IRQ, -1, 9)
    IRQ_MAP(SC64_TIMER_V2_2_IRQ, -1, 10)
    IRQ_MAP(SC64_TIMER_V2_3_IRQ, -1, 11)
    IRQ_MAP(SC64_TIMER_V2_4_IRQ, -1, 12)
    IRQ_MAP(SC64_TIMER_V2_5_IRQ, -1, 13)
    IRQ_MAP(SC64_TIMER_V2_6_IRQ, -1, 14)
    IRQ_MAP(SC64_TIMER_V2_7_IRQ, -1, 15)

    IRQ_MAP(SC64_CAN0_IRQ, -1, 16)
    IRQ_MAP(SC64_CAN1_IRQ, -1, 17)

    IRQ_MAP(SC64_VDC35_IRQ, DUNA_VDC35_IRQ, 46)

    IRQ_MAP(SC64_SATA_IRQ, DUNA_SATA_IRQ, 37)

    IRQ_MAP(SC64_I2C0_IRQ, DUNA_I2C0_IRQ, 18)
    IRQ_MAP(SC64_I2C1_IRQ, DUNA_I2C1_IRQ, 19)

    IRQ_MAP(SC64_UART0_IRQ, DUNA_UART0_IRQ, 20)
    IRQ_MAP(SC64_UART1_IRQ, DUNA_UART1_IRQ, 21)

    IRQ_MAP(SC64_FASTUART0_IRQ, -1, 40)
    IRQ_MAP(SC64_FASTUART1_IRQ, -1, 41)

    IRQ_MAP(SC64_SPI_IRQ, DUNA_SPI_IRQ, 26)
    IRQ_MAP(SC64_NAND_IRQ, DUNA_NAND_IRQ, 27)

    IRQ_MAP(SC64_PORTA2_IRQ, DUNA_PORTA2_IRQ, 28)
    IRQ_MAP(SC64_PORTA3_IRQ, DUNA_PORTA3_IRQ, 29)
    IRQ_MAP(SC64_PORTB2_IRQ, DUNA_PORTB2_IRQ, 30)
    IRQ_MAP(SC64_PORTB3_IRQ, DUNA_PORTB3_IRQ, 31)

    IRQ_MAP(SC64_OUTER0_IRQ, DUNA_OUTER_IRQ0, 32)
    IRQ_MAP(SC64_OUTER1_IRQ, DUNA_OUTER_IRQ1, 33)
    IRQ_MAP(SC64_MPU_IRQ, -1, 34)

    IRQ_MAP(SC64_USB_IRQ, DUNA_USB_IRQ, 35)

    IRQ_MAP(SC64_RIO_IRQ, DUNA_RIO_IRQ, 36)

    IRQ_MAP(SC64_ETH0_IRQ, DUNA_ETH0_IRQ, 38)
    IRQ_MAP(SC64_ETH1_IRQ, -1, 39)

    IRQ_MAP(SC64_IDMA_IRQ, -1, 42)

    IRQ_MAP(SC64_PCI_A_IRQ, 0, -1)
    IRQ_MAP(SC64_PCI_B_IRQ, 1, -1)
    IRQ_MAP(SC64_PCI_C_IRQ, 2, -1)
    IRQ_MAP(SC64_PCI_D_IRQ, 3, -1)

    IRQ_MAP(SC64_PCIE0_A_IRQ, -1, 0)
    IRQ_MAP(SC64_PCIE0_B_IRQ, -1, 1)
    IRQ_MAP(SC64_PCIE0_C_IRQ, -1, 2)
    IRQ_MAP(SC64_PCIE0_D_IRQ, -1, 3)
    IRQ_MAP(SC64_PCIE0_MSIX_IRQ, -1, 44)

    IRQ_MAP(SC64_PCIE1_A_IRQ, -1, 4)
    IRQ_MAP(SC64_PCIE1_B_IRQ, -1, 5)
    IRQ_MAP(SC64_PCIE1_C_IRQ, -1, 6)
    IRQ_MAP(SC64_PCIE1_D_IRQ, -1, 7)
    IRQ_MAP(SC64_PCIE1_MSIX_IRQ, -1, 45)

    IRQ_MAP(SC64_TEMP_IRQ, -1, 43)
};

#define GET_IRQ_ENTRY(x)    &duna_irq_mapping[(x) * 2]

typedef struct Sc64SccrState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint32_t regs[SC64_SCCR_SIZE << 2];
    /*CPUMIPSState *cpu_env;*/

    qemu_irq *pic[2];
} Sc64SccrState;

static void
sc64_sccr_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    Sc64SccrState *s = opaque;
    uint32_t saddr;
    DeviceState *dev;
    MemoryRegion *mem;
    int i;

    saddr = (addr & 0xfff) >> 2;
    switch (addr) {
    case SC64_MISC:
        dev = qdev_find_recursive(sysbus_get_default(), TYPE_SC64_ONFI);
        if (dev) {
            mem = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
            memory_region_set_enabled(mem, !!(val & SC64_MISC_NAND));
        }
        /* Fall through */
    case SC64_SYS_DEV:
    case SC64_SYS_RAM_FREQ:
    case SC64_SYS_NAND:
    case SC64_MEM_CTRL:
    case SC64_ERROR_ADDR:
    case SC64_MEM_FREQ:
    case SC64_PIX_FREQ:
    case SC64_CONFIG_CTRL:
    case SC64_AXI_FREQ:
        s->regs[saddr] = val;
    break;
    case SC64_SYS_CPU: /* bits 31:16 are read-only */
        s->regs[saddr] = (s->regs[saddr] & 0xFFFF0000) |
                          (val & 0x0000FFFF);
    break;

    case SC64_SPC_DBE:
        /*cpu_unassigned_access(s->cpu_env, addr, 1, 0, 0, 4);*/
    break;

    case SC64_VDC_CONF:
        s->regs[saddr] = val;
        dev = qdev_find_recursive(sysbus_get_default(), TYPE_VDC35_VGAFB);

        if (dev == 0) {
            printf("sc64-sccr: Warning: videocontroller not found\n");
        } else {
            vdc35_set_swap_type(dev, (val >> 10) & 3);
        }

        break;
    case SC64_CAN_CONF:
        s->regs[saddr] = val;

        for (i = 0; i < SC64_CAN_NUM; i++) {
            dev = qdev_find_recursive(sysbus_get_default(), sc64_can_name[i]);

            if (dev) {
                sc64_can_mask_set(dev, (val >> (i * 4)) & 0xf);
            }
        }
    break;
    case SC64_CORE_MASK:
        break;
    default:
        printf("sc64-sccr: Write bad register offset 0x%x\n", (int)addr);
    break;
    }
}

static uint64_t sc64_sccr_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64SccrState *s = opaque;
    uint32_t val = 0;
    uint32_t saddr;

    saddr = (addr & 0xfff) >> 2;
    switch (addr) {
    case SC64_SYS_DEV:
    case SC64_SYS_RAM_FREQ:
    case SC64_SYS_NAND:
    case SC64_SYS_CPU:
    case SC64_MEM_CTRL:
    case SC64_ERROR_ADDR:
    case SC64_MISC:
    case SC64_MEM_FREQ:
    case SC64_CPU_FREQ:
    case SC64_PIX_FREQ:
    case SC64_CONFIG_CTRL:
    case SC64_AXI_FREQ:
    case SC64_VDC_CONF:
    case SC64_CORE_MASK:
        val = s->regs[saddr];
    break;

    case SC64_SPC_DBE:
        /*cpu_unassigned_access(s->cpu_env, addr, 1, 0, 0, 4);*/
    break;

    default:
        printf("sc64-sccr: Read bad register offset 0x%x\n", (int)addr);
    break;
    }

    return val;
}

static const MemoryRegionOps sccr_mem_ops = {
    .read = sc64_sccr_read,
    .write = sc64_sccr_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void sc64_sccr_reset(DeviceState *dev)
{
    Sc64SccrState *s = SC64_SCCR(dev);

#define WR32(off, val)  s->regs[(off) >> 2] = (val)
    WR32(SC64_SYS_DEV, 0x04440001);
    WR32(SC64_SYS_RAM_FREQ, 0x00230406);
    WR32(SC64_SYS_NAND, 0x00000000);
    WR32(SC64_SYS_CPU, 0xb02a00c0 /* 0x0000c0c0 */ );
/* test registers do not reset */
/*  WR32(SC64_SYS_TEST1, 0x00000000);
    WR32(SC64_SYS_TEST2, 0x00000000);*/
    WR32(SC64_MEM_CTRL, 0x00000080);
    WR32(SC64_ERROR_ADDR, 0x00000000);
    WR32(SC64_MISC, 0x00007ff0
#ifdef PCI_ARBITER
                                & ~SC64_MISC_ARBITER /* arbiter turned on */
#else
                                | SC64_MISC_ARBITER /* arbiter turned off */
#endif
    );
    /*
     * popular FREQ values:
     *       0x06801604    96 MHz
     *       0x06413004   100 MHz
     *       0x06403004   200 MHz
     */
    WR32(SC64_MEM_FREQ, 0x04072005);
    WR32(SC64_CPU_FREQ, 0x06400405);
    WR32(SC64_PIX_FREQ, 0x00006100);
    WR32(SC64_CONFIG_CTRL, 0x00000000);
    WR32(SC64_AXI_FREQ, 0x04072005);
    WR32(SC64_CORE_MASK, 1);
#undef WR32

    /* Side-effect of register write is imporant */
    sc64_sccr_write(s, SC64_CAN_CONF, 0x21, 4);
}

static DeviceState *sc64_sccr_register(const char *name, hwaddr addr)
{
    DeviceState *dev;
    SysBusDevice *s;

    dev = qdev_create(NULL, name);
    s = SYS_BUS_DEVICE(dev);
    qdev_init_nofail(dev);
    if (addr != (hwaddr)-1) {
        sysbus_mmio_map(s, 0, addr);
    }

    return dev;
}

void sysbus_sccr_register(hwaddr addr)
{
    sc64_sccr_register(TYPE_SC64_SCCR, addr);
}

static void sc64_sccr_irq_handler(void *opaque, int in_irq, int level)
{
    Sc64SccrState *s = opaque;
    bool pic48_enabled = s->regs[SC64_SYS_DEV >> 2] & PIC48_BIT;
    int pic_index = pic48_enabled ? 1 : 0;
    const int *out = GET_IRQ_ENTRY(in_irq);

    int map = out[pic_index];
    if (map == -1) {
        return;
    }
    qemu_set_irq(s->pic[pic_index][map], level);
}

qemu_irq *sysbus_sccr_v2_register(hwaddr addr, qemu_irq *pic24, qemu_irq *pic48)
{
    DeviceState *dev = sc64_sccr_register(TYPE_SC64_SCCR, addr);
    Sc64SccrState *s = SC64_SCCR(dev);

    s->pic[0] = pic24;
    s->pic[1] = pic48;

    return qemu_allocate_irqs(sc64_sccr_irq_handler, s, SC64_MAX_IRQ);
}

static int sc64_sccr_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    Sc64SccrState *s = SC64_SCCR(dev);

    memory_region_init_io(&s->iomem, OBJECT(s),
                          &sccr_mem_ops, s, TYPE_SC64_SCCR, SC64_SCCR_SIZE);

    sysbus_init_mmio(sbd, &s->iomem);

    s->pic[0] = NULL;
    s->pic[1] = NULL;

    return 0;
}

static void sc64_sccr_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_sccr_init;
    dc->desc = "SC64 SCCR block";
    dc->reset = sc64_sccr_reset;
}

static const TypeInfo sc64_sccr_sysbus_info = {
    .name          = TYPE_SC64_SCCR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64SccrState),
    .class_init    = sc64_sccr_sysbus_class_init,
};

static void sc64_sccr_register_types(void)
{
    type_register_static(&sc64_sccr_sysbus_info);
}

type_init(sc64_sccr_register_types)
