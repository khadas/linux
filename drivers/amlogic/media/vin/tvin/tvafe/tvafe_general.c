// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/delay.h>
/*#include <mach/am_regs.h>*/

/*#include <mach/am_regs.h>*/

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vout/vclk_serve.h>
#include <linux/amlogic/media/vout/vdac_dev.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "tvafe.h"
#include "tvafe_regs.h"
#include "tvafe_cvd.h"
#include "tvafe_debug.h"
#include "tvafe_general.h"
/***************************Local defines**************************/
/* calibration validation function enable/disable */
/* #define TVAFE_ADC_CAL_VALIDATION */

/* edid config reg value */
#define TVAFE_EDID_CONFIG	0x03804050/* 0x03800050 */
#define HHI_ATV_DMD_SYS_CLK_CNTL	0xf3
#define VAFE_CLK_EN				23
#define VAFE_CLK_EN_WIDTH		1
#define VAFE_CLK_SELECT			24
#define VAFE_CLK_SELECT_WIDTH	2

/* TOP */
static const unsigned int cvbs_top_reg_default[][2] = {
	{TVFE_DVSS_MUXCTRL, 0x07000008,},
	{TVFE_DVSS_MUXVS_REF, 0x00000000,},
	{TVFE_DVSS_MUXCOAST_V, 0x00000000,},
	{TVFE_DVSS_SEP_HVWIDTH, 0x00000000,},
	{TVFE_DVSS_SEP_HPARA, 0x00000000,},
	{TVFE_DVSS_SEP_VINTEG, 0x00000000,},
	{TVFE_DVSS_SEP_H_THR, 0x00000000,},
	{TVFE_DVSS_SEP_CTRL, 0x00000000,},
	{TVFE_DVSS_GEN_WIDTH, 0x00000000,},
	{TVFE_DVSS_GEN_PRD, 0x00000000,},
	{TVFE_DVSS_GEN_COAST, 0x00000000,},
	{TVFE_DVSS_NOSIG_PARA, 0x00000000,},
	{TVFE_DVSS_NOSIG_PLS_TH, 0x00000000,},
	/* TVFE_DVSS_NOSIG_PLS_TH */
	{TVFE_DVSS_GATE_H, 0x00000000,},
	{TVFE_DVSS_GATE_V, 0x00000000,},
	{TVFE_DVSS_INDICATOR1, 0x00000000,},
	{TVFE_DVSS_INDICATOR2, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL1, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL2, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL3, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL4, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL5, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL6, 0x00000000,},
	{TVFE_DVSS_MVDET_CTRL7, 0x00000000,},
	{TVFE_SYNCTOP_SPOL_MUXCTRL, 0x00000009,},
	/* TVFE_SYNCTOP_SPOL_MUXCTRL */
	{TVFE_SYNCTOP_INDICATOR1_HCNT, 0x00000000,},
	/* TVFE_SYNCTOP_INDICATOR1_HCNT */
	{TVFE_SYNCTOP_INDICATOR2_VCNT, 0x00000000,},
	/* TVFE_SYNCTOP_INDICATOR2_VCNT */
	{TVFE_SYNCTOP_INDICATOR3, 0x00000000,},
	/* TVFE_SYNCTOP_INDICATOR3 */
	{TVFE_SYNCTOP_SFG_MUXCTRL1, 0x00000000,},
	/* TVFE_SYNCTOP_SFG_MUXCTRL1 */
	{TVFE_SYNCTOP_SFG_MUXCTRL2, 0x00330000,},
	/* TVFE_SYNCTOP_SFG_MUXCTRL2 */
	{TVFE_SYNCTOP_INDICATOR4, 0x00000000,},
	/* TVFE_SYNCTOP_INDICATOR4 */
	{TVFE_SYNCTOP_SAM_MUXCTRL, 0x00082001,},
	/* TVFE_SYNCTOP_SAM_MUXCTRL */
	{TVFE_MISC_WSS1_MUXCTRL1, 0x00000000,},
	/* TVFE_MISC_WSS1_MUXCTRL1 */
	{TVFE_MISC_WSS1_MUXCTRL2, 0x00000000,},
	/* TVFE_MISC_WSS1_MUXCTRL2 */
	{TVFE_MISC_WSS2_MUXCTRL1, 0x00000000,},
	/* TVFE_MISC_WSS2_MUXCTRL1 */
	{TVFE_MISC_WSS2_MUXCTRL2, 0x00000000,},
	/* TVFE_MISC_WSS2_MUXCTRL2 */
	{TVFE_MISC_WSS1_INDICATOR1, 0x00000000,},
	/* TVFE_MISC_WSS1_INDICATOR1 */
	{TVFE_MISC_WSS1_INDICATOR2, 0x00000000,},
	/* TVFE_MISC_WSS1_INDICATOR2 */
	{TVFE_MISC_WSS1_INDICATOR3, 0x00000000,},
	/* TVFE_MISC_WSS1_INDICATOR3 */
	{TVFE_MISC_WSS1_INDICATOR4, 0x00000000,},
	/* TVFE_MISC_WSS1_INDICATOR4 */
	{TVFE_MISC_WSS1_INDICATOR5, 0x00000000,},
	/* TVFE_MISC_WSS1_INDICATOR5 */
	{TVFE_MISC_WSS2_INDICATOR1, 0x00000000,},
	/* TVFE_MISC_WSS2_INDICATOR1 */
	{TVFE_MISC_WSS2_INDICATOR2, 0x00000000,},
	/* TVFE_MISC_WSS2_INDICATOR2 */
	{TVFE_MISC_WSS2_INDICATOR3, 0x00000000,},
	/* TVFE_MISC_WSS2_INDICATOR3 */
	{TVFE_MISC_WSS2_INDICATOR4, 0x00000000,},
	/* TVFE_MISC_WSS2_INDICATOR4 */
	{TVFE_MISC_WSS2_INDICATOR5, 0x00000000,},
	/* TVFE_MISC_WSS2_INDICATOR5 */
	{TVFE_AP_MUXCTRL1, 0x00000000,},
	{TVFE_AP_MUXCTRL2, 0x00000000,},
	{TVFE_AP_MUXCTRL3, 0x00000000,},
	{TVFE_AP_MUXCTRL4, 0x00000000,},
	{TVFE_AP_MUXCTRL5, 0x00000000,},
	{TVFE_AP_INDICATOR1, 0x00000000,},
	{TVFE_AP_INDICATOR2, 0x00000000,},
	{TVFE_AP_INDICATOR3, 0x00000000,},
	{TVFE_AP_INDICATOR4, 0x00000000,},
	{TVFE_AP_INDICATOR5, 0x00000000,},
	{TVFE_AP_INDICATOR6, 0x00000000,},
	{TVFE_AP_INDICATOR7, 0x00000000,},
	{TVFE_AP_INDICATOR8, 0x00000000,},
	{TVFE_AP_INDICATOR9, 0x00000000,},
	{TVFE_AP_INDICATOR10, 0x00000000,},
	{TVFE_AP_INDICATOR11, 0x00000000,},
	{TVFE_AP_INDICATOR12, 0x00000000,},
	{TVFE_AP_INDICATOR13, 0x00000000,},
	{TVFE_AP_INDICATOR14, 0x00000000,},
	{TVFE_AP_INDICATOR15, 0x00000000,},
	{TVFE_AP_INDICATOR16, 0x00000000,},
	{TVFE_AP_INDICATOR17, 0x00000000,},
	{TVFE_AP_INDICATOR18, 0x00000000,},
	{TVFE_AP_INDICATOR19, 0x00000000,},
	{TVFE_BD_MUXCTRL1, 0x00000000,},
	{TVFE_BD_MUXCTRL2, 0x00000000,},
	{TVFE_BD_MUXCTRL3, 0x00000000,},
	{TVFE_BD_MUXCTRL4, 0x00000000,},
	{TVFE_CLP_MUXCTRL1, 0x00000000,},
	{TVFE_CLP_MUXCTRL2, 0x00000000,},
	{TVFE_CLP_MUXCTRL3, 0x00000000,},
	{TVFE_CLP_MUXCTRL4, 0x00000000,},
	{TVFE_CLP_INDICATOR1, 0x00000000,},
	{TVFE_BPG_BACKP_H, 0x00000000,},
	{TVFE_BPG_BACKP_V, 0x00000000,},
	{TVFE_DEG_H, 0x00000000,},
	{TVFE_DEG_VODD, 0x00000000,},
	{TVFE_DEG_VEVEN, 0x00000000,},
	{TVFE_OGO_OFFSET1, 0x00000000,},
	{TVFE_OGO_GAIN1, 0x00000000,},
	{TVFE_OGO_GAIN2, 0x00000000,},
	{TVFE_OGO_OFFSET2, 0x00000000,},
	{TVFE_OGO_OFFSET3, 0x00000000,},
	{TVFE_VAFE_CTRL, 0x00000000,},
	{TVFE_VAFE_STATUS, 0x00000000,},
#ifdef CRYSTAL_25M
	{TVFE_TOP_CTRL, 0x30c4e6c
	/*0xc4f64 0x00004B60*/,},
	/* TVFE_TOP_CTRL */
#else/* 24M */
	{TVFE_TOP_CTRL, 0x30c4f64
	/*0xc4f64 0x00004B60*/,},
	/* TVFE_TOP_CTRL */
#endif
	/*enable in tvafe_dec_open or avdetect avplug in*/
	{TVFE_CLAMP_INTF, 0x00000666,},
	{TVFE_RST_CTRL, 0x00000000,},
	{TVFE_EXT_VIDEO_AFE_CTRL_MUX1, 0x00000000,},
	/* TVFE_EXT_VIDEO_AFE_CTRL_MUX1 */
	{TVFE_EDID_CONFIG, TVAFE_EDID_CONFIG,},
	/* TVFE_EDID_CONFIG */
	{TVFE_EDID_RAM_ADDR, 0x00000000,},
	{TVFE_EDID_RAM_WDATA, 0x00000000,},
	{TVFE_EDID_RAM_RDATA, 0x00000000,},
	{TVFE_APB_ERR_CTRL_MUX1, 0x8fff8fff,},
	/* TVFE_APB_ERR_CTRL_MUX1 */
	{TVFE_APB_ERR_CTRL_MUX2, 0x00008fff,},
	/* TVFE_APB_ERR_CTRL_MUX2 */
	{TVFE_APB_INDICATOR1, 0x00000000,},
	{TVFE_APB_INDICATOR2, 0x00000000,},
	{TVFE_ADC_READBACK_CTRL, 0x80140003,},
	/* TVFE_ADC_READBACK_CTRL */
	{TVFE_ADC_READBACK_INDICATOR, 0x00000000,},
	/* TVFE_ADC_READBACK_INDICATOR */
	{TVFE_INT_CLR, 0x00000000,},
	{TVFE_INT_MSKN, 0x00000000,},
	{TVFE_INT_INDICATOR1, 0x00000000,},
	{TVFE_INT_SET, 0x00000000,},
	/* {TVFE_CHIP_VERSION                      ,0x00000000,}, */
	/* TVFE_CHIP_VERSION */
	{TVFE_FREERUN_GEN_WIDTH, 0x00000000,},/* TVFE_FREERUN_GEN_WIDTH */
	{TVFE_FREERUN_GEN_PRD,  0x00000000,},/* TVFE_FREERUN_GEN_PRD */
	{TVFE_FREERUN_GEN_COAST, 0x00000000,},/* TVFE_FREERUN_GEN_COAST */
	{TVFE_FREERUN_GEN_CTRL, 0x00000000,},/* TVFE_FREERUN_GEN_CTRL */

	{TVFE_AAFILTER_CTRL1,   0x00100000,},
	/* TVFE_AAFILTER_CTRL1 bypass all */
	{TVFE_AAFILTER_CTRL2,   0x00000000,},
	/* TVFE_AAFILTER_CTRL2 */
	{TVFE_AAFILTER_CTRL3,   0x00000000,},
	/* TVFE_AAFILTER_CTRL3 */
	{TVFE_AAFILTER_CTRL4,   0x00000000,},
	/* TVFE_AAFILTER_CTRL4 */
	{TVFE_AAFILTER_CTRL5,   0x00000000,},
	/* TVFE_AAFILTER_CTRL5 */

	{TVFE_SOG_MON_CTRL1,   0x00000000,},/* TVFE_SOG_MON_CTRL1 */
	{TVFE_ADC_READBACK_CTRL1,   0x00000000,},/* TVFE_ADC_READBACK_CTRL1 */
	{TVFE_ADC_READBACK_CTRL2,   0x00000000,},/* TVFE_ADC_READBACK_CTRL2 */
#ifdef CRYSTAL_25M
	{TVFE_AFC_CTRL1,	 0x85730459,},/* TVFE_AFC_CTRL1 */
	{TVFE_AFC_CTRL2,	 0x342fa9ed,},/* TVFE_AFC_CTRL2 */
	{TVFE_AFC_CTRL3,	 0x2a02396,},/* TVFE_AFC_CTRL3 */
	{TVFE_AFC_CTRL4,	 0xfefbff14,},/* TVFE_AFC_CTRL4 */
	{TVFE_AFC_CTRL5,		0x0,},/* TVFE_AFC_CTRL5 */
#else/* for 24M */
	{TVFE_AFC_CTRL1,   0x893904d2,},
	/* TVFE_AFC_CTRL1 */
	{TVFE_AFC_CTRL2,   0xf4b9ac9,},
	/* TVFE_AFC_CTRL2 */
	{TVFE_AFC_CTRL3,   0x1fd8c36,},
	/* TVFE_AFC_CTRL3 */
	{TVFE_AFC_CTRL4,   0x2de6d04f,},
	/* TVFE_AFC_CTRL4 */
	{TVFE_AFC_CTRL5,          0x4,},
	/* TVFE_AFC_CTRL5 */
#endif
	{0xFFFFFFFF, 0x00000000,}
};

