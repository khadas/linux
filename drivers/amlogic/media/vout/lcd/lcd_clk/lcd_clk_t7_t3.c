// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/clk.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "lcd_clk_config.h"
#include "lcd_clk_ctrl.h"
#include "lcd_clk_utils.h"

static void lcd_pll_ss_init(struct lcd_clk_config_s *cconf)
{
	int ret;

	if (!cconf)
		return;

	if (cconf->ss_level > 0) {
		ret = lcd_pll_ss_level_generate(cconf);
		if (ret == 0)
			cconf->ss_en = 1;
	} else {
		cconf->ss_en = 0;
	}
}

static void lcd_pll_ss_enable(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2, offset;
	unsigned int flag;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	offset = cconf->pll_offset;
	pll_ctrl2 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2 + offset);
	pll_ctrl2 &= ~((0xf << 16) | (0xf << 28));

	if (status) {
		if (cconf->ss_level > 0)
			flag = 1;
		else
			flag = 0;
	} else {
		flag = 0;
	}

	if (flag) {
		cconf->ss_en = 1;
		pll_ctrl2 |= ((cconf->ss_dep_sel << 28) | (cconf->ss_str_m << 16));
		LCDPR("[%d]: pll ss enable: level %d, %dppm\n",
			pdrv->index, cconf->ss_level, cconf->ss_ppm);
	} else {
		cconf->ss_en = 0;
		LCDPR("[%d]: pll ss disable\n", pdrv->index);
	}
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, pll_ctrl2);
}

static void lcd_set_pll_ss_level(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2, offset;
	int ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	offset = cconf->pll_offset;
	pll_ctrl2 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2 + offset);
	pll_ctrl2 &= ~((0xf << 16) | (0xf << 28));

	if (cconf->ss_level > 0) {
		ret = lcd_pll_ss_level_generate(cconf);
		if (ret == 0) {
			cconf->ss_en = 1;
			pll_ctrl2 |= ((cconf->ss_dep_sel << 28) | (cconf->ss_str_m << 16));
			LCDPR("[%d]: set pll spread spectrum: level %d, %dppm\n",
				pdrv->index, cconf->ss_level, cconf->ss_ppm);
		}
	} else {
		cconf->ss_en = 0;
		LCDPR("[%d]: set pll spread spectrum: disable\n", pdrv->index);
	}
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, pll_ctrl2);
}

static void lcd_set_pll_ss_advance(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2, offset;
	unsigned int freq, mode;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	freq = cconf->ss_freq;
	mode = cconf->ss_mode;
	offset = cconf->pll_offset;
	pll_ctrl2 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2 + offset);
	pll_ctrl2 &= ~(0x7 << 24); /* ss_freq */
	pll_ctrl2 |= (freq << 24);
	pll_ctrl2 &= ~(0x3 << 22); /* ss_mode */
	pll_ctrl2 |= (mode << 22);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, pll_ctrl2);

	LCDPR("[%d]: set pll spread spectrum: freq=%d, mode=%d\n",
	      pdrv->index, freq, mode);
}

static void lcd_set_pll_t7(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl, pll_ctrl1, pll_stts, offset;
	unsigned int tcon_div_sel;
	int ret, cnt = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	tcon_div_sel = cconf->pll_tcon_div_sel;
	pll_ctrl = ((0x3 << 17) | /* gate ctrl */
		(tcon_div[tcon_div_sel][2] << 16) |
		(cconf->pll_n << LCD_PLL_N_TL1) |
		(cconf->pll_m << LCD_PLL_M_TL1) |
		(cconf->pll_od3_sel << LCD_PLL_OD3_T7) |
		(cconf->pll_od2_sel << LCD_PLL_OD2_T7) |
		(cconf->pll_od1_sel << LCD_PLL_OD1_T7));
	pll_ctrl1 = (1 << 28) |
		(tcon_div[tcon_div_sel][0] << 22) |
		(tcon_div[tcon_div_sel][1] << 21) |
		((1 << 20) | /* sdm_en */
		(cconf->pll_frac << 0));

	offset = cconf->pll_offset;
	switch (cconf->pll_id) {
	case 1:
		pll_stts = ANACTRL_TCON_PLL1_STS;
		break;
	case 2:
		pll_stts = ANACTRL_TCON_PLL2_STS;
		break;
	case 0:
	default:
		pll_stts = ANACTRL_TCON_PLL0_STS;
		break;
	}

set_pll_retry_t7:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0 + offset, pll_ctrl);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 1, LCD_PLL_RST_TL1, 1);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 1, LCD_PLL_EN_TL1, 1);
	udelay(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1 + offset, pll_ctrl1);
	udelay(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, 0x0000110c);
	udelay(10);
	if (cconf->pll_fvco < 3800000000ULL)
		lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3 + offset, 0x10051100);
	else
		lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3 + offset, 0x10051400);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL4 + offset, 0x0100c0, 0, 24);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL4 + offset, 0x8300c0, 0, 24);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 1, 26, 1);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 0, LCD_PLL_RST_TL1, 1);
	udelay(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, 0x0000300c);

	ret = lcd_pll_wait_lock(pll_stts, LCD_PLL_LOCK_T7);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_pll_retry_t7;
		LCDERR("[%d]: pll lock failed\n", pdrv->index);
	} else {
		udelay(100);
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL2 + offset, 1, 5, 1);
	}

	if (cconf->ss_level > 0) {
		lcd_set_pll_ss_level(pdrv);
		lcd_set_pll_ss_advance(pdrv);
	}
}

