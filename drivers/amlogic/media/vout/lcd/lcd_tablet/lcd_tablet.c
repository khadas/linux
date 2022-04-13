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
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/clk.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_tablet.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "mipi_dsi_util.h"

/* **************************************************
 * vout server api
 * **************************************************
 */
static struct vinfo_s *lcd_get_current_info(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return NULL;
	return &pdrv->vinfo;
}

static int lcd_check_same_vmodeattr(char *mode, void *data)
{
	return 1;
}

static int lcd_vmode_is_supported(enum vmode_e mode, void *data)
{
	mode &= VMODE_MODE_BIT_MASK;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s vmode = %d\n", __func__, mode);

	if (mode == VMODE_LCD)
		return true;
	return false;
}

static enum vmode_e lcd_validate_vmode(char *mode, unsigned int frac,
				       void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv || !mode)
		return VMODE_MAX;
	if (frac)
		return VMODE_MAX;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: display_mode=%s, drv_mode=%s\n",
		      pdrv->index, __func__, mode, pdrv->vinfo.name);
	}
	if ((strcmp(mode, pdrv->vinfo.name)) == 0)
		return VMODE_LCD;

	return VMODE_MAX;
}

static int lcd_set_current_vmode(enum vmode_e mode, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	int ret = 0;

	if (!pdrv)
		return -1;

	mutex_lock(&lcd_power_mutex);

	pdrv->vrr_dev = kzalloc(sizeof(*pdrv->vrr_dev), GFP_KERNEL);
	if (pdrv->vrr_dev) {
		sprintf(pdrv->vrr_dev->name, "lcd%d_dev", pdrv->index);
		pdrv->vrr_dev->output_src = VRR_OUTPUT_ENCL;
		lcd_vrr_dev_update(pdrv);
		aml_vrr_register_device(pdrv->vrr_dev, pdrv->index);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: drv_mode=%s\n",
		      pdrv->index, __func__, pdrv->vinfo.name);
	}
	if (VMODE_LCD == (mode & VMODE_MODE_BIT_MASK)) {
		if (mode & VMODE_INIT_BIT_MASK) {
			lcd_clk_gate_switch(pdrv, 1);
		} else {
			aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, (void *)pdrv);
			lcd_if_enable_retry(pdrv);
		}
	} else {
		ret = -EINVAL;
	}

	pdrv->status |= LCD_STATUS_VMODE_ACTIVE;
	mutex_unlock(&lcd_power_mutex);

	return ret;
}

static int lcd_vout_disable(enum vmode_e cur_vmod, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	mutex_lock(&lcd_power_mutex);
	if (pdrv->vrr_dev) {
		aml_vrr_unregister_device(pdrv->index);
		kfree(pdrv->vrr_dev);
		pdrv->vrr_dev = NULL;
	}

	pdrv->status &= ~LCD_STATUS_VMODE_ACTIVE;
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, (void *)pdrv);
	LCDPR("%s finished\n", __func__);
	mutex_unlock(&lcd_power_mutex);

	return 0;
}

static int lcd_vout_set_state(int index, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	pdrv->vout_state |= (1 << index);
	pdrv->viu_sel = index;

	return 0;
}

static int lcd_vout_clr_state(int index, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	pdrv->vout_state &= ~(1 << index);
	if (pdrv->viu_sel == index)
		pdrv->viu_sel = LCD_VIU_SEL_NONE;

	return 0;
}

static int lcd_vout_get_state(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	return pdrv->vout_state;
}

static int lcd_vout_get_disp_cap(char *buf, void *data)
{
	int ret = 0;

	ret += sprintf(buf + ret, "%s\n", "panel");
	return ret;
}

struct lcd_vframe_match_s {
	int frame_rate; /* *100 */
	unsigned int duration_num;
	unsigned int duration_den;
	unsigned int frac;
};

static struct lcd_vframe_match_s lcd_vframe_match_table_1[] = {
	{5000, 50, 1, 0},
	{6000, 60, 1, 0},
	{5994, 5994, 100, 1}
};

static struct lcd_vframe_match_s lcd_vframe_match_table_2[] = {
	{5000, 50, 1, 0},
	{6000, 60, 1, 0},
	{4800, 48, 1, 0},
	{5994, 5994, 100, 1}
};

