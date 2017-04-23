/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_CONTROL_H
#define _EVENLESS_CONTROL_H

#include <linux/atomic.h>
#include <linux/notifier.h>
#include <evenless/factory.h>

#define EVL_ABI_LEVEL  0

enum evl_run_states {
	EVL_STATE_DISABLED,
	EVL_STATE_RUNNING,
	EVL_STATE_STOPPED,
	EVL_STATE_TEARDOWN,
	EVL_STATE_WARMUP,
};

extern atomic_t evl_runstate;

static inline enum evl_run_states get_evl_state(void)
{
	return atomic_read(&evl_runstate);
}

static inline int evl_is_warming(void)
{
	return get_evl_state() == EVL_STATE_WARMUP;
}

static inline int evl_is_running(void)
{
	return get_evl_state() == EVL_STATE_RUNNING;
}

static inline int evl_is_enabled(void)
{
	return get_evl_state() != EVL_STATE_DISABLED;
}

static inline int evl_is_stopped(void)
{
	return get_evl_state() == EVL_STATE_STOPPED;
}

static inline void set_evl_state(enum evl_run_states state)
{
	atomic_set(&evl_runstate, state);
}

void evl_add_state_chain(struct notifier_block *nb);

void evl_remove_state_chain(struct notifier_block *nb);

#endif /* !_EVENLESS_CONTROL_H */
