/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_STAX_H
#define _EVL_STAX_H

#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/irq_work.h>
#include <evl/wait.h>

struct evl_stax {
	atomic_t gate;
	struct evl_wait_queue oob_wait;
	wait_queue_head_t inband_wait;
	struct irq_work irq_work;
};

void evl_init_stax(struct evl_stax *stax);

void evl_destroy_stax(struct evl_stax *stax);

int evl_lock_stax(struct evl_stax *stax);

int evl_trylock_stax(struct evl_stax *stax);

void evl_unlock_stax(struct evl_stax *stax);

#endif /* !_EVL_STAX_H */
