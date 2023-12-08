// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/clk-provider.h>

#include "effects_v2.h"
#include "effects_hw_v2.h"
#include "effects_hw_v2_coeff.h"
#include "ddr_mngr.h"
#include "regs.h"
#include "iomap.h"

#define DRV_NAME "Effects"
#define AED_REG_NUM	13

/*
 * AED Diagram
 * DC -- ND -- MIX -- EQ -- Multiband DRC -- LR Vol
 *    -- Fullband DRC -- Master Volume
 */

enum {
	AED_DC,
	AED_ND,
	AED_EQ,
	AED_MDRC,
	AED_FDRC
};

struct effect_chipinfo {
	/* v1 is for G12X(g12a, g12b)
	 * v2 is for tl1
	 * v3 is for sm1/tm2
	 */
	int version;
	bool reserved_frddr;
};

struct audioeffect {
	struct device *dev;

	/* gate */
	struct clk *gate;
	/* source mpll */
	struct clk *srcpll;
	/* eqdrc clk */
	struct clk *clk;

	struct effect_chipinfo *chipinfo;

	int lane_mask;
	int ch_mask;

	/* which module should be effected */
	int effect_module;
	unsigned int syssrc_clk_rate;
	/* store user setting */
	u32 user_setting[AED_REG_NUM];

};

struct audioeffect *s_effect;
/* Restore AED register parameters after standby */
static unsigned int aed_restore_list[AED_REG_NUM] = {
	AED_EQ_VOLUME,
	AED_EQ_VOLUME_SLEW_CNT,
	AED_MUTE,
	AED_MIX0_LL,
	AED_MIX0_RL,
	AED_MIX0_LR,
	AED_MIX0_RR,
	AED_CLIP_THD,
	AED_DC_EN,
	AED_ND_CNTL,
	AED_EQ_EN,
	AED_MDRC_CNTL,
	AED_DRC_CNTL,
};

static struct audioeffect *get_audioeffects(void)
{
	if (!s_effect) {
		pr_info("Not init audio effects\n");
		return NULL;
	}

	return s_effect;
}

int get_aed_dst(void)
{
	struct audioeffect *p_effect = get_audioeffects();

	if (!p_effect)
		return -1;
	else
		return p_effect->effect_module;
}

int check_aed_version(void)
{
	struct audioeffect *p_effect = get_audioeffects();

	if (!p_effect || !p_effect->chipinfo)
		return -1;

	return p_effect->chipinfo->version;
}

bool is_aed_reserve_frddr(void)
{
	struct audioeffect *p_effect = get_audioeffects();

	if (!p_effect || !p_effect->chipinfo)
		return false;

	return p_effect->chipinfo->reserved_frddr;
}

static int eqdrc_clk_set(struct audioeffect *p_effect)
{
	int ret = 0;
	char *clk_name = NULL;

	ret = clk_prepare_enable(p_effect->clk);
	if (ret) {
		pr_err("Can't enable eqdrc clock: %d\n",
			ret);
		return -EINVAL;
	}

	clk_name = (char *)__clk_get_name(p_effect->srcpll);
	if (!strcmp(clk_name, "hifi_pll") || !strcmp(clk_name, "t5_hifi_pll")) {
		if (p_effect->syssrc_clk_rate)
			clk_set_rate(p_effect->srcpll, p_effect->syssrc_clk_rate);
		else
			clk_set_rate(p_effect->srcpll, 1806336 * 1000);
	}
	ret = clk_prepare_enable(p_effect->srcpll);
	if (ret) {
		pr_err("Can't enable eqdrc src pll clock: %d\n",
			ret);
		return -EINVAL;
	}

	/* default clk */
	clk_set_rate(p_effect->clk, 200000000);

	pr_info("%s, src pll:%lu, clk:%lu\n",
		__func__,
		clk_get_rate(p_effect->srcpll),
		clk_get_rate(p_effect->clk));

	return 0;
}