static void lcd_prbs_set_pll_vx1_t7(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_stts, offset;
	unsigned int reg_vid_pll_div, reg_vid2_clk_ctrl;
	int cnt = 0, ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	offset = cconf->pll_offset;
	switch (cconf->pll_id) {
	case 1:
		pll_stts = ANACTRL_TCON_PLL1_STS;
		reg_vid_pll_div = COMBO_DPHY_VID_PLL1_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK1_CTRL;
		break;
	case 2:
		pll_stts = ANACTRL_TCON_PLL2_STS;
		reg_vid_pll_div = COMBO_DPHY_VID_PLL2_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK2_CTRL;
		break;
	case 0:
	default:
		pll_stts = ANACTRL_TCON_PLL0_STS;
		reg_vid_pll_div = COMBO_DPHY_VID_PLL0_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
		break;
	}

lcd_prbs_retry_pll_vx1_t7:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0 + offset, 0x000f04f7);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 1, LCD_PLL_RST_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 1, LCD_PLL_EN_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1 + offset, 0x10110000);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, 0x00001108);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3 + offset, 0x10051400);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4 + offset, 0x010100c0);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4 + offset, 0x038300c0);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 1, 26, 1);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 0, LCD_PLL_RST_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, 0x00003008);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, 0x00003028);
	usleep_range(10, 12);

	ret = lcd_pll_wait_lock(pll_stts, LCD_PLL_LOCK_T7);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto lcd_prbs_retry_pll_vx1_t7;
		LCDERR("[%d]: pll lock failed\n", pdrv->index);
	}

	/* pll_div */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* Disable the div output clock */
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 19, 1);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);

	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 18, 1);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 16, 2);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 0, 14);

	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 2, 16, 2);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 15, 1);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0x739c, 0, 15);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);

	/* Enable the final output clock */
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 19, 1);
}

static void lcd_prbs_set_pll_lvds_t7(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_stts, offset;
	unsigned int reg_vid_pll_div, reg_vid2_clk_ctrl;
	int cnt = 0, ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	offset = cconf->pll_offset;
	switch (cconf->pll_id) {
	case 1:
		pll_stts = ANACTRL_TCON_PLL1_STS;
		reg_vid_pll_div = COMBO_DPHY_VID_PLL1_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK1_CTRL;
		break;
	case 2:
		pll_stts = ANACTRL_TCON_PLL2_STS;
		reg_vid_pll_div = COMBO_DPHY_VID_PLL2_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK2_CTRL;
		break;
	case 0:
	default:
		pll_stts = ANACTRL_TCON_PLL0_STS;
		reg_vid_pll_div = COMBO_DPHY_VID_PLL0_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
		break;
	}

lcd_prbs_retry_pll_lvds_t7:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0 + offset, 0x008e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0 + offset, 0x208e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0 + offset, 0x3006049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1 + offset, 0x10000000);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, 0x00001102);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3 + offset, 0x10051400);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4 + offset, 0x010100c0);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4 + offset, 0x038300c0);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0 + offset, 0x348e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0 + offset, 0x148e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, 0x00003002);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, 0x00003022);
	usleep_range(10, 12);

	ret = lcd_pll_wait_lock(pll_stts, LCD_PLL_LOCK_T7);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto lcd_prbs_retry_pll_lvds_t7;
		LCDERR("[%d]: pll lock failed\n", pdrv->index);
	}

	/* pll_div */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* Disable the div output clock */
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 19, 1);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);

	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 18, 1);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 16, 2);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 0, 14);

	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 16, 2);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 15, 1);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0x3c78, 0, 15);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);

	/* Enable the final output clock */
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 19, 1);
}

static void lcd_prbs_config_clk_t7(struct aml_lcd_drv_s *pdrv, unsigned int lcd_prbs_mode)
{
	struct lcd_clk_config_s *cconf;
	unsigned int reg_vid2_clk_div, reg_vid2_clk_ctrl, reg_vid_clk_ctrl2;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	switch (cconf->pll_id) {
	case 1:
		reg_vid2_clk_div = CLKCTRL_VIID_CLK1_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK1_CTRL;
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK1_CTRL2;
		break;
	case 2:
		reg_vid2_clk_div = CLKCTRL_VIID_CLK2_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK2_CTRL;
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK2_CTRL2;
		break;
	case 0:
	default:
		reg_vid2_clk_div = CLKCTRL_VIID_CLK0_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK0_CTRL2;
		break;
	}

	if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
		lcd_prbs_set_pll_vx1_t7(pdrv);
	} else if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
		lcd_prbs_set_pll_lvds_t7(pdrv);
	} else {
		LCDERR("[%d]: %s: unsupport lcd_prbs_mode %d\n",
		       pdrv->index, __func__, lcd_prbs_mode);
		return;
	}

	lcd_clk_setb(reg_vid2_clk_div, 0, VCLK2_XD, 8);
	usleep_range(5, 10);

	/* select vid_pll_clk */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_CLK_IN_SEL, 3);
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(reg_vid2_clk_div, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(reg_vid2_clk_div, 1, VCLK2_XD_EN, 2);
	usleep_range(5, 10);

	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_SOFT_RST, 1);
	usleep_range(10, 12);
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_SOFT_RST, 1);
	usleep_range(5, 10);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(reg_vid_clk_ctrl2, 1, ENCL_GATE_VCLK, 1);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s ok\n", pdrv->index, __func__);
}

static void lcd_set_phy_dig_div_t7(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int reg_edp_clk_div, reg_dphy_tx_ctrl1;
	unsigned int port_sel, bit_div_en, bit_div0, bit_div1, bit_rst;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	reg_edp_clk_div = COMBO_DPHY_EDP_PIXEL_CLK_DIV;
	switch (cconf->pll_id) {
	case 1:
		reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
		port_sel = 1;
		bit_div_en = 25;
		bit_div0 = 8;
		bit_div1 = 12;
		bit_rst = 20;
		break;
	case 2:
		reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1;
		port_sel = 2;
		bit_div_en = 26;
		bit_div0 = 0;
		bit_div1 = 4;
		bit_rst = 7;
		break;
	case 0:
	default:
		reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
		port_sel = 0;
		bit_div_en = 24;
		bit_div0 = 0;
		bit_div1 = 4;
		bit_rst = 19;
		break;
	}

	lcd_reset_setb(pdrv, RESETCTRL_RESET1_MASK, 0, bit_rst, 1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 0, bit_rst, 1);
	udelay(1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 1, bit_rst, 1);
	udelay(10);

	// Enable dphy clock
	lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 0, 1);

	switch (pdrv->config.basic.lcd_type) {
	case LCD_EDP:
		if (port_sel == 2) {
			LCDERR("[%d]: %s: invalid port: %d\n",
			       pdrv->index, __func__, port_sel);
			return;
		}
		// Disable edp_div clock
		lcd_combo_dphy_setb(pdrv, reg_edp_clk_div, 0, bit_div_en, 1);
		lcd_combo_dphy_setb(pdrv, reg_edp_clk_div, cconf->edp_div0, bit_div0, 4);
		lcd_combo_dphy_setb(pdrv, reg_edp_clk_div, cconf->edp_div1, bit_div1, 4);
		// Enable edp_div clock
		lcd_combo_dphy_setb(pdrv, reg_edp_clk_div, 1, bit_div_en, 1);
		// sel edp_div clock
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 4, 1);
		break;
	case LCD_MIPI:
	case LCD_VBYONE:
		if (port_sel == 2) {
			LCDERR("[%d]: %s: invalid port: %d\n",
			       pdrv->index, __func__, port_sel);
			return;
		}
		// sel pll clock
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 4, 1);
		break;
	default:
		// sel pll clock
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 4, 1);
		break;
	}

	// sel tcon_pll clock
	lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 5, 1);
}

