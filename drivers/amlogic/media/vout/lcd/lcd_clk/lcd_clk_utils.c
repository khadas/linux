// SPDX-License-Identifier: GPL-2.0+
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

const unsigned int od_fb_table[2] = {1, 2};
const unsigned int od_table[6] = {1, 2, 4, 8, 16, 32};
const unsigned int tcon_div_table[5] = {1, 2, 4, 8, 16};
unsigned int tcon_div[5][3] = {
	/* div_mux, div2/4_sel, div4_bypass */
	{1, 0, 1},  /* div1 */
	{0, 0, 1},  /* div2 */
	{0, 1, 1},  /* div4 */
	{0, 0, 0},  /* div8 */
	{0, 1, 0},  /* div16 */
};

unsigned int lcd_clk_div_table[][3] = {
	/* divider,        shift_val,  shift_sel */
	{CLK_DIV_SEL_1,    0xffff,     0,},
	{CLK_DIV_SEL_2,    0x0aaa,     0,},
	{CLK_DIV_SEL_3,    0x0db6,     0,},
	{CLK_DIV_SEL_3p5,  0x36cc,     1,},
	{CLK_DIV_SEL_3p75, 0x6666,     2,},
	{CLK_DIV_SEL_4,    0x0ccc,     0,},
	{CLK_DIV_SEL_5,    0x739c,     2,},
	{CLK_DIV_SEL_6,    0x0e38,     0,},
	{CLK_DIV_SEL_6p25, 0x0000,     3,},
	{CLK_DIV_SEL_7,    0x3c78,     1,},
	{CLK_DIV_SEL_7p5,  0x78f0,     2,},
	{CLK_DIV_SEL_12,   0x0fc0,     0,},
	{CLK_DIV_SEL_14,   0x3f80,     1,},
	{CLK_DIV_SEL_15,   0x7f80,     2,},
	{CLK_DIV_SEL_2p5,  0x5294,     2,},
	{CLK_DIV_SEL_4p67, 0x0ccc,     1,},
	{CLK_DIV_SEL_MAX,  0xffff,     0,},
};

static unsigned int edp_div0_table[15] = {
	1, 2, 3, 4, 5, 7, 8, 9, 11, 13, 17, 19, 23, 29, 31
};

static unsigned int edp_div1_table[10] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 13
};

char *lcd_clk_div_sel_table[] = {
	"1",
	"2",
	"3",
	"3.5",
	"3.75",
	"4",
	"5",
	"6",
	"6.25",
	"7",
	"7.5",
	"12",
	"14",
	"15",
	"2.5",
	"4.67",
	"invalid",
};

/* ****************************************************
 * lcd pll & clk operation
 * ****************************************************
 */
static inline unsigned long lcd_abs(long a, long b)
{
	return (a >= b) ? (a - b) : (b - a);
}

int lcd_clk_msr_check(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int encl_clk_msr;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return 0;

	if (cconf->data->enc_clk_msr_id == -1)
		return 0;

	encl_clk_msr = lcd_encl_clk_msr(pdrv);
	if (lcd_abs((cconf->fout * 1000), encl_clk_msr) >= PLL_CLK_CHECK_MAX) {
		LCDERR("[%d]: %s: expected:%d, msr:%d\n",
		       pdrv->index, __func__,
		       (cconf->fout * 1000), encl_clk_msr);
		return -1;
	}

	return 0;
}

