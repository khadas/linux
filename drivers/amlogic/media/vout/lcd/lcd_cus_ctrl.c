// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include "lcd_common.h"

int lcd_cus_ctrl_dump_raw_data(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	return 0;
}

int lcd_cus_ctrl_dump_info(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_cus_ctrl_attr_config_s *attr_conf;
	struct lcd_detail_timing_s *ptiming;
	int i, j, n, len = 0, ret, herr, verr;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\ncus_ctrl info:\n"
		"ctrl_en:     0x%x\n"
		"ctrl_cnt:    %d\n"
		"timing_ctrl: valid:%d, active:0x%x\n",
		pdrv->config.cus_ctrl.ctrl_en,
		pdrv->config.cus_ctrl.ctrl_cnt,
		pdrv->config.cus_ctrl.timing_ctrl_valid,
		pdrv->config.cus_ctrl.active_timing_type);

	if (!pdrv->config.cus_ctrl.attr_config) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "\n");
		return len;
	}
	for (i = 0; i < pdrv->config.cus_ctrl.ctrl_cnt; i++) {
		attr_conf = &pdrv->config.cus_ctrl.attr_config[i];
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "attr[%d] %s:\n"
			"  attr_type:  0x%02x\n"
			"  attr_flag:  %d\n"
			"  param_flag: %d\n"
			"  param_size: %d\n",
			attr_conf->attr_index,
			(attr_conf->active ? "(v)" : ""),
			attr_conf->attr_type,
			attr_conf->attr_flag,
			attr_conf->param_flag,
			attr_conf->param_size);

		if (attr_conf->param_size == 0)
			continue;
		switch (attr_conf->attr_type) {
		case LCD_CUS_CTRL_TYPE_UFR:
			if (!attr_conf->attr.ufr_attr)
				break;
			ret = lcd_config_timing_check(pdrv, &attr_conf->attr.ufr_attr->timing);
			verr = (ret >> 4) & 0xf;
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"  ufr_conf: %dx%d@%dhz\n"
				"    vtotal_min:  %d\n"
				"    vtotal_max:  %d\n",
				attr_conf->attr.ufr_attr->timing.h_active,
				attr_conf->attr.ufr_attr->timing.v_active,
				attr_conf->attr.ufr_attr->timing.frame_rate,
				attr_conf->attr.ufr_attr->timing.v_period_min,
				attr_conf->attr.ufr_attr->timing.v_period_max);
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"    fr_range:    %d~%d\n"
				"    vrr_range:   %d~%d\n",
				attr_conf->attr.ufr_attr->timing.frame_rate_min,
				attr_conf->attr.ufr_attr->timing.frame_rate_max,
				attr_conf->attr.ufr_attr->timing.vfreq_vrr_min,
				attr_conf->attr.ufr_attr->timing.vfreq_vrr_max);
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"    vpw:         %d\n"
				"    vbp:         %d%s\n"
				"    vfp:         %d%s\n",
				attr_conf->attr.ufr_attr->timing.vsync_width,
				attr_conf->attr.ufr_attr->timing.vsync_bp,
				((verr & 0x4) ? "(X)" : ((verr & 0x8) ? "(!)" : "")),
				attr_conf->attr.ufr_attr->timing.vsync_fp,
				((verr & 0x1) ? "(X)" : ((verr & 0x2) ? "(!)" : "")));
			break;
		case LCD_CUS_CTRL_TYPE_DFR:
			if (!attr_conf->attr.dfr_attr || !attr_conf->attr.dfr_attr->fr)
				break;
			for (j = 0; j < attr_conf->attr.dfr_attr->fr_cnt; j++) {
				ptiming = &attr_conf->attr.dfr_attr->fr[j].timing;
				ret = lcd_config_timing_check(pdrv, ptiming);
				herr = ret & 0xf;
				verr = (ret >> 4) & 0xf;
				n = lcd_debug_info_len(len + offset);
				len += snprintf((buf + len), n,
					"  dfr_conf[%d]: %dx%d@%dhz %s:\n"
					"    fr_range:      %d~%d\n"
					"    vrr_range:     %d~%d\n"
					"    is_dft_timing: %d\n"
					"    timing_check:  hbp(%s),hfp(%s),vbp(%s),vfp(%s)\n",
					j,
					ptiming->h_active,
					ptiming->v_active,
					ptiming->frame_rate,
					attr_conf->active ?
						((attr_conf->priv_sel == j) ? "(v)" : "") : "",
					ptiming->frame_rate_min,
					ptiming->frame_rate_max,
					ptiming->vfreq_vrr_min,
					ptiming->vfreq_vrr_max,
					(attr_conf->attr.dfr_attr->fr[j].timing_index ? 0 : 1),
					((herr & 0x4) ? "X" : ((herr & 0x8) ? "!" : "v")),
					((herr & 0x1) ? "X" : ((herr & 0x2) ? "!" : "v")),
					((verr & 0x4) ? "X" : ((verr & 0x8) ? "!" : "v")),
					((verr & 0x1) ? "X" : ((verr & 0x2) ? "!" : "v")));
			}
			break;
		case LCD_CUS_CTRL_TYPE_EXTEND_TMG:
			if (!attr_conf->attr.extend_tmg_attr ||
			    !attr_conf->attr.extend_tmg_attr->timing)
				break;
			for (j = 0; j < attr_conf->attr.extend_tmg_attr->group_cnt; j++) {
				ptiming = &attr_conf->attr.extend_tmg_attr->timing[j];
				ret = lcd_config_timing_check(pdrv, ptiming);
				herr = ret & 0xf;
				verr = (ret >> 4) & 0xf;
				n = lcd_debug_info_len(len + offset);
				len += snprintf((buf + len), n,
					"  extend_tmg_conf[%d]: %dx%d@%dhz %s:\n"
					"    fr_adj_type: %d\n"
					"    pclk:      %dHz\n"
					"    htotal:    %d\n"
					"    vtotal:    %d\n"
					"    hsync:     %d %d%s %d%s %d\n"
					"    vsync:     %d %d%s %d%s %d\n"
					"    fr_range:  %d~%d\n"
					"    vrr_range: %d~%d\n",
					j,
					ptiming->h_active,
					ptiming->v_active,
					ptiming->frame_rate,
					attr_conf->active ?
						((attr_conf->priv_sel == j) ? "(v)" : "") : "",
					ptiming->fr_adjust_type,
					ptiming->pixel_clk, ptiming->h_period,
					ptiming->v_period, ptiming->hsync_width,
					ptiming->hsync_bp,
					((herr & 0x4) ? "(X)" : ((herr & 0x8) ? "(!)" : "")),
					ptiming->hsync_fp,
					((herr & 0x1) ? "(X)" : ((herr & 0x2) ? "(!)" : "")),
					ptiming->hsync_pol,
					ptiming->vsync_width,
					ptiming->vsync_bp,
					((verr & 0x4) ? "(X)" : ((verr & 0x8) ? "(!)" : "")),
					ptiming->vsync_fp,
					((verr & 0x1) ? "(X)" : ((verr & 0x2) ? "(!)" : "")),
					ptiming->vsync_pol,
					ptiming->frame_rate_min,
					ptiming->frame_rate_max,
					ptiming->vfreq_vrr_min,
					ptiming->vfreq_vrr_max);
			}
			break;
		case LCD_CUS_CTRL_TYPE_CLK_ADV:
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"  clk_adv:\n"
				"    ss_freq:  %d\n"
				"    ss_mode:  %d\n",
				attr_conf->attr.clk_adv_attr->ss_freq,
				attr_conf->attr.clk_adv_attr->ss_mode);
			break;
		case LCD_CUS_CTRL_TYPE_TCON_SW_POL:
			break;
		case LCD_CUS_CTRL_TYPE_TCON_SW_PDF:
			break;
		default:
			break;
		}
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\n");
	return len;
}

