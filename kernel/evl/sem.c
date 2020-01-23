/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evl/timer.h>
#include <evl/wait.h>
#include <evl/clock.h>
#include <evl/sched.h>
#include <evl/sem.h>

static bool down_ksem(struct evl_ksem *ksem)
{
	if (ksem->value > 0) {
		--ksem->value;
		return true;
	}

	return false;
}

int evl_down_timeout(struct evl_ksem *ksem, ktime_t timeout)
{
	return evl_wait_event_timeout(&ksem->wait, timeout,
				EVL_ABS, down_ksem(ksem));
}
EXPORT_SYMBOL_GPL(evl_down_timeout);

int evl_down(struct evl_ksem *ksem)
{
	return evl_wait_event(&ksem->wait, down_ksem(ksem));
}
EXPORT_SYMBOL_GPL(evl_down);

int evl_trydown(struct evl_ksem *ksem)
{
	unsigned long flags;
	bool ret;

	evl_spin_lock_irqsave(&ksem->wait.lock, flags);
	ret = down_ksem(ksem);
	evl_spin_unlock_irqrestore(&ksem->wait.lock, flags);

	return ret ? 0 : -EAGAIN;
}
EXPORT_SYMBOL_GPL(evl_trydown);

void evl_up(struct evl_ksem *ksem)
{
	unsigned long flags;

	evl_spin_lock_irqsave(&ksem->wait.lock, flags);
	ksem->value++;
	evl_wake_up_head(&ksem->wait);
	evl_spin_unlock_irqrestore(&ksem->wait.lock, flags);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_up);
