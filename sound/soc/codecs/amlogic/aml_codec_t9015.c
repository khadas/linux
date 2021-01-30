// SPDX-License-Identifier: GPL-2.0
/*
 * ALSA SoC Amlogic t9015c interenl codec driver
 *
 * Copyright (C) 2019 Amlogic,inc
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/reset.h>

#include <linux/amlogic/media/sound/auge_utils.h>

#include "aml_codec_t9015.h"

struct t9015_acodec_chipinfo {
	bool separate_toacodec_en;
	int data_sel_shift;
};

struct aml_T9015_audio_priv {
	struct snd_soc_component *component;
	struct snd_pcm_hw_params *params;

	/* tocodec ctrl supports in and out data */
	bool tocodec_inout;
	/* attach which tdm when play */
	int tdmout_index;
	/* channel map */
	int ch0_sel;
	int ch1_sel;
	struct t9015_acodec_chipinfo *chipinfo;
	struct reset_control *rst;
};

static struct t9015_acodec_chipinfo aml_acodec_cinfo = {
	.separate_toacodec_en = false,
	.data_sel_shift = DATA_SEL_SHIFT_VERSION0,
};

static struct t9015_acodec_chipinfo sc2_acodec_cinfo = {
	.separate_toacodec_en = true,
	.data_sel_shift = DATA_SEL_SHIFT_VERSION0,
};

static struct t9015_acodec_chipinfo s4_acodec_cinfo = {
	.separate_toacodec_en = true,
	.data_sel_shift = DATA_SEL_SHIFT_VERSION1,
};

static const struct reg_default t9015_init_list[] = {
	{AUDIO_CONFIG_BLOCK_ENABLE, 0x0000B03F},
	{ADC_VOL_CTR_PGA_IN_CONFIG, 0x00000000},
	{DAC_VOL_CTR_DAC_SOFT_MUTE, 0xFBFB0000},
	{LINE_OUT_CONFIG, 0x00001111},
	{POWER_CONFIG, 0x00010000},
};

static int aml_T9015_audio_reg_init(struct snd_soc_component *component)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(t9015_init_list); i++)
		snd_soc_component_write(component,
					t9015_init_list[i].reg,
					t9015_init_list[i].def);

	return 0;
}

static int aml_DAC_Gain_get_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	u32 reg_addr = ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = snd_soc_component_read32(component, reg_addr);
	u32 val1 = (val & (0x1 << DAC_GAIN_SEL_L))
					>> DAC_GAIN_SEL_L;
	u32 val2 = (val & (0x1 << DAC_GAIN_SEL_H))
					>> (DAC_GAIN_SEL_H);

	val = val1 | (val2 << 1);

	ucontrol->value.enumerated.item[0] = val;

	return 0;
}

static int aml_DAC_Gain_set_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	u32 addr = ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = snd_soc_component_read32(component, addr);

	if (ucontrol->value.enumerated.item[0] == 0) {
		val &= ~(0x1 << DAC_GAIN_SEL_H);
		val &= ~(0x1 << DAC_GAIN_SEL_L);
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		val &= ~(0x1 << DAC_GAIN_SEL_H);
		val |= (0x1 << DAC_GAIN_SEL_L);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 2) {
		val |= (0x1 << DAC_GAIN_SEL_H);
		val &= ~(0x1 << DAC_GAIN_SEL_L);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 3) {
		val |= (0x1 << DAC_GAIN_SEL_H);
		val |= (0x1 << DAC_GAIN_SEL_L);
		pr_info("It has risk of distortion!\n");
	}

	snd_soc_component_write(component, addr, val);
	return 0;
}

static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95250, 375, 1);

static const char *const DAC_Gain_texts[] = {
	"0dB", "6dB", "12dB", "18dB" };

static const struct soc_enum dac_gain_enum =
SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(DAC_Gain_texts),
		DAC_Gain_texts);

static const struct snd_kcontrol_new T9015_audio_snd_controls[] = {
	/*DAC Digital Volume control */
	SOC_DOUBLE_TLV("DAC Digital Playback Volume",
		       DAC_VOL_CTR_DAC_SOFT_MUTE,
		       DACL_VC, DACR_VC, 0xff, 0, dac_vol_tlv),

    /*DAC extra Digital Gain control */
	SOC_ENUM_EXT("DAC Extra Digital Gain",
		     dac_gain_enum,
		     aml_DAC_Gain_get_enum,
		     aml_DAC_Gain_set_enum),

};

/*line out Left Positive mux */
static const char * const T9015_out_lp_txt[] = {
	"None", "LOLP_SEL_DACL", "LOLP_SEL_DACL_INV"
};

static SOC_ENUM_SINGLE_DECL(T9015_out_lp_enum, LINE_OUT_CONFIG,
	LOLP_SEL_DACL, T9015_out_lp_txt);