int lcd_cus_ctrl_load_from_dts(struct aml_lcd_drv_s *pdrv, struct device_node *child)
{
	//todo
	return 0;
}

static int lcd_cus_ctrl_attr_parse_ufr_ukey(struct aml_lcd_drv_s *pdrv,
		struct lcd_cus_ctrl_attr_config_s *attr_conf, unsigned char *p)
{
	struct lcd_ufr_s *ufr_attr;
	struct lcd_detail_timing_s *ptiming;
	unsigned int hfp, vfp, offset = 0;

	ufr_attr = kzalloc(sizeof(*ufr_attr), GFP_KERNEL);
	if (!ufr_attr)
		return -1;

	ptiming = &ufr_attr->timing;
	memcpy(ptiming, &pdrv->config.timing.dft_timing, sizeof(struct lcd_detail_timing_s));

	ptiming->v_period_min = *(unsigned short *)(p + offset);
	offset += 2;
	ptiming->v_period_max = *(unsigned short *)(p + offset);
	offset += 2;
	if (attr_conf->param_size < offset)
		goto lcd_cus_ctrl_attr_parse_ufr_ukey_err;
	if (attr_conf->param_size == offset) {
		//frame_rate range is update for compatibility in lcd_fr_range_update
		ptiming->frame_rate_min = 0;
		ptiming->frame_rate_max = 0;
		goto lcd_cus_ctrl_attr_parse_ufr_ukey_next;
	}

	ptiming->frame_rate_min = *(unsigned short *)(p + offset);
	offset += 2;
	ptiming->frame_rate_max = *(unsigned short *)(p + offset);
	offset += 2;
	if (attr_conf->param_size < offset)
		goto lcd_cus_ctrl_attr_parse_ufr_ukey_err;
	if (attr_conf->param_size == offset)
		goto lcd_cus_ctrl_attr_parse_ufr_ukey_next;

