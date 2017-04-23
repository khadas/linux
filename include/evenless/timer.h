/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_TIMER_H
#define _EVENLESS_TIMER_H

#include <linux/types.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <evenless/clock.h>
#include <evenless/stat.h>
#include <evenless/list.h>
#include <evenless/assert.h>

/*
 * Basic assumption throughout the code: ktime_t is a 64bit signed
 * scalar type holding an internal time unit, which means that:
 *
 * - we may compare two ktime_t values using basic relational operators
 * - we may check for nullness by comparing to 0 directly
 * - we must use ktime_to_ns()/ns_to_ktime() helpers for converting
 *   to/from nanoseconds.
 */
#define EVL_INFINITE   0
#define EVL_NONBLOCK   ((s64)((u64)1 << 63))

static inline bool timeout_infinite(ktime_t kt)
{
	return kt == 0;
}

static inline bool timeout_nonblock(ktime_t kt)
{
	return kt < 0;
}

static inline bool timeout_valid(ktime_t kt)
{
	return kt > 0;
}

/* Timer modes */
enum evl_tmode {
	EVL_REL,
	EVL_ABS,
};

/* Timer status */
#define EVL_TIMER_DEQUEUED  0x00000001
#define EVL_TIMER_KILLED    0x00000002
#define EVL_TIMER_PERIODIC  0x00000004
#define EVL_TIMER_FIRED     0x00000010
#define EVL_TIMER_RUNNING   0x00000020
#define EVL_TIMER_KGRAVITY  0x00000040
#define EVL_TIMER_UGRAVITY  0x00000080
#define EVL_TIMER_IGRAVITY  0	     /* most conservative */

#define EVL_TIMER_GRAVITY_MASK	(EVL_TIMER_KGRAVITY|EVL_TIMER_UGRAVITY)
#define EVL_TIMER_INIT_MASK	EVL_TIMER_GRAVITY_MASK

/* Timer priorities */
#define EVL_TIMER_LOPRIO  (-999999999)
#define EVL_TIMER_STDPRIO 0
#define EVL_TIMER_HIPRIO  999999999

struct evl_tnode {
	struct rb_node rb;
	ktime_t date;
	int prio;
};

struct evl_tqueue {
	struct rb_root root;
	struct evl_tnode *head;
};

static inline void evl_init_tqueue(struct evl_tqueue *tq)
{
	tq->root = RB_ROOT;
	tq->head = NULL;
}

#define evl_destroy_tqueue(__tq)	do { } while (0)

static inline bool evl_tqueue_is_empty(struct evl_tqueue *tq)
{
	return tq->head == NULL;
}

static inline
struct evl_tnode *evl_get_tqueue_head(struct evl_tqueue *tq)
{
	return tq->head;
}

static inline
struct evl_tnode *evl_get_tqueue_next(struct evl_tqueue *tq,
				      struct evl_tnode *node)
{
	struct rb_node *_node = rb_next(&node->rb);
	return _node ? container_of(_node, struct evl_tnode, rb) : NULL;
}

static inline
void evl_remove_tnode(struct evl_tqueue *tq, struct evl_tnode *node)
{
	if (node == tq->head)
		tq->head = evl_get_tqueue_next(tq, node);

	rb_erase(&node->rb, &tq->root);
}

#define for_each_evl_tnode(__node, __tq)			\
	for ((__node) = evl_get_tqueue_head(__tq); (__node);	\
	     (__node) = evl_get_tqueue_next(__tq, __node))

void evl_insert_tnode(struct evl_tqueue *tq,
		      struct evl_tnode *node);

struct evl_rq;

struct evl_timerbase {
	hard_spinlock_t lock;
	struct evl_tqueue q;
};

static inline struct evl_timerbase *
evl_percpu_timers(struct evl_clock *clock, int cpu)
{
	return per_cpu_ptr(clock->timerdata, cpu);
}

static inline struct evl_timerbase *
evl_this_cpu_timers(struct evl_clock *clock)
{
	return raw_cpu_ptr(clock->timerdata);
}