static const struct snd_kcontrol_new line_out_lp_mux =
	SOC_DAPM_ENUM("ROUTE_LP_OUT", T9015_out_lp_enum);

/*line out Left Negative mux */
static const char * const T9015_out_ln_txt[] = {
	"None", "LOLN_SEL_DACL_INV", "LOLN_SEL_DACL"
};

static SOC_ENUM_SINGLE_DECL(T9015_out_ln_enum, LINE_OUT_CONFIG,
				  LOLN_SEL_DACL_INV, T9015_out_ln_txt);

static const struct snd_kcontrol_new line_out_ln_mux =
	SOC_DAPM_ENUM("ROUTE_LN_OUT", T9015_out_ln_enum);

/*line out Right Positive mux */
static const char * const T9015_out_rp_txt[] = {
	"None", "LORP_SEL_DACR", "LORP_SEL_DACR_INV"
};

static SOC_ENUM_SINGLE_DECL(T9015_out_rp_enum, LINE_OUT_CONFIG,
				  LORP_SEL_DACR, T9015_out_rp_txt);

static const struct snd_kcontrol_new line_out_rp_mux =
	SOC_DAPM_ENUM("ROUTE_RP_OUT", T9015_out_rp_enum);

/*line out Right Negative mux */
static const char * const T9015_out_rn_txt[] = {
	"None", "LORN_SEL_DACR_INV", "LORN_SEL_DACR"
};

static SOC_ENUM_SINGLE_DECL(T9015_out_rn_enum, LINE_OUT_CONFIG,
				  LORN_SEL_DACR_INV, T9015_out_rn_txt);

static const struct snd_kcontrol_new line_out_rn_mux =
	SOC_DAPM_ENUM("ROUTE_RN_OUT", T9015_out_rn_enum);

static const struct snd_soc_dapm_widget T9015_audio_dapm_widgets[] = {
	/*Output */
	SND_SOC_DAPM_OUTPUT("Lineout left N"),
	SND_SOC_DAPM_OUTPUT("Lineout left P"),
	SND_SOC_DAPM_OUTPUT("Lineout right N"),
	SND_SOC_DAPM_OUTPUT("Lineout right P"),

	/*DAC playback stream */
	SND_SOC_DAPM_DAC("Left DAC", "HIFI Playback",
			 AUDIO_CONFIG_BLOCK_ENABLE, DACL_EN, 0),
	SND_SOC_DAPM_DAC("Right DAC", "HIFI Playback",
			 AUDIO_CONFIG_BLOCK_ENABLE, DACR_EN, 0),

	/*DRV output */
	SND_SOC_DAPM_OUT_DRV("LOLP_OUT_EN", AUDIO_CONFIG_BLOCK_ENABLE,
			     VMID_GEN_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LOLN_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LORP_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LORN_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),

	/*MUX output source select */
	SND_SOC_DAPM_MUX("Lineout left P switch", SND_SOC_NOPM,
			 0, 0, &line_out_lp_mux),
	SND_SOC_DAPM_MUX("Lineout left N switch", SND_SOC_NOPM,
			 0, 0, &line_out_ln_mux),
	SND_SOC_DAPM_MUX("Lineout right P switch", SND_SOC_NOPM,
			 0, 0, &line_out_rp_mux),
	SND_SOC_DAPM_MUX("Lineout right N switch", SND_SOC_NOPM,
			 0, 0, &line_out_rn_mux),
};

static const struct snd_soc_dapm_route T9015_audio_dapm_routes[] = {
    /*Output path*/
	{"Lineout left P switch", "LOLP_SEL_DACL", "Left DAC"},
	{"Lineout left P switch", "LOLP_SEL_DACL_INV", "Left DAC"},

	{"Lineout left N switch", "LOLN_SEL_DACL_INV", "Left DAC"},
	{"Lineout left N switch", "LOLN_SEL_DACL", "Left DAC"},

	{"Lineout right P switch", "LORP_SEL_DACR", "Right DAC"},
	{"Lineout right P switch", "LORP_SEL_DACR_INV", "Right DAC"},

	{"Lineout right N switch", "LORN_SEL_DACR_INV", "Right DAC"},
	{"Lineout right N switch", "LORN_SEL_DACR", "Right DAC"},

	{"LOLN_OUT_EN", NULL, "Lineout left N switch"},
	{"LOLP_OUT_EN", NULL, "Lineout left P switch"},
	{"LORN_OUT_EN", NULL, "Lineout right N switch"},
	{"LORP_OUT_EN", NULL, "Lineout right P switch"},

	{"Lineout left N", NULL, "LOLN_OUT_EN"},
	{"Lineout left P", NULL, "LOLP_OUT_EN"},
	{"Lineout right N", NULL, "LORN_OUT_EN"},
	{"Lineout right P", NULL, "LORP_OUT_EN"},
};

