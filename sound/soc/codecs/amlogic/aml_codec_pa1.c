// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/amlogic/aml_gpio_consumer.h>

#include <sound/initval.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "aml_codec_pa1.h"
#define PA1_DRV_NAME    "pa1_acodec"
#define PA1_RATES	(SNDRV_PCM_RATE_8000 | \
			SNDRV_PCM_RATE_11025 | \
			SNDRV_PCM_RATE_16000 | \
			SNDRV_PCM_RATE_22050 | \
			SNDRV_PCM_RATE_32000 | \
			SNDRV_PCM_RATE_44100 | \
			SNDRV_PCM_RATE_48000 | \
			SNDRV_PCM_RATE_64000 | \
			SNDRV_PCM_RATE_88200 | \
			SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_176400 | \
			SNDRV_PCM_RATE_192000)
#define PA1_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE)

/* When multiple Codecs share a PDN, this flag is used to record the PDN status */
static bool reset_pin_setting;

struct pa1_acodec_priv {
	struct regmap *regmap;
	struct pa1_acodec_platform_data *pdata;
	int mute;
	unsigned int vol;
	unsigned int dsp_status;
	struct snd_soc_component *component;
	unsigned int pa1_page;
};

const struct regmap_config pa1_acodec_regmap = {
	.reg_bits = 8,
	.val_bits = 32,
	.reg_stride = 1,
	.cache_type = REGCACHE_NONE,
	.reg_format_endian = REGMAP_ENDIAN_LITTLE,
	.val_format_endian = REGMAP_ENDIAN_LITTLE,
};

static const DECLARE_TLV_DB_SCALE(pa1_vol_tlv, -12276, 12, 1);

static int top_conversion_to_dsp(struct snd_soc_component *component)
{
	struct pa1_acodec_priv *pa1_acodec = snd_soc_component_get_drvdata(component);

	if (pa1_acodec->pa1_page == 0) {
		snd_soc_component_write(component, PA1_PAGE_NUM, 0x01);
		pa1_acodec->pa1_page = 1;
	}

	return 0;
}

static int dsp_conversion_to_top(struct snd_soc_component *component)
{
	struct pa1_acodec_priv *pa1_acodec = snd_soc_component_get_drvdata(component);

	if (pa1_acodec->pa1_page == 1) {
		snd_soc_component_write(component, PA1_PAGE_NUM, 0x0);
		pa1_acodec->pa1_page = 0;
	}

	return 0;
}

static int pa1_acodec_mute(struct snd_soc_component *component, int mute)
{
	unsigned int pa1_mute_ctrl_value = 0;

	if (mute) {
		/* mute master */
		pa1_mute_ctrl_value = 0x4;
	} else {
		/* unmute */
		pa1_mute_ctrl_value = 0x0;
	}

	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AED_MUTE_CTRL, pa1_mute_ctrl_value);

	return 0;
}

static int pa1_mixer_aed_read(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value = 0;
	int ret;

	top_conversion_to_dsp(component);
	ret = snd_soc_component_read(component, reg, &value);
	value = (value >> shift) & max;
	if (invert)
		value = (~value) & max;
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int pa1_mixer_aed_write(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value = ucontrol->value.integer.value[0];
	unsigned int new_val = 0;
	int ret;

	top_conversion_to_dsp(component);
	ret = snd_soc_component_read(component, reg, &new_val);
	if (invert)
		value = (~value) & max;
	max = ~(max << shift);
	new_val &= max;
	new_val |= (value << shift);

	snd_soc_component_write(component, reg, new_val);

	return 0;
}

static int pa1_mixer_aed_get_mute(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct pa1_acodec_priv *pa1_acodec = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = pa1_acodec->mute;

	return 0;
}

static int pa1_mixer_aed_set_mute(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct pa1_acodec_priv *pa1_acodec = snd_soc_component_get_drvdata(component);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int new_val;

	top_conversion_to_dsp(component);
	new_val = (unsigned int)snd_soc_component_read32(component, reg);

	pa1_acodec->mute = ucontrol->value.integer.value[0];
	max = ~(max << shift);
	new_val &= max;
	new_val |= ((pa1_acodec->mute) << shift);

	snd_soc_component_write(component, reg, new_val);

	return 0;
}

void pa1_get_mixer_gain_value(struct snd_soc_component *component, int addr, unsigned int *value)
{
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, addr);

	snd_soc_component_read(component, PA1_AEQ_COEF_DATA, value);

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);
}

void pa1_set_mixer_gain_value(struct snd_soc_component *component, int addr,
						unsigned int value, bool ctrl_flag)
{
	unsigned int another_val = 0;

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	if (ctrl_flag)
		snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, addr + 1);
	else
		snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, addr - 1);

	snd_soc_component_read(component, PA1_AEQ_COEF_DATA, &another_val);

	if (ctrl_flag) {
		snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, addr);
		snd_soc_component_write(component, PA1_AEQ_COEF_DATA, value);
		snd_soc_component_write(component, PA1_AEQ_COEF_DATA, another_val);
	} else {
		snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, addr - 1);
		snd_soc_component_write(component, PA1_AEQ_COEF_DATA, another_val);
		snd_soc_component_write(component, PA1_AEQ_COEF_DATA, value);
	}
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);
}

