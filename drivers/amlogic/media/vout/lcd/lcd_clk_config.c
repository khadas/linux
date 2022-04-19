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
#include "lcd_reg.h"
#include "lcd_common.h"
#include "lcd_clk_config.h"
#include "lcd_clk_ctrl.h"

/* for lcd clk init */
spinlock_t lcd_clk_lock;

struct lcd_clk_config_s *get_lcd_clk_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (!pdrv->clk_conf) {
		LCDERR("[%d]: %s: clk_config is null\n", pdrv->index, __func__);
		return NULL;
	}
	cconf = (struct lcd_clk_config_s *)pdrv->clk_conf;
	if (!cconf->data) {
		LCDERR("[%d]: %s: clk config data is null\n",
		       pdrv->index, __func__);
		return NULL;
	}

	return cconf;
}

/* ****************************************************
 * lcd pll & clk operation
 * ****************************************************
 */
static unsigned int error_abs(unsigned int a, unsigned int b)
{
	if (a >= b)
		return (a - b);
	else
		return (b - a);
}

#define PLL_CLK_CHECK_MAX    2000000 /* Hz */
static int lcd_clk_msr_check(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int encl_clk_msr;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return 0;

	if (cconf->data->enc_clk_msr_id == -1)
		return 0;

	encl_clk_msr = lcd_encl_clk_msr(pdrv);
	if (error_abs((cconf->fout * 1000), encl_clk_msr) >= PLL_CLK_CHECK_MAX) {
		LCDERR("[%d]: %s: expected:%d, msr:%d\n",
		       pdrv->index, __func__,
		       (cconf->fout * 1000), encl_clk_msr);
		return -1;
	}

	return 0;
}

static int lcd_pll_ss_level_generate(unsigned int *data, unsigned int level, unsigned int step)
{
	unsigned int max = 10, val;
	unsigned int dep_sel, str_m, temp, min;

	if (!data)
		return -1;

	val = level * 1000;
	min = val;
	for (dep_sel = 1; dep_sel <= max; dep_sel++) { //dep_sel
		for (str_m = 1; str_m <= max; str_m++) { //str_m
			temp = error_abs((dep_sel * str_m * step), val);
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

static int lcd_pll_wait_lock(unsigned int reg, unsigned int lock_bit)
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

#define PLL_WAIT_LOCK_CNT_G12A    1000
static int lcd_pll_wait_lock_g12a(int path)
{
	unsigned int pll_ctrl, pll_ctrl3, pll_ctrl6;
	unsigned int pll_lock;
	int wait_loop = PLL_WAIT_LOCK_CNT_G12A; /* 200 */
	int ret = 0;

	if (path) {
		pll_ctrl = HHI_GP0_PLL_CNTL0;
		pll_ctrl3 = HHI_GP0_PLL_CNTL3;
		pll_ctrl6 = HHI_GP0_PLL_CNTL6;
	} else {
		pll_ctrl = HHI_HDMI_PLL_CNTL0;
		pll_ctrl3 = HHI_HDMI_PLL_CNTL3;
		pll_ctrl6 = HHI_HDMI_PLL_CNTL6;
	}
	do {
		udelay(50);
		pll_lock = lcd_ana_getb(pll_ctrl, 31, 1);
		wait_loop--;
	} while ((pll_lock != 1) && (wait_loop > 0));

	if (pll_lock == 1) {
		goto pll_lock_end_g12a;
	} else {
		LCDPR("path: %d, pll try 1, lock: %d\n", path, pll_lock);
		lcd_ana_setb(pll_ctrl3, 1, 31, 1);
		wait_loop = PLL_WAIT_LOCK_CNT_G12A;
		do {
			udelay(50);
			pll_lock = lcd_ana_getb(pll_ctrl, 31, 1);
			wait_loop--;
		} while ((pll_lock != 1) && (wait_loop > 0));
	}

	if (pll_lock == 1) {
		goto pll_lock_end_g12a;
	} else {
		LCDPR("path: %d, pll try 2, lock: %d\n", path, pll_lock);
		lcd_ana_write(pll_ctrl6, 0x55540000);
		wait_loop = PLL_WAIT_LOCK_CNT_G12A;
		do {
			udelay(50);
			pll_lock = lcd_ana_getb(pll_ctrl, 31, 1);
			wait_loop--;
		} while ((pll_lock != 1) && (wait_loop > 0));
	}

	if (pll_lock != 1)
		ret = -1;

pll_lock_end_g12a:
	LCDPR("%s: path=%d, pll_lock=%d, wait_loop=%d\n",
	      __func__, path, pll_lock, (PLL_WAIT_LOCK_CNT_G12A - wait_loop));
	return ret;
}

static void lcd_set_gp0_pll_g12a(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl, pll_ctrl1, pll_ctrl3, pll_ctrl4, pll_ctrl6;
	int ret, cnt = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl = ((1 << LCD_PLL_EN_GP0_G12A) |
		(cconf->pll_n << LCD_PLL_N_GP0_G12A) |
		(cconf->pll_m << LCD_PLL_M_GP0_G12A) |
		(cconf->pll_od1_sel << LCD_PLL_OD_GP0_G12A));
	pll_ctrl1 = (cconf->pll_frac << 0);
	if (cconf->pll_frac) {
		pll_ctrl |= (1 << 27);
		pll_ctrl3 = 0x6a285c00;
		pll_ctrl4 = 0x65771290;
		pll_ctrl6 = 0x56540000;
	} else {
		pll_ctrl3 = 0x48681c00;
		pll_ctrl4 = 0x33771290;
		pll_ctrl6 = 0x56540000;
	}

set_gp0_pll_retry_g12a:
	lcd_ana_write(HHI_GP0_PLL_CNTL0, pll_ctrl);
	lcd_ana_write(HHI_GP0_PLL_CNTL1, pll_ctrl1);
	lcd_ana_write(HHI_GP0_PLL_CNTL2, 0x00);
	lcd_ana_write(HHI_GP0_PLL_CNTL3, pll_ctrl3);
	lcd_ana_write(HHI_GP0_PLL_CNTL4, pll_ctrl4);
	lcd_ana_write(HHI_GP0_PLL_CNTL5, 0x39272000);
	lcd_ana_write(HHI_GP0_PLL_CNTL6, pll_ctrl6);
	lcd_ana_setb(HHI_GP0_PLL_CNTL0, 1, LCD_PLL_RST_GP0_G12A, 1);
	udelay(100);
	lcd_ana_setb(HHI_GP0_PLL_CNTL0, 0, LCD_PLL_RST_GP0_G12A, 1);

	ret = lcd_pll_wait_lock_g12a(1);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_gp0_pll_retry_g12a;
		LCDERR("gp0_pll lock failed\n");
	}
}

static void lcd_set_hpll_g12a(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl, pll_ctrl1, pll_ctrl3, pll_ctrl4, pll_ctrl6;
	int ret, cnt = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl = ((1 << LCD_PLL_EN_HPLL_G12A) |
		(1 << 25) | /* clk out gate */
		(cconf->pll_n << LCD_PLL_N_HPLL_G12A) |
		(cconf->pll_m << LCD_PLL_M_HPLL_G12A) |
		(cconf->pll_od1_sel << LCD_PLL_OD1_HPLL_G12A) |
		(cconf->pll_od2_sel << LCD_PLL_OD2_HPLL_G12A) |
		(cconf->pll_od3_sel << LCD_PLL_OD3_HPLL_G12A));
	pll_ctrl1 = (cconf->pll_frac << 0);
	if (cconf->pll_frac) {
		pll_ctrl |= (1 << 27);
		pll_ctrl3 = 0x6a285c00;
		pll_ctrl4 = 0x65771290;
		pll_ctrl6 = 0x56540000;
	} else {
		pll_ctrl3 = 0x48681c00;
		pll_ctrl4 = 0x33771290;
		pll_ctrl6 = 0x56540000;
	}

set_hpll_pll_retry_g12a:
	lcd_ana_write(HHI_HDMI_PLL_CNTL0, pll_ctrl);
	lcd_ana_write(HHI_HDMI_PLL_CNTL1, pll_ctrl1);
	lcd_ana_write(HHI_HDMI_PLL_CNTL2, 0x00);
	lcd_ana_write(HHI_HDMI_PLL_CNTL3, pll_ctrl3);
	lcd_ana_write(HHI_HDMI_PLL_CNTL4, pll_ctrl4);
	lcd_ana_write(HHI_HDMI_PLL_CNTL5, 0x39272000);
	lcd_ana_write(HHI_HDMI_PLL_CNTL6, pll_ctrl6);
	lcd_ana_setb(HHI_HDMI_PLL_CNTL0, 1, LCD_PLL_RST_HPLL_G12A, 1);
	udelay(100);
	lcd_ana_setb(HHI_HDMI_PLL_CNTL0, 0, LCD_PLL_RST_HPLL_G12A, 1);

	ret = lcd_pll_wait_lock_g12a(0);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_hpll_pll_retry_g12a;
		LCDERR("hpll lock failed\n");
	}
}

static void lcd_set_gp0_pll_g12b(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl, pll_ctrl1, pll_ctrl3, pll_ctrl4, pll_ctrl6;
	int ret, cnt = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl = ((1 << LCD_PLL_EN_GP0_G12A) |
		(cconf->pll_n << LCD_PLL_N_GP0_G12A) |
		(cconf->pll_m << LCD_PLL_M_GP0_G12A) |
		(cconf->pll_od1_sel << LCD_PLL_OD_GP0_G12A));
	pll_ctrl1 = (cconf->pll_frac << 0);
	if (cconf->pll_frac) {
		pll_ctrl |= (1 << 27);
		pll_ctrl3 = 0x6a285c00;
		pll_ctrl4 = 0x65771290;
		pll_ctrl6 = 0x56540000;
	} else {
		pll_ctrl3 = 0x48681c00;
		pll_ctrl4 = 0x33771290;
		pll_ctrl6 = 0x56540000;
	}

set_gp0_pll_retry_g12b:
	lcd_ana_write(HHI_GP0_PLL_CNTL0, pll_ctrl);
	lcd_ana_write(HHI_GP0_PLL_CNTL1, pll_ctrl1);
	lcd_ana_write(HHI_GP0_PLL_CNTL2, 0x00);
	lcd_ana_write(HHI_GP0_PLL_CNTL3, pll_ctrl3);
	lcd_ana_write(HHI_GP0_PLL_CNTL4, pll_ctrl4);
	lcd_ana_write(HHI_GP0_PLL_CNTL5, 0x39272000);
	lcd_ana_write(HHI_GP0_PLL_CNTL6, pll_ctrl6);
	lcd_ana_setb(HHI_GP0_PLL_CNTL0, 1, LCD_PLL_RST_GP0_G12A, 1);
	udelay(100);
	lcd_ana_setb(HHI_GP0_PLL_CNTL0, 0, LCD_PLL_RST_GP0_G12A, 1);

	ret = lcd_pll_wait_lock(HHI_GP0_PLL_CNTL0, LCD_PLL_LOCK_GP0_G12A);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_gp0_pll_retry_g12b;
		LCDERR("gp0_pll lock failed\n");
	}
}

