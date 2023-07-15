// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reset.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "../lcd_common.h"
#include "lcd_venc.h"

static struct lcd_venc_op_s lcd_venc_op = {
	.init_flag = 0,
	.wait_vsync = NULL,
	.get_max_lcnt = NULL,
	.gamma_test_en = NULL,
	.venc_debug_test = NULL,
	.venc_set_timing = NULL,
	.venc_set = NULL,
	.venc_change = NULL,
	.venc_enable = NULL,
	.mute_set = NULL,
	.get_venc_init_config = NULL,
	.venc_vrr_recovery = NULL,
	.get_encl_line_cnt = NULL,
	.get_encl_frm_cnt = NULL,
};

void lcd_wait_vsync(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->lcd_pxp)
		return;
	if (!lcd_venc_op.wait_vsync)
		return;

	lcd_venc_op.wait_vsync(pdrv);
}

unsigned int lcd_get_encl_line_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int lcnt;

	if (!lcd_venc_op.get_encl_line_cnt)
		return 0;
	if (!pdrv)
		return 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	lcnt = lcd_venc_op.get_encl_line_cnt(pdrv);
	return lcnt;
}

unsigned int lcd_get_max_line_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int max_lcnt;

	if (!lcd_venc_op.get_max_lcnt)
		return 0;
	if (!pdrv)
		return 0;

	max_lcnt = lcd_venc_op.get_max_lcnt(pdrv);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, max_lcnt);

	return max_lcnt;
}

void lcd_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (!lcd_venc_op.gamma_test_en)
		return;

	lcd_venc_op.gamma_test_en(pdrv, flag);
}

void lcd_debug_test(struct aml_lcd_drv_s *pdrv, unsigned int num)
{
	int ret;

	if (!lcd_venc_op.venc_debug_test) {
		LCDERR("[%d]: %s: invalid\n", pdrv->index, __func__);
		return;
	}

	ret = lcd_venc_op.venc_debug_test(pdrv, num);
	if (ret) {
		LCDERR("[%d]: %s: %d not support\n", pdrv->index, __func__, num);
		return;
	}

	if (num == 0)
		LCDPR("[%d]: disable test pattern\n", pdrv->index);
}

void lcd_screen_restore(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
		set_output_mute(false);
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
#endif
	}
}

void lcd_screen_black(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
		set_output_mute(true);
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
#endif
	}
}

void lcd_set_venc_timing(struct aml_lcd_drv_s *pdrv)
{
	if (!lcd_venc_op.venc_set_timing)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	lcd_venc_op.venc_set_timing(pdrv);
}

void lcd_set_venc(struct aml_lcd_drv_s *pdrv)
{
	if (!lcd_venc_op.venc_set) {
		LCDERR("[%d]: %s: invalid\n", pdrv->index, __func__);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	lcd_venc_op.venc_set(pdrv);
}

void lcd_venc_change(struct aml_lcd_drv_s *pdrv)
{
	if (!lcd_venc_op.venc_change) {
		LCDERR("[%d]: %s: invalid\n", pdrv->index, __func__);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	lcd_venc_op.venc_change(pdrv);
}

void lcd_venc_enable(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (!lcd_venc_op.venc_enable) {
		LCDERR("[%d]: %s: invalid\n", pdrv->index, __func__);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, flag);
	lcd_venc_op.venc_enable(pdrv, flag);
}

void lcd_mute_set(struct aml_lcd_drv_s *pdrv,  unsigned char flag)
{
	if (!lcd_venc_op.mute_set) {
		LCDERR("[%d]: %s: invalid\n", pdrv->index, __func__);
		return;
	}

	lcd_venc_op.mute_set(pdrv, flag);
	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, flag);
}

int lcd_get_venc_init_config(struct aml_lcd_drv_s *pdrv)
{
	int ret = 0;

	if (!lcd_venc_op.get_venc_init_config) {
		LCDERR("[%d]: %s: invalid\n", pdrv->index, __func__);
		return 0;
	}

	ret = lcd_venc_op.get_venc_init_config(pdrv);
	return ret;
}

unsigned int lcd_get_encl_frm_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int cnt = 0;

	if (!lcd_venc_op.get_encl_frm_cnt)
		return 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ISR)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	cnt = lcd_venc_op.get_encl_frm_cnt(pdrv);

	return cnt;
}

void lcd_venc_vrr_recovery(struct aml_lcd_drv_s *pdrv)
{
	if (!lcd_venc_op.venc_vrr_recovery)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDERR("[%d]: %s\n", pdrv->index, __func__);

	lcd_venc_op.venc_vrr_recovery(pdrv);
}

int lcd_venc_probe(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	if (lcd_venc_op.init_flag)
		return 0;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
	case LCD_CHIP_T3:
	case LCD_CHIP_T5W:
		ret = lcd_venc_op_init_t7(pdrv, &lcd_venc_op);
		break;
	case LCD_CHIP_A4:
		ret = lcd_venc_op_init_a4(pdrv, &lcd_venc_op);
		break;
	default:
		ret = lcd_venc_op_init_dft(pdrv, &lcd_venc_op);
		break;
	}
	if (ret) {
		LCDERR("%s: failed\n", __func__);
		return -1;
	}

	lcd_venc_op.init_flag = 1;

	return 0;
}
