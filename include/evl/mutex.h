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
#include <evl/sched.h>

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

struct evl_mutex {
	atomic_t *fastlock;
	int flags;
	u32 *ceiling_addr;
	struct evl_wait_channel wchan;
	struct list_head next_booster; /* thread->boosters */
	struct list_head next_owned;   /* thread->owned_mutexes */
	struct {
		struct evl_sched_class *sched_class;
		union evl_sched_param param;
		int wprio;
	} boost;
	struct evl_clock *clock;
};

void __evl_init_mutex(struct evl_mutex *mutex,
		struct evl_clock *clock,
		atomic_t *fastlock,
		u32 *ceiling,
		const char *name,
		struct lock_class_key *lock_key);

#define evl_init_mutex(__mutex, __clock, __fastlock, __ceiling)		\
	do {								\
		static struct lock_class_key key;			\
		__evl_init_mutex(__mutex, __clock, __fastlock, __ceiling, #__mutex, &key); \
	} while (0)

#define evl_init_mutex_pi(__mutex, __clock, __fastlock)			\
	evl_init_mutex(__mutex, __clock, __fastlock, NULL)

#define evl_init_mutex_pp(__mutex, __clock, __fastlock, __ceiling)	\
	evl_init_mutex(__mutex, __clock, __fastlock, __ceiling)

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

void evl_check_no_mutex(void);

bool evl_requeue_mutex_wait(struct evl_wait_channel *wchan,
			struct evl_thread *waiter);

void evl_drop_current_ownership(void);

static inline bool
evl_is_mutex_owner(atomic_t *fastlock, fundle_t ownerh)
{
	return evl_get_index(atomic_read(fastlock)) == ownerh;
}

struct evl_kmutex {
	struct evl_mutex mutex;
	atomic_t fastlock;
};

#define EVL_KMUTEX_INITIALIZER(__name) {				\
		.mutex = {						\
			.fastlock = &(__name).fastlock,			\
			.flags = EVL_MUTEX_PI,				\
			.wprio = -1,					\
			.ceiling_addr = NULL,				\
			.clock = &evl_mono_clock,			\
			.wchan = {					\
				.lock = __HARD_SPIN_LOCK_INITIALIZER((__name).wchan.lock), \
				.owner = NULL,				\
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

static inline int evl_ceiling_priority(struct evl_mutex *mutex)
{
	/*
	 * The ceiling priority value is stored in user-writable
	 * memory, make sure to constrain it within valid bounds for
	 * evl_sched_fifo before using it.
	 */
	return clamp(*mutex->ceiling_addr, 1U, (u32)EVL_FIFO_MAX_PRIO);
}

#endif /* !_EVL_MUTEX_H */
