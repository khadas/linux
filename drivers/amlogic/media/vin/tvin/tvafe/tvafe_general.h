/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _TVAFE_GENERAL_H
#define _TVAFE_GENERAL_H

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "tvafe_cvd.h"
#include <linux/amlogic/cpu_version.h>

/************************************************* */
/* *** macro definitions *********************** */
/* ************************************ */

#define HHI_ANA_CLK_BASE	0xff6463cc
#define ATV_DMD_SYS_CLK_CNTL    0xfe000218

/* reg need in tvafe_general.c */
#define HHI_VDAC_CNTL0			0xbd
#define HHI_VDAC_CNTL1			0xbe
#define HHI_VIID_CLK_DIV		0x4a
#define HHI_VIID_CLK_CNTL		0x4b
#define HHI_VIID_DIVIDER_CNTL	0x4c
#define HHI_VID_CLK_CNTL2		0x65
#define HHI_VID_DIVIDER_CNTL	0x66

/* T5W add 3d comb clk */
#define HHI_TVFE_CLK_CNTL		0x7e
#define TVFE_CLK_GATE			0x6
#define TVFE_CLK_GATE_WIDTH		1
#define TVFE_CLK_SEL			0x7
#define TVFE_CLK_SEL_WIDTH		3

#define VENC_VDAC_DACSEL0			0x1b78
#define P_VENC_VDAC_DACSEL0		VCBUS_REG_ADDR(VENC_VDAC_DACSEL0)
#define VENC_VDAC_DACSEL1			0x1b79
#define P_VENC_VDAC_DACSEL1		VCBUS_REG_ADDR(VENC_VDAC_DACSEL1)
#define VENC_VDAC_DACSEL2			0x1b7a
#define P_VENC_VDAC_DACSEL2		VCBUS_REG_ADDR(VENC_VDAC_DACSEL2)
#define VENC_VDAC_DACSEL3			0x1b7b
#define P_VENC_VDAC_DACSEL3		VCBUS_REG_ADDR(VENC_VDAC_DACSEL3)
#define VENC_VDAC_DACSEL4			0x1b7c
#define P_VENC_VDAC_DACSEL4		VCBUS_REG_ADDR(VENC_VDAC_DACSEL4)
#define VENC_VDAC_DACSEL5			0x1b7d
#define P_VENC_VDAC_DACSEL5		VCBUS_REG_ADDR(VENC_VDAC_DACSEL5)
#define VENC_VDAC_SETTING			0x1b7e
#define P_VENC_VDAC_SETTING		VCBUS_REG_ADDR(VENC_VDAC_SETTING)
#define VENC_VDAC_TST_VAL			0x1b7f
#define P_VENC_VDAC_TST_VAL		VCBUS_REG_ADDR(VENC_VDAC_TST_VAL)
#define VENC_VDAC_DAC0_GAINCTRL			0x1bf0
#define P_VENC_VDAC_DAC0_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC0_GAINCTRL)
#define VENC_VDAC_DAC0_OFFSET			0x1bf1
#define P_VENC_VDAC_DAC0_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC0_OFFSET)
#define VENC_VDAC_DAC1_GAINCTRL			0x1bf2
#define P_VENC_VDAC_DAC1_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC1_GAINCTRL)
#define VENC_VDAC_DAC1_OFFSET			0x1bf3
#define P_VENC_VDAC_DAC1_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC1_OFFSET)
#define VENC_VDAC_DAC2_GAINCTRL			0x1bf4
#define P_VENC_VDAC_DAC2_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC2_GAINCTRL)
#define VENC_VDAC_DAC2_OFFSET			0x1bf5
#define P_VENC_VDAC_DAC2_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC2_OFFSET)
#define VENC_VDAC_DAC3_GAINCTRL			0x1bf6
#define P_VENC_VDAC_DAC3_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC3_GAINCTRL)
#define VENC_VDAC_DAC3_OFFSET			0x1bf7
#define P_VENC_VDAC_DAC3_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC3_OFFSET)
#define VENC_VDAC_DAC4_GAINCTRL			0x1bf8
#define P_VENC_VDAC_DAC4_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC4_GAINCTRL)
#define VENC_VDAC_DAC4_OFFSET			0x1bf9
#define P_VENC_VDAC_DAC4_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC4_OFFSET)
#define VENC_VDAC_DAC5_GAINCTRL			0x1bfa
#define P_VENC_VDAC_DAC5_GAINCTRL	VCBUS_REG_ADDR(VENC_VDAC_DAC5_GAINCTRL)
#define VENC_VDAC_DAC5_OFFSET			0x1bfb
#define P_VENC_VDAC_DAC5_OFFSET		VCBUS_REG_ADDR(VENC_VDAC_DAC5_OFFSET)
#define VENC_VDAC_FIFO_CTRL			0x1bfc
#define P_VENC_VDAC_FIFO_CTRL		VCBUS_REG_ADDR(VENC_VDAC_FIFO_CTRL)

