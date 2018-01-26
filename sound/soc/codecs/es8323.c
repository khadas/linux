/*
 * es8323.c -- es8323 ALSA SoC audio driver
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@openedhand.com>
 *
 * Based on es8323.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of.h>

#ifdef CONFIG_HHBF_FAST_REBOOT
#include <linux/reboot.h>
#endif

#include "es8323.h"

#define AUDIO_NAME "ES8323"
#define ES8323_VERSION "v1.0"

#ifndef es8323_DEF_VOL
#define es8323_DEF_VOL 0x1c
#endif

static int id;

/* codec private data */
struct es8323_priv {
	struct snd_soc_codec *codec;
	int (*set_bias_level)(struct snd_soc_codec *,
			       enum snd_soc_bias_level level);
	enum snd_soc_control_type control_type;
	unsigned int sysclk;
};

/*
 * Debug
 */

// #define ES8323_DEBUG 1

#ifdef ES8323_DEBUG
#define alsa_dbg(format, arg...) \
	pr_info("== ES8323 == " format, ## arg)
#else
#define alsa_dbg(format, arg...) do {} while (0)
#endif

/*
 * es8323 register cache
 * We can't read the es8323 register space when we
 * are using 2 wire for device control, so we cache them instead.
 */
static const u16 es8323_reg[] = {
	0x06, 0x1C, 0xC3, 0xFC, /* 0 */
	0xC0, 0x00, 0x00, 0x7C,	/* 4 */
	0x80, 0x00, 0x00, 0x06,	/* 8 */
	0x00, 0x06, 0x30, 0x30,	/* 12 */
	0xC0, 0xC0, 0x38, 0xB0,	/* 16 */
	0x32, 0x06, 0x00, 0x00,	/* 20 */
	0x06, 0x32, 0xC0, 0xC0,	/* 24 */
	0x08, 0x06, 0x1F, 0xF7,	/* 28 */
	0xFD, 0xFF, 0x1F, 0xF7,	/* 32 */
	0xFD, 0xFF, 0x00, 0x38,	/* 36 */
	0x38, 0x38, 0x38, 0x38,	/* 40 */
	0x38, 0x00, 0x00, 0x00,	/* 44 */
	0x00, 0x00, 0x00, 0x00,	/* 48 */
	0x00, 0x00, 0x00, 0x00,	/* 52 */
};

static const u16 es8323_probe_regs[] = {
	/* same as 8323 probe reg order */
	ES8323_DACCONTROL23,
	ES8323_MASTERMODE,
	ES8323_DACCONTROL21,
	ES8323_CONTROL1,
	ES8323_CONTROL2,

	ES8323_CHIPLOPOW1,
	ES8323_CHIPLOPOW2,
	ES8323_DACPOWER,
	ES8323_ADCPOWER,
	ES8323_ANAVOLMANAG,

	ES8323_ADCCONTROL1,
	ES8323_ADCCONTROL2,
	ES8323_ADCCONTROL3,
	ES8323_ADCCONTROL4,
	ES8323_ADCCONTROL5,

	ES8323_LADC_VOL,
	ES8323_RADC_VOL,

	ES8323_ADCCONTROL10,
	ES8323_ADCCONTROL11,
	ES8323_ADCCONTROL12,
	ES8323_ADCCONTROL13,
	ES8323_ADCCONTROL14,

	ES8323_DACCONTROL1,
	ES8323_DACCONTROL2,
	ES8323_LDAC_VOL,
	ES8323_RDAC_VOL,
	ES8323_DAC_MUTE,

	ES8323_DACCONTROL17,
	ES8323_DACCONTROL20,
	ES8323_CHIPPOWER,

	ES8323_LOUT1_VOL,
	ES8323_ROUT1_VOL,
	ES8323_LOUT2_VOL,
	ES8323_ROUT2_VOL,
};

static const struct snd_kcontrol_new es8323_snd_controls[] = {
SOC_DOUBLE_R("Capture Volume", ES8323_LADC_VOL, ES8323_RADC_VOL, 0, 192, 1),
};

/*
 * DAPM Controls
 */

/* Channel Input Mixer */
static const char * const es8323_line_texts[] = {
	"Line 1", "Line 2", "Differential"
};

static const unsigned int es8323_line_values[] = {
	0, 1, 3
};

static const struct soc_enum es8323_lline_enum =
SOC_VALUE_ENUM_SINGLE(ES8323_ADCCONTROL3, 6, 0xC0,
		      ARRAY_SIZE(es8323_line_texts), es8323_line_texts,
		      es8323_line_values);
static const struct snd_kcontrol_new es8323_left_line_controls =
SOC_DAPM_VALUE_ENUM("Route", es8323_lline_enum);

static const struct soc_enum es8323_rline_enum =
SOC_VALUE_ENUM_SINGLE(ES8323_ADCCONTROL3, 4, 0x30,
		      ARRAY_SIZE(es8323_line_texts), es8323_line_texts,
		      es8323_line_values);
static const struct snd_kcontrol_new es8323_right_line_controls =
SOC_DAPM_VALUE_ENUM("Route", es8323_lline_enum);

/* Left Mixer */
static const struct snd_kcontrol_new es8323_left_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left Playback Switch", ES8323_DACCONTROL17, 7, 1, 0),
	SOC_DAPM_SINGLE("Left Bypass Switch", ES8323_DACCONTROL17, 6, 1, 0),
};

