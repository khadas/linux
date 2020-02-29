/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2006, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_CLOCK_H
#define _EVL_CLOCK_H

#include <linux/types.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/cpumask.h>
#include <evl/list.h>
#include <evl/factory.h>
#include <uapi/evl/clock.h>

#define ONE_BILLION  1000000000

struct evl_rq;
struct evl_timerbase;
struct clock_event_device;
struct __kernel_timex;

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
				const struct timespec64 *ts);
		void (*program_local_shot)(struct evl_clock *clock);
		void (*program_remote_shot)(struct evl_clock *clock,
					struct evl_rq *rq);
		int (*set_gravity)(struct evl_clock *clock,
				const struct evl_clock_gravity *p);
		void (*reset_gravity)(struct evl_clock *clock);
		void (*adjust)(struct evl_clock *clock);
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

void evl_core_tick(struct clock_event_device *dummy);

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
		const struct timespec64 *ts)
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

static inline ktime_t
u_timespec_to_ktime(const struct __evl_timespec u_ts)
{
	struct timespec64 ts64 = (struct timespec64){
		.tv_sec  = u_ts.tv_sec,
		.tv_nsec = u_ts.tv_nsec,
	};

	return timespec64_to_ktime(ts64);
}

static inline struct __evl_timespec
ktime_to_u_timespec(ktime_t t)
{
	struct timespec64 ts64 = ktime_to_timespec64(t);

	return (struct __evl_timespec){
		.tv_sec  = ts64.tv_sec,
		.tv_nsec = ts64.tv_nsec,
	};
}

static inline struct timespec64
u_timespec_to_timespec64(const struct __evl_timespec u_ts)
{
	return (struct timespec64){
		.tv_sec  = u_ts.tv_sec,
		.tv_nsec = u_ts.tv_nsec,
	};
}

static inline struct __evl_timespec
timespec64_to_u_timespec(const struct timespec64 ts64)
{
	return (struct __evl_timespec){
		.tv_sec  = ts64.tv_sec,
		.tv_nsec = ts64.tv_nsec,
	};
}

static inline struct itimerspec64
u_itimerspec_to_itimerspec64(const struct __evl_itimerspec u_its)
{
	return (struct itimerspec64){
		.it_value  = u_timespec_to_timespec64(u_its.it_value),
		.it_interval  = u_timespec_to_timespec64(u_its.it_interval),
	};
}

static inline struct __evl_itimerspec
itimerspec64_to_u_itimerspec(const struct itimerspec64 its64)
{
	return (struct __evl_itimerspec){
		.it_value  = timespec64_to_u_timespec(its64.it_value),
		.it_interval  = timespec64_to_u_timespec(its64.it_interval),
	};
}

#endif /* !_EVL_CLOCK_H */
