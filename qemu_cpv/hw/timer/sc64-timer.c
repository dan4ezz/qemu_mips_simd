/*
 * QEMU model of the SC64 timer block.
 *
 * Copyright (C) 2014 Antony Pavlov <antonynpavlov@gmail.com>
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
#include "hw/sysbus.h"
#include "hw/ptimer.h"
#include "qemu/main-loop.h"
#include "cpu.h"

#define TYPE_SC64_TIMER "sc64-timer"
#define SC64_TIMER(obj) OBJECT_CHECK(Sc64TimerState, (obj), TYPE_SC64_TIMER)

#define SC64_TIMER0              0x00
#define SC64_TIMER1              0x10
#define SC64_TIMER2              0x20
#define SC64_TIMER3              0x30
#define SC64_TIMER4              0x40

# define SC64_TIMER_COUNTER0     0x0
# define SC64_TIMER_COUNTER1     0x1
# define SC64_TIMER_COUNTER2     0x2
# define SC64_TIMER_COUNTER3     0x3
# define SC64_TIMER_COUNTER4     0x4
# define SC64_TIMER_COUNTER5     0x5
# define SC64_TIMER_COUNTER6     0x6
# define SC64_TIMER_COUNTER7     0x7
# define SC64_TIMER_CSR        0x8
#  define SC64_TIMER_CSR_SAVE  (1 << 0)
#  define SC64_TIMER_CSR_RESET (1 << 1)
#  define SC64_TIMER_CSR_MODE  (1 << 2)
#  define SC64_TIMER_CSR_DUMP  (1 << 5)
#  define SC64_TIMER_CSR_LOAD  (1 << 6)
#  define SC64_TIMER_CSR_DONE  (1 << 7)

#define SC64_TIMER_CONFIG         0x50
# define SC64_TIMER_CONFIG_COUNT0 (1 << 0)
# define SC64_TIMER_CONFIG_COUNT1 (1 << 1)
# define SC64_TIMER_CONFIG_COUNT2 (1 << 2)
# define SC64_TIMER_CONFIG_COUNT3 (1 << 3)
# define SC64_TIMER_CONFIG_COUNT4 (1 << 4)
# define SC64_TIMER_CONFIG_NMIN   (1 << 5)

#define SC64_TIMER_FREQ_IN_HZ    24000000

typedef struct Sc64TimerState Sc64TimerState;

typedef struct Sc64OneTimerState {
    Sc64TimerState *parent;
    ptimer_state *ptimer;

    bool has_nmi;
    bool oneshot;
    bool enabled;

    uint8_t csr;

    uint64_t in_latch;
    bool loaded;

    bool out_latched;
    uint64_t out_latch;

    bool maskint;

    qemu_irq irq;

    /*
     * In cases when guest system programs value > UINT32_MAX to avoid
     * QEMU timer overflow (and subsequent "hanging") use workaround
     * which splits 64 bit value into two 32 bit values. On every timer
     * tick decrease high part of the 64 bit value and if it hits zero
     * and timer itself hits zero then the whole 64 bit value is passed.
     */
    uint32_t load_high;
} Sc64OneTimerState;

typedef struct Sc64TimerState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    Sc64OneTimerState timer[5];
    uint8_t div;
} Sc64TimerState;

