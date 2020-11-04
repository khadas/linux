/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_AUDIO_SHAREBUFFER_H__
#define __AML_AUDIO_SHAREBUFFER_H__

int sharebuffer_prepare(struct snd_pcm_substream *substream,
			void *pfrddr,
			int samesource_sel,
			int lane_i2s,
			int offset,
			int separated);
int sharebuffer_free(struct snd_pcm_substream *substream,
		     void *pfrddr, int samesource_sel);
int sharebuffer_trigger(int cmd, int samesource_sel, bool reenable);
void sharebuffer_get_mclk_fs_ratio(int samesource_sel,
				   int *pll_mclk_ratio, int *mclk_fs_ratio);
#endif
