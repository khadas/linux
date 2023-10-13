// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/clk.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/auge_utils.h>

#include "../../../soc/amlogic/auge/iomap.h"
#include "../../../soc/amlogic/auge/regs.h"
#include "aml_codec_tl1_acodec.h"

struct tl1_acodec_chipinfo {
	int id;
	bool is_bclk_cap_inv;
	bool is_bclk_o_inv;
	bool is_lrclk_inv;
	int mclk_sel;
	bool separate_toacodec_en;
	int dac_num;
	int lineout_num;
};

struct tl1_acodec_priv {
	struct snd_soc_component *component;
	struct snd_pcm_hw_params *params;
	struct regmap *regmap;
	struct work_struct work;
	struct tl1_acodec_chipinfo *chipinfo;
	struct clk *acodec_clk;
	int tdmout_index;
	int dat0_ch_sel;
	int dat1_ch_sel;

	int tdmin_index;
	int adc_output_sel;
	int dac1_input_sel;
	int dac2_input_sel;
	struct reset_control *rst;
	int diff_output;
	int diff_input;
	int dac_extra_12db;
	int dac_output_invert;
	int lane_offset;
};

static const struct reg_default tl1_acodec_init_list[] = {
	{ACODEC_0, 0x3430BFCF},
	{ACODEC_1, 0x50503030},
	{ACODEC_2, 0xFBFB7400},
	{ACODEC_3, 0x00002222},
	{ACODEC_4, 0x00010000},
	{ACODEC_5, 0xFBFB0033},
	{ACODEC_6, 0x0},
	{ACODEC_7, 0x0},
	{ACODEC_8, 0x0}
};

static struct tl1_acodec_chipinfo tl1_acodec_cinfo = {
	.id = 0,
	.is_bclk_cap_inv = true,
	.is_bclk_o_inv = false,
	.is_lrclk_inv = false,

	.separate_toacodec_en = false,
	.dac_num = 4,
	.lineout_num = 4,
};

static struct tl1_acodec_chipinfo acodec_cinfo_v1 = {
	.id = 0,
	.is_bclk_cap_inv = true,
	.is_bclk_o_inv = false,
	.is_lrclk_inv = false,

	.separate_toacodec_en = true,
	.dac_num = 4,
	.lineout_num = 4,
};

static struct tl1_acodec_chipinfo acodec_cinfo_v2 = {
	.id = 0,
	.is_bclk_cap_inv = true,
	.is_bclk_o_inv = false,
	.is_lrclk_inv = false,

	.separate_toacodec_en = true,
	.dac_num = 2,
	.lineout_num = 2,
};

static struct tl1_acodec_chipinfo acodec_cinfo_v3 = {
	.id = 0,
	.is_bclk_cap_inv = true,
	.is_bclk_o_inv = false,
	.is_lrclk_inv = false,

	.separate_toacodec_en = true,
	.dac_num = 2,
	.lineout_num = 4,
};

static int tl1_acodec_reg_init(struct snd_soc_component *component)
{
	int i;
	struct tl1_acodec_priv *aml_acodec =
				snd_soc_component_get_drvdata(component);
	if (aml_acodec) {
		for (i = 0;
			i < ARRAY_SIZE(tl1_acodec_init_list); i++)
			snd_soc_component_write
			(component,
			tl1_acodec_init_list[i].reg,
			tl1_acodec_init_list[i].def);
	}

	if (aml_acodec && aml_acodec->chipinfo && aml_acodec->diff_output == 1) {
		if (aml_acodec->chipinfo->lineout_num == 4) {
			if (aml_acodec->chipinfo->dac_num == 4)
				snd_soc_component_write(component,
							ACODEC_3,
							0x00002442);
			else
				snd_soc_component_write(component,
							ACODEC_3,
							0x00004444);
		} else if (aml_acodec->chipinfo->lineout_num == 2)
			snd_soc_component_write(component,
						ACODEC_3,
						0x00002400);
	}

	if (aml_acodec && aml_acodec->diff_input == 1)
		snd_soc_component_update_bits(component, ACODEC_1, 0xffff << 0, 0x6969 << 0);

	if (aml_acodec && aml_acodec->chipinfo && aml_acodec->dac_extra_12db == 1) {
		u32 val = 0;

		if (aml_acodec->chipinfo->dac_num == 4) {
			val = snd_soc_component_read32(component, ACODEC_7);
			val |= (0x1 << REG_DAC2_GAIN_SEL_1);
			val &= ~(0x1 << REG_DAC2_GAIN_SEL_0);
			snd_soc_component_write(component, ACODEC_7, val);
		}

		val = snd_soc_component_read32(component, ACODEC_1);
		val |= (0x1 << REG_DAC_GAIN_SEL_1);
		val &= ~(0x1 << REG_DAC_GAIN_SEL_0);
		snd_soc_component_write(component, ACODEC_1, val);
	}

	if (aml_acodec && aml_acodec->dac_output_invert == 1)
		snd_soc_component_update_bits(component, ACODEC_0, 0x3 << 20, 0 << 20);

	return 0;
}

