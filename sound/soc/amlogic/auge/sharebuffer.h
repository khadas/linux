/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_AUDIO_SHAREBUFFER_H__
#define __AML_AUDIO_SHAREBUFFER_H__

#include <sound/pcm.h>
#include "../common/iec_info.h"

enum sharebuffer_srcs {
	SHAREBUFFER_NONE = -1,
	SHAREBUFFER_TDMA = 0,
	SHAREBUFFER_TDMB = 1,
	SHAREBUFFER_TDMC = 2,
	SHAREBUFFER_SPDIFA = 3,
	SHAREBUFFER_SPDIFB = 4,
	SHAREBUFFER_EARCTX = 5,
	SHAREBUFFER_SRC_NUM = 6
};

struct samesource_info {
	int channels;
	int rate;
};

struct clk;

struct samesrc_ops {
	enum sharebuffer_srcs src;
	struct frddr *fr;
	int share_lvl;
	void *private;

	int (*prepare)(struct snd_pcm_substream *substream,
			void *pfrddr,
			int samesource_sel,
			int lane_i2s,
			enum aud_codec_types type,
			int share_lvl,
			int separated);
	int (*trigger)(int cmd, int samesource_sel, bool reenable);
	int (*hw_free)(struct snd_pcm_substream *substream,
		     void *pfrddr,
		     int samesource_sel,
		     int share_lvl);
	int (*set_clks)(int id,
		struct clk *clk_src, int rate, int same);
	void (*mute)(int id, bool mute);
	void (*reset)(unsigned int id, int offset);
};

struct samesrc_ops *get_samesrc_ops(enum sharebuffer_srcs src);
int register_samesrc_ops(enum sharebuffer_srcs src, struct samesrc_ops *ops);

int sharebuffer_prepare(struct snd_pcm_substream *substream,
			void *pfrddr,
			int samesource_sel,
			int lane_i2s,
			enum aud_codec_types type,
			int share_lvl,
			int separated);
int sharebuffer_free(struct snd_pcm_substream *substream,
		     void *pfrddr,
		     int samesource_sel,
		     int share_lvl);
int sharebuffer_trigger(int cmd, int samesource_sel, bool reenable);

void sharebuffer_get_mclk_fs_ratio(int samesource_sel,
				   int *pll_mclk_ratio,
				   int *mclk_fs_ratio);
#endif