int lcd_pll_ss_level_generate(unsigned int *data, unsigned int level, unsigned int step)
{
	unsigned int max = 10, val;
	unsigned int dep_sel, str_m, temp, min;

	if (!data)
		return -1;

	val = level * 1000;
	min = val;
	for (dep_sel = 1; dep_sel <= max; dep_sel++) { //dep_sel
		for (str_m = 1; str_m <= max; str_m++) { //str_m
			temp = lcd_abs((dep_sel * str_m * step), val);
			if (min > temp) {
				min = temp;
				data[0] = dep_sel;
				data[1] = str_m;
			}
		}
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s: dep_sel=%d, str_m=%d, error=%d\n", __func__, data[0], data[1], min);

	return 0;
}

int lcd_pll_wait_lock(unsigned int reg, unsigned int lock_bit)
{
	unsigned int pll_lock;
	int wait_loop = PLL_WAIT_LOCK_CNT; /* 200 */
	int ret = 0;

	do {
		udelay(50);
		pll_lock = lcd_ana_getb(reg, lock_bit, 1);
		wait_loop--;
	} while ((pll_lock == 0) && (wait_loop > 0));
	if (pll_lock == 0)
		ret = -1;
	LCDPR("%s: pll_lock=%d, wait_loop=%d\n",
	      __func__, pll_lock, (PLL_WAIT_LOCK_CNT - wait_loop));

	return ret;
}

/* ****************************************************
 * lcd clk parameters calculate
 * ****************************************************
 */
unsigned int clk_vid_pll_div_calc(unsigned int clk,
					 unsigned int div_sel, int dir)
{
	unsigned int clk_ret;

	switch (div_sel) {
	case CLK_DIV_SEL_1:
		clk_ret = clk;
		break;
	case CLK_DIV_SEL_2:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk / 2;
		else
			clk_ret = clk * 2;
		break;
	case CLK_DIV_SEL_3:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk / 3;
		else
			clk_ret = clk * 3;
		break;
	case CLK_DIV_SEL_3p5:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk * 2 / 7;
		else
			clk_ret = clk * 7 / 2;
		break;
	case CLK_DIV_SEL_3p75:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk * 4 / 15;
		else
			clk_ret = clk * 15 / 4;
		break;
	case CLK_DIV_SEL_4:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk / 4;
		else
			clk_ret = clk * 4;
		break;
	case CLK_DIV_SEL_5:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk / 5;
		else
			clk_ret = clk * 5;
		break;
	case CLK_DIV_SEL_6:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk / 6;
		else
			clk_ret = clk * 6;
		break;
	case CLK_DIV_SEL_6p25:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk * 4 / 25;
		else
			clk_ret = clk * 25 / 4;
		break;
	case CLK_DIV_SEL_7:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk / 7;
		else
			clk_ret = clk * 7;
		break;
	case CLK_DIV_SEL_7p5:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk * 2 / 15;
		else
			clk_ret = clk * 15 / 2;
		break;
	case CLK_DIV_SEL_12:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk / 12;
		else
			clk_ret = clk * 12;
		break;
	case CLK_DIV_SEL_14:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk / 14;
		else
			clk_ret = clk * 14;
		break;
	case CLK_DIV_SEL_15:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk / 15;
		else
			clk_ret = clk * 15;
		break;
	case CLK_DIV_SEL_2p5:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk * 2 / 5;
		else
			clk_ret = clk * 5 / 2;
		break;
	case CLK_DIV_SEL_4p67:
		if (dir == CLK_DIV_I2O)
			clk_ret = clk * 3 / 14;
		else
			clk_ret = clk * 14 / 3;
		break;
	default:
		clk_ret = clk;
		LCDERR("clk_div_sel: Invalid parameter\n");
		break;
	}

	return clk_ret;
}

unsigned int clk_vid_pll_div_get(unsigned int clk_div)
{
	unsigned int div_sel;

	/* div * 100 */
	switch (clk_div) {
	case 375:
		div_sel = CLK_DIV_SEL_3p75;
		break;
	case 750:
		div_sel = CLK_DIV_SEL_7p5;
		break;
	case 1500:
		div_sel = CLK_DIV_SEL_15;
		break;
	case 500:
		div_sel = CLK_DIV_SEL_5;
		break;
	default:
		div_sel = CLK_DIV_SEL_MAX;
		break;
	}
	return div_sel;
}

int check_pll_3od(struct lcd_clk_config_s *cconf,
			 unsigned int pll_fout)
{
	struct lcd_clk_data_s *data = cconf->data;
	unsigned int m, n;
	unsigned int od1_sel, od2_sel, od3_sel, od1, od2, od3;
	unsigned int pll_fod2_in, pll_fod3_in, pll_fvco;
	unsigned int od_fb = 0, frac_range, pll_frac;
	int done;

	done = 0;
	if (pll_fout > data->pll_out_fmax ||
	    pll_fout < data->pll_out_fmin) {
		return done;
	}
	frac_range = data->pll_frac_range;
	for (od3_sel = data->pll_od_sel_max; od3_sel > 0; od3_sel--) {
		od3 = od_table[od3_sel - 1];
		pll_fod3_in = pll_fout * od3;
		for (od2_sel = od3_sel; od2_sel > 0; od2_sel--) {
			od2 = od_table[od2_sel - 1];
			pll_fod2_in = pll_fod3_in * od2;
			for (od1_sel = od2_sel; od1_sel > 0; od1_sel--) {
				od1 = od_table[od1_sel - 1];
				pll_fvco = pll_fod2_in * od1;
				if (pll_fvco < data->pll_vco_fmin ||
				    pll_fvco > data->pll_vco_fmax) {
					continue;
				}
				cconf->pll_od1_sel = od1_sel - 1;
				cconf->pll_od2_sel = od2_sel - 1;
				cconf->pll_od3_sel = od3_sel - 1;
				cconf->pll_fout = pll_fout;
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
					LCDPR("od1=%d, od2=%d, od3=%d, pll_fvco=%d\n",
					      (od1_sel - 1), (od2_sel - 1),
					      (od3_sel - 1), pll_fvco);
				}
				cconf->pll_fvco = pll_fvco;
				n = 1;
				od_fb = cconf->pll_od_fb;
				pll_fvco = pll_fvco / od_fb_table[od_fb];
				m = pll_fvco / cconf->fin;
				pll_frac = (pll_fvco % cconf->fin) * frac_range / cconf->fin;
				if (cconf->pll_mode & LCD_PLL_MODE_FRAC_SHIFT) {
					if ((pll_frac == (frac_range >> 1)) ||
					    (pll_frac == (frac_range >> 2))) {
						pll_frac |= 0x66;
						cconf->pll_frac_half_shift = 1;
					} else {
						cconf->pll_frac_half_shift = 0;
					}
				}
				cconf->pll_m = m;
				cconf->pll_n = n;
				cconf->pll_frac = pll_frac;
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
					LCDPR("m=%d, n=%d, frac=0x%x\n", m, n, pll_frac);
				done = 1;
				break;
			}
		}
	}
	return done;
}

