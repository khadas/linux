/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_POLL_H
#define _EVL_POLL_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <evl/lock.h>
#include <evl/wait.h>
#include <evl/factory.h>
#include <uapi/evl/poll.h>

struct file;

#define EVL_POLLHEAD_INITIALIZER(__name) {				\
		.watchpoints = LIST_HEAD_INIT((__name).watchpoints),	\
		lock = __EVL_SPIN_LOCK_INITIALIZER((__name).lock),	\
	}

struct evl_poll_head {
	struct list_head watchpoints; /* struct poll_watchpoint */
	evl_spinlock_t lock;
};

struct evl_poll_node {
	struct list_head next;	/* in evl_fd->poll_nodes */
};

/*
 * The watchpoint struct linked to poll heads by drivers. This watches
 * files not elements, so that we can monitor any type of EVL files.
 */
struct evl_poll_watchpoint {
	unsigned int fd;
	int events_polled;
	int events_received;
	struct oob_poll_wait wait;
	struct evl_flag *flag;
	struct file *filp;
	struct evl_poll_head *head;
	void (*unwatch)(struct file *filp);
	struct evl_poll_node node;
};

static inline
void evl_init_poll_head(struct evl_poll_head *head)
{
	INIT_LIST_HEAD(&head->watchpoints);
	evl_spin_lock_init(&head->lock);
}

void evl_poll_watch(struct evl_poll_head *head,
		struct oob_poll_wait *wait,
		void (*unwait)(struct file *filp));

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

void evl_drop_poll_table(struct evl_thread *thread);

void evl_drop_watchpoints(struct list_head *drop_list);

#endif /* !_EVL_POLL_H */
