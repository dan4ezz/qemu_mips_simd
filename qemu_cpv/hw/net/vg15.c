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

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/qdev.h"
#include "hw/hw.h"
#include "net/net.h"
#include "net/checksum.h"
#include "sysemu/sysemu.h"
#include "sysemu/dma.h"
#include "qemu/timer.h"

#include "hw/nvram/eeprom93xx.h"
#include "vg15.h"
#include "tulip_mdio.h"

#define TULIP_CSR0  0x00
 #define TULIP_CSR0_SWR_TX     0x01
 #define TULIP_CSR0_SWR_RX     0x02
 #define TULIP_CSR0_TAP_MASK    (0x03 << 17)
 #define VG15_CSR0_DBO         (1 << 20)
#define TULIP_CSR1  0x08
#define TULIP_CSR2  0x10
#define TULIP_CSR3  0x18
#define TULIP_CSR4  0x20
#define TULIP_CSR5  0x28
 #define TULIP_CSR5_TI         (1 <<  0) /* Transmit Interrupt */
 #define TULIP_CSR5_RI         (1 <<  6) /* Receive Interrupt */
 #define TULIP_CSR5_NIS        (1 << 16) /* Or'ed bits 0, 2, 6, 11, 14  */
 #define TULIP_CSR5_NIS_SHIFT  (16)
 #define TULIP_CSR5_RS_MASK    (7 << 17)
 #define TULIP_CSR5_TS_MASK    (7 << 20)

#define TULIP_CSR6  0x30 /* Operation Mode Register */
 #define TULIP_CSR6_TXON    0x2000
 #define TULIP_CSR6_RXON    0x0002
 #define TULIP_CSR6_EDE     (1 << 19)

#define TULIP_CSR7  0x38 /* Interrupt Enable Register */
#define TULIP_CSR8  0x40
#define TULIP_CSR9  0x48 /* Boot ROM, Serial ROM, and MII Management Register */
 #define TULIP_CSR9_MDI         (1 << 19) /* MDIO Data In */
 #define TULIP_CSR9_MDOM        (1 << 18) /* MDIO Operation Mode */
 #define TULIP_CSR9_MDO         (1 << 17) /* MDIO Data Out */
 #define TULIP_CSR9_MDC         (1 << 16) /* MDIO Clock */
 #define TULIP_CSR9_RD          (1 << 14)
 #define TULIP_CSR9_WR          (1 << 13)
 #define TULIP_CSR9_SR          (1 << 11) /* Serial ROM Select */
 #define TULIP_CSR9_SRDO        (1 << 3) /* Serial ROM Data Out */
 #define TULIP_CSR9_SRDI        (1 << 2) /* Serial ROM Data In */
 #define TULIP_CSR9_SRCK        (1 << 1) /* Serial ROM Clock */
 #define TULIP_CSR9_SRCS        (1) /* Serial ROM Chip Select */
#define TULIP_CSR10 0x50
#define VG15_CSR10_MDIO     (1 << 31)
#define VG15_CSR10_SROM     (1 << 30)

#define VG15_CSR10_MDIO_RW      (1 << 26)
#define VG15_CSR10_SROM_READ    2


#define TULIP_CSR11 0x58

#define TULIP_CSR12 0x60

#define TULIP_CSR13 0x68

#define TULIP_CSR14 0x70 /* SIA Transmit and Receive Register */
#define TULIP_CSR15 0x78

/* VG15e-specific registers. Reserved, except 0x118 */
#define VG15_E_PHY_START 0x80
#define VG15_E_PHY_END   0x200

/* VG15m-specific registers */
#define VG15_M_CRDAL     0x180 /* CSR2 */
#define VG15_M_CRDAH     0x184
#define VG15_M_CTDAL     0x188 /* CSR1 */
#define VG15_M_CTDAH     0x18C
#define VG15_M_RBAL      0x190 /* CSR3 */
#define VG15_M_RBAH      0x194
#define VG15_M_TBAL      0x198 /* CSR4 */
#define VG15_M_TBAH      0x19C

#define defreg(x)   x = (TULIP_##x>>2)
enum {
    defreg(CSR0),
    defreg(CSR1),
    defreg(CSR2),
    defreg(CSR3),
    defreg(CSR4),
    defreg(CSR5),
    defreg(CSR6),
    defreg(CSR7),
    defreg(CSR8),
    defreg(CSR9),
    defreg(CSR10),
    defreg(CSR11),
    defreg(CSR12),
    defreg(CSR13),
    defreg(CSR14),
    defreg(CSR15),
};

/* The Tulip Receive Descriptor */
struct tulip_rx_desc {
    uint32_t status;
    uint32_t status_aux;
    uint32_t frame_cs;
    uint32_t buffer2;
    uint32_t buffer1;
    uint32_t length;
};

