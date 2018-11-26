/*
 * QEMU 16550A UART emulation
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 * Copyright (c) 2008 Citrix Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
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
#include "hw/char/serial.h"
#include "chardev/char-serial.h"
#include "qapi/error.h"
#include "qemu/timer.h"
#include "exec/address-spaces.h"
#include "qemu/error-report.h"
#include "hw/char/sc64-fuart.h"
#include "sc64-fuart-internal.h"

/* #define DEBUG_SERIAL */

/* TODO Expose port settings to the external world.
 * Currently, we access those settings directly. */
#define FUART_SETTINGS_WORKAROUND

#define MAX_XMIT_RETRY      4

#ifdef DEBUG_SERIAL
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "Sc64 fuart modem: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while (0)
#endif

Sc64FuartState *sc64_fuart_ports[SC64_FUART_PORTS_NR];

static void fuart_modem_xmit(Sc64FuartState *ss);

static void fuart_modem_update_parameters(Sc64FuartState *ss)
{
    SerialState *s = ss->modem;
    int speed, parity, data_bits, stop_bits, frame_size;
    QEMUSerialSetParams ssp;

    if (s->divider == 0 || s->divider > s->baudbase) {
        return;
    }

    /* Start bit. */
    frame_size = 1;
    if (s->lcr & 0x08) {
        /* Parity bit. */
        frame_size++;
        if (s->lcr & 0x10) {
            parity = 'E';
        } else {
            parity = 'O';
        }
    } else {
            parity = 'N';
    }
    if (s->lcr & 0x04) {
        stop_bits = 2;
    } else {
        stop_bits = 1;
    }

    data_bits = (s->lcr & 0x03) + 5;
    frame_size += data_bits + stop_bits;
    speed = s->baudbase / s->divider;
    ssp.speed = speed;
    ssp.parity = parity;
    ssp.data_bits = data_bits;
    ssp.stop_bits = stop_bits;
    s->char_transmit_time =  (NANOSECONDS_PER_SECOND / speed) * frame_size;

#ifdef FUART_SETTINGS_WORKAROUND
    memcpy(&ss->settings, &ssp, sizeof ssp);
#else
    qemu_chr_fe_ioctl(&s->chr, CHR_IOCTL_SERIAL_SET_PARAMS, &ssp);
#endif

    DPRINTF("speed=%d parity=%c data=%d stop=%d\n",
           speed, parity, data_bits, stop_bits);
}

static void fuart_modem_update_msl(Sc64FuartState *ss)
{
    SerialState *s = ss->modem;
    uint8_t omsr;
    int flags = 0;

    if (s->mcr & UART_MCR_LOOP) {
        return;
    }

    timer_del(s->modem_status_poll);

#ifdef FUART_SETTINGS_WORKAROUND
    flags = ss->signals;
#else
    /* Connection with tty, pty and so on */
    if (qemu_chr_fe_ioctl(&s->chr, CHR_IOCTL_SERIAL_GET_TIOCM,
                          &flags) == -ENOTSUP) {
        timer_mod(s->modem_status_poll,
                    qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                    NANOSECONDS_PER_SECOND / 100);
        return;
    }
#endif

    omsr = s->msr;

    s->msr = (flags & CHR_TIOCM_CTS)
                ? s->msr | UART_MSR_CTS
                : s->msr & ~UART_MSR_CTS;

    s->msr = (flags & CHR_TIOCM_DSR)
                ? s->msr | UART_MSR_DSR
                : s->msr & ~UART_MSR_DSR;

    s->msr = (flags & CHR_TIOCM_CAR)
                ? s->msr | UART_MSR_DCD
                : s->msr & ~UART_MSR_DCD;

    s->msr = (flags & CHR_TIOCM_RI)
                ? s->msr | UART_MSR_RI
                : s->msr & ~UART_MSR_RI;

    if (s->msr != omsr) {
        /* Set delta bits */
        s->msr = s->msr | ((s->msr >> 4) ^ (omsr >> 4));
        /* UART_MSR_TERI only if change was from 1 -> 0 */
        if ((s->msr & UART_MSR_TERI) && !(omsr & UART_MSR_RI)) {
            s->msr &= ~UART_MSR_TERI;
        }
    }

    /* The real 16550A apparently has a 250ns response latency
     * to line status changes. We'll be lazy and poll only every 10ms,
     * and only poll it at all if MSI interrupts are turned on */

    if (s->poll_msl) {
        timer_mod(s->modem_status_poll, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                  NANOSECONDS_PER_SECOND / 100);
    }
}

static gboolean fuart_modem_watch_cb(GIOChannel *chan, GIOCondition cond,
                                void *opaque)
{
    Sc64FuartState *ss = opaque;
    SerialState *s = ss->modem;

    s->watch_tag = 0;
    fuart_modem_xmit(ss);
    return FALSE;
}