static int aml_dac_gain_get_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);

	u32 reg_addr = ACODEC_1;
	u32 val = snd_soc_component_read32(component, reg_addr);
	u32 val1 = (val & (0x1 <<  REG_DAC_GAIN_SEL_0))
					>> REG_DAC_GAIN_SEL_0;
	u32 val2 = (val & (0x1 <<  REG_DAC_GAIN_SEL_1))
					>> (REG_DAC_GAIN_SEL_1);
	val = val1 | (val2 << 1);

	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_dac_gain_set_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	u32 reg_addr = ACODEC_1;
	u32 val = snd_soc_component_read32(component, reg_addr);

	if (ucontrol->value.enumerated.item[0] == 0) {
		val &= ~(0x1 << REG_DAC_GAIN_SEL_1);
		val &= ~(0x1 << REG_DAC_GAIN_SEL_0);
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		val &= ~(0x1 << REG_DAC_GAIN_SEL_1);
		val |= (0x1 << REG_DAC_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 2) {
		val |= (0x1 << REG_DAC_GAIN_SEL_1);
		val &= ~(0x1 << REG_DAC_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 3) {
		val |= (0x1 << REG_DAC_GAIN_SEL_1);
		val |= (0x1 << REG_DAC_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	}

	snd_soc_component_write(component, reg_addr, val);
	return 0;
}

static int aml_dac2_gain_get_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);

	u32 reg_addr = ACODEC_7;
	u32 val = snd_soc_component_read32(component, reg_addr);
	u32 val1 = (val & (0x1 <<  REG_DAC2_GAIN_SEL_0))
					>> REG_DAC_GAIN_SEL_0;
	u32 val2 = (val & (0x1 <<  REG_DAC2_GAIN_SEL_1))
					>> (REG_DAC2_GAIN_SEL_1);
	val = val1 | (val2 << 1);

	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_dac2_gain_set_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	u32 reg_addr = ACODEC_7;
	u32 val = snd_soc_component_read32(component, reg_addr);

	if (ucontrol->value.enumerated.item[0] == 0) {
		val &= ~(0x1 << REG_DAC2_GAIN_SEL_1);
		val &= ~(0x1 << REG_DAC2_GAIN_SEL_0);
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		val &= ~(0x1 << REG_DAC2_GAIN_SEL_1);
		val |= (0x1 << REG_DAC2_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 2) {
		val |= (0x1 << REG_DAC2_GAIN_SEL_1);
		val &= ~(0x1 << REG_DAC2_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 3) {
		val |= (0x1 << REG_DAC2_GAIN_SEL_1);
		val |= (0x1 << REG_DAC2_GAIN_SEL_0);
		pr_info("It has risk of distortion!\n");
	}

	snd_soc_component_write(component, reg_addr, val);
	return 0;
}

static int aml_DAC_source_sel_get_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct tl1_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);
	int val = 0;

	if (aml_acodec->lane_offset == 4) {
		val = (audiobus_read(EE_AUDIO_TOACODEC_CTRL0) >> 16) & (0xf);
		val -= (aml_acodec->tdmout_index << 2);
	} else {
		val = (audiobus_read(EE_AUDIO_TOACODEC_CTRL0) >> 16) & (0x1f);
		val -= (aml_acodec->tdmout_index << 3);
	}

	if (val < 0 || val > 3) {
		pr_info("Warning: tdmout_index = %d, val = 0x%x\n", aml_acodec->tdmout_index, val);
		val = 0;
	}
	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_DAC_source_sel_set_enum
		(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct tl1_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);
	u32 val = ucontrol->value.enumerated.item[0];

	if (val > 3) {
		pr_info("Warning: tdmout_index = %d, val = 0x%x\n", aml_acodec->tdmout_index, val);
		return 0;
	}

	if (aml_acodec->lane_offset == 4) {
		val += (aml_acodec->tdmout_index << 2);
		audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0, (0xf << 16), (val << 16));
	} else {
		val += (aml_acodec->tdmout_index << 3);
		audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0, (0x1f << 16), (val << 16));
	}

	return 0;
}

static const DECLARE_TLV_DB_SCALE(pga_in_tlv, -1200, 250, 1);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -29625, 375, 1);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95250, 375, 1);
static const DECLARE_TLV_DB_SCALE(dac2_vol_tlv, -95250, 375, 1);

static const char *const dac_gain_texts[] = { "0dB", "6dB", "12dB", "18dB" };
static const char *const dac2_gain_texts[] = { "0dB", "6dB", "12dB", "18dB" };
static const char *const DAC_Src_texts[] = {"Lane0", "Lane1", "Lane2", "Lane3"};

static const struct soc_enum dac_gain_enum =
	SOC_ENUM_SINGLE
			(SND_SOC_NOPM, 0,
			ARRAY_SIZE(dac_gain_texts),
			dac_gain_texts);
