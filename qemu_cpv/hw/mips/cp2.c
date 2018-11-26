/*
 * Copyright (C) 2018 Alexander Kalmuk <alexkalmuk@gmail.com>
 *
 * This is mostly based on VMIPS (k128_dma_ctrl.cpp and k128_cp2_les.h)
 * and was adopted for QEMU.
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
#include "hw/hw.h"
#include "hw/mips/sccr-sysbus.h"
#include "hw/sysbus.h"
#include "sysemu/dma.h"
#include "qemu/typedefs.h"
#include "qemu/main-loop.h"
#include "qemu/log.h"
#include "hw/mips/cp2.h"

#include "k128cp2.h"

#define TYPE_CP2_DMA "cp2-dma"
#define CP2_DMA(obj) OBJECT_CHECK(CP2DmaState, (obj), TYPE_CP2_DMA)

#define MSG_LOG_WRITE(str, ...) \
    do { \
        if (unlikely(qemu_loglevel_mask(CPU_LOG_CP2))) { \
            char str_buf[1024]; \
            sprintf(str_buf, "cp2-dma: " str , ## __VA_ARGS__); \
            libk128cp2_print(s->cp2->cp2ptr, str_buf); \
        } \
    } while (0)

#define DSCR_LOG_WRITE(a, d1, d2) \
    MSG_LOG_WRITE("  DDR   (%8.8x) -> DMA"  \
         "  : 0x%16.16" PRIx64 ", 0x%16.16" PRIx64, a, d1, d2)

#define CP2PUT_LOG_WRITE(n, a1, a2, d1, b1, d2, b2) \
    MSG_LOG_WRITE("  CP2_%d (%8.8x) -> DDR   (%8.8x) : 0x%16.16" \
        PRIx64 " (0x%2.2x), 0x%16.16" PRIx64 " (0x%2.2x)", \
        n, a1, a2, d1, b1, d2, b2)

#define CP2GET_LOG_WRITE(n, a1, a2, d1, b1, d2, b2) \
    MSG_LOG_WRITE("  DDR   (%8.8x) -> CP2_%d (%8.8x) : 0x%16.16" \
        PRIx64 " (0x%2.2x), 0x%16.16" PRIx64 " (0x%2.2x)", \
        a1, n, a2, d1, b1, d2, b2)

#define CP2GET64_LOG_WRITE(n, a1, a2, d1, b1) \
    MSG_LOG_WRITE("  DDR   (%8.8x) -> CP2_%d (%8.8x) : 0x%16.16" \
        PRIx64 " (0x%2.2x)", a1, n, a2, d1, b1)

/* CHANNEL for CP2 */
#define DMA_CH_CP2_CTRL_OFFSET   0x00
#define DMA_CH_CP2_STATUS_OFFSET 0x04
#define DMA_CH_CP2_DADDR_OFFSET  0x08
#define DMA_CH_CP2_INT_U_OFFSET  0x0C
#define DMA_CH_CP2_CURDSC_OFFSET 0x10

#define DMA_CREG_ADDRLIM_OFFSET  0x00
#define DMA_CREG_INTMASK_OFFSET  0x04
#define DMA_CREG_BASEADDR_OFFSET 0x08
#define DMA_CREG_INTCAUSE_OFFSET 0x0C
#define DMA_CREG_CONFIG_OFFSET   0x10

#define CP2_DMA_SIZE             0x10000

#define DESCR_PROCESSING_TIMEOUT 10000

enum dma_chstate {
    DMA_IDLE,
    DMA_NODSCR,
    DMA_NOBUF,
    DMA_PRESYNC,
    DMA_WORKING,
    DMA_POSTSYNC,
    DMA_FINISHING
};

enum dma_dtype {
    DMA_D_PUT = 0,
    DMA_D_GET = 2,
    DMA_D_JUMP = 3
};

typedef struct CP2DmaState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem[2];

    CP2State *cp2;

    enum dma_chstate state;

    /* System and User registers */
    uint32_t r_daddr, r_curdescr, r_intcause, r_ctrl, r_status;
    uint32_t rc_addrlim, rc_baseaddr, rc_intmask, rc_config;

    /* descriptor variables */
    uint64_t d_descr[8];
    uint32_t d_addr;
    enum dma_dtype d_type;
    uint8_t d_cnt, d_jump_type, d_jump_reg;
    int16_t d_jump_offset;
    uint8_t d_put_flow, put_n, put_cnt;
    bool d_int, d_stop, d_sync_pre, d_sync_post, d_w64;

    /* memory variables */
    uint32_t m_ba;
    uint32_t m_bx, m_sx, m_nx, m_sy, m_ny;
    uint32_t m_cnt_b, m_cnt_x, m_cnt_y;
    int8_t m_breg;

    /* cp2 variables */
    uint16_t cp2_ba;
    uint16_t cp2_sx, cp2_nx, cp2_sy, cp2_ny, cp2_cnt_x, cp2_cnt_y;
    bool cp2_revx, cp2_revy, cp2_lo64;
    int8_t cp2_breg;
    uint16_t cp2_mask, cp2_mask_end;
    uint16_t cp2_dcnt_main, cp2_dcnt[4], cp2_dcnt_x[4],
        cp2_dcnt_base[4], cp2_dcnt_sx[4], cp2_dcnt_nx[4], cp2_dcnt_sy[4];
    uint16_t cp2_put_addr[4];

    AddressSpace *as;
} CP2DmaState;

