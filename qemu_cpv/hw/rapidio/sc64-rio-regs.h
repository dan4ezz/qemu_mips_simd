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

#ifndef SC64_RIO_REGS_H_
#define SC64_RIO_REGS_H_

#define DEV_VEN_ID          0x00100
#define CLASS               0x00108
#define SID                 0x0012c

#define RIO_M_STATUS        0x00038
#define    M_STATUS_RETRY (1 << 27)
#define    M_STATUS_NORESP (1 << 26)
#define    M_STATUS_BADRESP (1 << 25)
#define    M_STATUS_BADID (1 << 18)
#define    M_STATUS_ABORT (1 << 17)
#define    M_STATUS_ERROR (1 << 16)
#define    M_STATUS_BUSY (1 <<  8)
#define    M_STATUS_DONE (1 <<  0)
#define RIO_M_DATAIN_LO     0x00040
#define RIO_M_DATAIN_HI     0x00044

#define RIO_MAKE_FIELD(name, shift, mask) \
    const uint32_t name##_SHIFT = shift; \
    const uint32_t name##_MASK = mask;

#define RIO_MASK(x) x##_MASK
#define RIO_SHIFT(x) x##_SHIFT
#define RIO_SHIFTED_MASK(x) (x##_MASK << x##_SHIFT)

#define PRIORITY 0x10890
RIO_MAKE_FIELD(PRIORITY_WMASK, 0, 0x0FFF3333)

#define INTERRUPT_ADD1 0x10884

RIO_MAKE_FIELD(INTERRUPT_ADD1_MSGINPOL7, 7, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD1_MSGINPOL6, 6, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD1_MSGINPOL5, 5, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD1_MSGINPOL4, 4, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD1_MSGINPOL3, 3, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD1_MSGINPOL2, 2, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD1_MSGINPOL1, 1, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD1_MSGINPOL0, 0, 1)

#define INTERRUPT_ENA 0x108A0
RIO_MAKE_FIELD(INTERRUPT_ENA_WMASK, 0, 0xF3DFDFFF)

RIO_MAKE_FIELD(INTERRUPT_ENA_MSGINPOLMASK, 22, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_MSGOUTPOLMASK, 20, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_MSGINOWNMASK, 19, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_DBINERRMASK, 15, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_DBINTAILDONEMASK, 14, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_DBINDONEMASK, 12, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_DBOUTERRMASK, 11, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_DBOUTDONEMASK, 8, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_MSGINERRMASK, 7, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_MSGINEOLMASK, 6, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_MSGINCOMPLMASK, 5, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_MSGINDONEMASK, 4, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_MSGOUTERRMASK, 3, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_MSGOUTEOLMASK, 2, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_MSGOUTCOMPLMASK, 1, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_MSGOUTOWNMASK, 0, 1)

#define INTERRUPT_ENA_ADD 0x108A4
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_WMASK, 0, 0xFFFFFFFF)

RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINFAILMASK7, 31, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINOWNMASK7, 30, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINEOLMASK7, 29, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINCOMPLMASK7, 28, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINFAILMASK6, 27, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINOWNMASK6, 26, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINEOLMASK6, 25, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINCOMPLMASK6, 24, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINFAILMASK5, 23, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINOWNMASK5, 22, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINEOLMASK5, 21, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINCOMPLMASK5, 20, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINFAILMASK4, 19, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINOWNMASK4, 18, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINEOLMASK4, 17, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINCOMPLMASK4, 16, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINFAILMASK3, 15, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINOWNMASK3, 14, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINEOLMASK3, 13, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINCOMPLMASK3, 12, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINFAILMASK2, 11, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINOWNMASK2, 10, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINEOLMASK2, 9, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINCOMPLMASK2, 8, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINFAILMASK1, 7, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINOWNMASK1, 6, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINEOLMASK1, 5, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINCOMPLMASK1, 4, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINFAILMASK0, 3, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINOWNMASK0, 2, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINEOLMASK0, 1, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD_MSGINCOMPLMASK0, 0, 1)

#define INTERRUPT_ENA_ADD1 0x108AC
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD1_WMASK, 0, 0x8001FFFF)

RIO_MAKE_FIELD(INTERRUPT_ENA_ADD1_MSGINPOL7, 7, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD1_MSGINPOL6, 6, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD1_MSGINPOL5, 5, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD1_MSGINPOL4, 4, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD1_MSGINPOL3, 3, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD1_MSGINPOL2, 2, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD1_MSGINPOL1, 1, 1)
RIO_MAKE_FIELD(INTERRUPT_ENA_ADD1_MSGINPOL0, 0, 1)

