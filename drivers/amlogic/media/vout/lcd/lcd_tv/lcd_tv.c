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
#include <linux/amlogic/media/vrr/vrr.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_tv.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

static int lcd_init_on_flag;
static struct vrr_device_s *lcd_vrr_dev;

/* ************************************************** *
 * lcd mode function
 * **************************************************
 */
#define ACTIVE_FRAME_RATE_CNT     3
#define LCD_STD_FRAME_RATE_MAX    5
static unsigned int lcd_std_frame_rate[][3] = {
	{60, 60,    1},
	{59, 60000, 1001},
	{50, 50,    1},
	{48, 48,    1},
	{47, 48000, 1001},
	{60, 60,    1}
};

struct lcd_vmode_info_s {
	char *name;
	enum vmode_e mode;
	unsigned int width;
	unsigned int height;
	unsigned int frame_rate;
	unsigned int frac;
};

enum lcd_vmode_e {
	LCD_VMODE_600P = 0,
	LCD_VMODE_768P,
	LCD_VMODE_1080P,
	LCD_VMODE_2160P,
	LCD_VMODE_MAX,
};

static struct lcd_vmode_info_s lcd_vmode_info[] = {
	{
		.name              = "600p",
		.mode              = VMODE_LCD,
		.width             = 1024,
		.height            = 600,
		.frame_rate        = 60,
		.frac              = 0,
	},
	{
		.name              = "768p",
		.mode              = VMODE_LCD,
		.width             = 1366,
		.height            = 768,
		.frame_rate        = 60,
		.frac              = 0,
	},
	{
		.name              = "1080p",
		.mode              = VMODE_LCD,
		.width             = 1920,
		.height            = 1080,
		.frame_rate        = 60,
		.frac              = 0,
	},
	{
		.name              = "2160p",
		.mode              = VMODE_LCD,
		.width             = 3840,
		.height            = 2160,
		.frame_rate        = 60,
		.frac              = 0,
	},
	{
		.name              = "invalid",
		.mode              = VMODE_INIT_NULL,
		.width             = 1920,
		.height            = 1080,
		.frame_rate        = 60,
		.frac              = 0,
	},
};

static int lcd_vmode_is_mached(struct aml_lcd_drv_s *pdrv, int index)
{
	if (!pdrv)
		return -1;

	if (pdrv->config.basic.h_active == lcd_vmode_info[index].width &&
	    pdrv->config.basic.v_active == lcd_vmode_info[index].height)
		return 0;
	else
		return -1;
}

static int lcd_get_vmode(struct aml_lcd_drv_s *pdrv, enum vmode_e mode)
{
	int lcd_vmode = LCD_VMODE_MAX;
	int i, count = ARRAY_SIZE(lcd_vmode_info) - 1;
	int ret;

	for (i = 0; i < count; i++) {
		if (mode == lcd_vmode_info[i].mode) {
			ret = lcd_vmode_is_mached(pdrv, i);
			if (ret == 0) {
				lcd_vmode = i;
				break;
			}
		}
	}
	return lcd_vmode;
}

static int lcd_outputmode_to_vmode(const char *mode)
{
	int lcd_vmode = LCD_VMODE_MAX;
	int i, count = ARRAY_SIZE(lcd_vmode_info) - 1;
	char temp[30], *p;
	int n;

	p = strchr(mode, 'p');
	if (!p)
		return LCD_VMODE_MAX;
	n = p - mode + 1;
	strncpy(temp, mode, n);
	temp[n] = '\0';
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("outputmode=%s, lcd_vmode=%s\n", mode, temp);

	for (i = 0; i < count; i++) {
		if (strcmp(temp, lcd_vmode_info[i].name) == 0) {
			lcd_vmode = i;
			break;
		}
	}
	return lcd_vmode;
}

static int lcd_outputmode_to_frame_rate(const char *mode)
{
	int frame_rate = 0;
	char temp[30], *p;
	int n, i, ret = 0;

	p = strchr(mode, 'p');
	if (!p)
		return 0;
	n = p - mode + 1;
	strncpy(temp, mode + n, (strlen(mode) - n));
	p = strchr(temp, 'h');
	if (!p)
		return 0;
	*p = '\0';
	ret = kstrtoint(temp, 10, &n);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("outputmode=%s, frame_rate=%d\n", mode, n);

	for (i = 0; i < LCD_STD_FRAME_RATE_MAX; i++) {
		if (n == lcd_std_frame_rate[i][0]) {
			frame_rate = n;
			break;
		}
	}
	return frame_rate;
}

