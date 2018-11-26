/*
 *  QEMU model of the VDC35 VGA framebuffer.
 *
 *  Copyright (c) 2015 Dmitry Smagin <dmitry.s.smagin@gmail.com>
 *  Copyright (c) 2017 Denis Deryugin <deryugin.denis@gmail.com>
 *  Copyright (c) 2017 Aleksey Kuleshov <rndfax@yandex.ru>
 *
 *  Based on milkymist-vgafb.c
 *  Copyright (c) 2010-2012 Michael Walle <michael@walle.cc>
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
 *
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/display/vdc35-vgafb.h"
#include "hw/sysbus.h"
#include "trace.h"
#include "ui/console.h"
#include "framebuffer.h"
#include "ui/pixel_ops.h"
#include "qemu/error-report.h"
#include "string.h"
#include "sysemu/dma.h"

#define REG(ADDR) (((ADDR) >> 4) & 0x7F)

#define REG_TO_CHANNEL(reg)          (((reg) & 0x10) >> 4)

#define R_FB_CONFIG(port)           REG(0x1240 + (port) * 0x10)
# define R_FB_CONFIG_OUTPUT         (1 << 8)
# define R_FB_CONFIG_SWITCHPANEL    (1 << 9)
#define R_FB_ADDRESS(port)          REG(0x1260 + (port) * 0x10)
#define R_FB_STRIDE(port)           REG(0x1280 + (port) * 0x10)
#define R_FB_ORIGIN(port)           REG(0x12A0 + (port) * 0x10)
#define R_PANEL_CONFIG(port)        REG(0x13C0 + (port) * 0x10)
#define R_PANEL_HDISPLAY(port)      REG(0x1400 + (port) * 0x10)
#define R_PANEL_HSYNC(port)         REG(0x1420 + (port) * 0x10)
#define R_PANEL_VDISPLAY(port)      REG(0x1480 + (port) * 0x10)
#define R_PANEL_VSYNC(port)         REG(0x14A0 + (port) * 0x10)
#define R_CURSOR_CONFIG             REG(0x1520)
# define R_CURSOR_TYPE              0x3
# define R_CURSOR_DISPLAY_OFFT      4
# define R_CURSOR_DISPLAY           (1 << R_CURSOR_DISPLAY_OFFT)
#define R_CURSOR_ADDR               REG(0x1530)
#define R_CURSOR_X_OFFT             0
#define R_CURSOR_X_MASK             (0x7ff << R_CURSOR_X_OFFT)
#define R_CURSOR_Y_OFFT             16
#define R_CURSOR_Y_MASK             (0x7ff << R_CURSOR_Y_OFFT)
#define R_CURSOR_LOCATION           REG(0x1540)
#define R_CURSOR_BACKGROUND         REG(0x1550)
#define R_CURSOR_FOREGROUND         REG(0x1560)
#define R_DISPLAY_INTR              REG(0x1600)
#define R_DISPLAY_INTR_ENABLE       REG(0x1610)

#define VDC_CURSOR_WIDTH            32
#define VDC_CURSOR_HEIGHT           32

#define R_INTR_0                    (1 << 0)
#define R_INTR_1                    (1 << 8)
#define R_INTR(n)                   ((n) == 0 ? R_INTR_0 : R_INTR_1)

#define R_MAX            (R_DISPLAY_INTR_ENABLE + 1)

#define R_PANEL_CONFIG_DATA_ENABLE  (1 << 0)
#define R_PANEL_CONFIG_CLOCK_ENABLE (1 << 9)

#define VDC35_VGAFB(obj) \
    OBJECT_CHECK(Vdc35VgafbState, (obj), TYPE_VDC35_VGAFB)

#define CHANNEL_NUM 2

struct Vdc35VgafbState {
    SysBusDevice parent_obj;

    MemoryRegion regs_region;
    MemoryRegionSection fbsection[CHANNEL_NUM];
    QemuConsole *con[CHANNEL_NUM];

    uint32_t regs[R_MAX];

    qemu_irq irq;

    AddressSpace *dma_as;

    int32_t swap_type;
    int last_visible_con;
};

typedef struct Vdc35VgafbState Vdc35VgafbState;

void vdc35_set_swap_type(DeviceState *dev, int swap_type)
{
    Vdc35VgafbState *s = VDC35_VGAFB(dev);
    s->swap_type = swap_type;
}

static int vdc35_another_ch(int ch)
{
    if (ch < 0 || ch >= CHANNEL_NUM) {
        error_report(
            "vdc35_vgafb: Wrong display number: %d (should be 0 or 1)", ch);
        return 0;
    }
    return ch == 0 ? 1 : 0;
}

static int vdc35_is_mirror(Vdc35VgafbState *s, int ch)
{
    return s->regs[R_FB_CONFIG(ch)] & R_FB_CONFIG_SWITCHPANEL;
}

static int vdc35_is_independent(Vdc35VgafbState *s, int ch)
{
    return !vdc35_is_mirror(s, ch);
}

static int vdc35_get_cursor_owner(Vdc35VgafbState *s)
{
    return (s->regs[R_CURSOR_CONFIG] & R_CURSOR_DISPLAY) >>
            R_CURSOR_DISPLAY_OFFT;
}

static uint16_t swap_hword(uint16_t hw)
{
    return ((hw & 0x0ff) << 8) | ((hw >> 8) & 0x0ff);
}

static uint32_t swap_word(uint32_t w)
{
    return ((w & 0x0ff) << 24) | (((w >> 8) & 0x0ff) << 16) |
        (((w >> 16) & 0x0ff) << 8) | ((w >> 24) & 0x0ff);
}

static uint32_t vdc35_swap_value(uint32_t val, int swap)
{
    uint16_t lo, hi;

    switch (swap) {
    case 0:
    case 2:
        /* Do nothing */
        break;
    case 1:
        lo  = swap_hword((uint16_t)(val & 0xFFFF));
        hi  = swap_hword((uint16_t)(val >> 16));
        val = lo | (hi << 16);
        break;
    case 3:
        val = swap_word(val);
        break;
    default:
        printf("vdc35: Uknown swap type %d\n", swap);
    }

    return val;
}

