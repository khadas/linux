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
	bool raised;
};

#define DEFINE_EVL_FLAG(__name)					\
	struct evl_flag __name = {				\
		.wait = EVL_WAIT_INITIALIZER((__name).wait),	\
		.raised = false,				\
	}

static inline void evl_init_flag(struct evl_flag *wf)
{
	wf->wait = (struct evl_wait_queue)EVL_WAIT_INITIALIZER(wf->wait);
	wf->raised = false;
}

static inline void evl_destroy_flag(struct evl_flag *wf)
{
	evl_destroy_wait(&wf->wait);
}

static inline bool evl_read_flag(struct evl_flag *wf)
{
	if (wf->raised) {
		wf->raised = false;
		return true;
	}

	return false;
}

static inline
int evl_wait_flag_timeout(struct evl_flag *wf,
			ktime_t timeout, enum evl_tmode timeout_mode)
{
	return evl_wait_event_timeout(&wf->wait, timeout,
				timeout_mode, evl_read_flag(wf));
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

static inline void evl_raise_flag_nosched(struct evl_flag *wf)
{
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);
	wf->raised = true;
	evl_flush_wait_locked(&wf->wait, T_BCAST);
	xnlock_put_irqrestore(&nklock, flags);
}

static inline void evl_raise_flag(struct evl_flag *wf)
{
	evl_raise_flag_nosched(wf);
	evl_schedule();
}

static inline void evl_pulse_flag_nosched(struct evl_flag *wf)
{
	evl_flush_wait(&wf->wait, T_BCAST);
}

static inline void evl_pulse_flag(struct evl_flag *wf)
{
	evl_pulse_flag_nosched(wf);
	evl_schedule();
}

static inline void evl_flush_flag_nosched(struct evl_flag *wf, int reason)
{
	evl_flush_wait(&wf->wait, reason);
}

static inline void evl_flush_flag(struct evl_flag *wf, int reason)
{
	evl_flush_flag_nosched(wf, reason);
	evl_schedule();
}

#endif /* _EVENLESS_FLAG_H */