static uint64_t cp2_dread(CP2State *cp2, uint8_t n, uint16_t adr);
static void cp2_dwrite(CP2State *cp2, uint8_t n, uint16_t adr, uint64_t data);
static uint64_t cp2_ireg_read(CP2State *cp2, uint8_t i);
static void cp2_reset(CP2State *cp2);

static uint64_t cp2_dma_mask64(uint8_t be)
{
    uint64_t mask64 = 0;
    int i;
    for (i = 0; be != 0; i++, be >>= 1) {
        if (be & 0x1) {
            mask64 |= (0xFFULL << i * 8);
        }
    }
    return mask64;
}

static bool cp2_dma_check_limit(CP2DmaState *s, uint32_t addr)
{
    if (!(s->rc_config & 0x1)) {
        return 1;
    }

    if (addr >= s->rc_addrlim) {
        MSG_LOG_WRITE("Address limit exceeded. (0x%8.8x)",
                        s->rc_addrlim);
        s->r_intcause |= 0x1;

        s->state = DMA_IDLE;
        s->r_status &= ~0x1;
        MSG_LOG_WRITE("Channel stopped.");
        return 0;
    }
    return 1;
}

static uint32_t swap_word(uint32_t w)
{
    return ((w & 0x0ff) << 24) | (((w >> 8) & 0x0ff) << 16) |
        (((w >> 16) & 0x0ff) << 8) | ((w >> 24) & 0x0ff);
}

static uint64_t swap_dword(uint64_t d)
{
    uint32_t wl, wr;
    wr = d;
    wr = swap_word(wr);
    wl = d >> 32;
    wl = swap_word(wl);

    return (((uint64_t)wr) << 32) | wl;
}

static uint64_t cp2_dma_read64(CP2DmaState *s, uint64_t offset)
{
    uint64_t res;
    dma_memory_read(s->as, offset, &res, 8);
    return swap_dword(res);
}

static void cp2_dma_write64(CP2DmaState *s, uint64_t offset,
    uint64_t data, uint64_t mask)
{
    uint64_t val;

    data = swap_dword(data);
    mask = swap_dword(mask);

    dma_memory_read(s->as, offset, &val, 8);
    val = (val & ~mask) | (data & mask);
    dma_memory_write(s->as, offset, &val, 8);
}

static uint16_t cp2_dma_bitrev(CP2DmaState *s, uint16_t addr)
{
    uint16_t res = 0;
    int i;
    for (i = 0; i < 13; i++) {
        if (addr & (0x1 << i)) {
            res |= 0x1 << (13 - i);
        }
    }
    return res;
}

static void cp2_dma_state_idle(CP2DmaState *s)
{
    s->state = DMA_NODSCR;
    s->r_ctrl &= 0xFFFFFFFE;
    s->r_status |= 0x1;
    s->d_cnt = 0;
    s->d_addr = s->rc_baseaddr + s->r_daddr + s->r_curdescr * 0x40;
}

