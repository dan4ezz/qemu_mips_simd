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
#include "qemu/log.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/error-report.h"
#include "qemu/typedefs.h"
#include <string.h>

#define TYPE_SC64_MONITOR "sc64-monitor"
#define SC64_MONITOR(obj) \
    OBJECT_CHECK(Sc64MonitorState, (obj), TYPE_SC64_MONITOR)

#define SENSOR_NUM 8

#define REG_NUM 39

#define TEMP_DEFAULT 638
#define VOLT_DEFAULT 800

#define MAX_VALUE    0x3FF

typedef struct Sc64MonitorState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    uint32_t regs[REG_NUM];
    MemoryRegion iomem;

    unsigned int temp[SENSOR_NUM];
    unsigned int volt[SENSOR_NUM];

    enum {
        STATE_REFRESH,
        STATE_MONITOR,
        STATE_IDLE,
    } state;

    qemu_irq irq;
} Sc64MonitorState;

/* Monitor registers */
#define REG_CONFIG0         0
# define CONFIG0_RST        (1 << 31)
#define REG_CONFIG1         1
#define CONFIG1_MCH_OFFT(n) (16 + (n))
#define CONFIG1_EN_OFFT(n)  (24 + (n))
# define CONFIG1_INTDTM     (1 << 11)
# define CONFIG1_INTV       (1 << 14)
# define CONFIG1_INTT       (1 << 15)
#define REG_CALIBRATE0      2
#define REG_CALIBRATE1      3
#define REG_RANGE_TEMP(n)   (4 + (n))
#define REG_RANGE_VOLT(n)   (12 + (n))
#define REG_RANGE_TVEL(n)   (20 + (n))
#define REG_SENSOR(n)       (28 + (n))
# define SENSOR_VLOW        (1 << 28)
# define SENSOR_VHIGH       (1 << 29)
# define SENSOR_TLOW        (1 << 30)
# define SENSOR_THIGH       (1 << 31)
#define REG_ADCCALC         36
#define ADCCALC_DATA_MASK   MAX_VALUE
# define ADCCALC_ALARM(n)   (1 << (24 + (n)))
#define REG_DTM_TCONFIG     37
#define REG_DTM_FDELTA0     38

#define GET_MCH(s, n) (((s->regs[REG_CONFIG1]) >> CONFIG1_MCH_OFFT(n)) & 1)
#define GET_EN(s, n) (((s->regs[REG_CONFIG1]) >> CONFIG1_EN_OFFT(n)) & 1)

typedef enum MonitorType {
    MONITOR_TEMP = 0,
    MONITOR_VOLT = 1
} MonitorType;

#define IRQ_FLAG(type) \
    ((type) == MONITOR_TEMP ? CONFIG1_INTT : CONFIG1_INTV)

#define SENSOR_UNDERFLOW_FLAG(type) \
    ((type) == MONITOR_TEMP ? SENSOR_THIGH : SENSOR_VLOW)
#define SENSOR_OVERFLOW_FLAG(type) \
    ((type) == MONITOR_TEMP ? SENSOR_TLOW : SENSOR_VHIGH)
#define SENSOR_EVENT_MASK \
    (SENSOR_TLOW | SENSOR_THIGH | SENSOR_VLOW | SENSOR_VHIGH)

static uint64_t sc64_monitor_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64MonitorState *s = (Sc64MonitorState *) opaque;
    int reg_idx = addr / 4;

    switch (reg_idx) {
    case REG_CONFIG0:
    case REG_CONFIG1:
    case REG_CALIBRATE0:
    case REG_CALIBRATE1:
    case REG_RANGE_TEMP(0)...REG_RANGE_TEMP(7):
    case REG_RANGE_VOLT(0)...REG_RANGE_VOLT(7):
    case REG_RANGE_TVEL(0)...REG_RANGE_TVEL(7):
    case REG_SENSOR(0)...REG_SENSOR(7):
    case REG_ADCCALC:
    case REG_DTM_TCONFIG:
    case REG_DTM_FDELTA0:
        return s->regs[reg_idx];
    default:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-monitor: read access to unknown register 0x"
                      TARGET_FMT_plx, addr);
    }

    return 0;
}

static void sc64_monitor_reset(Sc64MonitorState *s)
{
    int i;

    s->state = STATE_REFRESH;

    memset(s->regs, 0, 4 * REG_NUM);

    s->regs[REG_CONFIG1]     = 0xFF00003F;
    s->regs[REG_CALIBRATE0]  = 0x028B0537;
    s->regs[REG_CALIBRATE1]  = 0x04E2001E;
    s->regs[REG_ADCCALC]     = 0x00080000;
    s->regs[REG_DTM_TCONFIG] = 0x9253F01C;
    s->regs[REG_DTM_FDELTA0] = 0x44053805;

    for (i = 0; i < SENSOR_NUM; i++) {
        s->regs[REG_RANGE_TEMP(i)] = 0x023A02E4;
        s->regs[REG_RANGE_VOLT(i)] = 0x038402E4;
    }
}