	ptiming->vsync_width = *(unsigned short *)(p + offset);
	offset += 2;
	ptiming->vsync_bp = *(unsigned short *)(p + offset);
	offset += 2;
	if (attr_conf->param_size < offset)
		goto lcd_cus_ctrl_attr_parse_ufr_ukey_err;

lcd_cus_ctrl_attr_parse_ufr_ukey_next:
	ptiming->v_active /= 2;
	ptiming->v_period /= 2;
	hfp = ptiming->h_period - ptiming->h_active - ptiming->hsync_width - ptiming->hsync_bp;
	vfp = ptiming->v_period - ptiming->v_active - ptiming->vsync_width - ptiming->vsync_bp;
	ptiming->hsync_fp = hfp;
	ptiming->vsync_fp = vfp;
	lcd_clk_frame_rate_init(ptiming);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: %dx%dp%dhz\n",
			pdrv->index, __func__,
			ptiming->h_active, ptiming->v_active, ptiming->frame_rate);
	}

	lcd_config_timing_check(pdrv, ptiming);

	pdrv->config.cus_ctrl.timing_cnt += 1;
	attr_conf->attr.ufr_attr = ufr_attr;
	return 0;

lcd_cus_ctrl_attr_parse_ufr_ukey_err:
	kfree(ufr_attr);
	LCDERR("[%d]: %s: param_size(%d) and offset(%d) are mismatch!\n",
		pdrv->index, __func__, attr_conf->param_size, offset);
	return -1;
}

static int lcd_cus_ctrl_attr_parse_dfr_ukey(struct aml_lcd_drv_s *pdrv,
		struct lcd_cus_ctrl_attr_config_s *attr_conf, unsigned char *p)
{
	struct lcd_dfr_s *dfr_attr;
	struct lcd_detail_timing_s *ptiming;
	unsigned int offset = 0;
	unsigned int hfp, vfp, temp;
	int i, n;

	dfr_attr = kzalloc(sizeof(*dfr_attr), GFP_KERNEL);
	if (!dfr_attr)
		return -1;

	dfr_attr->fr_cnt = *(p + offset);
	offset += 1;
	dfr_attr->tmg_group_cnt = *(p + offset);
	offset += 1;

	if (dfr_attr->fr_cnt) {
		dfr_attr->fr = kcalloc(dfr_attr->fr_cnt, sizeof(struct lcd_dfr_fr_s), GFP_KERNEL);
		if (!dfr_attr->fr) {
			kfree(dfr_attr);
			return -1;
		}
		for (i = 0; i < dfr_attr->fr_cnt; i++) {
			temp = *(unsigned short *)(p + offset);
			dfr_attr->fr[i].frame_rate = temp & 0xfff;
			dfr_attr->fr[i].timing_index = (temp >> 12) & 0xf;
			offset += 2;
		}
	}

	if (dfr_attr->tmg_group_cnt) {
		dfr_attr->dfr_timing = kcalloc(dfr_attr->tmg_group_cnt,
				sizeof(struct lcd_dfr_timing_s), GFP_KERNEL);
		if (!dfr_attr->dfr_timing) {
			kfree(dfr_attr->fr);
			kfree(dfr_attr);
			return -1;
		}
		for (i = 0;  i < dfr_attr->tmg_group_cnt; i++) {
			dfr_attr->dfr_timing[i].htotal = *(unsigned short *)(p + offset);
			offset += 2;
			dfr_attr->dfr_timing[i].vtotal = *(unsigned short *)(p + offset);
			offset += 2;
			dfr_attr->dfr_timing[i].vtotal_min = *(unsigned short *)(p + offset);
			offset += 2;
			dfr_attr->dfr_timing[i].vtotal_max = *(unsigned short *)(p + offset);
			offset += 2;
			dfr_attr->dfr_timing[i].frame_rate_min = *(unsigned short *)(p + offset);
			offset += 2;
			dfr_attr->dfr_timing[i].frame_rate_max = *(unsigned short *)(p + offset);
			offset += 2;
			dfr_attr->dfr_timing[i].hpw = *(unsigned short *)(p + offset);
			offset += 2;
			dfr_attr->dfr_timing[i].hbp = *(unsigned short *)(p + offset);
			offset += 2;
			dfr_attr->dfr_timing[i].vpw = *(unsigned short *)(p + offset);
			offset += 2;
			dfr_attr->dfr_timing[i].vbp = *(unsigned short *)(p + offset);
			offset += 2;
		}
	}

	if (attr_conf->param_size < offset) {
		kfree(dfr_attr->fr);
		kfree(dfr_attr->dfr_timing);
		kfree(dfr_attr);
		LCDERR("[%d]: %s: param_size(%d) and offset(%d) are mismatch!\n",
			pdrv->index, __func__, attr_conf->param_size, offset);
		return -1;
	}