static const unsigned int tvafe_pq_reg_trust_table[][2] = {
	/* reg    mask */
	{CVD2_CONTROL1,                     0xff}, /* 0x01 */
	{CVD2_OUTPUT_CONTROL,               0x0f}, /* 0x07 */
	{CVD2_LUMA_CONTRAST_ADJUSTMENT,     0xff}, /* 0x08 */
	{CVD2_LUMA_BRIGHTNESS_ADJUSTMENT,   0xff}, /* 0x09 */
	{CVD2_CHROMA_SATURATION_ADJUSTMENT, 0xff}, /* 0x0a */
	{CVD2_CHROMA_HUE_PHASE_ADJUSTMENT,  0xff}, /* 0x0b */
	{CVD2_REG_87,                       0xc0}, /* 0x87 */
	{CVD2_CHROMA_EDGE_ENHANCEMENT,      0xff}, /* 0xb5 */
	{CVD2_CHROMA_BW_MOTION,             0xff}, /* 0xe8 */
	{CVD2_REG_FA,                       0x80}, /* 0xfa */

	{ACD_REG_1B,                        0x0f000000},
	{ACD_REG_25,                        0xffffffff},
	{ACD_REG_53,                        0xffffffff},
	{ACD_REG_54,                        0xffffffff},
	{ACD_REG_55,                        0xc0fff3ff},
	{ACD_REG_56,                        0x00f00000},
	{ACD_REG_57,                        0x03ff81ff},
	{ACD_REG_58,                        0x8fffffff},
	{ACD_REG_59,                        0xffffffff},
	{ACD_REG_5B,                        0x3ff7ffff},
	{ACD_REG_64,                        0xffffffff},
	{ACD_REG_65,                        0xffffffff},
	{ACD_REG_66,                        0x80000ff0},
	{ACD_REG_6F,                        0xffffffff},
	{ACD_REG_74,                        0xffffffff},
	{ACD_REG_75,                        0x000000ff},
	{ACD_REG_86,                        0xc0000000},
	{ACD_REG_89,                        0x803ff3ff},
	{ACD_REG_8A,                        0x03ff1fff},
	{ACD_REG_8B,                        0x0fffffff},
	{ACD_REG_8C,                        0x0fffffff},
	{ACD_REG_94,                        0xffffffff},
	{ACD_REG_95,                        0xffffffff},
	{ACD_REG_96,                        0xffffffff},
	{ACD_REG_AE,                        0x00000001},
	{ACD_REG_AF,                        0x1f1f007f},

	{0xffffffff,                        0x00000000}, /* ending */
};

