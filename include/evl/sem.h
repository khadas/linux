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
	struct evl_wait_queue wait;
	unsigned int value;
};

#define EVL_KSEM_INITIALIZER(__name, __value) {			\
		.wait = EVL_WAIT_INITIALIZER((__name).wait),	\
		.value = (__value),				\
	}

#define DEFINE_EVL_KSEM(__name, __value)			\
	struct evl_ksem __name = EVL_KSEM_INITIALIZER(__name, __value)

static inline void evl_init_ksem(struct evl_ksem *ksem, unsigned int value)
{
	*ksem = (struct evl_ksem)EVL_KSEM_INITIALIZER(*ksem, value);
}

static inline void evl_destroy_ksem(struct evl_ksem *ksem)
{
	evl_destroy_wait(&ksem->wait);
}

int evl_down_timeout(struct evl_ksem *ksem,
		ktime_t timeout);

int evl_down(struct evl_ksem *ksem);

int evl_trydown(struct evl_ksem *ksem);

void evl_up(struct evl_ksem *ksem);

#endif /* !_EVL_SEM_H */
