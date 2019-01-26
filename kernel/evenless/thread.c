/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2006, 2016 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <stdarg.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/irq_work.h>
#include <linux/sched/signal.h>
#include <linux/sched/types.h>
#include <linux/sched/task.h>
#include <linux/jiffies.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/ptrace.h>
#include <linux/math64.h>
#include <evenless/sched.h>
#include <evenless/timer.h>
#include <evenless/synch.h>
#include <evenless/clock.h>
#include <evenless/stat.h>
#include <evenless/assert.h>
#include <evenless/lock.h>
#include <evenless/thread.h>
#include <evenless/sched.h>
#include <evenless/memory.h>
#include <evenless/file.h>
#include <evenless/factory.h>
#include <evenless/monitor.h>
#include <asm/evenless/syscall.h>
#include <trace/events/evenless.h>

static DECLARE_WAIT_QUEUE_HEAD(join_all);

static void inband_task_wakeup(struct irq_work *work);

static void timeout_handler(struct evl_timer *timer) /* hard irqs off */
{
	struct evl_thread *thread = container_of(timer, struct evl_thread, rtimer);

	xnlock_get(&nklock);
	thread->info |= T_TIMEO;
	evl_resume_thread(thread, T_DELAY);
	xnlock_put(&nklock);
}

static void periodic_handler(struct evl_timer *timer) /* hard irqs off */
{
	struct evl_thread *thread =
		container_of(timer, struct evl_thread, ptimer);

	xnlock_get(&nklock);
	/*
	 * Prevent unwanted round-robin, and do not wake up threads
	 * blocked on a resource.
	 */
	if ((thread->state & (T_DELAY|T_PEND)) == T_DELAY)
		evl_resume_thread(thread, T_DELAY);

	evl_set_timer_rq(&thread->ptimer, evl_thread_rq(thread));

	xnlock_put(&nklock);
}

static inline void enlist_new_thread(struct evl_thread *thread)
{				/* nklock held, irqs off */
	list_add_tail(&thread->next, &evl_thread_list);
	evl_nrthreads++;
}

static inline void set_oob_threadinfo(struct evl_thread *thread)
{
	struct oob_thread_state *p;

	p = dovetail_current_state();
	p->thread = thread;
}

static void pin_to_initial_cpu(struct evl_thread *thread)
{
	struct task_struct *p = current;
	unsigned long flags;
	struct evl_rq *rq;
	int cpu;

	/*
	 * @thread is the EVL extension of the current in-band
	 * task. If the current CPU is part of the affinity mask of
	 * this thread, pin the latter on this CPU. Otherwise pin it
	 * to the first CPU of that mask.
	 */
	cpu = task_cpu(p);
	if (!cpumask_test_cpu(cpu, &thread->affinity))
		cpu = cpumask_first(&thread->affinity);

	set_cpus_allowed_ptr(p, cpumask_of(cpu));
	/*
	 * @thread is still unstarted EVL-wise, we are in the process
	 * of mapping the current in-band task to it. Therefore
	 * evl_migrate_thread() can be called for pinning it on a
	 * real-time CPU.
	 */
	xnlock_get_irqsave(&nklock, flags);
	rq = evl_cpu_rq(cpu);
	evl_migrate_thread(thread, rq);
	xnlock_put_irqrestore(&nklock, flags);
}

int evl_init_thread(struct evl_thread *thread,
		    const struct evl_init_thread_attr *iattr,
		    struct evl_rq *rq,
		    const char *fmt, ...)
{
	int flags = iattr->flags & ~T_SUSP, ret, gravity;
	struct cpumask affinity;
	va_list args;

	if (!(flags & T_ROOT))
		flags |= T_DORMANT;

	/*
	 * If no rq was given, pick an initial CPU for the new thread
	 * which is part of its affinity mask, and therefore also part
	 * of the supported CPUs. This CPU may change in
	 * pin_to_initial_cpu().
	 */
	if (rq == NULL) {
		cpumask_and(&affinity, &iattr->affinity, &evl_cpu_affinity);
		if (cpumask_empty(&affinity))
			return -EINVAL;
		rq = evl_cpu_rq(cpumask_first(&affinity));
	}

	va_start(args, fmt);
	thread->name = kvasprintf(GFP_KERNEL, fmt, args);
	va_end(args);
	if (thread->name == NULL)
		return -ENOMEM;

	/*
	 * We mirror the global user debug state into the per-thread
	 * state, to speed up branch taking in libevenless wherever this
	 * needs to be tested.
	 */
	if (IS_ENABLED(CONFIG_EVENLESS_DEBUG_MONITOR_SLEEP))
		flags |= T_DEBUG;

	cpumask_and(&thread->affinity, &iattr->affinity, &evl_cpu_affinity);
	thread->rq = rq;
	thread->state = flags;
	thread->info = 0;
	thread->local_info = 0;
	thread->wprio = EVL_IDLE_PRIO;
	thread->cprio = EVL_IDLE_PRIO;
	thread->bprio = EVL_IDLE_PRIO;
	thread->lock_count = 0;
	thread->rrperiod = EVL_INFINITE;
	thread->wchan = NULL;
	thread->wwake = NULL;
	thread->wait_data = NULL;
	thread->res_count = 0;
	thread->u_window = NULL;
	memset(&thread->poll_context, 0, sizeof(thread->poll_context));
	memset(&thread->stat, 0, sizeof(thread->stat));
	memset(&thread->altsched, 0, sizeof(thread->altsched));
	init_irq_work(&thread->inband_work, inband_task_wakeup);

	INIT_LIST_HEAD(&thread->next);
	INIT_LIST_HEAD(&thread->boosters);
	init_completion(&thread->exited);

	gravity = flags & T_USER ? EVL_TIMER_UGRAVITY : EVL_TIMER_KGRAVITY;
	evl_init_timer(&thread->rtimer, &evl_mono_clock, timeout_handler,
		       rq, gravity);
	evl_set_timer_name(&thread->rtimer, thread->name);
	evl_set_timer_priority(&thread->rtimer, EVL_TIMER_HIPRIO);
	evl_init_timer(&thread->ptimer, &evl_mono_clock, periodic_handler,
		       rq, gravity);
	evl_set_timer_name(&thread->ptimer, thread->name);
	evl_set_timer_priority(&thread->ptimer, EVL_TIMER_HIPRIO);

	thread->base_class = NULL; /* evl_set_thread_policy() sets it. */
	ret = evl_init_rq_thread(thread);
	if (ret)
		goto err_out;

	ret = evl_set_thread_policy(thread, iattr->sched_class,
				    &iattr->sched_param);
	if (ret)
		goto err_out;

	trace_evl_init_thread(thread, iattr, ret);

	return 0;

err_out:
	evl_destroy_timer(&thread->rtimer);
	evl_destroy_timer(&thread->ptimer);
	trace_evl_init_thread(thread, iattr, ret);
	kfree(thread->name);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_init_thread);

/* Undoes evl_init_thread(), and only that. */
static void uninit_thread(struct evl_thread *thread)
{
	unsigned long flags;

	evl_destroy_timer(&thread->rtimer);
	evl_destroy_timer(&thread->ptimer);

	xnlock_get_irqsave(&nklock, flags);
	evl_forget_thread(thread);
	xnlock_put_irqrestore(&nklock, flags);

	kfree(thread->name);
}

static void do_cleanup_current(struct evl_thread *curr)
{
	struct evl_syn *synch, *tmp;
	unsigned long flags;

	evl_unindex_element(&curr->element);

	if (curr->state & T_USER) {
		evl_free_chunk(&evl_shared_heap, curr->u_window);
		curr->u_window = NULL;
		if (curr->poll_context.table)
			evl_free(curr->poll_context.table);
	}

	xnlock_get_irqsave(&nklock, flags);

	list_del(&curr->next);
	evl_nrthreads--;

	if (curr->state & T_READY) {
		EVL_WARN_ON(CORE, (curr->state & EVL_THREAD_BLOCK_BITS));
		evl_dequeue_thread(curr);
		curr->state &= ~T_READY;
	}

	if (curr->state & T_PEND)
		evl_forget_syn_waiter(curr);

	/*
	 * NOTE: we must be running over the root thread, or @curr
	 * is dormant, which means that we don't risk sched->curr to
	 * disappear due to voluntary rescheduling while holding the
	 * nklock, despite @curr bears the zombie bit.
	 */
	curr->state |= T_ZOMBIE;

	/* Release all resources owned by current. */
	for_each_evl_booster_safe(synch, tmp, curr)
		evl_release_syn(synch);

	xnlock_put_irqrestore(&nklock, flags);

	uninit_thread(curr);
}

static void cleanup_current_thread(void)
{
	struct oob_thread_state *p = dovetail_current_state();
	struct evl_thread *curr = evl_current_thread();

	/*
	 * We are called for exiting kernel and user threads over the
	 * in-band context.
	 */
	trace_evl_thread_unmap(curr);
	dovetail_stop_altsched();
	do_cleanup_current(curr);

	/* Wake up the joiner if any (we can't have more than one). */
	complete(&curr->exited);

	/* Notify our exit to evl_killall() if need be. */
	if (waitqueue_active(&join_all))
		wake_up(&join_all);

	p->thread = NULL;	/* evl_current_thread() <- NULL */
}

static void put_current_thread(void)
{
	struct evl_thread *curr = evl_current_thread();

	cleanup_current_thread();
	evl_put_element(&curr->element);
}

