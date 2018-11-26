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

#ifndef SC64_RIO_H_
#define SC64_RIO_H_

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "hw/rapidio/rio_regs.h"
#include "qemu/timer.h"
#include "chardev/char-fe.h"

#define TYPE_SC64_RIO "sc64-rio"
#define SC64_RIO(obj) OBJECT_CHECK(SC64RIOState, (obj), TYPE_SC64_RIO)

/* RapidIO controller version */
#define SC64_RIO_VERSION 1

/**
 *  * tt field values
 *   */
#define TT_8 0
#define TT_16 1

#define FTYPE_MAINTENANCE 8
#define FTYPE_DOORBELL    10
#define FTYPE_MESSAGE     11
#define FTYPE_RESPONSE    13
/**
 * maintenance transaction type field values
 */
#define TTYPE_MAINT_READ_REQUEST 0
#define TTYPE_MAINT_WRITE_REQUEST 1
#define TTYPE_MAINT_READ_RESPONSE 2
#define TTYPE_MAINT_WRITE_RESPONSE 3
#define TTYPE_MAINT_PORT_WRITE_REQUEST 4

/**
 * maintenance transaction type field values
 */
#define TTYPE_MESSAGE 0
#define TTYPE_DOORBELL 1

typedef struct Descr {
    /* The first word of the descriptor
       OUT: [47:32] - DstIDOut - destination identifier
       IN:  [47:32] - SrcIdIn  - sender identifier */
    uint id;
    /* OUT: [31] - OwnOut - if set, the descriptor is used by the controller
       IN:  [31] - OwnIn - if set, -//- */
    uint own;
    /* OUT: [30:16] - StatusOut - [14], [1], [0] may be used
       IN:  [30:16] - StatusIn -//-
        [1] - the length of id (0 - 8 bytes, 1 - 9 bytes)
        [0] - some error has happened */
    uint status;
    /* OUT: [15:12] - MsgSizeLastOut - size of the last segment:
       IN:  [15:12] - MsgSizeLastIn -//-
                 9 - 8 bytes, a - 16 bytes ... e - 256 bytes */
    uint last_size;
    /* OUT: [11:8] - MsgSizeOut - size of the each segment expect the last one
       (the same principle as MsgSizeLastOut)
       IN:  [11:8] - MsgSizeIn -//- */
    uint size;
    /* OUT: [7:4] - MsgLenOut - the number of segments: 0 - only one
                   1 - two ... f - sixteen
       IN:  [7:4] - MsgLenIn -//- */
    uint length;
    /* OUT: [3:2] - MBoxOut - the number of the mail box
       IN:  [3:2] - MBoxIn -//- */
    uint mbox;
    /* OUT: [1:0] - LetterOut - the number of letter in the mail box
       IN:  [1:0] - LetterIn -//- */
    uint letter;
    /* The second word of the descriptor
       OUT: [63:35] - Asrc1 - pointer to the first segment; should be aligned
       IN:  [63:35] - Adst1 -//- */
    uint seg1_addr;
    /* The third word of the descriptor
       OUT: [63:35] - Asrc2 - pointer to the other segments; should be aligned
       IN:  [63:35] - Adst2 -//- */
    uint segn_addr;
    /* The fourth word of the descriptor
       OUT: [63:35] - NextOut - pointer to the following descriptor;
                 should be aligned to 8 bytes
       IN:  [63:35] - NextIn -//- */
    uint next_descr_addr;
    /* OUT: [33] - PolOut - if set, make interrupt when closing the descriptor
       IN:  [33] - PolIn -//- */
    uint pol;
    /* OUT: [32] - EolOut - if set, this descriptor is the last one
       IN:  [32] - EolIn -//- */
    uint eol;
} Descr;

struct sc64_rio_msg_addr {
    uint16_t src;
    uint16_t dst;
    uint8_t mbox;
    uint8_t letter;
};

typedef struct sc64_rio_rcv_descr {
    struct sc64_rio_msg_addr addr;
    int msg_len; /* The whole length of the message with all segments */
    int seg_size; /* One segment size (not the last segment) */
    int timer_ticks; /* Timeout */
    QTAILQ_ENTRY(sc64_rio_rcv_descr) next;
    uint8_t data[4096]; /* Received message data */
} sc64_rio_rcv_descr;

struct sc64_rio_doorbell {
    uint8_t tid;
    uint8_t lsb;
    uint8_t msb;
};

#define SC64_RIO_REG_NUM 0x40000
#define MSGIN_QUEUE_CNT 9

typedef struct SC64RIOState {
    SysBusDevice busdev;
    MemoryRegion mmio;
    qemu_irq irq;

    uint8_t maint_tid;
    uint8_t doorbell_tid;

    uint32_t current_descr_out;
    uint32_t current_descr_in[MSGIN_QUEUE_CNT];
    QTAILQ_HEAD(, sc64_rio_rcv_descr) recv_queue[MSGIN_QUEUE_CNT];

    QEMUTimer *msg_in_timer;
    QEMUTimer *msg_out_timer;
    QEMUTimer *maint_timer;
    QEMUTimer *doorbell_timer;

    /* Count of closed descriptors */
    uint8_t msg_pol_in[MSGIN_QUEUE_CNT];
    /* Count of messages sent */
    int msg_out_sent;
    int waterline;

    uint32_t reg[SC64_RIO_REG_NUM];

    AddressSpace *dma_as;

    CharBackend chr;

    uint8_t in_buffer[4096 * 2];
    int in_pos;
    int in_await;
} SC64RIOState;

static inline uint32_t
sc64_rio_readreg(SC64RIOState *s, uint32_t index)
{
    uint32_t value = s->reg[index >> 2];
    return value;
}

static inline void
sc64_rio_writereg(SC64RIOState *s, uint32_t index, uint32_t val)
{
    s->reg[index >> 2] = val;
}

void sc64_rio_register(const char *name, hwaddr addr,
    AddressSpace *dma_as, qemu_irq irq);

#endif /* SC64_RIO_H_ */