static void tvafe_pq_apb_reg_trust_write(unsigned int addr,
		unsigned int mask, unsigned int val)
{
	unsigned int reg, i = 0, size;

	reg = (addr << 2);
	size = sizeof(tvafe_pq_reg_trust_table) / (sizeof(unsigned int) * 2);
	/* check reg trust */
	while (i < size) {
		if (tvafe_pq_reg_trust_table[i][0] == 0xFFFFFFFF) {
			tvafe_pr_info("%s: error: reg 0x%x is out of trust reg!\n",
				__func__, addr);
			return;
		}
		if (reg == tvafe_pq_reg_trust_table[i][0])
			break;
		i++;
	}
	if (i >= size)
		return;

	/* check mask trust */
	if ((mask & tvafe_pq_reg_trust_table[i][1]) != mask) {
		tvafe_pr_info("%s: warning: reg 0x%x mask 0x%x is out of trust mask 0x%x, change to 0x%x!\n",
				__func__, addr, mask,
				tvafe_pq_reg_trust_table[i][1],
				(mask & tvafe_pq_reg_trust_table[i][1]));
		mask &= tvafe_pq_reg_trust_table[i][1];
	}

	if (mask == 0xffffffff)
		W_APB_REG(reg, val);
	else
		W_APB_REG(reg, (R_APB_REG(reg) & (~(mask))) | (val & mask));

	if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
		tvafe_pr_info("%s: apb: Reg0x%x(%u)=0x%x(%u) val=%x(%u) mask=%x(%u)\n",
			__func__, addr, addr,
			(val & mask), (val & mask),
			val, val, mask, mask);

	cvd_reg87_pal = R_APB_REG(CVD2_REG_87);
	acd_166 = R_APB_REG(ACD_REG_66);
}

