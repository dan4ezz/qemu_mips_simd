/* QEMU System Emulator - RapidIO Controller
 *
 * Copyright (c) 2014 SRISA RAS
 * Copyright (c) 2014 Sergey Smolov <smolov@ispras.ru>
 * Copyright (C) 2017 Alexander Kalmuk <alexkalmuk@gmail.com>
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
 *
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "sysemu/dma.h"
#include "sc64-rio.h"
#include "sc64-rio-regs.h"

/* #define DEBUG */

/* Controller vendor & device identifier */
#define RIO_VID_SRISA 0x0074
#define RIO_DID_VM6 0x8717

#define MAX_MAILBOXES 4
#define MAX_LETTERS 4
#define MAX_SEGMENTS 16
#define MAX_QUEUES 9

#define MAX_DESCRIPTOR_SIZE (16 * 256)
#define IO_WRITE   0x50
#define MSG_OUT_TIMEOUT (NANOSECONDS_PER_SECOND * 5)
#define MSG_IN_DELAY    (NANOSECONDS_PER_SECOND / 100)
#define MSG_IN_TIMEOUT_TICKS 200
#define DOORBELL_TIMEOUT (NANOSECONDS_PER_SECOND * 5)

#define MSG_STATUS_DONE   0x0
#define MSG_STATUS_RETRY  0x3
#define MSG_STATUS_ERROR  0x7

#define DESCR_STATUS_OK    (0 << 0)
#define DESCR_STATUS_ERROR (1 << 0)

static void rio_message_send(SC64RIOState *s);
static void rio_send_descriptor(SC64RIOState *s, Descr *descr);
static Descr read_descriptor(SC64RIOState *s, uint address);
static void write_descriptor(SC64RIOState *s, uint address, Descr descr);
static void rio_update_interrupt(SC64RIOState *s, int nr, uint32_t irq_mask,
    bool set);

static uint8_t rio_out_packet[MAX_DESCRIPTOR_SIZE + 32];

