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

void evl_init_wait(struct evl_wait_queue *wq,
		struct evl_clock *clock, int flags)
{
	no_ugly_lock();
	wq->flags = flags;
	wq->clock = clock;
	evl_spin_lock_init(&wq->lock);
	wq->wchan.abort_wait = evl_abort_wait;
	wq->wchan.reorder_wait = evl_reorder_wait;
	INIT_LIST_HEAD(&wq->wchan.wait_list);
}
EXPORT_SYMBOL_GPL(evl_init_wait);

void evl_destroy_wait(struct evl_wait_queue *wq)
{
	no_ugly_lock();
	evl_flush_wait(wq, T_RMID);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_destroy_wait);

/* nklock held, irqs off */
void evl_add_wait_queue(struct evl_wait_queue *wq, ktime_t timeout,
			enum evl_tmode timeout_mode)
{
	struct evl_thread *curr = evl_current();

	requires_ugly_lock();

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

/* nklock held, irqs off */
struct evl_thread *evl_wake_up(struct evl_wait_queue *wq,
			struct evl_thread *waiter)
{
	requires_ugly_lock();

	trace_evl_wake_up(wq);

	if (list_empty(&wq->wchan.wait_list))
		waiter = NULL;
	else {
		if (waiter == NULL)
			waiter = list_first_entry(&wq->wchan.wait_list,
						struct evl_thread, wait_next);
		evl_wakeup_thread(waiter, T_PEND, 0);
	}

	return waiter;
}
EXPORT_SYMBOL_GPL(evl_wake_up);

/* nklock held, irqs off */
void evl_flush_wait_locked(struct evl_wait_queue *wq, int reason)
{
	struct evl_thread *waiter, *tmp;

	requires_ugly_lock();

	trace_evl_flush_wait(wq);

	list_for_each_entry_safe(waiter, tmp, &wq->wchan.wait_list, wait_next)
		evl_wakeup_thread(waiter, T_PEND, reason);
}
EXPORT_SYMBOL_GPL(evl_flush_wait_locked);

void evl_flush_wait(struct evl_wait_queue *wq, int reason)
{
	unsigned long flags;

	no_ugly_lock();
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

/* nklock held, irqs off */
void evl_abort_wait(struct evl_thread *thread,
		struct evl_wait_channel *wchan)
{
	requires_ugly_lock();
	list_del(&thread->wait_next);
}

/* nklock held, irqs off */
void evl_reorder_wait(struct evl_thread *thread)
{
	struct evl_wait_queue *wq = wchan_to_wait_queue(thread->wchan);

	requires_ugly_lock();

	if (wq->flags & EVL_WAIT_PRIO) {
		list_del(&thread->wait_next);
		list_add_priff(thread, &wq->wchan.wait_list, wprio, wait_next);
	}
}
EXPORT_SYMBOL_GPL(evl_reorder_wait);
