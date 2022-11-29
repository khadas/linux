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
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "lcd_venc.h"

static inline unsigned int lcd_venc_get_encl_lint_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg, line_cnt;

	if (!pdrv)
		return 0;

	reg = ENCL_INFO_READ;

	line_cnt = lcd_vcbus_getb(reg, 16, 13);
	return line_cnt;
}

static void lcd_wait_vsync_dft(struct aml_lcd_drv_s *pdrv)
{
	unsigned int line_cnt, line_cnt_previous;
	unsigned int reg;
	int i = 0;

	if (!pdrv || pdrv->lcd_pxp)
		return;

	reg = ENCL_INFO_READ;

	line_cnt = 0x1fff;
	line_cnt_previous = lcd_vcbus_getb(reg, 16, 13);
	while (i++ < LCD_WAIT_VSYNC_TIMEOUT) {
		line_cnt = lcd_vcbus_getb(reg, 16, 13);
		if (line_cnt < line_cnt_previous)
			break;
		line_cnt_previous = line_cnt;
		udelay(2);
	}
	/*LCDPR("line_cnt=%d, line_cnt_previous=%d, i=%d\n",
	 *	line_cnt, line_cnt_previous, i);
	 */
}

static void lcd_venc_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag)
{
	unsigned int reg;

	reg = L_GAMMA_CNTL_PORT;

	if (flag) {
		if (pdrv->gamma_en_flag) {
			if (lcd_vcbus_getb(reg, 0, 1) == 0) {
				lcd_vcbus_setb(reg, 1, 0, 1);
				LCDPR("[%d]: %s: %d\n", pdrv->index, __func__,
				      flag);
			}
		}
	} else {
		if (pdrv->gamma_en_flag) {
			if (lcd_vcbus_getb(reg, 0, 1)) {
				lcd_vcbus_setb(reg, 0, 0, 1);
				LCDPR("[%d]: %s: %d\n", pdrv->index, __func__,
				      flag);
			}
		}
	}
}

static void lcd_venc_gamma_check_en(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_vcbus_getb(L_GAMMA_CNTL_PORT, 0, 1))
		pdrv->gamma_en_flag = 1;
	else
		pdrv->gamma_en_flag = 0;
	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, pdrv->gamma_en_flag);
}

static void lcd_gamma_init(struct aml_lcd_drv_s *pdrv)
{
	unsigned int data[2];
	unsigned int index = pdrv->index;

	if (pdrv->lcd_pxp)
		return;

	data[0] = index;
	data[1] = 0xff; //default gamma lut

	aml_lcd_atomic_notifier_call_chain(LCD_EVENT_GAMMA_UPDATE,
					   (void *)data);
	lcd_venc_gamma_check_en(pdrv);
}