#define INTERRUPT 0x108B0
#define INTERRUPT_MASK 0x00000000

RIO_MAKE_FIELD(INTERRUPT_MSGINPOL, 22, 1)
RIO_MAKE_FIELD(INTERRUPT_MSGOUTPOL, 20, 1)
RIO_MAKE_FIELD(INTERRUPT_MSGINOWN, 19, 1)
RIO_MAKE_FIELD(INTERRUPT_DBINERR, 15, 1)
RIO_MAKE_FIELD(INTERRUPT_DBINTAILDONE, 14, 1)
RIO_MAKE_FIELD(INTERRUPT_DBINDONE, 12, 1)
RIO_MAKE_FIELD(INTERRUPT_DBOUTERR, 11, 1)
RIO_MAKE_FIELD(INTERRUPT_DBOUTDONE, 8, 1)
RIO_MAKE_FIELD(INTERRUPT_MSGINERR, 7, 1)
RIO_MAKE_FIELD(INTERRUPT_MSGINEOL, 6, 1)
RIO_MAKE_FIELD(INTERRUPT_MSGINCOMPL, 5, 1)
RIO_MAKE_FIELD(INTERRUPT_MSGINDONE, 4, 1)
RIO_MAKE_FIELD(INTERRUPT_MSGOUTERR, 3, 1)
RIO_MAKE_FIELD(INTERRUPT_MSGOUTEOL, 2, 1)
RIO_MAKE_FIELD(INTERRUPT_MSGOUTCOMPL, 1, 1)
RIO_MAKE_FIELD(INTERRUPT_MSGOUTOWN, 0, 1)

#define INTERRUPT_ADD 0x108B4

RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINFAIL7, 31, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINOWN7, 30, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINEOL7, 29, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINCOMPL7, 28, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINFAIL6, 27, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINOWN6, 26, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINEOL6, 25, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINCOMPL6, 24, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINFAIL5, 23, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINOWN5, 22, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINEOL5, 21, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINCOMPL5, 20, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINFAIL4, 19, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINOWN4, 18, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINEOL4, 17, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINCOMPL4, 16, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINFAIL3, 15, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINOWN3, 14, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINEOL3, 13, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINCOMPL3, 12, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINFAIL2, 11, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINOWN2, 10, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINEOL2, 9, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINCOMPL2, 8, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINFAIL1, 7, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINOWN1, 6, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINEOL1, 5, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINCOMPL1, 4, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINFAIL0, 3, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINOWN0, 2, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINEOL0, 1, 1)
RIO_MAKE_FIELD(INTERRUPT_ADD_MSGINCOMPL0, 0, 1)

/* [VM6] GA (geographic address) */
#define GA 0x00010

#define MAINT_OUT_TIMER 0x00024
RIO_MAKE_FIELD(MAINT_OUT_TIMER_WMASK, 24, 0xFF)

#define MAINT_OUT1 0x00028
RIO_MAKE_FIELD(MAINT_OUT1_WMASK, 0, 0xFF11FFFF)

RIO_MAKE_FIELD(MAINT_OUT1_WR, 20, 1)
RIO_MAKE_FIELD(MAINT_OUT1_ID, 16, 1)

#define MAINT_OUT2 0x0002C
#define MAINT_OUT3 0x00030
#define MAINT_OUT4 0x00034
RIO_MAKE_FIELD(MAINT_OUT4_WMASK, 3, 0xFFFFFFF1)

#define MAINT_OUT5 0x00038
RIO_MAKE_FIELD(MAINT_OUT5_WMASK, 0, 0x0E060101)

RIO_MAKE_FIELD(MAINT_OUT5_PROGRESS, 8, 1)
RIO_MAKE_FIELD(MAINT_OUT5_DONE, 0, 1)

#define MAINT_OUT6 0x00040
#define MAINT_OUT7 0x00044

#define LOG_ERR_DET 0x1008

#define MSG_OUT1 0x10800
RIO_MAKE_FIELD(MSG_OUT1_DESCR, 0, 0xffffffff)

#define MSG_OUT2 0x10804
RIO_MAKE_FIELD(MSG_OUT2_WATERLINE, 0, 0x000000ff)

#define MSG_OUT3 0x10808
RIO_MAKE_FIELD(MSG_OUT3_WMASK, 0, 0x000000FF)

