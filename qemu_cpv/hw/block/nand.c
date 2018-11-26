/*
 * Flash NAND memory emulation.  Based on "16M x 8 Bit NAND Flash
 * Memory" datasheet for the KM29U128AT / K9F2808U0A chips from
 * Samsung Electronic.
 *
 * Copyright (c) 2006 Openedhand Ltd.
 * Written by Andrzej Zaborowski <balrog@zabor.org>
 *
 * Support for additional features based on "MT29F2G16ABCWP 2Gx16"
 * datasheet from Micron Technology and "NAND02G-B2C" datasheet
 * from ST Microelectronics.
 *
 * This code is licensed under the GNU GPL v2.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#ifndef NAND_IO

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/block/flash.h"
#include "sysemu/block-backend.h"
#include "hw/qdev.h"
#include "qapi/error.h"
#include "qemu/error-report.h"

# define NAND_CMD_READ0		0x00
# define NAND_CMD_READ1		0x01
# define NAND_CMD_READ2		0x50
# define NAND_CMD_LPREAD2	0x30
# define NAND_CMD_NOSERIALREAD2	0x35
# define NAND_CMD_RANDOMREAD1	0x05
# define NAND_CMD_RANDOMREAD2	0xe0
# define NAND_CMD_READID	0x90
# define NAND_CMD_RESET		0xff
# define NAND_CMD_PAGEPROGRAM1	0x80
# define NAND_CMD_PAGEPROGRAM2	0x10
# define NAND_CMD_CACHEPROGRAM2	0x15
# define NAND_CMD_BLOCKERASE1	0x60
# define NAND_CMD_BLOCKERASE2	0xd0
# define NAND_CMD_READSTATUS	0x70
# define NAND_CMD_COPYBACKPRG1	0x85
# define NAND_CMD_PARAM         0xec

# define NAND_IOSTATUS_ERROR	(1 << 0)
# define NAND_IOSTATUS_PLANE0	(1 << 1)
# define NAND_IOSTATUS_PLANE1	(1 << 2)
# define NAND_IOSTATUS_PLANE2	(1 << 3)
# define NAND_IOSTATUS_PLANE3	(1 << 4)
# define NAND_IOSTATUS_TREADY   (1 << 5)
# define NAND_IOSTATUS_READY    (1 << 6)
# define NAND_IOSTATUS_UNPROTCT	(1 << 7)

# define MAX_PAGE               0x4000
# define MAX_OOB                0x1000

typedef struct NANDFlashState NANDFlashState;
struct NANDFlashState {
    DeviceState parent_obj;

    uint8_t manf_id, chip_id;
    uint8_t buswidth; /* in BYTES */
    long long size, pages;
    int page_shift, oob_shift, erase_shift, addr_shift;
    int oob_margin; /* In bytes */
    uint8_t *storage;
    BlockBackend *blk;
    int mem_oob;

    uint8_t cle, ale, ce, wp, gnd;

    uint8_t io[MAX_PAGE + MAX_OOB + 0x400];
    uint8_t *ioaddr;
    int iolen;

    bool onfi;
    char *part_num;

    uint32_t cmd;
    uint64_t addr;
    int addrlen;
    int status;
    int offset;

    void (*blk_write)(NANDFlashState *s);
    void (*blk_erase)(NANDFlashState *s);
    void (*blk_load)(NANDFlashState *s, uint64_t addr, int offset);

    uint32_t ioaddr_vmstate;
};

#define TYPE_NAND "nand"

#define NAND(obj) \
    OBJECT_CHECK(NANDFlashState, (obj), TYPE_NAND)

#define ONFI_ID_OFFSET      0x20

struct nand_onfi_params {
    /* rev info and features block */
    /* 'O' 'N' 'F' 'I'  */
    uint8_t sig[4];
    uint16_t revision;
    uint16_t features;
    uint16_t opt_cmd;
    uint8_t reserved0[2];
    uint16_t ext_param_page_length; /* since ONFI 2.1 */
    uint8_t num_of_param_pages;        /* since ONFI 2.1 */
    uint8_t reserved1[17];

    /* manufacturer information block */
    char manufacturer[12];
    char model[20];
    uint8_t jedec_id;
    uint16_t date_code;
    uint8_t reserved2[13];

    /* memory organization block */
    uint32_t byte_per_page;
    uint16_t spare_bytes_per_page;
    uint32_t data_bytes_per_ppage;
    uint16_t spare_bytes_per_ppage;
    uint32_t pages_per_block;
    uint32_t blocks_per_lun;
    uint8_t lun_count;
    uint8_t addr_cycles;
    uint8_t bits_per_cell;
    uint16_t bb_per_lun;
    uint16_t block_endurance;
    uint8_t guaranteed_good_blocks;
    uint16_t guaranteed_block_endurance;
    uint8_t programs_per_page;
    uint8_t ppage_attr;
    uint8_t ecc_bits;
    uint8_t interleaved_bits;
    uint8_t interleaved_ops;
    uint8_t reserved3[13];

