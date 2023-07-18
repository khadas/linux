// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#define  DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/extcon-provider.h>
#include <linux/pinctrl/consumer.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/asoundef.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/amlogic/pm.h>
#include <linux/clk-provider.h>
#include <linux/amlogic/cpu_version.h>

#include "ddr_mngr.h"
#include "spdif_hw.h"

#include "resample.h"
#include "resample_hw.h"
#include "spdif.h"
#include "spdif_match_table.h"
#include "sharebuffer.h"
#include "../common/iec_info.h"
#include "audio_utils.h"
#include "card.h"
#include "soft_locker.h"

#define DRV_NAME "snd_spdif"

/* for debug */
/*#define __SPDIFIN_INSERT_CHNUM__*/

struct aml_spdif *spdif_priv[2];

static int aml_dai_set_spdif_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir);

static void aml_set_spdifclk(struct aml_spdif *p_spdif, int freq, bool tune);

enum SPDIF_SRC {
	SPDIFIN_PAD = 0,
	SPDIFOUT,
	NOAP,
	HDMIRX
};

struct aml_spdif {
	struct pinctrl *pin_ctl;
	struct aml_audio_controller *actrl;
	struct device *dev;
	struct clk *gate_spdifin;
	struct clk *gate_spdifout;
	struct clk *sysclk;
	/* for 44100hz samplerate */
	struct clk *clk_src_cd;
	struct clk *fixed_clk;
	struct clk *clk_spdifin;
	struct clk *clk_spdifout;
	unsigned int sysclk_freq;
	/* bclk src selection */
	int irq_spdifin;
	struct toddr *tddr;
	struct frddr *fddr;

	/* external connect */
	struct extcon_dev *edev;

	unsigned int id;
	struct spdif_chipinfo *chipinfo;
	unsigned int clk_cont; /* CONTINUOUS CLOCK */

	int last_sample_rate_mode;
	/* last value for pc, pd */
	int pc_last;
	int pd_last;

	/* output audio codec type */
	enum aud_codec_types codec_type;

	/* mixer control vals */
	bool mute;
	enum SPDIF_SRC spdifin_src;
	int clk_tuning_enable;
	bool on;
	int same_src_on;
	int in_err_cnt;
	/* share buffer with module */
	enum sharebuffer_srcs samesource_sel;
	unsigned int l_src;
	unsigned int syssrc_clk_rate;
	struct regulator *regulator_vcc3v3;
	struct regulator *regulator_vcc5v;
	int suspend_clk_off;
	/* Standardization value by normal setting */
	unsigned int standard_sysclk;
};

unsigned int get_spdif_source_l_config(int id)
{
	return spdif_priv[id]->l_src;
}

#define SPDIF_BUFFER_BYTES (512 * 1024 * 2)
static const struct snd_pcm_hardware aml_spdif_hardware = {
	.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_PAUSE,
	.formats =
	    SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
	    SNDRV_PCM_FMTBIT_S32_LE,

	.period_bytes_min = 64,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = SPDIF_BUFFER_BYTES,

	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 2,
	.channels_max = 32,
};

static const unsigned int spdifin_extcon[] = {
	EXTCON_SPDIFIN_SAMPLERATE,
	EXTCON_SPDIFIN_AUDIOTYPE,
	EXTCON_NONE,
};

static void spdif_sharebuffer_prepare(struct snd_pcm_substream *substream,
	struct aml_spdif *p_spdif)
{
	bool valid = aml_check_sharebuffer_valid(p_spdif->fddr,
			p_spdif->samesource_sel);
	struct samesrc_ops *ops = NULL;

	if (!valid)
		return;

	ops = get_samesrc_ops(p_spdif->samesource_sel);
	if (ops) {
		if (ops->prepare)
			ops->prepare(substream,
				p_spdif->fddr,
				p_spdif->samesource_sel,
				0,
				p_spdif->codec_type,
				1,
				p_spdif->chipinfo->separate_tohdmitx_en
				);

		if (ops->set_clks)
			ops->set_clks(p_spdif->samesource_sel,
				p_spdif->sysclk,
				p_spdif->sysclk_freq, 1);
	}
}

static void spdif_sharebuffer_trigger(struct aml_spdif *p_spdif,
	int channels, int cmd)
{
	bool valid = aml_check_sharebuffer_valid(p_spdif->fddr,
			p_spdif->samesource_sel);
	struct samesrc_ops *ops = NULL;

	if (!valid)
		return;

	ops = get_samesrc_ops(p_spdif->samesource_sel);
	if (ops && ops->trigger) {
		int reenable = 0;

		if (channels > 2)
			reenable = 1;
		ops->trigger(cmd,
			p_spdif->samesource_sel,
			reenable);
	}
}

static void spdif_sharebuffer_mute(struct aml_spdif *p_spdif, bool mute)
{
	bool valid = aml_check_sharebuffer_valid(p_spdif->fddr,
			p_spdif->samesource_sel);
	struct samesrc_ops *ops = NULL;

	if (!valid)
		return;

	ops = get_samesrc_ops(p_spdif->samesource_sel);
	if (ops && ops->mute)
		ops->mute(p_spdif->samesource_sel, mute);
}

static void spdif_sharebuffer_free(struct aml_spdif *p_spdif,
	struct snd_pcm_substream *substream)
{
	bool valid = aml_check_sharebuffer_valid(p_spdif->fddr,
			p_spdif->samesource_sel);
	struct samesrc_ops *ops = NULL;

	if (!valid)
		return;

	ops = get_samesrc_ops(p_spdif->samesource_sel);
	if (ops && ops->hw_free) {
		ops->hw_free(substream,
			p_spdif->fddr, p_spdif->samesource_sel, 1);
	}
}

static int ss_prepare(struct snd_pcm_substream *substream,
			void *pfrddr,
			int samesource_sel,
			int lane_i2s,
			enum aud_codec_types type,
			int share_lvl,
			int separated)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct samesrc_ops *ops = NULL;
	int spdif_id = -1;

	pr_debug("%s() %d, lvl %d\n", __func__, __LINE__, share_lvl);
	if (samesource_sel >= SHAREBUFFER_SPDIFA &&
	    samesource_sel <= SHAREBUFFER_SPDIFB) {
		spdif_id = samesource_sel - SHAREBUFFER_SPDIFA;
	}

	if (get_hdmitx_audio_src(rtd->card) == spdif_id) {
		pr_info("%s[%d], hdmitx src switch to spdif %d\n", __func__,
			__LINE__, spdif_id);
		set_spdif_to_hdmitx_id(spdif_id);
	}

	sharebuffer_prepare(substream,
		pfrddr,
		samesource_sel,
		lane_i2s,
		type,
		share_lvl,
		separated);

	ops = get_samesrc_ops(samesource_sel);
	if (ops && ops->private) {
		struct aml_spdif *p_spdif = ops->private;

		p_spdif->same_src_on = 1;
		if (p_spdif->samesource_sel != SHAREBUFFER_NONE &&
		    get_samesrc_ops(p_spdif->samesource_sel) &&
		    get_samesrc_ops(p_spdif->samesource_sel)->prepare) {
			share_lvl++;
			get_samesrc_ops(p_spdif->samesource_sel)->prepare(substream,
				pfrddr,
				p_spdif->samesource_sel,
				lane_i2s,
				type,
				share_lvl,
				separated);
		}
	}

	return 0;
}

static int ss_set_clk(int samesource_sel,
		struct clk *clk_src, int rate, int same)
{
	struct samesrc_ops *ops = NULL;

	spdif_set_audio_clk(samesource_sel - 3,
		clk_src,
		rate, same, false);

	ops = get_samesrc_ops(samesource_sel);
	if (ops && ops->private) {
		struct aml_spdif *p_spdif = ops->private;

		if (p_spdif->samesource_sel != SHAREBUFFER_NONE &&
		    get_samesrc_ops(p_spdif->samesource_sel) &&
		    get_samesrc_ops(p_spdif->samesource_sel)->set_clks) {
			ops = get_samesrc_ops(p_spdif->samesource_sel);
			ops->set_clks(p_spdif->samesource_sel,
				clk_src,
				rate, 1);
		}
	}

	return 0;
}