static int cp2_dma_state_nodescr(CP2DmaState *s)
{
    int i;

    if (s->d_cnt == 0) {
        MSG_LOG_WRITE("Retrieving descriptor #%d.", s->r_curdescr);
    }

    while (s->d_cnt < 8) {
        if (cp2_dma_check_limit(s, s->d_addr)) {
            s->d_descr[s->d_cnt]     = cp2_dma_read64(s, s->d_addr);
            s->d_descr[s->d_cnt + 1] = cp2_dma_read64(s, s->d_addr + 8);
            DSCR_LOG_WRITE(s->d_addr, s->d_descr[s->d_cnt],
                            s->d_descr[s->d_cnt + 1]);
        } else {
            s->d_descr[s->d_cnt] = s->d_descr[s->d_cnt + 1] = 0;
        }

        s->d_cnt += 2;
        s->d_addr += 0x10;
    }

    /* decode descriptor */
    if (!((s->d_descr[0] >> 63) & 0x1)) { /* invalid */
        MSG_LOG_WRITE("Invalid descriptor.");
        s->r_status &= ~0x1;
        s->state = DMA_IDLE;
        return -1;
    } else {
        MSG_LOG_WRITE("Valid descriptor.");
    }

    s->d_int       = (s->d_descr[0] >> 62) & 0x1;
    s->d_stop      = (s->d_descr[0] >> 61) & 0x1;
    s->d_sync_pre  = (s->d_descr[0] >> 60) & 0x1;
    s->d_sync_post = (s->d_descr[0] >> 59) & 0x1;
    s->d_type      = (enum dma_dtype)((s->d_descr[0] >> 57) & 0x3);
    s->d_jump_type = (s->d_descr[0] >> 55) & ((s->d_type == DMA_D_JUMP)
                        ? 0x3 : 0x2);
    s->d_jump_reg  = (s->d_descr[0] >> ((s->d_type == DMA_D_JUMP) ? 49 : 47))
                        & 0x1F;
    s->d_w64 = s->cp2_lo64 = 0;

    MSG_LOG_WRITE("  Descriptor type - %s.",
                    (s->d_type == DMA_D_JUMP) ? "JUMP" :
                    (s->d_type == DMA_D_GET) ? "GET" : "PUT");

    switch (s->d_type) {
    case DMA_D_GET:
        s->d_w64 = (s->d_descr[0] >> 32) & 0x1;
        for (i = 0; i < 4; i++) {
            s->cp2_dcnt[i] = s->cp2_dcnt_base[i] = s->d_descr[4 + i] >> 48;
            s->cp2_dcnt_sx[i]   = s->d_descr[4 + i] >> 32;
            s->cp2_dcnt_nx[i]   = s->d_descr[4 + i] >> 16;
            s->cp2_dcnt_sy[i]   = s->d_descr[4 + i];
            s->cp2_dcnt_x[i] = 0;
        }
        s->put_n = s->put_cnt = s->d_put_flow = 0;
        /* no break here (common part) */
        /* Fall through */
    case DMA_D_PUT:
        s->d_jump_offset = (s->d_descr[0] >> 47) & 0x1FF;
        if (s->d_jump_offset & 0x0100) {
            s->d_jump_offset |= 0xFF00;
        }  /* 9-bit signed */

        s->m_ba = s->d_descr[0] & 0xFFFFFFFC;
        s->m_bx = (s->d_descr[0] >> 41) & 0x3F;
        s->m_sx = (s->d_descr[1] >> 40) & 0x00FFFFFC;
        s->m_sx = (int32_t)(s->m_sx << 8) >> 8;  /* sign extend */
        s->m_breg = ((s->d_descr[1] >> 39) & 0x1)
                        ? -1 : ((s->d_descr[1] >> 34) & 0x1F);
        s->m_nx = (s->d_descr[1] >> 12) & 0xFFFFF;
        s->m_sy = (((s->d_descr[1] & 0xFFF) << 12) |
                        ((s->d_descr[2] >> 32) & 0xFFC));
        s->m_sy = (int32_t)(s->m_sy << 8) >> 8;  /* sign extend */
        s->m_ny = (s->d_descr[2] >> 44) & 0xFFFFF;
        s->m_cnt_b = s->m_cnt_x = s->m_cnt_y = 0;

        s->cp2_ba = s->d_descr[2] >> 16;
        s->cp2_sx = s->d_descr[2];
        s->cp2_nx = s->d_descr[3] >> 48;
        s->cp2_sy = s->d_descr[3] >> 32;
        s->cp2_ny = s->d_descr[3] >> 16;
        s->cp2_revx = (s->d_descr[3] >> 15) & 0x1;
        s->cp2_revy = (s->d_descr[3] >> 14) & 0x1;
        s->cp2_breg = ((s->d_descr[3] >> 13) & 0x1)
                            ? -1 : ((s->d_descr[3] >> 8) & 0x1F);
        s->cp2_dcnt_main = s->cp2_cnt_x = s->cp2_cnt_y = 0;

        MSG_LOG_WRITE("  Memory: base=0x%8.8x + ireg[%d].",
                        s->m_ba, s->m_breg);
        MSG_LOG_WRITE("          bx=%2.2x sx=%4.4x, nx=%4.4x,"
                                "sy=%4.4x, ny=%4.4x.",
                                s->m_bx, s->m_sx, s->m_nx, s->m_sy, s->m_ny);
        MSG_LOG_WRITE("  CP2:    base=0x%4.4x + ireg[%d].",
                                s->cp2_ba, s->cp2_breg);
        MSG_LOG_WRITE("          sx=%4.4x, nx=%4.4x, sy=%4.4x,"
                    "ny=%4.4x%s%s.",
                    s->cp2_sx, s->cp2_nx, s->cp2_sy, s->cp2_ny,
                    s->cp2_revx ? ", reverse x" : "",
                    s->cp2_revy ? ", reverse y" : "");

        if (s->d_type != DMA_D_PUT) {
            break;
        }

        s->d_put_flow = (s->d_descr[0] >> 32) & 0xFF;
        if (s->d_put_flow > 4) {
            s->d_put_flow = 4;
        }
        s->put_n = s->put_cnt = 0;
        s->cp2_mask = s->cp2_mask_end = 0xFFFF;
        for (i = 0; i < 4; i++) {
            s->cp2_mask     &= ((s->d_descr[4] >> (63 - i)) & 0x1)
                                    ? ~(0xF << i * 4) : 0xFFFF;
            s->cp2_mask_end &= ((s->d_descr[4] >> (59 - i)) & 0x1)
                                    ? ~(0xF << i * 4) : 0xFFFF;
        }

        MSG_LOG_WRITE("          put_flow=%x,"
                    " cp2_mask=%4.4x, cp2_mask_end=%4.4x.",
                    s->d_put_flow, s->cp2_mask, s->cp2_mask_end);
        break;
    default:
        break;
    }

    s->state = DMA_PRESYNC;
    if (s->d_sync_pre) {
        s->r_status |= 0x2;
        MSG_LOG_WRITE("Need presync.");
    }

    return 0;
}

