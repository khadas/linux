/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2006 Jan Kiszka <jan.kiszka@web.de>.
 * Copyright (C) 2006 Dmitry Adamushko <dmitry.adamushko@gmail.com>
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_STAT_H
#define _EVENLESS_STAT_H

#include <evenless/clock.h>

struct evl_rq;

#ifdef CONFIG_EVENLESS_STATS

struct evl_account {
	ktime_t start;   /* Start of execution time accumulation */
	ktime_t total; /* Accumulated execution time */
};

/*
 * Return current date which can be passed to other accounting
 * services for immediate accounting.
 */
static inline ktime_t evl_get_timestamp(void)
{
	return evl_read_clock(&evl_mono_clock);
}

static inline ktime_t evl_get_account_total(struct evl_account *account)
{
	return account->total;
}

/*
 * Reset statistics from inside the accounted entity (e.g. after CPU
 * migration).
 */
static inline void evl_reset_account(struct evl_account *account)
{
	account->total = 0;
	account->start = evl_get_timestamp();
}

/*
 * Accumulate the time spent for the current account until now.
 * CAUTION: all changes must be committed before changing the
 * current_account reference in rq.
 */
#define evl_update_account(__rq)				\
	do {							\
		ktime_t __now = evl_get_timestamp();		\
		(__rq)->current_account->total +=		\
			__now - (__rq)->last_account_switch;	\
		(__rq)->last_account_switch = __now;		\
		smp_wmb();					\
	} while (0)

/* Obtain last account switch date of considered runqueue */
#define evl_get_last_account_switch(__rq)	((__rq)->last_account_switch)

/*
 * Update the current account reference, returning the previous one.
 */
#define evl_set_current_account(__rq, __new_account)			\
	({								\
		struct evl_account *__prev;				\
		__prev = (struct evl_account *)				\
			atomic_long_xchg(&(__rq)->current_account,	\
					(long)(__new_account));		\
		__prev;							\
	})

/*
 * Finalize an account (no need to accumulate the exectime, just mark
 * the switch date and set the new account).
 */
#define evl_close_account(__rq, __new_account)			\
	do {							\
		(__rq)->last_account_switch =			\
			evl_read_coreclk_monotonic();		\
		(__rq)->current_account = (__new_account);	\
	} while (0)

struct evl_counter {
	unsigned long counter;
};

static inline unsigned long evl_inc_counter(struct evl_counter *c)
{
	return c->counter++;
}

static inline unsigned long evl_get_counter(struct evl_counter *c)
{
	return c->counter;
}

static inline
void evl_set_counter(struct evl_counter *c, unsigned long value)
{
	c->counter = value;
}

#else /* !CONFIG_EVENLESS_STATS */

struct evl_account {
};

#define evl_get_timestamp()				({ 0; })
#define evl_get_account_total(__account)		({ 0; })
#define evl_reset_account(__account)			do { } while (0)
#define evl_update_account(__rq)			do { } while (0)
#define evl_set_current_account(__rq, __new_account)	({ (void)__rq; NULL; })
#define evl_close_account(__rq, __new_account)	do { } while (0)
#define evl_get_last_account_switch(__rq)		({ 0; })

struct evl_counter {
};

#define evl_inc_counter(__c) 	({ do { } while(0); 0; })
#define evl_get_counter(__c) 	({ 0; })
#define evl_set_counter(_c, __value)	do { } while (0)

#endif /* CONFIG_EVENLESS_STATS */

/*
 * Account the exectime of the current account until now, switch to
 * new_account, return the previous one.
 */
#define evl_switch_account(__rq, __new_account)			\
	({							\
		evl_update_account(__rq);			\
		evl_set_current_account(__rq, __new_account);	\
	})

#endif /* !_EVENLESS_STAT_H */