    /* electrical parameter block */
    uint8_t io_pin_capacitance_max;
    uint16_t async_timing_mode;
    uint16_t program_cache_timing_mode;
    uint16_t t_prog;
    uint16_t t_bers;
    uint16_t t_r;
    uint16_t t_ccs;
    uint16_t src_sync_timing_mode;
    uint8_t src_ssync_features;
    uint16_t clk_pin_capacitance_typ;
    uint16_t io_pin_capacitance_typ;
    uint16_t input_pin_capacitance_typ;
    uint8_t input_pin_capacitance_max;
    uint8_t driver_strength_support;
    uint16_t t_int_r;
    uint16_t t_adl;
    uint8_t reserved4[8];

    /* vendor */
    uint16_t vendor_revision;
    uint8_t vendor[88];

    uint16_t crc;
} __attribute__ ((packed));

static void mem_and(uint8_t *dest, const uint8_t *src, size_t n)
{
    /* Like memcpy() but we logical-AND the data into the destination */
    int i;
    for (i = 0; i < n; i++) {
        dest[i] &= src[i];
    }
}

# define NAND_NO_AUTOINCR	0x00000001
# define NAND_BUSWIDTH_16	0x00000002
# define NAND_NO_PADDING	0x00000004
# define NAND_CACHEPRG		0x00000008
# define NAND_COPYBACK		0x00000010
# define NAND_IS_AND		0x00000020
# define NAND_4PAGE_ARRAY	0x00000040
# define NAND_NO_READRDY	0x00000100
# define NAND_SAMSUNG_LP	(NAND_NO_PADDING | NAND_COPYBACK)
# define NAND_SAMSUNG_SRISA     0x80000000

# define NAND_IO

# define PAGE(addr)		((addr) >> ADDR_SHIFT)
# define PAGE_START(page, s)    (PAGE(page) * (PAGE_SIZE + OOB_SIZE(s)))
# define PAGE_MASK		((1 << ADDR_SHIFT) - 1)
# define OOB_SHIFT		(PAGE_SHIFT - 5)
# define OOB_SIZE(s)            ((1 << OOB_SHIFT) + (s)->oob_margin)
# define SECTOR(addr)		((addr) >> (9 + ADDR_SHIFT - PAGE_SHIFT))
# define SECTOR_OFFSET(addr)	((addr) & ((511 >> PAGE_SHIFT) << 8))

# define PAGE_SIZE		256
# define PAGE_SHIFT		8
# define PAGE_SECTORS		1
# define ADDR_SHIFT		8
# include "nand.c"
# define PAGE_SIZE		512
# define PAGE_SHIFT		9
# define PAGE_SECTORS		1
# define ADDR_SHIFT		8
# include "nand.c"
# define PAGE_SIZE		2048
# define PAGE_SHIFT		11
# define PAGE_SECTORS		4
# define ADDR_SHIFT		16
# include "nand.c"
# define PAGE_SIZE              4096
# define PAGE_SHIFT             12
# define PAGE_SECTORS           8
# define ADDR_SHIFT             16
# include "nand.c"
# define PAGE_SIZE              8192
# define PAGE_SHIFT             13
# define PAGE_SECTORS           16
# define ADDR_SHIFT             16
# include "nand.c"

