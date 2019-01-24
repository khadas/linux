/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_MUTEX_H
#define _EVENLESS_MUTEX_H

#include <linux/ktime.h>
#include <linux/atomic.h>
#include <evenless/synch.h>

struct evl_mutex {
	struct evl_syn wait_queue;
	atomic_t fastlock;
};

#endif /* !_EVENLESS_MUTEX_H */
