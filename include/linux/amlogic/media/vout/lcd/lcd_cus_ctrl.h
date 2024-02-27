/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _INC_LCD_CUS_CTRL_H
#define _INC_LCD_CUS_CTRL_H
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>

/* **********************************
 * custom control define
 * **********************************
 */
#define LCD_CUS_CTRL_SEL_NONE             0x00000000
#define LCD_CUS_CTRL_SEL_UFR              0x00000001
#define LCD_CUS_CTRL_SEL_DFR              0x00000002
#define LCD_CUS_CTRL_SEL_EXTEND_TMG       0x00000004
#define LCD_CUS_CTRL_SEL_CLK_ADV          0x00000008
#define LCD_CUS_CTRL_SEL_TCON_SW_POL      0x00010000
#define LCD_CUS_CTRL_SEL_TCON_SW_PDF      0x00020000

#define LCD_CUS_CTRL_SEL_TIMMING          (LCD_CUS_CTRL_SEL_UFR | \
		LCD_CUS_CTRL_SEL_DFR | LCD_CUS_CTRL_SEL_EXTEND_TMG)

#define LCD_CUS_CTRL_ATTR_CNT_MAX        32

#define LCD_CUS_CTRL_TYPE_UFR            0x00
#define LCD_CUS_CTRL_TYPE_DFR            0x01
#define LCD_CUS_CTRL_TYPE_EXTEND_TMG     0x02
#define LCD_CUS_CTRL_TYPE_CLK_ADV        0x03
#define LCD_CUS_CTRL_TYPE_TCON_SW_POL    0x10
#define LCD_CUS_CTRL_TYPE_TCON_SW_PDF    0x11
#define LCD_CUS_CTRL_TYPE_MAX            0xff

struct lcd_ufr_s {
	struct lcd_detail_timing_s timing;
};

struct lcd_dfr_fr_s {
	unsigned char timing_index;
	unsigned short frame_rate;
	struct lcd_detail_timing_s timing;
};

struct lcd_dfr_timing_s {
	unsigned short htotal;
	unsigned short vtotal;
	unsigned short vtotal_min;
	unsigned short vtotal_max;
	unsigned short frame_rate_min;
	unsigned short frame_rate_max;
	unsigned short hpw;
	unsigned short hbp;
	unsigned short vpw;
	unsigned short vbp;
};

struct lcd_dfr_s {
	unsigned char fr_cnt;
	unsigned char tmg_group_cnt;
	struct lcd_dfr_fr_s *fr;
	struct lcd_dfr_timing_s *dfr_timing;
};

struct lcd_extend_tmg_s {
	unsigned char group_cnt;
	struct lcd_detail_timing_s *timing;
};

struct lcd_clk_adv_s {
	unsigned char ss_freq;
	unsigned char ss_mode;
};

union lcd_cus_ctrl_attr_u {
	struct lcd_ufr_s *ufr_attr;
	struct lcd_dfr_s *dfr_attr;
	struct lcd_extend_tmg_s *extend_tmg_attr;
	struct lcd_clk_adv_s *clk_adv_attr;
};

struct lcd_cus_ctrl_attr_config_s {
	unsigned char attr_index;
	unsigned char attr_flag; //bit[3:0]
	unsigned char param_flag; //bit[7:4], according to each attr usage
	unsigned char attr_type;

	unsigned short param_size;
	unsigned char active;
	unsigned int priv_sel;

	union lcd_cus_ctrl_attr_u attr;
};

#endif