static int lcd_framerate_automation_set_mode(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv)
		return -1;

	LCDPR("%s\n", __func__);
	lcd_vout_notify_mode_change_pre(pdrv);

	/* update clk & timing config */
	lcd_tablet_config_update(pdrv);
	/* update interface timing if needed, current no need */
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.lcd_clk);
#endif

	/* change clk parameter */
	lcd_clk_change(pdrv);
	lcd_tablet_config_post_update(pdrv);
	lcd_venc_change(pdrv);

	lcd_vout_notify_mode_change(pdrv);

	return 0;
}

static int lcd_set_vframe_rate_hint(int duration, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct vinfo_s *info;
	unsigned int frame_rate = 6000;
	unsigned int duration_num = 60, duration_den = 1, frac = 0;
	struct lcd_vframe_match_s *vtable = lcd_vframe_match_table_1;
	int i, n, find = 0;

	if (!pdrv)
		return -1;
	if (pdrv->probe_done == 0)
		return -1;

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
		LCDPR("%s: lcd is disabled, exit\n", __func__);
		return -1;
	}

	if (lcd_fr_is_fixed(pdrv)) {
		LCDPR("%s: fixed timing, exit\n", __func__);
		return -1;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("fr_auto_policy = %d\n", pdrv->fr_auto_policy);

	info = &pdrv->vinfo;
	switch (pdrv->fr_auto_policy) {
	case 1:
		vtable = lcd_vframe_match_table_1;
		n = ARRAY_SIZE(lcd_vframe_match_table_1);
		break;
	case 2:
		vtable = lcd_vframe_match_table_2;
		n = ARRAY_SIZE(lcd_vframe_match_table_2);
		break;
	default:
		LCDPR("%s: fr_auto_policy = %d, disabled\n",
		      __func__, pdrv->fr_auto_policy);
		return 0;
	}

	if (duration == 0) { /* end hint */
		LCDPR("%s: return mode = %s, policy = %d\n", __func__,
			info->name, pdrv->fr_auto_policy);

		pdrv->fr_duration = 0;
		if (pdrv->fr_mode == 0) {
			LCDPR("%s: fr_mode is invalid, exit\n", __func__);
			return 0;
		}

		/* update frame rate */
		pdrv->config.timing.frame_rate = pdrv->cur_duration.frame_rate;
		pdrv->config.timing.sync_duration_num = pdrv->cur_duration.duration_num;
		pdrv->config.timing.sync_duration_den = pdrv->cur_duration.duration_den;
		pdrv->config.timing.frac = pdrv->cur_duration.frac;
		pdrv->fr_mode = 0;
	} else {
		for (i = 0; i < n; i++) {
			if (duration == vtable[i].frame_rate) {
				frame_rate = vtable[i].frame_rate;
				duration_num = vtable[i].duration_num;
				duration_den = vtable[i].duration_den;
				frac = vtable[i].frac;
				find = 1;
			}
		}
		if (find == 0) {
			LCDERR("%s: can't support duration %d\n, exit\n",
			       __func__, duration);
			return -1;
		}

		LCDPR("%s: policy = %d, duration = %d, frame_rate = %d\n",
		      __func__, pdrv->fr_auto_policy,
		      duration, frame_rate);

		pdrv->fr_duration = duration;
		/* if the sync_duration is same as current */
		if (duration_num == pdrv->config.timing.sync_duration_num &&
		    duration_den == pdrv->config.timing.sync_duration_den) {
			LCDPR("%s: sync_duration is the same, exit\n",
			      __func__);
			return 0;
		}

		/* update frame rate */
		pdrv->config.timing.frame_rate = frame_rate;
		pdrv->config.timing.sync_duration_num = duration_num;
		pdrv->config.timing.sync_duration_den = duration_den;
		pdrv->config.timing.frac = frac;
		pdrv->fr_mode = duration;
	}

	lcd_framerate_automation_set_mode(pdrv);

	return 0;
}

static int lcd_get_vframe_rate_hint(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return 0;

	return pdrv->fr_duration;
}

static void lcd_vout_debug_test(unsigned int num, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return;

	lcd_debug_test(pdrv, num);
}

static int lcd_suspend(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	mutex_lock(&lcd_power_mutex);
	pdrv->resume_flag &= ~LCD_RESUME_ENABLE;
	aml_lcd_notifier_call_chain(LCD_EVENT_DISABLE, (void *)pdrv);
	LCDPR("[%d]: early_suspend finished\n", pdrv->index);
	mutex_unlock(&lcd_power_mutex);
	return 0;
}