static int pa1_mixer_get_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = 0;

	pa1_get_mixer_gain_value(component, reg, &value);

	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int pa1_mixer_set_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int invert = mc->invert;
	unsigned int value = ucontrol->value.integer.value[0];

	if (invert)
		pa1_set_mixer_gain_value(component, reg, value, true);
	else
		pa1_set_mixer_gain_value(component, reg, value, false);

	return 0;
}

static int pa1_mixer_set_level_meter_coeff(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int value = ucontrol->value.integer.value[0];

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
						(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, PA1_LEVEL_METER_RAM_ADD);
	snd_soc_component_write(component, PA1_AEQ_COEF_DATA, value);
	snd_soc_component_write(component, PA1_AEQ_COEF_DATA, 0x0);

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static int pa1_str2int(char *str, unsigned int *data, int size)
{
	int num = 0;
	unsigned int temp = 0;
	char *ptr = str;
	unsigned int *val = data;

	while (size-- != 0) {
		if ((*ptr >= '0') && (*ptr <= '9')) {
			temp = temp * 16 + (*ptr - '0');
		} else if ((*ptr >= 'a') && (*ptr <= 'f')) {
			temp = temp * 16 + (*ptr - 'a' + 10);
		} else if (*ptr == ' ') {
			*(val + num) = temp;
			temp = 0;
			num++;
		}
		ptr++;
	}

	return num;
}

void pa1_aed_set_volume(struct snd_soc_component *component, unsigned int master_vol,
			unsigned int lch_vol, unsigned int rch_vol)
{
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AED_EQ_VOLUME_VAL,
			(master_vol << 20) |	/* master volume: 0dB */
			(lch_vol << 10) |	/* channel 2 volume: 0dB */
			(rch_vol << 0)		/* channel 1 volume: 0dB */
	);
	snd_soc_component_write(component, PA1_AED_EQ_VOLUME_STEP_CNT,
			(0x2 << 30) |
	/*volume step: 0x03:0.5dB, 0x02:0.5dB, 0x01:0.25dB, 0x00:0.125dB*/
			(0x2 << 28) |
	/*volume master step: 0x03:0.5dB, 0x02:0.5dB, 0x01:0.25dB, 0x00:0.125dB*/
			(0x1 << 12) |
	/*40ms from -120dB~0dB*/
			(0x1 << 0)
	);
	pa1_acodec_mute(component, 0);
}

void pa1_aed_eq_taps(struct snd_soc_component *component, unsigned int eq_taps)
{
	if (eq_taps > 15) {
		pr_err("Error EQ1_Tap = %d\n", eq_taps);
		return;
	}

	top_conversion_to_dsp(component);
	snd_soc_component_update_bits(component, PA1_AED_STATUS_CTRL, 0x1f << 8, eq_taps << 8);
}

void pa1_aed_get_ram_coeff(struct snd_soc_component *component, int add,
				int len, unsigned int *params)
{
	int i, ctrl_v;
	unsigned int *p = params;

	top_conversion_to_dsp(component);
	for (i = 0; i < len; i++, p++) {
		ctrl_v = add + i;
		snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, ctrl_v);
		snd_soc_component_read(component, PA1_AEQ_COEF_DATA, p);
	}
}

void pa1_aed_set_ram_coeff(struct snd_soc_component *component, int add,
				int len, unsigned int *params)
{
	int i;
	unsigned int *p = params;

	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, add);
	for (i = 0; i < len; i++, p++)
		snd_soc_component_write(component, PA1_AEQ_COEF_DATA, *p);
}

void pa1_aed_set_fullband_drc_coeff(struct snd_soc_component *component, unsigned int *params)
{
	unsigned int *p = params;
	int i;

	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, PA1_FULLBAND_FILTER_RAM_ADD);
	for (i = 0; i < PA1_AED_FULLBAND_DRC_SIZE; i++, p++)
		snd_soc_component_write(component, PA1_AEQ_COEF_DATA, *p);
}