static void vdc35_draw_line(int bpp, uint8_t *d, const uint8_t *s,
        int width, int swap)
{
    if (bpp == 3) { /* rgb565 */
        uint16_t rgb565;
        uint8_t r, g, b;

        while (width--) {
            rgb565 = vdc35_swap_value(lduw_he_p(s), swap);
            r = ((rgb565 >> 11) & 0x1f) << 3;
            g = ((rgb565 >>  5) & 0x3f) << 2;
            b = ((rgb565 >>  0) & 0x1f) << 3;
            *(uint32_t *)d = rgb_to_pixel32(r, g, b);
            d += 4;
            s += 2;
        }
    } else if (bpp == 4) { /* rgb888 is BGRA regardless of endianness */
        uint32_t rgb888;
        uint8_t r, g, b;

        while (width--) {
            rgb888 = vdc35_swap_value(ldl_he_p(s), swap);
            r = (rgb888 >> 16) & 0xff;
            g = (rgb888 >>  8) & 0xff;
            b = (rgb888 >>  0) & 0xff;
            *(uint32_t *)d = rgb_to_pixel32(r, g, b);
            d += 4;
            s += 4;
        }
    }
}

static void vdc35_draw_cursor(Vdc35VgafbState *s, int rows, int cols,
        DisplaySurface *surface, int *first, int *last)
{
    uint8_t alpha, r_pic, g_pic, b_pic, r_cur, g_cur, b_cur, r, g, b;
    int type, cur_x, cur_y, w, h;
    uint32_t *src, *dest;
    uint64_t tmp64;
    uint32_t tmp32;
    uint32_t lo, hi;

    type = s->regs[R_CURSOR_CONFIG] & R_CURSOR_TYPE;
    if (type == 0 || type > 2) {
        return;
    }

    cur_x = (s->regs[R_CURSOR_LOCATION] & R_CURSOR_X_MASK) >>
             R_CURSOR_X_OFFT;
    cur_y = (s->regs[R_CURSOR_LOCATION] & R_CURSOR_Y_MASK) >>
             R_CURSOR_Y_OFFT;
    src  = (void *) (uint64_t) s->regs[R_CURSOR_ADDR];
    dest = surface_data(surface) + 4 * (cols * cur_y + cur_x);

    switch (type) {
    case 1:
        /* Masked */
        for (w = 0; w < VDC_CURSOR_HEIGHT; w++) {
            dma_memory_read(s->dma_as, (hwaddr) src, &tmp64, 8);
            lo = vdc35_swap_value((uint32_t) (tmp64 & 0xFFFFFFFF),
                    s->swap_type);
            hi = vdc35_swap_value((uint32_t) (tmp64 >> 32),
                    s->swap_type);
            tmp64 = lo | (((uint64_t) hi) << 32);

            for (h = 0; h < VDC_CURSOR_WIDTH; h++) {
                switch (tmp64 & 0x3) {
                case 0:
                    *(dest + h) =
                        s->regs[R_CURSOR_BACKGROUND];
                    break;
                case 1:
                    *(dest + h) =
                        s->regs[R_CURSOR_FOREGROUND];
                    break;
                case 2:
                    /* Do nothing */
                    break;
                case 3:
                    /* Invert color */
                     *(dest + h) ^= 0xFFFFFF;
                    break;
                }

                tmp64 >>= 2;
            }

            dest += rows;
            src++;
        }

        break;
    case 2:
        /* A8R8G8B8 */
        for (w = 0; w < VDC_CURSOR_HEIGHT; w++) {
            for (h = 0; h < VDC_CURSOR_WIDTH; h++) {
                tmp32 = *(dest + h);
                r_pic = (tmp32 >> 16) & 0xFF;
                g_pic = (tmp32 >> 8) & 0xFF;
                b_pic = tmp32 & 0xFF;

                dma_memory_read(s->dma_as, (hwaddr) src, &tmp32, 4);
                tmp32 = vdc35_swap_value(tmp32, s->swap_type);
                alpha = (tmp32 >> 24) & 0xFF;
                r_cur = (tmp32 >> 16) & 0xFF;
                g_cur = (tmp32 >> 8) & 0xFF;
                b_cur = tmp32 & 0xFF;

                /* Apply alpha channel */
                r = (int)
                    (256 * r_pic + alpha * (r_cur - r_pic)) / 256;
                g = (int)
                    (256 * g_pic + alpha * (g_cur - g_pic)) / 256;
                b = (int)
                    (256 * b_pic + alpha * (b_cur - b_pic)) / 256;

                *(dest + h) = rgb_to_pixel32(r, g, b);
                src++;
            }

            dest += cols;
        }

        break;
    }

    *first = MIN(*first, cur_y);
    *last  = MAX(*last,  cur_y + VDC_CURSOR_HEIGHT);
}

