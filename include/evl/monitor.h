/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_MONITOR_H
#define _EVL_MONITOR_H

#include <evl/factory.h>
#include <evl/thread.h>
#include <evl/sched.h>

int evl_signal_monitor_targeted(struct evl_thread *target,
				int monfd);

void __evl_commit_monitor_ceiling(void);

static inline void evl_commit_monitor_ceiling(void)
{
	struct evl_thread *curr = evl_current();

	if (curr->u_window->pp_pending != EVL_NO_HANDLE)
		__evl_commit_monitor_ceiling();
}

#endif /* !_EVL_MONITOR_H */
