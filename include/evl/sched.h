/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SCHED_H
#define _EVL_SCHED_H

#include <linux/percpu.h>
#include <linux/list.h>
#include <evl/lock.h>
#include <evl/thread.h>
#include <evl/wait.h>
#include <evl/sched/queue.h>
#include <evl/sched/weak.h>
#include <evl/sched/quota.h>
#include <evl/sched/tp.h>
#include <evl/assert.h>
#include <evl/init.h>

/** Shared scheduler status bits **/

/*
 * A rescheduling call is pending.
 */
#define RQ_SCHED	0x10000000

/**
 * Private scheduler flags (combined in test operations with shared
 * bits, must not conflict with them).
 */

/*
 * Currently running in tick handler context.
 */
#define RQ_TIMER	0x00010000
/*
 * A proxy tick is being processed, i.e. matching an earlier timing
 * request from inband via set_next_event().
 */
#define RQ_TPROXY	0x00008000
/*
 * Currently running in IRQ handling context.
 */
#define RQ_IRQ		0x00004000
/*
 * Proxy tick is deferred, because we have more urgent real-time
 * duties to carry out first.
 */
#define RQ_TDEFER	0x00002000
/*
 * Idle state: there is no outstanding timer. We check this flag to
 * know whether we may allow the regular kernel to enter the CPU idle
 * state.
 */
#define RQ_IDLE		0x00001000
/*
 * Hardware timer is stopped.
 */
#define RQ_TSTOPPED	0x00000800

struct evl_sched_fifo {
	struct evl_multilevel_queue runnable;
};

struct evl_rq {
	unsigned long status;	/* Shared flags */
	unsigned long lflags;	/* Private flags (lockless) */
	struct evl_thread *curr;
#ifdef CONFIG_SMP
	int cpu;
	struct cpumask resched;	/* CPUs pending resched */
#endif
	struct evl_sched_fifo fifo;
	struct evl_sched_weak weak;
#ifdef CONFIG_EVL_SCHED_QUOTA
	struct evl_sched_quota quota;
#endif
#ifdef CONFIG_EVL_SCHED_TP
	struct evl_sched_tp tp;
#endif
	struct evl_timer inband_timer;
	struct evl_timer rrbtimer; /* Round-robin */
	struct evl_thread root_thread;
	char *proxy_timer_name;
	char *rrb_timer_name;
#ifdef CONFIG_EVL_WATCHDOG
	struct evl_timer wdtimer;
#endif
#ifdef CONFIG_EVL_RUNSTATS
	ktime_t last_account_switch;
	struct evl_account *current_account;
#endif
};

DECLARE_PER_CPU(struct evl_rq, evl_runqueues);

extern struct cpumask evl_cpu_affinity;

extern struct list_head evl_thread_list;

extern int evl_nrthreads;

union evl_sched_param;
struct evl_sched_config;