static void wakeup_kthread_parent(struct irq_work *irq_work)
{
	struct evl_kthread *kthread;
	kthread = container_of(irq_work, struct evl_kthread, irq_work);
	complete(&kthread->done);
}

static int map_kthread_self(struct evl_kthread *kthread)
{
	struct evl_thread *curr = &kthread->thread;
	unsigned long flags;

	pin_to_initial_cpu(curr);

	dovetail_init_altsched(&curr->altsched);
	evl_stop_thread(curr, T_INBAND);
	set_oob_threadinfo(curr);
	dovetail_start_altsched();
	evl_resume_thread(curr, T_DORMANT);

	trace_evl_thread_map(curr);

	/*
	 * Upon -weird- error from evl_switch_oob() for an emerging
	 * kernel thread, we still finalize the registration but the
	 * caller should self-cancel eventually.
	 */
	kthread->status = evl_switch_oob();

	/*
	 * In the normal case, __evl_run_kthread() can't start us
	 * before we stopped in evl_stop_thread() because
	 * irq_work_queue() schedules the in-band wakeup request on
	 * the current CPU: since we are currently running OOB on that
	 * CPU, there is no way __evl_run_kthread() could resume
	 * before we suspend. If we fail switching to OOB context,
	 * kthread->status tells __evl_run_kthread() not to start but
	 * cancel us instead.
	 */
	init_irq_work(&kthread->irq_work, wakeup_kthread_parent);
	irq_work_queue(&kthread->irq_work);

	xnlock_get_irqsave(&nklock, flags);
	enlist_new_thread(curr);
	xnlock_put_irqrestore(&nklock, flags);
	evl_stop_thread(curr, T_DORMANT);

	return kthread->status;
}

static int kthread_trampoline(void *arg)
{
	struct evl_kthread *kthread = arg;
	struct evl_thread *curr = &kthread->thread;
	struct sched_param param;
	int policy, prio, ret;

	/*
	 * It only makes sense to create EVL kthreads with the
	 * SCHED_FIFO, SCHED_NORMAL or SCHED_WEAK policies. So
	 * anything that is not from EVL's RT class is assumed to
	 * belong to in-band SCHED_NORMAL.
	 */
	if (curr->sched_class != &evl_sched_rt) {
		policy = SCHED_NORMAL;
		prio = 0;
	} else {
		policy = SCHED_FIFO;
		prio = curr->cprio;
		/* Normalize priority linux-wise. */
		if (prio >= MAX_RT_PRIO)
			prio = MAX_RT_PRIO - 1;
	}

	param.sched_priority = prio;
	sched_setscheduler(current, policy, &param);

	ret = map_kthread_self(kthread);
	if (!ret) {
		trace_evl_kthread_entry(curr);
		kthread->threadfn(kthread);
	}

	/* Handles nitty-gritty details like in-band switch. */
	evl_cancel_thread(curr);

	return 0;
}

int __evl_run_kthread(struct evl_kthread *kthread)
{
	struct evl_thread *thread = &kthread->thread;
	struct task_struct *p;
	int ret;

	ret = evl_init_element(&thread->element, &evl_thread_factory);
	if (ret)
		goto fail_element;

	ret = evl_create_element_device(&thread->element,
					&evl_thread_factory,
					thread->name);
	if (ret)
		goto fail_device;

	p = kthread_run(kthread_trampoline, kthread, "%s", thread->name);
	if (IS_ERR(p)) {
		ret = PTR_ERR(p);
		goto fail_spawn;
	}

	evl_index_element(&thread->element);
	wait_for_completion(&kthread->done);
	if (kthread->status)
		return kthread->status;

	evl_start_thread(thread);

	return 0;

fail_spawn:
	evl_remove_element_device(&thread->element);
fail_device:
	evl_destroy_element(&thread->element);
fail_element:
	uninit_thread(thread);

	return ret;
}
EXPORT_SYMBOL_GPL(__evl_run_kthread);

void evl_start_thread(struct evl_thread *thread)
{
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);

	/*
	 * A user-space thread starts immediately EVL-wise since we
	 * already have an underlying in-band context for it, so we
	 * can enlist it now.
	 *
	 * NOTE: starting an already started thread moves it to the
	 * head of the runqueue if running, nop otherwise.
	 */
	if ((thread->state & (T_DORMANT|T_USER)) == (T_DORMANT|T_USER))
		enlist_new_thread(thread);

	trace_evl_thread_start(thread);
	evl_resume_thread(thread, T_DORMANT);
	evl_schedule();

	xnlock_put_irqrestore(&nklock, flags);
}
EXPORT_SYMBOL_GPL(evl_start_thread);

void evl_suspend_thread(struct evl_thread *thread, int mask,
			ktime_t timeout, enum evl_tmode timeout_mode,
			struct evl_clock *clock,
			struct evl_syn *wchan)
{
	unsigned long oldstate;
	struct evl_rq *rq;
	unsigned long flags;

	/*
	 * Two things we can't do: suspend the root thread, ask for a
	 * conjunctive wait on multiple syns.
	 */
	if (EVL_WARN_ON(CORE, (thread->state & T_ROOT) ||
			(wchan && thread->wchan)))
		return;

	xnlock_get_irqsave(&nklock, flags);

	trace_evl_suspend_thread(thread, mask, timeout,
				 timeout_mode, clock, wchan);
	rq = thread->rq;
	oldstate = thread->state;

	/*
	 * If attempting to suspend a runnable thread which is pending
	 * a forced switch to in-band context (T_KICKED), just raise
	 * the T_BREAK status and return immediately, except if we are
	 * precisely doing such switch by applying T_INBAND.
	 *
	 * In the latter case, we also make sure to clear T_KICKED,
	 * since we won't go through prepare_for_signal() once
	 * running in in-band context.
	 */
	if (likely((oldstate & EVL_THREAD_BLOCK_BITS) == 0)) {
		if (likely((mask & T_INBAND) == 0)) {
			if (thread->info & T_KICKED)
				goto abort;
		}
		if (thread == rq->curr)
			thread->info &= ~(T_RMID|T_TIMEO|T_BREAK|
					  T_WAKEN|T_ROBBED|T_KICKED);
	}

	/*
	 * Don't start the timer for a thread delayed
	 * indefinitely. Zero relative delay currently means infinite
	 * wait, all other combinations mean timed wait.
	 */
	if (timeout_mode != EVL_REL || !timeout_infinite(timeout)) {
		evl_prepare_timer_wait(&thread->rtimer, clock,
				       evl_thread_rq(thread));
		if (timeout_mode == EVL_REL)
			timeout = evl_abs_timeout(&thread->rtimer, timeout);
		evl_start_timer(&thread->rtimer, timeout, EVL_INFINITE);
		thread->state |= T_DELAY;
	}

	if (oldstate & T_READY) {
		evl_dequeue_thread(thread);
		thread->state &= ~T_READY;
	}

	thread->state |= mask;

	/*
	 * We must make sure that we don't clear the wait channel if a
	 * thread is first blocked (wchan != NULL) then forcibly
	 * suspended (wchan == NULL), since these are conjunctive
	 * conditions.
	 */
	if (wchan)
		thread->wchan = wchan;

	/*
	 * If the current thread is switching to in-band context, we
	 * must have been called from evl_switch_inband(), in which
	 * case we introduce an opportunity for interrupt delivery
	 * right before switching context, which shortens the
	 * uninterruptible code path.
	 *
	 * CAVEAT: dovetail_leave_head() must run _before_ the in-band
	 * kernel is allowed to take interrupts again from the root
	 * stage, so that try_to_wake_up() does not block the wake up
	 * request for the switching thread, testing
	 * task_is_off_stage().
	 */
	if (likely(thread == rq->curr)) {
		evl_set_resched(rq);
		if (unlikely(mask & T_INBAND)) {
			dovetail_leave_oob();
			xnlock_clear_irqon(&nklock);
			oob_irq_disable();
			__evl_schedule(rq);
			oob_irq_enable();
			dovetail_resume_inband();
			return;
		}
		/*
		 * If the thread is running on another CPU,
		 * evl_schedule will trigger the IPI as required.
		 */
		__evl_schedule(rq);
		goto out;
	}

	/*
	 * Ok, this one is an interesting corner case, which requires
	 * a bit of background first. Here, we handle the case of
	 * suspending an in-band user thread which is _not_ current.
	 *
	 * The net effect is that we are attempting to stop the thread
	 * for the EVL core, whilst it is actually running some code
	 * under the control of the Linux scheduler.
	 *
	 *  To make this possible, we force the target task to switch
	 * back to the OOB context by sending it a
	 * SIGSHADOW_ACTION_HOME request.
	 *
	 * By forcing this switch, we make sure that EVL controls,
	 * hence properly stops, the target thread according to the
	 * requested suspension condition. Otherwise, the thread
	 * currently running in-band would just keep doing so, thus
	 * breaking the most common assumptions regarding suspended
	 * threads.
	 *
	 * We only care for threads that are not current, and for
	 * T_SUSP, T_DELAY, T_DORMANT and T_HALT conditions, because:
	 *
	 * - There is no point in dealing with in-band threads, since
	 * any OOB request causes the caller to switch to OOB context
	 * before it is handled.
	 *
	 * - among all blocking bits (EVL_THREAD_BLOCK_BITS), only
	 * T_SUSP, T_DELAY and T_HALT may be applied by the current
	 * thread to a non-current thread. T_PEND is always added by
	 * the caller to its own state, T_INBAND has special semantics
	 * escaping this issue.
	 *
	 * We don't signal threads which are already in a dormant
	 * state, since they are suspended by definition.
	 */
	if (((oldstate & (EVL_THREAD_BLOCK_BITS|T_USER)) == (T_INBAND|T_USER)) &&
	    (mask & (T_DELAY | T_SUSP | T_HALT)) != 0)
		evl_signal_thread(thread, SIGSHADOW,
				  SIGSHADOW_ACTION_HOME);
out:
	xnlock_put_irqrestore(&nklock, flags);
	return;

abort:
	if (wchan) {
		thread->wchan = wchan;
		evl_forget_syn_waiter(thread);
	}
	thread->info &= ~(T_RMID|T_TIMEO);
	thread->info |= T_BREAK;
	xnlock_put_irqrestore(&nklock, flags);
}
EXPORT_SYMBOL_GPL(evl_suspend_thread);

