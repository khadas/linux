// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include "audio_utils.h"
#include "regs.h"
#include "iomap.h"
#include "spdif_hw.h"
#include "resample.h"
#include "effects_v2.h"
#include "vad.h"
#include "ddr_mngr.h"
#include "card.h"
#include "tdm_hw.h"
#include "acc.h"
#include "soft_locker.h"

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/auge_utils.h>
#include <linux/amlogic/cpu_version.h>

struct snd_elem_info {
	struct soc_enum *ee;
	int reg;
	int shift;
	u32 mask;
};

/* For S4 HIFI used by EMMC/audio, we need force mpll in DTV */
static bool force_mpll_clk;

#define SND_BYTE(xname, type, func, xshift, xmask)   \
{                                      \
	.iface = SNDRV_CTL_ELEM_IFACE_PCM, \
	.name  = xname,                \
	.info  = snd_byte_info,        \
	.get   = snd_byte_get,         \
	.put   = snd_byte_set,         \
	.private_value =               \
	((unsigned long)&(struct snd_elem_info) \
		{.reg = EE_AUDIO_##type##_##func,   \
		 .shift = xshift, .mask = xmask })  \
}

static int snd_enum_info(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_info *uinfo)
{
	struct snd_elem_info *einfo = (void *)kcontrol->private_value;
	struct soc_enum *e = (struct soc_enum *)einfo->ee;

	return snd_ctl_enum_info(uinfo, e->shift_l == e->shift_r ? 1 : 2,
				 e->items, e->texts);
}

static int snd_enum_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int val;
	struct snd_elem_info *einfo = (void *)kcontrol->private_value;

	/* pr_info("%s:reg:0x%x, mask:0x%x",
	 *		__func__,
	 *		einfo->reg,
	 *		einfo->mask);
	 */

	val = audiobus_read(einfo->reg);
	val >>= einfo->shift;
	val &= einfo->mask;
	ucontrol->value.integer.value[0] = val;

	/* pr_info("\t val:0x%x\n", val); */

	return 0;
}

static int snd_enum_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_elem_info *einfo = (void *)kcontrol->private_value;
	int val  = (int)ucontrol->value.integer.value[0];

	/*	pr_info("%s:reg:0x%x, swap mask:0x%x, val:0x%x\n",
	 *		__func__,
	 *		einfo->reg,
	 *		einfo->mask,
	 *		val);
	 */
	audiobus_update_bits(einfo->reg,
			     einfo->mask << einfo->shift,
			     val << einfo->shift);

	return 0;
}

#define SND_ENUM(xname, type, func, xenum, xshift, xmask)   \
{                                      \
	.iface = SNDRV_CTL_ELEM_IFACE_PCM, \
	.name  = xname,                    \
	.info  = snd_enum_info,            \
	.get   = snd_enum_get,             \
	.put   = snd_enum_set,             \
	.private_value = ((unsigned long)&(struct snd_elem_info) \
		{.reg = EE_AUDIO_##type##_##func,   \
		 .ee = (struct soc_enum *)&(xenum),   \
		 .shift = xshift, .mask = xmask }) \
}

static const char * const in_swap_channel_text[] = {
	"Swap To Lane0 Left Channel",
	"Swap To Lane0 Right Channel",
	"Swap To Lane1 Left Channel",
	"Swap To Lane1 Right Channel",
	"Swap To Lane2 Left Channel",
	"Swap To Lane2 Right Channel",
	"Swap To Lane3 Left Channel",
	"Swap To Lane3 Right Channel",
};

static const struct soc_enum in_swap_channel_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(in_swap_channel_text),
			    in_swap_channel_text);

static const char * const out_swap_channel_text[] = {
	"Swap To CH0",
	"Swap To CH1",
	"Swap To CH2",
	"Swap To CH3",
	"Swap To CH4",
	"Swap To CH5",
	"Swap To CH6",
	"Swap To CH7",
};

static const struct soc_enum out_swap_channel_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(out_swap_channel_text),
			    out_swap_channel_text);

static const char * const lane0_mixer_text[] = {
	"Disable Mix",
	"Lane0 Mix Left and Right Channel",
};

static const struct soc_enum lane0_mixer_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lane0_mixer_text),
			    lane0_mixer_text);

static const char * const lane1_mixer_text[] = {
	"Disable Mix",
	"Lane1 Mix Left and Right Channel",
};

static const struct soc_enum lane1_mixer_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lane1_mixer_text),
			    lane1_mixer_text);

static const char * const lane2_mixer_text[] = {
	"Disable Mix",
	"Lane2 Mix Left and Right Channel",
};

static const struct soc_enum lane2_mixer_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lane2_mixer_text),
			    lane2_mixer_text);

static const char * const lane3_mixer_text[] = {
	"Disable Mix",
	"Lane3 Mix Left and Right Channel",
};

