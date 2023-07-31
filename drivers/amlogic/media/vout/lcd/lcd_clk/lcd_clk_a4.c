// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
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

static unsigned int fclk_div_table[][2] = {
/*  sel,  divclk */
	{1, 666666667},
	{2, 500000000},
	{3, 400000000},
	{6, 800000000},
	{7, 285714286},
	{LCD_CLK_CTRL_END, LCD_CLK_CTRL_END}
};

static void lcd_set_fclk_div(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int f_target;
	unsigned int max_div = 256;
	unsigned int i = 0, div = 0, min_err_sel_idx = 0, min_err_div = 1;
	unsigned int min_err = 100000000, error;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	f_target = pdrv->config.timing.lcd_clk;
	if (f_target >= cconf->data->xd_out_fmax) {
		LCDERR("%s: freq(%dHz) out of limit(%dHz)\n", __func__,
			f_target, cconf->data->xd_out_fmax);
		return;
	}

	while (fclk_div_table[i][0] != LCD_CLK_CTRL_END)	{
		for (div = 1; div <= max_div; div++) {
			error = lcd_abs((fclk_div_table[i][1]) / div, f_target);
			if (error < min_err) {
				min_err_sel_idx = i;
				min_err_div = div;
				min_err = error;
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
					LCDPR("_sel:%d, _div:%d, err:%d\n",
						fclk_div_table[i][0], min_err_div, min_err);
			}
		}
		i++;
	}

	cconf->xd = min_err_div;
	cconf->data->vclk_sel = fclk_div_table[min_err_sel_idx][0];
	cconf->fout = fclk_div_table[min_err_sel_idx][1] / min_err_div;

	LCDPR("[%d]: f_tar:%d, f_out:%d, fclk:%dHz, div:%d, error:%d\n", pdrv->index,
		f_target, cconf->fout, fclk_div_table[min_err_sel_idx][1], min_err_div, min_err);
}

static void lcd_clk_set_a4(struct aml_lcd_drv_s *pdrv)
{
	/* gp0 used by emmc, no permission for gp1, used fix clk */
	lcd_set_fclk_div(pdrv);
}

static void lcd_set_vclk_crt_a4(struct aml_lcd_drv_s *pdrv)  /* from c3 */
{
	struct lcd_clk_config_s *cconf;
	unsigned int clk_phase;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("lcd clk: set_vclk_crt_c3\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	/* phase */
	clk_phase = pdrv->config.control.rgb_cfg.clk_pol;
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, clk_phase, 28, 2);

	/* cts_vout_clk */
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, (cconf->xd - 1), 16, 7);
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, cconf->data->vclk_sel, 25, 3);
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, 1, 24, 1);  /* cts_vout_mclk en */
}

static void lcd_clk_disable_a4(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, 0, 24, 1);
}

static int lcd_clk_config_print_a4(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lcd clk config:\n"
		"vclk_sel      %d\n"
		"xd:           %d\n"
		"fout:         %dHz\n\n",
		cconf->data->vclk_sel,
		cconf->xd, cconf->fout);

	return len;
}

static struct lcd_clk_data_s lcd_clk_data_a4 = {
	.pll_od_fb = 0,
	.pll_m_max = 511,
	.pll_m_min = 2,
	.pll_n_max = 1,
	.pll_n_min = 1,
	.pll_frac_range = 0,
	.pll_frac_sign_bit = 0,
	.pll_od_sel_max = 0,
	.pll_ref_fmax = 25000000,
	.pll_ref_fmin = 5000000,
	.pll_vco_fmax = 0,
	.pll_vco_fmin = 0,
	.pll_out_fmax = 0,
	.pll_out_fmin = 0,
	.div_in_fmax = 0,
	.div_out_fmax = 0,
	.xd_out_fmax = 75000000,

	.vclk_sel = 0xff, //unassigned
	.enc_clk_msr_id = LCD_CLK_MSR_INVALID,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,
	.pll_ctrl_table = NULL,

	.clk_generate_parameter = NULL,
	.pll_frac_generate = NULL,
	.set_ss_level = NULL,
	.set_ss_advance = NULL,
	.clk_ss_enable = NULL,
	.clk_set = lcd_clk_set_a4,
	.vclk_crt_set = lcd_set_vclk_crt_a4,
	.clk_disable = lcd_clk_disable_a4,
	.clk_gate_switch = NULL,
	.clk_gate_optional_switch = NULL,
	.clktree_set = NULL,
	.clktree_probe = NULL,
	.clktree_remove = NULL,
	.clk_config_init_print = NULL,
	.clk_config_print = lcd_clk_config_print_a4,
};

void lcd_clk_config_chip_init_a4(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	cconf->data = &lcd_clk_data_a4;
}
