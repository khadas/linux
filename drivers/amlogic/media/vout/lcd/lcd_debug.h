/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_DEBUG_H__
#define __AML_LCD_DEBUG_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_reg.h"

#define LCD_DEBUG_REG_CNT_MAX    30
#define LCD_DEBUG_REG_END        0xffffffff

#define LCD_REG_DBG_VC_BUS          0
#define LCD_REG_DBG_ANA_BUS         1
#define LCD_REG_DBG_CLK_BUS         2
#define LCD_REG_DBG_PERIPHS_BUS     3
#define LCD_REG_DBG_MIPIHOST_BUS    4
#define LCD_REG_DBG_MIPIPHY_BUS     5
#define LCD_REG_DBG_TCON_BUS        6
#define LCD_REG_DBG_EDPHOST_BUS     7
#define LCD_REG_DBG_EDPDPCD_BUS     8
#define LCD_REG_DBG_COMBOPHY_BUS    9
#define LCD_REG_DBG_RST_BUS         10
#define LCD_REG_DBG_MAX_BUS         0xff

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

struct lcd_debug_info_if_s {
	int (*interface_print)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
	int (*reg_dump_interface)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
	int (*reg_dump_phy)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
	struct device_attribute *attrs;
};

struct lcd_debug_info_s {
	unsigned int *reg_pll_table;
	unsigned int *reg_clk_table;
	unsigned int *reg_clk_hiu_table;
	unsigned int *reg_encl_table;
	unsigned int *reg_pinmux_table;
	void (*prbs_test)(struct aml_lcd_drv_s *pdrv, unsigned int s,
			  unsigned int mode_flag);

	struct lcd_debug_info_if_s *debug_if_ttl;
	struct lcd_debug_info_if_s *debug_if_lvds;
	struct lcd_debug_info_if_s *debug_if_vbyone;
	struct lcd_debug_info_if_s *debug_if_mlvds;
	struct lcd_debug_info_if_s *debug_if_p2p;
	struct lcd_debug_info_if_s *debug_if_mipi;
	struct lcd_debug_info_if_s *debug_if_edp;
	struct lcd_debug_info_if_s *debug_if;
};

static unsigned int lcd_reg_dump_clk_axg[] = {
	HHI_GP0_PLL_CNTL_AXG,
	HHI_GP0_PLL_CNTL2_AXG,
	HHI_GP0_PLL_CNTL3_AXG,
	HHI_GP0_PLL_CNTL4_AXG,
	HHI_GP0_PLL_CNTL5_AXG,
	HHI_GP0_PLL_CNTL1_AXG,
	HHI_VIID_CLK_DIV,
	HHI_VIID_CLK_CNTL,
	HHI_VID_CLK_CNTL2,
	LCD_DEBUG_REG_END,
};

