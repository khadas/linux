/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evl/work.h>

static void do_wq_work(struct work_struct *wq_work)
{
	struct evl_work *work;

	work = container_of(wq_work, struct evl_work, wq_work);
	work->handler(work);
}

static void do_wq_work_sync(struct work_struct *wq_work)
{
	struct evl_sync_work *sync_work;

	sync_work = container_of(wq_work, struct evl_sync_work, work.wq_work);
	sync_work->result = sync_work->work.handler(sync_work);
	evl_raise_flag(&sync_work->done);
}

static void do_irq_work(struct irq_work *irq_work)
{
	struct evl_work *work;

	work = container_of(irq_work, struct evl_work, irq_work);
	queue_work(work->wq, &work->wq_work);
}

void evl_init_work(struct evl_work *work,
		void (*handler)(struct evl_work *work))
{
	init_irq_work(&work->irq_work, do_irq_work);
	INIT_WORK(&work->wq_work, do_wq_work);
	work->handler = (typeof(work->handler))handler;
}
EXPORT_SYMBOL_GPL(evl_init_work);

void evl_init_sync_work(struct evl_sync_work *sync_work,
			int (*handler)(struct evl_sync_work *sync_work))
{
	struct evl_work *work = &sync_work->work;

	init_irq_work(&work->irq_work, do_irq_work);
	INIT_WORK(&work->wq_work, do_wq_work_sync);
	work->handler = (typeof(work->handler))handler;
	evl_init_flag(&sync_work->done);
}
EXPORT_SYMBOL_GPL(evl_init_sync_work);

void evl_call_inband_from(struct evl_work *work,
			struct workqueue_struct *wq)
{
	work->wq = wq;
	irq_work_queue(&work->irq_work);
}
EXPORT_SYMBOL_GPL(evl_call_inband_from);

int evl_call_inband_sync_from(struct evl_sync_work *sync_work,
			struct workqueue_struct *wq)
{
	struct evl_work *work = &sync_work->work;

	sync_work->result = -EINVAL;

	if (unlikely(running_inband())) {
		evl_call_inband_from(work, wq);
		flush_work(&work->wq_work);
		return sync_work->result;
	}

	evl_call_inband_from(work, wq);

	return evl_wait_flag(&sync_work->done) ?: sync_work->result;
}
EXPORT_SYMBOL_GPL(evl_call_inband_sync_from);