static void lcd_set_encl_tcon(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int reg_rgb_base, reg_rgb_coeff, reg_dith_ctrl, reg_pol_ctrl;
	unsigned int reg_de_hs, reg_de_he, reg_de_vs, reg_de_ve;
	unsigned int reg_hsync_hs, reg_hsync_he, reg_hsync_vs, reg_hsync_ve;
	unsigned int reg_vsync_hs, reg_vsync_he, reg_vsync_vs, reg_vsync_ve;

	reg_rgb_base = L_RGB_BASE_ADDR;
	reg_rgb_coeff = L_RGB_COEFF_ADDR;
	reg_dith_ctrl = L_DITH_CNTL_ADDR;
	reg_pol_ctrl = L_POL_CNTL_ADDR;
	reg_de_hs = L_DE_HS_ADDR;
	reg_de_he = L_DE_HE_ADDR;
	reg_de_vs = L_DE_VS_ADDR;
	reg_de_ve = L_DE_VE_ADDR;
	reg_hsync_hs = L_HSYNC_HS_ADDR;
	reg_hsync_he = L_HSYNC_HE_ADDR;
	reg_hsync_vs = L_HSYNC_VS_ADDR;
	reg_hsync_ve = L_HSYNC_VE_ADDR;
	reg_vsync_hs = L_VSYNC_HS_ADDR;
	reg_vsync_he = L_VSYNC_HE_ADDR;
	reg_vsync_vs = L_VSYNC_VS_ADDR;
	reg_vsync_ve = L_VSYNC_VE_ADDR;

	lcd_vcbus_write(reg_rgb_base, 0x0);
	lcd_vcbus_write(reg_rgb_coeff, 0x400);

	switch (pconf->basic.lcd_bits) {
	case 6:
		lcd_vcbus_write(reg_dith_ctrl, 0x600);
		break;
	case 8:
		lcd_vcbus_write(reg_dith_ctrl, 0x400);
		break;
	case 10:
	default:
		lcd_vcbus_write(reg_dith_ctrl, 0x0);
		break;
	}

	switch (pconf->basic.lcd_type) {
	case LCD_LVDS:
		lcd_vcbus_setb(reg_pol_ctrl, 1, 0, 1);
		if (pconf->timing.vsync_pol)
			lcd_vcbus_setb(reg_pol_ctrl, 1, 1, 1);
		break;
	case LCD_VBYONE:
		if (pconf->timing.hsync_pol)
			lcd_vcbus_setb(reg_pol_ctrl, 1, 0, 1);
		if (pconf->timing.vsync_pol)
			lcd_vcbus_setb(reg_pol_ctrl, 1, 1, 1);
		break;
	case LCD_MIPI:
		//lcd_vcbus_setb(reg_pol_ctrl, 0x3, 0, 2);
		/*lcd_vcbus_write(reg_pol_ctrl,
		 *	(lcd_vcbus_read(reg_pol_ctrl) |
		 *	 ((0 << 2) | (vs_pol_adj << 1) | (hs_pol_adj << 0))));
		 */
		/*lcd_vcbus_write(reg_pol_ctrl, (lcd_vcbus_read(reg_pol_ctrl) |
		 *	 ((1 << LCD_TCON_DE_SEL) | (1 << LCD_TCON_VS_SEL) |
		 *	  (1 << LCD_TCON_HS_SEL))));
		 */
		break;
	case LCD_EDP:
		lcd_vcbus_setb(reg_pol_ctrl, 1, 0, 1);
		break;
	default:
		break;
	}

	/* DE signal */
	lcd_vcbus_write(reg_de_hs, pconf->timing.de_hs_addr);
	lcd_vcbus_write(reg_de_he, pconf->timing.de_he_addr);
	lcd_vcbus_write(reg_de_vs, pconf->timing.de_vs_addr);
	lcd_vcbus_write(reg_de_ve, pconf->timing.de_ve_addr);

	/* Hsync signal */
	lcd_vcbus_write(reg_hsync_hs, pconf->timing.hs_hs_addr);
	lcd_vcbus_write(reg_hsync_he, pconf->timing.hs_he_addr);
	lcd_vcbus_write(reg_hsync_vs, pconf->timing.hs_vs_addr);
	lcd_vcbus_write(reg_hsync_ve, pconf->timing.hs_ve_addr);

	/* Vsync signal */
	lcd_vcbus_write(reg_vsync_hs, pconf->timing.vs_hs_addr);
	lcd_vcbus_write(reg_vsync_he, pconf->timing.vs_he_addr);
	lcd_vcbus_write(reg_vsync_vs, pconf->timing.vs_vs_addr);
	lcd_vcbus_write(reg_vsync_ve, pconf->timing.vs_ve_addr);
}

static void lcd_venc_set_timing(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int hstart, hend, vstart, vend;
	unsigned int pre_de_vs, pre_de_ve, pre_de_hs, pre_de_he;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	hstart = pconf->timing.hstart;
	hend = pconf->timing.hend;
	vstart = pconf->timing.vstart;
	vend = pconf->timing.vend;

	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT, pconf->basic.h_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT, pconf->basic.v_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN, hstart);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END, hend);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE, vstart);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE, vend);

	/*update line_n trigger_line*/
	lcd_vcbus_write(VPP_INT_LINE_NUM, vend + 1);

	if (pconf->basic.lcd_type == LCD_P2P ||
	    pconf->basic.lcd_type == LCD_MLVDS) {
		switch (pdrv->data->chip_type) {
		case LCD_CHIP_TL1:
		case LCD_CHIP_TM2:
			pre_de_vs = vstart - 1 - 4;
			pre_de_ve = vstart - 1;
			pre_de_hs = hstart + PRE_DE_DELAY;
			pre_de_he = pconf->basic.h_active - 1 + pre_de_hs;
			break;
		default:
			pre_de_vs = vstart - 8;
			pre_de_ve = pconf->basic.v_active + pre_de_vs;
			pre_de_hs = hstart + PRE_DE_DELAY;
			pre_de_he = pconf->basic.h_active - 1 + pre_de_hs;
			break;
		}
		lcd_vcbus_write(ENCL_VIDEO_V_PRE_DE_BLINE, pre_de_vs);
		lcd_vcbus_write(ENCL_VIDEO_V_PRE_DE_ELINE, pre_de_ve);
		lcd_vcbus_write(ENCL_VIDEO_H_PRE_DE_BEGIN, pre_de_hs);
		lcd_vcbus_write(ENCL_VIDEO_H_PRE_DE_END, pre_de_he);
	}

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN, pconf->timing.hs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END, pconf->timing.hs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN, pconf->timing.vs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END, pconf->timing.vs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE, pconf->timing.vs_vs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE, pconf->timing.vs_ve_addr);

	/*[15:14]: 2'b10 or 2'b01*/
	lcd_vcbus_write(ENCL_INBUF_CNTL1,
			(2 << 14) | (pconf->basic.h_active - 1));
	lcd_vcbus_write(ENCL_INBUF_CNTL0, 0x200);

	lcd_set_encl_tcon(pdrv);
	aml_lcd_notifier_call_chain(LCD_EVENT_BACKLIGHT_UPDATE, (void *)pdrv);
}