static const struct soc_enum dac2_gain_enum =
	SOC_ENUM_SINGLE
			(SND_SOC_NOPM, 0,
			ARRAY_SIZE(dac2_gain_texts),
			dac2_gain_texts);
static const struct soc_enum DAC_source_sel_enum =
	SOC_ENUM_SINGLE
			(SND_SOC_NOPM, 0, ARRAY_SIZE(DAC_Src_texts),
			DAC_Src_texts);

static const struct snd_kcontrol_new tl1_acodec_snd_controls[] = {
	/*PGA_IN Gain */
	SOC_DOUBLE_TLV
			("PGA IN Gain", ACODEC_1,
			PGAL_IN_GAIN, PGAR_IN_GAIN,
			0x1f, 0, pga_in_tlv),

	/*ADC Digital Volume control */
	SOC_DOUBLE_TLV
			("ADC Digital Capture Volume", ACODEC_1,
			ADCL_VC, ADCR_VC,
			0x7f, 0, adc_vol_tlv),

	/*DAC Digital Volume control */
	SOC_DOUBLE_TLV
			("DAC Digital Playback Volume",
			ACODEC_2,
			DACL_VC, DACR_VC,
			0xff, 0, dac_vol_tlv),

	/*DAC extra Digital Gain control */
	SOC_ENUM_EXT
			("DAC Extra Digital Gain",
			dac_gain_enum,
			aml_dac_gain_get_enum,
			aml_dac_gain_set_enum),

	SOC_ENUM_EXT("DAC SOURCE SELECT",
			DAC_source_sel_enum,
			aml_DAC_source_sel_get_enum,
			aml_DAC_source_sel_set_enum),
};

static const struct snd_kcontrol_new tl1_acodec_snd_dac2_controls[] = {
	/*DAC 2 Digital Volume control */
	SOC_DOUBLE_TLV
			("DAC 2 Digital Playback Volume",
			ACODEC_5,
			DAC2L_VC, DAC2R_VC,
			0xff, 0, dac2_vol_tlv),

	/*DAC 2 extra Digital Gain control */
	SOC_ENUM_EXT
			("DAC2 Extra Digital Gain",
			dac2_gain_enum,
			aml_dac2_gain_get_enum,
			aml_dac2_gain_set_enum),
};

/*pgain Left Channel Input */
static const char * const linein_left_txt[] = {
	"None", "AIL1", "AIL2", "AIL3", "AIL4",
};

static SOC_ENUM_SINGLE_DECL(linein_left_enum,
				  ACODEC_1,
				  PGAL_IN_SEL, linein_left_txt);

static const struct snd_kcontrol_new lil_mux =
SOC_DAPM_ENUM("ROUTE_L", linein_left_enum);

/*pgain right Channel Input */
static const char * const linein_right_txt[] = {
	"None", "AIR1", "AIR2", "AIR3", "AIR4",
};

static SOC_ENUM_SINGLE_DECL(linein_right_enum,
				  ACODEC_1,
				  PGAR_IN_SEL, linein_right_txt);

static const struct snd_kcontrol_new lir_mux =
		SOC_DAPM_ENUM("ROUTE_R", linein_right_enum);

/*line out 1 Left mux */
static const char * const out_lo1l_txt[] = {
	"None", "LO1L_SEL_INL", "LO1L_SEL_DACL", "Reserved", "LO1L_SEL_DACR_INV"
};

static SOC_ENUM_SINGLE_DECL
		(out_lo1l_enum, ACODEC_3,
		LO1L_SEL_INL, out_lo1l_txt);

static const struct snd_kcontrol_new lo1l_mux =
		SOC_DAPM_ENUM("LO1L_MUX", out_lo1l_enum);

/*line out 1 right mux */
static const char * const out_lo1r_txt[] = {
	"None", "LO1R_SEL_INR", "LO1R_SEL_DACR", "Reserved", "LO1R_SEL_DACL_INV"
};

static SOC_ENUM_SINGLE_DECL
		(out_lo1r_enum, ACODEC_3,
		LO1R_SEL_INR, out_lo1r_txt);

static const struct snd_kcontrol_new lo1r_mux =
		SOC_DAPM_ENUM("LO1R_MUX", out_lo1r_enum);

/*line out 2 left mux */
static const char * const out_lo2l_txt[] = {
	"None", "LO2L_SEL_INL", "LO2L_SEL_DAC2L", "Reserved",
	"LO2L_SEL_DAC2R_INV"
};

static SOC_ENUM_SINGLE_DECL(out_lo2l_enum, ACODEC_3,
				  LO2L_SEL_INL, out_lo2l_txt);

static const struct snd_kcontrol_new lo2l_mux =
SOC_DAPM_ENUM("LO2L_MUX", out_lo2l_enum);

/*line out 2 Right mux */
static const char * const out_lo2r_txt[] = {
	"None", "LO2R_SEL_INR", "LO2R_SEL_DAC2R", "Reserved",
	"LO2R_SEL_DAC2L_INV"
};

