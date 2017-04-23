/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_SCHED_RT_H
#define _EVENLESS_SCHED_RT_H

#ifndef _EVENLESS_SCHED_H
#error "please don't include evenless/sched/rt.h directly"
#endif

/*
 * Global priority scale for the core scheduling class, available to
 * SCHED_EVL members.
 */
#define EVL_CORE_MIN_PRIO  0
#define EVL_CORE_MAX_PRIO  MAX_RT_PRIO
#define EVL_CORE_NR_PRIO   (EVL_CORE_MAX_PRIO - EVL_CORE_MIN_PRIO + 1)

/* Priority range for SCHED_FIFO. */
#define EVL_FIFO_MIN_PRIO  1
#define EVL_FIFO_MAX_PRIO  (MAX_USER_RT_PRIO - 1)

#if EVL_CORE_NR_PRIO > EVL_CLASS_WEIGHT_FACTOR ||	\
	EVL_CORE_NR_PRIO > EVL_MLQ_LEVELS
#error "EVL_MLQ_LEVELS is too low"
#endif

extern struct evl_sched_class evl_sched_rt;

static inline void __evl_requeue_rt_thread(struct evl_thread *thread)
{
	evl_add_schedq(&thread->rq->rt.runnable, thread);
}

static inline void __evl_enqueue_rt_thread(struct evl_thread *thread)
{
	evl_add_schedq_tail(&thread->rq->rt.runnable, thread);
}

static inline void __evl_dequeue_rt_thread(struct evl_thread *thread)
{
	evl_del_schedq(&thread->rq->rt.runnable, thread);
}

static inline
int __evl_chk_rt_schedparam(struct evl_thread *thread,
			    const union evl_sched_param *p)
{
	if (p->rt.prio < EVL_CORE_MIN_PRIO ||
	    p->rt.prio > EVL_CORE_MAX_PRIO)
		return -EINVAL;

	return 0;
}

static inline
bool __evl_set_rt_schedparam(struct evl_thread *thread,
			     const union evl_sched_param *p)
{
	bool ret = evl_set_effective_thread_priority(thread, p->rt.prio);

	if (!(thread->state & T_BOOST))
		thread->state &= ~T_WEAK;

	return ret;
}

static inline
void __evl_get_rt_schedparam(struct evl_thread *thread,
			     union evl_sched_param *p)
{
	p->rt.prio = thread->cprio;
}

static inline
void __evl_track_rt_priority(struct evl_thread *thread,
			     const union evl_sched_param *p)
{
	if (p)
		thread->cprio = p->rt.prio; /* Force update. */
	else {
		thread->cprio = thread->bprio;
		/* Leaving PI/PP, so neither boosted nor weak. */
		thread->state &= ~T_WEAK;
	}
}

static inline
void __evl_ceil_rt_priority(struct evl_thread *thread, int prio)
{
	/*
	 * The RT class supports the widest priority range from
	 * EVL_CORE_MIN_PRIO to EVL_CORE_MAX_PRIO inclusive,
	 * no need to cap the input value which is guaranteed to be in
	 * the range [1..EVL_CORE_MAX_PRIO].
	 */
	thread->cprio = prio;
}

static inline
void __evl_forget_rt_thread(struct evl_thread *thread)
{
}

static inline
int evl_init_rt_thread(struct evl_thread *thread)
{
	return 0;
}

struct evl_thread *evl_rt_pick(struct evl_rq *rq);

#endif /* !_EVENLESS_SCHED_RT_H */
