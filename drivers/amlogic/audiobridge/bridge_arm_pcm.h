/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#ifndef _BRIDGE_ARM_PCM_H
#define _BRIDGE_ARM_PCM_H
#include "bridge_common.h"

int arm_pcm_init(struct audio_pcm_function_t *audio_pcm,
			enum PCM_CARD pcm_card, enum PCM_MODE mode);
void arm_pcm_deinit(struct audio_pcm_function_t *audio_pcm);

#endif /*_BRIDGE_ARM_PCM_H*/
