// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/clk-provider.h>

#include <linux/amlogic/pm.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/amlogic/cpu_version.h>

#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include <linux/amlogic/media/sound/aout_notify.h>
#include <linux/amlogic/cpu_version.h>

#include "ddr_mngr.h"
#include "tdm_hw.h"
#include "sharebuffer.h"
#include "vad.h"
#include "spdif_hw.h"
#include "tdm_match_table.h"
#include "effects_v2.h"
#include "spdif.h"
#include "../common/misc.h"
#include "../common/debug.h"
#include "pcpd_monitor.h"
#include "../common/iec_info.h"
#include "iomap.h"
#include "audio_utils.h"
#include "card.h"
#include "soft_locker.h"

#define DRV_NAME "snd_tdm"

#define TDMOUT_DEFAULT_DELAY 3072
#define DEFAULT_UNDERRUN_THRESHOLD 960
#define DEFAULT_DIFF_THRESHOLD 128

static snd_pcm_uframes_t aml_tdm_pointer(struct snd_pcm_substream *substream);
static void dump_pcm_setting(struct pcm_setting *setting)
{
	if (!setting)
		return;

	pr_debug("%s...(%pK)\n", __func__, setting);
	pr_debug("\tpcm_mode(%d)\n", setting->pcm_mode);
	pr_debug("\tsysclk(%d)\n", setting->sysclk);
	pr_debug("\tsysclk_bclk_ratio(%d)\n", setting->sysclk_bclk_ratio);
	pr_debug("\tbclk(%d)\n", setting->bclk);
	pr_debug("\tbclk_lrclk_ratio(%d)\n", setting->bclk_lrclk_ratio);
	pr_debug("\tlrclk(%d)\n", setting->lrclk);
	pr_debug("\ttx_mask(%#x)\n", setting->tx_mask);
	pr_debug("\trx_mask(%#x)\n", setting->rx_mask);
	pr_debug("\tslots(%d)\n", setting->slots);
	pr_debug("\tslot_width(%d)\n", setting->slot_width);
	pr_debug("\tlane_mask_in(%#x)\n", setting->lane_mask_in);
	pr_debug("\tlane_mask_out(%#x)\n", setting->lane_mask_out);
}

#define SRC_LENGTH (32)
struct aml_tdm {
	struct pcm_setting setting;
	struct pinctrl *pin_ctl;
	struct aml_audio_controller *actrl;
	struct device *dev;
	struct clk *clk;
	/* for 44100hz samplerate */
	struct clk *clk_src_cd;
	struct clk *clk_gate;
	struct clk *mclk;
	/* mclk mux out to pad */
	struct clk *mclk2pad;
	struct clk *samesrc_srcpll;
	struct clk *samesrc_clk;
	bool contns_clk;
	unsigned int id;
	/* bclk src selection */
	unsigned int clk_sel;
	struct toddr *tddr;
	struct frddr *fddr;

	struct tdm_chipinfo *chipinfo;
	/* share buffer with module */
	enum sharebuffer_srcs samesource_sel;
	/* share buffer lane setting from DTS */
	int lane_ss;
	/* virtual link for i2s to hdmitx */
	int i2s2hdmitx;
	char tdmin_src_name[SRC_LENGTH];
	uint last_mpll_freq;
	uint last_mclk_freq;
	uint last_fmt;
	unsigned int lane_cnt;

	/* tdmin_lb src sel */
	int tdmin_lb_src;
	int start_clk_enable;
	int clk_tuning_enable;
	int last_rate;

	int ctrl_gain_enable;

	/* Hibernation for vad, suspended or not */
	bool vad_hibernation;
	/* whether vad buffer is used, for xrun */
	bool vad_buf_occupation;
	bool vad_buf_recovery;
	enum trigger_state tdm_trigger_state;
	int underrun_threshold;
	snd_pcm_sframes_t tdmout_last_delay;
	bool tdmout_gain_mute;
	int disable_gain_count;
	int tdm_for_speaker;
	bool tdm_fade_out_enable;
	unsigned int syssrc_clk_rate;
	void *pcpd_monitor_src;
	int pcpd_monitor_enable;
	struct regulator *regulator_vcc3v3;
	struct regulator *regulator_vcc5v;
	int suspend_clk_off;
};

#define TDM_BUFFER_BYTES (512 * 1024)
static const struct snd_pcm_hardware aml_tdm_hardware = {
	.info =
	SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_MMAP_VALID |
	SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_PAUSE,
	.formats =
	    SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
	    SNDRV_PCM_FMTBIT_S32_LE,

	.period_bytes_min = 64,
	.period_bytes_max = 256 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = TDM_BUFFER_BYTES,

	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 32,
};

static inline enum toddr_src aml_tdm_id2src(int id)
{
	enum toddr_src src;

	switch (id) {
	case 0:
		src = TDMIN_A;
	break;
	case 1:
		src = TDMIN_B;
	break;
	case 2:
		src = TDMIN_C;
	break;
	case 3:
		src = TDMIN_D;
	break;
	case 4:
		src = TDMIN_LB;
	break;
	default:
		pr_info("%s(), invalid id: %d\n",
			__func__, id);
		src = TDMIN_A;
	}

	return src;
}

static int pcm_setting_init(struct pcm_setting *setting, unsigned int rate,
			unsigned int channels)
{
	unsigned int ratio = 0;

	setting->lrclk = rate;
	setting->bclk_lrclk_ratio = setting->slots * setting->slot_width;
	setting->bclk = setting->lrclk * setting->bclk_lrclk_ratio;

	/* calculate mclk */
	if (setting->pcm_mode == SND_SOC_DAIFMT_DSP_A ||
		setting->pcm_mode == SND_SOC_DAIFMT_DSP_B) {
		/* for some TDM codec, mclk limites */
		ratio = 2;
	} else {
		ratio = 4;
	}
	setting->sysclk_bclk_ratio = ratio;
	setting->sysclk = ratio * setting->bclk;
	setting->standard_sysclk = setting->sysclk;

	return 0;
}

/* get counts of '1's in val */
static unsigned int pop_count(unsigned int val)
{
	unsigned int count = 0;

	while (val) {
		count++;
		val = val & (val - 1);
	}

	return count;
}

static int aml_tdm_set_lanes(struct aml_tdm *p_tdm,
				unsigned int channels, int stream)
{
	struct pcm_setting *setting = &p_tdm->setting;
	unsigned int lanes, swap_val = 0, swap_val1 = 0;
	unsigned int lane_mask;
	unsigned int i, mask_count;

	/* calc lanes by channels and slots */
	lanes = (channels - 1) / setting->slots + 1;
	if (lanes > p_tdm->lane_cnt) {
		pr_err("set lane error! asoc channels:%d, slots:%d, lane_cnt:%d\n",
		channels, setting->slots, p_tdm->lane_cnt);
		return -EINVAL;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int tx_mask = setting->tx_mask;

		/* set lanes mask accordingly */
		lane_mask = setting->lane_mask_out;
		mask_count = pop_count(tx_mask);

		for (i = 0; i < p_tdm->lane_cnt; i++) {
			if (((1 << i) & lane_mask) && lanes) {
				if (channels < mask_count && mask_count > 1)
					tx_mask = (1 << channels) - 1;
				aml_tdm_set_channel_mask(p_tdm->actrl,
					stream, p_tdm->id, i, tx_mask,
					p_tdm->chipinfo->use_vadtop);
				lanes--;
				if (channels > mask_count)
					channels = channels - mask_count;
			}
		}
		swap_val = 0x76543210;
		if (p_tdm->lane_cnt > LANE_MAX1)
			swap_val1 = 0xfedcba98;
		aml_tdm_set_lane_channel_swap(p_tdm->actrl,
			stream, p_tdm->id, swap_val, swap_val1,
			p_tdm->chipinfo->use_vadtop);
	} else {
		unsigned int rx_mask = setting->rx_mask;
		mask_count = pop_count(rx_mask);

		/* set lanes mask accordingly */
		lane_mask = setting->lane_mask_in;

		if (p_tdm->chipinfo->slot_num_en && setting->slots > 0)
			aml_tdmin_set_slot_num(p_tdm->actrl, p_tdm->id,
					       setting->slots,
					       p_tdm->chipinfo->use_vadtop);

		for (i = 0; i < p_tdm->lane_cnt; i++) {
			if (((1 << i) & lane_mask) && lanes) {
				if (channels < mask_count && mask_count > 1)
					rx_mask = (1 << channels) - 1;
				aml_tdm_set_channel_mask(p_tdm->actrl,
					stream, p_tdm->id, i, rx_mask,
					p_tdm->chipinfo->use_vadtop);
				lanes--;
				if (channels > mask_count)
					channels = channels - mask_count;
			}
		}
		swap_val = 0x76543210;
		if (p_tdm->lane_cnt > LANE_MAX1)
			swap_val1 = 0xfedcba98;
		aml_tdm_set_lane_channel_swap(p_tdm->actrl,
			stream, p_tdm->id, swap_val, swap_val1,
			p_tdm->chipinfo->use_vadtop);
	}

	return 0;
}

static int aml_set_bclk_ratio(struct aml_tdm *p_tdm, unsigned int ratio)
{
	unsigned int bclk_ratio, lrclk_hi;
	unsigned int mode = p_tdm->setting.pcm_mode;

	p_tdm->setting.bclk_lrclk_ratio = ratio;
	bclk_ratio = ratio - 1;
	lrclk_hi = 0;

	if (mode == SND_SOC_DAIFMT_I2S || mode == SND_SOC_DAIFMT_LEFT_J) {
		pr_debug("%s, select I2S mode\n", __func__);
		lrclk_hi = bclk_ratio / 2;
	} else {
		pr_debug("%s, select TDM mode\n", __func__);
	}
	aml_tdm_set_bclk_ratio
		(p_tdm->actrl,
		p_tdm->clk_sel,
		lrclk_hi,
		bclk_ratio,
		p_tdm->chipinfo->use_vadtop);

	return 0;
}

static int aml_tdm_set_clkdiv(struct aml_tdm *p_tdm, int div)
{
	unsigned int mclk_ratio;

	pr_debug("%s, div %d, clksel(%d)\n", __func__, div, p_tdm->clk_sel);
	p_tdm->setting.sysclk_bclk_ratio = div;
	mclk_ratio = div - 1;
	aml_tdm_set_lrclkdiv(p_tdm->actrl, p_tdm->clk_sel, mclk_ratio,
			     p_tdm->chipinfo->use_vadtop);
	return 0;
}

static unsigned int aml_mpll_mclk_ratio(unsigned int freq)
{
	unsigned int i, ratio = 2;
	unsigned int mpll_freq = 0;

	for (i = 1; i < 15; i++) {
		ratio = 1 << i;
		ratio = ratio * 4 * 5; /* for same with earctx, so need *5 */
		mpll_freq = freq * ratio;
		/*when in tdm mode ,tdm slot > 8,the mpll freq maybe > 500M*/
		while (mpll_freq > AML_MPLL_FREQ_MAX) {
			ratio = ratio >> 1;
			mpll_freq = freq * ratio;
		}
		if (mpll_freq > AML_MPLL_FREQ_MIN)
			break;
	}
	//Currently for tdmB/spdifab with the samesource while the tdma running
	//tdmb sysclk 12288000  tdma sysclk 8192000
	if (freq % 8000 == 0)
		ratio = 491520000 / freq;

	return ratio;
}