static void inband_task_wakeup(struct irq_work *work)
{
	struct evl_thread *thread;
	struct task_struct *p;

	thread = container_of(work, struct evl_thread, inband_work);
	p = thread->altsched.task;
	trace_evl_inband_wakeup(p);
	wake_up_process(p);
}

void evl_switch_inband(int cause)
{
	struct evl_thread *curr = evl_current_thread();
	struct task_struct *p = current;
	struct kernel_siginfo si;
	int cpu __maybe_unused;

	oob_context_only();

	trace_evl_switching_inband(cause);

	/*
	 * Enqueue the request to move the running thread from the oob
	 * stage to the in-band stage.  This will cause the in-band
	 * task to resume using the same register file.
	 *
	 * If you intend to change the following interrupt-free
	 * sequence, /first/ make sure to check the special handling
	 * of T_INBAND in evl_suspend_thread() when switching out the
	 * current thread, not to break basic assumptions we make
	 * there.
	 *
	 * We disable interrupts during the stage transition, but
	 * evl_suspend_thread() has an interrupts-on section built in.
	 */
	oob_irq_disable();
	irq_work_queue(&curr->inband_work);

	/*
	 * This is the only location where we may assert T_INBAND for a
	 * thread.
	 */
	evl_stop_thread(curr, T_INBAND);

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

#ifdef CONFIG_SMP
	if (curr->local_info & T_MOVED) {
		curr->local_info &= ~T_MOVED;
		cpu = evl_rq_cpu(curr->rq);
		set_cpus_allowed_ptr(p, cpumask_of(cpu));
	}
#endif

	if ((curr->state & T_USER) && cause != SIGDEBUG_UNDEFINED) {
		if (curr->state & T_WARN) {
			/* Help debugging spurious mode switches. */
			memset(&si, 0, sizeof(si));
			si.si_signo = SIGDEBUG;
			si.si_code = SI_QUEUE;
			si.si_int = cause | sigdebug_marker;
			send_sig_info(SIGDEBUG, &si, p);
		}
		evl_detect_boost_drop(curr);
	}

	/* @current is now running inband. */
	evl_sync_uwindow(curr);
}
EXPORT_SYMBOL_GPL(evl_switch_inband);

int evl_switch_oob(void)
{
	struct task_struct *p = current;
	struct evl_thread *curr;
	struct evl_rq *rq;
	int ret;

	inband_context_only();

	curr = evl_current_thread();
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
	 * current is now running on the head interrupt stage. Hard
	 * irqs must be off, otherwise something is really wrong in
	 * the Dovetail layer.
	 */
	if (EVL_WARN_ON_ONCE(CORE, !hard_irqs_disabled()))
		hard_irqs_disabled();

	rq = this_evl_rq();

	xnlock_clear_irqon(&nklock);
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
				  SIGDEBUG_UNDEFINED);
		return -ERESTARTSYS;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(evl_switch_oob);

void evl_stop_thread(struct evl_thread *thread, int mask)
{
	evl_suspend_thread(thread, mask, EVL_INFINITE,
			   EVL_REL, NULL, NULL);
}
EXPORT_SYMBOL_GPL(evl_stop_thread);

void evl_set_kthread_priority(struct evl_kthread *kthread, int priority)
{
	union evl_sched_param param = { .rt = { .prio = priority } };
	evl_set_thread_schedparam(&kthread->thread, &evl_sched_rt, &param);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_set_kthread_priority);

int evl_unblock_kthread(struct evl_kthread *kthread)
{
	int ret = evl_unblock_thread(&kthread->thread);
	evl_schedule();
	return ret;
}
EXPORT_SYMBOL_GPL(evl_unblock_kthread);

ktime_t evl_get_thread_timeout(struct evl_thread *thread)
{
	struct evl_timer *timer;
	ktime_t timeout, now;

	if (!(thread->state & T_DELAY))
		return 0LL;

	if (evl_timer_is_running(&thread->rtimer))
		timer = &thread->rtimer;
	else if (evl_timer_is_running(&thread->ptimer))
		timer = &thread->ptimer;
	else
		return 0;

	now = evl_ktime_monotonic();
	timeout = evl_get_timer_date(timer);
	if (timeout <= now)
		return ktime_set(0, 1);

	return ktime_sub(timeout, now);
}
EXPORT_SYMBOL_GPL(evl_get_thread_timeout);

ktime_t evl_get_thread_period(struct evl_thread *thread)
{
	ktime_t period = 0;
	/*
	 * The current thread period might be:
	 * - the value of the timer interval for periodic threads (ns/ticks)
	 * - or, the value of the alloted round-robin quantum (ticks)
	 * - or zero, meaning "no periodic activity".
	 */
	if (evl_timer_is_running(&thread->ptimer))
		period = thread->ptimer.interval;
	else if (thread->state & T_RRB)
		period = thread->rrperiod;

	return period;
}
EXPORT_SYMBOL_GPL(evl_get_thread_period);

ktime_t evl_delay_thread(ktime_t timeout, enum evl_tmode timeout_mode,
			 struct evl_clock *clock)
{
	struct evl_thread *curr = evl_current_thread();
	unsigned long flags;
	ktime_t rem = 0;

	evl_suspend_thread(curr, T_DELAY, timeout,
			   timeout_mode, clock, NULL);

	if (curr->info & T_BREAK) {
		xnlock_get_irqsave(&nklock, flags);
		rem = __evl_get_stopped_timer_delta(&curr->rtimer);
		xnlock_put_irqrestore(&nklock, flags);
	}

	return rem;
}
EXPORT_SYMBOL_GPL(evl_delay_thread);

void evl_resume_thread(struct evl_thread *thread, int mask)
{
	unsigned long oldstate;
	struct evl_rq *rq;
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);

	trace_evl_resume_thread(thread, mask);

	rq = thread->rq;
	oldstate = thread->state;

	if ((oldstate & EVL_THREAD_BLOCK_BITS) == 0) {
		if (oldstate & T_READY)
			evl_dequeue_thread(thread);
		goto enqueue;
	}

	/* Clear the specified block bit(s) */
	thread->state &= ~mask;

	/*
	 * If T_DELAY was set in the clear mask, evl_unblock_thread()
	 * was called for the thread, or a timeout has elapsed. In the
	 * latter case, stopping the timer is a no-op.
	 */
	if (mask & T_DELAY)
		evl_stop_timer(&thread->rtimer);

	if (!(thread->state & EVL_THREAD_BLOCK_BITS))
		goto clear_wchan;

	if (mask & T_DELAY) {
		mask = thread->state & T_PEND;
		if (mask == 0)
			goto unlock_and_exit;
		if (thread->wchan)
			evl_forget_syn_waiter(thread);
		goto recheck_state;
	}

	if (thread->state & T_DELAY) {
		if (mask & T_PEND) {
			/*
			 * A resource became available to the thread.
			 * Cancel the watchdog timer.
			 */
			evl_stop_timer(&thread->rtimer);
			thread->state &= ~T_DELAY;
		}
		goto recheck_state;
	}

	/*
	 * The thread is still suspended, but is no more pending on a
	 * resource.
	 */
	if ((mask & T_PEND) != 0 && thread->wchan)
		evl_forget_syn_waiter(thread);

	goto unlock_and_exit;

recheck_state:
	if (thread->state & EVL_THREAD_BLOCK_BITS)
		goto unlock_and_exit;

clear_wchan:
	/*
	 * If the thread was actually suspended, clear the wait
	 * channel. This allows requests like
	 * evl_suspend_thread(thread, T_DELAY,...) to skip the
	 * following code when the suspended thread is woken up while
	 * undergoing a simple delay.
	 */
	if ((mask & ~T_DELAY) != 0 && thread->wchan != NULL)
		evl_forget_syn_waiter(thread);

	if (unlikely((oldstate & mask) & T_HALT)) {
		evl_requeue_thread(thread);
		goto ready;
	}
enqueue:
	evl_enqueue_thread(thread);
ready:
	thread->state |= T_READY;
	evl_set_resched(rq);
unlock_and_exit:
	xnlock_put_irqrestore(&nklock, flags);
}
EXPORT_SYMBOL_GPL(evl_resume_thread);

