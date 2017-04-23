/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2017 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_WAIT_H
#define _EVENLESS_WAIT_H

#include <evenless/synch.h>
#include <evenless/sched.h>
#include <evenless/poller.h>

/*
 * FIXME: general rework pending. Maybe merge that with synch.h. as
 * evenless/wait.h.
 */

struct evl_wait_queue {
	struct evl_syn wait;
};

#define EVL_WAIT_INITIALIZER(__name) {	\
		.wait = EVL_SYN_INITIALIZER((__name).wait, EVL_SYN_PRIO), \
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

static inline int evl_wait(struct evl_wait_queue *wq,
			   ktime_t timeout, enum evl_tmode timeout_mode)
{
	int ret;

	ret = evl_sleep_on_syn(&wq->wait, timeout, timeout_mode);
	if (ret & T_BREAK)
		return -EINTR;
	if (ret & T_TIMEO)
		return -ETIMEDOUT;
	if (ret & T_RMID)
		return -EIDRM;
	return 0;
}

static inline
int evl_wait_timeout(struct evl_wait_queue *wq,
		     ktime_t timeout, enum evl_tmode timeout_mode)
{
	return evl_wait(wq, timeout, timeout_mode);
}

/*
 * nklock held. Whine loudly if not atomic until we have eliminated
 * the superlock.
 */
#define evl_wait_event(__wq, __cond)					\
	({								\
		int __ret = 0;						\
		atomic_only();						\
		while (__ret == 0 && !(__cond))				\
			__ret = evl_wait(__wq,				\
					 EVL_INFINITE, EVL_REL); \
		__ret;							\
	})

/* nklock held. */
#define evl_wait_event_timeout(__wq, __cond, __timeout, __mode)		\
	({								\
		int __ret = 0;						\
		atomic_only();						\
		while (__ret == 0 && !(__cond))				\
			__ret = evl_wait_timeout(__wq, __timeout, __mode); \
		__ret;							\
	})

/* nklock held. */
static inline
struct evl_thread *evl_wait_head(struct evl_wait_queue *wq)
{
	atomic_only();
	return evl_syn_wait_head(&wq->wait);
}

static inline
bool evl_wait_active(struct evl_wait_queue *wq)
{
	return evl_syn_has_waiter(&wq->wait);
}

#define evl_wake_up_nosched(__wq)				\
	({							\
		struct evl_thread *__waiter;			\
		__waiter = evl_wake_up_syn(&(__wq)->wait);	\
		__waiter != NULL;				\
	})

#define evl_wake_up(__wq)				\
	({						\
		bool __ret = evl_wake_up_nosched(__wq);	\
		evl_schedule();				\
		__ret;					\
	})

#define evl_wake_up_all_nosched(__wq)		\
	evl_flush_syn(&(__wq)->wait, 0)

#define evl_wake_up_all(__wq)					\
	({							\
		bool __ret = evl_wake_up_all_nosched(__wq);	\
		evl_schedule();					\
		__ret;						\
	})

#define evl_flush_wait_nosched(__wq)		\
	evl_flush_syn(&(__wq)->wait, T_BREAK)

#define evl_flush_wait(__wq)				\
	({						\
		__ret = evl_flush_wait_nosched(__wq);	\
		evl_schedule();				\
		__ret;					\
	})

/* Does not reschedule(), complete with evl_schedule(). */
#define evl_wake_up_targeted(__wq, __waiter)			\
	evl_wake_up_targeted_syn(&(__wq)->wait, __waiter)

/* nklock held */
#define evl_for_each_waiter(__pos, __wq)		\
	evl_for_each_syn_waiter(__pos, &(__wq)->wait)

/* nklock held */
#define evl_for_each_waiter_safe(__pos, __tmp, __wq)			\
	evl_for_each_syn_waiter_safe(__pos, __tmp, &(__wq)->wait)

	struct evl_wait_flag {
		struct evl_wait_queue wq;
		struct evl_poll_head poll_head;
		bool signaled;
	};

#define DEFINE_EVL_WAIT_FLAG(__name)				\
	struct evl_wait_flag __name = {					\
		.wq = EVL_WAIT_INITIALIZER((__name).wq),		\
		.poll_head = EVL_POLLHEAD_INITIALIZER((__name).poll_head), \
		.signaled = false,					\
	}

static inline void evl_init_flag(struct evl_wait_flag *wf)
{
	evl_init_wait(&wf->wq);
	evl_init_poll_head(&wf->poll_head);
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

	if (!wf->signaled)
		ret = evl_wait_event_timeout(&wf->wq, wf->signaled,
					     timeout, timeout_mode);
	if (ret == 0) {
		wf->signaled = false;
		evl_clear_poll_events(&wf->poll_head, POLLIN|POLLRDNORM);
	}

	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}

static inline int evl_wait_flag(struct evl_wait_flag *wf)
{
	return evl_wait_flag_timeout(wf, EVL_INFINITE, EVL_REL);
}

static inline
struct evl_thread *evl_wait_flag_head(struct evl_wait_flag *wf)
{
	return evl_wait_head(&wf->wq);
}

static inline bool evl_raise_flag(struct evl_wait_flag *wf)
{
	unsigned long flags;
	bool ret;

	xnlock_get_irqsave(&nklock, flags);

	wf->signaled = true;
	evl_signal_poll_events(&wf->poll_head, POLLIN|POLLRDNORM);
	ret = evl_wake_up(&wf->wq);

	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}

#endif /* _EVENLESS_WAIT_H */