int check_pll_1od(struct lcd_clk_config_s *cconf, unsigned int pll_fout)
{
	struct lcd_clk_data_s *data = cconf->data;
	unsigned int m, n, od_sel, od;
	unsigned int pll_fvco;
	unsigned int od_fb = 0, pll_frac;
	int done = 0;

	if (pll_fout > data->pll_out_fmax || pll_fout < data->pll_out_fmin)
		return done;

	for (od_sel = data->pll_od_sel_max; od_sel > 0; od_sel--) {
		od = od_table[od_sel - 1];
		pll_fvco = pll_fout * od;
		if (pll_fvco < data->pll_vco_fmin || pll_fvco > data->pll_vco_fmax)
			continue;
		cconf->pll_od1_sel = od_sel - 1;
		cconf->pll_fout = pll_fout;
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
			LCDPR("od_sel=%d, pll_fvco=%d\n", (od_sel - 1), pll_fvco);

		cconf->pll_fvco = pll_fvco;
		n = 1;
		od_fb = cconf->pll_od_fb;
		pll_fvco = pll_fvco / od_fb_table[od_fb];
		m = pll_fvco / cconf->fin;
		pll_frac = (pll_fvco % cconf->fin) * data->pll_frac_range / cconf->fin;
		cconf->pll_m = m;
		cconf->pll_n = n;
		cconf->pll_frac = pll_frac;
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
			LCDPR("pll_m=%d, pll_n=%d, pll_frac=0x%x\n", m, n, pll_frac);
		done = 1;
		break;
	}
	return done;
}

int check_vco(struct lcd_clk_config_s *cconf, unsigned int pll_fvco)
{
	struct lcd_clk_data_s *data = cconf->data;
	unsigned int m, n;
	unsigned int od_fb = 0, pll_frac;
	int done = 0;

	if (pll_fvco < data->pll_vco_fmin || pll_fvco > data->pll_vco_fmax) {
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
			LCDPR("pll_fvco %d is out of range\n", pll_fvco);
		return done;
	}

	cconf->pll_fvco = pll_fvco;
	n = 1;
	od_fb = cconf->pll_od_fb;
	pll_fvco = pll_fvco / od_fb_table[od_fb];
	m = pll_fvco / cconf->fin;
	pll_frac = (pll_fvco % cconf->fin) * data->pll_frac_range / cconf->fin;
	cconf->pll_m = m;
	cconf->pll_n = n;
	cconf->pll_frac = pll_frac;
	if (cconf->pll_mode & LCD_PLL_MODE_FRAC_SHIFT) {
		if ((pll_frac == (data->pll_frac_range >> 1)) ||
		    (pll_frac == (data->pll_frac_range >> 2))) {
			pll_frac |= 0x66;
			cconf->pll_frac_half_shift = 1;
		} else {
			cconf->pll_frac_half_shift = 0;
		}
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("m=%d, n=%d, frac=0x%x, pll_fvco=%d\n",
		      m, n, pll_frac, pll_fvco);
	}
	done = 1;

	return done;
}

#define PLL_FVCO_ERR_MAX    2 /* kHz */
int check_od(struct lcd_clk_config_s *cconf, unsigned int pll_fout)
{
	struct lcd_clk_data_s *data = cconf->data;
	unsigned int od1_sel, od2_sel, od3_sel, od1, od2, od3;
	unsigned int pll_fod2_in, pll_fod3_in, pll_fvco;
	int done = 0;

	if (pll_fout > data->pll_out_fmax ||
	    pll_fout < data->pll_out_fmin) {
		return done;
	}

	for (od3_sel = data->pll_od_sel_max; od3_sel > 0; od3_sel--) {
		od3 = od_table[od3_sel - 1];
		pll_fod3_in = pll_fout * od3;
		for (od2_sel = od3_sel; od2_sel > 0; od2_sel--) {
			od2 = od_table[od2_sel - 1];
			pll_fod2_in = pll_fod3_in * od2;
			for (od1_sel = od2_sel; od1_sel > 0; od1_sel--) {
				od1 = od_table[od1_sel - 1];
				pll_fvco = pll_fod2_in * od1;
				if (pll_fvco < data->pll_vco_fmin ||
				    pll_fvco > data->pll_vco_fmax) {
					continue;
				}
				if (lcd_abs(pll_fvco, cconf->pll_fvco) <
					PLL_FVCO_ERR_MAX) {
					cconf->pll_od1_sel = od1_sel - 1;
					cconf->pll_od2_sel = od2_sel - 1;
					cconf->pll_od3_sel = od3_sel - 1;
					cconf->pll_fout = pll_fout;

					if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
						LCDPR("od1=%d, od2=%d, od3=%d\n",
						 (od1_sel - 1),
						 (od2_sel - 1),
						 (od3_sel - 1));
					}
					done = 1;
					break;
				}
			}
		}
	}
	return done;
}