static void lcd_set_hpll_g12b(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl, pll_ctrl1, pll_ctrl3, pll_ctrl4, pll_ctrl6;
	int ret, cnt = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl = ((1 << LCD_PLL_EN_HPLL_G12A) |
		(1 << 25) | /* clk out gate */
		(cconf->pll_n << LCD_PLL_N_HPLL_G12A) |
		(cconf->pll_m << LCD_PLL_M_HPLL_G12A) |
		(cconf->pll_od1_sel << LCD_PLL_OD1_HPLL_G12A) |
		(cconf->pll_od2_sel << LCD_PLL_OD2_HPLL_G12A) |
		(cconf->pll_od3_sel << LCD_PLL_OD3_HPLL_G12A));
	pll_ctrl1 = (cconf->pll_frac << 0);
	if (cconf->pll_frac) {
		pll_ctrl |= (1 << 27);
		pll_ctrl3 = 0x6a285c00;
		pll_ctrl4 = 0x65771290;
		pll_ctrl6 = 0x56540000;
	} else {
		pll_ctrl3 = 0x48681c00;
		pll_ctrl4 = 0x33771290;
		pll_ctrl6 = 0x56540000;
	}

set_hpll_pll_retry_g12b:
	lcd_ana_write(HHI_HDMI_PLL_CNTL0, pll_ctrl);
	lcd_ana_write(HHI_HDMI_PLL_CNTL1, pll_ctrl1);
	lcd_ana_write(HHI_HDMI_PLL_CNTL2, 0x00);
	lcd_ana_write(HHI_HDMI_PLL_CNTL3, pll_ctrl3);
	lcd_ana_write(HHI_HDMI_PLL_CNTL4, pll_ctrl4);
	lcd_ana_write(HHI_HDMI_PLL_CNTL5, 0x39272000);
	lcd_ana_write(HHI_HDMI_PLL_CNTL6, pll_ctrl6);
	lcd_ana_setb(HHI_HDMI_PLL_CNTL0, 1, LCD_PLL_RST_HPLL_G12A, 1);
	udelay(100);
	lcd_ana_setb(HHI_HDMI_PLL_CNTL0, 0, LCD_PLL_RST_HPLL_G12A, 1);

	ret = lcd_pll_wait_lock(HHI_HDMI_PLL_CNTL0, LCD_PLL_LOCK_HPLL_G12A);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_hpll_pll_retry_g12b;
		LCDERR("hpll lock failed\n");
	}
}

static void lcd_pll_ss_enable_tl1(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2;
	unsigned int level, flag;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	level = cconf->ss_level;
	pll_ctrl2 = lcd_ana_read(HHI_TCON_PLL_CNTL2);
	if (status) {
		if (level > 0)
			flag = 1;
		else
			flag = 0;
	} else {
		flag = 0;
	}
	if (flag) {
		cconf->ss_en = 1;
		pll_ctrl2 |= (1 << 15);
		LCDPR("pll ss enable: %s\n", lcd_ss_level_table_tl1[level]);
	} else {
		cconf->ss_en = 0;
		pll_ctrl2 &= ~(1 << 15);
		LCDPR("pll ss disable\n");
	}
	lcd_ana_write(HHI_TCON_PLL_CNTL2, pll_ctrl2);
}

static void lcd_set_pll_ss_level_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int level, pll_ctrl2;
	unsigned int dep_sel, str_m;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	level = cconf->ss_level;
	pll_ctrl2 = lcd_ana_read(HHI_TCON_PLL_CNTL2);
	pll_ctrl2 &= ~((1 << 15) | (0xf << 16) | (0xf << 28));

	if (level > 0) {
		cconf->ss_en = 1;
		dep_sel = pll_ss_reg_tl1[level][0];
		str_m = pll_ss_reg_tl1[level][1];
		dep_sel = (dep_sel > 10) ? 10 : dep_sel;
		str_m = (str_m > 10) ? 10 : str_m;
		pll_ctrl2 |= ((1 << 15) | (dep_sel << 28) | (str_m << 16));
	} else {
		cconf->ss_en = 0;
	}
	lcd_ana_write(HHI_TCON_PLL_CNTL2, pll_ctrl2);

	LCDPR("set pll spread spectrum: %s\n", lcd_ss_level_table_tl1[level]);
}

static void lcd_set_pll_ss_advance_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2;
	unsigned int freq, mode;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	freq = cconf->ss_freq;
	mode = cconf->ss_mode;
	pll_ctrl2 = lcd_ana_read(HHI_TCON_PLL_CNTL2);
	pll_ctrl2 &= ~(0x7 << 24); /* ss_freq */
	pll_ctrl2 |= (freq << 24);
	pll_ctrl2 &= ~(0x3 << 22); /* ss_mode */
	pll_ctrl2 |= (mode << 22);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, pll_ctrl2);

	LCDPR("set pll spread spectrum: freq=%d, mode=%d\n", freq, mode);
}

static void lcd_set_pll_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl, pll_ctrl1;
	unsigned int tcon_div_sel;
	int ret, cnt = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	tcon_div_sel = cconf->pll_tcon_div_sel;
	pll_ctrl = ((0x3 << 17) | /* gate ctrl */
		(tcon_div[tcon_div_sel][2] << 16) |
		(cconf->pll_n << LCD_PLL_N_TL1) |
		(cconf->pll_m << LCD_PLL_M_TL1) |
		(cconf->pll_od3_sel << LCD_PLL_OD3_TL1) |
		(cconf->pll_od2_sel << LCD_PLL_OD2_TL1) |
		(cconf->pll_od1_sel << LCD_PLL_OD1_TL1));
	pll_ctrl1 = (1 << 28) |
		(tcon_div[tcon_div_sel][0] << 22) |
		(tcon_div[tcon_div_sel][1] << 21) |
		((1 << 20) | /* sdm_en */
		(cconf->pll_frac << 0));

set_pll_retry_tl1:
	lcd_ana_write(HHI_TCON_PLL_CNTL0, pll_ctrl);
	udelay(10);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, LCD_PLL_RST_TL1, 1);
	udelay(10);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, LCD_PLL_EN_TL1, 1);
	udelay(10);
	lcd_ana_write(HHI_TCON_PLL_CNTL1, pll_ctrl1);
	udelay(10);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, 0x0000110c);
	udelay(10);
	lcd_ana_write(HHI_TCON_PLL_CNTL3, 0x10051400);
	udelay(10);
	lcd_ana_setb(HHI_TCON_PLL_CNTL4, 0x0100c0, 0, 24);
	udelay(10);
	lcd_ana_setb(HHI_TCON_PLL_CNTL4, 0x8300c0, 0, 24);
	udelay(10);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, 26, 1);
	udelay(10);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 0, LCD_PLL_RST_TL1, 1);
	udelay(10);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, 0x0000300c);

	ret = lcd_pll_wait_lock(HHI_TCON_PLL_CNTL0, LCD_PLL_LOCK_TL1);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_pll_retry_tl1;
		LCDERR("hpll lock failed\n");
	} else {
		udelay(100);
		lcd_ana_setb(HHI_TCON_PLL_CNTL2, 1, 5, 1);
	}

	if (cconf->ss_level > 0) {
		lcd_set_pll_ss_level_tl1(pdrv);
		lcd_set_pll_ss_advance_tl1(pdrv);
	}
}

static void lcd_pll_ss_enable_t7(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2, offset;
	unsigned int level, flag;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	level = cconf->ss_level;
	offset = cconf->pll_offset;
	pll_ctrl2 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2 + offset);
	if (status) {
		if (level > 0)
			flag = 1;
		else
			flag = 0;
	} else {
		flag = 0;
	}
	if (flag) {
		cconf->ss_en = 1;
		pll_ctrl2 |= (1 << 15);
		LCDPR("[%d]: pll ss enable: %dppm\n", pdrv->index, (level * 1000));
	} else {
		cconf->ss_en = 0;
		pll_ctrl2 &= ~(1 << 15);
		LCDPR("[%d]: pll ss disable\n", pdrv->index);
	}
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, pll_ctrl2);
}

static void lcd_set_pll_ss_level_t7(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2, offset;
	unsigned int level, dep_sel, str_m;
	unsigned int data[2] = {0, 0};
	int ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	level = cconf->ss_level;
	offset = cconf->pll_offset;
	pll_ctrl2 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2 + offset);
	pll_ctrl2 &= ~((1 << 15) | (0xf << 16) | (0xf << 28));

	if (level > 0) {
		cconf->ss_en = 1;
		ret = lcd_pll_ss_level_generate(data, level, 500);
		if (ret == 0) {
			dep_sel = data[0];
			str_m = data[1];
			dep_sel = (dep_sel > 10) ? 10 : dep_sel;
			str_m = (str_m > 10) ? 10 : str_m;
			pll_ctrl2 |= ((1 << 15) | (dep_sel << 28) |
				     (str_m << 16));
		}
	} else {
		cconf->ss_en = 0;
	}

	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, pll_ctrl2);

	if (level > 0) {
		LCDPR("[%d]: set pll spread spectrum: %dppm\n",
		      pdrv->index, (level * 1000));
	} else {
		LCDPR("[%d]: set pll spread spectrum: disable\n", pdrv->index);
	}
}

static void lcd_set_pll_ss_advance_t7(struct aml_lcd_drv_s *pdrv)
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
		lcd_set_pll_ss_level_t7(pdrv);
		lcd_set_pll_ss_advance_t7(pdrv);
	}
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
		lcd_set_pll_ss_level_t7(pdrv);
		lcd_set_pll_ss_advance_t7(pdrv);
	}
}

