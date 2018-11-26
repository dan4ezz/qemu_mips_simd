/*
 * QEMU model of the SC64 timer block.
 *
 * Copyright (C) 2014 Antony Pavlov <antonynpavlov@gmail.com>
 * Copyright (C) 2014 Dmitry Smagin <dmitry.s.smagin@gmail.com>
 * Copyright (C) 2017 Alexander Kalmuk <alexkalmuk@gmail.com>
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
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "hw/ptimer.h"
#include "hw/timer/sc64-timer.h"
#include "qemu/main-loop.h"
#include "qemu/bitops.h"
#include "cpu.h"

#define SC64_TIMER_V2(obj) \
    OBJECT_CHECK(Sc64TimerStateV2, (obj), TYPE_SC64_TIMER_V2)

#define SC64_TIMER_V2_REG_MEM_SIZE  0x016c

#define SC64_TIMER_V2_ID            0x000
#define SC64_TIMER_V2_CAUSE         0x008

#define SC64_TIMER0_V2              0x020
#define SC64_TIMER1_V2              0x040
#define SC64_TIMER2_V2              0x060
#define SC64_TIMER3_V2              0x080
#define SC64_TIMER4_V2              0x0a0
#define SC64_TIMER5_V2              0x0c0
#define SC64_TIMER6_V2              0x0e0
#define SC64_TIMER7_V2              0x100
#define SC64_WATCHDOG_V2            0x120
#define SC64_TIMESTAMP              0x140

#define SC64_TIMER_V2_CONTROL       0x00
#define SC64_TIMER_V2_CNT_EN        (1 << 0)
#define SC64_TIMER_V2_MODE          (1 << 1)
#define SC64_TIMER_V2_INT_EN        (1 << 2)
#define SC64_TIMER_V2_NMI_EN        (1 << 3)
#define SC64_TIMER_V2_OUT_POL       (1 << 4)
#define SC64_TIMER_V2_PWM_EN        (1 << 5)
#define SC64_TIMER_V2_MEA_EN        (1 << 6)
#define SC64_TIMER_V2_CLK_SEL       (1 << 7)
#define SC64_TIMER_V2_DIV           0x7FF0000
#define SC64_TIMER_V2_DEFAULT \
            (SC64_TIMER_V2_DIV | SC64_TIMER_V2_CLK_SEL)

#define SC64_TIMER_V2_STATUS        0x04
#define SC64_TIMER_V2_ST            (1 << 0)

#define SC64_TIMER_V2_LOAD          0x08
#define SC64_TIMER_V2_READ          0x10
#define SC64_TIMER_V2_PWM           0x18

#define SC64_TIMER_V2_FREQ_IN_HZ    125000000

#define SC64_TIMER_V2_CNT_WDT_QUANTITY      9

#define SC64_TIMER_V2_NR_MASK       0xFFE0
#define SC64_TIMER_V2_CONTROL_MASK ((0x7FF << 16) | 0xFF)

#define SC64_WATCHDOG_V2_CONTROL_MASK \
           (SC64_TIMER_V2_DIV | SC64_TIMER_V2_CNT_EN | SC64_TIMER_V2_MODE | \
             SC64_TIMER_V2_NMI_EN | SC64_TIMER_V2_CLK_SEL)

#define SC64_TIMESTAMP_CONTROL       0x00
# define SC64_TIMESTAMP_CTRL_TSENA         (1 << 0)
# define SC64_TIMESTAMP_CTRL_TSCFUPDT      (1 << 1)
# define SC64_TIMESTAMP_CTRL_TSINIT        (1 << 2)
# define SC64_TIMESTAMP_CTRL_TSUPDT        (1 << 3)
# define SC64_TIMESTAMP_CTRL_TSTRIG        (1 << 4)
# define SC64_TIMESTAMP_CTRL_TSADDREG      (1 << 5)
# define SC64_TIMESTAMP_CTRL_TSCTRLSSR     (1 << 9)
#define SC64_TIMESTAMP_CTRL_MASK  \
               (SC64_TIMESTAMP_CTRL_TSENA | SC64_TIMESTAMP_CTRL_TSCFUPDT | \
                SC64_TIMESTAMP_CTRL_TSINIT | SC64_TIMESTAMP_CTRL_TSUPDT | \
                SC64_TIMESTAMP_CTRL_TSTRIG | SC64_TIMESTAMP_CTRL_TSADDREG | \
                SC64_TIMESTAMP_CTRL_TSCTRLSSR)

#define SC64_TIMESTAMP_CTRL_MAX_DEC       0x3b9ac9ff
#define SC64_TIMESTAMP_CTRL_MAX_BIN       0x7fffffff

#define SC64_TIMESTAMP_SUB_SEC_INC    0x04
#define SC64_TIMESTAMP_SYS_TIME_SEC   0x08
#define SC64_TIMESTAMP_SYS_TIME_NSEC  0x0C
#define SC64_TIMESTAMP_SEC_UPDATE     0x10
#define SC64_TIMESTAMP_NSEC_UPDATE    0x14
#define SC64_TIMESTAMP_ADDEND         0x18
#define SC64_TIMESTAMP_TARGET_SEC     0x1C
#define SC64_TIMESTAMP_TARGET_NSEC    0x20
#define SC64_TIMESTAMP_HIGH_WORD_SEC  0x24
#define SC64_TIMESTAMP_STATUS         0x28
# define SC64_TIMESTAMP_STAT_TSSOVF    (1 << 0)
# define SC64_TIMESTAMP_STAT_TSTARGT   (1 << 1)
# define SC64_TIMESTAMP_STAT_TSTRGTERR (1 << 3)

#define REG_READ32(reg, start, len) \
    extract32(reg, 8 * (start), 8 * (len))

#define REG_READ64(reg, start, len) \
    extract64(reg, 8 * (start), 8 * (len))

#define REG_WRITE32(reg, val, start, len) \
    reg = deposit32(reg, 8 * (start), 8 * (len), val);

#define REG_WRITE64(reg, val, start, len) \
    reg = deposit64(reg, 8 * (start), 8 * (len), val);

typedef struct Sc64TimerStateV2 Sc64TimerStateV2;

typedef struct Sc64OneTimerStateV2 {
    Sc64TimerStateV2 *parent;
    ptimer_state *ptimer;

    uint8_t nr;

    uint32_t control;
    uint32_t status;
    uint64_t load;
    uint64_t pwm;

    qemu_irq irq;

    /*
     * In cases when guest system programs value > UINT32_MAX to avoid
     * QEMU timer overflow (and subsequent "hanging") use workaround
     * which splits 64 bit value into two 32 bit values. On every timer
     * tick decrease high part of the 64 bit value and if it hits zero
     * and timer itself hits zero then the whole 64 bit value is passed.
     */
    uint32_t load_high;
} Sc64OneTimerStateV2;

