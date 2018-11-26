/*
 * CAN device - Duna emulation for QEMU
 *
 * Copyright (c) 2018 Denis Deryugin <deryugin.denis@gmail.com>
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

#include "qemu/osdep.h"
#include "chardev/char.h"
#include "hw/can/sc64-can.h"
#include "hw/hw.h"
#include "hw/mips/sccr-sysbus.h"
#include "hw/sysbus.h"
#include "hw/timer/sc64-timer.h"
#include "qemu/error-report.h"
#include "qemu/timer.h"
#include "sysemu/sysemu.h"

/* #define DEBUG */

#ifdef DEBUG
#define dbg printf
#else
#define dbg(...)
#endif

#define TYPE_SC64_CAN "sc64-can"
#define SC64_CAN(obj) \
    OBJECT_CHECK(Sc64CanState, (obj), TYPE_SC64_CAN)

#define REG_CANID           0x000
#define REG_CANME           0x100
#define REG_CANMD           0x104
#define REG_CANTRS          0x108
#define REG_CANTRR          0x10C
#define REG_CANTA           0x110
#define REG_CANAA           0x114
#define REG_CANRMP          0x118
#define REG_CANRML          0x11C
#define REG_CANRFP          0x120
#define REG_CANMC           0x128
# define CANMC_SRES         (1 << 5)
# define CANMC_STM          (1 << 6)
# define CANMC_PDR          (1 << 11)
# define CANMC_CCR          (1 << 12)
# define CANMC_LOM          (1 << 16)
# define CANMC_ETM          (1 << 17)
#define REG_CANBTC          0x12C
#define REG_CANES           0x130
# define CANES_TM           (1 << 0)
# define CANES_RM           (1 << 1)
# define CANES_PDA          (1 << 3)
# define CANES_CCE          (1 << 4)
#define REG_CANTEC          0x134
#define REG_CANREC          0x138
#define REG_CANGIF0         0x13C
# define CANGIF0_RMLIF      (1 << 11)
# define CANGIF0_WDIF       (1 << 13)
# define CANGIF0_AAIF       (1 << 14)
# define CANGIF0_GMIF       (1 << 15)
# define CANGIF0_MAIF       (1 << 17)
#define CANGIF0_MIV_MASK    0x1F
#define REG_CANGIM          0x140
# define CANGIM_RMLIM      (1 << 11)
# define CANGIM_WDIM       (1 << 13)
# define CANGIM_AAIM       (1 << 14)
# define CANGIM_MAIM       (1 << 17)
#define REG_CANGIF1         0x144
#define REG_CANMIM          0x148
#define REG_CANMIL          0x14C
#define REG_CANOPC          0x150
#define REG_CANTIOC         0x154
# define CANTIOC_TXFUNC     (1 << 3)
#define REG_CANRIOC         0x158
# define CANRIOC_RXFUNC     (1 << 3)
#define REG_CANLNT          0x15C
#define REG_CANTOC          0x160
#define REG_CANTOS          0x164
#define REG_CANTRCT         0x168
#define REG_CANSS           0x16C
#define REG_CANGTH          0x170
#define REG_CANGTL          0x174
#define REG_CANCE           0x178
#define CANCE_LIMIT_MASK    0x1F
# define CANCE_CLR          (1 << 16)
# define CANCE_Empty        (1 << 17)
# define CANCE_AlmFull      (1 << 18)
# define CANCE_Full         (1 << 19)
#define REG_CANEE           0x17C
#define REG_CANEH           0x180
#define REG_CANEM           0x184
#define REG_CANEL           0x188
#define REG_CANLRBC         0x18C
#define REG_CANEWC          0x190
#define REG_CANEPC          0x194
#define REG_CANBOC          0x198
#define REG_CANTES          0x19C
#define REG_CANRES          0x1A0
#define REG_CANSRME         0x1A4
#define REG_CANSRMS         0x1A8
#define REG_Id(n)           (0x400 + ((n) * 0x10))
#define REG_Ctrl(n)         (0x404 + ((n) * 0x10))
# define Ctrl_RTR           (1 << 4)
#define REG_DataLo(n)       (0x408 + ((n) * 0x10))
#define REG_DataHi(n)       (0x40C + ((n) * 0x10))
#define REG_AcceptMask(n)   (0x600 + ((n) * 0x4))
#define REG_TimeStampL(n)   (0x680 + ((n) * 0x4))
#define REG_TimeoutL(n)     (0x700 + ((n) * 0x4))
#define REG_TimeStampH(n)   (0x780 + ((n) * 0x4))
#define REG_TimeoutH(n)     (0x800 + ((n) * 0x4))