static void lcd_prbs_set_pll_vx1_tl1(struct aml_lcd_drv_s *pdrv)
{
	int cnt = 0, ret;

lcd_prbs_retry_pll_vx1_tl1:
	lcd_ana_write(HHI_TCON_PLL_CNTL0, 0x000f04f7);
	usleep_range(10, 12);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, LCD_PLL_RST_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, LCD_PLL_EN_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL1, 0x10110000);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, 0x00001108);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL3, 0x10051400);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL4, 0x010100c0);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL4, 0x038300c0);
	usleep_range(10, 12);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, 26, 1);
	usleep_range(10, 12);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 0, LCD_PLL_RST_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, 0x00003008);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, 0x00003028);
	usleep_range(10, 12);

	ret = lcd_pll_wait_lock(HHI_TCON_PLL_CNTL0, LCD_PLL_LOCK_TL1);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto lcd_prbs_retry_pll_vx1_tl1;
		LCDERR("pll lock failed\n");
	}

	/* pll_div */
	/* Disable the div output clock */
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 18, 1);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 16, 2);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 0, 14);

	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 2, 16, 2);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 15, 1);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0x739c, 0, 15);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	/* Enable the final output clock */
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);
}

static void lcd_prbs_set_pll_lvds_tl1(struct aml_lcd_drv_s *pdrv)
{
	int cnt = 0, ret;

lcd_prbs_retry_pll_lvds_tl1:
	lcd_ana_write(HHI_TCON_PLL_CNTL0, 0x008e049f);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL0, 0x208e049f);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL0, 0x3006049f);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL1, 0x10000000);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, 0x00001102);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL3, 0x10051400);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL4, 0x010100c0);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL4, 0x038300c0);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL0, 0x348e049f);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL0, 0x148e049f);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, 0x00003002);
	usleep_range(10, 12);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, 0x00003022);
	usleep_range(10, 12);

	ret = lcd_pll_wait_lock(HHI_TCON_PLL_CNTL0, LCD_PLL_LOCK_TL1);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto lcd_prbs_retry_pll_lvds_tl1;
		LCDERR("pll lock failed\n");
	}

	/* pll_div */
	/* Disable the div output clock */
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 18, 1);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 16, 2);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 0, 14);

	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 16, 2);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 15, 1);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0x3c78, 0, 15);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	/* Enable the final output clock */
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);
}

void lcd_prbs_config_clk_tl1(struct aml_lcd_drv_s *pdrv, unsigned int lcd_prbs_mode)
{
	if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
		lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
		lcd_prbs_set_pll_vx1_tl1(pdrv);
	} else if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
		lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
		lcd_prbs_set_pll_lvds_tl1(pdrv);
	} else {
		LCDERR("%s: unsupport lcd_prbs_mode %d\n",
		       __func__, lcd_prbs_mode);
		return;
	}

	lcd_clk_setb(HHI_VIID_CLK_DIV, 0, VCLK2_XD, 8);
	usleep_range(5, 10);

	/* select vid_pll_clk */
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_CLK_IN_SEL, 3);
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(HHI_VIID_CLK_DIV, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(HHI_VIID_CLK_DIV, 1, VCLK2_XD_EN, 2);
	usleep_range(5, 10);

	lcd_clk_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_SOFT_RST, 1);
	usleep_range(10, 12);
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_SOFT_RST, 1);
	usleep_range(5, 10);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(HHI_VID_CLK_CNTL2, 1, ENCL_GATE_VCLK, 1);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s ok\n", __func__);
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

static void lcd_prbs_config_clk_t5w(struct aml_lcd_drv_s *pdrv, unsigned int lcd_prbs_mode)
{
	unsigned int reg_vid2_clk_div, reg_vid2_clk_ctrl, reg_vid_clk_ctrl2;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	reg_vid2_clk_div = HHI_VIID_CLK0_DIV;
	reg_vid2_clk_ctrl = HHI_VIID_CLK0_CTRL;
	reg_vid_clk_ctrl2 = HHI_VID_CLK0_CTRL2;

	if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
		lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);
		lcd_prbs_set_pll_vx1_tl1(pdrv);
	} else if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
		lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);
		lcd_prbs_set_pll_lvds_tl1(pdrv);
	} else {
		LCDERR("[%d]: %s: unsupport lcd_prbs_mode %d\n",
		       pdrv->index, __func__, lcd_prbs_mode);
		return;
	}

	lcd_clk_setb(reg_vid2_clk_div, 0, VCLK2_XD, 8);
	udelay(5);

	/* select vid_pll_clk */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_CLK_IN_SEL, 3);
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_EN, 1);
	udelay(5);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(reg_vid2_clk_div, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(reg_vid2_clk_div, 1, VCLK2_XD_EN, 2);
	udelay(5);

	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_SOFT_RST, 1);
	udelay(5);
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(reg_vid_clk_ctrl2, 1, ENCL_GATE_VCLK, 1);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s ok\n", pdrv->index, __func__);
}

static void lcd_set_vid_pll_div(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int shift_val, shift_sel;
	int i;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);

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
		lcd_clk_setb(HHI_VIID_CLK_DIV, cconf->xd, VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(HHI_VIID_CLK_CNTL, 7, VCLK2_CLK_IN_SEL, 3);
	} else {
		/* setup the XD divider value */
		lcd_clk_setb(HHI_VIID_CLK_DIV, (cconf->xd - 1), VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(HHI_VIID_CLK_CNTL, cconf->data->vclk_sel, VCLK2_CLK_IN_SEL, 3);
	}
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	udelay(2);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(HHI_VIID_CLK_DIV, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(HHI_VIID_CLK_DIV, 1, VCLK2_XD_EN, 2);
	udelay(5);

	lcd_clk_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_SOFT_RST, 1);
	udelay(10);
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(HHI_VID_CLK_CNTL2, 1, ENCL_GATE_VCLK, 1);
}

static void lcd_set_vclk_crt_t7(struct aml_lcd_drv_s *pdrv)
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

