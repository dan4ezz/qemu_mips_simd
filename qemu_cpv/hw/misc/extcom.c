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

#include "qemu/osdep.h"
#include "chardev/char-fd.h"
#include "chardev/char-fe.h"
#include "hw/misc/extcom.h"
#include "qemu/fifo8.h"
#include "qemu/iov.h"

struct extcom_hdr {
    uint8_t type;
#define EXTCOM_TYPE_NONE        0
#define EXTCOM_TYPE_REGULAR     1
#define EXTCOM_TYPE_RESPONSE    2
    uint16_t payload;
};

typedef struct ExtCom {
    CharBackend chr;

    uint8_t in_buffer[4096 * 2];
    int in_pos;

    bool hdr_awaiting;
    struct extcom_hdr in_hdr;

    bool resp_awaiting;
    uint8_t *resp_buf;

    /* Received data handler */
    void (*handler)(void *opaque, void *buf, int size);
    void *opaque;

    Fifo8 rx_fifo;

    /*
     * We are using busy-loops while waiting for send or incoming data.
     * In order to not hang QEMU, timeouts are used. If timeout is fired
     * no further operation is possible - connection is broken:
     * on reads immediately return 0xff's, on writes do nothing.
     */
    bool broken;
} ExtCom;

static void extcom_gather_reset(ExtCom *s)
{
    s->in_pos = 0;
    s->hdr_awaiting = true;
}

static bool extcom_do_gather(ExtCom *s, int size)
{
    const uint8_t *buf;
    int len, actual_len;

    len = MIN(fifo8_num_used(&s->rx_fifo), size - s->in_pos);
    assert(len > 0);
    buf = fifo8_pop_buf(&s->rx_fifo, len, (uint32_t *)&actual_len);

    assert(s->in_pos + actual_len < sizeof(s->in_buffer));
    memcpy(s->in_buffer + s->in_pos, buf, actual_len);
    s->in_pos += actual_len;

    return s->in_pos >= size;
}

static int extcom_gather(ExtCom *s)
{
    int rc = 0;

    if (fifo8_is_empty(&s->rx_fifo)) {
        return -1;
    }

    while (!fifo8_is_empty(&s->rx_fifo)) {
        if (s->hdr_awaiting) {
            if (!extcom_do_gather(s, sizeof(s->in_hdr))) {
                continue;
            }

            /* The 'in_buffer' is expected to be properly aligned */
            s->in_hdr = *(struct extcom_hdr *)s->in_buffer;
            assert(s->in_hdr.payload > 0);
            s->in_pos = 0;
            s->hdr_awaiting = false;
        } else if (extcom_do_gather(s, s->in_hdr.payload)) {
            rc = s->in_pos;
            extcom_gather_reset(s);
            break;
        }
    }

    return rc;
}

static void extcom_pop_fifo(ExtCom *s)
{
    while (true) {
        int data_avail = extcom_gather(s);
        if (data_avail <= 0) {
            break;
        }
        if (s->in_hdr.type == EXTCOM_TYPE_RESPONSE) {
            assert(s->resp_awaiting);
            s->resp_awaiting = false;
            s->in_hdr.type = EXTCOM_TYPE_NONE;
            memcpy(s->resp_buf, s->in_buffer, data_avail);
        } else {
            s->handler(s->opaque, s->in_buffer, data_avail);
        }
    }
}

static bool is_broken(ExtCom *s, int64_t time)
{
    if ((qemu_clock_get_ns(QEMU_CLOCK_HOST) - time) > 1000000000) {
        fprintf(stderr, "extcom timeout occured, shutting down...\n");
        s->broken = true;
        return true;
    }
    return false;
}

static gboolean extcom_rx(QIOChannel *ioc, GIOCondition condition,
                                                            gpointer user_data)
{
    ExtCom *s = user_data;
    uint8_t rbuf[512];

    while (true) {
        int size = qio_channel_read(ioc, (char *)rbuf, sizeof(rbuf), NULL);
        uint8_t *buf = rbuf;

        if (size == QIO_CHANNEL_ERR_BLOCK) {
            return TRUE;
        } else if (size <= 0) {
            return FALSE;
        }

        while (size > 0) {
            int fsize = MIN(fifo8_num_free(&s->rx_fifo), size);

            fifo8_push_all(&s->rx_fifo, buf, fsize);
            extcom_pop_fifo(s);

            size -= fsize;
            buf += fsize;
        }
    }

    return TRUE;
}