struct evl_timer {
	struct evl_clock *clock;
	/* Link in timers list. */
	struct evl_tnode node;
	struct list_head adjlink;
	/* Timer status. */
	int status;
	/* Periodic interval (clock ticks, 0 == one shot). */
	ktime_t interval;
	/* First tick date in periodic mode. */
	ktime_t start_date;
	/* Position of next periodic release point. */
	u64 pexpect_ticks;
	/* Count of timer ticks in periodic mode. */
	u64 periodic_ticks;
#ifdef CONFIG_SMP
	/* Runqueue the timer is affine to. */
	struct evl_rq *rq;
#endif
	/* Per-cpu base. */
	struct evl_timerbase *base;
	/* Timeout handler. */
	void (*handler)(struct evl_timer *timer);
#ifdef CONFIG_EVENLESS_STATS
	/* Timer name to be displayed. */
	const char *name;
	/* Timer holder in timebase. */
	struct list_head next_stat;
	/* Number of timer schedules. */
	struct evl_counter scheduled;
	/* Number of timer events. */
	struct evl_counter fired;
#endif /* CONFIG_EVENLESS_STATS */
};

#define evl_tdate(__timer)	((__timer)->node.date)

void evl_start_timer(struct evl_timer *timer,
		     ktime_t value,
		     ktime_t interval);

void __evl_stop_timer(struct evl_timer *timer);

static inline int evl_timer_is_running(struct evl_timer *timer)
{
	return (timer->status & EVL_TIMER_RUNNING) != 0;
}

static inline int evl_timer_is_periodic(struct evl_timer *timer)
{
	return (timer->status & EVL_TIMER_PERIODIC) != 0;
}

static inline void evl_stop_timer(struct evl_timer *timer)
{
	if (evl_timer_is_running(timer))
		__evl_stop_timer(timer);
}

void evl_destroy_timer(struct evl_timer *timer);

static inline ktime_t evl_abs_timeout(struct evl_timer *timer,
				      ktime_t delta)
{
	return ktime_add(evl_read_clock(timer->clock), delta);
}

#ifdef CONFIG_SMP
static inline struct evl_rq *evl_get_timer_rq(struct evl_timer *timer)
{
	return timer->rq;
}
#else /* !CONFIG_SMP */
#define evl_get_timer_rq(t)	this_evl_rq()
#endif /* !CONFIG_SMP */

/*
 * timer base locked so that ->clock does not change under our
 * feet.
 */
static inline unsigned long evl_get_timer_gravity(struct evl_timer *timer)
{
	struct evl_clock *clock = timer->clock;

	if (timer->status & EVL_TIMER_KGRAVITY)
		return clock->gravity.kernel;

	if (timer->status & EVL_TIMER_UGRAVITY)
		return clock->gravity.user;

	return clock->gravity.irq;
}

/* timer base locked. */
static inline void evl_update_timer_date(struct evl_timer *timer)
{
	evl_tdate(timer) = ktime_add_ns(timer->start_date,
					(timer->periodic_ticks * ktime_to_ns(timer->interval))
					- evl_get_timer_gravity(timer));
}

static inline
ktime_t evl_get_timer_next_date(struct evl_timer *timer)
{
	return ktime_add_ns(timer->start_date,
			    timer->pexpect_ticks * ktime_to_ns(timer->interval));
}

static inline
void evl_set_timer_priority(struct evl_timer *timer, int prio)
{
	timer->node.prio = prio;
}

void __evl_init_timer(struct evl_timer *timer,
		      struct evl_clock *clock,
		      void (*handler)(struct evl_timer *timer),
		      struct evl_rq *rq,
		      int flags);

void evl_set_timer_gravity(struct evl_timer *timer,
			   int gravity);

#ifdef CONFIG_EVENLESS_STATS