void pa1_aed_set_mixer_params(struct snd_soc_component *component)
{
	unsigned int *p = &PA1_MIXER_PARAM[0];

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
						(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	/* Initialize parameters of input mixer */
	pa1_aed_set_ram_coeff(component, PA1_MIXER_GAIN_RAM_ADD, PA1_AED_MIXER_INPUT_SIZE, p);

	/* Initialize parameters of post mixer */
	p = &PA1_MIXER_PARAM[PA1_AED_MIXER_INPUT_SIZE];
	pa1_aed_set_ram_coeff(component, PA1_MIXER_GAIN_DAC_RAM_ADD, PA1_AED_MIXER_POST_SIZE, p);

	snd_soc_component_write(component, PA1_AED_CLIP_THD, 0x7ffffff);
}

static void pa1_aed_set_filter_data(struct snd_soc_component *component)
{
	unsigned int *p;
	int i, addr;

	/* set default filter param*/
	p = &PA1_DC_CUT_COEFF[0];
	pa1_aed_set_ram_coeff(component, PA1_DC_CUT_RAM_ADD, PA1_DC_FILTER_PARAM_SIZE, p);
	p = &PA1_EQ_COEFF[0];
	pa1_aed_set_ram_coeff(component, PA1_EQ_FILTER_RAM_ADD, PA1_EQ_FILTER_SIZE, p);
	p = &PA1_3D_SURROUND_COEFF[0];
	pa1_aed_set_ram_coeff(component, PA1_3D_SURROUND_RAM_ADD, PA1_AED_3D_SURROUND_SIZE, p);

	for (i = 0; i < 4; i++) {
		p = &PA1_CROSSOVER_COEFF[i * (PA1_FILTER_PARAM_SIZE + 1)];
		if (i == 0)
			addr = PA1_CROSSOVER_FILTER_RAM_ADD + PA1_FILTER_PARAM_SIZE + 1;
		else if (i == 1)
			addr = PA1_CROSSOVER_FILTER_RAM_ADD + 2 * (PA1_FILTER_PARAM_SIZE + 1);
		else if (i == 2)
			addr = PA1_CROSSOVER_FILTER_RAM_ADD;
		else if (i == 3)
			addr = PA1_CROSSOVER_FILTER_RAM_ADD + 3 * (PA1_FILTER_PARAM_SIZE + 1);

		pa1_aed_set_ram_coeff(component, addr, (PA1_FILTER_PARAM_SIZE + 1), p);
	}
}

void pa1_aed_set_multiband_drc_coeff(struct snd_soc_component *component, int band)
{
	int i, ctrl_v;
	unsigned int *p = &PA1_MULTIBAND_DRC_COEFF[0];

	ctrl_v = PA1_MULTIBAND_FILTER_RAM_ADD + band * 2;
	p = &PA1_MULTIBAND_DRC_COEFF[band * PA1_AED_SINGLE_BAND_DRC_SIZE];
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, ctrl_v);
	for (i = 0; i < 2; i++, p++)
		snd_soc_component_write(component, PA1_AEQ_COEF_DATA, *p);

	ctrl_v = PA1_MULTIBAND_FILTER_RAM_ADD + band * 10 + 6;
	p = &PA1_MULTIBAND_DRC_COEFF[band * PA1_AED_SINGLE_BAND_DRC_SIZE + 2];
	snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, ctrl_v);
	for (i = 0; i < 10; i++, p++)
		snd_soc_component_write(component, PA1_AEQ_COEF_DATA, *p);

	ctrl_v = PA1_MULTIBAND_FILTER_RAM_ADD + band * 2 + 36;
	p = &PA1_MULTIBAND_DRC_COEFF[band * PA1_AED_SINGLE_BAND_DRC_SIZE + 12];
	snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, ctrl_v);
	for (i = 0; i < 2; i++, p++)
		snd_soc_component_write(component, PA1_AEQ_COEF_DATA, *p);
}

void pa1_aed_get_multiband_drc_coeff(struct snd_soc_component *component,
				int band, unsigned int *params)
{
	int i, ctrl_v, add;
	unsigned int value = 0;
	unsigned int *p = params;

	ctrl_v = PA1_MULTIBAND_FILTER_RAM_ADD + band * 2;
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, ctrl_v);
	snd_soc_component_read(component, PA1_AEQ_COEF_DATA, &value);

	*p++ = cpu_to_be32(value);

	ctrl_v = PA1_MULTIBAND_FILTER_RAM_ADD + band * 10 + 6;
	snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, ctrl_v);
	snd_soc_component_read(component, PA1_AEQ_COEF_DATA, &value);

	*p++ = cpu_to_be32(value);

	add = PA1_MULTIBAND_FILTER_RAM_ADD + band * 10 + 8;
	for (i = 0; i < 8; i++, p++) {
		ctrl_v = add + i;
		snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, ctrl_v);
		snd_soc_component_read(component, PA1_AEQ_COEF_DATA, &value);
		*p++ = cpu_to_be32(value);
	}

	ctrl_v = PA1_MULTIBAND_FILTER_RAM_ADD + band * 2 + 36;
	snd_soc_component_write(component, PA1_AEQ_COEF_ADDR, ctrl_v);
	snd_soc_component_read(component, PA1_AEQ_COEF_DATA, &value);
	*p++ = cpu_to_be32(value);
}

void pa1_aed_set_multiband_drc_param(struct snd_soc_component *component)
{
	int i;

	for (i = 0; i < 3; i++)
		pa1_aed_set_multiband_drc_coeff(component, i);
}

static void pa1_aed_set_fullband_drc_param(struct snd_soc_component *component)
{
	unsigned int *p;

	p = &PA1_FULLBAND_DRC_COEFF[0];
	pa1_aed_set_fullband_drc_coeff(component, p);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);
}

static void pa1_aed_enable_dsp_process_module(struct snd_soc_component *component)
{
	top_conversion_to_dsp(component);
	snd_soc_component_update_bits(component, PA1_AED_STATUS_CTRL,
						0x7 << 20 | 0x3 << 24,
						0x7 << 20 | 0x3 << 24);
}

