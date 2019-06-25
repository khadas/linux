/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SCHED_FIFO_H
#define _EVL_SCHED_FIFO_H

#ifndef _EVL_SCHED_H
#error "please don't include evl/sched/fifo.h directly"
#endif

/*
 * EVL's SCHED_FIFO class is meant to map onto the inband SCHED_FIFO
 * priority scale when applied to user threads. EVL kthreads may use a
 * couple of levels more, from EVL_CORE_MIN_PRIO to EVL_CORE_MAX_PRIO.
 */
#define EVL_FIFO_MIN_PRIO  1
#define EVL_FIFO_MAX_PRIO  (MAX_USER_RT_PRIO - 1)

extern struct evl_sched_class evl_sched_fifo;

static inline void __evl_requeue_fifo_thread(struct evl_thread *thread)
{
	evl_add_schedq(&thread->rq->fifo.runnable, thread);
}

static inline void __evl_enqueue_fifo_thread(struct evl_thread *thread)
{
	evl_add_schedq_tail(&thread->rq->fifo.runnable, thread);
}

static inline void __evl_dequeue_fifo_thread(struct evl_thread *thread)
{
	evl_del_schedq(&thread->rq->fifo.runnable, thread);
}

static inline
int __evl_chk_fifo_schedparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	int min = EVL_FIFO_MIN_PRIO, max = EVL_FIFO_MAX_PRIO;

	if (!(thread->state & T_USER)) {
		min = EVL_CORE_MIN_PRIO;
		max = EVL_CORE_MAX_PRIO;
	}

	if (p->fifo.prio < min || p->fifo.prio > max)
		return -EINVAL;

	return 0;
}

static inline
bool __evl_set_fifo_schedparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	bool ret = evl_set_effective_thread_priority(thread, p->fifo.prio);

	if (!(thread->state & T_BOOST))
		thread->state &= ~T_WEAK;

	return ret;
}

static inline
void __evl_get_fifo_schedparam(struct evl_thread *thread,
			union evl_sched_param *p)
{
	p->fifo.prio = thread->cprio;
}

static inline
void __evl_track_fifo_priority(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	if (p)
		thread->cprio = p->fifo.prio; /* Force update. */
	else {
		thread->cprio = thread->bprio;
		/* Leaving PI/PP, so neither boosted nor weak. */
		thread->state &= ~T_WEAK;
	}
}

static inline
void __evl_ceil_fifo_priority(struct evl_thread *thread, int prio)
{
	/*
	 * The FIFO class supports the widest priority range from
	 * EVL_CORE_MIN_PRIO to EVL_CORE_MAX_PRIO inclusive, no need
	 * to cap the priority argument which is guaranteed to be in
	 * this range.
	 */
	thread->cprio = prio;
}

static inline
void __evl_forget_fifo_thread(struct evl_thread *thread)
{
}

static inline
int evl_init_fifo_thread(struct evl_thread *thread)
{
	return 0;
}

struct evl_thread *evl_fifo_pick(struct evl_rq *rq);

#endif /* !_EVL_SCHED_FIFO_H */