static int lcd_resume(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	if ((pdrv->status & LCD_STATUS_VMODE_ACTIVE) == 0)
		return 0;

	if (pdrv->resume_flag & LCD_RESUME_ENABLE)
		return 0;

	if (pdrv->resume_type) {
		lcd_queue_work(&pdrv->late_resume_work);
	} else {
		mutex_lock(&lcd_power_mutex);
		LCDPR("[%d]: directly lcd late_resume\n", pdrv->index);
		pdrv->resume_flag |= LCD_RESUME_ENABLE;
		aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)pdrv);
		lcd_if_enable_retry(pdrv);
		LCDPR("[%d]: late_resume finished\n", pdrv->index);
		mutex_unlock(&lcd_power_mutex);
	}

	return 0;
}

static void lcd_tablet_vinfo_update(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf;
	unsigned int temp;

	if (!pdrv)
		return;

	pconf = &pdrv->config;

	/* store current duration */
	pdrv->cur_duration.frame_rate = pconf->timing.frame_rate;
	pdrv->cur_duration.duration_num = pconf->timing.sync_duration_num;
	pdrv->cur_duration.duration_den = pconf->timing.sync_duration_den;
	pdrv->cur_duration.frac = pconf->timing.frac;

	pdrv->vinfo.width = pconf->basic.h_active;
	pdrv->vinfo.height = pconf->basic.v_active;
	pdrv->vinfo.field_height = pconf->basic.v_active;
	pdrv->vinfo.aspect_ratio_num = pconf->basic.screen_width;
	pdrv->vinfo.aspect_ratio_den = pconf->basic.screen_height;
	pdrv->vinfo.screen_real_width = pconf->basic.screen_width;
	pdrv->vinfo.screen_real_height = pconf->basic.screen_height;
	pdrv->vinfo.sync_duration_num = pconf->timing.sync_duration_num;
	pdrv->vinfo.sync_duration_den = pconf->timing.sync_duration_den;
	pdrv->vinfo.frac = pconf->timing.frac;
	pdrv->vinfo.std_duration = pconf->timing.frame_rate;
	pdrv->vinfo.vfreq_max = pconf->basic.frame_rate_max;
	pdrv->vinfo.vfreq_min = pconf->basic.frame_rate_min;
	pdrv->vinfo.video_clk = pconf->timing.lcd_clk;
	pdrv->vinfo.htotal = pconf->basic.h_period;
	pdrv->vinfo.vtotal = pconf->basic.v_period;
	pdrv->vinfo.hsw = pconf->timing.hsync_width;
	pdrv->vinfo.hbp = pconf->timing.hsync_bp;
	temp = pconf->basic.h_period - pconf->basic.h_active -
		pconf->timing.hsync_width - pconf->timing.hsync_bp;
	pdrv->vinfo.hfp = temp;
	pdrv->vinfo.vsw = pconf->timing.vsync_width;
	pdrv->vinfo.vbp = pconf->timing.vsync_bp;
	temp = pconf->basic.v_period - pconf->basic.v_active -
		pconf->timing.vsync_width - pconf->timing.vsync_bp;
	pdrv->vinfo.vfp = temp;
	switch (pconf->timing.fr_adjust_type) {
	case 0:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_CLK;
		break;
	case 1:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_HTOTAL;
		break;
	case 2:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_VTOTAL;
		break;
	case 3:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_COMBO;
		break;
	case 4:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_HDMI;
		break;
	case 5:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_FREERUN;
		break;
	default:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_NONE;
		break;
	}

	lcd_optical_vinfo_update(pdrv);
}

static void lcd_tablet_vinfo_update_default(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;

	memset(pdrv->output_name, 0, sizeof(pdrv->output_name));
	if (pdrv->index == 0) {
		snprintf(pdrv->output_name, sizeof(pdrv->output_name), "panel");
	} else {
		snprintf(pdrv->output_name, sizeof(pdrv->output_name),
			 "panel%d", pdrv->index);
	}
	pdrv->vinfo.name = pdrv->output_name;
	pdrv->vinfo.mode = VMODE_LCD;
	pdrv->vinfo.width =  pconf->basic.h_active;
	pdrv->vinfo.height = pconf->basic.v_active;
	pdrv->vinfo.field_height = pconf->basic.v_active;
	pdrv->vinfo.aspect_ratio_num =  pconf->basic.h_active;
	pdrv->vinfo.aspect_ratio_den = pconf->basic.v_active;
	pdrv->vinfo.screen_real_width =  pconf->basic.h_active;
	pdrv->vinfo.screen_real_height = pconf->basic.v_active;
	pdrv->vinfo.sync_duration_num = 60;
	pdrv->vinfo.sync_duration_den = 1;
	pdrv->vinfo.frac = 0;
	pdrv->vinfo.std_duration = 60;
	pdrv->vinfo.vfreq_max = pdrv->vinfo.std_duration;
	pdrv->vinfo.vfreq_min = pdrv->vinfo.std_duration;
	pdrv->vinfo.video_clk = 0;
	pdrv->vinfo.htotal = pconf->basic.h_period;
	pdrv->vinfo.vtotal = pconf->basic.v_period;
	pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_NONE;
}

