/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2004 Gilles Chanteperdrix  <gilles.chanteperdrix@xenomai.org>
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <evl/sched.h>
#include <evl/thread.h>
#include <evl/timer.h>
#include <evl/clock.h>
#include <evl/tick.h>
#include <asm/div64.h>
#include <trace/events/evl.h>

#ifdef CONFIG_SMP

static struct evl_timerbase *
lock_timer_base(struct evl_timer *timer, unsigned long *flags)
{
	struct evl_timerbase *base;

	for (;;) {
		base = timer->base;
		raw_spin_lock_irqsave(&base->lock, *flags);
		/*
		 * Careful about a bolting of the same timer happening
		 * concurrently from a different CPU.
		 */
		if (base == timer->base)
			break;
		raw_spin_unlock_irqrestore(&base->lock, *flags);
	}

	return base;
}

static inline void unlock_timer_base(struct evl_timerbase *base,
				unsigned long flags)
{
	raw_spin_unlock_irqrestore(&base->lock, flags);
}

#else

static inline struct evl_timerbase *
lock_timer_base(struct evl_timer *timer, unsigned long *flags)
{
	*flags = hard_local_irq_save();

	return timer->base;
}

static inline void unlock_timer_base(struct evl_timerbase *base,
				unsigned long flags)
{
	hard_local_irq_restore(flags);
}

#endif

/* hard irqs off */
static inline void double_timer_base_lock(struct evl_timerbase *tb1,
					struct evl_timerbase *tb2)
{
	if (tb1 == tb2)
		raw_spin_lock(&tb1->lock);
	else if (tb1 < tb2) {
		raw_spin_lock(&tb1->lock);
		raw_spin_lock(&tb2->lock);
	} else {
		raw_spin_lock(&tb2->lock);
		raw_spin_lock(&tb1->lock);
	}
}

static inline void double_timer_base_unlock(struct evl_timerbase *tb1,
					struct evl_timerbase *tb2)
{
	raw_spin_unlock(&tb1->lock);
	if (tb1 != tb2)
		raw_spin_unlock(&tb2->lock);
}

/* timer base locked. */
static bool timer_at_front(struct evl_timer *timer)
{
	struct evl_rq *rq = evl_get_timer_rq(timer);
	struct evl_tqueue *tq;
	struct evl_tnode *tn;

	tq = &timer->base->q;
	tn = evl_get_tqueue_head(tq);
	if (tn == &timer->node)
		return true;

	if (rq->lflags & RQ_TDEFER) {
		tn = evl_get_tqueue_next(tq, tn);
		if (tn == &timer->node)
			return true;
	}

	return false;
}

/* timer base locked. */
static void program_timer(struct evl_timer *timer,
			struct evl_tqueue *tq)
{
	struct evl_rq *rq;

	evl_enqueue_timer(timer, tq);

	rq = evl_get_timer_rq(timer);
	if (!(rq->lflags & RQ_TSTOPPED) && !timer_at_front(timer))
		return;

	if (rq != this_evl_rq())
		evl_program_remote_tick(timer->clock, rq);
	else
		evl_program_local_tick(timer->clock);
}

void evl_start_timer(struct evl_timer *timer,
		ktime_t value, ktime_t interval)
{
	struct evl_timerbase *base;
	struct evl_tqueue *tq;
	ktime_t date, gravity;
	unsigned long flags;

	trace_evl_timer_start(timer, value, interval);

	base = lock_timer_base(timer, &flags);
	tq = &base->q;

	if ((timer->status & EVL_TIMER_DEQUEUED) == 0)
		evl_dequeue_timer(timer, tq);

	timer->status &= ~(EVL_TIMER_FIRED | EVL_TIMER_PERIODIC);

	date = ktime_sub(value, timer->clock->offset);

	/*
	 * To cope with the basic system latency, we apply a clock
	 * gravity value, which is the amount of time expressed in
	 * nanoseconds by which we should anticipate the shot for the
	 * timer. The gravity value varies with the type of context
	 * the timer wakes up, i.e. irq handler, kernel or user
	 * thread.
	 */
	gravity = evl_get_timer_gravity(timer);
	evl_tdate(timer) = ktime_sub(date, gravity);

	timer->interval = EVL_INFINITE;
	if (!timeout_infinite(interval)) {
		timer->interval = interval;
		timer->start_date = value;
		timer->pexpect_ticks = 0;
		timer->periodic_ticks = 0;
		timer->status |= EVL_TIMER_PERIODIC;
	}

	timer->status |= EVL_TIMER_RUNNING;
	program_timer(timer, tq);

	unlock_timer_base(base, flags);
}
EXPORT_SYMBOL_GPL(evl_start_timer);

/* timer base locked. */
bool evl_timer_deactivate(struct evl_timer *timer)
{
	struct evl_tqueue *tq = &timer->base->q;
	bool heading = true;

	if (!(timer->status & EVL_TIMER_DEQUEUED)) {
		heading = timer_at_front(timer);
		evl_dequeue_timer(timer, tq);
	}

	timer->status &= ~(EVL_TIMER_FIRED|EVL_TIMER_RUNNING);

	return heading;
}