static void lcd_set_vid_pll_div_t7(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int reg_vid_pll_div, reg_vid2_clk_ctrl;
	unsigned int shift_val, shift_sel;
	int i;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	switch (cconf->pll_id) {
	case 1:
		reg_vid_pll_div = COMBO_DPHY_VID_PLL1_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK1_CTRL;
		break;
	case 2:
		reg_vid_pll_div = COMBO_DPHY_VID_PLL2_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK2_CTRL;
		break;
	case 0:
	default:
		reg_vid_pll_div = COMBO_DPHY_VID_PLL0_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
		break;
	}

	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);
	udelay(5);

	/* Disable the div output clock */
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 19, 1);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);

	i = 0;
	while (lcd_clk_div_table[i][0] != CLK_DIV_SEL_MAX) {
		if (cconf->div_sel == lcd_clk_div_table[i][0])
			break;
		i++;
	}
	if (lcd_clk_div_table[i][0] == CLK_DIV_SEL_MAX)
		LCDERR("[%d]: invalid clk divider\n", pdrv->index);
	shift_val = lcd_clk_div_table[i][1];
	shift_sel = lcd_clk_div_table[i][2];

	if (shift_val == 0xffff) { /* if divide by 1 */
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 18, 1);
	} else {
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 18, 1);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 16, 2);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 0, 14);

		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, shift_sel, 16, 2);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 15, 1);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, shift_val, 0, 15);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);
	}
	/* Enable the final output clock */
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 19, 1);
}

static void lcd_set_vclk_crt(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int reg_vid2_clk_div, reg_vid2_clk_ctrl, reg_vid_clk_ctrl2;
	unsigned int venc_clk_sel_bit = 0xff;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	switch (cconf->pll_id) {
	case 1:
		reg_vid2_clk_div = CLKCTRL_VIID_CLK1_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK1_CTRL;
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK1_CTRL2;
		break;
	case 2:
		reg_vid2_clk_div = CLKCTRL_VIID_CLK2_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK2_CTRL;
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK2_CTRL2;
		venc_clk_sel_bit = 25;
		break;
	case 0:
	default:
		reg_vid2_clk_div = CLKCTRL_VIID_CLK0_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK0_CTRL2;
		venc_clk_sel_bit = 24;
		break;
	}

	lcd_clk_write(reg_vid_clk_ctrl2, 0);
	lcd_clk_write(reg_vid2_clk_ctrl, 0);
	lcd_clk_write(reg_vid2_clk_div, 0);
	udelay(5);

	if (pdrv->lcd_pxp) {
		/* setup the XD divider value */
		lcd_clk_setb(reg_vid2_clk_div, cconf->xd, VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(reg_vid2_clk_ctrl, 7, VCLK2_CLK_IN_SEL, 3);
	} else {
		if (venc_clk_sel_bit < 0xff)
			lcd_clk_setb(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 0, venc_clk_sel_bit, 1);

		/* setup the XD divider value */
		lcd_clk_setb(reg_vid2_clk_div, (cconf->xd - 1), VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(reg_vid2_clk_ctrl, cconf->data->vclk_sel, VCLK2_CLK_IN_SEL, 3);
	}
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_EN, 1);
	udelay(2);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(reg_vid2_clk_div, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(reg_vid2_clk_div, 1, VCLK2_XD_EN, 2);
	udelay(5);

	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_SOFT_RST, 1);
	udelay(10);
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(reg_vid_clk_ctrl2, 1, ENCL_GATE_VCLK, 1);
}

static void lcd_set_dsi_meas_clk_t7(int index)
{
	if (index) {
		lcd_clk_setb(CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL, 7, 12, 7);
		lcd_clk_setb(CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL, 0, 21, 3);
		lcd_clk_setb(CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL, 1, 20, 1);
	} else {
		lcd_clk_setb(CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL, 7, 0, 7);
		lcd_clk_setb(CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL, 0, 9, 3);
		lcd_clk_setb(CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL, 1, 8, 1);
	}
}

static void lcd_set_dsi_phy_clk_t7(int index)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", index, __func__);
	if (index) {
		lcd_clk_setb(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, 0, 16, 7);
		lcd_clk_setb(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, 0, 25, 3);
		lcd_clk_setb(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, 1, 24, 1);
	} else {
		lcd_clk_setb(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, 0, 0, 7);
		lcd_clk_setb(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, 0, 12, 3);
		lcd_clk_setb(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, 1, 8, 1);
	}
}

static void lcd_clk_set_t7(struct aml_lcd_drv_s *pdrv)
{
	lcd_set_pll_t7(pdrv);
	lcd_set_phy_dig_div_t7(pdrv);
	lcd_set_vid_pll_div_t7(pdrv);

	if (pdrv->config.basic.lcd_type == LCD_MIPI) {
		lcd_set_dsi_meas_clk_t7(pdrv->index);
		lcd_set_dsi_phy_clk_t7(pdrv->index);
	}
}