static int cp2_dma_state_presync(CP2DmaState *s)
{
    if (s->d_sync_pre) {
        return -1;
    } /* waiting */
    s->r_status &= ~0x2;
    if (s->d_type != DMA_D_JUMP) {
        if (s->m_breg >= 0) {
            uint32_t t = cp2_ireg_read(s->cp2, s->m_breg) & 0xFFFFFFFC;
            s->m_ba += t;
            MSG_LOG_WRITE("  ireg[%d]=0x%8.8x, memory base=0x%8.8x.",
                            s->m_breg, t, s->m_ba);
        }
        if (s->cp2_breg >= 0) {
            uint16_t t = cp2_ireg_read(s->cp2, s->cp2_breg);
            s->cp2_ba += t;
            MSG_LOG_WRITE("  ireg[%d]=0x%4.4x, cp2 base=0x%4.4x.",
                            s->cp2_breg, t, s->cp2_ba);
        }
        if (!s->d_w64) {
            s->cp2_ba &= ~0x1;
            s->cp2_sx &= ~0x1;
            s->cp2_sy &= ~0x1;
        }
        MSG_LOG_WRITE("Starting data operations.");
    }
    s->state = DMA_WORKING;
    return 0;
}

static int cp2_dma_state_working(CP2DmaState *s)
{
    if (s->d_type != DMA_D_JUMP) {
        uint32_t memaddr = s->m_ba + s->m_cnt_b * 0x10
                            + s->m_cnt_x * s->m_sx + s->m_cnt_y * s->m_sy;

        if ((s->put_n == 0) || (s->d_put_flow == 0)) {
            s->cp2_put_addr[s->put_cnt] =
                (s->cp2_ba +
                (s->cp2_revx ? cp2_dma_bitrev(s, s->cp2_sx * s->cp2_cnt_x)
                    : s->cp2_sx * s->cp2_cnt_x) +
                (s->cp2_revy ? cp2_dma_bitrev(s, s->cp2_sy * s->cp2_cnt_y)
                    : s->cp2_sy * s->cp2_cnt_y))
                & 0x1FFF;
        }
        uint16_t cp2addr = s->cp2_put_addr[s->put_cnt];

        if (s->d_type == DMA_D_GET) {
            uint64_t tdata[2] = {(uint64_t)-1, (uint64_t)-1};
            bool check_ok = cp2_dma_check_limit(s, memaddr);
            int i;

            if (check_ok) {
                if (s->d_w64 && s->cp2_lo64) {
                    memaddr += 8;
                }

                tdata[0] = cp2_dma_read64(s, memaddr);
                tdata[1] = cp2_dma_read64(s, memaddr + 8);
            }

            for (i = 0; i < 4; i++) {
                if (s->cp2_dcnt[i] == s->cp2_dcnt_main) {
                    if (check_ok) {
                        cp2_dwrite(s->cp2, i, cp2addr, tdata[0]);
                        if (!s->d_w64) {
                            cp2_dwrite(s->cp2, i, cp2addr + 1, tdata[1]);
                        }

                        if (s->d_w64) {
                            CP2GET64_LOG_WRITE(i, memaddr, cp2addr,
                                tdata[0], 0xff);
                        } else {
                            CP2GET_LOG_WRITE(i, memaddr, cp2addr,
                                tdata[0], 0xff, tdata[1], 0xff);
                        }
                    }

                    s->cp2_dcnt_x[i]++;
                    if (s->cp2_dcnt_x[i] >= s->cp2_dcnt_nx[i]) {
                        s->cp2_dcnt_x[i] = 0;
                        s->cp2_dcnt_base[i] += s->cp2_dcnt_sy[i];
                    }
                    s->cp2_dcnt[i] = s->cp2_dcnt_base[i]
                                        + s->cp2_dcnt_x[i] * s->cp2_dcnt_sx[i];
                }
            }
            s->cp2_dcnt_main += s->d_w64 ? 1 : 2;
            if (s->d_w64) {
                s->cp2_lo64 = !s->cp2_lo64;
            }
        } else {  /* "put" */
            if (cp2_dma_check_limit(s, memaddr)) {
                uint16_t mask = (s->cp2_cnt_x + 1 == s->cp2_nx)
                                    ? s->cp2_mask_end : s->cp2_mask;
                uint64_t tdata[2];
                tdata[0] = cp2_dread(s->cp2, s->put_n, cp2addr);
                tdata[1] = cp2_dread(s->cp2, s->put_n, cp2addr + 1);

                cp2_dma_write64(s, memaddr,     tdata[0],
                                    cp2_dma_mask64(mask >> 8));
                cp2_dma_write64(s, memaddr + 8, tdata[1],
                                    cp2_dma_mask64(mask));

                CP2PUT_LOG_WRITE(s->put_n, cp2addr, memaddr, tdata[0],
                                    mask >> 8, tdata[1], mask & 0xff);
            }
        }

        /* update cp2 memory counters */
        if ((s->d_type == DMA_D_PUT) && (s->d_put_flow > 0)) {
            if (s->put_n == 0) {
                s->cp2_cnt_x++;
            }
            s->put_cnt++;
            if (s->put_cnt >= s->d_put_flow) {
                s->put_n++;
                s->put_cnt = 0;
                if (s->put_n >= 4) {
                    s->put_n = 0;
                }
            }
        } else {
            s->cp2_cnt_x++;
        }

        if (s->cp2_cnt_x >= s->cp2_nx) {
            s->cp2_cnt_x = 0;
            s->cp2_cnt_y++;
        }
        if (s->cp2_cnt_y >= s->cp2_ny) {
            s->cp2_cnt_y = 0;
            if (s->d_put_flow == 0) {
                s->put_n = (s->put_n + 1) % 4;
            }
        }

        /* update ext.memory counters */
        if (!s->cp2_lo64) {
            s->m_cnt_b++;
        }
        if (s->m_cnt_b >= s->m_bx) {
            s->m_cnt_b = 0;
            s->m_cnt_x++;
        }

        if (s->m_cnt_x >= s->m_nx) {
            s->m_cnt_x = 0;
            s->m_cnt_y++;
        }
        if (s->m_cnt_y < s->m_ny) {
            /* Contrinue working */
            return 0;
        }
    }

    s->state = DMA_POSTSYNC;
    if (s->d_sync_post) {
        s->r_status |= 0x2;
        MSG_LOG_WRITE("Need postsync.");
    }

    return 0;
}