	for (i = 0; i < dfr_attr->fr_cnt; i++) {
		ptiming = &dfr_attr->fr[i].timing;
		memcpy(ptiming, &pdrv->config.timing.dft_timing,
			sizeof(struct lcd_detail_timing_s));
		ptiming->frame_rate = dfr_attr->fr[i].frame_rate;
		if (dfr_attr->fr[i].timing_index == 0) {
			ptiming->pixel_clk = ptiming->frame_rate *
				ptiming->h_period * ptiming->v_period;
			//frame_rate range is update for compatibility in lcd_fr_range_update
			ptiming->frame_rate_min = 0;
			ptiming->frame_rate_max = 0;
		} else {
			n = dfr_attr->fr[i].timing_index - 1;
			if (n >= dfr_attr->tmg_group_cnt) {
				LCDERR("[%d]: %s: timing_index %d err, tmg_group_cnt %d\n",
					pdrv->index, __func__, dfr_attr->fr[i].timing_index,
					dfr_attr->tmg_group_cnt);
				kfree(dfr_attr->fr);
				kfree(dfr_attr->dfr_timing);
				kfree(dfr_attr);
				return -1;
			}
			ptiming->h_period = dfr_attr->dfr_timing[n].htotal;
			ptiming->v_period = dfr_attr->dfr_timing[n].vtotal;
			ptiming->v_period_min = dfr_attr->dfr_timing[n].vtotal_min;
			ptiming->v_period_max = dfr_attr->dfr_timing[n].vtotal_max;
			ptiming->frame_rate_min = dfr_attr->dfr_timing[n].frame_rate_min;
			ptiming->frame_rate_max = dfr_attr->dfr_timing[n].frame_rate_max;
			ptiming->pixel_clk = ptiming->frame_rate *
				ptiming->h_period * ptiming->v_period;
		}
		hfp = ptiming->h_period - ptiming->h_active -
			ptiming->hsync_width - ptiming->hsync_bp;
		vfp = ptiming->v_period - ptiming->v_active -
			ptiming->vsync_width - ptiming->vsync_bp;
		ptiming->hsync_fp = hfp;
		ptiming->vsync_fp = vfp;
		lcd_clk_frame_rate_init(ptiming);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: %s: dfr[%d]: %dx%dp%dhz\n",
				pdrv->index, __func__, i,
				ptiming->h_active, ptiming->v_active, ptiming->frame_rate);
		}

		lcd_config_timing_check(pdrv, ptiming);
	}

	pdrv->config.cus_ctrl.timing_cnt += dfr_attr->fr_cnt;
	attr_conf->attr.dfr_attr = dfr_attr;
	return 0;
}

static int lcd_cus_ctrl_attr_parse_extend_tmg_ukey(struct aml_lcd_drv_s *pdrv,
		struct lcd_cus_ctrl_attr_config_s *attr_conf, unsigned char *p)
{
	struct lcd_extend_tmg_s *extend_tmg_attr;
	struct lcd_detail_timing_s *ptiming;
	unsigned int offset = 0;
	unsigned int hfp, vfp, temp;
	int i;

	extend_tmg_attr = kzalloc(sizeof(*extend_tmg_attr), GFP_KERNEL);
	if (!extend_tmg_attr)
		return -1;

	extend_tmg_attr->group_cnt = attr_conf->param_flag;
	if (extend_tmg_attr->group_cnt) {
		extend_tmg_attr->timing = kcalloc(extend_tmg_attr->group_cnt,
				sizeof(struct lcd_detail_timing_s), GFP_KERNEL);
		if (!extend_tmg_attr->timing) {
			kfree(extend_tmg_attr);
			return -1;
		}
		for (i = 0;  i < extend_tmg_attr->group_cnt; i++) {
			ptiming = &extend_tmg_attr->timing[i];
			ptiming->h_active = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->v_active = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->h_period = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->v_period = *(unsigned short *)(p + offset);
			offset += 2;
			temp = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->hsync_width = temp & 0xfff;
			ptiming->hsync_pol = (temp >> 12) & 0xf;
			ptiming->hsync_bp = *(unsigned short *)(p + offset);
			offset += 2;
			temp = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->vsync_width = temp & 0xfff;
			ptiming->vsync_pol = (temp >> 12) & 0xf;
			ptiming->vsync_bp = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->fr_adjust_type = *(p + offset);
			offset += 1;
			ptiming->pixel_clk = *(unsigned int *)(p + offset);
			offset += 4;

			ptiming->h_period_min = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->h_period_max = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->v_period_min = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->v_period_max = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->frame_rate_min = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->frame_rate_max = *(unsigned short *)(p + offset);
			offset += 2;
			ptiming->pclk_min = *(unsigned int *)(p + offset);
			offset += 4;
			ptiming->pclk_max = *(unsigned int *)(p + offset);
			offset += 4;

			hfp = ptiming->h_period - ptiming->h_active -
				ptiming->hsync_width - ptiming->hsync_bp;
			vfp = ptiming->v_period - ptiming->v_active -
				ptiming->vsync_width - ptiming->vsync_bp;
			ptiming->hsync_fp = hfp;
			ptiming->vsync_fp = vfp;

			lcd_clk_frame_rate_init(&extend_tmg_attr->timing[i]);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: %s: extend_tmg[%d]: %dx%dp%dhz\n",
					pdrv->index, __func__, i,
					ptiming->h_active, ptiming->v_active, ptiming->frame_rate);
			}