/* Right Mixer */
static const struct snd_kcontrol_new es8323_right_mixer_controls[] = {
	SOC_DAPM_SINGLE("Right Playback Switch", ES8323_DACCONTROL20, 7, 1, 0),
	SOC_DAPM_SINGLE("Right Bypass Switch", ES8323_DACCONTROL20, 6, 1, 0),
};

/* Differential Mux */
static const char * const es8323_diff_sel[] = { "Line 1", "Line 2" };

static const struct soc_enum left_diffmux =
SOC_ENUM_SINGLE(ES8323_ADCCONTROL2, 2, 2, es8323_diff_sel);
static const struct snd_kcontrol_new es8323_left_diffmux_controls =
SOC_DAPM_ENUM("Route", left_diffmux);

static const struct soc_enum right_diffmux =
SOC_ENUM_SINGLE(ES8323_ADCCONTROL3, 7, 2, es8323_diff_sel);
static const struct snd_kcontrol_new es8323_right_diffmux_controls =
SOC_DAPM_ENUM("Route", right_diffmux);

/* Mono ADC Mux */
static const char * const es8323_mono_mux[] = {
	"Stereo", "Mono (Left)", "Mono (Right)", "NONE"
};

static const struct soc_enum monomux =
SOC_ENUM_SINGLE(ES8323_ADCCONTROL3, 3, 4, es8323_mono_mux);
static const struct snd_kcontrol_new es8323_monomux_controls =
SOC_DAPM_ENUM("Route", monomux);

static const struct snd_soc_dapm_widget es8323_dapm_widgets[] = {
	/* DAC Part */
	SND_SOC_DAPM_MIXER("Left Mixer", SND_SOC_NOPM, 0, 0,
			   &es8323_left_mixer_controls[0],
			   ARRAY_SIZE(es8323_left_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Mixer", SND_SOC_NOPM, 0, 0,
			   &es8323_right_mixer_controls[0],
			   ARRAY_SIZE(es8323_right_mixer_controls)),

	SND_SOC_DAPM_MUX("Left Line Mux", SND_SOC_NOPM, 0, 0,
			 &es8323_left_line_controls),
	SND_SOC_DAPM_MUX("Right Line Mux", SND_SOC_NOPM, 0, 0,
			 &es8323_right_line_controls),

	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", ES8323_DACCONTROL38, 7,
			 1),
	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", ES8323_DACCONTROL38, 6,
			 1),
	SND_SOC_DAPM_PGA("Left Out 1", SND_SOC_NOPM, 5, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Out 1", SND_SOC_NOPM, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left Out 2", SND_SOC_NOPM, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Out 2", SND_SOC_NOPM, 2, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),
	SND_SOC_DAPM_OUTPUT("VREF"),

	/* -------------------------------------------- */
	/* ADC Part */
	/* -------------------------------------------- */
	SND_SOC_DAPM_MUX("Differential Left Mux", SND_SOC_NOPM, 0, 0,
			 &es8323_left_diffmux_controls),
	SND_SOC_DAPM_MUX("Differential Right Mux", SND_SOC_NOPM, 0, 0,
			 &es8323_right_diffmux_controls),

	SND_SOC_DAPM_MUX("Left ADC Mux", SND_SOC_NOPM, 0, 0,
			 &es8323_monomux_controls),
	SND_SOC_DAPM_MUX("Right ADC Mux", SND_SOC_NOPM, 0, 0,
			 &es8323_monomux_controls),

	SND_SOC_DAPM_PGA("Left Analog Input", 0x37 /*ES8323_ADCPOWER */ , 7, 1,
			 NULL, 0),
	SND_SOC_DAPM_PGA("Right Analog Input", 0x37 /*ES8323_ADCPOWER */ , 6, 1,
			 NULL, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", 0x37 /*ES8323_ADCPOWER */ ,
			 5, 1),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture",
			 0x37 /*ES8323_ADCPOWER */ , 4, 1),

	SND_SOC_DAPM_MICBIAS("Mic Bias", 0x37 /*ES8323_ADCPOWER */ , 3, 1),

	SND_SOC_DAPM_INPUT("MICIN"),
	SND_SOC_DAPM_INPUT("LINPUT1"),
	SND_SOC_DAPM_INPUT("LINPUT2"),
	SND_SOC_DAPM_INPUT("RINPUT1"),
	SND_SOC_DAPM_INPUT("RINPUT2"),
};

