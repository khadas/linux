/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SCHED_QUEUE_H
#define _EVL_SCHED_QUEUE_H

#include <linux/bitmap.h>
#include <evl/list.h>

/*
 * EVL core priority scale. We reserve a couple of additional priority
 * levels above the highest inband kthread priority (MAX_RT_PRIO-1),
 * which is guaranteed not to be less than the highest EVL user task
 * priority (MAX_USER_RT_PRIO-1) we use for SCHED_FIFO. Those extra
 * levels can be used for EVL kthreads which must top the priority of
 * any userland thread.
 */
#define EVL_CORE_MIN_PRIO  0
#define EVL_CORE_MAX_PRIO  (MAX_RT_PRIO + 1)
#define EVL_CORE_NR_PRIO   (EVL_CORE_MAX_PRIO - EVL_CORE_MIN_PRIO + 1)

#define EVL_MLQ_LEVELS		 EVL_CORE_NR_PRIO

#define EVL_CLASS_WEIGHT_FACTOR	 1024

#if EVL_CORE_NR_PRIO > EVL_CLASS_WEIGHT_FACTOR ||	\
	EVL_CORE_NR_PRIO > EVL_MLQ_LEVELS
#error "EVL_MLQ_LEVELS is too low"
#endif

struct evl_multilevel_queue {
	int elems;
	DECLARE_BITMAP(prio_map, EVL_MLQ_LEVELS);
	struct list_head heads[EVL_MLQ_LEVELS];
};

struct evl_thread;

void evl_init_schedq(struct evl_multilevel_queue *q);

void evl_add_schedq(struct evl_multilevel_queue *q,
		struct evl_thread *thread);

void evl_add_schedq_tail(struct evl_multilevel_queue *q,
			struct evl_thread *thread);

void evl_del_schedq(struct evl_multilevel_queue *q,
		struct evl_thread *thread);

struct evl_thread *evl_get_schedq(struct evl_multilevel_queue *q);

static inline int evl_schedq_is_empty(struct evl_multilevel_queue *q)
{
	return q->elems == 0;
}

static inline int evl_get_schedq_weight(struct evl_multilevel_queue *q)
{
	/* Highest priorities are mapped to lowest array elements. */
	return find_first_bit(q->prio_map, EVL_MLQ_LEVELS);
}

struct evl_thread *
evl_lookup_schedq(struct evl_multilevel_queue *q, int prio);

#endif /* !_EVL_SCHED_QUEUE_H */
