/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_SEM_H
#define _EVENLESS_SEM_H

#include <linux/ktime.h>
#include <evenless/wait.h>

struct evl_sem {
	unsigned int value;
	struct evl_wait_queue wait_queue;
};

void evl_init_sem(struct evl_sem *sem,
		unsigned int value);

void evl_destroy_sem(struct evl_sem *sem);

int evl_down_timeout(struct evl_sem *sem,
		ktime_t timeout);

int evl_down(struct evl_sem *sem);

int evl_trydown(struct evl_sem *sem);

void evl_up(struct evl_sem *sem);

#endif /* !_EVENLESS_SEM_H */
