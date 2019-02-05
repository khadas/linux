/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
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
#include <evenless/sched.h>
#include <evenless/thread.h>
#include <evenless/timer.h>
#include <evenless/memory.h>
#include <evenless/clock.h>
#include <evenless/tick.h>
#include <evenless/monitor.h>
#include <uapi/evenless/signal.h>
#include <trace/events/evenless.h>

DEFINE_PER_CPU(struct evl_rq, evl_runqueues);
EXPORT_PER_CPU_SYMBOL_GPL(evl_runqueues);

struct cpumask evl_cpu_affinity = CPU_MASK_ALL;
EXPORT_SYMBOL_GPL(evl_cpu_affinity);

LIST_HEAD(evl_thread_list);

int evl_nrthreads;

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
#ifdef CONFIG_EVENLESS_SCHED_QUOTA
	register_one_class(&evl_sched_quota);
#endif
	register_one_class(&evl_sched_rt);
}

#ifdef CONFIG_EVENLESS_WATCHDOG

static unsigned long wd_timeout_arg = CONFIG_EVENLESS_WATCHDOG_TIMEOUT;
module_param_named(watchdog_timeout, wd_timeout_arg, ulong, 0644);

static inline ktime_t get_watchdog_timeout(void)
{
	return ns_to_ktime(wd_timeout_arg * 1000000000ULL);
}

static void watchdog_handler(struct evl_timer *timer) /* hard irqs off */
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
		printk(EVL_WARNING "watchdog triggered on CPU #%d -- runaway thread "
			"'%s' signaled\n", evl_rq_cpu(this_rq), curr->name);
		evl_call_mayday(curr, SIGDEBUG_WATCHDOG);
	} else {
		printk(EVL_WARNING "watchdog triggered on CPU #%d -- runaway thread "
			"'%s' canceled\n", evl_rq_cpu(this_rq), curr->name);
		/*
		 * On behalf on an IRQ handler, evl_cancel_thread()
		 * would go half way cancelling the preempted
		 * thread. Therefore we manually raise T_KICKED to
		 * cause the next blockig call to return early in
		 * T_BREAK condition, and T_CANCELD so that @curr
		 * exits next time it invokes evl_test_cancel().
		 */
		xnlock_get(&nklock);
		curr->info |= (T_KICKED|T_CANCELD);
		xnlock_put(&nklock);
	}
}

#endif /* CONFIG_EVENLESS_WATCHDOG */

static void roundrobin_handler(struct evl_timer *timer) /* hard irqs off */
{
	struct evl_rq *this_rq;

	this_rq = container_of(timer, struct evl_rq, rrbtimer);
	xnlock_get(&nklock);
	evl_sched_tick(this_rq);
	xnlock_put(&nklock);
}

