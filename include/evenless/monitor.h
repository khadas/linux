/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_MONITOR_H
#define _EVENLESS_MONITOR_H

#include <evenless/factory.h>
#include <evenless/thread.h>
#include <evenless/sched.h>

int evl_signal_monitor_targeted(struct evl_thread *target,
				int monfd);

void __evl_commit_monitor_ceiling(void);

static inline void evl_commit_monitor_ceiling(void)
{
	struct evl_thread *curr = evl_current();

	if (curr->u_window->pp_pending != EVL_NO_HANDLE)
		__evl_commit_monitor_ceiling();
}

#endif /* !_EVENLESS_MONITOR_H */
