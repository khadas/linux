/*
 * drivers/amlogic/atv_demod/atv_demod_ext.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/types.h>
#include <linux/mutex.h>

#include "atv_demod_ext.h"

hook_func_t aml_fe_hook_atv_status;
hook_func_t aml_fe_hook_hv_lock;
hook_func_t aml_fe_hook_get_fmt;
hook_func1_t aml_fe_hook_set_mode;

static DEFINE_MUTEX(aml_fe_hook_mutex);

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
EXPORT_SYMBOL(aml_fe_hook_cvd);

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
	} else
		state = false;

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
	} else
		state = false;

	mutex_unlock(&aml_fe_hook_mutex);

	return state;
}