static int cp2_dma_state_postsync(CP2DmaState *s)
{
    if (s->d_sync_post) {
        return -1;
    }
    s->r_status &= ~0x2;

    MSG_LOG_WRITE("Operations finished.");

    int8_t t = cp2_ireg_read(s->cp2, s->d_jump_reg);
    if (s->d_type == DMA_D_JUMP) {
        switch (s->d_jump_type) {
        case 0:
        {  /* rel jump */
            s->r_curdescr += t;
            MSG_LOG_WRITE("Relative jump. ireg[%d]=%d. "
                                    "Next descriptor #%d.",
                                    s->d_jump_reg, t, s->r_curdescr);
            break;
        }
        case 1:
        {  /* table jump */
            t &= 0xF;
            s->r_curdescr +=
                (s->d_descr[1 + (t >> 2)] >> (16 * (3 - (t & 0x3))))
                & 0xFFFF;
            MSG_LOG_WRITE("Table jump. (ireg[%d] & 0xF)=%d. "
                                    "Next descriptor #%d.",
                                    s->d_jump_reg, t, s->r_curdescr);
            break;
        }
        case 2:
        {  /* abs jump */
            s->r_curdescr = (uint8_t)t;
            MSG_LOG_WRITE("Absolute jump. ireg[%d]=%d. "
                                    "Next descriptor #%d.",
                                    s->d_jump_reg, s->r_curdescr,
                                    s->r_curdescr);
            break;
        }
        case 3:
        {  /* cond jump */
            uint32_t b = s->d_descr[0] & 0xFFFFFFFF;
            if ((uint32_t)t == b) {
                s->r_curdescr += (int16_t)((s->d_descr[1] >> 48) & 0xFFFF);
            } else {
                s->r_curdescr += (int16_t)((s->d_descr[1] >> 32) & 0xFFFF);
            }
            MSG_LOG_WRITE("Condition jump. ireg[%d]=0x%x %s 0x%x. "
                          "Next descriptor #%d.",
                          s->d_jump_reg, t, (t == b) ? "==" : "!=",
                          b, s->r_curdescr);
            break;
        }
        }
    } else {  /* for "put" & "get" */
        if (s->d_jump_type == 0) { /* ijump */
            s->r_curdescr += s->d_jump_offset;
            MSG_LOG_WRITE("Immediate jump. imm=%d. "
                                    "Next descriptor #%d.",
                                    s->d_jump_offset, s->r_curdescr);
        } else { /* rjump */
            s->r_curdescr += t;
            MSG_LOG_WRITE("Relative jump. ireg[%d]=%d. "
                                    "Next descriptor #%d.",
                                    s->d_jump_reg, t, s->r_curdescr);
        }
    }

    if (s->d_int) {
        s->r_intcause |= 0x4;
    }

    if (s->r_ctrl & 0x2) {  /* clear stop bit, stop */
        s->r_ctrl &= ~0x2;
        s->r_intcause |= 0x2;
    } else if (!s->d_stop) {
        s->d_cnt = 0;
        s->d_addr = s->rc_baseaddr + s->r_daddr + s->r_curdescr * 0x40;
        s->state = DMA_NODSCR;
        return 0;
    }  /* start */

    s->state = DMA_IDLE;
    s->r_status &= ~0x1;
    MSG_LOG_WRITE("Channel stopped.");

    return 0;
}