int evl_unblock_thread(struct evl_thread *thread)
{
	unsigned long flags;
	int ret = 1;

	/*
	 * Attempt to abort an undergoing wait for the given thread.
	 * If this state is due to an alarm that has been armed to
	 * limit the sleeping thread's waiting time while it pends for
	 * a resource, the corresponding T_PEND state will be cleared
	 * by evl_resume_thread() in the same move. Otherwise, this call
	 * may abort an undergoing infinite wait for a resource (if
	 * any).
	 */
	xnlock_get_irqsave(&nklock, flags);

	trace_evl_unblock_thread(thread);

	if (thread->state & T_DELAY)
		evl_resume_thread(thread, T_DELAY);
	else if (thread->state & T_PEND)
		evl_resume_thread(thread, T_PEND);
	else
		ret = 0;

	/*
	 * We should not clear a previous break state if this service
	 * is called more than once before the target thread actually
	 * resumes, so we only set the bit here and never clear
	 * it. However, we must not raise the T_BREAK bit if the
	 * target thread was already awake at the time of this call,
	 * so that downstream code does not get confused by some
	 * "successful but interrupted syscall" condition. IOW, a
	 * break state raised here must always trigger an error code
	 * downstream, and an already successful syscall cannot be
	 * marked as interrupted.
	 */
	if (ret)
		thread->info |= T_BREAK;

	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_unblock_thread);

int evl_sleep_until(ktime_t timeout)
{
	ktime_t rem;

	if (!EVL_ASSERT(CORE, !evl_cannot_block()))
		return -EPERM;

	rem = evl_delay_thread(timeout, EVL_ABS, &evl_mono_clock);

	return rem ? -EINTR : 0;
}
EXPORT_SYMBOL_GPL(evl_sleep_until);

int evl_sleep(ktime_t delay)
{
	ktime_t end = ktime_add(evl_read_clock(&evl_mono_clock), delay);
	return evl_sleep_until(end);
}
EXPORT_SYMBOL_GPL(evl_sleep);

int evl_set_thread_period(struct evl_clock *clock,
			  ktime_t idate, ktime_t period)
{
	struct evl_thread *curr = evl_current_thread();
	unsigned long flags;
	int ret = 0;

	if (curr == NULL)
		return -EPERM;

	if (clock == NULL || period == EVL_INFINITE) {
		evl_stop_timer(&curr->ptimer);
		return 0;
	}

	xnlock_get_irqsave(&nklock, flags);

	/*
	 * LART: detect periods which are shorter than the target
	 * clock gravity for kernel thread timers. This can't work,
	 * caller must have messed up arguments.
	 */
	if (period < evl_get_clock_gravity(clock, kernel)) {
		ret = -EINVAL;
		goto unlock_and_exit;
	}

	evl_prepare_timer_wait(&curr->ptimer, clock,
			       evl_thread_rq(curr));

	if (timeout_infinite(idate))
		idate = evl_abs_timeout(&curr->ptimer, period);

	evl_start_timer(&curr->ptimer, idate, period);

unlock_and_exit:
	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_set_thread_period);

int evl_wait_thread_period(unsigned long *overruns_r)
{
	unsigned long overruns = 0, flags;
	struct evl_thread *curr;
	struct evl_clock *clock;
	ktime_t now;
	int ret = 0;

	if (!EVL_ASSERT(CORE, !evl_cannot_block()))
		return -EPERM;

	curr = evl_current_thread();

	xnlock_get_irqsave(&nklock, flags);

	if (unlikely(!evl_timer_is_running(&curr->ptimer))) {
		ret = -EWOULDBLOCK;
		goto out;
	}

	trace_evl_thread_wait_period(curr);

	clock = curr->ptimer.clock;
	now = evl_read_clock(clock);
	if (likely(now < evl_get_timer_next_date(&curr->ptimer))) {
		evl_stop_thread(curr, T_DELAY);
		if (unlikely(curr->info & T_BREAK)) {
			ret = -EINTR;
			goto out;
		}
	}

	overruns = evl_get_timer_overruns(&curr->ptimer);
	if (overruns) {
		ret = -ETIMEDOUT;
		trace_evl_thread_missed_period(curr);
	}

	if (likely(overruns_r != NULL))
		*overruns_r = overruns;
out:
	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_wait_thread_period);

void evl_cancel_thread(struct evl_thread *thread)
{
	unsigned long flags;

	if (EVL_WARN_ON(CORE, thread->state & T_ROOT))
		return;

	xnlock_get_irqsave(&nklock, flags);

	if (thread->info & T_CANCELD)
		goto check_self_cancel;

	trace_evl_thread_cancel(thread);

	thread->info |= T_CANCELD;

	/*
	 * If @thread is not started yet, fake a start request,
	 * raising the kicked condition bit to make sure it will reach
	 * evl_test_cancel() on its wakeup path.
	 *
	 * NOTE: if T_DORMANT and !T_INBAND, then some non-mapped
	 * emerging thread is self-cancelling due to an early error in
	 * the prep work.
	 */
	if (thread->state & T_DORMANT) {
		if (!(thread->state & T_INBAND))
			goto check_self_cancel;
		thread->info |= T_KICKED;
		evl_resume_thread(thread, T_DORMANT);
		goto out;
	}

check_self_cancel:
	if (evl_current_thread() == thread) {
		xnlock_put_irqrestore(&nklock, flags);
		evl_test_cancel();
		/*
		 * May return if on behalf of an IRQ handler which has
		 * preempted @thread.
		 */
		return;
	}

	/*
	 * Force the non-current thread to exit:
	 *
	 * - unblock a user thread, switch it to weak scheduling,
	 * then send it SIGTERM.
	 *
	 * - just unblock a kernel thread, it is expected to reach a
	 * cancellation point soon after
	 * (i.e. evl_test_cancel()).
	 */
	if (thread->state & T_USER) {
		__evl_demote_thread(thread);
		evl_signal_thread(thread, SIGTERM, 0);
	} else
		__evl_kick_thread(thread);
out:
	xnlock_put_irqrestore(&nklock, flags);

	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_cancel_thread);

int evl_detach_self(void)
{
	if (evl_current_thread() == NULL)
		return -EPERM;

	put_current_thread();

	return 0;
}

struct wait_grace_struct {
	struct completion done;
	struct rcu_head rcu;
};

static void grace_elapsed(struct rcu_head *head)
{
	struct wait_grace_struct *wgs;

	wgs = container_of(head, struct wait_grace_struct, rcu);
	complete(&wgs->done);
}

static void wait_for_rcu_grace_period(struct pid *pid)
{
	struct wait_grace_struct wait = {
		.done = COMPLETION_INITIALIZER_ONSTACK(wait.done),
	};
	struct task_struct *p;

	init_rcu_head_on_stack(&wait.rcu);

	for (;;) {
		call_rcu(&wait.rcu, grace_elapsed);
		wait_for_completion(&wait.done);
		if (pid == NULL)
			break;
		rcu_read_lock(); /* pid_task() is RCU-protected. */
		p = pid_task(pid, PIDTYPE_PID);
		rcu_read_unlock();
		if (p == NULL)
			break;
		reinit_completion(&wait.done);
	}
}

int evl_join_thread(struct evl_thread *thread, bool uninterruptible)
{
	struct evl_thread *curr = evl_current_thread();
	bool switched = false;
	unsigned long flags;
	struct pid *pid;
	int ret = 0;
	pid_t tpid;

	if (EVL_WARN_ON(CORE, thread->state & T_ROOT))
		return -EINVAL;

	if (thread == curr)
		return -EDEADLK;

	xnlock_get_irqsave(&nklock, flags);

	/*
	 * We allow multiple callers to join @thread, this is purely a
	 * synchronization mechanism with no resource collection.
	 */

	if (thread->info & T_DORMANT)
		goto out;

	trace_evl_thread_join(thread);

	tpid = evl_get_inband_pid(thread);

	if (curr && !(curr->state & T_INBAND)) {
		xnlock_put_irqrestore(&nklock, flags);
		evl_switch_inband(SIGDEBUG_UNDEFINED);
		switched = true;
	} else
		xnlock_put_irqrestore(&nklock, flags);

	/*
	 * Since in theory, we might be sleeping there for a long
	 * time, we get a reference on the pid struct holding our
	 * target, then we check for its existence upon wake up.
	 */
	pid = find_get_pid(tpid);
	if (pid == NULL)
		goto done;

	/*
	 * We have a tricky issue to deal with, which involves code
	 * relying on the assumption that a destroyed thread will have
	 * scheduled away from do_exit() before evl_join_thread()
	 * returns. A typical example is illustrated by the following
	 * sequence, with a EVL kthread implemented in a dynamically
	 * loaded module:
	 *
	 * CPU0:  evl_cancel_kthread(kthread)
	 *           evl_cancel_thread(kthread)
	 *           evl_join_thread(kthread)
	 *        ...<back to user>..
	 *        rmmod(module)
	 *
	 * CPU1:  in kthread()
	 *        ...
	 *        ...
	 *          __evl_test_cancel()
	 *             do_exit()
         *                schedule()
	 *
	 * In such a sequence, the code on CPU0 would expect the EVL
	 * kthread to have scheduled away upon return from
	 * evl_cancel_kthread(), so that unmapping the cancelled
	 * kthread code and data memory when unloading the module is
	 * always safe.
	 *
	 * To address this, the joiner first waits for the joinee to
	 * signal completion from the EVL thread cleanup handler
	 * (cleanup_current_thread), then waits for a full RCU grace
	 * period to have elapsed. Since the completion signal is sent
	 * on behalf of do_exit(), we may assume that the joinee has
	 * scheduled away before the RCU grace period ends.
	 */
	if (uninterruptible)
		wait_for_completion(&thread->exited);
	else {
		ret = wait_for_completion_interruptible(&thread->exited);
		if (ret < 0) {
			put_pid(pid);
			return -EINTR;
		}
	}

	/* Make sure the joinee has scheduled away ultimately. */
	wait_for_rcu_grace_period(pid);

	put_pid(pid);
done:
	ret = 0;
	if (switched)
		ret = evl_switch_oob();

	return ret;
out:
	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_join_thread);