#define TULIP_RDES0_OWN   (1 << 31)
#define TULIP_RDES0_FS    (1 <<  9) /* First descriptor */
#define TULIP_RDES0_LS    (1 <<  8) /* Last descriptor */
#define TULIP_RDES1_FT    (1 << 3)

/* The Tulip Transmit Descriptor */
struct tulip_tx_desc {
    uint32_t status;
    uint32_t header_cs;
    uint32_t checksum;
    uint32_t buffer2;  /* Linux use only buffer 1. */
    uint32_t buffer1;
    uint32_t length;
};

#define TULIP_TDES0_OWN   (1 << 31)

#define TULIP_TDES1_IC    (1 << 31) /* Interrupt on Completion */
#define TULIP_TDES1_LS    (1 << 30) /* Last Segment */
#define TULIP_TDES1_FS    (1 << 29) /* First Segment */
#define TULIP_TDES1_SET   (1 << 27) /* Setup Packet */
#define TULIP_TDES1_TER   (1 << 25) /* Transmit End Of Ring */

#define VG15_TDES2_CE     (1 << 28)
#define VG15_TDES2_TCP     (1 << 29)

struct vg15m_rx_desc {
    uint32_t status;
    uint32_t status_aux;
    uint32_t timestamp_lo;
    uint32_t timestamp_hi;
    uint32_t checksum;
    uint32_t length;
    uint32_t next_lo;
    uint32_t next_hi;
    uint32_t data_lo;
    uint32_t data_hi;
};

struct vg15m_tx_desc {
    uint32_t status;
    uint32_t timestamp_lo;
    uint32_t timestamp_hi;
    uint32_t header_cs;
    uint32_t checksum;
    uint32_t length;
    uint32_t next_lo;
    uint32_t next_hi;
    uint32_t data_lo;
    uint32_t data_hi;
};

#define VG15_M_DATA_ADDR(d) \
    (((uint64_t) (d)->data_hi << 32) | (d)->data_lo)

#define VG15_M_NEXT_ADDR(d) \
    (((uint64_t) (d)->next_hi << 32) | (d)->next_lo)

/* Common structures for both  VG15e and VG15m */
struct vg15_tx_descr {
    uint32_t status;
    uint32_t header_cs;
    union {
        uint64_t timestamp; /* VG15m only */
        uint64_t padding;
    };
    uint32_t checksum;
    uint32_t length;
    uint64_t next;
    uint64_t data;
};

struct vg15_rx_descr {
    uint32_t status;
    uint32_t status_aux;
    union {
        uint64_t timestamp; /* VG15m only */
        uint64_t padding;
    };
    uint32_t checksum;
    uint32_t length;
    uint64_t next;
    uint64_t data;
};

#define EEPROM_SIZE 64
#define EEPROM_MACADDR_OFFSET 10
/*
 * DEC has developed its own EEPROM format
 * see Digital Semiconductor 21X4 Serial ROM Format ver. 4.05 2-Mar-1998
 * for details.
 *
 * Also see tulip-diag utility code from nictools-pci package
 *     http://ftp.debian.org/debian/pool/main/n/nictools-pci/
 */

static const
uint16_t vg15_eeprom[EEPROM_SIZE] = {
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
 0xfa00, 0xffff, 0x6000, 0x0001, 0x012e, 0xffff, 0xffff, 0xffff,
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
};

#define GB_SROM_LEN     126
#define GB_SROM_POLY    0x04c11db6

static void process_tx(VG15State *s);

static inline uint32_t field_to_cpu(uint32_t field, int dbo_big)
{
    return dbo_big ? be32_to_cpu(field) : le32_to_cpu(field);
}

/* calculate SROM CRC */
static unsigned short gbSromCrcGet(unsigned char *data)
{
    unsigned crc, flippedCRC, ix, bit, msb;
    unsigned char val;

    for (ix = 0, crc = 0xffffffff; ix < GB_SROM_LEN; ix++) {
        val = data[ix];

        for (bit = 0; bit < 8; bit++) {
            msb = (crc >> 31) & 1;
            crc <<= 1;

            if (msb ^ (val & 1)) {
                crc ^= GB_SROM_POLY;
                crc |= 1;
            }

            val >>= 1;
        }
    }

    for (ix = 0, flippedCRC = 0; ix < 32; ix++) {
        flippedCRC <<= 1;
        bit = crc & 1;
        crc >>= 1;
        flippedCRC += bit;
    }

    crc = flippedCRC ^ 0xffffffff;

    return crc & 0xffff;
}

static void tulip_update_irq(VG15State *s);

static inline uint32_t mac_readreg(VG15State *s, int index)
{
    return s->mac_reg[index];
}

static inline void mac_writereg(VG15State *s, int index, uint32_t val)
{
    s->mac_reg[index] = val;
}

/* FIXME: we use external MII tranceiver, so we must change it status here */
static void tulip_set_link_status(NetClientState *nc)
{
    VG15State *s = qemu_get_nic_opaque(nc);
    s->mii.set_link_status(&s->mii, nc->link_down);
}

