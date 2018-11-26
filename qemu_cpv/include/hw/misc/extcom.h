/*
 * Copyright (c) 2018 Aleksey Kuleshov <rndfax@yandex.ru>
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

#ifndef _EXTCOM_H_
#define _EXTCOM_H_

#include "exec/hwaddr.h"
#include "exec/memory.h"

typedef struct ExtCom ExtCom;

ExtCom *extcom_init(const char *name,
            void (*handler)(void *opaque, void *buf, int size), void *opaque);

void extcom_send(ExtCom *s, const uint8_t *buf, int size);
void extcom_send_with_ack(ExtCom *s, const uint8_t *buf,
                                                int size, uint8_t *resp_buf);
void extcom_send_ack(ExtCom *s, const uint8_t *buf, int size);

typedef struct {
    void *owner;
    MemoryRegion mr;
    int n;
} ExtComWindow;

typedef struct {
    uint8_t type;
    uint64_t address;
    uint64_t data;
    uint8_t size;
    uint8_t n;
} ExtComWindowReq;

uint64_t extcom_win_read(ExtCom *s, uint8_t type, hwaddr addr, unsigned size,
                                                                        int n);
void extcom_win_write(ExtCom *s, uint8_t type, hwaddr addr, unsigned size,
                                                        int n, uint64_t data);

#endif