static void lcd_clk_disable(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	unsigned int reg_vid_clk_ctrl2, reg_vid2_clk_ctrl, offset;
	int i = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	switch (cconf->pll_id) {
	case 1:
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK1_CTRL2;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK1_CTRL;
		break;
	case 2:
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK2_CTRL2;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK2_CTRL;
		break;
	case 0:
	default:
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK0_CTRL2;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
		break;
	}
	offset = cconf->pll_offset;

	lcd_clk_setb(reg_vid_clk_ctrl2, 0, ENCL_GATE_VCLK, 1);

	/* close vclk2_div gate: [4:0] */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, 0, 5);
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);

	if (!cconf->data->pll_ctrl_table)
		return;
	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_END)
			break;
		if (table[i].flag == LCD_CLK_CTRL_EN)
			lcd_ana_setb(table[i].reg + offset, 0, table[i].bit, table[i].len);
		else if (table[i].flag == LCD_CLK_CTRL_RST)
			lcd_ana_setb(table[i].reg + offset, 1, table[i].bit, table[i].len);
		i++;
	}
}

static void lcd_clk_gate_switch_t7(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (status) {
		switch (pdrv->config.basic.lcd_type) {
		case LCD_MIPI:
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_host_gate))
				LCDERR("%s: dsi_host_gate\n", __func__);
			else
				clk_prepare_enable(cconf->clktree.dsi_host_gate);
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_phy_gate))
				LCDERR("%s: dsi_phy_gate\n", __func__);
			else
				clk_prepare_enable(cconf->clktree.dsi_phy_gate);
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_meas))
				LCDERR("%s: dsi_meas\n", __func__);
			else
				clk_prepare_enable(cconf->clktree.dsi_meas);
			break;
		default:
			break;
		}
	} else {
		switch (pdrv->config.basic.lcd_type) {
		case LCD_MIPI:
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_host_gate))
				LCDERR("%s: dsi_host_gate\n", __func__);
			else
				clk_disable_unprepare(cconf->clktree.dsi_host_gate);
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_phy_gate))
				LCDERR("%s: dsi_phy_gate\n", __func__);
			else
				clk_disable_unprepare(cconf->clktree.dsi_phy_gate);
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_meas))
				LCDERR("%s: dsi_meas\n", __func__);
			else
				clk_disable_unprepare(cconf->clktree.dsi_meas);
			break;
		default:
			break;
		}
	}
}

static void lcd_clktree_probe_t7(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	cconf->clktree.clk_gate_state = 0;

	cconf->clktree.dsi_host_gate = devm_clk_get(pdrv->dev, "dsi_host_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.dsi_host_gate))
		LCDERR("%s: clk dsi_host_gate\n", __func__);

	cconf->clktree.dsi_phy_gate = devm_clk_get(pdrv->dev, "dsi_phy_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.dsi_phy_gate))
		LCDERR("%s: clk dsi_phy_gate\n", __func__);

	cconf->clktree.dsi_meas = devm_clk_get(pdrv->dev, "dsi_meas");
	if (IS_ERR_OR_NULL(cconf->clktree.dsi_meas))
		LCDERR("%s: clk dsi_meas\n", __func__);

	LCDPR("lcd_clktree_probe\n");
}

static void lcd_clktree_remove_t7(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd_clktree_remove\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (!IS_ERR_OR_NULL(cconf->clktree.dsi_host_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.dsi_host_gate);
	if (!IS_ERR_OR_NULL(cconf->clktree.dsi_phy_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.dsi_phy_gate);
	if (!IS_ERR_OR_NULL(cconf->clktree.dsi_meas))
		devm_clk_put(pdrv->dev, cconf->clktree.dsi_meas);
}

static void lcd_set_pll_t3(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl, pll_ctrl1, pll_stts, offset;
	unsigned int tcon_div_sel;
	int ret, cnt = 0;

	if (pdrv->index) /* clk_path1 invalid */
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	tcon_div_sel = cconf->pll_tcon_div_sel;
	pll_ctrl = ((0x3 << 17) | /* gate ctrl */
		(tcon_div[tcon_div_sel][2] << 16) |
		(cconf->pll_n << LCD_PLL_N_TL1) |
		(cconf->pll_m << LCD_PLL_M_TL1) |
		(cconf->pll_od3_sel << LCD_PLL_OD3_T7) |
		(cconf->pll_od2_sel << LCD_PLL_OD2_T7) |
		(cconf->pll_od1_sel << LCD_PLL_OD1_T7));
	pll_ctrl1 = (1 << 28) |
		(tcon_div[tcon_div_sel][0] << 22) |
		(tcon_div[tcon_div_sel][1] << 21) |
		((1 << 20) | /* sdm_en */
		(cconf->pll_frac << 0));

	offset = cconf->pll_offset;
	pll_stts = ANACTRL_TCON_PLL0_STS;

set_pll_retry_t3:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0 + offset, pll_ctrl);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 1, LCD_PLL_RST_TL1, 1);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 1, LCD_PLL_EN_TL1, 1);
	udelay(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1 + offset, pll_ctrl1);
	udelay(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, 0x0000110c);
	udelay(10);
	if (cconf->pll_fvco < 3800000000ULL)
		lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3 + offset, 0x10051100);
	else
		lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3 + offset, 0x10051400);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL4 + offset, 0x0100c0, 0, 24);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL4 + offset, 0x8300c0, 0, 24);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 1, 26, 1);
	udelay(10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0 + offset, 0, LCD_PLL_RST_TL1, 1);
	udelay(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, 0x0000300c);

	ret = lcd_pll_wait_lock(pll_stts, LCD_PLL_LOCK_T7);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_pll_retry_t3;
		LCDERR("[%d]: pll lock failed\n", pdrv->index);
	} else {
		udelay(100);
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL2 + offset, 1, 5, 1);
	}

	if (cconf->ss_level > 0) {
		lcd_set_pll_ss_level(pdrv);
		lcd_set_pll_ss_advance(pdrv);
	}
}