static unsigned int lcd_reg_dump_clk_gp0_g12a[] = {
	HHI_GP0_PLL_CNTL0,
	HHI_GP0_PLL_CNTL1,
	HHI_GP0_PLL_CNTL2,
	HHI_GP0_PLL_CNTL3,
	HHI_GP0_PLL_CNTL4,
	HHI_GP0_PLL_CNTL5,
	HHI_GP0_PLL_CNTL6,
	HHI_VIID_CLK_DIV,
	HHI_VIID_CLK_CNTL,
	HHI_VID_CLK_CNTL2,
	HHI_MIPIDSI_PHY_CLK_CNTL,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_clk_hpll_g12a[] = {
	HHI_HDMI_PLL_CNTL0,
	HHI_HDMI_PLL_CNTL1,
	HHI_HDMI_PLL_CNTL2,
	HHI_HDMI_PLL_CNTL3,
	HHI_HDMI_PLL_CNTL4,
	HHI_HDMI_PLL_CNTL5,
	HHI_HDMI_PLL_CNTL6,
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

static unsigned int lcd_reg_dump_pll_t5[] = {
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

static unsigned int lcd_reg_dump_clk_t5w[] = {
	HHI_VIID_CLK0_DIV,
	HHI_VIID_CLK0_CTRL,
	HHI_VID_CLK0_CTRL2,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pll_t7_0[] = {
	ANACTRL_TCON_PLL0_CNTL0,
	ANACTRL_TCON_PLL0_CNTL1,
	ANACTRL_TCON_PLL0_CNTL2,
	ANACTRL_TCON_PLL0_CNTL3,
	ANACTRL_TCON_PLL0_CNTL4,
	ANACTRL_TCON_PLL0_STS,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pll_t7_1[] = {
	ANACTRL_TCON_PLL1_CNTL0,
	ANACTRL_TCON_PLL1_CNTL1,
	ANACTRL_TCON_PLL1_CNTL2,
	ANACTRL_TCON_PLL1_CNTL3,
	ANACTRL_TCON_PLL1_CNTL4,
	ANACTRL_TCON_PLL1_STS,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pll_t7_2[] = {
	ANACTRL_TCON_PLL2_CNTL0,
	ANACTRL_TCON_PLL2_CNTL1,
	ANACTRL_TCON_PLL2_CNTL2,
	ANACTRL_TCON_PLL2_CNTL3,
	ANACTRL_TCON_PLL2_CNTL4,
	ANACTRL_TCON_PLL2_STS,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_clk_t7_0[] = {
	CLKCTRL_VIID_CLK0_DIV,
	CLKCTRL_VIID_CLK0_CTRL,
	CLKCTRL_VID_CLK0_CTRL2,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_clk_t7_1[] = {
	CLKCTRL_VIID_CLK1_DIV,
	CLKCTRL_VIID_CLK1_CTRL,
	CLKCTRL_VID_CLK1_CTRL2,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_clk_t7_2[] = {
	CLKCTRL_VIID_CLK2_DIV,
	CLKCTRL_VIID_CLK2_CTRL,
	CLKCTRL_VID_CLK2_CTRL2,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pll_t3_0[] = {
	ANACTRL_TCON_PLL0_CNTL0,
	ANACTRL_TCON_PLL0_CNTL1,
	ANACTRL_TCON_PLL0_CNTL2,
	ANACTRL_TCON_PLL0_CNTL3,
	ANACTRL_TCON_PLL0_CNTL4,
	ANACTRL_TCON_PLL0_STS,
	ANACTRL_VID_PLL_CLK_DIV,
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

static unsigned int lcd_reg_dump_encl_t7_0[] = {
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
	VPU_DISP_VIU0_CTRL,
	VPU_VENC_CTRL,
	LCD_GAMMA_CNTL_PORT0,
	LCD_RGB_BASE_ADDR,
	LCD_RGB_COEFF_ADDR,
	LCD_POL_CNTL_ADDR,
	LCD_DITH_CNTL_ADDR,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_encl_t7_1[] = {
	VPU_VIU_VENC_MUX_CTRL,
	ENCL_VIDEO_EN + 0x600,
	ENCL_VIDEO_MODE + 0x600,
	ENCL_VIDEO_MODE_ADV + 0x600,
	ENCL_VIDEO_MAX_PXCNT + 0x600,
	ENCL_VIDEO_MAX_LNCNT + 0x600,
	ENCL_VIDEO_HAVON_BEGIN + 0x600,
	ENCL_VIDEO_HAVON_END + 0x600,
	ENCL_VIDEO_VAVON_BLINE + 0x600,
	ENCL_VIDEO_VAVON_ELINE + 0x600,
	ENCL_VIDEO_HSO_BEGIN + 0x600,
	ENCL_VIDEO_HSO_END + 0x600,
	ENCL_VIDEO_VSO_BEGIN + 0x600,
	ENCL_VIDEO_VSO_END + 0x600,
	ENCL_VIDEO_VSO_BLINE + 0x600,
	ENCL_VIDEO_VSO_ELINE + 0x600,
	ENCL_VIDEO_RGBIN_CTRL + 0x600,
	ENCL_INBUF_CNTL0 + 0x600,
	ENCL_INBUF_CNTL1 + 0x600,
	VPU_DISP_VIU1_CTRL,
	VPU_VENC_CTRL + 0x600,
	LCD_GAMMA_CNTL_PORT0 + 0x100,
	LCD_RGB_BASE_ADDR + 0x100,
	LCD_RGB_COEFF_ADDR + 0x100,
	LCD_POL_CNTL_ADDR + 0x100,
	LCD_DITH_CNTL_ADDR + 0x100,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_encl_t7_2[] = {
	VPU_VIU_VENC_MUX_CTRL,
	ENCL_VIDEO_EN + 0x800,
	ENCL_VIDEO_MODE + 0x800,
	ENCL_VIDEO_MODE_ADV + 0x800,
	ENCL_VIDEO_MAX_PXCNT + 0x800,
	ENCL_VIDEO_MAX_LNCNT + 0x800,
	ENCL_VIDEO_HAVON_BEGIN + 0x800,
	ENCL_VIDEO_HAVON_END + 0x800,
	ENCL_VIDEO_VAVON_BLINE + 0x800,
	ENCL_VIDEO_VAVON_ELINE + 0x800,
	ENCL_VIDEO_HSO_BEGIN + 0x800,
	ENCL_VIDEO_HSO_END + 0x800,
	ENCL_VIDEO_VSO_BEGIN + 0x800,
	ENCL_VIDEO_VSO_END + 0x800,
	ENCL_VIDEO_VSO_BLINE + 0x800,
	ENCL_VIDEO_VSO_ELINE + 0x800,
	ENCL_VIDEO_RGBIN_CTRL + 0x800,
	ENCL_INBUF_CNTL0 + 0x800,
	ENCL_INBUF_CNTL1 + 0x800,
	VPU_DISP_VIU2_CTRL,
	VPU_VENC_CTRL + 0x800,
	LCD_GAMMA_CNTL_PORT0 + 0x200,
	LCD_RGB_BASE_ADDR + 0x200,
	LCD_RGB_COEFF_ADDR + 0x200,
	LCD_POL_CNTL_ADDR + 0x200,
	LCD_DITH_CNTL_ADDR + 0x200,
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

static unsigned int lcd_reg_dump_pinmux_t7[] = {
	PADCTRL_PIN_MUX_REGK,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pinmux_t3[] = {
	PADCTRL_PIN_MUX_REG7,
	PADCTRL_PIN_MUX_REG8,
	LCD_DEBUG_REG_END
};

#endif
