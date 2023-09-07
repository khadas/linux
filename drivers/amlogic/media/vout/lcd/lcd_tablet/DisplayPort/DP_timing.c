// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "../lcd_tablet.h"
#include "DP_tx.h"
#include "../../lcd_reg.h"
#include "../../lcd_common.h"

__maybe_unused struct dptx_detail_timing_s DP_safemode_640x480 = {
	.h_a = 480,
	.v_a = 640,
	.h_b = 144,
	.v_b = 29,
	.pclk = 25175000,
	.h_pw = 40,
	.h_fp = 8,
	.v_pw = 2,
	.v_fp = 2,
	.timing_ctrl = 0x0,
	.v_size = 90,
	.h_size = 160,

	.flag = TIMING_FLAG_VALID | TIMING_FLAG_SUPPORT | TIMING_FLAG_PRESET,
	.timing_res = DP_TIMING_1080P_LOWER,
};

__maybe_unused struct dptx_detail_timing_s DP_std_1080P60 = {
	.h_a = 1920,
	.v_a = 1080,
	.h_b = 280,
	.v_b = 45,
	.pclk = 148500000,
	.h_pw = 40,
	.h_fp = 88,
	.v_pw = 5,
	.v_fp = 4,
	.timing_ctrl = 0x0,
	.v_size = 90,
	.h_size = 160,

	.flag = TIMING_FLAG_VALID | TIMING_FLAG_PRESET,
	.timing_res = DP_TIMING_1080P,
};

__maybe_unused struct dptx_detail_timing_s DP_std_2K60 = {
	.h_a = 2560,
	.v_a = 1440,
	.h_b = 80,
	.v_b = 41,
	.pclk = 234590000,
	.h_pw = 32,
	.h_fp = 8,
	.v_pw = 8,
	.v_fp = 27,
	.timing_ctrl = 0x0,
	.v_size = 90,
	.h_size = 160,

	.flag = TIMING_FLAG_VALID | TIMING_FLAG_PRESET,
	.timing_res = DP_TIMING_2K,
};

__maybe_unused struct dptx_detail_timing_s DP_std_4K60 = {
	.h_a = 3840,
	.v_a = 2160,
	.h_b = 560,
	.v_b = 90,
	.pclk = 594000000,
	.h_pw = 88,
	.h_fp = 176,
	.v_pw = 10,
	.v_fp = 8,
	.timing_ctrl = 0x0,
	.v_size = 90,
	.h_size = 160,

	.flag = TIMING_FLAG_VALID | TIMING_FLAG_PRESET,
	.timing_res = DP_TIMING_4K,
};

unsigned char DP_fr_step[DP_FR_STEP_NUM] = {144, 120, 90, 75, 60, 48, 30};

struct dptx_detail_timing_s *timing_list[LCD_MAX_DRV][DP_MAX_TIMING];

enum DP_TIMING_RES DP_timing_detect(int ha, int va)
{
	if (ha == 3840 && va == 2160)
		return DP_TIMING_4K;
	if (ha == 2560 && va == 1440)
		return DP_TIMING_2K;
	if (ha == 1920 && va == 1080)
		return DP_TIMING_1080P;

	if (ha >= 3840 && va >= 2160)
		return DP_TIMING_4K_UPPER;
	if ((ha >= 2560 && ha <= 3840) && (va >= 1440 && va <= 2160))
		return DP_TIMING_4K_2K;
	if ((ha >= 1920 && ha <= 2560) && (va >= 1080 && va <= 1440))
		return DP_TIMING_2K_1080p;
	if (ha <= 1920 && va <= 1080)
		return DP_TIMING_1080P_LOWER;

	return DP_TIMING_SPEC;
}

/* @note: using DP_cfg->link_rate to check
 * return:
 *  [0] : v/h failed
 *  [1] : framerate failed
 *  [2] : band width failed
 */
unsigned char dptx_check_timing(struct aml_lcd_drv_s *pdrv, struct dptx_detail_timing_s *timing)
{
	struct DP_dev_support_s *source_sp;
	struct edp_config_s *DP_cfg = &pdrv->config.control.edp_cfg;
	int ret_code = 0;
	unsigned int frame_rate;

	if (!pdrv || !timing)
		return 0xff;

	if (!(timing->pclk && timing->h_a && timing->v_a)) { //invalid timing
		ret_code = 1 << 7;
		return ret_code;
	}

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
	default:
		source_sp = &source_support_T7;
		break;
	}

	if (timing->h_a > source_sp->h_active || timing->v_a > source_sp->v_active)
		ret_code |= 1 << 0;

	frame_rate = (timing->h_a + timing->h_b) * (timing->v_a + timing->v_b);
	frame_rate = timing->pclk / frame_rate;
	if (frame_rate > source_sp->frame_rate)
		ret_code |= 1 << 1;

	if (dptx_band_width_check(DP_cfg->link_rate, DP_cfg->lane_count, timing->pclk, 24))
		ret_code |= 1 << 2;

	if (ret_code == 0)
		timing->flag |= TIMING_FLAG_SUPPORT;

	return ret_code;
}