static void lcd_venc_change_timing(struct aml_lcd_drv_s *pdrv)
{
	unsigned int htotal, vtotal;

	if (pdrv->vmode_update) {
		lcd_timing_init_config(pdrv);
		lcd_set_venc_timing(pdrv);
	} else {
		htotal = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT) + 1;
		vtotal = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;

		if (pdrv->config.basic.h_period != htotal) {
			lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT,
					pdrv->config.basic.h_period - 1);
		}
		if (pdrv->config.basic.v_period != vtotal) {
			lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT,
					pdrv->config.basic.v_period - 1);
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: venc changed: %d,%d\n", pdrv->index,
			      pdrv->config.basic.h_period,
			      pdrv->config.basic.v_period);
		}
	}

	aml_lcd_notifier_call_chain(LCD_EVENT_BACKLIGHT_UPDATE, (void *)pdrv);
}

#define LCD_ENC_TST_NUM_MAX    9
static char *lcd_enc_tst_str[] = {
	"0-None",        /* 0 */
	"1-Color Bar",   /* 1 */
	"2-Thin Line",   /* 2 */
	"3-Dot Grid",    /* 3 */
	"4-Gray",        /* 4 */
	"5-Red",         /* 5 */
	"6-Green",       /* 6 */
	"7-Blue",        /* 7 */
	"8-Black",       /* 8 */
};

static unsigned int lcd_enc_tst[][7] = {
/*tst_mode,    Y,       Cb,     Cr,     tst_en,  vfifo_en  rgbin*/
	{0,    0x200,   0x200,  0x200,   0,      1,        3},  /* 0 */
	{1,    0x200,   0x200,  0x200,   1,      0,        1},  /* 1 */
	{2,    0x200,   0x200,  0x200,   1,      0,        1},  /* 2 */
	{3,    0x200,   0x200,  0x200,   1,      0,        1},  /* 3 */
	{0,    0x1ff,   0x1ff,  0x1ff,   1,      0,        3},  /* 4 */
	{0,    0x3ff,     0x0,    0x0,   1,      0,        3},  /* 5 */
	{0,      0x0,   0x3ff,    0x0,   1,      0,        3},  /* 6 */
	{0,      0x0,     0x0,  0x3ff,   1,      0,        3},  /* 7 */
	{0,      0x0,     0x0,    0x0,   1,      0,        3},  /* 8 */
};

static int lcd_venc_debug_test(struct aml_lcd_drv_s *pdrv, unsigned int num)
{
	unsigned int h_active, video_on_pixel;

	if (num >= LCD_ENC_TST_NUM_MAX)
		return -1;

	lcd_queue_work(&pdrv->test_check_work);

	h_active = pdrv->config.basic.h_active;
	video_on_pixel = pdrv->config.timing.hstart;
	if (num > 0)
		lcd_venc_gamma_debug_test_en(pdrv, 0);
	else
		lcd_venc_gamma_debug_test_en(pdrv, 1);

	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, lcd_enc_tst[num][6]);
	lcd_vcbus_write(ENCL_TST_MDSEL, lcd_enc_tst[num][0]);
	lcd_vcbus_write(ENCL_TST_Y, lcd_enc_tst[num][1]);
	lcd_vcbus_write(ENCL_TST_CB, lcd_enc_tst[num][2]);
	lcd_vcbus_write(ENCL_TST_CR, lcd_enc_tst[num][3]);
	lcd_vcbus_write(ENCL_TST_CLRBAR_STRT, video_on_pixel);
	lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH, (h_active / 9));
	lcd_vcbus_write(ENCL_TST_EN, lcd_enc_tst[num][4]);
	lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV, lcd_enc_tst[num][5], 3, 1);
	if (num > 0)
		LCDPR("[%d]: show test pattern: %s\n", pdrv->index, lcd_enc_tst_str[num]);

	return 0;
}