static unsigned int lcd_std_frame_rate_index(unsigned int frame_rate)
{
	unsigned int i;

	for (i = 0; i < LCD_STD_FRAME_RATE_MAX; i++) {
		if (frame_rate == lcd_std_frame_rate[i][0])
			return i;
	}

	LCDERR("%s: invalid frame_rate: %d\n", __func__, frame_rate);
	return LCD_STD_FRAME_RATE_MAX;
}

static void lcd_vmode_vinfo_update(struct aml_lcd_drv_s *pdrv,
				   enum vmode_e mode)
{
	struct lcd_vmode_info_s *info;
	struct lcd_config_s *pconf;
	unsigned int index;

	if (!pdrv)
		return;

	pconf = &pdrv->config;
	pdrv->output_vmode = lcd_get_vmode(pdrv, mode);
	info = &lcd_vmode_info[pdrv->output_vmode];
	memset(pdrv->output_name, 0, sizeof(pdrv->output_name));
	sprintf(pdrv->output_name, "%s%dhz", info->name, info->frame_rate);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s vmode = %d, lcd_vmode = %d, outputmode = %s\n",
		      pdrv->index, __func__, mode, pdrv->output_vmode,
		      pdrv->output_name);
	}

	/* store standard duration */
	if (lcd_fr_is_fixed(pdrv)) {
		pdrv->std_duration.duration_num =
			((pdrv->config.timing.lcd_clk /
			pconf->basic.h_period) * 100) /
			pconf->basic.v_period;
		pdrv->std_duration.duration_den = 100;
		pdrv->std_duration.frac = 0;
	} else {
		index = lcd_std_frame_rate_index(info->frame_rate);
		if (info->frac) {
			if (index < LCD_STD_FRAME_RATE_MAX)
				index++;
			pdrv->std_duration.frac = 1;
		} else {
			pdrv->std_duration.frac = 0;
		}
		pdrv->std_duration.duration_num = lcd_std_frame_rate[index][1];
		pdrv->std_duration.duration_den = lcd_std_frame_rate[index][2];
	}

	/* update vinfo */
	pdrv->vinfo.name = pdrv->output_name;
	pdrv->vinfo.mode = info->mode;
	pdrv->vinfo.width = info->width;
	pdrv->vinfo.height = info->height;
	pdrv->vinfo.field_height = info->height;
	pdrv->vinfo.aspect_ratio_num = pconf->basic.screen_width;
	pdrv->vinfo.aspect_ratio_den = pconf->basic.screen_height;
	pdrv->vinfo.screen_real_width = pconf->basic.screen_width;
	pdrv->vinfo.screen_real_height = pconf->basic.screen_height;
	pdrv->vinfo.sync_duration_num = pdrv->std_duration.duration_num;
	pdrv->vinfo.sync_duration_den = pdrv->std_duration.duration_den;
	pdrv->vinfo.frac = pdrv->std_duration.frac;
	pdrv->vinfo.video_clk = pdrv->config.timing.lcd_clk;
	pdrv->vinfo.htotal = pconf->basic.h_period;
	pdrv->vinfo.vtotal = pconf->basic.v_period;
	pdrv->vinfo.viu_mux = VIU_MUX_ENCL;
	switch (pdrv->config.timing.fr_adjust_type) {
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

static unsigned int lcd_parse_vout_init_name(char *name)
{
	char *p, *frac_str;
	unsigned int frac = 0;

	p = strchr(name, ',');
	if (!p) {
		frac = 0;
	} else {
		frac_str = p + 1;
		*p = '\0';
		if (strcmp(frac_str, "frac") == 0)
			frac = 1;
	}

	return frac;
}

/* ************************************************** *
 * vout server api
 * **************************************************
 */
static enum vmode_e lcd_validate_vmode(char *mode, unsigned int frac,
				       void *data)
{
	int lcd_vmode, frame_rate;
	int ret;
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	if (lcd_vout_serve_bypass) {
		LCDPR("[%d]: vout_serve bypass\n", pdrv->index);
		return VMODE_MAX;
	}
	if (!mode)
		return VMODE_MAX;

	lcd_vmode = lcd_outputmode_to_vmode(mode);
	if (lcd_vmode >= LCD_VMODE_MAX)
		return VMODE_MAX;

	ret = lcd_vmode_is_mached(pdrv, lcd_vmode);
	if (ret) {
		LCDERR("[%d]: %s: outputmode is not support\n",
		       pdrv->index, __func__);
		return VMODE_MAX;
	}
	if (lcd_fr_is_fixed(pdrv)) {
		LCDPR("[%d]: %s: fixed timing\n", pdrv->index, __func__);
		return lcd_vmode_info[lcd_vmode].mode;
	}

	frame_rate = lcd_outputmode_to_frame_rate(mode);
	if (frame_rate == 0) {
		LCDERR("[%d]: %s: frame_rate is not support\n",
		       pdrv->index, __func__);
		return VMODE_MAX;
	}
	if (frac) {
		if (frame_rate != 60) {
			LCDERR("[%d]: %s: don't support frac under mode %s\n",
			       pdrv->index, __func__, mode);
			return VMODE_MAX;
		}
		lcd_vmode_info[lcd_vmode].frac = 1;
	} else {
		lcd_vmode_info[lcd_vmode].frac = 0;
	}
	lcd_vmode_info[lcd_vmode].frame_rate = frame_rate;

	return lcd_vmode_info[lcd_vmode].mode;
}

static struct vinfo_s *lcd_get_current_info(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return NULL;
	return &pdrv->vinfo;
}

static int lcd_set_current_vmode(enum vmode_e mode, void *data)
{
	int ret = 0;
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	if (lcd_vout_serve_bypass) {
		LCDPR("[%d]: vout_serve bypass\n", pdrv->index);
		return 0;
	}

	if (lcd_fr_is_fixed(pdrv)) {
		LCDPR("[%d]: fixed timing, exit vmode change\n", pdrv->index);
		return -1;
	}
	mutex_lock(&lcd_power_mutex);
	/* do not change mode value here, for bit mask is useful */
	lcd_vmode_vinfo_update(pdrv, mode & VMODE_MODE_BIT_MASK);
	if (VMODE_LCD == (mode & VMODE_MODE_BIT_MASK)) {
		if (mode & VMODE_INIT_BIT_MASK) {
			lcd_clk_gate_switch(pdrv, 1);
		} else if (lcd_init_on_flag == 0) {
			lcd_init_on_flag = 1;
			if (pdrv->key_valid == 0 &&
			    !(pdrv->status & LCD_STATUS_ENCL_ON)) {
				aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, (void *)pdrv);
				lcd_if_enable_retry(pdrv);
			} else if (pdrv->driver_change) {
				mutex_lock(&lcd_vout_mutex);
				ret = pdrv->driver_change(pdrv);
				mutex_unlock(&lcd_vout_mutex);
			}
		} else if (lcd_init_on_flag == 1) {
			mutex_lock(&lcd_vout_mutex);
			if (pdrv->driver_change)
				ret = pdrv->driver_change(pdrv);
			else
				ret = -1;
			mutex_unlock(&lcd_vout_mutex);
		}
	} else {
		ret = -EINVAL;
	}

	pdrv->status |= LCD_STATUS_VMODE_ACTIVE;

	mutex_unlock(&lcd_power_mutex);
	return ret;
}

