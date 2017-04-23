/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2013, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_SCHED_WEAK_H
#define _EVENLESS_SCHED_WEAK_H

#ifndef _EVENLESS_SCHED_H
#error "please don't include evenless/sched/weak.h directly"
#endif

#define EVL_WEAK_MIN_PRIO  0
#define EVL_WEAK_MAX_PRIO  99
#define EVL_WEAK_NR_PRIO   (EVL_WEAK_MAX_PRIO - EVL_WEAK_MIN_PRIO + 1)

#if EVL_WEAK_NR_PRIO > EVL_CLASS_WEIGHT_FACTOR ||	\
	 EVL_WEAK_NR_PRIO > EVL_MLQ_LEVELS
#error "WEAK class has too many priority levels"
#endif

extern struct evl_sched_class evl_sched_weak;

struct evl_sched_weak {
	evl_schedqueue_t runnable;	/*!< Runnable thread queue. */
};

static inline int evl_weak_init_thread(struct evl_thread *thread)
{
	return 0;
}

#endif /* !_EVENLESS_SCHED_WEAK_H */