static int aml_T9015_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	u32 val = snd_soc_component_read32(component,
					   AUDIO_CONFIG_BLOCK_ENABLE);

	pr_debug("%s, format:%x, codec = %p\n", __func__, fmt, component);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		val |= (0x1 << I2S_MODE);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		val &= ~(0x1 << I2S_MODE);
		break;
	default:
		return -EINVAL;
	}

	snd_soc_component_write(component, AUDIO_CONFIG_BLOCK_ENABLE, val);

	return 0;
}

static int aml_T9015_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_component_get_drvdata(component);

	T9015_audio->params = params;

	return 0;
}

static int aml_T9015_audio_set_bias_level(struct snd_soc_component *component,
					  enum snd_soc_bias_level level)
{
	pr_info("%s\n", __func__);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		if (component->dapm.bias_level == SND_SOC_BIAS_OFF)
			snd_soc_component_cache_sync(component);
		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_component_write(component,
					AUDIO_CONFIG_BLOCK_ENABLE, 0);
		break;

	default:
		break;
	}
	component->dapm.bias_level = level;

	return 0;
}

static int aml_T9015_audio_reset(struct snd_soc_component *component)
{
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_component_get_drvdata(component);

	if (T9015_audio && !IS_ERR(T9015_audio->rst)) {
		pr_info("call standard reset interface\n");
		reset_control_reset(T9015_audio->rst);
	} else {
		pr_info("no call standard reset interface\n");
	}
	return 0;
}

static int aml_T9015_audio_start_up(struct snd_soc_component *component)
{
	snd_soc_component_write(component, AUDIO_CONFIG_BLOCK_ENABLE, 0xF000);
	msleep(200);
	snd_soc_component_write(component, AUDIO_CONFIG_BLOCK_ENABLE, 0xB000);

	return 0;
}

static int aml_T9015_codec_mute_stream(struct snd_soc_dai *dai,
				       int mute, int stream)
{
	struct snd_soc_component *component = dai->component;
	u32 reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg = snd_soc_component_read32(component,
					       DAC_VOL_CTR_DAC_SOFT_MUTE);
		if (mute)
			reg |= 0x1 << DAC_SOFT_MUTE;
		else
			reg &= ~(0x1 << DAC_SOFT_MUTE);

		snd_soc_component_write(component,
					DAC_VOL_CTR_DAC_SOFT_MUTE, reg);
	}
	return 0;
}

static int aml_T9015_audio_probe(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_component_get_drvdata(component);

	if (!T9015_audio) {
		pr_info("T9015_audio is null!\n");
		return -ENODEV;
	}
	dapm->idle_bias_off = 0;
	T9015_audio->rst = devm_reset_control_get(component->dev, "acodec");

	/*reset audio codec register*/
	aml_T9015_audio_reset(component);
	aml_T9015_audio_start_up(component);
	aml_T9015_audio_reg_init(component);

	if (T9015_audio->tocodec_inout)
		auge_toacodec_ctrl_ext(T9015_audio->tdmout_index,
				       T9015_audio->ch0_sel,
				       T9015_audio->ch1_sel,
				       T9015_audio->chipinfo->separate_toacodec_en,
				       T9015_audio->chipinfo->data_sel_shift);
	else
		auge_toacodec_ctrl(T9015_audio->tdmout_index);

	component->dapm.bias_level = SND_SOC_BIAS_STANDBY;
	T9015_audio->component = component;

	return 0;
}

static void aml_T9015_audio_remove(struct snd_soc_component *component)
{
	pr_info("%s!\n", __func__);

	aml_T9015_audio_set_bias_level(component, SND_SOC_BIAS_OFF);
}

static int aml_T9015_audio_suspend(struct snd_soc_component *component)
{
	pr_info("%s!\n", __func__);

	aml_T9015_audio_set_bias_level(component, SND_SOC_BIAS_OFF);

	return 0;
}

static int aml_T9015_audio_resume(struct snd_soc_component *component)
{
	pr_info("%s!\n", __func__);

	aml_T9015_audio_reset(component);
	aml_T9015_audio_start_up(component);
	aml_T9015_audio_reg_init(component);
	component->dapm.bias_level = SND_SOC_BIAS_STANDBY;
	aml_T9015_audio_set_bias_level(component, SND_SOC_BIAS_STANDBY);

	return 0;
}

#define T9015_AUDIO_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define T9015_AUDIO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_dai_ops T9015_audio_aif_dai_ops = {
	.hw_params = aml_T9015_hw_params,
	.set_fmt = aml_T9015_set_dai_fmt,
	.mute_stream = aml_T9015_codec_mute_stream,
};