static const struct snd_soc_dapm_route intercon[] = {
	/* left mixer */
	{"Left Mixer", "Left Playback Switch", "Left DAC"},

	/* right mixer */
	{"Right Mixer", "Right Playback Switch", "Right DAC"},

	/* left out 1 */
	{"Left Out 1", NULL, "Left Mixer"},
	{"LOUT1", NULL, "Left Out 1"},

	/* right out 1 */
	{"Right Out 1", NULL, "Right Mixer"},
	{"ROUT1", NULL, "Right Out 1"},

	/* left out 2 */
	{"Left Out 2", NULL, "Left Mixer"},
	{"LOUT2", NULL, "Left Out 2"},

	/* right out 2 */
	{"Right Out 2", NULL, "Right Mixer"},
	{"ROUT2", NULL, "Right Out 2"},

	/* Differential Mux */
	{"Differential Left Mux", "Line 1", "LINPUT1"},
	{"Differential Right Mux", "Line 1", "RINPUT1"},
	{"Differential Left Mux", "Line 2", "LINPUT2"},
	{"Differential Right Mux", "Line 2", "RINPUT2"},

	/* Left Line Mux */
	{"Left Line Mux", "Line 1", "LINPUT1"},
	{"Left Line Mux", "Line 2", "LINPUT2"},
	{"Left Line Mux", "Differential", "Differential Left Mux"},

	/* Right Line Mux */
	{"Right Line Mux", "Line 1", "RINPUT1"},
	{"Right Line Mux", "Line 2", "RINPUT2"},
	{"Right Line Mux", "Differential", "Differential Right Mux"},

	/* Left ADC Mux */
	{"Left ADC Mux", "Stereo", "Left Line Mux"},
/* {"Left ADC Mux", "Mono (Left)" , "Left Line Mux"}, */

	/* Right ADC Mux */
	{"Right ADC Mux", "Stereo", "Right Line Mux"},
/* {"Right ADC Mux", "Mono (Right)", "Right Line Mux"}, */

	/* ADC */
	{"Left ADC", NULL, "Left ADC Mux"},
	{"Right ADC", NULL, "Right ADC Mux"},

	{"Left ADC", NULL, "Mic Bias"},
	{"Right ADC", NULL, "Mic Bias"},

	{"Mic Bias", NULL, "MICIN"},
};

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u8 sr:5;
	u8 single_double:1;
	u8 blckdiv:4;
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{12000000, 8000, 1500, 0x1B, 0, 0xa},

	/* 11.025k */
	{12000000, 11025, 1088, 0x19, 0, 0xa},

	/* 12k */
	{12000000, 12000, 1000, 0x18, 0, 0xa},

	/* 16k */
	{12000000, 16000, 750, 0x17, 0, 0x6},

	/* 22.05k */
	{12000000, 22050, 544, 0x16, 0, 0x6},

	/* 24k */
	{12000000, 24000, 500, 0x15, 0, 0x6},

	/* 32k */
	{12000000, 32000, 375, 0x14, 0, 0x6},

	/* 44.1k */
	{11289600, 44100, 256, 0x02, 0, 0x4},
	{12000000, 44100, 272, 0x13, 0, 0x4},

	/* 48k */
	{12000000, 48000, 250, 0x12, 0, 0x4},
	{12288000, 48000, 256, 0x02, 0, 0x4},

	/* 88.2k */
	{12000000, 88200, 136, 0x11, 1, 0x2},

	/* 96k */
	{12000000, 96000, 125, 0x10, 1, 0x2},
};