int vg15_can_receive(NetClientState *nc)
{
    VG15State *s = qemu_get_nic_opaque(nc);

    return mac_readreg(s, CSR6) & TULIP_CSR6_RXON;
}

#define PROTO_TCP  6
#define PROTO_UDP 17

static uint32_t vg15_checksum(VG15State *s, uint8_t *data, int length)
{
    uint32_t csr6 = mac_readreg(s, CSR6);
    int hlen, plen, proto, csum_offset;
    uint32_t csum = 0;

    if ((data[14] & 0xf0) != 0x40) {
        return 0xbaadbaad; /* not IPv4 */
    }
    hlen  = (data[14] & 0x0f) * 4;
    plen  = (data[16] << 8 | data[17]) - hlen;
    proto = data[23];

    switch (proto) {
    case PROTO_TCP:
        csum_offset = 16;
        break;
    case PROTO_UDP:
        csum_offset = 6;
        break;
    default:
        return 0xbaadbaad;
    }

    if (plen < csum_offset+2) {
        return 0xbaadbaad;
    }

    if (csr6 & TULIP_CSR6_EDE) {
        csum = net_checksum_add(plen + hlen, data + 14);
    } else {
        csum = net_checksum_add(length, data);
    }
    csum = ~net_checksum_finish(csum);
    return (csum << 24) | ((csum & 0xff00) << 8);
}

static void vg15_store_rx_addr(VG15State *s, uint64_t cur_rx_desc)
{
    switch (s->devid) {
    case VG15E_ID:
        mac_writereg(s, CSR2, cur_rx_desc);
    case VG15M_ID:
        mac_writereg(s, VG15_M_CRDAL >> 2, cur_rx_desc);
        mac_writereg(s, VG15_M_CRDAH >> 2, cur_rx_desc >> 32);
    }
}

static void vg15_store_tx_addr(VG15State *s, uint64_t cur_tx_desc)
{
    switch (s->devid) {
    case VG15E_ID:
        mac_writereg(s, CSR1, cur_tx_desc);
    case VG15M_ID:
        mac_writereg(s, VG15_M_CTDAL >> 2, cur_tx_desc);
        mac_writereg(s, VG15_M_CTDAH >> 2, cur_tx_desc >> 32);
    }
}

static uint64_t vg15_load_start_tx_addr(VG15State *s)
{
    int ret = 0;

    switch (s->devid) {
    case VG15E_ID:
        ret = mac_readreg(s, CSR4);
        break;
    case VG15M_ID:
        ret = ((uint64_t) mac_readreg(s, VG15_M_TBAH >> 2)) << 32 |
                          mac_readreg(s, VG15_M_TBAL >> 2);
        break;
    }
    return ret;
}

static void vg15_read_tx_descr(VG15State *s, struct vg15_tx_descr *descr,
                              uint64_t cur_tx_desc, uint32_t dbo_big)
{
    struct tulip_tx_desc vg15e_desc;
    struct vg15m_tx_desc vg15m_desc;

    switch (s->devid) {
    case VG15E_ID:
        s->phys_mem_read(s->dma_opaque, cur_tx_desc, (void *)&vg15e_desc,
                         sizeof(vg15e_desc));
        descr->status = field_to_cpu(vg15e_desc.status, dbo_big);
        descr->header_cs = field_to_cpu(vg15e_desc.header_cs, dbo_big);
        descr->checksum = field_to_cpu(vg15e_desc.checksum, dbo_big);
        descr->length = field_to_cpu(vg15e_desc.length, dbo_big);
        descr->next = field_to_cpu(vg15e_desc.buffer2, dbo_big);
        descr->data = field_to_cpu(vg15e_desc.buffer1, dbo_big);
        descr->timestamp = 0;
        break;
    case VG15M_ID:
        s->phys_mem_read(s->dma_opaque, cur_tx_desc, (void *)&vg15m_desc,
                         sizeof(vg15m_desc));
        descr->status = field_to_cpu(vg15m_desc.status, dbo_big);
        descr->header_cs = field_to_cpu(vg15m_desc.header_cs, dbo_big);
        descr->checksum = field_to_cpu(vg15m_desc.checksum, dbo_big);
        descr->length = field_to_cpu(vg15m_desc.length, dbo_big);
        descr->next = field_to_cpu(VG15_M_NEXT_ADDR(&vg15m_desc), dbo_big);
        descr->data = field_to_cpu(VG15_M_DATA_ADDR(&vg15m_desc), dbo_big);
        descr->timestamp = 0;
        break;
    }
}

