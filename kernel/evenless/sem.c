/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evenless/timer.h>
#include <evenless/wait.h>
#include <evenless/clock.h>
#include <evenless/sched.h>
#include <evenless/sem.h>

void evl_init_sem(struct evl_sem *sem, unsigned int value)
{
	sem->value = value;
	evl_init_wait(&sem->wait_queue, &evl_mono_clock, EVL_WAIT_PRIO);
}
EXPORT_SYMBOL_GPL(evl_init_sem);

void evl_destroy_sem(struct evl_sem *sem)
{
	evl_destroy_wait(&sem->wait_queue);
}
EXPORT_SYMBOL_GPL(evl_destroy_sem);

static bool down_sem(struct evl_sem *sem)
{
	if (sem->value > 0) {
		--sem->value;
		return true;
	}

	return false;
}

int evl_down_timeout(struct evl_sem *sem, ktime_t timeout)
{
	return evl_wait_event_timeout(&sem->wait_queue, timeout,
				EVL_ABS, down_sem(sem));
}
EXPORT_SYMBOL_GPL(evl_down_timeout);

int evl_down(struct evl_sem *sem)
{
	return evl_wait_event_timeout(&sem->wait_queue, EVL_INFINITE,
				EVL_REL, down_sem(sem));
}
EXPORT_SYMBOL_GPL(evl_down);

int evl_trydown(struct evl_sem *sem)
{
	unsigned long flags;
	bool ret;

	xnlock_get_irqsave(&nklock, flags);
	ret = down_sem(sem);
	xnlock_put_irqrestore(&nklock, flags);

	return ret ? 0 : -EAGAIN;
}
EXPORT_SYMBOL_GPL(evl_trydown);

void evl_up(struct evl_sem *sem)
{
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);

	if (!evl_wake_up_head(&sem->wait_queue))
		sem->value++;

	xnlock_put_irqrestore(&nklock, flags);

	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_up);