#define evl_init_timer(__timer, __clock, __handler, __rq, __flags)	\
	do {								\
		__evl_init_timer(__timer, __clock, __handler, __rq, __flags); \
		evl_set_timer_name(__timer, #__handler);		\
	} while (0)

static inline
void evl_reset_timer_stats(struct evl_timer *timer)
{
	evl_set_counter(&timer->scheduled, 0);
	evl_set_counter(&timer->fired, 0);
}

static inline
void evl_account_timer_scheduled(struct evl_timer *timer)
{
	evl_inc_counter(&timer->scheduled);
}

static inline
void evl_account_timer_fired(struct evl_timer *timer)
{
	evl_inc_counter(&timer->fired);
}

static inline
void evl_set_timer_name(struct evl_timer *timer, const char *name)
{
	timer->name = name;
}

#else /* !CONFIG_EVENLESS_STATS */

#define evl_init_timer	__evl_init_timer

static inline
void evl_reset_timer_stats(struct evl_timer *timer) { }

static inline
void evl_account_timer_scheduled(struct evl_timer *timer) { }

static inline
void evl_account_timer_fired(struct evl_timer *timer) { }

static inline
void evl_set_timer_name(struct evl_timer *timer, const char *name) { }

#endif /* !CONFIG_EVENLESS_STATS */

#define evl_init_core_timer(__timer, __handler)				\
	evl_init_timer(__timer, &evl_mono_clock, __handler, NULL,	\
		       EVL_TIMER_IGRAVITY)

#define evl_init_timer_on_cpu(__timer, __cpu, __handler)		\
	do {								\
		struct evl_rq *__rq = evl_cpu_rq(__cpu);		\
		evl_init_timer(__timer, &evl_mono_clock, __handler,	\
			       __rq, EVL_TIMER_IGRAVITY);		\
	} while (0)

bool evl_timer_deactivate(struct evl_timer *timer);

/* timer base locked. */
static inline ktime_t evl_get_timer_expiry(struct evl_timer *timer)
{
	/* Ideal expiry date without anticipation (no gravity) */
	return ktime_add(evl_tdate(timer),
			 evl_get_timer_gravity(timer));
}

static inline
void evl_move_timer_backward(struct evl_timer *timer, ktime_t delta)
{
	evl_tdate(timer) = ktime_sub(evl_tdate(timer), delta);
}

/* no lock required. */
ktime_t evl_get_timer_date(struct evl_timer *timer);

/* no lock required. */
ktime_t __evl_get_timer_delta(struct evl_timer *timer);

ktime_t xntimer_get_interval(struct evl_timer *timer);

/* no lock required. */
static inline ktime_t evl_get_timer_delta(struct evl_timer *timer)
{
	if (!evl_timer_is_running(timer))
		return EVL_INFINITE;

	return __evl_get_timer_delta(timer);
}

/* no lock required. */
static inline
ktime_t __evl_get_stopped_timer_delta(struct evl_timer *timer)
{
	return __evl_get_timer_delta(timer);
}

static inline
ktime_t evl_get_stopped_timer_delta(struct evl_timer *timer)
{
	ktime_t t = __evl_get_stopped_timer_delta(timer);

	if (ktime_to_ns(t) <= 1)
		return EVL_INFINITE;

	return t;
}

static inline void evl_dequeue_timer(struct evl_timer *timer,
				     struct evl_tqueue *tq)
{
	evl_remove_tnode(tq, &timer->node);
	timer->status |= EVL_TIMER_DEQUEUED;
}

/* timer base locked. */
static inline
void evl_enqueue_timer(struct evl_timer *timer,
		       struct evl_tqueue *tq)
{
	evl_insert_tnode(tq, &timer->node);
	timer->status &= ~EVL_TIMER_DEQUEUED;
	evl_account_timer_scheduled(timer);
}

void evl_enqueue_timer(struct evl_timer *timer,
		       struct evl_tqueue *tq);

unsigned long evl_get_timer_overruns(struct evl_timer *timer);

void evl_bolt_timer(struct evl_timer *timer,
		    struct evl_clock *clock,
		    struct evl_rq *rq);

#ifdef CONFIG_SMP

void __evl_set_timer_rq(struct evl_timer *timer,
			struct evl_clock *clock,
			struct evl_rq *rq);

static inline void evl_set_timer_rq(struct evl_timer *timer,
				    struct evl_rq *rq)
{
	if (rq != timer->rq)
		__evl_set_timer_rq(timer, timer->clock, rq);
}

static inline void evl_prepare_timer_wait(struct evl_timer *timer,
					  struct evl_clock *clock,
					  struct evl_rq *rq)
{
	/* We may change the reference clock before waiting. */
	if (rq != timer->rq || clock != timer->clock)
		__evl_set_timer_rq(timer, clock, rq);
}

static inline bool evl_timer_on_rq(struct evl_timer *timer,
				   struct evl_rq *rq)
{
	return timer->rq == rq;
}

#else /* ! CONFIG_SMP */

static inline void evl_set_timer_rq(struct evl_timer *timer,
				    struct evl_rq *rq)
{ }

static inline void evl_prepare_timer_wait(struct evl_timer *timer,
					  struct evl_clock *clock,
					  struct evl_rq *rq)
{
	if (clock != timer->clock)
		evl_bolt_timer(timer, clock, rq);
}

static inline bool evl_timer_on_rq(struct evl_timer *timer,
				   struct evl_rq *rq)
{
	return true;
}

#endif /* CONFIG_SMP */

#endif /* !_EVENLESS_TIMER_H */
