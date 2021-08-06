/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_VRR_H_
#define _AML_VRR_H_

enum vrr_input_src_e {
	VRR_INPUT_TVIN = 0,
	VRR_INPUT_GPU = 1,
	VRR_INPUT_DOS = 1,
	VRR_INPUT_MAX,
};

enum vrr_output_src_e {
	VRR_OUTPUT_ENCP = 0,
	VRR_OUTPUT_ENCL = 1,
	VRR_OUTPUT_MAX,
};

#define VRR_NAME_LEN_MAX    30
struct vrr_device_s {
	char name[VRR_NAME_LEN_MAX];
	enum vrr_output_src_e output_src;
	unsigned int enable;
	unsigned int vmax;
	unsigned int vmin;
};

struct vrr_notifier_data_s {
	int index;
	enum vrr_input_src_e input_src;
	unsigned int target_duration_num;
	unsigned int target_duration_den;
};

#ifdef CONFIG_AMLOGIC_MEDIA_VRR
int aml_vrr_register_device(struct vrr_device_s *vrr_dev, int index);
int aml_vrr_unregister_device(int index);
/* atomic notify */
int aml_vrr_atomic_notifier_register(struct notifier_block *nb);
int aml_vrr_atomic_notifier_unregister(struct notifier_block *nb);
int aml_vrr_atomic_notifier_call_chain(unsigned long event, void *v);
#else
static inline int aml_vrr_register_device(struct vrr_device_s *vrr_dev,
					  int index)
{
	return 0;
}

static inline int aml_vrr_unregister_device(int index)
{
	return 0;
}

int aml_vrr_atomic_notifier_register(struct notifier_block *nb)
{
	return 0;
}

int aml_vrr_atomic_notifier_unregister(struct notifier_block *nb)
{
	return 0;
}

int aml_vrr_atomic_notifier_call_chain(unsigned long event, void *v)
{
	return 0;
}
#endif

#endif