static SOC_ENUM_SINGLE_DECL(out_lo2r_enum, ACODEC_3,
				  LO2R_SEL_INR, out_lo2r_txt);

static const struct snd_kcontrol_new lo2r_mux =
SOC_DAPM_ENUM("LO2R_MUX", out_lo2r_enum);

static const struct snd_soc_dapm_widget tl1_acodec_dapm_widgets[] = {
	/* Input */
	SND_SOC_DAPM_INPUT("Linein left 1"),
	SND_SOC_DAPM_INPUT("Linein left 2"),
	SND_SOC_DAPM_INPUT("Linein left 3"),
	SND_SOC_DAPM_INPUT("Linein left 4"),

	SND_SOC_DAPM_INPUT("Linein right 1"),
	SND_SOC_DAPM_INPUT("Linein right 2"),
	SND_SOC_DAPM_INPUT("Linein right 3"),
	SND_SOC_DAPM_INPUT("Linein right 4"),

	/*PGA input */
	SND_SOC_DAPM_PGA("PGAL_IN_EN", SND_SOC_NOPM,
			 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGAR_IN_EN", SND_SOC_NOPM,
			 0, 0, NULL, 0),

	/*PGA input source select */
	SND_SOC_DAPM_MUX("Linein left switch", SND_SOC_NOPM,
			 0, 0, &lil_mux),
	SND_SOC_DAPM_MUX("Linein right switch", SND_SOC_NOPM,
			 0, 0, &lir_mux),

	/*ADC capture stream */
	SND_SOC_DAPM_ADC("Left ADC", "Capture", SND_SOC_NOPM,
			 0, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Capture", SND_SOC_NOPM,
			 0, 0),

	/*Output */
	SND_SOC_DAPM_OUTPUT("Lineout 1 left"),
	SND_SOC_DAPM_OUTPUT("Lineout 1 right"),
	SND_SOC_DAPM_OUTPUT("Lineout 2 left"),
	SND_SOC_DAPM_OUTPUT("Lineout 2 right"),

	/*DAC playback stream */
	SND_SOC_DAPM_DAC
			("Left DAC", "Playback",
			SND_SOC_NOPM,
			0, 0),
	SND_SOC_DAPM_DAC
			("Right DAC", "Playback",
			SND_SOC_NOPM,
			0, 0),

	/*DAC 2 playback stream */
	SND_SOC_DAPM_DAC
			("Left DAC2", "Playback",
			SND_SOC_NOPM,
			0, 0),
	SND_SOC_DAPM_DAC
			("Right DAC2", "Playback",
			SND_SOC_NOPM,
			0, 0),

	/*DRV output */
	SND_SOC_DAPM_OUT_DRV
			("LO1L_OUT_EN", ACODEC_0,
			LO1L_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV
			("LO1R_OUT_EN", ACODEC_0,
			LO1R_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV
			("LO2L_OUT_EN", ACODEC_0,
			LO2L_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV
			("LO2R_OUT_EN", ACODEC_0,
			LO2R_EN, 0, NULL, 0),

	/*MUX output source select */
	SND_SOC_DAPM_MUX("Lineout 1 left switch", SND_SOC_NOPM,
			 0, 0, &lo1l_mux),
	SND_SOC_DAPM_MUX("Lineout 1 right switch", SND_SOC_NOPM,
			 0, 0, &lo1r_mux),
	SND_SOC_DAPM_MUX("Lineout 2 left switch", SND_SOC_NOPM,
			 0, 0, &lo2l_mux),
	SND_SOC_DAPM_MUX("Lineout 2 right switch", SND_SOC_NOPM,
			 0, 0, &lo2r_mux),

};

static const struct snd_soc_dapm_route tl1_acodec_dapm_routes[] = {
/* Input path */
	{"Linein left switch", "AIL1", "Linein left 1"},
	{"Linein left switch", "AIL2", "Linein left 2"},
	{"Linein left switch", "AIL3", "Linein left 3"},
	{"Linein left switch", "AIL4", "Linein left 4"},

	{"Linein right switch", "AIR1", "Linein right 1"},
	{"Linein right switch", "AIR2", "Linein right 2"},
	{"Linein right switch", "AIR3", "Linein right 3"},
	{"Linein right switch", "AIR4", "Linein right 4"},

	{"PGAL_IN_EN", NULL, "Linein left switch"},
	{"PGAR_IN_EN", NULL, "Linein right switch"},

	{"Left ADC", NULL, "PGAL_IN_EN"},
	{"Right ADC", NULL, "PGAR_IN_EN"},

/*Output path*/
	{"Lineout 1 left switch", NULL, "Left DAC"},
	{"Lineout 1 left switch", NULL, "Right DAC"},
	{"Lineout 1 left switch", NULL, "PGAL_IN_EN"},

	{"Lineout 1 right switch", NULL, "Right DAC"},
	{"Lineout 1 right switch", NULL, "Left DAC"},
	{"Lineout 1 right switch", NULL, "PGAR_IN_EN"},

	{"Lineout 2 left switch", NULL, "Left DAC2"},
	{"Lineout 2 left switch", NULL, "Right DAC2"},
	{"Lineout 2 left switch", NULL, "PGAL_IN_EN"},

	{"Lineout 2 right switch", NULL, "Right DAC2"},
	{"Lineout 2 right switch", NULL, "Left DAC2"},
	{"Lineout 2 right switch", NULL, "PGAR_IN_EN"},

	{"LO1L_OUT_EN", NULL, "Lineout 1 left switch"},
	{"LO1R_OUT_EN", NULL, "Lineout 1 right switch"},
	{"LO2L_OUT_EN", NULL, "Lineout 2 left switch"},
	{"LO2R_OUT_EN", NULL, "Lineout 2 right switch"},

	{"Lineout 1 left", NULL, "LO1L_OUT_EN"},
	{"Lineout 1 right", NULL, "LO1R_OUT_EN"},
	{"Lineout 2 left", NULL, "LO2L_OUT_EN"},
	{"Lineout 2 right", NULL, "LO2R_OUT_EN"},
};

static int tl1_acodec_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	u32 val = snd_soc_component_read32(component, ACODEC_0);

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

	snd_soc_component_write(component, ACODEC_0, val);

	return 0;
}

static int tl1_acodec_dai_set_sysclk
	(struct snd_soc_dai *dai, int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int tl1_acodec_dai_hw_params
	(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct tl1_acodec_priv *aml_acodec =
	    snd_soc_component_get_drvdata(component);

	pr_debug("%s!\n", __func__);

	aml_acodec->params = params;

	return 0;
}

static int tl1_acodec_dai_set_bias_level
	(struct snd_soc_component *component,
	enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		snd_soc_component_write(component, ACODEC_8, 0x3);

		break;

	case SND_SOC_BIAS_PREPARE:

		break;

	case SND_SOC_BIAS_STANDBY:
		if (component->dapm.bias_level == SND_SOC_BIAS_OFF) {
			snd_soc_component_cache_sync(component);
			snd_soc_component_write(component, ACODEC_0, tl1_acodec_init_list[0].def);
		}
		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_component_update_bits(component, ACODEC_2, 1 << DAC_SOFT_MUTE,
							1 << DAC_SOFT_MUTE);
		snd_soc_component_update_bits(component, ACODEC_6, 1 << DAC2_SOFT_MUTE,
							1 << DAC2_SOFT_MUTE);
		snd_soc_component_write(component, ACODEC_0, 0);
		snd_soc_component_write(component, ACODEC_8, 0);
		break;

	default:
		break;
	}
	component->dapm.bias_level = level;

	return 0;
}

static int tl1_acodec_dai_prepare
	(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return 0;
}

static int tl1_acodec_reset(struct snd_soc_component *component)
{
	struct tl1_acodec_priv *tl1_acodec = snd_soc_component_get_drvdata(component);

	if (tl1_acodec && !IS_ERR(tl1_acodec->rst)) {
		pr_info("call standard reset interface\n");
		reset_control_reset(tl1_acodec->rst);
		usleep_range(950, 1000);
	} else {
		pr_info("no call standard reset interface\n");
	}
	return 0;
}

static int tl1_acodec_start_up(struct snd_soc_component *component)
{
	snd_soc_component_write(component, ACODEC_0, 0xF000);
	msleep(200);
	snd_soc_component_write(component, ACODEC_0, 0xB000);

	return 0;
}

static int tl1_acodec_set_toacodec(struct tl1_acodec_priv *aml_acodec);

static void tl1_acodec_release_fast_mode_work_func(struct work_struct *p_work)
{
	struct tl1_acodec_priv *aml_acodec;
	struct snd_soc_component *component;
	int i;

	aml_acodec = container_of(p_work, struct tl1_acodec_priv, work);
	if (!aml_acodec) {
		pr_err("%s, Get tl1_acodec_priv fail\n", __func__);
		return;
	}

	component = aml_acodec->component;
	if (!component) {
		pr_err("%s, Get snd_soc_codec fail\n", __func__);
		return;
	}

	pr_info("%s\n", __func__);
	tl1_acodec_set_toacodec(aml_acodec);
	/*
	 * reset audio codec register
	 * after init, need do reset again.
	 * only reset before init acodec, there is the
	 * output phase difference of left and right channels.
	 */
	for (i = 0; i < 2; i++) {
		tl1_acodec_reset(component);
		snd_soc_component_write(component, ACODEC_0, 0xF000);
		msleep(200);
		snd_soc_component_write(component, ACODEC_0, 0xB000);
		tl1_acodec_reg_init(component);
	}

	aml_acodec->component = component;
	tl1_acodec_dai_set_bias_level(component, SND_SOC_BIAS_STANDBY);
}

static int tl1_acodec_dai_mute_stream
		(struct snd_soc_dai *dai, int mute,
		int stream)
{
	struct snd_soc_component *component = dai->component;

	pr_debug("%s, mute:%d\n", __func__, mute);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (mute) {
			/* DAC 1 */
			snd_soc_component_update_bits(component, ACODEC_2,
						1 << DAC_SOFT_MUTE,
						1 << DAC_SOFT_MUTE);
			/* DAC 2 */
			snd_soc_component_update_bits(component, ACODEC_6,
						1 << DAC2_SOFT_MUTE,
						1 << DAC2_SOFT_MUTE);
		} else {
			snd_soc_component_update_bits(component, ACODEC_2,
						1 << DAC_SOFT_MUTE, 0);
			snd_soc_component_update_bits(component, ACODEC_6,
						1 << DAC2_SOFT_MUTE, 0);
		}
	}

	return 0;
}

static int tl1_acodec_dai_trigger(struct snd_pcm_substream *substream, int cmd,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			pr_debug("%s(), start\n", __func__);
			snd_soc_component_update_bits(component, ACODEC_2,
						1 << DAC_SOFT_MUTE, 0);
			snd_soc_component_update_bits(component, ACODEC_6,
						1 << DAC2_SOFT_MUTE, 0);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			pr_debug("%s(), stop\n", __func__);
			/* DAC 1 */
			snd_soc_component_update_bits(component, ACODEC_2,
						1 << DAC_SOFT_MUTE,
						1 << DAC_SOFT_MUTE);
			/* DAC 2 */
			snd_soc_component_update_bits(component, ACODEC_6,
						1 << DAC2_SOFT_MUTE,
						1 << DAC2_SOFT_MUTE);
			break;
		}
	}
	return 0;
}