static void vg15_write_tx_descr(VG15State *s, struct vg15_tx_descr *descr,
                              uint64_t cur_tx_desc, uint32_t dbo_big)
{
    struct tulip_tx_desc vg15e_desc;
    struct vg15m_tx_desc vg15m_desc;

    switch (s->devid) {
    case VG15E_ID:
        vg15e_desc.status = field_to_cpu(descr->status, dbo_big);
        vg15e_desc.header_cs = field_to_cpu(descr->header_cs, dbo_big);
        vg15e_desc.checksum = field_to_cpu(descr->checksum, dbo_big);
        vg15e_desc.length = field_to_cpu(descr->length, dbo_big);
        vg15e_desc.buffer2 = field_to_cpu(descr->next, dbo_big);
        vg15e_desc.buffer1 = field_to_cpu(descr->data, dbo_big);
        s->phys_mem_write(s->dma_opaque, cur_tx_desc, (void *)&vg15e_desc,
                         sizeof(vg15e_desc));
        break;
    case VG15M_ID:
        vg15m_desc.status = field_to_cpu(descr->status, dbo_big);
        vg15m_desc.timestamp_hi = field_to_cpu(descr->timestamp >> 32, dbo_big);
        vg15m_desc.timestamp_lo = field_to_cpu(descr->timestamp, dbo_big);
        vg15m_desc.header_cs = field_to_cpu(descr->header_cs, dbo_big);
        vg15m_desc.checksum = field_to_cpu(descr->checksum, dbo_big);
        vg15m_desc.length = field_to_cpu(descr->length, dbo_big);
        vg15m_desc.next_hi = field_to_cpu(descr->next >> 32, dbo_big);
        vg15m_desc.next_lo = field_to_cpu(descr->next, dbo_big);
        vg15m_desc.data_hi = field_to_cpu(descr->data >> 32, dbo_big);
        vg15m_desc.data_lo = field_to_cpu(descr->data, dbo_big);
        s->phys_mem_write(s->dma_opaque, cur_tx_desc, (void *)&vg15m_desc,
                         sizeof(vg15m_desc));
        break;
    }
}

static void vg15_read_rx_descr(VG15State *s, struct vg15_rx_descr *descr,
                              uint64_t cur_rx_desc, uint32_t dbo_big)
{
    struct tulip_rx_desc vg15e_desc;
    struct vg15m_rx_desc vg15m_desc;

    switch (s->devid) {
    case VG15E_ID:
        s->phys_mem_read(s->dma_opaque, cur_rx_desc, (void *)&vg15e_desc,
                         sizeof(vg15e_desc));
        descr->status = field_to_cpu(vg15e_desc.status, dbo_big);
        descr->status_aux = field_to_cpu(vg15e_desc.status_aux, dbo_big);
        descr->checksum = field_to_cpu(vg15e_desc.frame_cs, dbo_big);
        descr->length = field_to_cpu(vg15e_desc.length, dbo_big);
        descr->next = field_to_cpu(vg15e_desc.buffer2, dbo_big);
        descr->data = field_to_cpu(vg15e_desc.buffer1, dbo_big);
        descr->timestamp = 0;
        break;
    case VG15M_ID:
        s->phys_mem_read(s->dma_opaque, cur_rx_desc, (void *)&vg15m_desc,
                         sizeof(vg15m_desc));
        descr->status = field_to_cpu(vg15m_desc.status, dbo_big);
        descr->status_aux = field_to_cpu(vg15m_desc.status_aux, dbo_big);
        descr->checksum = field_to_cpu(vg15m_desc.checksum, dbo_big);
        descr->length = field_to_cpu(vg15m_desc.length, dbo_big);
        descr->next = field_to_cpu(VG15_M_NEXT_ADDR(&vg15m_desc), dbo_big);
        descr->data = field_to_cpu(VG15_M_DATA_ADDR(&vg15m_desc), dbo_big);
        descr->timestamp = 0;
        break;
    }
}

static void vg15_write_rx_descr(VG15State *s, struct vg15_rx_descr *descr,
                              uint64_t cur_rx_desc, uint32_t dbo_big)
{
    struct tulip_rx_desc vg15e_desc;
    struct vg15m_rx_desc vg15m_desc;

    switch (s->devid) {
    case VG15E_ID:
        vg15e_desc.status = field_to_cpu(descr->status, dbo_big);
        vg15e_desc.status_aux = field_to_cpu(descr->status_aux, dbo_big);
        vg15e_desc.frame_cs = field_to_cpu(descr->checksum, dbo_big);
        vg15e_desc.length = field_to_cpu(descr->length, dbo_big);
        vg15e_desc.buffer2 = field_to_cpu(descr->next, dbo_big);
        vg15e_desc.buffer1 = field_to_cpu(descr->data, dbo_big);
        s->phys_mem_write(s->dma_opaque, cur_rx_desc, (void *)&vg15e_desc,
                         sizeof(vg15e_desc));
        break;
    case VG15M_ID:
        vg15m_desc.status = field_to_cpu(descr->status, dbo_big);
        vg15m_desc.status_aux = field_to_cpu(descr->status_aux, dbo_big);
        vg15m_desc.timestamp_hi = field_to_cpu(descr->timestamp >> 32, dbo_big);
        vg15m_desc.timestamp_lo = field_to_cpu(descr->timestamp, dbo_big);
        vg15m_desc.checksum = field_to_cpu(descr->checksum, dbo_big);
        vg15m_desc.length = field_to_cpu(descr->length, dbo_big);
        vg15m_desc.next_hi = field_to_cpu(descr->next >> 32, dbo_big);
        vg15m_desc.next_lo = field_to_cpu(descr->next, dbo_big);
        vg15m_desc.data_hi = field_to_cpu(descr->data >> 32, dbo_big);
        vg15m_desc.data_lo = field_to_cpu(descr->data, dbo_big);
        s->phys_mem_write(s->dma_opaque, cur_rx_desc, (void *)&vg15m_desc,
                         sizeof(vg15m_desc));
        break;
    }
}