static void lcd_set_vclk_crt_t5w(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (pdrv->lcd_pxp) {
		/* setup the XD divider value */
		lcd_clk_setb(HHI_VIID_CLK0_DIV, cconf->xd, VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(HHI_VIID_CLK0_CTRL, 7, VCLK2_CLK_IN_SEL, 3);
	} else {
		/* setup the XD divider value */
		lcd_clk_setb(HHI_VIID_CLK0_DIV, (cconf->xd - 1),
			     VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(HHI_VIID_CLK0_CTRL, cconf->data->vclk_sel,
			     VCLK2_CLK_IN_SEL, 3);
	}
	lcd_clk_setb(HHI_VIID_CLK0_CTRL, 1, VCLK2_EN, 1);
	udelay(2);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(HHI_VIID_CLK0_DIV, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(HHI_VIID_CLK0_DIV, 1, VCLK2_XD_EN, 2);
	udelay(5);

	lcd_clk_setb(HHI_VIID_CLK0_CTRL, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(HHI_VIID_CLK0_CTRL, 1, VCLK2_SOFT_RST, 1);
	udelay(10);
	lcd_clk_setb(HHI_VIID_CLK0_CTRL, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(HHI_VID_CLK0_CTRL2, 1, ENCL_GATE_VCLK, 1);
}

static void lcd_set_dsi_meas_clk(void)
{
	lcd_clk_setb(HHI_VDIN_MEAS_CLK_CNTL, 0, 21, 3);
	lcd_clk_setb(HHI_VDIN_MEAS_CLK_CNTL, 0, 12, 7);
	lcd_clk_setb(HHI_VDIN_MEAS_CLK_CNTL, 1, 20, 1);
}

static void lcd_set_dsi_phy_clk(int sel)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s\n", __func__);

	lcd_clk_setb(HHI_MIPIDSI_PHY_CLK_CNTL, sel, 12, 3);
	lcd_clk_setb(HHI_MIPIDSI_PHY_CLK_CNTL, 1, 8, 1);
	lcd_clk_setb(HHI_MIPIDSI_PHY_CLK_CNTL, 0, 0, 7);
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

static void lcd_set_tcon_clk_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int freq, val;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	switch (pconf->basic.lcd_type) {
	case LCD_MLVDS:
		val = pconf->control.mlvds_cfg.clk_phase & 0xfff;
		lcd_ana_setb(HHI_TCON_PLL_CNTL1, (val & 0xf), 24, 4);
		lcd_ana_setb(HHI_TCON_PLL_CNTL4, ((val >> 4) & 0xf), 28, 4);
		lcd_ana_setb(HHI_TCON_PLL_CNTL4, ((val >> 8) & 0xf), 24, 4);

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
}

static void lcd_set_tcon_clk_t5(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->config.basic.lcd_type != LCD_MLVDS &&
	    pdrv->config.basic.lcd_type != LCD_P2P)
		return;

	lcd_set_tcon_clk_tl1(pdrv);

	lcd_tcon_global_reset(pdrv);
}

static void lcd_set_tcon_clk_t3(struct aml_lcd_drv_s *pdrv)
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

/* ****************************************************
 * lcd clk parameters calculate
 * ****************************************************
 */
static unsigned int clk_vid_pll_div_calc(unsigned int clk,
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

static unsigned int clk_vid_pll_div_get(unsigned int clk_div)
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

static int check_pll_3od(struct lcd_clk_config_s *cconf,
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

static int check_pll_1od(struct lcd_clk_config_s *cconf, unsigned int pll_fout)
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

static int check_vco(struct lcd_clk_config_s *cconf, unsigned int pll_fvco)
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
static int check_od(struct lcd_clk_config_s *cconf, unsigned int pll_fout)
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
				if (error_abs(pll_fvco, cconf->pll_fvco) <
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

static int edp_div_check(struct lcd_clk_config_s *cconf, unsigned int bit_rate)
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
			tmp = error_abs(tmp, cconf->fout);
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

static void lcd_clk_generate_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int pll_fout, pll_fvco, bit_rate;
	unsigned int clk_div_in, clk_div_out;
	unsigned int clk_div_sel, xd, tcon_div_sel = 0, phy_div = 1;
	unsigned int od1, od2, od3;
	unsigned int dsi_bit_rate_max = 0, dsi_bit_rate_min = 0, tmp;
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
		tmp = pconf->control.mipi_cfg.bit_rate_max;
		dsi_bit_rate_max = tmp * 1000; /* change to kHz */
		dsi_bit_rate_min = dsi_bit_rate_max - cconf->fout;

		clk_div_sel = CLK_DIV_SEL_1;
		for (xd = 1; xd <= cconf->xd_max; xd++) {
			pll_fout = cconf->fout * xd;
			if (pll_fout > dsi_bit_rate_max || pll_fout < dsi_bit_rate_min)
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

static void lcd_pll_frac_generate_dft(struct aml_lcd_drv_s *pdrv)
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

static void lcd_clk_generate_g12a(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int pll_fout;
	unsigned int xd;
	unsigned int dsi_bit_rate_max = 0, dsi_bit_rate_min = 0;
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
		tmp = pconf->control.mipi_cfg.bit_rate_max;
		dsi_bit_rate_max = tmp * 1000; /* change to kHz */
		dsi_bit_rate_min = dsi_bit_rate_max - cconf->fout;

		for (xd = 1; xd <= cconf->xd_max; xd++) {
			pll_fout = cconf->fout * xd;
			if (pll_fout > dsi_bit_rate_max || pll_fout < dsi_bit_rate_min)
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

static void lcd_clk_generate_hpll_g12a(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int pll_fout;
	unsigned int clk_div_sel, xd;
	unsigned int dsi_bit_rate_max = 0, dsi_bit_rate_min = 0;
	unsigned int tmp;
	int done = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	cconf->fout = pconf->timing.lcd_clk / 1000; /* kHz */
	cconf->err_fmin = MAX_ERROR;

	if (cconf->fout > cconf->data->xd_out_fmax)
		LCDERR("%s: wrong lcd_clk value %dkHz\n", __func__, cconf->fout);

	switch (pconf->basic.lcd_type) {
	case LCD_MIPI:
		cconf->xd_max = CRT_VID_DIV_MAX;
		tmp = pconf->control.mipi_cfg.bit_rate_max;
		dsi_bit_rate_max = tmp * 1000; /* change to kHz */
		dsi_bit_rate_min = dsi_bit_rate_max - cconf->fout;

		clk_div_sel = CLK_DIV_SEL_1;
		for (xd = 1; xd <= cconf->xd_max; xd++) {
			pll_fout = cconf->fout * xd;
			if (pll_fout > dsi_bit_rate_max ||
			    pll_fout < dsi_bit_rate_min) {
				continue;
			}
			if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
				LCDPR("fout=%d, xd=%d\n", cconf->fout, xd);

			pconf->timing.bit_rate = pll_fout * 1000;
			pconf->control.mipi_cfg.clk_factor = xd;
			cconf->xd = xd;
			cconf->div_sel = clk_div_sel;
			done = check_pll_3od(cconf, pll_fout);
			if (done)
				goto generate_clk_done_hpll_g12a;
		}
		break;
	default:
		break;
	}

generate_clk_done_hpll_g12a:
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
			(cconf->pll_frac << CLK_CTRL_FRAC);
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
		LCDERR("Out of clock range\n");
	}
}

/* ****************************************************
 * lcd clk match function
 * ****************************************************
 */
static void lcd_clk_set_g12a_path0(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	lcd_set_hpll_g12a(pdrv);
	lcd_set_vid_pll_div(pdrv);
	lcd_set_dsi_meas_clk();
	lcd_set_dsi_phy_clk(0);
}

static void lcd_clk_set_g12a_path1(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	lcd_set_gp0_pll_g12a(pdrv);
	lcd_set_dsi_meas_clk();
	lcd_set_dsi_phy_clk(1);
}

static void lcd_clk_set_g12b_path0(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	lcd_set_hpll_g12b(pdrv);
	lcd_set_vid_pll_div(pdrv);
	lcd_set_dsi_meas_clk();
	lcd_set_dsi_phy_clk(0);
}

static void lcd_clk_set_g12b_path1(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	lcd_set_gp0_pll_g12b(pdrv);
	lcd_set_dsi_meas_clk();
	lcd_set_dsi_phy_clk(1);
}

static void lcd_clk_set_tl1(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	lcd_set_pll_tl1(pdrv);
	lcd_set_vid_pll_div(pdrv);
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

static void lcd_clk_set_t3(struct aml_lcd_drv_s *pdrv)
{
	lcd_set_pll_t3(pdrv);
	lcd_set_vid_pll_div_t3(pdrv);
}

static void lcd_clk_set_t5w(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(HHI_VIID_CLK0_CTRL, 0, VCLK2_EN, 1);
	lcd_set_pll_tl1(pdrv);
	lcd_set_vid_pll_div(pdrv);
}

static void lcd_clk_disable_dft(struct aml_lcd_drv_s *pdrv)
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

static void lcd_clk_disable_t7(struct aml_lcd_drv_s *pdrv)
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

static void lcd_clk_gate_switch_dft(struct aml_lcd_drv_s *pdrv, int status)
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

static void lcd_clk_gate_switch_g12a(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (status) {
		if (cconf->data->vclk_sel) {
			if (IS_ERR_OR_NULL(cconf->clktree.gp0_pll))
				LCDERR("%s: gp0_pll\n", __func__);
			else
				clk_prepare_enable(cconf->clktree.gp0_pll);
		}

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
		if (IS_ERR_OR_NULL(cconf->clktree.encl_top_gate))
			LCDERR("%s: encl_top_gate\n", __func__);
		else
			clk_prepare_enable(cconf->clktree.encl_top_gate);
		if (IS_ERR_OR_NULL(cconf->clktree.encl_int_gate))
			LCDERR("%s: encl_int_gata\n", __func__);
		else
			clk_prepare_enable(cconf->clktree.encl_int_gate);
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
		if (IS_ERR_OR_NULL(cconf->clktree.encl_int_gate))
			LCDERR("%s: encl_int_gate\n", __func__);
		else
			clk_disable_unprepare(cconf->clktree.encl_int_gate);
		if (IS_ERR_OR_NULL(cconf->clktree.encl_top_gate))
			LCDERR("%s: encl_top_gate\n", __func__);
		else
			clk_disable_unprepare(cconf->clktree.encl_top_gate);

		if (cconf->data->vclk_sel) {
			if (IS_ERR_OR_NULL(cconf->clktree.gp0_pll))
				LCDERR("%s: gp0_pll\n", __func__);
			else
				clk_disable_unprepare(cconf->clktree.gp0_pll);
		}
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

static void lcd_clk_gate_optional_switch_tl1(struct aml_lcd_drv_s *pdrv,
					     int status)
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

static void lcd_clktree_probe_g12a(struct aml_lcd_drv_s *pdrv)
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

	cconf->clktree.encl_top_gate = devm_clk_get(pdrv->dev, "encl_top_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.encl_top_gate))
		LCDERR("%s: clk encl_top_gate\n", __func__);

	cconf->clktree.encl_int_gate = devm_clk_get(pdrv->dev, "encl_int_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.encl_int_gate))
		LCDERR("%s: clk encl_int_gate\n", __func__);

	cconf->clktree.gp0_pll = devm_clk_get(pdrv->dev, "gp0_pll");
	if (IS_ERR_OR_NULL(cconf->clktree.gp0_pll))
		LCDERR("%s: clk gp0_pll\n", __func__);

	LCDPR("lcd_clktree_probe\n");
}

static void lcd_clktree_probe_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct clk *temp_clk;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	cconf->clktree.clk_gate_state = 0;

	cconf->clktree.encl_top_gate = devm_clk_get(pdrv->dev, "encl_top_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.encl_top_gate))
		LCDERR("%s: get encl_top_gate error\n", __func__);

	cconf->clktree.encl_int_gate = devm_clk_get(pdrv->dev, "encl_int_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.encl_int_gate))
		LCDERR("%s: get encl_int_gate error\n", __func__);

	cconf->clktree.tcon_gate = devm_clk_get(pdrv->dev, "tcon_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.tcon_gate))
		LCDERR("%s: get tcon_gate error\n", __func__);

	temp_clk = devm_clk_get(pdrv->dev, "fclk_div5");
	if (IS_ERR_OR_NULL(temp_clk)) {
		LCDERR("%s: clk fclk_div5\n", __func__);
		return;
	}
	cconf->clktree.tcon_clk = devm_clk_get(pdrv->dev, "clk_tcon");
	if (IS_ERR_OR_NULL(cconf->clktree.tcon_clk))
		LCDERR("%s: clk clk_tcon\n", __func__);
	else
		clk_set_parent(cconf->clktree.tcon_clk, temp_clk);

	LCDPR("lcd_clktree_probe\n");
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

static void lcd_clktree_probe_t3(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct clk *temp_clk;

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
	if (IS_ERR_OR_NULL(cconf->clktree.tcon_clk))
		LCDERR("%s: clk clk_tcon\n", __func__);
	else
		clk_set_parent(cconf->clktree.tcon_clk, temp_clk);

	LCDPR("lcd_clktree_probe\n");
}

static void lcd_clktree_remove_g12a(struct aml_lcd_drv_s *pdrv)
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
	if (!IS_ERR_OR_NULL(cconf->clktree.encl_top_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.encl_top_gate);
	if (!IS_ERR_OR_NULL(cconf->clktree.encl_int_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.encl_int_gate);
}

static void lcd_clktree_remove_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd_clktree_remove\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (!IS_ERR_OR_NULL(cconf->clktree.encl_top_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.encl_top_gate);
	if (!IS_ERR_OR_NULL(cconf->clktree.encl_int_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.encl_int_gate);
	if (!IS_ERR_OR_NULL(cconf->clktree.tcon_clk))
		devm_clk_put(pdrv->dev, cconf->clktree.tcon_clk);
	if (IS_ERR_OR_NULL(cconf->clktree.tcon_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.tcon_gate);
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

static void lcd_clk_config_init_print_dft(struct aml_lcd_drv_s *pdrv)
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
		data->xd_out_fmax, data->ss_level_max,
		data->ss_freq_max, data->ss_mode_max);
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

static int lcd_clk_config_print_dft(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
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

static int lcd_clk_config_print_g12a(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	n = lcd_debug_info_len(len + offset);
	if (cconf->data->vclk_sel) {
		len += snprintf((buf + len), n,
			"lcd clk config:\n"
			"vclk_sel      %d\n"
			"pll_m:        %d\n"
			"pll_n:        %d\n"
			"pll_frac:     0x%03x\n"
			"pll_fvco:     %dkHz\n"
			"pll_od:       %d\n"
			"pll_out:      %dkHz\n"
			"xd:           %d\n"
			"fout:         %dkHz\n\n",
			cconf->data->vclk_sel,
			cconf->pll_m, cconf->pll_n,
			cconf->pll_frac, cconf->pll_fvco,
			cconf->pll_od1_sel, cconf->pll_fout,
			cconf->xd, cconf->fout);
	} else {
		len += snprintf((buf + len), n,
			"lcd clk config:\n"
			"vclk_sel        %d\n"
			"pll_m:          %d\n"
			"pll_n:          %d\n"
			"pll_frac:       0x%03x\n"
			"pll_fvco:       %dkHz\n"
			"pll_od1:        %d\n"
			"pll_od2:        %d\n"
			"pll_od3:        %d\n"
			"pll_out:        %dkHz\n"
			"div_sel:        %s(index %d)\n"
			"xd:             %d\n"
			"fout:           %dkHz\n\n",
			cconf->data->vclk_sel,
			cconf->pll_m, cconf->pll_n,
			cconf->pll_frac, cconf->pll_fvco,
			cconf->pll_od1_sel, cconf->pll_od2_sel,
			cconf->pll_od3_sel, cconf->pll_fout,
			lcd_clk_div_sel_table[cconf->div_sel],
			cconf->div_sel, cconf->xd,
			cconf->fout);
	}

	return len;
}

/* ****************************************************
 * lcd clk function api
 * ****************************************************
 */
void lcd_clk_generate_parameter(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int ss_level, ss_freq, ss_mode;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (cconf->data->clk_generate_parameter)
		cconf->data->clk_generate_parameter(pdrv);

	ss_level = pdrv->config.timing.ss_level;
	cconf->ss_level = (ss_level >= cconf->data->ss_level_max) ? 0 : ss_level;
	ss_freq = pdrv->config.timing.ss_freq;
	cconf->ss_freq = (ss_freq >= cconf->data->ss_freq_max) ? 0 : ss_freq;
	ss_mode = pdrv->config.timing.ss_mode;
	cconf->ss_mode = (ss_mode >= cconf->data->ss_mode_max) ? 0 : ss_mode;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("[%d]: %s: ss_level=%d, ss_freq=%d, ss_mode=%d\n",
		      pdrv->index, __func__,
		      cconf->ss_level, cconf->ss_freq, cconf->ss_mode);
	}
}

int lcd_get_ss(struct aml_lcd_drv_s *pdrv, char *buf)
{
	struct lcd_clk_config_s *cconf;
	unsigned int temp;
	int len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf) {
		len += sprintf(buf + len, "[%d]: clk config data is null\n", pdrv->index);
		return len;
	}
	if (cconf->data->ss_level_max == 0) {
		len += sprintf(buf + len, "[%d]: spread spectrum is invalid\n", pdrv->index);
		return len;
	}

	temp = (cconf->ss_level >= cconf->data->ss_level_max) ?
		0 : cconf->ss_level;
	if (cconf->data->ss_level_table)
		len += sprintf(buf + len, "ss_level: %s\n", cconf->data->ss_level_table[temp]);
	temp = (cconf->ss_freq >= cconf->data->ss_freq_max) ? 0 : cconf->ss_freq;
	if (cconf->data->ss_freq_table)
		len += sprintf(buf + len, "ss_freq: %s\n", cconf->data->ss_freq_table[temp]);
	temp = (cconf->ss_mode >= cconf->data->ss_mode_max) ? 0 : cconf->ss_mode;
	if (cconf->data->ss_mode_table)
		len += sprintf(buf + len, "ss_mode: %s\n", cconf->data->ss_mode_table[temp]);

	return len;
}

void lcd_clk_ss_config_update(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int temp;

	if (!cconf || !cconf->data)
		return;

	temp = pdrv->config.timing.ss_level & 0xff;
	cconf->ss_level = (temp >= cconf->data->ss_level_max) ? 0 : temp;
	temp = (pdrv->config.timing.ss_level >> 8) & 0xff;
	temp = (temp >> LCD_CLK_SS_BIT_FREQ) & 0xf;
	cconf->ss_freq = (temp >= cconf->data->ss_freq_max) ? 0 : temp;
	temp = (pdrv->config.timing.ss_level >> 8) & 0xff;
	temp = (temp >> LCD_CLK_SS_BIT_MODE) & 0xf;
	cconf->ss_mode = (temp >= cconf->data->ss_mode_max) ? 0 : temp;
}

int lcd_set_ss(struct aml_lcd_drv_s *pdrv, unsigned int level, unsigned int freq, unsigned int mode)
{
	struct lcd_clk_config_s *cconf;
	unsigned long flags = 0;
	int ret = -1;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	spin_lock_irqsave(&lcd_clk_lock, flags);

	if (level < 0xff) {
		if (level >= cconf->data->ss_level_max) {
			LCDERR("[%d]: %s: ss_level %d is out of support (max %d)\n",
			       pdrv->index, __func__, level,
			       (cconf->data->ss_level_max - 1));
			goto lcd_set_ss_end;
		}
	}
	if (freq < 0xff) {
		if (freq >= cconf->data->ss_freq_max) {
			LCDERR("[%d]: %s: ss_freq %d is out of support (max %d)\n",
			       pdrv->index, __func__, freq,
			       (cconf->data->ss_freq_max - 1));
			goto lcd_set_ss_end;
		}
	}
	if (mode < 0xff) {
		if (mode >= cconf->data->ss_mode_max) {
			LCDERR("[%d]: %s: ss_mode %d is out of support (max %d)\n",
			       pdrv->index, __func__, mode,
			       (cconf->data->ss_mode_max - 1));
			goto lcd_set_ss_end;
		}
	}

	if (cconf->data->set_ss_level) {
		if (level < 0xff) {
			if (level > cconf->data->ss_level_max)
				cconf->ss_level = cconf->data->ss_level_max;
			else
				cconf->ss_level = level;
			cconf->data->set_ss_level(pdrv);
		}
	}

	ret = 0;
	if (cconf->data->set_ss_advance) {
		if (freq == 0xff && mode == 0xff)
			goto lcd_set_ss_end;
		if (freq < 0xff) {
			if (freq > cconf->data->ss_freq_max)
				cconf->ss_freq = cconf->data->ss_freq_max;
			else
				cconf->ss_freq = freq;
		}
		if (mode < 0xff) {
			if (mode > cconf->data->ss_mode_max)
				cconf->ss_mode = cconf->data->ss_mode_max;
			else
				cconf->ss_mode = mode;
		}
		cconf->data->set_ss_advance(pdrv);
	}

lcd_set_ss_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	return ret;
}

/* design for vlock, don't save ss_level to clk_config */
int lcd_ss_enable(int index, unsigned int flag)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;
	unsigned long flags = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", index, __func__);

	spin_lock_irqsave(&lcd_clk_lock, flags);

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		goto lcd_ss_enable_end;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		goto lcd_ss_enable_end;

	if (cconf->data->clk_ss_enable)
		cconf->data->clk_ss_enable(pdrv, flag);

lcd_ss_enable_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);

	return 0;
}

int lcd_encl_clk_msr(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	int clk_mux;
	int encl_clk = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return 0;

	clk_mux = cconf->data->enc_clk_msr_id;
	if (clk_mux == -1)
		return 0;
	encl_clk = meson_clk_measure(clk_mux);

	return encl_clk;
}

void lcd_pll_reset(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	int i = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_clk_lock, flags);

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		goto lcd_pll_reset_end;
	if (!cconf->data->pll_ctrl_table)
		goto lcd_pll_reset_end;

	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_END)
			break;
		if (table[i].flag == LCD_CLK_CTRL_RST) {
			lcd_ana_setb(table[i].reg, 1, table[i].bit, table[i].len);
			udelay(10);
			lcd_ana_setb(table[i].reg, 0, table[i].bit, table[i].len);
		}
		i++;
	}

lcd_pll_reset_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);
	LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

void lcd_vlock_m_update(int index, unsigned int vlock_m)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	int i = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_clk_lock, flags);

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		goto lcd_vlock_m_update_end;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		goto lcd_vlock_m_update_end;
	if (!cconf->data->pll_ctrl_table) {
		LCDERR("[%d]: %s: pll_ctrl_table null\n",
		       index, __func__);
		goto lcd_vlock_m_update_end;
	}

	vlock_m &= 0xff;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("[%d]: %s, vlcok_m: 0x%x,",
		      index, __func__, vlock_m);
	}

	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_M) {
			lcd_ana_setb(table[i].reg, vlock_m, table[i].bit, table[i].len);
			break;
		}
		i++;
	}

lcd_vlock_m_update_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);
}

void lcd_vlock_frac_update(int index, unsigned int vlock_frac)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	int i = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_clk_lock, flags);

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		goto lcd_vlock_frac_update_end;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		goto lcd_vlock_frac_update_end;
	if (!cconf->data->pll_ctrl_table) {
		LCDERR("[%d]: %s: pll_ctrl_table null\n",
		       index, __func__);
		goto lcd_vlock_frac_update_end;
	}

	vlock_frac &= 0x1ffff;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("[%d]: %s, vlock_frac: 0x%x\n",
		      index, __func__, vlock_frac);
	}

	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_FRAC) {
			lcd_ana_setb(table[i].reg, vlock_frac, table[i].bit, table[i].len);
			break;
		}
		i++;
	}

lcd_vlock_frac_update_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);
}

/* for frame rate change */
void lcd_update_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	unsigned int offset, reg, val;
	int i = 0;
	unsigned long flags = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	spin_lock_irqsave(&lcd_clk_lock, flags);

	if (cconf->data->pll_frac_generate)
		cconf->data->pll_frac_generate(pdrv);

	offset = cconf->pll_offset;

	if (!cconf->data->pll_ctrl_table)
		goto lcd_clk_update_end;
	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_END)
			break;
		if (table[i].flag == LCD_CLK_CTRL_FRAC) {
			reg = table[i].reg + offset;
			val = lcd_ana_read(reg);
			lcd_ana_setb(reg, cconf->pll_frac, table[i].bit, table[i].len);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: %s: pll_frac reg 0x%x: 0x%08x->0x%08x\n",
					pdrv->index, __func__, reg,
					val, lcd_ana_read(reg));
			}
		}
		i++;
	}

