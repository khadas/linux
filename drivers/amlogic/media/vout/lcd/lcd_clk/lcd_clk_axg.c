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

static void lcd_set_pll(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl, pll_ctrl1, pll_ctrl2;
	int ret, cnt = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl = ((1 << LCD_PLL_EN_AXG) |
		(cconf->pll_n << LCD_PLL_N_AXG) |
		(cconf->pll_m << LCD_PLL_M_AXG) |
		(cconf->pll_od1_sel << LCD_PLL_OD_AXG));
	pll_ctrl1 = 0xc084a000;
	pll_ctrl1 |= ((1 << 12) | (cconf->pll_frac << 0));
	pll_ctrl2 = 0xb75020be | (cconf->pll_od_fb << 19);

set_pll_retry_axg:
	lcd_hiu_write(HHI_GP0_PLL_CNTL_AXG, pll_ctrl);
	lcd_hiu_write(HHI_GP0_PLL_CNTL1_AXG, pll_ctrl1);
	lcd_hiu_write(HHI_GP0_PLL_CNTL2_AXG, pll_ctrl2);
	lcd_hiu_write(HHI_GP0_PLL_CNTL3_AXG, 0x0a59a288);
	lcd_hiu_write(HHI_GP0_PLL_CNTL4_AXG, 0xc000004d);
	if (cconf->pll_fvco >= 1632000)
		lcd_hiu_write(HHI_GP0_PLL_CNTL5_AXG, 0x00058000);
	else
		lcd_hiu_write(HHI_GP0_PLL_CNTL5_AXG, 0x00078000);
	lcd_hiu_setb(HHI_GP0_PLL_CNTL_AXG, 1, LCD_PLL_RST_AXG, 1);
	lcd_hiu_setb(HHI_GP0_PLL_CNTL_AXG, 0, LCD_PLL_RST_AXG, 1);

	ret = lcd_pll_wait_lock(HHI_GP0_PLL_CNTL_AXG, LCD_PLL_LOCK_AXG);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_pll_retry_axg;
		LCDERR("gp0_pll lock failed\n");
	}
}

static void lcd_set_vclk_crt(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (pdrv->lcd_pxp) {
		/* setup the XD divider value */
		lcd_hiu_setb(HHI_VIID_CLK_DIV, cconf->xd, VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_hiu_setb(HHI_VIID_CLK_CNTL, 7, VCLK2_CLK_IN_SEL, 3);
	} else {
		/* setup the XD divider value */
		lcd_hiu_setb(HHI_VIID_CLK_DIV, (cconf->xd - 1), VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_hiu_setb(HHI_VIID_CLK_CNTL, cconf->data->vclk_sel, VCLK2_CLK_IN_SEL, 3);
	}
	lcd_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	udelay(2);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_hiu_setb(HHI_VIID_CLK_DIV, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_hiu_setb(HHI_VIID_CLK_DIV, 1, VCLK2_XD_EN, 2);
	udelay(5);

	lcd_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_DIV1_EN, 1);
	lcd_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_SOFT_RST, 1);
	udelay(10);
	lcd_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	/* enable CTS_ENCL clk gate */
	lcd_hiu_setb(HHI_VID_CLK_CNTL2, 1, ENCL_GATE_VCLK, 1);
}

static void lcd_clk_set(struct aml_lcd_drv_s *pdrv)
{
	lcd_set_pll(pdrv);
}

static void lcd_clk_disable(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	int i = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	lcd_hiu_setb(HHI_VID_CLK_CNTL2, 0, ENCL_GATE_VCLK, 1);

	/* close vclk2_div gate: 0x104b[4:0] */
	lcd_hiu_setb(HHI_VIID_CLK_CNTL, 0, 0, 5);
	lcd_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);

	if (!cconf->data->pll_ctrl_table)
		return;
	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_END)
			break;
		if (table[i].flag == LCD_CLK_CTRL_EN)
			lcd_hiu_setb(table[i].reg, 0, table[i].bit, table[i].len);
		else if (table[i].flag == LCD_CLK_CTRL_RST)
			lcd_hiu_setb(table[i].reg, 1, table[i].bit, table[i].len);
		i++;
	}
}