static int ss_free(struct snd_pcm_substream *substream,
	void *pfrddr, int samesource_sel, int share_lvl)
{
	struct samesrc_ops *ops = NULL;

	pr_info("%s() samesrc %d, lvl %d\n",
		__func__, samesource_sel, share_lvl);
	if (aml_check_sharebuffer_valid(pfrddr,
			samesource_sel)) {
		sharebuffer_free(substream,
			pfrddr, samesource_sel, share_lvl);
	}

	ops = get_samesrc_ops(samesource_sel);
	if (ops && ops->private) {
		struct aml_spdif *p_spdif = ops->private;

		p_spdif->same_src_on = 0;
		if (p_spdif->samesource_sel != SHAREBUFFER_NONE &&
		    get_samesrc_ops(p_spdif->samesource_sel) &&
		    get_samesrc_ops(p_spdif->samesource_sel)->hw_free) {
			share_lvl++;
			get_samesrc_ops(p_spdif->samesource_sel)->hw_free(substream,
				pfrddr,
				p_spdif->samesource_sel,
				share_lvl);
		}
	}

	return 0;
}

static int ss_trigger(int cmd, int samesource_sel, bool reenable)
{
	struct samesrc_ops *ops = NULL;

	pr_debug("%s() ss %d\n", __func__, samesource_sel);
	sharebuffer_trigger(cmd,
		samesource_sel, reenable);

	ops = get_samesrc_ops(samesource_sel);
	if (ops && ops->private) {
		struct aml_spdif *p_spdif = ops->private;

		if (p_spdif->samesource_sel != SHAREBUFFER_NONE &&
		    get_samesrc_ops(p_spdif->samesource_sel) &&
		    get_samesrc_ops(p_spdif->samesource_sel)->trigger) {
			get_samesrc_ops(p_spdif->samesource_sel)->trigger(cmd,
				p_spdif->samesource_sel,
				reenable);
		}
	}

	return 0;
}

static void ss_mute(int samesource_sel, bool mute)
{
	struct samesrc_ops *ops = NULL;

	pr_debug("%s() %d, mute %d, id %d\n", __func__, __LINE__,
		mute, samesource_sel - 3);
	aml_spdifout_mute_without_actrl(samesource_sel - 3, mute);

	ops = get_samesrc_ops(samesource_sel);
	if (ops && ops->private) {
		struct aml_spdif *p_spdif = ops->private;

		if (p_spdif->samesource_sel != SHAREBUFFER_NONE &&
		    get_samesrc_ops(p_spdif->samesource_sel) &&
		    get_samesrc_ops(p_spdif->samesource_sel)->mute) {
			get_samesrc_ops(p_spdif->samesource_sel)->mute(p_spdif->samesource_sel,
				mute);
		}
	}
}

struct samesrc_ops spdifa_ss_ops = {
	.prepare = ss_prepare,
	.trigger = ss_trigger,
	.hw_free = ss_free,
	.set_clks = ss_set_clk,
	.mute	 = ss_mute,
	.reset	 = aml_spdif_out_reset,
};

struct samesrc_ops spdifb_ss_ops = {
	.prepare = ss_prepare,
	.trigger = ss_trigger,
	.hw_free = ss_free,
	.set_clks = ss_set_clk,
	.mute    = ss_mute,
	.reset	 = aml_spdif_out_reset,
};

int spdifout_get_lane_mask_version(int id)
{
	int ret = SPDIFOUT_LANE_MASK_V1;

	if (spdif_priv[id] && spdif_priv[id]->chipinfo)
		ret = spdif_priv[id]->chipinfo->spdifout_lane_mask;

	return ret;
}

unsigned int spdif_get_codec(void)
{
	if (spdif_priv[0])
		return spdif_priv[0]->codec_type;
	return 0;
}

static int spdifin_samplerate_get_enum(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int val = spdifin_get_sample_rate();

	if (val == 0x7)
		val = 0;
	else
		val += 1;

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static const struct soc_enum spdif_audio_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_type_texts),
			audio_type_texts);

static int spdifin_check_audio_type(void)
{
	int total_num = sizeof(type_texts) / sizeof(struct spdif_audio_info);
	int pc = spdifin_get_audio_type();
	int audio_type = 0;
	int i;
	bool is_raw = spdifin_get_ch_status0to31() & IEC958_AES0_NONAUDIO;

	/*
	 * Raw->pcm case, the HW Pc & Pd would keep the value.
	 * So we need check channel status first.
	 * If it's non-pcm audio, then get the audio type from Pc reg.
	 */
	if (!is_raw)
		return 0;

	for (i = 0; i < total_num; i++) {
		if (pc == type_texts[i].pc) {
			audio_type = type_texts[i].aud_type;
			break;
		}
	}

	return audio_type;
}

static int spdifin_audio_type_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] =
		spdifin_check_audio_type();

	return 0;
}

static int aml_spdif_suspend(struct aml_spdif *p_spdif)
{
	struct pinctrl_state *pstate = NULL;
	int stream = SNDRV_PCM_STREAM_PLAYBACK;

	if (p_spdif->chipinfo->regulator || (p_spdif->suspend_clk_off && !is_pm_s2idle_mode())) {
		if (!IS_ERR(p_spdif->clk_spdifout)) {
			while (__clk_is_enabled(p_spdif->clk_spdifout))
				clk_disable_unprepare(p_spdif->clk_spdifout);
		}

		if (!IS_ERR(p_spdif->clk_spdifin)) {
			while (__clk_is_enabled(p_spdif->clk_spdifin))
				clk_disable_unprepare(p_spdif->clk_spdifin);
		}

		if (!IS_ERR_OR_NULL(p_spdif->regulator_vcc5v))
			regulator_disable(p_spdif->regulator_vcc5v);
		if (!IS_ERR_OR_NULL(p_spdif->regulator_vcc3v3))
			regulator_disable(p_spdif->regulator_vcc3v3);
	}

	if (!IS_ERR_OR_NULL(p_spdif->pin_ctl)) {
		pstate = pinctrl_lookup_state
		(p_spdif->pin_ctl, "spdif_pins_mute");
		if (!IS_ERR_OR_NULL(pstate))
			pinctrl_select_state(p_spdif->pin_ctl, pstate);
	}
	aml_spdif_enable(p_spdif->actrl, stream, p_spdif->id, false);
	return 0;
}

static int aml_spdif_platform_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct aml_spdif *p_spdif = dev_get_drvdata(&pdev->dev);

	aml_spdif_suspend(p_spdif);
	pr_info("%s is mute\n", __func__);
	return 0;
}

static int aml_spdif_resume(struct aml_spdif *p_spdif)
{
	struct pinctrl_state *state = NULL;
	int stream = SNDRV_PCM_STREAM_PLAYBACK;
	int ret = 0;

	if (p_spdif->chipinfo->regulator || (p_spdif->suspend_clk_off && !is_pm_s2idle_mode())) {
		if (!IS_ERR(p_spdif->clk_spdifout)) {
			clk_set_parent(p_spdif->clk_spdifout, NULL);
			ret = clk_set_parent(p_spdif->clk_spdifout, p_spdif->sysclk);
			if (ret)
				dev_warn(p_spdif->dev, "Can't set spdif clk_spdifout parent\n");
			clk_prepare_enable(p_spdif->clk_spdifout);
		}

		if (!IS_ERR(p_spdif->clk_spdifin)) {
			clk_set_parent(p_spdif->clk_spdifin, NULL);
			ret = clk_set_parent(p_spdif->clk_spdifin, p_spdif->fixed_clk);
			if (ret)
				dev_warn(p_spdif->dev, "Can't set spdif clk_spdifout parent\n");
			clk_prepare_enable(p_spdif->clk_spdifin);
		}
		aml_set_spdifclk(p_spdif, p_spdif->sysclk_freq, false);

		if (!IS_ERR_OR_NULL(p_spdif->regulator_vcc5v))
			ret = regulator_enable(p_spdif->regulator_vcc5v);
		if (ret)
			dev_err(p_spdif->dev, "regulator spdif5v enable failed:   %d\n", ret);

		if (!IS_ERR_OR_NULL(p_spdif->regulator_vcc3v3))
			ret = regulator_enable(p_spdif->regulator_vcc3v3);
		if (ret)
			dev_err(p_spdif->dev, "regulator spdif3v3 enable failed:   %d\n", ret);
	}
	if (!IS_ERR_OR_NULL(p_spdif->pin_ctl)) {
		state = pinctrl_lookup_state
		(p_spdif->pin_ctl, "spdif_pins");
		if (!IS_ERR_OR_NULL(state))
			pinctrl_select_state(p_spdif->pin_ctl, state);
	}

	aml_spdif_enable(p_spdif->actrl, stream, p_spdif->id, true);

	return 0;
}

