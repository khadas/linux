/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_MUTEX_H
#define _EVENLESS_MUTEX_H

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/atomic.h>
#include <evenless/list.h>
#include <evenless/assert.h>
#include <evenless/timer.h>
#include <evenless/thread.h>
#include <uapi/evenless/mutex.h>

struct evl_clock;
struct evl_thread;

#define EVL_MUTEX_PI      	BIT(0)
#define EVL_MUTEX_PP      	BIT(1)
#define EVL_MUTEX_CLAIMED	BIT(2)
#define EVL_MUTEX_CEILING	BIT(3)

struct evl_mutex {
	int wprio;
	int flags;
	struct evl_thread *owner;
	struct evl_clock *clock;
	atomic_t *fastlock;
	u32 *ceiling_ref;
	struct evl_wait_channel wchan;
	struct list_head wait_list;
	struct list_head next;	/* thread->boosters */
};

#define evl_for_each_mutex_waiter(__pos, __mutex)			\
	list_for_each_entry(__pos, &(__mutex)->wait_list, wait_next)

void evl_init_mutex_pi(struct evl_mutex *mutex,
		struct evl_clock *clock,
		atomic_t *fastlock);

void evl_init_mutex_pp(struct evl_mutex *mutex,
		struct evl_clock *clock,
		atomic_t *fastlock,
		u32 *ceiling_ref);

void evl_destroy_mutex(struct evl_mutex *mutex);

int evl_trylock_mutex(struct evl_mutex *mutex);

int evl_lock_mutex_timeout(struct evl_mutex *mutex, ktime_t timeout,
			enum evl_tmode timeout_mode);

static inline int evl_lock_mutex(struct evl_mutex *mutex)
{
	return evl_lock_mutex_timeout(mutex, EVL_INFINITE, EVL_REL);
}

void __evl_unlock_mutex(struct evl_mutex *mutex);

void evl_unlock_mutex(struct evl_mutex *mutex);

void evl_flush_mutex(struct evl_mutex *mutex,
		int reason);

void evl_commit_mutex_ceiling(struct evl_mutex *mutex);

#ifdef CONFIG_EVENLESS_DEBUG_MUTEX_INBAND
void evl_detect_boost_drop(struct evl_thread *owner);
#else
static inline
void evl_detect_boost_drop(struct evl_thread *owner) { }
#endif

void evl_abort_mutex_wait(struct evl_thread *thread,
			struct evl_wait_channel *wchan);

void evl_reorder_mutex_wait(struct evl_thread *thread);

struct evl_kmutex {
	struct evl_mutex mutex;
	atomic_t fastlock;
};

#define EVL_KMUTEX_INITIALIZER(__name) {				\
		.mutex = {						\
			.fastlock = &(__name).fastlock,			\
			.flags = EVL_MUTEX_PI,				\
			.owner = NULL,					\
			.wprio = -1,					\
			.ceiling_ref = NULL,				\
			.clock = &evl_mono_clock,			\
			.wait_list = LIST_HEAD_INIT((__name).mutex.wait_list), \
			.wchan = {					\
				.abort_wait = evl_abort_mutex_wait,	\
				.reorder_wait = evl_reorder_mutex_wait,	\
				.lock = __HARD_SPIN_LOCK_INITIALIZER((__name).wchan.lock), \
			},						\
		},							\
			.fastlock = ATOMIC_INIT(0),			\
	}

#define DEFINE_EVL_MUTEX(__name)					\
	struct evl_kmutex __name = EVL_KMUTEX_INITIALIZER(__name)

static inline
void evl_init_kmutex(struct evl_kmutex *kmutex)
{
	*kmutex = (struct evl_kmutex)EVL_KMUTEX_INITIALIZER(*kmutex);
}

static inline
void evl_destroy_kmutex(struct evl_kmutex *kmutex)
{
	evl_destroy_mutex(&kmutex->mutex);
}

static inline
int evl_trylock_kmutex(struct evl_kmutex *kmutex)
{
	return evl_trylock_mutex(&kmutex->mutex);
}

static inline
int evl_lock_kmutex(struct evl_kmutex *kmutex)
{
	return evl_lock_mutex(&kmutex->mutex);
}

static inline
void evl_unlock_kmutex(struct evl_kmutex *kmutex)
{
	return evl_unlock_mutex(&kmutex->mutex);
}

#endif /* !_EVENLESS_MUTEX_H */
