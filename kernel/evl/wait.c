/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evl/sched.h>
#include <evl/wait.h>
#include <evl/thread.h>
#include <evl/clock.h>
#include <uapi/evl/signal.h>
#include <trace/events/evl.h>

void __evl_init_wait(struct evl_wait_queue *wq,
		struct evl_clock *clock, int flags,
		const char *name, struct lock_class_key *key)
{
	wq->flags = flags;
	wq->clock = clock;
	evl_spin_lock_init(&wq->lock);
	wq->wchan.reorder_wait = evl_reorder_wait;
	wq->wchan.follow_depend = evl_follow_wait_depend;
	INIT_LIST_HEAD(&wq->wchan.wait_list);
	lockdep_set_class_and_name(&wq->lock._lock, key, name);
}
EXPORT_SYMBOL_GPL(__evl_init_wait);

void evl_destroy_wait(struct evl_wait_queue *wq)
{
	evl_flush_wait(wq, T_RMID);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_destroy_wait);

/* wq->lock held, irqs off */
void evl_add_wait_queue(struct evl_wait_queue *wq, ktime_t timeout,
			enum evl_tmode timeout_mode)
{
	struct evl_thread *curr = evl_current();

	assert_evl_lock(&wq->lock);

	trace_evl_wait(wq);

	if ((curr->state & T_WOLI) &&
		atomic_read(&curr->inband_disable_count) > 0)
		evl_signal_thread(curr, SIGDEBUG, SIGDEBUG_MUTEX_SLEEP);

	if (!(wq->flags & EVL_WAIT_PRIO))
		list_add_tail(&curr->wait_next, &wq->wchan.wait_list);
	else
		list_add_priff(curr, &wq->wchan.wait_list, wprio, wait_next);

	evl_sleep_on(timeout, timeout_mode, wq->clock, &wq->wchan);
}
EXPORT_SYMBOL_GPL(evl_add_wait_queue);

/* wq->lock held, irqs off */
struct evl_thread *evl_wake_up(struct evl_wait_queue *wq,
			struct evl_thread *waiter)
{
	assert_evl_lock(&wq->lock);

	trace_evl_wake_up(wq);

	if (list_empty(&wq->wchan.wait_list))
		waiter = NULL;
	else {
		if (waiter == NULL)
			waiter = list_first_entry(&wq->wchan.wait_list,
						struct evl_thread, wait_next);
		list_del_init(&waiter->wait_next);
		evl_wakeup_thread(waiter, T_PEND, 0);
	}

	return waiter;
}
EXPORT_SYMBOL_GPL(evl_wake_up);

/* wq->lock held, irqs off */
void evl_flush_wait_locked(struct evl_wait_queue *wq, int reason)
{
	struct evl_thread *waiter, *tmp;

	assert_evl_lock(&wq->lock);

	trace_evl_flush_wait(wq);

	list_for_each_entry_safe(waiter, tmp, &wq->wchan.wait_list, wait_next) {
		list_del_init(&waiter->wait_next);
		evl_wakeup_thread(waiter, T_PEND, reason);
	}
}
EXPORT_SYMBOL_GPL(evl_flush_wait_locked);

void evl_flush_wait(struct evl_wait_queue *wq, int reason)
{
	unsigned long flags;

	evl_spin_lock_irqsave(&wq->lock, flags);
	evl_flush_wait_locked(wq, reason);
	evl_spin_unlock_irqrestore(&wq->lock, flags);
}
EXPORT_SYMBOL_GPL(evl_flush_wait);

static inline struct evl_wait_queue *
wchan_to_wait_queue(struct evl_wait_channel *wchan)
{
	return container_of(wchan, struct evl_wait_queue, wchan);
}

/* thread->lock held, irqs off */
int evl_reorder_wait(struct evl_thread *waiter, struct evl_thread *originator)
{
	struct evl_wait_queue *wq = wchan_to_wait_queue(waiter->wchan);

	assert_evl_lock(&waiter->lock);
	assert_evl_lock(&originator->lock);

	evl_spin_lock(&wq->lock);

	if (wq->flags & EVL_WAIT_PRIO) {
		list_del(&waiter->wait_next);
		list_add_priff(waiter, &wq->wchan.wait_list, wprio, wait_next);
	}

	evl_spin_unlock(&wq->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(evl_reorder_wait);

/* originator->lock held, irqs off */
int evl_follow_wait_depend(struct evl_wait_channel *wchan,
			struct evl_thread *originator)
{
	return 0;
}
EXPORT_SYMBOL_GPL(evl_follow_wait_depend);

int evl_wait_schedule(struct evl_wait_queue *wq)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	int ret = 0, info;

	evl_schedule();

	/*
	 * Upon return from schedule, we may or may not have been
	 * unlinked from the wait channel, depending on whether we
	 * actually resumed as a result of receiving a wakeup signal
	 * from evl_wake_up() or evl_flush_wait(). The following logic
	 * applies in order, depending on the information flags:
	 *
	 * - if T_RMID is set, evl_flush_wait() removed us from the
	 * waitqueue before the wait channel got destroyed, and
	 * therefore cannot be referred to anymore since it may be
	 * stale: -EIDRM is returned.
	 *
	 * - if neither T_TIMEO or T_BREAK are set, we got a wakeup
	 * and success is returned (zero). In addition, the caller may
	 * need to check for T_BCAST if the signal is not paired with
	 * a condition but works as a pulse instead.
	 *
	 * - otherwise, if any of T_TIMEO or T_BREAK is set:
	 *
	 *   + if we are still linked to the waitqueue, the wait was
	 * aborted prior to receiving any wakeup so we translate the
	 * information bit to the corresponding error status,
	 * i.e. -ETIMEDOUT or -EINTR respectively.
	 *
	 *  + in the rare case where we have been unlinked and we also
	 * got any of T_TIMEO|T_BREAK, then both the wakeup signal and
	 * some abort condition have occurred simultaneously on
	 * different cores, in which case we ignore the latter. In the
	 * particular case of T_BREAK caused by
	 * handle_sigwake_event(), T_KICKED will be detected on the
	 * return path from the OOB syscall, yielding -ERESTARTSYS as
	 * expected.
	 */
	info = evl_current()->info;
	if (info & T_RMID)
		return -EIDRM;

	if (info & (T_TIMEO|T_BREAK)) {
		evl_spin_lock_irqsave(&wq->lock, flags);
		if (!list_empty(&curr->wait_next)) {
			list_del_init(&curr->wait_next);
			if (info & T_TIMEO)
				ret = -ETIMEDOUT;
			else if (info & T_BREAK)
				ret = -EINTR;
		}
		evl_spin_unlock_irqrestore(&wq->lock, flags);
	} else if (IS_ENABLED(CONFIG_EVL_DEBUG_CORE)) {
		bool empty;
		evl_spin_lock_irqsave(&wq->lock, flags);
		empty = list_empty(&curr->wait_next);
		evl_spin_unlock_irqrestore(&wq->lock, flags);
		EVL_WARN_ON_ONCE(CORE, !empty);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(evl_wait_schedule);