static int sc64_range_max(Sc64MonitorState *s, int sensor, MonitorType type)
{
    if (type == MONITOR_TEMP) {
        return s->regs[REG_RANGE_TEMP(sensor)] & 0xFFFF;
    } else {
        return s->regs[REG_RANGE_VOLT(sensor)] >> 16;
    }
}

static int sc64_range_min(Sc64MonitorState *s, int sensor, MonitorType type)
{
    if (type == MONITOR_TEMP) {
        return s->regs[REG_RANGE_TEMP(sensor)] >> 16;
    } else {
        return s->regs[REG_RANGE_VOLT(sensor)] & 0xFFFF;
    }
}

static void sc64_monitor_update_irq(Sc64MonitorState *s)
{
    int i;
    int type;
    int do_irq = 0;

    if (s->state == STATE_MONITOR) {
        for (i = 0; i < SENSOR_NUM; i++) {
            type = GET_MCH(s, i);
            if (GET_EN(s, i) &&
                    (s->regs[REG_SENSOR(i)] & SENSOR_EVENT_MASK) &&
                    (s->regs[REG_CONFIG1] & IRQ_FLAG(type)) &&
                    (s->regs[REG_CONFIG1] & CONFIG1_INTDTM)) {
                    do_irq = 1;
            }
        }
    }

    qemu_set_irq(s->irq, do_irq);
}

static void sc64_sensor_update(Sc64MonitorState *s, int n,
        int value, MonitorType type)
{
    int mode, active_sensor;
    if (n < 0 || n >= SENSOR_NUM) {
        error_printf("Wrong sensor id: %d", n);
        return;
    }

    if (type == MONITOR_TEMP) {
        s->temp[n] = value;
    } else {
        s->volt[n] = value;
    }

    switch (s->state) {
    case STATE_REFRESH:
        mode = (s->regs[REG_CONFIG0] >> 14) & 0x3;
        active_sensor = s->regs[REG_CONFIG0] & 0x7;
        if (mode == type && n == active_sensor) {
            s->regs[REG_ADCCALC] &= ~ADCCALC_DATA_MASK;
            s->regs[REG_ADCCALC] |= MIN(value, MAX_VALUE);
        }

        break;
    case STATE_MONITOR:
        if (GET_EN(s, n) == 0) {
            break;
        }

        if (GET_MCH(s, n) != type) {
            break;
        }

        /* Clear all event flags */
        s->regs[REG_SENSOR(n)] = MIN(value, MAX_VALUE);
        s->regs[REG_ADCCALC] &= ~ADCCALC_ALARM(n);

        if (value < sc64_range_min(s, n, type)) {
            s->regs[REG_SENSOR(n)] |= SENSOR_UNDERFLOW_FLAG(type);
            s->regs[REG_ADCCALC] |= ADCCALC_ALARM(n);
        }

        if (value > sc64_range_max(s, n, type)) {
            s->regs[REG_SENSOR(n)] |= SENSOR_OVERFLOW_FLAG(type);
            s->regs[REG_ADCCALC] |= ADCCALC_ALARM(n);
        }

        break;
    case STATE_IDLE:
        /* Do nothing */
        break;
    }

    sc64_monitor_update_irq(s);
}

static void sc64_monitor_update(Sc64MonitorState *s)
{
    int i;
    for (i = 0; i < SENSOR_NUM; i++) {
        sc64_sensor_update(s, i, s->temp[i], MONITOR_TEMP);
        sc64_sensor_update(s, i, s->volt[i], MONITOR_VOLT);
    }
}

static void sc64_monitor_write(void *opaque, hwaddr addr,
                        uint64_t value, unsigned size)
{
    Sc64MonitorState *s = (Sc64MonitorState *) opaque;
    int reg_idx = addr / 4;
    int mode, i;

    switch (reg_idx) {
    case REG_CONFIG0:
        if (value & CONFIG0_RST) {
            sc64_monitor_reset(s);
            break;
        }

        s->regs[reg_idx] = value;
        mode = (value >> 14) & 0x3;

        switch (mode) {
        case 0: /* Get temperature */
        case 1: /* Get voltage */
            s->state = STATE_REFRESH;
            break;
        case 2: /* Monitor ranges */
            for (i = 0; i < SENSOR_NUM; i++) {
                s->regs[REG_SENSOR(i)] = 0;
            }

            s->state = STATE_MONITOR;
            break;
        case 3: /* Calibrate */
            s->state = STATE_IDLE; /* Not implemented */
            break;
        }

        sc64_monitor_update(s);
        break;
    case REG_CONFIG1:
        s->regs[reg_idx] = value;
        sc64_monitor_update(s);
        break;
    case REG_CALIBRATE0:
    case REG_CALIBRATE1:
    case REG_RANGE_TEMP(0)...REG_RANGE_TEMP(7):
    case REG_RANGE_VOLT(0)...REG_RANGE_VOLT(7):
    case REG_RANGE_TVEL(0)...REG_RANGE_TVEL(7):
    case REG_DTM_TCONFIG:
    case REG_DTM_FDELTA0:
        s->regs[reg_idx] = value;
        break;
    case REG_ADCCALC:
    case REG_SENSOR(0)...REG_SENSOR(7):
        /* Readonly registers */
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "sc64-monitor: write access to unknown register 0x"
                      TARGET_FMT_plx, addr);
    }
}

