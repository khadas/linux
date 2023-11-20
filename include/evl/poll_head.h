/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_POLL_HEAD_H
#define _EVL_POLL_HEAD_H

#include <linux/list.h>
#include <linux/spinlock.h>

struct oob_poll_wait;

#define EVL_POLLHEAD_INITIALIZER(__name) {				\
		.watchpoints = LIST_HEAD_INIT((__name).watchpoints),	\
		.lock = __HARD_SPIN_LOCK_INITIALIZER((__name).lock),	\
	}

struct evl_poll_head {
	struct list_head watchpoints; /* struct evl_poll_watchpoint */
	hard_spinlock_t lock;
};

static inline
void evl_init_poll_head(struct evl_poll_head *head)
{
	INIT_LIST_HEAD(&head->watchpoints);
	raw_spin_lock_init(&head->lock);
}

void evl_poll_watch(struct evl_poll_head *head,
		struct oob_poll_wait *wait,
		void (*unwatch)(struct evl_poll_head *head));

void __evl_signal_poll_events(struct evl_poll_head *head,
			      int events);

static inline void
evl_signal_poll_events(struct evl_poll_head *head,
		       int events)
{
	/* Quick check. We'll redo under lock */
	if (!list_empty(&head->watchpoints))
		__evl_signal_poll_events(head, events);
}

#endif /* !_EVL_POLL_HEAD_H */
