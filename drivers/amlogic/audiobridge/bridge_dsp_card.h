/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#ifndef _BRIDGE_DSP_CARD_H_
#define _BRIDGE_DSP_CARD_H_
#include "bridge_common.h"

struct aml_aprocess_card {
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct device *dev;
	void *c_prm;
	void *p_prm;
};

struct aml_aprocess {
	struct aml_aprocess_card *a_card;
	struct snd_pcm_substream *g_substream;
	struct snd_pcm_hardware g_hw;
	spinlock_t lock;/*Prevents resources from being released while in use*/
	int status;
	int g_hwptr;
};

struct aml_aprocess *aml_aprocess_init(struct device *dev, const char *pcm_name,
					const char *card_name, enum PCM_MODE modeid);
void aml_aprocess_destroy(struct aml_aprocess *p_aprocess);
int aml_aprocess_complete(struct aml_aprocess *p_aprocess, char *out, int size);
void aml_aprocess_set_hw(struct aml_aprocess *p_aprocess, int channels,
					int format, int rate, int period);

#endif
