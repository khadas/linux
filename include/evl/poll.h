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
#include <evl/poll_head.h>
#include <evl/wait.h>
#include <evl/factory.h>
#include <uapi/evl/types.h>
#include <uapi/evl/poll-abi.h>

struct file;

struct evl_poll_node {
	struct list_head next;	/* in watchpoint->poll_nodes */
};

/*
 * The watchpoint struct linked to poll heads by drivers. This watches
 * files not elements, so that we can monitor any type of EVL files.
 */
struct evl_poll_watchpoint {
	unsigned int fd;
	int events_polled;
	union evl_value pollval;
	struct oob_poll_wait wait;
	struct evl_flag *flag;
	struct file *filp;
	struct evl_poll_node node;
};

void evl_drop_poll_table(struct evl_thread *thread);

void evl_drop_watchpoints(struct list_head *drop_list);

#endif /* !_EVL_POLL_H */