/* Information based on Linux drivers/mtd/nand/nand_ids.c */
static const struct {
    int size;
    int width;
    int page_shift;
    int erase_shift;
    uint32_t options;
} nand_flash_ids[0x100] = {
    [0 ... 0xff] = { 0 },

    [0x6e] = { 1,	8,	8, 4, 0 },
    [0x64] = { 2,	8,	8, 4, 0 },
    [0x6b] = { 4,	8,	9, 4, 0 },
    [0xe8] = { 1,	8,	8, 4, 0 },
    [0xec] = { 1,	8,	8, 4, 0 },
    [0xea] = { 2,	8,	8, 4, 0 },
    [0xd5] = { 4,	8,	9, 4, 0 },
    [0xe3] = { 4,	8,	9, 4, 0 },
    [0xe5] = { 4,	8,	9, 4, 0 },
    [0xd6] = { 8,	8,	9, 4, 0 },

    [0x39] = { 8,	8,	9, 4, 0 },
    [0xe6] = { 8,	8,	9, 4, 0 },
    [0x49] = { 8,	16,	9, 4, NAND_BUSWIDTH_16 },
    [0x59] = { 8,	16,	9, 4, NAND_BUSWIDTH_16 },

    [0x33] = { 16,	8,	9, 5, 0 },
    [0x73] = { 16,	8,	9, 5, 0 },
    [0x43] = { 16,	16,	9, 5, NAND_BUSWIDTH_16 },
    [0x53] = { 16,	16,	9, 5, NAND_BUSWIDTH_16 },

    [0x35] = { 32,	8,	9, 5, 0 },
    [0x75] = { 32,	8,	9, 5, 0 },
    [0x45] = { 32,	16,	9, 5, NAND_BUSWIDTH_16 },
    [0x55] = { 32,	16,	9, 5, NAND_BUSWIDTH_16 },

    [0x36] = { 64,	8,	9, 5, 0 },
    [0x76] = { 64,	8,	9, 5, 0 },
    [0x46] = { 64,	16,	9, 5, NAND_BUSWIDTH_16 },
    [0x56] = { 64,	16,	9, 5, NAND_BUSWIDTH_16 },

    [0x78] = { 128,	8,	9, 5, 0 },
    [0x39] = { 128,	8,	9, 5, 0 },
    [0x79] = { 128,	8,	9, 5, 0 },
    [0x72] = { 128,	16,	9, 5, NAND_BUSWIDTH_16 },
    [0x49] = { 128,	16,	9, 5, NAND_BUSWIDTH_16 },
    [0x74] = { 128,	16,	9, 5, NAND_BUSWIDTH_16 },
    [0x59] = { 128,	16,	9, 5, NAND_BUSWIDTH_16 },

    [0x71] = { 256,	8,	9, 5, 0 },

    /*
     * These are the new chips with large page size. The pagesize and the
     * erasesize is determined from the extended id bytes
     */
# define LP_OPTIONS	(NAND_SAMSUNG_LP | NAND_NO_READRDY | NAND_NO_AUTOINCR)
# define LP_OPTIONS16	(LP_OPTIONS | NAND_BUSWIDTH_16)

    /* 512 Megabit */
    [0xa2] = { 64,	8,	0, 0, LP_OPTIONS },
    [0xf2] = { 64,	8,	0, 0, LP_OPTIONS },
    [0xb2] = { 64,	16,	0, 0, LP_OPTIONS16 },
    [0xc2] = { 64,	16,	0, 0, LP_OPTIONS16 },

    /* 1 Gigabit */
    [0xa1] = { 128,	8,	0, 0, LP_OPTIONS },
    [0xf1] = { 128,	8,	0, 0, LP_OPTIONS },
    [0xb1] = { 128,	16,	0, 0, LP_OPTIONS16 },
    [0xc1] = { 128,	16,	0, 0, LP_OPTIONS16 },

    /* 2 Gigabit */
    [0xaa] = { 256,	8,	0, 0, LP_OPTIONS },
    [0xda] = { 256,	8,	0, 0, LP_OPTIONS },
    [0xba] = { 256,	16,	0, 0, LP_OPTIONS16 },
    [0xca] = { 256,	16,	0, 0, LP_OPTIONS16 },

    /* 4 Gigabit */
    [0xac] = { 512,	8,	0, 0, LP_OPTIONS },
    [0xdc] = { 512,	8,	0, 0, LP_OPTIONS },
    [0xbc] = { 512,	16,	0, 0, LP_OPTIONS16 },
    [0xcc] = { 512,	16,	0, 0, LP_OPTIONS16 },

    /* 8 Gigabit */
    [0xa3] = { 1024,	8,	0, 0, LP_OPTIONS },
    [0xd3] = { 1024,    8,      0, 0, LP_OPTIONS | NAND_SAMSUNG_SRISA },
    [0xb3] = { 1024,	16,	0, 0, LP_OPTIONS16 },
    [0xc3] = { 1024,	16,	0, 0, LP_OPTIONS16 },

    /* 16 Gigabit */
    [0xa5] = { 2048,	8,	0, 0, LP_OPTIONS },
    [0xd5] = { 2048,	8,	0, 0, LP_OPTIONS },
    [0xb5] = { 2048,	16,	0, 0, LP_OPTIONS16 },
    [0xc5] = { 2048,	16,	0, 0, LP_OPTIONS16 },
};

static const struct {
    char const *manufacturer;
    char const *part_num;
    uint8_t manf_id;
    uint8_t chip_id;
    long long size;
    int width;
    int page_shift;
    int erase_shift;
    int oob_margin;
} nand_partn_ids[] = {
    { "Micron", "MT29F1G08ABBDA", 0x2c, 0xa1, 128,  8, 11, 7, 0 },
    { "Micron", "MT29F32G08ABCA", 0x2c, 0x68, 4096, 8, 12, 7, 0x60 },
    { "Micron", "MT29F64G08CBAB", 0x2c, 0xa1, 8192, 8, 13, 8, 0xc0 },
    { "Micron", "MT29F1G16ABBDA", 0x2c, 0xb1, 128, 16, 11, 6, 0x0 },
    { NULL, NULL }, /* End of list */
};

static int id_by_part_num(char const *part_num)
{
    int res = 0;
    assert(part_num);

    while (nand_partn_ids[res].part_num != NULL) {
        if (!strcmp(part_num, nand_partn_ids[res].part_num)) {
            return res;
        }
        res++;
    }

    return -1;
}

static void nand_reset(DeviceState *dev)
{
    NANDFlashState *s = NAND(dev);
    s->cmd = NAND_CMD_READ0;
    s->addr = 0;
    s->addrlen = 0;
    s->iolen = 0;
    s->offset = 0;
    s->status &= NAND_IOSTATUS_UNPROTCT;
    s->status |= NAND_IOSTATUS_READY | NAND_IOSTATUS_TREADY;
}

static inline void nand_pushio_byte(NANDFlashState *s, uint8_t value)
{
    s->ioaddr[s->iolen++] = value;
    for (value = s->buswidth; --value;) {
        s->ioaddr[s->iolen++] = 0;
    }
}

