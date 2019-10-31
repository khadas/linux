/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/signal.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/mmu_context.h>
#include <asm/div64.h>
#include <asm/switch_to.h>
#include <evl/sched.h>
#include <evl/thread.h>
#include <evl/timer.h>
#include <evl/memory.h>
#include <evl/clock.h>
#include <evl/tick.h>
#include <evl/monitor.h>
#include <evl/mutex.h>
#include <uapi/evl/signal.h>
#include <trace/events/evl.h>

DEFINE_PER_CPU(struct evl_rq, evl_runqueues);
EXPORT_PER_CPU_SYMBOL_GPL(evl_runqueues);

struct cpumask evl_cpu_affinity = CPU_MASK_ALL;
EXPORT_SYMBOL_GPL(evl_cpu_affinity);

static struct evl_sched_class *evl_sched_highest;

#define for_each_evl_sched_class(p)			\
	for (p = evl_sched_highest; p; p = p->next)

static void register_one_class(struct evl_sched_class *sched_class)
{
	sched_class->next = evl_sched_highest;
	evl_sched_highest = sched_class;

	/*
	 * Classes shall be registered by increasing priority order,
	 * idle first and up.
	 */
	EVL_WARN_ON(CORE, sched_class->next &&
		sched_class->next->weight > sched_class->weight);
}

static void register_classes(void)
{
	register_one_class(&evl_sched_idle);
	register_one_class(&evl_sched_weak);
#ifdef CONFIG_EVL_SCHED_QUOTA
	register_one_class(&evl_sched_quota);
#endif
#ifdef CONFIG_EVL_SCHED_TP
	register_one_class(&evl_sched_tp);
#endif
	register_one_class(&evl_sched_fifo);
}

#ifdef CONFIG_EVL_WATCHDOG

static unsigned long wd_timeout_arg = CONFIG_EVL_WATCHDOG_TIMEOUT;
module_param_named(watchdog_timeout, wd_timeout_arg, ulong, 0644);

static inline ktime_t get_watchdog_timeout(void)
{
	return ns_to_ktime(wd_timeout_arg * 1000000000ULL);
}

static void watchdog_handler(struct evl_timer *timer) /* oob stage stalled */
{
	struct evl_rq *this_rq = this_evl_rq();
	struct evl_thread *curr = this_rq->curr;

	trace_evl_watchdog_signal(curr);

	/*
	 * CAUTION: The watchdog tick might have been delayed while we
	 * were busy switching the CPU to in-band context at the
	 * trigger date eventually. Make sure that we are not about to
	 * kick the incoming root thread.
	 */
	if (curr->state & T_ROOT)
		return;

	if (curr->state & T_USER) {
		evl_spin_lock(&curr->lock);
		evl_spin_lock(&this_rq->lock);
		curr->info |= T_KICKED;
		evl_spin_unlock(&this_rq->lock);
		evl_spin_unlock(&curr->lock);
		evl_signal_thread(curr, SIGDEBUG, SIGDEBUG_WATCHDOG);
		dovetail_send_mayday(current);
		printk(EVL_WARNING "watchdog triggered on CPU #%d -- runaway thread "
			"'%s' signaled\n", evl_rq_cpu(this_rq), curr->name);
	} else {
		printk(EVL_WARNING "watchdog triggered on CPU #%d -- runaway thread "
			"'%s' canceled\n", evl_rq_cpu(this_rq), curr->name);
		/*
		 * On behalf on an IRQ handler, evl_cancel_thread()
		 * would go half way cancelling the preempted
		 * thread. Therefore we manually raise T_KICKED to
		 * cause the next blocking call to return early in
		 * T_BREAK condition, and T_CANCELD so that @curr
		 * exits next time it invokes evl_test_cancel().
		 */
		evl_spin_lock(&curr->lock);
		evl_spin_lock(&this_rq->lock);
		curr->info |= (T_KICKED|T_CANCELD);
		evl_spin_unlock(&this_rq->lock);
		evl_spin_unlock(&curr->lock);
	}
}

#endif /* CONFIG_EVL_WATCHDOG */

static void roundrobin_handler(struct evl_timer *timer) /* hard irqs off */
{
	struct evl_rq *this_rq;

	this_rq = container_of(timer, struct evl_rq, rrbtimer);
	evl_spin_lock(&this_rq->lock);
	evl_sched_tick(this_rq);
	evl_spin_unlock(&this_rq->lock);
}

