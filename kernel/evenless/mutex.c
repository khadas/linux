/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evenless/timer.h>
#include <evenless/synch.h>
#include <evenless/clock.h>
#include <evenless/sched.h>
#include <evenless/thread.h>
#include <evenless/mutex.h>

void evl_init_mutex(struct evl_mutex *mutex)
{
	evl_init_syn(&mutex->wait_queue, EVL_SYN_PI,
		     &evl_mono_clock, &mutex->fastlock);
}
EXPORT_SYMBOL_GPL(evl_init_mutex);

void evl_destroy_mutex(struct evl_mutex *mutex)
{
	evl_destroy_syn(&mutex->wait_queue);
}
EXPORT_SYMBOL_GPL(evl_destroy_mutex);

static int lock_timeout(struct evl_mutex *mutex, ktime_t timeout,
			enum evl_tmode timeout_mode)
{
	int ret;

	ret = evl_try_acquire_syn(&mutex->wait_queue);
	if (ret != -EBUSY)
		return ret;

	if (timeout_nonblock(timeout))
		return -EWOULDBLOCK;

	for (;;) {
		ret = evl_acquire_syn(&mutex->wait_queue, timeout, timeout_mode);
		if (ret == 0)
			break;
		if (ret & T_BREAK)
			continue;
		ret = ret & T_TIMEO ? -ETIMEDOUT : -EIDRM;
		break;
	}

	return ret;
}

int evl_lock_timeout(struct evl_mutex *mutex, ktime_t timeout)
{
	return lock_timeout(mutex, timeout, EVL_ABS);
}
EXPORT_SYMBOL_GPL(evl_lock_timeout);

int evl_lock(struct evl_mutex *mutex)
{
	return lock_timeout(mutex, EVL_INFINITE, EVL_REL);
}
EXPORT_SYMBOL_GPL(evl_lock);

void evl_unlock(struct evl_mutex *mutex)
{
	if (evl_release_syn(&mutex->wait_queue))
		evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_unlock);