static bool cp2_dma_main_action(void *opaque)
{
    CP2DmaState *s = (CP2DmaState *) opaque;

    switch (s->state) {
    case DMA_NODSCR:
        cp2_dma_state_nodescr(s);
        break;
    case DMA_PRESYNC:
        cp2_dma_state_presync(s);
        break;
    case DMA_WORKING:
        cp2_dma_state_working(s);
        break;
    case DMA_POSTSYNC:
        cp2_dma_state_postsync(s);
        break;
    case DMA_IDLE:
    default:
        break;
    }

    return s->state == DMA_WORKING;
}

static void cp2_dma_start_channel(void *opaque)
{
    CP2DmaState *s = (CP2DmaState *) opaque;
    if (s->state == DMA_PRESYNC) {
        MSG_LOG_WRITE("PRESYNC FALSE");
        s->d_sync_pre = false;
    } else if (s->state == DMA_POSTSYNC) {
        MSG_LOG_WRITE("POSTSYNC FALSE");
        s->d_sync_post = false;
    }
}

static void cp2_dma_write(void *opaque, hwaddr addr,
    uint64_t val, unsigned size)
{
    CP2DmaState *s = opaque;
    uint32_t data = val;

    if (addr & 0x1000) {  /* system regs */
        switch (addr & 0x001C) {
        case DMA_CREG_ADDRLIM_OFFSET:
            s->rc_addrlim = data & 0xFFFFFFF8;
            break;
        case DMA_CREG_INTMASK_OFFSET:
            s->rc_intmask = data & 0x7;
            break;
        case DMA_CREG_BASEADDR_OFFSET:
            s->rc_baseaddr = data & 0xFFFFFFF8;
            break;
        case DMA_CREG_CONFIG_OFFSET:
            s->rc_config = data & 0x3;
            break;
        default:
            printf("cp2: Invalid write at addr %lx\n", addr);
            break;
        }
    } else {
        switch (addr & 0x001C) {
        case DMA_CH_CP2_CTRL_OFFSET:
            s->r_ctrl = data & 0x3;
            if (s->r_ctrl & 0x1) {
                MSG_LOG_WRITE("DMA START");
                cp2_dma_state_idle(s);
            }
            break;
        case DMA_CH_CP2_DADDR_OFFSET:
            s->r_daddr = data & 0xFFFFFFF8;
            break;
        case DMA_CH_CP2_CURDSC_OFFSET:
            s->r_curdescr = data;
            break;
        default:
            printf("cp2: Invalid write at addr %lx\n", addr);
            break;
        }
    }
}

