/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_WAIT_H
#define _EVENLESS_WAIT_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <evenless/list.h>
#include <evenless/timer.h>
#include <evenless/clock.h>
#include <evenless/thread.h>
#include <uapi/evenless/thread.h>

#define EVL_WAIT_FIFO    0
#define EVL_WAIT_PRIO    BIT(0)

struct evl_wait_queue {
	int flags;
	struct list_head wait_list;
	struct evl_clock *clock;
	struct evl_wait_channel wchan;
};

#define EVL_WAIT_INITIALIZER(__name) {					\
		.flags = EVL_WAIT_PRIO,					\
		.wait_list = LIST_HEAD_INIT((__name).wait_list), 	\
		.clock = &evl_mono_clock,				\
		.wchan = {						\
			.abort_wait = evl_abort_wait,			\
			.reorder_wait = evl_reorder_wait,		\
			.lock = __HARD_SPIN_LOCK_INITIALIZER((__name).wchan.lock), \
		},							\
	}

#define evl_head_waiter(__wq)				\
	list_first_entry_or_null(&(__wq)->wait_list,	\
				struct evl_thread, wait_next)

#define evl_for_each_waiter(__pos, __wq)	\
	list_for_each_entry(__pos, &(__wq)->wait_list, wait_next)

#define evl_for_each_waiter_safe(__pos, __tmp, __wq)		\
	list_for_each_entry_safe(__pos, __tmp, &(__wq)->wait_list, wait_next)

static inline bool evl_wait_active(struct evl_wait_queue *wq)
{
	return !list_empty(&wq->wait_list);
}

static inline
struct evl_thread *evl_wait_head(struct evl_wait_queue *wq)
{
	return list_first_entry_or_null(&wq->wait_list,
					struct evl_thread, wait_next);
}

void evl_init_wait(struct evl_wait_queue *wq,
		struct evl_clock *clock,
		int flags);

void evl_destroy_wait(struct evl_wait_queue *wq);

int __must_check evl_wait_timeout(struct evl_wait_queue *wq,
				ktime_t timeout,
				enum evl_tmode timeout_mode);

static inline int evl_wait(struct evl_wait_queue *wq)
{
	return evl_wait_timeout(wq, EVL_INFINITE, EVL_REL);
}

struct evl_thread *evl_wake_up(struct evl_wait_queue *wq,
			struct evl_thread *waiter);

static inline
struct evl_thread *evl_wake_up_head(struct evl_wait_queue *wq)
{
	return evl_wake_up(wq, NULL);
}

void evl_flush_wait(struct evl_wait_queue *wq, int reason);

void evl_abort_wait(struct evl_thread *thread);

void evl_reorder_wait(struct evl_thread *thread);

#endif /* !_EVENLESS_WAIT_H_ */