static int lcd_check_same_vmodeattr(char *mode, void *data)
{
	return 1;
}

static int lcd_vmode_is_supported(enum vmode_e mode, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	int lcd_vmode;

	mode &= VMODE_MODE_BIT_MASK;
	lcd_vmode = lcd_get_vmode(pdrv, mode);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s vmode = %d, lcd_vmode = %d(%s)\n",
		      pdrv->index, __func__, mode, lcd_vmode,
		      lcd_vmode_info[lcd_vmode].name);
	}

	if (lcd_vmode < LCD_VMODE_MAX)
		return true;
	else
		return false;
}

static int lcd_vout_disable(enum vmode_e cur_vmod, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	pdrv->status &= ~LCD_STATUS_VMODE_ACTIVE;

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
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct lcd_vmode_info_s *info;
	int ret = 0, i;

	info = &lcd_vmode_info[pdrv->output_vmode];
	for (i = 0; i < ACTIVE_FRAME_RATE_CNT; i++)
		ret += sprintf(buf + ret, "%s%dhz\n", info->name,
			      lcd_std_frame_rate[i][0]);

	return ret;
}

struct lcd_vframe_match_s {
	int frame_rate; /* *100 */
	unsigned int duration_num;
	unsigned int duration_den;
	unsigned int frac;
};