void dptx_timing_update(struct aml_lcd_drv_s *pdrv, struct dptx_detail_timing_s *timing)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int sync_duration;

	pconf->basic.h_active = timing->h_a;
	pconf->basic.v_active = timing->v_a;
	pconf->basic.h_period = timing->h_a + timing->h_b;
	pconf->basic.v_period = timing->v_a + timing->v_b;

	pconf->timing.lcd_clk = timing->pclk;
	sync_duration = timing->pclk / pconf->basic.h_period;
	sync_duration = (sync_duration * 100) / pconf->basic.v_period;
	pconf->timing.frame_rate = sync_duration / 100;
	pconf->timing.sync_duration_num = sync_duration;
	pconf->timing.sync_duration_den = 100;

	pconf->timing.hsync_width = timing->h_pw;
	pconf->timing.hsync_bp = timing->h_b - timing->h_fp - timing->h_pw;
	pconf->timing.hsync_pol = (timing->timing_ctrl >> 1) & 0x1;
	pconf->timing.vsync_width = timing->v_pw;
	pconf->timing.vsync_bp = timing->v_b - timing->v_fp - timing->v_pw;
	pconf->timing.vsync_pol = (timing->timing_ctrl >> 2) & 0x1;

	pconf->basic.screen_width = timing->h_size;
	pconf->basic.screen_height = timing->v_size;

	lcd_timing_init_config(pdrv);
	lcd_tablet_config_update(pdrv);
	lcd_tablet_config_post_update(pdrv);

	// lcd_timing_init_config(pconf);
	// lcd_tablet_config_update(pdrv);
	// lcd_clk_generate_parameter(pdrv);

	lcd_set_clk(pdrv);
	lcd_set_venc_timing(pdrv);
}

static int add_timing_list(struct aml_lcd_drv_s *pdrv, struct dptx_detail_timing_s *dt)
{
	int idx;
	unsigned long fr100;

	for (idx = 0; idx < DP_MAX_TIMING; idx++) {
		if (!(timing_list[pdrv->index][idx]->flag & TIMING_FLAG_VALID))
			goto search_done;
		if (timing_list[pdrv->index][idx]->h_a  == dt->h_a &&
			timing_list[pdrv->index][idx]->v_a  == dt->v_a &&
			timing_list[pdrv->index][idx]->pclk == dt->pclk)
			return 0;
	}
search_done:
	memcpy(timing_list[pdrv->index][idx], dt, sizeof(struct dptx_detail_timing_s));
	timing_list[pdrv->index][idx]->timing_res = DP_timing_detect(dt->h_a, dt->v_a);
	timing_list[pdrv->index][idx]->flag = dt->flag | TIMING_FLAG_VALID; //set flag

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		fr100 = (dt->pclk * 100) / ((dt->v_a + dt->v_b) * (dt->h_a + dt->h_b));
		LCDPR(" - add: %4u * %4u @ %3lu.%2luHz (h_blank:%3u, v_blank:%3u, pclk:%10lu)\n",
			timing_list[pdrv->index][idx]->h_a, timing_list[pdrv->index][idx]->v_a,
			fr100 / 100, fr100 % 100, timing_list[pdrv->index][idx]->h_b,
			timing_list[pdrv->index][idx]->v_b, timing_list[pdrv->index][idx]->pclk);
	}
	return 1;
}