static void vdc35_fill_black(Vdc35VgafbState *s, int ch)
{
    DisplaySurface *surface;
    uint32_t *dest32;
    int cols, rows;
    int i;
    /* If this channel is mirror it must use only that mirrored settings */
    int fb_ch = vdc35_is_independent(s, ch) ? ch : vdc35_another_ch(ch);

    cols = s->regs[R_PANEL_HDISPLAY(fb_ch)] & 0xFFF;
    rows = s->regs[R_PANEL_VDISPLAY(fb_ch)] & 0xFFF;

    surface = qemu_console_surface(s->con[ch]);

    /* Fill whole screen with black color */
    dest32 = surface_data(surface);
    for (i = 0; i < cols * rows; i++) {
        *(dest32 + i) = 0xff000000;
    }

    dpy_gfx_update(s->con[ch], 0, 0, cols, rows);
}

static void vdc35_update_channel(Vdc35VgafbState *s, int ch)
{
    DisplaySurface *surface;
    int dirty, src_width, reset_dirty;
    int first, last, cols, rows, k;
    uint8_t *src, *dest;
    hwaddr src_len;
    ram_addr_t addr;
    MemoryRegion *mem;
    MemoryRegionSection *mem_section;
    /* If this channel is mirror it must use only that mirrored settings */
    int fb_ch = vdc35_is_independent(s, ch) ? ch : vdc35_another_ch(ch);
    int stride = s->regs[R_FB_STRIDE(fb_ch)] & 0x3FFF;

    mem_section = &s->fbsection[fb_ch];
    mem = mem_section->mr;
    if (!mem) {
        return;
    }

    cols = s->regs[R_PANEL_HDISPLAY(fb_ch)] & 0xFFF;
    rows = s->regs[R_PANEL_VDISPLAY(fb_ch)] & 0xFFF;
    src_width = cols * 4;
    src_len = src_width * rows;

    memory_region_sync_dirty_bitmap(mem);
    reset_dirty = 0;

    surface = qemu_console_surface(s->con[ch]);

    first = -1;
    last = 0;
    addr = mem_section->offset_within_region;
    src = memory_region_get_ram_ptr(mem) + addr;
    dest = surface_data(surface);

    for (k = 0; k < rows; k++) {
        dirty = memory_region_get_dirty(mem, addr, src_width,
                                             DIRTY_MEMORY_VGA);
        if (dirty) {
            vdc35_draw_line(s->regs[R_FB_CONFIG(fb_ch)] & 0x7,
                    dest, src, cols, s->swap_type);
            if (first == -1) {
                first = k;
            }
            last = k;
        }
        addr += stride;
        src  += stride;
        dest += src_width;
    }

    if (vdc35_get_cursor_owner(s) == ch) {
        vdc35_draw_cursor(s, rows, cols, surface, &first, &last);
    }

    if (first >= 0) {
        reset_dirty = 1;
        dpy_gfx_update(s->con[ch], 0, first, cols, last - first + 1);
    }

    if (s->regs[R_DISPLAY_INTR_ENABLE] & R_INTR(ch)) {
        s->regs[R_DISPLAY_INTR] |= R_INTR(ch);
        qemu_irq_raise(s->irq);
    }

    if (reset_dirty) {
        memory_region_reset_dirty(mem, mem_section->offset_within_region,
                                    src_len, DIRTY_MEMORY_VGA);
    }
}