static void init_rq(struct evl_rq *rq, int cpu)
{
	struct evl_sched_class *sched_class;
	struct evl_init_thread_attr iattr;
	const char *name_fmt;

#ifdef CONFIG_SMP
	rq->cpu = cpu;
	name_fmt = "ROOT/%u";
	rq->proxy_timer_name = kasprintf(GFP_KERNEL, "[proxy-timer/%u]", cpu);
	rq->rrb_timer_name = kasprintf(GFP_KERNEL, "[rrb-timer/%u]", cpu);
	cpumask_clear(&rq->resched_cpus);
#else
	name_fmt = "ROOT";
	rq->proxy_timer_name = kstrdup("[proxy-timer]", GFP_KERNEL);
	rq->rrb_timer_name = kstrdup("[rrb-timer]", GFP_KERNEL);
#endif
	evl_spin_lock_init(&rq->lock);

	for_each_evl_sched_class(sched_class) {
		if (sched_class->sched_init)
			sched_class->sched_init(rq);
	}

	rq->flags = 0;
	rq->local_flags = RQ_IDLE;
	rq->curr = &rq->root_thread;

	/*
	 * No handler needed for the inband timer since proxy timer
	 * events are handled specifically by the generic timer code
	 * (do_clock_tick()).
	 */
	evl_init_timer_on_rq(&rq->inband_timer, &evl_mono_clock, NULL,
			rq, EVL_TIMER_IGRAVITY);
	evl_set_timer_priority(&rq->inband_timer, EVL_TIMER_LOPRIO);
	evl_set_timer_name(&rq->inband_timer, rq->proxy_timer_name);
	evl_init_timer_on_rq(&rq->rrbtimer, &evl_mono_clock, roundrobin_handler,
			rq, EVL_TIMER_IGRAVITY);
	evl_set_timer_name(&rq->rrbtimer, rq->rrb_timer_name);
	evl_set_timer_priority(&rq->rrbtimer, EVL_TIMER_LOPRIO);
#ifdef CONFIG_EVL_WATCHDOG
	evl_init_timer_on_rq(&rq->wdtimer, &evl_mono_clock, watchdog_handler,
			rq, EVL_TIMER_IGRAVITY);
	evl_set_timer_name(&rq->wdtimer, "[watchdog]");
	evl_set_timer_priority(&rq->wdtimer, EVL_TIMER_LOPRIO);
#endif /* CONFIG_EVL_WATCHDOG */

	evl_set_current_account(rq, &rq->root_thread.stat.account);

	/*
	 * Postpone evl_init_thread() - which sets RQ_SCHED upon
	 * setting the schedparams for the root thread - until we have
	 * enough of the runqueue initialized, so that attempting to
	 * reschedule from evl_exit_irq() later on is harmless.
	 */
	iattr.flags = T_ROOT;
	iattr.affinity = *cpumask_of(cpu);
	iattr.sched_class = &evl_sched_idle;
	iattr.sched_param.idle.prio = EVL_IDLE_PRIO;
	evl_init_thread(&rq->root_thread, &iattr, rq, name_fmt, cpu);

	dovetail_init_altsched(&rq->root_thread.altsched);

	list_add_tail(&rq->root_thread.next, &evl_thread_list);
	evl_nrthreads++;
}

static void destroy_rq(struct evl_rq *rq)
{
	evl_destroy_timer(&rq->inband_timer);
	evl_destroy_timer(&rq->rrbtimer);
	kfree(rq->proxy_timer_name);
	kfree(rq->rrb_timer_name);
	evl_destroy_timer(&rq->root_thread.ptimer);
	evl_destroy_timer(&rq->root_thread.rtimer);
#ifdef CONFIG_EVL_WATCHDOG
	evl_destroy_timer(&rq->wdtimer);
#endif /* CONFIG_EVL_WATCHDOG */
}

#ifdef CONFIG_EVL_DEBUG_CORE

void evl_disable_preempt(void)
{
	__evl_disable_preempt();
}
EXPORT_SYMBOL(evl_disable_preempt);

void evl_enable_preempt(void)
{
	__evl_enable_preempt();
}
EXPORT_SYMBOL(evl_enable_preempt);

#endif /* CONFIG_EVL_DEBUG_CORE */

#ifdef CONFIG_SMP

static inline
void evl_double_rq_lock(struct evl_rq *rq1, struct evl_rq *rq2)
{
	EVL_WARN_ON_ONCE(CORE, !oob_irqs_disabled());

	/* Prevent ABBA deadlock, always lock rqs in address order. */

	if (rq1 == rq2) {
		evl_spin_lock(&rq1->lock);
	} else if (rq1 < rq2) {
		evl_spin_lock(&rq1->lock);
		evl_spin_lock_nested(&rq2->lock, SINGLE_DEPTH_NESTING);
	} else {
		evl_spin_lock(&rq2->lock);
		evl_spin_lock_nested(&rq1->lock, SINGLE_DEPTH_NESTING);
	}
}

static inline
void evl_double_rq_unlock(struct evl_rq *rq1, struct evl_rq *rq2)
{
	evl_spin_unlock(&rq1->lock);
	if (rq1 != rq2)
		evl_spin_unlock(&rq2->lock);
}

static void migrate_rq(struct evl_thread *thread, struct evl_rq *dst_rq)
{
	struct evl_sched_class *sched_class = thread->sched_class;
	struct evl_rq *src_rq = thread->rq;

	evl_double_rq_lock(src_rq, dst_rq);

	if (thread->state & T_READY) {
		evl_dequeue_thread(thread);
		thread->state &= ~T_READY;
	}

	if (sched_class->sched_migrate)
		sched_class->sched_migrate(thread, dst_rq);
	/*
	 * WARNING: the scheduling class may have just changed as a
	 * result of calling the per-class migration hook.
	 */
	thread->rq = dst_rq;

	if (!(thread->state & EVL_THREAD_BLOCK_BITS)) {
		evl_requeue_thread(thread);
		thread->state |= T_READY;
		evl_set_resched(dst_rq);
		evl_set_resched(src_rq);
	}

	evl_double_rq_unlock(src_rq, dst_rq);
}