#define HHI_VAFE_CLKXTALIN_CNTL		0x8f
#define P_HHI_VAFE_CLKXTALIN_CNTL	CBUS_REG_ADDR(HHI_VAFE_CLKXTALIN_CNTL)
#define HHI_VAFE_CLKOSCIN_CNTL		0x90
#define P_HHI_VAFE_CLKOSCIN_CNTL	CBUS_REG_ADDR(HHI_VAFE_CLKOSCIN_CNTL)
#define HHI_VAFE_CLKIN_CNTL			0x91
#define P_HHI_VAFE_CLKIN_CNTL		CBUS_REG_ADDR(HHI_VAFE_CLKIN_CNTL)
#define HHI_TVFE_AUTOMODE_CLK_CNTL	0x92
#define P_HHI_TVFE_AUTOMODE_CLK_CNTL CBUS_REG_ADDR(HHI_TVFE_AUTOMODE_CLK_CNTL)
#define HHI_VAFE_CLKPI_CNTL			0x93
#define P_HHI_VAFE_CLKPI_CNTL		CBUS_REG_ADDR(HHI_VAFE_CLKPI_CNTL)

#define HHI_VPU_CLK_CNTL			0x6f

#define HHI_CADC_CNTL				0x20
#define HHI_CADC_CNTL2				0x21
#define HHI_CADC_CNTL3				0x22
#define HHI_CADC_CNTL4				0x23
#define HHI_CADC_CNTL5				0x24
#define HHI_CADC_CNTL6				0x25

#define HHI_DADC_CNTL				0x27
#define HHI_DADC_CNTL2				0x28
#define HHI_DADC_RDBK0_I			0x29
#define HHI_DADC_CNTL3				0x2a
#define HHI_DADC_CNTL4				0x2b

#define HHI_ADC_PLL_CNTL			0xaa
#define P_HHI_ADC_PLL_CNTL			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL)
#define HHI_ADC_PLL_CNTL2			0xab
#define P_HHI_ADC_PLL_CNTL2			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL2)
#define HHI_ADC_PLL_CNTL3			0xac
#define P_HHI_ADC_PLL_CNTL3			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL3)
#define HHI_ADC_PLL_CNTL4			0xad
#define P_HHI_ADC_PLL_CNTL4			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL4)
#define HHI_ADC_PLL_CNTL5			0x9e
#define P_HHI_ADC_PLL_CNTL5			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL5)
#define HHI_ADC_PLL_CNTL6			0x9f
#define P_HHI_ADC_PLL_CNTL6			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL6)
#define HHI_ADC_PLL_CNTL1			0xaf
#define P_HHI_ADC_PLL_CNTL1			CBUS_REG_ADDR(HHI_ADC_PLL_CNTL1)
#define HHI_GCLK_OTHER              0x54

#define HHI_ADC_PLL_CNTL0_TL1		0xb0

/* adc pll ctl, atv demod & tvafe use the same adc module*/
/* module index: atv demod:0x01; tvafe:0x2*/
#define ADC_EN_ATV_DEMOD	0x1
#define ADC_EN_TVAFE		0x2
#define ADC_EN_DTV_DEMOD	0x4
#define ADC_EN_DTV_DEMOD_PLL	0x8

#define LOG_ADC_CAL
/* #define LOG_VGA_EDID */
/* ************************************************** */
/* *** enum definitions ***************************** */
/* ************************************************** */
enum tvafe_adc_ch_e {
	TVAFE_ADC_CH_NULL = 0,
	TVAFE_ADC_CH_0 = 1,  /* avin ch0 */
	TVAFE_ADC_CH_1 = 2,  /* avin ch1 */
	TVAFE_ADC_CH_2 = 3,  /* avin ch2 */
	TVAFE_ADC_CH_ATV = 4,  /* virtual channel for atv demod */
};

enum tvafe_cpu_type {
	TVAFE_CPU_TYPE_TL1  = 3,
	TVAFE_CPU_TYPE_TM2  = 4,
	TVAFE_CPU_TYPE_TM2_B  = 5,
	TVAFE_CPU_TYPE_T5  = 6,
	TVAFE_CPU_TYPE_T5D  = 7,
	TVAFE_CPU_TYPE_T3  = 8,
	TVAFE_CPU_TYPE_T5W  = 9,
	TVAFE_CPU_TYPE_MAX,
};

#define TVAFE_PQ_CONFIG_NUM_MAX    20
struct tvafe_reg_table_s {
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
};

struct meson_tvafe_data {
	enum tvafe_cpu_type cpu_id;
	const char *name;

	struct tvafe_reg_table_s **cvbs_pq_conf;
	struct tvafe_reg_table_s **rf_pq_conf;
};

struct tvafe_clkgate_type {
	/* clktree */
	unsigned int clk_gate_state;
	struct clk *vdac_clk_gate;
};

/* ********************************************* */
/* ******** function claims ******************** */
/* ********************************************* */
enum tvafe_adc_ch_e tvafe_port_to_channel(enum tvin_port_e port,
					  struct tvafe_pin_mux_s *pinmux);
int tvafe_adc_pin_mux(enum tvafe_adc_ch_e adc_ch);
void tvafe_set_regmap(struct am_regs_s *p);
void tvafe_init_reg(struct tvafe_cvd2_s *cvd2, struct tvafe_cvd2_mem_s *mem,
		    enum tvin_port_e port);
void tvafe_set_apb_bus_err_ctrl(void);
void tvafe_enable_module(bool enable);
void tvafe_enable_avout(enum tvin_port_e port, bool enable);
int tvafe_cpu_type(void);
void tvafe_clk_gate_ctrl(int status);
void white_pattern_pga_reset(enum tvin_port_e port);

extern unsigned int cvd_reg87_pal;
extern unsigned int acd_166;
extern void __iomem *ana_addr;
#endif  /* _TVAFE_GENERAL_H */