struct snd_soc_dai_driver aml_T9015_audio_dai[] = {
	{
		.name = "T9015-audio-hifi",
		.id = 0,
		.playback = {
			.stream_name  = "HIFI Playback",
			.channels_min = 2,
			.channels_max = 16,
			.rates        = T9015_AUDIO_STEREO_RATES,
			.formats      = T9015_AUDIO_FORMATS,
		},
		.capture = {
			.stream_name  = "HIFI Capture",
			.channels_min = 2,
			.channels_max = 8,
			.rates        = T9015_AUDIO_STEREO_RATES,
			.formats      = T9015_AUDIO_FORMATS,
		},
		.ops = &T9015_audio_aif_dai_ops,
	 },
};

static const struct snd_soc_component_driver soc_codec_dev_aml_T9015_audio = {
	.probe            = aml_T9015_audio_probe,
	.remove           = aml_T9015_audio_remove,
	.suspend          = aml_T9015_audio_suspend,
	.resume           = aml_T9015_audio_resume,
	.set_bias_level   = aml_T9015_audio_set_bias_level,

	.controls         = T9015_audio_snd_controls,
	.num_controls     = ARRAY_SIZE(T9015_audio_snd_controls),
	.dapm_widgets     = T9015_audio_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(T9015_audio_dapm_widgets),
	.dapm_routes      = T9015_audio_dapm_routes,
	.num_dapm_routes  = ARRAY_SIZE(T9015_audio_dapm_routes),
};

static const struct regmap_config t9015_codec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x14,
	.reg_defaults = t9015_init_list,
	.num_reg_defaults = ARRAY_SIZE(t9015_init_list),
	.cache_type = REGCACHE_RBTREE,
};

static int aml_T9015_audio_codec_probe(struct platform_device *pdev)
{
	struct resource *res_mem;
	void __iomem *regs;
	struct regmap *regmap;
	struct aml_T9015_audio_priv *T9015_audio;
	int ret = 0;
	struct t9015_acodec_chipinfo *p_chipinfo;

	dev_info(&pdev->dev, "%s\n", __func__);

	T9015_audio = devm_kzalloc(&pdev->dev,
				   sizeof(struct aml_T9015_audio_priv),
				   GFP_KERNEL);
	if (!T9015_audio)
		return -ENOMEM;

	p_chipinfo = (struct t9015_acodec_chipinfo *)
		of_device_get_match_data(&pdev->dev);
	if (!p_chipinfo) {
		dev_warn_once(&pdev->dev,
			      "no update t9015_acodec_chipinfo\n");
		return -EINVAL;
	}

	T9015_audio->chipinfo = p_chipinfo;

	platform_set_drvdata(pdev, T9015_audio);

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	regs = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	regmap = devm_regmap_init_mmio(&pdev->dev, regs,
				       &t9015_codec_regmap_config);

	T9015_audio->tocodec_inout =
		of_property_read_bool(pdev->dev.of_node, "tocodec_inout");

	of_property_read_u32(pdev->dev.of_node,
			     "tdmout_index", &T9015_audio->tdmout_index);

	of_property_read_u32(pdev->dev.of_node,
			     "ch0_sel", &T9015_audio->ch0_sel);

	of_property_read_u32(pdev->dev.of_node,
			     "ch1_sel", &T9015_audio->ch1_sel);

	pr_info("T9015 acodec tdmout index:%d\n",
		T9015_audio->tdmout_index);

	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &soc_codec_dev_aml_T9015_audio,
					      &aml_T9015_audio_dai[0], 1);

	return ret;
}

static void aml_T9015_audio_codec_shutdown(struct platform_device *pdev)
{
	struct aml_T9015_audio_priv *aml_acodec;
	struct snd_soc_component *component;

	aml_acodec = platform_get_drvdata(pdev);
	component = aml_acodec->component;
	if (component)
		aml_T9015_audio_remove(component);
}

static const struct of_device_id aml_T9015_codec_dt_match[] = {
	{
		.compatible = "amlogic, aml_codec_T9015",
		.data = &aml_acodec_cinfo,
	},
	{
		.compatible = "amlogic, sc2_codec_T9015",
		.data = &sc2_acodec_cinfo,
	},
	{
		.compatible = "amlogic, s4_codec_T9015",
		.data = &s4_acodec_cinfo,
	},
	{}
};

static struct platform_driver aml_T9015_codec_platform_driver = {
	.driver  = {
		.name           = "aml_codec_T9015",
		.owner          = THIS_MODULE,
		.of_match_table = aml_T9015_codec_dt_match,
	},
	.probe    = aml_T9015_audio_codec_probe,
	.shutdown = aml_T9015_audio_codec_shutdown,
};

module_platform_driver(aml_T9015_codec_platform_driver);

MODULE_DESCRIPTION("ASoC AML T9015 audio codec driver");
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");