			lcd_config_timing_check(pdrv, ptiming);
		}
	}

	if (attr_conf->param_size < offset) {
		kfree(extend_tmg_attr->timing);
		kfree(extend_tmg_attr);
		LCDERR("[%d]: %s: param_size(%d) and offset(%d) are mismatch!\n",
			pdrv->index, __func__, attr_conf->param_size, offset);
		return -1;
	}

	pdrv->config.cus_ctrl.timing_cnt += extend_tmg_attr->group_cnt;
	attr_conf->attr.extend_tmg_attr = extend_tmg_attr;
	return 0;
}

static int lcd_cus_ctrl_attr_parse_clk_adv_ukey(struct aml_lcd_drv_s *pdrv,
		struct lcd_cus_ctrl_attr_config_s *attr_conf, unsigned char *p)
{
	struct lcd_clk_adv_s *adv_attr;
	unsigned int offset = 0;

	adv_attr = kzalloc(sizeof(*adv_attr), GFP_KERNEL);
	if (!adv_attr)
		return -1;

	if (attr_conf->attr_flag & (1 << 0)) {
		adv_attr->ss_freq = *(unsigned char *)(p + offset);
		offset += 1;
		adv_attr->ss_mode = *(unsigned char *)(p + offset);
		offset += 1;
	}

	if (attr_conf->param_size < offset) {
		kfree(adv_attr);
		LCDERR("[%d]: %s: param_size(%d) and offset(%d) are mismatch!\n",
			pdrv->index, __func__, attr_conf->param_size, offset);
		return -1;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: ss_freq = %d, ss_mode = %d\n",
			pdrv->index, __func__,
			adv_attr->ss_freq, adv_attr->ss_mode);
	}

	attr_conf->attr.clk_adv_attr = adv_attr;
	return 0;
}

static int lcd_cus_ctrl_attr_parse_tcon_sw_pol_ukey(struct aml_lcd_drv_s *pdrv,
		struct lcd_cus_ctrl_attr_config_s *attr_conf, unsigned char *p)
{
	return 0;
}

static int lcd_cus_ctrl_attr_parse_tcon_sw_pdf_ukey(struct aml_lcd_drv_s *pdrv,
		struct lcd_cus_ctrl_attr_config_s *attr_conf, unsigned char *p)
{
	return 0;
}

int lcd_cus_ctrl_load_from_unifykey(struct aml_lcd_drv_s *pdrv, unsigned char *buf,
		unsigned int max_size)
{
	unsigned char *p;
	struct lcd_cus_ctrl_attr_config_s *attr_conf;
	unsigned int ctrl_en, ctrl_attr, ctrl_cnt = 0;
	unsigned int offset = 0, param_size, i, n;
	unsigned char timing_ctrl_valid = 0;
	char str[128];
	int len, ret;

	ctrl_en = *(unsigned int *)buf;
	for (i = 0; i < LCD_CUS_CTRL_ATTR_CNT_MAX; i++) {
		if (ctrl_en & (1 << i))
			ctrl_cnt = i + 1;
	}
	if (ctrl_cnt == 0)
		return 0;

	pdrv->config.cus_ctrl.ctrl_en = ctrl_en;
	pdrv->config.cus_ctrl.ctrl_cnt = ctrl_cnt;
	pdrv->config.cus_ctrl.timing_cnt = 0;
	pdrv->config.cus_ctrl.active_timing_type = LCD_CUS_CTRL_TYPE_MAX;
	pdrv->config.cus_ctrl.timing_switch_flag = 0;

	attr_conf = kcalloc(ctrl_cnt, sizeof(*attr_conf), GFP_KERNEL);
	if (!attr_conf)
		return -1;
	pdrv->config.cus_ctrl.attr_config = attr_conf;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: ctrl_en=0x%x, ctrl_cnt=%d\n",
			pdrv->index, __func__, ctrl_en, ctrl_cnt);
	}

	offset = 4; //ctrl_en size
	n = 0;
	for (i = 0; i < LCD_CUS_CTRL_ATTR_CNT_MAX; i++) {
		if (n >= ctrl_cnt)
			break;
		if (offset >= max_size) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: %s: offset %d reach max_size %d, exit\n",
					pdrv->index, __func__, offset, max_size);
			}
			break;
		}
		p = buf + offset;

		if (ctrl_en & (1 << i)) { //valid
			ctrl_attr = *(unsigned short *)p;
			attr_conf[n].attr_index = i;
			attr_conf[n].attr_flag = ctrl_attr & 0xf;
			attr_conf[n].param_flag = (ctrl_attr >> 4) & 0xf;
			attr_conf[n].attr_type = (ctrl_attr >> 8) & 0xff;
			param_size = *(unsigned short *)(p + 2);
			attr_conf[n].param_size = param_size;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				len = sprintf(str, "attr_type=%x, attr_flag=%d, ",
					attr_conf[n].attr_type,
					attr_conf[n].attr_flag);
				sprintf(str + len, "param_flag=%d, param_size=%d",
					attr_conf[n].param_flag,
					attr_conf[n].param_size);
				LCDPR("[%d]: %s: attr[%d]: %s\n",
					pdrv->index, __func__, i, str);
			}

			switch (attr_conf[n].attr_type) {
			case LCD_CUS_CTRL_TYPE_UFR:
				if (attr_conf[n].attr_flag > 0)
					timing_ctrl_valid = 1;
				ret = lcd_cus_ctrl_attr_parse_ufr_ukey(pdrv,
					&attr_conf[n], (p + 4));
				break;
			case LCD_CUS_CTRL_TYPE_DFR:
				if (attr_conf[n].attr_flag > 0)
					timing_ctrl_valid = 1;
				ret = lcd_cus_ctrl_attr_parse_dfr_ukey(pdrv,
					&attr_conf[n], (p + 4));
				break;
			case LCD_CUS_CTRL_TYPE_EXTEND_TMG:
				if (attr_conf[n].attr_flag > 0)
					timing_ctrl_valid = 1;
				ret = lcd_cus_ctrl_attr_parse_extend_tmg_ukey(pdrv,
					&attr_conf[n], (p + 4));
				break;
			case LCD_CUS_CTRL_TYPE_CLK_ADV:
				ret = lcd_cus_ctrl_attr_parse_clk_adv_ukey(pdrv,
					&attr_conf[n], (p + 4));
				break;
			case LCD_CUS_CTRL_TYPE_TCON_SW_POL:
				LCDERR("todo\n");
				ret = lcd_cus_ctrl_attr_parse_tcon_sw_pol_ukey(pdrv,
					&attr_conf[n], (p + 4));
				break;
			case LCD_CUS_CTRL_TYPE_TCON_SW_PDF:
				LCDERR("todo\n");
				ret = lcd_cus_ctrl_attr_parse_tcon_sw_pdf_ukey(pdrv,
					&attr_conf[n], (p + 4));
				break;
			default:
				LCDERR("[%d]: %s: invalid attr_type: 0x%x\n",
					pdrv->index, __func__, attr_conf[n].attr_type);
				goto lcd_cus_ctrl_load_from_unifykey_err;
			}
			n++;
		} else {
			param_size = 0;
			ret = 0;
		}
		if (ret)
			goto lcd_cus_ctrl_load_from_unifykey_err;
		offset += (param_size + 4); //4 for attr_x and param_size
	}

	if (timing_ctrl_valid)
		pdrv->config.cus_ctrl.timing_ctrl_valid = timing_ctrl_valid;
	return 0;

