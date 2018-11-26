/*
 * Copyright (C) 2018 Denis Deryugin <deryugin.denis@gmail.com>
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
#include "hw/block/flash.h"
#include "hw/block/sc64-onfi.h"
#include "hw/can/sc64-can.h"
#include "hw/sysbus.h"
#include "qemu/error-report.h"
#include "qemu/typedefs.h"
#include "sysemu/dma.h"

#define SC64_ONFI(obj) \
    OBJECT_CHECK(SC64ONFIState, (obj), TYPE_SC64_ONFI)

/* #define DEBUG */

#ifdef DEBUG
#define dbg(...)     printf("NAND Controller: " __VA_ARGS__)
#define dbg_raw(...)     printf(__VA_ARGS__)
#else
#define dbg(...)
#define dbg_raw(...)
#endif

#define NAND_COMMAND                0x00
# define COMMAND_GO                 (1 << 31)
# define COMMAND_CLE                (1 << 30)
# define COMMAND_ALE                (1 << 29)
# define COMMAND_PIO                (1 << 28)
# define COMMAND_TX                 (1 << 27)
# define COMMAND_RX                 (1 << 26)
# define COMMAND_SEC_CMD            (1 << 25)
# define COMMAND_AFT_DAT            (1 << 24)
# define COMMAND_A_VALID            (1 << 19)
# define COMMAND_B_VALID            (1 << 18)
# define COMMAND_RD_STATUS_CHK      (1 << 17)
# define COMMAND_RBSY_CHK           (1 << 16)
# define COMMAND_CE(n)              (1 << (8 + (n)))
#define NAND_STATUS                 0x04
# define STATUS_ISEMPTY             (1 << 0)
#define NAND_ISR                    0x08
#define NAND_IER                    0x0C
# define IER_IE_CMD_DONE            (1 << 5)
# define IER_IE_LL_DONE             (1 << 3)
# define IER_IE_LL_ERR              (1 << 2)
# define IER_GIE                    (1 << 0)
#define NAND_CONFIG                 0x10
#define NAND_TIMING                 0x14
#define NAND_RESP                   0x18
#define NAND_TIMING2                0x1C
#define NAND_CMD_REG1               0x20
#define NAND_CMD_REG2               0x24
#define NAND_ADDR_REG1              0x28
#define NAND_ADDR_REG2              0x2C
#define NAND_DMA                    0x30
# define DMA_GO                     (1 << 31)
# define DMA_DIR                    (1 << 30)
# define IE_DMA_DONE                (1 << 28)
# define DMA_IS_DONE                (1 << 20)
#define NAND_DMA_CFG_A              0x34
#define NAND_DMA_CFG_B              0x38
#define NAND_FIFO_CTRL               0x3C
# define LL_BUF_EMPTY               (1 << 15)
# define LL_BUF_FULL                (1 << 14)
# define LL_BUF_CLR                 (1 << 3)
#define NAND_DATA_BLOCK_PTR         0x40
#define NAND_TAG_PTR                0x44
#define NAND_ECC_PTR                0x48
#define NAND_DEC_STATUS             0x4C
#define NAND_HWSTATUS_CMD           0x50
#define NAND_HWSTATUS_MASK          0x54
#define NAND_LL_CONFIG              0x58
# define LL_START                   (1 << 31)
# define LL_WORD_CNT_EN             (1 << 30)
#define NAND_LL_PTR                 0x5C
#define NAND_LL_STATUS              0x60
#define LL_LEN_LAST_OFFT            16
#define LL_LEN_LAST_MASK            (0xF << LL_LEN_LAST_OFFT)
#define LL_LEN_MASK                 0xFFF
#define LL_PKT_ID_OFFT              24
#define LL_PKT_ID_MASK              (0xFF << LL_PKT_ID_OFFT)
# define IS_LL_DONE                 (1 << 23)
# define IS_LL_ERR                  (1 << 22)
#define NAND_LOCK_CONTROL           0x64
#define NAND_LOCK_STATUS            0x68
#define NAND_LOCK_APER_START(n)     (0x6C + 4 * (n))
#define NAND_LOCK_APER_END(n)       (0x8C + 4 * (n))
#define NAND_LOCK_APER_CHIPID(n)    (0xAC + 4 * (n))
#define NAND_BCH_CONFIG             0xCC
#define NAND_BCH_DEC_RESULT         0xD0
#define NAND_BCH_DEC_STATUS_BUF     0xD4
#define NAND_CONTROL                0xE0
# define CONTROL_RESET              (1 << 0)
#define NAND_CONFIG2                0xE4
#define NAND_VERSION                0xF0
#define NAND_DATE                   0xF4
#define REG_SIZE                    (0xF8 / 4)

