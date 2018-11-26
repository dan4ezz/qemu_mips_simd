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

#ifndef _SC64_GPIO_H_
#define _SC64_GPIO_H_

#include <hw/sysbus.h>

extern DeviceState *sc64_gpio_register(hwaddr gpio_base, hwaddr outer_base,
        qemu_irq irq_a2, qemu_irq irq_a3,
        qemu_irq irq_b2, qemu_irq irq_b3,
        qemu_irq irq_outer0, qemu_irq irq_outer1);

#endif
