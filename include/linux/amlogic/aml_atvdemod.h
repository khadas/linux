/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_ATVDEMOD_H__
#define __AML_ATVDEMOD_H__

#include <linux/amlogic/aml_demod_common.h>

typedef int (*hook_func_t)(void);
typedef int (*hook_func1_t)(bool);

#if (defined CONFIG_AMLOGIC_ATV_DEMOD ||\
		defined CONFIG_AMLOGIC_ATV_DEMOD_MODULE)
/* For audio driver get atv audio state */
void aml_fe_get_atvaudio_state(int *state);

/* For atv demod hook tvafe driver state */
void aml_fe_hook_cvd(hook_func_t atv_mode, hook_func_t cvd_hv_lock,
		hook_func_t get_fmt, hook_func1_t set_mode);
#else
static inline __maybe_unused void aml_fe_get_atvaudio_state(int *state)
{
	*state = 0;
}

static inline __maybe_unused void aml_fe_hook_cvd(hook_func_t atv_mode,
		hook_func_t cvd_hv_lock, hook_func_t get_fmt, hook_func1_t set_mode)
{
}
#endif

#endif /* __AML_ATVDEMOD_H__ */
