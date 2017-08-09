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



#ifndef _KBASE_IPA_H_
#define _KBASE_IPA_H_

struct kbase_ipa_model {
	struct list_head link;
	struct kbase_device *kbdev;
	void *model_data;
	struct kbase_ipa_model_ops *ops;
};

struct kbase_ipa_model_ops {
	char *name;
	/* The init and term ops on the default model are always called.
	 * However, all the other models are only invoked if the model
	 * is selected in the device tree. Otherwise they are never
	 * initialized. Additional resources can be acquired by models
	 * in init(), however they must be terminated in the term().
	 */
	int (*init)(struct kbase_ipa_model *model);
	void (*term)(struct kbase_ipa_model *model);
	/* get_dynamic_power() - return a coefficient with units pW/(Hz V^2),
	 * which is scaled by the IPA framework according to the current OPP's
	 * frequency and voltage. */
	unsigned long (*get_dynamic_power)(struct kbase_ipa_model *model);
	/* get_static_power() - return an estimate of the current static power
	 * consumption, in mW, based on the current OPP's voltage, in mV. */
	unsigned long (*get_static_power)(struct kbase_ipa_model *model,
					  unsigned long voltage);
	unsigned long (*power_to_state)(struct kbase_ipa_model *model,
					unsigned long power);
};

/* Models can be registered only in the platform's platform_init_func call */
int kbase_ipa_model_ops_register(struct kbase_device *kbdev,
			     struct kbase_ipa_model_ops *new_model_ops);

int kbase_ipa_model_init(struct kbase_device *kbdev);
void kbase_ipa_model_term(struct kbase_device *kbdev);
void kbase_ipa_model_use_fallback_locked(struct kbase_device *kbdev);
void kbase_ipa_model_use_configured_locked(struct kbase_device *kbdev);

extern struct kbase_ipa_model_ops kbase_generic_ipa_model_ops;

/**
 * kbase_ipa_dynamic_power - calculate power
 * @kbdev:      kbase device
 * @freq:       frequency in Hz
 * @voltage:    voltage in mV
 *
 * Return:      returns power consumption
 */
unsigned long kbase_ipa_dynamic_power(struct kbase_device *kbdev,
					     unsigned long freq,
					     unsigned long voltage);


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
extern struct devfreq_cooling_ops power_model_ops;
#else
extern struct devfreq_cooling_power power_model_ops;
#endif

#endif