static uint16_t onfi_crc16(uint16_t crc, uint8_t const *p, size_t len)
{
    int i;
    while (len--) {
        crc ^= *p++ << 8;
        for (i = 0; i < 8; i++) {
            crc = (crc << 1) ^ ((crc & 0x8000) ? 0x8005 : 0);
        }
    }

    return crc;
}

static void nand_fill_onfi(NANDFlashState *s)
{
    static struct nand_onfi_params onfi;
    int i;

    s->ioaddr = s->io;
    s->iolen = 0;

    onfi = (struct nand_onfi_params) {
        .sig = { 'O', 'N', 'F', 'I' },
        .revision = 2,
        .features = s->buswidth == 2 ? 0x1 : 0x0,
        .jedec_id = 0xff,
        .date_code = 0xcafe,
        .byte_per_page = 1 << s->page_shift,
        .spare_bytes_per_page = (1 << s->oob_shift) + s->oob_margin,
        .data_bytes_per_ppage = 1 << s->page_shift,
        .pages_per_block = 1 << s->erase_shift,
        .blocks_per_lun = s->pages >> s->erase_shift,
        .lun_count = 1,
        .bits_per_cell = 24,
    };

    memset(onfi.manufacturer, 0, sizeof(onfi.manufacturer));
    memset(onfi.model, 0, sizeof(onfi.model));

    i = id_by_part_num(s->part_num);

    assert(i != -1);

    strncpy(onfi.manufacturer, nand_partn_ids[i].manufacturer,
            sizeof(onfi.manufacturer));
    strncpy(onfi.model, s->part_num, sizeof(onfi.model));

    onfi.crc = onfi_crc16(0x4F4E, (uint8_t *) &onfi, 254);

    for (i = 0; i < sizeof(onfi); i++) {
        nand_pushio_byte(s, *(((uint8_t *) &onfi) + i));
    }
}

static void nand_command(NANDFlashState *s)
{
    unsigned int offset;
    switch (s->cmd) {
    case NAND_CMD_READ0:
        s->iolen = 0;
        break;

    case NAND_CMD_READID:
        s->ioaddr = s->io;
        s->iolen = 0;

        if (s->onfi && s->addr == ONFI_ID_OFFSET) {
            nand_pushio_byte(s, 'O');
            nand_pushio_byte(s, 'N');
            nand_pushio_byte(s, 'F');
            nand_pushio_byte(s, 'I');
            break;
        }

        nand_pushio_byte(s, s->manf_id);
        nand_pushio_byte(s, s->chip_id);
        nand_pushio_byte(s, 'Q'); /* Don't-care byte (often 0xa5) */

        if (nand_flash_ids[s->chip_id].options & NAND_SAMSUNG_SRISA) {
            nand_pushio_byte(s, 0x95);
            nand_pushio_byte(s, 0x58);
            break;
        }

        if (nand_flash_ids[s->chip_id].options & NAND_SAMSUNG_LP) {
            /* Page Size, Block Size, Spare Size; bit 6 indicates
             * 8 vs 16 bit width NAND.
             */
            nand_pushio_byte(s, (s->buswidth == 2) ? 0x55 : 0x15);
        } else {
            nand_pushio_byte(s, 0xc0); /* Multi-plane */
        }
        break;
    case NAND_CMD_PARAM:
        if (s->onfi) {
            nand_fill_onfi(s);
        }
        break;

    case NAND_CMD_RANDOMREAD2:
    case NAND_CMD_NOSERIALREAD2:
        if (!(nand_flash_ids[s->chip_id].options & NAND_SAMSUNG_LP))
            break;

        offset = s->addr & ((1 << s->addr_shift) - 1);
        s->blk_load(s, s->addr, offset);
        if (s->gnd)
            s->iolen = (1 << s->page_shift) - offset;
        else
            s->iolen = (1 << s->page_shift) + (1 << s->oob_shift) +
                        s->oob_margin - offset;
        break;

    case NAND_CMD_RESET:
        nand_reset(DEVICE(s));
        break;

    case NAND_CMD_PAGEPROGRAM1:
        s->ioaddr = s->io;
        s->iolen = 0;
        break;

    case NAND_CMD_PAGEPROGRAM2:
        if (s->wp) {
            s->blk_write(s);
        }
        break;

    case NAND_CMD_BLOCKERASE1:
        break;

    case NAND_CMD_BLOCKERASE2:
        s->addr &= (1ull << s->addrlen * 8) - 1;

        if (s->part_num == NULL) {
            s->addr <<= nand_flash_ids[s->chip_id].options & NAND_SAMSUNG_LP ?
                                                                        16 : 8;
        } else {
            s->addr <<= s->addr_shift;
        }

        if (s->wp) {
            s->blk_erase(s);
        }
        break;

    case NAND_CMD_READSTATUS:
        s->ioaddr = s->io;
        s->iolen = 0;
        nand_pushio_byte(s, s->status);
        break;

    default:
        printf("%s: Unknown NAND command 0x%02x\n", __FUNCTION__, s->cmd);
    }
}

