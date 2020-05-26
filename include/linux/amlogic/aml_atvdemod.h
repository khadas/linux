/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_ATVDEMOD_H__
#define __AML_ATVDEMOD_H__

#include <linux/amlogic/aml_demod_common.h>

#if (defined CONFIG_AMLOGIC_ATV_DEMOD ||\
		defined CONFIG_AMLOGIC_ATV_DEMOD_MODULE)
/* For audio driver get atv audio state */
void aml_fe_get_atvaudio_state(int *state);
#else
static inline __maybe_unused void aml_fe_get_atvaudio_state(int *state)
{
	*state = 0;
}
#endif

#endif /* __AML_ATVDEMOD_H__ */
