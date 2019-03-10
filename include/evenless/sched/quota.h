/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2013, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SCHED_QUOTA_H
#define _EVL_SCHED_QUOTA_H

#ifndef _EVENLESS_SCHED_H
#error "please don't include evenless/sched/quota.h directly"
#endif

#ifdef CONFIG_EVL_SCHED_QUOTA

#define EVL_QUOTA_MIN_PRIO	1
#define EVL_QUOTA_MAX_PRIO	255
#define EVL_QUOTA_NR_PRIO	\
	(EVL_QUOTA_MAX_PRIO - EVL_QUOTA_MIN_PRIO + 1)

extern struct evl_sched_class evl_sched_quota;

struct evl_quota_group {
	struct evl_rq *rq;
	ktime_t quota;
	ktime_t quota_peak;
	ktime_t run_start;
	ktime_t run_budget;
	ktime_t run_credit;
	struct list_head members;
	struct list_head expired;
	struct list_head next;
	int nr_active;
	int nr_threads;
	int tgid;
	int quota_percent;
	int quota_peak_percent;
};

struct evl_sched_quota {
	ktime_t period;
	struct evl_timer refill_timer;
	struct evl_timer limit_timer;
	struct list_head groups;
};

static inline int evl_quota_init_thread(struct evl_thread *thread)
{
	thread->quota = NULL;
	INIT_LIST_HEAD(&thread->quota_expired);

	return 0;
}

int evl_quota_create_group(struct evl_quota_group *tg,
			struct evl_rq *rq,
			int *quota_sum_r);

int evl_quota_destroy_group(struct evl_quota_group *tg,
			int force,
			int *quota_sum_r);

void evl_quota_set_limit(struct evl_quota_group *tg,
			int quota_percent, int quota_peak_percent,
			int *quota_sum_r);

struct evl_quota_group *
evl_quota_find_group(struct evl_rq *rq, int tgid);

int evl_quota_sum_all(struct evl_rq *rq);

void evl_set_quota_period(ktime_t period);

ktime_t evl_get_quota_period(void);

#endif /* !CONFIG_EVL_SCHED_QUOTA */

#endif /* !_EVL_SCHED_QUOTA_H */