static uint8_t fuart_byte_mask(SerialState *s)
{
    int data_bits = (s->lcr & 0x03) + 5;
    assert(data_bits >= 5 && data_bits <= 8);
    return (1 << data_bits) - 1;
}

void fuart_modem_start_xmit(Sc64FuartState *ss)
{
    SerialState *s = ss->modem;

#ifdef FUART_SETTINGS_WORKAROUND
    uint8_t mask = 0xFF;
    int peer_port = !ss->port_nr;
    int i;

    mask &= fuart_byte_mask(s);
    mask &= fuart_byte_mask(sc64_fuart_ports[peer_port]->modem);
    for (i = 0; i < ss->txb.len; i++) {
        ss->txb.buf[i] &= mask;
    }
#endif

    fuart_modem_xmit(ss);
}

static void fuart_modem_xmit(Sc64FuartState *ss)
{
    SerialState *s = ss->modem;
    int transmitted;

    s->lsr &= ~UART_LSR_TEMT;

    if (s->mcr & UART_MCR_LOOP) {
        /* We send only data amount that can be received by the peer.
         * The rest will be dropped. */
        transmitted = MIN(ss->txb.len, fuart_dma_can_receive(ss));
        fuart_dma_receive(ss, ss->txb.data, transmitted);
    } else {
        transmitted = qemu_chr_fe_write(&s->chr, ss->txb.data, ss->txb.len);
        ss->txb.data += transmitted;
        ss->txb.len -= transmitted;

        if (transmitted != ss->txb.len && s->tsr_retry < MAX_XMIT_RETRY) {
            assert(s->watch_tag == 0);

            s->watch_tag =
                qemu_chr_fe_add_watch(&s->chr, G_IO_OUT | G_IO_HUP,
                                      fuart_modem_watch_cb, ss);
            if (s->watch_tag > 0) {
                s->tsr_retry++;
                return;
            }
        }
    }
    s->tsr_retry = 0;

    s->last_xmit_ts = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    s->lsr |= UART_LSR_TEMT;
}

void fuart_modem_update_tiocm(Sc64FuartState *ss)
{
    SerialState *s = ss->modem;
    int flags;
    int peer_port = !ss->port_nr;

#ifdef FUART_SETTINGS_WORKAROUND
    flags = ss->signals;
#else
    qemu_chr_fe_ioctl(&s->chr, CHR_IOCTL_SERIAL_GET_TIOCM, &flags);
#endif

    flags &= ~(CHR_TIOCM_RTS | CHR_TIOCM_DTR);

    if (s->mcr & UART_MCR_RTS) {
        flags |= CHR_TIOCM_RTS;
    }
    if (s->mcr & UART_MCR_DTR) {
        flags |= CHR_TIOCM_DTR;
    }

#ifdef FUART_SETTINGS_WORKAROUND
    ss->signals = flags;
    if (flags & CHR_TIOCM_RTS) {
        sc64_fuart_ports[peer_port]->signals |= CHR_TIOCM_CTS;
    } else {
        sc64_fuart_ports[peer_port]->signals &= ~CHR_TIOCM_CTS;
    }
#else
    qemu_chr_fe_ioctl(&s->chr, CHR_IOCTL_SERIAL_SET_TIOCM, &flags);
#endif
}

static void fuart_modem_update_tiocm_loop(SerialState *s)
{
    int omsr, flags = 0;

    if (s->mcr & UART_MCR_RTS) {
        flags |= UART_MSR_CTS;
    }
    if (s->mcr & UART_MCR_DTR) {
        flags |= UART_MSR_DSR;
    }
    if (s->mcr & UART_MCR_OUT1) {
        flags |= UART_MSR_RI;
    }
    if (s->mcr & UART_MCR_OUT2) {
        flags |= UART_MSR_DCD;
    }

    omsr = s->msr;

    s->msr = (flags & UART_MSR_CTS)
                ? s->msr | UART_MSR_CTS
                : s->msr & ~UART_MSR_CTS;

    s->msr = (flags & UART_MSR_DSR)
                ? s->msr | UART_MSR_DSR
                : s->msr & ~UART_MSR_DSR;

    s->msr = (flags & UART_MSR_DCD)
                ? s->msr | UART_MSR_DCD
                : s->msr & ~UART_MSR_DCD;

    s->msr = (flags & UART_MSR_RI)
                ? s->msr | UART_MSR_RI
                : s->msr & ~UART_MSR_RI;

    if (s->msr != omsr) {
        /* Set delta bits */
        s->msr = s->msr | ((s->msr >> 4) ^ (omsr >> 4));
        /* UART_MSR_TERI only if change was from 1 -> 0 */
        if ((s->msr & UART_MSR_TERI) && !(omsr & UART_MSR_RI)) {
            s->msr &= ~UART_MSR_TERI;
        }
    }
}

