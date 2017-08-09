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
 * A core availability policy implementing random core rotation
 */

#ifndef MALI_KBASE_PM_CA_RANDOM_H
#define MALI_KBASE_PM_CA_RANDOM_H

/**
 * struct kbasep_pm_ca_policy_random - Private structure for random ca policy
 *
 * This contains data that is private to the random core availability
 * policy.
 *
 * @cores_desired: Cores that the policy wants to be available
 * @cores_enabled: Cores that the policy is currently returning as available
 * @cores_used: Cores currently powered or transitioning
 * @core_change_timer: Timer for changing desired core mask
 */
struct kbasep_pm_ca_policy_random {
	u64 cores_desired;
	u64 cores_enabled;
	u64 cores_used;
	struct timer_list core_change_timer;
};

extern const struct kbase_pm_ca_policy kbase_pm_ca_random_policy_ops;

#endif /* MALI_KBASE_PM_CA_RANDOM_H */

