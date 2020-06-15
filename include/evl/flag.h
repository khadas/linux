/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2017 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_FLAG_H
#define _EVL_FLAG_H

#include <evl/wait.h>
#include <evl/sched.h>

struct evl_flag {
	struct evl_wait_queue wait;
	bool raised;
};

#define EVL_FLAG_INITIALIZER(__name) {				\
		.wait = EVL_WAIT_INITIALIZER((__name).wait),	\
		.raised = false,				\
	}

#define DEFINE_EVL_FLAG(__name)					\
	struct evl_flag __name = EVL_FLAG_INITIALIZER(__name)

static inline void evl_init_flag(struct evl_flag *wf)
{
	evl_init_wait(&wf->wait, &evl_mono_clock, EVL_WAIT_PRIO);
	wf->raised = false;
}

static inline void evl_destroy_flag(struct evl_flag *wf)
{
	evl_destroy_wait(&wf->wait);
}

static inline bool evl_peek_flag(struct evl_flag *wf)
{
	smp_wmb();
	return wf->raised;
}

static inline bool evl_read_flag(struct evl_flag *wf)
{
	if (wf->raised) {
		wf->raised = false;
		return true;
	}

	return false;
}

#define evl_lock_flag(__wf, __flags)		\
	evl_spin_lock_irqsave(&(__wf)->wait.lock, __flags)

#define evl_unlock_flag(__wf, __flags)		\
	evl_spin_unlock_irqrestore(&(__wf)->wait.lock, __flags)

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

/* wf->wait.lock held, irqs off */
static inline struct evl_thread *evl_wait_flag_head(struct evl_flag *wf)
{
	return evl_wait_head(&wf->wait);
}

/* wf->wait.lock held, irqs off */
static inline void evl_raise_flag_locked(struct evl_flag *wf)
{
	wf->raised = true;
	evl_flush_wait_locked(&wf->wait, 0);
}

static inline void evl_raise_flag_nosched(struct evl_flag *wf)
{
	unsigned long flags;

	evl_lock_flag(wf, flags);
	evl_raise_flag_locked(wf);
	evl_unlock_flag(wf, flags);
}

static inline void evl_raise_flag(struct evl_flag *wf)
{
	evl_raise_flag_nosched(wf);
	evl_schedule();
}

/* wf->wait.lock held, irqs off */
static inline void evl_flush_flag_locked(struct evl_flag *wf, int reason)
{
	evl_flush_wait_locked(&wf->wait, reason);
}

static inline void evl_flush_flag_nosched(struct evl_flag *wf, int reason)
{
	unsigned long flags;

	evl_lock_flag(wf, flags);
	evl_flush_flag_locked(wf, reason);
	evl_unlock_flag(wf, flags);
}

static inline void evl_flush_flag(struct evl_flag *wf, int reason)
{
	evl_flush_flag_nosched(wf, reason);
	evl_schedule();
}

/* wf->wait.lock held, irqs off */
static inline void evl_pulse_flag_locked(struct evl_flag *wf)
{
	evl_flush_flag_locked(wf, T_BCAST);
}

static inline void evl_pulse_flag_nosched(struct evl_flag *wf)
{
	evl_flush_flag_nosched(wf, T_BCAST);
}

static inline void evl_pulse_flag(struct evl_flag *wf)
{
	evl_flush_flag(wf, T_BCAST);
}

static inline void evl_clear_flag(struct evl_flag *wf)
{
	wf->raised = false;
}

#endif /* _EVL_FLAG_H */