static void fuart_modem_ioport_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned size)
{
    Sc64FuartState *ss = opaque;
    SerialState *s = ss->modem;

    addr &= 7;
    DPRINTF("write addr=0x%" HWADDR_PRIx " val=0x%" PRIx64 ", port = %x\n",
            addr, val, ss->port_nr);
    switch (addr) {
    case 0:
        if (s->lcr & UART_LCR_DLAB) {
            s->divider = (s->divider & 0xff00) | val;
            fuart_modem_update_parameters(ss);
        }
        break;
    case 1:
        if (s->lcr & UART_LCR_DLAB) {
            s->divider = (s->divider & 0x00ff) | (val << 8);
            fuart_modem_update_parameters(ss);
        }
        break;
    case 2:
        /* Did the enable/disable flag change?
         * If so, make sure FIFOs get flushed */
        if ((val ^ s->fcr) & UART_FCR_FE) {
            val |= UART_FCR_XFR | UART_FCR_RFR;
        }

        /* FIFO clear */
        if (val & UART_FCR_RFR) {
            s->lsr &= ~(UART_LSR_DR | UART_LSR_BI);
            fifo8_reset(&s->recv_fifo);
        }

        if (val & UART_FCR_XFR) {
            s->lsr |= UART_LSR_THRE;
        }

        s->fcr = val & 0xC9;
        break;
    case 3:
        {
            int break_enable;
            s->lcr = val;
            fuart_modem_update_parameters(ss);
            break_enable = (val >> 6) & 1;
            if (break_enable != s->last_break_enable) {
                s->last_break_enable = break_enable;
                qemu_chr_fe_ioctl(&s->chr, CHR_IOCTL_SERIAL_SET_BREAK,
                                  &break_enable);
            }
        }
        break;
    case 4:
        {
            int old_mcr = s->mcr;
            s->mcr = val & 0x1f;
            if (val & UART_MCR_LOOP) {
                fuart_modem_update_tiocm_loop(s);
                break;
            }

            if (s->poll_msl >= 0 && old_mcr != s->mcr) {
                fuart_modem_update_tiocm(ss);
                /* Update the modem status after a one-character-send
                 * wait-time, since there may be a response from
                 * the device/computer at the other end of
                 * the fuart_modem line */
                timer_mod(s->modem_status_poll,
                            qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                            s->char_transmit_time);
            }
        }
        break;
    case 6:
        break;
    case 7:
        s->scr = val;
        break;
    default:
        DPRINTF("write invalid addr=0x%lx\n", addr);
        break;
    }
}

static uint64_t fuart_modem_ioport_read(void *opaque, hwaddr addr,
    unsigned size)
{
    Sc64FuartState *ss = opaque;
    SerialState *s = ss->modem;
    uint32_t ret;

    addr &= 7;
    switch (addr) {
    case 0:
        if (s->lcr & UART_LCR_DLAB) {
            ret = s->divider & 0xff;
        }
        break;
    case 1:
        if (s->lcr & UART_LCR_DLAB) {
            ret = (s->divider >> 8) & 0xff;
        }
        break;
    case 3:
        ret = s->lcr;
        break;
    case 4:
        ret = s->mcr;
        break;
    case 5:
        ret = s->lsr;
        /* Clear break and overrun interrupts */
        if (s->lsr & (UART_LSR_BI | UART_LSR_OE)) {
            s->lsr &= ~(UART_LSR_BI | UART_LSR_OE);
        }
        break;
    case 6:
        if (s->poll_msl >= 0) {
            fuart_modem_update_msl(ss);
        }
        ret = s->msr;
        /* Clear delta bits & msr int after read, if they were set */
        if (s->msr & UART_MSR_ANY_DELTA) {
            s->msr &= 0xF0;
        }
        break;
    case 7:
        ret = s->scr;
        break;
    default:
        DPRINTF("read invalid addr=0x%lx\n", addr);
        break;
    }
    DPRINTF("read addr=0x%" HWADDR_PRIx " val=0x%02x, port = %x\n", addr,
            ret, ss->port_nr);
    return ret;
}

static void fuart_modem_receive_break(Sc64FuartState *ss)
{
    /* When the LSR_DR is set a null byte is pushed into the fifo */
    uint8_t b = '\0';
    fuart_dma_receive(ss, &b, 1);
    ss->modem->lsr |= UART_LSR_BI | UART_LSR_DR;
}

static void fuart_modem_event(void *opaque, int event)
{
    Sc64FuartState *ss = opaque;

    DPRINTF("event %x\n", event);
    if (event == CHR_EVENT_BREAK) {
        fuart_modem_receive_break(ss);
    }
}