#define MPLL_HBR_FIXED_FREQ   (491520000)
#define MPLL_CD_FIXED_FREQ    (451584000)

/* normal hifi+mpll clk src each for 44.1k and 48k */
static int aml_set_tdm_mclk_s4(struct aml_tdm *p_tdm,
		unsigned int freq, bool tune)
{
	int ret = -1;
	bool force_mpll = is_force_mpll_clk();

	pr_info("%s: force_mpll = %d, freq = %d\n", __func__, force_mpll, freq);
	if (IS_ERR(p_tdm->clk_src_cd)) {
		pr_err("%s: please make sure S4 DTS support 2 clk source\n", __func__);
		return 0;
	}

	if (force_mpll) {
		int ratio = 0;

		if (p_tdm->setting.standard_sysclk % 8000 == 0)
			ratio = MPLL_HBR_FIXED_FREQ / p_tdm->setting.standard_sysclk;
		else if (p_tdm->setting.standard_sysclk % 11025 == 0)
			ratio = MPLL_CD_FIXED_FREQ / p_tdm->setting.standard_sysclk;

		clk_set_rate(p_tdm->clk_src_cd, freq * ratio);
		ret = clk_set_parent(p_tdm->mclk, p_tdm->clk_src_cd);
		if (ret)
			dev_warn(p_tdm->dev, "can't set tdm parent cd clock\n");
	} else {
		if (p_tdm->setting.standard_sysclk % 8000 == 0) {
			if (p_tdm->syssrc_clk_rate)
				clk_set_rate(p_tdm->clk, p_tdm->syssrc_clk_rate);
			else
				pr_warn("%s(), DTS miss hifi clk rate config", __func__);
			ret = clk_set_parent(p_tdm->mclk, p_tdm->clk);
			if (ret)
				dev_warn(p_tdm->dev, "can't set tdm parent clock\n");
		} else if (p_tdm->setting.standard_sysclk % 11025 == 0) {
			clk_set_rate(p_tdm->clk_src_cd, MPLL_CD_FIXED_FREQ);
			ret = clk_set_parent(p_tdm->mclk, p_tdm->clk_src_cd);
			if (ret)
				dev_warn(p_tdm->dev, "can't set tdm parent cd clock\n");

		} else {
			dev_warn(p_tdm->dev, "unsupport clock rate %d\n",
				p_tdm->setting.standard_sysclk);
		}
	}

	if (!tune)
		clk_set_rate(p_tdm->mclk, freq);
	p_tdm->last_mclk_freq = freq;

	return 0;
}

/* normal 1 audio clk src both for 44.1k and 48k */
static int aml_set_tdm_mclk_1(struct aml_tdm *p_tdm,
		unsigned int freq, bool tune)
{
	unsigned int ratio = aml_mpll_mclk_ratio(freq);
	unsigned int mpll_freq = 0;
	char *clk_name;

	clk_name = (char *)__clk_get_name(p_tdm->clk);
	if (!strcmp(clk_name, "hifi_pll") || !strcmp(clk_name, "t5_hifi_pll") ||
		!strcmp(clk_name, "hifi1_pll"))
		if (p_tdm->syssrc_clk_rate)
			mpll_freq = p_tdm->syssrc_clk_rate;
		else
			mpll_freq = 1806336 * 1000;
	else
		mpll_freq = freq * ratio;

	pr_debug("%s:set mpll_freq: %d\n", __func__, mpll_freq);

	if (mpll_freq != p_tdm->last_mpll_freq) {
		clk_set_rate(p_tdm->clk, mpll_freq);
		p_tdm->last_mpll_freq = mpll_freq;
	}
	clk_set_rate(p_tdm->mclk, freq);
	if (freq != p_tdm->last_mclk_freq) {
		if (!tune)
			clk_set_rate(p_tdm->mclk, freq);
		p_tdm->last_mclk_freq = freq;
	}

	pr_debug("set mclk:%d, mpll:%d, get mclk:%lu, mpll:%lu\n",
		freq,
		freq * ratio,
		clk_get_rate(p_tdm->mclk),
		clk_get_rate(p_tdm->clk));

	return 0;
}

/* 2 audio clk src each for 44.1k and 48k */
static int aml_set_tdm_mclk_2(struct aml_tdm *p_tdm,
		unsigned int freq, bool tune)
{
	int ret = -1;
	char *clk_name = (char *)__clk_get_name(p_tdm->clk);
	int ratio = 0;

	if (IS_ERR(p_tdm->clk_src_cd) || strcmp(clk_name, "hifi_pll")) {
		pr_err("%s: please make sure DTS support 2 clk source\n", __func__);
		return 0;
	}

	if (p_tdm->setting.standard_sysclk % 8000 == 0) {
		ratio = MPLL_HBR_FIXED_FREQ / p_tdm->setting.standard_sysclk;

		clk_set_rate(p_tdm->clk, freq * ratio);
		ret = clk_set_parent(p_tdm->mclk, p_tdm->clk);
		if (ret)
			dev_warn(p_tdm->dev, "can't set tdm parent clock\n");
	} else if (p_tdm->setting.standard_sysclk % 11025 == 0) {
		ratio = MPLL_CD_FIXED_FREQ / p_tdm->setting.standard_sysclk;

		clk_set_rate(p_tdm->clk_src_cd, freq * ratio);
		ret = clk_set_parent(p_tdm->mclk, p_tdm->clk_src_cd);
		if (ret)
			dev_warn(p_tdm->dev, "can't set tdm parent cd clock\n");
	} else {
		dev_warn(p_tdm->dev, "unsupport clock rate %d\n", p_tdm->setting.standard_sysclk);
	}

	if (!tune)
		clk_set_rate(p_tdm->mclk, freq);
	p_tdm->last_mclk_freq = freq;

	pr_debug("set mclk:%d, get mclk:%lu, mpll:%lu, clk_src_cd:%lu\n",
		freq,
		clk_get_rate(p_tdm->mclk),
		clk_get_rate(p_tdm->clk),
		clk_get_rate(p_tdm->clk_src_cd));

	return 0;
}

static int aml_set_tdm_mclk_normal(struct aml_tdm *p_tdm,
			unsigned int freq, bool tune)
{
	int ret = 0;

	if (IS_ERR(p_tdm->clk_src_cd))
		ret = aml_set_tdm_mclk_1(p_tdm, freq, tune);
	else
		ret = aml_set_tdm_mclk_2(p_tdm, freq, tune);

	return ret;
}

static int aml_set_tdm_mclk(struct aml_tdm *p_tdm, unsigned int freq, bool tune)
{
	int ret = 0;
	char *clk_name = (char *)__clk_get_name(p_tdm->clk);

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_S4 && !strcmp(clk_name, "hifi_pll"))
		ret = aml_set_tdm_mclk_s4(p_tdm, freq, tune);
	else
		ret = aml_set_tdm_mclk_normal(p_tdm, freq, tune);

	return ret;
}

static int aml_tdm_set_fmt(struct aml_tdm *p_tdm, unsigned int fmt, bool capture_active)
{
	bool tdmin_src_hdmirx = false;

	if (!p_tdm)
		return -EINVAL;

	pr_debug("%s...%#x, id(%d), clksel(%d)\n",
		 __func__, fmt, p_tdm->id, p_tdm->clk_sel);
	/*
	 *if (p_tdm->last_fmt == fmt) {
	 *	pr_debug("%s(), fmt not change\n", __func__);
	 *	goto capture;
	 *} else {
	 *	p_tdm->last_fmt = fmt;
	 *}
	 */

	switch (fmt & SND_SOC_DAIFMT_CLOCK_MASK) {
	case SND_SOC_DAIFMT_CONT:
		p_tdm->contns_clk = true;
		break;
	case SND_SOC_DAIFMT_GATED:
		p_tdm->contns_clk = false;
		break;
	default:
		return -EINVAL;
	}

	p_tdm->setting.sclk_ws_inv = p_tdm->chipinfo->sclk_ws_inv;

	if (strncmp(p_tdm->tdmin_src_name, SRC_HDMIRX,
		    strlen(SRC_HDMIRX)) == 0)
		tdmin_src_hdmirx = true;
	aml_tdm_set_format(p_tdm->actrl, &p_tdm->setting,
			   p_tdm->clk_sel, p_tdm->id, fmt, 1, 1,
			   tdmin_src_hdmirx,
			   p_tdm->chipinfo->use_vadtop);
	if (p_tdm->contns_clk && !IS_ERR(p_tdm->mclk)) {
		int ret = clk_prepare_enable(p_tdm->mclk);

		if (ret) {
			pr_err("Can't enable mclk: %d\n", ret);
			return ret;
		}
	}

/* capture: */
	/* update skew for ACODEC_ADC */
	if (capture_active &&
	    p_tdm->chipinfo->adc_fn &&
	    strncmp(p_tdm->tdmin_src_name, SRC_ACODEC,
		    strlen(SRC_ACODEC)) == 0) {
		aml_update_tdmin_skew(p_tdm->actrl, p_tdm->id, 4,
				      p_tdm->chipinfo->use_vadtop);
		aml_update_tdmin_rev_ws(p_tdm->actrl, p_tdm->id, 0,
					p_tdm->chipinfo->use_vadtop);
	}

	return 0;
}

void aml_tdm_trigger(struct aml_tdm *p_tdm, int stream, bool enable)
{
	if (!p_tdm)
		return;

	aml_tdm_enable(p_tdm->actrl, stream, p_tdm->id, enable, p_tdm->tdm_fade_out_enable,
		       p_tdm->chipinfo->use_vadtop);
}