static int aml_spdif_platform_resume(struct platform_device *pdev)
{
	struct aml_spdif *p_spdif = dev_get_drvdata(&pdev->dev);

	aml_spdif_resume(p_spdif);
	pr_info("%s is unmute\n", __func__);

	return 0;
}

static void aml_spdif_platform_shutdown(struct platform_device *pdev)
{
	struct aml_spdif *p_spdif = dev_get_drvdata(&pdev->dev);
	struct pinctrl_state *pstate = NULL;
	int stream = SNDRV_PCM_STREAM_PLAYBACK;

	if (!IS_ERR_OR_NULL(p_spdif->pin_ctl)) {
		pstate = pinctrl_lookup_state
		(p_spdif->pin_ctl, "spdif_pins_mute");
		if (!IS_ERR_OR_NULL(pstate))
			pinctrl_select_state(p_spdif->pin_ctl, pstate);
	}
	aml_spdif_enable(p_spdif->actrl, stream, p_spdif->id, false);

	if (!IS_ERR_OR_NULL(p_spdif->regulator_vcc5v))
		regulator_disable(p_spdif->regulator_vcc5v);
	if (!IS_ERR_OR_NULL(p_spdif->regulator_vcc3v3))
		regulator_disable(p_spdif->regulator_vcc3v3);

	pr_info("%s is mute\n", __func__);
}

static int spdif_format_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);

	if (!p_spdif)
		return -1;

	ucontrol->value.enumerated.item[0] = p_spdif->codec_type;

	return 0;
}

static int spdif_format_set_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);
	int index = ucontrol->value.enumerated.item[0];

	if (!p_spdif)
		return -1;

	if (index >= 10) {
		pr_err("bad parameter for spdif format set\n");
		return -1;
	}
	p_spdif->codec_type = index;

	return 0;
}

static int aml_audio_get_spdif_mute(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = p_spdif->mute;

	return 0;
}

static int aml_audio_set_spdif_mute(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);
	struct pinctrl_state *state = NULL;
	bool mute = !!ucontrol->value.integer.value[0];

	if (IS_ERR_OR_NULL(p_spdif->pin_ctl)) {
		pr_err("%s(), no pinctrl", __func__);
		return 0;
	}
	if (mute) {
		state = pinctrl_lookup_state
			(p_spdif->pin_ctl, "spdif_pins_mute");

		if (!IS_ERR_OR_NULL(state))
			pinctrl_select_state(p_spdif->pin_ctl, state);
	} else {
		state = pinctrl_lookup_state
			(p_spdif->pin_ctl, "spdif_pins");

		if (!IS_ERR_OR_NULL(state))
			pinctrl_select_state(p_spdif->pin_ctl, state);
	}

	p_spdif->mute = mute;

	return 0;
}

static const char *const spdifin_src_texts[] = {
	"spdifin pad", "spdifout", "N/A", "HDMIRX"
};

const struct soc_enum spdifin_src_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(spdifin_src_texts),
	spdifin_src_texts);

int spdifin_source_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = p_spdif->spdifin_src;

	return 0;
}

int spdifin_source_set_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);
	int src = ucontrol->value.enumerated.item[0];

	if (src > 3) {
		pr_err("bad parameter for spdifin src set\n");
		return -1;
	}
	spdifin_set_src(src);
	p_spdif->spdifin_src = src;

	return 0;
}

int spdif_set_audio_clk(int id, struct clk *clk_src,
		int rate, int same, bool tune)
{
	int ret = 0;

	if (spdif_priv[id]->on && same) {
		pr_debug("spdif priority");
		return 0;
	}

	if (rate == 0)
		return 0;

	clk_set_parent(spdif_priv[id]->clk_spdifout, clk_src);
	if (!tune)
		clk_set_rate(spdif_priv[id]->clk_spdifout, rate);

	ret = clk_prepare_enable(spdif_priv[id]->clk_spdifout);
	if (ret) {
		pr_err("%s Can't enable clk_spdifout clock, ret %d\n",
		__func__, ret);
	}
	return 0;
}

static int spdif_clk_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] =
			clk_get_rate(p_spdif->clk_spdifout);
	return 0;
}

static int spdif_clk_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);
	int sysclk = p_spdif->sysclk_freq;
	int value = ucontrol->value.enumerated.item[0];

	if (value > 2000000 || value < 0) {
		pr_err("Fine spdif sysclk setting range(0~2000000), %d\n",
				value);
		return 0;
	}
	value = value - 1000000;
	sysclk += value;

	aml_set_spdifclk(p_spdif, sysclk, true);

	return 0;
}

static int spdif_clk_ppm_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);
	int sysclk = p_spdif->standard_sysclk;
	int value = ucontrol->value.enumerated.item[0];

	if (value > 2000 || value < 0) {
		pr_err("Fine spdif sysclk ppm tuning range(0~2000), %d\n", value);
		return 0;
	}
	sysclk = sysclk + sysclk * (value - 1000) / 1000000;

	aml_set_spdifclk(p_spdif, sysclk, true);

	return 0;
}

static int spdif_get_cs(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);
	int id = p_spdif->id;

	ucontrol->value.integer.value[0] = spdif_get_channel_status0(id);
	return 0;
}

#define SPDIF_CS_L_SRC   0x1

/*
 * func: set spdif channels status
 * kcontrol value: lower 16 bits are config masks, higher 16 bits are
 * corrisponding config value.
 */
static int spdif_set_cs(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);
	int value = ucontrol->value.integer.value[0];
	int mask = value & 0xffff;
	int val = value >> 16;
	int status0 = spdif_get_channel_status0(p_spdif->id);

	/* L src bit */
	if (mask & SPDIF_CS_L_SRC) {
		if (val & SPDIF_CS_L_SRC) {
			status0 |= (IEC958_AES1_CON_ORIGINAL << 8);
			p_spdif->l_src = 1;
		} else {
			status0 &= ~(IEC958_AES1_CON_ORIGINAL << 8);
			p_spdif->l_src = 0;
		}
	}
	pr_info("%s(), status0=%#x\n", __func__, status0);
	spdif_set_channel_status0(p_spdif->id, status0);
	return 0;
}

static const struct snd_kcontrol_new snd_spdif_controls[] = {
	SOC_ENUM_EXT("SPDIFIN audio samplerate",
				spdifin_sample_rate_enum,
				spdifin_samplerate_get_enum,
				NULL),

	SOC_ENUM_EXT("SPDIFIN Audio Type",
				spdif_audio_type_enum,
				spdifin_audio_type_get_enum,
				NULL),

	SOC_ENUM_EXT("Audio spdif format",
				aud_codec_type_enum,
				spdif_format_get_enum,
				spdif_format_set_enum),

	SOC_SINGLE_BOOL_EXT("Audio spdif mute",
				0, aml_audio_get_spdif_mute,
				aml_audio_set_spdif_mute),

	SOC_ENUM_EXT("Audio spdifin source",
				spdifin_src_enum,
				spdifin_source_get_enum,
				spdifin_source_set_enum),

#ifdef CONFIG_AMLOGIC_HDMITX
	SOC_SINGLE_BOOL_EXT("Audio hdmi-out mute",
				0, aml_get_hdmi_out_audio,
				aml_set_hdmi_out_audio),
#endif

	SOC_SINGLE_EXT("spdif out channel status",
			0, 0, 0xffffffff, 0,
			spdif_get_cs,
			spdif_set_cs),
};

static const struct snd_kcontrol_new snd_spdif_clk_controls[] = {
	SOC_SINGLE_EXT("SPDIF CLK Fine Setting",
				0, 0, 2000000, 0,
				spdif_clk_get,
				spdif_clk_set),
	SOC_SINGLE_EXT("SPDIF CLK Fine PPM Tuning",
				0, 0, 2000, 0,
				spdif_clk_get,
				spdif_clk_ppm_set),
};