static void extcom_push(ExtCom *s, struct iovec *iov, unsigned int iov_cnt)
{
    FDChardev *fs = FD_CHARDEV(s->chr.chr);
    QIOChannel *ioc = QIO_CHANNEL(fs->ioc_out);
    QIOChannel *ioc_in = QIO_CHANNEL(fs->ioc_in);
    int64_t time = qemu_clock_get_ns(QEMU_CLOCK_HOST);

    while (iov_cnt > 0) {
        int rc = qio_channel_writev(ioc, iov, iov_cnt, NULL);
        if (rc > 0) {
            iov_discard_front(&iov, &iov_cnt, rc);
        }
        /*
         * When we're here iothread is locked and watch callbacks do not work.
         * To prevent RX fifo overflow on both sides do its job right here.
         */
        extcom_rx(ioc_in, G_IO_IN, s);
        if (is_broken(s, time)) {
            fprintf(stderr, "extcom transmitter timed out\n");
            break;
        }
    }
}

static void extcom_do_send(ExtCom *s, int type, const uint8_t *buf, int size,
                                                            uint8_t *resp_buf)
{
    struct extcom_hdr hdr = {
        .type = type,
        .payload = size,
    };
    struct iovec iovec[2] = {
        {
            .iov_base = &hdr,
            .iov_len = sizeof(hdr),
        },
        {
            .iov_base = (void *)buf,
            .iov_len = size,
        },
    };

    assert(size + sizeof(hdr) < sizeof(s->in_buffer));

    if (resp_buf) {
        /* Read requests are supported only for CPU */
        assert(!s->resp_awaiting);
        s->resp_awaiting = true;
        s->resp_buf = resp_buf;
    }

    extcom_push(s, iovec, ARRAY_SIZE(iovec));

    if (resp_buf) {
        FDChardev *fs = FD_CHARDEV(s->chr.chr);
        QIOChannel *ioc = QIO_CHANNEL(fs->ioc_in);
        int64_t time = qemu_clock_get_ns(QEMU_CLOCK_HOST);

        while (s->resp_awaiting) {
            /*
             * When we're here iothread is locked and watch callback don't work.
             * So we can safely do its job right here.
             */
            extcom_rx(ioc, G_IO_IN, s);
            if (is_broken(s, time)) {
                fprintf(stderr, "extcom receiver timed out\n");
                break;
            }
        }
    }
}

ExtCom *extcom_init(const char *name,
            void (*handler)(void *opaque, void *buf, int size), void *opaque)
{
    FDChardev *fs;
    QIOChannel *ioc;
    ExtCom *s = g_malloc0(sizeof(ExtCom));

    s->handler = handler;
    s->opaque = opaque;
    s->resp_awaiting = false;
    s->broken = false;
    extcom_gather_reset(s);

    fifo8_create(&s->rx_fifo, 512);

    Chardev *fe = qemu_chr_find(name);
    assert(fe);
    qemu_chr_fe_init(&s->chr, fe, NULL);

    fs = FD_CHARDEV(s->chr.chr);
    ioc = QIO_CHANNEL(fs->ioc_out);
    qio_channel_set_blocking(ioc, false, NULL);

    ioc = QIO_CHANNEL(fs->ioc_in);
    qio_channel_set_blocking(ioc, false, NULL);
    qio_channel_add_watch(ioc, G_IO_IN, extcom_rx, s, NULL);

    return s;
}

void extcom_send(ExtCom *s, const uint8_t *buf, int size)
{
    if (s->broken) {
        return;
    }
    extcom_do_send(s, EXTCOM_TYPE_REGULAR, buf, size, NULL);
}

void extcom_send_with_ack(ExtCom *s, const uint8_t *buf, int size,
                                                            uint8_t *resp_buf)
{
    if (s->broken) {
        return;
    }
    extcom_do_send(s, EXTCOM_TYPE_REGULAR, buf, size, resp_buf);
}

void extcom_send_ack(ExtCom *s, const uint8_t *buf, int size)
{
    if (s->broken) {
        return;
    }
    extcom_do_send(s, EXTCOM_TYPE_RESPONSE, buf, size, NULL);
}

uint64_t extcom_win_read(ExtCom *s, uint8_t type, hwaddr addr, unsigned size,
                                                                        int n)
{
    ExtComWindowReq req = {
        .type = type,
        .address = addr,
        .size = size,
        .n = n,
    };
    uint64_t data;

    if (s->broken) {
        return -1;
    }
    extcom_send_with_ack(s, (void *)&req, sizeof(req), (void *)&data);
    return data;
}

void extcom_win_write(ExtCom *s, uint8_t type, hwaddr addr, unsigned size,
                                                        int n, uint64_t data)
{
    ExtComWindowReq req = {
        .type = type,
        .address = addr,
        .data = data,
        .size = size,
        .n = n,
    };

    if (s->broken) {
        return;
    }
    extcom_send(s, (void *)&req, sizeof(req));
}