static int nand_pre_save(void *opaque)
{
    NANDFlashState *s = NAND(opaque);

    s->ioaddr_vmstate = s->ioaddr - s->io;

    return 0;
}

static int nand_post_load(void *opaque, int version_id)
{
    NANDFlashState *s = NAND(opaque);

    if (s->ioaddr_vmstate > sizeof(s->io)) {
        return -EINVAL;
    }
    s->ioaddr = s->io + s->ioaddr_vmstate;

    return 0;
}

static const VMStateDescription vmstate_nand = {
    .name = "nand",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_save = nand_pre_save,
    .post_load = nand_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(cle, NANDFlashState),
        VMSTATE_UINT8(ale, NANDFlashState),
        VMSTATE_UINT8(ce, NANDFlashState),
        VMSTATE_UINT8(wp, NANDFlashState),
        VMSTATE_UINT8(gnd, NANDFlashState),
        VMSTATE_BUFFER(io, NANDFlashState),
        VMSTATE_UINT32(ioaddr_vmstate, NANDFlashState),
        VMSTATE_INT32(iolen, NANDFlashState),
        VMSTATE_UINT32(cmd, NANDFlashState),
        VMSTATE_UINT64(addr, NANDFlashState),
        VMSTATE_INT32(addrlen, NANDFlashState),
        VMSTATE_INT32(status, NANDFlashState),
        VMSTATE_INT32(offset, NANDFlashState),
        /* XXX: do we want to save s->storage too? */
        VMSTATE_END_OF_LIST()
    }
};

static void nand_realize(DeviceState *dev, Error **errp)
{
    int pagesize;
    NANDFlashState *s = NAND(dev);
    int ret;

    if (s->part_num != NULL) {
        int i = id_by_part_num(s->part_num);
        if (i == -1) {
            error_report("Unknown part number: %s", s->part_num);
            return;
        }

        s->manf_id = nand_partn_ids[i].manf_id;
        s->chip_id = nand_partn_ids[i].chip_id;
        s->buswidth = nand_partn_ids[i].width >> 3;
        s->size = nand_partn_ids[i].size << 20;
        s->page_shift = nand_partn_ids[i].page_shift;
        s->erase_shift = nand_partn_ids[i].erase_shift;
        s->onfi = true;
        s->oob_margin = nand_partn_ids[i].oob_margin;
    } else {
        s->buswidth = nand_flash_ids[s->chip_id].width >> 3;
        s->size = nand_flash_ids[s->chip_id].size << 20;
        if (nand_flash_ids[s->chip_id].options & NAND_SAMSUNG_LP) {
            s->page_shift = 11;
            s->erase_shift = 6;
        } else {
            s->page_shift = nand_flash_ids[s->chip_id].page_shift;
            s->erase_shift = nand_flash_ids[s->chip_id].erase_shift;
        }
    }

    switch (1 << s->page_shift) {
    case 256:
        nand_init_256(s);
        break;
    case 512:
        nand_init_512(s);
        break;
    case 2048:
        nand_init_2048(s);
        break;
    case 4096:
        nand_init_4096(s);
        break;
    case 8192:
        nand_init_8192(s);
        break;
    default:
        error_setg(errp, "Unsupported NAND block size %#x",
                   1 << s->page_shift);
        return;
    }

    pagesize = (1 << s->oob_shift) + s->oob_margin;
    s->mem_oob = 1;
    if (s->blk) {
        if (blk_is_read_only(s->blk)) {
            error_setg(errp, "Can't use a read-only drive");
            return;
        }
        ret = blk_set_perm(s->blk, BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE,
                           BLK_PERM_ALL, errp);
        if (ret < 0) {
            return;
        }
        if (blk_getlength(s->blk) >= s->pages *
                ((1 << s->page_shift) + (1 << s->oob_shift) + s->oob_margin)) {
            pagesize = 0;
            s->mem_oob = 0;
        }
    } else {
        pagesize += 1 << s->page_shift;
    }
    if (pagesize) {
        s->storage = (uint8_t *) memset(g_malloc(s->pages * pagesize),
                        0xff, s->pages * pagesize);
    }
    /* Give s->ioaddr a sane value in case we save state before it is used. */
    s->ioaddr = s->io;
}

static Property nand_properties[] = {
    DEFINE_PROP_UINT8("manufacturer_id", NANDFlashState, manf_id, 0),
    DEFINE_PROP_UINT8("chip_id", NANDFlashState, chip_id, 0),
    DEFINE_PROP_DRIVE("drive", NANDFlashState, blk),
    DEFINE_PROP_STRING("part_num", NANDFlashState, part_num),
    DEFINE_PROP_END_OF_LIST(),
};

static void nand_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = nand_realize;
    dc->reset = nand_reset;
    dc->vmsd = &vmstate_nand;
    dc->props = nand_properties;
    dc->bus_type = TYPE_NAND_BUS;
}

static const TypeInfo nand_info = {
    .name          = TYPE_NAND,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(NANDFlashState),
    .class_init    = nand_class_init,
};

static const TypeInfo nand_bus_info = {
    .name = TYPE_NAND_BUS,
    .parent = TYPE_BUS,
};

