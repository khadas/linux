/*
 * sound/soc/aml/g9tv/aml_g9tv.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#define pr_fmt(fmt) "aml_g9tv: " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/tas57xx.h>
#include <linux/switch.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/sound/audin_regs.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/io.h>

#include "aml_i2s.h"
#include "aml_audio_hw.h"
#include "aml_g9tv.h"

#define DRV_NAME "aml_snd_card_g9tv"

int aml_audio_Hardware_resample = 0;
unsigned int clk_rate = 0;

static const char *const audio_in_source_texts[] = { "LINEIN", "ATV", "HDMI" };

static const struct soc_enum audio_in_source_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_in_source_texts),
			audio_in_source_texts);

static int aml_audio_get_in_source(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int value = aml_read_cbus(AUDIN_SOURCE_SEL) & 0x3;

	if (value == 0)
		ucontrol->value.enumerated.item[0] = 0;
	else if (value == 1)
		ucontrol->value.enumerated.item[0] = 1;
	else if (value == 2)
		ucontrol->value.enumerated.item[0] = 2;
	return 0;
}

static int aml_audio_set_in_source(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] == 0) {
		/* select external codec output as I2S source */
		aml_write_cbus(AUDIN_SOURCE_SEL, 0);
		audio_in_source = 0;
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		/* select ATV output as I2S source */
		aml_write_cbus(AUDIN_SOURCE_SEL, 1);
		audio_in_source = 1;
	} else if (ucontrol->value.enumerated.item[0] == 2) {
		/* select HDMI-rx as I2S source */
		/* [14:12]cntl_hdmirx_chsts_sel: */
		/* 0=Report chan1 status; 1=Report chan2 status */
		/* [11:8] cntl_hdmirx_chsts_en */
		/* [5:4] spdif_src_sel:*/
		/* 1=Select HDMIRX SPDIF output as AUDIN source */
		/* [1:0] i2sin_src_sel: */
		/*2=Select HDMIRX I2S output as AUDIN source */
		aml_write_cbus(AUDIN_SOURCE_SEL, (0 << 12) |
			       (0xf << 8) | (1 << 4) | (2 << 0));
		audio_in_source = 2;
	}
	set_i2s_source(audio_in_source);
	return 0;
}

/* i2s audio format detect: LPCM or NONE-LPCM */
static const char *const i2s_audio_type_texts[] = {
	"LPCM", "NONE-LPCM", "UN-KNOWN"
};
static const struct soc_enum i2s_audio_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(i2s_audio_type_texts),
			i2s_audio_type_texts);

static int aml_i2s_audio_type_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ch_status = 0;

	if ((aml_read_cbus(AUDIN_DECODE_CONTROL_STATUS) >> 24) & 0x1) {
		ch_status = aml_read_cbus(AUDIN_DECODE_CHANNEL_STATUS_A_0);
		if (ch_status & 2)
			ucontrol->value.enumerated.item[0] = 1;
		else
			ucontrol->value.enumerated.item[0] = 0;
	} else {
		ucontrol->value.enumerated.item[0] = 2;
	}
	return 0;
}

static int aml_i2s_audio_type_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

/* spdif in audio format detect: LPCM or NONE-LPCM */
struct sppdif_audio_info {
	unsigned char aud_type;
	/*IEC61937 package presamble Pc value*/
	short pc;
	char *aud_type_str;
};
static const char *const spdif_audio_type_texts[] = {
	"LPCM",
	"AC3",
	"EAC3",
	"DTS",
	"DTS-HD",
	"TRUEHD",
};
static const struct sppdif_audio_info type_texts[] = {
	{0, 0, "LPCM"},
	{1, 0x1, "AC3"},
	{2, 0x15, "EAC3"},
	{3, 0xb, "DTS-I"},
	{3, 0x10c, "DTS-II"},
	{3, 0x20d, "DTS-III"},
	{3, 0x411, "DTS-IV"},
	{4, 0, "DTS-HD"},
	{5, 0x16, "TRUEHD"},
};
static const struct soc_enum spdif_audio_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(spdif_audio_type_texts),
			spdif_audio_type_texts);