static int pa1_mixer_get_3D_Surround_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &PA1_3D_SURROUND_COEFF[0];
	int i;

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	pa1_aed_get_ram_coeff(component, PA1_3D_SURROUND_RAM_ADD, PA1_AED_FULLBAND_DRC_SIZE, p);

	for (i = 0; i < PA1_AED_FULLBAND_DRC_SIZE; i++) {
		if (i == 5)
			p++;
		else
			*value++ = cpu_to_be32(*p++);
	}

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static int pa1_mixer_set_3D_Surround_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int tmp_data[PA1_FILTER_PARAM_SIZE + 2] = {0};
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[PA1_FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &PA1_3D_SURROUND_COEFF[0];
	int num, i, band_id, addr;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, PA1_FILTER_PARAM_BYTE);

	num = pa1_str2int(p_string, p_data, PA1_FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (PA1_FILTER_PARAM_SIZE + 1) || band_id >= PA1_3D_SURROUND_BAND) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	p_data = &tmp_data[1];
	p = &PA1_3D_SURROUND_COEFF[band_id * (PA1_FILTER_PARAM_SIZE + 1)];
	for (i = 0; i < (PA1_FILTER_PARAM_SIZE + 1); i++, p++, p_data++)
		*p = *p_data;

	p = &PA1_3D_SURROUND_COEFF[band_id * (PA1_FILTER_PARAM_SIZE + 1)];
	if (band_id == 0) {
		addr = PA1_3D_SURROUND_RAM_ADD;
		pa1_aed_set_ram_coeff(component, addr, PA1_FILTER_PARAM_SIZE + 1, p);
	} else if (band_id == 1) {
		addr = PA1_3D_SURROUND_RAM_ADD + PA1_FILTER_PARAM_SIZE + 1;
		pa1_aed_set_ram_coeff(component, addr, PA1_FILTER_PARAM_SIZE + 1, p);
	}

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static int pa1_mixer_get_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &PA1_EQ_COEFF[0];
	int i, j = 0;

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	pa1_aed_get_ram_coeff(component, PA1_EQ_FILTER_RAM_ADD, PA1_EQ_FILTER_SIZE_CH, p);

	for (i = 0; i < (PA1_EQ_FILTER_SIZE_CH - 1); i++) {
		j = i + 1;
		if (j % 6 == 0)
			p++;
		else
			*value++ = cpu_to_be32(*p++);
	}

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static int pa1_mixer_set_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int tmp_data[PA1_FILTER_PARAM_SIZE + 1] = {0};
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[PA1_FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &PA1_EQ_COEFF[0];
	int num, i, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, PA1_FILTER_PARAM_BYTE);

	num = pa1_str2int(p_string, p_data, PA1_FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (PA1_FILTER_PARAM_SIZE + 1) || band_id >= PA1_EQ_BAND) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	p_data = &tmp_data[1];
	p = &PA1_EQ_COEFF[band_id * (PA1_FILTER_PARAM_SIZE + 1)];
	for (i = 0; i < PA1_FILTER_PARAM_SIZE; i++, p++, p_data++) {
		*p = *p_data;
		*(p + PA1_EQ_FILTER_SIZE_CH) = *p_data;
	}

	p = &PA1_EQ_COEFF[band_id * (PA1_FILTER_PARAM_SIZE + 1)];
	pa1_aed_set_ram_coeff(component, (PA1_EQ_FILTER_RAM_ADD +
		band_id * (PA1_FILTER_PARAM_SIZE + 1)),
		PA1_FILTER_PARAM_SIZE + 1, p);

	p = &PA1_EQ_COEFF[band_id * (PA1_FILTER_PARAM_SIZE + 1) + PA1_EQ_FILTER_SIZE_CH];
	pa1_aed_set_ram_coeff(component, (PA1_EQ_FILTER_RAM_ADD +
		PA1_EQ_FILTER_SIZE_CH + band_id * (PA1_FILTER_PARAM_SIZE + 1)),
		PA1_FILTER_PARAM_SIZE + 1, p);

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static int pa1_mixer_get_fullband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &PA1_FULLBAND_DRC_COEFF[0];
	int i;
	int len = PA1_AED_FULLBAND_DRC_SIZE;

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	pa1_aed_get_ram_coeff(component, PA1_FULLBAND_FILTER_RAM_ADD, len, p);

	for (i = 0; i < len; i++)
		if (i == 1 || i == 3)
			p++;
		else
			*value++ = cpu_to_be32(*p++);

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static int pa1_mixer_set_fullband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int tmp_data[PA1_AED_FULLBAND_DRC_SIZE + 1] = {0};
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[PA1_AED_FULLBAND_DRC_BYTES];
	char *p_string = &tmp_string[0];
	unsigned int *p = &PA1_FULLBAND_DRC_COEFF[0];
	int num, i, group;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, PA1_AED_FULLBAND_DRC_BYTES);

	num = pa1_str2int(p_string, p_data, PA1_AED_FULLBAND_DRC_BYTES);
	group = tmp_data[0];
	if (num != (PA1_AED_FULLBAND_DRC_SIZE + 1) ||
			group >= PA1_AED_FULLBAND_DRC_GROUP_SIZE) {
		pr_info("Error: parma_num = %d, group = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	p_data = &tmp_data[1];
	p = &PA1_FULLBAND_DRC_COEFF[0];
	for (i = 0; i < PA1_AED_FULLBAND_DRC_SIZE; i++)
		*p++ = *p_data++;

	p = &PA1_FULLBAND_DRC_COEFF[0];
	pa1_aed_set_fullband_drc_coeff(component, p);

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static int pa1_mixer_get_multiband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	int i;

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	for (i = 0; i < 3; i++)
		pa1_aed_get_multiband_drc_coeff(component, i, value);

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static int pa1_mixer_set_multiband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int tmp_data[PA1_AED_SINGLE_BAND_DRC_SIZE + 1] = {0};
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[PA1_MULTIBAND_DRC_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &PA1_MULTIBAND_DRC_COEFF[0];
	int num, i, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, PA1_MULTIBAND_DRC_PARAM_BYTE);

	num = pa1_str2int(p_string, p_data, PA1_MULTIBAND_DRC_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (PA1_AED_SINGLE_BAND_DRC_SIZE + 1) ||
			band_id >= PA1_AED_MULTIBAND_DRC_BANDS) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	/*Don't update offset and gain*/
	p_data = &tmp_data[1];
	p = &PA1_MULTIBAND_DRC_COEFF[band_id * PA1_AED_SINGLE_BAND_DRC_SIZE];

	for (i = 0; i < PA1_AED_SINGLE_BAND_DRC_SIZE; i++)
		*p++ = *p_data++;

	pa1_aed_set_multiband_drc_coeff(component, band_id);

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static int pa1_mixer_get_crossover_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &PA1_CROSSOVER_COEFF[0];
	int i, j = 0;

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	pa1_aed_get_ram_coeff(component,
			(PA1_CROSSOVER_FILTER_RAM_ADD + PA1_FILTER_PARAM_SIZE + 1),
						PA1_FILTER_PARAM_SIZE, p);

	p = &PA1_CROSSOVER_COEFF[PA1_FILTER_PARAM_SIZE + 1];
	pa1_aed_get_ram_coeff(component,
			(PA1_CROSSOVER_FILTER_RAM_ADD + 2 * (PA1_FILTER_PARAM_SIZE + 1)),
							PA1_FILTER_PARAM_SIZE, p);

	p = &PA1_CROSSOVER_COEFF[2 * (PA1_FILTER_PARAM_SIZE + 1)];
	pa1_aed_get_ram_coeff(component, PA1_CROSSOVER_FILTER_RAM_ADD,
						PA1_FILTER_PARAM_SIZE, p);

	p = &PA1_CROSSOVER_COEFF[3 * (PA1_FILTER_PARAM_SIZE + 1)];
	pa1_aed_get_ram_coeff(component,
			(PA1_CROSSOVER_FILTER_RAM_ADD + 3 * (PA1_FILTER_PARAM_SIZE + 1)),
							PA1_FILTER_PARAM_SIZE, p);

	p = &PA1_CROSSOVER_COEFF[0];
	for (i = 0; i < (PA1_CROSSOVER_FILTER_SIZE - 1); i++) {
		j = i + 1;
		if (j % 6 == 0)
			p++;
		else
			*value++ = cpu_to_be32(*p++);
	}

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static int pa1_mixer_set_crossover_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int tmp_data[PA1_FILTER_PARAM_SIZE + 1] = {0};
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[PA1_FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &PA1_CROSSOVER_COEFF[0];
	int num, i, band_id, addr;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, PA1_FILTER_PARAM_BYTE);

	num = pa1_str2int(p_string, p_data, PA1_FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (PA1_FILTER_PARAM_SIZE + 1) ||
			band_id >= PA1_CROSSOVER_FILTER_BAND) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);

	p_data = &tmp_data[1];
	p = &PA1_CROSSOVER_COEFF[band_id * (PA1_FILTER_PARAM_SIZE + 1)];

	for (i = 0; i < PA1_FILTER_PARAM_SIZE; i++)
		*p++ = *p_data++;

	p = &PA1_CROSSOVER_COEFF[band_id * (PA1_FILTER_PARAM_SIZE + 1)];
	if (band_id == 0) {
		addr = PA1_CROSSOVER_FILTER_RAM_ADD + PA1_FILTER_PARAM_SIZE + 1;
		pa1_aed_set_ram_coeff(component, addr, PA1_FILTER_PARAM_SIZE + 1, p);
	} else if (band_id == 1) {
		addr = PA1_CROSSOVER_FILTER_RAM_ADD + 2 * (PA1_FILTER_PARAM_SIZE + 1);
		pa1_aed_set_ram_coeff(component, addr, PA1_FILTER_PARAM_SIZE + 1, p);
	} else if (band_id == 2) {
		addr = PA1_CROSSOVER_FILTER_RAM_ADD;
		pa1_aed_set_ram_coeff(component, addr, PA1_FILTER_PARAM_SIZE + 1, p);
	} else if (band_id == 3) {
		addr = PA1_CROSSOVER_FILTER_RAM_ADD + 3 * (PA1_FILTER_PARAM_SIZE + 1);
		pa1_aed_set_ram_coeff(component, addr, PA1_FILTER_PARAM_SIZE + 1, p);
	}

	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	return 0;
}