static uint64_t cp2_dma_read(void *opaque, hwaddr addr, unsigned size)
{
    CP2DmaState *s = opaque;
    uint64_t rv = 0;  /* return value */

    if (addr & 0x1000) {  /* system regs */
        switch (addr & 0x001C) {
        case DMA_CREG_ADDRLIM_OFFSET:
            rv = s->rc_addrlim;
            break;
        case DMA_CREG_INTMASK_OFFSET:
            rv = s->rc_intmask;
            break;
        case DMA_CREG_BASEADDR_OFFSET:
            rv = s->rc_baseaddr;
            break;
        case DMA_CREG_INTCAUSE_OFFSET:
            rv = s->r_intcause;
            break;
        case DMA_CREG_CONFIG_OFFSET:
            rv = s->rc_config;
            break;
        default:
            printf("cp2: Invalid read at addr %lx\n", addr);
            break;
        }
    } else {
        switch (addr & 0x001C) {
        case DMA_CH_CP2_CTRL_OFFSET:
            rv = s->r_ctrl;
            break;
        case DMA_CH_CP2_STATUS_OFFSET:
            rv = s->r_status;
            break;
        case DMA_CH_CP2_DADDR_OFFSET:
            rv = s->r_daddr;
            break;
        case DMA_CH_CP2_INT_U_OFFSET:
            rv = s->r_intcause;
            if (s->rc_config & 0x2) {
                s->r_intcause = 0;
            }
            break;
        case DMA_CH_CP2_CURDSC_OFFSET:
            rv = s->r_curdescr;
            break;
        default:
            printf("cp2: Invalid read at addr %lx\n", addr);
            break;
        }
    }

    return rv;
}

static const MemoryRegionOps dma_mem_ops = {
    .read = cp2_dma_read,
    .write = cp2_dma_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static void cp2_dma_reset(DeviceState *dev)
{
    CP2DmaState *s = CP2_DMA(dev);

    s->state = DMA_IDLE;

    s->r_ctrl = 0;
    s->r_status = 0;
    s->r_daddr = 0;
    s->r_curdescr = 0;
    s->r_intcause = 0;
    s->rc_addrlim = 0xFFFFFFFF;
    s->rc_baseaddr = 0;
    s->rc_intmask = 0;
    s->rc_config = 0;

    cp2_reset(s->cp2);
}

static int cp2_dma_check_function(struct k128dma_ctrl_state *dma_ctx)
{
    CP2DmaState *s = (CP2DmaState *) dma_ctx;
    char res = 0;

    if (s->state == DMA_NODSCR) {
        return 0x1;
    }
    if ((s->r_status & 0x01) & !(s->r_ctrl & 0x01)) {
        res |= 0x1; /* 1 = work, 0 = idle */
    }
    if ((s->state == DMA_PRESYNC) || (s->state == DMA_POSTSYNC)) {
        res |= 0x2; /* 1 = waiting for sync */
    } else if (s->state == DMA_WORKING) {
        res |= 0x4; /* 1 = dma working */
    }

    return res;
}

static int cp2_dma_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    CP2DmaState *s = CP2_DMA(dev);

    memory_region_init_io(&s->iomem[0], OBJECT(s),
                          &dma_mem_ops, s, TYPE_CP2_DMA, CP2_DMA_SIZE);
    sysbus_init_mmio(sbd, &s->iomem[0]);

    return 0;
}

static void cp2_dma_sysbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = cp2_dma_init;
    dc->desc = "CP2 DMA Controller";
    dc->reset = cp2_dma_reset;
}

static const TypeInfo cp2_dma_sysbus_info = {
    .name          = TYPE_CP2_DMA,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CP2DmaState),
    .class_init    = cp2_dma_sysbus_class_init,
};

static void cp2_dma_register_types(void)
{
    type_register_static(&cp2_dma_sysbus_info);
}

type_init(cp2_dma_register_types)

#define CTRLREG_PC         0x00
#define CTRLREG_COMM       0x01
#define CTRLREG_CONTROL    0x02
#define CTRLREG_STATUS     0x03
#define CTRLREG_PSP        0x04
#define CTRLREG_LC         0x05
#define CTRLREG_LA         0x06
#define CTRLREG_LSP        0x07
#define CTRLREG_RIND       0x08
#define CTRLREG_RSTEP      0x09
#define CTRLREG_RMASK      0x0a
#define CTRLREG_STOPCODE   0x0b

static void *cp2_thread_fn(void *arg);

static void cp2_reset(CP2State *cp2)
{
    bool locked = false;
    if (!qemu_mutex_iothread_locked()) {
        qemu_mutex_lock_iothread();
        locked = true;
    }
    libk128cp2_reset(cp2->cp2ptr);
    if (locked) {
        qemu_mutex_unlock_iothread();
    }
}

static CP2State *cp2_create(void)
{
    CP2State *cp2;

    cp2 = g_malloc0(sizeof(CP2State));
    memset(cp2, 0, sizeof *cp2);

    libk128cp2_create(&cp2->cp2ptr);
    libk128cp2_init(cp2->cp2ptr);
    libk128cp2_reset(cp2->cp2ptr);

    return cp2;
}

