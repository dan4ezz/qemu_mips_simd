/*
 *  QEMU model of the VDC35 VGA framebuffer.
 *
 *  Copyright (c) 2017 Denis Deryugin <deryugin.denis@gmail.com>
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

#define TYPE_VDC35_VGAFB        "vdc35-vgafb"

extern void sysbus_vdc35_register(hwaddr addr, qemu_irq irq,
                                                        AddressSpace *dma_as);

extern void vdc35_set_swap_type(DeviceState *dev, int swap_type);