int edp_div_check(struct lcd_clk_config_s *cconf, unsigned int bit_rate)
{
	unsigned int edp_div0, edp_div1, tmp_div, tmp;

	for (edp_div0 = 0; edp_div0 < 15; edp_div0++) {
		for (edp_div1 = 0; edp_div1 < 10; edp_div1++) {
			tmp_div = edp_div0_table[edp_div0] * edp_div1_table[edp_div1];
			tmp = bit_rate / tmp_div;
			if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
				LCDPR("fout=%d, _clk=%d, tmp_div=%d, edp_div0=%d, edp_div1=%d\n",
				      cconf->fout, tmp, tmp_div, edp_div0, edp_div1);
			}
			tmp = lcd_abs(tmp, cconf->fout);
			if (cconf->err_fmin > tmp) {
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
					LCDPR("err=%d, edp_div0=%d, edp_div1=%d\n",
					      tmp, edp_div0, edp_div1);
				}
				cconf->err_fmin = tmp;
				cconf->edp_div0 = edp_div0;
				cconf->edp_div1 = edp_div1;
			}
		}
	}

	return 0;
}

int lcd_clk_config_print_dft(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lcd%d clk config:\n"
		"pll_id:           %d\n"
		"pll_offset:       %d\n"
		"pll_mode:         %d\n"
		"pll_m:            %d\n"
		"pll_n:            %d\n"
		"pll_frac:         0x%03x\n"
		"pll_frac_half_shift:  %d\n"
		"pll_fvco:         %dkHz\n"
		"pll_od1:          %d\n"
		"pll_od2:          %d\n"
		"pll_od3:          %d\n"
		"pll_tcon_div_sel: %d\n"
		"pll_out:          %dkHz\n"
		"edp_div0:         %d\n"
		"edp_div1:         %d\n"
		"div_sel:          %s(index %d)\n"
		"xd:               %d\n"
		"fout:             %dkHz\n"
		"ss_level:         %d\n"
		"ss_freq:          %d\n"
		"ss_mode:          %d\n"
		"ss_en:            %d\n",
		pdrv->index,
		cconf->pll_id, cconf->pll_offset,
		cconf->pll_mode, cconf->pll_m, cconf->pll_n,
		cconf->pll_frac, cconf->pll_frac_half_shift,
		cconf->pll_fvco,
		cconf->pll_od1_sel, cconf->pll_od2_sel,
		cconf->pll_od3_sel, cconf->pll_tcon_div_sel,
		cconf->pll_fout,
		cconf->edp_div0, cconf->edp_div1,
		lcd_clk_div_sel_table[cconf->div_sel],
		cconf->div_sel, cconf->xd,
		cconf->fout, cconf->ss_level,
		cconf->ss_freq, cconf->ss_mode, cconf->ss_en);

	return len;
}

void lcd_pll_frac_generate_dft(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int pll_fout;
	unsigned int clk_div_in, clk_div_out, clk_div_sel;
	unsigned int od1, od2, od3, pll_fvco;
	unsigned int m, n, od_fb, frac_range, frac, offset, temp;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	cconf->fout = pconf->timing.lcd_clk / 1000; /* kHz */
	clk_div_sel = cconf->div_sel;
	od1 = od_table[cconf->pll_od1_sel];
	od2 = od_table[cconf->pll_od2_sel];
	od3 = od_table[cconf->pll_od3_sel];
	m = cconf->pll_m;
	n = cconf->pll_n;
	od_fb = cconf->pll_od_fb;
	frac_range = cconf->data->pll_frac_range;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("m=%d, n=%d, od1=%d, od2=%d, od3=%d\n",
		      m, n, cconf->pll_od1_sel, cconf->pll_od2_sel,
		      cconf->pll_od3_sel);
		LCDPR("clk_div_sel=%s(index %d), xd=%d\n",
		      lcd_clk_div_sel_table[clk_div_sel],
		      clk_div_sel, cconf->xd);
	}
	if (cconf->fout > cconf->data->xd_out_fmax) {
		LCDERR("%s: wrong lcd_clk value %dkHz\n",
		       __func__, cconf->fout);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s pclk=%d\n", __func__, cconf->fout);

	clk_div_out = cconf->fout * cconf->xd;
	if (clk_div_out > cconf->data->div_out_fmax) {
		LCDERR("%s: wrong clk_div_out value %dkHz\n",
		       __func__, clk_div_out);
		return;
	}

	clk_div_in =
		clk_vid_pll_div_calc(clk_div_out, clk_div_sel, CLK_DIV_O2I);
	if (clk_div_in > cconf->data->div_in_fmax) {
		LCDERR("%s: wrong clk_div_in value %dkHz\n",
		       __func__, clk_div_in);
		return;
	}

	pll_fout = clk_div_in;
	if (pll_fout > cconf->data->pll_out_fmax ||
	    pll_fout < cconf->data->pll_out_fmin) {
		LCDERR("%s: wrong pll_fout value %dkHz\n", __func__, pll_fout);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s pll_fout=%d\n", __func__, pll_fout);

	pll_fvco = pll_fout * od1 * od2 * od3;
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
		LCDERR("%s: pll changing %dkHz is too much\n",
		       __func__, temp);
		return;
	}
	frac = temp * frac_range * n / cconf->fin;
	if (cconf->pll_mode & LCD_PLL_MODE_FRAC_SHIFT) {
		if ((frac == (frac_range >> 1)) || (frac == (frac_range >> 2))) {
			frac |= 0x66;
			cconf->pll_frac_half_shift = 1;
		} else {
			cconf->pll_frac_half_shift = 0;
		}
	}
	cconf->pll_frac = frac | (offset << cconf->data->pll_frac_sign_bit);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd_pll_frac_generate: frac=0x%x\n", frac);
}