#define MSG_OUT_STATUS 0x10818
RIO_MAKE_FIELD(MSG_OUT_STATUS_WMASK, 0, 0x00003741)

RIO_MAKE_FIELD(MSG_OUT_STATUS_POL, 13, 1)
RIO_MAKE_FIELD(MSG_OUT_STATUS_OWN, 12, 1)
RIO_MAKE_FIELD(MSG_OUT_STATUS_FAIL, 11, 1)
RIO_MAKE_FIELD(MSG_OUT_STATUS_EOL, 10, 1)
RIO_MAKE_FIELD(MSG_OUT_STATUS_COMPL, 9, 1)
RIO_MAKE_FIELD(MSG_OUT_STATUS_MODE, 6, 1)
RIO_MAKE_FIELD(MSG_OUT_STATUS_INIT, 0, 1)

RIO_MAKE_FIELD(MSG_OUT_STATUS_IRQ, 0,
    (1 << 13) | (1 << 12) | (1 << 11) | (1 << 10) | (1 << 9))

#define MSG_OUT_ERROR 0x1081C
RIO_MAKE_FIELD(MSG_OUT_ERROR_WMASK, 0, 0x00000000)

RIO_MAKE_FIELD(MSG_OUT_ERROR_ERR_WMASK, 0, 0x00ffffff)
RIO_MAKE_FIELD(MSG_OUT_ERROR_ERR_IRQ, 0, 0x00ffffff)

RIO_MAKE_FIELD(MSG_OUT_ERROR_RETRY_LIMIT, 11, 1)
RIO_MAKE_FIELD(MSG_OUT_ERROR_TIMEOUT, 2, 1)

#define MSG_IN1_9 0x10820

RIO_MAKE_FIELD(MSG_IN1_9_DESCR, 0, 0xffffffff)

#define MSG_IN2_9 0x10824
RIO_MAKE_FIELD(MSG_IN2_9_WMASK, 0, 0x000300FF)

RIO_MAKE_FIELD(MSG_IN2_9_TOENA, 17, 1)
RIO_MAKE_FIELD(MSG_IN2_9_RETRYENA, 16, 1)

#define MSG_IN_STATUS_9 0x10828
RIO_MAKE_FIELD(MSG_IN_STATUS_9_WMASK, 0, 0x00007FC1)
RIO_MAKE_FIELD(MSG_IN_STATUS_1_8_WMASK, 0, 0x00007E41)

RIO_MAKE_FIELD(MSG_IN_STATUS_9_STOP, 15, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_9_POL, 13, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_9_OWN, 12, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_9_FAIL, 11, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_9_EOL, 10, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_9_COMPL, 9, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_9_DONE, 8, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_9_18MODE, 7, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_9_MODE, 6, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_9_INIT, 0, 1)

RIO_MAKE_FIELD(MSG_IN_STATUS_9_IRQ, 8, 0x3F)

#define MSG_IN_ERROR_9 0x1082C
RIO_MAKE_FIELD(MSG_IN_ERROR_9_WMASK, 0, 0x000007FF)

RIO_MAKE_FIELD(MSG_IN_ERROR_9_IRQ, 0, 0x307ff)

#define DBOUT1 0x10830
#define DBOUT2 0x10834
#define DBOUT_STATUS 0x10838
RIO_MAKE_FIELD(DBOUT_STATUS_WMASK, 0, 0x00000901)

RIO_MAKE_FIELD(DBOUT_FAIL, 11, 1)
RIO_MAKE_FIELD(DBOUT_DONE, 8, 1)
RIO_MAKE_FIELD(DBOUT_PROGRESS, 0, 1)

#define DBOUT_ERROR 0x1083C
RIO_MAKE_FIELD(DBOUT_RESP_IN_ERR, 16, 0xFF)

RIO_MAKE_FIELD(DBOUT_ERROR_TIMEOUT, 2, 1)

#define DBIN_STATUS_CONTROL 0x10848

RIO_MAKE_FIELD(DBIN_DONE, 8, 1)
RIO_MAKE_FIELD(DBIN_MODE, 1, 1)
RIO_MAKE_FIELD(DBIN_INIT, 0, 1)

#define DBIN_STATUS1  0x10850
#define DBIN_STATUS2  0x10854

RIO_MAKE_FIELD(DBIN_DATA, 16, 0xFFFF)
RIO_MAKE_FIELD(DBIN_TID, 8, 0xFF)