static void lcd_clk_gate_switch_axg(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (status) {
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
		if (IS_ERR_OR_NULL(cconf->clktree.mipi_enable_gate))
			LCDERR("%s: mipi_enable_gate\n", __func__);
		else
			clk_prepare_enable(cconf->clktree.mipi_enable_gate);
		if (IS_ERR_OR_NULL(cconf->clktree.mipi_bandgap_gate))
			LCDERR("%s: mipi_bandgap_gate\n", __func__);
		else
			clk_prepare_enable(cconf->clktree.mipi_bandgap_gate);
	} else {
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
		if (IS_ERR_OR_NULL(cconf->clktree.mipi_enable_gate))
			LCDERR("%s: mipi_enable_gate\n", __func__);
		else
			clk_disable_unprepare(cconf->clktree.mipi_enable_gate);
		if (IS_ERR_OR_NULL(cconf->clktree.mipi_bandgap_gate))
			LCDERR("%s: mipi_bandgap_gate\n", __func__);
		else
			clk_disable_unprepare(cconf->clktree.mipi_bandgap_gate);
	}
}

static void lcd_clktree_probe_axg(struct aml_lcd_drv_s *pdrv)
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

	cconf->clktree.mipi_enable_gate = devm_clk_get(pdrv->dev,
		"mipi_enable_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.mipi_enable_gate))
		LCDERR("%s: clk mipi_enable_gate\n", __func__);

	cconf->clktree.mipi_bandgap_gate = devm_clk_get(pdrv->dev,
		"mipi_bandgap_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.mipi_bandgap_gate))
		LCDERR("%s: clk mipi_bandgap_gate\n", __func__);

	LCDPR("lcd_clktree_probe\n");
}

static void lcd_clktree_remove_axg(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd_clktree_remove\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (!IS_ERR_OR_NULL(cconf->clktree.mipi_bandgap_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.mipi_bandgap_gate);
	if (!IS_ERR_OR_NULL(cconf->clktree.mipi_enable_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.mipi_enable_gate);
	if (!IS_ERR_OR_NULL(cconf->clktree.dsi_meas))
		devm_clk_put(pdrv->dev, cconf->clktree.dsi_meas);
	if (!IS_ERR_OR_NULL(cconf->clktree.dsi_phy_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.dsi_phy_gate);
	if (!IS_ERR_OR_NULL(cconf->clktree.dsi_host_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.dsi_host_gate);
	if (!IS_ERR_OR_NULL(cconf->clktree.gp0_pll))
		devm_clk_put(pdrv->dev, cconf->clktree.gp0_pll);
}

static int lcd_clk_config_print_axg(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lcd clk config:\n"
		"pll_m:        %d\n"
		"pll_n:        %d\n"
		"pll_frac:     0x%03x\n"
		"pll_fvco:     %dkHz\n"
		"pll_od:       %d\n"
		"pll_out:      %dkHz\n"
		"xd:           %d\n"
		"fout:         %dkHz\n\n",
		cconf->pll_m, cconf->pll_n,
		cconf->pll_frac, cconf->pll_fvco,
		cconf->pll_od1_sel, cconf->pll_fout,
		cconf->xd, cconf->fout);

	return len;
}

static void lcd_clk_generate_g12a(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int pll_fout;
	unsigned int xd;
	unsigned int bit_rate_max = 0, bit_rate_min = 0;
	unsigned int tmp;
	int done = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	cconf->fout = pconf->timing.lcd_clk / 1000; /* kHz */
	cconf->err_fmin = MAX_ERROR;

	if (cconf->fout > cconf->data->xd_out_fmax) {
		LCDERR("%s: wrong lcd_clk value %dkHz\n", __func__, cconf->fout);
		goto generate_clk_done_g12a;
	}

	switch (pconf->basic.lcd_type) {
	case LCD_MIPI:
		cconf->xd_max = CRT_VID_DIV_MAX;
		bit_rate_max = pconf->control.mipi_cfg.local_bit_rate_max;
		bit_rate_min = pconf->control.mipi_cfg.local_bit_rate_min;
		tmp = bit_rate_max - cconf->fout;
		if (tmp >= bit_rate_min)
			bit_rate_min = tmp;

		for (xd = 1; xd <= cconf->xd_max; xd++) {
			pll_fout = cconf->fout * xd;
			if (pll_fout > bit_rate_max || pll_fout < bit_rate_min)
				continue;
			if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
				LCDPR("fout=%d, xd=%d\n", cconf->fout, xd);

			pconf->timing.bit_rate = pll_fout * 1000;
			pconf->control.mipi_cfg.clk_factor = xd;
			cconf->xd = xd;
			done = check_pll_1od(cconf, pll_fout);
			if (done)
				goto generate_clk_done_g12a;
		}
		break;
	default:
		break;
	}

generate_clk_done_g12a:
	if (done) {
		pconf->timing.pll_ctrl =
			(cconf->pll_od1_sel << PLL_CTRL_OD1) |
			(cconf->pll_n << PLL_CTRL_N) |
			(cconf->pll_m << PLL_CTRL_M);
		pconf->timing.div_ctrl =
			(CLK_DIV_SEL_1 << DIV_CTRL_DIV_SEL) |
			(cconf->xd << DIV_CTRL_XD);
		pconf->timing.clk_ctrl =
			(cconf->pll_frac << CLK_CTRL_FRAC);
		cconf->done = 1;
	} else {
		pconf->timing.pll_ctrl =
			(1 << PLL_CTRL_OD1) |
			(1 << PLL_CTRL_N)   |
			(50 << PLL_CTRL_M);
		pconf->timing.div_ctrl =
			(CLK_DIV_SEL_1 << DIV_CTRL_DIV_SEL) |
			(7 << DIV_CTRL_XD);
		pconf->timing.clk_ctrl = (0 << CLK_CTRL_FRAC);
		cconf->done = 0;
		LCDERR("Out of clock range\n");
	}
}

static void lcd_pll_frac_generate_g12a(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int pll_fout;
	unsigned int od, pll_fvco;
	unsigned int m, n, od_fb, frac, offset, temp;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	cconf->fout = pconf->timing.lcd_clk / 1000; /* kHz */
	od = od_table[cconf->pll_od1_sel];
	m = cconf->pll_m;
	n = cconf->pll_n;
	od_fb = cconf->pll_od_fb;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("m=%d, n=%d, od=%d, xd=%d\n", m, n, cconf->pll_od1_sel, cconf->xd);
	if (cconf->fout > cconf->data->xd_out_fmax) {
		LCDERR("%s: wrong lcd_clk value %dkHz\n", __func__, cconf->fout);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s pclk=%d\n", __func__, cconf->fout);

	pll_fout = cconf->fout * cconf->xd;
	if (pll_fout > cconf->data->pll_out_fmax ||
	    pll_fout < cconf->data->pll_out_fmin) {
		LCDERR("%s: wrong pll_fout value %dkHz\n", __func__, pll_fout);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s pll_fout=%d\n", __func__, pll_fout);

	pll_fvco = pll_fout * od;
	if (pll_fvco < cconf->data->pll_vco_fmin ||
	    pll_fvco > cconf->data->pll_vco_fmax) {
		LCDERR("%s: wrong pll_fvco value %dkHz\n", __func__, pll_fvco);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s pll_fvco=%d\n", __func__, pll_fvco);

	cconf->pll_fvco = pll_fvco;
	pll_fvco = pll_fvco / od_fb_table[od_fb];
	temp = cconf->fin * m / n;
	if (pll_fvco >= temp) {
		temp = pll_fvco - temp;
		offset = 0;
	} else {
		temp = temp - pll_fvco;
		offset = 1;
	}
	if (temp >= (2 * cconf->fin)) {
		LCDERR("%s: pll changing %dkHz is too much\n", __func__, temp);
		return;
	}
	frac = temp * cconf->data->pll_frac_range * n / cconf->fin;
	cconf->pll_frac = frac | (offset << 11);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd_pll_frac_generate: frac=0x%x\n", frac);
}

static void lcd_clk_config_init_print_g12a(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_data_s *data;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	data = cconf->data;
	LCDPR("lcd clk config data init:\n"
		"vclk_sel:          %d\n"
		"pll_m_max:         %d\n"
		"pll_m_min:         %d\n"
		"pll_n_max:         %d\n"
		"pll_n_min:         %d\n"
		"pll_od_fb:         %d\n"
		"pll_frac_range:    %d\n"
		"pll_od_sel_max:    %d\n"
		"pll_ref_fmax:      %d\n"
		"pll_ref_fmin:      %d\n"
		"pll_vco_fmax:      %d\n"
		"pll_vco_fmin:      %d\n"
		"pll_out_fmax:      %d\n"
		"pll_out_fmin:      %d\n"
		"xd_out_fmax:       %d\n\n",
		data->vclk_sel,
		data->pll_m_max, data->pll_m_min,
		data->pll_n_max, data->pll_n_min,
		data->pll_od_fb, data->pll_frac_range,
		data->pll_od_sel_max,
		data->pll_ref_fmax, data->pll_ref_fmin,
		data->pll_vco_fmax, data->pll_vco_fmin,
		data->pll_out_fmax, data->pll_out_fmin,
		data->xd_out_fmax);
}

static struct lcd_clk_ctrl_s pll_ctrl_table_axg[] = {
	/* flag             reg                    bit               len*/
	{LCD_CLK_CTRL_EN,   HHI_GP0_PLL_CNTL_AXG,  LCD_PLL_EN_AXG,   1},
	{LCD_CLK_CTRL_RST,  HHI_GP0_PLL_CNTL_AXG,  LCD_PLL_RST_AXG,  1},
	{LCD_CLK_CTRL_M,    HHI_GP0_PLL_CNTL_AXG,  LCD_PLL_M_AXG,    8},
	{LCD_CLK_CTRL_FRAC, HHI_GP0_PLL_CNTL1_AXG,               0, 19},
	{LCD_CLK_CTRL_END,  LCD_CLK_REG_END,                     0,  0},
};

static struct lcd_clk_data_s lcd_clk_data_axg = {
	.pll_od_fb = PLL_OD_FB_AXG,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_AXG,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_AXG,
	.pll_od_sel_max = PLL_OD_SEL_MAX_AXG,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_AXG,
	.pll_vco_fmin = PLL_VCO_MIN_AXG,
	.pll_out_fmax = CRT_VID_CLK_IN_MAX_AXG,
	.pll_out_fmin = PLL_VCO_MIN_AXG / 4,
	.div_in_fmax = 0,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_AXG,
	.xd_out_fmax = ENCL_CLK_IN_MAX_AXG,

	.vclk_sel = 0,
	.enc_clk_msr_id = 9,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,
	.pll_ctrl_table = pll_ctrl_table_axg,

	.ss_support = 0,

	.clk_generate_parameter = lcd_clk_generate_g12a,
	.pll_frac_generate = lcd_pll_frac_generate_g12a,
	.set_ss_level = NULL,
	.set_ss_advance = NULL,
	.clk_ss_enable = NULL,
	.clk_set = lcd_clk_set,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable,
	.clk_gate_switch = lcd_clk_gate_switch_axg,
	.clk_gate_optional_switch = NULL,
	.clktree_set = NULL,
	.clktree_probe = lcd_clktree_probe_axg,
	.clktree_remove = lcd_clktree_remove_axg,
	.clk_config_init_print = lcd_clk_config_init_print_g12a,
	.clk_config_print = lcd_clk_config_print_axg,
	.prbs_test = NULL,
};

void lcd_clk_config_chip_init_axg(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	cconf->data = &lcd_clk_data_axg;
	cconf->pll_od_fb = lcd_clk_data_axg.pll_od_fb;
	cconf->clk_path_change = NULL;
}