struct evl_sched_class {
	void (*sched_init)(struct evl_rq *rq);
	void (*sched_enqueue)(struct evl_thread *thread);
	void (*sched_dequeue)(struct evl_thread *thread);
	void (*sched_requeue)(struct evl_thread *thread);
	struct evl_thread *(*sched_pick)(struct evl_rq *rq);
	void (*sched_tick)(struct evl_rq *rq);
	void (*sched_rotate)(struct evl_rq *rq,
			     const union evl_sched_param *p);
	void (*sched_migrate)(struct evl_thread *thread,
			      struct evl_rq *rq);
	/*
	 * Set base scheduling parameters. This routine is indirectly
	 * called upon a change of base scheduling settings through
	 * __evl_set_thread_schedparam() -> evl_set_thread_policy(),
	 * exclusively.
	 *
	 * The scheduling class implementation should do the necessary
	 * housekeeping to comply with the new settings.
	 * thread->base_class is up to date before the call is made,
	 * and should be considered for the new weighted priority
	 * calculation. On the contrary, thread->sched_class should
	 * NOT be referred to by this handler.
	 *
	 * sched_setparam() is NEVER involved in PI or PP
	 * management. However it must deny a priority update if it
	 * contradicts an ongoing boost for @a thread. This is
	 * typically what the evl_set_effective_thread_priority() helper
	 * does for such handler.
	 *
	 * Returns true if the effective priority was updated
	 * (thread->cprio).
	 */
	bool (*sched_setparam)(struct evl_thread *thread,
			       const union evl_sched_param *p);
	void (*sched_getparam)(struct evl_thread *thread,
			       union evl_sched_param *p);
	int (*sched_chkparam)(struct evl_thread *thread,
			      const union evl_sched_param *p);
	void (*sched_trackprio)(struct evl_thread *thread,
				const union evl_sched_param *p);
	void (*sched_ceilprio)(struct evl_thread *thread, int prio);
	/* Prep work for assigning a policy to a thread. */
	int (*sched_declare)(struct evl_thread *thread,
			     const union evl_sched_param *p);
	void (*sched_forget)(struct evl_thread *thread);
	void (*sched_kick)(struct evl_thread *thread);
	ssize_t (*sched_show)(struct evl_thread *thread,
			      char *buf, ssize_t count);
	int (*sched_control)(int cpu, union evl_sched_ctlparam *ctlp,
			union evl_sched_ctlinfo *infp);
	int nthreads;
	struct evl_sched_class *next;
	int weight;
	int policy;
	const char *name;
};

#define EVL_CLASS_WEIGHT(n)	(n * EVL_CLASS_WEIGHT_FACTOR)

#define for_each_evl_thread(__thread)				\
	list_for_each_entry(__thread, &evl_thread_list, next)

static inline struct evl_rq *evl_cpu_rq(int cpu)
{
	return &per_cpu(evl_runqueues, cpu);
}

static inline struct evl_rq *this_evl_rq(void)
{
	/* IRQs off */
	return raw_cpu_ptr(&evl_runqueues);
}

static inline struct evl_thread *this_evl_rq_thread(void)
{
	return this_evl_rq()->curr;
}

/* Test resched flag of given rq. */
static inline int evl_need_resched(struct evl_rq *rq)
{
	return rq->status & RQ_SCHED;
}

/* Set self resched flag for the current scheduler. */
static inline void evl_set_self_resched(struct evl_rq *rq)
{
	requires_ugly_lock();
	rq->status |= RQ_SCHED;
}

/* Set resched flag for the given scheduler. */
#ifdef CONFIG_SMP

static inline bool is_evl_cpu(int cpu)
{
	return !!cpumask_test_cpu(cpu, &evl_oob_cpus);
}

static inline int evl_rq_cpu(struct evl_rq *rq)
{
	return rq->cpu;
}

static inline void evl_set_resched(struct evl_rq *rq)
{
	struct evl_rq *this_rq = this_evl_rq();

	if (this_rq == rq)
		this_rq->status |= RQ_SCHED;
	else if (!evl_need_resched(rq)) {
		cpumask_set_cpu(evl_rq_cpu(rq), &this_rq->resched);
		rq->status |= RQ_SCHED;
		this_rq->status |= RQ_SCHED;
	}
}

static inline bool is_threading_cpu(int cpu)
{
	return !!cpumask_test_cpu(cpu, &evl_cpu_affinity);
}

#else /* !CONFIG_SMP */

static inline bool is_evl_cpu(int cpu)
{
	return true;
}

static inline int evl_rq_cpu(struct evl_rq *rq)
{
	return 0;
}

static inline void evl_set_resched(struct evl_rq *rq)
{
	evl_set_self_resched(rq);
}

static inline bool is_threading_cpu(int cpu)
{
	return true;
}

#endif /* !CONFIG_SMP */

#define for_each_evl_cpu(cpu)		\
	for_each_online_cpu(cpu)	\
		if (is_evl_cpu(cpu))

bool __evl_schedule(struct evl_rq *this_rq);

