/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_DEVICES_GPIO_H
#define _EVL_DEVICES_GPIO_H

#include <evl/device.h>
#include <uapi/evl/devices/gpio.h>

#ifdef CONFIG_EVL

#include <evl/poll.h>
#include <evl/wait.h>
#include <evl/irq.h>

struct lineevent_oob_state {
	struct evl_file efile;
	struct evl_poll_head poll_head;
	struct evl_wait_queue wait;
	hard_spinlock_t lock;
};

#else

struct lineevent_oob_state {
	struct evl_file efile;
};

#endif

struct linehandle_oob_state {
	struct evl_file efile;
};

#endif /* !_EVL_DEVICES_GPIO_H */