#ifdef CONFIG_SMP

void evl_migrate_thread(struct evl_thread *thread, struct evl_rq *rq)
{				/* nklocked, IRQs off */
	if (thread->rq == rq)
		return;

	trace_evl_thread_migrate(thread, evl_rq_cpu(rq));
	/*
	 * Timer migration is postponed until the next timeout happens
	 * for the periodic and rrb timers. The resource timer will be
	 * moved to the right CPU next time it is armed in
	 * evl_suspend_thread().
	 */
	evl_migrate_rq(thread, rq);

	evl_reset_account(&thread->stat.lastperiod);
}

#endif	/* CONFIG_SMP */

int evl_set_thread_schedparam(struct evl_thread *thread,
			      struct evl_sched_class *sched_class,
			      const union evl_sched_param *sched_param)
{
	unsigned long flags;
	int ret;

	xnlock_get_irqsave(&nklock, flags);
	ret = __evl_set_thread_schedparam(thread, sched_class, sched_param);
	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_set_thread_schedparam);

int __evl_set_thread_schedparam(struct evl_thread *thread,
				struct evl_sched_class *sched_class,
				const union evl_sched_param *sched_param)
{
	int old_wprio, new_wprio, ret;

	old_wprio = thread->wprio;

	ret = evl_set_thread_policy(thread, sched_class, sched_param);
	if (ret)
		return ret;

	new_wprio = thread->wprio;

	/*
	 * If the thread is waiting on a synchronization object,
	 * update its position in the corresponding wait queue, unless
	 * the (weighted) priority has not changed (to prevent
	 * spurious round-robin effects).
	 */
	if (old_wprio != new_wprio && thread->wchan &&
	    (thread->wchan->status & EVL_SYN_PRIO))
		evl_requeue_syn_waiter(thread);
	/*
	 * We should not move the thread at the end of its priority
	 * group, if any of these conditions is true:
	 *
	 * - thread is not runnable;
	 * - thread bears the ready bit which means that evl_set_thread_policy()
	 * already reordered the run queue;
	 * - thread currently holds the scheduler lock, so we don't want
	 * any round-robin effect to take place;
	 * - a priority boost is undergoing for this thread.
	 */
	if (!(thread->state & (EVL_THREAD_BLOCK_BITS|T_READY|T_BOOST)) &&
	    thread->lock_count == 0)
		evl_putback_thread(thread);

	thread->info |= T_SCHEDP;
	/* Ask the target thread to call back if in-band. */
	if (thread->state & T_INBAND)
		evl_signal_thread(thread, SIGSHADOW,
				  SIGSHADOW_ACTION_HOME);

	return ret;
}

void __evl_test_cancel(struct evl_thread *curr)
{
	/*
	 * Just in case evl_test_cancel() is called from an IRQ
	 * handler, in which case we may not take the exit path.
	 *
	 * NOTE: curr->rq is stable from our POV and can't change
	 * under our feet.
	 */
	if (curr->rq->lflags & RQ_IRQ)
		return;

	if (!(curr->state & T_INBAND))
		evl_switch_inband(SIGDEBUG_UNDEFINED);

	do_exit(0);
	/* ... won't return ... */
	EVL_WARN_ON(EVENLESS, 1);
}
EXPORT_SYMBOL_GPL(__evl_test_cancel);

void __evl_propagate_schedparam_change(struct evl_thread *curr)
{
	int kpolicy = SCHED_FIFO, kprio = curr->bprio, ret;
	struct task_struct *p = current;
	struct sched_param param;
	unsigned long flags;

	/*
	 * Test-set race for T_SCHEDP is ok, the propagation is meant
	 * to be done asap but not guaranteed to be carried out
	 * immediately, and the request will remain pending until it
	 * is eventually handled. We just have to protect against a
	 * set-clear race.
	 */
	xnlock_get_irqsave(&nklock, flags);
	curr->info &= ~T_SCHEDP;
	xnlock_put_irqrestore(&nklock, flags);

	/*
	 * Map our policies/priorities to the regular kernel's
	 * (approximated).
	 */
	if ((curr->state & T_WEAK) && kprio == 0)
		kpolicy = SCHED_NORMAL;
	else if (kprio >= MAX_USER_RT_PRIO)
		kprio = MAX_USER_RT_PRIO - 1;

	if (p->policy != kpolicy || (kprio > 0 && p->rt_priority != kprio)) {
		param.sched_priority = kprio;
		ret = sched_setscheduler_nocheck(p, kpolicy, &param);
		EVL_WARN_ON(CORE, ret != 0);
	}
}

static int force_wakeup(struct evl_thread *thread) /* nklock locked, irqs off */
{
	int ret = 0;

	if (thread->info & T_KICKED)
		return 1;

	if (evl_unblock_thread(thread)) {
		thread->info |= T_KICKED;
		ret = 1;
	}

	/*
	 * CAUTION: we must NOT raise T_BREAK when clearing a forcible
	 * block state, such as T_SUSP, T_HALT. The caller of
	 * evl_suspend_thread() we unblock shall proceed as for a
	 * normal return, until it traverses a cancellation point if
	 * T_CANCELD was raised earlier, or calls
	 * evl_suspend_thread() which will detect T_KICKED and act
	 * accordingly.
	 *
	 * Rationale: callers of evl_suspend_thread() may assume
	 * that receiving T_BREAK means that the process that
	 * motivated the blocking did not go to completion. E.g. the
	 * wait context was NOT updated before evl_sleep_on_syn()
	 * returned, leaving no useful data there.  Therefore, in case
	 * only T_SUSP remains set for the thread on entry to
	 * force_wakeup(), after T_PEND was lifted earlier when the
	 * wait went to successful completion (i.e. no timeout), then
	 * we want the kicked thread to know that it did receive the
	 * requested resource, not finding T_BREAK in its state word.
	 *
	 * Callers of evl_suspend_thread() may inquire for T_KICKED
	 * to detect forcible unblocks from T_SUSP, T_HALT, if they
	 * should act upon this case specifically.
	 */
	if (thread->state & (T_SUSP|T_HALT)) {
		evl_resume_thread(thread, T_SUSP|T_HALT);
		thread->info |= T_KICKED;
	}

	/*
	 * Tricky cases:
	 *
	 * - a thread which was ready on entry wasn't actually
	 * running, but nevertheless waits for the CPU in OOB context,
	 * so we have to make sure that it will be notified of the
	 * pending break condition as soon as it enters
	 * evl_suspend_thread() from a blocking EVL syscall.
	 *
	 * - a ready/readied thread on exit may be prevented from
	 * running by the scheduling policy module it belongs
	 * to. Typically, policies enforcing a runtime budget do not
	 * block threads with no budget, but rather keep them out of
	 * their run queue, so that ->sched_pick() won't elect
	 * them. We tell the policy handler about the fact that we do
	 * want such thread to run until it switches to in-band
	 * context, whatever this entails internally for the
	 * implementation.
	 */
	if (thread->state & T_READY)
		evl_force_thread(thread);

	return ret;
}

void __evl_kick_thread(struct evl_thread *thread) /* nklock locked, irqs off */
{
	struct task_struct *p = thread->altsched.task;

	if (thread->state & T_INBAND) /* nop? */
		return;

	/*
	 * First, try to kick the thread out of any blocking syscall
	 * EVL-wise. If that succeeds, then the thread will switch to
	 * in-band context on its return path to user-space.
	 */
	if (force_wakeup(thread))
		return;

	/*
	 * If that did not work out because the thread was not blocked
	 * (i.e. T_PEND/T_DELAY) in a syscall, then force a mayday
	 * trap. Note that we don't want to send that thread any linux
	 * signal, we only want to force it to switch to in-band
	 * context asap.
	 */
	thread->info |= T_KICKED;

	/*
	 * We may send mayday signals to userland threads only.
	 * However, no need to run a mayday trap if the current thread
	 * kicks itself out of OOB context: it will switch to in-band
	 * context on its way back to userland via the current syscall
	 * epilogue. Otherwise, we want that thread to enter the
	 * mayday trap asap.
	 */
	if (thread != this_evl_rq_thread() &&
	    (thread->state & T_USER))
		dovetail_send_mayday(p);
}

void evl_kick_thread(struct evl_thread *thread)
{
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);
	__evl_kick_thread(thread);
	xnlock_put_irqrestore(&nklock, flags);
}
EXPORT_SYMBOL_GPL(evl_kick_thread);

void __evl_demote_thread(struct evl_thread *thread) /* nklock locked, irqs off */
{
	struct evl_sched_class *sched_class;
	union evl_sched_param param;

	/*
	 * First we kick the thread out of oob context, and have it
	 * resume execution immediately on the in-band stage.
	 */
	__evl_kick_thread(thread);

	/*
	 * Then we demote it, turning that thread into a non real-time
	 * EVL thread, which still has access to EVL resources, but
	 * won't compete for real-time scheduling anymore. In effect,
	 * moving the thread to a weak scheduling class/priority will
	 * prevent it from sticking back to OOB context.
	 */
	param.weak.prio = 0;
	sched_class = &evl_sched_weak;
	__evl_set_thread_schedparam(thread, sched_class, &param);
}

void evl_demote_thread(struct evl_thread *thread)
{
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);
	__evl_demote_thread(thread);
	xnlock_put_irqrestore(&nklock, flags);
}
EXPORT_SYMBOL_GPL(evl_demote_thread);