#define REG_IDX(x) ((x & 0xfff) >> 2)

#define REG(s, r) ((s)->regs[REG_IDX(r)])

#define DEV_NUM 8

#define CHIP_ENABLED(s, n)  (REG(s, NAND_COMMAND) & COMMAND_CE(n) ? 1 : 0)
#define PAGE_BYTES(s)      ((((REG(s, NAND_COMMAND) >> 20) & 0xF) == 0x8) ?  \
        (1 << (8 + ((REG(s, NAND_CONFIG) >> 16) & 0x7))) : \
        1 + ((REG(s, NAND_COMMAND) >> 20) & 0xF))
#define CLE_BYTES(s)        (1 + ((REG(s, NAND_COMMAND) >> 4) & 0x3))
#define ALE_BYTES(s)        (1 + (REG(s, NAND_COMMAND) & 0xF))
#define QUEUE_BYTES(s)      (4 * (REG(s, NAND_LL_CONFIG) & 0xFFF))

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    uint32_t regs[REG_SIZE];

    qemu_irq irq;

    BusState *bus;
    DeviceState *nand[DEV_NUM];

    int dma_cur_page;

    AddressSpace *dma_as;
} SC64ONFIState;

#define SC64_ONFI_PACKET_REGISTERS     14
static const int sc64_onfi_queue_format[] = {
    NAND_CONFIG,
    NAND_DMA_CFG_A,
    NAND_DMA_CFG_B,
    NAND_DATA_BLOCK_PTR,
    NAND_TAG_PTR,
    NAND_DMA,
    NAND_ADDR_REG1,
    NAND_ADDR_REG2,
    NAND_CMD_REG1,
    NAND_CMD_REG2,
    NAND_HWSTATUS_CMD,
    NAND_COMMAND,
};

static void sc64_onfi_write(void *opaque, hwaddr addr, uint64_t val,
                                                                unsigned size);
static void sc64_onfi_reset(DeviceState *dev);

static uint32_t sc64_onfi_process_packet(SC64ONFIState *s)
{
    int offt = 1, pkt_id;
    uint32_t packet_header, tmp;
    uint32_t packet_addr = REG(s, NAND_LL_PTR) +
                           4 * (REG(s, NAND_LL_STATUS) & LL_LEN_MASK);
    int i;

    dma_memory_read(s->dma_as, packet_addr,
            &packet_header, sizeof(uint32_t));

    pkt_id = packet_header >> LL_PKT_ID_OFFT;

    for (i = 0; i < SC64_ONFI_PACKET_REGISTERS; i++) {
        if (packet_header & (1 << i)) {
            dma_memory_read(s->dma_as, packet_addr + offt * 4,
                    &tmp, sizeof(uint32_t));

            sc64_onfi_write(s,
                    sc64_onfi_queue_format[i],
                    tmp, 4);

            offt++;
        }
    }

    if (REG(s, NAND_LL_CONFIG) & LL_WORD_CNT_EN) {
        tmp = REG(s, NAND_LL_STATUS) & LL_LEN_MASK;
        tmp += offt;
        REG(s, NAND_LL_STATUS) &= ~LL_LEN_MASK;
        REG(s, NAND_LL_STATUS) |= tmp & LL_LEN_MASK;

        REG(s, NAND_LL_STATUS) &= ~LL_LEN_LAST_MASK;
        REG(s, NAND_LL_STATUS) |= (offt << LL_LEN_LAST_OFFT) & LL_LEN_LAST_MASK;
    }

    REG(s, NAND_LL_STATUS) &= ~LL_PKT_ID_MASK;
    REG(s, NAND_LL_STATUS) |= pkt_id << LL_PKT_ID_OFFT;

    return packet_addr + 4 * offt;
}