static void vgafb_update_display(void *opaque)
{
    Vdc35VgafbState *s = opaque;
    DisplaySurface *surface;
    int i;

    for (i = 0; i < CHANNEL_NUM; i++) {
        if (!qemu_console_is_visible(s->con[i])) {
            continue;
        }

        /* Redraw entire screen on console switching */
        if (s->last_visible_con != -1 && s->last_visible_con != i) {
            graphic_hw_invalidate(s->con[i]);
        }
        s->last_visible_con = i;

        surface = qemu_console_surface(s->con[i]);
        if (surface_bits_per_pixel(surface) != 32) {
            printf("Warning: QEMU console %d has %d bits per pixel, "
                    "but only 32 is supported.\n",
                    i, surface_bits_per_pixel(surface));
            continue;
        }

        if (s->regs[R_FB_CONFIG(i)] & R_FB_CONFIG_OUTPUT) {
            vdc35_update_channel(s, i);
        } else {
            vdc35_fill_black(s, i);
        }
    }
}

static void vdc35_reset_fb_addr(Vdc35VgafbState *s, int ch)
{
    MemoryRegionSection *mem_section = &s->fbsection[ch];
    int cols = s->regs[R_PANEL_HDISPLAY(ch)] & 0xFFF;
    int rows = s->regs[R_PANEL_VDISPLAY(ch)] & 0xFFF;
    int src_width = cols * 4;
    MemoryRegion *mem = mem_section->mr;
    if (mem) {
        memory_region_reset_dirty(mem, mem_section->offset_within_region,
                                    src_width * rows, DIRTY_MEMORY_VGA);
    }
    framebuffer_update_memory_section(mem_section,
                                s->dma_as->root,
                                s->regs[R_FB_ADDRESS(ch)],
                                rows,
                                src_width);
    mem = mem_section->mr;
    memory_region_set_dirty(mem, mem_section->offset_within_region,
        src_width * rows);
}