#ifdef DEBUG
#define DPRINTF(fmt, ...) \
    do { printf("sc64-rio: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

#define PRINT_REG(r) \
    DPRINTF("    "#r"(%5x) = %x\n", (r) << 2, sc64_rio_readreg(s, r))

static void sc64_rio_print_raw_packet(const char *title, const uint8_t *buf,
    int size)
{
#define STR_BYTES 8
#define MAX_ROWS  3

#ifndef DEBUG
    return;
#endif
    int i, j, pos, rows;
    char c;

    printf("PACKET(%d) %s:\n", size, title);
    rows = (size + STR_BYTES - 1) / STR_BYTES;

    if (rows > MAX_ROWS) {
        rows = MAX_ROWS;
    }

    for (i = 0; i < rows; i++) {
        for (j = 0; j < STR_BYTES; j++) {
            pos = i * STR_BYTES + j;
            if (pos < size) {
                printf(" %02hhX", *(buf + pos));
            } else {
                printf("   ");
            }
        }
        printf("   ");
        for (j = 0; j < STR_BYTES; j++) {
            pos = i * STR_BYTES + j;
            if (pos < size) {
                c = (char) *(buf + pos);
                if (c < 33 || c > 126) {
                    c = '.';
                }
                printf("%c", c);
            }
        }
        printf("\n");
    }
#undef STR_BYTES
#undef MAX_ROWS
}

#define RIO_READ_FIELD(s, index, field) \
    rio_read_mask(s, index, RIO_SHIFT(field), RIO_MASK(field))

#define RIO_WRITE_FIELD(s, index, field, val) \
    rio_write_mask(s, index, RIO_SHIFT(field), RIO_MASK(field), val)

#define RIO_WRITE_MASKED(s, reg, val, wmask) \
    rio_write_masked_reg(s, reg, val, RIO_SHIFTED_MASK(wmask))

static inline uint32_t
rio_read_mask(SC64RIOState *s, int index, uint32_t shift, uint32_t mask)
{
    return (sc64_rio_readreg(s, index) >> shift) & mask;
}

static inline void
rio_write_mask(SC64RIOState *s, int index, uint32_t shift,
    uint32_t mask, uint32_t val)
{
    uint32_t reg = sc64_rio_readreg(s, index);
    /* Clear field value */
    reg &= ~(mask << shift);
    /* Write shifted field value */
    val = (val & mask) << shift;
    sc64_rio_writereg(s, index, reg | val);
}

static inline void
rio_write_masked_reg(SC64RIOState *s, int index, uint32_t val, uint32_t mask)
{
    uint32_t read_only = sc64_rio_readreg(s, index) & (~mask);

    sc64_rio_writereg(s, index, (val & mask) | read_only);
}

static inline uint32_t msginx_x(int i, int j)
{
    int msgin1_x, msginx_x;
    switch (i) {
    case 0:
        msgin1_x = MSG_IN1_1;
        break;
    case 1:
        msgin1_x = MSG_IN1_2;
        break;
    case 2 ... 7:
        i -= 2;
        msgin1_x = MSG_IN1_3 + i * 0xC;
        break;
    case 8:
        msgin1_x = MSG_IN1_9;
        break;
    default:
        assert(0);
    }
    msginx_x = msgin1_x + 0x4 * j;
    return msginx_x;
}

#define MSGIN2_X(i) msginx_x(i, 1)
#define MSGIN_STATUS_X(i) msginx_x(i, 2)

static void sc64_rio_print_descriptor(SC64RIOState *s, Descr descr,
    uint32_t address)
{
    int i;
    uint64_t descriptor[4];

    dma_memory_read(s->dma_as, address, &descriptor[0], 4 * sizeof(uint64_t));

    DPRINTF("read_descriptor (0x%08x): ", address);
    for (i = 0; i < 4; i++) {
        DPRINTF(" [%d]: %08x_%08x ", i, (uint32_t) (descriptor[i] >> 32),
            (uint32_t) descriptor[i]);
    }
    DPRINTF("\n");
    DPRINTF("read_descriptor (0x%08x): "
        "id=%x, own=%x, status=%x, last_size=%x, size=%x, length=%x,"
        "mbox=%x, letter=%x, seg1_addr=%x, segn_addr=%x, next=%x, pol=%x, "
        "eol=%x, next = %x\n",
        address, descr.id, descr.own, descr.status, descr.last_size,
        descr.size, descr.length, descr.mbox, descr.letter, descr.seg1_addr,
        descr.segn_addr, descr.next_descr_addr, descr.pol, descr.eol,
        descr.next_descr_addr);
}

static int get_queue_nr(uint32_t addr)
{
    switch (addr) {
    case MSG_IN1_1:
    case MSG_IN2_1:
    case MSG_IN_STATUS_1:
        return 0;
    case MSG_IN1_2:
    case MSG_IN2_2:
    case MSG_IN_STATUS_2:
        return 1;
    case MSG_IN1_3:
    case MSG_IN2_3:
    case MSG_IN_STATUS_3:
        return 2;
    case MSG_IN1_4:
    case MSG_IN2_4:
    case MSG_IN_STATUS_4:
        return 3;
    case MSG_IN1_5:
    case MSG_IN2_5:
    case MSG_IN_STATUS_5:
        return 4;
    case MSG_IN1_6:
    case MSG_IN2_6:
    case MSG_IN_STATUS_6:
        return 5;
    case MSG_IN1_7:
    case MSG_IN2_7:
    case MSG_IN_STATUS_7:
        return 6;
    case MSG_IN1_8:
    case MSG_IN2_8:
    case MSG_IN_STATUS_8:
        return 7;
    case MSG_IN1_9:
    case MSG_IN2_9:
    case MSG_IN_STATUS_9:
        return 8;
    }

    assert(0);
}

static void sc64_rio_update_irq(SC64RIOState *s, uint done)
{
    if (done) {
        qemu_irq_raise(s->irq);
    } else {
        qemu_irq_lower(s->irq);
    }
}

static uint32_t sc64_rio_reg_offset(unsigned addr)
{
    if ((addr <= 0x0003C)
        || (0x0040 <= addr && addr <= 0x000FC)
        || (0x00100 <= addr && addr <= 0x003FC)) {
        return (uint32_t)(addr + 0x11000);
    } else if ((0x1000 <= addr && addr <= 0x013FC)
        || (0x10200 <= addr && addr <= 0x102FC)
        || (0x10800 <= addr && addr <= 0x108FC)
        || (0x10C00 <= addr && addr <= 0x10CFC)
        || (0x30000 <= addr && addr <= 0x30FFC)
        || (0x50000 <= addr && addr <= 0x50FFC)
        || (0x70000 <= addr && addr <= 0x70FFC)
        || (0x90000 <= addr && addr <= 0x90FFC)
        || (0xb0000 <= addr && addr <= 0xbFFFC)) {
        return (uint32_t)addr;
    } else if ((0x11000 <= addr && addr <= 0x110FC)
        || (0x11100 <= addr && addr <= 0x111FC)
        || (0x12400 <= addr && addr <= 0x12FFC)) {
        return (uint32_t)(addr - 0x11000);
    } else {
        qemu_log_mask(LOG_GUEST_ERROR, "%s:%s: wrong address: %x\n",
            __FILE__, __func__, (uint32_t)addr);
        assert(0);
    }
}

static void rio_maint_timer(void *opaque)
{
    SC64RIOState *s = opaque;
    sc64_rio_writereg(s, RIO_M_STATUS, M_STATUS_DONE | M_STATUS_ERROR);
    DPRINTF("rio_maint_timer\n");
}

static void rio_doorbell_timer(void *opaque)
{
    SC64RIOState *s = opaque;

    RIO_WRITE_FIELD(s, DBOUT_ERROR, DBOUT_ERROR_TIMEOUT, 1);
    RIO_WRITE_FIELD(s, DBOUT_STATUS, DBOUT_FAIL, 1);
    rio_update_interrupt(s, 0,
        RIO_SHIFTED_MASK(INTERRUPT_DBOUTERR), 1);
    DPRINTF("rio_doorbell_timer\n");
}

static int8_t convert_message_size(int size)
{
    switch (size) {
    case 8: return 0x9;
    case 16: return 0xa;
    case 32: return 0xb;
    case 64: return 0xc;
    case 128: return 0xd;
    case 256: return 0xe;
    default:
        DPRINTF("write_descriptor: Unsupported size - %d\n", size);
        return -1;
    }
}

static int mk_pkt_type(uint8_t *pkt, uint8_t type)
{
    pkt[0] = IO_WRITE;
    return 1;
}

static int mk_pkt_header(uint8_t *pkt, uint8_t ftype,
           uint8_t tt, uint16_t dstId, uint16_t srcId)
{
    int hdr_len;

    hdr_len = 8;

    pkt[0] = ftype;
    pkt[1] = tt;
    pkt[2] = 0; /* phys_res_1 */
    pkt[3] = 0; /* phys_res_2 */
    stw_be_p((pkt + 4), dstId);
    stw_be_p((pkt + 6), srcId);

    return hdr_len;
}

static int mk_message(SC64RIOState *s, uint8_t *pkt, Descr *descr)
{
    int ll_len = 7, msg_len, i;
    hwaddr src;
    uint8_t *dst;
    int ssize;
    int seg_size = 1 << (descr->size - 6);
    int last_seg_size = 1 << (descr->last_size - 6);

    pkt[4] = descr->letter;
    pkt[5] = descr->mbox;
    pkt[6] = descr->size;

    msg_len = 0;
    src = descr->seg1_addr;
    dst = pkt + 7;
    /* Concatenate all segments except the last one */
    for (i = 0; i < descr->length + 1; i++) {
        ssize = i == descr->length ? last_seg_size : seg_size;
        dma_memory_read(s->dma_as, src, dst, ssize);
        src = descr->segn_addr + ssize * i;
        dst += ssize;
        msg_len += ssize;
    }

    ll_len += msg_len;
    stl_be_p(pkt, ll_len);

    return ll_len;
}

static int mk_doorbell(uint8_t *pkt, uint8_t tid, uint8_t lsb, uint8_t msb)
{
    int ll_len = 7;

    pkt[4] = tid;
    pkt[5] = lsb;
    pkt[6] = msb;

    stl_be_p(pkt, ll_len);

    return ll_len;
}

static int mk_response(uint8_t *pkt, uint8_t ttype, uint8_t status,
        uint8_t letter, uint8_t mbox)
{
    int ll_len = 8;

    pkt[4] = ttype;
    pkt[5] = status;
    pkt[6] = letter;
    pkt[7] = mbox;

    stl_be_p(pkt, ll_len);

    return ll_len;
}

static int mk_maint_read_request(uint8_t *pkt, uint8_t hop_count,
           uint8_t TID, uint32_t offset)
{
    int ll_len;

    ll_len = 12; /* <<logical level>> length */
    stl_be_p(pkt, ll_len);

    pkt[4] = hop_count;
    pkt[5] = TTYPE_MAINT_READ_REQUEST;
    pkt[6] = 4; /* rdsize */
    pkt[7] = TID;
    stl_be_p(pkt + 8, offset);

    return ll_len;
}

static int mk_maint_read_response(uint8_t *pkt, uint8_t hop_count,
           uint8_t TID, uint32_t data)
{
    int ll_len;

    ll_len = 12; /* <<logical level>> length */
    stl_be_p(pkt, ll_len);

    pkt[4] = hop_count;
    pkt[5] = TTYPE_MAINT_READ_RESPONSE;
    pkt[6] = 0; /* status */
    pkt[7] = TID;
    stl_be_p(pkt + 8, data);

    return ll_len;
}

static int mk_maint_write_request(uint8_t *pkt, uint8_t hop_count,
           uint8_t TID, uint32_t offset, uint32_t data)
{
    int ll_len;

    ll_len = 16; /* <<logical level>> length */
    stl_be_p(pkt, ll_len);

    pkt[4] = hop_count;
    pkt[5] = TTYPE_MAINT_WRITE_REQUEST;
    pkt[6] = 4; /* rdsize */
    pkt[7] = TID;
    stl_be_p(pkt + 8, offset);
    stl_be_p(pkt + 12, data);

    return ll_len;
}

static int mk_maint_write_response(uint8_t *pkt, uint8_t hop_count,
           uint8_t TID)
{
    int ll_len;

    ll_len = 8; /* <<logical level>> length */
    stl_be_p(pkt, ll_len);

    pkt[4] = hop_count;
    pkt[5] = TTYPE_MAINT_WRITE_RESPONSE;
    pkt[6] = 0; /* status */
    pkt[7] = TID;

    return ll_len;
}

Descr read_descriptor(SC64RIOState *s, uint32_t address)
{
    Descr descr;
    uint64_t descriptor[4];

    dma_memory_read(s->dma_as, address, &descriptor[0], 4 * sizeof(uint64_t));

    descr.id = (descriptor[0] >> 32) & 0xFFFF;
    descr.own = (descriptor[0] >> 31) & 0x1;
    descr.status = (descriptor[0] >> 16) & 0x7FFF;
    descr.last_size = (descriptor[0] >> 12) & 0xF;
    descr.size = (descriptor[0] >> 8) & 0xF;
    descr.length = (descriptor[0] >> 4) & 0xF;
    descr.mbox = (descriptor[0] >> 2) & 0x3;
    descr.letter = descriptor[0] & 0x3;
    descr.seg1_addr = descriptor[1] >> 32; /* [63:35] but aligned to 8 bytes */
    descr.segn_addr = descriptor[2] >> 32; /* the same */
    descr.next_descr_addr = (descriptor[3] >> 32) & ~0x7; /* the same */
    descr.pol = (descriptor[3] >> 33) & 0x1;
    descr.eol = (descriptor[3] >> 32) & 0x1;

    return descr;
}

static void write_descriptor(SC64RIOState *s, uint address, Descr descr)
{
    uint64_t descriptor[4] = {0, 0, 0, 0};

    descriptor[0] = ((uint64_t)descr.id << 32) |
    (descr.own << 31) | (descr.status << 16) |
    (descr.last_size << 12) | (descr.size << 8) |
    (descr.length << 4) | (descr.mbox >> 2) | (descr.letter);
    descriptor[1] |= (uint64_t)descr.seg1_addr << 32;
    descriptor[2] |= (uint64_t)descr.segn_addr << 32;
    descriptor[3] |= ((uint64_t)descr.next_descr_addr << 32) |
    ((uint64_t)descr.pol << 33) | ((uint64_t)descr.eol << 32);

    dma_memory_write(s->dma_as, address, descriptor, 32);
}

static struct sc64_rio_rcv_descr *create_rcv_descriptor(SC64RIOState *s,
    struct sc64_rio_msg_addr *addr, int msg_len,
    void *data, int seg_size)
{
    sc64_rio_rcv_descr *d = g_new0(sc64_rio_rcv_descr, 1);

    if (!d) {
        DPRINTF("%s: g_new0 failed\n", __func__);
        return NULL;
    }

    d->addr = *addr;

    d->timer_ticks = 0;
    d->msg_len = msg_len;
    d->seg_size = seg_size;

    memcpy(d->data, data, msg_len);

    return d;
}

static void free_rcv_descriptor(struct sc64_rio_rcv_descr *d)
{
    g_free(d);
}

static void sc64_fill_doorbell(SC64RIOState *s,
    struct sc64_rio_doorbell *doorbell, uint8_t tid, uint8_t lsb, uint8_t msb)
{
    doorbell->tid = tid;
    doorbell->lsb = lsb;
    doorbell->msb = msb;
}

static void rio_send_doorbell(SC64RIOState *s)
{
    uint8_t pkt[64];
    int pkt_len;
    uint32_t data;
    uint16_t src, dest;
    struct sc64_rio_doorbell doorbell;

    /* Fill packet header */
    pkt_len = mk_pkt_type(pkt, IO_WRITE);

    src = (sc64_rio_readreg(s, BASE_DEV_CSR) >> 16) & 0xff;
    dest = (sc64_rio_readreg(s, DBOUT1)) & 0xff;
    pkt_len += mk_pkt_header(pkt + pkt_len, FTYPE_DOORBELL, 1, dest, src);

    /* Fill DOORBELL */
    data = sc64_rio_readreg(s, DBOUT2) & 0xFFFF;
    sc64_fill_doorbell(s, &doorbell, ++s->doorbell_tid, data, data >> 8);
    pkt_len += mk_doorbell(pkt + pkt_len, doorbell.tid, doorbell.lsb,
                    doorbell.msb);

    DPRINTF("rio_send_doorbell: src=%d, dst=%d, tid=0x%x, lsb=0x%x, msb=0x%x\n",
        src, dest, doorbell.tid, doorbell.lsb, doorbell.msb);

    sc64_rio_print_raw_packet("rio_send_doorbell", pkt, pkt_len);

    timer_mod(s->doorbell_timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + DOORBELL_TIMEOUT);
    qemu_chr_fe_write(&s->chr, pkt, pkt_len);
}

/*
 * 0 - INTERRUPT_ENA, INTERRUPT
 * 1 - INTERRUPT_ENA_ADD, INTERRUPT_ADD
 * 2 - INTERRUPT_ENA_ADD1, INTERRUPT_ADD1
 */
static void rio_update_interrupt(SC64RIOState *s, int nr, uint32_t irq_mask,
    bool set)
{
    int interrupt_ena_reg, interrupt_reg;
    uint32_t interrupt_ena, interrupt;

    switch (nr) {
    case 0:
        interrupt_ena_reg = INTERRUPT_ENA;
        interrupt_reg = INTERRUPT;
        break;
    case 1:
        interrupt_ena_reg = INTERRUPT_ENA_ADD;
        interrupt_reg = INTERRUPT_ADD;
        break;
    case 2:
        interrupt_ena_reg = INTERRUPT_ENA_ADD1;
        interrupt_reg = INTERRUPT_ADD1;
        break;
    }

    interrupt_ena = sc64_rio_readreg(s, interrupt_ena_reg);
    interrupt = sc64_rio_readreg(s, interrupt_reg);

    if (set) {
        interrupt |= irq_mask;
    } else {
        interrupt &= ~irq_mask;
    }
    interrupt &= interrupt_ena;

    sc64_rio_writereg(s, interrupt_reg, interrupt);

    interrupt |= sc64_rio_readreg(s, INTERRUPT_ADD) |
                 sc64_rio_readreg(s, INTERRUPT_ADD1);

    if (interrupt) {
        DPRINTF("rio_update_interrupt (%d): INTERRUPT ENABLED - %x\n", nr,
            irq_mask);
        sc64_rio_update_irq(s, 1);
    } else {
        DPRINTF("rio_update_interrupt (%d): INTERRUPT DISABLED - %x\n", nr,
            irq_mask);
        sc64_rio_update_irq(s, 0);
    }
}

static void rio_clear_interrupt_mask(SC64RIOState *s, int reg, int mask)
{
    int irq_mask = 0;
    int queue_nr;

    switch (reg) {
    case MSG_OUT_STATUS:
        if (mask & RIO_SHIFTED_MASK(MSG_OUT_STATUS_POL)) {
            irq_mask |= RIO_SHIFTED_MASK(INTERRUPT_MSGOUTPOL);
        }
        if (mask & RIO_SHIFTED_MASK(MSG_OUT_STATUS_OWN)) {
            irq_mask |= RIO_SHIFTED_MASK(INTERRUPT_MSGOUTOWN);
        }
        if (mask & RIO_SHIFTED_MASK(MSG_OUT_STATUS_EOL)) {
            irq_mask |= RIO_SHIFTED_MASK(INTERRUPT_MSGOUTEOL);
        }
        if (mask & RIO_SHIFTED_MASK(MSG_OUT_STATUS_COMPL)) {
            irq_mask |= RIO_SHIFTED_MASK(INTERRUPT_MSGOUTCOMPL);
        }
        rio_update_interrupt(s, 0, irq_mask, 0);
        break;
    case MSG_IN_STATUS_9:
        if (mask & RIO_SHIFTED_MASK(MSG_IN_STATUS_9_POL)) {
            irq_mask |= RIO_SHIFTED_MASK(INTERRUPT_MSGINPOL);
        }
        if (mask & RIO_SHIFTED_MASK(MSG_IN_STATUS_9_OWN)) {
            irq_mask |= RIO_SHIFTED_MASK(INTERRUPT_MSGINOWN);
        }
        if (mask & RIO_SHIFTED_MASK(MSG_IN_STATUS_9_EOL)) {
            irq_mask |= RIO_SHIFTED_MASK(INTERRUPT_MSGINEOL);
        }
        if (mask & RIO_SHIFTED_MASK(MSG_IN_STATUS_9_COMPL)) {
            irq_mask |= RIO_SHIFTED_MASK(INTERRUPT_MSGINCOMPL);
        }
        if (mask & RIO_SHIFTED_MASK(MSG_IN_STATUS_9_DONE)) {
            irq_mask |= RIO_SHIFTED_MASK(INTERRUPT_MSGINDONE);
        }
        rio_update_interrupt(s, 0, irq_mask, 0);
        break;
    /* MSGIN_STATUS_X */
    case MSG_IN_STATUS_1:
    case MSG_IN_STATUS_2:
    case MSG_IN_STATUS_3:
    case MSG_IN_STATUS_4:
    case MSG_IN_STATUS_5:
    case MSG_IN_STATUS_6:
    case MSG_IN_STATUS_7:
    case MSG_IN_STATUS_8:
        queue_nr = get_queue_nr(reg);

        if (mask & RIO_SHIFTED_MASK(MSG_IN_STATUS_POL)) {
            irq_mask |= MSGINPOLINT_SHIFTED_MASK(queue_nr);
            rio_update_interrupt(s, 2, irq_mask, 0);
        }
        irq_mask = 0;

        if (mask & RIO_SHIFTED_MASK(MSG_IN_STATUS_OWN)) {
            irq_mask |= MSGINOWNINT_SHIFTED_MASK(queue_nr);
        }
        if (mask & RIO_SHIFTED_MASK(MSG_IN_STATUS_FAIL)) {
            irq_mask |= MSGINFAILINT_SHIFTED_MASK(queue_nr);
        }
        if (mask & RIO_SHIFTED_MASK(MSG_IN_STATUS_EOL)) {
            irq_mask |= MSGINEOLINT_SHIFTED_MASK(queue_nr);
        }
        if (mask & RIO_SHIFTED_MASK(MSG_IN_STATUS_COMPL)) {
            irq_mask |= MSGINCOMPLINT_SHIFTED_MASK(queue_nr);
        }
        rio_update_interrupt(s, 1, irq_mask, 0);
        break;
    case MSG_OUT_ERROR:
        /* Clear interrupt iff all bits are 0 */
        if (!sc64_rio_readreg(s, MSG_OUT_ERROR)) {
            RIO_WRITE_FIELD(s, MSG_OUT_STATUS,
                              MSG_OUT_STATUS_FAIL, 0);
            rio_update_interrupt(s, 0,
                RIO_SHIFTED_MASK(INTERRUPT_MSGOUTERR), 0);
        }
        break;
    }
}

static uint32_t rio_update_register_with_irq(SC64RIOState *s, int reg,
    uint32_t val, uint32_t all_irq, uint32_t wmask)
{
    uint32_t old_val, enabled_irq, disabled_irq;

    old_val = sc64_rio_readreg(s, reg);

    enabled_irq = (old_val & all_irq) & ~val;
    disabled_irq = all_irq & ~enabled_irq;
    val = (val & ~all_irq) | enabled_irq;

    rio_write_masked_reg(s, reg, val, wmask);

    rio_clear_interrupt_mask(s, reg, disabled_irq);

    return val;
}

static void handle_error(SC64RIOState *s, int err_reg, uint32_t shift,
    uint32_t mask)
{
    uint32_t descr_addr;
    Descr descr;

    switch (err_reg) {
    case MSG_OUT_ERROR:
        DPRINTF("HANDLE ERROR reg - %x (bit - %x)\n",
            err_reg << 2, (mask << shift));
        descr_addr = s->current_descr_out;
        descr = read_descriptor(s, descr_addr);
        descr.own = 0;
        descr.status = DESCR_STATUS_ERROR;
        write_descriptor(s, descr_addr, descr);

        rio_write_mask(s, err_reg, shift, mask, 1);
        RIO_WRITE_FIELD(s, MSG_OUT_STATUS, MSG_OUT_STATUS_FAIL, 1);
        rio_update_interrupt(s, 0,
            RIO_SHIFTED_MASK(INTERRUPT_MSGOUTERR), 1);
        break;
    }
}

static void rio_handle_doorbell_response(SC64RIOState *s, uint8_t tid,
    int status)
{
    assert(tid == s->doorbell_tid);
    assert(timer_pending(s->doorbell_timer));

    timer_del(s->doorbell_timer);

    switch (status) {
    case MSG_STATUS_DONE:
        DPRINTF("DOORBELL HANDLED! (response received)\n");
        break;
    case MSG_STATUS_ERROR:
        DPRINTF("%s: MSG_STATUS_ERROR\n", __func__);
        /* 1 << 4 is an ERROR response */
        RIO_WRITE_FIELD(s, DBOUT_ERROR, DBOUT_RESP_IN_ERR, (1 << 4));
        break;
    }

    /* Clear DbOutProgress */
    RIO_WRITE_FIELD(s, DBOUT_STATUS, DBOUT_PROGRESS, 0);

    /* Set up DbOutDone and generate interrupt if required */
    RIO_WRITE_FIELD(s, DBOUT_STATUS, DBOUT_DONE, 1);
    rio_update_interrupt(s, 0,
        RIO_SHIFTED_MASK(INTERRUPT_DBOUTDONE), 1);
}

static void rio_message_send_timer(void *opaque)
{
    SC64RIOState *s = opaque;
    Descr descr;

    DPRINTF("Error: Send timeout\n");

    descr = read_descriptor(s, s->current_descr_out);
    descr.own = 0;
    descr.status = DESCR_STATUS_ERROR;
    write_descriptor(s, s->current_descr_out, descr);

    handle_error(s, MSG_OUT_ERROR, RIO_SHIFT(MSG_OUT_ERROR_TIMEOUT),
        RIO_MASK(MSG_OUT_ERROR_TIMEOUT));
}

static void sc64_rio_update_waterline(SC64RIOState *s)
{
    int val = RIO_READ_FIELD(s, MSG_OUT2, MSG_OUT2_WATERLINE);

    assert(((val & (val - 1)) == 0) && (val <= 0x80));

    if (val == 0) {
        s->waterline = 0;
    } else {
        s->waterline = 1 << (val & ~1);
    }
}

static void sc64_rio_update_msg_compl_out(SC64RIOState *s)
{
    if (s->waterline && (++s->msg_out_sent >= s->waterline)) {
        s->msg_out_sent = 0;
        RIO_WRITE_FIELD(s, MSG_OUT_STATUS, MSG_OUT_STATUS_COMPL, 1);
        rio_update_interrupt(s, 0,
            RIO_SHIFTED_MASK(INTERRUPT_MSGOUTCOMPL), 1);
    }
}

static void sc64_rio_update_msg_pol_out(SC64RIOState *s, int pol)
{
    if (pol) {
        RIO_WRITE_FIELD(s, MSG_OUT_STATUS, MSG_OUT_STATUS_POL, 1);
        rio_update_interrupt(s, 0,
            RIO_SHIFTED_MASK(INTERRUPT_MSGOUTPOL), 1);
    }
}

static void sc64_rio_update_msg_eol_out(SC64RIOState *s, int eol)
{
    if (eol) {
        RIO_WRITE_FIELD(s, MSG_OUT_STATUS, MSG_OUT_STATUS_EOL, 1);
        rio_update_interrupt(s, 0,
            RIO_SHIFTED_MASK(INTERRUPT_MSGOUTEOL), 1);
    }
}

static void rio_handle_msg_response(SC64RIOState *s, uint16_t src, uint16_t dst,
    uint8_t mbox, uint8_t letter, int status)
{
    uint32_t descr_addr;
    Descr descr;

    DPRINTF("%s: src = %d, dst = %d, mbox = %d, letter = %d, status = %d\n",
        __func__, src, dst, mbox, letter, status);

    assert(timer_pending(s->msg_out_timer));

    /* the address of the first descriptor */
    descr_addr = s->current_descr_out;
    descr = read_descriptor(s, descr_addr);

    assert((descr.id == src) && (descr.mbox == mbox)
                                                && (descr.letter == letter));

    timer_del(s->msg_out_timer);

    switch (status) {
    case MSG_STATUS_DONE:
        DPRINTF("MESSAGE HANDLED! (response received)\n");

        descr.own = 0;
        descr.status = DESCR_STATUS_OK;
        write_descriptor(s, descr_addr, descr);

        /* Try to send next descriptor */
        s->current_descr_out = descr.next_descr_addr;
        rio_message_send(s);
        break;
    case MSG_STATUS_ERROR:
        /* Set error as if we got N Retry packets and still failed */
        handle_error(s, MSG_OUT_ERROR,
            RIO_SHIFT(MSG_OUT_ERROR_RETRY_LIMIT),
            RIO_MASK(MSG_OUT_ERROR_RETRY_LIMIT));
        break;
    }

    sc64_rio_update_msg_pol_out(s, descr.pol);
    sc64_rio_update_msg_eol_out(s, descr.eol);
    sc64_rio_update_msg_compl_out(s);
}

static void rio_send_descriptor(SC64RIOState *s, Descr *descr)
{
    uint8_t *pkt;
    size_t pkt_len;
    uint16_t src, dest;
    uint8_t tt;

    pkt = rio_out_packet;

    /* Fill packet header */
    pkt_len = mk_pkt_type(pkt, IO_WRITE);
    tt = (descr->status >> 1) & 1;
    src = (sc64_rio_readreg(s, BASE_DEV_CSR) >> 16) & 0xff;
    dest = descr->id;
    pkt_len += mk_pkt_header(pkt + pkt_len, FTYPE_MESSAGE, tt, dest, src);
    pkt_len += mk_message(s, pkt + pkt_len, descr);

    DPRINTF("%s: src=%x, dest=%x, ttype=%x, len=%d\n", __func__,
            src, dest, tt, (uint32_t) be32_to_cpup((uint32_t *) (pkt + 9)));
    DPRINTF("%s: letter=%x, mbox=%x, descr->length=%d\n", __func__,
            descr->letter, descr->mbox, descr->length);

    sc64_rio_print_raw_packet("MESSAGE OUT descriptor", pkt, pkt_len);

    timer_mod(s->msg_out_timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + MSG_OUT_TIMEOUT);

    qemu_chr_fe_write(&s->chr, pkt, pkt_len);
}

static void rio_message_send(SC64RIOState *s)
{
    int mode;
    uint32_t descr_addr;
    Descr descr;

    mode = RIO_READ_FIELD(s, MSG_OUT_STATUS, MSG_OUT_STATUS_MODE);
    DPRINTF("Mode = %x\n", mode);

    if (RIO_READ_FIELD(s, MSG_OUT_STATUS,
            MSG_OUT_STATUS_INIT) != 1) {
        DPRINTF("%s: MSG_OUT_STATUS_INIT != 1\n", __func__);
        return;
    }

    descr_addr = s->current_descr_out;
    descr = read_descriptor(s, descr_addr);

    DPRINTF("Descriptor address = %x\n", descr_addr);
    sc64_rio_print_descriptor(s, descr, descr_addr);
    PRINT_REG(MSG_OUT_STATUS);
    PRINT_REG(MSG_OUT1);

    if (descr.own == 0) {
        DPRINTF("descr.own == 0 (cannot be used)\n");
        if (!mode) {
            DPRINTF("Generate MsgOutOwn irq if allowed\n");
            RIO_WRITE_FIELD(s, MSG_OUT_STATUS,
                MSG_OUT_STATUS_OWN, 1);
            rio_update_interrupt(s, 0,
                RIO_SHIFTED_MASK(INTERRUPT_MSGOUTOWN), 1);
        }
        RIO_WRITE_FIELD(s, MSG_OUT_STATUS,
            MSG_OUT_STATUS_INIT, 0);
        return;
    }
    rio_send_descriptor(s, &descr);
}

static void handle_response(SC64RIOState *s, const uint8_t *buf)
{
    uint8_t ttype;
    int status;
    uint8_t in_letter, in_mbox, in_tid;
    uint16_t in_srcid, in_dstid;

    in_dstid = (uint16_t) be16_to_cpu(*(uint16_t *)(buf + 5));
    in_srcid = (uint16_t) be16_to_cpu(*(uint16_t *)(buf + 7));
    ttype = buf[13];
    status = buf[14];

    /* unpack response */
    DPRINTF("sc64: In response: src=%x, dest=%x, ttype=%x\n",
                                 in_srcid, in_dstid, ttype);

    switch (ttype) {
    case TTYPE_MESSAGE:
        in_letter = buf[15];
        in_mbox = buf[16];
        DPRINTF("sc64: In response: letter=%x, mbox=%x\n",
            in_letter, in_mbox);
        rio_handle_msg_response(s, in_srcid, in_dstid, in_mbox,
            in_letter, status);
        break;
    case TTYPE_DOORBELL:
        in_tid = buf[15];
        rio_handle_doorbell_response(s, in_tid, status);
        break;
    default:
        DPRINTF("Unknown response ttype\n");
        break;
    }
}

static void handle_doorbell(SC64RIOState *s, const uint8_t *buf)
{
    int status = MSG_STATUS_DONE;
    uint8_t pkt[64];
    int pkt_len;
    uint16_t in_srcid, in_dstid;
    struct sc64_rio_doorbell doorbell;
    uint32_t reg_status;
    uint16_t data;

    /* Unpack packet common header */
    in_dstid = (uint16_t) be16_to_cpu(*(uint16_t *)(buf + 5));
    in_srcid = (uint16_t) be16_to_cpu(*(uint16_t *)(buf + 7));

    sc64_fill_doorbell(s, &doorbell, buf[13], buf[14], buf[15]);
    DPRINTF("%s: tid=%x, lsb=%x, msb=%x\n", __func__,
        doorbell.tid, doorbell.lsb, doorbell.msb);

    reg_status = sc64_rio_readreg(s, DBIN_STATUS_CONTROL);
    if (reg_status & RIO_SHIFTED_MASK(DBIN_MODE)) {
        DPRINTF("%s: EROROR: store to memory is unsupported!\n", __func__);
        return;
    }
    if (!(reg_status & RIO_SHIFTED_MASK(DBIN_INIT))) {
        status = MSG_STATUS_ERROR;
    } else {
        /* Store DATA and TID */
        data = doorbell.lsb | (((uint16_t)doorbell.msb) << 8);
        RIO_WRITE_FIELD(s, DBIN_STATUS2, DBIN_DATA, data);
        RIO_WRITE_FIELD(s, DBIN_STATUS2, DBIN_TID, doorbell.tid);

        /* Set up DbInDone */
        RIO_WRITE_FIELD(s, DBIN_STATUS_CONTROL, DBIN_DONE, 1);
        rio_update_interrupt(s, 0,
            RIO_SHIFTED_MASK(INTERRUPT_DBINDONE), 1);
    }

    /* Send RESPONSE */
    pkt_len = mk_pkt_type(pkt, IO_WRITE);
    pkt_len += mk_pkt_header(pkt + pkt_len, FTYPE_RESPONSE, 1,
                    in_srcid, in_dstid);
    pkt_len += mk_response(pkt + pkt_len, TTYPE_DOORBELL, status,
                    doorbell.tid, 0);

    DPRINTF("%s: (RESPONSE) status=%x, src=%x, dst=%x\n", __func__,
        status, in_dstid, in_srcid);
    sc64_rio_print_raw_packet("(RESPONSE) handle_doorbell", pkt, pkt_len);

    qemu_chr_fe_write(&s->chr, pkt, pkt_len);
}

static void sc64_rio_update_msg_pol_in(SC64RIOState *s, int pol, int queue_nr)
{
    uint8_t msg_in_pol_limit;

    if (!pol) {
        return;
    }

    switch (queue_nr) {
    case 0:
        msg_in_pol_limit = RIO_READ_FIELD(s, MSG_IN3_ADD1, MSG_IN3_ADD1_LIMIT0);
        break;
    case 1:
        msg_in_pol_limit = RIO_READ_FIELD(s, MSG_IN3_ADD1, MSG_IN3_ADD1_LIMIT1);
        break;
    case 2:
        msg_in_pol_limit = RIO_READ_FIELD(s, MSG_IN3_ADD2, MSG_IN3_ADD2_LIMIT2);
        break;
    case 3:
        msg_in_pol_limit = RIO_READ_FIELD(s, MSG_IN3_ADD2, MSG_IN3_ADD2_LIMIT3);
        break;
    case 4:
        msg_in_pol_limit = RIO_READ_FIELD(s, MSG_IN3_ADD3, MSG_IN3_ADD3_LIMIT4);
        break;
    case 5:
        msg_in_pol_limit = RIO_READ_FIELD(s, MSG_IN3_ADD3, MSG_IN3_ADD3_LIMIT5);
        break;
    case 6:
        msg_in_pol_limit = RIO_READ_FIELD(s, MSG_IN3_ADD4, MSG_IN3_ADD4_LIMIT6);
    case 7:
        msg_in_pol_limit = RIO_READ_FIELD(s, MSG_IN3_ADD4, MSG_IN3_ADD4_LIMIT7);
        break;
    case 8:
        msg_in_pol_limit = RIO_READ_FIELD(s, MSG_IN3_9, MSG_IN3_9_POLLIMIT);
        break;
    }

    if (++s->msg_pol_in[queue_nr] == (msg_in_pol_limit + 1)) {
        s->msg_pol_in[queue_nr] = 0;

        if (queue_nr == 8) {
            RIO_WRITE_FIELD(s, MSG_IN_STATUS_9, MSG_IN_STATUS_9_POL, 1);
            rio_update_interrupt(s, 0, RIO_SHIFTED_MASK(INTERRUPT_MSGINPOL), 1);
        } else {
            RIO_WRITE_FIELD(s, MSGIN_STATUS_X(queue_nr), MSG_IN_STATUS_POL, 1);
            /* Update INTERRUPT_ENA_ADD1 and INTERRUPT_ADD1 */
            rio_update_interrupt(s, 2, MSGINPOLINT_SHIFTED_MASK(queue_nr), 1);
        }
    }
}

static void send_response(SC64RIOState *s, struct sc64_rio_msg_addr *addr,
    int status)
{
    uint8_t pkt[64], ttype;
    int pkt_len;

    pkt_len = mk_pkt_type(pkt, IO_WRITE);
    ttype = TTYPE_MESSAGE;
    pkt_len += mk_pkt_header(pkt + pkt_len, FTYPE_RESPONSE, ttype + 1,
                    addr->src, addr->dst);
    pkt_len += mk_response(pkt + pkt_len, ttype, status, addr->letter,
                    addr->mbox);

    DPRINTF("(Out response): src=%x, dest=%x, ttype=%d, len=%d\n",
                                 addr->src, addr->dst, ttype, pkt_len);
    DPRINTF("(Out response): status=%x, letter=%x, mbox=%x\n",
        status, addr->letter, addr->mbox);
    PRINT_REG(MSG_IN_STATUS_9);

    qemu_chr_fe_write(&s->chr, pkt, pkt_len);
}

static int rio_find_in_queue(SC64RIOState *s, struct sc64_rio_msg_addr *addr)
{
    int i;
    int mbox;
    int src;
    uint32_t msgin_status;

    for (i = 0; i < MSGIN_QUEUE_CNT - 1; i++) {
        msgin_status = sc64_rio_readreg(s, MSGIN_STATUS_X(i));
        if (!(msgin_status & RIO_SHIFTED_MASK(MSG_IN_STATUS_INIT))) {
            continue;
        }

        mbox = RIO_READ_FIELD(s, MSGIN2_X(i), MSG_IN2_X_MBOXTRAPMASK);
        if (RIO_READ_FIELD(s, MSGIN2_X(i), MSG_IN2_X_SRCID16TRAP)) {
            src = RIO_READ_FIELD(s, MSGIN2_X(i), MSG_IN2_X_SRCIDTRAP_HI_LO);
        } else {
            src = RIO_READ_FIELD(s, MSGIN2_X(i), MSG_IN2_X_SRCIDTRAP_LO);
        }

        if ((src == addr->src) && (mbox == addr->mbox)) {
            return i;
        }
    }

    /* We didn't find appropriate queue, so use 8'th queue */
    msgin_status = sc64_rio_readreg(s, MSGIN_STATUS_X(8));
    if (!(msgin_status & RIO_SHIFTED_MASK(MSG_IN_STATUS_INIT))) {
        DPRINTF("%s: error: MSG_IN_STATUS_INIT is not enabled\n"
             __func__);
        return -1;
    }

    return 8;
}

static void rio_fill_in_msg(SC64RIOState *s, int queue_nr,
    struct sc64_rio_msg_addr *addr, int msg_len,
    void *data, int seg_size)
{
    Descr descr;
    int descr_in;
    int i;
    uint8_t *src;
    hwaddr dst;
    int seg_cnt = (msg_len + seg_size - 1) / seg_size;
    int last_seg_size = msg_len - (seg_cnt - 1) * seg_size;

    descr_in = s->current_descr_in[queue_nr];
    descr = read_descriptor(s, descr_in);

    /* Fill in descriptor */
    descr.last_size = convert_message_size(
        msg_len - (seg_cnt - 1) * seg_size);
    descr.size = convert_message_size(seg_size);
    descr.id = addr->src;
    descr.letter = addr->letter;
    descr.mbox = addr->mbox;
    descr.length = seg_cnt - 1;

    /* Copy segments */
    src = data;
    dst = descr.seg1_addr;
    for (i = 0; i < descr.length + 1; i++) {
        seg_size = i == descr.length ? last_seg_size : seg_size;
        dma_memory_write(s->dma_as, dst, src, seg_size);
        dst = descr.segn_addr + seg_size * i;
        src += seg_size;
    }

    /* Close descriptor */
    descr.own = 0;
    descr.status = DESCR_STATUS_OK;
    write_descriptor(s, descr_in, descr);

    /* Update status and generate interrupt */
    if (queue_nr == 8) {
        RIO_WRITE_FIELD(s, MSG_IN_STATUS_9, MSG_IN_STATUS_9_DONE, 1);
        sc64_rio_update_msg_pol_in(s, descr.pol, 8);
    } else {
        sc64_rio_update_msg_pol_in(s, descr.pol, queue_nr);
    }
}

static void handle_message_in_timer(void *opaque)
{
    SC64RIOState *s = opaque;
    int descr_in;
    Descr descr;
    struct sc64_rio_rcv_descr *d;
    bool timer_run = false;
    int i;

    DPRINTF(">>>>>>>>>>> %s\n", __func__);

    for (i = 0; i < MSGIN_QUEUE_CNT; i++) {
        if (QTAILQ_EMPTY(&s->recv_queue[i])) {
            continue;
        }

        QTAILQ_FOREACH(d, &s->recv_queue[i], next) {
            descr_in = s->current_descr_in[i];
            descr = read_descriptor(s, descr_in);

            if (descr.own) {
                rio_fill_in_msg(s, i, &d->addr, d->msg_len,
                    d->data, d->seg_size);
                send_response(s, &d->addr, MSG_STATUS_DONE);

                QTAILQ_REMOVE(&s->recv_queue[i], d, next);
                free_rcv_descriptor(d);
                s->current_descr_in[i] = descr.next_descr_addr;
            } else {
                if (++d->timer_ticks > MSG_IN_TIMEOUT_TICKS) {
                    send_response(s, &d->addr, MSG_STATUS_ERROR);

                    QTAILQ_REMOVE(&s->recv_queue[i], d, next);
                    free_rcv_descriptor(d);
                } else {
                    timer_run = true;
                }
            }
        }
    }

    if (timer_run) {
        timer_mod(s->msg_in_timer,
            qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + MSG_IN_DELAY);
    }
}

static void handle_message(SC64RIOState *s, const uint8_t *buf)
{
    struct sc64_rio_rcv_descr *d;
    uint32_t lllen;
    struct sc64_rio_msg_addr addr;
    int queue_nr;
    int descr_in;
    int msg_len, seg_size;
    void *data;
    Descr descr;

    /* Unpack packet common header */
    addr.dst = (uint16_t) be16_to_cpu(*(uint16_t *)(buf + 5));
    addr.src = (uint16_t) be16_to_cpu(*(uint16_t *)(buf + 7));
    addr.mbox = buf[14];
    addr.letter = buf[13];
    lllen = (uint32_t) be32_to_cpu(*(uint32_t *)(buf + 9));

    data = (void *) &buf[16];
    msg_len = lllen - 7;
    seg_size = 1 << (buf[15] - 6);

    queue_nr = rio_find_in_queue(s, &addr);
    if (queue_nr == -1) {
        send_response(s, &addr, MSG_STATUS_ERROR);
        return;
    }

    descr_in = s->current_descr_in[queue_nr];
    descr = read_descriptor(s, descr_in);

    if (QTAILQ_EMPTY(&s->recv_queue[queue_nr])) {
        if (!descr.own) {
            d = create_rcv_descriptor(s, &addr, msg_len, data, seg_size);
            QTAILQ_INSERT_TAIL(&s->recv_queue[queue_nr], d, next);
            timer_mod(s->msg_in_timer,
                qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + MSG_IN_DELAY);
        } else {
            send_response(s, &addr, MSG_STATUS_DONE);
            rio_fill_in_msg(s, queue_nr, &addr, msg_len, data, seg_size);
            s->current_descr_in[queue_nr] = descr.next_descr_addr;
        }
    } else {
        d = create_rcv_descriptor(s, &addr, msg_len, data, seg_size);
        QTAILQ_INSERT_TAIL(&s->recv_queue[queue_nr], d, next);
    }

    DPRINTF("handle_message (In message): src=%x, dest=%x, ttype=%x, len=%d\n",
            addr.src, addr.dst, buf[2], lllen);
    DPRINTF("                  letter=%x, mbox=%x, msg_len=%d, seg_size=%d\n",
            addr.letter, addr.mbox, msg_len, seg_size);
    PRINT_REG(INTERRUPT);
    PRINT_REG(INTERRUPT_ENA);
    PRINT_REG(INTERRUPT_ADD1);
    PRINT_REG(INTERRUPT_ADD);

    PRINT_REG(MSG_IN1_1);
    PRINT_REG(MSG_IN1_2);
    PRINT_REG(MSG_IN1_3);
    PRINT_REG(MSG_IN1_4);
    PRINT_REG(MSG_IN1_5);
    PRINT_REG(MSG_IN1_6);
    PRINT_REG(MSG_IN1_7);
    PRINT_REG(MSG_IN1_8);
    PRINT_REG(MSG_IN1_9);
    PRINT_REG(MSG_IN_STATUS_9);
    PRINT_REG(MSG_OUT_STATUS);
    PRINT_REG(MSG_OUT_ERROR);
}

static void sc64_rio_reset(void *opaque)
{
    SC64RIOState *s = opaque;
    int i;

    for (i = 0; i < MSGIN_QUEUE_CNT; i++) {
        s->msg_pol_in[i] = 0;
    }
    s->msg_out_sent = 0;
    memset(s->reg, 0, sizeof(s->reg));
    sc64_rio_writereg(s, DEV_VEN_ID, 0x5A52191E);
    sc64_rio_writereg(s, CLASS, 0x06800000);
    sc64_rio_writereg(s, SID, 0x78340037);

    /* Mezzanine module address */
    sc64_rio_writereg(s, GA, 0x00001f20);

    sc64_rio_writereg(s, DEV_ID_CAR,
            (RIO_DID_VM6 << RIO_DEV_ID_CAR_DI_SHIFT) | RIO_VID_SRISA);

    sc64_rio_writereg(s, MSG_OUT3, 0xC);

    sc64_rio_writereg(s, DEV_INFO_CAR, 0x1);
    sc64_rio_writereg(s, ASM_ID_CAR, 0);
    sc64_rio_writereg(s, ASM_INFO_CAR, 0x100);
    sc64_rio_writereg(s, PE_FEAT_CAR,
            RIO_PEF_BRIDGE
            | RIO_PEF_MEMORY
            | RIO_PEF_PROCESSOR
            | RIO_PEF_SWITCH
            | RIO_PEF_STD_RT
            | RIO_PEF_EXT_FEATURES
            | RIO_PEF_ADDR_34);
    sc64_rio_writereg(s, SWITCH_PI_CAR, 0x300);
    sc64_rio_writereg(s, SOURCE_OP_CAR,
            RIO_SRC_OPS_READ
            | RIO_SRC_OPS_WRITE
            | RIO_SRC_OPS_STREAM_WRITE
            | RIO_SRC_OPS_WRITE_RESPONSE
            | RIO_SRC_OPS_DATA_MSG
            | RIO_SRC_OPS_DOORBELL
            | RIO_SRC_OPS_PORT_WRITE);
    sc64_rio_writereg(s, DEST_OP_CAR,
            RIO_DST_OPS_READ
            | RIO_DST_OPS_WRITE
            | RIO_DST_OPS_STREAM_WRITE
            | RIO_DST_OPS_WRITE_RESPONSE
            | RIO_DST_OPS_DATA_MSG
            | RIO_DST_OPS_DOORBELL
            | RIO_DST_OPS_PORT_WRITE);
    sc64_rio_writereg(s, LIMIT_ID_CAR, (512 - 1));

    sc64_rio_writereg(s, PE_LL_CTL_CSR, RIO_PELL_ADDR_34);

    sc64_rio_writereg(s, BASE_DEV_CSR, 0);
    sc64_rio_writereg(s, HB_DEV_ID_LOCK_CSR,
            RIO_HB_DEV_ID_LOCK_CSR_HBDID_RESET);
    sc64_rio_writereg(s, COMP_TAG_CSR, 0);

    sc64_rio_writereg(s, PORT_LT_CTL, 0xFFFFFF00);
    sc64_rio_writereg(s, PORT_RT_CTL, 0xFFFFFF00);
    sc64_rio_writereg(s, PORT0_ERR_STAT, RIO_PORT_N_ERR_STS_PORT_UNINIT);
    sc64_rio_writereg(s, PORT1_ERR_STAT, RIO_PORT_N_ERR_STS_PORT_UNINIT);
    sc64_rio_writereg(s, PORT2_ERR_STAT, RIO_PORT_N_ERR_STS_PORT_UNINIT);
    sc64_rio_writereg(s, PORT0_CTL, 0);
    sc64_rio_writereg(s, PORT1_CTL, RIO_PORT_N_CTL_PWIDTH_4 |
                                    RIO_PORT_N_CTL_P_TYP_SER);
    sc64_rio_writereg(s, PORT2_CTL, RIO_PORT_N_CTL_PWIDTH_1 |
                                    RIO_PORT_N_CTL_P_TYP_SER);
}

static void rio_send_maintenance(SC64RIOState *s,
    uint8_t hopcount, uint32_t offset)
{
    uint32_t m1, data;
    m1 = sc64_rio_readreg(s, MAINT_OUT1);
    uint8_t ttype, be;
    uint16_t dest, src;
    int pkt_len;
    ttype = (m1 >> 20) & 1;
    dest = m1 & 0x000000ff;
    src = (sc64_rio_readreg(s, BASE_DEV_CSR) >> 16) & 0xff;
    be = (m1 >> 24) & 0xff;
    uint8_t pkt[64];

    switch (be) {
    case 0xf0:
        offset += 4;
        data = sc64_rio_readreg(s, MAINT_OUT3);
        break;
    case 0x0f:
        data = sc64_rio_readreg(s, MAINT_OUT2);
        break;
    default:
        DPRINTF("sc64_rio: wrong be=%d\n", be);
        break;
    }

    DPRINTF("sc64: Out packet: src=%x, dest=%x, hc=%x, offset=%x, ttype=%d\n",
                            src, dest, hopcount, offset, ttype);
    DPRINTF("data: %x [ %08x %08x ]\n", data,
        sc64_rio_readreg(s, MAINT_OUT2),
        sc64_rio_readreg(s, MAINT_OUT3)
    );

    s->maint_tid++;
    pkt_len = mk_pkt_type(pkt, IO_WRITE);
    pkt_len += mk_pkt_header(pkt + pkt_len, FTYPE_MAINTENANCE,
        ttype,
        dest,
        src);

    if (ttype) {
        pkt_len += mk_maint_write_request(pkt + pkt_len,
            hopcount,
            s->maint_tid,
            offset,
            data);
    } else {
        pkt_len += mk_maint_read_request(pkt + pkt_len,
            hopcount,
            s->maint_tid,
            offset);
    }

    sc64_rio_writereg(s, RIO_M_STATUS, M_STATUS_BUSY);
    qemu_chr_fe_write(&s->chr, pkt, pkt_len);
    timer_mod(s->maint_timer,
        qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 15 * NANOSECONDS_PER_SECOND);
}

static void
sc64_rio_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                 unsigned size)
{
    uint32_t old_val;
    uint32_t disabled_irqs;
    int queue_nr = -1;
    SC64RIOState *s = opaque;

    DPRINTF("sc64: write: addr=%" HWADDR_PRIx ", val=%" PRIx64 "\n", addr, val);

    addr &= ~3;

    switch (addr) {
    case PRIORITY:
        RIO_WRITE_MASKED(s, addr, val, PRIORITY_WMASK);
        break;
    case INTERRUPT: /* read only */
        DPRINTF("sc64_rio_mmio_write: Write to read-only %lx\n", addr);
        break;
    case INTERRUPT_ADD:
        DPRINTF("sc64_rio_mmio_write: Write to read-only %lx\n", addr);
        break;
    case INTERRUPT_ADD1:
        DPRINTF("sc64_rio_mmio_write: Write to read-only %lx\n", addr);
        break;
    case INTERRUPT_ENA:
        val &= RIO_SHIFTED_MASK(INTERRUPT_ENA_WMASK);
        disabled_irqs = sc64_rio_readreg(s, addr) & ~val;
        rio_update_interrupt(s, 0, disabled_irqs, 0);
        sc64_rio_writereg(s, addr, val);
        break;
    case INTERRUPT_ENA_ADD:
        val &= RIO_SHIFTED_MASK(INTERRUPT_ENA_ADD_WMASK);
        disabled_irqs = sc64_rio_readreg(s, addr) & ~val;
        rio_update_interrupt(s, 1, disabled_irqs, 0);
        sc64_rio_writereg(s, addr, val);
        break;
    case INTERRUPT_ENA_ADD1:
        val &= RIO_SHIFTED_MASK(INTERRUPT_ENA_ADD1_WMASK);
        disabled_irqs = sc64_rio_readreg(s, addr) & ~val;
        rio_update_interrupt(s, 2, disabled_irqs, 0);
        sc64_rio_writereg(s, addr, val);
        break;
    case MAINT_OUT_TIMER:
        RIO_WRITE_MASKED(s, addr, val, MAINT_OUT_TIMER_WMASK);
        break;
    case MAINT_OUT1:
        RIO_WRITE_MASKED(s, addr, val, MAINT_OUT1_WMASK);
        break;
    case MAINT_OUT2:
    case MAINT_OUT3:
        sc64_rio_writereg(s, addr, val);
        break;
    case MAINT_OUT4:
        RIO_WRITE_MASKED(s, addr, val, MAINT_OUT4_WMASK);
        if (!timer_pending(s->maint_timer)) {
            rio_send_maintenance(s, val >> 24, val & 0xffffff);
        }
        break;
    case MAINT_OUT5:
        /* Has read-only & reset bits.
         * To reset bits write 1 to them.
         */
        val &= RIO_SHIFTED_MASK(MAINT_OUT5_WMASK);
        old_val = sc64_rio_readreg(s, addr);

        if (val) {
            sc64_rio_writereg(s, addr, (old_val & (~val)));
        }
        break;
    case MAINT_OUT6:
    case MAINT_OUT7:
        /* Each writing makes both of registers 0. */
        sc64_rio_writereg(s, MAINT_OUT6, 0);
        sc64_rio_writereg(s, MAINT_OUT7, 0);
        break;
    case GA:
    case DEV_ID_CAR:
    case DEV_INFO_CAR:
    case ASM_ID_CAR:
    case ASM_INFO_CAR:
    case PE_FEAT_CAR:
    case SWITCH_PI_CAR:
    case SOURCE_OP_CAR:
    case DEST_OP_CAR:
    case LIMIT_ID_CAR:
    case PE_LL_CTL_CSR:
        goto guest_error;

    case BASE_DEV_CSR:
        RIO_WRITE_MASKED(s, addr, val, RIO_BASE_DEV_CSR_WMASK);
        break;
    case HB_DEV_ID_LOCK_CSR:
        old_val = sc64_rio_readreg(s, addr);
        val &= RIO_SHIFTED_MASK(RIO_HB_DEV_ID_LOCK_CSR_WMASK);
        if (old_val == RIO_HB_DEV_ID_LOCK_CSR_HBDID_RESET) {
            sc64_rio_writereg(s, addr, val);
        } else if (old_val == val) {
            sc64_rio_writereg(s, addr, RIO_HB_DEV_ID_LOCK_CSR_HBDID_RESET);
        }
        break;
    case COMP_TAG_CSR:
        sc64_rio_writereg(s, addr, val);
        break;
    case MSG_OUT_STATUS:
        rio_update_register_with_irq(s, addr, val,
            RIO_SHIFTED_MASK(MSG_OUT_STATUS_IRQ),
            RIO_SHIFTED_MASK(MSG_OUT_STATUS_WMASK));

        /* setting up MsgOutStatusInit starts the message transmittion */
        if ((val & RIO_SHIFTED_MASK(MSG_OUT_STATUS_INIT)) != 0) {
            if (!timer_pending(s->msg_out_timer)) {
                rio_message_send(s);
            }
        }
        break;
    case MSG_OUT_ERROR:
        val = rio_update_register_with_irq(s, addr, val,
            RIO_SHIFTED_MASK(MSG_OUT_ERROR_ERR_IRQ),
            RIO_SHIFTED_MASK(MSG_OUT_ERROR_ERR_WMASK));
        break;
    case MSG_OUT1:
        s->current_descr_out = val;
        sc64_rio_writereg(s, addr, val);
        break;
    case MSG_OUT2:
        sc64_rio_writereg(s, addr, val);
        sc64_rio_update_waterline(s);
        break;
    case MSG_OUT3:
        sc64_rio_writereg(s, addr, val);
        break;
    case MSG_IN_ERROR_9:
        rio_update_register_with_irq(s, addr, val,
            RIO_SHIFTED_MASK(MSG_IN_ERROR_9_IRQ),
            RIO_SHIFTED_MASK(MSG_IN_ERROR_9_WMASK));

        if ((val & RIO_SHIFTED_MASK(MSG_IN_ERROR_9_WMASK)) == 0) {
            RIO_WRITE_FIELD(s, MSG_IN_STATUS_9,
                MSG_IN_STATUS_9_FAIL, 0);
            rio_update_interrupt(s, 0,
                RIO_SHIFTED_MASK(INTERRUPT_MSGOUTERR), 0);
        }
        break;
    case MSG_IN1_1:
    case MSG_IN1_2:
    case MSG_IN1_3:
    case MSG_IN1_4:
    case MSG_IN1_5:
    case MSG_IN1_6:
    case MSG_IN1_7:
    case MSG_IN1_8:
    case MSG_IN1_9:
        queue_nr = get_queue_nr(addr);
        s->current_descr_in[queue_nr] = val;
        sc64_rio_writereg(s, addr, val);
        break;
    case MSG_IN2_1:
    case MSG_IN2_2:
    case MSG_IN2_3:
    case MSG_IN2_4:
    case MSG_IN2_5:
    case MSG_IN2_6:
    case MSG_IN2_7:
    case MSG_IN2_8:
    case MSG_IN2_9:
        sc64_rio_writereg(s, addr, val);
        break;
    case MSG_IN_STATUS_1:
    case MSG_IN_STATUS_2:
    case MSG_IN_STATUS_3:
    case MSG_IN_STATUS_4:
    case MSG_IN_STATUS_5:
    case MSG_IN_STATUS_6:
    case MSG_IN_STATUS_7:
    case MSG_IN_STATUS_8:
        if (!(val & RIO_SHIFTED_MASK(MSG_IN_STATUS_INIT))) {
            /* Remain old value of MSG_IN_STATUS_9_INIT */
            old_val = sc64_rio_readreg(s, addr);
            val |= old_val & RIO_SHIFTED_MASK(MSG_IN_STATUS_INIT);
        }
        if (val & RIO_SHIFTED_MASK(MSG_IN_STATUS_STOP)) {
            val &= ~RIO_SHIFTED_MASK(MSG_IN_STATUS_INIT);
        }

        rio_update_register_with_irq(s, addr, val,
            RIO_SHIFTED_MASK(MSG_IN_STATUS_1_8_IRQ),
            RIO_SHIFTED_MASK(MSG_IN_STATUS_1_8_WMASK));
        break;
    case MSG_IN_STATUS_9:
        if (!(val & RIO_SHIFTED_MASK(MSG_IN_STATUS_9_INIT))) {
            /* Remain old value of MSG_IN_STATUS_9_INIT */
            old_val = sc64_rio_readreg(s, addr);
            val |= old_val & RIO_SHIFTED_MASK(MSG_IN_STATUS_9_INIT);
        }
        if (val & RIO_SHIFTED_MASK(MSG_IN_STATUS_9_STOP)) {
            val &= ~RIO_SHIFTED_MASK(MSG_IN_STATUS_9_INIT);
        }

        rio_update_register_with_irq(s, addr, val,
            RIO_SHIFTED_MASK(MSG_IN_STATUS_9_IRQ),
            RIO_SHIFTED_MASK(MSG_IN_STATUS_9_WMASK));
        break;
    case MSG_IN3_9:
    case ADD_W_R2:
    case ADD_W_R3:
    case MSG_IN3_ADD1:
    case MSG_IN3_ADD2:
    case MSG_IN3_ADD3:
    case MSG_IN3_ADD4:
        sc64_rio_writereg(s, addr, val);
        break;
    case PORT_LT_CTL:
    case PORT_RT_CTL:
    case PORT0_ERR_STAT:
    case PORT1_ERR_STAT:
    case PORT2_ERR_STAT:
    case PORT0_CTL:
    case PORT1_CTL:
    case PORT2_CTL:
        /* stub phys level registers: just ignore writes */
        break;
    case DBOUT1:
    case DBOUT2:
        sc64_rio_writereg(s, addr, val);
        break;
    case DBOUT_STATUS:
        if (val & RIO_SHIFTED_MASK(DBOUT_DONE)) {
            val &= ~RIO_SHIFTED_MASK(DBOUT_DONE);
        }

        sc64_rio_writereg(s, addr, val);

        if (val & RIO_SHIFTED_MASK(DBOUT_PROGRESS)) {
            if (!timer_pending(s->doorbell_timer)) {
                rio_send_doorbell(s);
            }
        }
        break;
    case DBOUT_ERROR:
        old_val = sc64_rio_readreg(s, addr);
        sc64_rio_writereg(s, addr, old_val & ~val);
        break;
    case DBIN_STATUS1:
    case DBIN_STATUS2:
        sc64_rio_writereg(s, addr, val);
        break;
    case DBIN_STATUS_CONTROL:
        if (val & RIO_SHIFTED_MASK(DBIN_DONE)) {
            val &= ~RIO_SHIFTED_MASK(DBIN_DONE);
        }

        sc64_rio_writereg(s, addr, val);
        break;

    default:
        DPRINTF("write: unimplemented register: %lx, val = %lx\n", addr, val);
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented register: %"PRIx64"\n",
                __func__, addr);
    }

    return;

guest_error:
    qemu_log_mask(LOG_GUEST_ERROR,
                "%s: writing to read-only register: %"PRIx64"\n",
                __func__, addr);
}

static uint64_t
sc64_rio_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    SC64RIOState *s = opaque;
    uint64_t val = 0;

    addr &= ~3;

    switch (addr) {
    case PRIORITY:
    case GA:
    case DEV_VEN_ID:
    case CLASS:
    case SID:
    case INTERRUPT:
    case INTERRUPT_ADD:
    case INTERRUPT_ADD1:
    case INTERRUPT_ENA:
    case INTERRUPT_ENA_ADD:
    case INTERRUPT_ENA_ADD1:
    case MAINT_OUT_TIMER:
    case MAINT_OUT1:
    case MAINT_OUT2:
    case MAINT_OUT3:
    case MAINT_OUT4:
    case MAINT_OUT5:
    case MAINT_OUT6:
    case MAINT_OUT7:
    case MSG_OUT1:
    case MSG_OUT2:
    case MSG_OUT3:
    case MSG_OUT_STATUS:
    case MSG_OUT_ERROR:
    case MSG_IN1_9:
    case MSG_IN2_9:
    case MSG_IN_STATUS_9:
    case MSG_IN_ERROR_9:
    case MSG_IN1_1:
    case MSG_IN2_1:
    case MSG_IN_STATUS_1:
    case MSG_IN1_2:
    case MSG_IN2_2:
    case MSG_IN_STATUS_2:
    case MSG_IN3_9:
    case MSG_IN1_3:
    case MSG_IN2_3:
    case MSG_IN_STATUS_3:
    case MSG_IN1_4:
    case MSG_IN2_4:
    case MSG_IN_STATUS_4:
    case MSG_IN1_5:
    case MSG_IN2_5:
    case MSG_IN_STATUS_5:
    case MSG_IN1_6:
    case MSG_IN2_6:
    case MSG_IN_STATUS_6:
    case MSG_IN1_7:
    case MSG_IN2_7:
    case MSG_IN_STATUS_7:
    case MSG_IN1_8:
    case MSG_IN2_8:
    case MSG_IN_STATUS_8:
    case ADD_W_R2:
    case ADD_W_R3:
    case MSG_IN3_ADD1:
    case MSG_IN3_ADD2:
    case MSG_IN3_ADD3:
    case MSG_IN3_ADD4:
    case DEV_ID_CAR:
    case DEV_INFO_CAR:
    case ASM_ID_CAR:
    case ASM_INFO_CAR:
    case PE_FEAT_CAR:
    case SWITCH_PI_CAR:
    case SOURCE_OP_CAR:
    case DEST_OP_CAR:
    case LIMIT_ID_CAR:
    case PE_LL_CTL_CSR:
    case BASE_DEV_CSR:
    case HB_DEV_ID_LOCK_CSR:
    case COMP_TAG_CSR:
    case PORT_LT_CTL:
    case PORT_RT_CTL:
    case PORT0_ERR_STAT:
    case PORT1_ERR_STAT:
    case PORT2_ERR_STAT:
    case PORT0_CTL:
    case PORT1_CTL:
    case PORT2_CTL:
    case DBOUT1:
    case DBOUT2:
    case DBOUT_STATUS:
    case DBOUT_ERROR:
    case DBIN_STATUS1:
    case DBIN_STATUS2:
    case DBIN_STATUS_CONTROL:
    case LOG_ERR_DET:
        val = sc64_rio_readreg(s, addr);
        break;

    default:
        DPRINTF("read: unimplemented register: %lx\n", addr);
        qemu_log_mask(LOG_UNIMP,
                "%s: unimplemented register offset: %"PRIx64"\n",
                __func__, addr);
    }
    return val;
}