#define REG_NUM (0x880 / 4)

#define BUFFER_BASE(s)          ((void *) &s->regs[REG_IDX(REG_Id(0))])

#define CAN_TIMER_HZ        100
#define SLOWDOWN_CYCLES     5
#define BUF_MAX             31

#define REG_IDX(x) ((x) / 4)
#define REG(s, reg)             ((s)->regs[REG_IDX(reg)])

#define CAN_BUF_ACTIVE(s, p)        (REG(s, REG_CANME) & (1 << (p)))
#define CAN_BUF_TYPE_RECEIVE(s, p)  (REG(s, REG_CANMD) & (1 << (p)))
#define CAN_BUF_TYPE_TRANSMIT(s, p) (!(CAN_BUF_TYPE_RECEIVE(s, p)))
#define CAN_BUF_PROTECTED(s, p)     (REG(s, REG_CANOPC) & (1 << (p)))
#define CAN_BUF_FILLED(s, p)        (REG(s, REG_CANRMP) & (1 << (p)))
#define CAN_BUF_EVENT_ENABLED(s, p) (REG(s, REG_CANEE) & (1 << (p)))

#define EVENT_OK_TX         0x00
#define EVENT_STUFF_TX      0x01
#define EVENT_BIT_TX        0x02
#define EVENT_ACK_TX        0x03
#define EVENT_ARB_TX        0x04
#define EVENT_ABORT_TX      0x05
#define EVENT_TIMEOUT_TX    0x06
#define EVENT_OK_RX         0x07
#define EVENT_OK_RR         0x08
#define EVENT_OVERWRITE_RR  0x09
#define EVENT_STUFF_RX      0x0A
#define EVENT_FORM_RX       0x0B
#define EVENT_CRC_RX        0x0C
#define EVENT_TIMEOUT_RX    0x0D
#define EVENT_ERR96         0x0E
#define EVENT_ERR128        0x0F
#define EVENT_ERR255        0x10
#define EVENT_RECOVER       0x11

static const struct Sc64CanRegAccess {
    uint32_t read_mask;
    uint32_t write_mask;
    uint32_t w1c_mask;
} Sc64CanRegAccess[REG_NUM] = {
    [REG_IDX(REG_CANID)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANME)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANMD)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANTRS)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANTRR)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANTA)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANAA)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANRMP)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANRML)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANRFP)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANMC)] = {
        .read_mask  = 0x0001F8E0,
        .write_mask = 0x0001D8F0,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANBTC)] = {
        .read_mask  = 0x00FF03FF,
        .write_mask = 0x00FF03FF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANES)] = {
        .read_mask  = 0x01BFFF1B,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x01B8FF00,
    },
    [REG_IDX(REG_CANTEC)] = {
        .read_mask  = 0x000000FF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANREC)] = {
        .read_mask  = 0x000000FF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANGIF0)] = {
        .read_mask  = 0x0003EF1F,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00032700,
    },
    [REG_IDX(REG_CANGIM)] = {
        .read_mask  = 0x00036F07,
        .write_mask = 0x00036F07,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANGIF1)] = {
        .read_mask  = 0x0003EF1F,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00032700,
    },
    [REG_IDX(REG_CANMIM)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANMIL)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANOPC)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANTIOC)] = {
        .read_mask  = 0x00000008,
        .write_mask = 0x00000008,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANRIOC)] = {
        .read_mask  = 0x00000008,
        .write_mask = 0x00000008,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANLNT)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANTOC)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANTOS)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANTRCT)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANSS)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANGTH)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANGTL)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANCE)] = {
        .read_mask  = 0xFFFEFFFF,
        .write_mask = 0x0001FF0F,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANEE)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x0003FFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANEH)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANEM)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANEL)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANLRBC)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANEWC)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANEPC)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANBOC)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANTES)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANRES)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_CANSRME)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_CANSRMS)] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0x00000000,
        .w1c_mask   = 0xFFFFFFFF,
    },
    [REG_IDX(REG_Id(0)) ... REG_IDX(REG_Id(31))] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_Ctrl(0)) ... REG_IDX(REG_Ctrl(31))] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_DataLo(0)) ... REG_IDX(REG_DataLo(31))] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_DataHi(0)) ... REG_IDX(REG_DataHi(31))] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_AcceptMask(0)) ... REG_IDX(REG_AcceptMask(31))] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_TimeStampL(0)) ... REG_IDX(REG_TimeStampL(31))] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_TimeoutL(0)) ... REG_IDX(REG_TimeoutL(31))] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_TimeStampH(0)) ... REG_IDX(REG_TimeStampH(31))] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
    [REG_IDX(REG_TimeoutH(0)) ... REG_IDX(REG_TimeoutH(31))] = {
        .read_mask  = 0xFFFFFFFF,
        .write_mask = 0xFFFFFFFF,
        .w1c_mask   = 0x00000000,
    },
};

