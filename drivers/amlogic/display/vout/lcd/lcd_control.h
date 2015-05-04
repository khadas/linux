/*
 * drivers/amlogic/display/vout/lcd/lcd_control.h
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

#ifndef LCD_CONTROL_H
#define LCD_CONTROL_H

#include <linux/types.h>

/* **********************************
 * delay between video encoder & tcon
 * ********************************** */
#define MIPI_DELAY                  8
#define LVDS_DELAY                  8
#define EDP_DELAY                   8
#define TTL_DELAY                   19

/* **********************************
 * pll & clk parameter
 * ********************************** */
/* ******** clk calculation ******** */
#define PLL_WAIT_LOCK_CNT           200
 /* frequency unit: kHz */
#define FIN_FREQ                    (24 * 1000)
/* clk max error */
#define MAX_ERROR                   (2 * 1000)

/* ******** register bit ******** */
/* divider */
#define CRT_VID_DIV_MAX             15

#define DIV_PRE_SEL_MAX             6
#define EDP_DIV0_SEL_MAX            15
#define EDP_DIV1_SEL_MAX            8

static const unsigned int od_table[4] = {
	1, 2, 4, 8
};
static const unsigned int div_pre_table[6] = {
	1, 2, 3, 4, 5, 6
};
static const unsigned int edp_div0_table[15] = {
	1, 2, 3, 4, 5, 7, 8, 9, 11, 13, 17, 19, 23, 29, 31
};
static const unsigned int edp_div1_table[8] = {
	1, 2, 4, 5, 6, 7, 9, 13
};

/* **********************************
 * M6
 * ********************************** */
/* ******** register bit ******** */
/* PLL_CNTL */
#define LCD_PLL_LOCK_M6             31
#define LCD_PLL_PD_M6               30
#define LCD_PLL_RST_M6              29
#define LCD_PLL_OD_M6               16
#define LCD_PLL_N_M6                9
#define LCD_PLL_M_M6                0

/* ******** frequency limit (unit: kHz) ******** */
/* pll */
#define SS_LEVEL_MAX_M6             7
#define PLL_M_MIN_M6                2
#define PLL_M_MAX_M6                100
#define PLL_N_MIN_M6                1
#define PLL_N_MAX_M6                1
#define PLL_OD_SEL_MAX_M6           2
#define PLL_FREF_MIN_M6             (5 * 1000)
#define PLL_FREF_MAX_M6             (30 * 1000)
#define PLL_VCO_MIN_M6              (750 * 1000)
#define PLL_VCO_MAX_M6              (1500 * 1000)

/* video */
#define DIV_PRE_CLK_IN_MAX_M6       (1300 * 1000)
#define DIV_POST_CLK_IN_MAX_M6      (800 * 1000)
#define CRT_VID_CLK_IN_MAX_M6       (600 * 1000)
#define ENCL_CLK_IN_MAX_M6          (208 * 1000)

/* **********************************
 * M8
 * ********************************** */
/* ******** register bit ******** */
/* PLL_CNTL */
#define LCD_PLL_LOCK_M8             31
#define LCD_PLL_EN_M8               30
#define LCD_PLL_RST_M8              29
#define LCD_PLL_N_M8                24
#define LCD_PLL_OD_M8               9
#define LCD_PLL_M_M8                0

/* ******** frequency limit (unit: kHz) ******** */
/* pll */
#define SS_LEVEL_MAX_M8             5
#define PLL_M_MIN_M8                2
#define PLL_M_MAX_M8                511
#define PLL_N_MIN_M8                1
#define PLL_N_MAX_M8                1
#define PLL_OD_SEL_MAX_M8           3
#define PLL_FREF_MIN_M8             (5 * 1000)
#define PLL_FREF_MAX_M8             (25 * 1000)
#define PLL_VCO_MIN_M8              (1200 * 1000)
#define PLL_VCO_MAX_M8              (3000 * 1000)

/* video */
#define DIV_PRE_CLK_IN_MAX_M8       (1500 * 1000)
#define DIV_POST_CLK_IN_MAX_M8      (1000 * 1000)
#define CRT_VID_CLK_IN_MAX_M8       (1300 * 1000)
#define ENCL_CLK_IN_MAX_M8          (333 * 1000)

/* **********************************
 * M8B
 * ********************************** */
/* ******** register bit ******** */
/* PLL_CNTL */
#define LCD_PLL_LOCK_M8B            31
#define LCD_PLL_EN_M8B              30
#define LCD_PLL_RST_M8B             29
#define LCD_PLL_OD_M8B              16
#define LCD_PLL_N_M8B               10
#define LCD_PLL_M_M8B               0

/* ******** frequency limit (unit: kHz) ******** */
/* pll */
#define SS_LEVEL_MAX_M8B            5
#define PLL_M_MIN_M8B               2
#define PLL_M_MAX_M8B               511
#define PLL_N_MIN_M8B               1
#define PLL_N_MAX_M8B               1
#define PLL_OD_SEL_MAX_M8B          3
#define PLL_FREF_MIN_M8B            (5 * 1000)
#define PLL_FREF_MAX_M8B            (25 * 1000)
#define PLL_VCO_MIN_M8B             (1200 * 1000)
#define PLL_VCO_MAX_M8B             (3000 * 1000)

