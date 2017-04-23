/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2017 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <evenless/sched.h>

/* hard IRQs off. */
void enter_oob_irq(void)
{
	struct evl_rq *rq = this_evl_rq();

	rq->lflags |= RQ_IRQ;
}

/* hard IRQs off. */
void exit_oob_irq(void)
{
	struct evl_rq *rq = this_evl_rq();

	rq->lflags &= ~RQ_IRQ;

	/*
	 * We are only interested in RQ_SCHED previously set by an OOB
	 * handler on the current CPU, so there is no cache coherence
	 * issue. Remote CPUs pair RQ_SCHED requests with an IPI, so
	 * we don't care about missing them here.
	 */
	if (rq->status & RQ_SCHED)
		evl_schedule();
}