static void sc64_onfi_irq_update(SC64ONFIState *s)
{
    if ((REG(s, NAND_IER) & IER_GIE) &&
            (REG(s, NAND_ISR) & REG(s, NAND_IER) ||
                ((REG(s, NAND_DMA) & DMA_IS_DONE) &&
                 (REG(s, NAND_DMA) & IE_DMA_DONE)))) {
        qemu_irq_raise(s->irq);
    } else {
        qemu_irq_lower(s->irq);
    }
}

static void sc64_onfi_push_bytes(SC64ONFIState *s, int dev, const void *buf,
        int cnt)
{
    const uint8_t *bytes = buf;
    int i;
    for (i = 0; i < cnt; i++) {
        nand_setio(s->nand[dev], bytes[i]);
    }
}

static void sc64_onfi_push_cmd(SC64ONFIState *s, uint32_t cmd)
{
    int i;
    assert(CLE_BYTES(s) <= 4);
    for (i = 0; i < DEV_NUM; i++) {
        if (!s->nand[i]) {
            continue;
        }
        nand_setpins(s->nand[i], 1, 0, !CHIP_ENABLED(s, i), 1, 0);
        sc64_onfi_push_bytes(s, i, &cmd, CLE_BYTES(s));
    }
}

static void sc64_onfi_push_addr(SC64ONFIState *s)
{
    uint64_t addr = ((uint64_t) REG(s, NAND_ADDR_REG1)) |
        (((uint64_t) REG(s, NAND_ADDR_REG2)) << 32);
    int i;

    assert(ALE_BYTES(s) <= 8);

    if (!(REG(s, NAND_COMMAND) & COMMAND_PIO)) {
        /*
         * ONFI defines address for read/write page as follows:
         *   <page[39:16], offset[15:0]>
         */
        addr += (uint64_t)s->dma_cur_page << 16;
    }

    for (i = 0; i < DEV_NUM; i++) {
        if (!s->nand[i]) {
            continue;
        }

        nand_setpins(s->nand[i], 0, 1, !CHIP_ENABLED(s, i), 1, 0);
        sc64_onfi_push_bytes(s, i, &addr, ALE_BYTES(s));
    }
}

static void sc64_onfi_do_transfer(SC64ONFIState *s, int i, bool write)
{
    int w = nand_getbuswidth(s->nand[i]) / 8;
    int j;

    if (write) {
        dbg("Dump data write:");
    } else {
        dbg("Dump data read:");
    }

    for (j = 0; j < (PAGE_BYTES(s) + w - 1) / w; j++) {
        hwaddr addr = REG(s, NAND_DATA_BLOCK_PTR) + j * w +
                                                s->dma_cur_page * PAGE_BYTES(s);
        uint32_t data = 0;

        if (!write) {
            data = nand_getio(s->nand[i]);
        }

        if (REG(s, NAND_COMMAND) & COMMAND_PIO) {
            if (write) {
                data = REG(s, NAND_RESP) >> (8 * j * w);
                data &= (1 << (w * 8)) - 1;
            } else {
                REG(s, NAND_RESP) |= ((uint64_t)data) << (8 * j * w);
            }
        } else if ((REG(s, NAND_DMA) & DMA_GO)) {
            if (write) {
                assert(REG(s, NAND_DMA) & DMA_DIR);
                dma_memory_read(s->dma_as, addr, &data, w);
            } else {
                assert(!(REG(s, NAND_DMA) & DMA_DIR));
                dma_memory_write(s->dma_as, addr, &data, w);
            }
        } else {
            assert(0);
        }

        if (write) {
            nand_setio(s->nand[i], data);
        }

        if (j < 16) {
            dbg_raw(" [0x%x] = %02x", j, data);
        }
    }
    if (PAGE_BYTES(s) > 16) {
        dbg_raw("...\n");
    } else {
        dbg_raw("\n");
    }
}