ssize_t vg15_receive(NetClientState *nc, const uint8_t *buf,
                             size_t size)
{
    VG15State *s = qemu_get_nic_opaque(nc);
    struct vg15_rx_descr desc;
    uint32_t status;
    uint64_t data, next_descr_addr;
    uint64_t cur_rx_desc;
    uint32_t dbo_big = mac_readreg(s, CSR0) & VG15_CSR0_DBO;

    if (!vg15_can_receive(nc)) {
        return -1;
    }

    cur_rx_desc = s->cur_rx_desc;
    vg15_store_rx_addr(s, cur_rx_desc);

    vg15_read_rx_descr(s, &desc, cur_rx_desc, dbo_big);

    status = desc.status;
    data = desc.data;
    next_descr_addr = desc.next;

    /* FIXME: check desc.length too */
    if (!(status & TULIP_RDES0_OWN)) {
        return -1;
    }

    desc.status = (((size + 4) << 16) & 0x3fff0000)
        | TULIP_RDES0_FS | TULIP_RDES0_LS;
    desc.status_aux = ((size + 4) << 16) |
        (size + 4 > 1500 ? TULIP_RDES1_FT : 0);
    desc.checksum = vg15_checksum(s, (void *)buf, size);

    s->phys_mem_write(s->dma_opaque, data, (uint8_t *)buf, size);
    vg15_write_rx_descr(s, &desc, cur_rx_desc, dbo_big);
    mac_writereg(s, CSR5, TULIP_CSR5_RI | mac_readreg(s, CSR5));

    s->cur_rx_desc = next_descr_addr;

    tulip_update_irq(s);

    return size;
}

static inline void tulip_csr0_write(VG15State *s, uint32_t val)
{
    if (val & TULIP_CSR0_SWR_RX || val & TULIP_CSR0_SWR_TX) {
        vg15_reset(s);
        return;
    }

    if (val & TULIP_CSR0_TAP_MASK) {
        s->tx_polling = 1;
    } else {
        s->tx_polling = 0;
    }

    mac_writereg(s, CSR0, val);
}

static inline void tulip_csr5_write(VG15State *s, uint32_t val)
{
    uint32_t csr5_write_mask = 0x02fe0000;

    val = val & (~csr5_write_mask);

    mac_writereg(s, CSR5, mac_readreg(s, CSR5) & (~val));
}

static inline void tulip_csr6_write(VG15State *s, uint32_t val)
{
    mac_writereg(s, CSR6, val);
}

static inline void tulip_csr_1_2_write(VG15State *s, uint32_t val)
{
    if (mac_readreg(s, CSR6) & TULIP_CSR6_TXON) {
        process_tx(s);
        tulip_update_irq(s);
    }
}

static inline void tulip_csr9_write(VG15State *s, uint32_t val)
{
    if (1) {
        int srdo;
        int srcs = ((val & TULIP_CSR9_SRCS) != 0);
        int srck = ((val & TULIP_CSR9_SRCK) != 0);
        int srdi = ((val & TULIP_CSR9_SRDI) != 0);

        eeprom93xx_write(s->eeprom, srcs, srck, srdi);

        srdo = eeprom93xx_read(s->eeprom);
        if (srdo) {
            val |= TULIP_CSR9_SRDO;
        } else {
            val &= ~TULIP_CSR9_SRDO;
        }
    }

    if (1) {
        int mdo;

        mdo = mii_tick(&s->mii, (val & TULIP_CSR9_MDC) >> 16,
                        (val & TULIP_CSR9_MDO) >> 17);

        if (val & TULIP_CSR9_MDOM) {
            if (mdo) {
                val |= TULIP_CSR9_MDI;
            } else {
                val &= ~TULIP_CSR9_MDI;
            }
        }
    }
    mac_writereg(s, CSR9, val);
}