struct inband_signal {
	fundle_t fundle;
	int signo, sigval;
	struct irq_work work;
};

static void inband_task_signal(struct irq_work *work)
{
	struct evl_thread *thread;
	struct inband_signal *req;
	struct kernel_siginfo si;
	struct task_struct *p;
	int signo;

	req = container_of(work, struct inband_signal, work);
	thread = evl_get_element_by_fundle(&evl_thread_factory,
			   req->fundle, struct evl_thread);
	if (thread == NULL)
		goto done;

	p = thread->altsched.task;
	signo = req->signo;
	trace_evl_inband_signal(p, signo);

	if (signo == SIGSHADOW || signo == SIGDEBUG) {
		memset(&si, '\0', sizeof(si));
		si.si_signo = signo;
		si.si_code = SI_QUEUE;
		si.si_int = req->sigval;
		send_sig_info(signo, &si, p);
	} else
		send_sig(signo, p, 1);

	evl_put_element(&thread->element);
done:
	evl_free(req);
}

void evl_signal_thread(struct evl_thread *thread, int sig, int arg)
{
	struct inband_signal *req;

	if (EVL_WARN_ON(CORE, !(thread->state & T_USER)))
		return;

	req = evl_alloc(sizeof(*req));
	init_irq_work(&req->work, inband_task_signal);
	req->fundle = fundle_of(thread);
	req->signo = sig;
	req->sigval = sig == SIGDEBUG ? arg | sigdebug_marker : arg;
	irq_work_queue(&req->work);
}
EXPORT_SYMBOL_GPL(evl_signal_thread);

#ifdef CONFIG_MMU

static inline int commit_process_memory(void)
{
	struct task_struct *p = current;

	if (!(p->mm->def_flags & VM_LOCKED))
		return -EINVAL;

	return force_commit_memory();
}

#else /* !CONFIG_MMU */

static inline int commit_process_memory(void)
{
	return 0;
}

#endif /* !CONFIG_MMU */

/* nklock locked, irqs off */
void evl_call_mayday(struct evl_thread *thread, int reason)
{
	struct task_struct *p = thread->altsched.task;

	/* Mayday traps are available to userland threads only. */
	if (EVL_WARN_ON(CORE, !(thread->state & T_USER)))
		return;

	thread->info |= T_KICKED;
	evl_signal_thread(thread, SIGDEBUG, reason);
	dovetail_send_mayday(p);
}
EXPORT_SYMBOL_GPL(evl_call_mayday);

int evl_killall(int mask)
{
	int nrkilled = 0, nrthreads, count;
	struct evl_thread *t;
	unsigned long flags;
	long ret;

	inband_context_only();

	if (evl_current_thread())
		return -EPERM;

	/*
	 * We may hold the core lock across calls to evl_cancel_thread()
	 * provided that we won't self-cancel.
	 */
	xnlock_get_irqsave(&nklock, flags);

	nrthreads = evl_nrthreads;

	for_each_evl_thread(t) {
		if ((t->state & T_ROOT) || (t->state & mask) != mask)
			continue;

		if (EVL_DEBUG(CORE))
			printk(EVL_INFO "terminating %s[%d]\n",
			       t->name, evl_get_inband_pid(t));
		nrkilled++;
		evl_cancel_thread(t);
	}

	xnlock_put_irqrestore(&nklock, flags);

	count = nrthreads - nrkilled;
	if (EVL_DEBUG(CORE))
		printk(EVL_INFO "waiting for %d threads to exit\n",
		       nrkilled);

	ret = wait_event_interruptible(join_all,
				       evl_nrthreads == count);

	/* Wait for a full RCU grace period to expire. */
	wait_for_rcu_grace_period(NULL);

	if (EVL_DEBUG(CORE))
		printk(EVL_INFO "joined %d threads\n",
		       count + nrkilled - evl_nrthreads);

	return ret < 0 ? -EINTR : 0;
}
EXPORT_SYMBOL_GPL(evl_killall);

pid_t evl_get_inband_pid(struct evl_thread *thread)
{
	if (thread->state & (T_ROOT|T_DORMANT|T_ZOMBIE))
		return 0;

	if (thread->altsched.task == NULL)
		return -1;	/* weird */

	return task_pid_nr(thread->altsched.task);
}

void arch_inband_task_init(struct task_struct *tsk)
{
	struct oob_thread_state *p = dovetail_task_state(tsk);

	evl_init_thread_state(p);
}

void handle_oob_trap(unsigned int trapnr, struct pt_regs *regs)
{
	struct evl_thread *curr;

	oob_context_only();

	curr = evl_current_thread();
	trace_evl_thread_fault(trapnr, regs);

#if defined(CONFIG_EVENLESS_DEBUG_CORE) || defined(CONFIG_EVENLESS_DEBUG_USER)
	if (xnarch_fault_notify(trapnr))
		printk(EVL_WARNING
		       "%s switching in-band [pid=%d, excpt=%#x, %spc=%#lx]\n",
		       curr->name,
		       evl_get_inband_pid(curr),
		       trapnr,
		       user_mode(regs) ? "" : "kernel_",
		       instruction_pointer(regs));
#endif
	if (xnarch_fault_pf_p(trapnr))
		/*
		 * The page fault counter is not SMP-safe, but it's a
		 * simple indicator that something went wrong wrt
		 * memory locking anyway.
		 */
		evl_inc_counter(&curr->stat.pf);

	/*
	 * We received a trap on the oob stage, switch to in-band
	 * before handling the exception.
	 */
	evl_switch_inband(xnarch_fault_notify(trapnr) ?
			  SIGDEBUG_MIGRATE_FAULT :
			  SIGDEBUG_UNDEFINED);
}

void handle_oob_mayday(struct pt_regs *regs)
{
	struct evl_thread *curr = evl_current_thread();

	if (EVL_WARN_ON(CORE, !(curr->state & T_USER)))
		return;

	/*
	 * It might happen that a thread gets a mayday trap right
	 * after it switched to in-band mode while returning from a
	 * syscall. Filter this case out.
	 */
	if (!(curr->state & T_INBAND))
		evl_switch_inband(SIGDEBUG_UNDEFINED);
}

#ifdef CONFIG_SMP

static void handle_migration_event(struct dovetail_migration_data *d)
{
	struct task_struct *p = d->task;
	struct evl_thread *thread;

	thread = evl_thread_from_task(p);
	if (thread == NULL)
		return;

	/*
	 * Detect an EVL thread sleeping in OOB context which is
	 * required to migrate to another CPU by the in-band kernel.
	 *
	 * We may NOT fix up thread->sched immediately using the
	 * migration call, because the latter always has to take place
	 * on behalf of the target thread itself while running
	 * in-band. Therefore, that thread needs to switch to in-band
	 * context first, then move back to OOB, so that affinity_ok()
	 * does the fixup work.
	 *
	 * We force this by sending a SIGSHADOW signal to the migrated
	 * thread, asking it to switch back to OOB context from the
	 * handler, at which point the interrupted syscall may be
	 * restarted.
	 */
	if (thread->state & (EVL_THREAD_BLOCK_BITS & ~T_INBAND))
		evl_signal_thread(thread, SIGSHADOW,
				  SIGSHADOW_ACTION_HOME);
}

static inline bool affinity_ok(struct task_struct *p) /* nklocked, IRQs off */
{
	struct evl_thread *thread = evl_thread_from_task(p);
	struct evl_rq *rq;
	int cpu = task_cpu(p);

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
	 * affinity_ok() detects when a EVL thread switching back to
	 * OOB context did move to another CPU earlier while running
	 * in-band. If so, do the fixups to reflect the change.
	 */
	if (!is_threading_cpu(cpu)) {
		printk(EVL_WARNING "thread %s[%d] switched to non-rt CPU%d, aborted.\n",
		       thread->name, evl_get_inband_pid(thread), cpu);
		/*
		 * Can't call evl_cancel_thread() from a CPU migration
		 * point, that would break. Since we are on the wakeup
		 * path to OOB context, just raise T_CANCELD to catch
		 * it in evl_switch_oob().
		 */
		thread->info |= T_CANCELD;
		return false;
	}

	rq = evl_cpu_rq(cpu);
	if (rq == thread->rq)
		return true;

	/*
	 * The current thread moved to a supported real-time CPU,
	 * which is not part of its original affinity mask
	 * though. Assume user wants to extend this mask.
	 */
	if (!cpumask_test_cpu(cpu, &thread->affinity))
		cpumask_set_cpu(cpu, &thread->affinity);

	evl_migrate_thread(thread, rq);

	return true;
}

#else /* !CONFIG_SMP */

static void handle_migration_event(struct dovetail_migration_data *d)
{
}

static inline bool affinity_ok(struct task_struct *p)
{
	return true;
}

#endif /* CONFIG_SMP */

void resume_oob_task(struct task_struct *p) /* hw IRQs off */
{
	struct evl_thread *thread = evl_thread_from_task(p);

	/*
	 * We fire the handler before the thread is migrated, so that
	 * thread->rq does not change between paired invocations of
	 * relax_thread/harden handlers.
	 */
	xnlock_get(&nklock);
	if (affinity_ok(p))
		evl_resume_thread(thread, T_INBAND);
	xnlock_put(&nklock);

	evl_schedule();
}