typedef struct Sc64CanState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;

    const char *id;
    uint32_t regs[REG_NUM];

    qemu_irq irq;

    CanBusClientState bus_client[SC64_CAN_PORT_NUM];

    uint32_t tx_abort;
    int active_tx;

    /* CAN controller can be connected/disconnected to/from bus
     * during the runtime, so we need to figure out which bus
     * is active at the moment. This value changes when SCCR register
     * at offset 0x420 is written.
     *
     * i-th bit represents connection between controller and bus #i.
     * If it equals 1, then controller is connected to this bus.
     * i from 0 to 3 */
    uint32_t canbus_mask;

    int error_state;
    void *timer_read;
    void *timer_opaque;

    qemu_can_frame next_frame;
    int next_num;
} Sc64CanState;

#define ERROR_NO_TIMER_READ (1 << 0)

typedef struct Sc64CanBuffer {
    unsigned int id:29;
    unsigned int aam:1;
    unsigned int ame:1;
    unsigned int ide:1;

    unsigned int dlc:4;
    unsigned int rtr:1;
    unsigned int pad1:3;
    unsigned int tpl:6;
    unsigned int pad2:18;

    uint32_t datalo;
    uint32_t datahi;
} Sc64CanBuffer;

#define CAN_ID_AME      (1 << 30)
#define CAN_ID_IDE      (1 << 31)

#define CAN_ID_MASK     0x1FFFFFFF

#define CAN_ID_STD_OFFT 18

static ssize_t sc64_can_receive(CanBusClientState *client,
                        const qemu_can_frame *frames, size_t frames_cnt);

void sc64_can_mask_set(DeviceState *dev, int val)
{
    Sc64CanState *s = SC64_CAN(dev);

    assert(s);

    s->canbus_mask = val;
}

static void sc64_can_reset(Sc64CanState *s)
{
    memset(s->regs, 0, REG_NUM * 4);

    REG(s, REG_CANID) = 0x00000C01;
    REG(s, REG_CANMC) = 0x00003000;
    REG(s, REG_CANES) = 0x00000010;
    REG(s, REG_CANCE) = 0x00000200;

    s->active_tx = -1;
}

static void sc64_can_timestamp_update(Sc64CanState *s)
{
    uint64_t t;
    uint64_t (*hnd)(void *opaque) = s->timer_read;

    if (hnd == NULL) {
        if (s->error_state & ERROR_NO_TIMER_READ) {
            error_report("sc64-can: No timer read handler provided!");
        } else {
            s->error_state |= ERROR_NO_TIMER_READ;
        }

        return;
    }

    t = hnd(s->timer_opaque);

    REG(s, REG_CANGTH) = (uint32_t) (t >> 32);
    REG(s, REG_CANGTL) = (uint32_t) (t & 0xFFFFFFFF);
}