static int aml_spdif_audio_type_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int audio_type = 0;
	int i;
	int total_num = sizeof(type_texts)/sizeof(struct sppdif_audio_info);
	int pc = aml_read_cbus(AUDIN_SPDIF_NPCM_PCPD)>>16;
	pc = pc&0xff;
	for (i = 0; i < total_num; i++) {
		if (pc == type_texts[i].pc) {
			audio_type = type_texts[i].aud_type;
			break;
		}
	}
	ucontrol->value.enumerated.item[0] = audio_type;
	return 0;
}

static int aml_spdif_audio_type_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int hardware_resample_locked_flag = 0;
#define RESAMPLE_BUFFER_SOURCE 1
/*Cnt_ctrl = mclk/fs_out-1 ; fest 256fs */
#define RESAMPLE_CNT_CONTROL 255

static int hardware_resample_enable(int input_sr)
{
	u16 Avg_cnt_init = 0;
	unsigned int clk_rate = clk81;

	if (hardware_resample_locked_flag == 1)
		return 0;

	if (input_sr < 8000 || input_sr > 48000) {
		pr_err("Error input sample rate,input_sr = %d!\n", input_sr);
		return -1;
	}

	Avg_cnt_init = (u16)(clk_rate * 4 / input_sr);
	pr_info("clk_rate = %u, input_sr = %d, Avg_cnt_init = %u\n",
		clk_rate, input_sr, Avg_cnt_init);

	aml_write_cbus(AUD_RESAMPLE_CTRL0, (1 << 31));
	aml_write_cbus(AUD_RESAMPLE_CTRL0, 0);
	aml_write_cbus(AUD_RESAMPLE_CTRL0,
				(1 << 29)
				| (1 << 28)
				| (0 << 26)
				| (RESAMPLE_CNT_CONTROL << 16)
				| Avg_cnt_init);

	return 0;
}

static int hardware_resample_disable(void)
{
	aml_write_cbus(AUD_RESAMPLE_CTRL0, 0);
	return 0;
}

static const char *const hardware_resample_texts[] = {
	"Disable",
	"Enable:48K",
	"Enable:44K",
	"Enable:32K",
	"Lock Resample",
	"Unlock Resample"
};

static const struct soc_enum hardware_resample_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(hardware_resample_texts),
			hardware_resample_texts);

static int aml_hardware_resample_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = aml_audio_Hardware_resample;
	return 0;
}

static int aml_hardware_resample_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] == 0) {
		hardware_resample_disable();
		aml_audio_Hardware_resample = 0;
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		hardware_resample_enable(48000);
		aml_audio_Hardware_resample = 1;
	} else if (ucontrol->value.enumerated.item[0] == 2) {
		hardware_resample_enable(44100);
		aml_audio_Hardware_resample = 2;
	} else if (ucontrol->value.enumerated.item[0] == 3) {
		hardware_resample_enable(32000);
		aml_audio_Hardware_resample = 3;
	} else if (ucontrol->value.enumerated.item[0] == 4) {
		hardware_resample_disable();
		aml_audio_Hardware_resample = 4;
		hardware_resample_locked_flag = 1;
	} else if (ucontrol->value.enumerated.item[0] == 5) {
		hardware_resample_locked_flag = 0;
		hardware_resample_enable(48000);
		aml_audio_Hardware_resample = 5;
	}
	return 0;
}

static const char *const output_swap_texts[] = { "L/R", "L/L", "R/R", "R/L" };

static const struct soc_enum output_swap_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(output_swap_texts),
			output_swap_texts);

static int aml_output_swap_get_enum(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = read_i2s_mute_swap_reg();
	return 0;
}

static int aml_output_swap_set_enum(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	audio_i2s_swap_left_right(ucontrol->value.enumerated.item[0]);
	return 0;
}

static const struct snd_soc_dapm_widget aml_asoc_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("LINEIN"),
	SND_SOC_DAPM_OUTPUT("LINEOUT"),
};

int audio_in_GPIO = 0;
struct gpio_desc *av_source;
static const char * const audio_in_switch_texts[] = { "AV", "Karaok"};

static const struct soc_enum audio_in_switch_enum = SOC_ENUM_SINGLE(
		SND_SOC_NOPM, 0, ARRAY_SIZE(audio_in_switch_texts),
		audio_in_switch_texts);