static const struct snd_kcontrol_new pa1_acodec_vol_control[] = {
	SOC_SINGLE_EXT_TLV("PA1 AED master volume",
			PA1_AED_EQ_VOLUME_VAL, 20, 0x3FF, 1,
			pa1_mixer_aed_read, pa1_mixer_aed_write,
			pa1_vol_tlv),

	SOC_SINGLE_EXT_TLV("PA1 AED Lch volume",
			PA1_AED_EQ_VOLUME_VAL, 0, 0x3FF, 1,
			pa1_mixer_aed_read, pa1_mixer_aed_write,
			pa1_vol_tlv),

	SOC_SINGLE_EXT_TLV("PA1 AED Rch volume",
			PA1_AED_EQ_VOLUME_VAL, 10, 0x3FF, 1,
			pa1_mixer_aed_read, pa1_mixer_aed_write,
			pa1_vol_tlv),

	SOC_SINGLE_EXT("PA1 AED Mute",
			PA1_AED_MUTE_CTRL, 2, 0x1, 0,
			pa1_mixer_aed_get_mute, pa1_mixer_aed_set_mute),

	SOC_SINGLE_EXT("PA1 AED DC cut enable",
			PA1_AED_STATUS_CTRL, 0, 0x1, 0,
			pa1_mixer_aed_read, pa1_mixer_aed_write),

	SOC_SINGLE_EXT("PA1 AED 3D Surround enable",
			PA1_AED_STATUS_CTRL, 3, 0x1, 0,
			pa1_mixer_aed_read, pa1_mixer_aed_write),

	SND_SOC_BYTES_EXT("PA1 AED 3D Surround Parameters",
			PA1_AED_FULLBAND_DRC_BYTES,
			pa1_mixer_get_3D_Surround_params,
			pa1_mixer_set_3D_Surround_params),

	SOC_SINGLE_EXT("PA1 AED Level Meter enable",
			PA1_AED_STATUS_CTRL, 6, 0x1, 0,
			pa1_mixer_aed_read, pa1_mixer_aed_write),

	SOC_SINGLE_EXT("PA1 AED EQ enable",
			PA1_AED_STATUS_CTRL, 2, 0x1, 0,
			pa1_mixer_aed_read, pa1_mixer_aed_write),

	SND_SOC_BYTES_EXT("PA1 AED EQ Parameters",
			(PA1_EQ_FILTER_SIZE_CH * 4),
			pa1_mixer_get_EQ_params,
			pa1_mixer_set_EQ_params),

	SOC_SINGLE_EXT("PA1 AED Multi-band DRC enable",
			PA1_AED_STATUS_CTRL, 7, 0x1, 0,
			pa1_mixer_aed_read, pa1_mixer_aed_write),

	SND_SOC_BYTES_EXT("PA1 AED Crossover Filter Parameters",
			(PA1_CROSSOVER_FILTER_SIZE * 4),
			pa1_mixer_get_crossover_params,
			pa1_mixer_set_crossover_params),

	SND_SOC_BYTES_EXT("PA1 AED Multi-band DRC Parameters",
			(PA1_AED_MULTIBAND_DRC_SIZE * 4),
			pa1_mixer_get_multiband_DRC_params,
			pa1_mixer_set_multiband_DRC_params),

	SOC_SINGLE_EXT("PA1 AED Full-band DRC enable",
			PA1_AED_STATUS_CTRL, 23, 0x1, 0,
			pa1_mixer_aed_read, pa1_mixer_aed_write),

	SND_SOC_BYTES_EXT("PA1 AED Full-band DRC Parameters",
			PA1_AED_FULLBAND_DRC_BYTES,
			pa1_mixer_get_fullband_DRC_params,
			pa1_mixer_set_fullband_DRC_params),

	SOC_SINGLE_EXT("PA1 AED Clip THD",
			PA1_AED_CLIP_THD, 0, 0x7FFFFFF, 0,
			pa1_mixer_aed_read, pa1_mixer_aed_write),

	SOC_SINGLE_EXT("PA1 AED Input Mixer Gain LL",
			PA1_MIXER_GAIN_RAM_ADD, 0, 0xfffffff, 1,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Input Mixer Gain RL",
			PA1_MIXER_GAIN_RAM_ADD + 1, 0, 0xfffffff, 0,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Input Mixer Gain LR",
			PA1_MIXER_GAIN_RAM_ADD + 2, 0, 0xfffffff, 1,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Input Mixer Gain RR",
			PA1_MIXER_GAIN_RAM_ADD + 3, 0, 0xfffffff, 0,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Mixer Gain LL DAC",
			PA1_MIXER_GAIN_DAC_RAM_ADD, 0, 0xfffffff, 1,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Mixer Gain RL DAC",
			PA1_MIXER_GAIN_DAC_RAM_ADD + 1, 0, 0xfffffff, 0,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Mixer Gain LR DAC",
			PA1_MIXER_GAIN_DAC_RAM_ADD + 2, 0, 0xfffffff, 1,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Mixer Gain RR DAC",
			PA1_MIXER_GAIN_DAC_RAM_ADD + 3, 0, 0xfffffff, 0,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Mixer Gain LL I2S",
			PA1_MIXER_GAIN_I2S_RAM_ADD, 0, 0xfffffff, 1,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Mixer Gain RL I2S",
			PA1_MIXER_GAIN_I2S_RAM_ADD + 1, 0, 0xfffffff, 0,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Mixer Gain LR I2S",
			PA1_MIXER_GAIN_I2S_RAM_ADD + 2, 0, 0xfffffff, 1,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Mixer Gain RR I2S",
			PA1_MIXER_GAIN_I2S_RAM_ADD + 3, 0, 0xfffffff, 0,
			pa1_mixer_get_gain, pa1_mixer_set_gain),

	SOC_SINGLE_EXT("PA1 AED Level Meter RMS Parameters",
			PA1_LEVEL_METER_RAM_ADD, 0, 0xfffffff, 0,
			pa1_mixer_get_gain, pa1_mixer_set_level_meter_coeff),
};