int aml_tdm_hw_setting_init(struct aml_tdm *p_tdm,
		       unsigned int rate,
		       unsigned int channels,
		       int stream)
{
	struct pcm_setting *setting = NULL;
	int ret = 0;

	if (!p_tdm)
		return -EINVAL;

	setting = &p_tdm->setting;
	ret = pcm_setting_init(setting, rate, channels);
	if (ret)
		return ret;

	dump_pcm_setting(setting);

	/* set pcm dai hw params */
	p_tdm->setting.standard_sysclk = setting->sysclk;
	aml_set_tdm_mclk(p_tdm, setting->sysclk, false);
	aml_tdm_set_clkdiv(p_tdm, setting->sysclk_bclk_ratio);
	aml_set_bclk_ratio(p_tdm, setting->bclk_lrclk_ratio);

	ret = aml_tdm_set_lanes(p_tdm, channels, stream);
	if (ret)
		return ret;

	/* Must enabe channel number for VAD */
	if (p_tdm->chipinfo->chnum_en &&
	    stream == SNDRV_PCM_STREAM_CAPTURE &&
	    vad_tdm_is_running(p_tdm->id))
		tdmin_set_chnum_en(p_tdm->actrl, p_tdm->id, true,
				   p_tdm->chipinfo->use_vadtop);

	if (!p_tdm->contns_clk && !IS_ERR(p_tdm->mclk)) {
		pr_debug("%s(), enable mclk for tdm-%d\n", __func__, p_tdm->id);
		ret = clk_prepare_enable(p_tdm->mclk);
		if (ret) {
			pr_err("Can't enable mclk: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

void aml_tdm_hw_setting_free(struct aml_tdm *p_tdm, int stream)
{
	int i;

	if (!p_tdm)
		return;

	for (i = 0; i < p_tdm->lane_cnt; i++)
		aml_tdm_set_channel_mask(p_tdm->actrl,
			stream, p_tdm->id, i, 0,
			p_tdm->chipinfo->use_vadtop);

	/* disable clock and gate */
	if (!p_tdm->contns_clk && !IS_ERR(p_tdm->mclk)) {
		pr_err("%s(), disable mclk for tdm-%d", __func__, p_tdm->id);
		clk_disable_unprepare(p_tdm->mclk);
	}
}

unsigned int get_tdmin_src(struct src_table *table, const char *src)
{
	for (; table->name[0]; table++) {
		if (strncmp(table->name, src, strlen(src)) == 0)
			return table->val;
	}

	return 0;
}

void aml_tdmin_set_src(struct aml_tdm *p_tdm)
{
	int src_val;

	if (!p_tdm)
		return;

	/* if tdm* is using internal ADC, reset tdmin src to ACODEC */
	if (p_tdm->chipinfo->adc_fn && strlen(p_tdm->tdmin_src_name) > 0) {
		src_val = get_tdmin_src(p_tdm->chipinfo->tdmin_srcs,
			p_tdm->tdmin_src_name);
	} else {
		const char *tdm_src;

		switch (p_tdm->id) {
		case TDM_A:
			tdm_src = SRC_TDMIN_A;
			break;
		case TDM_B:
			tdm_src = SRC_TDMIN_B;
			break;
		case TDM_C:
			tdm_src = SRC_TDMIN_C;
			break;
		case TDM_D:
			tdm_src = SRC_TDMIN_D;
			break;
		default:
			tdm_src = SRC_TDMIN_A;
			break;
		}
		src_val = get_tdmin_src(p_tdm->chipinfo->tdmin_srcs, tdm_src);
	}
	aml_update_tdmin_src(p_tdm->actrl, p_tdm->id, src_val, p_tdm->chipinfo->use_vadtop);
}

void tdm_mute_capture(struct aml_tdm *p_tdm, bool mute)
{
	if (!p_tdm)
		return;

	aml_tdm_mute_capture(p_tdm->actrl, p_tdm->id,
			mute, p_tdm->lane_cnt,
			p_tdm->chipinfo->use_vadtop);
}

static int tdm_clk_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	ucontrol->value.enumerated.item[0] = clk_get_rate(p_tdm->mclk);
	return 0;
}

static int tdm_clk_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	int mclk_rate = p_tdm->last_mclk_freq;
	int value = ucontrol->value.enumerated.item[0];

	if (value > 2000000 || value < 0) {
		pr_err("Fine tdm clk setting range (0~2000000), %d\n", value);
		return 0;
	}
	mclk_rate += (value - 1000000);

	mclk_rate >>= 1;
	mclk_rate <<= 1;

	aml_set_tdm_mclk(p_tdm, mclk_rate, true);

	return 0;
}

static int tdm_clk_ppm_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	int mclk_rate = p_tdm->setting.standard_sysclk;
	int value = ucontrol->value.enumerated.item[0];

	if (value > 2000 || value < 0) {
		pr_err("Fine tdm clk ppm tuning range (0~2000), %d\n", value);
		return 0;
	}
	mclk_rate = mclk_rate + mclk_rate * (value - 1000) / 1000000;

	aml_set_tdm_mclk(p_tdm, mclk_rate, true);

	return 0;
}

static int tdmout_gain_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	ucontrol->value.enumerated.item[0] = aml_tdmout_get_gain(p_tdm->id);

	return 0;
}

static int tdmout_gain_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	int value = ucontrol->value.enumerated.item[0];

	pr_info("%s, id: %d, gain: 0x%x\n", __func__, p_tdm->id, value);
	if (p_tdm->chipinfo->gain_ver == GAIN_VER3)
		aml_tdmout_auto_gain_enable(p_tdm->id);
	aml_tdmout_set_gain(p_tdm->id, value);

	return 0;
}

static int tdmout_get_mute_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	ucontrol->value.enumerated.item[0] = aml_tdmout_get_mute(p_tdm->id);

	return 0;
}

static int tdmout_set_mute_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	int value = ucontrol->value.enumerated.item[0];

	aml_tdmout_set_mute(p_tdm->id, value);

	return 0;
}

static const struct snd_kcontrol_new snd_tdm_clk_controls[] = {
	SOC_SINGLE_EXT("TDM MCLK Fine Setting",
				0, 0, 2000000, 0,
				tdm_clk_get,
				tdm_clk_set),
	SOC_SINGLE_EXT("TDM MCLK Fine PPM Tuning",
				0, 0, 2000, 0,
				tdm_clk_get,
				tdm_clk_ppm_set),
};

static const char * const tdmin_source_text[] = {
	"tdmin_a", "tdmin_b", "tdmin_c", "NULL", "NULL",
	"NULL", "hdmirx", "acodec_adc",	"NULL",	"NULL",
	"NULL", "NULL", "tdmout_a", "tdmout_b", "tdmout_c"
};

static const struct soc_enum tdmin_source_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(tdmin_source_text),
		tdmin_source_text);

static int tdmin_src_enum_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	int value = 0;

	if (p_tdm->chipinfo->tdmin_srcs)
		value = get_tdmin_src(p_tdm->chipinfo->tdmin_srcs,
					p_tdm->tdmin_src_name);

	ucontrol->value.enumerated.item[0] = value;

	return 0;
}

static int tdmin_src_enum_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	int value = ucontrol->value.enumerated.item[0];
	char *p = (char *)p_tdm->tdmin_src_name;

	if (value >= ARRAY_SIZE(tdmin_source_text))
		return -EINVAL;

	if (p)
		strncpy(p, tdmin_source_text[value],	SRC_LENGTH - 1);

	return 0;
}

static const struct soc_enum hdmi_audio_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_type_texts),
			audio_type_texts);

static int hdmiin_audio_type_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	struct pcpd_monitor *pc_pd = (struct pcpd_monitor *)p_tdm->pcpd_monitor_src;

	if (!pc_pd)
		return 0;
	ucontrol->value.enumerated.item[0] =
		pcpd_monitor_check_audio_type(pc_pd);

	return 0;
}

static int tdm_port_pcpd_detect_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	ucontrol->value.enumerated.item[0] = p_tdm->pcpd_monitor_enable;

	return 0;
}

static int tdm_port_pcpd_detect_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	struct pcpd_monitor *pc_pd = (struct pcpd_monitor *)p_tdm->pcpd_monitor_src;
	int value = ucontrol->value.enumerated.item[0];

	p_tdm->pcpd_monitor_enable = value;
	if (pc_pd)
		aml_pcpd_monitor_enable(pc_pd, value);

	return 0;
}

static const struct snd_kcontrol_new snd_tdm_a_controls[] = {
	/*TDMOUT_A gain, enable data * gain*/
	SOC_SINGLE_EXT("TDMOUT_A GAIN",
		       0, 0, 255, 0,
		       tdmout_gain_get,
		       tdmout_gain_set),
	SOC_SINGLE_BOOL_EXT("TDMOUT_A Mute",
			    0,
			    tdmout_get_mute_enum,
			    tdmout_set_mute_enum),
	SOC_ENUM_EXT("TDMIN_A source select",
				tdmin_source_enum,
				tdmin_src_enum_get,
				tdmin_src_enum_put),
	SOC_SINGLE_EXT("TDMA_Port PC_PD Detect enable",
		       0, 0, 1, 0,
		       tdm_port_pcpd_detect_get,
		       tdm_port_pcpd_detect_set),
};

static const struct snd_kcontrol_new snd_tdm_b_controls[] = {
	/*TDMOUT_B gain, enable data * gain*/
	SOC_SINGLE_EXT("TDMOUT_B GAIN",
		       0, 0, 255, 0,
		       tdmout_gain_get,
		       tdmout_gain_set),
	SOC_SINGLE_BOOL_EXT("TDMOUT_B Mute",
			    0,
			    tdmout_get_mute_enum,
			    tdmout_set_mute_enum),
	SOC_ENUM_EXT("TDMIN_B source select",
				tdmin_source_enum,
				tdmin_src_enum_get,
				tdmin_src_enum_put),

	SOC_ENUM_EXT("HDMI TDM Audio Type",
				hdmi_audio_type_enum,
				hdmiin_audio_type_get_enum,
				NULL),
	SOC_SINGLE_EXT("TDMB_Port PC_PD Detect enable",
		       0, 0, 1, 0,
		       tdm_port_pcpd_detect_get,
		       tdm_port_pcpd_detect_set),

};

static const struct snd_kcontrol_new snd_tdm_c_controls[] = {
	/*TDMOUT_C gain, enable data * gain*/
	SOC_SINGLE_EXT("TDMOUT_C GAIN",
		       0, 0, 255, 0,
		       tdmout_gain_get,
		       tdmout_gain_set),
	SOC_SINGLE_BOOL_EXT("TDMOUT_C Mute",
			    0,
			    tdmout_get_mute_enum,
			    tdmout_set_mute_enum),
};

static const struct snd_kcontrol_new snd_tdm_d_controls[] = {
	/*TDMOUT_C gain, enable data * gain*/
	SOC_SINGLE_EXT("TDMOUT_D GAIN",
		       0, 0, 255, 0,
		       tdmout_gain_get,
		       tdmout_gain_set),
	SOC_SINGLE_BOOL_EXT("TDMOUT_D Mute",
			    0,
			    tdmout_get_mute_enum,
			    tdmout_set_mute_enum),
};
static irqreturn_t aml_tdm_ddr_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)devid;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->cpu_dai->dev;
	struct aml_tdm *p_tdm = (struct aml_tdm *)dev_get_drvdata(dev);

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
	    p_tdm->chipinfo->reset_tdmin &&
	    (aml_tdmin_get_status(p_tdm->id, p_tdm->chipinfo->use_vadtop) & 0x3ff)) {
		pr_info("%s(), reset tdmin, jiffies:%lu\n", __func__, jiffies);
		snd_pcm_stop_xrun(substream);
	}

	if (p_tdm->tdm_fade_out_enable && substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_pcm_sframes_t delay;
		int err;

		err = snd_pcm_kernel_ioctl(substream, SNDRV_PCM_IOCTL_DELAY,
						&delay);
		if (!err) {
			int difference = (p_tdm->tdmout_last_delay > delay) ?
				(p_tdm->tdmout_last_delay - delay) :
				(delay - p_tdm->tdmout_last_delay);

			if ((delay < p_tdm->underrun_threshold &&
			    delay < p_tdm->tdmout_last_delay &&
			    difference > DEFAULT_DIFF_THRESHOLD &&
			    !p_tdm->tdmout_gain_mute &&
			    p_tdm->disable_gain_count < 5) ||
			    (substream->runtime->status->state == SNDRV_PCM_STATE_DRAINING &&
			     !p_tdm->tdmout_gain_mute)) {
				pr_info("enable gain step, delay: %ld, last: %ld, threshold: %d\n",
					delay, p_tdm->tdmout_last_delay, p_tdm->underrun_threshold);
				aml_tdmout_gain_step(p_tdm->id, true);
				p_tdm->tdmout_gain_mute = true;
			} else if (p_tdm->tdmout_gain_mute &&
				   delay >= p_tdm->tdmout_last_delay &&
				   substream->runtime->status->state != SNDRV_PCM_STATE_DRAINING) {
				pr_info("disable gain step, delay: %ld, last: %ld, threshold: %d\n",
					delay, p_tdm->tdmout_last_delay, p_tdm->underrun_threshold);
				aml_tdmout_gain_step(p_tdm->id, false);
				p_tdm->tdmout_gain_mute = false;
				p_tdm->disable_gain_count++;
			}

			p_tdm->tdmout_last_delay = delay;
		}
	}