enum tvafe_adc_ch_e tvafe_port_to_channel(enum tvin_port_e port,
					  struct tvafe_pin_mux_s *pinmux)
{
	enum tvafe_adc_ch_e adc_ch = TVAFE_ADC_CH_NULL;

	if (!IS_TVAFE_SRC(port)) {
		tvafe_pr_info("%s set pin mux error!!!!!.\n", __func__);
		return TVAFE_ADC_CH_NULL;
	}

	adc_ch = (enum tvafe_adc_ch_e)pinmux->pin[port - TVIN_PORT_CVBS0];

	return adc_ch;
}

/*
 * tvafe pin mux setting for input source, cvd2 video poaition reg setting
 */
int tvafe_adc_pin_muxing(enum tvafe_adc_ch_e adc_ch)
{
	if (adc_ch == TVAFE_ADC_CH_NULL) {
		tvafe_pr_info("%s set pin mux error!!!!!.\n", __func__);
		return -1;
	}

	if (adc_ch == TVAFE_ADC_CH_0) {
		W_APB_BIT(TVFE_VAFE_CTRL1, 1,
			  VAFE_IN_SEL_BIT, VAFE_IN_SEL_WID);
		if (tvafe_cpu_type() < TVAFE_CPU_TYPE_TL1)
			W_APB_BIT(TVFE_VAFE_CTRL2, 3, 4, 3);

	} else if (adc_ch == TVAFE_ADC_CH_1) {
		W_APB_BIT(TVFE_VAFE_CTRL1, 2,
			  VAFE_IN_SEL_BIT, VAFE_IN_SEL_WID);
		if (tvafe_cpu_type() < TVAFE_CPU_TYPE_TL1)
			W_APB_BIT(TVFE_VAFE_CTRL2, 5, 4, 3);

	} else if (adc_ch == TVAFE_ADC_CH_2) {
		W_APB_BIT(TVFE_VAFE_CTRL1, 3,
			  VAFE_IN_SEL_BIT, VAFE_IN_SEL_WID);
		W_APB_BIT(TVFE_VAFE_CTRL2, 6, 4, 3);
	} else if (adc_ch == TVAFE_ADC_CH_ATV) {
		/* atv demod data for cvd2 */
		W_APB_REG(TVFE_ATV_DMD_CLP_CTRL, 0x1300010);
		W_APB_BIT(TVFE_VAFE_CTRL2, 6, 4, 3);
	} else {
		tvafe_pr_info("%s set pin mux error!!!!!.\n", __func__);
		return -1;
	}

	tvafe_pr_info("%s set pin mux to adc_ch: %d ok.\n",
		      __func__, (unsigned int)adc_ch);
	return 0;
}