static struct lcd_vframe_match_s lcd_vframe_match_table_1[] = {
	{6000, 60, 1, 0},
	{5994, 60000, 1001, 1},
	{5000, 50, 1, 0}
};

static struct lcd_vframe_match_s lcd_vframe_match_table_2[] = {
	{6000, 60, 1, 0},
	{5994, 60000, 1001, 1},
	{5000, 50, 1, 0},
	{4800, 48, 1, 0},
	{4795, 48000, 1001, 0}
};

static int lcd_framerate_automation_set_mode(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv)
		return -1;

	LCDPR("[%d]: %s\n", pdrv->index, __func__);

	/* update interface timing */
	lcd_tv_config_update(pdrv);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.lcd_clk);
#endif

	if (pdrv->config.basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_interrupt_enable(pdrv, 0);
	/* change clk parameter */
	lcd_clk_change(pdrv);
	lcd_venc_change(pdrv);
	if (pdrv->config.basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_wait_stable(pdrv);

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

	if (lcd_vout_serve_bypass) {
		LCDPR("[%d]: vout_serve bypass\n", pdrv->index);
		return 0;
	}
	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
		LCDPR("[%d]: %s: lcd is disabled, exit\n",
		      pdrv->index, __func__);
		return -1;
	}

	if (lcd_fr_is_fixed(pdrv)) {
		LCDPR("[%d]: %s: fixed timing, exit\n", pdrv->index, __func__);
		return -1;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: fr_auto_policy = %d\n",
		      pdrv->index, pdrv->fr_auto_policy);
	}

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
		LCDPR("[%d]: %s: fr_auto_policy = %d, disabled\n",
		      pdrv->index, __func__, pdrv->fr_auto_policy);
		return 0;
	}

	if (duration == 0) { /* end hint */
		LCDPR("[%d]: %s: return mode = %s, policy = %d\n",
		      pdrv->index, __func__,
		      info->name, pdrv->fr_auto_policy);

		if (pdrv->fr_mode == 0) {
			LCDPR("[%d]: %s: fr_mode is invalid, exit\n",
			      pdrv->index, __func__);
			return 0;
		}

		pdrv->fr_duration = 0;
		/* update vinfo */
		info->sync_duration_num = pdrv->std_duration.duration_num;
		info->sync_duration_den = pdrv->std_duration.duration_den;
		info->frac = 0;
		pdrv->fr_mode = 0;
	} else {
		for (i = 0; i < n; i++) {
			if (duration == vtable[i].frame_rate) {
				frame_rate = vtable[i].frame_rate;
				duration_num = vtable[i].duration_num;
				duration_den = vtable[i].duration_den;
				frac = vtable[i].frac;
				find = 1;
				break;
			}
		}
		if (find == 0) {
			LCDERR("[%d]: %s: can't support duration %d\n, exit\n",
			       pdrv->index, __func__, duration);
			return -1;
		}

		LCDPR("[%d]: %s: policy = %d, duration = %d, frame_rate = %d\n",
		      pdrv->index, __func__, pdrv->fr_auto_policy,
		      duration, frame_rate);

		pdrv->fr_duration = duration;
		/* if the sync_duration is same as current */
		if (duration_num == info->sync_duration_num &&
		    duration_den == info->sync_duration_den &&
		    duration_den == info->sync_duration_den) {
			LCDPR("[%d]: %s: sync_duration is the same, exit\n",
			      pdrv->index, __func__);
			return 0;
		}

		/* update vinfo */
		info->sync_duration_num = duration_num;
		info->sync_duration_den = duration_den;
		info->frac = frac;
		pdrv->fr_mode = 1;
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
		LCDPR("[%d]: directly late_resume\n", pdrv->index);
		pdrv->resume_flag |= LCD_RESUME_ENABLE;
		aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)pdrv);
		lcd_if_enable_retry(pdrv);
		LCDPR("[%d]: late_resume finished\n", pdrv->index);
		mutex_unlock(&lcd_power_mutex);
	}

	return 0;
}