/* video */
#define DIV_PRE_CLK_IN_MAX_M8B      (1500 * 1000)
#define DIV_POST_CLK_IN_MAX_M8B     (1000 * 1000)
#define CRT_VID_CLK_IN_MAX_M8B      (1300 * 1000)
#define ENCL_CLK_IN_MAX_M8B         (333 * 1000)

/* **********************************
 * clk config
 * ********************************** */
struct lcd_clk_config_s { /* unit: kHz */
	/* IN-OUT parameters */
	unsigned int fin;
	unsigned int fout;

	/* pll parameters */
	unsigned int pll_m;
	unsigned int pll_n;
	unsigned int pll_od_sel;
	unsigned int pll_level;
	unsigned int pll_frac;
	unsigned int pll_fout;
	unsigned int ss_level;
	unsigned int edp_div0;
	unsigned int edp_div1;
	unsigned int div_pre;
	unsigned int div_post;
	unsigned int xd;

	/* clk path node parameters */
	unsigned int ss_level_max;
	unsigned int pll_m_max;
	unsigned int pll_m_min;
	unsigned int pll_n_max;
	unsigned int pll_n_min;
	unsigned int pll_od_sel_max;
	unsigned int div_pre_sel_max;
	unsigned int xd_max;
	unsigned int pll_ref_fmax;
	unsigned int pll_ref_fmin;
	unsigned int pll_vco_fmax;
	unsigned int pll_vco_fmin;
	unsigned int pll_out_fmax;
	unsigned int pll_out_fmin;
	unsigned int div_pre_in_fmax;
	unsigned int div_pre_out_fmax;
	unsigned int div_post_out_fmax;
	unsigned int xd_out_fmax;
	unsigned int err_fmin;
};

extern struct lcd_clk_config_s *get_lcd_clk_config(void);
extern enum Bool_check_e lcd_chip_valid_check(void);

/* **********************************
 * PHY Config
 * ********************************** */
/* ******** M6 ******** */
/* bit[6:0] */
#define BIT_PHY_LANE_M6         0
#define PHY_LANE_WIDTH_M6       7

/* LVDS */
#define LVDS_LANE_0_M6          (1 << 0)
#define LVDS_LANE_1_M6          (1 << 1)
#define LVDS_LANE_2_M6          (1 << 2)
#define LVDS_LANE_3_M6          (1 << 3)
#define LVDS_LANE_CLK_M6        (1 << 5)
#define LVDS_LANE_COUNT_3_M6    (LVDS_LANE_CLK_M6 | LVDS_LANE_0_M6 |\
				LVDS_LANE_1_M6 | LVDS_LANE_2_M6)
#define LVDS_LANE_COUNT_4_M6    (LVDS_LANE_CLK_M6 | LVDS_LANE_0_M6 |\
				LVDS_LANE_1_M6 | LVDS_LANE_2_M6 |\
				LVDS_LANE_3_M6)

/* ******** M8,M8b,M8M2 ******** */
/* bit[15:11] */
#define BIT_PHY_LANE_M8         11
#define PHY_LANE_WIDTH_M8       5

/* LVDS */
#define LVDS_LANE_0_M8          (1 << 4)
#define LVDS_LANE_1_M8          (1 << 3)
#define LVDS_LANE_2_M8          (1 << 1)
#define LVDS_LANE_3_M8          (1 << 0)
#define LVDS_LANE_CLK_M8        (1 << 2)
#define LVDS_LANE_COUNT_3_M8    (LVDS_LANE_CLK_M8 | LVDS_LANE_0_M8 |\
				LVDS_LANE_1_M8 | LVDS_LANE_2_M8)
#define LVDS_LANE_COUNT_4_M8    (LVDS_LANE_CLK_M8 | LVDS_LANE_0_M8 |\
				LVDS_LANE_1_M8 | LVDS_LANE_2_M8 |\
				LVDS_LANE_3_M8)

/* MIPI-DSI */
#define DSI_LANE_0              (1 << 4)
#define DSI_LANE_1              (1 << 3)
#define DSI_LANE_2              (1 << 2)
#define DSI_LANE_3              (1 << 1)
#define DSI_LANE_CLK            (1 << 0)
#define DSI_LANE_COUNT_1        (DSI_LANE_CLK | DSI_LANE_0)
#define DSI_LANE_COUNT_2        (DSI_LANE_CLK | DSI_LANE_0 | DSI_LANE_1)
#define DSI_LANE_COUNT_3        (DSI_LANE_CLK | DSI_LANE_0 |\
					DSI_LANE_1 | DSI_LANE_2)
#define DSI_LANE_COUNT_4        (DSI_LANE_CLK | DSI_LANE_0 |\
					DSI_LANE_1 | DSI_LANE_2 | DSI_LANE_3)

/* eDP */
#define EDP_LANE_AUX            (1 << 4)
#define EDP_LANE_0              (1 << 3)
#define EDP_LANE_1              (1 << 2)
#define EDP_LANE_2              (1 << 1)
#define EDP_LANE_3              (1 << 0)
#define EDP_LANE_COUNT_1        (EDP_LANE_AUX | EDP_LANE_0)
#define EDP_LANE_COUNT_2        (EDP_LANE_AUX | EDP_LANE_0 | EDP_LANE_1)
#define EDP_LANE_COUNT_4        (EDP_LANE_AUX | EDP_LANE_0 | EDP_LANE_1 |\
					EDP_LANE_2 | EDP_LANE_3)
#endif