static int es8323_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
				 int div_id, int div)
{
	return 0;
}

static unsigned int es8323_read_reg_cache(struct snd_soc_codec *codec,
					  unsigned int reg)
{
	u16 *cache = codec->reg_cache;

	if (reg >= codec->driver->reg_cache_size)
		return -1;
	return cache[reg];
}

static int es8323_write(struct snd_soc_codec *codec, unsigned int reg,
			unsigned int value)
{
	u16 *cache = codec->reg_cache;
	u8 data[2];
	int ret;

	BUG_ON(codec->volatile_register);

	data[0] = reg;
	data[1] = value & 0x00ff;

	if (reg < codec->driver->reg_cache_size)
		cache[reg] = value;
	ret = codec->hw_write(codec->control_data, data, 2);

	if (ret == 2)
		return 0;
	if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int es8323_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				 int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct es8323_priv *es8323 = snd_soc_codec_get_drvdata(codec);
	alsa_dbg("%s----%d, %d\n", __func__, __LINE__, freq);
	switch (freq) {
	case 11289600:
	case 12000000:
	case 12288000:
	case 16934400:
	case 18432000:
		es8323->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int es8323_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u8 iface = 0;
	u8 adciface = 0;
	u8 daciface = 0;
	alsa_dbg("%s----%d, fmt[%02x]\n", __func__, __LINE__, fmt);

	iface = snd_soc_read(codec, ES8323_IFACE);
	adciface = snd_soc_read(codec, ES8323_ADC_IFACE);
	daciface = snd_soc_read(codec, ES8323_DAC_IFACE);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:	/* MASTER MODE */
		iface |= 0x80;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:	/* SLAVE MODE */
		iface &= 0x7F;
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		adciface &= 0xFC;
		/* daciface &= 0xF9; //updated by david-everest,5-25 */
		daciface &= 0xF9;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	case SND_SOC_DAIFMT_DSP_A:
		break;
	case SND_SOC_DAIFMT_DSP_B:
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		iface &= 0xDF;
		adciface &= 0xDF;
		/* daciface &= 0xDF; //UPDATED BY david-everest,5-25 */
		daciface &= 0xBF;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x20;
		/* adciface &= 0xDF; //UPDATED BY david-everest,5-25 */
		adciface |= 0x20;
		/* daciface &= 0xDF; //UPDATED BY david-everest,5-25 */
		daciface |= 0x40;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x20;
		/* adciface |= 0x40; //UPDATED BY david-everest,5-25 */
		adciface &= 0xDF;
		/* daciface |= 0x40; //UPDATED BY david-everest,5-25 */
		daciface &= 0xBF;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface &= 0xDF;
		adciface |= 0x20;
		/* daciface |= 0x20; //UPDATED BY david-everest,5-25 */
		daciface |= 0x40;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, ES8323_IFACE, iface, iface);
	snd_soc_update_bits(codec, ES8323_ADC_IFACE, adciface, adciface);
	snd_soc_update_bits(codec, ES8323_DAC_IFACE, daciface, daciface);
	return 0;
}

static int es8323_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	u16 iface;

	alsa_dbg("es8323_pcm_hw_params()----%d, sampling rate[%d]\n", __LINE__,
		 params_rate(params));

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		alsa_dbg("es8323_pcm_hw_params() playback stream\n\n");

		iface = snd_soc_read(codec, ES8323_DAC_IFACE) & 0xC7;

		/* bit size */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			iface |= 0x0018;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			iface |= 0x0008;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			iface |= 0x0020;
			break;
		}
		/* snd_soc_write(codec, ES8323_DAC_IFACE, iface); */
		snd_soc_update_bits(codec, ES8323_DAC_IFACE, iface, iface);
	} else {
		alsa_dbg("es8323_pcm_hw_params() capture stream\n\n");

		iface = snd_soc_read(codec, ES8323_ADC_IFACE) & 0xE3;

		/* bit size */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			iface |= 0x000C;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			iface |= 0x0004;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			iface |= 0x0010;
			break;
		}
		/* set iface */
		/* snd_soc_write(codec, ES8323_ADC_IFACE, iface); */
		snd_soc_update_bits(codec, ES8323_ADC_IFACE, iface, iface);
	}

	return 0;
}

