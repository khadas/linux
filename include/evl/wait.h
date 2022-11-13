/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_WAIT_H
#define _EVL_WAIT_H

#include <linux/errno.h>
#include <linux/list.h>
#include <evl/assert.h>
#include <evl/timeout.h>

#define EVL_WAIT_FIFO    0
#define EVL_WAIT_PRIO    BIT(0)

struct evl_thread;

struct evl_wait_channel {
	hard_spinlock_t lock;
	struct lock_class_key lock_key;	/* lockdep disambiguation */
	s64 pi_serial;
	struct evl_thread *owner;
	void (*requeue_wait)(struct evl_wait_channel *wchan,
			     struct evl_thread *waiter);
	struct list_head wait_list;
	const char *name;
};

/* Modes for PI chain walk. */
enum evl_walk_mode {
	evl_pi_adjust,		/* Adjust priority of members. */
	evl_pi_reset,		/* Revert members to their base priority. */
	evl_pi_check,		/* Check the PI chain (no change). */
};

#ifdef CONFIG_LOCKDEP
struct evl_lock_key_addr {
	struct lock_class_key *addr;
};
#define __EVL_LOCK_KEY_ADDR_INITIALIZER  (struct foo){ .addr = NULL }
#else
struct evl_lock_key_addr { };
#define __EVL_LOCK_KEY_ADDR_INITIALIZER  (struct foo){ }
#endif

struct evl_wait_queue {
	int flags;
	struct evl_clock *clock;
	struct evl_wait_channel wchan;
	struct evl_lock_key_addr lock_key_addr;
};

#define EVL_WAIT_INITIALIZER(__name) {					\
		.flags = EVL_WAIT_PRIO,					\
		.clock = &evl_mono_clock,				\
		.lock_key_addr = __EVL_LOCK_KEY_ADDR_INITIALIZER,	\
		.wchan = {						\
			.lock = __HARD_SPIN_LOCK_INITIALIZER((__name).wchan.lock), \
			.pi_serial = 0,					\
			.owner = NULL,					\
			.requeue_wait = evl_requeue_wait,		\
			.wait_list = LIST_HEAD_INIT((__name).wchan.wait_list), \
			.name = #__name,				\
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
	raw_spin_lock_irqsave(&(__wq)->wchan.lock, __flags);		\
	if (!(__cond)) {						\
		if (timeout_nonblock(__timeout)) {			\
			__ret = -EAGAIN;				\
		} else {						\
			do {						\
				evl_add_wait_queue(__wq, __timeout,	\
						__timeout_mode);	\
				raw_spin_unlock_irqrestore(&(__wq)->wchan.lock, __flags); \
				__ret = evl_wait_schedule(__wq);	\
				__bcast = evl_current()->info & T_BCAST; \
				raw_spin_lock_irqsave(&(__wq)->wchan.lock, __flags); \
			} while (!__ret && !__bcast && !(__cond));	\
		}							\
	}								\
	raw_spin_unlock_irqrestore(&(__wq)->wchan.lock, __flags);	\
	__ret;								\
})

#define evl_wait_event(__wq, __cond)					\
	evl_wait_event_timeout(__wq, EVL_INFINITE, EVL_REL, __cond)

void evl_add_wait_queue(struct evl_wait_queue *wq,
			ktime_t timeout,
			enum evl_tmode timeout_mode);

void evl_add_wait_queue_unchecked(struct evl_wait_queue *wq,
				  ktime_t timeout,
				  enum evl_tmode timeout_mode);

int __evl_wait_schedule(struct evl_wait_channel *wchan);

static inline int evl_wait_schedule(struct evl_wait_queue *wq)
{
	return __evl_wait_schedule(&wq->wchan);
}

static inline bool evl_wait_active(struct evl_wait_queue *wq)
{
	assert_hard_lock(&wq->wchan.lock);
	return !list_empty(&wq->wchan.wait_list);
}

void __evl_init_wait(struct evl_wait_queue *wq,
		struct evl_clock *clock,
		int flags,
		const char *name,
		struct lock_class_key *lock_key);

#define evl_init_named_wait(__wq, __clock, __flags, __name)	\
	__evl_init_wait(__wq, __clock, __flags, __name, NULL)

#define evl_init_wait(__wq, __clock, __flags)	\
	evl_init_named_wait(__wq, __clock, __flags, #__wq)

#define evl_init_wait_on_stack(__wq, __clock, __flags)	\
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
			struct evl_thread *waiter,
			int reason);

static inline
struct evl_thread *evl_wake_up_head(struct evl_wait_queue *wq)
{
	return evl_wake_up(wq, NULL, 0);
}

void evl_requeue_wait(struct evl_wait_channel *wchan,
		struct evl_thread *waiter);

void evl_adjust_wait_priority(struct evl_thread *thread,
			      enum evl_walk_mode mode);

int evl_walk_pi_chain(struct evl_wait_channel *orig_wchan,
		struct evl_thread *orig_waiter,
		enum evl_walk_mode mode);

#endif /* !_EVL_WAIT_H_ */