static void lcd_prbs_set_pll_vx1_t3(struct aml_lcd_drv_s *pdrv)
{
	unsigned int pll_stts;
	unsigned int reg_vid_pll_div, reg_vid2_clk_ctrl;
	int cnt = 0, ret;

	pll_stts = ANACTRL_TCON_PLL0_STS;
	reg_vid_pll_div = COMBO_DPHY_VID_PLL0_DIV;
	reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;

lcd_prbs_retry_pll_vx1_t3:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x000f04f7);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, LCD_PLL_RST_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, LCD_PLL_EN_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1, 0x10110000);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00001108);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3, 0x10051400);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x010100c0);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x038300c0);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, 26, 1);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 0, LCD_PLL_RST_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00003008);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00003028);
	usleep_range(10, 12);

	ret = lcd_pll_wait_lock(pll_stts, LCD_PLL_LOCK_T7);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto lcd_prbs_retry_pll_vx1_t3;
		LCDERR("pll lock failed\n");
	}

	/* pll_div */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* Disable the div output clock */
	lcd_ana_setb(reg_vid_pll_div, 0, 19, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);

	lcd_ana_setb(reg_vid_pll_div, 0, 18, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 16, 2);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 0, 14);

	lcd_ana_setb(reg_vid_pll_div, 2, 16, 2);
	lcd_ana_setb(reg_vid_pll_div, 1, 15, 1);
	lcd_ana_setb(reg_vid_pll_div, 0x739c, 0, 15);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);

	/* Enable the final output clock */
	lcd_ana_setb(reg_vid_pll_div, 1, 19, 1);
}

static void lcd_prbs_set_pll_lvds_t3(struct aml_lcd_drv_s *pdrv)
{
	unsigned int pll_stts;
	unsigned int reg_vid_pll_div, reg_vid2_clk_ctrl;
	int cnt = 0, ret;

	pll_stts = ANACTRL_TCON_PLL0_STS;
	reg_vid_pll_div = COMBO_DPHY_VID_PLL0_DIV;
	reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;

lcd_prbs_retry_pll_lvds_t3:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x008e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x208e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x3006049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1, 0x10000000);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00001102);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3, 0x10051400);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x010100c0);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x038300c0);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x348e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x148e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00003002);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00003022);
	usleep_range(10, 12);

	ret = lcd_pll_wait_lock(pll_stts, LCD_PLL_LOCK_T7);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto lcd_prbs_retry_pll_lvds_t3;
		LCDERR("[%d]: pll lock failed\n", pdrv->index);
	}

	/* pll_div */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* Disable the div output clock */
	lcd_ana_setb(reg_vid_pll_div, 0, 19, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);

	lcd_ana_setb(reg_vid_pll_div, 0, 18, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 16, 2);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 0, 14);

	lcd_ana_setb(reg_vid_pll_div, 1, 16, 2);
	lcd_ana_setb(reg_vid_pll_div, 1, 15, 1);
	lcd_ana_setb(reg_vid_pll_div, 0x3c78, 0, 15);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);

	/* Enable the final output clock */
	lcd_ana_setb(reg_vid_pll_div, 1, 19, 1);
}

static void lcd_prbs_config_clk_t3(struct aml_lcd_drv_s *pdrv, unsigned int lcd_prbs_mode)
{
	unsigned int reg_vid2_clk_div, reg_vid2_clk_ctrl, reg_vid_clk_ctrl2;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	reg_vid2_clk_div = CLKCTRL_VIID_CLK0_DIV;
	reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
	reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK0_CTRL2;

	if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
		lcd_prbs_set_pll_vx1_t3(pdrv);
	} else if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
		lcd_prbs_set_pll_lvds_t3(pdrv);
	} else {
		LCDERR("[%d]: %s: unsupport lcd_prbs_mode %d\n",
		       pdrv->index, __func__, lcd_prbs_mode);
		return;
	}

	lcd_clk_setb(reg_vid2_clk_div, 0, VCLK2_XD, 8);
	usleep_range(5, 10);

	/* select vid_pll_clk */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_CLK_IN_SEL, 3);
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(reg_vid2_clk_div, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(reg_vid2_clk_div, 1, VCLK2_XD_EN, 2);
	usleep_range(5, 10);

	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_SOFT_RST, 1);
	usleep_range(10, 12);
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_SOFT_RST, 1);
	usleep_range(5, 10);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(reg_vid_clk_ctrl2, 1, ENCL_GATE_VCLK, 1);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s ok\n", pdrv->index, __func__);
}

static void lcd_set_vid_pll_div_t3(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int shift_val, shift_sel;
	int i;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	if (cconf->pll_id) {
		/* only crt_video valid for clk path1 */
		lcd_clk_setb(CLKCTRL_VIID_CLK1_CTRL, 0, VCLK2_EN, 1);
		udelay(5);
		return;
	}

	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, VCLK2_EN, 1);
	udelay(5);

	/* Disable the div output clock */
	lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 19, 1);
	lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 15, 1);

	i = 0;
	while (lcd_clk_div_table[i][0] != CLK_DIV_SEL_MAX) {
		if (cconf->div_sel == lcd_clk_div_table[i][0])
			break;
		i++;
	}
	if (lcd_clk_div_table[i][0] == CLK_DIV_SEL_MAX)
		LCDERR("[%d]: invalid clk divider\n", pdrv->index);
	shift_val = lcd_clk_div_table[i][1];
	shift_sel = lcd_clk_div_table[i][2];

	if (shift_val == 0xffff) { /* if divide by 1 */
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 1, 18, 1);
	} else {
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 18, 1);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 20, 1);/*div8_25*/
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 16, 2);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 15, 1);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 0, 15);

		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, shift_sel, 16, 2);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 1, 15, 1);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, shift_val, 0, 15);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 15, 1);
	}
	/* Enable the final output clock */
	lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 1, 19, 1);
}

static void lcd_set_tcon_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int freq, val;

	if (pdrv->index > 0) /* tcon_clk only valid for lcd0 */
		return;

	if (pdrv->config.basic.lcd_type != LCD_MLVDS &&
	    pdrv->config.basic.lcd_type != LCD_P2P)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	switch (pconf->basic.lcd_type) {
	case LCD_MLVDS:
		val = pconf->control.mlvds_cfg.clk_phase & 0xfff;
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL1, (val & 0xf), 24, 4);
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL4, ((val >> 4) & 0xf), 28, 4);
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL4, ((val >> 8) & 0xf), 24, 4);

		/* tcon_clk */
		if (pconf->timing.lcd_clk >= 100000000) /* 25M */
			freq = 25000000;
		else /* 12.5M */
			freq = 12500000;
		if (!IS_ERR_OR_NULL(cconf->clktree.tcon_clk)) {
			clk_set_rate(cconf->clktree.tcon_clk, freq);
			clk_prepare_enable(cconf->clktree.tcon_clk);
		}
		break;
	case LCD_P2P:
		if (!IS_ERR_OR_NULL(cconf->clktree.tcon_clk)) {
			clk_set_rate(cconf->clktree.tcon_clk, 50000000);
			clk_prepare_enable(cconf->clktree.tcon_clk);
		}
		break;
	default:
		break;
	}

	lcd_tcon_global_reset(pdrv);
}