lcd_cus_ctrl_load_from_unifykey_err:
	lcd_cus_ctrl_config_remove(pdrv);
	return -1;
}

void lcd_cus_ctrl_config_remove(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_cus_ctrl_attr_config_s *attr_conf;
	int i;

	if (!pdrv->config.cus_ctrl.attr_config)
		return;

	for (i = 0; i < pdrv->config.cus_ctrl.ctrl_cnt; i++) {
		attr_conf = &pdrv->config.cus_ctrl.attr_config[i];
		switch (attr_conf->attr_type) {
		case LCD_CUS_CTRL_TYPE_UFR:
			kfree(attr_conf->attr.ufr_attr);
			break;
		case LCD_CUS_CTRL_TYPE_DFR:
			if (attr_conf->attr.dfr_attr) {
				kfree(attr_conf->attr.dfr_attr->fr);
				kfree(attr_conf->attr.dfr_attr->dfr_timing);
				kfree(attr_conf->attr.dfr_attr);
			}
			break;
		case LCD_CUS_CTRL_TYPE_EXTEND_TMG:
			if (attr_conf->attr.extend_tmg_attr) {
				kfree(attr_conf->attr.extend_tmg_attr->timing);
				kfree(attr_conf->attr.extend_tmg_attr);
			}
			break;
		case LCD_CUS_CTRL_TYPE_CLK_ADV:
			kfree(attr_conf->attr.clk_adv_attr);
			break;
		case LCD_CUS_CTRL_TYPE_TCON_SW_POL:
		case LCD_CUS_CTRL_TYPE_TCON_SW_PDF:
		default:
			break;
		}
	}
	kfree(pdrv->config.cus_ctrl.attr_config);
	pdrv->config.cus_ctrl.attr_config = NULL;
}

static int lcd_cus_ctrl_config_update_clk_adv(struct aml_lcd_drv_s *pdrv,
		struct lcd_cus_ctrl_attr_config_s *attr_conf)
{
	if (attr_conf->attr_flag & (1 << 0)) {
		pdrv->config.timing.ss_freq = attr_conf->attr.clk_adv_attr->ss_freq;
		pdrv->config.timing.ss_mode = attr_conf->attr.clk_adv_attr->ss_mode;
	}

	attr_conf->active = 1;
	return 0;
}

