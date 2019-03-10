/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2016 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_TICK_H
#define _EVENLESS_TICK_H

#include <linux/types.h>
#include <linux/ktime.h>
#include <evenless/clock.h>
#include <uapi/evenless/types.h>

struct evl_rq;

static inline void evl_program_local_tick(struct evl_clock *clock)
{
	struct evl_clock *master = clock->master;

	if (master->ops.program_local_shot)
		master->ops.program_local_shot(master);
}

static inline void evl_program_remote_tick(struct evl_clock *clock,
					struct evl_rq *rq)
{
#ifdef CONFIG_SMP
	struct evl_clock *master = clock->master;

	if (master->ops.program_remote_shot)
		master->ops.program_remote_shot(master, rq);
#endif
}

int evl_enable_tick(void);

void evl_disable_tick(void);

void evl_notify_proxy_tick(struct evl_rq *this_rq);

void evl_program_proxy_tick(struct evl_clock *clock);

void evl_send_timer_ipi(struct evl_clock *clock,
			struct evl_rq *rq);

#endif /* !_EVENLESS_TICK_H */