lcd_clk_update_end:
	spin_unlock_irqrestore(&lcd_clk_lock, flags);
	LCDPR("[%d]: %s: pll_frac=0x%x\n", pdrv->index, __func__, cconf->pll_frac);
}

/* for timing change */
void lcd_set_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned long flags = 0;
	int cnt = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (pdrv->lcd_pxp) {
		if (cconf->data->vclk_crt_set)
			cconf->data->vclk_crt_set(pdrv);
		return;
	}

lcd_set_clk_retry:
	spin_lock_irqsave(&lcd_clk_lock, flags);
	if (cconf->data->clk_set)
		cconf->data->clk_set(pdrv);
	if (cconf->data->vclk_crt_set)
		cconf->data->vclk_crt_set(pdrv);
	spin_unlock_irqrestore(&lcd_clk_lock, flags);
	usleep_range(10000, 10001);

	while (lcd_clk_msr_check(pdrv)) {
		if (cnt++ >= 10) {
			LCDERR("[%d]: %s timeout\n", pdrv->index, __func__);
			break;
		}
		goto lcd_set_clk_retry;
	}

	if (cconf->data->clktree_set)
		cconf->data->clktree_set(pdrv);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

void lcd_disable_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (cconf->data->clk_disable)
		cconf->data->clk_disable(pdrv);

	LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