struct snd_soc_dai_ops tl1_acodec_dai_ops = {
	.hw_params = tl1_acodec_dai_hw_params,
	.prepare = tl1_acodec_dai_prepare,
	.set_fmt = tl1_acodec_dai_set_fmt,
	.set_sysclk = tl1_acodec_dai_set_sysclk,
	.mute_stream = tl1_acodec_dai_mute_stream,
	.trigger = tl1_acodec_dai_trigger,
};

static int tl1_acodec_probe(struct snd_soc_component *component)
{
	struct tl1_acodec_priv *aml_acodec =
		snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (!aml_acodec) {
		pr_err("Failed to get tl1 acodec priv\n");
		return -EINVAL;
	}
	aml_acodec->component = component;
	aml_acodec->rst = devm_reset_control_get(component->dev, "acodec");
	INIT_WORK(&aml_acodec->work, tl1_acodec_release_fast_mode_work_func);
	schedule_work(&aml_acodec->work);

	if (aml_acodec->chipinfo->dac_num == 4) {
		ret = snd_soc_add_component_controls(component,
						tl1_acodec_snd_dac2_controls,
						ARRAY_SIZE(tl1_acodec_snd_dac2_controls));
	}

	if (ret < 0) {
		dev_err(component->dev, "%s: could not add kcontrol for component (err=%d)\n",
			 __func__, ret);
		return ret;
	}

	pr_info("%s\n", __func__);
	return 0;
}

