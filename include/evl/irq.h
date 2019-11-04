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
	 * CAUTION: Switching stages as a result of rescheduling may
	 * re-enable irqs, shut them off before returning if so.
	 */
	if ((this_rq->flags|this_rq->local_flags) & RQ_SCHED) {
		evl_schedule();
		if (!hard_irqs_disabled())
			hard_local_irq_disable();
	}
}

#endif /* !_EVL_IRQ_H */