static int aml_get_audio_in_switch(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {

	if (audio_in_GPIO == 0) {
		ucontrol->value.enumerated.item[0] = 0;
		pr_info("audio in source: AV\n");
	} else if (audio_in_GPIO == 1) {
		ucontrol->value.enumerated.item[0] = 1;
		pr_info("audio in source: Karaok\n");
	}
	return 0;
}

static int aml_set_audio_in_switch(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol) {
	if (ucontrol->value.enumerated.item[0] == 0) {
		gpiod_direction_output(av_source,
					   GPIOF_OUT_INIT_LOW);
		audio_in_GPIO = 0;
		pr_info("Set audio in source: AV\n");
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		gpiod_direction_output(av_source,
					   GPIOF_OUT_INIT_HIGH);
		audio_in_GPIO = 1;
		pr_info("Set audio in source: Karaok\n");
	}
	return 0;
}

static const struct snd_kcontrol_new av_controls[] = {
	SOC_ENUM_EXT("AudioIn Switch",
			 audio_in_switch_enum,
			 aml_get_audio_in_switch,
			 aml_set_audio_in_switch),
};

static const struct snd_kcontrol_new aml_g9tv_controls[] = {
	SOC_ENUM_EXT("Audio In Source",
		     audio_in_source_enum,
		     aml_audio_get_in_source,
		     aml_audio_set_in_source),

	SOC_ENUM_EXT("I2SIN Audio Type",
		     i2s_audio_type_enum,
		     aml_i2s_audio_type_get_enum,
		     aml_i2s_audio_type_set_enum),

	SOC_ENUM_EXT("SPDIFIN Audio Type",
		     spdif_audio_type_enum,
		     aml_spdif_audio_type_get_enum,
		     aml_spdif_audio_type_set_enum),

	SOC_ENUM_EXT("Hardware resample enable",
		     hardware_resample_enum,
		     aml_hardware_resample_get_enum,
		     aml_hardware_resample_set_enum),

	SOC_ENUM_EXT("Output Swap",
		     output_swap_enum,
		     aml_output_swap_get_enum,
		     aml_output_swap_set_enum),
};

static int aml_suspend_pre(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;

	pr_info("enter %s\n", __func__);
	p_aml_audio = snd_soc_card_get_drvdata(card);
	return 0;
}

static int aml_suspend_post(struct snd_soc_card *card)
{
	pr_info("enter %s\n", __func__);
	return 0;
}

static int aml_resume_pre(struct snd_soc_card *card)
{
	pr_info("enter %s\n", __func__);
	return 0;
}

static int aml_resume_post(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;

	pr_info("enter %s\n", __func__);
	p_aml_audio = snd_soc_card_get_drvdata(card);

	return 0;
}

static int aml_asoc_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai,
				  SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_IB_NF
				  | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		pr_err("%s: set cpu dai fmt failed!\n", __func__);
		return ret;
	}

	return 0;
}

static struct snd_soc_ops aml_asoc_ops = {
	.hw_params	= aml_asoc_hw_params,
};

static int aml_asoc_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct aml_audio_private_data *p_aml_audio;
	int ret = 0;

	pr_info("enter %s\n", __func__);
	p_aml_audio = snd_soc_card_get_drvdata(card);

	ret = snd_soc_add_card_controls(codec->card, aml_g9tv_controls,
					ARRAY_SIZE(aml_g9tv_controls));
	if (ret)
		return ret;

	/* Add specific widgets */
	snd_soc_dapm_new_controls(dapm, aml_asoc_dapm_widgets,
				  ARRAY_SIZE(aml_asoc_dapm_widgets));

	return 0;
}