typedef struct Sc64Timestamp {
    Sc64TimerStateV2 *parent;
    ptimer_state *ptimer;

    uint32_t control;
    uint32_t status;
    uint32_t sub_sec_inc;
    uint32_t systime_sec;
    uint32_t systime_nsec;
    uint32_t sec_update;
    uint32_t nsec_update;
    uint32_t timestamp_addend;
    uint32_t target_time_sec;
    uint32_t target_time_nsec;
    uint32_t high_word_sec;

    uint32_t max_nsec;
    uint32_t accum;
    bool     fine;

    qemu_irq *irq;
} Sc64Timestamp;

typedef struct Sc64TimerStateV2 {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;

    Sc64OneTimerStateV2 timer[SC64_TIMER_V2_CNT_WDT_QUANTITY];
    Sc64Timestamp timestamp;
    uint32_t id;
    uint64_t cause;
} Sc64TimerStateV2;

/* Warning: This state is not full and not tested */
static const VMStateDescription vmstate_sc64_one_timer_state_v2 = {
    .name = "sc64.timer/one_state",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_PTIMER(ptimer, Sc64OneTimerStateV2),
        VMSTATE_UINT8(nr, Sc64OneTimerStateV2),
        VMSTATE_UINT32(control, Sc64OneTimerStateV2),
        VMSTATE_UINT32(status, Sc64OneTimerStateV2),
        VMSTATE_UINT64(load, Sc64OneTimerStateV2),
        VMSTATE_UINT64(pwm, Sc64OneTimerStateV2),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_sc64_timer_v2 = {
    .name = "sc64.timer",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT_ARRAY(timer, Sc64TimerStateV2,
            SC64_TIMER_V2_CNT_WDT_QUANTITY, 0,
            vmstate_sc64_one_timer_state_v2, Sc64OneTimerStateV2),
        VMSTATE_UINT32(id, Sc64TimerStateV2),
        VMSTATE_UINT64(cause, Sc64TimerStateV2),
        VMSTATE_END_OF_LIST()
    },
};

