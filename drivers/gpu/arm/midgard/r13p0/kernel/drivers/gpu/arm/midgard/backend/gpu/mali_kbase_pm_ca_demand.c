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
 * A core availability policy implementing demand core rotation.
 *
 * This policy periodically selects a new core mask at demand. This new mask is
 * applied in two stages. It initially powers off all undesired cores, by
 * removing them from the core availability mask. Once it is confirmed that
 * these cores are powered off, then the desired cores are powered on.
 */

#include <mali_kbase.h>
#include <mali_kbase_pm.h>
#include <backend/gpu/mali_kbase_pm_internal.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/printk.h>

#ifndef MALI_CA_TIMER
static struct workqueue_struct *mali_ca_wq = NULL;
#endif
static int num_shader_cores_to_be_enabled = 0;

int mali_perf_set_num_pp_cores(int cores)
{
#ifndef MALI_CA_TIMER
	struct kbase_device *kbdev = (struct kbase_device *)priv;
	struct kbasep_pm_ca_policy_demand *data =
				&kbdev->pm.backend.ca_policy_data.demand;
#endif

    num_shader_cores_to_be_enabled = cores -1;
    printk("pp num=%d\n", num_shader_cores_to_be_enabled);
    if (num_shader_cores_to_be_enabled < 1)
        num_shader_cores_to_be_enabled = 1;

#ifndef MALI_CA_TIMER
    queue_work(mali_ca_wq, &data->wq_work);
#endif

    return 0;
}

#ifdef MALI_CA_TIMER
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
/* random32 was renamed to prandom_u32 in 3.8 */
#define prandom_u32 random32
#endif

void demand_timer_callback(unsigned long cb)
{
	struct kbase_device *kbdev = (struct kbase_device *)cb;
	struct kbasep_pm_ca_policy_demand *data =
				&kbdev->pm.backend.ca_policy_data.demand;
	unsigned long flags;

	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	/* Select new core mask, ensuring that core group 0 is not powered off
	 */
#if 0
	do {
		data->cores_desired = prandom_u32() &
				kbdev->gpu_props.props.raw_props.shader_present;
	} while (!(data->cores_desired &
		kbdev->gpu_props.props.coherency_info.group[0].core_mask));
#endif
    data->cores_desired = num_shader_cores_to_be_enabled;

	if (!(data->cores_desired &
		kbdev->gpu_props.props.coherency_info.group[0].core_mask))
            dev_info(kbdev->dev, "error, ca demand policy : new core mask=%llX\n",
				data->cores_desired);

	/* Disable any cores that are now unwanted */
	data->cores_enabled &= data->cores_desired;

	kbdev->pm.backend.ca_in_transition = true;

	/* If there are no cores to be powered off then power on desired cores
	 */
	if (!(data->cores_used & ~data->cores_desired)) {
		data->cores_enabled = data->cores_desired;
		kbdev->pm.backend.ca_in_transition = false;
	}

	data->core_change_timer.expires = jiffies + HZ;

	add_timer(&data->core_change_timer);

	kbase_pm_update_cores_state_nolock(kbdev);

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	dev_info(kbdev->dev, "ca demand policy : new core mask=%llX %llX\n",
				data->cores_desired, data->cores_enabled);
}

KBASE_EXPORT_TEST_API(demand_timer_callback);
#else
static void do_ca_scaling(struct work_struct *work)
{
    //unsigned long flags;
	struct kbase_device *kbdev = = container_of(work,
            struct kbasep_pm_ca_policy_demand, wq_work);
	struct kbasep_pm_ca_policy_random *data =
				&kbdev->pm.backend.ca_policy_data.random;
	unsigned long flags;

	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	/* Select new core mask, ensuring that core group 0 is not powered off
	 */
    data->cores_desired =  num_shader_cores_to_be_enabled;

	/* Disable any cores that are now unwanted */
	data->cores_enabled &= data->cores_desired;

	kbdev->pm.backend.ca_in_transition = true;

	/* If there are no cores to be powered off then power on desired cores
	 */
	if (!(data->cores_used & ~data->cores_desired)) {
		data->cores_enabled = data->cores_desired;
		kbdev->pm.backend.ca_in_transition = false;
	}

	kbase_pm_update_cores_state_nolock(kbdev);

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	dev_info(kbdev->dev, "Random policy : new core mask=%llX %llX\n",
				data->cores_desired, data->cores_enabled);

}
#endif