static void aml_g9tv_pinmux_init(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;

	p_aml_audio = snd_soc_card_get_drvdata(card);
	p_aml_audio->pin_ctl =
		devm_pinctrl_get_select(card->dev, "aml_snd_g9tv");
	if (IS_ERR(p_aml_audio->pin_ctl)) {
		pr_info("%s, aml_g9tv_pinmux_init error!\n", __func__);
		return;
	}

	p_aml_audio->mute_desc = gpiod_get(card->dev, "mute_gpio");
	if (!IS_ERR(p_aml_audio->mute_desc)) {
		pr_info("%s, make avmute gpio high!\n", __func__);
		gpiod_direction_output(p_aml_audio->mute_desc,
					   GPIOF_OUT_INIT_HIGH);
	}

	av_source = gpiod_get(card->dev, "av_source");
	if (!IS_ERR(av_source)) {
		pr_info("%s, make av_source gpio low!\n", __func__);
		gpiod_direction_output(av_source, GPIOF_OUT_INIT_LOW);
		snd_soc_add_card_controls(card, av_controls,
					ARRAY_SIZE(av_controls));
	}
	return;
}

static int aml_card_dai_parse_of(struct device *dev,
				 struct snd_soc_dai_link *dai_link,
				 int (*init)(
					 struct snd_soc_pcm_runtime *rtd),
				 struct device_node *cpu_node,
				 struct device_node *codec_node,
				 struct device_node *plat_node)
{
	int ret;

	/* get cpu dai->name */
	ret = snd_soc_of_get_dai_name(cpu_node, &dai_link->cpu_dai_name);
	if (ret < 0)
		goto parse_error;

	/* get codec dai->name */
	ret = snd_soc_of_get_dai_name(codec_node, &dai_link->codec_dai_name);
	if (ret < 0)
		goto parse_error;

	dai_link->name = dai_link->stream_name = dai_link->cpu_dai_name;
	dai_link->codec_of_node = of_parse_phandle(codec_node, "sound-dai", 0);
	dai_link->platform_of_node = plat_node;
	dai_link->init = init;

	return 0;

parse_error:
	return ret;
}

struct snd_soc_aux_dev g9tv_audio_aux_dev;
static struct snd_soc_codec_conf g9tv_audio_codec_conf[] = {
	{
		.name_prefix = "AMP",
	},
};
static struct codec_probe_priv prob_priv;
static struct codec_info codec_info_aux;

static int get_audio_codec_i2c_info(struct device_node *p_node,
				struct aml_audio_codec_info *audio_codec_dev)
{
	const char *str;
	int ret = 0;
	unsigned i2c_addr;

	ret = of_property_read_string(p_node, "codec_name",
				      &audio_codec_dev->name);
	if (ret) {
		pr_info("get audio codec name failed!\n");
		goto err_out;
	}

	ret = of_property_match_string(p_node, "status", "okay");
	if (ret) {
		pr_info("%s:this audio codec is disabled!\n",
			audio_codec_dev->name);
		goto err_out;
	}

	pr_debug("use audio aux codec %s\n", audio_codec_dev->name);

	ret = of_property_read_string(p_node, "i2c_bus", &str);
	if (ret) {
		pr_err("%s: faild to get i2c_bus str,use default i2c bus!\n",
		       audio_codec_dev->name);
		audio_codec_dev->i2c_bus_type = AML_I2C_BUS_D;
	} else {
		if (!strncmp(str, "i2c_bus_a", 9))
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_A;
		else if (!strncmp(str, "i2c_bus_b", 9))
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_B;
		else if (!strncmp(str, "i2c_bus_c", 9))
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_C;
		else if (!strncmp(str, "i2c_bus_d", 9))
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_D;
		else if (!strncmp(str, "i2c_bus_ao", 10))
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_AO;
		else
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_D;
	}

	ret = of_property_read_u32(p_node, "i2c_addr", &i2c_addr);
	if (!ret)
		audio_codec_dev->i2c_addr = i2c_addr;
	/*pr_info("audio aux codec addr: 0x%x, audio codec i2c bus: %d\n",
	 *      audio_codec_dev->i2c_addr, audio_codec_dev->i2c_bus_type);*/
err_out:
	return ret;
}