static inline void cp2_do_one_step(CP2State *cp2)
{
    if (libk128cp2_start_dma_read(cp2->cp2ptr)) {
        cp2_dma_start_channel(cp2->dma_ptr);
    }

    libk128cp2_clock(cp2->cp2ptr, cp2->cp2_clock_count++);
}

void cp2_do_work(CP2State *cp2)
{
    while (1) {
        cp2_do_one_step(cp2);

        if (!libk128cp2_pending_work(cp2->cp2ptr)) {
            cp2_do_one_step(cp2);

            /* Here we try to do several iterations to make DMA faster.
             * It is required for some CP2 tests under Linux */
            while (cp2_dma_main_action(cp2->dma_ptr)) {
                ;
            }

            break;
        }
    }
}

static void *cp2_thread_fn(void *arg)
{
    CP2State *cp2 = arg;

    while (1) {
        qemu_mutex_lock_iothread();
        cp2_do_work(cp2);
        qemu_mutex_unlock_iothread();
    }
    return NULL;
}

static uint64_t cp2_ireg_read(CP2State *cp2, uint8_t i)
{
    return libk128cp2_ireg_read(cp2->cp2ptr, i / 2) >> ((i & 0x1) ? 0 : 32);
}

static uint64_t cp2_dread(CP2State *cp2, uint8_t n, uint16_t adr)
{
    uint64_t data;
    libk128cp2_read64(cp2->cp2ptr, &data, adr, n);
    return data;
}

static void cp2_dwrite(CP2State *cp2, uint8_t n, uint16_t adr, uint64_t data)
{
    libk128cp2_write64(cp2->cp2ptr, &data, adr, n);
}

bool cp2_fifo_full(CP2State *cp2)
{
    return libk128cp2_condcode(cp2->cp2ptr, 0);
}

void cp2_ldc2(CP2State *cp2, uint64_t value)
{
    libk128cp2_reg_write(cp2->cp2ptr, 0, value, 0xFFFFFFFFFFFFFFFFULL, 1);
}

void cp2_reg_write(CP2State *cp2, int reg, uint64_t value)
{
    libk128cp2_reg_write(cp2->cp2ptr, reg, value, 0xFFFFFFFFFFFFFFFFULL, 0);
}

uint64_t cp2_reg_read(CP2State *cp2, int reg)
{
    return libk128cp2_reg_read(cp2->cp2ptr, reg);
}

uint64_t cp2_get_reg(CP2State *cp2, enum cp2regtype type, uint8_t sect,
    uint8_t num)
{
    switch (type) {
    case CP2_SYSREG:
        return k128cp2_debug_ctrlreg_read(cp2->cp2ptr, num);
    case CP2_FPSYSREG:
        return k128cp2_debug_fpu_ctrlreg_read(cp2->cp2ptr, num, sect);
    case CP2_ARF:
        return k128cp2_debug_areg_read(cp2->cp2ptr, num, 0) |
               k128cp2_debug_areg_read(cp2->cp2ptr, num, 1) |
               k128cp2_debug_areg_read(cp2->cp2ptr, num, 2);
    case CP2_PSTACK_PC:
        return k128cp2_debug_pstack_read(cp2->cp2ptr, num);
    case CP2_LSTACK_LC:
        return k128cp2_debug_lstack_read(cp2->cp2ptr, num, 0);
    case CP2_LSTACK_LA:
        return k128cp2_debug_lstack_read(cp2->cp2ptr, num, 1);
    default:
        return 0;
    }
}

uint64_t cp2_iread(CP2State *cp2, uint16_t addr)
{
    uint64_t data;
    libk128cp2_iram_read(cp2->cp2ptr, &data, 1, addr);
    return data;
}

CP2State *sc64_cp2_register(hwaddr addr, AddressSpace *as)
{
    DeviceState *dev;
    SysBusDevice *s;
    CP2DmaState *cp2_dma;
    CP2State *cp2;

    cp2 = cp2_create();

    dev = qdev_create(NULL, TYPE_CP2_DMA);
    qdev_init_nofail(dev);

    cp2_dma = CP2_DMA(dev);
    cp2->dma_ptr = cp2_dma;
    cp2_dma->cp2 = cp2;
    cp2_dma->as = as;

    libk128cp2_set_dma_context(cp2->cp2ptr, cp2_dma_check_function,
        (struct k128dma_ctrl_state *) cp2->dma_ptr);
    qemu_thread_create(&cp2->thread, "cp2_thread", cp2_thread_fn,
                       cp2, QEMU_THREAD_JOINABLE);

    s = SYS_BUS_DEVICE(dev);
    if (addr != (hwaddr)-1) {
        sysbus_mmio_map(s, 0, addr);
    }

    return cp2;
}