static int pa1_acodec_set_bias_level(struct snd_soc_component *component,
				  enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		/* Full power on */
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		/* The chip runs through the power down sequence for us. */
		break;
	}
	component->dapm.bias_level = level;
	return 0;
}

static int pa1_acodec_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *codec_dai)
{
	struct pa1_acodec_priv *pa1_acodec = snd_soc_dai_get_drvdata(codec_dai);
	struct snd_soc_component *component = pa1_acodec->component;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			pr_debug("%s(), start\n", __func__);
			if (!pa1_acodec->mute)
				pa1_acodec_mute(component, 0);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			pr_debug("%s(), stop\n", __func__);
			if (!pa1_acodec->mute)
				pa1_acodec_mute(component, 1);
			break;
		}
	}
	return 0;
}

static int reset_pa1_acodec_GPIO(struct device *dev)
{
	struct pa1_acodec_priv *pa1_acodec =  dev_get_drvdata(dev);
	struct pa1_acodec_platform_data *pdata = pa1_acodec->pdata;
	int ret = 0;

	if (!gpio_is_valid(pdata->reset_pin))
		return 0;

	if (!reset_pin_setting) {
		ret = gpio_request(pdata->reset_pin, NULL);
		if (ret < 0)
			return -1;

		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
		usleep_range(10 * 500, 10 * 1000);

		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
		usleep_range(10 * 500, 10 * 1000);
		reset_pin_setting = true;
		gpio_free(pdata->reset_pin);
	}

	return 0;
}