static const struct soc_enum lane3_mixer_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lane3_mixer_text),
			    lane3_mixer_text);

static const char * const spdif_channel_status_text[] = {
	"Channel A Status[31:0]",
	"Channel A Status[63:32]",
	"Channel A Status[95:64]",
	"Channel A Status[127:96]",
	"Channel A Status[159:128]",
	"Channel A Status[191:160]",
	"Channel B Status[31:0]",
	"Channel B Status[63:32]",
	"Channel B Status[95:64]",
	"Channel B Status[127:96]",
	"Channel B Status[159:128]",
	"Channel B Status[191:160]",
};

static const struct soc_enum spdif_channel_status_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spdif_channel_status_text),
			    spdif_channel_status_text);

static int spdifin_channel_status;

static int spdifout_channel_status;

#define SPDIFIN_CHSTS_REG EE_AUDIO_SPDIFIN_STAT1

#define SPDIFOUT_CHSTS_REG(xinstance) \
	(EE_AUDIO_SPDIFOUT_CHSTS0 + (xinstance))

static int spdif_channel_status_info(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	/* struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	 * int i;
	 */

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffffffff;
	uinfo->count = 1;

	/*
	 * for (i = 0; i < e->items; i++)
	 *     pr_info("Item:%d, %s\n", i, e->texts[i]);
	 */

	return 0;
}

static int spdifin_channel_status_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	/* struct soc_enum *e = (struct soc_enum *)kcontrol->private_value; */
	int reg, status;

	/* pr_info("set which channel status you wanted to get firstly\n"); */
	reg = SPDIFIN_CHSTS_REG;
	status = spdif_get_channel_status(reg);

	ucontrol->value.enumerated.item[0] = status;

	/*channel status value in printk information*/
	/*	pr_info("%s: 0x%x\n",
	 *		e->texts[spdifin_channel_status],
	 *		status
	 *		);
	 */
	return 0;
}

static int spdifin_channel_status_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int chst = ucontrol->value.enumerated.item[0];
	int ch, valid_bits;

	if (chst < 0 || chst > e->items - 1) {
		pr_err("out of value, fixed it\n");

		if (chst < 0)
			chst = 0;
		if (chst > e->items - 1)
			chst = e->items - 1;
	}
	ch = (chst >= 6);
	valid_bits = (chst >= 6) ? (chst - 6) : chst;

	spdifin_channel_status = chst;
	/*	pr_info("%s\n",
	 *		e->texts[spdifin_channel_status]);
	 */

	spdifin_set_channel_status(ch, valid_bits);

	return 0;
}

static int spdifout_channel_status_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	/* struct soc_enum *e = (struct soc_enum *)kcontrol->private_value; */
	int reg, status;

	/* pr_info("set which channel status you wanted to get firstly\n"); */
	reg = SPDIFOUT_CHSTS_REG(spdifout_channel_status);
	status = spdif_get_channel_status(reg);

	ucontrol->value.enumerated.item[0] = status;

	/*channel status value in printk information*/
	/*	pr_info("%s: reg:0x%x, status:0x%x\n",
	 *		e->texts[spdifout_channel_status],
	 *		reg,
	 *		status
	 *		);
	 */
	return 0;
}

static int spdifout_channel_status_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int chst = ucontrol->value.enumerated.item[0];

	if (chst < 0 || chst > e->items - 1) {
		pr_err("out of value, fixed it\n");

		if (chst < 0)
			chst = 0;
		if (chst > e->items - 1)
			chst = e->items - 1;
	}

	spdifout_channel_status = chst;
	/*	pr_info("%s\n",
	 *		e->texts[chst]);
	 */
	return 0;
}

#define SPDIFIN_CHSTATUS(xname, xenum)   \
{                                        \
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,   \
	.name  = xname,                      \
	.info  = spdif_channel_status_info,  \
	.get   = spdifin_channel_status_get, \
	.put   = spdifin_channel_status_set,   \
	.private_value = (unsigned long)&(xenum) \
}

#define SPDIFOUT_CHSTATUS(xname, xenum)   \
{                                         \
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,    \
	.name  = xname,                       \
	.info  = spdif_channel_status_info,   \
	.get   = spdifout_channel_status_get, \
	.put   = spdifout_channel_status_set, \
	.private_value = (unsigned long)&(xenum) \
}

static const char *const audio_locker_texts[] = {
	"Disable",
	"Enable",
};

static const struct soc_enum audio_locker_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_locker_texts),
			audio_locker_texts);

static int audio_locker_get_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = audio_locker_get();
	return 0;
}

static int audio_locker_set_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.enumerated.item[0];

	audio_locker_set(enable);
	return 0;
}

static const char *const audio_inskew_texts[] = {
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
};