void tvafe_set_regmap(struct am_regs_s *p)
{
	unsigned short i;

for (i = 0; i < p->length; i++) {
	switch (p->am_reg[i].type) {
	case REG_TYPE_PHY:
		if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
			tvafe_pr_info("%s: bus type: phy..\n", __func__);
		break;
	case REG_TYPE_CBUS:
		if (p->am_reg[i].mask == 0xffffffff)
			aml_write_cbus(p->am_reg[i].addr, p->am_reg[i].val);
		else
			aml_write_cbus(p->am_reg[i].addr,
			(aml_read_cbus(p->am_reg[i].addr) &
			(~(p->am_reg[i].mask))) |
			(p->am_reg[i].val & p->am_reg[i].mask));
		if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
			tvafe_pr_info("%s: cbus: Reg0x%x(%u)=0x%x(%u)val=%x(%u)mask=%x(%u)\n",
				__func__, p->am_reg[i].addr, p->am_reg[i].addr,
				(p->am_reg[i].val & p->am_reg[i].mask),
				(p->am_reg[i].val & p->am_reg[i].mask),
				p->am_reg[i].val, p->am_reg[i].val,
				p->am_reg[i].mask, p->am_reg[i].mask);
		break;
	case REG_TYPE_APB:
		tvafe_pq_apb_reg_trust_write(p->am_reg[i].addr,
			p->am_reg[i].mask, p->am_reg[i].val);
		break;
	default:
		if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
			tvafe_pr_info("%s: bus type error!!!bustype = 0x%x................\n",
				__func__, p->am_reg[i].type);
		break;
		}
	}
}

/*
 * tvafe init cvbs setting with pal-i
 */