static void nand_register_types(void)
{
    type_register_static(&nand_info);
    type_register_static(&nand_bus_info);
}

/*
 * Chip inputs are CLE, ALE, CE, WP, GND and eight I/O pins.  Chip
 * outputs are R/B and eight I/O pins.
 *
 * CE, WP and R/B are active low.
 */
void nand_setpins(DeviceState *dev, uint8_t cle, uint8_t ale,
                  uint8_t ce, uint8_t wp, uint8_t gnd)
{
    NANDFlashState *s = NAND(dev);

    s->cle = cle;
    s->ale = ale;
    s->ce = ce;
    s->wp = wp;
    s->gnd = gnd;
    if (wp) {
        s->status |= NAND_IOSTATUS_UNPROTCT;
    } else {
        s->status &= ~NAND_IOSTATUS_UNPROTCT;
    }
}

void nand_getpins(DeviceState *dev, int *rb)
{
    *rb = 1;
}

void nand_setio(DeviceState *dev, uint32_t value)
{
    int i;
    NANDFlashState *s = NAND(dev);

    if (!s->ce && s->cle) {
        if (nand_flash_ids[s->chip_id].options & NAND_SAMSUNG_LP) {
            if (s->cmd == NAND_CMD_READ0 && value == NAND_CMD_LPREAD2)
                return;
            if (value == NAND_CMD_RANDOMREAD1) {
                s->addr &= ~((1 << s->addr_shift) - 1);
                s->addrlen = 0;
                return;
            }
        }
        if (value == NAND_CMD_READ0) {
            s->offset = 0;
        } else if (value == NAND_CMD_READ1) {
            s->offset = 0x100;
            value = NAND_CMD_READ0;
        } else if (value == NAND_CMD_READ2) {
            s->offset = 1 << s->page_shift;
            value = NAND_CMD_READ0;
        }

        if (s->onfi && s->cmd == NAND_CMD_READ0 && value == NAND_CMD_LPREAD2) {
            s->iolen = 0;
            return;
        }

        s->cmd = value;

        if (s->cmd == NAND_CMD_READSTATUS ||
                s->cmd == NAND_CMD_PAGEPROGRAM2 ||
                s->cmd == NAND_CMD_BLOCKERASE1 ||
                s->cmd == NAND_CMD_BLOCKERASE2 ||
                s->cmd == NAND_CMD_NOSERIALREAD2 ||
                s->cmd == NAND_CMD_RANDOMREAD2 ||
                s->cmd == NAND_CMD_RESET) {
            nand_command(s);
        }

        if (s->cmd != NAND_CMD_RANDOMREAD2) {
            s->addrlen = 0;
        }
    }

    if (s->ale) {
        unsigned int shift = s->addrlen * 8;
        uint64_t mask = ~(0xffull << shift);
        uint64_t v = (uint64_t)value << shift;

        s->addr = (s->addr & mask) | v;
        s->addrlen ++;

        switch (s->addrlen) {
        case 1:
            if (s->cmd == NAND_CMD_READID ||
                    (s->onfi && s->cmd == NAND_CMD_PARAM)) {
                nand_command(s);
            }
            break;
        case 2: /* fix cache address as a byte address */
            if (!s->onfi) {
                s->addr <<= (s->buswidth - 1);
            }
            break;
        case 3:
            if (!(nand_flash_ids[s->chip_id].options & NAND_SAMSUNG_LP) &&
                    (s->cmd == NAND_CMD_READ0 ||
                     s->cmd == NAND_CMD_PAGEPROGRAM1)) {
                nand_command(s);
            }
            break;
        case 4:
            if ((nand_flash_ids[s->chip_id].options & NAND_SAMSUNG_LP) &&
                    nand_flash_ids[s->chip_id].size < 256 && /* 1Gb or less */
                    (s->cmd == NAND_CMD_READ0 ||
                     s->cmd == NAND_CMD_PAGEPROGRAM1)) {
                nand_command(s);
            }
            break;
        case 5:
            if ((nand_flash_ids[s->chip_id].options & NAND_SAMSUNG_LP) &&
                    nand_flash_ids[s->chip_id].size >= 256 && /* 2Gb or more */
                    (s->cmd == NAND_CMD_READ0 ||
                     s->cmd == NAND_CMD_PAGEPROGRAM1)) {
                nand_command(s);
            }
            break;
        default:
            break;
        }
    }

    if (!s->cle && !s->ale && s->cmd == NAND_CMD_PAGEPROGRAM1) {
        if (s->iolen < (1 << s->page_shift) +
                        (1 << s->oob_shift) + s->oob_margin) {
            for (i = s->buswidth; i--; value >>= 8) {
                s->io[s->iolen ++] = (uint8_t) (value & 0xff);
            }
        }
    } else if (!s->cle && !s->ale && s->cmd == NAND_CMD_COPYBACKPRG1) {
        if ((s->addr & ((1 << s->addr_shift) - 1)) <
                (1 << s->page_shift) + (1 << s->oob_shift) + s->oob_margin) {
            for (i = s->buswidth; i--; s->addr++, value >>= 8) {
                s->io[s->iolen + (s->addr & ((1 << s->addr_shift) - 1))] =
                    (uint8_t) (value & 0xff);
            }
        }
    }
}