#ifdef DEBUG_IRQ
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && p_tdm->fddr) {
		unsigned int addr = aml_tdm_pointer(substream);
		struct snd_pcm_runtime *runtime = substream->runtime;

		get_time_stamp(FRDDR, p_tdm->fddr->fifo_id, addr,
			(runtime->control->appl_ptr) % (runtime->buffer_size),
			runtime->buffer_size);
	}
#endif

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static int of_parse_tdm_lane_slot_in(struct device_node *np,
			      unsigned int *lane_mask)
{
	if (lane_mask)
		return snd_soc_of_get_slot_mask(np,
			"dai-tdm-lane-slot-mask-in", lane_mask);

	return -EINVAL;
}

static int of_parse_tdm_lane_slot_out(struct device_node *np,
			      unsigned int *lane_mask)
{
	if (lane_mask)
		return snd_soc_of_get_slot_mask(np,
			"dai-tdm-lane-slot-mask-out", lane_mask);

	return -EINVAL;
}

static void tdm_sharebuffer_prepare(struct snd_pcm_substream *substream,
	struct aml_tdm *p_tdm)
{
	bool valid = aml_check_sharebuffer_valid(p_tdm->fddr,
			p_tdm->samesource_sel);
	struct samesrc_ops *ops = NULL;

	if (!valid)
		return;

	ops = get_samesrc_ops(p_tdm->samesource_sel);
	if (ops) {
		ops->prepare(substream,
			p_tdm->fddr,
			p_tdm->samesource_sel,
			p_tdm->lane_ss,
			AUD_CODEC_TYPE_STEREO_PCM,
			1,
			p_tdm->chipinfo->separate_tohdmitx_en);
		ops->set_clks(p_tdm->samesource_sel,
			p_tdm->clk,
			(p_tdm->last_mclk_freq >> 1), 1);
	}
}

static void tdm_sharebuffer_trigger(struct aml_tdm *p_tdm,
	int channels, int cmd)
{
	bool valid = aml_check_sharebuffer_valid(p_tdm->fddr,
			p_tdm->samesource_sel);
	struct samesrc_ops *ops = NULL;

	if (!valid)
		return;

	ops = get_samesrc_ops(p_tdm->samesource_sel);
	if (ops) {
		int reenable = 0;

		if (channels > 2 && !(p_tdm->fddr->chipinfo->chnum_sync))
			reenable = 1;
		ops->trigger(cmd,
			p_tdm->samesource_sel,
			reenable);
	}
}

static void tdm_sharebuffer_mute(struct aml_tdm *p_tdm, bool mute)
{
	bool valid = aml_check_sharebuffer_valid(p_tdm->fddr,
			p_tdm->samesource_sel);
	struct samesrc_ops *ops = NULL;

	if (!valid)
		return;

	ops = get_samesrc_ops(p_tdm->samesource_sel);
	if (ops)
		ops->mute(p_tdm->samesource_sel, mute);
}

static void tdm_sharebuffer_free(struct aml_tdm *p_tdm,
	struct snd_pcm_substream *substream)
{
	bool valid = aml_check_sharebuffer_valid(p_tdm->fddr,
			p_tdm->samesource_sel);
	struct samesrc_ops *ops = NULL;

	if (!valid)
		return;

	ops = get_samesrc_ops(p_tdm->samesource_sel);
	if (ops) {
		ops->hw_free(substream,
			p_tdm->fddr, p_tdm->samesource_sel, 1);
	}
}

static void tdm_sharebuffer_reset(struct aml_tdm *p_tdm, int channels)
{
	int offset = p_tdm->chipinfo->reset_reg_offset;
	bool valid = aml_check_sharebuffer_valid(p_tdm->fddr,
			p_tdm->samesource_sel);
	struct samesrc_ops *ops = NULL;

	if (!valid)
		return;

	ops = get_samesrc_ops(p_tdm->samesource_sel);
	if (ops && channels > 2 && !(p_tdm->fddr->chipinfo->chnum_sync))
		ops->reset(p_tdm->samesource_sel - 3,
				offset);
}

static int aml_tdm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->cpu_dai->dev;
	struct aml_tdm *p_tdm;
	int ret = 0;

	p_tdm = (struct aml_tdm *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(substream, &aml_tdm_hardware);
	snd_pcm_lib_preallocate_pages(substream, SNDRV_DMA_TYPE_DEV,
		dev, TDM_BUFFER_BYTES / 2, TDM_BUFFER_BYTES);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		int dst_id = get_aed_dst();
		bool aed_dst_status = false;

		if (dst_id == p_tdm->id && is_aed_reserve_frddr())
			aed_dst_status = true;
		p_tdm->fddr = aml_audio_register_frddr(dev,
			aml_tdm_ddr_isr,
			substream, aed_dst_status);
		if (!p_tdm->fddr) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim from ddr\n");
			goto err_ddr;
		}
	} else {
		p_tdm->tddr = aml_audio_register_toddr(dev,
			aml_tdm_ddr_isr, substream);
		if (!p_tdm->tddr) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim to ddr\n");
			goto err_ddr;
		}
	}

	runtime->private_data = p_tdm;
	return 0;

err_ddr:
	snd_pcm_lib_preallocate_free(substream);
	return ret;
}

static int aml_tdm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_tdm *p_tdm = runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aml_audio_unregister_frddr(p_tdm->dev,
				substream);
	else
		aml_audio_unregister_toddr(p_tdm->dev,
				substream);
	snd_pcm_lib_preallocate_free(substream);

	return 0;
}

static int aml_tdm_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int aml_tdm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int aml_tdm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_tdm *p_tdm = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;
	unsigned int period, threshold;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - FIFO_BURST;
	period	 = frames_to_bytes(runtime, runtime->period_size);
	int_addr = period / FIFO_BURST;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_tdm->fddr;

		if (p_tdm->chipinfo->async_fifo) {
			int offset = p_tdm->chipinfo->reset_reg_offset;

			pr_debug("%s(), reset fddr\n", __func__);
			aml_frddr_reset(p_tdm->fddr, offset);
			aml_tdm_out_reset(p_tdm->id, offset);
			if (p_tdm->samesource_sel != SHAREBUFFER_NONE)
				tdm_sharebuffer_reset(p_tdm, runtime->channels);
		}

		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, fr->chipinfo->fifo_depth);
		threshold /= 2;
		/* Use all the fifo */
		aml_frddr_set_fifos(fr, fr->chipinfo->fifo_depth, threshold);

		aml_frddr_set_buf(fr, start_addr, end_addr);
		aml_frddr_set_intrpt(fr, int_addr);
	} else {
		struct toddr *to = p_tdm->tddr;

		if (p_tdm->tdm_trigger_state == TRIGGER_START_ALSA_BUF ||
		    p_tdm->tdm_trigger_state == TRIGGER_START_VAD_BUF) {
			pr_err("%s, trigger state is %d\n", __func__,
				p_tdm->tdm_trigger_state);
			return 0;
		}

		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, to->chipinfo->fifo_depth);
		threshold /= 2;
		aml_toddr_set_fifos(to, threshold);

		aml_toddr_set_buf(to, start_addr, end_addr);
		aml_toddr_set_intrpt(to, int_addr);
	}

	return 0;
}

static snd_pcm_uframes_t aml_tdm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_tdm *p_tdm = runtime->private_data;
	unsigned int addr, start_addr;
	snd_pcm_uframes_t frames;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_tdm->fddr);
	else
		addr = aml_toddr_get_position(p_tdm->tddr);

	frames = bytes_to_frames(runtime, addr - start_addr);
	if (frames > runtime->buffer_size)
		frames = 0;

	return frames;
}

static int aml_tdm_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops aml_tdm_ops = {
	.open = aml_tdm_open,
	.close = aml_tdm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = aml_tdm_hw_params,
	.hw_free = aml_tdm_hw_free,
	.prepare = aml_tdm_prepare,
	.pointer = aml_tdm_pointer,
	.mmap = aml_tdm_mmap,
};

static int tdm_fade_out_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	p_tdm->tdm_fade_out_enable = ucontrol->value.integer.value[0];
	return 0;
}

static int tdm_fade_out_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	ucontrol->value.integer.value[0] = p_tdm->tdm_fade_out_enable;
	return 0;
}

static int tdm_underrun_threshold_put(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	p_tdm->underrun_threshold = ucontrol->value.integer.value[0];
	return 0;
}

static int tdm_underrun_threshold_get(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *cpu_dai = snd_kcontrol_chip(kcontrol);
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	ucontrol->value.integer.value[0] = p_tdm->underrun_threshold;
	return 0;
}

static const struct snd_kcontrol_new tdm_fade_out_controls[] = {
	SOC_SINGLE_BOOL_EXT("tdm fade out enable",
			    0,
			    tdm_fade_out_enable_get,
			    tdm_fade_out_enable_put),
	SOC_SINGLE_EXT("tdm underrun auto fade-out threshold",
		       0, 0, 4096, 0,
		       tdm_underrun_threshold_get,
		       tdm_underrun_threshold_put),
};

static const struct snd_soc_component_driver aml_tdm_component = {
	.name           = DRV_NAME,
	.ops            = &aml_tdm_ops,
};

static void set_aud_param_ch_status(struct iec958_chsts *chsts,
	struct aud_para *aud_param)
{
	aud_param->status[0] = chsts->chstat0_l & 0xff;
	aud_param->status[1] = (chsts->chstat0_l >> 8) & 0xff;
	aud_param->status[2] = chsts->chstat1_l & 0xff;
	aud_param->status[3] = (chsts->chstat1_l >> 8) & 0xff;
}