static int es8323_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned char val = 0;

	alsa_dbg("%s----%d, %d\n", __func__, __LINE__, mute);

	val = snd_soc_read(codec, ES8323_DAC_MUTE);
	if (mute)
		snd_soc_write(codec, 0x19, 0x26);
	else
		snd_soc_write(codec, 0x19, 0x22);

	return 0;
}

static int es8323_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		snd_soc_write(codec, ES8323_DACPOWER, 0x3C);
		dev_dbg(codec->dev, "%s on\n", __func__);
		break;
	case SND_SOC_BIAS_PREPARE:
		dev_dbg(codec->dev, "%s prepare\n", __func__);
		snd_soc_write(codec, ES8323_ANAVOLMANAG, 0x7C);
		snd_soc_write(codec, ES8323_CHIPLOPOW1, 0x00);
		snd_soc_write(codec, ES8323_CHIPLOPOW2, 0x00);
		snd_soc_write(codec, ES8323_CHIPPOWER, 0x00);
		snd_soc_write(codec, ES8323_ADCPOWER, 0x00);
		snd_soc_write(codec, ES8323_DACPOWER, 0x3C);
		break;
	case SND_SOC_BIAS_STANDBY:
		dev_dbg(codec->dev, "%s standby\n", __func__);
		/* es8323_off_amp(true); */
		snd_soc_write(codec, ES8323_CHIPLOPOW1, 0xFF);
		snd_soc_write(codec, ES8323_CHIPLOPOW2, 0xFF);
		break;
	case SND_SOC_BIAS_OFF:
		dev_dbg(codec->dev, "%s off\n", __func__);
		/* es8323_off_amp(true); */
		snd_soc_write(codec, ES8323_ADCPOWER, 0xFC);
		/* snd_soc_write(codec, ES8323_DACPOWER, 0xC0); */
		snd_soc_write(codec, ES8323_CHIPLOPOW1, 0xFF);
		snd_soc_write(codec, ES8323_CHIPLOPOW2, 0xFF);
		snd_soc_write(codec, ES8323_CHIPPOWER, 0xFF);
		snd_soc_write(codec, ES8323_ANAVOLMANAG, 0x7B);
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

#define ES8323_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | \
	SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 | \
	SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000)

#define ES8323_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops es8323_ops = {
	.hw_params = es8323_pcm_hw_params,
	.set_fmt = es8323_set_dai_fmt,
	.set_sysclk = es8323_set_dai_sysclk,
	.digital_mute = es8323_mute,
	.set_clkdiv = es8323_set_dai_clkdiv,
};

struct snd_soc_dai_driver es8323_dai = {
	.name = "es8323-hifi",
	.playback = {
	     .stream_name = "Playback",
	     .channels_min = 2,
	     .channels_max = 8,
	     .rates = ES8323_RATES,
	     .formats = ES8323_FORMATS,
	},
	.capture = {
	    .stream_name = "Capture",
	    .channels_min = 1,
	    .channels_max = 2,
	    .rates = ES8323_RATES,
	    .formats = ES8323_FORMATS,
	},
	.ops = &es8323_ops,
};

#ifdef CONFIG_PM
static int es8323_i2c_ts_suspend(struct device *dev)
{
	struct i2c_client *client = i2c_verify_client(dev);
	struct es8323_priv *es8323 = dev_get_drvdata(&client->dev);
	struct snd_soc_codec *codec = es8323->codec;

	/* Power off */
	dev_info(codec->dev, "es8323_i2c_ts_suspend\n");
	snd_soc_write(codec, 0x19, 0x26);
	snd_soc_write(codec, 0x30, 0x18);
	snd_soc_write(codec, 0x31, 0x18);
	msleep(20);
	snd_soc_write(codec, 0x30, 0x08);
	snd_soc_write(codec, 0x31, 0x08);
	snd_soc_write(codec, ES8323_CONTROL2, 0x58);
	snd_soc_write(codec, ES8323_CONTROL1, 0x32);
	snd_soc_write(codec, ES8323_CHIPPOWER, 0xf3);
	snd_soc_write(codec, ES8323_DACPOWER, 0xc0);
	snd_soc_write(codec, ES8323_DACCONTROL26, 0x00);
	snd_soc_write(codec, ES8323_DACCONTROL27, 0x00);
	snd_soc_write(codec, ES8323_CONTROL1, 0x30);
	snd_soc_write(codec, ES8323_CONTROL1, 0x34);
	msleep(20);
	return 0;
}

