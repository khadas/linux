/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_MUTEX_H
#define _EVL_MUTEX_H

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/atomic.h>
#include <evl/list.h>
#include <evl/assert.h>
#include <evl/timer.h>
#include <evl/wait.h>
#include <uapi/evl/mutex.h>

struct evl_clock;
struct evl_thread;

/* Priority inheritance protocol enforced. */
#define EVL_MUTEX_PI      	BIT(0)
/* Priority protection protocol enforced. */
#define EVL_MUTEX_PP      	BIT(1)
/* Causes a priority boost for its owner. */
#define EVL_MUTEX_PIBOOST	BIT(2)
/* Causes a priority ceiling for its owner. */
#define EVL_MUTEX_CEILING	BIT(3)
/* Counted in thread->held_mutex_count. */
#define EVL_MUTEX_COUNTED	BIT(4)

/* Modes for PI chain walk. */
enum evl_walk_mode {
	evl_pi_adjust,		/* Adjust priority of members. */
	evl_pi_reset,		/* Revert members to their base priority. */
	evl_pi_check,		/* Check the PI chain (no change). */
};

struct evl_mutex {
	int wprio;
	int flags;
	struct evl_clock *clock;
	atomic_t *fastlock;
	u32 *ceiling_ref;
	struct evl_wait_channel wchan;
	struct list_head next_booster; /* thread->boosters */
	struct list_head next_owned;   /* thread->owned_mutexes */
};

void __evl_init_mutex(struct evl_mutex *mutex,
		struct evl_clock *clock,
		atomic_t *fastlock,
		u32 *ceiling_ref,
		const char *name);

#define evl_init_named_mutex_pi(__mutex, __clock, __fastlock, __name)	\
	__evl_init_mutex(__mutex, __clock, __fastlock, NULL, __name)

#define evl_init_mutex_pi(__mutex, __clock, __fastlock)			\
	evl_init_named_mutex_pi(__mutex, __clock, __fastlock, #__mutex)

#define evl_init_named_mutex_pp(__mutex, __clock, __fastlock, __ceiling, __name) \
	__evl_init_mutex(__mutex, __clock, __fastlock, __ceiling, __name)

#define evl_init_mutex_pp(__mutex, __clock, __fastlock, __ceiling)	\
	evl_init_named_mutex_pp(__mutex, __clock, __fastlock, __ceiling, #__mutex)

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

void evl_detect_boost_drop(void);

int evl_reorder_mutex_wait(struct evl_thread *waiter,
			struct evl_thread *originator);

void evl_requeue_mutex_wait(struct evl_wait_channel *wchan,
			struct evl_thread *waiter);

void evl_drop_current_ownership(void);

struct evl_kmutex {
	struct evl_mutex mutex;
	atomic_t fastlock;
};

#define EVL_KMUTEX_INITIALIZER(__name) {				\
		.mutex = {						\
			.fastlock = &(__name).fastlock,			\
			.flags = EVL_MUTEX_PI,				\
			.wprio = -1,					\
			.ceiling_ref = NULL,				\
			.clock = &evl_mono_clock,			\
			.wchan = {					\
				.lock = __HARD_SPIN_LOCK_INITIALIZER((__name).wchan.lock), \
				.pi_serial = 0,				\
				.owner = NULL,				\
				.reorder_wait = evl_reorder_mutex_wait,	\
				.requeue_wait = evl_requeue_mutex_wait,	\
				.wait_list = LIST_HEAD_INIT((__name).mutex.wchan.wait_list), \
				.name = #__name,			\
			},						\
		},							\
		.fastlock = ATOMIC_INIT(0),				\
	}

#define DEFINE_EVL_KMUTEX(__name)					\
	struct evl_kmutex __name = EVL_KMUTEX_INITIALIZER(__name)

static inline
void evl_init_kmutex(struct evl_kmutex *kmutex)
{
	atomic_set(&kmutex->fastlock, 0);
	evl_init_mutex_pi(&kmutex->mutex, &evl_mono_clock, &kmutex->fastlock);
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

void evl_adjust_wait_priority(struct evl_thread *thread,
			      enum evl_walk_mode mode);

#endif /* !_EVL_MUTEX_H */