static const struct soc_enum audio_inskew_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_inskew_texts),
			audio_inskew_texts);

static int audio_inskew_get_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = get_aml_audio_inskew(card);

	return 0;
}

static int audio_inskew_set_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	unsigned int reg_in, off_set;
	int inskew;
	int id;

	id = (ucontrol->value.enumerated.item[0] >> 16) & 0xffff;
	if (id > 2) {
		pr_warn("%s(), invalid id = %d\n", __func__, id);
		return 0;
	}

	inskew = (int)(ucontrol->value.enumerated.item[0] & 0xffff);
	if (inskew > 7) {
		pr_warn("%s(), invalid inskew = %d\n", __func__, inskew);
		return 0;
	}

	set_aml_audio_inskew(card, inskew);
	set_aml_audio_inskew_index(card, id);
	off_set = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
	reg_in = EE_AUDIO_TDMIN_A_CTRL + off_set * id;
	pr_info("id=%d set inskew=%d\n", id, inskew);
	audiobus_update_bits(reg_in, 0x7 << 16, inskew << 16);

	return 0;
}

static const char *const tdmout_c_binv_texts[] = {
	"0",
	"1",
};

static const struct soc_enum tdmout_c_binv_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(tdmout_c_binv_texts),
			tdmout_c_binv_texts);

static int tdmout_c_binv_get_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int val;
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	val = get_aml_audio_binv(card);
	if (val < 0) {
		val = audiobus_read(EE_AUDIO_CLK_TDMOUT_C_CTRL);
		ucontrol->value.enumerated.item[0] = ((val >> 29) & 0x1);
	} else {
		ucontrol->value.enumerated.item[0] = val;
	}

	return 0;
}

static int tdmout_c_binv_set_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int binv;
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	binv = ucontrol->value.enumerated.item[0];
	set_aml_audio_binv(card, binv);
	set_aml_audio_binv_index(card, TDM_C);
	audiobus_update_bits(EE_AUDIO_CLK_TDMOUT_C_CTRL, 0x1 << 29, binv << 29);

	return 0;
}

static int clk_get_force_mpll(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = force_mpll_clk;

	return 0;
}

static int clk_set_force_mpll(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* only valid for S4 */
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_S4)
		force_mpll_clk = ucontrol->value.integer.value[0];
	pr_info("%s: force_mpll_clk = %d\n", __func__, force_mpll_clk);

	return 0;
}

#define SND_MIX(xname, type, xenum, xshift, xmask)   \
	SND_ENUM(xname, type, CTRL0, xenum, xshift, xmask)

#define SND_SWAP(xname, type, xenum, xshift, xmask)   \
	SND_ENUM(xname, type, SWAP0, xenum, xshift, xmask)

#define SND_SPDIFOUT_SWAP(xname, type, xenum, xshift, xmask)   \
	SND_ENUM(xname, type, SWAP, xenum, xshift, xmask)

#define TDM_GAIN(xname, type, func, xshift, xmask)   \
	SND_BYTE(xname, type, func, xshift, xmask)

static const struct snd_kcontrol_new snd_auge_controls[] = {
	/* SPDIFIN Channel Status */
	SPDIFIN_CHSTATUS("SPDIFIN Channel Status",
			 spdif_channel_status_enum),

	/*SPDIFOUT swap*/
	SND_SPDIFOUT_SWAP("SPDIFOUT Lane0 Left Channel Swap",
			  SPDIFOUT, out_swap_channel_enum, 0, 0x7),
	SND_SPDIFOUT_SWAP("SPDIFOUT Lane0 Right Channel Swap",
			  SPDIFOUT, out_swap_channel_enum, 4, 0x7),
	/*SPDIFOUT mixer*/
	SND_MIX("SPDIFOUT Mixer Channel",
		SPDIFOUT, lane0_mixer_enum, 23, 0x1),
	/* SPDIFIN Channel Status */
	SPDIFOUT_CHSTATUS("SPDIFOUT Channel Status",
			  spdif_channel_status_enum),

	/* audio locker */
	SOC_ENUM_EXT("audio locker enable",
		     audio_locker_enum,
		     audio_locker_get_enum,
		     audio_locker_set_enum),

	/* audio inskew */
	SOC_ENUM_EXT("audio inskew set",
		     audio_inskew_enum,
		     audio_inskew_get_enum,
		     audio_inskew_set_enum),
	/* tdmc out binv */
	SOC_ENUM_EXT("tdmout_c binv set",
		     tdmout_c_binv_enum,
		     tdmout_c_binv_get_enum,
		     tdmout_c_binv_set_enum),
	SOC_SINGLE_BOOL_EXT("DTV clk force MPLL",
			0,
			clk_get_force_mpll,
			clk_set_force_mpll),
};