void lcd_clk_disable_dft(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	int i = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	lcd_clk_setb(HHI_VID_CLK_CNTL2, 0, ENCL_GATE_VCLK, 1);

	/* close vclk2_div gate: 0x104b[4:0] */
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, 0, 5);
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);

	if (!cconf->data->pll_ctrl_table)
		return;
	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_END)
			break;
		if (table[i].flag == LCD_CLK_CTRL_EN)
			lcd_ana_setb(table[i].reg, 0, table[i].bit, table[i].len);
		else if (table[i].flag == LCD_CLK_CTRL_RST)
			lcd_ana_setb(table[i].reg, 1, table[i].bit, table[i].len);
		i++;
	}
}

void lcd_clk_gate_switch_dft(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (status) {
		if (IS_ERR_OR_NULL(cconf->clktree.encl_top_gate))
			LCDERR("%s: encl_top_gate\n", __func__);
		else
			clk_prepare_enable(cconf->clktree.encl_top_gate);
		if (IS_ERR_OR_NULL(cconf->clktree.encl_int_gate))
			LCDERR("%s: encl_int_gata\n", __func__);
		else
			clk_prepare_enable(cconf->clktree.encl_int_gate);
	} else {
		if (IS_ERR_OR_NULL(cconf->clktree.encl_int_gate))
			LCDERR("%s: encl_int_gata\n", __func__);
		else
			clk_disable_unprepare(cconf->clktree.encl_int_gate);
		if (IS_ERR_OR_NULL(cconf->clktree.encl_top_gate))
			LCDERR("%s: encl_top_gata\n", __func__);
		else
			clk_disable_unprepare(cconf->clktree.encl_top_gate);
	}
}

void lcd_clk_gate_optional_switch_fdt(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (status) {
		switch (pdrv->config.basic.lcd_type) {
		case LCD_MLVDS:
		case LCD_P2P:
			if (IS_ERR_OR_NULL(cconf->clktree.tcon_gate))
				LCDERR("%s: tcon_gate\n", __func__);
			else
				clk_prepare_enable(cconf->clktree.tcon_gate);
			if (IS_ERR_OR_NULL(cconf->clktree.tcon_clk))
				LCDERR("%s: tcon_clk\n", __func__);
			else
				clk_prepare_enable(cconf->clktree.tcon_clk);
			cconf->clktree.clk_gate_optional_state = 1;
			break;
		default:
			break;
		}
	} else {
		switch (pdrv->config.basic.lcd_type) {
		case LCD_MLVDS:
		case LCD_P2P:
			if (IS_ERR_OR_NULL(cconf->clktree.tcon_clk))
				LCDERR("%s: tcon_clk\n", __func__);
			else
				clk_disable_unprepare(cconf->clktree.tcon_clk);
			if (IS_ERR_OR_NULL(cconf->clktree.tcon_gate))
				LCDERR("%s: tcon_gate\n", __func__);
			else
				clk_disable_unprepare(cconf->clktree.tcon_gate);
			cconf->clktree.clk_gate_optional_state = 0;
			break;
		default:
			break;
		}
	}
}

void lcd_set_vid_pll_div_dft(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int shift_val, shift_sel;
	int i;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);

	// lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	// udelay(5);

	/* Disable the div output clock */
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	i = 0;
	while (lcd_clk_div_table[i][0] != CLK_DIV_SEL_MAX) {
		if (cconf->div_sel == lcd_clk_div_table[i][0])
			break;
		i++;
	}
	if (lcd_clk_div_table[i][0] == CLK_DIV_SEL_MAX)
		LCDERR("invalid clk divider\n");
	shift_val = lcd_clk_div_table[i][1];
	shift_sel = lcd_clk_div_table[i][2];

	if (shift_val == 0xffff) { /* if divide by 1 */
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 18, 1);
	} else {
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 18, 1);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 16, 2);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 0, 14);

		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, shift_sel, 16, 2);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 15, 1);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, shift_val, 0, 15);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);
	}
	/* Enable the final output clock */
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);
}