/* thread->lock held, oob stalled. @thread must not be running oob. */
void evl_migrate_thread(struct evl_thread *thread, struct evl_rq *dst_rq)
{
	assert_evl_lock(&thread->lock);

	if (thread->rq == dst_rq)
		return;

	trace_evl_thread_migrate(thread, evl_rq_cpu(dst_rq));

	/*
	 * Timer migration is postponed until the next timeout happens
	 * for the periodic and rrb timers. The resource/periodic
	 * timer will be moved to the right CPU next time
	 * evl_prepare_timed_wait() is called for it (via
	 * evl_sleep_on()).
	 */
	migrate_rq(thread, dst_rq);

	evl_reset_account(&thread->stat.lastperiod);
}

static bool check_cpu_affinity(struct task_struct *p) /* inband, oob stage stalled */
{
	struct evl_thread *thread = evl_thread_from_task(p);
	int cpu = task_cpu(p);
	struct evl_rq *rq = evl_cpu_rq(cpu);
	bool ret = true;

	evl_spin_lock(&thread->lock);

	/*
	 * To maintain consistency between both the EVL and in-band
	 * schedulers, reflecting a thread migration to another CPU
	 * into EVL's scheduler state must happen from in-band context
	 * only, on behalf of the migrated thread itself once it runs
	 * on the target CPU.
	 *
	 * This means that the EVL scheduler state regarding the CPU
	 * information lags behind the in-band scheduler state until
	 * the migrated thread switches back to OOB context
	 * (i.e. task_cpu(p) !=
	 * evl_rq_cpu(evl_thread_from_task(p)->rq)).  This is ok since
	 * EVL will not schedule such thread until then.
	 *
	 * check_cpu_affinity() detects when a EVL thread switching back to
	 * OOB context did move to another CPU earlier while running
	 * in-band. If so, do the fixups to reflect the change.
	 */
	if (unlikely(!is_threading_cpu(cpu))) {
		printk(EVL_WARNING "thread %s[%d] switched to non-rt CPU%d, aborted.\n",
			thread->name, evl_get_inband_pid(thread), cpu);
		/*
		 * Can't call evl_cancel_thread() from a CPU migration
		 * point, that would break. Since we are on the wakeup
		 * path to OOB context, just raise T_CANCELD to catch
		 * it in evl_switch_oob().
		 */
		evl_spin_lock(&thread->rq->lock);
		thread->info |= T_CANCELD;
		evl_spin_unlock(&thread->rq->lock);
		ret = false;
		goto out;
	}

	if (likely(rq == thread->rq))
		goto out;

	/*
	 * If the current thread moved to a supported out-of-band CPU,
	 * which is not part of its original affinity mask, assume
	 * user wants to extend this mask.
	 */
	if (!cpumask_test_cpu(cpu, &thread->affinity))
		cpumask_set_cpu(cpu, &thread->affinity);

	evl_migrate_thread(thread, rq);
out:
	evl_spin_unlock(&thread->lock);

	return ret;
}

#else

#define evl_double_rq_lock(__rq1, __rq2)  \
	EVL_WARN_ON_ONCE(CORE, !oob_irqs_disabled());

#define evl_double_rq_unlock(__rq1, __rq2)  do { } while (0)

static inline bool check_cpu_affinity(struct task_struct *p)
{
	return true;
}

#endif	/* CONFIG_SMP */

/* thread->lock + thread->rq->lock held, irqs off. */
void evl_putback_thread(struct evl_thread *thread)
{
	assert_evl_lock(&thread->lock);
	assert_evl_lock(&thread->rq->lock);

	if (thread->state & T_READY)
		evl_dequeue_thread(thread);
	else
		thread->state |= T_READY;

	evl_enqueue_thread(thread);
	evl_set_resched(thread->rq);
}