static int es8323_i2c_ts_resume(struct device *dev)
{
	struct i2c_client *client = i2c_verify_client(dev);
	struct es8323_priv *es8323 = dev_get_drvdata(&client->dev);
	struct snd_soc_codec *codec = es8323->codec;

	dev_info(codec->dev, "es8323_i2c_ts_resume\n");

	snd_soc_write(codec, 0x02, 0xf3);
	snd_soc_write(codec, 0x2B, 0x80);
	snd_soc_write(codec, 0x08, 0x00);
	snd_soc_write(codec, 0x00, 0x32);
	snd_soc_write(codec, 0x01, 0x72);
	snd_soc_write(codec, 0x03, 0x59);
	snd_soc_write(codec, 0x05, 0x00);
	snd_soc_write(codec, 0x06, 0x00);
	snd_soc_write(codec, 0x09, 0x99);
	snd_soc_write(codec, 0x0a, 0xf0);
	snd_soc_write(codec, 0x0b, 0x82);
	snd_soc_write(codec, 0x0C, 0x4c);
	snd_soc_write(codec, 0x0d, 0x02);
	snd_soc_write(codec, 0x10, 0x00);
	snd_soc_write(codec, 0x11, 0x00);
	snd_soc_write(codec, 0x12, 0xea);
	snd_soc_write(codec, 0x13, 0xa0);
	snd_soc_write(codec, 0x14, 0x05);
	snd_soc_write(codec, 0x15, 0x06);
	snd_soc_write(codec, 0x16, 0x33);
	snd_soc_write(codec, 0x17, 0x18);
	snd_soc_write(codec, 0x18, 0x02);
	snd_soc_write(codec, 0x1A, 0x00);
	snd_soc_write(codec, 0x1B, 0x00);

	snd_soc_write(codec, 0x26, 0x12);
	snd_soc_write(codec, 0x27, 0xb8);
	snd_soc_write(codec, 0x2A, 0xb8);
	snd_soc_write(codec, 0x02, 0x00);
	snd_soc_write(codec, 0x19, 0x22);
	snd_soc_write(codec, 0x04, 0x00);
	/* msleep(100); */
	snd_soc_write(codec, 0x2e, 0x08);
	snd_soc_write(codec, 0x2f, 0x08);
	snd_soc_write(codec, 0x30, 0x08);
	snd_soc_write(codec, 0x31, 0x08);
	/* msleep(200); */
	snd_soc_write(codec, 0x2e, 0x0f);
	snd_soc_write(codec, 0x2f, 0x0f);
	snd_soc_write(codec, 0x30, 0x0f);
	snd_soc_write(codec, 0x31, 0x0f);
	/* msleep(200); */
	snd_soc_write(codec, 0x2e, 0x1e);
	snd_soc_write(codec, 0x2f, 0x1e);
	snd_soc_write(codec, 0x30, 0x1e);
	snd_soc_write(codec, 0x31, 0x1e);
	snd_soc_write(codec, 0x04, 0x3c);

	/* Sync reg_cache with the hardware */
#if 0
	for (i = 0; i < ARRAY_SIZE(/*es8323_reg */ es8323_probe_regs); i++) {
		data[0] = es8323_probe_regs[i];
		data[1] = cache[es8323_probe_regs[i]] & 0xFF;
		codec->hw_write(codec->control_data, data, 2);
	}
#endif
	return 0;
}

static const struct dev_pm_ops es8323_dev_pm_ops = {
	.suspend = es8323_i2c_ts_suspend,
	.resume = es8323_i2c_ts_resume,
};

#else
#define es8323_i2c_ts_suspend NULL
#define es8323_i2c_ts_resume NULL
#endif

static void es8323_shutdown(struct i2c_client *client)
{
	struct es8323_priv *es8323 = dev_get_drvdata(&client->dev);
	struct snd_soc_codec *codec = es8323->codec;

	snd_soc_write(codec, ES8323_CONTROL2, 0x58);
	snd_soc_write(codec, ES8323_CONTROL1, 0x32);
	snd_soc_write(codec, ES8323_CHIPPOWER, 0xf3);
	snd_soc_write(codec, ES8323_DACPOWER, 0xc0);
	snd_soc_write(codec, ES8323_DACCONTROL26, 0x00);
	snd_soc_write(codec, ES8323_DACCONTROL27, 0x00);
	snd_soc_write(codec, ES8323_CONTROL1, 0x30);
	snd_soc_write(codec, ES8323_CONTROL1, 0x34);
	/* return 0; */
}

