/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#ifndef _BRIDGE_PCM_HAL_H
#define _BRIDGE_PCM_HAL_H
#include "bridge_common.h"

struct audio_pcm_function_t *audio_pcm_alloc(struct device *dev, enum PCM_CORE pcm_core,
			enum PCM_CARD pcm_card, enum PCM_MODE mode);
void audio_pcm_free(struct audio_pcm_function_t *audio_pcm);
char *find_card_desc(enum PCM_CARD dev);
char *find_mode_desc(enum PCM_MODE dev);
char *find_core_desc(enum PCM_CORE dev);

#endif /*_BRIDGE_PCM_HAL_H*/