static char drc1_table[15] = "drc1_table_0";
static char drc1_tko_table[20] = "drc1_tko_table_0";
static char drc2_table[15] = "drc2_table_0";
static char drc2_tko_table[20] = "drc2_tko_table_0";
static int __init aml_drc_type_select(char *s)
{
	char *sel = s;

	if (NULL != s) {
		sprintf(drc1_table, "%s%s", "drc1_table_", sel);
		sprintf(drc1_tko_table, "%s%s", "drc1_tko_table_", sel);
		sprintf(drc2_table, "%s%s", "drc2_table_", sel);
		sprintf(drc2_tko_table, "%s%s", "drc2_tko_table_", sel);
		pr_info("select drc type: %s\n", sel);
	}
	return 0;
}
__setup("amp_drc_type=", aml_drc_type_select);

static char table[10] = "table_0";
static char wall[10] = "wall_0";
static char sub_bq_table[20] = "sub_bq_table_0";
static int __init aml_eq_type_select(char *s)
{
	char *sel = s;

	if (NULL != s) {
		sprintf(table, "%s%s", "table_", sel);
		sprintf(wall, "%s%s", "wall_", sel);
		sprintf(sub_bq_table, "%s%s", "sub_bq_table_", sel);
		pr_info("select eq type: %s\n", sel);
	}
	return 0;
}
__setup("amp_eq_type=", aml_eq_type_select);

static void *alloc_and_get_data_array(struct device_node *p_node, char *str,
				      int *lenp)
{
	int ret = 0, length = 0;
	char *p = NULL;

	if (of_find_property(p_node, str, &length) == NULL) {
		pr_err("DTD of %s not found!\n", str);
		goto exit;
	}
	pr_debug("prop name=%s,length=%d\n", str, length);
	p = kzalloc(length * sizeof(char *), GFP_KERNEL);
	if (p == NULL) {
		pr_err("ERROR, NO enough mem for %s!\n", str);
		length = 0;
		goto exit;
	}

	ret = of_property_read_u8_array(p_node, str, p, length);
	if (ret) {
		pr_err("no of property %s!\n", str);
		kfree(p);
		p = NULL;
		goto exit;
	}

	*lenp = length;

exit: return p;
}

static int of_get_eq_pdata(struct tas57xx_platform_data *pdata,
			   struct device_node *p_node)
{
	int length = 0;
	char *regs = NULL;
	int ret = 0;

	ret = of_property_read_u32(p_node, "eq_enable", &pdata->eq_enable);
	if (pdata->eq_enable == 0 || ret != 0) {
		pr_err("Fail to get eq_enable node or EQ disable!\n");
		return -2;
	}

	prob_priv.num_eq = 2;
	pdata->num_eq_cfgs = prob_priv.num_eq;

	prob_priv.eq_configs = kzalloc(
		prob_priv.num_eq * sizeof(struct tas57xx_eq_cfg), GFP_KERNEL);

	regs = alloc_and_get_data_array(p_node, table, &length);
	if (regs == NULL) {
		kfree(prob_priv.eq_configs);
		return -2;
	}
	strncpy(prob_priv.eq_configs[0].name, table, NAME_SIZE);
	prob_priv.eq_configs[0].regs = regs;
	prob_priv.eq_configs[0].reg_bytes = length;

	regs = alloc_and_get_data_array(p_node, wall, &length);
	if (regs == NULL) {
		kfree(prob_priv.eq_configs);
		return -2;
	}
	strncpy(prob_priv.eq_configs[1].name, wall, NAME_SIZE);
	prob_priv.eq_configs[1].regs = regs;
	prob_priv.eq_configs[1].reg_bytes = length;

	pdata->eq_cfgs = prob_priv.eq_configs;
	return 0;
}

static int of_get_drc_pdata(struct tas57xx_platform_data *pdata,
			    struct device_node *p_node)
{
	int length = 0;
	char *pd = NULL;
	int ret = 0;

	ret = of_property_read_u32(p_node, "drc_enable", &pdata->drc_enable);
	if (pdata->drc_enable == 0 || ret != 0) {
		pr_err("Fail to get drc_enable node or DRC disable!\n");
		return -2;
	}

	/* get drc1 table */
	pd = alloc_and_get_data_array(p_node, drc1_table, &length);
	if (pd == NULL)
		return -2;
	pdata->custom_drc1_table_len = length;
	pdata->custom_drc1_table = pd;

	/* get drc1 tko table */
	length = 0;
	pd = NULL;