#define MSG_IN1_1 0x10870
#define MSG_IN2_1 0x10874

RIO_MAKE_FIELD(MSG_IN2_X_MBOXTRAPMASK, 20, 0x3)
RIO_MAKE_FIELD(MSG_IN2_X_SRCID16TRAP, 16, 1)
RIO_MAKE_FIELD(MSG_IN2_X_SRCIDTRAP_HI_LO, 0, 0xFFFF)
RIO_MAKE_FIELD(MSG_IN2_X_SRCIDTRAP_HI, 8, 0xFF)
RIO_MAKE_FIELD(MSG_IN2_X_SRCIDTRAP_LO, 0, 0xFF)

#define MSG_IN_STATUS_1 0x10878

RIO_MAKE_FIELD(MSG_IN_STATUS_X_POL, 13, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_X_OWN, 12, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_X_FAIL, 11, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_X_EOL, 10, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_X_COMPL, 9, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_X_18MODE, 6, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_X_INIT, 0, 1)

#define MSG_IN1_2 0x10894
#define MSG_IN2_2 0x10898
#define MSG_IN_STATUS_2 0x1089C

#define MSG_IN3_9 0x108A8
RIO_MAKE_FIELD(MSG_IN3_9_WMASK, 16, 0xFFFF)

RIO_MAKE_FIELD(MSG_IN3_9_POLLIMIT, 16, 0xFF)

#define MSG_IN1_3 0x108B8
#define MSG_IN2_3 0x108BC
#define MSG_IN_STATUS_3 0x108C0
#define MSG_IN1_4 0x108C4
#define MSG_IN2_4 0x108C8
#define MSG_IN_STATUS_4 0x108CC
#define MSG_IN1_5 0x108D0
#define MSG_IN2_5 0x108D4
#define MSG_IN_STATUS_5 0x108D8
#define MSG_IN1_6 0x108DC
#define MSG_IN2_6 0x108E0
#define MSG_IN_STATUS_6 0x108E4
#define MSG_IN1_7 0x108E8
#define MSG_IN2_7 0x108EC
#define MSG_IN_STATUS_7 0x108F0
#define MSG_IN1_8 0x108F4
#define MSG_IN2_8 0x108F8
#define MSG_IN_STATUS_8 0x108FC

/* Common for all queues */
RIO_MAKE_FIELD(MSG_IN_STATUS_STOP, 15, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_POL,  13, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_OWN,  12, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_FAIL, 11, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_EOL,  10, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_COMPL, 9, 1)
RIO_MAKE_FIELD(MSG_IN_STATUS_INIT,  0, 1)

RIO_MAKE_FIELD(MSG_IN_STATUS_1_8_IRQ, 9, 0x1F)

#define MSGINPOLINT_SHIFTED_MASK(i)   (1 << (i + 1))

#define MSGINCOMPLINT_SHIFTED_MASK(i) (1 << (4 * i + 0))
#define MSGINEOLINT_SHIFTED_MASK(i)   (1 << (4 * i + 1))
#define MSGINOWNINT_SHIFTED_MASK(i)   (1 << (4 * i + 2))
#define MSGINFAILINT_SHIFTED_MASK(i)  (1 << (4 * i + 3))

#define ADD_W_R2 0x10C04
RIO_MAKE_FIELD(ADD_W_R2, 8, 0x00009C)

RIO_MAKE_FIELD(ADD_W_R2_MSGINPRELETTERMODE, 12, 1)
RIO_MAKE_FIELD(ADD_W_R2_MSGOUTPRELETTERMODE, 11, 1)
RIO_MAKE_FIELD(ADD_W_R2_MSGINNOWNENA, 10, 1)

#define ADD_W_R3 0x10C08

RIO_MAKE_FIELD(ADD_W_R3_MSGIN9MODE, 30, 1)
RIO_MAKE_FIELD(ADD_W_R3, 28, 0x4)

#define MSG_IN3_ADD1 0x10C10

RIO_MAKE_FIELD(MSG_IN3_ADD1_LIMIT1, 16, 0xFF)
RIO_MAKE_FIELD(MSG_IN3_ADD1_LIMIT0, 0, 0xFF)

#define MSG_IN3_ADD2 0x10C14

RIO_MAKE_FIELD(MSG_IN3_ADD2_LIMIT3, 16, 0xFF)
RIO_MAKE_FIELD(MSG_IN3_ADD2_LIMIT2, 0, 0xFF)

