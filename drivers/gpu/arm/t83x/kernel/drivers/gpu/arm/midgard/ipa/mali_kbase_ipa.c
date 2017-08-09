/*
 *
 * (C) COPYRIGHT 2016 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */


#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/devfreq_cooling.h>
#include <linux/of.h>
#include "mali_kbase.h"

int kbase_ipa_model_ops_register(struct kbase_device *kbdev,
			     struct kbase_ipa_model_ops *new_model_ops)
{
	struct kbase_ipa_model *new_model;

	new_model = kzalloc(sizeof(struct kbase_ipa_model), GFP_KERNEL);
	if (!new_model)
		return -ENOMEM;
	INIT_LIST_HEAD(&new_model->link);
	new_model->kbdev = kbdev;
	new_model->ops = new_model_ops;

	list_add(&new_model->link, &kbdev->ipa_power_models);

	return 0;
}

static int kbase_ipa_internal_models_append_list(struct kbase_device *kbdev)
{
	int err;

	/* Always have the generic IPA */
	err = kbase_ipa_model_ops_register(kbdev, &kbase_generic_ipa_model_ops);

	if (err)
		return err;

	return err;
}

struct kbase_ipa_model *kbase_ipa_get_model(struct kbase_device *kbdev,
					    const char *name)
{
	/* Search registered power models first */
	struct list_head *it;

	list_for_each(it, &kbdev->ipa_power_models) {
		struct kbase_ipa_model *model =
				list_entry(it,
					   struct kbase_ipa_model,
					   link);
		if (strcmp(model->ops->name, name) == 0)
			return model;
	}

	return NULL;
}

void kbase_ipa_model_use_fallback_locked(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);
	if (kbdev->ipa_current_model != kbdev->ipa_fallback_model)
		kbdev->ipa_current_model = kbdev->ipa_fallback_model;
}


void kbase_ipa_model_use_configured_locked(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);
	if (kbdev->ipa_current_model != kbdev->ipa_configured_model)
		kbdev->ipa_current_model = kbdev->ipa_configured_model;
}

const char *kbase_ipa_model_name_from_id(u32 gpu_id)
{
	const u32 prod_id = (gpu_id & GPU_ID_VERSION_PRODUCT_ID) >>
			GPU_ID_VERSION_PRODUCT_ID_SHIFT;

	if (GPU_ID_IS_NEW_FORMAT(prod_id)) {
		switch (GPU_ID2_MODEL_MATCH_VALUE(prod_id)) {
		case GPU_ID2_PRODUCT_TMIX:
			return "generic_ipa_model";
		default:
			return "generic_ipa_model";
		}
	}

	return "generic_ipa_model";
}

int kbase_ipa_model_init(struct kbase_device *kbdev)
{

	const char *model_name;
	struct kbase_ipa_model *default_model = NULL;
	struct kbase_ipa_model *model = NULL;
	int err;

	/* Add default ones to the list */
	err = kbase_ipa_internal_models_append_list(kbdev);

	default_model = kbase_ipa_get_model(kbdev, "generic_ipa_model");
	/* The generic_ipa_model 'MUST' always be present.*/
	if (!default_model) {
		err = -EINVAL;
		goto end;
	}

	err = default_model->ops->init(default_model);
	if (err) {
		dev_err(kbdev->dev,
			"failed to init default generic power model err %d\n",
			err);
		goto end;
	}
	kbdev->ipa_fallback_model = default_model;
	err = of_property_read_string(kbdev->dev->of_node,
				      "ipa-model",
				      &model_name);
	if (err) {
		/* Attempt to load a match from GPU-ID */
		u32 gpu_id;

		gpu_id = kbdev->gpu_props.props.raw_props.gpu_id;
		model_name = kbase_ipa_model_name_from_id(gpu_id);
	}

	if (strcmp("generic_ipa_model", model_name) != 0) {

		model = kbase_ipa_get_model(kbdev, model_name);
		kbdev->ipa_configured_model = model;

		if (model) {
			kbdev->ipa_current_model = model;
			err = model->ops->init(model);
			goto end;
		}
		dev_err(kbdev->dev,
			"Failed to find IPA model matching: %s\n",
			model_name);
		err = -EINVAL;
	} else {

		kbdev->ipa_current_model = default_model;
		kbdev->ipa_configured_model = default_model;
		dev_dbg(kbdev->dev, "Using generic-ipa-model by default\n");
		err = 0;
	}
end:
	if (err)
		kbase_ipa_model_term(kbdev);

	return err;
}

void kbase_ipa_model_term(struct kbase_device *kbdev)
{
	/* Clean up the models */
	struct kbase_ipa_model *model = kbdev->ipa_configured_model;

	if (model) {
		if (model->ops->term)
			model->ops->term(model);
		/* Always terminate the default model,
		 * unless it was the configured model */
		if (model != kbdev->ipa_fallback_model)
			kbdev->ipa_fallback_model->ops->term(model);
	}
	/* Clean up the list */
	if (!list_empty(&kbdev->ipa_power_models)) {
		struct kbase_ipa_model *model_p, *model_n;

		list_for_each_entry_safe(model_p, model_n, &kbdev->ipa_power_models, link) {
			list_del(&model_p->link);
			kfree(model_p);
		}
	}

}