static int mixer_aed_read(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value =
			(((unsigned int)eqdrc_read(reg)) >> shift) & max;

	if (invert)
		value = (~value) & max;
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int mixer_aed_write(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int max = mc->max;
	unsigned int invert = mc->invert;
	unsigned int value = ucontrol->value.integer.value[0];
	unsigned int new_val = (unsigned int)eqdrc_read(reg);

	if (invert)
		value = (~value) & max;
	max = ~(max << shift);
	new_val &= max;
	new_val |= (value << shift);

	eqdrc_write(reg, new_val);

	return 0;
}

static int mixer_get_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &EQ_COEFF[0];
	int i;

	aed_get_ram_coeff(EQ_FILTER_RAM_ADD, EQ_FILTER_SIZE_CH, p);

	for (i = 0; i < EQ_FILTER_SIZE_CH; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int str2int(char *str, unsigned int *data, int size)
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

static int mixer_set_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[FILTER_PARAM_SIZE + 1];
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &EQ_COEFF[0];
	int num, i, band_id, version;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, FILTER_PARAM_BYTE);

	num = str2int(p_string, p_data, FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (FILTER_PARAM_SIZE + 1) || band_id >= EQ_BAND) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &EQ_COEFF[band_id * FILTER_PARAM_SIZE];
	for (i = 0; i < FILTER_PARAM_SIZE; i++, p++, p_data++) {
		*p = *p_data;
		*(p + EQ_FILTER_SIZE_CH) = *p_data;
	}

	version = check_aed_version();
	p = &EQ_COEFF[band_id * FILTER_PARAM_SIZE];
	aed_set_ram_coeff(version, (EQ_FILTER_RAM_ADD +
		band_id * FILTER_PARAM_SIZE),
		FILTER_PARAM_SIZE, p);

	p = &EQ_COEFF[band_id * FILTER_PARAM_SIZE + EQ_FILTER_SIZE_CH];
	aed_set_ram_coeff(version, (EQ_FILTER_RAM_ADD +
		EQ_FILTER_SIZE_CH + band_id * FILTER_PARAM_SIZE),
		FILTER_PARAM_SIZE, p);

	return 0;
}

static int mixer_get_crossover_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &CROSSOVER_COEFF[0];
	int i;

	aed_get_ram_coeff(CROSSOVER_FILTER_RAM_ADD, CROSSOVER_FILTER_SIZE, p);

	for (i = 0; i < CROSSOVER_FILTER_SIZE; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int mixer_set_crossover_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[FILTER_PARAM_SIZE + 1];
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &CROSSOVER_COEFF[0];
	int num, i, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, FILTER_PARAM_BYTE);

	num = str2int(p_string, p_data, FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (FILTER_PARAM_SIZE + 1) ||
			band_id >= CROSSOVER_FILTER_BAND) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &CROSSOVER_COEFF[band_id * FILTER_PARAM_SIZE];
	for (i = 0; i < FILTER_PARAM_SIZE; i++)
		*p++ = *p_data++;

	p = &CROSSOVER_COEFF[band_id * FILTER_PARAM_SIZE];
	aed_set_ram_coeff(check_aed_version(), (CROSSOVER_FILTER_RAM_ADD +
		band_id * FILTER_PARAM_SIZE),
		FILTER_PARAM_SIZE, p);

	return 0;
}

static int mixer_get_multiband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &multiband_drc_coeff[0];
	int i;

	for (i = 0; i < 3; i++) {
		aed_get_multiband_drc_coeff(i,
			p + i * AED_SINGLE_BAND_DRC_SIZE);
	}

	for (i = 0; i < AED_MULTIBAND_DRC_SIZE; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int mixer_set_multiband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[AED_SINGLE_BAND_DRC_SIZE + 1];
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[MULTIBAND_DRC_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &multiband_drc_coeff[0];
	int num, i, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, MULTIBAND_DRC_PARAM_BYTE);

	num = str2int(p_string, p_data, MULTIBAND_DRC_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (AED_SINGLE_BAND_DRC_SIZE + 1) ||
			band_id >= AED_MULTIBAND_DRC_BANDS) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

    /*Don't update offset and gain*/
	p_data = &tmp_data[1];
	p = &multiband_drc_coeff[band_id * AED_SINGLE_BAND_DRC_SIZE];
	for (i = 0; i < AED_SINGLE_BAND_DRC_SIZE; i++)
		*p++ = *p_data++;

	p = &multiband_drc_coeff[0];
	aed_set_multiband_drc_coeff(band_id, p);

	return 0;
}