static void lcd_clk_set_t3(struct aml_lcd_drv_s *pdrv)
{
	lcd_set_pll_t3(pdrv);
	lcd_set_vid_pll_div_t3(pdrv);
}

static void lcd_clktree_probe_t3(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct clk *temp_clk;
	int ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	if (pdrv->index > 0) /* tcon_clk invalid for lcd1 */
		return;

	cconf->clktree.clk_gate_state = 0;

	cconf->clktree.tcon_gate = devm_clk_get(pdrv->dev, "tcon_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.tcon_gate))
		LCDERR("%s: get tcon_gate error\n", __func__);

	temp_clk = devm_clk_get(pdrv->dev, "fclk_div5");
	if (IS_ERR_OR_NULL(temp_clk)) {
		LCDERR("%s: clk fclk_div5\n", __func__);
		return;
	}
	cconf->clktree.tcon_clk = devm_clk_get(pdrv->dev, "clk_tcon");
	if (IS_ERR_OR_NULL(cconf->clktree.tcon_clk)) {
		LCDERR("%s: clk clk_tcon\n", __func__);
	} else {
		ret = clk_set_parent(cconf->clktree.tcon_clk, temp_clk);
		if (ret)
			LCDERR("%s: clk clk_tcon set_parent error\n", __func__);
	}

	LCDPR("lcd_clktree_probe\n");
}

static void lcd_clktree_remove_t3(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd_clktree_remove\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	if (pdrv->index > 0) /* tcon_clk invalid for lcd1 */
		return;

	if (!IS_ERR_OR_NULL(cconf->clktree.tcon_clk))
		devm_clk_put(pdrv->dev, cconf->clktree.tcon_clk);
	if (IS_ERR_OR_NULL(cconf->clktree.tcon_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.tcon_gate);
}

static void lcd_clk_prbs_test_t7(struct aml_lcd_drv_s *pdrv,
			unsigned int ms, unsigned int mode_flag)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int reg_phy_tx_ctrl0, reg_phy_tx_ctrl1, reg_ctrl_out, bit_width;
	int encl_msr_id, fifo_msr_id;
	unsigned int lcd_prbs_mode, lcd_prbs_cnt;
	unsigned int val1, val2, timeout;
	unsigned int clk_err_cnt = 0;
	int i, j, ret;

	if (!cconf)
		return;

	switch (pdrv->index) {
	case 0:
		reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
		reg_phy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
		reg_ctrl_out = COMBO_DPHY_RO_EDP_LVDS_TX_PHY0_CNTL1;
		bit_width = 8;
		break;
	case 1:
		reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
		reg_phy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
		reg_ctrl_out = COMBO_DPHY_RO_EDP_LVDS_TX_PHY1_CNTL1;
		bit_width = 8;
		break;
	case 2:
		reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0;
		reg_phy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1;
		reg_ctrl_out = COMBO_DPHY_RO_EDP_LVDS_TX_PHY2_CNTL1;
		bit_width = 10;
		break;
	default:
		LCDERR("[%d]: %s: invalid drv_index\n", pdrv->index, __func__);
		return;
	}
	encl_msr_id = cconf->data->enc_clk_msr_id;
	fifo_msr_id = cconf->data->fifo_clk_msr_id;

	timeout = (ms > 1000) ? 1000 : ms;

	for (i = 0; i < LCD_PRBS_MODE_MAX; i++) {
		if ((mode_flag & (1 << i)) == 0)
			continue;

		lcd_combo_dphy_write(pdrv, reg_phy_tx_ctrl0, 0);
		lcd_combo_dphy_write(pdrv, reg_phy_tx_ctrl1, 0);

		lcd_prbs_cnt = 0;
		clk_err_cnt = 0;
		lcd_prbs_mode = (1 << i);
		LCDPR("[%d]: lcd_prbs_mode: 0x%x\n", pdrv->index, lcd_prbs_mode);
		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_encl_clk_check_std = 136000000;
			lcd_fifo_clk_check_std = 48000000;
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_encl_clk_check_std = 594000000;
			lcd_fifo_clk_check_std = 297000000;
		}

		lcd_prbs_config_clk_t7(pdrv, lcd_prbs_mode);
		usleep_range(500, 510);

		/* set fifo_clk_sel: div 10 */
		lcd_combo_dphy_write(pdrv, reg_phy_tx_ctrl0, (3 << 5));
		/* set cntl_ser_en:  10-channel */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0x3ff, 16, 10);
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 1, 2, 1);
		/* decoupling fifo enable, gated clock enable */
		lcd_combo_dphy_write(pdrv, reg_phy_tx_ctrl1, (1 << 6) | (1 << 0));
		/* decoupling fifo write enable after fifo enable */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl1, 1, 7, 1);

		/* prbs_err en */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 1, 13, 1);
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 1, 12, 1);

		while (lcd_prbs_flag) {
			if (lcd_prbs_cnt++ >= timeout)
				break;
			ret = 1;
			val1 = lcd_combo_dphy_getb(pdrv, reg_ctrl_out,
						   bit_width, bit_width);
			usleep_range(1000, 1001);

			for (j = 0; j < 20; j++) {
				usleep_range(5, 10);
				val2 = lcd_combo_dphy_getb(pdrv, reg_ctrl_out,
							   bit_width, bit_width);
				if (val2 != val1) {
					ret = 0;
					break;
				}
			}
			if (ret) {
				LCDERR("[%d]: prbs check error 1, val:0x%03x, cnt:%d\n",
				       pdrv->index, val2, lcd_prbs_cnt);
				goto lcd_prbs_test_err_t7;
			}
			if (lcd_combo_dphy_getb(pdrv, reg_ctrl_out, 0, bit_width)) {
				LCDERR("[%d]: prbs check error 2, cnt:%d\n",
				       pdrv->index, lcd_prbs_cnt);
				goto lcd_prbs_test_err_t7;
			}

			if (lcd_prbs_clk_check(lcd_encl_clk_check_std, encl_msr_id,
					       lcd_fifo_clk_check_std, fifo_msr_id,
					       lcd_prbs_cnt))
				clk_err_cnt++;
			else
				clk_err_cnt = 0;
			if (clk_err_cnt >= 10) {
				LCDERR("[%d]: prbs check error 3(clkmsr), cnt: %d\n",
				       pdrv->index, lcd_prbs_cnt);
				goto lcd_prbs_test_err_t7;
			}
		}

		lcd_combo_dphy_write(pdrv, reg_phy_tx_ctrl0, 0);
		lcd_combo_dphy_write(pdrv, reg_phy_tx_ctrl1, 0);

		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_LVDS);
			LCDPR("[%d]: lvds prbs check ok\n", pdrv->index);
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_prbs_performed |= LCD_PRBS_MODE_VX1;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_VX1);
			LCDPR("[%d]: vx1 prbs check ok\n", pdrv->index);
		} else {
			LCDPR("[%d]: prbs check: unsupport mode\n", pdrv->index);
		}
		continue;