static void demand_init(struct kbase_device *kbdev)
{
	struct kbasep_pm_ca_policy_demand *data =
				&kbdev->pm.backend.ca_policy_data.demand;

	data->cores_enabled = kbdev->gpu_props.props.raw_props.shader_present;
	data->cores_desired = kbdev->gpu_props.props.raw_props.shader_present;
	data->cores_used = 0;
	kbdev->pm.backend.ca_in_transition = false;

#ifdef MALI_CA_TIMER
	init_timer(&data->core_change_timer);

	data->core_change_timer.function = demand_timer_callback;
	data->core_change_timer.data = (unsigned long) kbdev;
	data->core_change_timer.expires = jiffies + 5 * HZ;

	add_timer(&data->core_change_timer);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	mali_ca_wq = alloc_workqueue("gpu_ca_wq", WQ_HIGHPRI | WQ_UNBOUND, 0);
#else
	mali_ca_wq = create_workqueue("gpu_ca_wq");
#endif
	INIT_WORK(&data->wq_work, do_ca_scaling);
    if (mali_ca_wq == NULL) printk("Unable to create gpu scaling workqueue\n");
#endif
}

static void demand_term(struct kbase_device *kbdev)
{
	struct kbasep_pm_ca_policy_demand *data =
				&kbdev->pm.backend.ca_policy_data.demand;

#ifdef MALI_CA_TIMER
	del_timer(&data->core_change_timer);
#else
    if (mali_ca_wq) {
        flush_workqueue(mali_ca_wq);
        destroy_workqueue(mali_ca_wq);
        mali_ca_wq = NULL;
    }
#endif
}

static u64 demand_get_core_mask(struct kbase_device *kbdev)
{
	return kbdev->pm.backend.ca_policy_data.demand.cores_enabled;
}

static void demand_update_core_status(struct kbase_device *kbdev,
							u64 cores_ready,
							u64 cores_transitioning)
{
	struct kbasep_pm_ca_policy_demand *data =
				&kbdev->pm.backend.ca_policy_data.demand;

	lockdep_assert_held(&kbdev->pm.power_change_lock);

	data->cores_used = cores_ready | cores_transitioning;

	/* If in desired state then clear transition flag */
	if (data->cores_enabled == data->cores_desired)
		kbdev->pm.backend.ca_in_transition = false;

	/* If all undesired cores are now off then power on desired cores.
	 * The direct comparison against cores_enabled limits potential
	 * recursion to one level */
	if (!(data->cores_used & ~data->cores_desired) &&
				data->cores_enabled != data->cores_desired) {
		data->cores_enabled = data->cores_desired;

		kbase_pm_update_cores_state_nolock(kbdev);

		kbdev->pm.backend.ca_in_transition = false;
	}
}

/*
 * The struct kbase_pm_ca_policy structure for the demand core availability
 * policy.
 *
 * This is the static structure that defines the demand core availability power
 * policy's callback and name.
 */
const struct kbase_pm_ca_policy kbase_pm_ca_demand_policy_ops = {
	"demand",			/* name */
	demand_init,			/* init */
	demand_term,			/* term */
	demand_get_core_mask,		/* get_core_mask */
	demand_update_core_status,	/* update_core_status */
	0u,				/* flags */
	KBASE_PM_CA_POLICY_ID_DEMAND,	/* id */
};

KBASE_EXPORT_TEST_API(kbase_pm_ca_demand_policy_ops);