static inline uint16_t sc64_timer_v2_get_freq(Sc64OneTimerStateV2 *t)
{
    return (t->control & SC64_TIMER_V2_DIV) >> 16;
}

static inline void sc64_timer_v2_set_cause_irq(Sc64OneTimerStateV2 *t)
{
    set_bit(t->nr, (unsigned long *)&t->parent->cause);
}

static inline void sc64_timer_v2_set_cause_nmi(Sc64OneTimerStateV2 *t)
{
    set_bit(t->nr + 32, (unsigned long *)&t->parent->cause);
}

static void sc64_timer_v2_clear_cause_irq(Sc64OneTimerStateV2 *t)
{
    clear_bit(t->nr, (unsigned long *)&t->parent->cause);
}

static void sc64_timer_v2_clear_cause_nmi(Sc64OneTimerStateV2 *t)
{
    clear_bit(t->nr + 32, (unsigned long *)&t->parent->cause);
}

static inline bool sc64_timer_v2_has_irq(Sc64OneTimerStateV2 *t)
{
    return REG_READ32(t->control, 0, 4) & SC64_TIMER_V2_INT_EN;
}

static inline bool sc64_timer_v2_has_nmi(Sc64OneTimerStateV2 *t)
{
    return REG_READ32(t->control, 0, 4) & SC64_TIMER_V2_NMI_EN;
}

static inline bool sc64_timer_v2_enabled(Sc64OneTimerStateV2 *t)
{
    return REG_READ32(t->control, 0, 4) & SC64_TIMER_V2_CNT_EN;
}

static inline bool sc64_timer_v2_periodic(Sc64OneTimerStateV2 *t)
{
    return REG_READ32(t->control, 0, 4) & SC64_TIMER_V2_MODE;
}

static inline void sc64_timer_v2_raise_irq(Sc64OneTimerStateV2 *t)
{
    sc64_timer_v2_set_cause_irq(t);
    qemu_irq_raise(t->irq);
}

static inline void sc64_timer_v2_raise_nmi(Sc64OneTimerStateV2 *t)
{
    sc64_timer_v2_set_cause_nmi(t);
    /* FIXME Use proper NMI delivering through the QEMU irq system. */
    cpu_interrupt(first_cpu, CPU_INTERRUPT_NMI);
}

static inline bool sc64_timer_is_watchdog(Sc64OneTimerStateV2 *t)
{
    return t->nr == 8;
}

static inline void sc64_timer_v2_update_divider(Sc64OneTimerStateV2 *t,
        uint16_t div)
{
    uint32_t freq;

    if (div == 0x7FF) {
        freq = SC64_TIMER_V2_FREQ_IN_HZ;
    } else {
        freq = SC64_TIMER_V2_FREQ_IN_HZ / (2 * div + 1);
    }

    ptimer_set_freq(t->ptimer, freq);
}