static void lcd_clk_gate_optional_switch(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (!cconf->data->clk_gate_optional_switch)
		return;

	if (status) {
		if (cconf->clktree.clk_gate_optional_state) {
			LCDPR("[%d]: clk_gate_optional is already on\n",
			      pdrv->index);
		} else {
			cconf->data->clk_gate_optional_switch(pdrv, 1);
		}
	} else {
		if (cconf->clktree.clk_gate_optional_state == 0) {
			LCDPR("[%d]: clk_gate_optional is already off\n",
			      pdrv->index);
		} else {
			cconf->data->clk_gate_optional_switch(pdrv, 0);
		}
	}
}

void lcd_clk_gate_switch(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (status) {
		if (cconf->clktree.clk_gate_state) {
			LCDPR("[%d]: clk_gate is already on\n", pdrv->index);
		} else {
#ifdef CONFIG_AMLOGIC_VPU
			vpu_dev_clk_gate_on(pdrv->lcd_vpu_dev);
#endif
			if (cconf->data->clk_gate_switch)
				cconf->data->clk_gate_switch(pdrv, 1);
			cconf->clktree.clk_gate_state = 1;
		}
		lcd_clk_gate_optional_switch(pdrv, 1);
	} else {
		lcd_clk_gate_optional_switch(pdrv, 0);
		if (cconf->clktree.clk_gate_state == 0) {
			LCDPR("[%d]: clk_gate is already off\n",  pdrv->index);
		} else {
			if (cconf->data->clk_gate_switch)
				cconf->data->clk_gate_switch(pdrv, 0);
#ifdef CONFIG_AMLOGIC_VPU
			vpu_dev_clk_gate_off(pdrv->lcd_vpu_dev);
#endif
			cconf->clktree.clk_gate_state = 0;
		}
	}
}

int lcd_clk_clkmsr_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int clk;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "[%d]: %s: clk config is null\n",
				pdrv->index, __func__);
		return len;
	}

	if (cconf->data->enc_clk_msr_id == -1)
		goto lcd_clk_clkmsr_print_step_1;
	clk = meson_clk_measure(cconf->data->enc_clk_msr_id);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"encl_clk:    %u\n", clk);

lcd_clk_clkmsr_print_step_1:
	if (cconf->data->fifo_clk_msr_id == -1)
		goto lcd_clk_clkmsr_print_step_2;
	clk = meson_clk_measure(cconf->data->fifo_clk_msr_id);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"fifo_clk:    %u\n", clk);

lcd_clk_clkmsr_print_step_2:
	switch (pdrv->config.basic.lcd_type) {
	case LCD_MLVDS:
	case LCD_P2P:
		if (cconf->data->tcon_clk_msr_id == -1)
			break;
		clk = meson_clk_measure(cconf->data->tcon_clk_msr_id);
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"tcon_clk:    %u\n", clk);
	default:
		break;
	}

	return len;
}

int lcd_clk_config_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "[%d]: %s: clk config is null\n",
				pdrv->index, __func__);
		return len;
	}

	if (cconf->data->clk_config_print)
		len = cconf->data->clk_config_print(pdrv, buf, offset);

	return len;
}

/* ****************************************************
 * lcd clk config
 * ****************************************************
 */
static struct lcd_clk_data_s lcd_clk_data_g12a_path0 = {
	.pll_od_fb = PLL_OD_FB_HPLL_G12A,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_HPLL_G12A,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_HPLL_G12A,
	.pll_od_sel_max = PLL_OD_SEL_MAX_HPLL_G12A,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_HPLL_G12A,
	.pll_vco_fmin = PLL_VCO_MIN_HPLL_G12A,
	.pll_out_fmax = CRT_VID_CLK_IN_MAX_G12A,
	.pll_out_fmin = PLL_VCO_MIN_HPLL_G12A / 16,
	.div_in_fmax = 0,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_G12A,
	.xd_out_fmax = ENCL_CLK_IN_MAX_G12A,

	.clk_path_valid = 1,
	.vclk_sel = 0,
	.enc_clk_msr_id = 9,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,
	.pll_ctrl_table = pll_ctrl_table_g12a_path0,

	.ss_level_max = 0,
	.ss_freq_max = 0,
	.ss_mode_max = 0,
	.ss_level_table = NULL,
	.ss_freq_table = NULL,
	.ss_mode_table = NULL,

	.clk_generate_parameter = lcd_clk_generate_hpll_g12a,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = NULL,
	.set_ss_advance = NULL,
	.clk_ss_enable = NULL,
	.clk_set = lcd_clk_set_g12a_path0,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable_dft,
	.clk_gate_switch = lcd_clk_gate_switch_g12a,
	.clk_gate_optional_switch = NULL,
	.clktree_set = NULL,
	.clktree_probe = lcd_clktree_probe_g12a,
	.clktree_remove = lcd_clktree_remove_g12a,
	.clk_config_init_print = lcd_clk_config_init_print_g12a,
	.clk_config_print = lcd_clk_config_print_g12a,
	.prbs_clk_config = NULL,
};

static struct lcd_clk_data_s lcd_clk_data_g12a_path1 = {
	.pll_od_fb = PLL_OD_FB_GP0_G12A,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_GP0_G12A,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_GP0_G12A,
	.pll_od_sel_max = PLL_OD_SEL_MAX_GP0_G12A,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_GP0_G12A,
	.pll_vco_fmin = PLL_VCO_MIN_GP0_G12A,
	.pll_out_fmax = CRT_VID_CLK_IN_MAX_G12A,
	.pll_out_fmin = PLL_VCO_MIN_GP0_G12A / 16,
	.div_in_fmax = 0,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_G12A,
	.xd_out_fmax = ENCL_CLK_IN_MAX_G12A,

	.clk_path_valid = 1,
	.vclk_sel = 1,
	.enc_clk_msr_id = 9,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,
	.pll_ctrl_table = pll_ctrl_table_g12a_path1,

	.ss_level_max = 0,
	.ss_freq_max = 0,
	.ss_mode_max = 0,
	.ss_level_table = NULL,
	.ss_freq_table = NULL,
	.ss_mode_table = NULL,

	.clk_generate_parameter = lcd_clk_generate_g12a,
	.pll_frac_generate = lcd_pll_frac_generate_g12a,
	.set_ss_level = NULL,
	.set_ss_advance = NULL,
	.clk_ss_enable = NULL,
	.clk_set = lcd_clk_set_g12a_path1,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable_dft,
	.clk_gate_switch = lcd_clk_gate_switch_g12a,
	.clk_gate_optional_switch = NULL,
	.clktree_set = NULL,
	.clktree_probe = lcd_clktree_probe_g12a,
	.clktree_remove = lcd_clktree_remove_g12a,
	.clk_config_init_print = lcd_clk_config_init_print_g12a,
	.clk_config_print = lcd_clk_config_print_g12a,
	.prbs_clk_config = NULL,
};

static struct lcd_clk_data_s lcd_clk_data_g12b_path0 = {
	.pll_od_fb = PLL_OD_FB_HPLL_G12A,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_HPLL_G12A,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_HPLL_G12A,
	.pll_od_sel_max = PLL_OD_SEL_MAX_HPLL_G12A,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_HPLL_G12A,
	.pll_vco_fmin = PLL_VCO_MIN_HPLL_G12A,
	.pll_out_fmax = CRT_VID_CLK_IN_MAX_G12A,
	.pll_out_fmin = PLL_VCO_MIN_HPLL_G12A / 16,
	.div_in_fmax = 0,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_G12A,
	.xd_out_fmax = ENCL_CLK_IN_MAX_G12A,

	.clk_path_valid = 1,
	.vclk_sel = 0,
	.enc_clk_msr_id = 9,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,
	.pll_ctrl_table = pll_ctrl_table_g12a_path0,

	.ss_level_max = 0,
	.ss_freq_max = 0,
	.ss_mode_max = 0,
	.ss_level_table = NULL,
	.ss_freq_table = NULL,
	.ss_mode_table = NULL,

	.clk_generate_parameter = lcd_clk_generate_hpll_g12a,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = NULL,
	.set_ss_advance = NULL,
	.clk_ss_enable = NULL,
	.clk_set = lcd_clk_set_g12b_path0,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable_dft,
	.clk_gate_switch = lcd_clk_gate_switch_g12a,
	.clk_gate_optional_switch = NULL,
	.clktree_set = NULL,
	.clktree_probe = lcd_clktree_probe_g12a,
	.clktree_remove = lcd_clktree_remove_g12a,
	.clk_config_init_print = lcd_clk_config_init_print_g12a,
	.clk_config_print = lcd_clk_config_print_g12a,
	.prbs_clk_config = NULL,
};