void lcd_clk_generate_dft(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int pll_fout, pll_fvco, bit_rate;
	unsigned int clk_div_in, clk_div_out;
	unsigned int clk_div_sel, xd, tcon_div_sel = 0, phy_div = 1;
	unsigned int od1, od2, od3;
	unsigned int bit_rate_max = 0, bit_rate_min = 0, tmp;
	unsigned int tmp_div;
	int done;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	done = 0;
	cconf->fout = pconf->timing.lcd_clk / 1000; /* kHz */
	cconf->err_fmin = MAX_ERROR;

	if (cconf->fout > cconf->data->xd_out_fmax) {
		LCDERR("%s: wrong lcd_clk value %dkHz\n", __func__, cconf->fout);
		goto generate_clk_done_tl1;
	}

	bit_rate = pconf->timing.bit_rate / 1000;

	cconf->pll_mode = pconf->timing.clk_auto;

	switch (pconf->basic.lcd_type) {
	case LCD_TTL:
		clk_div_sel = CLK_DIV_SEL_1;
		cconf->xd_max = CRT_VID_DIV_MAX;
		for (xd = 1; xd <= cconf->xd_max; xd++) {
			clk_div_out = cconf->fout * xd;
			if (clk_div_out > cconf->data->div_out_fmax)
				continue;
			if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
				LCDPR("fout=%d, xd=%d, clk_div_out=%d\n",
				      cconf->fout, xd, clk_div_out);
			}
			clk_div_in = clk_vid_pll_div_calc(clk_div_out, clk_div_sel, CLK_DIV_O2I);
			if (clk_div_in > cconf->data->div_in_fmax)
				continue;
			cconf->xd = xd;
			cconf->div_sel = clk_div_sel;
			pll_fout = clk_div_in;
			if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
				LCDPR("clk_div_sel=%s(index %d), pll_fout=%d\n",
				      lcd_clk_div_sel_table[clk_div_sel],
				      clk_div_sel, pll_fout);
			}
			done = check_pll_3od(cconf, pll_fout);
			if (done)
				goto generate_clk_done_tl1;
		}
		break;
	case LCD_LVDS:
		clk_div_sel = CLK_DIV_SEL_7;
		xd = 1;
		clk_div_out = cconf->fout * xd;
		if (clk_div_out > cconf->data->div_out_fmax)
			goto generate_clk_done_tl1;
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
			LCDPR("fout=%d, xd=%d, clk_div_out=%d\n",
			      cconf->fout, xd, clk_div_out);
		}
		clk_div_in = clk_vid_pll_div_calc(clk_div_out, clk_div_sel, CLK_DIV_O2I);
		if (clk_div_in > cconf->data->div_in_fmax)
			goto generate_clk_done_tl1;
		cconf->xd = xd;
		cconf->div_sel = clk_div_sel;
		pll_fout = clk_div_in;
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
			LCDPR("clk_div_sel=%s(index %d), pll_fout=%d\n",
			      lcd_clk_div_sel_table[clk_div_sel],
			      clk_div_sel, pll_fout);
		}
		done = check_pll_3od(cconf, pll_fout);
		if (done == 0)
			goto generate_clk_done_tl1;
		done = 0;
		if (pconf->control.lvds_cfg.dual_port)
			phy_div = 2;
		else
			phy_div = 1;
		od1 = od_table[cconf->pll_od1_sel];
		od2 = od_table[cconf->pll_od2_sel];
		od3 = od_table[cconf->pll_od3_sel];
		for (tcon_div_sel = 0; tcon_div_sel < 5; tcon_div_sel++) {
			if (tcon_div_table[tcon_div_sel] == phy_div * od1 * od2 * od3) {
				cconf->pll_tcon_div_sel = tcon_div_sel;
				done = 1;
				break;
			}
		}
		break;
	case LCD_VBYONE:
		cconf->div_sel_max = CLK_DIV_SEL_MAX;
		cconf->xd_max = CRT_VID_DIV_MAX;
		pll_fout = bit_rate;
		clk_div_in = pll_fout;
		if (clk_div_in > cconf->data->div_in_fmax)
			goto generate_clk_done_tl1;
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
			LCDPR("pll_fout=%d\n", pll_fout);
		if ((clk_div_in / cconf->fout) > 15)
			cconf->xd = 4;
		else
			cconf->xd = 1;
		clk_div_out = cconf->fout * cconf->xd;
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
			LCDPR("clk_div_in=%d, fout=%d, xd=%d, clk_div_out=%d\n",
			      clk_div_in, cconf->fout, cconf->xd, clk_div_out);
		}
		if (clk_div_out > cconf->data->div_out_fmax)
			goto generate_clk_done_tl1;
		clk_div_sel = clk_vid_pll_div_get(clk_div_in * 100 / clk_div_out);
		if (clk_div_sel == CLK_DIV_SEL_MAX) {
			clk_div_sel = CLK_DIV_SEL_1;
			cconf->xd *= clk_div_in / clk_div_out;
		} else {
			cconf->div_sel = clk_div_sel;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
			LCDPR("clk_div_sel=%s(index %d), xd=%d\n",
			      lcd_clk_div_sel_table[clk_div_sel],
			      cconf->div_sel, cconf->xd);
		}
		done = check_pll_3od(cconf, pll_fout);
		if (done == 0)
			goto generate_clk_done_tl1;
		done = 0;
		od1 = od_table[cconf->pll_od1_sel];
		od2 = od_table[cconf->pll_od2_sel];
		od3 = od_table[cconf->pll_od3_sel];
		for (tcon_div_sel = 0; tcon_div_sel < 5; tcon_div_sel++) {
			if (tcon_div_table[tcon_div_sel] == od1 * od2 * od3) {
				cconf->pll_tcon_div_sel = tcon_div_sel;
				done = 1;
				break;
			}
		}
		break;
	case LCD_MLVDS:
		/* must go through div4 for clk phase */
		for (tcon_div_sel = 3; tcon_div_sel < 5; tcon_div_sel++) {
			pll_fvco = bit_rate * tcon_div_table[tcon_div_sel];
			done = check_vco(cconf, pll_fvco);
			if (done == 0)
				continue;
			cconf->xd_max = CRT_VID_DIV_MAX;
			for (xd = 1; xd <= cconf->xd_max; xd++) {
				clk_div_out = cconf->fout * xd;
				if (clk_div_out > cconf->data->div_out_fmax)
					continue;
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
					LCDPR("fout=%d, xd=%d, clk_div_out=%d\n",
					      cconf->fout, xd, clk_div_out);
				}
				for (clk_div_sel = CLK_DIV_SEL_1; clk_div_sel < CLK_DIV_SEL_MAX;
				     clk_div_sel++) {
					clk_div_in = clk_vid_pll_div_calc(clk_div_out,
							     clk_div_sel, CLK_DIV_O2I);
					if (clk_div_in > cconf->data->div_in_fmax)
						continue;
					cconf->xd = xd;
					cconf->div_sel = clk_div_sel;
					cconf->pll_tcon_div_sel = tcon_div_sel;
					pll_fout = clk_div_in;
					if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
						LCDPR("clk_div_sel=%s(%d)\n",
						      lcd_clk_div_sel_table[clk_div_sel],
						      clk_div_sel);
						LCDPR("pll_fout=%d, tcon_div_sel=%d\n",
						      pll_fout, tcon_div_sel);
					}
					done = check_od(cconf, pll_fout);
					if (done)
						goto generate_clk_done_tl1;
				}
			}
		}
		break;
	case LCD_P2P:
		for (tcon_div_sel = 0; tcon_div_sel < 5; tcon_div_sel++) {
			pll_fvco = bit_rate * tcon_div_table[tcon_div_sel];
			done = check_vco(cconf, pll_fvco);
			if (done == 0)
				continue;
			cconf->xd_max = CRT_VID_DIV_MAX;
			for (xd = 1; xd <= cconf->xd_max; xd++) {
				clk_div_out = cconf->fout * xd;
				if (clk_div_out > cconf->data->div_out_fmax)
					continue;
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
					LCDPR("fout=%d, xd=%d, clk_div_out=%d\n",
					      cconf->fout, xd, clk_div_out);
				}
				for (clk_div_sel = CLK_DIV_SEL_1; clk_div_sel < CLK_DIV_SEL_MAX;
				     clk_div_sel++) {
					clk_div_in = clk_vid_pll_div_calc(clk_div_out, clk_div_sel,
									  CLK_DIV_O2I);
					if (clk_div_in > cconf->data->div_in_fmax)
						continue;
					cconf->xd = xd;
					cconf->div_sel = clk_div_sel;
					cconf->pll_tcon_div_sel = tcon_div_sel;
					pll_fout = clk_div_in;
					if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
						LCDPR("clk_div_sel=%s(%d)\n",
						      lcd_clk_div_sel_table[clk_div_sel],
						      clk_div_sel);
						LCDPR("pll_fout=%d, tcon_div_sel=%d\n",
						      pll_fout, tcon_div_sel);
					}
					done = check_od(cconf, pll_fout);
					if (done)
						goto generate_clk_done_tl1;
				}
			}
		}
		break;
	case LCD_MIPI:
		cconf->xd_max = CRT_VID_DIV_MAX;
		bit_rate_max = pconf->control.mipi_cfg.local_bit_rate_max;
		bit_rate_min = pconf->control.mipi_cfg.local_bit_rate_min;
		tmp = bit_rate_max - cconf->fout;
		if (tmp >= bit_rate_min)
			bit_rate_min = tmp;

		clk_div_sel = CLK_DIV_SEL_1;
		for (xd = 1; xd <= cconf->xd_max; xd++) {
			pll_fout = cconf->fout * xd;
			if (pll_fout > bit_rate_max || pll_fout < bit_rate_min)
				continue;
			if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
				LCDPR("fout=%d, xd=%d\n", cconf->fout, xd);

			pconf->timing.bit_rate = pll_fout * 1000;
			pconf->control.mipi_cfg.clk_factor = xd;
			cconf->xd = xd;
			cconf->div_sel = clk_div_sel;
			cconf->pll_tcon_div_sel = 2;
			done = check_pll_3od(cconf, pll_fout);
			if (done)
				goto generate_clk_done_tl1;
		}
		break;
	case LCD_EDP:
		switch (pconf->control.edp_cfg.link_rate) {
		case 0: /* 1.62G */
			cconf->pll_n = 1;
			cconf->pll_m = 135;
			cconf->pll_frac = 0x0;
			cconf->pll_fvco = 3240000;
			cconf->pll_fout = 1620000;
			bit_rate = 1620000;
			break;
		case 1: /* 2.7G */
		default:
			cconf->pll_n = 1;
			cconf->pll_m = 225;
			cconf->pll_frac = 0x0;
			cconf->pll_fvco = 5400000;
			cconf->pll_fout = 2700000;
			bit_rate = 2700000;
			break;
		}
		cconf->pll_od1_sel = 1;
		cconf->pll_od2_sel = 0;
		cconf->pll_od3_sel = 0;
		cconf->pll_frac_half_shift = 0;
		cconf->div_sel = CLK_DIV_SEL_1;
		cconf->xd = 1;
		cconf->err_fmin = 10000; /* 10M basic error */
		for (tcon_div_sel = 0; tcon_div_sel < 5; tcon_div_sel++) {
			if (tcon_div_table[tcon_div_sel] != cconf->pll_fvco / bit_rate)
				continue;
			cconf->pll_tcon_div_sel = tcon_div_sel;
			if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
				LCDPR("bit_rate=%d, tcon_div=%d\n",
				      bit_rate, tcon_div_table[tcon_div_sel]);
			}
			if (edp_div_check(cconf, bit_rate) == 0)
				done = 1;
		}
		if (done == 0)
			break;
		tmp_div = edp_div0_table[cconf->edp_div0] * edp_div1_table[cconf->edp_div1];
		cconf->fout = bit_rate / tmp_div;
		pconf->timing.lcd_clk = cconf->fout * 1000; /* Hz */
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
			LCDPR("final fout=%d, tmp_div=%d, edp_div0=%d, edp_div1=%d\n",
			      cconf->fout, tmp_div,
			      cconf->edp_div0, cconf->edp_div1);
		}
		break;
	default:
		break;
	}

