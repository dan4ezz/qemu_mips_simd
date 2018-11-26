/*
 * SC64 NAND controller declarations.
 *
 * Copyright (C) 2015 Antony Pavlov <antonynpavlov@gmail.com>
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

#ifndef HW_BLOCK_DIGIC_TIMER_H
#define HW_BLOCK_DIGIC_TIMER_H

#include "hw/sysbus.h"
#include "qemu/typedefs.h"

#define TYPE_SC64_NAND "sc64-nand"
#define SC64_NAND(obj) OBJECT_CHECK(SC64NANDState, (obj), TYPE_SC64_NAND)

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    DeviceState *nand;
    uint8_t manf_id;
    uint8_t chip_id;

    MemoryRegion iomem;
    unsigned int rdy:1;
    unsigned int ale:1;
    unsigned int cle:1;
    unsigned int ce:1;
} SC64NANDState;

extern void sc64_nand_flash_register(hwaddr base);

#endif /* HW_BLOCK_DIGIC_TIMER_H */