static void tvafe_set_cvbs_default(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem, enum tvin_port_e port)
{
	unsigned int i = 0;

	/**disable auto mode clock**/
	if (tvafe_cpu_type() < TVAFE_CPU_TYPE_TL1)
		W_HIU_REG(HHI_TVFE_AUTOMODE_CLK_CNTL, 0);

	/** enable tv_decoder mem clk **/
	if (tvafe_cpu_type() < TVAFE_CPU_TYPE_TM2_B)
		W_HIU_BIT(HHI_VPU_CLK_CNTL, 1, 28, 1);

	/** write top register **/
	i = 0;
	while (cvbs_top_reg_default[i][0] != 0xFFFFFFFF) {
		W_APB_REG(cvbs_top_reg_default[i][0],
			cvbs_top_reg_default[i][1]);
		i++;
	}
	if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_TL1) {
		if (IS_TVAFE_ATV_SRC(port)) {
			adc_set_filter_ctrl(true, FILTER_ATV_DEMOD, NULL);
		} else if (IS_TVAFE_AVIN_SRC(port)) {
			adc_set_filter_ctrl(true, FILTER_TVAFE, NULL);
		}

#if (defined(CONFIG_ADC_DOUBLE_SAMPLING_FOR_CVBS) && defined(CRYSTAL_24M))
		if (!IS_TVAFE_ATV_SRC(port)) {
			W_APB_REG(TVFE_TOP_CTRL, 0x010c4d6c);
			W_APB_REG(TVFE_AAFILTER_CTRL1, 0x00012721);
			W_APB_REG(TVFE_AAFILTER_CTRL2, 0x1304fcfa);
			W_APB_REG(TVFE_AFC_CTRL1, 0x893904d2);
			W_APB_REG(TVFE_AFC_CTRL2, 0x0f4b9ac9);
			W_APB_REG(TVFE_AFC_CTRL3, 0x01fd8c36);
			W_APB_REG(TVFE_AFC_CTRL4, 0x2de6d04f);
			W_APB_REG(TVFE_AFC_CTRL5, 0x00000004);
		} else {
			W_APB_REG(TVFE_AAFILTER_CTRL1, 0x00182222);
			W_APB_REG(TVFE_AAFILTER_CTRL2, 0x252b39c6);
			W_APB_REG(TVFE_AFC_CTRL1, 0x05730459);
			W_APB_REG(TVFE_AFC_CTRL2, 0xf4b9ac9);
			W_APB_REG(TVFE_AFC_CTRL3, 0x1fd8c36);
			W_APB_REG(TVFE_AFC_CTRL4, 0x2de6d04f);
			W_APB_REG(TVFE_AFC_CTRL5, 0x00000004);
		}
#else
		W_APB_REG(TVFE_AAFILTER_CTRL1, 0x00182222);
		W_APB_REG(TVFE_AAFILTER_CTRL2, 0x252b39c6);
		W_APB_REG(TVFE_AFC_CTRL1, 0x05730459);
		W_APB_REG(TVFE_AFC_CTRL2, 0xf4b9ac9);
		W_APB_REG(TVFE_AFC_CTRL3, 0x1fd8c36);
		W_APB_REG(TVFE_AFC_CTRL4, 0x2de6d04f);
		W_APB_REG(TVFE_AFC_CTRL5, 0x00000004);
#endif
	}
	/* init some variables  */
	cvd2->vd_port = port;

	/* set cvd2 default format to pal-i */
	tvafe_cvd2_try_format(cvd2, mem, TVIN_SIG_FMT_CVBS_PAL_I);
}

void tvafe_enable_avout(enum tvin_port_e port, bool enable)
{
	if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_TL1) {
		if (enable) {
			tvafe_clk_gate_ctrl(1);
			if (IS_TVAFE_ATV_SRC(port)) {
#ifdef CONFIG_AMLOGIC_VDAC
				vdac_enable(1, VDAC_MODULE_AVOUT_ATV);
#endif
			} else {
				W_APB_REG(TVFE_ATV_DMD_CLP_CTRL, 0);
#ifdef CONFIG_AMLOGIC_VDAC
				vdac_enable(1, VDAC_MODULE_AVOUT_AV);
#endif
			}
		} else {
#ifdef CONFIG_AMLOGIC_VDAC
			if (IS_TVAFE_ATV_SRC(port))
				vdac_enable(0, VDAC_MODULE_AVOUT_ATV);
			else
				vdac_enable(0, VDAC_MODULE_AVOUT_AV);
#endif
			tvafe_clk_gate_ctrl(0);
		}
	}
}

/*
 * tvafe init the whole module
 */
void tvafe_init_reg(struct tvafe_cvd2_s *cvd2, struct tvafe_cvd2_mem_s *mem,
		    enum tvin_port_e port)
{
	unsigned int module_sel = ADC_EN_TVAFE;
	struct tvafe_user_param_s *user_param = tvafe_get_user_param();

	if (IS_TVAFE_ATV_SRC(port))
		module_sel = ADC_EN_ATV_DEMOD;
	else if (IS_TVAFE_AVIN_SRC(port))
		module_sel = ADC_EN_TVAFE;

	if (IS_TVAFE_SRC(port)) {
#ifdef CRYSTAL_25M
		if (tvafe_cpu_type() < TVAFE_CPU_TYPE_TL1)
			/* can't write !!! */
			W_HIU_REG(HHI_VAFE_CLKIN_CNTL, 0x703);
#endif

#if (defined(CONFIG_ADC_DOUBLE_SAMPLING_FOR_CVBS) && defined(CRYSTAL_24M))
		if (!IS_TVAFE_ATV_SRC(port)) {
			W_HIU_REG(HHI_ADC_PLL_CNTL3, 0xa92a2110);
			W_HIU_REG(HHI_ADC_PLL_CNTL4, 0x02973800);
			W_HIU_REG(HHI_ADC_PLL_CNTL, 0x08664220);
			W_HIU_REG(HHI_ADC_PLL_CNTL2, 0x34e0bf80);
			W_HIU_REG(HHI_ADC_PLL_CNTL3, 0x292a2110);
		} else {
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
			adc_set_pll_cntl(1, module_sel, NULL);
#endif
		}
#else
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
			adc_set_pll_cntl(1, module_sel, NULL);
#endif
#endif
		tvafe_set_cvbs_default(cvd2, mem, port);
		/*turn on/off av out*/
		tvafe_enable_avout(port, user_param->avout_en);
		/* CDAC_CTRL_RESV2<1>=0 */
	}