void lcd_tablet_vout_server_init(struct aml_lcd_drv_s *pdrv)
{
	struct vout_server_s *vserver;
	int i;

	lcd_tablet_vinfo_update_default(pdrv);
	for (i = 0; i < 3; i++) {
		vserver = kzalloc(sizeof(*vserver), GFP_KERNEL);
		if (!vserver)
			return;
		vserver->name = kzalloc(32, GFP_KERNEL);
		if (!vserver->name) {
			kfree(vserver);
			return;
		}
		pdrv->vout_server[i] = vserver;

		sprintf(vserver->name, "lcd%d_vout%d_server", pdrv->index, i);
		vserver->op.get_vinfo = lcd_get_current_info;
		vserver->op.set_vmode = lcd_set_current_vmode;
		vserver->op.validate_vmode = lcd_validate_vmode;
		vserver->op.check_same_vmodeattr = lcd_check_same_vmodeattr;
		vserver->op.vmode_is_supported = lcd_vmode_is_supported;
		vserver->op.disable = lcd_vout_disable;
		vserver->op.set_state = lcd_vout_set_state;
		vserver->op.clr_state = lcd_vout_clr_state;
		vserver->op.get_state = lcd_vout_get_state;
		vserver->op.get_disp_cap = lcd_vout_get_disp_cap;
		vserver->op.set_vframe_rate_hint = lcd_set_vframe_rate_hint;
		vserver->op.get_vframe_rate_hint = lcd_get_vframe_rate_hint;
		vserver->op.set_bist = lcd_vout_debug_test;
		vserver->op.vout_suspend = lcd_suspend;
		vserver->op.vout_resume = lcd_resume;
		vserver->data = (void *)pdrv;
	}

	vout_register_server(pdrv->vout_server[0]);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_register_server(pdrv->vout_server[1]);
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	vout3_register_server(pdrv->vout_server[2]);
#endif
}

void lcd_tablet_vout_server_remove(struct aml_lcd_drv_s *pdrv)
{
	vout_unregister_server(pdrv->vout_server[0]);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_unregister_server(pdrv->vout_server[1]);
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	vout3_unregister_server(pdrv->vout_server[2]);
#endif
}

static void lcd_config_init(struct aml_lcd_drv_s *pdrv)
{
	lcd_basic_timing_range_init(pdrv);
	lcd_timing_init_config(pdrv);

	lcd_tablet_vinfo_update(pdrv);

	lcd_tablet_config_update(pdrv);
	lcd_clk_generate_parameter(pdrv);

	lcd_clk_ss_config_update(pdrv);

	lcd_tablet_config_post_update(pdrv);
}

/* **************************************************
 * lcd notify
 * **************************************************
 */
/* sync_duration is real_value * 100 */
static void lcd_frame_rate_adjust(struct aml_lcd_drv_s *pdrv, int duration)
{
	LCDPR("%s: sync_duration=%d\n", __func__, duration);

	lcd_vout_notify_mode_change_pre(pdrv);

	/* update frame rate */
	pdrv->config.timing.frame_rate = duration / 100;
	pdrv->config.timing.sync_duration_num = duration;
	pdrv->config.timing.sync_duration_den = 100;

	/* update interface timing */
	lcd_tablet_config_update(pdrv);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.lcd_clk);
#endif

	/* change clk parameter */
	lcd_clk_change(pdrv);
	lcd_tablet_config_post_update(pdrv);
	lcd_venc_change(pdrv);

	lcd_vout_notify_mode_change(pdrv);
}

/* **************************************************
 * lcd tablet
 * **************************************************
 */
int lcd_mode_tablet_init(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv)
		return -1;

	lcd_config_init(pdrv);

	pdrv->driver_init_pre = lcd_tablet_driver_init_pre;
	pdrv->driver_disable_post = lcd_tablet_driver_disable_post;
	pdrv->driver_init = lcd_tablet_driver_init;
	pdrv->driver_disable = lcd_tablet_driver_disable;
	pdrv->fr_adjust = lcd_frame_rate_adjust;

	return 0;
}

int lcd_mode_tablet_remove(struct aml_lcd_drv_s *pdrv)
{
	return 0;
}