static int mixer_get_fullband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &fullband_drc_coeff[0];
	int i;

	aed_get_fullband_drc_coeff(AED_FULLBAND_DRC_SIZE, p);

	for (i = 0; i < AED_FULLBAND_DRC_SIZE; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int mixer_set_fullband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[AED_FULLBAND_DRC_OFFSET + 1];
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[AED_FULLBAND_DRC_BYTES];
	char *p_string = &tmp_string[0];
	unsigned int *p = &fullband_drc_coeff[0];
	int num, i, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, AED_FULLBAND_DRC_BYTES);

	num = str2int(p_string, p_data, AED_FULLBAND_DRC_BYTES);
	band_id = tmp_data[0];
	if (num != (AED_FULLBAND_DRC_OFFSET + 1) ||
			band_id >= AED_FULLBAND_DRC_GROUP_SIZE) {
		pr_info("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &fullband_drc_coeff[band_id * AED_FULLBAND_DRC_OFFSET];
	for (i = 0; i < AED_FULLBAND_DRC_OFFSET; i++)
		*p++ = *p_data++;

	p = &fullband_drc_coeff[band_id * AED_FULLBAND_DRC_OFFSET];
	aed_set_fullband_drc_coeff(band_id, p);

	return 0;
}

static void aed_set_filter_data(int version)
{
	int *p;

	/* set default filter param*/
	p = &DC_CUT_COEFF[0];
	aed_init_ram_coeff(version,
			   DC_CUT_FILTER_RAM_ADD, DC_CUT_FILTER_SIZE, p);
	p = &EQ_COEFF[0];
	aed_init_ram_coeff(version, EQ_FILTER_RAM_ADD, EQ_FILTER_SIZE, p);
	p = &CROSSOVER_COEFF[0];
	aed_init_ram_coeff(version,
			   CROSSOVER_FILTER_RAM_ADD, CROSSOVER_FILTER_SIZE, p);

}

static void aed_set_drc_data(void)
{
	unsigned int *p = &multiband_drc_coeff[0];
	int i;

	/*set MDRC default value*/
	for (i = 0; i < AED_MULTIBAND_DRC_BANDS; i++)
		aed_set_multiband_drc_coeff(i, p);

	/*set FDRC default value*/
	p = &fullband_drc_coeff[0];
	for (i = 0; i < AED_FULLBAND_DRC_GROUP_SIZE; i++) {
		aed_set_fullband_drc_coeff(i,
			p + i * AED_FULLBAND_DRC_OFFSET);
	}
}

static const DECLARE_TLV_DB_SCALE(master_vol_tlv, -12276, 12, 1);
static const DECLARE_TLV_DB_SCALE(lr_vol_tlv, -12750, 50, 1);

static const struct snd_kcontrol_new snd_effect_controls[] = {
	SOC_SINGLE_EXT("AED DC cut enable",
		       AED_DC_EN, 0, 0x1, 0,
		       mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Noise Detect enable",
		       AED_ND_CNTL, 0, 0x1, 0,
		       mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED EQ enable",
		       AED_EQ_EN, 0, 0x1, 0,
		       mixer_aed_read, mixer_aed_write),

	SND_SOC_BYTES_EXT("AED EQ Parameters",
			  (EQ_FILTER_SIZE_CH * 4),
			  mixer_get_EQ_params,
			  mixer_set_EQ_params),

	SOC_SINGLE_EXT("AED Multi-band DRC enable",
		       AED_MDRC_CNTL, 8, 0x1, 0,
		       mixer_aed_read, mixer_aed_write),

	SND_SOC_BYTES_EXT("AED Crossover Filter Parameters",
			  (CROSSOVER_FILTER_SIZE * 4),
			  mixer_get_crossover_params,
			  mixer_set_crossover_params),

	SND_SOC_BYTES_EXT("AED Multi-band DRC Parameters",
			  (AED_MULTIBAND_DRC_SIZE * 4),
			  mixer_get_multiband_DRC_params,
			  mixer_set_multiband_DRC_params),

	SOC_SINGLE_EXT("AED Full-band DRC enable",
		       AED_DRC_CNTL, 0, 0x1, 0,
		       mixer_aed_read, mixer_aed_write),

	SND_SOC_BYTES_EXT("AED Full-band DRC Parameters",
			  AED_FULLBAND_DRC_BYTES,
			  mixer_get_fullband_DRC_params,
			  mixer_set_fullband_DRC_params),

	SOC_SINGLE_EXT_TLV("AED Lch volume",
			   AED_EQ_VOLUME, 0, 0xFF, 1,
			   mixer_aed_read, mixer_aed_write,
			   lr_vol_tlv),

	SOC_SINGLE_EXT_TLV("AED Rch volume",
			   AED_EQ_VOLUME, 8, 0xFF, 1,
			   mixer_aed_read, mixer_aed_write,
			   lr_vol_tlv),

	SOC_SINGLE_EXT_TLV("AED master volume",
			   AED_EQ_VOLUME, 16, 0x3FF, 1,
			   mixer_aed_read, mixer_aed_write,
			   master_vol_tlv),

	SOC_SINGLE_EXT("AED Clip THD",
		       AED_CLIP_THD, 0, 0x7FFFFF, 0,
		       mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Mixer Gain LL",
		       AED_MIX0_LL, 0, 0x3ffffff, 0,
		       mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Mixer Gain RL",
		       AED_MIX0_RL, 0, 0x3ffffff, 0,
		       mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Mixer Gain LR",
		       AED_MIX0_LR, 0, 0x3ffffff, 0,
		       mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Mixer Gain RR",
		       AED_MIX0_RR, 0, 0x3ffffff, 0,
		       mixer_aed_read, mixer_aed_write),
};

int card_add_effect_v2_kcontrols(struct snd_soc_card *card)
{
	unsigned int idx;
	int err;

	if (!s_effect) {
		pr_info("effect_v2 is not init\n");
		return 0;
	}

	for (idx = 0; idx < ARRAY_SIZE(snd_effect_controls); idx++) {
		err = snd_ctl_add(card->snd_card,
				snd_ctl_new1(&snd_effect_controls[idx],
					s_effect));
		if (err < 0)
			return err;
	}

	return 0;
}

static struct effect_chipinfo tl1_effect_chipinfo = {
	.version = VERSION2,
	.reserved_frddr = true,
};

static struct effect_chipinfo sm1_effect_chipinfo = {
	.version = VERSION3,
	.reserved_frddr = true,
};

/* tm2_revb afterward */
static struct effect_chipinfo tm2_revb_effect_chipinfo = {
	.version = VERSION4,
	.reserved_frddr = false,
};

static const struct of_device_id effect_device_id[] = {
	{
		.compatible = "amlogic, snd-effect-v1"
	},
	{
		.compatible = "amlogic, snd-effect-v2",
		.data       = &tl1_effect_chipinfo,
	},
	{
		.compatible = "amlogic, snd-effect-v3",
		.data       = &sm1_effect_chipinfo,
	},
	{
		.compatible = "amlogic, snd-effect-v4",
		.data       = &tm2_revb_effect_chipinfo,
	},
	{}
};
MODULE_DEVICE_TABLE(of, effect_device_id);

static void effect_init(struct platform_device *pdev)
{
	struct audioeffect *p_effect = dev_get_drvdata(&pdev->dev);
	int version;
	/*set eq/drc module lane & channels*/
	version = check_aed_version();
	if (version > VERSION2)
		aed_set_lane_and_channels_v3(p_effect->lane_mask, p_effect->ch_mask);
	else
		aed_set_lane_and_channels(p_effect->lane_mask, p_effect->ch_mask);

	/*set default mixer gain*/
	aed_set_mixer_params();
	/*all 20 bands for EQ1*/
	aed_eq_taps(EQ_BAND);
	/*set default filter param*/
	aed_set_filter_data(version);
	/*set multi-band drc param*/
	aed_set_multiband_drc_param();
	/*set multi/full-band drc data*/
	aed_set_drc_data();
	/*set full-band drc param, enable 2 band*/
	aed_set_fullband_drc_param(2);
	/*set EQ/DRC module enable*/
	aml_set_aed(1, p_effect->effect_module);

	if (p_effect->chipinfo &&
		p_effect->chipinfo->reserved_frddr) {
		aml_aed_set_frddr_reserved();
	}
}

static int effect_platform_probe(struct platform_device *pdev)
{
	struct audioeffect *p_effect;
	struct device *dev = &pdev->dev;
	struct effect_chipinfo *p_chipinfo;
	int lane_mask = -1, channel_mask = -1, eqdrc_module = -1;
	int ret;

	p_effect = devm_kzalloc(&pdev->dev,
			sizeof(struct audioeffect),
			GFP_KERNEL);
	if (!p_effect) {
		dev_err(&pdev->dev, "Can't allocate pcm_p\n");
		return -ENOMEM;
	}

	/* match data */
	p_chipinfo = (struct effect_chipinfo *)
		of_device_get_match_data(dev);
	if (!p_chipinfo)
		dev_warn_once(dev, "check whether to update effect chipinfo\n");
	p_effect->chipinfo = p_chipinfo;

	ret = of_property_read_u32(dev->of_node, "src-clk-freq",
				   &p_effect->syssrc_clk_rate);
	if (ret < 0)
		p_effect->syssrc_clk_rate = 0;
	else
		pr_debug("%s sys-src clk rate from dts:%d\n",
			__func__, p_effect->syssrc_clk_rate);

	p_effect->gate = devm_clk_get(&pdev->dev, "gate");
	if (IS_ERR(p_effect->gate)) {
		dev_err(&pdev->dev,
			"Can't retrieve eqdrc gate clock\n");
		ret = PTR_ERR(p_effect->gate);
		return ret;
	}

	p_effect->srcpll = devm_clk_get(&pdev->dev, "srcpll");
	if (IS_ERR(p_effect->srcpll)) {
		dev_err(&pdev->dev,
			"Can't retrieve source mpll clock\n");
		ret = PTR_ERR(p_effect->srcpll);
		return ret;
	}

	p_effect->clk = devm_clk_get(&pdev->dev, "eqdrc");
	if (IS_ERR(p_effect->clk)) {
		dev_err(&pdev->dev,
			"Can't retrieve eqdrc clock\n");
		ret = PTR_ERR(p_effect->clk);
		return ret;
	}

	ret = clk_set_parent(p_effect->clk, p_effect->srcpll);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set eqdrc clock parent clock\n");
		return ret;
	}

	ret = eqdrc_clk_set(p_effect);
	if (ret < 0) {
		dev_err(&pdev->dev, "set eq drc module clk fail!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"eqdrc_module",
			&eqdrc_module);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve eqdrc_module\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"lane_mask",
			&lane_mask);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve lane_mask\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"channel_mask",
			&channel_mask);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't retrieve channel_mask\n");
		return -EINVAL;
	}

	pr_debug("%s \t module:%d, lane_mask:%d, ch_mask:%d\n",
		__func__,
		eqdrc_module,
		lane_mask,
		channel_mask
		);

	/* config from dts */
	p_effect->lane_mask        = lane_mask;
	p_effect->ch_mask          = channel_mask;
	p_effect->effect_module    = eqdrc_module;

	p_effect->dev = dev;
	s_effect = p_effect;
	dev_set_drvdata(&pdev->dev, p_effect);

	effect_init(pdev);

	/*set master & channel volume gain to 0dB*/
	aed_set_volume(0xc0, 0x30, 0x30);

	return 0;
}