/* timerbase locked, hard irqs off */
static void stop_timer_locked(struct evl_timer *timer)
{
	bool heading;

	/*
	 * If we removed the heading timer, reprogram the next shot if
	 * any. If the timer was running on another CPU, let it tick.
	 */
	if (evl_timer_is_running(timer)) {
		heading = evl_timer_deactivate(timer);
		if (heading && evl_timer_on_rq(timer, this_evl_rq()))
			evl_program_local_tick(timer->clock);
	}
}

void __evl_stop_timer(struct evl_timer *timer)
{
	struct evl_timerbase *base;
	unsigned long flags;

	trace_evl_timer_stop(timer);
	base = lock_timer_base(timer, &flags);
	stop_timer_locked(timer);
	unlock_timer_base(base, flags);
}
EXPORT_SYMBOL_GPL(__evl_stop_timer);

ktime_t evl_get_timer_date(struct evl_timer *timer)
{
	struct evl_timerbase *base;
	unsigned long flags;
	ktime_t expiry;

	base = lock_timer_base(timer, &flags);

	if (!evl_timer_is_running(timer))
		expiry = EVL_INFINITE;
	else
		expiry = evl_get_timer_expiry(timer);

	unlock_timer_base(base, flags);

	return expiry;
}
EXPORT_SYMBOL_GPL(evl_get_timer_date);

ktime_t __evl_get_timer_delta(struct evl_timer *timer)
{
	struct evl_timerbase *base;
	ktime_t expiry, now;
	unsigned long flags;

	base = lock_timer_base(timer, &flags);
	expiry = evl_get_timer_expiry(timer);
	unlock_timer_base(base, flags);
	now = evl_read_clock(timer->clock);
	if (expiry <= now)
		return ktime_set(0, 1);  /* Will elapse shortly. */

	return ktime_sub(expiry, now);
}
EXPORT_SYMBOL_GPL(__evl_get_timer_delta);

#ifdef CONFIG_SMP

static inline int get_clock_cpu(struct evl_clock *clock, int cpu)
{
	/*
	 * Check a CPU number against the possible set of CPUs
	 * receiving events from the underlying clock device. If the
	 * suggested CPU does not receive events from this device,
	 * return the first one which does instead.
	 *
	 * A global clock device with no particular IRQ affinity may
	 * tick on any CPU, but timers should always be queued on
	 * CPU0.
	 *
	 * NOTE: we have scheduler slots initialized for all online
	 * CPUs, we can program and receive clock ticks on any of
	 * them. So there is no point in restricting the valid CPU set
	 * to cobalt_cpu_affinity, which specifically refers to the
	 * set of CPUs which may run real-time threads. Although
	 * receiving a clock tick for waking up a thread living on a
	 * remote CPU is not optimal since this involves IPI-signaled
	 * rescheds, this is still a valid case.
	 */
	if (cpumask_empty(&clock->affinity))
		return 0;

	if (cpumask_test_cpu(cpu, &clock->affinity))
		return cpu;

	return cpumask_first(&clock->affinity);
}

/**
 * __evl_set_timer_rq - change the CPU affinity of a timer
 * @timer:      timer to modify
 * @rq:         runqueue to assign the timer to
 */
void __evl_set_timer_rq(struct evl_timer *timer,
			struct evl_clock *clock,
			struct evl_rq *rq)
{
	int cpu;

	requires_ugly_lock();

	/*
	 * Figure out which CPU is best suited for managing this
	 * timer, preferably picking evl_rq_cpu(rq) if the ticking
	 * device moving the timer clock beats on that CPU. Otherwise,
	 * pick the first CPU from the clock affinity mask if set. If
	 * not, the timer is backed by a global device with no
	 * particular IRQ affinity, so it should always be queued to
	 * CPU0.
	 */
	cpu = 0;
	if (!cpumask_empty(&clock->master->affinity))
		cpu = get_clock_cpu(clock->master, evl_rq_cpu(rq));

	evl_bolt_timer(timer, clock, evl_cpu_rq(cpu));
}
EXPORT_SYMBOL_GPL(__evl_set_timer_rq);

#endif /* CONFIG_SMP */

void __evl_init_timer(struct evl_timer *timer,
		struct evl_clock *clock,
		void (*handler)(struct evl_timer *timer),
		struct evl_rq *rq,
		int opflags)
{
	int cpu __maybe_unused;

	timer->clock = clock;
	evl_tdate(timer) = EVL_INFINITE;
	evl_set_timer_priority(timer, EVL_TIMER_STDPRIO);
	timer->status = (EVL_TIMER_DEQUEUED|(opflags & EVL_TIMER_INIT_MASK));
	timer->handler = handler;
	timer->interval = EVL_INFINITE;
	/*
	 * Set the timer affinity, preferably to rq if given, CPU0
	 * otherwise.
	 */
	if (!rq)
		rq = evl_cpu_rq(0);
#ifdef CONFIG_SMP
	cpu = 0;
	if (!cpumask_empty(&clock->master->affinity))
		cpu = get_clock_cpu(clock->master, evl_rq_cpu(rq));
	timer->rq = evl_cpu_rq(cpu);
#endif
	timer->base = evl_percpu_timers(clock, evl_rq_cpu(rq));
	timer->clock = clock;
	timer->name = "<timer>";
	evl_reset_timer_stats(timer);
}
EXPORT_SYMBOL_GPL(__evl_init_timer);