static int spdif_b_format_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = p_spdif->codec_type;
	return 0;
}

static int spdif_b_format_set_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);
	int index = ucontrol->value.enumerated.item[0];

	if (index > HIGH_SR_STEREO_LPCM) {
		pr_err("bad parameter for spdif format set\n");
		return -1;
	}
	p_spdif->codec_type = index;
	return 0;
}

static const struct snd_kcontrol_new snd_spdif_b_controls[] = {
	SOC_SINGLE_BOOL_EXT("Audio spdif_b mute",
				0, aml_audio_get_spdif_mute,
				aml_audio_set_spdif_mute),
	SOC_ENUM_EXT("Audio spdif_b format",
		     aud_codec_type_enum,
		     spdif_b_format_get_enum,
		     spdif_b_format_set_enum),
	SOC_SINGLE_EXT("SPDIF_B CLK Fine Setting",
		       0, 0, 2000000, 0,
		       spdif_clk_get, spdif_clk_set),
	SOC_SINGLE_EXT("SPDIF_B CLK Fine PPM Tuning",
		       0, 0, 2000, 0,
		       spdif_clk_get, spdif_clk_ppm_set),
	SOC_SINGLE_EXT("spdif_b out channel status",
			0, 0, 0xffffffff, 0,
			spdif_get_cs,
			spdif_set_cs),
};

#define SPDIFIN_ERR_CNT 100
static void spdifin_status_event(struct aml_spdif *p_spdif)
{
	int intrpt_status;

	if (!p_spdif)
		return;

	/* interrupt status, check and clear by reg_clk_interrupt */
	intrpt_status = aml_spdifin_status_check(p_spdif->actrl);

	/* clear irq bits immediately */
	aml_spdifin_clr_irq(p_spdif->actrl,
			p_spdif->chipinfo->clr_irq_all_bits,
			intrpt_status & 0xff);

	if (intrpt_status & 0x1)
		pr_info("over flow!!\n");
	if (intrpt_status & 0x2)
		pr_info("parity error\n");

	if (intrpt_status & 0x4) {
		int mode = (intrpt_status >> 28) & 0x7;

		pr_debug("sample rate, mode:%x\n", mode);
		if (mode == 0x7 || (((intrpt_status >> 18) & 0x3ff) == 0x3ff)) {
			p_spdif->in_err_cnt++;
			if (p_spdif->in_err_cnt > SPDIFIN_ERR_CNT) {
				pr_err("Not detect sample rate, spdifin may be disconnected\n");
				p_spdif->in_err_cnt = 0;
			}
			extcon_set_state(p_spdif->edev,
				EXTCON_SPDIFIN_SAMPLERATE, 0);
		} else if (mode >= 0) {
			if (p_spdif->last_sample_rate_mode != mode) {
				pr_info("Event: EXTCON_SPDIFIN_SAMPLERATE, new sample rate:%s\n",
					spdifin_samplerate[mode + 1]);
				extcon_set_state(p_spdif->edev,
					EXTCON_SPDIFIN_SAMPLERATE, 1);
			}
		}
		p_spdif->last_sample_rate_mode = mode;

	}

	if (p_spdif->chipinfo->pcpd_separated) {
		if (intrpt_status & 0x8) {
			pr_debug("Pc changed, try to read spdifin audio type\n");

			extcon_set_state(p_spdif->edev,
				EXTCON_SPDIFIN_AUDIOTYPE, 1);

		}
		if (intrpt_status & 0x10)
			pr_debug("Pd changed\n");
	} else {
		if (intrpt_status & 0x8)
			pr_debug("CH status changed\n");

		if (intrpt_status & 0x10) {
			int val = spdifin_get_ch_status0to31();
			int pc_v = (val >> 16) & 0xffff;
			int pd_v = val & 0xffff;

			if (pc_v != p_spdif->pc_last) {
				p_spdif->pc_last = pc_v;
				pr_info("Pc changed\n");
			}
			if (pd_v != p_spdif->pd_last) {
				p_spdif->pd_last = pd_v;
				pr_info("Pd changed\n");
			}
		}
	}

	if (intrpt_status & 0x20) {
		pr_info("nonpcm to pcm\n");
		extcon_set_state(p_spdif->edev,
			EXTCON_SPDIFIN_AUDIOTYPE, 0);
	}

	if (intrpt_status & 0x40)
		pr_info("valid changed\n");
}

static irqreturn_t aml_spdif_ddr_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)devid;

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

/* detect PCM/RAW and sample changes by the source */
static irqreturn_t aml_spdifin_status_isr(int irq, void *devid)
{
	struct aml_spdif *p_spdif = (struct aml_spdif *)devid;

	spdifin_status_event(p_spdif);

	return IRQ_HANDLED;
}

static int release_spdif_same_src(struct aml_spdif *p_spdif,
		struct snd_pcm_substream *substream)
{
	int samesource_sel = p_spdif->id + SHAREBUFFER_SPDIFA;
	struct samesrc_ops *ops = get_samesrc_ops(samesource_sel);

	if (!ops || !ops->fr || !ops->hw_free)
		return 0;

	pr_info("%s(), %d src sel %d\n", __func__, __LINE__, samesource_sel);
	ops->hw_free(substream, ops->fr, samesource_sel, ops->share_lvl);

	return 0;
}

static int aml_spdif_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->cpu_dai->dev;
	struct aml_spdif *p_spdif;
	int ret = 0;

	p_spdif = (struct aml_spdif *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(substream, &aml_spdif_hardware);
	snd_pcm_lib_preallocate_pages(substream, SNDRV_DMA_TYPE_DEV,
		dev, SPDIF_BUFFER_BYTES / 2, SPDIF_BUFFER_BYTES);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_spdif->on = true;

		if (p_spdif->same_src_on)
			release_spdif_same_src(p_spdif, substream);

		p_spdif->fddr = aml_audio_register_frddr(dev,
			aml_spdif_ddr_isr, substream, false);
		if (!p_spdif->fddr) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim from ddr\n");
			goto err_ddr;
		}
	} else {
		p_spdif->tddr = aml_audio_register_toddr(dev,
			aml_spdif_ddr_isr, substream);
		if (!p_spdif->tddr) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim to ddr\n");
			goto err_ddr;
		}

		ret = request_irq(p_spdif->irq_spdifin,
				aml_spdifin_status_isr, 0, "irq_spdifin",
				p_spdif);
		if (ret) {
			dev_err(p_spdif->dev, "failed to claim irq_spdifin %u, ret: %d\n",
						p_spdif->irq_spdifin, ret);
			goto err_irq;
		}
	}

	runtime->private_data = p_spdif;

	return 0;

err_irq:
	aml_audio_unregister_toddr(p_spdif->dev, substream);
err_ddr:
	snd_pcm_lib_preallocate_free(substream);
	return ret;
}

static int aml_spdif_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_spdif *p_spdif = runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_spdif->on = false;
		aml_audio_unregister_frddr(p_spdif->dev, substream);
	} else {
		aml_audio_unregister_toddr(p_spdif->dev, substream);
		free_irq(p_spdif->irq_spdifin, p_spdif);

		/* clear extcon status */
		if (p_spdif->id == 0) {
			extcon_set_state(p_spdif->edev,
				EXTCON_SPDIFIN_SAMPLERATE, 0);

			extcon_set_state(p_spdif->edev,
				EXTCON_SPDIFIN_AUDIOTYPE, 0);
		}
	}

	runtime->private_data = NULL;
	snd_pcm_lib_preallocate_free(substream);

	return 0;
}

static int aml_spdif_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int aml_spdif_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int aml_spdif_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_spdif *p_spdif = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;
	unsigned int period, threshold;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - FIFO_BURST;
	period	 = frames_to_bytes(runtime, runtime->period_size);
	int_addr = period / FIFO_BURST;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_spdif->fddr;

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
		struct toddr *to = p_spdif->tddr;

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

static snd_pcm_uframes_t aml_spdif_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_spdif *p_spdif = runtime->private_data;
	unsigned int addr, start_addr;
	snd_pcm_uframes_t frames;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_spdif->fddr);
	else
		addr = aml_toddr_get_position(p_spdif->tddr);

	frames = bytes_to_frames(runtime, addr - start_addr);
	if (frames > runtime->buffer_size)
		frames = 0;

	return frames;
}