	tvafe_pr_info("%s ok.\n", __func__);
}

/*
 * tvafe set APB bus register accessing error exception
 */
void tvafe_set_apb_bus_err_ctrl(void)
{
	W_APB_REG(TVFE_APB_ERR_CTRL_MUX1, 0x8fff8fff);
	W_APB_REG(TVFE_APB_ERR_CTRL_MUX2, 0x00008fff);
}

/*
 * tvafe reset the whole module
 */
static void tvafe_reset_module(void)
{
	pr_info("%s: reset module\n", __func__);
	W_APB_BIT(TVFE_RST_CTRL, 1, ALL_CLK_RST_BIT, ALL_CLK_RST_WID);
	W_APB_BIT(TVFE_RST_CTRL, 0, ALL_CLK_RST_BIT, ALL_CLK_RST_WID);
	/*reset vdin asynchronous fifo*/
	/*for greenscreen on repeatly power on/off*/
	W_APB_BIT(TVFE_RST_CTRL, 1, SAMPLE_OUT_RST_BIT, SAMPLE_OUT_RST_WID);
	W_APB_BIT(TVFE_RST_CTRL, 0, SAMPLE_OUT_RST_BIT, SAMPLE_OUT_RST_WID);
}

void tvafe_reg_setb(void __iomem *reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	writel(((readl(reg) & (~(((1L << _len) - 1) << _start))) |
	       ((value & ((1L << _len) - 1)) << _start)), reg);
}

/*
 * tvafe power control of the module
 */