static const MemoryRegionOps sc64_rio_mmio_ops = {
    .read = sc64_rio_mmio_read,
    .write = sc64_rio_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const VMStateDescription vmstate_sc64_rio = {
    .name = "sc64-rio",
    .version_id = SC64_RIO_VERSION,
    .minimum_version_id = SC64_RIO_VERSION
};

static int sc64_rio_can_rx(void *opaque)
{
    return 65536;
}

static void handle_maintenance(SC64RIOState *s, const uint8_t *buf)
{
    uint8_t ttype;
    uint8_t tid;

    ttype = buf[14];
    tid = buf[16];

    switch (ttype) {
    case TTYPE_MAINT_READ_REQUEST:
    case TTYPE_MAINT_WRITE_REQUEST:
    {
        uint8_t pkt[64];
        int pkt_len;

        uint32_t offset;
        uint32_t data;
        uint32_t in_srcid, in_dstid;
        uint32_t addr;

        in_dstid = (uint16_t) be16_to_cpu(*(uint16_t *)(buf + 5));
        in_srcid = (uint16_t) be16_to_cpu(*(uint16_t *)(buf + 7));

        offset = ldl_be_p(buf + 17);
        pkt_len = mk_pkt_type(pkt, IO_WRITE);
        pkt_len += mk_pkt_header(pkt + pkt_len, FTYPE_MAINTENANCE,
                TT_8,
                in_srcid,
                in_dstid);

        if (ttype == TTYPE_MAINT_WRITE_REQUEST) {
            data = ldl_be_p(buf + 21);
            addr = sc64_rio_reg_offset(offset);
            sc64_rio_writereg(s, addr, data);
            pkt_len += mk_maint_write_response(pkt + pkt_len, 255, tid);
        } else if (ttype == TTYPE_MAINT_READ_REQUEST) {
            addr = sc64_rio_reg_offset(offset);
            data = sc64_rio_readreg(s, addr);
            pkt_len += mk_maint_read_response(pkt + pkt_len, 255, tid, data);
        }
        qemu_chr_fe_write(&s->chr, pkt, pkt_len);
    }
        break;
    case TTYPE_MAINT_READ_RESPONSE:
    case TTYPE_MAINT_WRITE_RESPONSE:
        assert(timer_pending(s->maint_timer));

        uint8_t status;
        status = buf[15];

        if (tid == s->maint_tid) {
            timer_del(s->maint_timer);

            if (ttype == TTYPE_MAINT_READ_RESPONSE) {
                uint32_t data = ldl_be_p(buf + 17);

                /* Fill both halves since we emulate only 4 byte sizes */
                sc64_rio_writereg(s, RIO_M_DATAIN_LO, data);
                sc64_rio_writereg(s, RIO_M_DATAIN_HI, data);
            }
            if (status == 0) {
                sc64_rio_writereg(s, RIO_M_STATUS, M_STATUS_DONE);
            } else {
                sc64_rio_writereg(s, RIO_M_STATUS,
                        M_STATUS_DONE | M_STATUS_ERROR);
            }
        }
    }
}

#define min(a, b) ((a) < (b) ? (a) : (b))

static void sc64_gather(SC64RIOState *s, const uint8_t **ptr_buf, int *ptr_size)
{
    const uint8_t *buf = *ptr_buf;
    int size = *ptr_size;

    if (s->in_pos == 0) {
        s->in_await = 0;
    }

    if (s->in_await == 0) {
        if (s->in_pos < 13) {
            int sz = min(size, 13 - s->in_pos);
            memcpy(s->in_buffer + s->in_pos, buf, sz);
            s->in_pos += sz;
            buf += sz;
            size -= sz;
        }

        if (s->in_pos >= 13) {
            uint32_t payload;
            memcpy(&payload, s->in_buffer + 1 + 8, 4);
            s->in_await = s->in_pos + be32_to_cpu(payload) - 4;
        }
    }

    if (s->in_await != 0) {
        int sz = min(size, s->in_await - s->in_pos);
        memcpy(s->in_buffer + s->in_pos, buf, sz);
        s->in_pos += sz;
        buf += sz;
        size -= sz;
    }

    *ptr_buf = buf;
    *ptr_size = size;
}

static bool sc64_gather_inprogress(SC64RIOState *s)
{
    return s->in_pos != 0;
}

static bool sc64_gather_complete(SC64RIOState *s)
{
    return sc64_gather_inprogress(s) && s->in_await == s->in_pos;
}

static void sc64_gather_reset(SC64RIOState *s)
{
    s->in_pos = 0;
    s->in_await = 0;
}

/* Returns false if the whole buffer is consumed and more bytes needed */
static bool sc64_rio_rx_packet(SC64RIOState *s, const uint8_t **ptr_buf,
    int *ptr_size)
{
    const uint8_t *buf = *ptr_buf;
    int size = *ptr_size;
    uint8_t ftype;
    bool rc = true;

    sc64_gather(s, &buf, &size);
    if (!sc64_gather_complete(s)) {
        rc = false;
        goto out;
    }

    ftype = s->in_buffer[1];
    switch (ftype) {
    case FTYPE_MAINTENANCE:
        handle_maintenance(s, s->in_buffer);
        break;
    case FTYPE_MESSAGE:
        handle_message(s, s->in_buffer);
        break;
    case FTYPE_DOORBELL:
        handle_doorbell(s, s->in_buffer);
        break;
    case FTYPE_RESPONSE:
        handle_response(s, s->in_buffer);
        break;
    default:
        DPRINTF("Unknown packet ftype\n");
        break;
    }

out:
    *ptr_buf = buf;
    *ptr_size = size;
    return rc;
}

static void sc64_rio_rx(void *opaque, const uint8_t *buf, int size)
{
    SC64RIOState *s = opaque;

    sc64_rio_print_raw_packet("RECEIVE (sc64_rio_rx)", buf, size);

    if (sc64_gather_inprogress(s)) {
        if (!sc64_rio_rx_packet(s, &buf, &size)) {
            return;
        }
        sc64_gather_reset(s);
    }

    while (size > 0) {
        switch (buf[0]) {
        case 'C':
            sc64_rio_writereg(s, PORT0_ERR_STAT, RIO_PORT_N_ERR_STS_PORT_OK);
            size--;
            break;
        case IO_WRITE:
            if (!sc64_rio_rx_packet(s, &buf, &size)) {
                return;
            }
            break;
        default:
            DPRINTF("Unknown packet type\n");
            break;
        }
        sc64_gather_reset(s);
    }
}

void sc64_rio_register(const char *name, hwaddr addr,
    AddressSpace *dma_as, qemu_irq irq)
{
    DeviceState *dev;
    SysBusDevice *sbd;
    SC64RIOState *s;

    dev = qdev_create(NULL, name);
    s = SC64_RIO(dev);
    s->dma_as = dma_as;
    qdev_init_nofail(dev);

    sbd = SYS_BUS_DEVICE(dev);
    if (addr != (hwaddr)-1) {
        sysbus_mmio_map(sbd, 0, addr);
    }
    sysbus_connect_irq(sbd, 0, irq);
}

static void sc64_rio_realize(DeviceState *dev, Error **errp)
{
    SC64RIOState *state = SC64_RIO(dev);
    int i = 0;
    Chardev *fe = qemu_chr_find("rio");

    if (fe != NULL) {
        qemu_chr_fe_init(&state->chr, fe, NULL);
    }
    qemu_chr_fe_set_handlers(&state->chr, sc64_rio_can_rx, sc64_rio_rx, NULL,
        NULL, state, NULL, false);
    qemu_chr_fe_write(&state->chr, (const uint8_t *)"C", 1);

    for (i = 0; i < MSGIN_QUEUE_CNT; i++) {
        QTAILQ_INIT(&state->recv_queue[i]);
    }

    state->maint_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, rio_maint_timer,
        state);
    state->doorbell_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                rio_doorbell_timer, state);
    state->msg_in_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                handle_message_in_timer, state);
    state->msg_out_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                rio_message_send_timer, state);
}

static void sc64_rio_init(Object *obj)
{
    SysBusDevice *dev = SYS_BUS_DEVICE(obj);
    SC64RIOState *state = DO_UPCAST(SC64RIOState, busdev, dev);

    memory_region_init_io(
            &state->mmio,
            OBJECT(state),
            &sc64_rio_mmio_ops, state,
            TYPE_SC64_RIO,
            0x200000);
    sysbus_init_mmio(dev, &state->mmio);
    sysbus_init_irq(dev, &state->irq);
}

static void qdev_sc64_rio_reset(DeviceState *dev)
{
    SC64RIOState *d = SC64_RIO(dev);
    sc64_rio_reset(d);
}

static void sc64_rio_class_init(ObjectClass *cl, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(cl);

    dc->realize = sc64_rio_realize;
    dc->vmsd  = &vmstate_sc64_rio;
    dc->reset = qdev_sc64_rio_reset;
}

static TypeInfo sc64_rio_info = {
    .name = TYPE_SC64_RIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SC64RIOState),
    .instance_init = sc64_rio_init,
    .class_init = sc64_rio_class_init,
};

static void sc64_rio_register_types(void)
{
    type_register_static(&sc64_rio_info);
}

type_init(sc64_rio_register_types);
