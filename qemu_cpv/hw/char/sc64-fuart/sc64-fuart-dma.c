/*
 * FastUART emulation
 *
 * Copyright (C) 2018 Alexander Kalmuk <alexkalmuk@gmail.com>
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
#include "hw/sysbus.h"
#include "sysemu/dma.h"
#include "sysemu/sysemu.h"
#include "hw/char/serial.h"
#include "exec/address-spaces.h"
#include "qemu/fifo8.h"
#include "qemu/timer.h"
#include "hw/char/sc64-fuart.h"
#include "sc64-fuart-internal.h"

/* #define DEBUG */

#ifdef DEBUG
#define DPRINTF(fmt, ...) \
    do { printf("sc64-fuart: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

#define MAKE_WMASK(X, wmask) \
    static const uint64_t SC64_FUART_##X##_WMASK = wmask;

#define WMASK(X) SC64_FUART_##X##_WMASK

#define FIELD_SHIFT_MASK(X, shift, mask) \
    static const uint64_t X##_SHIFT = shift; \
    static const uint64_t X##_MASK = mask;

#define REG(s, reg) \
    (s->dma_regs[SC64_FUART_##reg >> 3])

#define REG_FIELD(s, reg, field) \
    ((s->dma_regs[SC64_FUART_##reg >> 3] >> field##_SHIFT) & field##_MASK)

#define SET_REG_FIELD(s, reg, field, val) \
    s->dma_regs[SC64_FUART_##reg >> 3] = \
        (s->dma_regs[SC64_FUART_##reg >> 3] &  \
        ~(field##_MASK << field##_SHIFT)) |     \
        ((val) & field##_MASK) << field##_SHIFT;

#define SC64_FUART_INTERRUPT_EN    0x8
#define SC64_FUART_INTERRUPT       0x10
# define SC64_FUART_INTERRUPT_RX_MBT  (1 << 0)
# define SC64_FUART_INTERRUPT_TX_CL   (1 << 1)
# define SC64_FUART_INTERRUPT_RX_MBF  (1 << 2)
# define SC64_FUART_INTERRUPT_TX_RD   (1 << 3)
# define SC64_FUART_INTERRUPT_RX_ERB  (1 << 4)
# define SC64_FUART_INTERRUPT_CTS_DA  (1 << 5)
#define SC64_FUART_RX_BASE         0x18
#define SC64_FUART_RX_SIZE         0x20
#define SC64_FUART_RX_HEAD_PTR     0x28
#define SC64_FUART_RX_TAIL_PTR     0x30
#define SC64_FUART_RX_STATUS       0x38
FIELD_SHIFT_MASK(BNUM_COUNT, 32, 0xFFFFFFFF)
FIELD_SHIFT_MASK(RX_FIFO_FULL, 0, 0x7FF)
#define SC64_FUART_RX_CONTROL      0x40
FIELD_SHIFT_MASK(MEM_FIFO_TIMEOUT, 51, 0xFFF)
FIELD_SHIFT_MASK(RX_BCOUNT_MODE, 50, 0x1)
FIELD_SHIFT_MASK(MEM_FIFO_THRESHOLD, 30, 0xFFFFF)
FIELD_SHIFT_MASK(RX_FIFO_TIMEOUT, 14, 0xFFFF)
FIELD_SHIFT_MASK(RX_FIFO_THRESHOLD, 3, 0x7FF)
FIELD_SHIFT_MASK(RX_RESET, 1, 0x1)
FIELD_SHIFT_MASK(RX_READY, 0, 0x1)
#define SC64_FUART_TX_BASE         0x48
#define SC64_FUART_TX_STATUS       0x50
FIELD_SHIFT_MASK(TX_BYTES_COUNT, 32, 0xFFFFFFFF)
FIELD_SHIFT_MASK(TX_DESCR_COUNT, 16, 0xFFFF)
#define SC64_FUART_TX_CONTROL      0x58
FIELD_SHIFT_MASK(TX_RUN, 0, 0x1)
FIELD_SHIFT_MASK(TX_STOP, 1, 0x1)
FIELD_SHIFT_MASK(TX_RESET, 2, 0x1)
# define SC64_FUART_TX_CONTROL_RUN  (1 << 0)
# define SC64_FUART_TX_CONTROL_STOP (1 << 1)
#define SC64_FUART_FLOW_CONTROL    0x60
FIELD_SHIFT_MASK(FLOW_MODE, 0, 0x3)
FIELD_SHIFT_MASK(CTS_TIMEOUT, 16, 0xFFFF)

MAKE_WMASK(INTERRUPT_EN, 0x3F)
MAKE_WMASK(RX_BASE,      0xFFFFFFFFFULL)
MAKE_WMASK(RX_SIZE,      0xFF000)
MAKE_WMASK(RX_TAIL_PTR,  0xFFFFFFFFFULL)
MAKE_WMASK(RX_CONTROL,  (0xFFFFFFFFULL << 32) | 0xFFFFFFFFFULL)
MAKE_WMASK(TX_BASE,      0xFFFFFFFC)
MAKE_WMASK(TX_STATUS,    0xFFF)
MAKE_WMASK(TX_CONTROL,   0x7)
MAKE_WMASK(FLOW_CONTROL, 0xFFFF000F)

#define SC64_FUART_DESCR_OWN     0x1
#define SC64_FUART_DESCR_COUNT   0xFFE
#define SC64_FUART_DESCR_BUFFER  0xFFFFFFF8
#define SC64_FUART_DESCR_NEXT    0xFFFFFFF8

#define FLOW_MODE_SOFT 0
#define FLOW_MODE_HARD 1

#define SC64_FUART_TX_TIMEOUT 1000000

typedef struct Sc64FuartDescr {
    uint8_t own;
    uint16_t count;
    uint64_t buffer;
    uint64_t next;
} Sc64FuartDescr;

static void fuart_dma_write_reg(Sc64FuartState *s, int addr,
    uint64_t wmask, uint64_t val, unsigned size)
{
    uint64_t read_only_val;
    int reg = addr & ~0x7;
    int shift = addr & 0x7;

    wmask &= MAKE_64BIT_MASK(shift * 8, size * 8);
    read_only_val = s->dma_regs[reg >> 3] & ~wmask;
    val <<= 8 * shift;
    s->dma_regs[reg >> 3] = read_only_val | (val & wmask);
}

static void fuart_dma_read_descr(Sc64FuartState *s, uint64_t addr,
                                   Sc64FuartDescr *descr)
{
    uint64_t p[3];

    dma_memory_read(s->dma_as, addr, p, sizeof *descr);

    descr->own    = p[0] & SC64_FUART_DESCR_OWN;
    descr->count  = ((p[0] & SC64_FUART_DESCR_COUNT) >> 1) + 1;
    descr->buffer = p[1] & SC64_FUART_DESCR_BUFFER;
    descr->next   = p[2] & SC64_FUART_DESCR_NEXT;
}

static void fuart_dma_write_descr(Sc64FuartState *s, uint64_t addr,
                                   Sc64FuartDescr *descr)
{
    uint64_t p[3];

    p[0] = descr->own | ((descr->count - 1) << 1);
    p[1] = descr->buffer;
    p[2] = descr->next;

    dma_memory_write(s->dma_as, addr, p, sizeof *descr);
}

static void fuart_dma_rx_reset(Sc64FuartState *s)
{
    fifo8_reset(&s->modem->recv_fifo);

    timer_del(s->rx_timer);
    timer_del(s->mem_fifo_timer);

    memset(&s->rx_mem_fifo, 0, sizeof(s->rx_mem_fifo));
    s->recv_fifo_timeout = 0;
    s->mem_fifo_timeout = 0;

    REG(s, RX_BASE) = 0;
    REG(s, RX_SIZE) = 0xFFF;
    REG(s, RX_STATUS) = 0;
    REG(s, RX_CONTROL) = 0;
}

static void fuart_dma_tx_reset(Sc64FuartState *s)
{
    fifo8_reset(&s->modem->xmit_fifo);

    REG(s, TX_BASE) = 0;
    REG(s, TX_STATUS) = 0;
    REG(s, TX_CONTROL) = 0;
}

static inline int fuart_dma_mem_fifo_size(Sc64FuartState *s)
{
    uint32_t size = s->rx_mem_fifo.size;
    int num = s->rx_mem_fifo.head_ptr - s->rx_mem_fifo.tail_ptr;

    if (num < 0) {
        num += size;
    }
    return num;
}

static void fuart_dma_update_irq(Sc64FuartState *s)
{
    uint32_t irq_mask = REG(s, INTERRUPT_EN) & s->requested_irq;
    REG(s, INTERRUPT) = irq_mask;
    qemu_set_irq(s->irq, !!irq_mask);
}

static void fuart_dma_mem_fifo_timer_hnd(Sc64FuartState *s)
{
    timer_del(s->rx_timer);

    s->requested_irq |= SC64_FUART_INTERRUPT_RX_MBT;
    fuart_dma_update_irq(s);

    timer_mod(s->mem_fifo_timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + s->mem_fifo_timeout);
}

static void fuart_dma_push_recv_fifo(Sc64FuartState *s, const uint8_t *buf,
    int size)
{
    uint32_t num_used;
    SerialState *m = s->modem;

    assert(size <= fifo8_num_free(&m->recv_fifo));

    fifo8_push_all(&m->recv_fifo, buf, size);

    num_used = fifo8_num_used(&m->recv_fifo);
    SET_REG_FIELD(s, RX_STATUS, RX_FIFO_FULL, num_used);
}

static void fuart_dma_pop_all_recv_fifo(Sc64FuartState *s)
{
    SerialState *m = s->modem;
    uint64_t base = s->rx_mem_fifo.base;
    uint32_t size = s->rx_mem_fifo.size;
    uint32_t num = 0, received = 0, ret_num = 0;
    int rts_level = 0;
    const uint8_t *buf;
    uint32_t bcount;

    if (!REG_FIELD(s, RX_CONTROL, RX_READY)) {
        return;
    }

    num = fifo8_num_used(&m->recv_fifo);
    if (!num) {
        return;
    }

    do {
        buf = fifo8_pop_buf(&m->recv_fifo, num - received, &ret_num);
        dma_memory_write(s->dma_as, s->rx_mem_fifo.head_ptr + received,
                            buf, ret_num);
        received += ret_num;
    } while (received != num);

    s->rx_mem_fifo.head_ptr += num;

    if (s->rx_mem_fifo.head_ptr > base + size) {
        s->rx_mem_fifo.head_ptr -= size;
    }
    SET_REG_FIELD(s, RX_STATUS, RX_FIFO_FULL,
                  fifo8_num_used(&m->recv_fifo));

    if (fuart_dma_mem_fifo_size(s) >
            REG_FIELD(s, RX_CONTROL, MEM_FIFO_THRESHOLD)) {
        s->requested_irq |= SC64_FUART_INTERRUPT_RX_MBF;
        rts_level = 1;
    }

    if (0 == REG_FIELD(s, RX_CONTROL, RX_BCOUNT_MODE)) {
        bcount = REG_FIELD(s, RX_STATUS, BNUM_COUNT);
        SET_REG_FIELD(s, RX_STATUS, BNUM_COUNT, bcount + num);
    }

    if (REG_FIELD(s, FLOW_CONTROL, FLOW_MODE) == FLOW_MODE_HARD) {
        /* Update RTS/CTS if MEM_THRESHOLD is exceeded. */
        if (rts_level) {
            m->mcr |= UART_MCR_RTS;
        } else {
            m->mcr &= ~UART_MCR_RTS;
        }
    }
    fuart_modem_update_tiocm(s);

    fuart_dma_update_irq(s);
}

static void fuart_dma_rx_timer_hnd(Sc64FuartState *s)
{
    timer_del(s->rx_timer);
    fuart_dma_pop_all_recv_fifo(s);
    timer_mod(s->rx_timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + s->recv_fifo_timeout);
}

int fuart_dma_can_receive(void *opaque)
{
    Sc64FuartState *s = opaque;
    return fifo8_num_free(&s->modem->recv_fifo);
}

void fuart_dma_receive(void *opaque, const uint8_t *buf, int size)
{
    Sc64FuartState *s = opaque;
    Sc64FuartState *peer = sc64_fuart_ports[!s->port_nr];
    SerialState *m = s->modem;
    int num_used;

    SET_REG_FIELD(s, RX_CONTROL, RX_READY, 1);

    fuart_dma_push_recv_fifo(s, buf, size);

    /* TODO Check parity mismatch in another way */
    if (peer->settings.parity != s->settings.parity) {
        s->requested_irq |= SC64_FUART_INTERRUPT_RX_ERB;
        fuart_dma_update_irq(s);
        DPRINTF(">>> parity error\n");
    }

    num_used = fifo8_num_used(&m->recv_fifo);
    if (num_used >= REG_FIELD(s, RX_CONTROL, RX_FIFO_THRESHOLD)) {
        fuart_dma_pop_all_recv_fifo(s);
    }
}

static void fuart_dma_tx_descr_close(Sc64FuartState *s, uint64_t addr)
{
    Sc64FuartDescr descr;
    uint32_t tx_descr_count, tx_bytes_count;

    fuart_dma_read_descr(s, addr, &descr);
    descr.own = 0;
    fuart_dma_write_descr(s, addr, &descr);

    tx_bytes_count = REG_FIELD(s, TX_STATUS, TX_BYTES_COUNT);
    tx_descr_count = REG_FIELD(s, TX_STATUS, TX_DESCR_COUNT);
    SET_REG_FIELD(s, TX_STATUS, TX_BYTES_COUNT,
                  tx_bytes_count + descr.count);
    SET_REG_FIELD(s, TX_STATUS, TX_DESCR_COUNT, tx_descr_count + 1);

    s->requested_irq |= SC64_FUART_INTERRUPT_TX_CL;
    fuart_dma_update_irq(s);
}

static int fuart_dma_tx_descr(Sc64FuartState *s, uint64_t next)
{
    Sc64FuartDescr descr;

    fuart_dma_read_descr(s, next, &descr);
    if (!descr.own) {
        s->requested_irq |= SC64_FUART_INTERRUPT_TX_RD;
        fuart_dma_update_irq(s);
        SET_REG_FIELD(s, TX_CONTROL, TX_RUN, 0);
        return 1;
    }

    dma_memory_read(s->dma_as, descr.buffer, s->txb.buf, descr.count);
    s->txb.data = s->txb.buf;
    s->txb.len = descr.count;

    fuart_modem_start_xmit(s);
    return 0;
}

static void fuart_dma_tx_timer_hnd(Sc64FuartState *s)
{
    SerialState *modem = s->modem;
    Sc64FuartDescr descr;

    timer_del(s->tx_timer);

    if (!(modem->lsr & UART_LSR_TEMT)) {
        goto out_tick;
    }
    fuart_dma_read_descr(s, s->cur_tx_descr_addr, &descr);

    fuart_dma_tx_descr_close(s, s->cur_tx_descr_addr);

    if (!REG_FIELD(s, TX_CONTROL, TX_STOP)) {
        s->cur_tx_descr_addr = descr.next;
        if (fuart_dma_tx_descr(s, s->cur_tx_descr_addr) != 0) {
            /* Last descriptor handled */
            return;
        }
    }

out_tick:
    timer_mod(s->tx_timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + SC64_FUART_TX_TIMEOUT);
}

static void fuart_dma_cts_timer_hnd(Sc64FuartState *s)
{
    SerialState *m = s->modem;

    timer_del(s->cts_timer);
    if (m->msr & UART_MSR_DCTS) {
        s->requested_irq |= SC64_FUART_INTERRUPT_CTS_DA;
        fuart_dma_update_irq(s);
    }
    timer_mod(s->cts_timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + s->cts_signal_timeout);
}

static void fuart_dma_tx(Sc64FuartState *s)
{
    DPRINTF("fuart_dma_tx %lx (port = %x)\n", s->cur_tx_descr_addr, s->port_nr);

    if (fuart_dma_tx_descr(s, s->cur_tx_descr_addr) != 0) {
        /* Last descriptor handled */
        return;
    }

    timer_mod(s->tx_timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + SC64_FUART_TX_TIMEOUT);
}

static uint64_t fuart_dma_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64FuartState *s = opaque;
    uint32_t index = addr & ~0x7;
    uint64_t ret = 0;
    int shift = addr & 0x7;
    uint64_t mask = MAKE_64BIT_MASK(shift * 8, size * 8);

    switch (index) {
    case SC64_FUART_RX_BASE:
        ret = s->rx_mem_fifo.base;
        break;
    case SC64_FUART_RX_SIZE:
        ret = s->rx_mem_fifo.size;
        break;
    case SC64_FUART_RX_HEAD_PTR:
        ret = s->rx_mem_fifo.head_ptr;
        break;
    case SC64_FUART_RX_TAIL_PTR:
        ret = s->rx_mem_fifo.tail_ptr;
        break;
    case SC64_FUART_INTERRUPT:
        ret = s->dma_regs[index >> 3];
        REG(s, INTERRUPT) = 0;
        s->requested_irq = 0;
        fuart_dma_update_irq(s);
        break;
    case SC64_FUART_RX_STATUS:
    case SC64_FUART_RX_CONTROL:
    case SC64_FUART_TX_STATUS:
    case SC64_FUART_TX_CONTROL:
    case SC64_FUART_FLOW_CONTROL:
        ret = s->dma_regs[index >> 3];
        break;
    default:
        printf("sc64-fuart: Unknown read at address 0x%lx\n", addr);
        break;
    }

    DPRINTF("fuart-read: addr = 0x%lx, index=0x%x, ret=0x%lx\n", addr, index,
            (ret & mask) >> (8 * shift));

    return (ret & mask) >> (8 * shift);
}

static void fuart_dma_write(void *opaque, hwaddr addr,
                              uint64_t val, unsigned size)
{
    Sc64FuartState *s = opaque;
    uint32_t index = addr & ~0x7;

    DPRINTF("fuart-write: addr = 0x%lx, index=0x%x, val=0x%lx\n",
            addr, index, val);

    switch (index) {
    case SC64_FUART_INTERRUPT_EN:
        fuart_dma_write_reg(s, addr, WMASK(INTERRUPT_EN), val, size);
        break;
    case SC64_FUART_RX_BASE:
        fuart_dma_write_reg(s, addr, WMASK(RX_BASE), val, size);
        s->rx_mem_fifo.base = REG(s, RX_BASE);
        s->rx_mem_fifo.head_ptr = s->rx_mem_fifo.tail_ptr
                                = s->rx_mem_fifo.base;
        break;
    case SC64_FUART_RX_SIZE:
        fuart_dma_write_reg(s, addr, WMASK(RX_SIZE), val, size);
        s->rx_mem_fifo.size = REG(s, RX_SIZE) + 1;
        break;
    case SC64_FUART_RX_TAIL_PTR:
        fuart_dma_write_reg(s, addr, WMASK(RX_TAIL_PTR), val, size);
        s->rx_mem_fifo.tail_ptr = REG(s, RX_TAIL_PTR);
        fuart_dma_update_irq(s);
        break;
    case SC64_FUART_RX_CONTROL:
        fuart_dma_write_reg(s, addr, WMASK(RX_CONTROL), val, size);

        if (REG_FIELD(s, RX_CONTROL, RX_RESET)) {
            fuart_dma_rx_reset(s);
            break;
        }
        s->recv_fifo_timeout = REG_FIELD(s, RX_CONTROL, RX_FIFO_TIMEOUT) *
                               s->modem->char_transmit_time;
        if (s->recv_fifo_timeout) {
            timer_mod(s->rx_timer,
                qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + s->recv_fifo_timeout);
        }
        s->mem_fifo_timeout = REG_FIELD(s, RX_CONTROL, MEM_FIFO_TIMEOUT) *
                              s->modem->char_transmit_time;
        if (s->mem_fifo_timeout) {
            timer_mod(s->mem_fifo_timer,
                qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + s->mem_fifo_timeout);
        }
        break;
    case SC64_FUART_TX_BASE:
        fuart_dma_write_reg(s, addr, WMASK(TX_BASE), val, size);
        s->cur_tx_descr_addr = REG(s, TX_BASE);
        break;
    case SC64_FUART_TX_STATUS:
        fuart_dma_write_reg(s, addr, WMASK(TX_STATUS), val, size);
        break;
    case SC64_FUART_TX_CONTROL:
        fuart_dma_write_reg(s, addr, WMASK(TX_CONTROL), val, size);

        if (REG_FIELD(s, TX_CONTROL, TX_RESET)) {
            fuart_dma_tx_reset(s);
        }
        if (val & SC64_FUART_TX_CONTROL_RUN) {
            fuart_dma_tx(s);
        }
        break;
    case SC64_FUART_FLOW_CONTROL:
        fuart_dma_write_reg(s, addr, WMASK(FLOW_CONTROL), val, size);

        if (REG_FIELD(s, FLOW_CONTROL, FLOW_MODE) == FLOW_MODE_HARD) {
            s->cts_signal_timeout = REG_FIELD(s, FLOW_CONTROL, CTS_TIMEOUT) *
                             s->modem->char_transmit_time;
            if (s->cts_signal_timeout) {
                timer_mod(s->cts_timer,
                    qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                    s->cts_signal_timeout);
            }
        }
        break;
    default:
        printf("sc64-fuart: Unknown write at address 0x%lx\n", addr);
        break;
    }
}

static const MemoryRegionOps fuart_dma_mem_ops = {
    .read = fuart_dma_read,
    .write = fuart_dma_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void fuart_dma_reset(DeviceState *d)
{
    Sc64FuartState *s = SC64_FUART(d);

    memset(s->dma_regs, 0, sizeof s->dma_regs);
    REG(s, RX_SIZE) = 0xFFF;
    s->requested_irq = 0;
    s->signals = 0;

    s->rx_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                              (QEMUTimerCB *) fuart_dma_rx_timer_hnd, s);
    s->mem_fifo_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                              (QEMUTimerCB *) fuart_dma_mem_fifo_timer_hnd,
                              s);
    s->tx_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                              (QEMUTimerCB *) fuart_dma_tx_timer_hnd, s);
    s->cts_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                              (QEMUTimerCB *) fuart_dma_cts_timer_hnd, s);
    fuart_dma_rx_reset(s);
    fuart_dma_tx_reset(s);
}

static int fuart_dma_init(SysBusDevice *dev)
{
    Sc64FuartState *s = SC64_FUART(dev);

    sysbus_init_irq(dev, &s->irq);

    memory_region_init_io(&s->dma_mem, OBJECT(s), &fuart_dma_mem_ops, s,
                          TYPE_SC64_FUART, 0x100);
    sysbus_init_mmio(dev, &s->dma_mem);

    return 0;
}

static void fuart_dma_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = fuart_dma_init;
    dc->desc = "SC64 FastUART";
    dc->reset = fuart_dma_reset;
}

static const TypeInfo fuart_dma_sysbus_info = {
    .name          = TYPE_SC64_FUART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64FuartState),
    .class_init    = fuart_dma_sysbus_class_init,
};

static void fuart_dma_register_types(void)
{
    type_register_static(&fuart_dma_sysbus_info);
}

type_init(fuart_dma_register_types);