void tvafe_enable_module(bool enable)
{
	/* enable */

	/* main clk up */
	if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_T5) {
		tvafe_reg_setb(ana_addr, 1, VAFE_CLK_SELECT,
			       VAFE_CLK_SELECT_WIDTH);
		tvafe_reg_setb(ana_addr, 1, VAFE_CLK_EN,
			       VAFE_CLK_EN_WIDTH);
	} else if ((tvafe_cpu_type() >= TVAFE_CPU_TYPE_TL1) &&
		  (tvafe_cpu_type() <= TVAFE_CPU_TYPE_TM2_B)) {
		W_HIU_BIT(HHI_ATV_DMD_SYS_CLK_CNTL, 1,
			  VAFE_CLK_SELECT, VAFE_CLK_SELECT_WIDTH);
		W_HIU_BIT(HHI_ATV_DMD_SYS_CLK_CNTL, 1,
			  VAFE_CLK_EN, VAFE_CLK_EN_WIDTH);
	} else {
		W_HIU_REG(HHI_VAFE_CLKXTALIN_CNTL, 0x100);
		W_HIU_REG(HHI_VAFE_CLKOSCIN_CNTL, 0x100);
		W_HIU_REG(HHI_VAFE_CLKIN_CNTL, 0x100);
		W_HIU_REG(HHI_VAFE_CLKPI_CNTL, 0x100);
		W_HIU_REG(HHI_TVFE_AUTOMODE_CLK_CNTL, 0x100);
	}

	/* T5W add 3d comb clk */
	if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_T5W) {
		vclk_clk_reg_setb(HHI_TVFECLK_CNTL, 1, TVFECLK_GATE,
				  TVFECLK_GATE_WIDTH);
		vclk_clk_reg_setb(HHI_TVFECLK_CNTL, 1, TVFECLK_SEL,
				  TVFECLK_SEL_WIDTH);
	}

	/* tvfe power up */
	W_APB_BIT(TVFE_TOP_CTRL, 1, COMP_CLK_ENABLE_BIT, COMP_CLK_ENABLE_WID);
	W_APB_BIT(TVFE_TOP_CTRL, 1, EDID_CLK_EN_BIT, EDID_CLK_EN_WID);
	W_APB_BIT(TVFE_TOP_CTRL, 1, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
	W_APB_BIT(TVFE_TOP_CTRL, 1, VAFE_MCLK_EN_BIT, VAFE_MCLK_EN_WID);
	W_APB_BIT(TVFE_TOP_CTRL, 3, TVFE_ADC_CLK_DIV_BIT, TVFE_ADC_CLK_DIV_WID);

	/*reset module*/
	tvafe_reset_module();

	/* disable */
	if (!enable) {
		W_APB_BIT(TVFE_VAFE_CTRL0, 0,
			VAFE_FILTER_EN_BIT, VAFE_FILTER_EN_WID);
		W_APB_BIT(TVFE_VAFE_CTRL1, 0,
			VAFE_PGA_EN_BIT, VAFE_PGA_EN_WID);
		/*disable Vref buffer*/
		W_APB_BIT(TVFE_VAFE_CTRL2, 0, 28, 1);
		/*disable afe buffer*/
		W_APB_BIT(TVFE_VAFE_CTRL2, 0, 0, 1);
		W_APB_BIT(TVFE_VAFE_CTRL2, 0, 21, 1);

		/* tvfe power down */
		W_APB_BIT(TVFE_TOP_CTRL, 0, COMP_CLK_ENABLE_BIT,
				COMP_CLK_ENABLE_WID);
		W_APB_BIT(TVFE_TOP_CTRL, 0, EDID_CLK_EN_BIT, EDID_CLK_EN_WID);
		W_APB_BIT(TVFE_TOP_CTRL, 0, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
		W_APB_BIT(TVFE_TOP_CTRL, 0, VAFE_MCLK_EN_BIT, VAFE_MCLK_EN_WID);
		W_APB_BIT(TVFE_TOP_CTRL, 0, TVFE_ADC_CLK_DIV_BIT,
			TVFE_ADC_CLK_DIV_WID);

		/* T5W add 3d comb clk */
		if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_T5W) {
			vclk_clk_reg_setb(HHI_TVFECLK_CNTL, 0, TVFECLK_GATE,
					  TVFECLK_GATE_WIDTH);
			vclk_clk_reg_setb(HHI_TVFECLK_CNTL, 0, TVFECLK_SEL,
					  TVFECLK_SEL_WIDTH);
		}

		/* main clk down */
		if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_T5) {
			tvafe_reg_setb(ana_addr, 0, VAFE_CLK_SELECT,
				       VAFE_CLK_SELECT_WIDTH);
			tvafe_reg_setb(ana_addr, 0, VAFE_CLK_EN,
				       VAFE_CLK_EN_WIDTH);
		} else if ((tvafe_cpu_type() >= TVAFE_CPU_TYPE_TL1) &&
			  (tvafe_cpu_type() <= TVAFE_CPU_TYPE_TM2_B)) {
			W_HIU_BIT(HHI_ATV_DMD_SYS_CLK_CNTL, 0,
				  VAFE_CLK_SELECT, VAFE_CLK_SELECT_WIDTH);
			W_HIU_BIT(HHI_ATV_DMD_SYS_CLK_CNTL, 0,
				  VAFE_CLK_EN, VAFE_CLK_EN_WIDTH);
		} else {
			W_HIU_REG(HHI_VAFE_CLKXTALIN_CNTL, 0);
			W_HIU_REG(HHI_VAFE_CLKOSCIN_CNTL, 0);
			W_HIU_REG(HHI_VAFE_CLKIN_CNTL, 0);
			W_HIU_REG(HHI_VAFE_CLKPI_CNTL, 0);
			W_HIU_REG(HHI_TVFE_AUTOMODE_CLK_CNTL, 0);
		}
	}
}

/*
 * tvafe power control of the module
 */
static void tvafe_top_enable_module(bool enable)
{
	/* disable */
	if (!enable) {
		W_APB_BIT(TVFE_VAFE_CTRL1, 0,
			  VAFE_PGA_EN_BIT, VAFE_PGA_EN_WID);
	}
	W_APB_BIT(TVFE_TOP_CTRL, 0, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
	tvafe_pr_info("reset module\n");
	W_APB_BIT(TVFE_RST_CTRL, 1, DCLK_RST_BIT, DCLK_RST_WID);
	W_APB_BIT(TVFE_RST_CTRL, 0, DCLK_RST_BIT, DCLK_RST_WID);
}

static void tvafe_top_init_reg(enum tvin_port_e port)
{
	W_APB_BIT(TVFE_TOP_CTRL, 1, DCLK_ENABLE_BIT, DCLK_ENABLE_WID);
	if (IS_TVAFE_AVIN_SRC(port)) {
		if (port == TVIN_PORT_CVBS1)
			W_APB_REG(TVFE_VAFE_CTRL1, 0x0000110e);
		else
			W_APB_REG(TVFE_VAFE_CTRL1, 0x0000210e);
	}
	tvafe_pr_info("%s ok.\n", __func__);
}

void white_pattern_pga_reset(enum tvin_port_e port)
{
	tvafe_top_enable_module(false);
	tvafe_top_init_reg(port);
	W_APB_BIT(TVFE_CLAMP_INTF, 1, CLAMP_EN_BIT, CLAMP_EN_WID);
}

