/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SEM_H
#define _EVL_SEM_H

#include <linux/ktime.h>
#include <evl/wait.h>

struct evl_ksem {
	unsigned int value;
	struct evl_wait_queue wait_queue;
};

void evl_init_ksem(struct evl_ksem *ksem,
		unsigned int value);

void evl_destroy_ksem(struct evl_ksem *ksem);

int evl_down_timeout(struct evl_ksem *ksem,
		ktime_t timeout);

int evl_down(struct evl_ksem *ksem);

int evl_trydown(struct evl_ksem *ksem);

void evl_up(struct evl_ksem *ksem);

#endif /* !_EVL_SEM_H */
