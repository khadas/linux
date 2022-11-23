/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _LCD_CLK_CONFIG_CTRL_H
#define _LCD_CLK_CONFIG_CTRL_H

#include "../lcd_reg.h"
#include "lcd_clk_config.h"

/* **********************************
 * COMMON
 * **********************************
 */
#define PLL_M_MIN                   2
#define PLL_M_MAX                   511
#define PLL_N_MIN                   1
#define PLL_N_MAX                   1
#define PLL_FREF_MIN                (5 * 1000)
#define PLL_FREF_MAX                (25 * 1000)

/* **********************************
 * AXG
 * **********************************
 */
/* ******** register bit ******** */
/* PLL_CNTL */
#define LCD_PLL_LOCK_AXG            31
#define LCD_PLL_EN_AXG              30
#define LCD_PLL_RST_AXG             29
#define LCD_PLL_OD_AXG              16
#define LCD_PLL_N_AXG               9
#define LCD_PLL_M_AXG               0

/* ******** frequency limit (unit: kHz) ******** */
#define PLL_OD_FB_AXG               0
#define PLL_FRAC_RANGE_AXG          BIT(10)
#define PLL_FRAC_SIGN_BIT_AXG       11
#define PLL_OD_SEL_MAX_AXG          3
#define PLL_VCO_MIN_AXG             (960 * 1000)
#define PLL_VCO_MAX_AXG             (1920 * 1000)

/* video */
#define CRT_VID_CLK_IN_MAX_AXG      (1920 * 1000)
#define ENCL_CLK_IN_MAX_AXG         (200 * 1000)

/* G12A */
/* ******** register bit ******** */
/* PLL_CNTL bit: GP0 */
#define LCD_PLL_LOCK_GP0_G12A       31
#define LCD_PLL_EN_GP0_G12A         28
#define LCD_PLL_RST_GP0_G12A        29
#define LCD_PLL_OD_GP0_G12A         16
#define LCD_PLL_N_GP0_G12A          10
#define LCD_PLL_M_GP0_G12A          0

/* ******** frequency limit (unit: kHz) ******** */
#define PLL_OD_FB_GP0_G12A          0
#define PLL_FRAC_RANGE_GP0_G12A     BIT(17)
#define PLL_FRAC_SIGN_BIT_GP0_G12A  18
#define PLL_OD_SEL_MAX_GP0_G12A     5
#define PLL_VCO_MIN_GP0_G12A        (3000 * 1000)
#define PLL_VCO_MAX_GP0_G12A        (6000 * 1000)

/* PLL_CNTL bit: hpll */
#define LCD_PLL_LOCK_HPLL_G12A      31
#define LCD_PLL_EN_HPLL_G12A        28
#define LCD_PLL_RST_HPLL_G12A       29
#define LCD_PLL_N_HPLL_G12A         10
#define LCD_PLL_M_HPLL_G12A         0

#define LCD_PLL_OD3_HPLL_G12A       20
#define LCD_PLL_OD2_HPLL_G12A       18
#define LCD_PLL_OD1_HPLL_G12A       16

/* ******** frequency limit (unit: kHz) ******** */
#define PLL_OD_FB_HPLL_G12A         0
#define PLL_FRAC_RANGE_HPLL_G12A    BIT(17)
#define PLL_FRAC_SIGN_BIT_HPLL_G12A 18
#define PLL_OD_SEL_MAX_HPLL_G12A    3
#define PLL_VCO_MIN_HPLL_G12A       (3000 * 1000)
#define PLL_VCO_MAX_HPLL_G12A       (6000 * 1000)

/* video */
#define CRT_VID_CLK_IN_MAX_G12A     (6000 * 1000)
#define ENCL_CLK_IN_MAX_G12A        (200 * 1000)

/* **********************************
 * TL1
 * **********************************
 */
/* ******** register bit ******** */
/* PLL_CNTL 0x20 */
#define LCD_PLL_LOCK_TL1            31
#define LCD_PLL_EN_TL1              28
#define LCD_PLL_RST_TL1             29
#define LCD_PLL_N_TL1               10
#define LCD_PLL_M_TL1               0