generate_clk_done_tl1:
	if (done) {
		pconf->timing.pll_ctrl =
			(cconf->pll_od1_sel << PLL_CTRL_OD1) |
			(cconf->pll_od2_sel << PLL_CTRL_OD2) |
			(cconf->pll_od3_sel << PLL_CTRL_OD3) |
			(cconf->pll_n << PLL_CTRL_N)         |
			(cconf->pll_m << PLL_CTRL_M);
		pconf->timing.div_ctrl =
			(cconf->div_sel << DIV_CTRL_DIV_SEL) |
			(cconf->xd << DIV_CTRL_XD);
		pconf->timing.clk_ctrl =
			(cconf->pll_frac << CLK_CTRL_FRAC) |
			(cconf->pll_frac_half_shift << CLK_CTRL_FRAC_SHIFT);
		cconf->done = 1;
	} else {
		pconf->timing.pll_ctrl =
			(1 << PLL_CTRL_OD1) |
			(1 << PLL_CTRL_OD2) |
			(1 << PLL_CTRL_OD3) |
			(1 << PLL_CTRL_N)   |
			(50 << PLL_CTRL_M);
		pconf->timing.div_ctrl =
			(CLK_DIV_SEL_1 << DIV_CTRL_DIV_SEL) |
			(7 << DIV_CTRL_XD);
		pconf->timing.clk_ctrl = (0 << CLK_CTRL_FRAC);
		cconf->done = 0;
		LCDERR("[%d]: %s: Out of clock range\n", pdrv->index, __func__);
	}
}