static void proxy_tick_handler(struct evl_timer *timer) /* hard irqs off */
{
	struct evl_rq *this_rq;

	/*
	 * Propagating the proxy tick to the host kernel is a low
	 * priority duty: postpone this until the end of the core tick
	 * handler.
	 */
	this_rq = container_of(timer, struct evl_rq, htimer);
	this_rq->lflags |= RQ_TPROXY;
	this_rq->lflags &= ~RQ_TDEFER;
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
	cpumask_clear(&rq->resched);
#else
	name_fmt = "ROOT";
	rq->proxy_timer_name = kstrdup("[proxy-timer]", GFP_KERNEL);
	rq->rrb_timer_name = kstrdup("[rrb-timer]", GFP_KERNEL);
#endif
	for_each_evl_sched_class(sched_class) {
		if (sched_class->sched_init)
			sched_class->sched_init(rq);
	}

	rq->status = 0;
	rq->lflags = RQ_IDLE;
	rq->curr = &rq->root_thread;

	/*
	 * No direct handler here since proxy timer events are handled
	 * specifically by the generic timer code.
	 */
	evl_init_timer(&rq->htimer, &evl_mono_clock, proxy_tick_handler,
		rq, EVL_TIMER_IGRAVITY);
	evl_set_timer_priority(&rq->htimer, EVL_TIMER_LOPRIO);
	evl_set_timer_name(&rq->htimer, rq->proxy_timer_name);
	evl_init_timer(&rq->rrbtimer, &evl_mono_clock, roundrobin_handler,
		rq, EVL_TIMER_IGRAVITY);
	evl_set_timer_name(&rq->rrbtimer, rq->rrb_timer_name);
	evl_set_timer_priority(&rq->rrbtimer, EVL_TIMER_LOPRIO);
#ifdef CONFIG_EVENLESS_WATCHDOG
	evl_init_timer(&rq->wdtimer, &evl_mono_clock, watchdog_handler,
		rq, EVL_TIMER_IGRAVITY);
	evl_set_timer_name(&rq->wdtimer, "[watchdog]");
	evl_set_timer_priority(&rq->wdtimer, EVL_TIMER_LOPRIO);
#endif /* CONFIG_EVENLESS_WATCHDOG */

	evl_set_current_account(rq, &rq->root_thread.stat.account);

	/*
	 * Postpone evl_init_thread() - which sets RQ_SCHED upon
	 * setting the schedparams for the root thread - until we have
	 * enough of the runqueue initialized, so that attempting to
	 * reschedule from exit_oob_irq() later on is harmless.
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

static void destroy_rq(struct evl_rq *rq) /* nklock held, irqs off */
{
	evl_destroy_timer(&rq->htimer);
	evl_destroy_timer(&rq->rrbtimer);
	kfree(rq->proxy_timer_name);
	kfree(rq->rrb_timer_name);
	evl_destroy_timer(&rq->root_thread.ptimer);
	evl_destroy_timer(&rq->root_thread.rtimer);
#ifdef CONFIG_EVENLESS_WATCHDOG
	evl_destroy_timer(&rq->wdtimer);
#endif /* CONFIG_EVENLESS_WATCHDOG */
}

static inline void set_thread_running(struct evl_rq *rq,
				struct evl_thread *thread)
{
	thread->state &= ~T_READY;
	if (thread->state & T_RRB)
		evl_start_timer(&rq->rrbtimer,
				evl_abs_timeout(&rq->rrbtimer, thread->rrperiod),
				EVL_INFINITE);
	else
		evl_stop_timer(&rq->rrbtimer);
}

/* Must be called with nklock locked, interrupts off. */
struct evl_thread *evl_pick_thread(struct evl_rq *rq)
{
	struct evl_sched_class *sched_class __maybe_unused;
	struct evl_thread *curr = rq->curr;
	struct evl_thread *thread;

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
	 * Find the runnable thread having the highest priority among
	 * all scheduling classes, scanned by decreasing priority.
	 */
	for_each_evl_sched_class(sched_class) {
		thread = sched_class->sched_pick(rq);
		if (thread) {
			set_thread_running(rq, thread);
			return thread;
		}
	}

	return NULL; /* Never executed because of the idle class. */
}

#ifdef CONFIG_EVENLESS_DEBUG_CORE

void evl_disable_preempt(void)
{
	oob_context_only();
	__evl_disable_preempt();
}
EXPORT_SYMBOL(evl_disable_preempt);

void evl_enable_preempt(void)
{
	oob_context_only();
	__evl_enable_preempt();
}
EXPORT_SYMBOL(evl_enable_preempt);

#endif /* CONFIG_EVENLESS_DEBUG_CORE */

/* nklock locked, interrupts off. */
void evl_putback_thread(struct evl_thread *thread)
{
	if (thread->state & T_READY)
		evl_dequeue_thread(thread);
	else
		thread->state |= T_READY;

	evl_enqueue_thread(thread);
	evl_set_resched(thread->rq);
}

/* nklock locked, interrupts off. */
int evl_set_thread_policy(struct evl_thread *thread,
			struct evl_sched_class *sched_class,
			const union evl_sched_param *p)
{
	struct evl_sched_class *orig_effective_class __maybe_unused;
	bool effective;
	int ret;

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
	 * exit_oob_irq() before all CPUs have their runqueue fully
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
EXPORT_SYMBOL_GPL(evl_set_thread_policy);

/* nklock locked, interrupts off. */
bool evl_set_effective_thread_priority(struct evl_thread *thread, int prio)
{
	int wprio = evl_calc_weighted_prio(thread->base_class, prio);

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

/* nklock locked, interrupts off. */
void evl_track_thread_policy(struct evl_thread *thread,
			struct evl_thread *target)
{
	union evl_sched_param param;

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
		 * priority group.
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
}

/* nklock locked, interrupts off. */
void evl_protect_thread_priority(struct evl_thread *thread, int prio)
{
	/*
	 * Apply a PP boost by changing the effective priority of a
	 * thread, forcing it to the RT class. Like
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

	thread->sched_class = &evl_sched_rt;
	evl_ceil_priority(thread, prio);

	if (thread->state & T_READY)
		evl_enqueue_thread(thread);

	trace_evl_thread_set_current_prio(thread);

	evl_set_resched(thread->rq);
}

static void migrate_thread(struct evl_thread *thread, struct evl_rq *rq)
{
	struct evl_sched_class *sched_class = thread->sched_class;

	if (thread->state & T_READY) {
		evl_dequeue_thread(thread);
		thread->state &= ~T_READY;
	}

	if (sched_class->sched_migrate)
		sched_class->sched_migrate(thread, rq);
	/*
	 * WARNING: the scheduling class may have just changed as a
	 * result of calling the per-class migration hook.
	 */
	thread->rq = rq;
}

/*
 * nklock locked, interrupts off. Thread may be blocked.
 */
void evl_migrate_rq(struct evl_thread *thread, struct evl_rq *rq)
{
	struct evl_rq *last_rq = thread->rq;

	migrate_thread(thread, rq);

	if (!(thread->state & EVL_THREAD_BLOCK_BITS)) {
		evl_requeue_thread(thread);
		thread->state |= T_READY;
		evl_set_resched(last_rq);
	}
}

void evl_init_schedq(struct evl_multilevel_queue *q)
{
	int prio;

	q->elems = 0;
	bitmap_zero(q->prio_map, EVL_MLQ_LEVELS);

	for (prio = 0; prio < EVL_MLQ_LEVELS; prio++)
		INIT_LIST_HEAD(q->heads + prio);
}

static inline int get_qindex(struct evl_multilevel_queue *q, int prio)
{
	/*
	 * BIG FAT WARNING: We need to rescale the priority level to a
	 * 0-based range. We use find_first_bit() to scan the bitmap
	 * which is a bit scan forward operation. Therefore, the lower
	 * the index value, the higher the priority (since least
	 * significant bits will be found first when scanning the
	 * bitmap).
	 */
	return EVL_MLQ_LEVELS - prio - 1;
}

static struct list_head *add_q(struct evl_multilevel_queue *q, int prio)
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

void evl_add_schedq(struct evl_multilevel_queue *q,
		struct evl_thread *thread)
{
	struct list_head *head = add_q(q, thread->cprio);
	list_add(&thread->rq_next, head);
}

void evl_add_schedq_tail(struct evl_multilevel_queue *q,
			struct evl_thread *thread)
{
	struct list_head *head = add_q(q, thread->cprio);
	list_add_tail(&thread->rq_next, head);
}

static void del_q(struct evl_multilevel_queue *q,
		struct list_head *entry, int idx)
{
	struct list_head *head = q->heads + idx;

	list_del(entry);
	q->elems--;

	if (list_empty(head))
		__clear_bit(idx, q->prio_map);
}

void evl_del_schedq(struct evl_multilevel_queue *q,
		struct evl_thread *thread)
{
	del_q(q, &thread->rq_next, get_qindex(q, thread->cprio));
}

struct evl_thread *evl_get_schedq(struct evl_multilevel_queue *q)
{
	struct evl_thread *thread;
	struct list_head *head;
	int idx;

	if (q->elems == 0)
		return NULL;

	idx = evl_get_schedq_weight(q);
	head = q->heads + idx;
	thread = list_first_entry(head, struct evl_thread, rq_next);
	del_q(q, &thread->rq_next, idx);

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

struct evl_thread *evl_rt_pick(struct evl_rq *rq)
{
	struct evl_multilevel_queue *q = &rq->rt.runnable;
	struct evl_thread *thread;
	struct list_head *head;
	int idx;

	if (q->elems == 0)
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
	if (unlikely(thread->sched_class != &evl_sched_rt))
		return thread->sched_class->sched_pick(rq);

	del_q(q, &thread->rq_next, idx);

	return thread;
}

static inline int test_resched(struct evl_rq *rq)
{
	int resched = evl_need_resched(rq);
#ifdef CONFIG_SMP
	/* Send resched IPI to remote CPU(s). */
	if (unlikely(!cpumask_empty(&rq->resched))) {
		smp_mb();
		irq_pipeline_send_remote(RESCHEDULE_OOB_IPI, &rq->resched);
		cpumask_clear(&rq->resched);
	}
#endif
	rq->status &= ~RQ_SCHED;

	return resched;
}

static inline void enter_root(struct evl_thread *root)
{
#ifdef CONFIG_EVENLESS_WATCHDOG
	evl_stop_timer(&evl_thread_rq(root)->wdtimer);
#endif
}

static inline void leave_root(struct evl_thread *root)
{
	dovetail_resume_oob(&root->altsched);

#ifdef CONFIG_EVENLESS_WATCHDOG
	evl_start_timer(&evl_thread_rq(root)->wdtimer,
			evl_abs_timeout(&evl_thread_rq(root)->wdtimer,
					get_watchdog_timeout()),
			EVL_INFINITE);
#endif
}

static irqreturn_t reschedule_interrupt(int irq, void *dev_id)
{
	/* hw interrupts are off. */
	trace_evl_schedule_remote(this_evl_rq());
	evl_schedule();

	return IRQ_HANDLED;
}

bool __evl_schedule(struct evl_rq *this_rq)
{
	struct evl_thread *prev, *next, *curr;
	bool switched, oob_entry;
	unsigned long flags;

	trace_evl_schedule(this_rq);

	xnlock_get_irqsave(&nklock, flags);

	curr = this_rq->curr;
	/*
	 * CAUTION: curr->altsched.task may be unsynced and even stale
	 * if curr == &this_rq->root_thread, since the task logged by
	 * leave_root() may not still be the current one. Use
	 * "current" for disambiguating if you need to refer to the
	 * underlying inband task.
	 */
	if (curr->state & T_USER)
		evl_commit_monitor_ceiling();

	switched = false;
	if (!test_resched(this_rq))
		goto out;

	next = evl_pick_thread(this_rq);
	if (next == curr) {
		if (unlikely(next->state & T_ROOT)) {
			if (this_rq->lflags & RQ_TPROXY)
				evl_notify_proxy_tick(this_rq);
			if (this_rq->lflags & RQ_TDEFER)
				evl_program_local_tick(&evl_mono_clock);
		}
		goto out;
	}

	prev = curr;

	trace_evl_switch_context(prev, next);

	this_rq->curr = next;
	oob_entry = true;

	if (prev->state & T_ROOT) {
		leave_root(prev);
		oob_entry = false;
	} else if (next->state & T_ROOT) {
		if (this_rq->lflags & RQ_TPROXY)
			evl_notify_proxy_tick(this_rq);
		if (this_rq->lflags & RQ_TDEFER)
			evl_program_local_tick(&evl_mono_clock);
		enter_root(next);
	}

	evl_switch_account(this_rq, &next->stat.account);
	evl_inc_counter(&next->stat.csw);
	dovetail_context_switch(&prev->altsched, &next->altsched);

	/*
	 * Refresh the current rq and thread pointers, this is
	 * required to cope with in-band<->oob stage transitions.
	 */
	this_rq = this_evl_rq();
	curr = this_rq->curr;

	EVL_WARN_ON(CORE, curr->state & EVL_THREAD_BLOCK_BITS);

	/*
	 * If we entered __evl_schedule() over the oob stage but don't
	 * have the OOB flag set into our flags anymore, this portion
	 * of code is the switch tail of the inband schedule()
	 * routine, finalizing an earlier call to evl_switch_inband():
	 *
	 * irq_work_queue() ->
	 *        IRQ:wake_up_process() ->
	 *                         schedule() ->
	 *                               back from dovetail_context_switch()
	 */
	if (oob_entry && !test_thread_local_flags(_TLF_OOB)) {
		EVL_WARN_ON_ONCE(CORE, !(preempt_count() & STAGE_MASK));
		preempt_count_sub(STAGE_OFFSET);
		return true;
	}

	switched = true;
out:
	xnlock_put_irqrestore(&nklock, flags);

	return switched;
}
EXPORT_SYMBOL_GPL(__evl_schedule);

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

	/*
	 * NOTE: The user-defined policy may be different than ours,
	 * e.g. SCHED_FIFO,prio=-7 from userland would be interpreted
	 * as SCHED_WEAK,prio=7 in kernel space.
	 */
	if (prio < 0) {
		prio = -prio;
		policy = SCHED_WEAK;
	}
	sched_class = &evl_sched_rt;
	param->rt.prio = prio;

	switch (policy) {
	case SCHED_NORMAL:
		if (prio)
			return NULL;
		/* Fallback wanted */
	case SCHED_WEAK:
		if (prio < EVL_WEAK_MIN_PRIO ||
			prio > EVL_WEAK_MAX_PRIO)
			return NULL;
		param->weak.prio = prio;
		sched_class = &evl_sched_weak;
		break;
	case SCHED_RR:
		/* if unspecified, use current one. */
		tslice = timespec_to_ktime(attrs->sched_rr_quantum);
		if (timeout_infinite(tslice) && tslice_r)
			tslice = *tslice_r;
		/* falldown wanted */
	case SCHED_FIFO:
		if (prio < EVL_FIFO_MIN_PRIO ||
			prio > EVL_FIFO_MAX_PRIO)
			return NULL;
		break;
	case SCHED_EVL:
		if (prio < EVL_CORE_MIN_PRIO ||
			prio > EVL_CORE_MAX_PRIO)
			return NULL;
		break;
#ifdef CONFIG_EVENLESS_SCHED_QUOTA
	case SCHED_QUOTA:
		param->quota.prio = attrs->sched_priority;
		param->quota.tgid = attrs->sched_quota_group;
		sched_class = &evl_sched_quota;
		break;
#endif
	default:
		return NULL;
	}

	*tslice_r = tslice;

	return sched_class;
}

bool evl_sched_yield(void)
{
	struct evl_thread *curr = evl_current();

	evl_release_thread(curr, 0, 0);

	return evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_sched_yield);

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
	case SCHED_NORMAL:
		break;
	case SCHED_RR:
	case SCHED_FIFO:
	case SCHED_EVL:
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
					"Evenless reschedule",
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