static void tl1_acodec_remove(struct snd_soc_component *component)
{
	struct tl1_acodec_priv *aml_acodec =
		snd_soc_component_get_drvdata(component);
	pr_info("%s!\n", __func__);
	cancel_work_sync(&aml_acodec->work);
	tl1_acodec_dai_set_bias_level(component, SND_SOC_BIAS_OFF);
}

static int tl1_acodec_suspend(struct snd_soc_component *component)
{
	pr_info("%s!\n", __func__);

	tl1_acodec_dai_set_bias_level(component, SND_SOC_BIAS_OFF);

	return 0;
}

static int tl1_acodec_resume(struct snd_soc_component *component)
{
	struct tl1_acodec_priv *aml_acodec =
		snd_soc_component_get_drvdata(component);

	if (!aml_acodec) {
		pr_err("Failed to get tl1 acodec priv\n");
		return -EINVAL;
	}
	pr_info("%s\n", __func__);
	tl1_acodec_set_toacodec(aml_acodec);
	tl1_acodec_reset(component);
	tl1_acodec_start_up(component);
	tl1_acodec_reg_init(component);

	tl1_acodec_dai_set_bias_level(component, SND_SOC_BIAS_STANDBY);

	return 0;
}

const static struct snd_soc_component_driver soc_codec_dev_tl1_acodec = {
	.probe = tl1_acodec_probe,
	.remove = tl1_acodec_remove,
	.suspend = tl1_acodec_suspend,
	.resume = tl1_acodec_resume,
	.set_bias_level = tl1_acodec_dai_set_bias_level,
	.controls = tl1_acodec_snd_controls,
	.num_controls = ARRAY_SIZE(tl1_acodec_snd_controls),
	.dapm_widgets = tl1_acodec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tl1_acodec_dapm_widgets),
	.dapm_routes = tl1_acodec_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(tl1_acodec_dapm_routes),
};