static int aml_dai_tdm_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	int bit_depth, separated = 0;
	int audio_inskew = -1;
	int audio_tdm_index = -1;
	int audio_binv = -1;
	int audio_binv_tdm_index = -1;

	bit_depth = snd_pcm_format_width(runtime->format);

	aml_tdm_hw_setting_init(p_tdm, runtime->rate, runtime->channels, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_tdm->fddr;
		enum frddr_dest dst;
		unsigned int fifo_id;
		/*  for ss feature,we only disable that configuration
		 *  for multi-ch to HDMITX,for spdif feature 2 of 8 ch
		 *  case, we still config spdif module
		 *  TODO FIXME, consider 8 ch i2s, 2 ch spdif case even hdmitx 8 ch.
		 */
		if (p_tdm->samesource_sel != SHAREBUFFER_NONE &&
			get_i2s2hdmitx_audio_format(rtd->card) == AUD_CODEC_TYPE_STEREO_PCM)
			tdm_sharebuffer_prepare(substream, p_tdm);

		/* i2s source to hdmix */
		if (get_hdmitx_audio_src(rtd->card) == (p_tdm->id + HDMITX_SRC_TDM_A)) {
			enum aud_codec_types codec_type = get_i2s2hdmitx_audio_format(rtd->card);
			int i2s_out_mask = get_hdmitx_i2s_mask(rtd->card);
			unsigned int event_type = 0;
			struct iec958_chsts chsts;
			struct aud_para aud_param;

			memset(&aud_param, 0, sizeof(aud_param));
			aud_param.rate = runtime->rate;
			aud_param.size = runtime->sample_bits;
			if (codec_type == AUD_CODEC_TYPE_STEREO_PCM) {
				aud_param.chs  = 2;
				aud_param.i2s_ch_mask = (u8)i2s_out_mask;
			} else {
				aud_param.chs  = runtime->channels;
				aud_param.i2s_ch_mask = (1 << (runtime->channels / 2)) - 1;
			}
			aud_param.aud_src_if = AUD_SRC_IF_I2S;

			memset(&chsts, 0, sizeof(chsts));
			separated = p_tdm->chipinfo->separate_tohdmitx_en;
			pr_info("notify tdm to hdmitx,id %d codec_type %d, chs %d, ch_mask %d",
				p_tdm->id, codec_type, aud_param.chs, aud_param.i2s_ch_mask);
			i2s_to_hdmitx_ctrl(separated, p_tdm->id, p_tdm->clk_sel);

			if (codec_type == AUD_CODEC_TYPE_TRUEHD)
				event_type = AOUT_EVENT_RAWDATA_MAT_MLP;
			else if (codec_type == AUD_CODEC_TYPE_DTS_HD_MA)
				event_type = AOUT_EVENT_RAWDATA_DTS_HD_MA;
			else
				event_type = AOUT_EVENT_IEC_60958_PCM;
			iec_get_channel_status_info(&chsts, codec_type,
				runtime->rate, bit_depth, 0);
			set_aud_param_ch_status(&chsts, &aud_param);
			aout_notifier_call_chain(event_type, &aud_param);
		}

		fifo_id = aml_frddr_get_fifo_id(fr);
		aml_tdm_fifo_ctrl(p_tdm->actrl,
			bit_depth,
			substream->stream,
			p_tdm->id,
			fifo_id);

		switch (p_tdm->id) {
		case 0:
			dst = TDMOUT_A;
			break;
		case 1:
			dst = TDMOUT_B;
			break;
		case 2:
			dst = TDMOUT_C;
			break;
		case 3:
			dst = TDMOUT_D;
			break;
		default:
			dev_err(p_tdm->dev,	"invalid id: %d\n",
					p_tdm->id);
			return -EINVAL;
		}
		aml_frddr_set_format(fr,
			runtime->channels,
			runtime->rate,
			bit_depth - 1,
			tdmout_get_frddr_type(bit_depth));
		aml_frddr_select_dst(fr, dst);
	} else {
		struct toddr *to = p_tdm->tddr;
		enum toddr_src src = aml_tdm_id2src(p_tdm->id);
		unsigned int lsb = 32 - bit_depth;
		unsigned int toddr_type;
		struct toddr_fmt fmt;

		if (p_tdm->tdm_trigger_state == TRIGGER_START_ALSA_BUF ||
		    p_tdm->tdm_trigger_state == TRIGGER_START_VAD_BUF) {
			pr_err("%s, trigger state is %d\n", __func__,
				p_tdm->tdm_trigger_state);
			return 0;
		}

		switch (bit_depth) {
		case 8:
		case 16:
		case 32:
			toddr_type = 0;
			break;
		case 24:
			toddr_type = 4;
			break;
		default:
			dev_err(p_tdm->dev, "invalid bit_depth: %d\n",
					bit_depth);
			return -EINVAL;
		}

		dev_info(substream->pcm->card->dev, "tdm prepare capture\n");
		aml_tdmin_set_src(p_tdm);

		fmt.type      = toddr_type;
		fmt.msb       = 31;
		fmt.lsb       = lsb;
		fmt.endian    = 0;
		fmt.bit_depth = bit_depth;
		fmt.ch_num    = runtime->channels;
		fmt.rate      = runtime->rate;
		aml_toddr_select_src(to, src);
		aml_toddr_set_format(to, &fmt);
		if (p_tdm->pcpd_monitor_src) {
			struct pcpd_monitor *pc_pd = (struct pcpd_monitor *)p_tdm->pcpd_monitor_src;

			pc_pd->tddr = p_tdm->tddr;
			aml_pcpd_monitor_init(pc_pd);
		}
		/*app adjust tdm inskew*/
		audio_inskew = get_aml_audio_inskew(rtd->card);
		audio_tdm_index = get_aml_audio_inskew_index(rtd->card);
		if (audio_inskew >= 0 && p_tdm->id == audio_tdm_index)
			aml_update_tdmin_skew(p_tdm->actrl, p_tdm->id, audio_inskew,
			p_tdm->chipinfo->use_vadtop);
		}
		audio_binv = get_aml_audio_binv(rtd->card);
		audio_binv_tdm_index = get_aml_audio_binv_index(rtd->card);
		if (audio_binv >= 0 && p_tdm->id == audio_binv_tdm_index)
			aml_update_tdmin_binv(p_tdm->actrl, p_tdm->id, audio_binv);

	return 0;
}

static int aml_dai_tdm_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *cpu_dai)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_pcm_runtime *runtime = substream->runtime;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		    p_tdm->tdm_trigger_state == TRIGGER_START_VAD_BUF) {
			/* VAD switch to alsa buffer */
			vad_update_buffer(false);
			audio_toddr_irq_enable(p_tdm->tddr, true);
			p_tdm->tdm_trigger_state = TRIGGER_START_ALSA_BUF;
			break;
		}

		/* reset fifo here.
		 * If not, xrun will cause channel mapping mismatch
		 */
		aml_tdm_fifo_reset(p_tdm->actrl, substream->stream, p_tdm->id,
				   p_tdm->chipinfo->use_vadtop);

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			struct snd_soc_card *card = cpu_dai->component->card;
			/* output START sequence:
			 * 1. Frddr/TDMOUT/SPDIF reset(may cause the AVR mute)
			 * 2. ctrl0 set to 0
			 * 3. TDMOUT enable
			 * 4. SPDIFOUT enable
			 * 5. FRDDR enable
			 */
			dev_dbg(substream->pcm->card->dev,
				 "TDM[%d] Playback enable\n",
				 p_tdm->id);
			/*don't change this flow*/
			aml_aed_top_enable(p_tdm->fddr, true);
			aml_tdm_enable(p_tdm->actrl,
				substream->stream, p_tdm->id, true, p_tdm->tdm_fade_out_enable,
				p_tdm->chipinfo->use_vadtop);
			if (p_tdm->samesource_sel != SHAREBUFFER_NONE)
				tdm_sharebuffer_trigger(p_tdm, runtime->channels, cmd);

			p_tdm->tdmout_last_delay = TDMOUT_DEFAULT_DELAY;
			p_tdm->disable_gain_count = 0;
			p_tdm->tdmout_gain_mute = false;
			aml_frddr_enable(p_tdm->fddr, true);
			udelay(100);
			if (!p_tdm->tdm_fade_out_enable)
				aml_tdmout_enable_gain(p_tdm->id, false,
					p_tdm->chipinfo->gain_ver);
			if (p_tdm->chipinfo->need_mute_tdm)
				aml_tdm_mute_playback(p_tdm->actrl, p_tdm->id,
					false, p_tdm->lane_cnt);
			if (p_tdm->samesource_sel != SHAREBUFFER_NONE)
				tdm_sharebuffer_mute(p_tdm, false);

			locker_reset(aml_get_card_locker(card));
		} else {
			dev_info(substream->pcm->card->dev,
				 "TDM[%d] Capture enable\n",
				 p_tdm->id);
			aml_toddr_enable(p_tdm->tddr, 1);
			aml_tdm_enable(p_tdm->actrl,
				substream->stream, p_tdm->id, true, p_tdm->tdm_fade_out_enable,
				p_tdm->chipinfo->use_vadtop);
			p_tdm->tdm_trigger_state = TRIGGER_START_ALSA_BUF;
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		    vad_tdm_is_running(p_tdm->id) &&
		    pm_audio_is_suspend() &&
			p_tdm->tdm_trigger_state == TRIGGER_START_ALSA_BUF) {
			/* switch to VAD buffer */
			vad_update_buffer(true);
			audio_toddr_irq_enable(p_tdm->tddr, false);
			p_tdm->tdm_trigger_state = TRIGGER_START_VAD_BUF;
			break;
		}

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			/* output STOP sequence:
			 * 1. TDMOUT->muteval
			 * 2. SPDIFOUT->muteval
			 * 3. TDMOUT/SPDIF Disable
			 * 4. FRDDR Disable
			 */
			dev_dbg(substream->pcm->card->dev,
				 "TDM[%d] Playback stop\n",
				 p_tdm->id);
			if (!p_tdm->tdm_fade_out_enable)
				aml_tdmout_enable_gain(p_tdm->id, true,
					p_tdm->chipinfo->gain_ver);
			if (p_tdm->chipinfo->need_mute_tdm)
				aml_tdm_mute_playback(p_tdm->actrl, p_tdm->id,
					true, p_tdm->lane_cnt);
			if (p_tdm->samesource_sel != SHAREBUFFER_NONE)
				tdm_sharebuffer_mute(p_tdm, true);
			aml_aed_top_enable(p_tdm->fddr, false);
			aml_tdm_enable(p_tdm->actrl,
				substream->stream, p_tdm->id, false, p_tdm->tdm_fade_out_enable,
				p_tdm->chipinfo->use_vadtop);
			if (p_tdm->samesource_sel != SHAREBUFFER_NONE)
				tdm_sharebuffer_trigger(p_tdm, runtime->channels, cmd);

			if (p_tdm->chipinfo->async_fifo)
				aml_frddr_check(p_tdm->fddr);

			aml_frddr_enable(p_tdm->fddr, false);
		} else {
			bool toddr_stopped = false;

			if (p_tdm->tdm_trigger_state == TRIGGER_STOP)
				break;
			aml_tdm_enable(p_tdm->actrl,
				substream->stream, p_tdm->id, false, p_tdm->tdm_fade_out_enable,
				p_tdm->chipinfo->use_vadtop);
			dev_info(substream->pcm->card->dev,
				 "TDM[%d] Capture stop\n",
				 p_tdm->id);

			toddr_stopped = aml_toddr_burst_finished(p_tdm->tddr);
			if (toddr_stopped)
				aml_toddr_enable(p_tdm->tddr, false);
			else
				pr_err("%s(), toddr may be stuck\n", __func__);
			p_tdm->tdm_trigger_state = TRIGGER_STOP;
		}

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int aml_dai_tdm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *cpu_dai)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int rate = params_rate(params);
	unsigned int channels = params_channels(params);
	struct snd_soc_card *card = cpu_dai->component->card;
	struct soft_locker *locker = aml_get_card_locker(card);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_tdm->fddr;

		fr->buf_frames = params_buffer_size(params);
		fr->frame_size = snd_soc_params_to_frame_size(params) / 8;
		fr->rate = rate;
		locker_register_frddr(locker, fr, cpu_dai->name);
	} else {
		struct toddr *to = p_tdm->tddr;

		to->buf_frames = params_buffer_size(params);
		to->frame_size = snd_soc_params_to_frame_size(params) / 8;
		to->rate = rate;
		locker_register_toddr(locker, to, cpu_dai->name);
	}
	locker_en_ddr_by_dai_name(locker,
			cpu_dai->name, substream->stream);

	if (get_hdmitx_audio_src(card) == (p_tdm->id + HDMITX_SRC_TDM_A))
		/* notify HDMITX to disable audio packet */
		notify_hdmitx_to_prepare();

	return aml_tdm_hw_setting_init(p_tdm, rate, channels, substream->stream);
}

