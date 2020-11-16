/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_DEBUG_H__
#define __AML_LCD_DEBUG_H__
#include "lcd_reg.h"

#define LCD_DEBUG_REG_CNT_MAX    30
#define LCD_DEBUG_REG_END        0xffffffff

/*tcon adb port use */
#define LCD_ADB_TCON_REG_RW_MODE_NULL              0
#define LCD_ADB_TCON_REG_RW_MODE_RN                1
#define LCD_ADB_TCON_REG_RW_MODE_WM                2
#define LCD_ADB_TCON_REG_RW_MODE_WN                3
#define LCD_ADB_TCON_REG_RW_MODE_WS                4
#define LCD_ADB_TCON_REG_RW_MODE_ERR               5

#define ADB_TCON_REG_8_bit                         0
#define ADB_TCON_REG_32_bit                        1

struct lcd_tcon_adb_reg_s {
	unsigned int rw_mode;
	unsigned int bit_width;
	unsigned int addr;
	unsigned int len;
};

struct lcd_debug_info_reg_s {
	unsigned int *reg_ana_table;
	unsigned int *reg_clk_table;
	unsigned int *reg_encl_table;
	unsigned int *reg_pinmux_table;
};

struct lcd_debug_info_if_s {
	int (*interface_print)(char *buf, int offset);
	int (*reg_dump_interface)(char *buf, int offset);
	int (*reg_dump_phy)(char *buf, int offset);
	struct class_attribute *class_attrs;
};

static unsigned int lcd_reg_dump_clk_gp0_g12a[] = {
	HHI_GP0_PLL_CNTL0_G12A,
	HHI_GP0_PLL_CNTL1_G12A,
	HHI_GP0_PLL_CNTL2_G12A,
	HHI_GP0_PLL_CNTL3_G12A,
	HHI_GP0_PLL_CNTL4_G12A,
	HHI_GP0_PLL_CNTL5_G12A,
	HHI_GP0_PLL_CNTL6_G12A,
	HHI_VIID_CLK_DIV,
	HHI_VIID_CLK_CNTL,
	HHI_VID_CLK_CNTL2,
	HHI_MIPIDSI_PHY_CLK_CNTL,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_clk_hpll_g12a[] = {
	HHI_HDMI_PLL_CNTL,
	HHI_HDMI_PLL_CNTL2,
	HHI_HDMI_PLL_CNTL3,
	HHI_HDMI_PLL_CNTL4,
	HHI_HDMI_PLL_CNTL5,
	HHI_HDMI_PLL_CNTL6,
	HHI_HDMI_PLL_CNTL7,
	HHI_VID_PLL_CLK_DIV,
	HHI_VIID_CLK_DIV,
	HHI_VIID_CLK_CNTL,
	HHI_VID_CLK_CNTL2,
	HHI_MIPIDSI_PHY_CLK_CNTL,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_clk_tl1[] = {
	HHI_TCON_PLL_CNTL0,
	HHI_TCON_PLL_CNTL1,
	HHI_TCON_PLL_CNTL2,
	HHI_TCON_PLL_CNTL3,
	HHI_TCON_PLL_CNTL4,
	HHI_VID_PLL_CLK_DIV,
	HHI_VIID_CLK_DIV,
	HHI_VIID_CLK_CNTL,
	HHI_VID_CLK_CNTL2,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_ana_t5[] = {
	HHI_TCON_PLL_CNTL0,
	HHI_TCON_PLL_CNTL1,
	HHI_TCON_PLL_CNTL2,
	HHI_TCON_PLL_CNTL3,
	HHI_TCON_PLL_CNTL4,
	HHI_VID_PLL_CLK_DIV,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_clk_t5[] = {
	HHI_VIID_CLK_DIV,
	HHI_VIID_CLK_CNTL,
	HHI_VID_CLK_CNTL2,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_encl_dft[] = {
	VPU_VIU_VENC_MUX_CTRL,
	ENCL_VIDEO_EN,
	ENCL_VIDEO_MODE,
	ENCL_VIDEO_MODE_ADV,
	ENCL_VIDEO_MAX_PXCNT,
	ENCL_VIDEO_MAX_LNCNT,
	ENCL_VIDEO_HAVON_BEGIN,
	ENCL_VIDEO_HAVON_END,
	ENCL_VIDEO_VAVON_BLINE,
	ENCL_VIDEO_VAVON_ELINE,
	ENCL_VIDEO_HSO_BEGIN,
	ENCL_VIDEO_HSO_END,
	ENCL_VIDEO_VSO_BEGIN,
	ENCL_VIDEO_VSO_END,
	ENCL_VIDEO_VSO_BLINE,
	ENCL_VIDEO_VSO_ELINE,
	ENCL_VIDEO_RGBIN_CTRL,
	L_GAMMA_CNTL_PORT,
	L_RGB_BASE_ADDR,
	L_RGB_COEFF_ADDR,
	L_POL_CNTL_ADDR,
	L_DITH_CNTL_ADDR,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_encl_tl1[] = {
	VPU_VIU_VENC_MUX_CTRL,
	ENCL_VIDEO_EN,
	ENCL_VIDEO_MODE,
	ENCL_VIDEO_MODE_ADV,
	ENCL_VIDEO_MAX_PXCNT,
	ENCL_VIDEO_MAX_LNCNT,
	ENCL_VIDEO_HAVON_BEGIN,
	ENCL_VIDEO_HAVON_END,
	ENCL_VIDEO_VAVON_BLINE,
	ENCL_VIDEO_VAVON_ELINE,
	ENCL_VIDEO_HSO_BEGIN,
	ENCL_VIDEO_HSO_END,
	ENCL_VIDEO_VSO_BEGIN,
	ENCL_VIDEO_VSO_END,
	ENCL_VIDEO_VSO_BLINE,
	ENCL_VIDEO_VSO_ELINE,
	ENCL_VIDEO_RGBIN_CTRL,
	ENCL_INBUF_CNTL0,
	ENCL_INBUF_CNTL1,
	L_GAMMA_CNTL_PORT,
	L_RGB_BASE_ADDR,
	L_RGB_COEFF_ADDR,
	L_POL_CNTL_ADDR,
	L_DITH_CNTL_ADDR,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pinmux_tl1[] = {
	PERIPHS_PIN_MUX_7_TL1,
	PERIPHS_PIN_MUX_8_TL1,
	PERIPHS_PIN_MUX_9_TL1,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pinmux_t5[] = {
	PERIPHS_PIN_MUX_5_TL1,
	PERIPHS_PIN_MUX_6_TL1,
	LCD_DEBUG_REG_END
};

#endif