static const VMStateDescription vmstate_sc64_one_timer_state = {
    .name = "sc64.timer/one_state",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_PTIMER(ptimer, Sc64OneTimerState),
        VMSTATE_BOOL(oneshot, Sc64OneTimerState),
        VMSTATE_BOOL(enabled, Sc64OneTimerState),
        VMSTATE_UINT8(csr, Sc64OneTimerState),
        VMSTATE_UINT64(in_latch, Sc64OneTimerState),
        VMSTATE_BOOL(loaded, Sc64OneTimerState),
        VMSTATE_BOOL(out_latched, Sc64OneTimerState),
        VMSTATE_UINT64(out_latch, Sc64OneTimerState),
        VMSTATE_BOOL(maskint, Sc64OneTimerState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_sc64_timer = {
    .name = "sc64.timer",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT_ARRAY(timer, Sc64TimerState, 5, 0,
            vmstate_sc64_one_timer_state, Sc64OneTimerState),
        VMSTATE_UINT8(div, Sc64TimerState),
        VMSTATE_END_OF_LIST()
    },
};

static void sc64_timer_update_irq(Sc64OneTimerState *t, bool done)
{
    if (done) {
        t->csr |= SC64_TIMER_CSR_DONE;
        qemu_irq_raise(t->irq);
        if (t->has_nmi && t->maskint) {
            /* FIXME Use proper NMI delivering through the QEMU irq system. */
            cpu_interrupt(first_cpu, CPU_INTERRUPT_NMI);
        }
    } else {
        t->csr &= ~SC64_TIMER_CSR_DONE;
        qemu_irq_lower(t->irq);
    }
}

static inline void sc64_timer_update_divider(Sc64TimerState *s, uint8_t val)
{
    int i;
    uint32_t freq;

    s->div = val;
    if (s->div > 0xfe) {
        freq = SC64_TIMER_FREQ_IN_HZ;
    } else {
        freq = SC64_TIMER_FREQ_IN_HZ / (2 * (s->div + 1));
    }

    for (i = 0; i < 5; i++) {
        ptimer_set_freq(s->timer[i].ptimer, freq);
    }
}

static uint64_t sc64_timer_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64TimerState *s = opaque;
    uint64_t val;
    uint64_t res;

    val = 0;

    switch (addr & 0x70) {
    case SC64_TIMER0:
    case SC64_TIMER1:
    case SC64_TIMER2:
    case SC64_TIMER3:
    case SC64_TIMER4:
        ;
        Sc64OneTimerState *t = &(s->timer[(addr & 0x70) >> 4]);
        int field = (addr & 0xf);

        switch (field) {
        case SC64_TIMER_COUNTER0:
        case SC64_TIMER_COUNTER1:
        case SC64_TIMER_COUNTER2:
        case SC64_TIMER_COUNTER3:
        case SC64_TIMER_COUNTER4:
        case SC64_TIMER_COUNTER5:
        case SC64_TIMER_COUNTER6:
        case SC64_TIMER_COUNTER7:
            if (t->out_latched) {
                res = t->out_latch;
            } else {
                /*
                 * QEMU, normally, never returns 0 even if timer is not counting.
                 * So return last written data instead.
                 */
                if (t->enabled) {
                    res = ptimer_get_count(t->ptimer);
                    res |= (uint64_t)t->load_high << 32;
                } else {
                    res = t->in_latch;
                }
            }

            if (size == 1) {
                val = (res & (0xffULL << (field * 8))) >> (field * 8);
                if (field == SC64_TIMER_COUNTER7) {
                    t->out_latched = false;
                }
            }

            if (size == 2) {
                val = (res & (0xffffULL << (field * 8))) >> (field * 8);
                if (field == SC64_TIMER_COUNTER6) {
                    t->out_latched = false;
                }
            }

            if (size == 4) {
                val = (res & (0xffffffffULL << (field * 8))) >> (field * 8);
                if (field == SC64_TIMER_COUNTER4) {
                    t->out_latched = false;
                }
            }

            if (size == 8) {
                val = res;
                t->out_latched = false;
            }

        break;
        case SC64_TIMER_CSR:
            val = t->csr;
        break;
        }
    break;

    case SC64_TIMER_CONFIG:
        switch (addr & 1) {
        case 0: /* configuration register */
            val = (s->timer[0].enabled << 0) |
                (s->timer[1].enabled << 1) |
                (s->timer[2].enabled << 2) |
                (s->timer[3].enabled << 3) |
                (s->timer[4].enabled << 4) |
                (s->timer[0].maskint << 5);
            val &= 0x3f;
        break;

        case 1: /* divider register */
            val = s->div;
        break;

        default:
        break;
        }
    break;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-timer: read access to unknown register 0x"
                      TARGET_FMT_plx, addr);
    }

    return val;
}

