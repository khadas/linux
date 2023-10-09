/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_CODEC_PA1_H
#define _AML_CODEC_PA1_H

#include <linux/regmap.h>

#define TOP_CTRL_BASE				(0x00)
#define PA1_RESET_CTRL				(TOP_CTRL_BASE + 0x1)
#define PA1_DEVICE_CTRL2			(TOP_CTRL_BASE + 0x3)
#define PA1_EFUSE_CTRL				(TOP_CTRL_BASE + 0x6)
#define PA1_ANA_TRIM_4				(TOP_CTRL_BASE + 0xa)
#define PA1_ANA_FORCE2				(TOP_CTRL_BASE + 0x12)
#define PA1_ANA_FORCE6				(TOP_CTRL_BASE + 0x16)
#define PA1_ANA_FORCE7				(TOP_CTRL_BASE + 0x17)
#define PA1_PINMUX_IE				(TOP_CTRL_BASE + 0x18)
#define PA1_PINMUX_OE				(TOP_CTRL_BASE + 0x19)
#define PA1_PINMUX_SEL				(TOP_CTRL_BASE + 0x1c)
#define PA1_PLL_CTRL0				(TOP_CTRL_BASE + 0x2a)
#define PA1_PLL_CTRL1				(TOP_CTRL_BASE + 0x2b)
#define PA1_TOP_TEST1				(TOP_CTRL_BASE + 0x2f)
#define PA1_SAP_CTRL3				(TOP_CTRL_BASE + 0x35)
#define PA1_AUTOCONFIG_CTRL			(TOP_CTRL_BASE + 0x3a)
#define PA1_ANA_CTRL0				(TOP_CTRL_BASE + 0x51)
#define PA1_ANA_CTRL1				(TOP_CTRL_BASE + 0x52)
#define PA1_ANA_CTRL2				(TOP_CTRL_BASE + 0x53)
#define PA1_VCM_ADJ				(TOP_CTRL_BASE + 0x54)
#define PA1_DAC_CTRL1				(TOP_CTRL_BASE + 0x5e)
#define PA1_DAC_CTRL2				(TOP_CTRL_BASE + 0x5f)
#define PA1_H_BRIDGE_CTRL			(TOP_CTRL_BASE + 0x62)
#define PA1_DSP_MISC0				(TOP_CTRL_BASE + 0x65)
#define PA1_SS_CTRL1				(TOP_CTRL_BASE + 0x6c)
#define PA1_SS_CTRL2				(TOP_CTRL_BASE + 0x6d)
#define PA1_PIN_CONTROL1			(TOP_CTRL_BASE + 0x74)
#define PA1_PIN_CONTROL2			(TOP_CTRL_BASE + 0x75)
#define PA1_MISC_CONTROL			(TOP_CTRL_BASE + 0x76)
#define PA1_FAULT_CLEAR				(TOP_CTRL_BASE + 0x78)
#define PA1_DEBUG_SEL1				(TOP_CTRL_BASE + 0x7d)
#define PA1_DEBUG_SEL2				(TOP_CTRL_BASE + 0x7e)

/* Switch the top and dsp registers*/
#define PA1_PAGE_NUM				(TOP_CTRL_BASE + 0xFF)

#define DSP_CTRL_BASE				(0x00)
#define PA1_AEQ_COEF_ADDR			(DSP_CTRL_BASE + (0x0 << 2))
#define PA1_AEQ_COEF_DATA			(DSP_CTRL_BASE + (0x1 << 2))
#define PA1_AEQ_IRQ_MASK			(DSP_CTRL_BASE + (0x7 << 2))
#define PA1_AEQ_MVOL_RAMP_DOWN_MUX		(DSP_CTRL_BASE + (0x8 << 2))
#define PA1_AEQ_SOFT_REST			(DSP_CTRL_BASE + (0x9 << 2))
#define PA1_AED_STATUS_CTRL			(DSP_CTRL_BASE + (0x10 << 2))
#define PA1_AED_SEL				(DSP_CTRL_BASE + (0x11 << 2))
#define PA1_AED_EQ_VOLUME_VAL			(DSP_CTRL_BASE + (0x12 << 2))
#define PA1_AED_EQ_VOLUME_STEP_CNT		(DSP_CTRL_BASE + (0x13 << 2))
#define PA1_AED_MUTE_CTRL			(DSP_CTRL_BASE + (0x14 << 2))
#define PA1_AEQ_DSP_VOL_DBG_SEL			(DSP_CTRL_BASE + (0x15 << 2))
#define PA1_AED_HY_CTRL				(DSP_CTRL_BASE + (0x16 << 2))
#define PA1_AED_CLIP_THD			(DSP_CTRL_BASE + (0x17 << 2))
#define PA1_AED_HY_MASK1			(DSP_CTRL_BASE + (0x18 << 2))
#define PA1_AED_HY_MASK2			(DSP_CTRL_BASE + (0x19 << 2))
#define PA1_AED_DRC_DOWN			(DSP_CTRL_BASE + (0x20 << 2))
#define PA1_AEQ_RO_DSP_DBG			(DSP_CTRL_BASE + (0x30 << 2))
#define PA1_AEQ_RO_LM_RMS_OUT			(DSP_CTRL_BASE + (0x31 << 2))
#define PA1_AEQ_RO_DSP_ERR			(DSP_CTRL_BASE + (0x32 << 2))

