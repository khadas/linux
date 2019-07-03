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

struct evl_thread *evl_get_schedq(struct evl_multilevel_queue *q);

struct evl_thread *
evl_lookup_schedq(struct evl_multilevel_queue *q, int prio);

static __always_inline
int evl_schedq_is_empty(struct evl_multilevel_queue *q)
{
	return q->elems == 0;
}

static __always_inline
int evl_get_schedq_weight(struct evl_multilevel_queue *q)
{
	/* Highest priorities are mapped to lowest array elements. */
	return find_first_bit(q->prio_map, EVL_MLQ_LEVELS);
}

static __always_inline
int get_qindex(struct evl_multilevel_queue *q, int prio)
{
	/*
	 * find_first_bit() is used to scan the bitmap, so the lower
	 * the index value, the higher the priority.
	 */
	return EVL_MLQ_LEVELS - prio - 1;
}

static __always_inline
struct list_head *add_q(struct evl_multilevel_queue *q, int prio)
{
	struct list_head *head;
	int idx;

	idx = get_qindex(q, prio);
	head = q->heads + idx;
	q->elems++;

	/* New item is not linked yet. */
	if (list_empty(head))
		__set_bit(idx, q->prio_map);

	return head;
}

static __always_inline
void evl_add_schedq(struct evl_multilevel_queue *q,
		struct evl_thread *thread)
{
	struct list_head *head = add_q(q, thread->cprio);
	list_add(&thread->rq_next, head);
}

static __always_inline
void evl_add_schedq_tail(struct evl_multilevel_queue *q,
			struct evl_thread *thread)
{
	struct list_head *head = add_q(q, thread->cprio);
	list_add_tail(&thread->rq_next, head);
}

static __always_inline
void __evl_del_schedq(struct evl_multilevel_queue *q,
		struct list_head *entry, int idx)
{
	struct list_head *head = q->heads + idx;

	list_del(entry);
	q->elems--;

	if (list_empty(head))
		__clear_bit(idx, q->prio_map);
}

static __always_inline
void evl_del_schedq(struct evl_multilevel_queue *q,
	struct evl_thread *thread)
{
	__evl_del_schedq(q, &thread->rq_next, get_qindex(q, thread->cprio));
}

#endif /* !_EVL_SCHED_QUEUE_H */
