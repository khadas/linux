// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/mutex.h>

#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_atvdemod.h>
#ifdef CONFIG_AMLOGIC_POWER
//#include <linux/amlogic/power_domain.h>
//#include <dt-bindings/power/amlogic,pd.h>
#endif

#include "atv_demod_ext.h"

static DEFINE_MUTEX(aml_fe_hook_mutex);

static hook_func_t aml_fe_hook_atv_status;
static hook_func_t aml_fe_hook_hv_lock;
static hook_func_t aml_fe_hook_get_fmt;
static hook_func1_t aml_fe_hook_set_mode;

void aml_fe_hook_cvd(hook_func_t atv_mode, hook_func_t cvd_hv_lock,
	hook_func_t get_fmt, hook_func1_t set_mode)
{
	mutex_lock(&aml_fe_hook_mutex);

	aml_fe_hook_atv_status = atv_mode;
	aml_fe_hook_hv_lock = cvd_hv_lock;
	aml_fe_hook_get_fmt = get_fmt;
	aml_fe_hook_set_mode = set_mode;

	mutex_unlock(&aml_fe_hook_mutex);

	pr_info("%s: %s OK.\n", __func__, atv_mode != NULL ? "set" : "reset");
}
EXPORT_SYMBOL_GPL(aml_fe_hook_cvd);

bool aml_fe_has_hook_up(void)
{
	bool state = false;

	mutex_lock(&aml_fe_hook_mutex);

	if (!aml_fe_hook_atv_status ||
			!aml_fe_hook_hv_lock ||
			!aml_fe_hook_get_fmt ||
			!aml_fe_hook_set_mode)
		state = false;
	else
		state = true;

	mutex_unlock(&aml_fe_hook_mutex);

	return state;
}

bool aml_fe_hook_call_get_fmt(int *fmt)
{
	bool state = false;

	mutex_lock(&aml_fe_hook_mutex);

	if (aml_fe_hook_get_fmt && fmt) {
		*fmt = aml_fe_hook_get_fmt();
		state = true;
	} else {
		state = false;
	}

	mutex_unlock(&aml_fe_hook_mutex);

	return state;
}

bool aml_fe_hook_call_set_mode(bool mode)
{
	bool state = false;

	mutex_lock(&aml_fe_hook_mutex);

	if (aml_fe_hook_set_mode) {
		aml_fe_hook_set_mode(mode);
		state = true;
	} else {
		state = false;
	}

	mutex_unlock(&aml_fe_hook_mutex);

	return state;
}

void atvdemod_power_switch(bool on)
{
#ifdef CONFIG_AMLOGIC_POWER
	if (is_meson_tm2_cpu()) {
		if (on)
			;//power_domain_switch(PM_ATV_DEMOD, PWR_ON);
		else
			;//power_domain_switch(PM_ATV_DEMOD, PWR_OFF);
	}
#endif
}