static uint32_t sc64_can_timestamp_high(Sc64CanState *s)
{
    if (REG(s, REG_CANMC) & CANMC_ETM) {
        sc64_can_timestamp_update(s);
        return REG(s, REG_CANGTH);
    } else {
        return 0;
    }
}

static uint32_t sc64_can_timestamp_low(Sc64CanState *s)
{
    if (REG(s, REG_CANMC) & CANMC_ETM) {
        sc64_can_timestamp_update(s);
        return REG(s, REG_CANGTH);
    } else {
        return REG(s, REG_CANLNT);
    }
}

static void sc64_can_irq_update(Sc64CanState *s)
{
    int level = 0;

    if (REG(s, REG_CANTA) & REG(s, REG_CANMIM)) {
        /* TX interrupt */
        level = 1;
    }

    if (REG(s, REG_CANRMP) & REG(s, REG_CANMIM)) {
        /* RX interrupt */
        level = 1;
    }

    if (level == 0) {
        /* No RX, no TX irqs */
        REG(s, REG_CANGIF0) &= ~CANGIF0_GMIF;
    }

    if (REG(s, REG_CANGIF0) & CANGIF0_WDIF) {
        /* Write-denied interrupt */
        if (REG(s, REG_CANGIM) & CANGIM_WDIM) {
            level = 1;
        }
    }

    if (REG(s, REG_CANGIF0) & CANGIF0_MAIF) {
        /* Timeout interrupt */
        if (REG(s, REG_CANGIM) & CANGIM_MAIM) {
            level = 1;
        }
    }

    if (REG(s, REG_CANAA)) {
        /* Abort aknowledge interrupt */
        if (REG(s, REG_CANGIM) & CANGIM_AAIM) {
            level = 1;
        }
        REG(s, REG_CANGIF0) |= CANGIF0_AAIF;
    } else {
        REG(s, REG_CANGIF0) &= ~CANGIF0_AAIF;
    }

    if (REG(s, REG_CANRML)) {
        /* Buffer overwrite */
        if (REG(s, REG_CANGIM) & CANGIM_RMLIM) {
            level = 1;
        }
        REG(s, REG_CANGIF0) |= CANGIF0_RMLIF;
    } else {
        REG(s, REG_CANGIF0) &= ~CANGIF0_RMLIF;
    }

    qemu_set_irq(s->irq, level);
}

static int sc64_get_prio_buf(Sc64CanState *s)
{
    int prio = -1;
    int num = -1;
    int i;

    Sc64CanBuffer *buf = BUFFER_BASE(s);

    if (REG(s, REG_CANMC) & CANMC_LOM) {
        /* Listen-only mode */
        return -1;
    }

    for (i = 0; i <= BUF_MAX; i++) {
        if (!CAN_BUF_ACTIVE(s, i)) {
            continue;
        }

        if ((!CAN_BUF_TYPE_TRANSMIT(s, i)) &&
                (!(REG(s, REG_Ctrl(i)) & Ctrl_RTR))) {
            continue;
        }

        if (!(REG(s, REG_CANTRS) & (1 << i))) {
            continue;
        }

        if (s->tx_abort & (1 << i)) {
            continue;
        }

        if (buf[i].tpl >= prio) {
            num = i;
            prio = buf[i].tpl;
        }
    }

    return num;
}

static void sc64_can_fill_frame(Sc64CanBuffer *buf, qemu_can_frame *frame)
{
    assert(buf != NULL);
    assert(frame != NULL);

    memset(frame, 0, sizeof(qemu_can_frame));

    frame->can_id = buf->id;
    if (buf->ide) {
        frame->can_id |= QEMU_CAN_EFF_FLAG;
    }

    if (buf->rtr) {
        frame->can_id |= QEMU_CAN_RTR_FLAG;
    }

    frame->can_dlc = buf->dlc;
    frame->data[0] = buf->datalo >> 24;
    frame->data[1] = (buf->datalo >> 16) & 0xFF;
    frame->data[2] = (buf->datalo >> 8) & 0xFF;
    frame->data[3] = buf->datalo & 0xFF;
    frame->data[4] = buf->datahi >> 24;
    frame->data[5] = (buf->datahi >> 16) & 0xFF;
    frame->data[6] = (buf->datahi >> 8) & 0xFF;
    frame->data[7] = buf->datahi & 0xFF;
}