static const MemoryRegionOps sc64_monitor_ops = {
    .read = sc64_monitor_read,
    .write = sc64_monitor_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int sc64_monitor_init(SysBusDevice *dev)
{
    Sc64MonitorState *s = SC64_MONITOR(dev);
    int i;

    sysbus_init_irq(dev, &s->irq);

    memory_region_init_io(&s->iomem, OBJECT(s), &sc64_monitor_ops,
                            s, "sc64-monitor", 4 * REG_NUM);
    sysbus_init_mmio(dev, &s->iomem);

    sc64_monitor_reset(s);

    for (i = 0; i < SENSOR_NUM; i++) {
        s->temp[i] = TEMP_DEFAULT;
        s->volt[i] = VOLT_DEFAULT;
    }

    sc64_monitor_update(s);

    return 0;
}

static const VMStateDescription vmstate_sc64_monitor_regs = {
    .name = "sc64-monitor",
    .version_id = 1,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, Sc64MonitorState, REG_NUM),
        VMSTATE_END_OF_LIST(),
    },
};

static Property sc64_monitor_sysbus_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void sc64_monitor_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_monitor_init;
    dc->desc = "SC64 monitor block";
    dc->vmsd = &vmstate_sc64_monitor_regs;
    dc->props = sc64_monitor_sysbus_properties;
}

static void sc64_get_temperature(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    Sc64MonitorState *s = SC64_MONITOR(obj);
    int num = (uint64_t) opaque;
    int64_t value = s->temp[num];
    int t_ref = s->regs[REG_CALIBRATE1] & 0xFFFF;
    int n_ref = s->regs[REG_CALIBRATE0] >> 16;
    int kp = s->regs[REG_CALIBRATE0] & 0xFFFF;

    value = (double) t_ref + (n_ref - value) / (kp / 1000.);

    visit_type_int(v, name, &value, errp);
}

static void sc64_set_temperature(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    Sc64MonitorState *s = SC64_MONITOR(obj);
    Error *local_err = NULL;
    int64_t temp;
    int num = (uint64_t) opaque;
    int t_ref = s->regs[REG_CALIBRATE1] & 0xFFFF;
    int n_ref = s->regs[REG_CALIBRATE0] >> 16;
    int kp = s->regs[REG_CALIBRATE0] & 0xFFFF;

    visit_type_int(v, name, &temp, &local_err);

    temp = (double) -1. * (temp - t_ref) * (kp / 1000.) + n_ref;

    sc64_sensor_update(s, num, temp, MONITOR_TEMP);
}

static void sc64_get_voltage(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    Sc64MonitorState *s = SC64_MONITOR(obj);
    int num = (uint64_t) opaque;
    int64_t value = s->volt[num];
    int v_ref = s->regs[REG_CALIBRATE0] >> 16;

    value = (double) v_ref / 1023. * value;

    visit_type_int(v, name, &value, errp);
}

static void sc64_set_voltage(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    Sc64MonitorState *s = SC64_MONITOR(obj);
    Error *local_err = NULL;
    int64_t volt;
    int num = (uint64_t) opaque;
    int v_ref = s->regs[REG_CALIBRATE0] >> 16;

    visit_type_int(v, name, &volt, &local_err);

    volt = volt * 1023 / v_ref;

    sc64_sensor_update(s, num, volt, MONITOR_VOLT);
}

static const char *temp_sensor_names[] = {
    "temperature0",
    "temperature1",
    "temperature2",
    "temperature3",
    "temperature4",
    "temperature5",
    "temperature6",
    "temperature7",
};

static const char *volt_sensor_names[] = {
    "voltage0",
    "voltage1",
    "voltage2",
    "voltage3",
    "voltage4",
    "voltage5",
    "voltage6",
    "voltage7",
};

static void sc64_monitor_initfn(Object *obj)
{
    int i;

    for (i = 0; i < SENSOR_NUM; i++) {
        object_property_add(obj, temp_sensor_names[i], "int",
                            sc64_get_temperature,
                            sc64_set_temperature,
                            NULL, (void *) (uint64_t) i, NULL);
        object_property_add(obj, volt_sensor_names[i], "int",
                            sc64_get_voltage,
                            sc64_set_voltage,
                            NULL, (void *) (uint64_t) i, NULL);
    }
}

static const TypeInfo sc64_monitor_sysbus_info = {
    .name          = TYPE_SC64_MONITOR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64MonitorState),
    .class_init    = sc64_monitor_sysbus_class_init,
    .instance_init = sc64_monitor_initfn,
};

static void sc64_monitor_register_types(void)
{
    type_register_static(&sc64_monitor_sysbus_info);
}

type_init(sc64_monitor_register_types)