	pd = alloc_and_get_data_array(p_node, drc1_tko_table, &length);
	if (pd == NULL)
		return -2;
	pdata->custom_drc1_tko_table_len = length;
	pdata->custom_drc1_tko_table = pd;
	pdata->enable_ch1_drc = 1;

	/* get drc2 table */
	length = 0;
	pd = NULL;
	pd = alloc_and_get_data_array(p_node, drc2_table, &length);
	if (pd == NULL)
		return -1;
	pdata->custom_drc2_table_len = length;
	pdata->custom_drc2_table = pd;

	/* get drc2 tko table */
	length = 0;
	pd = NULL;
	pd = alloc_and_get_data_array(p_node, drc2_tko_table, &length);
	if (pd == NULL)
		return -1;
	pdata->custom_drc2_tko_table_len = length;
	pdata->custom_drc2_tko_table = pd;
	pdata->enable_ch2_drc = 1;

	return 0;
}

static int of_get_init_pdata(struct tas57xx_platform_data *pdata,
			     struct device_node *p_node)
{
	int length = 0;
	char *pd = NULL;

	pd = alloc_and_get_data_array(p_node, "input_mux_reg_buf", &length);
	if (pd == NULL) {
		pr_err("%s : can't get input_mux_reg_buf\n", __func__);
		return -1;
	}

	/*Now only support 0x20 input mux init*/
	pdata->num_init_regs = length;
	pdata->init_regs = pd;

	if (of_property_read_u32(p_node, "master_vol",
				 &pdata->custom_master_vol)) {
		pr_err("%s fail to get master volume\n", __func__);
		return -1;
	}

	return 0;
}

static int of_get_resetpin_pdata(struct tas57xx_platform_data *pdata,
				 struct device_node *p_node)
{
	struct gpio_desc *reset_desc;

	reset_desc = of_get_named_gpiod_flags(p_node, "reset_pin", 0, NULL);
	if (IS_ERR(reset_desc)) {
		pr_err("%s fail to get reset pin from dts!\n", __func__);
	} else {
		int reset_pin = desc_to_gpio(reset_desc);
		gpio_request(reset_pin, NULL);
		gpio_direction_output(reset_pin, GPIOF_OUT_INIT_LOW);
		pdata->reset_pin = reset_pin;
		pr_info("%s pdata->reset_pin = %d!\n", __func__,
			pdata->reset_pin);
	}
	return 0;
}

static int of_get_phonepin_pdata(struct tas57xx_platform_data *pdata,
				 struct device_node *p_node)
{
	struct gpio_desc *phone_desc;
	phone_desc = of_get_named_gpiod_flags(p_node, "phone_pin", 0, NULL);
	if (IS_ERR(phone_desc)) {
		pr_err("%s fail to get phone pin from dts!\n", __func__);
	} else {
		int phone_pin = desc_to_gpio(phone_desc);
		gpio_request(phone_pin, NULL);
		gpio_direction_output(phone_pin, GPIOF_OUT_INIT_LOW);
		pdata->phone_pin = phone_pin;
		pr_info("%s pdata->phone_pin = %d!\n", __func__,
			pdata->phone_pin);
	}
	return 0;
}
static int of_get_scanpin_pdata(struct tas57xx_platform_data *pdata,
				 struct device_node *p_node)
{
	struct gpio_desc *scan_desc;
	scan_desc = of_get_named_gpiod_flags(p_node, "scan_pin", 0, NULL);
	if (IS_ERR(scan_desc)) {
		pr_err("%s fail to get scan pin from dts!\n", __func__);
	} else {
		int scan_pin = desc_to_gpio(scan_desc);
		gpio_request(scan_pin, NULL);
		gpio_direction_input(scan_pin);
		pdata->scan_pin = scan_pin;
		pr_info("%s pdata->scan_pin = %d!\n", __func__,
			pdata->scan_pin);
	}
	return 0;
}

static int codec_get_of_pdata(struct tas57xx_platform_data *pdata,
			      struct device_node *p_node)
{
	int ret = 0;

	ret = of_get_resetpin_pdata(pdata, p_node);
	if (ret)
		pr_info("codec reset pin is not found in dts\n");
	ret = of_get_phonepin_pdata(pdata, p_node);
	if (ret)
		pr_info("codec phone pin is not found in dtd\n");