#define PA1_FILTER_PARAM_SIZE			(5)
#define PA1_FILTER_PARAM_BYTE			(66) /*"0x%8.8x "*/

#define PA1_DC_CUT_RAM_ADD			(0)
#define PA1_DC_FILTER_PARAM_SIZE		(6)

#define PA1_EQ_FILTER_SIZE			(180)
#define PA1_EQ_BAND				(15)
#define PA1_EQ_FILTER_SIZE_CH			(90)
#define PA1_EQ_FILTER_RAM_ADD			(10)

#define PA1_AED_FULLBAND_DRC_SIZE		(12)
#define PA1_AED_FULLBAND_DRC_BYTES		(136)
#define PA1_AED_FULLBAND_DRC_GROUP_SIZE		(1)
#define PA1_AED_FULLBAND_DRC_OFFSET_1		(7)
#define PA1_FULLBAND_FILTER_RAM_ADD		(268)

#define PA1_AED_SINGLE_BAND_DRC_SIZE		(14)
#define PA1_AED_MULTIBAND_DRC_SIZE		(42)
#define PA1_MULTIBAND_DRC_PARAM_BYTE		(158) /*"0x%8.8x "*/
#define PA1_AED_MULTIBAND_DRC_BANDS		(3)
#define PA1_AED_MULTIBAND_DRC_OFFSET		(5)
#define PA1_MULTIBAND_FILTER_RAM_ADD		(226)

#define PA1_CROSSOVER_FILTER_SIZE		(24)
#define PA1_CROSSOVER_FILTER_RAM_ADD		(202)
#define PA1_CROSSOVER_FILTER_BAND		(4)

#define PA1_3D_SURROUND_BAND			(2)
#define PA1_AED_3D_SURROUND_SIZE		(12)
#define PA1_3D_SURROUND_RAM_ADD			(190)

#define PA1_MIXER_GAIN_RAM_ADD			(6)
#define PA1_MIXER_GAIN_DAC_RAM_ADD		(284)
#define PA1_MIXER_GAIN_I2S_RAM_ADD		(288)
#define PA1_LEVEL_METER_RAM_ADD			(292)
#define PA1_AED_MIXER_PARAM_SIZE		(12)
#define PA1_AED_MIXER_INPUT_SIZE		(4)
#define PA1_AED_MIXER_POST_SIZE			(8)

/* 20Hz, highpass filter */
static unsigned int PA1_DC_CUT_COEFF[PA1_DC_FILTER_PARAM_SIZE] = {
	0x01ff0d95, 0x0c01e4d5, 0x0e01e462, 0x03fe1ab8, 0x01ff0d95, 0x0
};

static unsigned int PA1_EQ_COEFF[PA1_EQ_FILTER_SIZE] = {
	/*Ch1 EQ 0~14*/
	/*0~4 band*/
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	/*5~9 band*/
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	/*10~14 band*/
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	/*Ch2 EQ 0~14*/
	/*0~4 band*/
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	/*5~9 band*/
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	/*10~14 band*/
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x2000000, 0x0,
};

/*3d surround*/
static unsigned int PA1_3D_SURROUND_COEFF[PA1_AED_3D_SURROUND_SIZE] = {
	/*low-pass filter1*/
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x02000000,
	0x00000000,
	/*high-pass filter1*/
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x02000000,
	/* 3d gain  */
	0x00800000,
};

/*fiter1 fc: 150Hz; fiter2 fc: 5KHz*/
static unsigned int PA1_CROSSOVER_COEFF[PA1_CROSSOVER_FILTER_SIZE] = {
	/*low-pass filter1*/
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x2000000, 0x00000000,
	/*high-pass filter1*/
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x2000000, 0x00000000,
	/*low-pass filter2*/
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x2000000, 0x00000000,
	/*high-pass filter2*/
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x2000000, 0x00000000,
};