static inline bool evl_schedule(void)
{
	struct evl_rq *this_rq = this_evl_rq();

	/*
	 * If we race here reading the scheduler state locklessly
	 * because of a CPU migration, we must be running over the
	 * in-band stage, in which case the call to __evl_schedule()
	 * will be escalated to the oob stage where migration cannot
	 * happen, ensuring safe access to the runqueue state.
	 *
	 * Remote RQ_SCHED requests are paired with out-of-band IPIs
	 * running on the oob stage by definition, so we can't miss
	 * them here.
	 *
	 * Finally, RQ_IRQ is always tested from the CPU which handled
	 * an out-of-band interrupt, there is no coherence issue.
	 */
	if (((this_rq->status|this_rq->lflags) & (RQ_IRQ|RQ_SCHED)) != RQ_SCHED)
		return false;

	return (bool)run_oob_call((int (*)(void *))__evl_schedule, this_rq);
}

static inline int evl_preempt_count(void)
{
	return dovetail_current_state()->preempt_count;
}

static inline void __evl_disable_preempt(void)
{
	dovetail_current_state()->preempt_count++;
}

static inline void __evl_enable_preempt(void)
{
	if (--dovetail_current_state()->preempt_count == 0)
		evl_schedule();
}

#ifdef CONFIG_EVL_DEBUG_CORE

void evl_disable_preempt(void);
void evl_enable_preempt(void);

#else

static inline void evl_disable_preempt(void)
{
	__evl_disable_preempt();
}

static inline void evl_enable_preempt(void)
{
	__evl_enable_preempt();
}

#endif

static inline bool evl_in_irq(void)
{
	return !!(this_evl_rq()->lflags & RQ_IRQ);
}

static inline bool evl_is_inband(void)
{
	return !!(this_evl_rq_thread()->state & T_ROOT);
}

static inline bool evl_cannot_block(void)
{
	return evl_in_irq() || evl_is_inband();
}

bool evl_set_effective_thread_priority(struct evl_thread *thread,
				       int prio);

#include <evl/sched/idle.h>
#include <evl/sched/fifo.h>

void evl_putback_thread(struct evl_thread *thread);

int evl_set_thread_policy(struct evl_thread *thread,
			  struct evl_sched_class *sched_class,
			  const union evl_sched_param *p);

void evl_track_thread_policy(struct evl_thread *thread,
			     struct evl_thread *target);

void evl_protect_thread_priority(struct evl_thread *thread,
				 int prio);

void evl_migrate_rq(struct evl_thread *thread,
		    struct evl_rq *rq);

static inline
void evl_rotate_rq(struct evl_rq *rq,
		   struct evl_sched_class *sched_class,
		   const union evl_sched_param *sched_param)
{
	sched_class->sched_rotate(rq, sched_param);
}

static inline int evl_init_rq_thread(struct evl_thread *thread)
{
	int ret = 0;

	evl_init_idle_thread(thread);
	evl_init_fifo_thread(thread);
#ifdef CONFIG_EVL_SCHED_QUOTA
	ret = evl_quota_init_thread(thread);
	if (ret)
		return ret;
#endif
#ifdef CONFIG_EVL_SCHED_TP
	ret = evl_tp_init_thread(thread);
	if (ret)
		return ret;
#endif

	return ret;
}

/* nklock held, irqs off */
static inline void evl_sched_tick(struct evl_rq *rq)
{
	struct evl_thread *curr = rq->curr;
	struct evl_sched_class *sched_class = curr->sched_class;
	/*
	 * A thread that undergoes round-robin scheduling only
	 * consumes its time slice when it runs within its own
	 * scheduling class, which excludes temporary PI boosts, and
	 * does not hold the scheduler lock.
	 */
	if (sched_class == curr->base_class &&
	    sched_class->sched_tick &&
	    (curr->state & (EVL_THREAD_BLOCK_BITS|T_RRB)) == T_RRB &&
	    evl_preempt_count() == 0)
		sched_class->sched_tick(rq);
}