static void lcd_vinfo_update_default(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int index, frame_rate, frac;
	char *mode;

	if (!pdrv)
		return;

	mode = kstrdup(get_vout_mode_uboot(), GFP_KERNEL);
	if (!mode)
		return;
	frac = lcd_parse_vout_init_name(mode);
	frame_rate = lcd_outputmode_to_frame_rate(mode);
	if (frac) {
		if (frame_rate != 60 && frame_rate != 48) {
			LCDERR("[%d]: %s frac error\n", pdrv->index, __func__);
			kfree(mode);
			return;
		}
	}

	memset(pdrv->output_name, 0, sizeof(pdrv->output_name));
	snprintf(pdrv->output_name, sizeof(pdrv->output_name), "%s", mode);
	pdrv->vinfo.name = pdrv->output_name;
	pdrv->vinfo.mode = VMODE_LCD;
	pdrv->vinfo.width = pconf->basic.h_active;
	pdrv->vinfo.height = pconf->basic.v_active;
	pdrv->vinfo.field_height = pconf->basic.v_active;
	pdrv->vinfo.aspect_ratio_num = pconf->basic.h_active;
	pdrv->vinfo.aspect_ratio_den = pconf->basic.v_active;
	pdrv->vinfo.screen_real_width = pconf->basic.h_active;
	pdrv->vinfo.screen_real_height = pconf->basic.v_active;
	index = lcd_std_frame_rate_index(frame_rate);
	if (frac) {
		if (index < LCD_STD_FRAME_RATE_MAX)
			index++;
		pdrv->vinfo.frac = 1;
	} else {
		pdrv->vinfo.frac = 0;
	}
	pdrv->vinfo.sync_duration_num = lcd_std_frame_rate[index][1];
	pdrv->vinfo.sync_duration_den = lcd_std_frame_rate[index][2];
	pdrv->vinfo.video_clk = 0;
	pdrv->vinfo.htotal = pconf->basic.h_period;
	pdrv->vinfo.vtotal = pconf->basic.v_period;
	pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_NONE;

	kfree(mode);
}

void lcd_tv_vout_server_init(struct aml_lcd_drv_s *pdrv)
{
	struct vout_server_s *vserver;

	vserver = kzalloc(sizeof(*vserver), GFP_KERNEL);
	if (!vserver)
		return;
	vserver->name = kzalloc(32, GFP_KERNEL);
	if (!vserver->name) {
		kfree(vserver);
		return;
	}
	pdrv->vout_server[0] = vserver;

	sprintf(vserver->name, "lcd%d_vout_server", pdrv->index);
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

	lcd_vinfo_update_default(pdrv);

	vout_register_server(pdrv->vout_server[0]);
}

void lcd_tv_vout_server_remove(struct aml_lcd_drv_s *pdrv)
{
	vout_unregister_server(pdrv->vout_server[0]);
}

static void lcd_vmode_init(struct aml_lcd_drv_s *pdrv)
{
	char *mode, *init_mode;
	enum vmode_e vmode;
	unsigned int frac;

	init_mode = get_vout_mode_uboot();
	mode = kstrdup(init_mode, GFP_KERNEL);
	if (!mode)
		return;

	LCDPR("[%d]: %s: mode: %s\n", pdrv->index, __func__, mode);
	frac = lcd_parse_vout_init_name(mode);
	vmode = lcd_validate_vmode(mode, frac, (void *)pdrv);
	if (vmode >= VMODE_MAX) {
		LCDERR("[%d]: %s: invalid vout_init_mode: %s\n",
		       pdrv->index, __func__, init_mode);
		vmode = VMODE_LCD;
	}
	lcd_vmode_vinfo_update(pdrv, vmode & VMODE_MODE_BIT_MASK);
	kfree(mode);
}

