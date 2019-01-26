/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2017 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_WAIT_H
#define _EVENLESS_WAIT_H

#include <linux/spinlock.h>
#include <evenless/synch.h>
#include <evenless/sched.h>

/*
 * FIXME: general rework pending. Maybe merge that with synch.h. as
 * evenless/wait.h, provided the superlock is gone and synch.c
 * serializes with a local per-synch lock, which would allow to turn
 * evl_syn into evl_wait_queue.
 */

struct evl_wait_queue {
	struct evl_syn wait;
	hard_spinlock_t lock;
};

#define EVL_WAIT_INITIALIZER(__name) {	\
		.wait = EVL_SYN_INITIALIZER((__name).wait, EVL_SYN_PRIO), \
		.lock = __HARD_SPIN_LOCK_INITIALIZER((__name).lock),	  \
	}

#define DEFINE_EVL_WAIT(__name)	\
	struct evl_wait_queue __name = EVL_WAIT_INITIALIZER(__name)

#define DEFINE_EVL_WAIT_ONSTACK(__name)  DEFINE_EVL_WAIT(__name)

static inline void evl_init_wait(struct evl_wait_queue *wq)
{
	*wq = (struct evl_wait_queue)EVL_WAIT_INITIALIZER(*wq);
}

static inline void evl_destroy_wait(struct evl_wait_queue *wq)
{
	evl_destroy_syn(&wq->wait);
}

struct evl_wait_flag {
	struct evl_wait_queue wq;
	bool signaled;
};

#define DEFINE_EVL_WAIT_FLAG(__name)					\
	struct evl_wait_flag __name = {					\
		.wq = EVL_WAIT_INITIALIZER((__name).wq),		\
		.signaled = false,					\
	}

static inline void evl_init_flag(struct evl_wait_flag *wf)
{
	evl_init_wait(&wf->wq);
	wf->signaled = false;
}

static inline void evl_destroy_flag(struct evl_wait_flag *wf)
{
	evl_destroy_wait(&wf->wq);
}

static inline int evl_wait_flag_timeout(struct evl_wait_flag *wf,
					ktime_t timeout, enum evl_tmode timeout_mode)
{
	unsigned long flags;
	int ret = 0;

	xnlock_get_irqsave(&nklock, flags);

	while (!wf->signaled) {
		ret = evl_sleep_on_syn(&wf->wq.wait, timeout, timeout_mode);
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

static inline int evl_wait_flag(struct evl_wait_flag *wf)
{
	return evl_wait_flag_timeout(wf, EVL_INFINITE, EVL_REL);
}

static inline			/* nklock held. */
struct evl_thread *evl_wait_flag_head(struct evl_wait_flag *wf)
{
	return evl_syn_wait_head(&wf->wq.wait);
}

static inline bool evl_raise_flag(struct evl_wait_flag *wf)
{
	struct evl_thread *waiter;
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);

	wf->signaled = true;
	waiter = evl_wake_up_syn(&wf->wq.wait);
	evl_schedule();

	xnlock_put_irqrestore(&nklock, flags);

	return waiter != NULL;
}

#endif /* _EVENLESS_WAIT_H */