static void timing_list_reorder(struct aml_lcd_drv_s *pdrv)
{
	unsigned int x, y, fr100_a, fr100_b, s_size, pidx;
	struct dptx_detail_timing_s temp_timing;

	pidx = pdrv->index;
	s_size = sizeof(struct dptx_detail_timing_s);

	for (x = 0; x < DP_MAX_TIMING; x++)
		for (y = 0; y + 1 < DP_MAX_TIMING - x; y++) {
			fr100_a = (timing_list[pidx][y]->pclk * 100) /
				((timing_list[pidx][y]->h_a + timing_list[pidx][y]->h_b) *
				((timing_list[pidx][y]->v_a + timing_list[pidx][y]->v_b)));

			fr100_b = (timing_list[pidx][y + 1]->pclk * 100) /
				((timing_list[pidx][y + 1]->h_a + timing_list[pidx][y + 1]->h_b) *
				((timing_list[pidx][y + 1]->v_a + timing_list[pidx][y + 1]->v_b)));

			if (fr100_a < fr100_b) {
				memcpy(&temp_timing, timing_list[pidx][y], s_size);
				memcpy(timing_list[pidx][y], timing_list[pidx][y + 1], s_size);
				memcpy(timing_list[pidx][y + 1], &temp_timing, s_size);
			}
		}

	for (x = 0; x < DP_MAX_TIMING; x++)
		for (y = 0; y + 1 < DP_MAX_TIMING - x; y++)
			if (timing_list[pidx][y]->v_a < timing_list[pidx][y + 1]->v_a) {
				memcpy(&temp_timing, timing_list[pidx][y], s_size);
				memcpy(timing_list[pidx][y], timing_list[pidx][y + 1], s_size);
				memcpy(timing_list[pidx][y + 1], &temp_timing, s_size);
			}

	for (x = 0; x < DP_MAX_TIMING; x++)
		for (y = 0; y + 1 < DP_MAX_TIMING - x; y++)
			if (timing_list[pidx][y]->h_a < timing_list[pidx][y + 1]->h_a) {
				memcpy(&temp_timing, timing_list[pidx][y], s_size);
				memcpy(timing_list[pidx][y], timing_list[pidx][y + 1], s_size);
				memcpy(timing_list[pidx][y + 1], &temp_timing, s_size);
			}
}

/* dptx_manage_timing: gen timing
 * @brief:
 * 1. search all detail_timing, copy to timing_list
 *     if no detail_timing, save 640p safemode
 * 2. check lower resolution, apply 4k/2k/1080p@60 timing
 * 3. if range_limit != NULL, expand by DP_fr_step
 * 4. reorder, check available
 */