static int es8323_probe(struct snd_soc_codec *codec)
{
	struct es8323_priv *es8323 = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret = 0, idread = 0;

	es8323->set_bias_level = es8323_set_bias_level;
	es8323->codec = codec;
	codec->read = es8323_read_reg_cache;
	codec->write = es8323_write;
	codec->hw_write = (hw_write_t) i2c_master_send;
	codec->control_data = container_of(codec->dev, struct i2c_client, dev);
	idread = snd_soc_read(codec, ES8323_DACCONTROL18);
	if (idread == 0x28) {
		id = 0;
		dev_info(codec->dev, "es8323_probe\r\n");
	} else if (idread == 0x00) {
		id = 1;
		dev_info(codec->dev, "es8323s_probe\r\n");
	} else {
		id = 0;
		dev_info(codec->dev, "id read invalid, id=%d\n", idread);
	}
	es8323_set_bias_level(codec, SND_SOC_BIAS_OFF);
	if (ret) {
		dev_err(codec->dev, "ES8323 chip not exist\n");
		kfree(es8323);
	}
	if (id == 0) {
		snd_soc_write(codec, 0x02, 0xf3);
		snd_soc_write(codec, 0x2B, 0x80);
		snd_soc_write(codec, 0x08, 0x00);
		snd_soc_write(codec, 0x00, 0x32);
		snd_soc_write(codec, 0x01, 0x72);
		snd_soc_write(codec, 0x03, 0x59);
		snd_soc_write(codec, 0x05, 0x00);
		snd_soc_write(codec, 0x06, 0x00);
		snd_soc_write(codec, 0x09, 0x99);
		snd_soc_write(codec, 0x0a, 0xf0);
		snd_soc_write(codec, 0x0b, 0x82);
		snd_soc_write(codec, 0x0C, 0x4c);
		snd_soc_write(codec, 0x0d, 0x02);
		snd_soc_write(codec, 0x10, 0x00);
		snd_soc_write(codec, 0x11, 0x00);
		snd_soc_write(codec, 0x12, 0xea);
		snd_soc_write(codec, 0x13, 0x70);
		snd_soc_write(codec, 0x14, 0x05);
		snd_soc_write(codec, 0x15, 0x06);
		snd_soc_write(codec, 0x16, 0x33);
		snd_soc_write(codec, 0x17, 0x18);
		snd_soc_write(codec, 0x18, 0x02);
		snd_soc_write(codec, 0x1A, 0x00);
		snd_soc_write(codec, 0x1B, 0x00);
		snd_soc_write(codec, 0x26, 0x12);
		snd_soc_write(codec, 0x27, 0xb8);
		snd_soc_write(codec, 0x2A, 0xb8);
		snd_soc_write(codec, 0x02, 0x00);
		snd_soc_write(codec, 0x19, 0x26);
		snd_soc_write(codec, 0x04, 0x2c);
		msleep(100);
		snd_soc_write(codec, 0x2e, 0x00);
		snd_soc_write(codec, 0x2f, 0x00);
		snd_soc_write(codec, 0x30, 0x08);
		snd_soc_write(codec, 0x31, 0x08);
		msleep(200);
		snd_soc_write(codec, 0x30, 0x0f);
		snd_soc_write(codec, 0x31, 0x0f);
		msleep(200);
		snd_soc_write(codec, 0x30, 0x1e);
		snd_soc_write(codec, 0x31, 0x1e);

	} else if (id == 1) {
		snd_soc_write(codec, 0x35, 0xa0);
		snd_soc_write(codec, 0x38, 0x02);
		snd_soc_write(codec, 0x02, 0xf3);
		snd_soc_write(codec, 0x00, 0x36);
		snd_soc_write(codec, 0x01, 0x72);
		snd_soc_write(codec, 0x08, 0x00);
		snd_soc_write(codec, 0x2d, 0x10);
		snd_soc_write(codec, 0x2b, 0x80);
		snd_soc_write(codec, 0x06, 0xff);
		snd_soc_write(codec, 0x05, 0x00);
		snd_soc_write(codec, 0x07, 0x7c);
		snd_soc_write(codec, 0x0a, 0xf0);
		snd_soc_write(codec, 0x0b, 0x82);
		snd_soc_write(codec, 0x09, 0x50);
		snd_soc_write(codec, 0x0C, 0x4c);
		snd_soc_write(codec, 0x0d, 0x02);
		snd_soc_write(codec, 0x0e, 0x2c);
		snd_soc_write(codec, 0x10, 0x00);
		snd_soc_write(codec, 0x11, 0x00);
		snd_soc_write(codec, 0x12, 0xd2);
		snd_soc_write(codec, 0x13, 0x80);
		snd_soc_write(codec, 0x14, 0x05);
		snd_soc_write(codec, 0x15, 0x06);
		snd_soc_write(codec, 0x16, 0x33);
		snd_soc_write(codec, 0x17, 0x18);
		snd_soc_write(codec, 0x18, 0x02);
		snd_soc_write(codec, 0x1A, 0x00);
		snd_soc_write(codec, 0x1B, 0x00);
		snd_soc_write(codec, 0x26, 0x12);
		snd_soc_write(codec, 0x27, 0x80);
		snd_soc_write(codec, 0x2A, 0x80);
		snd_soc_write(codec, 0x03, 0x00);
		snd_soc_write(codec, 0x02, 0x00);
		snd_soc_write(codec, 0x19, 0x20);
		snd_soc_write(codec, 0x04, 0x2c);
		msleep(100);
		snd_soc_write(codec, 0x2e, 0x00);
		snd_soc_write(codec, 0x2f, 0x00);
		snd_soc_write(codec, 0x30, 0x08);
		snd_soc_write(codec, 0x31, 0x08);
		msleep(200);
		snd_soc_write(codec, 0x30, 0x0f);
		snd_soc_write(codec, 0x31, 0x0f);
		msleep(200);
		snd_soc_write(codec, 0x30, 0x1e);
		snd_soc_write(codec, 0x31, 0x1e);
		snd_soc_write(codec, 0x32, 0x03);
	}
	es8323_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	snd_soc_add_codec_controls(codec, es8323_snd_controls,
				   ARRAY_SIZE(es8323_snd_controls));
	snd_soc_dapm_new_controls(dapm, es8323_dapm_widgets,
				  ARRAY_SIZE(es8323_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, intercon, ARRAY_SIZE(intercon));
	pr_info("es8323 init ok, drv version = %s\n", ES8323_VERSION);
	return ret;
}

/* power down chip */
static int es8323_remove(struct snd_soc_codec *codec)
{
	es8323_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_es8323 = {
	.probe = es8323_probe,
	.remove = es8323_remove,
	.set_bias_level = es8323_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(es8323_reg),
	.reg_word_size = sizeof(u16),
	.reg_cache_default = es8323_reg,
	.read = es8323_read_reg_cache,
	.write = es8323_write,
};

static int es8323_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct es8323_priv *es8323;
	int ret;

	es8323 = kzalloc(sizeof(struct es8323_priv), GFP_KERNEL);
	if (es8323 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, es8323);
	es8323->control_type = SND_SOC_I2C;

	ret = snd_soc_register_codec(&i2c->dev,
				     &soc_codec_dev_es8323, &es8323_dai, 1);
	if (ret < 0)
		kfree(es8323);
	return ret;
}

static int es8323_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct of_device_id es8323_of_match[] = {
	{.compatible = "es, es8323",},
	{},
};

static const struct i2c_device_id es8323_i2c_id[] = {
	{"es8323", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, es8323_i2c_id);

static struct i2c_driver es8323_i2c_driver = {
	.driver = {
		   .name = "es8323",
		   .owner = THIS_MODULE,
		   .of_match_table = es8323_of_match,
#ifdef CONFIG_PM
		   .pm = &es8323_dev_pm_ops,
#endif
		   },
	.probe = es8323_i2c_probe,
	.remove = es8323_i2c_remove,
	.shutdown = es8323_shutdown,
	.id_table = es8323_i2c_id,
};
module_i2c_driver(es8323_i2c_driver);

MODULE_DESCRIPTION("ASoC es8323 driver");
MODULE_AUTHOR("Liam Girdwood");
MODULE_LICENSE("GPL");