uint32_t nand_getio(DeviceState *dev)
{
    int offset;
    uint32_t x = 0;
    NANDFlashState *s = NAND(dev);

    /* Allow sequential reading */
    if (!s->iolen && s->cmd == NAND_CMD_READ0) {
        offset = (int) (s->addr & ((1 << s->addr_shift) - 1)) + s->offset;
        s->offset = 0;

        s->blk_load(s, s->addr, offset);
        if (s->gnd)
            s->iolen = (1 << s->page_shift) - offset;
        else
            s->iolen = (1 << s->page_shift) + (1 << s->oob_shift) +
                        s->oob_margin - offset;
    }

    if (s->ce || s->iolen <= 0) {
        return 0;
    }

    for (offset = s->buswidth; offset--;) {
        x |= s->ioaddr[offset] << (offset << 3);
    }
    /* after receiving READ STATUS command all subsequent reads will
     * return the status register value until another command is issued
     */
    if (s->cmd != NAND_CMD_READSTATUS) {
        s->addr   += s->buswidth;
        s->ioaddr += s->buswidth;
        s->iolen  -= s->buswidth;
    }
    return x;
}

uint32_t nand_getbuswidth(DeviceState *dev)
{
    NANDFlashState *s = (NANDFlashState *) dev;
    return s->buswidth << 3;
}

DeviceState *nand_init(BlockBackend *blk, int manf_id, int chip_id)
{
    DeviceState *dev;

    if (nand_flash_ids[chip_id].size == 0) {
        hw_error("%s: Unsupported NAND chip ID.\n", __FUNCTION__);
    }
    dev = DEVICE(object_new(TYPE_NAND));
    qdev_prop_set_uint8(dev, "manufacturer_id", manf_id);
    qdev_prop_set_uint8(dev, "chip_id", chip_id);
    if (blk) {
        qdev_prop_set_drive(dev, "drive", blk, &error_fatal);
    }

    qdev_init_nofail(dev);
    return dev;
}

DeviceState *nand_init_onfi(BlockBackend *blk, char const *model)
{
    DeviceState *dev;

    dev = DEVICE(object_new(TYPE_NAND));
    qdev_prop_set_string(dev, "part_num", model);
    if (blk) {
        qdev_prop_set_drive(dev, "drive", blk, &error_fatal);
    }

    qdev_init_nofail(dev);
    return dev;
}

type_init(nand_register_types)

#else

/* Program a single page */
static void glue(nand_blk_write_, PAGE_SIZE)(NANDFlashState *s)
{
    uint64_t off, page, sector, soff;
    uint8_t iobuf[(PAGE_SECTORS + 2) * 0x200];
    if (PAGE(s->addr) >= s->pages)
        return;

    if (!s->blk) {
        mem_and(s->storage + PAGE_START(s->addr, s) +
                    (s->addr & PAGE_MASK) + s->offset, s->io, s->iolen);
    } else if (s->mem_oob) {
        sector = SECTOR(s->addr);
        off = (s->addr & PAGE_MASK) + s->offset;
        soff = SECTOR_OFFSET(s->addr);
        if (blk_pread(s->blk, sector << BDRV_SECTOR_BITS, iobuf,
                      PAGE_SECTORS << BDRV_SECTOR_BITS) < 0) {
            printf("%s: read error in sector %" PRIu64 "\n", __func__, sector);
            return;
        }

        mem_and(iobuf + (soff | off), s->io, MIN(s->iolen, PAGE_SIZE - off));
        if (off + s->iolen > PAGE_SIZE) {
            page = PAGE(s->addr);
            mem_and(s->storage + page * OOB_SIZE(s),
                            s->io + PAGE_SIZE - off,
                            MIN(OOB_SIZE(s), off + s->iolen - PAGE_SIZE));
        }

        if (blk_pwrite(s->blk, sector << BDRV_SECTOR_BITS, iobuf,
                       PAGE_SECTORS << BDRV_SECTOR_BITS, 0) < 0) {
            printf("%s: write error in sector %" PRIu64 "\n", __func__, sector);
        }
    } else {
        off = PAGE_START(s->addr, s) + (s->addr & PAGE_MASK) + s->offset;
        sector = off >> 9;
        soff = off & 0x1ff;
        if (blk_pread(s->blk, sector << BDRV_SECTOR_BITS, iobuf,
                      (PAGE_SECTORS + 2) << BDRV_SECTOR_BITS) < 0) {
            printf("%s: read error in sector %" PRIu64 "\n", __func__, sector);
            return;
        }

        mem_and(iobuf + soff, s->io, s->iolen);

        if (blk_pwrite(s->blk, sector << BDRV_SECTOR_BITS, iobuf,
                       (PAGE_SECTORS + 2) << BDRV_SECTOR_BITS, 0) < 0) {
            printf("%s: write error in sector %" PRIu64 "\n", __func__, sector);
        }
    }
    s->offset = 0;
}