static unsigned int PA1_MIXER_PARAM[PA1_AED_MIXER_PARAM_SIZE] = {
	0x00800000,  /* Input mixer */
	0x00000000,
	0x00000000,
	0x00800000,

	0x0073f780,  /* Post mixer of DAC*/
	0x00000000,
	0x00000000,
	0x0073f780,

	0x00800000,  /* Post mixer of I2S */
	0x00000000,
	0x00000000,
	0x00800000,
};

static unsigned int PA1_FULLBAND_DRC_COEFF[PA1_AED_FULLBAND_DRC_SIZE] = {
	0x0005ba29,  /* RMS coeff00 */
	0x00000000,
	0x00000000,  /* K0 */
	0x00000000,
	0x00000000,  /* THD0 */
	0xce000000,  /* THD1 */

	0x00040000,  /* K1 */
	0x00040000,  /* K2 */
	0x00000000,  /* offset1 */
	0x00000000,  /* offset2 */
	0x00004aea,  /* RELEASE_COEF00 */
	0x0002e578,  /* ATTACK_COEF00 */
};

static unsigned int PA1_MULTIBAND_DRC_COEFF[PA1_AED_MULTIBAND_DRC_SIZE] = {
	0x00012aa0,  /* Low RMS coeff0 */
	0x00000000,
	0x00000000,  /* Low K0*/
	0x00000000,
	0x00000000,  /* Low THD1 */
	0x00000000,  /* Low THD2 */
	0x00000000,  /* Low K1*/
	0x00000000,  /* Low K2*/
	0x00000000,  /* Low OFFSET1*/
	0x00000000,  /* Low OFFSET2*/
	0x00000eff,  /* Low RELEASE coeff0 */
	0x000095a8,  /* Low ATTACK coeff0 */
	0x00000000,  /* low gain*/
	0x00000000,

	0x0002e578,  /* Mid RMS coeff0 */
	0x00000000,
	0x00000000,  /* Mid K0*/
	0x00000000,
	0x00000000,  /* Mid THD1 */
	0x00000000,  /* Mid THD2 */
	0x00000000,  /* Mid K1*/
	0x00000000,  /* Mid K2*/
	0x00000000,  /* Mid OFFSET1*/
	0x00000000,  /* Mid OFFSET2*/
	0x00000eff,  /* Mid RELEASE coeff0 */
	0x000095a8,  /* Mid ATTACK coeff0 */
	0x00000000,  /* mid gain*/
	0x00000000,

	0x0005ba29,  /* High RMS coeff0 */
	0x00000000,
	0x00000000,  /* High K0*/
	0x00000000,
	0x00000000,  /* High THD1 */
	0x00000000,  /* High THD2 */
	0x00000000,  /* High K1*/
	0x00000000,  /* High K2*/
	0x00000000,  /* High OFFSET1*/
	0x00000000,  /* High OFFSET2*/
	0x00000eff,  /* High RELEASE coeff0 */
	0x000095a8,  /* High ATTACK coeff0 */
	0x00000000,  /* high gain*/
	0x00000000,
};

static const int pa1_reg_set[][2] = {
	/* vision C dig reg set */
	{ PA1_ANA_TRIM_4, 0x72 },
	{ PA1_PLL_CTRL1, 0xb4 },
	{ PA1_PLL_CTRL0, 0x34 },
	{ PA1_AUTOCONFIG_CTRL, 0x1 },
	{ PA1_PINMUX_IE, 0x1b },
	{ PA1_DEVICE_CTRL2, 0x84 },
	{ PA1_PIN_CONTROL1, 0x2 },
	{ PA1_ANA_FORCE2, 0x10 },
	{ PA1_ANA_FORCE6, 0x20 },
	{ PA1_FAULT_CLEAR, 0xFF },
	{ PA1_FAULT_CLEAR, 0x0 },
	{ PA1_MISC_CONTROL, 0x20 },
	{ PA1_MISC_CONTROL, 0x0 },
	{ PA1_PIN_CONTROL2, 0x3f },
	{ PA1_PINMUX_IE, 0x1f },
	/* ana reg set */
	{ PA1_VCM_ADJ, 0xc8 },
	{ PA1_DAC_CTRL1, 0x5d },
	{ PA1_DAC_CTRL2, 0x5d },
	{ PA1_ANA_CTRL1, 0xbc },
	/* enter play mode */
	{ PA1_DEVICE_CTRL2, 0x85 },
};

struct pa1_acodec_platform_data {
	int reset_pin;
};

#endif