static void vgafb_invalidate_display(void *opaque)
{
    Vdc35VgafbState *s = opaque;
    int i;
    for (i = 0; i < CHANNEL_NUM; i++) {
        vdc35_reset_fb_addr(s, i);
    }
}

static void vgafb_resize(Vdc35VgafbState *s, int ch)
{
    int fb_ch = vdc35_is_independent(s, ch) ? ch : vdc35_another_ch(ch);
    qemu_console_resize(s->con[ch],
                                s->regs[R_PANEL_HDISPLAY(fb_ch)] & 0xFFF,
                                s->regs[R_PANEL_VDISPLAY(fb_ch)] & 0xFFF);
}

static uint64_t vgafb_read(void *opaque, hwaddr addr,
                           unsigned size)
{
    Vdc35VgafbState *s = opaque;
    uint32_t r = 0;
    uint32_t i = REG(addr);

    switch (i) {
    case R_FB_CONFIG(0):
    case R_FB_ADDRESS(0):
    case R_FB_STRIDE(0):
    case R_FB_ORIGIN(0):
    case R_PANEL_CONFIG(0):
    case R_PANEL_HDISPLAY(0):
    case R_PANEL_HSYNC(0):
    case R_PANEL_VDISPLAY(0):
    case R_PANEL_VSYNC(0):
    case R_FB_CONFIG(1):
    case R_FB_ADDRESS(1):
    case R_FB_STRIDE(1):
    case R_FB_ORIGIN(1):
    case R_PANEL_CONFIG(1):
    case R_PANEL_HDISPLAY(1):
    case R_PANEL_HSYNC(1):
    case R_PANEL_VDISPLAY(1):
    case R_PANEL_VSYNC(1):
    case R_CURSOR_CONFIG:
    case R_CURSOR_ADDR:
    case R_CURSOR_LOCATION:
    case R_CURSOR_BACKGROUND:
    case R_CURSOR_FOREGROUND:
    case R_DISPLAY_INTR_ENABLE:
        r = s->regs[i];
        break;
    case R_DISPLAY_INTR:
        r = s->regs[i];
        s->regs[i] = 0;
        qemu_irq_lower(s->irq);
        break;
    default:
        error_report("vdc35_vgafb: read access to unknown register 0x"
                TARGET_FMT_plx, addr);
        break;
    }

    return r;
}

static void vgafb_write(void *opaque, hwaddr addr, uint64_t value,
                        unsigned size)
{
    Vdc35VgafbState *s = opaque;
    uint32_t i = REG(addr);
    int ch;

    ch = REG_TO_CHANNEL(addr);

    switch (i) {
    case R_FB_CONFIG(0):
    case R_FB_CONFIG(1):
        s->regs[i] = value;
        vgafb_resize(s, ch);
        graphic_hw_invalidate(s->con[ch]);
        break;
    case R_FB_STRIDE(0):
    case R_FB_ORIGIN(0):
    case R_PANEL_CONFIG(0):
    case R_PANEL_HDISPLAY(0):
    case R_PANEL_HSYNC(0):
    case R_PANEL_VDISPLAY(0):
    case R_PANEL_VSYNC(0):
    case R_FB_STRIDE(1):
    case R_FB_ORIGIN(1):
    case R_PANEL_CONFIG(1):
    case R_PANEL_HDISPLAY(1):
    case R_PANEL_HSYNC(1):
    case R_PANEL_VDISPLAY(1):
    case R_PANEL_VSYNC(1):
        s->regs[i] = value;
        if (vdc35_is_independent(s, ch)) {
            vgafb_resize(s, ch);
        }
        if (vdc35_is_mirror(s, vdc35_another_ch(ch))) {
            vgafb_resize(s, vdc35_another_ch(ch));
        }
        break;
    case R_FB_ADDRESS(0):
    case R_FB_ADDRESS(1):
        if (value & 0x1f) {
            error_report("vdc35_vgafb: framebuffer base address have to "
                     "be 32 byte aligned");
            break;
        }
        s->regs[i] = value;
        graphic_hw_invalidate(s->con[ch]);
        break;
    case R_CURSOR_CONFIG:
    case R_CURSOR_ADDR:
    case R_CURSOR_LOCATION:
    case R_CURSOR_BACKGROUND:
    case R_CURSOR_FOREGROUND:
        s->regs[i] = value;
        graphic_hw_invalidate(s->con[ch]);
        break;
    case R_DISPLAY_INTR_ENABLE:
        s->regs[i] = value;
        break;
    default:
        error_report("vdc35_vgafb: write access to unknown register 0x"
                TARGET_FMT_plx, addr);
        break;
    }
}