static void sc64_can_buffer_transmit(Sc64CanState *s, int p)
{
    qemu_can_frame frame;
    Sc64CanBuffer *buf = BUFFER_BASE(s);
    int res = 0;
    int i;

    buf += p;

    sc64_can_fill_frame(buf, &frame);

    if (REG(s, REG_CANMC) & CANMC_STM) {
        /* Loopback */
        res = sc64_can_receive(&s->bus_client[0], &frame, 1);
    } else {
        for (i = 0; i < SC64_CAN_PORT_NUM; i++) {
            if (s->canbus_mask & (1 << i)) {
                res += can_bus_client_send(&s->bus_client[i], &frame, 1);
            }
        }
    }
}

static int sc64_can_buf_num(hwaddr addr)
{
    assert(addr >= REG_Id(0));
    return (addr - REG_Id(0)) / 0x10;
}

static void sc64_can_write(void *opaque, hwaddr addr, uint64_t val,
        unsigned size)
{
    Sc64CanState *s = opaque;
    int saddr = REG_IDX(addr);
    uint32_t w1c_mask, write_mask;

    if (saddr > REG_NUM) {
        printf("CAN: write memory out of range!\n");
    }

    dbg("%p write %08x->%02x\n", s, val, addr);
    w1c_mask = Sc64CanRegAccess[saddr].w1c_mask;
    write_mask = Sc64CanRegAccess[saddr].write_mask;

    s->regs[saddr] &= ~(val & w1c_mask);

    s->regs[saddr] &= ~write_mask;
    s->regs[saddr] |= val & write_mask;

    if ((addr >= REG_Id(0)) && (addr <= REG_Id(BUF_MAX)) &&
            ((addr & 0xF) == 0) && CAN_BUF_ACTIVE(s, sc64_can_buf_num(addr))) {
        /* Can't change active buffer ID register */
        REG(s, REG_CANGIF0) |= CANGIF0_WDIF;
        sc64_can_irq_update(s);
        return;
    }

    switch (addr) {
    case REG_CANME:
    case REG_CANMD:
    case REG_CANTRS:
        /* Transmit request will be picked up by can_core */
        s->tx_abort &= ~val;
        break;
    case REG_CANTRR:
        dbg("abort requset %08x, active_tx is %d\n", val, s->active_tx);
        if (s->active_tx != -1) {
            val &= ~(1 << s->active_tx);
        }
        REG(s, REG_CANAA) |= val;
        s->tx_abort |= val;
        break;
    case REG_CANTA:
    case REG_CANAA:
    case REG_CANRMP:
        REG(s, REG_CANRML) &= ~val;
        break;
    case REG_CANMC:
        if (val & CANMC_SRES) {
            sc64_can_reset(s);
            break;
        }

        if (val & CANMC_CCR) {
            REG(s, REG_CANES) |= CANES_CCE;
        } else {
            REG(s, REG_CANES) &= ~CANES_CCE;
        }

        if (val & CANMC_PDR) {
            REG(s, REG_CANES) |= CANES_PDA;
        } else {
            REG(s, REG_CANES) &= ~CANES_PDA;
        }

        break;
    case REG_CANCE:
        if (val & CANCE_CLR) {
            REG(s, REG_CANCE) |= CANCE_Empty;
            REG(s, REG_CANCE) &= ~(CANCE_Full | CANCE_AlmFull | CANCE_CLR);
            REG(s, REG_CANEH) = REG(s, REG_CANEM) = REG(s, REG_CANEL) = 0;
        }
        break;
    default:
        /* Do nothing */
        break;
    }

    sc64_can_irq_update(s);
}