/**
 * kbase_scale_power() - Scale a model-specific power coefficient to an OPP
 * @c:		Model coefficient, in pW/(Hz V^2). Should be in range
 *		0 < c < 2^16 to prevent overflow.
 * @freq:	Frequency, in Hz. Range: 2^23 < freq < 2^30 (~8MHz to ~1GHz)
 * @voltage:	Voltage, in mV. Range: 2^9 < voltage < 2^13 (~0.5V to ~8V)
 *
 * Keep a record of the approximate range of each value at every stage of the
 * calculation, to ensure we don't overflow. This makes heavy use of the
 * approximations 1000 = 2^10 and 1000000 = 2^20, but does the actual
 * calculations in decimal for increased accuracy.
 *
 * Return: Power consumption, in mW. Range: 0 < p < 2^13 (0W to ~8W)
 */
static inline unsigned long kbase_scale_power(const unsigned long c,
					      const unsigned long freq,
					      const unsigned long voltage)
{
	/* Range: 2^8 < v2 < 2^16 m(V^2) */
	const unsigned long v2 = (voltage * voltage) / 1000;

	/* Range: 2^3 < f_MHz < 2^10 MHz */
	const unsigned long f_MHz = freq / 1000000;

	/* Range: 2^11 < v2f_big < 2^26 kHz V^2 */
	const unsigned long v2f_big = v2 * f_MHz;

	/* Range: 2^1 < v2f < 2^16 MHz V^2 */
	const unsigned long v2f = v2f_big / 1000;

	/* Range (working backwards from next line): 0 < v2fc < 2^23 uW */
	const unsigned long v2fc = c * v2f;

	/* Range: 0 < v2fc / 1000 < 2^13 mW */
	return v2fc / 1000;
}

#ifdef CONFIG_MALI_PWRSOFT_765
static unsigned long kbase_get_static_power(struct devfreq *df,
					    unsigned long voltage)
#else
static unsigned long kbase_get_static_power(unsigned long voltage)

#endif
{
	struct kbase_ipa_model *model;
#ifdef CONFIG_MALI_PWRSOFT_765
	struct kbase_device *kbdev = dev_get_drvdata(&df->dev);
#else
	struct kbase_device *kbdev = kbase_find_device(-1);
#endif

	/* We make sure that the model we access is the correct one */
	model = ACCESS_ONCE(kbdev->ipa_current_model);

#ifndef CONFIG_MALI_PWRSOFT_765
	kbase_release_device(kbdev);
#endif

	if (model)
		return model->ops->get_static_power(model, voltage);
	return 0;
}

#ifdef CONFIG_MALI_PWRSOFT_765
static unsigned long kbase_get_dynamic_power(struct devfreq *df,
					     unsigned long freq,
					     unsigned long voltage)
#else
static unsigned long kbase_get_dynamic_power(unsigned long freq,
					     unsigned long voltage)
#endif
{
	struct kbase_ipa_model *model;
#ifdef CONFIG_MALI_PWRSOFT_765
	struct kbase_device *kbdev = dev_get_drvdata(&df->dev);
#else
	struct kbase_device *kbdev = kbase_find_device(-1);
#endif

	/* We make sure that the model we access is the correct one */
	model = ACCESS_ONCE(kbdev->ipa_current_model);

#ifndef CONFIG_MALI_PWRSOFT_765
	kbase_release_device(kbdev);
#endif

	if (model) {
		const unsigned long c = model->ops->get_dynamic_power(model);

		return kbase_scale_power(c, freq, voltage);
	}

	return 0;
}

#ifdef CONFIG_MALI_PWRSOFT_765
unsigned long kbase_power_to_state(struct devfreq *df, u32 target_power)
{
	struct kbase_ipa_model *model;
	struct kbase_device *kbdev = dev_get_drvdata(&df->dev);

	/* We make sure that the model we access is the correct one */
	model = ACCESS_ONCE(kbdev->ipa_current_model);

	if (model && model->ops->power_to_state)
		return model->ops->power_to_state(model, target_power);

	return 0;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
struct devfreq_cooling_ops power_model_ops = {
#else
struct devfreq_cooling_power power_model_ops = {
#endif
	.get_static_power = &kbase_get_static_power,
	.get_dynamic_power = &kbase_get_dynamic_power,
};

unsigned long kbase_ipa_dynamic_power(struct kbase_device *kbdev,
					     unsigned long freq,
					     unsigned long voltage)
{
#ifdef CONFIG_MALI_PWRSOFT_765
	struct devfreq *df = kbdev->devfreq;

	return kbase_get_dynamic_power(df, freq, voltage);
#else
	return kbase_get_dynamic_power(freq, voltage);
#endif
}

KBASE_EXPORT_TEST_API(kbase_ipa_dynamic_power);