static void lcd_config_init(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int temp;

	if (pdrv->config.timing.lcd_clk == 0) { /* default 0 for 60hz */
		pdrv->config.timing.lcd_clk = 60;
	} else {
		LCDPR("[%d]: custome clk: %d\n",
		      pdrv->index, pdrv->config.timing.lcd_clk);
	}
	temp = pdrv->config.timing.lcd_clk;
	if (temp < 200) { /* regard as frame_rate */
		pdrv->config.timing.lcd_clk = temp * pconf->basic.h_period *
			pconf->basic.v_period;
	} else { /* regard as pixel clock */
		pdrv->config.timing.lcd_clk = temp;
	}

	pdrv->config.timing.lcd_clk_dft = pdrv->config.timing.lcd_clk;
	pdrv->config.timing.h_period_dft = pconf->basic.h_period;
	pdrv->config.timing.v_period_dft = pconf->basic.v_period;
	/* before vmode_init to avoid period changing */
	lcd_timing_init_config(pdrv);
	lcd_vmode_init(pdrv);
	lcd_tv_config_update(pdrv);
	lcd_clk_generate_parameter(pdrv);

	if (cconf && cconf->data) {
		temp = pdrv->config.timing.ss_level & 0xff;
		cconf->ss_level = (temp >= cconf->data->ss_level_max) ? 0 : temp;
		temp = (pdrv->config.timing.ss_level >> 8) & 0xff;
		temp = (temp >> LCD_CLK_SS_BIT_FREQ) & 0xf;
		cconf->ss_freq = (temp >= cconf->data->ss_freq_max) ? 0 : temp;
		temp = (pdrv->config.timing.ss_level >> 8) & 0xff;
		temp = (temp >> LCD_CLK_SS_BIT_MODE) & 0xf;
		cconf->ss_mode = (temp >= cconf->data->ss_mode_max) ? 0 : temp;
	}
}

/* ************************************************** *
 * lcd notify
 * **************************************************
 */
/* sync_duration is real_value * 100 */
static void lcd_frame_rate_adjust(struct aml_lcd_drv_s *pdrv, int duration)
{
	LCDPR("[%d]: %s: sync_duration=%d\n", pdrv->index, __func__, duration);

	lcd_vout_notify_mode_change_pre(pdrv);

	/* update vinfo */
	pdrv->vinfo.sync_duration_num = duration;
	pdrv->vinfo.sync_duration_den = 100;

	/* update interface timing */
	lcd_tv_config_update(pdrv);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.lcd_clk);
#endif

	if (pdrv->config.basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_interrupt_enable(pdrv, 0);
	/* change clk parameter */
	lcd_clk_change(pdrv);
	lcd_venc_change(pdrv);
	if (pdrv->config.basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_wait_stable(pdrv);

	lcd_vout_notify_mode_change(pdrv);
}

/* ************************************************** *
 * lcd tv
 * **************************************************
 */
int lcd_mode_tv_init(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv)
		return -1;

	lcd_config_init(pdrv);

	pdrv->driver_init_pre = lcd_tv_driver_init_pre;
	pdrv->driver_disable_post = lcd_tv_driver_disable_post;
	pdrv->driver_init = lcd_tv_driver_init;
	pdrv->driver_disable = lcd_tv_driver_disable;
	pdrv->driver_change = lcd_tv_driver_change;
	pdrv->fr_adjust = lcd_frame_rate_adjust;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_interrupt_up(pdrv);
		break;
	default:
		break;
	}

	lcd_vrr_dev = kzalloc(sizeof(*lcd_vrr_dev), GFP_KERNEL);
	if (lcd_vrr_dev) {
		sprintf(lcd_vrr_dev->name, "lcd%d_dev", pdrv->index);
		lcd_vrr_dev->output_src = VRR_OUTPUT_ENCL;
		if (pdrv->config.timing.fr_adjust_type == 2) /* vtotal adj */
			lcd_vrr_dev->enable = 1;
		else
			lcd_vrr_dev->enable = 0;
		lcd_vrr_dev->vmax = pdrv->config.basic.v_period_max;
		lcd_vrr_dev->vmin = pdrv->config.basic.v_period_min;
		aml_vrr_register_device(lcd_vrr_dev, pdrv->index);
	}

	return 0;
}

int lcd_mode_tv_remove(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_vrr_dev)
		aml_vrr_unregister_device(pdrv->index);

	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_interrupt_down(pdrv);
		break;
	default:
		break;
	}

	return 0;
}