void lcd_clk_config_init_print_dft(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_data_s *data;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	data = cconf->data;
	LCDPR("lcd%d clk config data init:\n"
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
		"div_in_fmax:       %d\n"
		"div_out_fmax:      %d\n"
		"xd_out_fmax:       %d\n"
		"ss_level_max:      %d\n"
		"ss_freq_max:       %d\n"
		"ss_mode_max:       %d\n\n",
		pdrv->index,
		data->pll_m_max, data->pll_m_min,
		data->pll_n_max, data->pll_n_min,
		data->pll_od_fb, data->pll_frac_range,
		data->pll_od_sel_max,
		data->pll_ref_fmax, data->pll_ref_fmin,
		data->pll_vco_fmax, data->pll_vco_fmin,
		data->pll_out_fmax, data->pll_out_fmin,
		data->div_in_fmax, data->div_out_fmax,
		data->xd_out_fmax, ss_level_max,
		ss_freq_max, ss_mode_max);
}

#define CLK_CHK_MAX    2000000  /*Hz*/
unsigned long lcd_encl_clk_check_std = 121000000;
unsigned long lcd_fifo_clk_check_std = 42000000;

int lcd_prbs_clk_check(unsigned long encl_clk, unsigned int encl_msr_id,
			      unsigned long fifo_clk, unsigned int fifo_msr_id, unsigned int cnt)
{
	unsigned long clk_check, temp;

	if (encl_msr_id == LCD_CLK_MSR_INVALID)
		goto lcd_prbs_clk_check_next;
	clk_check = meson_clk_measure(encl_msr_id);
	if (clk_check != encl_clk) {
		temp = lcd_abs(clk_check, encl_clk);
		if (temp >= CLK_CHK_MAX) {
			if (lcd_debug_print_flag & LCD_DBG_PR_TEST) {
				LCDERR("encl clkmsr error %ld, cnt:%d\n",
				       clk_check, cnt);
			}
			return -1;
		}
	}

lcd_prbs_clk_check_next:
	if (encl_msr_id == LCD_CLK_MSR_INVALID)
		return 0;
	clk_check = meson_clk_measure(fifo_msr_id);
	if (clk_check != fifo_clk) {
		temp = lcd_abs(clk_check, fifo_clk);
		if (temp >= CLK_CHK_MAX) {
			if (lcd_debug_print_flag & LCD_DBG_PR_TEST) {
				LCDERR("fifo clkmsr error %ld, cnt:%d\n",
				       clk_check, cnt);
			}
			return -1;
		}
	}

	return 0;
}
