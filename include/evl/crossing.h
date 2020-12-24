/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_CROSSING_H
#define _EVL_CROSSING_H

#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/irq_work.h>

/*
 * This is a simple synchronization mechanism allowing an in-band
 * caller to pass a point in the code making sure that no out-of-band
 * operations which might traverse the same crossing are in
 * flight. The protection works once, after which the crossing must be
 * reinitialized.
 *
 * Out-of-band callers delimit the danger zone by down-ing and up-ing
 * the barrier at the crossing, the in-band code should ask for
 * passing the crossing.
 *
 * CAUTION: the caller must guarantee that evl_down_crossing() cannot
 * be invoked _after_ evl_pass_crossing() is entered for a given
 * crossing.
 */

struct evl_crossing {
	atomic_t oob_refs;
	struct completion oob_done;
	struct irq_work oob_work;
};

static inline void evl_open_crossing(struct irq_work *work)
{
	struct evl_crossing *c = container_of(work, struct evl_crossing, oob_work);
	complete(&c->oob_done);
}

static inline void evl_init_crossing(struct evl_crossing *c)
{
	atomic_set(&c->oob_refs, 1);
	init_completion(&c->oob_done);
	init_irq_work(&c->oob_work, evl_open_crossing);
}

static inline void evl_reinit_crossing(struct evl_crossing *c)
{
	atomic_set(&c->oob_refs, 1);
	reinit_completion(&c->oob_done);
}

static inline void evl_down_crossing(struct evl_crossing *c)
{
	atomic_inc(&c->oob_refs);
}

static inline void evl_up_crossing(struct evl_crossing *c)
{
	/* CAUTION: See word of caution in the initial comment. */

	if (atomic_dec_return(&c->oob_refs) == 0)
		irq_work_queue(&c->oob_work);
}

static inline void evl_pass_crossing(struct evl_crossing *c)
{
	if (atomic_dec_return(&c->oob_refs) > 0)
		wait_for_completion(&c->oob_done);
}

#endif /* !_EVL_CROSSING_H */