static void sc64_onfi_transfer(SC64ONFIState *s)
{
    int i;

    if (!(REG(s, NAND_COMMAND) & COMMAND_PIO) && !(REG(s, NAND_DMA) & DMA_GO)) {
        return;
    }

    if (!(REG(s, NAND_COMMAND) & (COMMAND_RX | COMMAND_TX))) {
        return;
    }

    for (i = 0; i < DEV_NUM; i++) {
        if (!s->nand[i]) {
            continue;
        }

        nand_setpins(s->nand[i], 0, 0, !CHIP_ENABLED(s, i), 1, 0);

        if (!CHIP_ENABLED(s, i)) {
            continue;
        }

        sc64_onfi_do_transfer(s, i, REG(s, NAND_COMMAND) & COMMAND_TX);
    }
}

static void sc64_onfi_write(void *opaque, hwaddr addr, uint64_t val,
        unsigned size)
{
    SC64ONFIState *s = opaque;
    uint32_t packet = 0;

    dbg("write %08lx->%02lx\n", val, addr);

    switch (addr) {
    case NAND_IER:
        REG(s, addr) = val;
        if (!(val & IER_GIE)) {
            qemu_irq_lower(s->irq);
        }
        break;
    case NAND_CONTROL:
        if (val & CONTROL_RESET) {
            sc64_onfi_reset(DEVICE(s));
        }

        REG(s, NAND_CONTROL) = 0;
        break;
    case NAND_ISR:
        REG(s, NAND_ISR) &= ~val;
        break;
    case NAND_LL_CONFIG:
        REG(s, addr) = val;

        if (val & LL_START) {
            REG(s, NAND_LL_STATUS) &= ~(LL_LEN_MASK | LL_LEN_LAST_MASK);

            packet = REG(s, NAND_LL_PTR);
            while (packet < REG(s, NAND_LL_PTR) + QUEUE_BYTES(s)) {
                packet = sc64_onfi_process_packet(s);
            }

            REG(s, NAND_ISR) |= IS_LL_DONE;
            REG(s, NAND_FIFO_CTRL) |= LL_BUF_EMPTY;
        }

        REG(s, NAND_LL_CONFIG) &= ~LL_START;
        REG(s, NAND_LL_STATUS) |= IS_LL_DONE;
        break;
    case NAND_FIFO_CTRL:
        if (val & LL_BUF_CLR) {
            dbg("FIFO clear: NIY\n");
        }
        break;
    case NAND_DMA_CFG_A:
    case NAND_DMA_CFG_B:
        val &= 0xFFFF; /* 16-bit registers */
        REG(s, addr) = val;
        break;
    case NAND_DMA:
        if (val & DMA_GO) {
            s->dma_cur_page = 0;
        }
        val &= ~DMA_IS_DONE; /* Read-only bit */
        /* Fall through */
    case NAND_COMMAND:
    case NAND_STATUS:
    case NAND_CONFIG:
    case NAND_TIMING:
    case NAND_RESP:
    case NAND_TIMING2:
    case NAND_CMD_REG1:
    case NAND_CMD_REG2:
    case NAND_ADDR_REG1:
    case NAND_ADDR_REG2:
    case NAND_DATA_BLOCK_PTR:
    case NAND_TAG_PTR:
    case NAND_ECC_PTR:
    case NAND_DEC_STATUS:
    case NAND_HWSTATUS_CMD:
    case NAND_HWSTATUS_MASK:
    case NAND_LL_PTR:
    case NAND_LL_STATUS:
    case NAND_LOCK_CONTROL:
    case NAND_LOCK_STATUS:
    case NAND_LOCK_APER_START(0) ... NAND_LOCK_APER_START(7):
    case NAND_LOCK_APER_END(0) ... NAND_LOCK_APER_END(7):
    case NAND_LOCK_APER_CHIPID(0) ... NAND_LOCK_APER_CHIPID(7):
    case NAND_BCH_CONFIG:
    case NAND_BCH_DEC_RESULT:
    case NAND_BCH_DEC_STATUS_BUF:
    case NAND_CONFIG2:
    case NAND_VERSION:
    case NAND_DATE:
        REG(s, addr) = val;
        break;
    default:
        /* Unknown register */
        warn_report("Writing unknown register: %02lx", addr);
        break;
    }

    if (REG(s, NAND_COMMAND) & COMMAND_GO) {
        dbg("Do cmd: CLE=%d (%d) ALE=%d (%d) TRANS=%d (%d) MODE=%s\n",
                REG(s, NAND_COMMAND) & COMMAND_CLE ? 1 : 0,
                CLE_BYTES(s),
                REG(s, NAND_COMMAND) & COMMAND_ALE ? 1 : 0,
                ALE_BYTES(s),
                REG(s, NAND_COMMAND) & COMMAND_RX ? 1 : 0,
                PAGE_BYTES(s),
                REG(s, NAND_COMMAND) & COMMAND_PIO ? "PIO" : "DMA");

        if (!(REG(s, NAND_COMMAND) & COMMAND_PIO)) {
            dbg("DMA RAM addr=0x%x\n", REG(s, NAND_DATA_BLOCK_PTR));
        } else {
            s->dma_cur_page = 0;
            if (REG(s, NAND_COMMAND) & COMMAND_RX) {
                REG(s, NAND_RESP) = 0;
            }
        }

        if (REG(s, NAND_COMMAND) & COMMAND_CLE) {
            sc64_onfi_push_cmd(s, REG(s, NAND_CMD_REG1));
        }

        if (REG(s, NAND_COMMAND) & COMMAND_ALE) {
            sc64_onfi_push_addr(s);
        }

        if ((REG(s, NAND_COMMAND) & COMMAND_CLE) &&
                (REG(s, NAND_COMMAND) & COMMAND_SEC_CMD) &&
                !(REG(s, NAND_COMMAND) & COMMAND_AFT_DAT)) {
            sc64_onfi_push_cmd(s, REG(s, NAND_CMD_REG2));
        }

        sc64_onfi_transfer(s);

        if ((REG(s, NAND_COMMAND) & COMMAND_CLE) &&
            (REG(s, NAND_COMMAND) & COMMAND_SEC_CMD) &&
                (REG(s, NAND_COMMAND) & COMMAND_AFT_DAT)) {
            sc64_onfi_push_cmd(s, REG(s, NAND_CMD_REG2));
        }

        if (!(REG(s, NAND_COMMAND) & COMMAND_PIO)) {
            s->dma_cur_page++;
            int bytes_transfered = s->dma_cur_page * PAGE_BYTES(s);
            if ((REG(s, NAND_DMA) & DMA_GO) &&
                    (bytes_transfered > REG(s, NAND_DMA_CFG_A))) {
                REG(s, NAND_DMA) &= ~DMA_GO;
                REG(s, NAND_DMA) |= DMA_IS_DONE;
                s->dma_cur_page = 0;
            }
        }

        REG(s, NAND_COMMAND) &= ~COMMAND_GO;

        REG(s, NAND_ISR) |= IER_IE_CMD_DONE;
    }


    sc64_onfi_irq_update(s);
}