/* thread->lock + thread->rq->lock held, irqs off. */
int evl_set_thread_policy_locked(struct evl_thread *thread,
				struct evl_sched_class *sched_class,
				const union evl_sched_param *p)
{
	struct evl_sched_class *orig_effective_class __maybe_unused;
	bool effective;
	int ret;

	assert_evl_lock(&thread->lock);
	assert_evl_lock(&thread->rq->lock);

	/* Check parameters early on. */
	ret = evl_check_schedparams(sched_class, thread, p);
	if (ret)
		return ret;

	/*
	 * Declaring a thread to a new scheduling class may fail, so
	 * we do that early, while the thread is still a member of the
	 * previous class. However, this also means that the
	 * declaration callback shall not do anything that might
	 * affect the previous class (such as touching thread->rq_next
	 * for instance).
	 */
	if (sched_class != thread->base_class) {
		ret = evl_declare_thread(sched_class, thread, p);
		if (ret)
			return ret;
	}

	/*
	 * As a special case, we may be called from evl_init_thread()
	 * with no previous scheduling class at all.
	 */
	if (likely(thread->base_class != NULL)) {
		if (thread->state & T_READY)
			evl_dequeue_thread(thread);

		if (sched_class != thread->base_class)
			evl_forget_thread(thread);
	}

	/*
	 * Set the base and effective scheduling parameters. However,
	 * evl_set_schedparam() will deny lowering the effective
	 * priority if a boost is undergoing, only recording the
	 * change into the base priority field in such situation.
	 */
	thread->base_class = sched_class;
	/*
	 * Referring to the effective class from a setparam() handler
	 * is wrong: make sure to break if so.
	 */
	if (EVL_DEBUG(CORE)) {
		orig_effective_class = thread->sched_class;
		thread->sched_class = NULL;
	}

	/*
	 * This is the ONLY place where calling
	 * evl_set_schedparam() is legit, sane and safe.
	 */
	effective = evl_set_schedparam(thread, p);
	if (effective) {
		thread->sched_class = sched_class;
		thread->wprio = evl_calc_weighted_prio(sched_class, thread->cprio);
	} else if (EVL_DEBUG(CORE))
		thread->sched_class = orig_effective_class;

	if (thread->state & T_READY)
		evl_enqueue_thread(thread);

	/*
	 * Make sure not to raise RQ_SCHED when setting up the root
	 * thread, so that we can't start rescheduling from
	 * evl_exit_irq() before all CPUs have their runqueue fully
	 * built. Filtering on T_ROOT here is correct because the root
	 * thread enters the idle class once as part of the runqueue
	 * setup process and never leaves it afterwards.
	 */
	if (!(thread->state & (T_DORMANT|T_ROOT)))
		evl_set_resched(thread->rq);
	else
		EVL_WARN_ON(CORE, (thread->state & T_ROOT) &&
			sched_class != &evl_sched_idle);
	return 0;
}

int evl_set_thread_policy(struct evl_thread *thread,
			struct evl_sched_class *sched_class,
			const union evl_sched_param *p)
{
	unsigned long flags;
	struct evl_rq *rq;
	int ret;

	rq = evl_get_thread_rq(thread, flags);
	ret = evl_set_thread_policy_locked(thread, sched_class, p);
	evl_put_thread_rq(thread, rq, flags);

	return ret;
}

/* thread->lock + thread->rq->lock held, irqs off. */
bool evl_set_effective_thread_priority(struct evl_thread *thread, int prio)
{
	int wprio = evl_calc_weighted_prio(thread->base_class, prio);

	assert_evl_lock(&thread->lock);
	assert_evl_lock(&thread->rq->lock);

	thread->bprio = prio;
	if (wprio == thread->wprio)
		return true;

	/*
	 * We may not lower the effective/current priority of a
	 * boosted thread when changing the base scheduling
	 * parameters. Only evl_track_thread_policy() and
	 * evl_protect_thread_priority() may do so when dealing with PI
	 * and PP synchs resp.
	 */
	if (wprio < thread->wprio && (thread->state & T_BOOST))
		return false;

	thread->cprio = prio;

	trace_evl_thread_set_current_prio(thread);

	return true;
}

/* thread->lock + target->lock held, irqs off */
void evl_track_thread_policy(struct evl_thread *thread,
			struct evl_thread *target)
{
	union evl_sched_param param;

	assert_evl_lock(&thread->lock);
	assert_evl_lock(&target->lock);
	no_ugly_lock();

	evl_double_rq_lock(thread->rq, target->rq);

	/*
	 * Inherit (or reset) the effective scheduling class and
	 * priority of a thread. Unlike evl_set_thread_policy(), this
	 * routine is allowed to lower the weighted priority with no
	 * restriction, even if a boost is undergoing.
	 */
	if (thread->state & T_READY)
		evl_dequeue_thread(thread);
	/*
	 * Self-targeting means to reset the scheduling policy and
	 * parameters to the base settings. Otherwise, make thread
	 * inherit the scheduling parameters from target.
	 */
	if (target == thread) {
		thread->sched_class = thread->base_class;
		evl_track_priority(thread, NULL);
		/*
		 * Per SuSv2, resetting the base scheduling parameters
		 * should not move the thread to the tail of its
		 * priority group, which makes sense.
		 */
		if (thread->state & T_READY)
			evl_requeue_thread(thread);

	} else {
		evl_get_schedparam(target, &param);
		thread->sched_class = target->sched_class;
		evl_track_priority(thread, &param);
		if (thread->state & T_READY)
			evl_enqueue_thread(thread);
	}

	trace_evl_thread_set_current_prio(thread);

	evl_set_resched(thread->rq);

	evl_double_rq_unlock(thread->rq, target->rq);
}

