/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2001, 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evenless/sched.h>
#include <evenless/wait.h>
#include <evenless/thread.h>
#include <evenless/clock.h>
#include <uapi/evenless/signal.h>
#include <trace/events/evenless.h>

void evl_init_wait(struct evl_wait_queue *wq,
		struct evl_clock *clock, int flags)
{
	wq->flags = flags;
	wq->clock = clock;
	INIT_LIST_HEAD(&wq->wait_list);
	wq->wchan.abort_wait = evl_abort_wait;
	wq->wchan.reorder_wait = evl_reorder_wait;
	raw_spin_lock_init(&wq->wchan.lock);
}
EXPORT_SYMBOL_GPL(evl_init_wait);

void evl_destroy_wait(struct evl_wait_queue *wq)
{
	evl_flush_wait(wq, T_RMID);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_destroy_wait);

/* nklock held, irqs off */
void evl_add_wait_queue(struct evl_wait_queue *wq, ktime_t timeout,
			enum evl_tmode timeout_mode)
{
	struct evl_thread *curr = evl_current();

	trace_evl_wait(wq);

	if (IS_ENABLED(CONFIG_EVENLESS_DEBUG_MUTEX_SLEEP) &&
		atomic_read(&curr->inband_disable_count) &&
		(curr->state & T_WARN))
		evl_signal_thread(curr, SIGDEBUG, SIGDEBUG_MUTEX_SLEEP);

	if (!(wq->flags & EVL_WAIT_PRIO))
		list_add_tail(&curr->wait_next, &wq->wait_list);
	else
		list_add_priff(curr, &wq->wait_list, wprio, wait_next);

	evl_sleep_on(timeout, timeout_mode, wq->clock, &wq->wchan);
}
EXPORT_SYMBOL_GPL(evl_add_wait_queue);

/* nklock held, irqs off */
struct evl_thread *evl_wake_up(struct evl_wait_queue *wq,
			struct evl_thread *waiter)
{
	trace_evl_wait_wakeup(wq);

	if (list_empty(&wq->wait_list))
		waiter = NULL;
	else {
		if (waiter == NULL)
			waiter = list_first_entry(&wq->wait_list,
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

	trace_evl_wait_flush(wq);

	list_for_each_entry_safe(waiter, tmp, &wq->wait_list, wait_next)
		evl_wakeup_thread(waiter, T_PEND, reason);
}
EXPORT_SYMBOL_GPL(evl_flush_wait_locked);

void evl_flush_wait(struct evl_wait_queue *wq, int reason)
{
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);
	evl_flush_wait_locked(wq, reason);
	xnlock_put_irqrestore(&nklock, flags);
}
EXPORT_SYMBOL_GPL(evl_flush_wait);

/* nklock held, irqs off */
void evl_abort_wait(struct evl_thread *thread,
		struct evl_wait_channel *wchan)
{
	list_del(&thread->wait_next);
}
EXPORT_SYMBOL_GPL(evl_abort_wait);

static inline struct evl_wait_queue *
wchan_to_wait_queue(struct evl_wait_channel *wchan)
{
	return container_of(wchan, struct evl_wait_queue, wchan);
}

/* nklock held, irqs off */
void evl_reorder_wait(struct evl_thread *thread)
{
	struct evl_wait_queue *wq = wchan_to_wait_queue(thread->wchan);

	if (wq->flags & EVL_WAIT_PRIO) {
		list_del(&thread->wait_next);
		list_add_priff(thread, &wq->wait_list, wprio, wait_next);
	}
}
EXPORT_SYMBOL_GPL(evl_reorder_wait);