static inline int sc64_timestamp_overflow(Sc64Timestamp *ts)
{
    return (ts->systime_sec >= ts->target_time_sec &&
                (ts->systime_sec > ts->target_time_sec ||
                    ts->systime_nsec >= ts->target_time_nsec));
}

static uint64_t sc64_timer_v2_read_count(Sc64OneTimerStateV2 *t)
{
    uint64_t val;
    val = ptimer_get_count(t->ptimer);
    val |= (uint64_t)t->load_high << 32;
    return val;
}

static void sc64_timer_v2_start(Sc64OneTimerStateV2 *t)
{
    ptimer_run(t->ptimer, t->load_high ? 0 : !sc64_timer_v2_periodic(t));
}

static void sc64_timer_v2_load(Sc64OneTimerStateV2 *t, uint64_t val)
{
    ptimer_stop(t->ptimer);
    t->load_high = t->load >> 32;
    ptimer_set_limit(t->ptimer, t->load & 0xffffffff, 1);
    if (sc64_timer_v2_enabled(t)) {
        sc64_timer_v2_start(t);
    }
}

static uint64_t sc64_timer_v2_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64TimerStateV2 *s = opaque;
    Sc64OneTimerStateV2 *t;
    int reg;
    uint64_t val;

    val = 0;

    switch (addr & SC64_TIMER_V2_NR_MASK) {
    case SC64_TIMER0_V2:
    case SC64_TIMER1_V2:
    case SC64_TIMER2_V2:
    case SC64_TIMER3_V2:
    case SC64_TIMER4_V2:
    case SC64_TIMER5_V2:
    case SC64_TIMER6_V2:
    case SC64_TIMER7_V2:
    case SC64_WATCHDOG_V2:
        t = &(s->timer[extract32(addr, 5, 8) - 1]);

        reg = addr & 0x18;
        if (reg == 0) {
            reg = addr & 0x4 ? SC64_TIMER_V2_STATUS : SC64_TIMER_V2_CONTROL;
        }

        switch (reg) {
        case SC64_TIMER_V2_CONTROL:
            val = REG_READ32(t->control, addr & 0x3, size);
            break;
        case SC64_TIMER_V2_STATUS:
            val = REG_READ32(t->status, addr & 0x3, size);
            break;
        case SC64_TIMER_V2_LOAD:
            val = REG_READ64(t->load, addr & 0x7, size);
            break;
        case SC64_TIMER_V2_READ:
            val = sc64_timer_v2_read_count(t);
            val = REG_READ64(val, addr & 0x7, size);
            break;
        case SC64_TIMER_V2_PWM:
            val = REG_READ64(t->pwm, addr & 0x7, size);
            break;
        }
        break;
    case SC64_TIMESTAMP ... 0x160:
    {
        Sc64Timestamp *ts = &s->timestamp;

        reg = (addr & 0xFC) - 0x40;

        switch (reg) {
        case SC64_TIMESTAMP_CONTROL:
            val = REG_READ32(ts->control, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_SYS_TIME_SEC:
            val = REG_READ32(ts->systime_sec, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_SYS_TIME_NSEC:
            val = REG_READ32(ts->systime_nsec, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_SUB_SEC_INC:
            val = REG_READ32(ts->sub_sec_inc, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_SEC_UPDATE:
            val = REG_READ32(ts->sec_update, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_NSEC_UPDATE:
            val = REG_READ32(ts->nsec_update, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_ADDEND:
            val = REG_READ32(ts->timestamp_addend, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_TARGET_SEC:
            val = REG_READ32(ts->target_time_sec, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_TARGET_NSEC:
            val = REG_READ32(ts->target_time_nsec, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_HIGH_WORD_SEC:
            val = REG_READ32(ts->high_word_sec, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_STATUS:
            val = REG_READ32(ts->status, addr & 0x3, size);
            ts->status &= ~SC64_TIMESTAMP_STAT_TSTRGTERR;
            break;
        }
        break;
    }
    case 0x0:
        reg = addr & 0x8;
        switch (reg) {
        case SC64_TIMER_V2_ID:
            val = REG_READ32(s->id, addr & 0x3, size);
            break;
        case SC64_TIMER_V2_CAUSE:
            val = REG_READ64(s->cause, addr & 0x7, size);
            break;
        }
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-timer-v2: read access to unknown register 0x"
                      TARGET_FMT_plx, addr);
    }

    return val;
}

static void sc64_timer_v2_write(void *opaque, hwaddr addr,
                              uint64_t val, unsigned size)
{
    Sc64TimerStateV2 *s = opaque;
    Sc64OneTimerStateV2 *t;
    int reg;

    switch (addr & SC64_TIMER_V2_NR_MASK) {
    case SC64_TIMER0_V2:
    case SC64_TIMER1_V2:
    case SC64_TIMER2_V2:
    case SC64_TIMER3_V2:
    case SC64_TIMER4_V2:
    case SC64_TIMER5_V2:
    case SC64_TIMER6_V2:
    case SC64_TIMER7_V2:
    case SC64_WATCHDOG_V2:
        t = &(s->timer[extract32(addr, 5, 8) - 1]);

        reg = addr & 0x18;
        if (reg == 0) {
            reg = addr & 0x4 ? SC64_TIMER_V2_STATUS : SC64_TIMER_V2_CONTROL;
        }

        switch (reg) {
        case SC64_TIMER_V2_CONTROL:
            if (!sc64_timer_is_watchdog(t)) {
                val &= SC64_TIMER_V2_CONTROL_MASK;
            } else {
                val &= SC64_WATCHDOG_V2_CONTROL_MASK;
                val |= SC64_TIMER_V2_MODE;
            }

            REG_WRITE32(t->control, val, addr & 0x3, size);
            sc64_timer_v2_update_divider(t, (val & SC64_TIMER_V2_DIV) >> 16);

            if (sc64_timer_v2_enabled(t)) {
                sc64_timer_v2_start(t);
            } else {
                ptimer_stop(t->ptimer);
            }
            break;
        case SC64_TIMER_V2_STATUS:
            if (val == 1) {
                REG_WRITE32(t->status, 0, 0, 1);
                sc64_timer_v2_clear_cause_irq(t);
                sc64_timer_v2_clear_cause_nmi(t);
                qemu_irq_lower(t->irq);
            }
            break;
        case SC64_TIMER_V2_LOAD:
            REG_WRITE64(t->load, val, addr & 0x7, size);
            sc64_timer_v2_load(t, val);
            break;
        case SC64_TIMER_V2_READ:
            break;
        case SC64_TIMER_V2_PWM:
            REG_WRITE64(t->pwm, val, addr & 0x7, size);
            break;
        }
        break;
    case SC64_TIMESTAMP ... 0x160:
    {
        Sc64Timestamp *ts = &s->timestamp;

        reg = (addr & 0xFC) - 0x40;

        switch (reg) {
        case SC64_TIMESTAMP_CONTROL:
            val &= SC64_TIMESTAMP_CTRL_MASK;
            REG_WRITE32(ts->control, val, addr & 0x3, size);

            ts->fine = val & SC64_TIMESTAMP_CTRL_TSCFUPDT ? 1 : 0;

            if (val & SC64_TIMESTAMP_CTRL_TSCTRLSSR) {
                ts->max_nsec = SC64_TIMESTAMP_CTRL_MAX_DEC;
            } else {
                ts->max_nsec = SC64_TIMESTAMP_CTRL_MAX_BIN;
            }

            if (val & SC64_TIMESTAMP_CTRL_TSUPDT) {
                /*TODO : sub */
                ts->systime_sec += ts->sec_update;
                ts->systime_nsec += ts->nsec_update;
            }

            if (val & SC64_TIMESTAMP_CTRL_TSINIT) {
                ts->systime_sec = ts->sec_update;
                ts->systime_nsec = ts->nsec_update;
            }

            if (val & SC64_TIMESTAMP_CTRL_TSENA) {
                ts->accum = 0;
                if (!sc64_timestamp_overflow(ts)) {
                    ptimer_run(ts->ptimer, 0);
                } else {
                    ts->status |= SC64_TIMESTAMP_STAT_TSTRGTERR;
                }
            } else {
                ptimer_stop(ts->ptimer);
            }
            break;
        case SC64_TIMESTAMP_SUB_SEC_INC:
            REG_WRITE32(ts->sub_sec_inc, val, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_SEC_UPDATE:
            REG_WRITE32(ts->sec_update, val, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_NSEC_UPDATE:
            REG_WRITE32(ts->nsec_update, val, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_ADDEND:
            REG_WRITE32(ts->timestamp_addend, val, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_TARGET_SEC:
            REG_WRITE32(ts->target_time_sec, val, addr & 0x3, size);
            if (val < ts->systime_sec) {
                ts->status |= SC64_TIMESTAMP_STAT_TSTRGTERR;
            }
            break;
        case SC64_TIMESTAMP_TARGET_NSEC:
            REG_WRITE32(ts->target_time_nsec, val, addr & 0x3, size);
            if (val < ts->systime_nsec &&
                        ts->systime_sec >= ts->target_time_sec) {
                ts->status |= SC64_TIMESTAMP_STAT_TSTRGTERR;
            }
            break;
        case SC64_TIMESTAMP_HIGH_WORD_SEC:
            REG_WRITE32(ts->high_word_sec, val, addr & 0x3, size);
            break;
        case SC64_TIMESTAMP_STATUS:
            if (val & SC64_TIMESTAMP_STAT_TSTARGT) {
                ts->status &= ~SC64_TIMESTAMP_STAT_TSTARGT;
                qemu_irq_lower(*ts->irq);
            }
            /* TODO: handle TSSOVF */
            break;
        }
        break;
    }
    default:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-timer-v2: read access to unknown register 0x"
                      TARGET_FMT_plx, addr);
    }
}

static const MemoryRegionOps sc64_timer_v2_ops = {
    .read = sc64_timer_v2_read,
    .write = sc64_timer_v2_write,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

uint64_t sc64_timestamp_get(void *opaque)
{
    assert(opaque);

    Sc64TimerStateV2 *s = SC64_TIMER_V2(opaque);
    Sc64Timestamp *ts = &s->timestamp;

    return (((uint64_t) ts->systime_sec) << 32) | ts->systime_nsec;
}

static void sc64_timestamp_reset(Sc64Timestamp *ts)
{
    ptimer_stop(ts->ptimer);
    ptimer_set_limit(ts->ptimer, 0x1LL, 0);
    ptimer_set_count(ts->ptimer, 0x1LL);
    ptimer_set_freq(ts->ptimer, SC64_TIMER_V2_FREQ_IN_HZ);

    ts->control = 0;
    ts->status = 0;
    ts->sub_sec_inc = 0;
    ts->systime_sec = 0;
    ts->systime_nsec = 0;
    ts->sec_update = 0;
    ts->nsec_update = 0;
    ts->timestamp_addend = 0;
    ts->target_time_sec = 0;
    ts->target_time_nsec = 0;
    ts->high_word_sec = 0;

    ts->max_nsec = 0;
    ts->accum    = 0;
    ts->fine     = 0;
}

static void sc64_timer_v2_reset(DeviceState *dev)
{
    Sc64TimerStateV2 *s = SC64_TIMER_V2(dev);
    Sc64OneTimerStateV2 *t;
    int i;

    s->id = 0x20;
    s->cause = 0;

    for (i = 0; i < SC64_TIMER_V2_CNT_WDT_QUANTITY; i++) {
        t = &(s->timer[i]);

        ptimer_stop(t->ptimer);
        ptimer_set_limit(t->ptimer, 0, 0);
        ptimer_set_count(t->ptimer, 0);

        t->nr = i;
        t->control = SC64_TIMER_V2_DEFAULT;
        t->status = 0;
        t->load = 0;
        t->pwm = 0;
        t->load_high = 0;

        qemu_irq_lower(t->irq);
        sc64_timer_v2_update_divider(t, 0x7FF);
    }

    sc64_timestamp_reset(&s->timestamp);
}

static void sc64_timer_v2_hit(void *opaque)
{
    Sc64OneTimerStateV2 *t = opaque;

    if (t->load_high) {
        t->load_high--;
        return;
    }
    t->load_high = t->load >> 32;
    if (t->load_high && !sc64_timer_v2_periodic(t)) {
        ptimer_stop(t->ptimer);
    }

    REG_WRITE32(t->status, 1, 0, 1);

    if (sc64_timer_v2_has_irq(t)) {
        sc64_timer_v2_raise_irq(t);
    }

    if (sc64_timer_v2_has_nmi(t)) {
        sc64_timer_v2_raise_nmi(t);
    }
}

static void sc64_timestamp_hit(void *opaque)
{
    Sc64Timestamp *ts = opaque;

    if (ts->fine) {
        ts->accum += ts->timestamp_addend;
        if (ts->accum >= ts->timestamp_addend) {
            /* no overflow */
            return;
        }
    }

    /* Increment nsec and sec at first */
    if (ts->systime_nsec + ts->sub_sec_inc > ts->max_nsec) {
        ts->systime_nsec = 0;
        ts->systime_sec++;
    } else {
        ts->systime_nsec += ts->sub_sec_inc;
    }

    /* Check for overflow */
    if (sc64_timestamp_overflow(ts) &&
                (0 == (ts->status & SC64_TIMESTAMP_STAT_TSTARGT))) {
        ts->status |= SC64_TIMESTAMP_STAT_TSTARGT;
        if (ts->control & SC64_TIMESTAMP_CTRL_TSTRIG) {
            qemu_irq_raise(*ts->irq);
        }
    }
}

static int sc64_timer_v2_init(SysBusDevice *dev)
{
    Sc64TimerStateV2 *s = SC64_TIMER_V2(dev);
    Sc64OneTimerStateV2 *t;
    Sc64Timestamp *ts;
    QEMUBH *bh;
    int i;

    for (i = 0; i < SC64_TIMER_V2_CNT_WDT_QUANTITY; i++) {
        t = &(s->timer[i]);
        t->parent = s;
        sysbus_init_irq(dev, &t->irq);
        bh = qemu_bh_new(sc64_timer_v2_hit, t);
        t->ptimer = ptimer_init(bh, PTIMER_POLICY_DEFAULT);
    }

    ts = &s->timestamp;
    ts->parent = s;
    ts->irq = &s->timer[7].irq;
    bh = qemu_bh_new(sc64_timestamp_hit, ts);
    ts->ptimer = ptimer_init(bh, PTIMER_POLICY_DEFAULT);

    memory_region_init_io(&s->iomem, OBJECT(s), &sc64_timer_v2_ops, s,
                          TYPE_SC64_TIMER_V2, SC64_TIMER_V2_REG_MEM_SIZE);
    sysbus_init_mmio(dev, &s->iomem);

    return 0;
}

static void sc64_timer_v2_class_init(ObjectClass *klass, void *class_data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_timer_v2_init;
    dc->reset = sc64_timer_v2_reset;
    dc->vmsd = &vmstate_sc64_timer_v2;
}

static const TypeInfo sc64_timer_v2_info = {
    .name = TYPE_SC64_TIMER_V2,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64TimerStateV2),
    .class_init = sc64_timer_v2_class_init,
};

static void sc64_timer_v2_register_type(void)
{
    type_register_static(&sc64_timer_v2_info);
}

type_init(sc64_timer_v2_register_type)