static void lcd_test_pattern_init(struct aml_lcd_drv_s *pdrv, unsigned int num)
{
	unsigned int h_active, video_on_pixel;

	num = (num >= LCD_ENC_TST_NUM_MAX) ? 0 : num;

	h_active = pdrv->config.basic.h_active;
	video_on_pixel = pdrv->config.timing.hstart;

	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, lcd_enc_tst[num][6]);
	lcd_vcbus_write(ENCL_TST_MDSEL, lcd_enc_tst[num][0]);
	lcd_vcbus_write(ENCL_TST_Y, lcd_enc_tst[num][1]);
	lcd_vcbus_write(ENCL_TST_CB, lcd_enc_tst[num][2]);
	lcd_vcbus_write(ENCL_TST_CR, lcd_enc_tst[num][3]);
	lcd_vcbus_write(ENCL_TST_CLRBAR_STRT, video_on_pixel);
	lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH, (h_active / 9));
	lcd_vcbus_write(ENCL_TST_EN, lcd_enc_tst[num][4]);
	lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV, lcd_enc_tst[num][5], 3, 1);
	if (num > 0)
		LCDPR("[%d]: init test pattern: %s\n", pdrv->index, lcd_enc_tst_str[num]);
}

static void lcd_venc_set(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	lcd_vcbus_write(ENCL_VIDEO_EN, 0);

	lcd_vcbus_write(ENCL_VIDEO_MODE, 0x8000); /* bit[15] shadown en */
	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV, 0x0418); /* Sampling rate: 1 */
	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL, 0x1000); /* bypass filter */

	lcd_set_venc_timing(pdrv);

	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, 3);
	//restore test pattern
	lcd_test_pattern_init(pdrv, pdrv->test_state);

	lcd_vcbus_write(ENCL_VIDEO_EN, 1);

	lcd_gamma_init(pdrv);
}

static void lcd_venc_vrr_recovery_dft(struct aml_lcd_drv_s *pdrv)
{
	unsigned int vtotal;

	vtotal = pdrv->config.basic.v_period;

	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT, vtotal);
}

static void lcd_venc_enable_ctrl(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (flag)
		lcd_vcbus_write(ENCL_VIDEO_EN, 1);
	else
		lcd_vcbus_write(ENCL_VIDEO_EN, 0);
}

static int lcd_venc_get_init_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int init_state;

	pconf->basic.h_active = lcd_vcbus_read(ENCL_VIDEO_HAVON_END)
		- lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN) + 1;
	pconf->basic.v_active = lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE)
		- lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE) + 1;
	pconf->basic.h_period = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT) + 1;
	pconf->basic.v_period = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;

	lcd_venc_gamma_check_en(pdrv);

	init_state = lcd_vcbus_read(ENCL_VIDEO_EN);
	return init_state;
}

static void lcd_venc_mute_set(struct aml_lcd_drv_s *pdrv, unsigned char flag)
{
	if (flag) {
		lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, 3);
		lcd_vcbus_write(ENCL_TST_MDSEL, 0);
		lcd_vcbus_write(ENCL_TST_Y, 0);
		lcd_vcbus_write(ENCL_TST_CB, 0);
		lcd_vcbus_write(ENCL_TST_CR, 0);
		lcd_vcbus_write(ENCL_TST_EN, 1);
		lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV, 0, 3, 1);
	} else {
		lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV, 1, 3, 1);
		lcd_vcbus_write(ENCL_TST_EN, 0);
	}
}

int lcd_venc_op_init_dft(struct aml_lcd_drv_s *pdrv,
			 struct lcd_venc_op_s *venc_op)
{
	if (!venc_op)
		return -1;

	venc_op->wait_vsync = lcd_wait_vsync_dft;
	venc_op->gamma_test_en = lcd_venc_gamma_debug_test_en;
	venc_op->venc_debug_test = lcd_venc_debug_test;
	venc_op->venc_set_timing = lcd_venc_set_timing;
	venc_op->venc_set = lcd_venc_set;
	venc_op->venc_change = lcd_venc_change_timing;
	venc_op->venc_enable = lcd_venc_enable_ctrl;
	venc_op->mute_set = lcd_venc_mute_set;
	venc_op->get_venc_init_config = lcd_venc_get_init_config;
	venc_op->venc_vrr_recovery = lcd_venc_vrr_recovery_dft;
	venc_op->get_encl_lint_cnt = lcd_venc_get_encl_lint_cnt;

	return 0;
};