static uint64_t sc64_can_read(void *opaque, hwaddr addr, unsigned size)
{
    Sc64CanState *s = opaque;
    int saddr = REG_IDX(addr);

    if (saddr > REG_NUM) {
        printf("CAN: read memory out of range!\n");
    }

    if ((addr == REG_CANGTH) || (addr == REG_CANGTL) || (addr == REG_CANLNT)) {
        sc64_can_timestamp_update(s);
    }

    dbg("%p read %08x<-%02x\n", s,
            s->regs[saddr] & Sc64CanRegAccess[saddr].read_mask, addr);
    return s->regs[saddr] & Sc64CanRegAccess[saddr].read_mask;
}

static const MemoryRegionOps sc64_can_mem_ops = {
    .read = sc64_can_read,
    .write = sc64_can_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int sc64_can_can_receive(CanBusClientState *client)
{
    Sc64CanState *s = client->priv;
    int i;

    if (!REG(s, REG_CANRIOC)) {
        /* Receiver not enabled */
        return 0;
    }

    for (i = 0; i < SC64_CAN_PORT_NUM; i++) {
        if (client == &s->bus_client[i]) {
            if (s->canbus_mask & (1 << i)) {
                return 1;
            }
        }
    }

    return 0;
}

static int sc64_can_try_autoanswer(Sc64CanState *s,
        const qemu_can_frame *frame)
{
    int i, tmp;
    Sc64CanBuffer *buf = BUFFER_BASE(s);

    for (i = BUF_MAX; i >= 0; i--) {
        if (!CAN_BUF_ACTIVE(s, i)) {
            continue;
        }

        if (buf[i].id == (frame->can_id & QEMU_CAN_EFF_MASK)) {
            tmp = i;
        } else {
            continue;
        }

        if (buf[i].aam) {
            REG(s, REG_CANRFP) |= 1 << i;
            REG(s, REG_CANTRS) |= 1 << i;

            sc64_can_buffer_transmit(s, i);
            return 1;
        }
    }

    REG(s, REG_CANRMP) |= 1 << tmp;

    return 0;
}

static int sc64_can_try_receive(Sc64CanState *s, const qemu_can_frame *frame)
{
    int i, overwrite_prot = 0;
    Sc64CanBuffer *buf = BUFFER_BASE(s);

    if (REG(s, REG_CANMC) & CANMC_LOM) {
        /* Listen-only mode */
        return 1;
    }

    for (i = BUF_MAX; i >= 0; i--) {
        int id, frame_id;

        if (!CAN_BUF_ACTIVE(s, i)) {
            continue;
        }

        if (!CAN_BUF_TYPE_RECEIVE(s, i)) {
            continue;
        }

        id = buf[i].id & CAN_ID_MASK;
        frame_id = frame->can_id & QEMU_CAN_EFF_MASK;
        /* Checking accept mask */
        if (buf[i].ame) {
            if ((~REG(s, REG_AcceptMask(i)) &
                    (buf[i].id ^ frame->can_id) & CAN_ID_MASK)) {
                continue;
            }
        } else {
            if (buf[i].id & CAN_ID_IDE) {
                id >>= CAN_ID_STD_OFFT;
                frame_id &= QEMU_CAN_SFF_MASK;
            }

            if (id != frame_id) {
                continue;
            }
        }

        if (CAN_BUF_FILLED(s, i)) {
            if (CAN_BUF_PROTECTED(s, i)) {
                overwrite_prot = 1;
                continue;
            } else {
                /* Buffer overwrite */
                REG(s, REG_CANRML) |= 1 << i;
            }
        }

        REG(s, REG_CANRMP) |= (1 << i);

        buf[i].id = frame->can_id & QEMU_CAN_EFF_MASK;
        buf[i].dlc = frame->can_dlc;

        buf[i].datahi = (frame->data[4] << 24) |
                        (frame->data[5] << 16) |
                        (frame->data[6] << 8) |
                        frame->data[7];

        buf[i].datalo = (frame->data[0] << 24) |
                        (frame->data[1] << 16) |
                        (frame->data[2] << 8) |
                        frame->data[3];

        REG(s, REG_TimeStampL(i)) = sc64_can_timestamp_low(s);
        REG(s, REG_TimeStampH(i)) = sc64_can_timestamp_high(s);

        if (REG(s, REG_CANMIM) & (1 << i)) {
            /* Interrupt is enabled */
            REG(s, REG_CANGIF0) |= CANGIF0_GMIF;
            REG(s, REG_CANGIF0) &= ~CANGIF0_MIV_MASK;
            REG(s, REG_CANGIF0) |= i;

            qemu_irq_raise(s->irq);
        }

        sc64_can_irq_update(s); /* In case of buffer overwrite */
        return 1;
    }

    if (overwrite_prot) {
        /* Increase error counter */
        REG(s, REG_CANLRBC)++;
    }

    return 0;
}

static ssize_t sc64_can_receive(CanBusClientState *client,
                        const qemu_can_frame *frames, size_t frames_cnt)
{
    Sc64CanState *s = client->priv;

    if (!REG(s, REG_CANRIOC)) {
        /* Receiver not enabled */
        dbg("%p receiver not enabled\n", s);
        return 0;
    }

    if (frames[0].can_id & QEMU_CAN_RTR_FLAG) {
        sc64_can_try_autoanswer(s, &frames[0]);
    } else {
        sc64_can_try_receive(s, &frames[0]);
    }

    return frames_cnt;
}

static void sc64_can_update_state(CanBusClientState *client, int state)
{
    Sc64CanState *s = client->priv;
    int p;
    REG(s, REG_CANLNT) += 10;
    dbg("sc64-can %p update; state %02x\n", s, state);

    switch (state) {
    case CAN_CLIENT_STALL:
        s->active_tx = -1;
        REG(s, REG_CANES) &= ~(CANES_RM | CANES_TM);
        break;
    case CAN_PREPARE_TX:
        s->active_tx = s->next_num;
        REG(s, REG_CANES) |= CANES_TM;
        REG(s, REG_CANES) &= ~CANES_RM;
        break;
    case CAN_PREPARE_RX:
        s->active_tx = -1;
        REG(s, REG_CANES) |= CANES_RM;
        REG(s, REG_CANES) &= ~CANES_TM;
        break;
    case CAN_PREPARE_TX | CAN_PREPARE_RX:
        s->active_tx = s->next_num;
        REG(s, REG_CANES) |= CANES_TM | CANES_RM;
        break;
    case CAN_SUCC_TX:
        if (s->active_tx == -1) {
            break;
        }

        s->active_tx = -1;
        p = s->next_num;
        REG(s, REG_CANTA) |= 1 << p;
        REG(s, REG_CANTRS) &= ~(1 << p);

        REG(s, REG_TimeStampL(p)) = REG(s, REG_CANLNT);
        dbg("succ tx #%d\n", p);
        if (REG(s, REG_CANMIM) & (1 << p)) {
            /* Interrupt is enabled */
            REG(s, REG_CANGIF0) |= CANGIF0_GMIF;
            REG(s, REG_CANGIF0) &= ~CANGIF0_MIV_MASK;
            REG(s, REG_CANGIF0) |= p;

            qemu_irq_raise(s->irq);
        }
        break;
    }
}

static qemu_can_frame *sc64_can_next_frame(CanBusClientState *client)
{
    Sc64CanState *s = client->priv;
    Sc64CanBuffer *buf = BUFFER_BASE(s);
    int top = sc64_get_prio_buf(s);
    qemu_can_frame *frame;

    if (top == -1) {
        dbg("next frame NULL\n");
        return NULL;
    }

    dbg("sc64-can %p next_frame=%d\n", s, top);

    if (s->active_tx != -1) {
        top = s->active_tx;
    }

    s->next_num = top;
    frame = &s->next_frame;

    buf += top;

    sc64_can_fill_frame(buf, frame);

    return frame;
}

static int sc64_can_loopback(CanBusClientState *client)
{
    Sc64CanState *s = client->priv;

    return !!(REG(s, REG_CANMC) & CANMC_STM);
}

static int sc64_can_is_active(CanBusClientState *client)
{
    Sc64CanState *s = client->priv;
    int i;

    for (i = 0; i < SC64_CAN_PORT_NUM; i++) {
        if (client == &s->bus_client[i]) {
            if (s->canbus_mask & (1 << i)) {
                return 1;
            }
        }
    }

    return 0;
}

static CanBusClientInfo sc64_can_bus_client_info = {
    .can_receive = sc64_can_can_receive,
    .receive = sc64_can_receive,
    .update_state = sc64_can_update_state,
    .next_frame = sc64_can_next_frame,
    .loopback = sc64_can_loopback,
    .is_active = sc64_can_is_active,
};

static int sc64_can_connect_to_bus(Sc64CanState *s, CanBusState *bus, int n)
{
    s->bus_client[n].info = &sc64_can_bus_client_info;
    s->bus_client[n].priv = s;

    if (can_bus_insert_client(bus, &s->bus_client[n]) < 0) {
        error_report("sc64-can: Failed to connect CAN device #%d", n);
        return -1;
    }

    return 0;
}

void sc64_can_register(hwaddr addr, qemu_irq irq, const char *id,
                       uint64_t (*timer_read)(void *opaque), void *timer_opaque)
{
    DeviceState *dev;
    SysBusDevice *s;

    dev = qdev_create(NULL, TYPE_SC64_CAN);
    dev->id = id;

    qdev_prop_set_ptr(dev, "timer_read", timer_read);
    qdev_prop_set_ptr(dev, "timer_opaque", timer_opaque);

    qdev_init_nofail(dev);

    s = SYS_BUS_DEVICE(dev);
    if (addr != (hwaddr)-1) {
        sysbus_mmio_map(s, 0, addr);
    }

    sysbus_connect_irq(s, 0, irq);
}

static int sc64_can_init(SysBusDevice *dev)
{
    Sc64CanState *s = SC64_CAN(dev);
    CanBusState *can_bus;
    int i;

    sysbus_init_irq(dev, &s->irq);

    memory_region_init_io(&s->iomem, OBJECT(s), &sc64_can_mem_ops,
                            s, "sc64-can", 4 * REG_NUM);
    sysbus_init_mmio(dev, &s->iomem);

    sc64_can_reset(s);
    s->canbus_mask = 0;

    for (i = 0; i < SC64_CAN_PORT_NUM; i++) {
        can_bus = can_bus_find_by_name(host_canbus[i] ?
                host_canbus[i] : "default QEMU CAN bus", true);
        if (can_bus == NULL) {
            error_report("Can't find/allocate CAN bus");
            exit(1);
        }

        if (host_canbus[i]) {
            if (can_bus_connect_to_host_device(can_bus, host_canbus[i]) < 0) {
                error_report("Can't connect to host device");
                exit(1);
            }
        }

        dbg("CONNECT CAN PORT %d TO BUS %p\n", i, can_bus);

        sc64_can_connect_to_bus(s, can_bus, i);
    }

    return 0;
}

static const VMStateDescription vmstate_sc64_can_regs = {
    .name = "sc64-can",
    .version_id = 1,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, Sc64CanState, REG_NUM),
        VMSTATE_END_OF_LIST(),
    },
};

static Property sc64_can_sysbus_properties[] = {
    DEFINE_PROP_PTR("timer_read", Sc64CanState, timer_read),
    DEFINE_PROP_PTR("timer_opaque", Sc64CanState, timer_opaque),
    DEFINE_PROP_END_OF_LIST(),
};

static void sc64_can_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = sc64_can_init;
    dc->desc = "SC64 CAN block";
    dc->vmsd = &vmstate_sc64_can_regs;
    dc->props = sc64_can_sysbus_properties;
}

static const TypeInfo sc64_can_sysbus_info = {
    .name          = TYPE_SC64_CAN,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Sc64CanState),
    .class_init    = sc64_can_sysbus_class_init,
};

static void sc64_can_register_types(void)
{
    type_register_static(&sc64_can_sysbus_info);
}

type_init(sc64_can_register_types)
