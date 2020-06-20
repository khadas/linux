/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_WAIT_H
#define _EVL_WAIT_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <evl/lock.h>
#include <evl/list.h>
#include <evl/timer.h>
#include <evl/clock.h>
#include <uapi/evl/thread.h>

#define EVL_WAIT_FIFO    0
#define EVL_WAIT_PRIO    BIT(0)

struct evl_wait_channel {
	int (*reorder_wait)(struct evl_thread *waiter,
			struct evl_thread *originator);
	int (*follow_depend)(struct evl_wait_channel *wchan,
			struct evl_thread *originator);
	struct list_head wait_list;
};

struct evl_wait_queue {
	int flags;
	struct evl_clock *clock;
	struct evl_wait_channel wchan;
	evl_spinlock_t lock;
};

#define EVL_WAIT_INITIALIZER(__name) {					\
		.flags = EVL_WAIT_PRIO,					\
		.clock = &evl_mono_clock,				\
		.lock = __EVL_SPIN_LOCK_INITIALIZER((__name).lock),	\
		.wchan = {						\
			.reorder_wait = evl_reorder_wait,		\
			.follow_depend = evl_follow_wait_depend,	\
			.wait_list = LIST_HEAD_INIT((__name).wchan.wait_list), \
		},							\
	}

#define evl_head_waiter(__wq)						\
	list_first_entry_or_null(&(__wq)->wchan.wait_list,		\
				struct evl_thread, wait_next)

#define evl_for_each_waiter(__pos, __wq)				\
	list_for_each_entry(__pos, &(__wq)->wchan.wait_list, wait_next)

#define evl_for_each_waiter_safe(__pos, __tmp, __wq)			\
	list_for_each_entry_safe(__pos, __tmp,				\
				&(__wq)->wchan.wait_list, wait_next)

#define evl_wait_event_timeout(__wq, __timeout, __timeout_mode, __cond)	\
({									\
	int __ret = 0, __bcast;						\
	unsigned long __flags;						\
									\
	evl_spin_lock_irqsave(&(__wq)->lock, __flags);			\
	if (!(__cond)) {						\
		if (timeout_nonblock(__timeout))			\
			__ret = -EAGAIN;				\
		else {							\
			do {						\
				evl_add_wait_queue(__wq, __timeout,	\
						__timeout_mode);	\
				evl_spin_unlock_irqrestore(&(__wq)->lock, __flags); \
				__ret = evl_wait_schedule(__wq);	\
				__bcast = evl_current()->info & T_BCAST; \
				evl_spin_lock_irqsave(&(__wq)->lock, __flags); \
			} while (!__ret && !__bcast && !(__cond));	\
		}							\
	}								\
	evl_spin_unlock_irqrestore(&(__wq)->lock, __flags);		\
	__ret;								\
})

#define evl_wait_event(__wq, __cond)					\
	evl_wait_event_timeout(__wq, EVL_INFINITE, EVL_REL, __cond)

void evl_add_wait_queue(struct evl_wait_queue *wq,
			ktime_t timeout,
			enum evl_tmode timeout_mode);

int evl_wait_schedule(struct evl_wait_queue *wq);

static inline bool evl_wait_active(struct evl_wait_queue *wq)
{
	assert_evl_lock(&wq->lock);
	return !list_empty(&wq->wchan.wait_list);
}

void __evl_init_wait(struct evl_wait_queue *wq,
		struct evl_clock *clock,
		int flags,
		const char *name,
		struct lock_class_key *key);

#define evl_init_wait(__wq, __clock, __flags)				\
	do {								\
		static struct lock_class_key __key;			\
		__evl_init_wait(__wq, __clock, __flags, #__wq, &__key); \
	} while (0)

void evl_destroy_wait(struct evl_wait_queue *wq);

struct evl_thread *evl_wait_head(struct evl_wait_queue *wq);

void evl_flush_wait_locked(struct evl_wait_queue *wq,
			int reason);

void evl_flush_wait(struct evl_wait_queue *wq,
		int reason);

struct evl_thread *evl_wake_up(struct evl_wait_queue *wq,
			struct evl_thread *waiter);

static inline
struct evl_thread *evl_wake_up_head(struct evl_wait_queue *wq)
{
	return evl_wake_up(wq, NULL);
}

int evl_reorder_wait(struct evl_thread *waiter,
		struct evl_thread *originator);

int evl_follow_wait_depend(struct evl_wait_channel *wchan,
			struct evl_thread *originator);

#endif /* !_EVL_WAIT_H_ */