static struct lcd_clk_data_s lcd_clk_data_g12b_path1 = {
	.pll_od_fb = PLL_OD_FB_GP0_G12A,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_GP0_G12A,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_GP0_G12A,
	.pll_od_sel_max = PLL_OD_SEL_MAX_GP0_G12A,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_GP0_G12A,
	.pll_vco_fmin = PLL_VCO_MIN_GP0_G12A,
	.pll_out_fmax = CRT_VID_CLK_IN_MAX_G12A,
	.pll_out_fmin = PLL_VCO_MIN_GP0_G12A / 16,
	.div_in_fmax = 0,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_G12A,
	.xd_out_fmax = ENCL_CLK_IN_MAX_G12A,

	.clk_path_valid = 1,
	.vclk_sel = 1,
	.enc_clk_msr_id = 9,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,
	.pll_ctrl_table = pll_ctrl_table_g12a_path1,

	.ss_level_max = 0,
	.ss_freq_max = 0,
	.ss_mode_max = 0,
	.ss_level_table = NULL,
	.ss_freq_table = NULL,
	.ss_mode_table = NULL,

	.clk_generate_parameter = lcd_clk_generate_g12a,
	.pll_frac_generate = lcd_pll_frac_generate_g12a,
	.set_ss_level = NULL,
	.set_ss_advance = NULL,
	.clk_ss_enable = NULL,
	.clk_set = lcd_clk_set_g12b_path1,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable_dft,
	.clk_gate_switch = lcd_clk_gate_switch_g12a,
	.clk_gate_optional_switch = NULL,
	.clktree_set = NULL,
	.clktree_probe = lcd_clktree_probe_g12a,
	.clktree_remove = lcd_clktree_remove_g12a,
	.clk_config_init_print = lcd_clk_config_init_print_g12a,
	.clk_config_print = lcd_clk_config_print_g12a,
	.prbs_clk_config = NULL,
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct lcd_clk_data_s lcd_clk_data_tl1 = {
	.pll_od_fb = PLL_OD_FB_TL1,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_TL1,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_TL1,
	.pll_od_sel_max = PLL_OD_SEL_MAX_TL1,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_TL1,
	.pll_vco_fmin = PLL_VCO_MIN_TL1,
	.pll_out_fmax = CLK_DIV_IN_MAX_TL1,
	.pll_out_fmin = PLL_VCO_MIN_TL1 / 16,
	.div_in_fmax = CLK_DIV_IN_MAX_TL1,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_TL1,
	.xd_out_fmax = ENCL_CLK_IN_MAX_TL1,

	.clk_path_valid = 0,
	.vclk_sel = 0,
	.enc_clk_msr_id = 9,
	.fifo_clk_msr_id = 129,
	.tcon_clk_msr_id = 128,
	.pll_ctrl_table = pll_ctrl_table_tl1,

	.ss_level_max = sizeof(lcd_ss_level_table_tl1) / sizeof(char *),
	.ss_freq_max = sizeof(lcd_ss_freq_table_tl1) / sizeof(char *),
	.ss_mode_max = sizeof(lcd_ss_mode_table_tl1) / sizeof(char *),
	.ss_level_table = lcd_ss_level_table_tl1,
	.ss_freq_table = lcd_ss_freq_table_tl1,
	.ss_mode_table = lcd_ss_mode_table_tl1,

	.clk_generate_parameter = lcd_clk_generate_tl1,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = lcd_set_pll_ss_level_tl1,
	.set_ss_advance = lcd_set_pll_ss_advance_tl1,
	.clk_ss_enable = lcd_pll_ss_enable_tl1,
	.clk_set = lcd_clk_set_tl1,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable_dft,
	.clk_gate_switch = lcd_clk_gate_switch_dft,
	.clk_gate_optional_switch = lcd_clk_gate_optional_switch_tl1,
	.clktree_set = lcd_set_tcon_clk_tl1,
	.clktree_probe = lcd_clktree_probe_tl1,
	.clktree_remove = lcd_clktree_remove_tl1,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.prbs_clk_config = lcd_prbs_config_clk_tl1,
};
#endif

static struct lcd_clk_data_s lcd_clk_data_tm2 = {
	.pll_od_fb = PLL_OD_FB_TL1,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_TL1,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_TL1,
	.pll_od_sel_max = PLL_OD_SEL_MAX_TL1,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_TM2,
	.pll_vco_fmin = PLL_VCO_MIN_TM2,
	.pll_out_fmax = CLK_DIV_IN_MAX_TL1,
	.pll_out_fmin = PLL_VCO_MIN_TL1 / 16,
	.div_in_fmax = CLK_DIV_IN_MAX_TL1,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_TL1,
	.xd_out_fmax = ENCL_CLK_IN_MAX_TL1,

	.clk_path_valid = 0,
	.vclk_sel = 0,
	.enc_clk_msr_id = 9,
	.fifo_clk_msr_id = 129,
	.tcon_clk_msr_id = 128,
	.pll_ctrl_table = pll_ctrl_table_tl1,

	.ss_level_max = sizeof(lcd_ss_level_table_tl1) / sizeof(char *),
	.ss_freq_max = sizeof(lcd_ss_freq_table_tl1) / sizeof(char *),
	.ss_mode_max = sizeof(lcd_ss_mode_table_tl1) / sizeof(char *),
	.ss_level_table = lcd_ss_level_table_tl1,
	.ss_freq_table = lcd_ss_freq_table_tl1,
	.ss_mode_table = lcd_ss_mode_table_tl1,

	.clk_generate_parameter = lcd_clk_generate_tl1,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = lcd_set_pll_ss_level_tl1,
	.set_ss_advance = lcd_set_pll_ss_advance_tl1,
	.clk_ss_enable = lcd_pll_ss_enable_tl1,
	.clk_set = lcd_clk_set_tl1,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable_dft,
	.clk_gate_switch = lcd_clk_gate_switch_dft,
	.clk_gate_optional_switch = lcd_clk_gate_optional_switch_tl1,
	.clktree_set = lcd_set_tcon_clk_tl1,
	.clktree_probe = lcd_clktree_probe_tl1,
	.clktree_remove = lcd_clktree_remove_tl1,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.prbs_clk_config = lcd_prbs_config_clk_tl1,
};

static struct lcd_clk_data_s lcd_clk_data_t5 = {
	.pll_od_fb = PLL_OD_FB_TL1,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_TL1,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_TL1,
	.pll_od_sel_max = PLL_OD_SEL_MAX_TL1,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_TM2,
	.pll_vco_fmin = PLL_VCO_MIN_TM2,
	.pll_out_fmax = CLK_DIV_IN_MAX_TL1,
	.pll_out_fmin = PLL_VCO_MIN_TL1 / 16,
	.div_in_fmax = CLK_DIV_IN_MAX_TL1,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_TL1,
	.xd_out_fmax = ENCL_CLK_IN_MAX_TL1,

	.clk_path_valid = 0,
	.vclk_sel = 0,
	.enc_clk_msr_id = 9,
	.fifo_clk_msr_id = 129,
	.tcon_clk_msr_id = 128,
	.pll_ctrl_table = pll_ctrl_table_tl1,

	.ss_level_max = sizeof(lcd_ss_level_table_tl1) / sizeof(char *),
	.ss_freq_max = sizeof(lcd_ss_freq_table_tl1) / sizeof(char *),
	.ss_mode_max = sizeof(lcd_ss_mode_table_tl1) / sizeof(char *),
	.ss_level_table = lcd_ss_level_table_tl1,
	.ss_freq_table = lcd_ss_freq_table_tl1,
	.ss_mode_table = lcd_ss_mode_table_tl1,

	.clk_generate_parameter = lcd_clk_generate_tl1,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = lcd_set_pll_ss_level_tl1,
	.set_ss_advance = lcd_set_pll_ss_advance_tl1,
	.clk_ss_enable = lcd_pll_ss_enable_tl1,
	.clk_set = lcd_clk_set_tl1,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable_dft,
	.clk_gate_switch = lcd_clk_gate_switch_dft,
	.clk_gate_optional_switch = lcd_clk_gate_optional_switch_tl1,
	.clktree_set = lcd_set_tcon_clk_t5,
	.clktree_probe = lcd_clktree_probe_tl1,
	.clktree_remove = lcd_clktree_remove_tl1,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.prbs_clk_config = lcd_prbs_config_clk_tl1,
};

static struct lcd_clk_data_s lcd_clk_data_t5d = {
	.pll_od_fb = PLL_OD_FB_TL1,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_TL1,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_TL1,
	.pll_od_sel_max = PLL_OD_SEL_MAX_TL1,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_TM2,
	.pll_vco_fmin = PLL_VCO_MIN_TM2,
	.pll_out_fmax = CLK_DIV_IN_MAX_T5D,
	.pll_out_fmin = PLL_VCO_MIN_TL1 / 16,
	.div_in_fmax = CLK_DIV_IN_MAX_T5D,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_T5D,
	.xd_out_fmax = ENCL_CLK_IN_MAX_T5D,

	.clk_path_valid = 0,
	.vclk_sel = 0,
	.enc_clk_msr_id = 9,
	.fifo_clk_msr_id = 129,
	.tcon_clk_msr_id = 128,
	.pll_ctrl_table = pll_ctrl_table_tl1,

	.ss_level_max = sizeof(lcd_ss_level_table_tl1) / sizeof(char *),
	.ss_freq_max = sizeof(lcd_ss_freq_table_tl1) / sizeof(char *),
	.ss_mode_max = sizeof(lcd_ss_mode_table_tl1) / sizeof(char *),
	.ss_level_table = lcd_ss_level_table_tl1,
	.ss_freq_table = lcd_ss_freq_table_tl1,
	.ss_mode_table = lcd_ss_mode_table_tl1,

	.clk_generate_parameter = lcd_clk_generate_tl1,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = lcd_set_pll_ss_level_tl1,
	.set_ss_advance = lcd_set_pll_ss_advance_tl1,
	.clk_ss_enable = lcd_pll_ss_enable_tl1,
	.clk_set = lcd_clk_set_tl1,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable_dft,
	.clk_gate_switch = lcd_clk_gate_switch_dft,
	.clk_gate_optional_switch = lcd_clk_gate_optional_switch_tl1,
	.clktree_set = lcd_set_tcon_clk_t5,
	.clktree_probe = lcd_clktree_probe_tl1,
	.clktree_remove = lcd_clktree_remove_tl1,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.prbs_clk_config = lcd_prbs_config_clk_tl1,
};

static struct lcd_clk_data_s lcd_clk_data_t7 = {
	.pll_od_fb = PLL_OD_FB_TL1,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_TL1,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_TL1,
	.pll_od_sel_max = PLL_OD_SEL_MAX_TL1,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_TM2,
	.pll_vco_fmin = PLL_VCO_MIN_TM2,
	.pll_out_fmax = CLK_DIV_IN_MAX_TL1,
	.pll_out_fmin = PLL_VCO_MIN_TL1 / 16,
	.div_in_fmax = CLK_DIV_IN_MAX_TL1,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_TL1,
	.xd_out_fmax = ENCL_CLK_IN_MAX_TL1,

