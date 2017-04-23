/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_SCHED_QUEUE_H
#define _EVENLESS_SCHED_QUEUE_H

#include <linux/bitmap.h>
#include <evenless/list.h>

#define EVL_CLASS_WEIGHT_FACTOR	1024

/*
 * Multi-level priority queue, suitable for handling the runnable
 * thread queue of the core scheduling class with O(1) property. We
 * only manage a descending queuing order, i.e. highest numbered
 * priorities come first.
 */
#define EVL_MLQ_LEVELS  (MAX_RT_PRIO + 1) /* i.e. EVL_CORE_NR_PRIO */

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
	return find_first_bit(q->prio_map, EVL_MLQ_LEVELS);
}

typedef struct evl_multilevel_queue evl_schedqueue_t;

struct evl_thread *evl_lookup_schedq(evl_schedqueue_t *q, int prio);

#endif /* !_EVENLESS_SCHED_QUEUE_H */
