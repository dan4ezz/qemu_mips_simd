/*
 * Copyright (C) 2018 Denis Deryugin <deryugin.denis@gmail.com>
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

#ifndef SC64_DEVICEBUS_H_
#define SC64_DEVICEBUS_H_

#define SC64_DEVICEBUS_DEV_NUM      8

BusState *sc64_devicebus_register(hwaddr base,
        hwaddr outer_base, void (*wdog_callback)(void *), void *wdog_arg,
        void (*bootmap_enable_callback)(MemoryRegion *, bool));

#endif /* SC64_DEVICEBUS_H_ */