lcd_prbs_test_err_t7:
		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err |= LCD_PRBS_MODE_LVDS;
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_prbs_performed |= LCD_PRBS_MODE_VX1;
			lcd_prbs_err |= LCD_PRBS_MODE_VX1;
		}
	}

	lcd_prbs_flag = 0;
}

static void lcd_clk_prbs_test_t3(struct aml_lcd_drv_s *pdrv,
				unsigned int ms, unsigned int mode_flag)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int reg_phy_tx_ctrl0, reg_phy_tx_ctrl1;
	int encl_msr_id, fifo_msr_id;
	unsigned int lcd_prbs_mode, lcd_prbs_cnt;
	unsigned int val1, val2, timeout;
	unsigned int clk_err_cnt = 0;
	int i, j, ret;

	if (!cconf)
		return;

	switch (pdrv->index) {
	case 0:
		reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
		reg_phy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL1;
		break;
	case 1:
		reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL2;
		reg_phy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL3;
		break;
	default:
		LCDERR("[%d]: %s: invalid drv_index\n", pdrv->index, __func__);
		return;
	}
	encl_msr_id = cconf->data->enc_clk_msr_id;
	fifo_msr_id = cconf->data->fifo_clk_msr_id;

	timeout = (ms > 1000) ? 1000 : ms;

	for (i = 0; i < LCD_PRBS_MODE_MAX; i++) {
		if ((mode_flag & (1 << i)) == 0)
			continue;

		lcd_ana_write(reg_phy_tx_ctrl0, 0);
		lcd_ana_write(reg_phy_tx_ctrl1, 0);

		lcd_prbs_cnt = 0;
		clk_err_cnt = 0;
		lcd_prbs_mode = (1 << i);
		LCDPR("[%d]: lcd_prbs_mode: 0x%x\n", pdrv->index, lcd_prbs_mode);
		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_encl_clk_check_std = 136000000;
			lcd_fifo_clk_check_std = 48000000;
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_encl_clk_check_std = 594000000;
			lcd_fifo_clk_check_std = 297000000;
		}

		lcd_prbs_config_clk_t3(pdrv, lcd_prbs_mode);
		usleep_range(500, 510);

		/* set fifo_clk_sel: div 10 */
		lcd_ana_write(reg_phy_tx_ctrl0, (3 << 6));
		/* set cntl_ser_en:  12-channel */
		lcd_ana_setb(reg_phy_tx_ctrl0, 0xfff, 16, 12);
		lcd_ana_setb(reg_phy_tx_ctrl0, 1, 2, 1);
		/* decoupling fifo enable, gated clock enable */
		lcd_ana_write(reg_phy_tx_ctrl1, (1 << 30) | (1 << 24));
		/* decoupling fifo write enable after fifo enable */
		lcd_ana_setb(reg_phy_tx_ctrl1, 1, 31, 1);
		/* prbs_err en */
		lcd_ana_setb(reg_phy_tx_ctrl0, 1, 13, 1);
		lcd_ana_setb(reg_phy_tx_ctrl0, 1, 12, 1);

		while (lcd_prbs_flag) {
			if (lcd_prbs_cnt++ >= timeout)
				break;
			ret = 1;
			val1 = lcd_ana_getb(reg_phy_tx_ctrl1, 12, 12);
			usleep_range(1000, 1001);

			for (j = 0; j < 20; j++) {
				usleep_range(5, 10);
				val2 = lcd_ana_getb(reg_phy_tx_ctrl1, 12, 12);
				if (val2 != val1) {
					ret = 0;
					break;
				}
			}
			if (ret) {
				LCDERR("[%d]: prbs check error 1, val:0x%03x, cnt:%d\n",
				       pdrv->index, val2, lcd_prbs_cnt);
				goto lcd_prbs_test_err_t3;
			}
			if (lcd_ana_getb(reg_phy_tx_ctrl1, 0, 12)) {
				LCDERR("[%d]: prbs check error 2, cnt:%d\n",
				       pdrv->index, lcd_prbs_cnt);
				goto lcd_prbs_test_err_t3;
			}

			if (lcd_prbs_clk_check(lcd_encl_clk_check_std, encl_msr_id,
					       lcd_fifo_clk_check_std, fifo_msr_id,
					       lcd_prbs_cnt))
				clk_err_cnt++;
			else
				clk_err_cnt = 0;
			if (clk_err_cnt >= 10) {
				LCDERR("[%d]: prbs check error 3(clkmsr), cnt: %d\n",
				       pdrv->index, lcd_prbs_cnt);
				goto lcd_prbs_test_err_t3;
			}
		}

		lcd_ana_write(reg_phy_tx_ctrl0, 0);
		lcd_ana_write(reg_phy_tx_ctrl1, 0);

		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_LVDS);
			LCDPR("[%d]: lvds prbs check ok\n", pdrv->index);
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_prbs_performed |= LCD_PRBS_MODE_VX1;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_VX1);
			LCDPR("[%d]: vx1 prbs check ok\n", pdrv->index);
		} else {
			LCDPR("[%d]: prbs check: unsupport mode\n", pdrv->index);
		}
		continue;

