/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2017 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_IRQ_H
#define _EVL_IRQ_H

#include <evl/sched.h>

/* hard IRQs off. */
static inline void evl_enter_irq(void)
{
	struct evl_rq *rq = this_evl_rq();

	rq->lflags |= RQ_IRQ;
}

/* hard IRQs off. */
static inline void evl_leave_irq(void)
{
	struct evl_rq *rq = this_evl_rq();

	rq->lflags &= ~RQ_IRQ;

	/*
	 * We are only interested in RQ_SCHED previously set by an OOB
	 * handler on the current CPU, so there is no cache coherence
	 * issue. Remote CPUs pair RQ_SCHED requests with an IPI, so
	 * we don't care about missing them here.
	 *
	 * CAUTION: Switching stages as a result of rescheduling may
	 * re-enable irqs, shut them off before returning if so.
	 */
	if (rq->status & RQ_SCHED) {
		evl_schedule();
		if (!hard_irqs_disabled())
			hard_local_irq_disable();
	}
}

#endif /* !_EVL_IRQ_H */