static inline void vg15_csr10_write(VG15State *s, uint32_t val)
{
    uint32_t i;

    if (val & VG15_CSR10_MDIO) { /* mdio access */
        uint32_t id, my_id = s->mii.mii_data[31];

        id = (val >> 21) & 0x1f;
        if (id == my_id) {
            i = (val >> 16) & 0x1f;
            if (val & VG15_CSR10_MDIO_RW) { /* write access */
                val = val & 0xffff;
                if (i == PHY_CTRL) {
                    val &= ~PHY_CTRL_RST; /* don't set reset bit */
                }
                s->mii.mii_data[i] = val;
            } else { /* read access */
                val = s->mii.mii_data[i];
            }
        } else {
            val = 0xffff;
        }
    }

    if (val & VG15_CSR10_SROM) { /* srom access */
        uint32_t op;
        i = (val >> 16) & 0xff;
        op = (val >> 24) & 3;
        switch (op) {
        case VG15_CSR10_SROM_READ: /* read operation */
            val = eeprom93xx_data(s->eeprom)[i];
            break;
        default:
            printf("VG15: SROM access via CSR10: operation not implemented");
            break;
        }
    }

    mac_writereg(s, CSR10, val);
}

/* Just now we don't use MAC-address filtering (we always use promisc mode),
   so just drop any setup frame */

static void process_setup_frame(VG15State *s, uint8_t *a)
{
}

static void process_tx(VG15State *s)
{
    struct vg15_tx_descr desc;
    uint32_t status, length;
    uint64_t data, next_descr_addr;
    uint32_t checksum;
    uint8_t do_hw_cs;
    uint64_t cur_tx_desc;
    uint32_t dbo_big = mac_readreg(s, CSR0) & VG15_CSR0_DBO;

    int cur_offset;
    int frame_length;

    cur_offset = 0;
    frame_length = 0;

    cur_tx_desc = s->cur_tx_desc;
    vg15_store_tx_addr(s, cur_tx_desc);

    while (1) {
        uint32_t to_copy;

        vg15_read_tx_descr(s, &desc, cur_tx_desc, dbo_big);

        status = desc.status;
        length = desc.length;
        data = desc.data;
        next_descr_addr = desc.next;
        checksum = desc.checksum;

        if (!(status & TULIP_TDES0_OWN)) {
            break;
        }


        to_copy = 0x7ff & length;

        /* First Segment */
        if (length & TULIP_TDES1_FS) {
            cur_offset = 0;
            frame_length = to_copy;
            if (checksum & VG15_TDES2_CE) {
                do_hw_cs = 1;
            } else {
                do_hw_cs = 0;
            }
        }

        if (length & TULIP_TDES1_SET) { /* Setup Frame */
            /* FIXME: check (to_copy == 192) */
            s->phys_mem_read(s->dma_opaque, data, s->tx_buf, to_copy);

            process_setup_frame(s, s->tx_buf);
        } else {

            s->phys_mem_read(s->dma_opaque, data,
                              (uint8_t *)(s->tx_buf + cur_offset), to_copy);

            cur_offset += to_copy;

            /* ! First Segment */
            if (!(length & TULIP_TDES1_FS)) {
                frame_length += to_copy;
            }

            /* Last Segment */
            if (length & TULIP_TDES1_LS) {
                if (do_hw_cs) {
                    net_checksum_calculate(s->tx_buf, frame_length);
                }
                qemu_send_packet(qemu_get_queue(s->nic), s->tx_buf,
                                frame_length);
            }
        }

        desc.status = status & ~TULIP_TDES0_OWN;
        vg15_write_tx_descr(s, &desc, cur_tx_desc, dbo_big);

        /* Last Segment */
        if (length & TULIP_TDES1_LS) {
            /* FIXME: check IC */
            /* FIXME: mac_writereg -> tulip_csr5_write */
            mac_writereg(s, CSR5, TULIP_CSR5_TI | mac_readreg(s, CSR5));
        }

        if (length & TULIP_TDES1_TER) {
            cur_tx_desc = vg15_load_start_tx_addr(s);
            s->cur_tx_desc = cur_tx_desc;

            continue;
        }

        cur_tx_desc = next_descr_addr;
        s->cur_tx_desc = cur_tx_desc;
    }
}

