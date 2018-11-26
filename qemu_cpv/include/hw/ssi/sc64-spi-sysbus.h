/*
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

#ifndef _SPI_SYSBUS_H_
#define _SPI_SYSBUS_H_

#include <hw/sysbus.h>

void sysbus_spi_register(const char *name, hwaddr addr,
                                        qemu_irq irq, AddressSpace *dma_as,
                                        MemoryRegion *cs0, MemoryRegion *cs1);
#endif