static const struct regmap_config tl1_acodec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x20,
	.reg_defaults = tl1_acodec_init_list,
	.num_reg_defaults = ARRAY_SIZE(tl1_acodec_init_list),
	.cache_type = REGCACHE_RBTREE,
};

#define TL1_ACODEC_RATES		SNDRV_PCM_RATE_8000_96000
#define TL1_ACODEC_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_dai_driver aml_tl1_acodec_dai = {
	.name = "tl1-acodec-hifi",
	.id = 0,
	.playback = {
	      .stream_name = "Playback",
	      .channels_min = 2,
	      .channels_max = 8,
	      .rates = TL1_ACODEC_RATES,
	      .formats = TL1_ACODEC_FORMATS,
	      },
	.capture = {
	     .stream_name = "Capture",
	     .channels_min = 2,
	     .channels_max = 8,
	     .rates = TL1_ACODEC_RATES,
	     .formats = TL1_ACODEC_FORMATS,
	     },
	.ops = &tl1_acodec_dai_ops,
};

static int tl1_acodec_set_toacodec(struct tl1_acodec_priv *aml_acodec)
{
	int dat0_sel, dat1_sel, lrclk_sel, bclk_sel, mclk_sel;
	unsigned int update_bits_msk = 0x0, update_bits = 0x0;

	update_bits_msk = 0xFF7777;
	if (aml_acodec->chipinfo->is_bclk_cap_inv)
		update_bits |= (0x1 << 9);
	if (aml_acodec->chipinfo->is_bclk_o_inv)
		update_bits |= (0x1 << 8);
	if (aml_acodec->chipinfo->is_lrclk_inv)
		update_bits |= (0x1 << 10);

	if (aml_acodec->lane_offset == 4) {
		dat0_sel = (aml_acodec->tdmout_index << 2) + aml_acodec->dat0_ch_sel;
		dat0_sel = dat0_sel << 16;
		dat1_sel = (aml_acodec->tdmout_index << 2) + aml_acodec->dat1_ch_sel;
		dat1_sel = dat1_sel << 20;
	} else {
		dat0_sel = (aml_acodec->tdmout_index << 3) + aml_acodec->dat0_ch_sel;
		dat0_sel = dat0_sel << 16;
		dat1_sel = (aml_acodec->tdmout_index << 3) + aml_acodec->dat1_ch_sel;
		dat1_sel = dat1_sel << 22;
	}

	lrclk_sel = (aml_acodec->tdmout_index) << 12;
	bclk_sel = (aml_acodec->tdmout_index) << 4;

	//mclk_sel = aml_acodec->chipinfo->mclk_sel;
	mclk_sel = aml_acodec->tdmin_index;

	update_bits |= dat0_sel | dat1_sel | lrclk_sel | bclk_sel | mclk_sel;

	audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0, update_bits_msk, update_bits);

	/* if toacodec_en is separated, need do:
	 * step1: enable/disable mclk
	 * step2: enable/disable bclk
	 * step3: enable/disable dat
	 */
	if (aml_acodec->chipinfo->separate_toacodec_en) {
		audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0, 0x20000000, 0x1 << 29);
		audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0, 0x40000000, 0x1 << 30);
	}
	audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0, 0x80000000, 0x1 << 31);

	pr_info("%s, is_bclk_cap_inv %s\n", __func__,
		aml_acodec->chipinfo->is_bclk_cap_inv ? "true" : "false");
	pr_info("%s, is_bclk_o_inv %s\n", __func__,
		aml_acodec->chipinfo->is_bclk_o_inv ? "true" : "false");
	pr_info("%s, is_lrclk_inv %s\n", __func__,
		aml_acodec->chipinfo->is_lrclk_inv ? "true" : "false");
	pr_info("%s read EE_AUDIO_TOACODEC_CTRL0=0x%08x\n", __func__,
		audiobus_read(EE_AUDIO_TOACODEC_CTRL0));

	return 0;
}