	ret = of_get_scanpin_pdata(pdata, p_node);
	if (ret)
		pr_info("codec scanp pin is not found in dtd\n");

	ret = of_get_drc_pdata(pdata, p_node);
	if (ret == -2)
		pr_info("codec DRC configs are not found in dts\n");

	ret = of_get_eq_pdata(pdata, p_node);
	if (ret)
		pr_info("codec EQ configs are not found in dts\n");

	ret = of_get_init_pdata(pdata, p_node);
	if (ret)
		pr_info("codec init configs are not found in dts\n");
	return ret;
}

static int aml_aux_dev_parse_of(struct snd_soc_card *card)
{
	struct device_node *audio_codec_node = card->dev->of_node;
	struct device_node *child;
	struct i2c_board_info board_info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	struct aml_audio_codec_info temp_audio_codec;
	struct tas57xx_platform_data *pdata;
	char tmp[I2C_NAME_SIZE];
	const char *aux_dev;
	if (of_property_read_string(audio_codec_node, "aux_dev", &aux_dev)) {
		pr_info("no aux dev!\n");
		return -ENODEV;
	}
	pr_info("aux name = %s\n", aux_dev);
	child = of_get_child_by_name(audio_codec_node, aux_dev);
	if (child == NULL) {
		pr_info("error: failed to find aux dev node %s\n", aux_dev);
		return -1;
	}

	memset(&temp_audio_codec, 0, sizeof(struct aml_audio_codec_info));
	/*pr_info("%s, child name:%s\n", __func__, child->name);*/

	if (get_audio_codec_i2c_info(child, &temp_audio_codec) == 0) {
		memset(&board_info, 0, sizeof(board_info));
		strncpy(board_info.type, temp_audio_codec.name, I2C_NAME_SIZE);
		adapter = i2c_get_adapter(temp_audio_codec.i2c_bus_type);
		board_info.addr = temp_audio_codec.i2c_addr;
		board_info.platform_data = &temp_audio_codec;
		client = i2c_new_device(adapter, &board_info);
		snprintf(tmp, I2C_NAME_SIZE, "%s", temp_audio_codec.name);
		strlcpy(codec_info_aux.name, tmp, I2C_NAME_SIZE);
		snprintf(tmp, I2C_NAME_SIZE, "%s.%s", temp_audio_codec.name,
				dev_name(&client->dev));
		strlcpy(codec_info_aux.name_bus, tmp, I2C_NAME_SIZE);

		g9tv_audio_aux_dev.name = codec_info_aux.name;
		g9tv_audio_aux_dev.codec_name = codec_info_aux.name_bus;
		g9tv_audio_codec_conf[0].dev_name = codec_info_aux.name_bus;

		card->aux_dev = &g9tv_audio_aux_dev,
		card->num_aux_devs = 1,
		card->codec_conf = g9tv_audio_codec_conf,
		card->num_configs = ARRAY_SIZE(g9tv_audio_codec_conf),

		pdata =
			kzalloc(sizeof(struct tas57xx_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			pr_err("error: malloc tas57xx_platform_data!\n");
			return -ENOMEM;
		}
		codec_get_of_pdata(pdata, child);
		client->dev.platform_data = pdata;
	}
	return 0;
}
static int aml_card_dais_parse_of(struct snd_soc_card *card)
{
	struct device_node *np = card->dev->of_node;
	struct device_node *cpu_node, *codec_node, *plat_node;
	struct device *dev = card->dev;
	struct snd_soc_dai_link *dai_links;
	int num_dai_links, cpu_num, codec_num, plat_num;
	int i, ret;

	int (*init)(struct snd_soc_pcm_runtime *rtd);

	ret = of_count_phandle_with_args(np, "cpu_list", NULL);
	if (ret < 0) {
		dev_err(dev, "AML sound card no cpu_list errno: %d\n", ret);
		goto err;
	} else {
		cpu_num = ret;
	}
	ret = of_count_phandle_with_args(np, "codec_list", NULL);
	if (ret < 0) {
		dev_err(dev, "AML sound card no codec_list errno: %d\n", ret);
		goto err;
	} else {
		codec_num = ret;
	}
	ret = of_count_phandle_with_args(np, "plat_list", NULL);
	if (ret < 0) {
		dev_err(dev, "AML sound card no plat_list errno: %d\n", ret);
		goto err;
	} else {
		plat_num = ret;
	}
	if ((cpu_num == codec_num) && (cpu_num == plat_num)) {
		num_dai_links = cpu_num;
	} else {
		dev_err(dev,
			"AML sound card cpu_dai num, codec_dai num, platform num don't match: %d\n",
			ret);
		ret = -EINVAL;
		goto err;
	}

	dai_links =
		devm_kzalloc(dev,
			     num_dai_links * sizeof(struct snd_soc_dai_link),
			     GFP_KERNEL);
	if (!dai_links) {
		dev_err(dev, "Can't allocate snd_soc_dai_links\n");
		ret = -ENOMEM;
		goto err;
	}
	card->dai_link = dai_links;
	card->num_links = num_dai_links;
	for (i = 0; i < num_dai_links; i++) {
		init = NULL;
		/* CPU sub-node */
		cpu_node = of_parse_phandle(np, "cpu_list", i);
		if (cpu_node < 0) {
			dev_err(dev, "parse aml sound card cpu list error\n");
			return -EINVAL;
		}
		/* CODEC sub-node */
		codec_node = of_parse_phandle(np, "codec_list", i);
		if (codec_node < 0) {
			dev_err(dev, "parse aml sound card codec list error\n");
			return ret;
		}
		/* Platform sub-node */
		plat_node = of_parse_phandle(np, "plat_list", i);
		if (plat_node < 0) {
			dev_err(dev,
				"parse aml sound card platform list error\n");
			return ret;
		}
		if (i == 0)
			init = aml_asoc_init;

		ret =
			aml_card_dai_parse_of(dev, &dai_links[i], init,
					      cpu_node,
					      codec_node, plat_node);

		dai_links[0].ops = &aml_asoc_ops;
	}

err:
	return ret;
}

static int aml_g9tv_audio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct snd_soc_card *card;
	struct aml_audio_private_data *p_aml_audio;
	int ret;

	p_aml_audio =
		devm_kzalloc(dev, sizeof(struct aml_audio_private_data),
			     GFP_KERNEL);
	if (!p_aml_audio) {
		dev_err(&pdev->dev, "Can't allocate aml_audio_private_data\n");
		ret = -ENOMEM;
		goto err;
	}

	card = devm_kzalloc(dev, sizeof(struct snd_soc_card), GFP_KERNEL);
	if (!card) {
		dev_err(dev, "Can't allocate snd_soc_card\n");
		ret = -ENOMEM;
		goto err;
	}

	snd_soc_card_set_drvdata(card, p_aml_audio);

	card->dev = dev;
	platform_set_drvdata(pdev, card);
	ret = snd_soc_of_parse_card_name(card, "aml_sound_card,name");
	if (ret < 0) {
		dev_err(dev, "no specific snd_soc_card name\n");
		goto err;
	}

	ret = aml_card_dais_parse_of(card);
	if (ret < 0) {
		dev_err(dev, "parse aml sound card dais error %d\n", ret);
		goto err;
	}
	aml_aux_dev_parse_of(card);

	card->suspend_pre = aml_suspend_pre,
	card->suspend_post = aml_suspend_post,
	card->resume_pre = aml_resume_pre,
	card->resume_post = aml_resume_post,

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret < 0) {
		dev_err(dev, "register aml sound card error %d\n", ret);
		goto err;
	}

	aml_g9tv_pinmux_init(card);
	return 0;
err:
	dev_err(dev, "Can't probe snd_soc_card\n");
	return ret;
}

static const struct of_device_id amlogic_audio_of_match[] = {
	{ .compatible = "aml, aml_snd_g9tv", },
	{},
};

static struct platform_driver aml_g9tv_audio_driver = {
	.driver			= {
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = amlogic_audio_of_match,
		.pm = &snd_soc_pm_ops,
	},
	.probe			= aml_g9tv_audio_probe,
};

module_platform_driver(aml_g9tv_audio_driver);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("AML_G9TV audio machine Asoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