/* thread->lock, irqs off */
void evl_protect_thread_priority(struct evl_thread *thread, int prio)
{
	assert_evl_lock(&thread->lock);
	no_ugly_lock();

	evl_spin_lock(&thread->rq->lock);

	/*
	 * Apply a PP boost by changing the effective priority of a
	 * thread, forcing it to the FIFO class. Like
	 * evl_track_thread_policy(), this routine is allowed to lower
	 * the weighted priority with no restriction, even if a boost
	 * is undergoing.
	 *
	 * This routine only deals with active boosts, resetting the
	 * base priority when leaving a PP boost is obtained by a call
	 * to evl_track_thread_policy().
	 */
	if (thread->state & T_READY)
		evl_dequeue_thread(thread);

	thread->sched_class = &evl_sched_fifo;
	evl_ceil_priority(thread, prio);

	if (thread->state & T_READY)
		evl_enqueue_thread(thread);

	trace_evl_thread_set_current_prio(thread);

	evl_set_resched(thread->rq);

	evl_spin_unlock(&thread->rq->lock);
}

void evl_init_schedq(struct evl_multilevel_queue *q)
{
	int prio;

	q->elems = 0;
	bitmap_zero(q->prio_map, EVL_MLQ_LEVELS);

	for (prio = 0; prio < EVL_MLQ_LEVELS; prio++)
		INIT_LIST_HEAD(q->heads + prio);
}

struct evl_thread *evl_get_schedq(struct evl_multilevel_queue *q)
{
	struct evl_thread *thread;
	struct list_head *head;
	int idx;

	if (evl_schedq_is_empty(q))
		return NULL;

	idx = evl_get_schedq_weight(q);
	head = q->heads + idx;
	thread = list_first_entry(head, struct evl_thread, rq_next);
	__evl_del_schedq(q, &thread->rq_next, idx);

	return thread;
}

struct evl_thread *
evl_lookup_schedq(struct evl_multilevel_queue *q, int prio)
{
	struct list_head *head;
	int idx;

	idx = get_qindex(q, prio);
	head = q->heads + idx;
	if (list_empty(head))
		return NULL;

	return list_first_entry(head, struct evl_thread, rq_next);
}

struct evl_thread *evl_fifo_pick(struct evl_rq *rq)
{
	struct evl_multilevel_queue *q = &rq->fifo.runnable;
	struct evl_thread *thread;
	struct list_head *head;
	int idx;

	if (evl_schedq_is_empty(q))
		return NULL;

	/*
	 * Some scheduling policies may be implemented as variants of
	 * the core SCHED_FIFO class, sharing its runqueue
	 * (e.g. SCHED_QUOTA). This means that we have to do some
	 * cascading to call the right pick handler eventually.
	 */
	idx = evl_get_schedq_weight(q);
	head = q->heads + idx;

	/*
	 * The active class (i.e. ->sched_class) is the one currently
	 * queuing the thread, reflecting any priority boost due to
	 * PI.
	 */
	thread = list_first_entry(head, struct evl_thread, rq_next);
	if (unlikely(thread->sched_class != &evl_sched_fifo))
		return thread->sched_class->sched_pick(rq);

	__evl_del_schedq(q, &thread->rq_next, idx);

	return thread;
}

static inline void enter_inband(struct evl_thread *root)
{
#ifdef CONFIG_EVL_WATCHDOG
	evl_stop_timer(&evl_thread_rq(root)->wdtimer);
#endif
}

static inline void leave_inband(struct evl_thread *root)
{
#ifdef CONFIG_EVL_WATCHDOG
	evl_start_timer(&evl_thread_rq(root)->wdtimer,
			evl_abs_timeout(&evl_thread_rq(root)->wdtimer,
					get_watchdog_timeout()),
			EVL_INFINITE);
#endif
}

/* oob stalled. */
static irqreturn_t reschedule_interrupt(int irq, void *dev_id)
{
	trace_evl_reschedule_ipi(this_evl_rq());

	/* Will reschedule from evl_exit_irq(). */

	return IRQ_HANDLED;
}

static inline void set_next_running(struct evl_rq *rq,
				struct evl_thread *next)
{
	next->state &= ~T_READY;
	if (next->state & T_RRB)
		evl_start_timer(&rq->rrbtimer,
				evl_abs_timeout(&rq->rrbtimer, next->rrperiod),
				EVL_INFINITE);
	else
		evl_stop_timer(&rq->rrbtimer);
}

/* rq->curr->lock + rq->lock held, irqs off. */
static struct evl_thread *pick_next_thread(struct evl_rq *rq)
{
	struct evl_sched_class *sched_class;
	struct evl_thread *curr = rq->curr;
	struct evl_thread *next;

	/*
	 * We have to switch the current thread out if a blocking
	 * condition is raised for it. Otherwise, check whether
	 * preemption is allowed.
	 */
	if (!(curr->state & (EVL_THREAD_BLOCK_BITS | T_ZOMBIE))) {
		if (evl_preempt_count() > 0) {
			evl_set_self_resched(rq);
			return curr;
		}
		/*
		 * Push the current thread back to the run queue of
		 * the scheduling class it belongs to, if not yet
		 * linked to it (T_READY tells us if it is).
		 */
		if (!(curr->state & T_READY)) {
			evl_requeue_thread(curr);
			curr->state |= T_READY;
		}
	}