static int aml_spdif_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops aml_spdif_ops = {
	.open      = aml_spdif_open,
	.close     = aml_spdif_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = aml_spdif_hw_params,
	.hw_free   = aml_spdif_hw_free,
	.prepare   = aml_spdif_prepare,
	.pointer   = aml_spdif_pointer,
	.mmap      = aml_spdif_mmap,
};

static int aml_spdif_new(struct snd_soc_pcm_runtime *rtd)
{
	struct device *dev = rtd->cpu_dai->dev;
	struct aml_spdif *p_spdif;

	p_spdif = (struct aml_spdif *)dev_get_drvdata(dev);

	pr_debug("%s spdif_%s, clk continuous:%d\n",
		__func__,
		(p_spdif->id == 0) ? "a" : "b",
		p_spdif->clk_cont);

	/* keep frddr when probe, after spdif_frddr_init done
	 * frddr can be released, and spdif outputs zero data
	 * without frddr used.
	 */
	if (p_spdif->clk_cont)
		spdifout_play_with_zerodata_free(p_spdif->id);

	return 0;
}

static int aml_spdif_component_probe(struct snd_soc_component *component)
{
	struct aml_spdif *p_spdif = snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (p_spdif->clk_tuning_enable == 1) {
		ret = snd_soc_add_component_controls
				(component,
				snd_spdif_clk_controls,
				ARRAY_SIZE(snd_spdif_clk_controls));
		if (ret < 0)
			pr_err("%s, failed add snd spdif clk controls\n",
				__func__);
	}

	return 0;
}

static int aml_dai_spdif_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);
	int ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* enable clock gate */
		ret = clk_prepare_enable(p_spdif->gate_spdifout);

		/* clock parent */
		ret = clk_set_parent(p_spdif->clk_spdifout, p_spdif->sysclk);
		if (ret) {
			pr_err("Can't set clk_spdifout parent clock\n");
			ret = PTR_ERR(p_spdif->clk_spdifout);
			goto err;
		}

		/* enable clock */
		ret = clk_prepare_enable(p_spdif->sysclk);
		if (ret) {
			pr_err("Can't enable pcm sysclk clock: %d\n", ret);
			goto err;
		}
		ret = clk_prepare_enable(p_spdif->clk_spdifout);
		if (ret) {
			pr_err("Can't enable pcm clk_spdifout clock: %d\n",
				ret);
			goto err;
		}
	} else {
		/* enable clock gate */
		ret = clk_prepare_enable(p_spdif->gate_spdifin);

		/* clock parent */
		ret = clk_set_parent(p_spdif->clk_spdifin, p_spdif->fixed_clk);
		if (ret) {
			pr_err("Can't set clk_spdifin parent clock\n");
			ret = PTR_ERR(p_spdif->clk_spdifin);
			goto err;
		}

		/* enable clock */
		ret = clk_prepare_enable(p_spdif->fixed_clk);
		if (ret) {
			pr_err("Can't enable pcm fixed_clk clock: %d\n", ret);
			goto err;
		}
		ret = clk_prepare_enable(p_spdif->clk_spdifin);
		if (ret) {
			pr_err("Can't enable pcm clk_spdifin clock: %d\n", ret);
			goto err;
		}
	}

	return 0;
err:
	pr_err("failed enable clock\n");
	return -EINVAL;
}

static void aml_dai_spdif_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_soc_card *card = cpu_dai->component->card;

	/* disable clock and gate */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (get_hdmitx_audio_src(card) == p_spdif->id)
			notify_hdmitx_to_prepare();

		if (p_spdif->clk_cont) {
			pr_info("spdif_%s keep clk continuous\n",
				(p_spdif->id == 0) ? "a" : "b");
			return;
		}
	}
}

static int aml_dai_spdif_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bit_depth = 0;
	unsigned int fifo_id = 0;
	int separated = 0;

	bit_depth = snd_pcm_format_width(runtime->format);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_spdif->fddr;
		enum frddr_dest dst;
		struct iec958_chsts chsts;
		unsigned int l_src = 0;

		switch (p_spdif->id) {
		case 0:
			dst = SPDIFOUT_A;
			break;
		case 1:
			dst = SPDIFOUT_B;
			break;
		default:
			dev_err(p_spdif->dev, "invalid id: %d\n", p_spdif->id);
			return -EINVAL;
		}

		fifo_id = aml_frddr_get_fifo_id(fr);
		aml_frddr_set_format(fr,
			runtime->channels,
			runtime->rate,
			bit_depth - 1,
			spdifout_get_frddr_type(bit_depth));
		aml_frddr_select_dst(fr, dst);

		l_src = get_spdif_source_l_config(p_spdif->id);
		/* check channel status info, and set them */
		iec_get_channel_status_info(&chsts,
					    p_spdif->codec_type,
					    runtime->rate,
					    bit_depth,
					    l_src);
		spdif_set_channel_status_info(&chsts, p_spdif->id);
		if (codec_is_raw(p_spdif->codec_type))
			spdif_set_validity(1, p_spdif->id);
		else
			spdif_set_validity(0, p_spdif->id);

		/* TOHDMITX_CTRL0
		 * Both spdif_a/spdif_b would notify to hdmitx
		 */
		if (get_hdmitx_audio_src(rtd->card) == p_spdif->id) {
			separated = p_spdif->chipinfo->separate_tohdmitx_en;
			enable_spdifout_to_hdmitx(separated);
		}

		if (p_spdif->codec_type == AUD_CODEC_TYPE_TRUEHD ||
		    p_spdif->codec_type == AUD_CODEC_TYPE_DTS_HD) {
			aml_spdif_enable(p_spdif->actrl,
				substream->stream, p_spdif->id, false);
			if (p_spdif->samesource_sel != SHAREBUFFER_NONE)
				spdif_sharebuffer_trigger(p_spdif, runtime->channels,
							  SNDRV_PCM_TRIGGER_STOP);
			aml_spdif_mute(p_spdif->actrl,
				substream->stream, p_spdif->id, false);
		}

		if (get_hdmitx_audio_src(rtd->card) == p_spdif->id) {
			/* notify to hdmitx */
			spdif_notify_to_hdmitx(substream, p_spdif->codec_type);
		}
		if (p_spdif->samesource_sel != SHAREBUFFER_NONE)
			spdif_sharebuffer_prepare(substream, p_spdif);
	} else {
		struct toddr *to = p_spdif->tddr;
		struct toddr_fmt fmt;
		unsigned int msb, lsb, toddr_type;

		switch (bit_depth) {
		case 8:
		case 16:
			toddr_type = 0;
			break;
		case 24:
			toddr_type = 4;
			break;
		case 32:
			toddr_type = 3;
			break;
		default:
			dev_err(p_spdif->dev,
				"runtime format invalid bit_depth: %d\n",
				bit_depth);
			return -EINVAL;
		}

		msb = 28 - 1;
		lsb = (bit_depth <= 24) ? 28 - bit_depth : 4;

		if (get_resample_version() >= T5_RESAMPLE &&
		    get_resample_source(RESAMPLE_A) == SPDIFIN) {
			msb = 31;
			lsb = 32 - bit_depth;
		}
		// to ddr spdifin
		fmt.type       = toddr_type;
		fmt.msb        = msb;
		fmt.lsb        = lsb;
		fmt.endian     = 0;
		fmt.bit_depth  = bit_depth;
		fmt.ch_num     = runtime->channels;
		fmt.rate       = runtime->rate;
		aml_toddr_select_src(to, SPDIFIN);
		aml_toddr_set_format(to, &fmt);
#ifdef __SPDIFIN_INSERT_CHNUM__
		aml_toddr_insert_chanum(to);
#endif
	}

	aml_spdif_fifo_ctrl(p_spdif->actrl, bit_depth,
			substream->stream, p_spdif->id, fifo_id);

#ifdef __SPDIFIN_INSERT_CHNUM__
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		aml_spdifin_chnum_en(p_spdif->actrl,
			p_spdif->id, true);
#endif

	return 0;
}