static inline
int evl_check_schedparams(struct evl_sched_class *sched_class,
			  struct evl_thread *thread,
			  const union evl_sched_param *p)
{
	int ret = 0;

	if (sched_class->sched_chkparam)
		ret = sched_class->sched_chkparam(thread, p);

	return ret;
}

static inline
int evl_declare_thread(struct evl_sched_class *sched_class,
		       struct evl_thread *thread,
		       const union evl_sched_param *p)
{
	int ret;

	if (sched_class->sched_declare) {
		ret = sched_class->sched_declare(thread, p);
		if (ret)
			return ret;
	}
	if (sched_class != thread->base_class)
		sched_class->nthreads++;

	return 0;
}

static inline int evl_calc_weighted_prio(struct evl_sched_class *sched_class,
					 int prio)
{
	return prio + sched_class->weight;
}

static __always_inline void evl_enqueue_thread(struct evl_thread *thread)
{
	struct evl_sched_class *sched_class = thread->sched_class;

	/*
	 * Enqueue for next pick: i.e. move to end of current priority
	 * group (i.e. FIFO).
	 */
	if (likely(sched_class == &evl_sched_fifo))
		__evl_enqueue_fifo_thread(thread);
	else if (sched_class != &evl_sched_idle)
		sched_class->sched_enqueue(thread);
}

static __always_inline void evl_dequeue_thread(struct evl_thread *thread)
{
	struct evl_sched_class *sched_class = thread->sched_class;

	/*
	 * Pull from the runnable thread queue.
	 */
	if (likely(sched_class == &evl_sched_fifo))
		__evl_dequeue_fifo_thread(thread);
	else if (sched_class != &evl_sched_idle)
		sched_class->sched_dequeue(thread);
}

static __always_inline void evl_requeue_thread(struct evl_thread *thread)
{
	struct evl_sched_class *sched_class = thread->sched_class;

	/*
	 * Put back at same place: i.e. requeue to head of current
	 * priority group (i.e. LIFO, used for preemption handling).
	 */
	if (likely(sched_class == &evl_sched_fifo))
		__evl_requeue_fifo_thread(thread);
	else if (sched_class != &evl_sched_idle)
		sched_class->sched_requeue(thread);
}

static inline
bool evl_set_schedparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	return thread->base_class->sched_setparam(thread, p);
}

static inline void evl_get_schedparam(struct evl_thread *thread,
				      union evl_sched_param *p)
{
	thread->sched_class->sched_getparam(thread, p);
}

static inline void evl_track_priority(struct evl_thread *thread,
				      const union evl_sched_param *p)
{
	thread->sched_class->sched_trackprio(thread, p);
	thread->wprio = evl_calc_weighted_prio(thread->sched_class, thread->cprio);
}

static inline void evl_ceil_priority(struct evl_thread *thread, int prio)
{
	thread->sched_class->sched_ceilprio(thread, prio);
	thread->wprio = evl_calc_weighted_prio(thread->sched_class, thread->cprio);
}

static inline void evl_forget_thread(struct evl_thread *thread)
{
	struct evl_sched_class *sched_class = thread->base_class;

	--sched_class->nthreads;

	if (sched_class->sched_forget)
		sched_class->sched_forget(thread);
}

static inline void evl_force_thread(struct evl_thread *thread)
{
	struct evl_sched_class *sched_class = thread->base_class;

	thread->info |= T_KICKED;

	if (sched_class->sched_kick)
		sched_class->sched_kick(thread);

	evl_set_resched(thread->rq);
}

struct evl_sched_group {
#ifdef CONFIG_EVL_SCHED_QUOTA
	struct evl_quota_group quota;
#endif
	struct list_head next;
};

struct evl_sched_class *
evl_find_sched_class(union evl_sched_param *param,
		     const struct evl_sched_attrs *attrs,
		     ktime_t *tslice_r);

bool evl_sched_yield(void);

int __init evl_init_sched(void);

void __init evl_cleanup_sched(void);

#endif /* !_EVL_SCHED_H */