	/*
	 * Find the next runnable thread having the highest priority
	 * amongst all scheduling classes, scanned by decreasing
	 * priority.
	 */
	for_each_evl_sched_class(sched_class) {
		next = sched_class->sched_pick(rq);
		if (likely(next)) {
			set_next_running(rq, next);
			return next;
		}
	}

	return NULL; /* NOT REACHED (idle class). */
}

static inline void prepare_rq_switch(struct evl_rq *this_rq,
				struct evl_thread *next)
{
	if (irq_pipeline_debug_locking())
		spin_release(&this_rq->lock._lock.rlock.dep_map,
			_THIS_IP_);
#ifdef CONFIG_DEBUG_SPINLOCK
	this_rq->lock._lock.rlock.owner = next->altsched.task;
#endif
}

static inline void finish_rq_switch(bool inband_tail, unsigned long flags)
{
	struct evl_rq *this_rq = this_evl_rq();

	EVL_WARN_ON(CORE, this_rq->curr->state & EVL_THREAD_BLOCK_BITS);

	/*
	 * Check whether we are completing a transition to the inband
	 * stage for the current task, i.e.:
	 *
	 * irq_work_queue() ->
	 *        IRQ:wake_up_process() ->
	 *                         schedule() ->
	 *                               back from dovetail_context_switch()
	 */
	if (likely(!inband_tail)) {
		if (irq_pipeline_debug_locking())
			spin_acquire(&this_rq->lock._lock.rlock.dep_map,
				0, 0, _THIS_IP_);
		evl_spin_unlock_irqrestore(&this_rq->lock, flags);
	}
}

static inline void finish_rq_switch_from_inband(void)
{
	struct evl_rq *this_rq = this_evl_rq();

	assert_evl_lock(&this_rq->lock);

	if (irq_pipeline_debug_locking())
		spin_acquire(&this_rq->lock._lock.rlock.dep_map,
			0, 0, _THIS_IP_);

	evl_spin_unlock_irq(&this_rq->lock);
}

/* oob stalled. */
static inline bool test_resched(struct evl_rq *this_rq)
{
	bool need_resched = evl_need_resched(this_rq);

#ifdef CONFIG_SMP
	/* Send resched IPI to remote CPU(s). */
	if (unlikely(!cpumask_empty(&this_rq->resched_cpus))) {
		irq_pipeline_send_remote(RESCHEDULE_OOB_IPI,
					&this_rq->resched_cpus);
		cpumask_clear(&this_rq->resched_cpus);
		this_rq->local_flags &= ~RQ_SCHED;
	}
#endif
	if (need_resched)
		this_rq->flags &= ~RQ_SCHED;

	return need_resched;
}

/*
 * CAUTION: curr->altsched.task may be unsynced and even stale if curr
 * == &this_rq->root_thread, since the task logged by leave_inband()
 * may not still be the current one. Use "current" for disambiguating
 * if you need to refer to the underlying inband task.
 */
void __evl_schedule(void) /* oob or oob stalled (CPU migration-safe) */
{
	struct evl_rq *this_rq = this_evl_rq();
	struct evl_thread *prev, *next, *curr;
	bool leaving_inband, inband_tail;
	unsigned long flags;

	no_ugly_lock();

	if (EVL_WARN_ON_ONCE(CORE, running_inband() && !oob_irqs_disabled()))
		return;

	trace_evl_schedule(this_rq);

	flags = oob_irq_save();

	/*
	 * Check whether we have a pending priority ceiling request to
	 * commit before putting the current thread to sleep.
	 * evl_current() may differ from rq->curr only if rq->curr ==
	 * &rq->root_thread. Testing T_USER eliminates this case since
	 * a root thread never bears this bit.
	 */
	curr = this_rq->curr;
	if (curr->state & T_USER)
		evl_commit_monitor_ceiling();

	/*
	 * Only holding this_rq->lock is required for test_resched(),
	 * but we grab curr->lock in advance in order to keep the
	 * locking order safe from ABBA deadlocking.
	 */
	evl_spin_lock(&curr->lock);
	evl_spin_lock(&this_rq->lock);

	if (unlikely(!test_resched(this_rq))) {
		evl_spin_unlock(&this_rq->lock);
		evl_spin_unlock_irqrestore(&curr->lock, flags);
		return;
	}

	next = pick_next_thread(this_rq);
	if (next == curr) {
		if (unlikely(next->state & T_ROOT)) {
			if (this_rq->local_flags & RQ_TPROXY)
				evl_notify_proxy_tick(this_rq);
			if (this_rq->local_flags & RQ_TDEFER)
				evl_program_local_tick(&evl_mono_clock);
		}
		evl_spin_unlock(&this_rq->lock);
		evl_spin_unlock_irqrestore(&curr->lock, flags);
		return;
	}

	prev = curr;
	trace_evl_switch_context(prev, next);
	this_rq->curr = next;
	leaving_inband = false;

	if (prev->state & T_ROOT) {
		leave_inband(prev);
		leaving_inband = true;
	} else if (next->state & T_ROOT) {
		if (this_rq->local_flags & RQ_TPROXY)
			evl_notify_proxy_tick(this_rq);
		if (this_rq->local_flags & RQ_TDEFER)
			evl_program_local_tick(&evl_mono_clock);
		enter_inband(next);
	}

	evl_switch_account(this_rq, &next->stat.account);
	evl_inc_counter(&next->stat.csw);
	evl_spin_unlock(&prev->lock);

	prepare_rq_switch(this_rq, next);
	inband_tail = dovetail_context_switch(&prev->altsched,
					&next->altsched, leaving_inband);
	finish_rq_switch(inband_tail, flags);
}
EXPORT_SYMBOL_GPL(__evl_schedule);