static int aml_dai_spdif_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_pcm_runtime *runtime = substream->runtime;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* reset fifo */
		aml_spdif_fifo_reset(p_spdif->actrl,
			substream->stream,
			p_spdif->id);

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev,
				 "S/PDIF[%d] Playback enable\n",
				 p_spdif->id);
			aml_spdif_enable(p_spdif->actrl,
			    substream->stream, p_spdif->id, true);
			if (p_spdif->samesource_sel != SHAREBUFFER_NONE)
				spdif_sharebuffer_trigger(p_spdif, runtime->channels, cmd);

			aml_frddr_enable(p_spdif->fddr, 1);
			udelay(100);
			aml_spdif_mute(p_spdif->actrl,
				substream->stream, p_spdif->id, false);
			if (p_spdif->samesource_sel != SHAREBUFFER_NONE)
				spdif_sharebuffer_mute(p_spdif, false);
		} else {
			struct snd_soc_card *card = cpu_dai->component->card;

			dev_info(substream->pcm->card->dev,
				 "S/PDIF[%d] Capture enable\n",
				 p_spdif->id);
			aml_toddr_enable(p_spdif->tddr, 1);
			aml_spdif_enable(p_spdif->actrl,
			    substream->stream, p_spdif->id, true);

			locker_reset(aml_get_card_locker(card));
		}

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev,
				 "S/PDIF[%d] Playback disable\n",
				 p_spdif->id);
			/* continuous-clock, spdif out is not disable,
			 * only mute, ensure spdif outputs zero data.
			 */
			if (p_spdif->clk_cont) {
				aml_spdif_mute(p_spdif->actrl,
					substream->stream, p_spdif->id, true);
				if (p_spdif->samesource_sel != SHAREBUFFER_NONE)
					spdif_sharebuffer_mute(p_spdif, true);
			} else {
				aml_spdif_enable(p_spdif->actrl,
					substream->stream, p_spdif->id, false);
				if (p_spdif->samesource_sel != SHAREBUFFER_NONE)
					spdif_sharebuffer_trigger(p_spdif, runtime->channels, cmd);
			}

			if (p_spdif->chipinfo->async_fifo)
				aml_frddr_check(p_spdif->fddr);
			aml_frddr_enable(p_spdif->fddr, 0);
		} else {
			bool toddr_stopped = false;

			aml_spdif_enable(p_spdif->actrl,
					substream->stream, p_spdif->id, false);
			dev_info(substream->pcm->card->dev,
				 "S/PDIF[%d] Capture disable\n",
				 p_spdif->id);

			toddr_stopped = aml_toddr_burst_finished(p_spdif->tddr);
			if (toddr_stopped)
				aml_toddr_enable(p_spdif->tddr, false);
			else
				pr_err("%s(), toddr may be stuck\n", __func__);
		}

		break;
	default:
		return -EINVAL;
	}

	return 0;
}
static int aml_dai_spdif_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_soc_card *card = cpu_dai->component->card;
	struct soft_locker *locker = aml_get_card_locker(card);
	unsigned int rate = params_rate(params);
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_spdif->fddr;

		fr->buf_frames = params_buffer_size(params);
		fr->frame_size = snd_soc_params_to_frame_size(params) / 8;
		fr->rate = rate;
		locker_register_frddr(locker, fr, cpu_dai->name);

		rate *= 128;
		if (get_hdmitx_audio_src(card) == p_spdif->id) {
			/* notify HDMITX to disable audio packet */
			notify_hdmitx_to_prepare();
			set_spdif_to_hdmitx_id(p_spdif->id);
		}

		snd_soc_dai_set_sysclk(cpu_dai,
				0, rate, SND_SOC_CLOCK_OUT);
	} else {
		struct toddr *to = p_spdif->tddr;

		to->buf_frames = params_buffer_size(params);
		to->frame_size = snd_soc_params_to_frame_size(params) / 8;
		to->rate = rate;
		locker_register_toddr(locker, to, cpu_dai->name);

		clk_set_rate(p_spdif->clk_spdifin, 500000000);
	}

	locker_en_ddr_by_dai_name(locker,
			cpu_dai->name, substream->stream);

	return ret;
}

static int aml_dai_spdif_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *cpu_dai)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_soc_card *card = cpu_dai->component->card;
	struct soft_locker *locker = aml_get_card_locker(card);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (p_spdif->samesource_sel != SHAREBUFFER_NONE)
			spdif_sharebuffer_free(p_spdif, substream);

		locker_release_frddr(locker, cpu_dai->name);
	} else {
		locker_release_toddr(locker, cpu_dai->name);
	}

	locker_en_ddr_by_dai_name(locker, cpu_dai->name, substream->stream);

	return 0;
}

#define MPLL_HBR_FIXED_FREQ   (491520000)
#define MPLL_CD_FIXED_FREQ    (451584000)

static void aml_set_spdifclk_s4(struct aml_spdif *p_spdif,
		int freq, bool tune)
{
	bool force_mpll = is_force_mpll_clk();

	if (freq == 0) {
		dev_err(p_spdif->dev, "%s(), clk 0 err\n", __func__);
		return;
	}

	pr_info("%s: spdifid %d, force_mpll = %d, freq = %d\n",
		__func__, p_spdif->id, force_mpll, freq);
	if (IS_ERR(p_spdif->clk_src_cd)) {
		pr_err("%s: please make sure S4 DTS support 2 clk source\n", __func__);
		return;
	}

	if (force_mpll) {
		int ratio = 0;

		if (p_spdif->standard_sysclk % 8000 == 0) {
			ratio = MPLL_HBR_FIXED_FREQ / p_spdif->standard_sysclk;
		} else if (p_spdif->standard_sysclk % 11025 == 0) {
			ratio = MPLL_CD_FIXED_FREQ / p_spdif->standard_sysclk;
		} else {
			dev_warn(p_spdif->dev, "unsupport clock rate %d\n",
				p_spdif->standard_sysclk);
			return;
		}

	        clk_set_rate(p_spdif->clk_src_cd, freq * ratio);
		spdif_set_audio_clk(p_spdif->id,
			p_spdif->clk_src_cd,
			freq, 0, tune);
	} else {
		if (p_spdif->standard_sysclk % 8000 == 0) {
			if (p_spdif->syssrc_clk_rate)
				clk_set_rate(p_spdif->sysclk, p_spdif->syssrc_clk_rate);
			else
				pr_warn("%s(), DTS miss hifi clk rate config", __func__);

			spdif_set_audio_clk(p_spdif->id,
				p_spdif->sysclk,
				freq, 0, tune);
		} else if (p_spdif->standard_sysclk % 11025 == 0) {
			clk_set_rate(p_spdif->clk_src_cd, MPLL_CD_FIXED_FREQ);

			spdif_set_audio_clk(p_spdif->id,
				p_spdif->clk_src_cd,
				freq, 0, tune);
		} else {
			dev_warn(p_spdif->dev, "unsupport clock rate %d\n",
				p_spdif->standard_sysclk);
		}
	}
}

/* normal 1 audio clk src both for 44.1k and 48k */
static void aml_set_spdifclk_1(struct aml_spdif *p_spdif, int freq, bool tune)
{
	unsigned int mpll_freq = 0;

	if (freq == 0) {
		dev_err(p_spdif->dev, "%s(), clk 0 err\n", __func__);
		return;
	}

	pr_info("%s: spdifid %d, freq = %d\n", __func__, p_spdif->id, freq);

	if (freq) {
		char *clk_name = (char *)__clk_get_name(p_spdif->sysclk);

		mpll_freq = freq *
				mpll2sys_clk_ratio_by_type(p_spdif->codec_type);
		/* make sure mpll_freq doesn't exceed MPLL max freq */
		while (mpll_freq > AML_MPLL_FREQ_MAX)
			mpll_freq = mpll_freq >> 1;

		if (!strcmp(clk_name, "hifi_pll") || !strcmp(clk_name, "t5_hifi_pll")) {
			if (p_spdif->syssrc_clk_rate)
				clk_set_rate(p_spdif->sysclk,
					p_spdif->syssrc_clk_rate);
			else
				clk_set_rate(p_spdif->sysclk, 1806336 * 1000);
		} else {
			clk_set_rate(p_spdif->sysclk, mpll_freq);
		}
		/*
		 * clk_set_rate(p_spdif->clk_spdifout, p_spdif->sysclk_freq);
		 */
		spdif_set_audio_clk(p_spdif->id,
			p_spdif->sysclk,
			freq, 0, tune);
		pr_debug("\t set spdifout clk:%d, mpll:%d\n",
			freq,
			mpll_freq);
		pr_debug("\t get spdifout clk:%lu, mpll:%lu\n",
			clk_get_rate(p_spdif->clk_spdifout),
			clk_get_rate(p_spdif->sysclk));
	}
}