static int pa1_acodec_snd_suspend(struct snd_soc_component *component)
{
	struct pa1_acodec_priv *pa1_acodec = snd_soc_component_get_drvdata(component);
	struct pa1_acodec_platform_data *pdata = pa1_acodec->pdata;
	unsigned int *p = &PA1_MIXER_PARAM[0];
	int len = PA1_AED_MIXER_INPUT_SIZE;
	int ret;

	dev_info(component->dev, "pa1_acodec_suspend!\n");
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, (0x1 << 6) | (0x1 << 0),
							(0x0 << 6) | (0x0 << 0));
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0xf);
	pa1_aed_get_ram_coeff(component, PA1_MIXER_GAIN_RAM_ADD, len, p);
	p = &PA1_MIXER_PARAM[4];
	len = PA1_AED_MIXER_POST_SIZE;
	pa1_aed_get_ram_coeff(component, PA1_MIXER_GAIN_DAC_RAM_ADD, len, p);
	snd_soc_component_write(component, PA1_AEQ_SOFT_REST, 0x0);
	dsp_conversion_to_top(component);
	snd_soc_component_update_bits(component, PA1_DSP_MISC0, 0x1 << 6, 0x1 << 6);

	top_conversion_to_dsp(component);
	pa1_acodec->vol = (unsigned int)snd_soc_component_read32(component, PA1_AED_EQ_VOLUME_VAL);
	pa1_acodec->dsp_status =
		(unsigned int)snd_soc_component_read32(component, PA1_AED_STATUS_CTRL);
	pa1_acodec_set_bias_level(component, SND_SOC_BIAS_OFF);

	if (gpio_is_valid(pdata->reset_pin) && reset_pin_setting) {
		// request amp PD pin control GPIO
		ret = gpio_request(pdata->reset_pin, NULL);
		if (ret < 0)
			dev_err(component->dev, "failed to request gpio: %d\n", ret);
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
		reset_pin_setting = false;
		gpio_free(pdata->reset_pin);
	}
	usleep_range(9, 15);

	return 0;
}

static void pa1_effect_init(struct snd_soc_component *component)
{
	/* all 15 bands for EQ */
	pa1_aed_eq_taps(component, PA1_EQ_BAND);
	/* set default mixer gain */
	pa1_aed_set_mixer_params(component);
	/* set default filter param */
	pa1_aed_set_filter_data(component);
	/* set multi-band drc param */
	pa1_aed_set_multiband_drc_param(component);
	/* set full-band drc param */
	pa1_aed_set_fullband_drc_param(component);
	/* enable some module which must be on when dsp is running */
	pa1_aed_enable_dsp_process_module(component);
	/* set master & channel volume gain to 0dB */
	pa1_aed_set_volume(component, 0xc0, 0xc0, 0xc0);
}

static void pa1_playback_mode(struct snd_soc_component *component)
{
	int j;

	dsp_conversion_to_top(component);
	snd_soc_component_write(component, PA1_RESET_CTRL, 0x10);
	msleep(20);
	for (j = 0; j < ARRAY_SIZE(pa1_reg_set); j++)
		snd_soc_component_write(component, pa1_reg_set[j][0],
							pa1_reg_set[j][1]);
}

static int pa1_acodec_snd_resume(struct snd_soc_component *component)
{
	struct pa1_acodec_priv *pa1_acodec = snd_soc_component_get_drvdata(component);
	struct pa1_acodec_platform_data *pdata = pa1_acodec->pdata;
	int ret;

	if (gpio_is_valid(pdata->reset_pin) && !reset_pin_setting) {
		// request amp PD pin control GPIO
		ret = gpio_request(pdata->reset_pin, NULL);
		if (ret < 0)
			dev_err(component->dev, "failed to request gpio: %d\n", ret);
		reset_pin_setting = true;
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
		/* need delay before regcache for spec request */
		gpio_free(pdata->reset_pin);
	}

	usleep_range(1 * 500, 1 * 1000);

	pa1_acodec_set_bias_level(component, SND_SOC_BIAS_STANDBY);
	msleep(20);
	pa1_playback_mode(component);
	msleep(30);
	pa1_effect_init(component);
	top_conversion_to_dsp(component);
	snd_soc_component_write(component, PA1_AED_EQ_VOLUME_VAL, pa1_acodec->vol);
	snd_soc_component_write(component, PA1_AED_STATUS_CTRL, pa1_acodec->dsp_status);
	pa1_acodec_mute(component, pa1_acodec->mute);

	return 0;
}