static const MemoryRegionOps vgafb_mmio_ops = {
    .read = vgafb_read,
    .write = vgafb_write,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void vdc35_vgafb_reset(DeviceState *d)
{
    Vdc35VgafbState *s = VDC35_VGAFB(d);
    int i;

    for (i = 0; i < R_MAX; i++) {
        s->regs[i] = 0;
    }

    /* defaults */
    for (i = 0; i < CHANNEL_NUM; i++) {
        s->regs[R_FB_CONFIG(i)] = 4; /* RGB888, output disabled */
        s->regs[R_FB_STRIDE(i)] = 320 * 4; /* hres * bpp */
        s->regs[R_PANEL_CONFIG(i)] = 0x101; /* data & clock enable */
        s->regs[R_PANEL_HDISPLAY(i)] = 320 | (320 << 16); /* visible, total */
        s->regs[R_PANEL_VDISPLAY(i)] = 240 | (240 << 16); /* visible, total */
        vgafb_resize(s, i);
    }

    s->last_visible_con = -1;
}

static const GraphicHwOps vgafb_ops = {
    .invalidate  = vgafb_invalidate_display,
    .gfx_update  = vgafb_update_display,
};

static int vdc35_vgafb_init(SysBusDevice *dev)
{
    Vdc35VgafbState *s = VDC35_VGAFB(dev);
    int i;

    memory_region_init_io(&s->regs_region, OBJECT(s), &vgafb_mmio_ops, s,
            "vdc35-vgafb", 0x20000);
    sysbus_init_mmio(dev, &s->regs_region);

    for (i = 0; i < CHANNEL_NUM; i++) {
        s->con[i] = graphic_console_init(DEVICE(dev), 0, &vgafb_ops, s);
    }

    s->swap_type = 0;
    sysbus_init_irq(dev, &s->irq);
    return 0;
}

void sysbus_vdc35_register(hwaddr addr, qemu_irq irq, AddressSpace *dma_as)
{
    DeviceState *dev;
    Vdc35VgafbState *s;
    SysBusDevice *sd;

    dev = qdev_create(NULL, TYPE_VDC35_VGAFB);
    dev->id = TYPE_VDC35_VGAFB;

    s = VDC35_VGAFB(dev);
    s->dma_as = dma_as;
    qdev_init_nofail(dev);

    sd = SYS_BUS_DEVICE(dev);
    if (addr != (hwaddr)-1) {
        sysbus_mmio_map(sd, 0, addr);
    }
    sysbus_connect_irq(sd, 0, irq);
}

static int vgafb_post_load(void *opaque, int version_id)
{
    vgafb_invalidate_display(opaque);
    return 0;
}

static const VMStateDescription vmstate_vdc35_vgafb = {
    .name = TYPE_VDC35_VGAFB,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = vgafb_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, Vdc35VgafbState, R_MAX),
        VMSTATE_END_OF_LIST()
    }
};

static void vdc35_vgafb_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = vdc35_vgafb_init;
    dc->reset = vdc35_vgafb_reset;
    dc->vmsd = &vmstate_vdc35_vgafb;
}

static const TypeInfo vdc35_vgafb_info = {
    .name          = TYPE_VDC35_VGAFB,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Vdc35VgafbState),
    .class_init    = vdc35_vgafb_class_init,
};

static void vdc35_vgafb_register_types(void)
{
    type_register_static(&vdc35_vgafb_info);
}

type_init(vdc35_vgafb_register_types)
