/*
 * QEMU DEC 21143 (Tulip) emulation -- sysbus adaptation (embedded Tulip)
 *
 * Copyright (C) 2012 Antony Pavlov, Mikhail Khropov
 * Copyright (C) 2013, 2014 Dmitry Smagin
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

#ifndef _TULIP_SYSBUS_H_
#define _TULIP_SYSBUS_H_

void sysbus_tulip_register(hwaddr addr, qemu_irq irq, AddressSpace *dma_as);

#endif

