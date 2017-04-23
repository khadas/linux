/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_SYN_H
#define _EVENLESS_SYN_H

#include <linux/types.h>
#include <evenless/list.h>
#include <evenless/assert.h>
#include <evenless/timer.h>
#include <uapi/evenless/synch.h>
#include <uapi/evenless/thread.h>

#define EVL_SYN_CLAIMED  0x100	/* Claimed by other thread(s) (PI) */
#define EVL_SYN_CEILING  0x200	/* Actively boosting (PP) */

struct evl_thread;
struct evl_syn;
struct evl_clock;

struct evl_syn {
	/* wait (weighted) prio in thread->boosters */
	int wprio;
	/* thread->boosters */
	struct list_head next;
	/*
	 *  &variable holding the current priority ceiling value
	 *  (evl_sched_rt-based, [1..255], EVL_SYN_PP).
	 */
	u32 *ceiling_ref;
	/* Status word */
	unsigned long status;
	/* Waiting threads */
	struct list_head wait_list;
	/* Thread which owns the resource */
	struct evl_thread *owner;
	/* Pointer to fast lock word */
	atomic_t *fastlock;
	/* Reference clock. */
	struct evl_clock *clock;
};

#define EVL_SYN_INITIALIZER(__name, __type)				\
	{								\
		.status = __type,					\
			.wprio = -1,					\
			.owner = NULL,					\
			.fastlock = NULL,				\
			.ceiling_ref = NULL,				\
			.wait_list = LIST_HEAD_INIT((__name).wait_list), \
			.next = LIST_HEAD_INIT((__name).next),		\
			}

#define evl_for_each_syn_waiter(__pos, __synch)				\
	list_for_each_entry(__pos, &(__synch)->wait_list, syn_next)

#define evl_for_each_syn_waiter_safe(__pos, __tmp, __synch)		\
	list_for_each_entry_safe(__pos, __tmp, &(__synch)->wait_list, syn_next)

static inline int evl_syn_has_waiter(struct evl_syn *synch)
{
	return !list_empty(&synch->wait_list);
}

struct evl_thread *evl_syn_wait_head(struct evl_syn *synch);

#ifdef CONFIG_EVENLESS_DEBUG_MONITOR_INBAND
void evl_detect_boost_drop(struct evl_thread *owner);
#else
static inline
void evl_detect_boost_drop(struct evl_thread *owner) { }
#endif

void evl_init_syn(struct evl_syn *synch, int flags,
		  struct evl_clock *clock,
		  atomic_t *fastlock);

void evl_init_syn_protect(struct evl_syn *synch,
			  struct evl_clock *clock,
			  atomic_t *fastlock, u32 *ceiling_ref);

bool evl_destroy_syn(struct evl_syn *synch);

int __must_check evl_sleep_on_syn(struct evl_syn *synch,
				  ktime_t timeout,
				  enum evl_tmode timeout_mode);

struct evl_thread *evl_wake_up_syn(struct evl_syn *synch);

int evl_wake_up_nr_syn(struct evl_syn *synch, int nr);

void evl_wake_up_targeted_syn(struct evl_syn *synch,
			      struct evl_thread *waiter);

int __must_check evl_acquire_syn(struct evl_syn *synch,
				 ktime_t timeout,
				 enum evl_tmode timeout_mode);

int __must_check evl_try_acquire_syn(struct evl_syn *synch);

bool evl_release_syn(struct evl_syn *synch, struct evl_thread *thread);

bool evl_flush_syn(struct evl_syn *synch, int reason);

void evl_requeue_syn_waiter(struct evl_thread *thread);

void evl_forget_syn_waiter(struct evl_thread *thread);

void evl_commit_syn_ceiling(struct evl_syn *synch,
			    struct evl_thread *curr);

#endif /* !_EVENLESS_SYN_H_ */