static void fuart_modem_reset(void *opaque)
{
    Sc64FuartState *ss = opaque;
    SerialState *s = ss->modem;

    if (s->watch_tag > 0) {
        g_source_remove(s->watch_tag);
        s->watch_tag = 0;
    }

    s->lcr = 0;
    s->lsr = UART_LSR_TEMT | UART_LSR_THRE;
    s->msr = 0;
    /* Default to 9600 baud, 1 start bit, 8 data bits, 1 stop bit, no parity. */
    s->divider = 0x0C;
    s->mcr = 0;
    s->scr = 0;
    s->tsr_retry = 0;
    s->char_transmit_time = (NANOSECONDS_PER_SECOND / 9600) * 10;

    /* Turn polling of the modem status lines on. */
    s->poll_msl = 1;

    timer_del(s->modem_status_poll);

    fifo8_reset(&s->recv_fifo);

    s->last_xmit_ts = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    s->last_break_enable = 0;

    fuart_modem_update_msl(ss);
    s->msr &= ~UART_MSR_ANY_DELTA;
}

static int fuart_modem_be_change(void *opaque)
{
    Sc64FuartState *ss = opaque;
    SerialState *s = ss->modem;

    fuart_modem_update_parameters(ss);

    qemu_chr_fe_ioctl(&s->chr, CHR_IOCTL_SERIAL_SET_BREAK,
                      &s->last_break_enable);

    if (s->poll_msl >= 0 && !(s->mcr & UART_MCR_LOOP)) {
        fuart_modem_update_tiocm(ss);
    }

    if (s->watch_tag > 0) {
        g_source_remove(s->watch_tag);
        s->watch_tag = qemu_chr_fe_add_watch(&s->chr, G_IO_OUT | G_IO_HUP,
                                             fuart_modem_watch_cb, ss);
    }

    return 0;
}

static void fuart_modem_realize_core(Sc64FuartState *ss, Error **errp)
{
    SerialState *s = ss->modem;
    if (!qemu_chr_fe_backend_connected(&s->chr)) {
        error_setg(errp, "Can't create fuart_modem device, empty char device");
        return;
    }

    s->modem_status_poll = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                (QEMUTimerCB *) fuart_modem_update_msl, ss);

    qemu_register_reset(fuart_modem_reset, ss);

    fifo8_create(&s->recv_fifo, FUART_FIFO_SIZE);
}

/* Memory mapped interface */
static uint64_t fuart_modem_mm_read(void *opaque, hwaddr addr,
                               unsigned size)
{
    Sc64FuartState *ss = opaque;
    return fuart_modem_ioport_read(ss, addr >> ss->modem->it_shift, 1);
}

static void fuart_modem_mm_write(void *opaque, hwaddr addr,
                            uint64_t value, unsigned size)
{
    Sc64FuartState *ss = opaque;
    value &= 255;
    fuart_modem_ioport_write(ss, addr >> ss->modem->it_shift, value, 1);
}

static const MemoryRegionOps fuart_modem_mm_ops = {
    .read = fuart_modem_mm_read,
    .write = fuart_modem_mm_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.max_access_size = 8,
    .impl.max_access_size = 8,
};

static SerialState *fuart_modem_init(SysBusDevice *dev,
                            Sc64FuartState *ss,
                            hwaddr base, int it_shift,
                            int baudbase, Chardev *chr)
{
    SerialState *s;

    ss->modem = s = g_malloc0(sizeof(SerialState));

    s->it_shift = it_shift;
    s->baudbase = baudbase;
    qemu_chr_fe_init(&s->chr, chr, &error_abort);

    fuart_modem_realize_core(ss, &error_fatal);

    memory_region_init_io(&s->io, NULL, &fuart_modem_mm_ops, ss,
                          "fuart_modem", 8 << it_shift);
    sysbus_init_mmio(dev, &s->io);
    sysbus_mmio_map(dev, 1, base);

    return s;
}

DeviceState *sc64_fuart_register(const char *name, hwaddr addr,
    qemu_irq irq, Chardev *chr, AddressSpace *dma_as, int port_nr)
{
    SysBusDevice *sb;
    DeviceState *dev;
    Sc64FuartState *s;

    dev = qdev_create(NULL, name);
    s = SC64_FUART(dev);
    s->dma_as = dma_as;
    qdev_init_nofail(dev);

    sb = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(sb, 0, addr);
    sysbus_connect_irq(sb, 0, irq);

    s->port_nr = port_nr;
    sc64_fuart_ports[port_nr] = s;

    s->modem = fuart_modem_init(sb, s, addr, 0, 115200, chr);
    qemu_chr_fe_set_handlers(&s->modem->chr, fuart_dma_can_receive,
                             fuart_dma_receive, fuart_modem_event,
                             fuart_modem_be_change, s, NULL, true);

    return dev;
}
