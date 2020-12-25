/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_OBSERVABLE_H
#define _EVL_OBSERVABLE_H

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <evl/factory.h>
#include <evl/wait.h>
#include <evl/poll.h>
#include <uapi/evl/observable.h>

struct file;

struct evl_observable {
	struct evl_element element;
	struct list_head observers; 	/* struct evl_observer */
	struct list_head flush_list; 	/* struct evl_observer */
	struct evl_wait_queue oob_wait;
	wait_queue_head_t inband_wait;
	struct evl_poll_head poll_head;
	struct irq_work wake_irqwork;
	struct irq_work flush_irqwork;
	hard_spinlock_t lock;		/* guards observers and flush_list */
	u32 serial_counter;
	int writable_observers;
};

void evl_drop_subscriptions(struct evl_subscriber *subscriber);

struct evl_observable *evl_alloc_observable(const char __user *u_name,
					int clone_flags);

void evl_flush_observable(struct evl_observable *observable);

bool evl_send_observable(struct evl_observable *observable, int tag,
			union evl_value details);

ssize_t evl_read_observable(struct evl_observable *observable,
			char __user *u_buf, size_t count, bool wait);

ssize_t evl_write_observable(struct evl_observable *observable,
			const char __user *u_buf, size_t count);

__poll_t evl_oob_poll_observable(struct evl_observable *observable,
				struct oob_poll_wait *wait);

__poll_t evl_poll_observable(struct evl_observable *observable,
			struct file *filp, poll_table *pt);

long evl_ioctl_observable(struct evl_observable *observable,
			unsigned int cmd, unsigned long arg);

#endif /* !_EVL_OBSERVABLE_H */