static void vg15_csr_write(void *opaque, uint32_t index, uint64_t val,
                 unsigned size)
{
    VG15State *s = opaque;

    switch (index) {
    case TULIP_CSR0: /* Bus Mode Register */
        tulip_csr0_write(s, (uint32_t)val);
        break;

    case TULIP_CSR2: /* Receive Poll Demand/Current Descriptor Address */
    case TULIP_CSR1: /* Transmit Poll Demand/Current Descriptor Address */
        tulip_csr_1_2_write(s, val);
        break;

    case TULIP_CSR3: /* Start of Receive List */
        mac_writereg(s, CSR3, val);
        s->cur_rx_desc = val;
        break;

    case TULIP_CSR4: /* Start of Transmit List */
        mac_writereg(s, CSR4, val);
        s->cur_tx_desc = val;
        break;

    case TULIP_CSR5: /* Status Register */
        tulip_csr5_write(s, (uint32_t)val);
        tulip_update_irq(s);
        break;

    case TULIP_CSR6: /* Command/Mode Register */
        tulip_csr6_write(s, (uint32_t)val);
        break;

    case TULIP_CSR7:
        mac_writereg(s, CSR7, val);
        break;

    case TULIP_CSR9: /* Boot/Serial ROM and MII Management Register */
        tulip_csr9_write(s, (uint32_t)val);
        break;

    case TULIP_CSR10: /* Boot/Serial ROM and MII Management Register */
        vg15_csr10_write(s, (uint32_t)val);
        break;

    case TULIP_CSR13:
        mac_writereg(s, CSR13, val);
        break;

    case TULIP_CSR14:
    case TULIP_CSR15:
        break;

    default:
        printf("tulip: write access to unknown register %x\n", index);
        qemu_log_mask(LOG_UNIMP,
            "tulip: write access to unknown register 0x%x", index);
    }
}

static uint64_t vg15_csr_read(void *opaque, uint32_t index, unsigned size)
{
    VG15State *s = opaque;
    uint64_t ret;

    ret = 0;

    switch (index) {
    case TULIP_CSR0: /* Bus Mode Register */
    case TULIP_CSR3: /* Start of Receive List */
    case TULIP_CSR4: /* Start of Transmit List */
    case TULIP_CSR5: /* Status Register */
    case TULIP_CSR6: /* Command/Mode Register */
    case TULIP_CSR7:
    case TULIP_CSR8:
    case TULIP_CSR9: /* Boot/Serial ROM and MII Management Register */
    case TULIP_CSR10:
    case TULIP_CSR12:
    case TULIP_CSR13:
    /* FIXME: tulip_csr_read: unimplemented register index = 70 */
    case TULIP_CSR14:
    case TULIP_CSR15:
        ret = (uint64_t)mac_readreg(s, (index >> 2));
        break;

    case TULIP_CSR1: /* Transmit Current Descriptor Address */
        ret = (uint64_t)s->cur_tx_desc;
        break;

    case TULIP_CSR2: /* Receive Current Descriptor Address */
        ret = (uint64_t)s->cur_rx_desc;
        break;

    default:
        printf("tulip: read access to unknown register %x\n", index);
        qemu_log_mask(LOG_UNIMP,
            "tulip: read access to unknown register 0x%x", index);

    }

    return ret;
}

void vg15e_write(void *opaque, hwaddr addr, uint64_t val,
                 unsigned size)
{
    unsigned int index = addr & (VG15E_REGION_SIZE - 1);

    switch (index) {
    case 0 ... (TULIP_CSR_REGION_SIZE - 1):
        vg15_csr_write(opaque, index, val, size);
        break;
    case VG15_E_PHY_START ... VG15_E_PHY_END:
        /* DO NOTHING */
        break;
    default:
        printf("vg15e: Write to unknown register %lx\n", addr);
    }
}

uint64_t vg15e_read(void *opaque, hwaddr addr, unsigned size)
{
    unsigned int index = addr & (VG15E_REGION_SIZE - 1);

    switch (index) {
    case 0 ... (TULIP_CSR_REGION_SIZE - 1):
        return vg15_csr_read(opaque, index, size);
    case VG15_E_PHY_START ... VG15_E_PHY_END:
        /* Return 0, as stated in the documentation. */
        return 0;
    default:
        printf("vg15e: Read from unknown register %lx\n", addr);
        return 0;
    }
}

void vg15m_write(void *opaque, hwaddr addr, uint64_t val,
                 unsigned size)
{
    VG15State *s = opaque;
    unsigned int index = addr & (VG15M_REGION_SIZE - 1);

    switch (index) {
    case 0 ... (TULIP_CSR_REGION_SIZE - 1):
        vg15_csr_write(opaque, index, val, size);
        break;
    case VG15_M_CRDAL: /* CSR2 */
    case VG15_M_CTDAL: /* CSR1 */
        tulip_csr_1_2_write(s, val);
        break;
    case VG15_M_CRDAH:
    case VG15_M_CTDAH:
        /* DO NOTHING */
        break;
    case VG15_M_RBAH:
        val &= 0xF; /* Clear all bits except 0..3 */
    case VG15_M_RBAL: /* CSR3 */
        mac_writereg(s, index >> 2, val);
        s->cur_rx_desc = ((uint64_t) mac_readreg(s, VG15_M_RBAH >> 2) << 32) |
                                     mac_readreg(s, VG15_M_RBAL >> 2);
        break;
    case VG15_M_TBAH:
        val &= 0xF; /* Clear all bits except 0..3 */
    case VG15_M_TBAL: /* CSR4 */
        mac_writereg(s, index >> 2, val);
        s->cur_tx_desc = ((uint64_t) mac_readreg(s, VG15_M_TBAH >> 2) << 32) |
                                     mac_readreg(s, VG15_M_TBAL >> 2);
        break;
    default:
        printf("vg15m: Write to unknown register %lx\n", addr);
    }
}