void dptx_manage_timing(struct aml_lcd_drv_s *pdrv, struct dptx_EDID_s *EDID_p)
{
	unsigned char timing_cnt, b_idx, dt_idx, fr_idx, pidx;
	unsigned char range_limit_valid = 0, detail_timing_valid = 0;
	enum DP_TIMING_RES max_timing = DP_TIMING_SPEC;
	struct dptx_detail_timing_s tmp_dt;

	pidx = pdrv->index;

	for (dt_idx = 0; dt_idx < DP_MAX_TIMING; dt_idx++) {
		if (timing_list[pidx][dt_idx])
			continue;
		timing_list[pidx][dt_idx] =
			kzalloc(sizeof(struct dptx_detail_timing_s), GFP_KERNEL);
		if (!timing_list[pidx][dt_idx])
			LCDERR("timing_list[%d][%d] kcalloc failed\n", pidx, dt_idx);
	}

	for (b_idx = 0; b_idx < DP_EDID_BLOCK_NUM; b_idx++) {
		range_limit_valid += EDID_p->block_identity[b_idx] == BLOCK_ID_RANGE_TIMING;
		detail_timing_valid += EDID_p->block_identity[b_idx] == BLOCK_ID_DETAIL_TIMING;
	}
	if (!detail_timing_valid)
		goto collect_timing_done;

	timing_cnt = 0;
	for (b_idx = 0; b_idx < DP_EDID_BLOCK_NUM; b_idx++)
		if (EDID_p->block_identity[b_idx] == BLOCK_ID_DETAIL_TIMING) {
			EDID_p->detail_timing[b_idx].flag = TIMING_FLAG_DETAIL | TIMING_FLAG_VALID;
			timing_cnt += add_timing_list(pdrv, &EDID_p->detail_timing[b_idx]);
		}
	LCDPR("[%d]: Collect %d form detail timing\n", pidx, timing_cnt);

	timing_cnt = 0;
	for (dt_idx = 0; dt_idx < DP_MAX_TIMING; dt_idx++)
		max_timing = timing_list[pidx][dt_idx]->timing_res > max_timing ?
			timing_list[pidx][dt_idx]->timing_res : max_timing;
	if (max_timing > DP_TIMING_4K)
		timing_cnt += add_timing_list(pdrv, &DP_std_4K60);
	if (max_timing > DP_TIMING_2K)
		timing_cnt += add_timing_list(pdrv, &DP_std_2K60);
	if (max_timing > DP_TIMING_1080P)
		timing_cnt += add_timing_list(pdrv, &DP_std_1080P60);
	LCDPR("[%d]: Collect %d form preset timing\n", pdrv->index, timing_cnt);

	timing_cnt = 0;
	if (!range_limit_valid)
		goto collect_timing_done;
	for (dt_idx = 0; dt_idx < DP_MAX_TIMING; dt_idx++) {
		if (!(timing_list[pidx][dt_idx]->flag & TIMING_FLAG_VALID))
			continue;
		if (timing_list[pidx][dt_idx]->flag & TIMING_FLAG_FR_STEP)
			continue;

		memcpy(&tmp_dt, timing_list[pidx][dt_idx], sizeof(struct dptx_detail_timing_s));
		tmp_dt.flag |= TIMING_FLAG_FR_STEP | TIMING_FLAG_VALID;

		for (fr_idx = 0; fr_idx < DP_FR_STEP_NUM; fr_idx++) {
			if (DP_fr_step[fr_idx] < EDID_p->range_limit.min_vfreq ||
				DP_fr_step[fr_idx] > EDID_p->range_limit.max_v_freq)
				continue;

			tmp_dt.pclk = DP_fr_step[fr_idx] * (tmp_dt.h_a + tmp_dt.h_b) *
				(tmp_dt.v_a + tmp_dt.v_b);
			if (tmp_dt.pclk > EDID_p->range_limit.max_pclk)
				continue;

			timing_cnt += add_timing_list(pdrv, &tmp_dt);
		}
	}
	LCDPR("[%d]: Collect %d from frame_rate step\n", pidx, timing_cnt);
collect_timing_done:

	add_timing_list(pdrv, &DP_safemode_640x480);

	timing_list_reorder(pdrv);

	for (dt_idx = 0; dt_idx < DP_MAX_TIMING; dt_idx++) {
		if (!(timing_list[pidx][dt_idx]->flag & TIMING_FLAG_VALID))
			continue;

		dptx_check_timing(pdrv, timing_list[pidx][dt_idx]);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		dptx_print_timing(pdrv, 0xff);
}

void dptx_clear_timing(struct aml_lcd_drv_s *pdrv)
{
	unsigned char dt_idx;

	for (dt_idx = 0; dt_idx < DP_MAX_TIMING; dt_idx++) {
		kfree(timing_list[pdrv->index][dt_idx]);
		timing_list[pdrv->index][dt_idx] = NULL;
	}
}

struct dptx_detail_timing_s *dptx_get_timing(struct aml_lcd_drv_s *pdrv, unsigned char th)
{
	if (th >= DP_MAX_TIMING)
		return NULL;

	if (timing_list[pdrv->index][th]->flag & TIMING_FLAG_VALID)
		return timing_list[pdrv->index][th];

	LCDERR("[%d]: %s %dth not available\n", pdrv->index, __func__, th);
	return NULL;
}

struct dptx_detail_timing_s *dptx_get_optimum_timing(struct aml_lcd_drv_s *pdrv)
{
	unsigned char i = 0;
	struct dptx_detail_timing_s *tm;

	while (i < DP_MAX_TIMING) {
		tm = dptx_get_timing(pdrv, i);
		i++;
		if (!tm)
			continue;
		if (tm->flag & TIMING_FLAG_VALID && tm->flag & TIMING_FLAG_SUPPORT)
			return tm;
	}
	LCDERR("[%d]: %s no timing available", pdrv->index, __func__);
	return NULL;
}

/* @param:
 * print_flag: 0~254: X-th supported, 0xff: all valid
 */
void dptx_print_timing(struct aml_lcd_drv_s *pdrv, unsigned char print_flag)
{
	unsigned char idx;
	unsigned long fr100;

	LCDPR("[%d]: DPtx Timing Table:\n", pdrv->index);

	for (idx = 0; idx < DP_MAX_TIMING; idx++) {
		if (!(timing_list[pdrv->index][idx]->flag & TIMING_FLAG_VALID))
			continue;
		if (!(print_flag == 0xff || print_flag == idx))
			continue;

		fr100 = (timing_list[pdrv->index][idx]->pclk * 100) /
			((timing_list[pdrv->index][idx]->h_a + timing_list[pdrv->index][idx]->h_b) *
			((timing_list[pdrv->index][idx]->v_a + timing_list[pdrv->index][idx]->v_b))
			);

		pr_info(" %s %2d: %4d * %4d @ %3ld.%2ldhz, pclk: %10luHz, %s timing, fr_step:%d\n",
			(timing_list[pdrv->index][idx]->flag & TIMING_FLAG_SUPPORT) ? "*" : "-",
			idx,
			timing_list[pdrv->index][idx]->h_a, timing_list[pdrv->index][idx]->v_a,
			fr100 / 100, fr100 % 100, timing_list[pdrv->index][idx]->pclk,
			(timing_list[pdrv->index][idx]->flag & TIMING_FLAG_DETAIL) ?
				"detail" : "preset",
			(timing_list[pdrv->index][idx]->flag & TIMING_FLAG_FR_STEP) && 1);
	}
}
