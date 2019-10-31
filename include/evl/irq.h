/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2017 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_IRQ_H
#define _EVL_IRQ_H

#include <evl/sched.h>

/* hard irqs off. */
static inline void evl_enter_irq(void)
{
	struct evl_rq *rq = this_evl_rq();

	rq->local_flags |= RQ_IRQ;
}

/* hard irqs off. */
static inline void evl_exit_irq(void)
{
	struct evl_rq *this_rq = this_evl_rq();

	this_rq->local_flags &= ~RQ_IRQ;

	/*
	 * We are only interested in RQ_SCHED previously set by an OOB
	 * handler on the current CPU, so there is no cache coherence
	 * issue. Remote CPUs pair RQ_SCHED requests with an IPI, so
	 * we don't care about missing them here.
	 *
	 * CAUTION: Switching stages as a result of rescheduling may
	 * re-enable irqs, shut them off before returning if so.
	 */
	if (evl_need_resched(this_rq)) {
		evl_schedule();
		if (!hard_irqs_disabled())
			hard_local_irq_disable();
	}
}

#endif /* !_EVL_IRQ_H */