int lcd_cus_ctrl_config_update(struct aml_lcd_drv_s *pdrv, void *param, unsigned int mask_sel)
{
	struct lcd_cus_ctrl_attr_config_s *attr_conf;
	struct lcd_detail_timing_s *tmg_match;
	struct lcd_detail_timing_s *ptiming;
	struct lcd_dfr_s *p_dfr;
	char str[128];
	int i, j, ret = -1;

	if (!pdrv->config.cus_ctrl.attr_config)
		return -1;

	for (i = 0; i < pdrv->config.cus_ctrl.ctrl_cnt; i++) {
		attr_conf = &pdrv->config.cus_ctrl.attr_config[i];
		switch (attr_conf->attr_type) {
		case LCD_CUS_CTRL_TYPE_UFR:
			if ((mask_sel & LCD_CUS_CTRL_SEL_UFR) == 0)
				break;
			if (attr_conf->attr_flag == 0)
				break;
			if (!attr_conf->attr.ufr_attr || !param)
				break;

			tmg_match = (struct lcd_detail_timing_s *)param;
			ptiming = &attr_conf->attr.ufr_attr->timing;
			if (tmg_match == ptiming) {
				pdrv->config.cus_ctrl.active_timing_type = LCD_CUS_CTRL_TYPE_UFR;
				pdrv->config.cus_ctrl.timing_switch_flag = attr_conf->attr_flag;
				attr_conf->active = 1;
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("[%d]: %s: attr[%d] ufr: %dx%dp%dhz\n",
						pdrv->index, __func__, i,
						ptiming->h_active,
						ptiming->v_active,
						ptiming->frame_rate);
				}
				return 0;
			}
			break;
		case LCD_CUS_CTRL_TYPE_DFR:
			if ((mask_sel & LCD_CUS_CTRL_SEL_DFR) == 0)
				break;
			if (attr_conf->attr_flag == 0)
				break;
			if (!attr_conf->attr.dfr_attr || !param)
				break;
			tmg_match = (struct lcd_detail_timing_s *)param;
			p_dfr = attr_conf->attr.dfr_attr;
			for (j = 0; j < p_dfr->fr_cnt; j++) {
				ptiming = &p_dfr->fr[j].timing;
				if (tmg_match == ptiming) {
					pdrv->config.cus_ctrl.active_timing_type =
						LCD_CUS_CTRL_TYPE_DFR;
					pdrv->config.cus_ctrl.timing_switch_flag =
						attr_conf->attr_flag;
					attr_conf->active = 1;
					attr_conf->priv_sel = j;
					if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
						LCDPR("[%d]: %s: attr[%d] dfr[%d]: %dx%dp%dhz\n",
							pdrv->index, __func__, i, j,
							ptiming->h_active,
							ptiming->v_active,
							ptiming->frame_rate);
					}
					return 0;
				}
			}
			break;
		case LCD_CUS_CTRL_TYPE_EXTEND_TMG:
			if ((mask_sel & LCD_CUS_CTRL_SEL_EXTEND_TMG) == 0)
				break;
			if (attr_conf->attr_flag == 0)
				break;
			if (!attr_conf->attr.extend_tmg_attr || !param)
				break;
			tmg_match = (struct lcd_detail_timing_s *)param;
			for (j = 0; j < attr_conf->attr.extend_tmg_attr->group_cnt; j++) {
				ptiming = &attr_conf->attr.extend_tmg_attr->timing[j];
				if (tmg_match == ptiming) {
					pdrv->config.cus_ctrl.active_timing_type =
						LCD_CUS_CTRL_TYPE_EXTEND_TMG;
					pdrv->config.cus_ctrl.timing_switch_flag =
						attr_conf->attr_flag;
					attr_conf->active = 1;
					attr_conf->priv_sel = j;
					if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
						sprintf(str, "extend_tmg[%d]: %dx%dp%dhz",
							j, ptiming->h_active,
							ptiming->v_active,
							ptiming->frame_rate);
						LCDPR("[%d]: %s: attr[%d] %s\n",
							pdrv->index, __func__, i, str);
					}
					return 0;
				}
			}
			break;
		case LCD_CUS_CTRL_TYPE_CLK_ADV:
			if ((mask_sel & LCD_CUS_CTRL_SEL_CLK_ADV) == 0)
				break;
			if (!attr_conf->attr.clk_adv_attr)
				break;
			ret = lcd_cus_ctrl_config_update_clk_adv(pdrv, attr_conf);
			return ret;
		case LCD_CUS_CTRL_TYPE_TCON_SW_POL:
			if ((mask_sel & LCD_CUS_CTRL_SEL_TCON_SW_POL) == 0)
				break;
			break;
		case LCD_CUS_CTRL_TYPE_TCON_SW_PDF:
			if ((mask_sel & LCD_CUS_CTRL_SEL_TCON_SW_PDF) == 0)
				break;
			break;
		default:
			break;
		}
	}

	return ret;
}