static void sc64_timer_write(void *opaque, hwaddr addr,
                              uint64_t val, unsigned size)
{
    Sc64TimerState *s = opaque;
    int i;

    switch (addr & 0x70) {
    case SC64_TIMER0:
    case SC64_TIMER1:
    case SC64_TIMER2:
    case SC64_TIMER3:
    case SC64_TIMER4:
        ;
        Sc64OneTimerState *t = &(s->timer[(addr & 0x70) >> 4]);
        int field;
        int load;

        load = 0;
        field = addr & 0xf;

        if (size == 2) {
            field = addr & 0xe;
        }

        if (size == 4) {
            field = addr & 0xc;
        }

        if (size == 8) {
            field = addr & 0x8;
        }

        switch (field) {
        case SC64_TIMER_COUNTER0:
        case SC64_TIMER_COUNTER1:
        case SC64_TIMER_COUNTER2:
        case SC64_TIMER_COUNTER3:
        case SC64_TIMER_COUNTER4:
        case SC64_TIMER_COUNTER5:
        case SC64_TIMER_COUNTER6:
        case SC64_TIMER_COUNTER7:

            if (size == 1) {
                t->in_latch = (t->in_latch & (~(0xffULL << (field * 8)))) |
                            ((val << (field * 8)) & (0xffULL << (field * 8)));
                if (field == SC64_TIMER_COUNTER7) {
                    load = 1;
                }
            }

            if (size == 2) {
                t->in_latch = (t->in_latch & (~(0xffffULL << (field * 8)))) |
                            ((val << (field * 8)) & (0xffffULL << (field * 8)));
                if (field == SC64_TIMER_COUNTER6) {
                    load = 1;
                }
            }

            if (size == 4) {
                switch (field) {
                case SC64_TIMER_COUNTER0:
                    t->in_latch = (t->in_latch & 0xffffffff00000000ULL) |
                            (val & 0xffffffffULL);
                    break;
                case SC64_TIMER_COUNTER4:
                    t->in_latch = (t->in_latch & 0xffffffffULL) |
                            ((val & 0xffffffffULL) << 32);
                    break;
                }

                if (field == SC64_TIMER_COUNTER4) {
                    load = 1;
                }
            }

            if (size == 8) {
                t->in_latch = val;
                load = 1;
            }

            t->loaded = !!load;

            if (t->loaded) {
                t->load_high = t->in_latch >> 32;

                ptimer_stop(t->ptimer);
                sc64_timer_update_irq(t, false);
                ptimer_set_limit(t->ptimer, t->in_latch & 0xffffffff, 1);

                if (t->enabled) {
                    ptimer_run(t->ptimer, t->load_high ? 0 : t->oneshot);
                    t->loaded = false;
                }
            }
        break;

        case SC64_TIMER_CSR:
            t->oneshot = !(val & SC64_TIMER_CSR_MODE);

            if (val & SC64_TIMER_CSR_SAVE) {
                t->out_latched = true;
                t->out_latch = ptimer_get_count(t->ptimer);
            }

            if ((val & SC64_TIMER_CSR_RESET) || (!!t->oneshot ^ !!(val & 4))) {
                sc64_timer_update_irq(t, false);
            }
        break;
        }
    break;

    case SC64_TIMER_CONFIG:
        switch (addr & 1) {
        case 0: /* configuration register */

            for (i = 0; i < 5; i++) {
                    Sc64OneTimerState *tt = &s->timer[i];
                    bool enabled = (val >> i) & 1;

                    tt->enabled = enabled;
                    tt->maskint = (val >> 5) & 1;

                    if (!enabled) {
                        ptimer_stop(tt->ptimer);
                        sc64_timer_update_irq(tt, false);
                    } else {
                        if (tt->loaded) {
                            ptimer_run(tt->ptimer,
                                            tt->load_high ? 0 : tt->oneshot);
                            tt->loaded = false;
                        }
                    }
            }
        break;

        case 1: /* divider register */
                sc64_timer_update_divider(s, val);
        break;

        default:
        break;
        }

    default:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-timer: read access to unknown register 0x"
                      TARGET_FMT_plx, addr);
    }
}

static const MemoryRegionOps sc64_timer_ops = {
    .read = sc64_timer_read,
    .write = sc64_timer_write,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void sc64_timer_reset(DeviceState *dev)
{
    Sc64TimerState *s = SC64_TIMER(dev);
    Sc64OneTimerState *t;
    int i;

    for (i = 0; i < 5; i++) {
        t = &(s->timer[i]);

        ptimer_stop(t->ptimer);
        ptimer_set_limit(t->ptimer, 0xffffffffffffffffLL, 0);
        ptimer_set_count(t->ptimer, 0xffffffffffffffffLL);

        t->enabled = false;
        t->oneshot = true;
        t->in_latch = 0;
        t->loaded = false;
        t->out_latched = false;
        t->out_latch = 0;
        t->csr = 0x0;
        t->maskint = false;
        t->load_high = 0;

        sc64_timer_update_irq(t, false);
    }

    sc64_timer_update_divider(s, 0x02);
}

static void sc64_timer_hit(void *opaque)
{
    Sc64OneTimerState *t = opaque;

    if (t->load_high) {
        t->load_high--;
        return;
    }
    t->load_high = t->in_latch >> 32;
    if (t->load_high && t->oneshot) {
        ptimer_stop(t->ptimer);
    }

    sc64_timer_update_irq(t, true);
}

static int sc64_timer_init(SysBusDevice *dev)
{
    Sc64TimerState *s = SC64_TIMER(dev);
    Sc64OneTimerState *t;
    QEMUBH *bh;
    int i;

    for (i = 0; i < 5; i++) {
        t = &(s->timer[i]);
        t->parent = s;
        sysbus_init_irq(dev, &t->irq);
        bh = qemu_bh_new(sc64_timer_hit, t);
        t->ptimer = ptimer_init(bh, PTIMER_POLICY_DEFAULT);
        t->has_nmi = false;
    }

    s->timer[4].has_nmi = true;

    memory_region_init_io(&s->iomem, OBJECT(s), &sc64_timer_ops, s,
                          TYPE_SC64_TIMER, 0x60);
    sysbus_init_mmio(dev, &s->iomem);

    return 0;
}

static void sc64_timer_class_init(ObjectClass *klass, void *class_data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_timer_init;
    dc->reset = sc64_timer_reset;
    dc->vmsd = &vmstate_sc64_timer;
}

static const TypeInfo sc64_timer_info = {
    .name = TYPE_SC64_TIMER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64TimerState),
    .class_init = sc64_timer_class_init,
};

static void sc64_timer_register_type(void)
{
    type_register_static(&sc64_timer_info);
}

type_init(sc64_timer_register_type)