	.clk_path_valid = 0,
	.vclk_sel = 0,
	.enc_clk_msr_id = 222,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,
	.pll_ctrl_table = pll_ctrl_table_t7,

	.ss_level_max = sizeof(lcd_ss_level_table_tl1) / sizeof(char *),
	.ss_freq_max = sizeof(lcd_ss_freq_table_tl1) / sizeof(char *),
	.ss_mode_max = sizeof(lcd_ss_mode_table_tl1) / sizeof(char *),
	.ss_level_table = lcd_ss_level_table_tl1,
	.ss_freq_table = lcd_ss_freq_table_tl1,
	.ss_mode_table = lcd_ss_mode_table_tl1,

	.clk_generate_parameter = lcd_clk_generate_tl1,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = lcd_set_pll_ss_level_t7,
	.set_ss_advance = lcd_set_pll_ss_advance_t7,
	.clk_ss_enable = lcd_pll_ss_enable_t7,
	.clk_set = lcd_clk_set_t7,
	.vclk_crt_set = lcd_set_vclk_crt_t7,
	.clk_disable = lcd_clk_disable_t7,
	.clk_gate_switch = lcd_clk_gate_switch_t7,
	.clk_gate_optional_switch = NULL,
	.clktree_set = NULL,
	.clktree_probe = lcd_clktree_probe_t7,
	.clktree_remove = lcd_clktree_remove_t7,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.prbs_clk_config = lcd_prbs_config_clk_t7,
};

static struct lcd_clk_data_s lcd_clk_data_t3 = {
	.pll_od_fb = PLL_OD_FB_TL1,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_TL1,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_TL1,
	.pll_od_sel_max = PLL_OD_SEL_MAX_TL1,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_TM2,
	.pll_vco_fmin = PLL_VCO_MIN_TM2,
	.pll_out_fmax = CLK_DIV_IN_MAX_TL1,
	.pll_out_fmin = PLL_VCO_MIN_TL1 / 16,
	.div_in_fmax = CLK_DIV_IN_MAX_TL1,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_TL1,
	.xd_out_fmax = ENCL_CLK_IN_MAX_TL1,

	.clk_path_valid = 0,
	.vclk_sel = 0,
	.enc_clk_msr_id = 222,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = 119,
	.pll_ctrl_table = pll_ctrl_table_t7,

	.ss_level_max = sizeof(lcd_ss_level_table_tl1) / sizeof(char *),
	.ss_freq_max = sizeof(lcd_ss_freq_table_tl1) / sizeof(char *),
	.ss_mode_max = sizeof(lcd_ss_mode_table_tl1) / sizeof(char *),
	.ss_level_table = lcd_ss_level_table_tl1,
	.ss_freq_table = lcd_ss_freq_table_tl1,
	.ss_mode_table = lcd_ss_mode_table_tl1,

	.clk_generate_parameter = lcd_clk_generate_tl1,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = lcd_set_pll_ss_level_t7,
	.set_ss_advance = lcd_set_pll_ss_advance_t7,
	.clk_ss_enable = lcd_pll_ss_enable_t7,
	.clk_set = lcd_clk_set_t3,
	.vclk_crt_set = lcd_set_vclk_crt_t7,
	.clk_disable = lcd_clk_disable_t7,
	.clk_gate_switch = lcd_clk_gate_switch_t7,
	.clk_gate_optional_switch = lcd_clk_gate_optional_switch_tl1,
	.clktree_set = lcd_set_tcon_clk_t3,
	.clktree_probe = lcd_clktree_probe_t3,
	.clktree_remove = lcd_clktree_remove_t3,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.prbs_clk_config = lcd_prbs_config_clk_t3,
};

static struct lcd_clk_data_s lcd_clk_data_t5w = {
	.pll_od_fb = PLL_OD_FB_TL1,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_TL1,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_TL1,
	.pll_od_sel_max = PLL_OD_SEL_MAX_TL1,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_TM2,
	.pll_vco_fmin = PLL_VCO_MIN_TM2,
	.pll_out_fmax = CLK_DIV_IN_MAX_TL1,
	.pll_out_fmin = PLL_VCO_MIN_TL1 / 16,
	.div_in_fmax = CLK_DIV_IN_MAX_TL1,
	.div_out_fmax = CRT_VID_CLK_IN_MAX_TL1,
	.xd_out_fmax = ENCL_CLK_IN_MAX_TL1,

	.clk_path_valid = 0,
	.vclk_sel = 0,
	.enc_clk_msr_id = 6,
	.fifo_clk_msr_id = 129,
	.tcon_clk_msr_id = 128,
	.pll_ctrl_table = pll_ctrl_table_tl1,

	.ss_level_max = sizeof(lcd_ss_level_table_tl1) / sizeof(char *),
	.ss_freq_max = sizeof(lcd_ss_freq_table_tl1) / sizeof(char *),
	.ss_mode_max = sizeof(lcd_ss_mode_table_tl1) / sizeof(char *),
	.ss_level_table = lcd_ss_level_table_tl1,
	.ss_freq_table = lcd_ss_freq_table_tl1,
	.ss_mode_table = lcd_ss_mode_table_tl1,

	.clk_generate_parameter = lcd_clk_generate_tl1,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = lcd_set_pll_ss_level_tl1,
	.set_ss_advance = lcd_set_pll_ss_advance_tl1,
	.clk_ss_enable = lcd_pll_ss_enable_tl1,
	.clk_set = lcd_clk_set_t5w,
	.vclk_crt_set = lcd_set_vclk_crt_t5w,
	.clk_disable = lcd_clk_disable_dft,
	.clk_gate_switch = lcd_clk_gate_switch_dft,
	.clk_gate_optional_switch = lcd_clk_gate_optional_switch_tl1,
	.clktree_set = lcd_set_tcon_clk_t5,
	.clktree_probe = lcd_clktree_probe_tl1,
	.clktree_remove = lcd_clktree_remove_tl1,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.prbs_clk_config = lcd_prbs_config_clk_t5w,
};

static void lcd_clk_config_chip_init(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	cconf->pll_id = 0;
	cconf->pll_offset = 0;
	cconf->fin = FIN_FREQ;
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_G12A:
	case LCD_CHIP_SM1:
		if (pdrv->clk_path)
			cconf->data = &lcd_clk_data_g12a_path1;
		else
			cconf->data = &lcd_clk_data_g12a_path0;
		break;
	case LCD_CHIP_G12B:
		if (pdrv->clk_path)
			cconf->data = &lcd_clk_data_g12b_path1;
		else
			cconf->data = &lcd_clk_data_g12b_path0;
		break;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case LCD_CHIP_TL1:
		cconf->data = &lcd_clk_data_tl1;
		break;
#endif
	case LCD_CHIP_TM2:
		cconf->data = &lcd_clk_data_tm2;
		break;
	case LCD_CHIP_T5:
		cconf->data = &lcd_clk_data_t5;
		break;
	case LCD_CHIP_T5D:
		cconf->data = &lcd_clk_data_t5d;
		break;
	case LCD_CHIP_T7:
		cconf->data = &lcd_clk_data_t7;
		switch (pdrv->index) {
		case 1:
			cconf->data->enc_clk_msr_id = 221;
			cconf->pll_id = 1;
			cconf->pll_offset = 0x5;
			break;
		case 2:
			cconf->data->enc_clk_msr_id = 220;
			cconf->pll_id = 2;
			cconf->pll_offset = 0xa;
			break;
		case 0:
		default:
			cconf->data->enc_clk_msr_id = 222;
			cconf->pll_id = 0;
			cconf->pll_offset = 0;
			break;
		}
		cconf->data->enc_clk_msr_id = -1;
		break;
	case LCD_CHIP_T3: /* only one pll */
		cconf->data = &lcd_clk_data_t3;
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
		break;
	case LCD_CHIP_T5W:
		cconf->data = &lcd_clk_data_t5w;
		cconf->data->enc_clk_msr_id = -1;
		break;
	default:
		LCDPR("[%d]: %s: invalid chip type\n", pdrv->index, __func__);
		return;
	}

	cconf->pll_od_fb = cconf->data->pll_od_fb;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		if (cconf->data->clk_config_init_print)
			cconf->data->clk_config_init_print(pdrv);
	}
}

int lcd_clk_path_change(struct aml_lcd_drv_s *pdrv, int sel)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	if (cconf->data->clk_path_valid == 0) {
		LCDPR("[%d]: %s: current chip not support\n",
		      pdrv->index, __func__);
		return -1;
	}

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_G12A:
	case LCD_CHIP_G12B:
	case LCD_CHIP_SM1:
		if (sel)
			cconf->data = &lcd_clk_data_g12a_path1;
		else
			cconf->data = &lcd_clk_data_g12a_path0;
		cconf->pll_od_fb = cconf->data->pll_od_fb;
		break;
	default:
		LCDERR("[%d]: %s: current chip not support\n",
		       pdrv->index, __func__);
		return -1;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		if (cconf->data->clk_config_init_print)
			cconf->data->clk_config_init_print(pdrv);
	}

	return 0;
}

void lcd_clk_config_probe(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = kzalloc(sizeof(*cconf), GFP_KERNEL);
	if (!cconf)
		return;
	pdrv->clk_conf = (void *)cconf;

	lcd_clk_config_chip_init(pdrv, cconf);
	if (!cconf->data)
		return;

	if (cconf->data->clktree_probe)
		cconf->data->clktree_probe(pdrv);
}

void lcd_clk_config_remove(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (!pdrv->clk_conf)
		return;

	cconf = (struct lcd_clk_config_s *)pdrv->clk_conf;
	if (cconf->data && cconf->data->clktree_remove)
		cconf->data->clktree_remove(pdrv);

	kfree(cconf);
	pdrv->clk_conf = NULL;
}

