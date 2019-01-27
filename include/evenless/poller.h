/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_POLLER_H
#define _EVENLESS_POLLER_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <evenless/synch.h>
#include <evenless/factory.h>
#include <uapi/evenless/poller.h>

#define EVL_POLLHEAD_INITIALIZER(__name) {				\
		.watchpoints = LIST_HEAD_INIT((__name).watchpoints),	\
		lock = __HARD_SPIN_LOCK_INITIALIZER(__name),		\
	}

struct evl_poll_head {
	struct list_head watchpoints; /* struct poll_watchpoint */
	hard_spinlock_t lock;
};

static inline
void evl_init_poll_head(struct evl_poll_head *head)
{
	INIT_LIST_HEAD(&head->watchpoints);
	hard_spin_lock_init(&head->lock);
}

void evl_poll_watch(struct evl_poll_head *head,
		    struct oob_poll_wait *wait);

void __evl_signal_poll_events(struct evl_poll_head *head,
			      int events);

void __evl_clear_poll_events(struct evl_poll_head *head,
			     int events);

static inline void
evl_signal_poll_events(struct evl_poll_head *head,
		       int events)
{
	/* Quick check. We'll redo under lock */
	if (!list_empty(&head->watchpoints))
		__evl_signal_poll_events(head, events);

}

static inline void
evl_clear_poll_events(struct evl_poll_head *head,
		      int events)
{
	/* Quick check. We'll redo under lock */
	if (!list_empty(&head->watchpoints))
		__evl_clear_poll_events(head, events);

}

#endif /* !_EVENLESS_POLLER_H */