static int pa1_acodec_probe(struct snd_soc_component *component)
{
	struct pa1_acodec_priv *pa1_acodec = snd_soc_component_get_drvdata(component);

	pr_info("%s: Init PA1!\n", __func__);

	snd_soc_add_component_controls(component, pa1_acodec_vol_control,
			ARRAY_SIZE(pa1_acodec_vol_control));
	pa1_acodec->component = component;

	msleep(20);
	pa1_acodec->pa1_page = 0;
	pa1_playback_mode(component);
	msleep(30);
	pa1_effect_init(component);

	return 0;
}

static void pa1_acodec_remove(struct snd_soc_component *component)
{
	struct pa1_acodec_priv *pa1_acodec = snd_soc_component_get_drvdata(component);
	struct pa1_acodec_platform_data *pdata = pa1_acodec->pdata;

	if (gpio_is_valid(pdata->reset_pin))
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);

	usleep_range(9, 15);
}

static const struct snd_soc_component_driver soc_codec_pa1_acodec = {
	.probe = pa1_acodec_probe,
	.remove = pa1_acodec_remove,
	.suspend = pa1_acodec_snd_suspend,
	.resume = pa1_acodec_snd_resume,
	.set_bias_level = pa1_acodec_set_bias_level,
};

static const struct snd_soc_dai_ops pa1_acodec_dai_ops = {
	.trigger = pa1_acodec_trigger,
};

static struct snd_soc_dai_driver pa1_acodec_dai = {
	.name = "pa1_acodec-amplifier",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 2,
		     .channels_max = 8,
		     .rates = PA1_RATES,
		     .formats = PA1_FORMATS,
		     },
	.ops = &pa1_acodec_dai_ops,
};

static int pa1_acodec_parse_dt(struct pa1_acodec_priv *pa1_acodec,
	struct device_node *np)
{
	int ret = 0;
	int reset_pin = -1;

	reset_pin = of_get_named_gpio(np, "reset_pin", 0);
	if (!gpio_is_valid(reset_pin)) {
		pr_err("%s fail to get reset pin from dts!\n", __func__);
		ret = -1;
	} else {
		pr_info("%s pdata->reset_pin = %d!\n", __func__, reset_pin);
	}

	pa1_acodec->pdata->reset_pin = reset_pin;
	return ret;
}

static int pa1_acodec_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config = pa1_acodec_regmap;
	struct pa1_acodec_priv *pa1_acodec;
	struct pa1_acodec_platform_data *pdata;
	int ret = 0;

	pa1_acodec = devm_kzalloc(&i2c->dev, sizeof(struct pa1_acodec_priv), GFP_KERNEL);
	if (!pa1_acodec)
		return -ENOMEM;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	pr_info("%s i2c addr:0x%x\n", __func__, i2c->addr);

	pdata = devm_kzalloc(&i2c->dev,
				sizeof(struct pa1_acodec_platform_data),
				GFP_KERNEL);
	if (!pdata) {
		pr_err("%s failed to kzalloc for pa1_acodec pdata\n", __func__);
		devm_kfree(&i2c->dev, i2c_get_clientdata(i2c));
		return -ENOMEM;
	}
	pa1_acodec->pdata = pdata;

	pa1_acodec_parse_dt(pa1_acodec, i2c->dev.of_node);
	pa1_acodec->regmap = regmap;

	dev_set_drvdata(&i2c->dev, pa1_acodec);

	ret = devm_snd_soc_register_component(&i2c->dev, &soc_codec_pa1_acodec,
			&pa1_acodec_dai, 1);
	if (ret != 0) {
		devm_kfree(&i2c->dev, i2c_get_clientdata(i2c));
		return -ENOMEM;
	}
	reset_pa1_acodec_GPIO(&i2c->dev);

	return ret;
}

static int pa1_acodec_i2c_remove(struct i2c_client *i2c)
{
	devm_kfree(&i2c->dev, i2c_get_clientdata(i2c));
	return 0;
}

static void pa1_acodec_i2c_shutdown(struct i2c_client *i2c)
{
	struct pa1_acodec_priv *pa1_acodec = i2c_get_clientdata(i2c);
	struct pa1_acodec_platform_data *pdata = pa1_acodec->pdata;
	int ret;

	if (gpio_is_valid(pdata->reset_pin) && reset_pin_setting) {
		// request amp PD pin control GPIO
		ret = gpio_request(pdata->reset_pin, NULL);
		if (ret < 0)
			pr_err("failed to request gpio: %d\n", ret);
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
		reset_pin_setting = false;
		gpio_free(pdata->reset_pin);
	}

}

static const struct i2c_device_id pa1_acodec_i2c_id[] = {
	{"pa1_acodec",},
	{}
};

MODULE_DEVICE_TABLE(i2c, pa1_acodec_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id pa1_acodec_of_match[] = {
	{.compatible = "amlogic, pa1_acodec",},
	{}
};

MODULE_DEVICE_TABLE(of, pa1_acodec_of_match);
#endif

static struct i2c_driver pa1_acodec_i2c_driver = {
	.probe = pa1_acodec_i2c_probe,
	.remove = pa1_acodec_i2c_remove,
	.shutdown = pa1_acodec_i2c_shutdown,
	.id_table = pa1_acodec_i2c_id,
	.driver = {
		   .name = PA1_DRV_NAME,
		   .of_match_table = pa1_acodec_of_match,
		   },
};

module_i2c_driver(pa1_acodec_i2c_driver);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("PA1 Audio Amplifier Driver");
MODULE_LICENSE("GPL v2");