static int aml_tl1_acodec_probe(struct platform_device *pdev)
{
	struct tl1_acodec_priv *aml_acodec;
	struct tl1_acodec_chipinfo *p_chipinfo;
	struct resource *res_mem;
	struct device_node *np;
	void __iomem *regs;
	int ret = 0;

	dev_info(&pdev->dev, "%s\n", __func__);

	np = pdev->dev.of_node;

	aml_acodec = devm_kzalloc
			(&pdev->dev,
			sizeof(struct tl1_acodec_priv),
			GFP_KERNEL);
	if (!aml_acodec)
		return -ENOMEM;
	/* match data */
	p_chipinfo = (struct tl1_acodec_chipinfo *)
		of_device_get_match_data(&pdev->dev);
	if (!p_chipinfo)
		dev_warn_once(&pdev->dev, "check whether to update tl1_acodec_chipinfo\n");

	aml_acodec->chipinfo = p_chipinfo;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem)
		return -ENODEV;

	regs = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	aml_acodec->regmap = devm_regmap_init_mmio
			(&pdev->dev, regs, &tl1_acodec_regmap_config);
	if (IS_ERR(aml_acodec->regmap))
		return PTR_ERR(aml_acodec->regmap);

	aml_acodec->acodec_clk = devm_clk_get(&pdev->dev, "acodec_clk");
	if (!IS_ERR(aml_acodec->acodec_clk))
		clk_prepare_enable(aml_acodec->acodec_clk);
	else
		dev_info(&pdev->dev, "Can't retrieve acodec clock\n");

	of_property_read_u32
			(pdev->dev.of_node,
			"tdmout_index",
			&aml_acodec->tdmout_index);

	of_property_read_u32(pdev->dev.of_node,
			"dat0_ch_sel", &aml_acodec->dat0_ch_sel);

	of_property_read_u32
			(pdev->dev.of_node,
			"dat1_ch_sel",
			&aml_acodec->dat1_ch_sel);

	of_property_read_u32
			(pdev->dev.of_node,
			"tdmin_index",
			&aml_acodec->tdmin_index);

	of_property_read_u32
			(pdev->dev.of_node,
			"diff_output",
			&aml_acodec->diff_output);

	of_property_read_u32
			(pdev->dev.of_node,
			"diff_input",
			&aml_acodec->diff_input);

	of_property_read_u32
			(pdev->dev.of_node,
			"dac_extra_12db",
			&aml_acodec->dac_extra_12db);

	of_property_read_u32
			(pdev->dev.of_node,
			"dac_output_invert",
			&aml_acodec->dac_output_invert);

	of_property_read_u32
			(pdev->dev.of_node,
			"lane_offset",
			&aml_acodec->lane_offset);

	pr_info("aml_tl1_acodec tdmout_index %d tdmin_index %d dat0_ch_sel %d dat1_ch_sel %d\n",
		aml_acodec->tdmout_index, aml_acodec->tdmin_index,
		aml_acodec->dat0_ch_sel, aml_acodec->dat1_ch_sel);

	pr_info("aml_tl1_acodec diff_output %d diff_input %d dac_extra_12db %d dac_output_invert %d lane_offset %d\n",
		aml_acodec->diff_output, aml_acodec->diff_input, aml_acodec->dac_extra_12db,
		aml_acodec->dac_output_invert, aml_acodec->lane_offset);

	platform_set_drvdata(pdev, aml_acodec);

	ret = devm_snd_soc_register_component
			(&pdev->dev,
			&soc_codec_dev_tl1_acodec,
			&aml_tl1_acodec_dai, 1);
	if (ret)
		pr_info("%s call snd_soc_register_codec error\n", __func__);
	else
		pr_info("%s over\n", __func__);

	return ret;
}

static int aml_tl1_acodec_remove(struct platform_device *pdev)
{
	struct tl1_acodec_priv *aml_acodec;

	aml_acodec = platform_get_drvdata(pdev);

	if (!IS_ERR(aml_acodec->acodec_clk))
		clk_disable_unprepare(aml_acodec->acodec_clk);

	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

static void aml_tl1_acodec_shutdown(struct platform_device *pdev)
{
	struct tl1_acodec_priv *aml_acodec;
	struct snd_soc_component *component;

	aml_acodec = platform_get_drvdata(pdev);
	component = aml_acodec->component;

	if (!IS_ERR(aml_acodec->acodec_clk))
		clk_disable_unprepare(aml_acodec->acodec_clk);

	if (component)
		tl1_acodec_dai_set_bias_level(component, SND_SOC_BIAS_OFF);
}

static const struct of_device_id aml_tl1_acodec_dt_match[] = {
	{
		.compatible = "amlogic, tl1_acodec",
		.data = &tl1_acodec_cinfo,
	},
	{
		.compatible = "amlogic, tm2_revb_acodec",
		.data = &acodec_cinfo_v1,
	},
	{
		.compatible = "amlogic, t3_acodec",
		.data = &acodec_cinfo_v2,
	},
	{
		.compatible = "amlogic, t5w_acodec",
		.data = &acodec_cinfo_v3,
	},
	{},
};

static struct platform_driver aml_tl1_acodec_platform_driver = {
	.driver = {
		   .name = "tl1_acodec",
		   .owner = THIS_MODULE,
		   .of_match_table = aml_tl1_acodec_dt_match,
		   },
	.probe = aml_tl1_acodec_probe,
	.remove = aml_tl1_acodec_remove,
	.shutdown = aml_tl1_acodec_shutdown,
};

static int __init aml_tl1_acodec_modinit(void)
{
	int ret = 0;

	ret = platform_driver_register(&aml_tl1_acodec_platform_driver);
	if (ret != 0)
		pr_err("register tl1 acodec fail: %d\n", ret);

	return ret;
}

module_init(aml_tl1_acodec_modinit);

static void __exit aml_tl1_acodec_modexit(void)
{
	platform_driver_unregister(&aml_tl1_acodec_platform_driver);
}

module_exit(aml_tl1_acodec_modexit);

MODULE_DESCRIPTION("ASoC AML TL1 audio codec driver");
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