/* 2 audio clk src each for 44.1k and 48k */
static void aml_set_spdifclk_2(struct aml_spdif *p_spdif, int freq, bool tune)
{
	int ratio = 0;

	if (freq == 0) {
		dev_err(p_spdif->dev, "%s(), clk 0 err\n", __func__);
		return;
	}

	pr_info("%s: spdifid %d, freq = %d, tune = %d\n",
			__func__, p_spdif->id, freq, tune);

	if (p_spdif->standard_sysclk % 8000 == 0) {
		ratio = MPLL_HBR_FIXED_FREQ / p_spdif->standard_sysclk;
		clk_set_rate(p_spdif->sysclk, freq * ratio);

		spdif_set_audio_clk(p_spdif->id,
			p_spdif->sysclk,
			freq, 0, tune);
	} else if (p_spdif->standard_sysclk % 11025 == 0) {
		ratio = MPLL_CD_FIXED_FREQ / p_spdif->standard_sysclk;
		clk_set_rate(p_spdif->clk_src_cd, freq * ratio);

		spdif_set_audio_clk(p_spdif->id,
			p_spdif->clk_src_cd,
			freq, 0, tune);
	} else {
		dev_warn(p_spdif->dev, "unsupport clock rate %d\n", p_spdif->standard_sysclk);
	}
}

static void aml_set_spdifclk_normal(struct aml_spdif *p_spdif,
		int freq, bool tune)
{
	if (IS_ERR(p_spdif->clk_src_cd))
		aml_set_spdifclk_1(p_spdif, freq, tune);
	else
		aml_set_spdifclk_2(p_spdif, freq, tune);
}

static void aml_set_spdifclk(struct aml_spdif *p_spdif, int freq, bool tune)
{
	char *clk_name = (char *)__clk_get_name(p_spdif->sysclk);

	/* S4: audio share HIFI clk with EMMC, need special method */
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_S4 && !strcmp(clk_name, "hifi_pll"))
		aml_set_spdifclk_s4(p_spdif, freq, tune);
	else
		aml_set_spdifclk_normal(p_spdif, freq, tune);
}

static int aml_dai_set_spdif_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct aml_spdif *p_spdif = snd_soc_dai_get_drvdata(cpu_dai);

	if (raw_is_4x_clk(p_spdif->codec_type)) {
		pr_debug("set 4x audio clk for 958\n");
		freq *= 4;
	} else {
		pr_debug("%s() set normal 512 fs /4 fs\n", __func__);
	}

	p_spdif->sysclk_freq = freq;
	p_spdif->standard_sysclk = freq;
	aml_set_spdifclk(p_spdif, freq, false);

	return 0;
}

static struct snd_soc_dai_ops aml_dai_spdif_ops = {
	.startup = aml_dai_spdif_startup,
	.shutdown = aml_dai_spdif_shutdown,
	.prepare = aml_dai_spdif_prepare,
	.trigger = aml_dai_spdif_trigger,
	.hw_params = aml_dai_spdif_hw_params,
	.hw_free   = aml_dai_spdif_hw_free,
	.set_sysclk = aml_dai_set_spdif_sysclk,
};

#define AML_DAI_SPDIF_RATES	(SNDRV_PCM_RATE_8000_192000)
#define AML_DAI_SPDIF_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE |\
				 SNDRV_PCM_FMTBIT_S24_LE |\
				 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver aml_spdif_dai[] = {
	{
		.name = "SPDIF-A",
		.id = 1,
		.playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = AML_DAI_SPDIF_RATES,
		      .formats = AML_DAI_SPDIF_FORMATS,
		},
		.capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = AML_DAI_SPDIF_RATES,
		     .formats = AML_DAI_SPDIF_FORMATS,
		},
		.ops = &aml_dai_spdif_ops,
	},
	{
		.name = "SPDIF-B",
		.id = 2,
		.playback = {
			  .channels_min = 1,
			  .channels_max = 2,
			  .rates = AML_DAI_SPDIF_RATES,
			  .formats = AML_DAI_SPDIF_FORMATS,
		},
		.ops = &aml_dai_spdif_ops,
	}
};

static const struct snd_soc_component_driver aml_spdif_component[] = {
	{
		.name			= "SPDIF",
		.controls		= snd_spdif_controls,
		.num_controls	= ARRAY_SIZE(snd_spdif_controls),
		.probe			= aml_spdif_component_probe,
		.pcm_new    = aml_spdif_new,
		.ops        = &aml_spdif_ops,
	},
	{
		.name			= "SPDIF-B",
		.controls		= snd_spdif_b_controls,
		.num_controls	= ARRAY_SIZE(snd_spdif_b_controls),
		.probe			= aml_spdif_component_probe,
		.pcm_new    = aml_spdif_new,
		.ops        = &aml_spdif_ops,
	}
};

static int aml_spdif_parse_of(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aml_spdif *p_spdif = dev_get_drvdata(dev);
	int ret = 0, ss = 0;

	ret = of_property_read_u32(dev->of_node, "src-clk-freq",
				   &p_spdif->syssrc_clk_rate);
	if (ret < 0)
		p_spdif->syssrc_clk_rate = 0;
	else
		pr_info("%s sys-src clk rate from dts:%d\n",
			__func__, p_spdif->syssrc_clk_rate);

	ret = of_property_read_u32(dev->of_node, "samesource_sel",
			&ss);
	if (ret < 0)
		p_spdif->samesource_sel = SHAREBUFFER_NONE;
	else
		p_spdif->samesource_sel = ss;

	/* clock for spdif in */
	if (p_spdif->id == 0) {
		/* clock gate */
		p_spdif->gate_spdifin = devm_clk_get(dev, "gate_spdifin");
		if (IS_ERR(p_spdif->gate_spdifin)) {
			dev_err(dev, "Can't get spdifin gate\n");
			return PTR_ERR(p_spdif->gate_spdifin);
		}
		/* pll */
		p_spdif->fixed_clk = devm_clk_get(dev, "fixed_clk");
		if (IS_ERR(p_spdif->fixed_clk)) {
			dev_err(dev, "Can't retrieve fixed_clk\n");
			return PTR_ERR(p_spdif->fixed_clk);
		}
		/* spdif in clk */
		p_spdif->clk_spdifin = devm_clk_get(dev, "clk_spdifin");
		if (IS_ERR(p_spdif->clk_spdifin)) {
			dev_err(dev, "Can't retrieve spdifin clock\n");
			return PTR_ERR(p_spdif->clk_spdifin);
		}
		ret = clk_set_parent(p_spdif->clk_spdifin, p_spdif->fixed_clk);
		if (ret) {
			dev_err(dev,
				"Can't set clk_spdifin parent clock\n");
			ret = PTR_ERR(p_spdif->clk_spdifin);
			return ret;
		}

		/* irqs */
		p_spdif->irq_spdifin =
			platform_get_irq_byname(pdev, "irq_spdifin");
		if (p_spdif->irq_spdifin < 0)
			dev_err(dev, "platform_get_irq_byname failed\n");

		/* spdifin sample rate change event */
		p_spdif->edev = devm_extcon_dev_allocate(dev, spdifin_extcon);
		if (IS_ERR(p_spdif->edev)) {
			pr_err("failed to allocate spdifin extcon!!!\n");
			ret = -ENOMEM;
			return ret;
		}

		/*
		 * p_spdif->edev->dev.parent  = dev;
		 * p_spdif->edev->name = "spdifin_event";
		 * dev_set_name(&p_spdif->edev->dev, "spdifin_event");
		 */
		ret = extcon_dev_register(p_spdif->edev);
		if (ret < 0)
			pr_err("SPDIF IN extcon failed to register!!, ignore it\n");

		spdifa_ss_ops.private = p_spdif;
		register_samesrc_ops(SHAREBUFFER_SPDIFA, &spdifa_ss_ops);
	} else {
		spdifb_ss_ops.private = p_spdif;
		register_samesrc_ops(SHAREBUFFER_SPDIFB, &spdifb_ss_ops);
	}

	p_spdif->pin_ctl = devm_pinctrl_get_select(dev, "spdif_pins");
	if (IS_ERR(p_spdif->pin_ctl))
		dev_dbg(dev, "spdif %d has no pinctrl!\n", p_spdif->id);

	/* clock for spdif out */
	/* clock gate */
	p_spdif->gate_spdifout = devm_clk_get(dev, "gate_spdifout");
	if (IS_ERR(p_spdif->gate_spdifout)) {
		dev_err(dev, "Can't get spdifout gate\n");
		return PTR_ERR(p_spdif->gate_spdifout);
	}
	/* pll */
	p_spdif->sysclk = devm_clk_get(dev, "sysclk");
	if (IS_ERR(p_spdif->sysclk)) {
		dev_err(dev, "Can't retrieve sysclk clock\n");
		return PTR_ERR(p_spdif->sysclk);
	}
	/* spdif out clock */
	p_spdif->clk_spdifout = devm_clk_get(dev, "clk_spdifout");
	if (IS_ERR(p_spdif->clk_spdifout)) {
		dev_err(dev, "Can't retrieve spdifout clock\n");
		return PTR_ERR(p_spdif->clk_spdifout);
	}

	p_spdif->clk_src_cd = devm_clk_get(&pdev->dev, "clk_src_cd");
	if (IS_ERR(p_spdif->clk_src_cd))
		dev_warn(&pdev->dev, "no clk_src_cd clock for 44k case\n");

	ret = of_property_read_u32(pdev->dev.of_node,
				"clk_tuning_enable",
				&p_spdif->clk_tuning_enable);
	if (ret < 0)
		p_spdif->clk_tuning_enable = 0;
	else
		pr_info("Spdif id %d tuning clk enable:%d\n",
			p_spdif->id, p_spdif->clk_tuning_enable);

	return 0;
}