static int aml_dai_tdm_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *cpu_dai)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	struct snd_soc_card *card = cpu_dai->component->card;
	struct soft_locker *locker = aml_get_card_locker(card);

	/* Disable channel number for VAD */
	if (p_tdm->chipinfo->chnum_en &&
	    substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
	    vad_tdm_is_running(p_tdm->id))
		tdmin_set_chnum_en(p_tdm->actrl, p_tdm->id, false,
				   p_tdm->chipinfo->use_vadtop);

	/* share buffer free */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (p_tdm->samesource_sel != SHAREBUFFER_NONE)
			tdm_sharebuffer_free(p_tdm, substream);
	}

	aml_tdm_hw_setting_free(p_tdm, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		locker_release_frddr(locker, cpu_dai->name);
	else
		locker_release_toddr(locker, cpu_dai->name);

	locker_en_ddr_by_dai_name(locker, cpu_dai->name, substream->stream);

	return 0;
}

static int aml_dai_set_tdm_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	return aml_tdm_set_fmt(p_tdm, fmt, cpu_dai->capture_active);
}

static int aml_dai_set_bclk_ratio(struct snd_soc_dai *cpu_dai,
						unsigned int ratio)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bclk_ratio, lrclk_hi;

	p_tdm->setting.bclk_lrclk_ratio = ratio;
	bclk_ratio = ratio - 1;
	lrclk_hi = 0;

	if (p_tdm->setting.pcm_mode == SND_SOC_DAIFMT_I2S ||
		p_tdm->setting.pcm_mode == SND_SOC_DAIFMT_LEFT_J) {
		pr_debug("%s, select I2S mode\n", __func__);
		lrclk_hi = bclk_ratio / 2;
	} else {
		pr_debug("%s, select TDM mode\n", __func__);
	}
	aml_tdm_set_bclk_ratio(p_tdm->actrl,
		p_tdm->clk_sel, lrclk_hi, bclk_ratio,
		p_tdm->chipinfo->use_vadtop);

	return 0;
}

static int aml_dai_set_clkdiv(struct snd_soc_dai *cpu_dai,
						int div_id, int div)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int mclk_ratio;

	pr_debug("aml_dai_set_clkdiv, div %d, clksel(%d)\n",
			div, p_tdm->clk_sel);

	p_tdm->setting.sysclk_bclk_ratio = div;
	mclk_ratio = div - 1;
	aml_tdm_set_lrclkdiv(p_tdm->actrl, p_tdm->clk_sel, mclk_ratio,
			     p_tdm->chipinfo->use_vadtop);

	return 0;
}

static int aml_dai_set_tdm_slot(struct snd_soc_dai *cpu_dai,
				unsigned int tx_mask, unsigned int rx_mask,
				int slots, int slot_width)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_soc_dai_driver *drv = cpu_dai->driver;
	unsigned int out_lanes = 0, in_lanes = 0;

	out_lanes = pop_count(p_tdm->setting.lane_mask_out);
	in_lanes = pop_count(p_tdm->setting.lane_mask_in);

	pr_debug("%s(), txmask(%#x), rxmask(%#x)\n",
		__func__, tx_mask, rx_mask);
	pr_debug("\tlanes_out_cnt(%d), lanes_in_cnt(%d)\n",
		out_lanes, in_lanes);
	pr_debug("\tslots(%d), slot_width(%d)\n",
		slots, slot_width);

	p_tdm->setting.tx_mask = tx_mask;
	p_tdm->setting.rx_mask = rx_mask;
	p_tdm->setting.slots = slots;
	p_tdm->setting.slot_width = slot_width;

	if (in_lanes > 0 && in_lanes <= LANE_MAX3)
		aml_tdm_set_slot_in(p_tdm->actrl,
			p_tdm->id, p_tdm->id, slot_width,
			p_tdm->chipinfo->use_vadtop);

	if (out_lanes > 0 && out_lanes <= LANE_MAX3)
		aml_tdm_set_slot_out(p_tdm->actrl,
			p_tdm->id, slots, slot_width);

	/* constrains hw channels_max by DTS configs */
	if (slots && out_lanes)
		drv->playback.channels_max = slots * out_lanes;
	if (slots && in_lanes)
		drv->capture.channels_max = slots * in_lanes;

	return 0;
}

static int aml_dai_tdm_probe(struct snd_soc_dai *cpu_dai)
{
	int ret = 0;
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	if (p_tdm->clk_tuning_enable == 1) {
		ret = snd_soc_add_dai_controls(cpu_dai,
				snd_tdm_clk_controls,
				ARRAY_SIZE(snd_tdm_clk_controls));
		if (ret < 0)
			pr_err("%s, failed add snd tdm clk controls\n",
				__func__);
	}

	if (p_tdm->tdm_for_speaker) {
		ret = snd_soc_add_dai_controls(cpu_dai,
				tdm_fade_out_controls,
				ARRAY_SIZE(tdm_fade_out_controls));
		if (ret < 0)
			pr_err("%s, failed add snd tdm fade out controls\n", __func__);
	}

	/* config ddr arb */
	aml_tdm_arb_config(p_tdm->actrl, p_tdm->chipinfo->use_arb);

	if (p_tdm->ctrl_gain_enable && p_tdm->id == 0) {
		ret = snd_soc_add_dai_controls(cpu_dai,
					       snd_tdm_a_controls,
					       ARRAY_SIZE(snd_tdm_a_controls));
		if (ret < 0)
			pr_err("failed add snd tdmA controls\n");
	} else if (p_tdm->ctrl_gain_enable && p_tdm->id == 1) {
		ret = snd_soc_add_dai_controls(cpu_dai,
					       snd_tdm_b_controls,
					       ARRAY_SIZE(snd_tdm_b_controls));
		if (ret < 0)
			pr_err("failed add snd tdmB controls\n");
	} else if (p_tdm->ctrl_gain_enable && p_tdm->id == 2) {
		ret = snd_soc_add_dai_controls(cpu_dai,
					       snd_tdm_c_controls,
					       ARRAY_SIZE(snd_tdm_c_controls));
		if (ret < 0)
			pr_err("failed add snd tdmC controls\n");
	} else if (p_tdm->ctrl_gain_enable && p_tdm->id == 3) {
		ret = snd_soc_add_dai_controls(cpu_dai,
						snd_tdm_d_controls,
						ARRAY_SIZE(snd_tdm_d_controls));
		if (ret < 0)
			pr_err("failed add snd tdmD controls\n");
	}
	return 0;
}

static int aml_dai_tdm_mute_stream(struct snd_soc_dai *cpu_dai,
				int mute, int stream)
{
	struct aml_tdm *p_tdm = snd_soc_dai_get_drvdata(cpu_dai);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("tdm playback mute: %d, lane_cnt = %d\n",
				mute, p_tdm->lane_cnt);
		//aml_tdm_mute_playback(p_tdm->actrl, p_tdm->id,
		//		mute, p_tdm->lane_cnt);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_debug("tdm capture mute: %d\n", mute);
		tdm_mute_capture(p_tdm, mute);
	}
	return 0;
}

static int aml_set_default_tdm_clk(struct aml_tdm *p_tdm)
{
	unsigned int mclk = 12288000;
	unsigned int ratio = aml_mpll_mclk_ratio(mclk);
	unsigned int lrclk_hi;
	unsigned long pll = mclk * ratio;
	char *clk_name;

	/*set default i2s  timing sequence*/
	int fmt = SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_I2S
	| SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CONT;

	aml_tdm_set_fmt(p_tdm, fmt, 1);
	/*set default i2s clk for codec sequence*/

	/*set default i2s clk for codec sequence*/
	p_tdm->setting.bclk_lrclk_ratio = 64;
	p_tdm->setting.sysclk_bclk_ratio = 4;
	lrclk_hi = p_tdm->setting.bclk_lrclk_ratio - 1;

	aml_tdm_set_lrclkdiv(p_tdm->actrl, p_tdm->clk_sel,
		p_tdm->setting.sysclk_bclk_ratio - 1,
		p_tdm->chipinfo->use_vadtop);

	aml_tdm_set_bclk_ratio(p_tdm->actrl,
		p_tdm->clk_sel, lrclk_hi / 2, lrclk_hi,
		p_tdm->chipinfo->use_vadtop);

	clk_name = (char *)__clk_get_name(p_tdm->clk);
	if (!strcmp(clk_name, "hifi_pll") || !strcmp(clk_name, "t5_hifi_pll") ||
		!strcmp(clk_name, "hifi1_pll")) {
		if (p_tdm->syssrc_clk_rate)
			pll = p_tdm->syssrc_clk_rate;
		else
			pll = 1806336 * 1000;
	}

	clk_prepare_enable(p_tdm->mclk);
	clk_set_rate(p_tdm->clk, pll);
	clk_set_rate(p_tdm->mclk, mclk);

	p_tdm->last_mclk_freq = mclk;
	p_tdm->last_mpll_freq = pll;

	return 0;
}

static struct snd_soc_dai_ops aml_dai_tdm_ops = {
	.prepare = aml_dai_tdm_prepare,
	.trigger = aml_dai_tdm_trigger,
	.hw_params = aml_dai_tdm_hw_params,
	.hw_free = aml_dai_tdm_hw_free,
	.set_fmt = aml_dai_set_tdm_fmt,
	.set_bclk_ratio = aml_dai_set_bclk_ratio,
	.set_clkdiv = aml_dai_set_clkdiv,
	.set_tdm_slot = aml_dai_set_tdm_slot,
	.mute_stream = aml_dai_tdm_mute_stream,
};

