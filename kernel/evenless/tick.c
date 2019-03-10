/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2016, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/percpu.h>
#include <linux/cpumask.h>
#include <linux/clockchips.h>
#include <linux/tick.h>
#include <linux/irqdomain.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/timekeeping.h>
#include <linux/irq_pipeline.h>
#include <linux/slab.h>
#include <evenless/sched.h>
#include <evenless/timer.h>
#include <evenless/clock.h>
#include <evenless/tick.h>
#include <evenless/control.h>
#include <trace/events/evenless.h>

/*
 * This is our high-precision clock tick device, which operates the
 * best rated clock event device taken over from the kernel. A head
 * stage handler forwards tick events to our clock management core.
 */
struct core_tick_device {
	struct clock_event_device *real_device;
};

static DEFINE_PER_CPU(struct core_tick_device, clock_cpu_device);

static int proxy_set_next_ktime(ktime_t expires,
				struct clock_event_device *proxy_ced)
{
	struct evl_rq *rq;
	unsigned long flags;
	ktime_t delta;

	/*
	 * Negative delta have been observed. evl_start_timer()
	 * will trigger an immediate shot in such an event.
	 */
	delta = ktime_sub(expires, ktime_get());

	flags = hard_local_irq_save(); /* Prevent CPU migration. */
	rq = this_evl_rq();
	evl_start_timer(&rq->inband_timer,
			evl_abs_timeout(&rq->inband_timer, delta),
			EVL_INFINITE);
	hard_local_irq_restore(flags);

	return 0;
}

static int proxy_set_oneshot_stopped(struct clock_event_device *ced)
{
	struct core_tick_device *ctd = this_cpu_ptr(&clock_cpu_device);
	unsigned long flags;
	struct evl_rq *rq;

	/*
	 * In-band wants to disable the clock hardware on entering a
	 * tickless state, so we have to stop our in-band tick
	 * emulation. Propagate the request for shutting down the
	 * hardware to the real device only if we have no outstanding
	 * OOB timers. CAUTION: the in-band timer is counted when
	 * assessing the RQ_IDLE condition, so we need to stop it
	 * prior to testing the latter.
	 */
	flags = hard_local_irq_save();

	rq = this_evl_rq();
	evl_stop_timer(&rq->inband_timer);
	rq->lflags |= RQ_TSTOPPED;

	if (rq->lflags & RQ_IDLE)
		ctd->real_device->set_state_oneshot_stopped(ctd->real_device);

	hard_local_irq_restore(flags);

	return 0;
}

static void proxy_device_register(struct clock_event_device *proxy_ced,
				struct clock_event_device *real_ced)
{
	struct core_tick_device *ctd = this_cpu_ptr(&clock_cpu_device);

	ctd->real_device = real_ced;
	proxy_ced->features |= CLOCK_EVT_FEAT_KTIME;
	proxy_ced->set_next_ktime = proxy_set_next_ktime;
	proxy_ced->set_next_event = NULL;
	if (real_ced->set_state_oneshot_stopped)
		proxy_ced->set_state_oneshot_stopped =
			proxy_set_oneshot_stopped;
	proxy_ced->rating = real_ced->rating + 1;
	proxy_ced->min_delta_ns = 1;
	proxy_ced->max_delta_ns = KTIME_MAX;
	proxy_ced->min_delta_ticks = 1;
	proxy_ced->max_delta_ticks = ULONG_MAX;
	clockevents_register_device(proxy_ced);
}

static void proxy_device_unregister(struct clock_event_device *proxy_ced,
				struct clock_event_device *real_ced)
{
	struct core_tick_device *ctd = this_cpu_ptr(&clock_cpu_device);

	ctd->real_device = NULL;
}

/*
 * This is our high-precision clock tick handler. We only have two
 * possible callers, each of them may only run over a CPU which is a
 * member of the real-time set:
 *
 * - our TIMER_OOB_IPI handler, such IPI is directed to members of our
 * real-time CPU set exclusively.
 *
 * - our clock_event_handler() routine. The IRQ pipeline
 * guarantees that such handler always runs over a CPU which is a
 * member of the CPU set passed to enable_clock_devices() (i.e. our
 * real-time CPU set).
 *
 * hard IRQs are off.
 */
static void clock_event_handler(struct clock_event_device *dummy)
{
	struct evl_rq *this_rq = this_evl_rq();

	if (EVL_WARN_ON_ONCE(CORE, !is_evl_cpu(evl_rq_cpu(this_rq))))
		return;

	evl_announce_tick(&evl_mono_clock);

	/*
	 * If a real-time thread was preempted by this clock
	 * interrupt, any transition to the root thread will cause a
	 * in-band tick to be propagated by evl_schedule() from
	 * irq_finish_head(), so we only need to propagate the in-band
	 * tick in case the root thread was preempted.
	 */
	if ((this_rq->lflags & RQ_TPROXY) && (this_rq->curr->state & T_ROOT))
		evl_notify_proxy_tick(this_rq);
}

void evl_notify_proxy_tick(struct evl_rq *this_rq) /* hard IRQs off. */
{
	/*
	 * A proxy clock event device is active on this CPU, make it
	 * tick asap when the in-band code resumes; this will honour a
	 * previous set_next_ktime() request received from the kernel
	 * we have carried out using our core timing services.
	 */
	this_rq->lflags &= ~RQ_TPROXY;
	tick_notify_proxy();
}

#ifdef CONFIG_SMP

