/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2006, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_CLOCK_H
#define _EVENLESS_CLOCK_H

#include <linux/types.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/cpumask.h>
#include <evenless/list.h>
#include <evenless/factory.h>
#include <uapi/evenless/clock.h>

#define ONE_BILLION  1000000000

struct evl_rq;
struct evl_timerbase;

struct evl_clock_gravity {
	ktime_t irq;
	ktime_t kernel;
	ktime_t user;
};

struct evl_clock {
	/* (ns) */
	ktime_t resolution;
	/* Anticipation values for timer shots. */
	struct evl_clock_gravity gravity;
	/* Clock name. */
	const char *name;
	struct {
		ktime_t (*read)(struct evl_clock *clock);
		u64 (*read_cycles)(struct evl_clock *clock);
		int (*set_time)(struct evl_clock *clock,
				const struct timespec *ts);
		void (*program_local_shot)(struct evl_clock *clock);
		void (*program_remote_shot)(struct evl_clock *clock,
					struct evl_rq *rq);
		int (*set_gravity)(struct evl_clock *clock,
				const struct evl_clock_gravity *p);
		void (*reset_gravity)(struct evl_clock *clock);
		void (*adjust)(struct evl_clock *clock);
		int (*adjust_time)(struct evl_clock *clock,
				struct timex *tx);
	} ops;
	struct evl_timerbase *timerdata;
	struct evl_clock *master;
	/* Offset from master clock. */
	ktime_t offset;
#ifdef CONFIG_SMP
	/* CPU affinity of clock beat. */
	struct cpumask affinity;
#endif
	struct list_head next;
	struct evl_element element;
	void (*dispose)(struct evl_clock *clock);
} ____cacheline_aligned;

extern struct evl_clock evl_mono_clock;

extern struct evl_clock evl_realtime_clock;

int evl_init_clock(struct evl_clock *clock,
		const struct cpumask *affinity);

int evl_init_slave_clock(struct evl_clock *clock,
			struct evl_clock *master);

void evl_announce_tick(struct evl_clock *clock);

void evl_adjust_timers(struct evl_clock *clock,
		ktime_t delta);

void evl_stop_timers(struct evl_clock *clock);

static inline u64 evl_read_clock_cycles(struct evl_clock *clock)
{
	return clock->ops.read_cycles(clock);
}

static ktime_t evl_ktime_monotonic(void)
{
	return ktime_get_mono_fast_ns();
}

static inline ktime_t evl_read_clock(struct evl_clock *clock)
{
	/*
	 * In many occasions on the fast path, evl_read_clock() is
	 * explicitly called with &evl_mono_clock which resolves as
	 * a constant. Skip the clock trampoline handler, branching
	 * immediately to the final code for such clock.
	 */
	if (clock == &evl_mono_clock)
		return evl_ktime_monotonic();

	return clock->ops.read(clock);
}

static inline int
evl_set_clock_time(struct evl_clock *clock,
		const struct timespec *ts)
{
	if (clock->ops.set_time)
		return clock->ops.set_time(clock, ts);

	return -EOPNOTSUPP;
}

static inline
ktime_t evl_get_clock_resolution(struct evl_clock *clock)
{
	return clock->resolution;
}

static inline
void evl_set_clock_resolution(struct evl_clock *clock,
			ktime_t resolution)
{
	clock->resolution = resolution;
}

static inline
int evl_set_clock_gravity(struct evl_clock *clock,
			const struct evl_clock_gravity *gravity)
{
	if (clock->ops.set_gravity)
		return clock->ops.set_gravity(clock, gravity);

	return -EOPNOTSUPP;
}

static inline void evl_reset_clock_gravity(struct evl_clock *clock)
{
	if (clock->ops.reset_gravity)
		clock->ops.reset_gravity(clock);
}

#define evl_get_clock_gravity(__clock, __type)  ((__clock)->gravity.__type)

static inline
int evl_clock_adjust_time(struct evl_clock *clock, struct timex *tx)
{
	if (clock->ops.adjust_time)
		return clock->ops.adjust_time(clock, tx);

	return -EOPNOTSUPP;
}

int evl_clock_init(void);

void evl_clock_cleanup(void);

int evl_register_clock(struct evl_clock *clock,
		const struct cpumask *affinity);

void evl_unregister_clock(struct evl_clock *clock);

struct evl_clock *evl_get_clock_by_fd(int efd);

static inline void evl_put_clock(struct evl_clock *clock)
{
	evl_put_element(&clock->element);
}

#endif /* !_EVENLESS_CLOCK_H */