static int aml_spdif_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct aml_spdif *aml_spdif = NULL;
	struct spdif_chipinfo *p_spdif_chipinfo;
	int ret = 0;

	aml_spdif = devm_kzalloc(dev, sizeof(struct aml_spdif), GFP_KERNEL);
	if (!aml_spdif)
		return -ENOMEM;

	aml_spdif->dev = dev;
	dev_set_drvdata(dev, aml_spdif);

	/* match data */
	p_spdif_chipinfo = (struct spdif_chipinfo *)
		of_device_get_match_data(dev);
	if (p_spdif_chipinfo) {
		aml_spdif->id = p_spdif_chipinfo->id;
		/* for spdif output zero data, clk be continuous,
		 * and keep silence when no valid data
		 */
		aml_spdif->clk_cont = 1;

		aml_spdif->chipinfo = p_spdif_chipinfo;

		if (p_spdif_chipinfo->sample_mode_filter_en)
			aml_spdifin_sample_mode_filter_en();
	} else {
		dev_warn_once(dev,
			"check whether to update spdif chipinfo\n");
	}

	pr_debug("%s, spdif ID = %u\n", __func__, aml_spdif->id);

	if (aml_spdif->chipinfo->regulator) {
		aml_spdif->regulator_vcc3v3 = devm_regulator_get(dev, "spdif3v3");
		ret = PTR_ERR_OR_ZERO(aml_spdif->regulator_vcc3v3);
		if (ret) {
			if (ret == -EPROBE_DEFER) {
				dev_err(&pdev->dev, "regulator spdif3v3 not ready, retry\n");
				return ret;
			}
			dev_err(&pdev->dev, "failed in regulator spdif3v3 %ld\n",
				PTR_ERR(aml_spdif->regulator_vcc3v3));
			aml_spdif->regulator_vcc3v3 = NULL;
		} else {
			ret = regulator_enable(aml_spdif->regulator_vcc3v3);
			if (ret) {
				dev_err(&pdev->dev,
					"regulator spdif3v3 enable failed:   %d\n", ret);
				aml_spdif->regulator_vcc3v3 = NULL;
			}
		}
		aml_spdif->regulator_vcc5v = devm_regulator_get(dev, "spdif5v");
		ret = PTR_ERR_OR_ZERO(aml_spdif->regulator_vcc5v);
		if (ret) {
			if (ret == -EPROBE_DEFER) {
				dev_err(&pdev->dev, "regulator spdif5v not ready, retry\n");
				return ret;
			}
			dev_err(&pdev->dev, "failed in regulator spdif5v %ld\n",
				PTR_ERR(aml_spdif->regulator_vcc5v));
			aml_spdif->regulator_vcc5v = NULL;
		} else {
			ret = regulator_enable(aml_spdif->regulator_vcc5v);
			if (ret) {
				dev_err(&pdev->dev,
					"regulator spdif5v enable failed:   %d\n", ret);
				aml_spdif->regulator_vcc5v = NULL;
			}
		}
	}

	ret = of_property_read_u32(node, "suspend-clk-off",
			&aml_spdif->suspend_clk_off);
	if (ret < 0)
		dev_err(&pdev->dev, "Can't retrieve suspend-clk-off\n");

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (!node_prt)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	aml_spdif->actrl = actrl;

	ret = aml_spdif_parse_of(pdev);
	if (ret)
		return -EINVAL;

	ret = devm_snd_soc_register_component(dev,
			&aml_spdif_component[aml_spdif->id],
			&aml_spdif_dai[aml_spdif->id], 1);
	if (ret) {
		dev_err(dev, "devm_snd_soc_register_component failed\n");
		return ret;
	}
	spdif_priv[aml_spdif->id] = aml_spdif;

	return 0;
}

static int aml_spdif_pm_freeze(struct device *dev)
{
	return 0;
}

static int aml_spdif_pm_thaw(struct device *dev)
{
	return 0;
}

static int aml_spdif_pm_poweroff(struct device *dev)
{
	struct aml_spdif *p_spdif = dev_get_drvdata(dev);

	aml_spdif_suspend(p_spdif);
	pr_info("%s spdif:(%d)\n", __func__, p_spdif->id);
	return 0;
}

static int aml_spdif_pm_suspend(struct device *dev)
{
	struct aml_spdif *p_spdif = dev_get_drvdata(dev);

	aml_spdif_suspend(p_spdif);
	pr_info("%s spdif:(%d)\n", __func__, p_spdif->id);
	return 0;
}

static int aml_spdif_pm_resume(struct device *dev)
{
	struct aml_spdif *p_spdif = dev_get_drvdata(dev);

	aml_spdif_resume(p_spdif);
	pr_info("%s spdif:(%d)\n", __func__, p_spdif->id);
	return 0;
}

static int aml_spdif_pm_restore(struct device *dev)
{
	struct aml_spdif *p_spdif = dev_get_drvdata(dev);

	aml_spdif_resume(p_spdif);
	pr_info("%s spdif:(%d)\n", __func__, p_spdif->id);
	return 0;
}

const struct dev_pm_ops aml_spdif_pm_ops = {
	.freeze = aml_spdif_pm_freeze,
	.thaw = aml_spdif_pm_thaw,
	.poweroff = aml_spdif_pm_poweroff,
	.restore = aml_spdif_pm_restore,
	.suspend = aml_spdif_pm_suspend,
	.resume = aml_spdif_pm_resume,
};

struct platform_driver aml_spdif_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = aml_spdif_device_id,
		.pm = &aml_spdif_pm_ops,
	},
	.probe = aml_spdif_platform_probe,
	.suspend = aml_spdif_platform_suspend,
	.resume  = aml_spdif_platform_resume,
	.shutdown = aml_spdif_platform_shutdown,
};

int __init spdif_init(void)
{
	return platform_driver_register(&aml_spdif_driver);
}

void __exit spdif_exit(void)
{
	platform_driver_unregister(&aml_spdif_driver);
}

#ifndef MODULE
module_init(spdif_init);
module_exit(spdif_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic S/PDIF ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, aml_spdif_device_id);
#endif