void resume_oob_task(struct task_struct *p) /* inband, oob stage stalled */
{
	struct evl_thread *thread = evl_thread_from_task(p);

	if (check_cpu_affinity(p))
		evl_release_thread(thread, T_INBAND, 0);

	evl_schedule();
}

int evl_switch_oob(void)
{
	struct task_struct *p = current;
	struct evl_thread *curr;
	int ret;

	inband_context_only();

	curr = evl_current();
	if (curr == NULL)
		return -EPERM;

	if (signal_pending(p))
		return -ERESTARTSYS;

	trace_evl_switching_oob(curr);

	evl_clear_sync_uwindow(curr, T_INBAND);

	ret = dovetail_leave_inband();
	if (ret) {
		evl_test_cancel();
		evl_set_sync_uwindow(curr, T_INBAND);
		return ret;
	}

	/*
	 * The current task is now running on the out-of-band
	 * execution stage, scheduled in by the latest call to
	 * __evl_schedule() on this CPU: we must be holding the
	 * runqueue lock and the oob stage must be stalled.
	 */
	oob_context_only();
	finish_rq_switch_from_inband();
	evl_test_cancel();

	trace_evl_switched_oob(curr);

	/*
	 * Recheck pending signals once again. As we block task
	 * wakeups during the stage transition and handle_sigwake_event()
	 * ignores signals until T_INBAND is cleared, any signal in
	 * between is just silently queued up to here.
	 */
	if (signal_pending(p)) {
		evl_switch_inband(!(curr->state & T_SSTEP) ?
				SIGDEBUG_MIGRATE_SIGNAL:
				SIGDEBUG_NONE);
		return -ERESTARTSYS;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(evl_switch_oob);

void evl_switch_inband(int cause)
{
	struct evl_thread *curr = evl_current();
	struct task_struct *p = current;
	struct kernel_siginfo si;
	struct evl_rq *rq;

	oob_context_only();

	trace_evl_switching_inband(cause);

	/*
	 * This is the only location where we may assert T_INBAND for
	 * a thread. Basic assumption: switching to the inband stage
	 * only applies to the current thread running out-of-band on
	 * this CPU.
	 *
	 * CAVEAT: dovetail_leave_oob() must run _before_ the in-band
	 * kernel is allowed to take interrupts again, so that
	 * try_to_wake_up() does not block the wake up request for the
	 * switching thread as a result of testing task_is_off_stage().
	 */
	oob_irq_disable();
	irq_work_queue(&curr->inband_work);
	evl_spin_lock(&curr->lock);
	rq = curr->rq;
	evl_spin_lock(&rq->lock);
	if (curr->state & T_READY) {
		evl_dequeue_thread(curr);
		curr->state &= ~T_READY;
	}
	curr->info &= ~EVL_THREAD_INFO_MASK;
	curr->state |= T_INBAND;
	curr->local_info &= ~T_SYSRST;
	evl_set_resched(rq);
	dovetail_leave_oob();
	evl_spin_unlock(&rq->lock);
	evl_spin_unlock(&curr->lock);
	__evl_schedule();
	/*
	 * this_rq()->lock was released when the root thread resumed
	 * from __evl_schedule() (i.e. inband_tail path).
	 */
	oob_irq_enable();
	dovetail_resume_inband();

	/*
	 * Basic sanity check after an expected transition to in-band
	 * context.
	 */
	EVL_WARN(CORE, !running_inband(),
		"evl_switch_inband() failed for thread %s[%d]",
		curr->name, evl_get_inband_pid(curr));

	/* Account for switch to in-band context. */
	evl_inc_counter(&curr->stat.isw);

	trace_evl_switched_inband(curr);

	/*
	 * When switching to in-band context, we check for propagating
	 * the current EVL schedparams that might have been set for
	 * current while running in OOB context.
	 *
	 * CAUTION: This obviously won't update the schedparams cached
	 * by the glibc for the caller in user-space, but this is the
	 * deal: we don't switch threads which issue
	 * EVL_THRIOC_SET_SCHEDPARAM to in-band mode, but then only
	 * the kernel side will be aware of the change, and glibc
	 * might cache obsolete information.
	 */
	evl_propagate_schedparam_change(curr);

	if ((curr->state & T_USER) && cause != SIGDEBUG_NONE) {
		/*
		 * Help debugging spurious stage switches by sending
		 * SIGDEBUG. We are running inband on the context of
		 * the receiver, so we may bypass evl_signal_thread()
		 * for this.
		 */
		if (curr->state & T_WOSS) {
			memset(&si, 0, sizeof(si));
			si.si_signo = SIGDEBUG;
			si.si_code = SI_QUEUE;
			si.si_int = cause | sigdebug_marker;
			send_sig_info(SIGDEBUG, &si, p);
		}
		/* May check for locking inconsistency too. */
		if (curr->state & T_WOLI)
			evl_detect_boost_drop(curr);
	}

	/* @curr is now running inband. */
	evl_sync_uwindow(curr);
}
EXPORT_SYMBOL_GPL(evl_switch_inband);

struct evl_sched_class *
evl_find_sched_class(union evl_sched_param *param,
		const struct evl_sched_attrs *attrs,
		ktime_t *tslice_r)
{
	struct evl_sched_class *sched_class;
	int prio, policy;
	ktime_t tslice;

	policy = attrs->sched_policy;
	prio = attrs->sched_priority;
	tslice = EVL_INFINITE;
	sched_class = &evl_sched_fifo;
	param->fifo.prio = prio;

	switch (policy) {
	case SCHED_NORMAL:
		if (prio)
			return NULL;
		/* fall-through */
	case SCHED_WEAK:
		if (prio < EVL_WEAK_MIN_PRIO ||	prio > EVL_WEAK_MAX_PRIO)
			return NULL;
		param->weak.prio = prio;
		sched_class = &evl_sched_weak;
		break;
	case SCHED_RR:
		/* if unspecified, use current one. */
		tslice = timespec_to_ktime(attrs->sched_rr_quantum);
		if (timeout_infinite(tslice) && tslice_r)
			tslice = *tslice_r;
		/* fall-through */
	case SCHED_FIFO:
		/*
		 * This routine handles requests submitted from
		 * user-space exclusively, so a SCHED_FIFO priority
		 * must be in the [FIFO_MIN..FIFO_MAX] range.
		 */
		if (prio < EVL_FIFO_MIN_PRIO ||	prio > EVL_FIFO_MAX_PRIO)
			return NULL;
		break;
#ifdef CONFIG_EVL_SCHED_QUOTA
	case SCHED_QUOTA:
		param->quota.prio = attrs->sched_priority;
		param->quota.tgid = attrs->sched_quota_group;
		sched_class = &evl_sched_quota;
		break;
#endif
#ifdef CONFIG_EVL_SCHED_TP
	case SCHED_TP:
		param->tp.prio = attrs->sched_priority;
		param->tp.ptid = attrs->sched_tp_partition;
		sched_class = &evl_sched_tp;
		break;
#endif
	default:
		return NULL;
	}

	*tslice_r = tslice;

	return sched_class;
}

#ifdef CONFIG_TRACING

const char *evl_trace_sched_attrs(struct trace_seq *p,
				struct evl_sched_attrs *attrs)
{
	const char *ret = trace_seq_buffer_ptr(p);

	switch (attrs->sched_policy) {
	case SCHED_QUOTA:
		trace_seq_printf(p, "priority=%d, group=%d",
				attrs->sched_priority,
				attrs->sched_quota_group);
		break;
	case SCHED_TP:
		trace_seq_printf(p, "priority=%d, partition=%d",
				attrs->sched_priority,
				attrs->sched_tp_partition);
	case SCHED_NORMAL:
		break;
	case SCHED_RR:
	case SCHED_FIFO:
	case SCHED_WEAK:
	default:
		trace_seq_printf(p, "priority=%d", attrs->sched_priority);
		break;
	}
	trace_seq_putc(p, '\0');

	return ret;
}

#endif /* CONFIG_TRACING */

/* in-band stage, hard_irqs_disabled() */
bool irq_cpuidle_control(struct cpuidle_device *dev,
			struct cpuidle_state *state)
{
	/*
	 * Deny entering sleep state if this entails stopping the
	 * timer (i.e. C3STOP misfeature).
	 */
	if (state && (state->flags & CPUIDLE_FLAG_TIMER_STOP))
		return false;

	return true;
}

int __init evl_init_sched(void)
{
	struct evl_rq *rq;
	int ret, cpu;

	register_classes();

	for_each_online_cpu(cpu) {
		rq = &per_cpu(evl_runqueues, cpu);
		init_rq(rq, cpu);
	}

	if (IS_ENABLED(CONFIG_SMP)) {
		ret = __request_percpu_irq(RESCHEDULE_OOB_IPI,
					reschedule_interrupt,
					IRQF_OOB,
					"EVL reschedule",
					&evl_machine_cpudata);
		if (ret)
			goto cleanup_rq;
	}

	return 0;

cleanup_rq:
	for_each_online_cpu(cpu) {
		rq = evl_cpu_rq(cpu);
		destroy_rq(rq);
	}

	return ret;
}

void __init evl_cleanup_sched(void)
{
	struct evl_rq *rq;
	int cpu;

	if (IS_ENABLED(CONFIG_SMP))
		free_percpu_irq(RESCHEDULE_OOB_IPI, &evl_machine_cpudata);

	for_each_online_cpu(cpu) {
		rq = evl_cpu_rq(cpu);
		destroy_rq(rq);
	}
}