static int effect_platform_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct audioeffect *p_effect = dev_get_drvdata(&pdev->dev);
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(aed_restore_list); i++)
		p_effect->user_setting[i] = eqdrc_read(aed_restore_list[i]);

	aml_set_aed(0, p_effect->effect_module);

	if (!IS_ERR(p_effect->clk)) {
		while (__clk_is_enabled(p_effect->clk))
			clk_disable_unprepare(p_effect->clk);
	}
	pr_info("%s\n", __func__);
	return 0;
}

static int effect_platform_resume(struct platform_device *pdev)
{
	struct audioeffect *p_effect = dev_get_drvdata(&pdev->dev);
	int ret;
	int i = 0;

	if (!IS_ERR(p_effect->clk)) {
		audiobus_write(EE_AUDIO_CLK_GATE_EN0, 0xffffffff);
		audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x7, 0x7);
		clk_set_parent(p_effect->clk, NULL);
		ret = clk_set_parent(p_effect->clk, p_effect->srcpll);
		if (ret) {
			dev_warn(&pdev->dev, "Can't set eqdrc clock parent clock\n");
			return ret;
		}

		ret = eqdrc_clk_set(p_effect);
		if (ret < 0) {
			dev_err(&pdev->dev, "set eq drc module clk fail!\n");
			return -EINVAL;
		}
	}
	effect_init(pdev);

	for (i = 0; i < ARRAY_SIZE(aed_restore_list); i++)
		eqdrc_write(aed_restore_list[i], p_effect->user_setting[i]);

	pr_info("%s\n", __func__);
	return 0;
}

static struct platform_driver effect_platform_driver = {
	.driver = {
		.name           = DRV_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(effect_device_id),
	},
	.probe  = effect_platform_probe,
	.suspend = effect_platform_suspend,
	.resume = effect_platform_resume,
};

int __init effect_platform_init(void)
{
	return platform_driver_register(&(effect_platform_driver));
}

void __exit effect_platform_exit(void)
{
	platform_driver_unregister(&effect_platform_driver);
}

#ifndef MODULE
module_init(effect_platform_init);
module_exit(effect_platform_exit);
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("Amlogic Audio Effects driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
#endif