#define MSG_IN3_ADD3 0x10C18

RIO_MAKE_FIELD(MSG_IN3_ADD3_LIMIT5, 16, 0xFF)
RIO_MAKE_FIELD(MSG_IN3_ADD3_LIMIT4, 0, 0xFF)

#define MSG_IN3_ADD4 0x10C1C

RIO_MAKE_FIELD(MSG_IN3_ADD4_LIMIT7, 16, 0xFF)
RIO_MAKE_FIELD(MSG_IN3_ADD4_LIMIT6, 0, 0xFF)

/*
 * Logic level CAR
 */

#define MAINT_BASE 0x11000

/* [I] Device Identity CAR */
#define DEV_ID_CAR (MAINT_BASE + RIO_DEV_ID_CAR)

/* [I] Device identifier */
#define RIO_DEV_ID_CAR_DI_SHIFT 16

/* [I] Device Information CAR */
#define DEV_INFO_CAR (MAINT_BASE + RIO_DEV_INFO_CAR)

/* [I] Assembly Identity CAR */
#define ASM_ID_CAR (MAINT_BASE + RIO_ASM_ID_CAR)

/* [I] Assembly Information CAR */
#define ASM_INFO_CAR (MAINT_BASE + RIO_ASM_INFO_CAR)

/* [I] Processing Element Features CAR */
#define PE_FEAT_CAR (MAINT_BASE + RIO_PEF_CAR)

/* [I] Switch Port Information CAR */
#define SWITCH_PI_CAR (MAINT_BASE + RIO_SWP_INFO_CAR)

/* [I] Source Operations CAR */
#define SOURCE_OP_CAR (MAINT_BASE + RIO_SRC_OPS_CAR)

/* [I] Destination Operations CAR */
#define DEST_OP_CAR (MAINT_BASE + RIO_DST_OPS_CAR)

/* [III] Switch Route Table Destination ID Limit CAR */
#define LIMIT_ID_CAR (MAINT_BASE + RIO_SWITCH_RT_LIMIT)

/*
 * Logic level CSR
 */

/* [I] Processing Element Logical Layer Control CSR */
#define PE_LL_CTL_CSR (MAINT_BASE + RIO_PELL_CTRL_CSR)

/*
 * Transport level CSR
 */

/* [III] Base Device ID CSR */
#define BASE_DEV_CSR (MAINT_BASE + RIO_DID_CSR)
RIO_MAKE_FIELD(RIO_BASE_DEV_CSR_WMASK, 0, 0x00FFFFFF)

/* [III] Host Base Device ID Lock CSR */
#define HB_DEV_ID_LOCK_CSR (MAINT_BASE + \
        RIO_HOST_DID_LOCK_CSR)
RIO_MAKE_FIELD(RIO_HB_DEV_ID_LOCK_CSR_WMASK, 0, 0x0000FFFF)

/* [III] Host_base_deviceID Reset value */
#define RIO_HB_DEV_ID_LOCK_CSR_HBDID_RESET 0xFFFF

/* [III] Component Tag CSR */
#define COMP_TAG_CSR (MAINT_BASE + RIO_COMPONENT_TAG_CSR)

/*
 * Physical level CSR
 */

#define PORT_N_BASE 0x0100

#define PORT_LT_CTL (PORT_N_BASE + RIO_PORT_REQ_CTL_CSR)
#define PORT_RT_CTL (PORT_N_BASE + RIO_PORT_RSP_CTL_CSR)

#define PORT_N_ERR_STAT_CSR(x) (PORT_N_BASE + \
        RIO_PORT_N_ERR_STS_CSR((x)))
#define PORT_N_CTL_CSR(x) (PORT_N_BASE + \
        RIO_PORT_N_CTL_CSR((x)))

#define PORT0_ERR_STAT (MAINT_BASE + \
        PORT_N_ERR_STAT_CSR(0))

#define PORT1_ERR_STAT (MAINT_BASE + \
        PORT_N_ERR_STAT_CSR(1))

#define PORT2_ERR_STAT (MAINT_BASE + \
        PORT_N_ERR_STAT_CSR(2))

#define PORT0_CTL (MAINT_BASE + \
        PORT_N_CTL_CSR(0))

#define PORT1_CTL (MAINT_BASE + \
        PORT_N_CTL_CSR(1))

#define PORT2_CTL (MAINT_BASE + \
        PORT_N_CTL_CSR(2))

#endif /* SC64_RIO_REGS_H_ */