int snd_card_add_kcontrols(struct snd_soc_card *card)
{
	int ret;

	pr_info("%s card:%p\n", __func__, card);

	ret = card_add_resample_kcontrols(card);
	if (ret < 0) {
		pr_err("Failed to add resample controls\n");
		return ret;
	}

	ret = card_add_ddr_kcontrols(card);
	if (ret < 0) {
		pr_err("Failed to add ddr controls\n");
		return ret;
	}

	ret = card_add_effect_v2_kcontrols(card);
	if (ret < 0) {
		pr_err("Failed to add AED v2 controls\n");
		return ret;
	}

	ret = card_add_acc_kcontrols(card);
	if (ret < 0)
		pr_warn_once("Failed to add acc controls\n");

	ret = card_add_locker_kcontrols(card);
	if (ret < 0)
		pr_warn_once("Failed to add soft locker controls\n");

	ret = card_add_vad_kcontrols(card);
	if (ret < 0)
		pr_warn_once("Failed to add VAD controls\n");

	return snd_soc_add_card_controls(card,
					 snd_auge_controls,
					 ARRAY_SIZE(snd_auge_controls));
}

void auge_toacodec_ctrl(int tdmout_id)
{
	// TODO: check skew for g12a
	audiobus_write(EE_AUDIO_TOACODEC_CTRL0,
		       1 << 31 |
		       ((tdmout_id << 2)) << 12 | /* data 0*/
		       tdmout_id << 8 | /* lrclk */
		       1 << 7 |         /* Bclk_cap_inv*/
		       tdmout_id << 4 | /* bclk */
		       tdmout_id << 0 /* mclk */
		);
}
EXPORT_SYMBOL(auge_toacodec_ctrl);

void auge_toacodec_ctrl_ext(int tdmout_id, int ch0_sel, int ch1_sel,
	bool separate_toacodec_en, int data_sel_shift)
{
	// TODO: check skew for tl1/sm1
	audiobus_write(EE_AUDIO_TOACODEC_CTRL0,
		tdmout_id << 12          /* lrclk */
		| 1 << 9                   /* Bclk_cap_inv*/
		| tdmout_id << 4           /* bclk */
		| tdmout_id << 0           /* mclk */
		);

	if (data_sel_shift == DATA_SEL_SHIFT_VERSION0)
		audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0,
			0xf << 20 | 0xf << 16,
			((tdmout_id << 2) + ch1_sel) << 20 | ((tdmout_id << 2) + ch0_sel) << 16);
	else
		audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0,
			0x1f << 22 | 0x1f << 16,
			((tdmout_id << 3) + ch1_sel) << 22 | ((tdmout_id << 3) + ch0_sel) << 16);

	/* if toacodec_en is separated, need do:
	 * step1: enable/disable mclk
	 * step2: enable/disable bclk
	 * step3: enable/disable dat
	 */
	if (separate_toacodec_en) {
		audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0,
				     0x1 << 29,
				     0x1 << 29);
		audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0,
				     0x1 << 30,
				     0x1 << 30);
	}
	audiobus_update_bits(EE_AUDIO_TOACODEC_CTRL0, 0x1 << 31, 0x1 << 31);

}
EXPORT_SYMBOL(auge_toacodec_ctrl_ext);

void fratv_enable(bool enable)
{
	/* Need reset firstly ? */
	if (enable) {
		audiobus_update_bits(EE_AUDIO_FRATV_CTRL0,
				     0x1 << 29, 0x1 << 29);
		audiobus_update_bits(EE_AUDIO_FRATV_CTRL0,
				     0x1 << 28, 0x1 << 28);
	} else {
		audiobus_update_bits(EE_AUDIO_FRATV_CTRL0,
				     0x3 << 28, 0x0 << 28);
	}

	audiobus_update_bits(EE_AUDIO_FRATV_CTRL0, 0x1 << 31, enable << 31);
}

/* source select
 * 0: select from ATV;
 * 1: select from ADEC;
 */
void fratv_src_select(bool src)
{
	audiobus_update_bits(EE_AUDIO_FRATV_CTRL0, 0x1 << 20, src << 20);
}
EXPORT_SYMBOL_GPL(fratv_src_select);

void fratv_LR_swap(bool swap)
{
	audiobus_update_bits(EE_AUDIO_FRATV_CTRL0, 0x1 << 19, swap << 19);
}
EXPORT_SYMBOL_GPL(fratv_LR_swap);

void cec_arc_enable(int src, bool enable)
{
	/* bits[1:0], 0x2: common; 0x1: single; 0x0: disabled */
	aml_hiubus_update_bits(HHI_HDMIRX_ARC_CNTL, 0x1f << 0,
			       src << 2 | (enable ? 0x1 : 0) << 0);
}

bool is_force_mpll_clk(void)
{
	return force_mpll_clk;
}
