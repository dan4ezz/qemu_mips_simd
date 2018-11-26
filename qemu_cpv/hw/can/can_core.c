/*
 * CAN common CAN bus emulation support
 *
 * Copyright (c) 2013-2014 Jin Yang
 * Copyright (c) 2014-2018 Pavel Pisa
 *
 * Initial development supported by Google GSoC 2013 from RTEMS project slot
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu/osdep.h"
#include "chardev/char.h"
#include "qemu/sockets.h"
#include "qemu/error-report.h"
#include "qemu/help_option.h"
#include "hw/hw.h"
#include "can/can_emu.h"

#ifndef DEBUG_CAN
#define DEBUG_CAN 0
#endif /*DEBUG_CAN*/

static QTAILQ_HEAD(, CanBusState) can_buses =
    QTAILQ_HEAD_INITIALIZER(can_buses);

#define CAN_TIMER_HZ        250
#define CAN_TIMER_PERIOD    2500

static void can_apply_send(CanBusClientState *sender)
{
    CanBusClientState *peer;
    CanBusState *bus = sender->bus;
    qemu_can_frame *frame = sender->info->next_frame(sender);

    sender->info->update_state(sender, CAN_SUCC_TX);

    if (sender->info->loopback && sender->info->loopback(sender)) {
        sender->info->receive(sender, frame, 1);
        return;
    }

    QTAILQ_FOREACH(peer, &bus->clients, next) {
        if (peer->info->is_active && !peer->info->is_active(peer)) {
            continue;
        }

        if (peer == sender) {
            continue;
        }

        if (!peer->info->can_receive(peer)) {
            continue;
        }

        peer->info->receive(peer, frame, 1);
    }
}

static void can_apply_status_update(CanBusClientState *sender)
{
    CanBusClientState *peer;
    CanBusState *bus = sender->bus;

    if (sender->info->loopback && sender->info->loopback(sender)) {
        sender->info->update_state(sender, CAN_PREPARE_RX | CAN_PREPARE_TX);
        return;
    }

    sender->info->update_state(sender, CAN_PREPARE_TX);

    QTAILQ_FOREACH(peer, &bus->clients, next) {
        if (peer->info->is_active && !peer->info->is_active(peer)) {
            continue;
        }

        if (!peer->info->update_state) {
            continue;
        }

        if (peer == sender) {
            continue;
        }

        peer->info->update_state(peer, CAN_PREPARE_RX);
    }
}

static void can_apply_status_stall(CanBusState *bus)
{
    CanBusClientState *peer;

    QTAILQ_FOREACH(peer, &bus->clients, next) {
        if (peer->info->is_active && !peer->info->is_active(peer)) {
            continue;
        }

        if (peer->info->update_state) {
            peer->info->update_state(peer, CAN_CLIENT_STALL);
        }
    }
}

static CanBusClientState *can_get_client_prio(CanBusState *bus)
{
    CanBusClientState *top = NULL, *peer;
    qemu_can_frame *frame_top = NULL, *frame_cur;

    QTAILQ_FOREACH(peer, &bus->clients, next) {
        if (peer->info->is_active && !peer->info->is_active(peer)) {
            continue;
        }

        if (peer->info->next_frame == NULL) {
            continue;
        }

        frame_cur = peer->info->next_frame(peer);

        if (frame_cur == NULL) {
            continue;
        }

        if (!frame_top || frame_cur->can_id < frame_top->can_id) {
            top = peer;
            frame_top = frame_cur;
        }
    }

    return top;
}

/*
 * Find the highest priority frame among all clients and send it
 * in state-machine manner in order to emulate it closer to real hardware.
 */
static void can_process_tx(void *opaque)
{
    CanBusState *bus = opaque;
    CanBusClientState *top = NULL;

    assert(bus != NULL);

    top = can_get_client_prio(bus);

    if (top == NULL) {
        can_apply_status_stall(bus);
        return;
    }

    switch (bus->state) {
    case CAN_BUS_FREE:
        bus->state = CAN_BUS_TRANSFER_DELAY;
        can_apply_status_stall(bus);
        break;
    case CAN_BUS_TRANSFER_ACTIVE:
        bus->state = CAN_BUS_FREE;
        if (top == bus->last_sender) {
            can_apply_send(top);
            top = can_get_client_prio(bus);
        }
        can_apply_status_stall(bus);
        break;
    case CAN_BUS_TRANSFER_DELAY:
        /* Start transfer on next iteration */
        bus->state = CAN_BUS_TRANSFER_ACTIVE;
        can_apply_status_update(top);
        break;
    }

    bus->last_sender = top;
}

CanBusState *can_bus_find_by_name(const char *name, bool create_missing)
{
    CanBusState *bus;
    QEMUBH *bh;

    if (name == NULL) {
        name = "canbus0";
    }

    QTAILQ_FOREACH(bus, &can_buses, next) {
        if (!strcmp(bus->name, name)) {
            return bus;
        }
    }

    if (!create_missing) {
        return 0;
    }

    bus = g_malloc0(sizeof(*bus));
    if (bus == NULL) {
        return NULL;
    }

    QTAILQ_INIT(&bus->clients);

    bus->name = g_strdup(name);

    QTAILQ_INSERT_TAIL(&can_buses, bus, next);

    bh = qemu_bh_new(can_process_tx, bus);
    bus->ptimer = ptimer_init(bh, PTIMER_POLICY_DEFAULT);
    ptimer_set_freq(bus->ptimer, CAN_TIMER_HZ);

    ptimer_set_limit(bus->ptimer, CAN_TIMER_HZ, 1);
    ptimer_set_period(bus->ptimer, CAN_TIMER_PERIOD);
    ptimer_run(bus->ptimer, 0);

    bus->state = CAN_BUS_FREE;

    return bus;
}

int can_bus_insert_client(CanBusState *bus, CanBusClientState *client)
{
    client->bus = bus;
    QTAILQ_INSERT_TAIL(&bus->clients, client, next);
    return 0;
}

int can_bus_remove_client(CanBusClientState *client)
{
    CanBusState *bus = client->bus;
    if (bus == NULL) {
        return 0;
    }

    QTAILQ_REMOVE(&bus->clients, client, next);
    client->bus = NULL;
    return 1;
}

ssize_t can_bus_client_send(CanBusClientState *client,
             const struct qemu_can_frame *frames, size_t frames_cnt)
{
    int ret = 0;
    CanBusState *bus = client->bus;
    CanBusClientState *peer;
    if (bus == NULL) {
        return -1;
    }

    QTAILQ_FOREACH(peer, &bus->clients, next) {
        if (peer->info->can_receive(peer)) {
            if (peer == client) {
                /* No loopback support for now */
                continue;
            }
            if (peer->info->receive(peer, frames, frames_cnt) > 0) {
                ret = 1;
            }
        }
    }

    return ret;
}

int can_bus_client_set_filters(CanBusClientState *client,
             const struct qemu_can_filter *filters, size_t filters_cnt)
{
    return 0;
}

int can_bus_connect_to_host_device(CanBusState *bus, const char *name)
{
    if (can_bus_connect_to_host_variant == NULL) {
        error_report("CAN bus connect to host device "
                     "not supported on this system");
        exit(1);
    }
    return can_bus_connect_to_host_variant(bus, name);
}

QemuOptsList qemu_can_opts = {
    .name = "can",
    .implied_opt_name = "can",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_can_opts.head),
    .desc = {
        {
            .name = "canid",
            .type = QEMU_OPT_NUMBER,
        },
        {
            .name = "canbus",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};