uint64_t vg15m_read(void *opaque, hwaddr addr, unsigned size)
{
    VG15State *s = opaque;
    unsigned int index = addr & (VG15M_REGION_SIZE - 1);
    int ret = 0;

    switch (index) {
    case 0 ... (TULIP_CSR_REGION_SIZE - 1):
        ret = vg15_csr_read(opaque, index, size);
        break;
    case VG15_M_CRDAL: /* CSR2 */
        ret = (uint32_t) s->cur_rx_desc;
        break;
    case VG15_M_CTDAL: /* CSR1 */
        ret = (uint32_t) s->cur_tx_desc;
        break;
    case VG15_M_CRDAH:
        ret = s->cur_rx_desc >> 32;
        break;
    case VG15_M_CTDAH:
        ret = s->cur_tx_desc >> 32;
        break;
    case VG15_M_RBAL: /* CSR3 */
    case VG15_M_RBAH:
    case VG15_M_TBAL: /* CSR4 */
    case VG15_M_TBAH:
        ret = mac_readreg(s, index >> 2);
        break;
    default:
        printf("vg15m: Read from unknown register %lx\n", addr);
    }

    return ret;
}

int vg15_init(DeviceState *dev, VG15State *s, NetClientInfo *info)
{
    int i;
    uint8_t *macaddr;
    uint16_t *eeprom_contents;

    dp83865_init(&s->mii, 0);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    macaddr = s->conf.macaddr.a;

    s->eeprom = eeprom93xx_new(dev, EEPROM_SIZE);
    eeprom_contents = eeprom93xx_data(s->eeprom);
    memmove(eeprom_contents, vg15_eeprom,
            EEPROM_SIZE * sizeof(uint16_t));

    /* copy macaddr to eeprom as linux driver want find it there */
    for (i = 0; i < 3; i++) {
        eeprom_contents[EEPROM_MACADDR_OFFSET + i] =
                (macaddr[2 * i + 1] << 8) | macaddr[2 * i];
    }

    eeprom_contents[GB_SROM_LEN / 2] = gbSromCrcGet((void *)eeprom_contents);

    /*
     * FIXME: we have to update eeprom checksum too.
     *
     * see tulip-diag utility code from nictools-pci package
     *   http://ftp.debian.org/debian/pool/main/n/nictools-pci/
     */

    s->nic = qemu_new_nic(info, &s->conf,
                          object_get_typename(OBJECT(dev)),
                          dev->id, s);

    qemu_format_nic_info_str(qemu_get_queue(s->nic), macaddr);

    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, vg15_timer, s);
    timer_mod(s->timer,
              qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + NANOSECONDS_PER_SECOND);
    info->link_status_changed = tulip_set_link_status;

    return 0;
}

static const uint32_t mac_reg_init[] = {
    [CSR0] = 0xfe000000,
    [CSR1] = 0xffffffff,
    [CSR2] = 0x00000000,
    [CSR3] = 0x00000000,
    [CSR4] = 0x00000000,
    [CSR5] = 0xf0000000,
    [CSR6] = 0x00000000,
    [CSR7] = 0x00000000,
    [CSR8] = 0xe0000000,
    [CSR9] = 0xfff483ff,
    [CSR10] = 0x00000000,
    [CSR11] = 0x00000000,
    [CSR12] = 0x00000000,
    [CSR13] = 0x00000000,
    [CSR14] = 0x00000000,
    [CSR15] = 0x00000000,
};

void vg15_reset(void *opaque)
{
    VG15State *d = opaque;

    memset(d->mac_reg, 0, sizeof d->mac_reg);
    memmove(d->mac_reg, mac_reg_init, sizeof mac_reg_init);

    d->tx_polling = 0;
}

static void tulip_update_irq(VG15State *s)
{
    int isr = 0;

    /* calculate NIS (sum of normal interrupts) */
    s->mac_reg[CSR5] &= ~TULIP_CSR5_NIS;

    if (s->mac_reg[CSR5] & (TULIP_CSR5_TI | TULIP_CSR5_RI)) {
        s->mac_reg[CSR5] |= TULIP_CSR5_NIS;
        isr = 1;
    }

    /* FIXME: when calculating NIS take into account masked interupts */

    /* FIXME: can we report about Abnormal Interrupt Summary too? */

    qemu_set_irq(s->irq, isr);
}

void vg15_timer(void *opaque)
{
    VG15State *s = opaque;

    if (s->tx_polling) {
        process_tx(s);
    }

    tulip_update_irq(s);

    timer_mod(s->timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + NANOSECONDS_PER_SECOND / 10);
}