static void handle_schedule_event(struct task_struct *next_task)
{
	struct task_struct *prev_task;
	struct evl_thread *next;
	unsigned long flags;
	sigset_t pending;

	evl_notify_inband_yield();

	prev_task = current;
	next = evl_thread_from_task(next_task);
	if (next == NULL)
		return;

	/*
	 * Check whether we need to unlock the timers, each time a
	 * Linux task resumes from a stopped state, excluding tasks
	 * resuming shortly for entering a stopped state asap due to
	 * ptracing. To identify the latter, we need to check for
	 * SIGSTOP and SIGINT in order to encompass both the NPTL and
	 * LinuxThreads behaviours.
	 */
	if (next->state & T_SSTEP) {
		if (signal_pending(next_task)) {
			/*
			 * Do not grab the sighand lock here: it's
			 * useless, and we already own the runqueue
			 * lock, so this would expose us to deadlock
			 * situations on SMP.
			 */
			sigorsets(&pending,
				  &next_task->pending.signal,
				  &next_task->signal->shared_pending.signal);
			if (sigismember(&pending, SIGSTOP) ||
			    sigismember(&pending, SIGINT))
				goto check;
		}
		xnlock_get_irqsave(&nklock, flags);
		next->state &= ~T_SSTEP;
		xnlock_put_irqrestore(&nklock, flags);
		next->local_info |= T_HICCUP;
	}

check:
	/*
	 * Do basic sanity checks on the incoming thread state.
	 * NOTE: we allow ptraced threads to run shortly in order to
	 * properly recover from a stopped state.
	 */
	if (!EVL_WARN(CORE, !(next->state & T_INBAND),
		      "Ouch: out-of-band thread %s[%d] running on the in-band stage"
		      "(status=0x%x, sig=%d, prev=%s[%d])",
		      next->name, task_pid_nr(next_task),
		      next->state,
		      signal_pending(next_task),
		      prev_task->comm, task_pid_nr(prev_task)))
		EVL_WARN(CORE,
			 !(next_task->ptrace & PT_PTRACED) &&
			 !(next->state & T_DORMANT)
			 && (next->state & T_PEND),
			 "Ouch: blocked EVL thread %s[%d] rescheduled in-band"
			 "(status=0x%x, sig=%d, prev=%s[%d])",
			 next->name, task_pid_nr(next_task),
			 next->state,
			 signal_pending(next_task), prev_task->comm,
			 task_pid_nr(prev_task));
}

static void handle_sigwake_event(struct task_struct *p)
{
	struct evl_thread *thread;
	unsigned long flags;
	sigset_t pending;

	thread = evl_thread_from_task(p);
	if (thread == NULL)
		return;

	xnlock_get_irqsave(&nklock, flags);

	/*
	 * CAUTION: __TASK_TRACED is not set in p->state yet. This
	 * state bit will be set right after we return, when the task
	 * is woken up.
	 */
	if ((p->ptrace & PT_PTRACED) && !(thread->state & T_SSTEP)) {
		/* We already own the siglock. */
		sigorsets(&pending,
			  &p->pending.signal,
			  &p->signal->shared_pending.signal);

		if (sigismember(&pending, SIGTRAP) ||
		    sigismember(&pending, SIGSTOP)
		    || sigismember(&pending, SIGINT))
			thread->state &= ~T_SSTEP;
	}

	if (thread->state & T_INBAND) {
		xnlock_put_irqrestore(&nklock, flags);
		return;
	}

	/*
	 * A thread running on the oob stage may not be picked by the
	 * in-band scheduler as it bears the _TLF_OFFSTAGE flag. We
	 * need to force that thread to switch to in-band context,
	 * which will clear that flag.
	 */
	__evl_kick_thread(thread);

	evl_schedule();

	xnlock_put_irqrestore(&nklock, flags);
}

static void handle_cleanup_event(struct mm_struct *mm)
{
	struct evl_thread *curr = evl_current_thread();

	/*
	 * Detect an EVL thread running exec(), i.e. still attached to
	 * the current Linux task (PF_EXITING is cleared for a task
	 * which did not explicitly run do_exit()). In this case, we
	 * emulate a task exit, since the EVL binding shall not
	 * survive the exec() syscall.
	 *
	 * NOTE: We are called for every userland task exiting from
	 * in-band context. We are NOT called for exiting kernel
	 * threads since they have no mm proper. We may get there
	 * after cleanup_current_thread() already ran though, so check
	 * @curr.
	 */
	if (curr && !(current->flags & PF_EXITING))
		put_current_thread();
}

void handle_inband_event(enum inband_event_type event, void *data)
{
	switch (event) {
	case INBAND_TASK_SCHEDULE:
		handle_schedule_event(data);
		break;
	case INBAND_TASK_SIGNAL:
		handle_sigwake_event(data);
		break;
	case INBAND_TASK_EXIT:
		put_current_thread();
		break;
	case INBAND_TASK_MIGRATION:
		handle_migration_event(data);
		break;
	case INBAND_PROCESS_CLEANUP:
		handle_cleanup_event(data);
		break;
	}
}

static int set_time_slice(struct evl_thread *thread, ktime_t quantum) /* nklock held, irqs off */
{
	struct evl_rq *rq;

	rq = thread->rq;
	thread->rrperiod = quantum;

	if (!timeout_infinite(quantum)) {
		if (quantum <= evl_get_clock_gravity(&evl_mono_clock, user))
			return -EINVAL;

		if (thread->base_class->sched_tick == NULL)
			return -EINVAL;

		thread->state |= T_RRB;
		if (rq->curr == thread)
			evl_start_timer(&rq->rrbtimer,
					evl_abs_timeout(&rq->rrbtimer, quantum),
					EVL_INFINITE);
	} else {
		thread->state &= ~T_RRB;
		if (rq->curr == thread)
			evl_stop_timer(&rq->rrbtimer);
	}

	return 0;
}

static int set_sched_attrs(struct evl_thread *thread,
			   const struct evl_sched_attrs *attrs)
{
	struct evl_sched_class *sched_class;
	union evl_sched_param param;
	unsigned long flags;
	int ret = -EINVAL;
	ktime_t tslice;

	trace_evl_thread_setsched(thread, attrs);

	xnlock_get_irqsave(&nklock, flags);

	tslice = thread->rrperiod;
	sched_class = evl_find_sched_class(&param, attrs, &tslice);
	if (sched_class == NULL)
		goto out;

	ret = set_time_slice(thread, tslice);
	if (ret)
		goto out;

	ret = __evl_set_thread_schedparam(thread, sched_class, &param);
out:
	xnlock_put_irqrestore(&nklock, flags);

	evl_schedule();

	return ret;
}

static int get_sched_attrs(struct evl_thread *thread,
			   struct evl_sched_attrs *attrs)
{
	struct evl_sched_class *base_class;
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);

	base_class = thread->base_class;
	attrs->sched_policy = base_class->policy;
	attrs->sched_priority = thread->bprio;
	if (attrs->sched_priority == 0) /* SCHED_FIFO/SCHED_WEAK */
		attrs->sched_policy = SCHED_NORMAL;

	if (base_class == &evl_sched_rt) {
		if (thread->state & T_RRB) {
			attrs->sched_rr_quantum =
				ktime_to_timespec(thread->rrperiod);
			attrs->sched_policy = SCHED_RR;
		}
		goto out;
	}

	if (base_class == &evl_sched_weak) {
		if (attrs->sched_policy != SCHED_WEAK)
			attrs->sched_priority = -attrs->sched_priority;
		goto out;
	}

#ifdef CONFIG_EVENLESS_SCHED_QUOTA
	if (base_class == &evl_sched_quota) {
		attrs->sched_quota_group = thread->quota->tgid;
		goto out;
	}
#endif

out:
	xnlock_put_irqrestore(&nklock, flags);

	trace_evl_thread_getsched(thread, attrs);

	return 0;
}

static int update_state_bits(struct evl_thread *thread,
			     __u32 mask, bool set)
{
	struct evl_thread *curr = evl_current_thread();
	unsigned long flags;

	if (curr != thread)
		return -EPERM;

	if (mask & ~T_WARN)
		return -EINVAL;

	trace_evl_thread_update_mode(mask, set);

	xnlock_get_irqsave(&nklock, flags);

	if (set)
		curr->state |= mask;
	else
		curr->state &= ~mask;

	xnlock_put_irqrestore(&nklock, flags);

	return 0;
}