#define LCD_PLL_OD3_TL1             19
#define LCD_PLL_OD2_TL1             23
#define LCD_PLL_OD1_TL1             21

/* ******** frequency limit (unit: kHz) ******** */
#define PLL_OD_FB_TL1               0
#define PLL_FRAC_RANGE_TL1          BIT(17)
#define PLL_FRAC_SIGN_BIT_TL1       18
#define PLL_OD_SEL_MAX_TL1          3
#define PLL_VCO_MIN_TL1             (3384 * 1000)
#define PLL_VCO_MAX_TL1             (6024 * 1000)

/* video */
#define CLK_DIV_IN_MAX_TL1          (3100 * 1000)
#define CRT_VID_CLK_IN_MAX_TL1      (3100 * 1000)
#define ENCL_CLK_IN_MAX_TL1         (750 * 1000)

/* **********************************
 * TM2
 * **********************************
 */
#define PLL_VCO_MIN_TM2             (3000 * 1000)
#define PLL_VCO_MAX_TM2             (6000 * 1000)

/* **********************************
 * T5D
 * **********************************
 */
/* video */
#define CLK_DIV_IN_MAX_T5D          (3100 * 1000)
#define CRT_VID_CLK_IN_MAX_T5D      (3100 * 1000)
#define ENCL_CLK_IN_MAX_T5D         (400 * 1000)

/* **********************************
 * T7
 * **********************************
 */
#define LCD_PLL_OD3_T7               23
#define LCD_PLL_OD2_T7               21
#define LCD_PLL_OD1_T7               19
#define LCD_PLL_LOCK_T7              31

/* **********************************
 * Spread Spectrum
 * **********************************
 */
#define LCD_SS_STEP_BASE            500 /* ppm */

/* **********************************
 * pll & clk parameter
 * **********************************
 */
/* ******** clk calculation ******** */
#define PLL_WAIT_LOCK_CNT           200
 /* frequency unit: kHz */
#define FIN_FREQ                    (24 * 1000)
/* clk max error */
#define MAX_ERROR                   (2 * 1000)

/* ******** register bit ******** */
/* divider */
#define CRT_VID_DIV_MAX             255

#define DIV_PRE_SEL_MAX             6
#define EDP_DIV0_SEL_MAX            15
#define EDP_DIV1_SEL_MAX            8

extern const unsigned int od_fb_table[2];
extern const unsigned int od_table[6];
extern const unsigned int tcon_div_table[5];
extern unsigned int tcon_div[5][3];

/* g9tv, g9bb, gxbb divider */
#define CLK_DIV_I2O     0
#define CLK_DIV_O2I     1
enum div_sel_e {
	CLK_DIV_SEL_1 = 0,
	CLK_DIV_SEL_2,    /* 1 */
	CLK_DIV_SEL_3,    /* 2 */
	CLK_DIV_SEL_3p5,  /* 3 */
	CLK_DIV_SEL_3p75, /* 4 */
	CLK_DIV_SEL_4,    /* 5 */
	CLK_DIV_SEL_5,    /* 6 */
	CLK_DIV_SEL_6,    /* 7 */
	CLK_DIV_SEL_6p25, /* 8 */
	CLK_DIV_SEL_7,    /* 9 */
	CLK_DIV_SEL_7p5,  /* 10 */
	CLK_DIV_SEL_12,   /* 11 */
	CLK_DIV_SEL_14,   /* 12 */
	CLK_DIV_SEL_15,   /* 13 */
	CLK_DIV_SEL_2p5,  /* 14 */
	CLK_DIV_SEL_4p67, /* 15 */
	CLK_DIV_SEL_MAX,
};

extern char *lcd_clk_div_sel_table[];
extern unsigned int lcd_clk_div_table[][3];

extern char *lcd_ss_level_table_tl1[];
extern char *lcd_ss_freq_table_tl1[];
extern char *lcd_ss_mode_table_tl1[];
extern unsigned char ss_level_max;
extern unsigned char ss_freq_max;
extern unsigned char ss_mode_max;

#endif