static uint64_t sc64_onfi_read(void *opaque, hwaddr addr, unsigned size)
{
    SC64ONFIState *s = opaque;
    uint64_t res = s->regs[REG_IDX(addr)];

    dbg("read from %02lx = %08lx\n", addr, res);

    return res;
}

static const MemoryRegionOps sc64_onfi_mem_ops = {
    .read = sc64_onfi_read,
    .write = sc64_onfi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static int sc64_onfi_walker(DeviceState *dev, void *opaque)
{
    SC64ONFIState *s = opaque;
    int cs;

    if (dev->id == NULL) {
        error_report("Can't attach device with null id");
        return -1;
    }

    if (EOF == sscanf(dev->id, "nand%i", &cs)) {
        error_report("Can't parse device id: %s, should be \"nandXX\"",
                dev->id);
        return -1;
    }

    if (cs >= DEV_NUM) {
        error_report("Device id is too big: %s, should be less than %d",
                dev->id, DEV_NUM);
        return -1;
    }

    s->nand[cs] = dev;

    return 0;
}

static void sc64_onfi_realize(DeviceState *dev, Error **errp)
{
    SC64ONFIState *nc = SC64_ONFI(dev);
    SysBusDevice *s = SYS_BUS_DEVICE(dev);

    nc->bus = qbus_create(TYPE_NAND_BUS, dev, "sc64-onfi-bus");

    memory_region_init_io(&nc->iomem, OBJECT(s),
            &sc64_onfi_mem_ops, nc,
            "sc64-onfi-regs", sizeof(nc->regs));

    sysbus_init_mmio(s, &nc->iomem);
    sysbus_init_irq(s, &nc->irq);
}

static void sc64_onfi_reset(DeviceState *dev)
{
    SC64ONFIState *s = SC64_ONFI(dev);
    int i;

    dbg("reset %p\n", s);

    memset(s->regs, 0, REG_SIZE * sizeof(s->regs[0]));

    REG(s, NAND_COMMAND) = (0x8 << 20) | 4;
    REG(s, NAND_STATUS) = STATUS_ISEMPTY;
    REG(s, NAND_VERSION) = 0xdeadbeef;
    REG(s, NAND_DATE) = 0xcafebabe;
    REG(s, NAND_CONFIG) = 0x3 << 16;

    /* Attach chip selects */
    qbus_walk_children(s->bus, sc64_onfi_walker, NULL, NULL, NULL, s);

    for (i = 0; i < DEV_NUM; i++) {
        if (s->nand[i] != NULL) {
            break;
        }
    }

    if (i == DEV_NUM) {
        /* Attach default ONFI flash */
        s->nand[0] = nand_init_onfi(NULL, "MT29F1G08ABBDA");
        assert(s->nand[0]);
        s->nand[0]->id = "nand0";
        qdev_set_parent_bus(s->nand[0], s->bus);
    }

    for (i = 0; i < DEV_NUM; i++) {
        if (!s->nand[i]) {
            continue;
        }
        nand_setpins(s->nand[i], 0, 0, 1, 1, 0);
    }

    return;
}

DeviceState *sc64_onfi_register(AddressSpace *dma_as)
{
    DeviceState *dev;
    SC64ONFIState *nc;

    dev = qdev_create(NULL, TYPE_SC64_ONFI);
    dev->id = TYPE_SC64_ONFI;

    nc = SC64_ONFI(dev);
    nc->dma_as = dma_as;
    qdev_init_nofail(dev);

    return dev;
}

static void sc64_onfi_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->desc = "SC64 NAND (ONFI)";
    dc->reset = sc64_onfi_reset;
    dc->realize = sc64_onfi_realize;
}

static const TypeInfo sc64_onfi_sysbus_info = {
    .name          = TYPE_SC64_ONFI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SC64ONFIState),
    .class_init    = sc64_onfi_sysbus_class_init,
};

static void sc64_onfi_register_types(void)
{
    type_register_static(&sc64_onfi_sysbus_info);
}

type_init(sc64_onfi_register_types)