lcd_prbs_test_err_t3:
		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err |= LCD_PRBS_MODE_LVDS;
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_prbs_performed |= LCD_PRBS_MODE_VX1;
			lcd_prbs_err |= LCD_PRBS_MODE_VX1;
		}
	}

	lcd_prbs_flag = 0;
}

static struct lcd_clk_ctrl_s pll_ctrl_table_t7[] = {
	/* flag             reg                      bit              len*/
	{LCD_CLK_CTRL_EN,   ANACTRL_TCON_PLL0_CNTL0, LCD_PLL_EN_TL1,   1},
	{LCD_CLK_CTRL_RST,  ANACTRL_TCON_PLL0_CNTL0, LCD_PLL_RST_TL1,  1},
	{LCD_CLK_CTRL_M,    ANACTRL_TCON_PLL0_CNTL0, LCD_PLL_M_TL1,    8},
	{LCD_CLK_CTRL_FRAC, ANACTRL_TCON_PLL0_CNTL1,               0, 17},
	{LCD_CLK_CTRL_END,  LCD_CLK_REG_END,                       0,  0},
};

static struct lcd_clk_data_s lcd_clk_data_t7 = {
	.pll_od_fb = 0,
	.pll_m_max = 511,
	.pll_m_min = 2,
	.pll_n_max = 1,
	.pll_n_min = 1,
	.pll_frac_range = (1 << 17),
	.pll_frac_sign_bit = 18,
	.pll_od_sel_max = 3,
	.pll_ref_fmax = 25000000,
	.pll_ref_fmin = 5000000,
	.pll_vco_fmax = 6000000000ULL,
	.pll_vco_fmin = 3000000000ULL,
	.pll_out_fmax = 3100000000ULL,
	.pll_out_fmin = 187500000,
	.div_in_fmax = 3100000000ULL,
	.div_out_fmax = 750000000,
	.xd_out_fmax = 750000000,

	.vclk_sel = 0,
	.enc_clk_msr_id = 222,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,
	.pll_ctrl_table = pll_ctrl_table_t7,

	.ss_support = 1,
	.ss_level_max = 60,
	.ss_freq_max = 6,
	.ss_mode_max = 2,
	.ss_dep_base = 500, //ppm
	.ss_dep_sel_max = 12,
	.ss_str_m_max = 10,

	.clk_generate_parameter = lcd_clk_generate_dft,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = lcd_set_pll_ss_level,
	.set_ss_advance = lcd_set_pll_ss_advance,
	.clk_ss_enable = lcd_pll_ss_enable,
	.clk_ss_init = lcd_pll_ss_init,
	.clk_set = lcd_clk_set_t7,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable,
	.clk_gate_switch = lcd_clk_gate_switch_t7,
	.clk_gate_optional_switch = NULL,
	.clktree_set = NULL,
	.clktree_probe = lcd_clktree_probe_t7,
	.clktree_remove = lcd_clktree_remove_t7,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.prbs_test = lcd_clk_prbs_test_t7,
};

static struct lcd_clk_data_s lcd_clk_data_t3 = {
	.pll_od_fb = 0,
	.pll_m_max = 511,
	.pll_m_min = 2,
	.pll_n_max = 1,
	.pll_n_min = 1,
	.pll_frac_range = (1 << 17),
	.pll_frac_sign_bit = 18,
	.pll_od_sel_max = 3,
	.pll_ref_fmax = 25000000,
	.pll_ref_fmin = 5000000,
	.pll_vco_fmax = 6000000000ULL,
	.pll_vco_fmin = 3000000000ULL,
	.pll_out_fmax = 3100000000ULL,
	.pll_out_fmin = 187500000,
	.div_in_fmax = 3100000000ULL,
	.div_out_fmax = 750000000,
	.xd_out_fmax = 750000000,

	.vclk_sel = 0,
	.enc_clk_msr_id = 222,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = 119,
	.pll_ctrl_table = pll_ctrl_table_t7,

	.ss_support = 1,
	.ss_level_max = 60,
	.ss_freq_max = 6,
	.ss_mode_max = 2,
	.ss_dep_base = 500, //ppm
	.ss_dep_sel_max = 12,
	.ss_str_m_max = 10,

	.clk_generate_parameter = lcd_clk_generate_dft,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = lcd_set_pll_ss_level,
	.set_ss_advance = lcd_set_pll_ss_advance,
	.clk_ss_enable = lcd_pll_ss_enable,
	.clk_ss_init = lcd_pll_ss_init,
	.clk_set = lcd_clk_set_t3,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable,
	.clk_gate_switch = lcd_clk_gate_switch_t7,
	.clk_gate_optional_switch = lcd_clk_gate_optional_switch_fdt,
	.clktree_set = lcd_set_tcon_clk,
	.clktree_probe = lcd_clktree_probe_t3,
	.clktree_remove = lcd_clktree_remove_t3,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.prbs_test = lcd_clk_prbs_test_t3,
};

void lcd_clk_config_chip_init_t7(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	cconf->data = &lcd_clk_data_t7;
	cconf->pll_od_fb = lcd_clk_data_t7.pll_od_fb;
	cconf->clk_path_change = NULL;
	switch (pdrv->index) {
	case 2:
		cconf->data->enc_clk_msr_id = 220;
		cconf->pll_id = 2;
		cconf->pll_offset = 0xa;
		break;
	case 1:
		cconf->data->enc_clk_msr_id = 221;
		cconf->pll_id = 1;
		cconf->pll_offset = 0x5;
		break;
	case 0:
	default:
		cconf->data->enc_clk_msr_id = 222;
		cconf->pll_id = 0;
		cconf->pll_offset = 0;
		break;
	}
	cconf->data->enc_clk_msr_id = -1;
}

void lcd_clk_config_chip_init_t3(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	cconf->data = &lcd_clk_data_t3;
	cconf->pll_od_fb = lcd_clk_data_t3.pll_od_fb;
	cconf->clk_path_change = NULL;
	switch (pdrv->index) {
	case 1:
		cconf->data->enc_clk_msr_id = 221;
		cconf->pll_id = 1;
		break;
	case 0:
	default:
		cconf->data->enc_clk_msr_id = 222;
		cconf->pll_id = 0;
		break;
	}
	cconf->data->enc_clk_msr_id = -1;
}