static long thread_common_ioctl(struct evl_thread *thread,
				unsigned int cmd, unsigned long arg)
{
	struct evl_sched_attrs attrs;
	long ret;

	switch (cmd) {
	case EVL_THRIOC_SET_SCHEDPARAM:
		ret = raw_copy_from_user(&attrs,
					 (struct evl_sched_attrs *)arg, sizeof(attrs));
		if (ret)
			return -EFAULT;
		ret = set_sched_attrs(thread, &attrs);
		break;
	case EVL_THRIOC_GET_SCHEDPARAM:
		ret = get_sched_attrs(thread, &attrs);
		if (ret)
			return ret;
		ret = raw_copy_to_user((struct evl_sched_attrs *)arg,
				       &attrs, sizeof(attrs));
		if (ret)
			return -EFAULT;
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static long thread_oob_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	struct evl_thread *thread = element_of(filp, struct evl_thread);
	__u32 monfd, mask;
	long ret;

	if (thread->state & T_ZOMBIE)
		return -ESTALE;

	switch (cmd) {
	case EVL_THRIOC_SIGNAL:
		ret = raw_get_user(monfd, (__u32 *)arg);
		if (ret)
			return -EFAULT;
		ret = evl_signal_monitor_targeted(thread, monfd);
		break;
	case EVL_THRIOC_SET_MODE:
	case EVL_THRIOC_CLEAR_MODE:
		ret = raw_get_user(mask, (__u32 *)arg);
		if (ret)
			return -EFAULT;
		ret = update_state_bits(thread, mask,
					cmd == EVL_THRIOC_SET_MODE);
		break;
	default:
		ret = thread_common_ioctl(thread, cmd, arg);
	}

	return ret;
}

static long thread_ioctl(struct file *filp, unsigned int cmd,
			 unsigned long arg)
{
	struct evl_thread *thread = element_of(filp, struct evl_thread);
	long ret;

	if (thread->state & T_ZOMBIE)
		return -ESTALE;

	switch (cmd) {
	case EVL_THRIOC_JOIN:
		ret = evl_join_thread(thread, false);
		break;
	default:
		ret = thread_common_ioctl(thread, cmd, arg);
	}

	return ret;
}

static const struct file_operations thread_fops = {
	.open		= evl_open_element,
	.release	= evl_close_element,
	.unlocked_ioctl	= thread_ioctl,
	.oob_ioctl	= thread_oob_ioctl,
};

static int map_uthread_self(struct evl_thread *thread)
{
	struct evl_user_window *u_window;
	int ret;

	ret = commit_process_memory();
	if (ret)
		return ret;

	u_window = evl_zalloc_chunk(&evl_shared_heap, sizeof(*u_window));
	if (u_window == NULL)
		return -ENOMEM;

	thread->u_window = u_window;
	pin_to_initial_cpu(thread);
	trace_evl_thread_map(thread);

	dovetail_init_altsched(&thread->altsched);
	evl_stop_thread(thread, T_INBAND);
	set_oob_threadinfo(thread);

	/*
	 * CAUTION: we enable dovetailing only when *thread is
	 * consistent, so that we won't trigger false positive in
	 * debug code from handle_schedule_event() and friends.
	 */
	dovetail_start_altsched();
	evl_start_thread(thread);
	evl_sync_uwindow(thread);

	return 0;
}

/*
 * Deconstruct a thread we just failed to map over a userland task.
 * Since the former must be dormant, it can't be part of any runqueue.
 */
static void discard_unmapped_uthread(struct evl_thread *thread)
{
	unsigned long flags;

	evl_destroy_timer(&thread->rtimer);
	evl_destroy_timer(&thread->ptimer);

	xnlock_get_irqsave(&nklock, flags);

	if (!list_empty(&thread->next)) {
		list_del(&thread->next);
		evl_nrthreads--;
	}

	xnlock_put_irqrestore(&nklock, flags);

	if (thread->u_window)
		evl_free_chunk(&evl_shared_heap, thread->u_window);

	kfree(thread);
}

static struct evl_element *
thread_factory_build(struct evl_factory *fac, const char *name,
		     void __user *u_attrs, u32 *state_offp)
{
	struct task_struct *tsk = current;
	struct evl_init_thread_attr iattr;
	struct evl_thread *curr;
	int ret;

	if (evl_current_thread())
		return ERR_PTR(-EBUSY);

	curr = kzalloc(sizeof(*curr), GFP_KERNEL);
	if (curr == NULL)
		return ERR_PTR(-ENOMEM);

	ret = evl_init_element(&curr->element, &evl_thread_factory);
	if (ret) {
		kfree(curr);
		return ERR_PTR(ret);
	}

	iattr.flags = T_USER;
	iattr.affinity = CPU_MASK_ALL;
	iattr.sched_class = &evl_sched_weak;
	iattr.sched_param.weak.prio = 0;
	ret = evl_init_thread(curr, &iattr, NULL, "%s", name);
	if (ret) {
		evl_destroy_element(&curr->element);
		kfree(curr);
		return ERR_PTR(ret);
	}

	ret = map_uthread_self(curr);
	if (ret) {
		evl_destroy_element(&curr->element);
		discard_unmapped_uthread(curr);
		return ERR_PTR(ret);
	}

	*state_offp = evl_shared_offset(curr->u_window);
	evl_index_element(&curr->element);

	/*
	 * Unlike most elements, a thread may exist in absence of any
	 * file reference, so we get a reference on the emerging
	 * thread here to block automatic disposal on last file
	 * release. put_current_thread() drops this reference when the
	 * thread exits, or voluntarily detaches by sending the
	 * EVL_CTLIOC_DETACH_SELF control request.
	 */
	evl_get_element(&curr->element);

	strncpy(tsk->comm, name, sizeof(tsk->comm));
	tsk->comm[sizeof(tsk->comm) - 1] = '\0';

	return &curr->element;
}

static void thread_factory_dispose(struct evl_element *e)
{
	struct evl_thread *thread;

	thread = container_of(e, struct evl_thread, element);

	/*
	 * Two ways to get there: if open_factory_node() fails
	 * creating a device for @thread which is current, or when the
	 * last file reference to @thread is dropped after it has
	 * exited. We detect the first case by checking the zombie
	 * state.
	 */
	if (!(thread->state & T_ZOMBIE)) {
		if (EVL_WARN_ON(CORE, evl_current_thread() != thread))
			return;
		cleanup_current_thread();
	}

	evl_destroy_element(&thread->element);

	if (thread->state & T_USER)
		kfree_rcu(thread, element.rcu);
}

static ssize_t state_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct evl_thread *thread;
	ssize_t ret;

	thread = evl_get_element_by_dev(dev, struct evl_thread);
	ret = snprintf(buf, PAGE_SIZE, "%#x\n", thread->state);
	evl_put_element(&thread->element);

	return ret;
}
static DEVICE_ATTR_RO(state);

static ssize_t sched_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct evl_sched_class *sched_class;
	struct evl_thread *thread;
	unsigned long flags;
	ssize_t ret, _ret;

	thread = evl_get_element_by_dev(dev, struct evl_thread);

	sched_class = thread->sched_class;

	ret = snprintf(buf, PAGE_SIZE, "%d %d %d %s ",
		       evl_rq_cpu(thread->rq),
		       thread->bprio,
		       thread->cprio,
		       sched_class->name);

	if (sched_class->sched_show) {
		xnlock_get_irqsave(&nklock, flags);
		_ret = sched_class->sched_show(thread, buf + ret,
					       PAGE_SIZE - ret);
		xnlock_put_irqrestore(&nklock, flags);
		if (_ret > 0) {
			ret += _ret;
			goto out;
		}
	}

	/* overwrites trailing whitespace */
	buf[ret - 1] = '\n';
out:
	evl_put_element(&thread->element);

	return ret;
}
static DEVICE_ATTR_RO(sched);

static ssize_t stats_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	ktime_t period, exectime, account;
	struct evl_thread *thread;
	unsigned long flags;
	struct evl_rq *rq;
	ssize_t ret;
	int usage;

	thread = evl_get_element_by_dev(dev, struct evl_thread);

	xnlock_get_irqsave(&nklock, flags);

	rq = evl_thread_rq(thread);

	period = rq->last_account_switch - thread->stat.lastperiod.start;
	if (period == 0 && thread == rq->curr) {
		exectime = ktime_set(0, 1);
		account = ktime_set(0, 1);
	} else {
		exectime = thread->stat.account.total -
			thread->stat.lastperiod.total;
		account = period;
	}

	thread->stat.lastperiod.total = thread->stat.account.total;
	thread->stat.lastperiod.start = rq->last_account_switch;

	xnlock_put_irqrestore(&nklock, flags);

	if (account) {
		while (account > 0xffffffffUL) {
			exectime >>= 16;
			account >>= 16;
		}

		exectime = ns_to_ktime(ktime_to_ns(exectime) * 1000LL);
		exectime = ktime_add_ns(exectime, ktime_to_ns(account) >> 1);
		usage = ktime_divns(exectime, account);
	} else
		usage = 0;

	ret = snprintf(buf, PAGE_SIZE, "%lu %lu %lu %Lu %d\n",
		       thread->stat.isw.counter,
		       thread->stat.csw.counter,
		       thread->stat.sc.counter,
		       ktime_to_ns(thread->stat.account.total),
		       usage);

	evl_put_element(&thread->element);

	return ret;
}
static DEVICE_ATTR_RO(stats);

static ssize_t timeout_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct evl_thread *thread;
	ssize_t ret;

	thread = evl_get_element_by_dev(dev, struct evl_thread);
	ret = snprintf(buf, PAGE_SIZE, "%Lu\n",
		       ktime_to_ns(evl_get_thread_timeout(thread)));
	evl_put_element(&thread->element);

	return ret;
}
static DEVICE_ATTR_RO(timeout);

static ssize_t pid_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct evl_thread *thread;
	ssize_t ret;

	thread = evl_get_element_by_dev(dev, struct evl_thread);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", evl_get_inband_pid(thread));
	evl_put_element(&thread->element);

	return ret;
}
static DEVICE_ATTR_RO(pid);

static struct attribute *thread_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_sched.attr,
	&dev_attr_timeout.attr,
	&dev_attr_stats.attr,
	&dev_attr_pid.attr,
	NULL,
};
ATTRIBUTE_GROUPS(thread);

struct evl_factory evl_thread_factory = {
	.name	=	"thread",
	.fops	=	&thread_fops,
	.build	=	thread_factory_build,
	.dispose =	thread_factory_dispose,
	.nrdev	=	CONFIG_EVENLESS_NR_THREADS,
	.attrs	=	thread_groups,
	.flags	=	EVL_FACTORY_CLONE,
};