void evl_set_timer_gravity(struct evl_timer *timer, int gravity)
{
	struct evl_timerbase *base;
	unsigned long flags;

	base = lock_timer_base(timer, &flags);
	timer->status &= ~EVL_TIMER_GRAVITY_MASK;
	timer->status |= gravity;
	unlock_timer_base(base, flags);

}
EXPORT_SYMBOL_GPL(evl_set_timer_gravity);

void evl_destroy_timer(struct evl_timer *timer)
{
	evl_stop_timer(timer);
	timer->status |= EVL_TIMER_KILLED;
#ifdef CONFIG_SMP
	timer->rq = NULL;
#endif
	timer->base = NULL;
}
EXPORT_SYMBOL_GPL(evl_destroy_timer);

/*
 * evl_bolt_timer - change the reference clock and/or the CPU
 *                     affinity of a timer
 * @timer:      timer to modify
 * @clock:      reference clock
 * @rq:         runqueue to assign the timer to
 */
void evl_bolt_timer(struct evl_timer *timer, /* nklocked, IRQs off */
		struct evl_clock *clock, struct evl_rq *rq)
{
	struct evl_timerbase *old_base, *new_base;
	struct evl_clock *master = clock->master;
	unsigned long flags;
	int cpu;

	trace_evl_timer_bolt(timer, clock, evl_rq_cpu(rq));

	/*
	 * This assertion triggers when the timer is migrated to a CPU
	 * for which we do not expect any clock events/IRQs from the
	 * associated clock device. If so, the timer would never fire
	 * since clock ticks would never happen on that CPU.
	 */
	cpu = evl_rq_cpu(rq);
	if (EVL_WARN_ON_SMP(CORE,
			!cpumask_empty(&master->affinity) &&
			!cpumask_test_cpu(cpu, &master->affinity)))
		return;

	old_base = lock_timer_base(timer, &flags);

	if (evl_timer_on_rq(timer, rq) && clock == timer->clock) {
		unlock_timer_base(old_base, flags);
		return;
	}

	new_base = evl_percpu_timers(master, cpu);

	if (timer->status & EVL_TIMER_RUNNING) {
		stop_timer_locked(timer);
		unlock_timer_base(old_base, flags);
		flags = hard_local_irq_save();
		double_timer_base_lock(old_base, new_base);
#ifdef CONFIG_SMP
		timer->rq = rq;
#endif
		timer->base = new_base;
		timer->clock = clock;
		evl_enqueue_timer(timer, &new_base->q);
		if (timer_at_front(timer))
			evl_program_remote_tick(clock, rq);
		double_timer_base_unlock(old_base, new_base);
		hard_local_irq_restore(flags);
	} else {
#ifdef CONFIG_SMP
		timer->rq = rq;
#endif
		timer->base = new_base;
		timer->clock = clock;
		unlock_timer_base(old_base, flags);
	}
}
EXPORT_SYMBOL_GPL(evl_bolt_timer);

unsigned long evl_get_timer_overruns(struct evl_timer *timer)
{
	unsigned long overruns = 0, flags;
	struct evl_timerbase *base;
	struct evl_thread *thread;
	struct evl_tqueue *tq;
	ktime_t now, delta;

	now = evl_read_clock(timer->clock);
	base = lock_timer_base(timer, &flags);

	delta = ktime_sub(now, evl_get_timer_next_date(timer));
	if (likely(delta < timer->interval))
		goto done;

	overruns = ktime_divns(delta, ktime_to_ns(timer->interval));
	timer->pexpect_ticks += overruns;
	if (!evl_timer_is_running(timer))
		goto done;

	EVL_WARN_ON_ONCE(CORE, (timer->status &
						(EVL_TIMER_DEQUEUED|EVL_TIMER_PERIODIC))
			!= EVL_TIMER_PERIODIC);
	tq = &base->q;
	evl_dequeue_timer(timer, tq);
	while (evl_tdate(timer) < now) {
		timer->periodic_ticks++;
		evl_update_timer_date(timer);
	}

	program_timer(timer, tq);
done:
	timer->pexpect_ticks++;

	unlock_timer_base(base, flags);

	/*
	 * Hide overruns due to the most recent ptracing session from
	 * the caller.
	 */
	thread = evl_current();
	if (thread->local_info & T_HICCUP)
		return 0;

	return overruns;
}
EXPORT_SYMBOL_GPL(evl_get_timer_overruns);