#define AML_DAI_TDM_RATES		(SNDRV_PCM_RATE_8000_192000)
#define AML_DAI_TDM_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver aml_tdm_dai[] = {
	{
		.name = "TDM-A",
		.id = 1,
		.probe = aml_dai_tdm_probe,
		.playback = {
		      .channels_min = 1,
		      .channels_max = 32,
		      .rates = AML_DAI_TDM_RATES,
		      .formats = AML_DAI_TDM_FORMATS,
		},
		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates = AML_DAI_TDM_RATES,
		     .formats = AML_DAI_TDM_FORMATS,
		},
		.ops = &aml_dai_tdm_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "TDM-B",
		.id = 2,
		.probe = aml_dai_tdm_probe,
		.playback = {
		      .channels_min = 1,
		      .channels_max = 32,
		      .rates = AML_DAI_TDM_RATES,
		      .formats = AML_DAI_TDM_FORMATS,
		},
		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates = AML_DAI_TDM_RATES,
		     .formats = AML_DAI_TDM_FORMATS,
		},
		.ops = &aml_dai_tdm_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "TDM-C",
		.id = 3,
		.probe = aml_dai_tdm_probe,
		.playback = {
		      .channels_min = 1,
		      .channels_max = 32,
		      .rates = AML_DAI_TDM_RATES,
		      .formats = AML_DAI_TDM_FORMATS,
		},
		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates = AML_DAI_TDM_RATES,
		     .formats = AML_DAI_TDM_FORMATS,
		},
		.ops = &aml_dai_tdm_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "TDM-D",
		.id = 4,
		.probe = aml_dai_tdm_probe,
		.playback = {
		      .channels_min = 1,
		      .channels_max = 32,
		      .rates = AML_DAI_TDM_RATES,
		      .formats = AML_DAI_TDM_FORMATS,
		},
		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates = AML_DAI_TDM_RATES,
		     .formats = AML_DAI_TDM_FORMATS,
		},
		.ops = &aml_dai_tdm_ops,
		.symmetric_rates = 1,
	},
};

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void tdm_platform_early_suspend(struct early_suspend *h)
{
	struct platform_device *pdev = h->param;

	if (pdev) {
		struct aml_tdm *p_tdm = dev_get_drvdata(&pdev->dev);

		if (!p_tdm)
			return;

		p_tdm->vad_hibernation = true;
	}
}

static void tdm_platform_late_resume(struct early_suspend *h)
{
	struct platform_device *pdev = h->param;

	if (pdev) {
		struct aml_tdm *p_tdm = dev_get_drvdata(&pdev->dev);

		if (!p_tdm)
			return;

		p_tdm->vad_hibernation    = false;
		p_tdm->vad_buf_occupation = false;
		p_tdm->vad_buf_recovery   = false;
	}
}

#define tdm_es_hdr(idx) \
static struct early_suspend tdm_es_hdr_##idx = { \
	.suspend = tdm_platform_early_suspend,       \
	.resume  = tdm_platform_late_resume,         \
}

tdm_es_hdr(0);
tdm_es_hdr(1);
tdm_es_hdr(2);

static void tdm_register_early_suspend_hdr(int idx, void *pdev)
{
	struct early_suspend *pes = NULL;

	if (idx == TDM_A)
		pes = &tdm_es_hdr_0;
	else if (idx == TDM_B)
		pes = &tdm_es_hdr_1;
	else if (idx == TDM_C)
		pes = &tdm_es_hdr_2;
	else
		return;

	pes->param   = pdev;
	register_early_suspend(pes);
}
#endif

static int check_channel_mask(const char *str)
{
	int ret = -1;

	if (!strncmp(str, "i2s_0/1", 7))
		ret = 0;
	else if (!strncmp(str, "i2s_2/3", 7))
		ret = 1;
	else if (!strncmp(str, "i2s_4/5", 7))
		ret = 2;
	else if (!strncmp(str, "i2s_6/7", 7))
		ret = 3;
	return ret;
}

/* spdif same source with i2s */
static void parse_samesrc_channel_mask(struct aml_tdm *p_tdm)
{
	struct device_node *node = p_tdm->dev->of_node;
	struct device_node *np = NULL;
	const char *str = NULL;
	int ret = 0;

	/* channel mask */
	np = of_get_child_by_name(node, "Channel_Mask");
	if (!np) {
		pr_info("No channel mask node %s\n",
				"Channel_Mask");
		return;
	}

	/* If spdif is same source to i2s,
	 * it can be muxed to i2s 2 channels
	 */
	ret = of_property_read_string(np,
			"Spdif_samesource_Channel_Mask", &str);
	if (ret) {
		pr_err("error:read Spdif_samesource_Channel_Mask\n");
		return;
	}
	p_tdm->lane_ss = check_channel_mask(str);
	of_node_put(np);

	pr_info("Channel_Mask: lane_ss = %d\n", p_tdm->lane_ss);
}

static int aml_tdm_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct aml_tdm *p_tdm = NULL;
	struct tdm_chipinfo *p_chipinfo = NULL;
	struct device_node *np_src = NULL;
	struct platform_device *dev_src = NULL;
	int ret = 0;

	p_tdm = devm_kzalloc(dev, sizeof(struct aml_tdm), GFP_KERNEL);
	if (!p_tdm)
		return -ENOMEM;

	/* match data */
	p_chipinfo = (struct tdm_chipinfo *)
		of_device_get_match_data(dev);
	if (!p_chipinfo) {
		dev_warn_once(dev, "check whether to update tdm chipinfo\n");
		return -ENOMEM;
	}
	p_tdm->chipinfo = p_chipinfo;
	p_tdm->id = p_chipinfo->id;
	if (!p_chipinfo->lane_cnt)
		p_chipinfo->lane_cnt = LANE_MAX1;

	p_tdm->lane_cnt = p_chipinfo->lane_cnt;
	pr_info("%s, tdm ID = %u, lane_cnt = %d\n", __func__,
			p_tdm->id, p_tdm->lane_cnt);

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (!node_prt)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	p_tdm->actrl = actrl;
	np_src = of_parse_phandle(node, "pcpd_monitor_src", 0);
	if (np_src) {
		dev_src = of_find_device_by_node(np_src);
		of_node_put(np_src);
		p_tdm->pcpd_monitor_src = platform_get_drvdata(dev_src);
		pr_info("%s(), pcpd src found\n", __func__);
	}
	ret = of_property_read_u32(dev->of_node, "src-clk-freq",
				   &p_tdm->syssrc_clk_rate);
	if (ret < 0)
		p_tdm->syssrc_clk_rate = 0;
	else
		pr_info("%s sys-src clk rate from dts:%d\n",
			__func__, p_tdm->syssrc_clk_rate);

	/* get tdm mclk sel configs */
	ret = of_property_read_u32(node, "dai-tdm-clk-sel",
			&p_tdm->clk_sel);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve dai-tdm-clk-sel\n");
		return -ENXIO;
	}

	ret = of_property_read_u32(node, "suspend-clk-off",
			&p_tdm->suspend_clk_off);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve suspend-clk-off\n");
	}

	/* default no same source */
	if (p_tdm->chipinfo->same_src_fn) {
		int ss = 0;

		ret = of_property_read_u32(node, "samesource_sel",
				&ss);
		if (ret < 0) {
			p_tdm->samesource_sel = SHAREBUFFER_NONE;
		} else {
			p_tdm->samesource_sel = ss;

			pr_info("TDM id %d samesource_sel:%d\n",
				p_tdm->id,
				p_tdm->samesource_sel);
		}
	}
	/* default no acodec_adc */
	if (p_tdm->chipinfo->adc_fn) {
		const char *p = NULL;

		ret = of_property_read_string(node, "tdmin-src-name", &p);
		if (p)
			strncpy(p_tdm->tdmin_src_name, p, SRC_LENGTH - 1);

		if (!ret)
			pr_info("TDM id %d supports %s\n",
				p_tdm->id, p_tdm->tdmin_src_name);
	}

	ret = of_property_read_u32(node, "i2s2hdmi", &p_tdm->i2s2hdmitx);
	if (ret < 0)
		p_tdm->i2s2hdmitx = 0;
	else
		pr_info("TDM id %d i2s2hdmi:%d\n",
			p_tdm->id,
			p_tdm->i2s2hdmitx);

	if (p_tdm->id == TDM_LB) {
		ret = of_property_read_u32(node, "lb-src-sel",
				&p_tdm->tdmin_lb_src);
		if (ret < 0 || p_tdm->tdmin_lb_src > 7) {
			dev_err(&pdev->dev, "invalid lb-src-sel:%d\n",
				p_tdm->tdmin_lb_src);
			return -EINVAL;
		}
		pr_info("TDM id %d lb-src-sel:%d\n",
			p_tdm->id,
			p_tdm->tdmin_lb_src);
	}

	/* get tdm lanes info. if not, set to default 0 */
	ret = of_parse_tdm_lane_slot_in(node, &p_tdm->setting.lane_mask_in);
	if (ret < 0)
		p_tdm->setting.lane_mask_in = 0x0;

	ret = of_parse_tdm_lane_slot_out(node, &p_tdm->setting.lane_mask_out);
	if (ret < 0)
		p_tdm->setting.lane_mask_out = 0x1;

	dev_info(&pdev->dev, "lane_mask_out = %x\n",
		 p_tdm->setting.lane_mask_out);

	p_tdm->clk = devm_clk_get(&pdev->dev, "clk_srcpll");
	if (IS_ERR(p_tdm->clk)) {
		dev_err(&pdev->dev, "Can't retrieve srcpll clock\n");
		return PTR_ERR(p_tdm->clk);
	}

	p_tdm->clk_src_cd = devm_clk_get(&pdev->dev, "clk_src_cd");
	if (IS_ERR(p_tdm->clk_src_cd))
		dev_warn(&pdev->dev, "no clk_src_cd clock for 44k case\n");

	p_tdm->mclk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(p_tdm->mclk)) {
		dev_err(&pdev->dev, "Can't retrieve mclk\n");
		return PTR_ERR(p_tdm->mclk);
	}

	if (!IS_ERR(p_tdm->mclk) && !IS_ERR(p_tdm->clk)) {
		ret = clk_set_parent(p_tdm->mclk, p_tdm->clk);
		if (ret)
			dev_warn(dev, "can't set tdm parent clock\n");
	}

	/* clk tree style after SM1, instead of legacy prop */
	p_tdm->mclk2pad = devm_clk_get(&pdev->dev, "mclk_pad");
	if (!IS_ERR(p_tdm->mclk2pad)) {
		ret = clk_set_parent(p_tdm->mclk2pad, p_tdm->mclk);
		if (ret) {
			dev_err(&pdev->dev, "Can't set tdm mclk_pad parent\n");
			return -EINVAL;
		}
		clk_prepare_enable(p_tdm->mclk2pad);
	}
	p_tdm->clk_gate = devm_clk_get(&pdev->dev, "gate_in");
	if (!IS_ERR(p_tdm->clk_gate))
		clk_prepare_enable(p_tdm->clk_gate);
	p_tdm->clk_gate = devm_clk_get(&pdev->dev, "gate_out");
	if (!IS_ERR(p_tdm->clk_gate))
		clk_prepare_enable(p_tdm->clk_gate);

	if (!p_tdm->i2s2hdmitx) {
		p_tdm->pin_ctl = devm_pinctrl_get_select(dev, "tdm_pins");
		if (IS_ERR(p_tdm->pin_ctl)) {
			dev_info(dev, "aml_tdm_get_pins error!\n");
			/*return PTR_ERR(p_tdm->pin_ctl);*/
		}
	}
	ret = of_property_read_u32(node, "start_clk_enable", &p_tdm->start_clk_enable);
	if (ret < 0)
		p_tdm->start_clk_enable = 0;
	else
		pr_info("TDM id %d output clk enable:%d\n",
			p_tdm->id, p_tdm->start_clk_enable);

	ret = of_property_read_u32(node, "ctrl_gain", &p_tdm->ctrl_gain_enable);
	if (ret < 0)
		p_tdm->ctrl_gain_enable = 0;

	/*set default clk for output*/
	if (p_tdm->start_clk_enable == 1)
		aml_set_default_tdm_clk(p_tdm);

	p_tdm->dev = dev;
	dev_set_drvdata(dev, p_tdm);

	/* spdif same source with i2s */
	parse_samesrc_channel_mask(p_tdm);

	ret = devm_snd_soc_register_component(dev, &aml_tdm_component,
					 &aml_tdm_dai[p_tdm->id], 1);
	if (ret) {
		dev_err(dev, "devm_snd_soc_register_component failed\n");
		return ret;
	}

	if (p_tdm->ctrl_gain_enable)
		aml_tdmout_auto_gain_enable(p_tdm->id);

	ret = of_property_read_u32(node, "clk_tuning_enable",
				&p_tdm->clk_tuning_enable);
	if (ret < 0)
		p_tdm->clk_tuning_enable = 0;
	else
		pr_info("TDM id %d tuning clk enable:%d\n",
			p_tdm->id, p_tdm->clk_tuning_enable);

	ret = of_property_read_u32(node, "tdm_for_speaker",
				&p_tdm->tdm_for_speaker);
	if (ret < 0)
		p_tdm->tdm_for_speaker = 0;
	else
		pr_info("TDM id %d tdm_for_speaker\n", p_tdm->id);

	if (p_tdm->chipinfo->regulator) {
		p_tdm->regulator_vcc3v3 = devm_regulator_get(dev, "tdm3v3");
		ret = PTR_ERR_OR_ZERO(p_tdm->regulator_vcc3v3);
		if (ret) {
			if (ret == -EPROBE_DEFER) {
				dev_err(&pdev->dev, "regulator tdm3v3 not ready, retry\n");
				return ret;
			}
			dev_err(&pdev->dev, "failed in regulator tdm3v3 %ld\n",
				PTR_ERR(p_tdm->regulator_vcc3v3));
			p_tdm->regulator_vcc3v3 = NULL;
		} else {
			ret = regulator_enable(p_tdm->regulator_vcc3v3);
			if (ret) {
				dev_err(&pdev->dev,
					"regulator tdm3v3 enable failed:   %d\n", ret);
				p_tdm->regulator_vcc3v3 = NULL;
			}
		}
		p_tdm->regulator_vcc5v = devm_regulator_get(dev, "tdm5v");
		ret = PTR_ERR_OR_ZERO(p_tdm->regulator_vcc5v);
		if (ret) {
			if (ret == -EPROBE_DEFER) {
				dev_err(&pdev->dev, "regulator tdm5v not ready, retry\n");
				return ret;
			}
			dev_err(&pdev->dev, "failed in regulator tdm5v %ld\n",
				PTR_ERR(p_tdm->regulator_vcc5v));
			p_tdm->regulator_vcc5v = NULL;
		} else {
			ret = regulator_enable(p_tdm->regulator_vcc5v);
			if (ret) {
				dev_err(&pdev->dev,
					"regulator tdm5v enable failed:   %d\n", ret);
				p_tdm->regulator_vcc5v = NULL;
			}
		}
	}

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	tdm_register_early_suspend_hdr(p_tdm->id, pdev);
#endif

	p_tdm->underrun_threshold = DEFAULT_UNDERRUN_THRESHOLD;

	return 0;
}

