/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_AUDIO_RESAMPLE_H__
#define __AML_AUDIO_RESAMPLE_H__

#include "resample_hw.h"

enum {
	AXG_RESAMPLE = 0,
	SM1_RESAMPLE = 1,
	T5_RESAMPLE = 2,
};

int card_add_resample_kcontrols(struct snd_soc_card *card);
int resample_set(enum resample_idx id, enum samplerate_index index);
int get_resample_module_num(void);
int set_resample_source(enum resample_idx id, enum toddr_src src);
struct audioresample *get_audioresample(enum resample_idx id);
int get_resample_version_id(enum resample_idx id);
bool get_resample_enable(enum resample_idx id);
bool get_resample_enable_chnum_sync(enum resample_idx id);
int get_resample_source(enum resample_idx id);
int get_resample_version(void);

#endif
