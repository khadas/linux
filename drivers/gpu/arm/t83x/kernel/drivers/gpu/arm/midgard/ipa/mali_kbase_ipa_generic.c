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



#include <linux/thermal.h>
#ifdef CONFIG_DEVFREQ_THERMAL
#include <linux/devfreq_cooling.h>
#endif
#include <linux/of.h>

#include "mali_kbase.h"
#include "mali_kbase_defs.h"

/*
 * This model is primarily designed for the Juno platform. It may not be
 * suitable for other platforms. The additional resources in this model
 * should preferably be minimal, as this model is rarely used when a dynamic
 * model is available.
 */

/**
 * struct kbase_ipa_model_generic_data - IPA context per device
 * @dynamic_coefficient: dynamic coefficient of the model
 * @static_coefficient:  static coefficient of the model
 * @ts:                  ts of the model
 * @gpu_tz:              thermal zone device
 * @ipa_lock:            protects the entire IPA context
 */

struct kbase_ipa_model_generic_data {
	u32 dynamic_coefficient;
	u32 static_coefficient;
	s32 ts[4];
	struct thermal_zone_device *gpu_tz;
	struct mutex ipa_lock;
};
#define FALLBACK_STATIC_TEMPERATURE 55000

static unsigned long model_static_power(struct kbase_ipa_model *model,
		unsigned long voltage)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0)
	unsigned long temperature;
#else
	int temperature;
#endif
	unsigned long temp;
	unsigned long temp_squared, temp_cubed, temp_scaling_factor;
	const unsigned long voltage_cubed = (voltage * voltage * voltage) >> 10;
	struct kbase_ipa_model_generic_data *model_data =
		(struct kbase_ipa_model_generic_data *) model->model_data;
	struct thermal_zone_device *gpu_tz = model_data->gpu_tz;

	if (gpu_tz) {
		int ret;

		ret = gpu_tz->ops->get_temp(gpu_tz, &temperature);
		if (ret) {
			pr_warn_ratelimited("Error reading temperature for gpu thermal zone: %d\n",
					ret);
			temperature = FALLBACK_STATIC_TEMPERATURE;
		}
	} else {
		temperature = FALLBACK_STATIC_TEMPERATURE;
	}

	/* Calculate the temperature scaling factor. To be applied to the
	 * voltage scaled power.
	 */
	temp = temperature / 1000;
	temp_squared = temp * temp;
	temp_cubed = temp_squared * temp;
	temp_scaling_factor =
			(model_data->ts[3] * temp_cubed)
			+ (model_data->ts[2] * temp_squared)
			+ (model_data->ts[1] * temp)
			+ model_data->ts[0];

	return (((model_data->static_coefficient * voltage_cubed) >> 20)
			* temp_scaling_factor)
				/ 1000000;
}

static unsigned long model_dynamic_power(struct kbase_ipa_model *model)
{
	struct kbase_ipa_model_generic_data *model_data =
		(struct kbase_ipa_model_generic_data *) model->model_data;

	return model_data->dynamic_coefficient;
}

static int kbase_generic_power_model_init(struct kbase_ipa_model *model)
{
	struct kbase_device *kbdev = model->kbdev;
	struct device_node *power_model_node;
	const char *tz_name;
	u32 static_power, dynamic_power;
	u32 voltage, voltage_squared, voltage_cubed, frequency;
	struct kbase_ipa_model_generic_data *model_data;
	int err;

	model_data = kzalloc(sizeof(struct kbase_ipa_model_generic_data),
			    GFP_KERNEL);

	if (!model_data)
		return -ENOMEM;

	power_model_node = of_get_child_by_name(kbdev->dev->of_node,
			"power_model");
	if (!power_model_node) {
		dev_err(kbdev->dev, "could not find power_model node\n");
		err = -ENODEV;
		goto error;
	}
	if (!of_device_is_compatible(power_model_node,
			"arm,mali-simple-power-model")) {
		dev_err(kbdev->dev, "power_model incompatible with simple power model\n");
		err = -ENODEV;
		goto error;
	}

	err = of_property_read_string(power_model_node, "thermal-zone",
			&tz_name);
	if (err) {
		dev_err(kbdev->dev, "thermal zone in power_model not available\n");
		goto error;
	}

	model_data->gpu_tz = thermal_zone_get_zone_by_name(tz_name);
	if (IS_ERR(model_data->gpu_tz)) {
		pr_warn_ratelimited("Error getting gpu thermal zone (%ld), not yet ready?\n",
				PTR_ERR(model_data->gpu_tz));
		model_data->gpu_tz = NULL;
		err = -EPROBE_DEFER;
		goto error;
	}

	err = of_property_read_u32(power_model_node, "static-power",
			&static_power);
	if (err) {
		dev_err(kbdev->dev, "static-power in power_model not available\n");
		goto error;
	}

	err = of_property_read_u32(power_model_node, "dynamic-power",
			&dynamic_power);
	if (err) {
		dev_err(kbdev->dev, "dynamic-power in power_model not available\n");
		goto error;
	}

	err = of_property_read_u32(power_model_node, "voltage",
			&voltage);
	if (err) {
		dev_err(kbdev->dev, "voltage in power_model not available\n");
		goto error;
	}

	err = of_property_read_u32(power_model_node, "frequency",
			&frequency);
	if (err) {
		dev_err(kbdev->dev, "frequency in power_model not available\n");
		goto error;
	}
	voltage_squared = (voltage * voltage) / 1000;
	voltage_cubed = voltage * voltage * voltage;
	model_data->static_coefficient =
		(static_power << 20) / (voltage_cubed >> 10);
	model_data->dynamic_coefficient =
		(((dynamic_power * 1000) / voltage_squared) * 1000) / frequency;

	err = of_property_read_u32_array(power_model_node,
				       "ts",
				       (u32 *)model_data->ts,
				       4);
	if (err) {
		dev_err(kbdev->dev, "ts in power_model not available\n");
		goto error;
	}

	model->model_data = (void *) model_data;

	return 0;
error:
	kfree(model_data);
	return err;
}

static void kbase_generic_power_model_term(struct kbase_ipa_model *model)
{
	struct kbase_ipa_model_generic_data *model_data =
			(struct kbase_ipa_model_generic_data *)model->model_data;

	kfree(model_data);
}

struct kbase_ipa_model_ops kbase_generic_ipa_model_ops = {
		.name = "generic_ipa_model",
		.init = &kbase_generic_power_model_init,
		.term = &kbase_generic_power_model_term,
		.get_dynamic_power = &model_dynamic_power,
		.get_static_power = &model_static_power,
		.power_to_state = NULL
};