/* Erase a single block */
static void glue(nand_blk_erase_, PAGE_SIZE)(NANDFlashState *s)
{
    uint64_t i, page, addr;
    uint8_t iobuf[0x200] = { [0 ... 0x1ff] = 0xff, };
    addr = s->addr & ~((1 << (ADDR_SHIFT + s->erase_shift)) - 1);

    if (PAGE(addr) >= s->pages) {
        return;
    }

    if (!s->blk) {
        memset(s->storage + PAGE_START(addr, s),
                        0xff, (PAGE_SIZE + OOB_SIZE(s)) << s->erase_shift);
    } else if (s->mem_oob) {
        memset(s->storage + PAGE(addr) * OOB_SIZE(s),
                        0xff, (OOB_SIZE(s)) << s->erase_shift);
        i = SECTOR(addr);
        page = SECTOR(addr + (1 << (ADDR_SHIFT + s->erase_shift)));
        for (; i < page; i ++)
            if (blk_pwrite(s->blk, i << BDRV_SECTOR_BITS, iobuf,
                           BDRV_SECTOR_SIZE, 0) < 0) {
                printf("%s: write error in sector %" PRIu64 "\n", __func__, i);
            }
    } else {
        addr = PAGE_START(addr, s);
        page = addr >> 9;
        if (blk_pread(s->blk, page << BDRV_SECTOR_BITS, iobuf,
                      BDRV_SECTOR_SIZE) < 0) {
            printf("%s: read error in sector %" PRIu64 "\n", __func__, page);
        }
        memset(iobuf + (addr & 0x1ff), 0xff, (~addr & 0x1ff) + 1);
        if (blk_pwrite(s->blk, page << BDRV_SECTOR_BITS, iobuf,
                       BDRV_SECTOR_SIZE, 0) < 0) {
            printf("%s: write error in sector %" PRIu64 "\n", __func__, page);
        }

        memset(iobuf, 0xff, 0x200);
        i = (addr & ~0x1ff) + 0x200;
        for (addr += ((PAGE_SIZE + OOB_SIZE(s)) << s->erase_shift) - 0x200;
                        i < addr; i += 0x200) {
            if (blk_pwrite(s->blk, i, iobuf, BDRV_SECTOR_SIZE, 0) < 0) {
                printf("%s: write error in sector %" PRIu64 "\n",
                       __func__, i >> 9);
            }
        }

        page = i >> 9;
        if (blk_pread(s->blk, page << BDRV_SECTOR_BITS, iobuf,
                      BDRV_SECTOR_SIZE) < 0) {
            printf("%s: read error in sector %" PRIu64 "\n", __func__, page);
        }
        memset(iobuf, 0xff, ((addr - 1) & 0x1ff) + 1);
        if (blk_pwrite(s->blk, page << BDRV_SECTOR_BITS, iobuf,
                       BDRV_SECTOR_SIZE, 0) < 0) {
            printf("%s: write error in sector %" PRIu64 "\n", __func__, page);
        }
    }
}

static void glue(nand_blk_load_, PAGE_SIZE)(NANDFlashState *s,
                uint64_t addr, int offset)
{
    if (PAGE(addr) >= s->pages) {
        return;
    }

    if (s->blk) {
        if (s->mem_oob) {
            if (blk_pread(s->blk, SECTOR(addr) << BDRV_SECTOR_BITS, s->io,
                          PAGE_SECTORS << BDRV_SECTOR_BITS) < 0) {
                printf("%s: read error in sector %" PRIu64 "\n",
                                __func__, SECTOR(addr));
            }
            memcpy(s->io + SECTOR_OFFSET(s->addr) + PAGE_SIZE,
                            s->storage + PAGE(s->addr) * OOB_SIZE(s),
                            OOB_SIZE(s));
            s->ioaddr = s->io + SECTOR_OFFSET(s->addr) + offset;
        } else {
            if (blk_pread(s->blk, PAGE_START(addr, s), s->io,
                          (PAGE_SECTORS + 2) << BDRV_SECTOR_BITS) < 0) {
                printf("%s: read error in sector %" PRIu64 "\n",
                                __func__, PAGE_START(addr, s) >> 9);
            }
            s->ioaddr = s->io + offset;
        }
    } else {
        memcpy(s->io, s->storage + PAGE_START(s->addr, s) +
                        offset, PAGE_SIZE + OOB_SIZE(s) - offset);
        s->ioaddr = s->io;
    }
}

static void glue(nand_init_, PAGE_SIZE)(NANDFlashState *s)
{
    s->oob_shift = PAGE_SHIFT - 5;
    s->pages = s->size >> PAGE_SHIFT;
    s->addr_shift = ADDR_SHIFT;
    if (s->onfi) {
        assert(PAGE_SHIFT >= 11);
        /* ONFI defines exact address format */
        assert(ADDR_SHIFT == 16);
    }

    s->blk_erase = glue(nand_blk_erase_, PAGE_SIZE);
    s->blk_write = glue(nand_blk_write_, PAGE_SIZE);
    s->blk_load = glue(nand_blk_load_, PAGE_SIZE);
}

# undef PAGE_SIZE
# undef PAGE_SHIFT
# undef PAGE_SECTORS
# undef ADDR_SHIFT
#endif	/* NAND_IO */
