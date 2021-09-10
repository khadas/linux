// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <sound/pcm.h>
#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include <linux/amlogic/media/sound/aout_notify.h>

#include "sharebuffer.h"
#include "ddr_mngr.h"
#include "spdif_hw.h"
#include "earc.h"

struct samesrc_ops *samesrc_ops_table[SHAREBUFFER_SRC_NUM];

struct samesrc_ops *get_samesrc_ops(enum sharebuffer_srcs src)
{
	if (src >= SHAREBUFFER_SRC_NUM)
		return NULL;

	return samesrc_ops_table[src];
}

int register_samesrc_ops(enum sharebuffer_srcs src, struct samesrc_ops *ops)
{
	if (src >= SHAREBUFFER_SRC_NUM)
		return -EINVAL;

	samesrc_ops_table[src] = ops;
	return 0;
}

static int sharebuffer_spdifout_prepare(struct snd_pcm_substream *substream,
	struct frddr *fr, int spdif_id, int lane_i2s,
	enum aud_codec_types type, int separated)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int bit_depth;
	struct iec958_chsts chsts;
	struct aud_para aud_param;

	memset(&aud_param, 0, sizeof(aud_param));

	bit_depth = snd_pcm_format_width(runtime->format);

	spdifout_samesource_set(spdif_id,
				aml_frddr_get_fifo_id(fr),
				bit_depth,
				runtime->channels,
				true,
				lane_i2s);

	/* spdif to hdmitx */
	enable_spdifout_to_hdmitx(separated);
	/* check and set channel status */
	iec_get_channel_status_info(&chsts,
				    type, runtime->rate);
	spdif_set_channel_status_info(&chsts, spdif_id);

	/* for samesource case, always 2ch substream to hdmitx */
	aud_param.rate = runtime->rate;
	aud_param.size = runtime->sample_bits;
	aud_param.chs = 2;

	/* notify hdmitx audio */
	if (get_spdif_to_hdmitx_id() == spdif_id)
		aout_notifier_call_chain(AOUT_EVENT_IEC_60958_PCM, &aud_param);

	return 0;
}

static int sharebuffer_spdifout_free(struct snd_pcm_substream *substream,
	struct frddr *fr, int spdif_id)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int bit_depth;

	bit_depth = snd_pcm_format_width(runtime->format);

	/* spdif b is always on */
	if (spdif_id != 1)
		spdifout_samesource_set
			(spdif_id, aml_frddr_get_fifo_id(fr),
			 bit_depth, runtime->channels, false, 0);

	return 0;
}

void sharebuffer_enable(int sel, bool enable, bool reenable)
{
	if (sel < SHAREBUFFER_TDMA) {
		pr_err("Not support same source\n");
		return;
	} else if (sel <= SHAREBUFFER_TDMC) {
		// TODO: same with tdm
	} else if (sel <= SHAREBUFFER_SPDIFB) {
		/* same source with spdif a/b */
		spdifout_enable(sel - 3, enable, reenable);
	} else if (sel == SHAREBUFFER_EARCTX) {
		aml_earctx_enable(enable);
	}
}

int sharebuffer_prepare(struct snd_pcm_substream *substream,
			void *pfrddr, int samesource_sel,
			int lane_i2s, enum aud_codec_types type, int share_lvl, int separated)
{
	struct frddr *fr = (struct frddr *)pfrddr;

	/* each module prepare, clocks and controls */
	if (samesource_sel < SHAREBUFFER_TDMA) {
		pr_err("Not support same source\n");
		return -EINVAL;
	} else if (samesource_sel <= SHAREBUFFER_TDMC) {
		// TODO: same with tdm
	} else if (samesource_sel <= SHAREBUFFER_SPDIFB) {
		/* same source with spdif a/b */
		sharebuffer_spdifout_prepare
		(substream, fr, samesource_sel - 3, lane_i2s, type, separated);
	} else if (samesource_sel == SHAREBUFFER_EARCTX) {
		sharebuffer_earctx_prepare(substream, fr, type, lane_i2s);
		if (!aml_get_earctx_enable())
			return 0;
	}

	/* frddr, share buffer, src_sel1 */
	aml_frddr_select_dst_ss(fr, samesource_sel, share_lvl, true);
	samesrc_ops_table[samesource_sel]->fr = fr;
	samesrc_ops_table[samesource_sel]->share_lvl = share_lvl;

	return 0;
}

int sharebuffer_free(struct snd_pcm_substream *substream,
	void *pfrddr, int samesource_sel, int share_lvl)
{
	struct frddr *fr = (struct frddr *)pfrddr;

	/* each module prepare, clocks and controls */
	if (samesource_sel < SHAREBUFFER_TDMA) {
		pr_err("Not support same source\n");
		return -EINVAL;
	} else if (samesource_sel <= SHAREBUFFER_TDMC) {
		// TODO: same with tdm
	} else if (samesource_sel <= SHAREBUFFER_SPDIFB) {
		/* same source with spdif a/b */
		sharebuffer_spdifout_free(substream, fr, samesource_sel - 3);
	} else if (samesource_sel == SHAREBUFFER_EARCTX) {
		//TODO: earxtx free
	}

	/* frddr, share buffer, src_sel1 */
	aml_frddr_select_dst_ss(fr, samesource_sel, share_lvl, false);

	return 0;
}

int sharebuffer_trigger(int cmd, int samesource_sel, bool reenable)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		sharebuffer_enable(samesource_sel, true, reenable);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		sharebuffer_enable(samesource_sel, false, reenable);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void sharebuffer_get_mclk_fs_ratio(int samesource_sel,
	int *pll_mclk_ratio, int *mclk_fs_ratio)
{
	if (samesource_sel < SHAREBUFFER_TDMA) {
		pr_err("Not support same source\n");
	} else if (samesource_sel <= SHAREBUFFER_TDMC) {
		// TODO: same with tdm
	} else if (samesource_sel <= SHAREBUFFER_SPDIFB) {
		/* spdif a/b */
		*pll_mclk_ratio = 4;
		*mclk_fs_ratio = 128;
	} else if (samesource_sel == SHAREBUFFER_EARCTX) {
		//TODO: earxtx
	}
}