static int aml_tdm_suspend(struct aml_tdm *p_tdm)
{
	if (p_tdm->chipinfo->regulator || (p_tdm->suspend_clk_off && !is_pm_s2idle_mode())) {
		if (!IS_ERR(p_tdm->mclk2pad)) {
			while (__clk_is_enabled(p_tdm->mclk2pad))
				clk_disable_unprepare(p_tdm->mclk2pad);
		}

		if (!IS_ERR(p_tdm->mclk)) {
			while (__clk_is_enabled(p_tdm->mclk))
				clk_disable_unprepare(p_tdm->mclk);
		}

		if (!IS_ERR_OR_NULL(p_tdm->regulator_vcc5v))
			regulator_disable(p_tdm->regulator_vcc5v);
		if (!IS_ERR_OR_NULL(p_tdm->regulator_vcc3v3))
			regulator_disable(p_tdm->regulator_vcc3v3);
	}

	/*mute default clk */
	if (!IS_ERR_OR_NULL(p_tdm->pin_ctl)) {
		struct pinctrl_state *ps = NULL;

		ps = pinctrl_lookup_state(p_tdm->pin_ctl, "tdmout_a_gpio");
		if (!IS_ERR_OR_NULL(ps)) {
			pinctrl_select_state(p_tdm->pin_ctl, ps);
			pr_info("%s tdm pins disable!\n", __func__);
		}
	}
	return 0;
}

static int aml_tdm_platform_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct aml_tdm *p_tdm = dev_get_drvdata(&pdev->dev);

	aml_tdm_suspend(p_tdm);
	pr_info("%s tdm:(%d)\n", __func__, p_tdm->id);
	return 0;
}

static int aml_tdm_resume(struct aml_tdm *p_tdm)
{
	int ret = 0;
	unsigned int out_lanes = 0, in_lanes = 0;

	out_lanes = pop_count(p_tdm->setting.lane_mask_out);
	in_lanes = pop_count(p_tdm->setting.lane_mask_in);

	if (p_tdm->chipinfo->regulator || (p_tdm->suspend_clk_off && !is_pm_s2idle_mode())) {

		audiobus_write(EE_AUDIO_CLK_GATE_EN0, 0xffffffff);
		audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x7, 0x7);

		if (!IS_ERR(p_tdm->mclk) && !IS_ERR(p_tdm->clk)) {
			clk_set_parent(p_tdm->mclk, NULL);
			ret = clk_set_parent(p_tdm->mclk, p_tdm->clk);
			if (ret)
				dev_warn(p_tdm->dev, "can't set tdm parent clock\n");
		}

		if (!IS_ERR(p_tdm->mclk2pad)) {
			clk_set_parent(p_tdm->mclk2pad, NULL);
			ret = clk_set_parent(p_tdm->mclk2pad, p_tdm->mclk);
			if (ret)
				dev_warn(p_tdm->dev, "Can't set tdm mclk_pad parent\n");
			clk_prepare_enable(p_tdm->mclk2pad);
		}

		if (in_lanes > 0 && in_lanes <= LANE_MAX3)
			aml_tdm_set_slot_in(p_tdm->actrl,
				p_tdm->id, p_tdm->id, p_tdm->setting.slot_width,
				p_tdm->chipinfo->use_vadtop);

		if (out_lanes > 0 && out_lanes <= LANE_MAX3)
			aml_tdm_set_slot_out(p_tdm->actrl,
				p_tdm->id, p_tdm->setting.slots, p_tdm->setting.slot_width);

		if (!IS_ERR_OR_NULL(p_tdm->regulator_vcc5v))
			ret = regulator_enable(p_tdm->regulator_vcc5v);
		if (ret)
			dev_err(p_tdm->dev, "regulator tdm5v enable failed:   %d\n", ret);

		if (!IS_ERR_OR_NULL(p_tdm->regulator_vcc3v3))
			ret = regulator_enable(p_tdm->regulator_vcc3v3);
		if (ret)
			dev_err(p_tdm->dev, "regulator tdm3v3 enable failed:   %d\n", ret);
	}

	/*set default clk for output*/
	if (p_tdm->start_clk_enable == 1)
		aml_set_default_tdm_clk(p_tdm);

	if (!IS_ERR_OR_NULL(p_tdm->pin_ctl)) {
		struct pinctrl_state *state = NULL;
		state = pinctrl_lookup_state(p_tdm->pin_ctl, "tdm_pins");
		if (!IS_ERR_OR_NULL(state)) {
			pinctrl_select_state(p_tdm->pin_ctl, state);
			pr_info("%s tdm pins enable!\n", __func__);
		}
	}
	return 0;
}

static int aml_tdm_platform_resume(struct platform_device *pdev)
{
	struct aml_tdm *p_tdm = dev_get_drvdata(&pdev->dev);

	aml_tdm_resume(p_tdm);

	pr_info("%s tdm:(%d)\n", __func__, p_tdm->id);
	return 0;
}

static void aml_tdm_platform_shutdown(struct platform_device *pdev)
{
	struct aml_tdm *p_tdm = dev_get_drvdata(&pdev->dev);

	/*mute default clk */
	if (p_tdm->start_clk_enable == 1 && !IS_ERR_OR_NULL(p_tdm->pin_ctl)) {
		struct pinctrl_state *ps = NULL;

		ps = pinctrl_lookup_state(p_tdm->pin_ctl, "tdmout_a_gpio");
		if (!IS_ERR_OR_NULL(ps)) {
			pinctrl_select_state(p_tdm->pin_ctl, ps);
			pr_info("%s tdm pins disable!\n", __func__);
		}
	}

	if (!IS_ERR_OR_NULL(p_tdm->regulator_vcc5v))
		regulator_disable(p_tdm->regulator_vcc5v);
	if (!IS_ERR_OR_NULL(p_tdm->regulator_vcc3v3))
		regulator_disable(p_tdm->regulator_vcc3v3);

	pr_info("%s tdm:(%d)\n", __func__, p_tdm->id);
}

static int aml_tdm_pm_freeze(struct device *dev)
{
	return 0;
}

static int aml_tdm_pm_thaw(struct device *dev)
{
	return 0;
}

static int aml_tdm_pm_poweroff(struct device *dev)
{
	struct aml_tdm *p_tdm = dev_get_drvdata(dev);

	aml_tdm_suspend(p_tdm);
	pr_info("%s tdm:(%d)\n", __func__, p_tdm->id);
	return 0;
}

static int aml_tdm_pm_suspend(struct device *dev)
{
	struct aml_tdm *p_tdm = dev_get_drvdata(dev);

	aml_tdm_suspend(p_tdm);
	pr_info("%s tdm:(%d)\n", __func__, p_tdm->id);
	return 0;
}

static int aml_tdm_pm_resume(struct device *dev)
{
	struct aml_tdm *p_tdm = dev_get_drvdata(dev);

	aml_tdm_resume(p_tdm);
	pr_info("%s tdm:(%d)\n", __func__, p_tdm->id);
	return 0;
}

static int aml_tdm_pm_restore(struct device *dev)
{
	struct aml_tdm *p_tdm = dev_get_drvdata(dev);

	aml_tdm_resume(p_tdm);
	pr_info("%s tdm:(%d)\n", __func__, p_tdm->id);
	return 0;
}

const struct dev_pm_ops aml_tdm_pm_ops = {
	.freeze = aml_tdm_pm_freeze,
	.thaw = aml_tdm_pm_thaw,
	.poweroff = aml_tdm_pm_poweroff,
	.restore = aml_tdm_pm_restore,
	.suspend = aml_tdm_pm_suspend,
	.resume  = aml_tdm_pm_resume,
};

struct platform_driver aml_tdm_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = aml_tdm_device_id,
		.pm = &aml_tdm_pm_ops,
	},
	.probe	 = aml_tdm_platform_probe,
	.suspend = aml_tdm_platform_suspend,
	.resume  = aml_tdm_platform_resume,
	.shutdown = aml_tdm_platform_shutdown,
};

int __init tdm_init(void)
{
	return platform_driver_register(&aml_tdm_driver);
}

void __exit tdm_exit(void)
{
	platform_driver_unregister(&aml_tdm_driver);
}

#ifndef MODULE
module_init(tdm_init);
module_exit(tdm_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic TDM ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, aml_tdm_device_id);
#endif
