/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2017 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_FLAG_H
#define _EVENLESS_FLAG_H

#include <evenless/wait.h>
#include <evenless/sched.h>

struct evl_flag {
	struct evl_wait_queue wait;
	bool signaled;
};

#define DEFINE_EVL_FLAG(__name)					\
	struct evl_flag __name = {				\
		.wait = EVL_WAIT_INITIALIZER((__name).wait),	\
		.signaled = false,				\
	}

static inline void evl_init_flag(struct evl_flag *wf)
{
	wf->wait = (struct evl_wait_queue)EVL_WAIT_INITIALIZER(wf->wait);
	wf->signaled = false;
}

static inline void evl_destroy_flag(struct evl_flag *wf)
{
	evl_destroy_wait(&wf->wait);
}

static inline
int evl_wait_flag_timeout(struct evl_flag *wf,
			ktime_t timeout, enum evl_tmode timeout_mode)
{
	unsigned long flags;
	int ret = 0;

	xnlock_get_irqsave(&nklock, flags);

	while (!wf->signaled) {
		ret = evl_wait_timeout(&wf->wait, timeout, timeout_mode);
		if (ret & T_BREAK)
			ret = -EINTR;
		if (ret & T_TIMEO)
			ret = -ETIMEDOUT;
		if (ret & T_RMID)
			ret = -EIDRM;
		if (ret)
			break;
	}

	if (ret == 0)
		wf->signaled = false;

	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}

static inline int evl_wait_flag(struct evl_flag *wf)
{
	return evl_wait_flag_timeout(wf, EVL_INFINITE, EVL_REL);
}

static inline			/* nklock held. */
struct evl_thread *evl_wait_flag_head(struct evl_flag *wf)
{
	return evl_wait_head(&wf->wait);
}

static inline bool evl_raise_flag(struct evl_flag *wf)
{
	struct evl_thread *waiter;
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);

	wf->signaled = true;
	waiter = evl_wake_up_head(&wf->wait);
	evl_schedule();

	xnlock_put_irqrestore(&nklock, flags);

	return waiter != NULL;
}

#endif /* _EVENLESS_FLAG_H */