void lcd_cus_ctrl_state_clear(struct aml_lcd_drv_s *pdrv, unsigned int mask_sel)
{
	struct lcd_cus_ctrl_attr_config_s *attr_conf;
	int i, tmg_clr = 0;

	if (!pdrv->config.cus_ctrl.attr_config)
		return;

	for (i = 0; i < pdrv->config.cus_ctrl.ctrl_cnt; i++) {
		attr_conf = &pdrv->config.cus_ctrl.attr_config[i];
		switch (attr_conf->attr_type) {
		case LCD_CUS_CTRL_TYPE_UFR:
			if (mask_sel & LCD_CUS_CTRL_SEL_UFR) {
				attr_conf->active = 0;
				tmg_clr = 1;
			}
			break;
		case LCD_CUS_CTRL_TYPE_DFR:
			if (mask_sel & LCD_CUS_CTRL_SEL_DFR) {
				attr_conf->active = 0;
				tmg_clr = 1;
			}
			break;
		case LCD_CUS_CTRL_TYPE_EXTEND_TMG:
			if (mask_sel & LCD_CUS_CTRL_SEL_EXTEND_TMG) {
				attr_conf->active = 0;
				tmg_clr = 1;
			}
			break;
		case LCD_CUS_CTRL_TYPE_CLK_ADV:
			if (mask_sel & LCD_CUS_CTRL_SEL_CLK_ADV)
				attr_conf->active = 0;
			break;
		case LCD_CUS_CTRL_TYPE_TCON_SW_POL:
			if (mask_sel & LCD_CUS_CTRL_SEL_TCON_SW_POL)
				attr_conf->active = 0;
			break;
		case LCD_CUS_CTRL_TYPE_TCON_SW_PDF:
			if (mask_sel & LCD_CUS_CTRL_SEL_TCON_SW_PDF)
				attr_conf->active = 0;
			break;
		default:
			break;
		}
	}
	if (tmg_clr) {
		pdrv->config.cus_ctrl.active_timing_type = LCD_CUS_CTRL_TYPE_MAX;
		pdrv->config.cus_ctrl.timing_switch_flag = 0;
	}
}

int lcd_cus_ctrl_timing_is_valid(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->config.cus_ctrl.timing_ctrl_valid)
		return 1;

	return 0;
}

int lcd_cus_ctrl_timing_is_activated(struct aml_lcd_drv_s *pdrv)
{
	int ret = 0;

	switch (pdrv->config.cus_ctrl.active_timing_type) {
	case LCD_CUS_CTRL_TYPE_UFR:
	case LCD_CUS_CTRL_TYPE_DFR:
	case LCD_CUS_CTRL_TYPE_EXTEND_TMG:
		if (pdrv->config.cus_ctrl.timing_switch_flag > 0)
			ret = 1;
		break;
	default:
		break;
	}

	return ret;
}

struct lcd_detail_timing_s **lcd_cus_ctrl_timing_match_get(struct aml_lcd_drv_s *pdrv)
{
	union lcd_cus_ctrl_attr_u *cus_ctrl_attr;
	struct lcd_detail_timing_s **timing_match;
	int cnt, i, j, n = 0;

	if (!pdrv->config.cus_ctrl.attr_config)
		return NULL;

	cnt = pdrv->config.cus_ctrl.timing_cnt;
	timing_match = kcalloc(cnt, sizeof(*timing_match), GFP_KERNEL);
	if (!timing_match)
		return NULL;

	for (i = 0; i < pdrv->config.cus_ctrl.ctrl_cnt; i++) {
		cus_ctrl_attr = &pdrv->config.cus_ctrl.attr_config[i].attr;
		switch (pdrv->config.cus_ctrl.attr_config[i].attr_type) {
		case LCD_CUS_CTRL_TYPE_UFR:
			if (!cus_ctrl_attr->ufr_attr)
				break;
			timing_match[n] = &cus_ctrl_attr->ufr_attr->timing;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: %s: attr[%d] ufr: %dx%dp%dhz\n",
					pdrv->index, __func__, i,
					timing_match[n]->h_active,
					timing_match[n]->v_active,
					timing_match[n]->frame_rate);
			}
			n++;
			break;
		case LCD_CUS_CTRL_TYPE_DFR:
			if (!cus_ctrl_attr->dfr_attr || !cus_ctrl_attr->dfr_attr->fr)
				break;
			for (j = 0; j < cus_ctrl_attr->dfr_attr->fr_cnt; j++) {
				timing_match[n] = &cus_ctrl_attr->dfr_attr->fr[j].timing;
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("[%d]: %s: attr[%d] dfr[%d]: %dx%dp%dhz\n",
						pdrv->index, __func__, i, j,
						timing_match[n]->h_active,
						timing_match[n]->v_active,
						timing_match[n]->frame_rate);
				}
				n++;
			}
			break;
		case LCD_CUS_CTRL_TYPE_EXTEND_TMG:
			if (!cus_ctrl_attr->extend_tmg_attr ||
			    !cus_ctrl_attr->extend_tmg_attr->timing)
				break;
			for (j = 0; j < cus_ctrl_attr->extend_tmg_attr->group_cnt; j++) {
				timing_match[n] = &cus_ctrl_attr->extend_tmg_attr->timing[j];
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("[%d]: %s: attr[%d] extend_tmg[%d]: %dx%dp%dhz\n",
						pdrv->index, __func__, i, j,
						timing_match[n]->h_active,
						timing_match[n]->v_active,
						timing_match[n]->frame_rate);
				}
				n++;
			}
			break;
		default:
			break;
		}
	}

	return timing_match;
}
