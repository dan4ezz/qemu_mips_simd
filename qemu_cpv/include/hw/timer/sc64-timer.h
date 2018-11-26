/*
 * QEMU model of the SC64 timer block.
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

#ifndef SC64_TIMER_H_
#define SC64_TIMER_H_

#define TYPE_SC64_TIMER_V2 "sc64-timer-v2"

extern uint64_t sc64_timestamp_get(void *opaque);

#endif /* SC64_TIMER_H_ */