static irqreturn_t clock_ipi_handler(int irq, void *dev_id)
{
	clock_event_handler(NULL);

	return IRQ_HANDLED;
}

#endif

static struct proxy_tick_ops proxy_ops = {
	.register_device = proxy_device_register,
	.unregister_device = proxy_device_unregister,
	.handle_event = clock_event_handler,
};

int evl_enable_tick(void)
{
	int ret;

#ifdef CONFIG_SMP
	ret = __request_percpu_irq(TIMER_OOB_IPI,
				clock_ipi_handler,
				IRQF_OOB, "EVL timer IPI",
				&evl_machine_cpudata);
	if (ret)
		return ret;
#endif

	/*
	 * CAUTION:
	 *
	 * - EVL timers may be started only _after_ the proxy clock
	 * device has been set up for the target CPU.
	 *
	 * - do not hold any lock across calls to evl_enable_tick().
	 *
	 * - tick_install_proxy() guarantees that the real clock
	 * device supports oneshot mode, or fails.
	 */
	ret = tick_install_proxy(&proxy_ops, &evl_oob_cpus);
	if (ret) {
#ifdef CONFIG_SMP
		free_percpu_irq(TIMER_OOB_IPI,
				&evl_machine_cpudata);
#endif
		return ret;
	}

	return 0;
}

void evl_disable_tick(void)
{
	tick_uninstall_proxy(&proxy_ops, &evl_oob_cpus);
#ifdef CONFIG_SMP
	free_percpu_irq(TIMER_OOB_IPI, &evl_machine_cpudata);
#endif
	/*
	 * When the kernel is swapping clock event devices on behalf
	 * of enable_clock_devices(), it may end up calling
	 * program_timer() via the synthetic device's
	 * ->set_next_event() handler for resuming the in-band timer.
	 * Therefore, no timer should remain queued before
	 * enable_clock_devices() is called, or unpleasant hangs may
	 * happen if the in-band timer is not at front of the
	 * queue. You have been warned.
	 */
	evl_stop_timers(&evl_mono_clock);
}

/* per-cpu timer queue locked. */
void evl_program_proxy_tick(struct evl_clock *clock)
{
	struct core_tick_device *ctd = raw_cpu_ptr(&clock_cpu_device);
	struct clock_event_device *real_ced = ctd->real_device;
	struct evl_rq *this_rq = this_evl_rq();
	struct evl_timerbase *tmb;
	struct evl_timer *timer;
	struct evl_tnode *tn;
	int64_t delta;
	u64 cycles;
	ktime_t t;
	int ret;

	/*
	 * Do not reprogram locally when inside the tick handler -
	 * will be done on exit anyway. Also exit if there is no
	 * pending timer.
	 */
	if (this_rq->lflags & RQ_TIMER)
		return;

	tmb = evl_this_cpu_timers(clock);
	tn = evl_get_tqueue_head(&tmb->q);
	if (tn == NULL) {
		this_rq->lflags |= RQ_IDLE;
		return;
	}

	/*
	 * Try to defer the next in-band tick, so that it does not
	 * preempt an OOB activity uselessly, in two cases:
	 *
	 * 1) a rescheduling is pending for the current CPU. We may
	 * assume that an EVL thread is about to resume, so we want to
	 * move the in-band tick out of the way until in-band activity
	 * resumes, unless there is no other outstanding timers.
	 *
	 * 2) the current EVL thread is running OOB, in which case we
	 * may defer the in-band tick until the in-band activity
	 * resumes.
	 *
	 * The in-band tick deferral is cleared whenever EVL is about
	 * to yield control to the in-band code (see
	 * __evl_schedule()), or a timer with an earlier timeout date
	 * is scheduled, whichever comes first.
	 */
	this_rq->lflags &= ~(RQ_TDEFER|RQ_IDLE|RQ_TSTOPPED);
	timer = container_of(tn, struct evl_timer, node);
	if (timer == &this_rq->inband_timer) {
		if (evl_need_resched(this_rq) ||
			!(this_rq->curr->state & T_ROOT)) {
			tn = evl_get_tqueue_next(&tmb->q, tn);
			if (tn) {
				this_rq->lflags |= RQ_TDEFER;
				timer = container_of(tn, struct evl_timer, node);
			}
		}
	}

	t = evl_tdate(timer);
	delta = ktime_to_ns(ktime_sub(t, evl_read_clock(clock)));

	if (real_ced->features & CLOCK_EVT_FEAT_KTIME) {
		real_ced->set_next_ktime(t, real_ced);
		trace_evl_timer_shot(delta);
	} else {
		if (delta <= 0)
			delta = real_ced->min_delta_ns;
		else {
			delta = min(delta, (int64_t)real_ced->max_delta_ns);
			delta = max(delta, (int64_t)real_ced->min_delta_ns);
		}
		cycles = ((u64)delta * real_ced->mult) >> real_ced->shift;
		ret = real_ced->set_next_event(cycles, real_ced);
		trace_evl_timer_shot(delta);
		if (ret) {
			real_ced->set_next_event(real_ced->min_delta_ticks, real_ced);
			trace_evl_timer_shot(real_ced->min_delta_ns);
		}
	}
}

#ifdef CONFIG_SMP
void evl_send_timer_ipi(struct evl_clock *clock, struct evl_rq *rq)
{
	irq_pipeline_send_remote(TIMER_OOB_IPI,
				cpumask_of(evl_rq_cpu(rq)));
}
#endif
