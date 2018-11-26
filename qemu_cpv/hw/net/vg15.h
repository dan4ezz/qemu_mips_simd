/*
 * QEMU DEC 21143 (Tulip) emulation
 *
 * Copyright (C) 2011, 2013 Antony Pavlov
 * Copyright (C) 2013 Dmitry Smagin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_TULIP_H
#define HW_TULIP_H

#include "tulip_mdio.h"

#define TULIP_CSR_REGION_SIZE       0x80
#define VG15M_REGION_SIZE           0x800
#define VG15E_REGION_SIZE           0x200

#define VG15_REGISTERS_COUNT \
    (MAX(VG15M_REGION_SIZE, VG15E_REGION_SIZE) >> 2)

#define VG15E_ID (0x191e | (0x3314 << 16))
#define VG15M_ID (0x191e | (0x3315 << 16))

#define MAX_PACKET_SIZE 65536

typedef struct VG15State {
    NICState *nic;
    NICConf conf;
    QEMUTimer *timer;
    MemoryRegion mmio;

    qemu_irq irq;

    uint32_t devid;

    void (*phys_mem_read)(void *dma_opaque, hwaddr addr,
                          uint8_t *buf, int len);
    void (*phys_mem_write)(void *dma_opaque, hwaddr addr,
                           uint8_t *buf, int len);
    void *dma_opaque;

    eeprom_t *eeprom;
    struct MiiTransceiver mii;

    uint32_t mac_reg[VG15_REGISTERS_COUNT];

    uint8_t tx_buf[MAX_PACKET_SIZE];

    uint64_t cur_tx_desc;
    uint64_t cur_rx_desc;
    int tx_polling;
} VG15State;

int vg15_init(DeviceState *dev, VG15State *s, NetClientInfo *info);
void vg15_cleanup(VG15State *s);
void vg15_reset(void *opaque);

void vg15_timer(void *opaque);

uint64_t vg15e_read(void *opaque, hwaddr addr, unsigned size);
void vg15e_write(void *opaque, hwaddr addr, uint64_t val, unsigned size);
uint64_t vg15m_read(void *opaque, hwaddr addr, unsigned size);
void vg15m_write(void *opaque, hwaddr addr, uint64_t val, unsigned size);

int vg15_can_receive(NetClientState *nc);
ssize_t vg15_receive(NetClientState *nc, const uint8_t *buf, size_t size);
void vg15_set_link_status(NetClientState *nc);

#endif
