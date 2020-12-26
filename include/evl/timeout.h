/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_TIMEOUT_H
#define _EVL_TIMEOUT_H

#include <linux/ktime.h>

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

/*
 * So that readers do not need to pull evl/clock.h for defining timed
 * object initializers which only refer to the built-in clock
 * addresses in the common case.
 */
extern struct evl_clock evl_mono_clock,
	evl_realtime_clock;

#endif /* !_EVL_TIMEOUT_H */
