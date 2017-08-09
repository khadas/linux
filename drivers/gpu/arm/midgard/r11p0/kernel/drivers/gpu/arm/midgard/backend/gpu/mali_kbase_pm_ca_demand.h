/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2013 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * A core availability policy implementing demand core rotation
 */

#ifndef MALI_KBASE_PM_CA_DEMAND_H
#define MALI_KBASE_PM_CA_DEMAND_H
#include <linux/workqueue.h>
/**
 * struct kbasep_pm_ca_policy_demand - Private structure for demand ca policy
 *
 * This contains data that is private to the demand core availability
 * policy.
 *
 * @cores_desired: Cores that the policy wants to be available
 * @cores_enabled: Cores that the policy is currently returning as available
 * @cores_used: Cores currently powered or transitioning
 * @core_change_timer: Timer for changing desired core mask
 */
#define MALI_CA_TIMER 1
struct kbasep_pm_ca_policy_demand {
	u64 cores_desired;
	u64 cores_enabled;
	u64 cores_used;
#ifdef MALI_CA_TIMER
	struct timer_list core_change_timer;
#else
	struct work_struct wq_work;
#endif
};

extern const struct kbase_pm_ca_policy kbase_pm_ca_demand_policy_ops;
extern int mali_perf_set_num_pp_cores(int cores);

#endif /* MALI_KBASE_PM_CA_DEMAND_H */

