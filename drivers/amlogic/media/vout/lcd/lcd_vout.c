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
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/compat.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/amlogic/pm.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#endif
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include "lcd_reg.h"
#include "lcd_common.h"

#include <linux/amlogic/gki_module.h>

#define LCD_CDEV_NAME  "lcd"

unsigned int lcd_debug_print_flag;
/* for driver global resource init:
 *  0: none
 *  n: initialized cnt
 */
static unsigned char lcd_global_init_flag;
static unsigned int lcd_drv_init_state;
static struct aml_lcd_drv_s *lcd_driver[LCD_MAX_DRV];
static struct workqueue_struct *lcd_workqueue;

/* 1: unlocked, 0: locked, negative: locked, possible waiters */
struct mutex lcd_vout_mutex;
/* 1: unlocked, 0: locked, negative: locked, possible waiters */
struct mutex lcd_power_mutex;
EXPORT_SYMBOL(lcd_power_mutex);

int lcd_vout_serve_bypass;

struct lcd_cdev_s {
	dev_t           devno;
	struct class    *class;
};

static struct lcd_cdev_s *lcd_cdev;
static char lcd_propname[LCD_MAX_DRV][24] = {"null", "null", "null"};
static char lcd_panel_name[LCD_MAX_DRV][24] = {"null", "null", "null"};

#define LCD_VSYNC_NONE_INTERVAL     msecs_to_jiffies(500)

/* *********************************************************
 * lcd config define
 * *********************************************************
 */
static struct lcd_boot_ctrl_s lcd_boot_ctrl_config[LCD_MAX_DRV] = {
	{
		.lcd_type = LCD_TYPE_MAX,
		.lcd_bits = 0,
		.advanced_flag = 0,
		.init_level = 0,
	},
	{
		.lcd_type = LCD_TYPE_MAX,
		.lcd_bits = 0,
		.advanced_flag = 0,
		.init_level = 0,
	},
	{
		.lcd_type = LCD_TYPE_MAX,
		.lcd_bits = 0,
		.advanced_flag = 0,
		.init_level = 0,
	}
};

static struct lcd_debug_ctrl_s lcd_debug_ctrl_config = {
	.debug_print_flag = 0,
	.debug_test_pattern = 0,
	.debug_para_source = 0,
	.debug_lcd_mode = 0,
};

static struct ioctl_phy_config_s ioctl_phy_config = {
	.flag = 0,
	.vswing = 0,
	.vcm = 0,
	.odt = 0,
	.ref_bias = 0,
	.mode = 0,
	.weakly_pull_down = 0,
	.lane_num = 0,
	.ext_pullup = 0,
	.ioctl_mode = 1,
	.vswing_level = 0,
	.preem_level = 0,
};

static struct aml_lcd_drv_s *lcd_driver_add(int index)
{
	struct aml_lcd_drv_s *pdrv = NULL;

	if (index >= LCD_MAX_DRV)
		return NULL;

	pdrv = kzalloc(sizeof(*pdrv), GFP_KERNEL);
	if (!pdrv)
		return NULL;

	pdrv->index = index;

	/* default config */
	strcpy(pdrv->config.propname, lcd_propname[index]);
	pdrv->config.basic.lcd_type = LCD_TYPE_MAX;
	pdrv->config.power.power_on_step[0].type = LCD_POWER_TYPE_MAX;
	pdrv->config.power.power_off_step[0].type = LCD_POWER_TYPE_MAX;
	pdrv->config.pinmux_flag = 0xff;
	pdrv->config.backlight_index = 0xff;

	/* default vinfo */
	pdrv->vinfo.mode = VMODE_LCD;
	pdrv->vinfo.frac = 0;
	pdrv->vinfo.viu_color_fmt = COLOR_FMT_RGB444;
	pdrv->vinfo.viu_mux = ((index << 4) | VIU_MUX_ENCL);
	pdrv->vinfo.vout_device = NULL;

	/* boot ctrl */
	pdrv->boot_ctrl = &lcd_boot_ctrl_config[index];
	pdrv->debug_ctrl = &lcd_debug_ctrl_config;

	return pdrv;
}

struct aml_lcd_drv_s *aml_lcd_get_driver(int index)
{
	if (index >= LCD_MAX_DRV)
		return NULL;

	return lcd_driver[index];
}
EXPORT_SYMBOL(aml_lcd_get_driver);

inline void lcd_queue_work(struct work_struct *work)
{
	if (lcd_workqueue)
		queue_work(lcd_workqueue, work);
	else
		schedule_work(work);
}

inline void lcd_queue_delayed_work(struct delayed_work *dwork, int ms)
{
	if (lcd_workqueue)
		queue_delayed_work(lcd_workqueue, dwork, msecs_to_jiffies(ms));
	else
		schedule_delayed_work(dwork, msecs_to_jiffies(ms));
}

/* ********************************************************* */
static void lcd_power_ctrl(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_power_step_s *power_step;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	struct lcd_extern_driver_s *edrv;
	struct lcd_extern_dev_s *edev;
#endif
	unsigned int i, index, wait;
	int value = -1;

	if (pdrv->lcd_pxp) {
		LCDPR("[%d]: %s: lcd_pxp bypass\n", pdrv->index, __func__);
		return;
	}

	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, status);
	i = 0;
	while (i < LCD_PWR_STEP_MAX) {
		if (status)
			power_step = &pdrv->config.power.power_on_step[i];
		else
			power_step = &pdrv->config.power.power_off_step[i];

		if (power_step->type >= LCD_POWER_TYPE_MAX)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: power_ctrl: %d, step %d\n",
			      pdrv->index, status, i);
			LCDPR("[%d]: %s: type=%d, index=%d, value=%d, delay=%d\n",
			      pdrv->index, __func__,
			      power_step->type, power_step->index,
			      power_step->value, power_step->delay);
		}
		switch (power_step->type) {
		case LCD_POWER_TYPE_CPU:
			index = power_step->index;
			lcd_cpu_gpio_set(pdrv, index, power_step->value);
			break;
		case LCD_POWER_TYPE_PMU:
			LCDPR("to do\n");
			break;
		case LCD_POWER_TYPE_SIGNAL:
			if (status)
				pdrv->driver_init(pdrv);
			else
				pdrv->driver_disable(pdrv);
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			index = power_step->index;
			edrv = lcd_extern_get_driver(pdrv->index);
			edev = lcd_extern_get_dev(edrv, index);
			if (!edrv || !edev)
				break;
			if (status) {
				if (edev->power_on)
					edev->power_on(edrv, edev);
				else
					LCDERR("[%d]: no ext_%d power on\n", pdrv->index, index);
			} else {
				if (edev->power_off)
					edev->power_off(edrv, edev);
				else
					LCDERR("[%d]: no ext_%d power off\n", pdrv->index, index);
			}
			break;
#endif
		case LCD_POWER_TYPE_WAIT_GPIO:
			index = power_step->index;
			lcd_cpu_gpio_set(pdrv, index, LCD_GPIO_INPUT);
			LCDPR("[%d]: lcd_power_type_wait_gpio wait\n",
			      pdrv->index);
			for (wait = 0; wait < power_step->delay; wait++) {
				value = lcd_cpu_gpio_get(pdrv, index);
				if (value == power_step->value) {
					LCDPR("[%d]: wait_gpio %d ok\n",
					      pdrv->index, value);
					break;
				}
				mdelay(1);
			}
			if (wait == power_step->delay) {
				LCDERR("[%d]: wait_gpio %d timeout!\n",
				       pdrv->index, value);
			}
			break;
		case LCD_POWER_TYPE_CLK_SS:
			break;
		default:
			break;
		}
		if (power_step->type != LCD_POWER_TYPE_WAIT_GPIO &&
		    power_step->delay > 0)
			lcd_delay_ms(power_step->delay);
		i++;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d finished\n", pdrv->index, __func__, status);
}

static void lcd_dlg_switch_mode(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_power_step_s *power_step;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	struct lcd_extern_driver_s *edrv;
	struct lcd_extern_dev_s *edev;
#endif
	unsigned int i = 0, index;
	unsigned long long local_time[3];

	LCDPR("[%d]: %s\n", pdrv->index, __func__);
	while (i < LCD_PWR_STEP_MAX) {
		power_step = &pdrv->config.power.power_on_step[i];

		if (power_step->type >= LCD_POWER_TYPE_MAX)
			break;
		switch (power_step->type) {
		case LCD_POWER_TYPE_SIGNAL:
			if (pdrv->config.basic.lcd_type == LCD_P2P) {
				local_time[0] = sched_clock();
				lcd_tcon_reload(pdrv);
				local_time[1] = sched_clock();
				pdrv->config.cus_ctrl.tcon_reload_time =
					local_time[1] - local_time[0];
			}
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			local_time[0] = sched_clock();
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: power_ctrl step %d\n",
					pdrv->index, i);
				LCDPR("[%d]: %s: type=%d, index=%d, value=%d, delay=%d\n",
					pdrv->index, __func__,
					power_step->type, power_step->index,
					power_step->value, power_step->delay);
			}
			index = power_step->index;
			edrv = lcd_extern_get_driver(pdrv->index);
			edev = lcd_extern_get_dev(edrv, index);
			if (!edrv || !edev)
				break;
			if (edev->power_on)
				edev->power_on(edrv, edev);
			else
				LCDERR("[%d]: no ext_%d power on\n", pdrv->index, index);
			local_time[1] = sched_clock();
			pdrv->config.cus_ctrl.level_shift_time = local_time[1] - local_time[0];
			break;
#endif
		default:
			break;
		}
		i++;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
}

static void lcd_dlg_power_ctrl(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_power_step_s *power_step;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	struct lcd_extern_driver_s *edrv;
	struct lcd_extern_dev_s *edev;
#endif
	unsigned int i, index;
	unsigned long long local_time[3];

	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, status);
	i = 0;
	while (i < LCD_PWR_STEP_MAX) {
		if (status)
			power_step = &pdrv->config.power.power_on_step[i];
		else
			power_step = &pdrv->config.power.power_off_step[i];

		if (power_step->type >= LCD_POWER_TYPE_MAX)
			break;
		switch (power_step->type) {
		case LCD_POWER_TYPE_SIGNAL:
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: power_ctrl: %d, step %d\n",
					pdrv->index, status, i);
				LCDPR("[%d]: %s: type=%d, index=%d, value=%d, delay=%d\n",
					pdrv->index, __func__,
					power_step->type, power_step->index,
					power_step->value, power_step->delay);
			}
			if (status)
				pdrv->driver_init(pdrv);
			else
				pdrv->driver_disable(pdrv);
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			local_time[0] = sched_clock();
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: power_ctrl: %d, step %d\n",
					pdrv->index, status, i);
				LCDPR("[%d]: %s: type=%d, index=%d, value=%d, delay=%d\n",
					pdrv->index, __func__,
					power_step->type, power_step->index,
					power_step->value, power_step->delay);
			}
			index = power_step->index;
			edrv = lcd_extern_get_driver(pdrv->index);
			edev = lcd_extern_get_dev(edrv, index);
			if (!edrv || !edev)
				break;
			if (status) {
				if (edev->power_on)
					edev->power_on(edrv, edev);
				else
					LCDERR("[%d]: no ext_%d power on\n", pdrv->index, index);
			} else {
				if (edev->power_off)
					edev->power_off(edrv, edev);
				else
					LCDERR("[%d]: no ext_%d power off\n", pdrv->index, index);
			}
			local_time[1] = sched_clock();
			pdrv->config.cus_ctrl.level_shift_time = local_time[1] - local_time[0];
			break;
#endif
		default:
			break;
		}
		i++;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d finished\n", pdrv->index, __func__, status);
}

static void lcd_power_encl_on(struct aml_lcd_drv_s *pdrv)
{
	mutex_lock(&lcd_vout_mutex);

	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		LCDPR("[%d]: %s: on already\n", pdrv->index, __func__);
		mutex_unlock(&lcd_vout_mutex);
		return;
	}

	pdrv->driver_init_pre(pdrv);
	pdrv->status |= LCD_STATUS_ENCL_ON;

	/* vsync_none_timer conditional enabled to save cpu loading */
	if (pdrv->viu_sel == LCD_VIU_SEL_NONE) {
		if (pdrv->vsync_none_timer_flag == 0) {
			pdrv->vs_none_timer.expires =
				jiffies + LCD_VSYNC_NONE_INTERVAL;
			add_timer(&pdrv->vs_none_timer);
			pdrv->vsync_none_timer_flag = 1;
			LCDPR("[%d]: add vs_none_timer handler\n", pdrv->index);
		}
	} else {
		if (pdrv->vsync_none_timer_flag) {
			del_timer_sync(&pdrv->vs_none_timer);
			pdrv->vsync_none_timer_flag = 0;
		}
	}

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_power_encl_off(struct aml_lcd_drv_s *pdrv)
{
	mutex_lock(&lcd_vout_mutex);

	if (!(pdrv->status & LCD_STATUS_ENCL_ON)) {
		LCDPR("[%d]: %s: off already\n", pdrv->index, __func__);
		mutex_unlock(&lcd_vout_mutex);
		return;
	}
	pdrv->status &= ~LCD_STATUS_ENCL_ON;
	pdrv->driver_disable_post(pdrv);

	if (pdrv->vsync_none_timer_flag) {
		del_timer_sync(&pdrv->vs_none_timer);
		pdrv->vsync_none_timer_flag = 0;
	}

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_dlg_power_if_on(struct aml_lcd_drv_s *pdrv)
{
	mutex_lock(&lcd_vout_mutex);
	if (!(pdrv->status & LCD_STATUS_IF_ON)) {
		if (pdrv->config.cus_ctrl.dlg_flag) {
			if (pdrv->config.cus_ctrl.dlg_flag == 1)
				lcd_power_ctrl(pdrv, 1);
			else if (pdrv->config.cus_ctrl.dlg_flag == 2)
				lcd_dlg_power_ctrl(pdrv, 1);
		} else {
			lcd_power_ctrl(pdrv, 1);
		}
		pdrv->status |= LCD_STATUS_IF_ON;
	}
	pdrv->config.change_flag = 0;

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_dlg_power_if_off(struct aml_lcd_drv_s *pdrv)
{
	mutex_lock(&lcd_vout_mutex);
	if (pdrv->status & LCD_STATUS_IF_ON) {
		pdrv->status &= ~LCD_STATUS_IF_ON;
		if (pdrv->config.cus_ctrl.dlg_flag) {
			if (pdrv->config.cus_ctrl.dlg_flag == 1)
				lcd_power_ctrl(pdrv, 0);
			else if (pdrv->config.cus_ctrl.dlg_flag == 2)
				lcd_dlg_power_ctrl(pdrv, 0);
		} else {
			lcd_power_ctrl(pdrv, 0);
		}
	}
	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_power_if_on(struct aml_lcd_drv_s *pdrv)
{
	mutex_lock(&lcd_vout_mutex);
	if (!(pdrv->status & LCD_STATUS_IF_ON)) {
		lcd_power_ctrl(pdrv, 1);
		pdrv->status |= LCD_STATUS_IF_ON;
	}
	pdrv->config.change_flag = 0;

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_power_if_off(struct aml_lcd_drv_s *pdrv)
{
	mutex_lock(&lcd_vout_mutex);
	if (pdrv->status & LCD_STATUS_IF_ON) {
		pdrv->status &= ~LCD_STATUS_IF_ON;
		lcd_power_ctrl(pdrv, 0);
	}
	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_power_screen_black(struct aml_lcd_drv_s *pdrv)
{
	mutex_lock(&lcd_vout_mutex);

	lcd_screen_black(pdrv);

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_power_screen_restore(struct aml_lcd_drv_s *pdrv)
{
	mutex_lock(&lcd_vout_mutex);

	lcd_screen_restore(pdrv);

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_module_reset(struct aml_lcd_drv_s *pdrv)
{
	mutex_lock(&lcd_vout_mutex);

	pdrv->status &= ~LCD_STATUS_ON;
	lcd_power_ctrl(pdrv, 0);

	msleep(500);

	pdrv->driver_init_pre(pdrv);
	lcd_power_ctrl(pdrv, 1);
	pdrv->status |= LCD_STATUS_ON;
	pdrv->config.change_flag = 0;

	lcd_screen_restore(pdrv);
	LCDPR("[%d]: clear mute\n", pdrv->index);

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_screen_restore_work(struct work_struct *work)
{
	unsigned long flags = 0;
	int ret = 0;
	struct aml_lcd_drv_s *pdrv;
	unsigned long long local_time[3];

	local_time[0] = sched_clock();
	pdrv = container_of(work, struct aml_lcd_drv_s, screen_restore_work);

	mutex_lock(&lcd_power_mutex);
	reinit_completion(&pdrv->vsync_done);
	spin_lock_irqsave(&pdrv->isr_lock, flags);
	if (pdrv->unmute_count_test)
		pdrv->mute_count = pdrv->unmute_count_test;
	else
		pdrv->mute_count = 4;
	pdrv->mute_flag = 1;
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);
	ret = wait_for_completion_timeout(&pdrv->vsync_done,
					  msecs_to_jiffies(500));
	if (!ret)
		LCDPR("vmode switch: wait_for_completion_timeout\n");
	lcd_screen_restore(pdrv);
	mutex_unlock(&lcd_power_mutex);
	local_time[1] = sched_clock();
	pdrv->config.cus_ctrl.unmute_time = local_time[1] - local_time[0];
}

static void lcd_lata_resume_work(struct work_struct *work)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = container_of(work, struct aml_lcd_drv_s, late_resume_work);

	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)pdrv);
	lcd_if_enable_retry(pdrv);
	LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	mutex_unlock(&lcd_power_mutex);
}

static void lcd_auto_test_delayed(struct work_struct *p_work)
{
	struct delayed_work *d_work;
	struct aml_lcd_drv_s *pdrv;

	d_work = container_of(p_work, struct delayed_work, work);
	pdrv = container_of(d_work, struct aml_lcd_drv_s,
			    test_delayed_work);

	LCDPR("[%d]: %s\n", pdrv->index, __func__);
	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, (void *)pdrv);
	mutex_unlock(&lcd_power_mutex);
}

static void lcd_auto_test_func(struct aml_lcd_drv_s *pdrv)
{
	pdrv->test_state = pdrv->auto_test;
	lcd_queue_delayed_work(&pdrv->test_delayed_work, 20000);
}

static int lcd_vsync_print_cnt;
static inline void lcd_vsync_handler(struct aml_lcd_drv_s *pdrv)
{
	unsigned long flags = 0;
	unsigned int temp;

	if (!pdrv)
		return;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_MIPI:
#ifdef CONFIG_AMLOGIC_LCD_TABLET
		if (pdrv->config.control.mipi_cfg.dread) {
			if (pdrv->config.control.mipi_cfg.dread->flag) {
				lcd_mipi_test_read(pdrv, pdrv->config.control.mipi_cfg.dread);
				pdrv->config.control.mipi_cfg.dread->flag = 0;
			}
		}
#endif
		break;
	case LCD_VBYONE:
		if (pdrv->vbyone_vsync_handler)
			pdrv->vbyone_vsync_handler(pdrv);
		break;
	case LCD_MLVDS:
	case LCD_P2P:
		lcd_tcon_vsync_isr(pdrv);
		break;
	default:
		break;
	}

	spin_lock_irqsave(&pdrv->isr_lock, flags);
	if (pdrv->mute_flag) {
		if (pdrv->mute_count > 0) {
			pdrv->mute_count--;
		} else if (pdrv->mute_count == 0) {
			complete(&pdrv->vsync_done);
			pdrv->mute_flag = 0;
		}
	}

	if (pdrv->test_flag != pdrv->test_state) {
		pdrv->test_state = pdrv->test_flag;
		lcd_debug_test(pdrv, pdrv->test_state);
	}
	if (pdrv->vs_msr && pdrv->vs_msr_rt && pdrv->vs_msr_en) {
		temp = vout_frame_rate_msr_high_res(pdrv->viu_sel);
		pdrv->vs_msr_rt[pdrv->vs_msr_i++] = temp;
		pdrv->vs_msr_sum_temp += temp;
		if (temp > pdrv->vs_msr_max)
			pdrv->vs_msr_max = temp;
		if (temp < pdrv->vs_msr_min)
			pdrv->vs_msr_min = temp;
		if (pdrv->vs_msr_i >= pdrv->config.timing.act_timing.frame_rate) {
			pdrv->vs_msr[pdrv->vs_msr_cnt++] =
				lcd_do_div(pdrv->vs_msr_sum_temp, pdrv->vs_msr_i);
			pdrv->vs_msr_sum_temp = 0;
			if (pdrv->vs_msr_cnt >= pdrv->vs_msr_cnt_max)
				pdrv->vs_msr_en = 0;
			else
				pdrv->vs_msr_i = 0;
		}
	}
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);

	if (lcd_vsync_print_cnt++ >= LCD_DEBUG_VSYNC_INTERVAL) {
		lcd_vsync_print_cnt = 0;
		if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
			LCDPR("[%d]: %s: viu_sel: %d, mute_count: %d\n",
			      pdrv->index, __func__, pdrv->viu_sel, pdrv->mute_count);
		}
	}
}

static irqreturn_t lcd_vsync_isr(int irq, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return IRQ_HANDLED;

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		return IRQ_HANDLED;

	if (pdrv->viu_sel == 1) {
		lcd_vsync_handler(pdrv);
		if (pdrv->vsync_cnt++ >= 65536)
			pdrv->vsync_cnt = 0;
	}

	return IRQ_HANDLED;
}

static irqreturn_t lcd_vsync2_isr(int irq, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return IRQ_HANDLED;

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		return IRQ_HANDLED;

	if (pdrv->viu_sel == 2) {
		lcd_vsync_handler(pdrv);
		if (pdrv->vsync_cnt++ >= 65536)
			pdrv->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t lcd_vsync3_isr(int irq, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return IRQ_HANDLED;

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		return IRQ_HANDLED;

	if (pdrv->viu_sel == 3) {
		lcd_vsync_handler(pdrv);
		if (pdrv->vsync_cnt++ >= 65536)
			pdrv->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static void lcd_vsync_none_timer_handler(struct timer_list *timer)
{
	struct aml_lcd_drv_s *pdrv = from_timer(pdrv, timer, vs_none_timer);

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		goto lcd_vsync_none_timer_handler_end;

	if (pdrv->vsync_cnt == pdrv->vsync_cnt_previous) {
		lcd_wait_vsync(pdrv);
		lcd_vsync_handler(pdrv);
	}

	pdrv->vsync_cnt_previous = pdrv->vsync_cnt;

lcd_vsync_none_timer_handler_end:
	if (pdrv->vsync_none_timer_flag) {
		pdrv->vs_none_timer.expires =
			jiffies + LCD_VSYNC_NONE_INTERVAL;
		add_timer(&pdrv->vs_none_timer);
	}
}

/* ****************************************
 * lcd notify
 * ****************************************
 */
static int lcd_power_encl_on_notifier(struct notifier_block *nb,
				      unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_ENCL_ON) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: 0x%lx\n", pdrv->index, __func__, event);

	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		LCDPR("[%d]: lcd is already enabled\n", pdrv->index);
		return NOTIFY_OK;
	}

	lcd_power_encl_on(pdrv);

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_encl_on_nb = {
	.notifier_call = lcd_power_encl_on_notifier,
	.priority = LCD_PRIORITY_POWER_ENCL_ON,
};

static int lcd_power_encl_off_notifier(struct notifier_block *nb,
				       unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_ENCL_OFF) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: 0x%lx\n", pdrv->index, __func__, event);

	if (pdrv->status & LCD_STATUS_IF_ON) {
		LCDPR("[%d]: %s: force power off interface ahead\n", pdrv->index, __func__);
		lcd_power_if_off(pdrv);
	}

	lcd_power_encl_off(pdrv);

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_encl_off_nb = {
	.notifier_call = lcd_power_encl_off_notifier,
	.priority = LCD_PRIORITY_POWER_ENCL_OFF,
};

static int lcd_dlg_switch_mode_notifier(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_DLG_SWITCH_MODE) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: 0x%lx\n", pdrv->index, __func__, event);

	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		mutex_lock(&lcd_vout_mutex);
		lcd_dlg_switch_mode(pdrv);
		mutex_unlock(&lcd_vout_mutex);
	} else {
		LCDERR("[%d]: %s: can't power on when controller is off\n",
		       pdrv->index, __func__);
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block lcd_dlg_switch_mode_nb = {
	.notifier_call = lcd_dlg_switch_mode_notifier,
	.priority = LCD_PRIORITY_DLG_SWITCH_MODE,
};

static int lcd_dlg_power_if_on_notifier(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_DLG_POWER_ON) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: 0x%lx\n", pdrv->index, __func__, event);

	if (pdrv->status & LCD_STATUS_IF_ON) {
		LCDPR("[%d]: lcd interface is already enabled\n", pdrv->index);
		return NOTIFY_OK;
	}

	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		lcd_dlg_power_if_on(pdrv);
	} else {
		LCDERR("[%d]: %s: can't power on when controller is off\n",
		       pdrv->index, __func__);
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block lcd_dlg_power_if_on_nb = {
	.notifier_call = lcd_dlg_power_if_on_notifier,
	.priority = LCD_PRIORITY_DLG_POWER_IF_ON,
};

static int lcd_dlg_power_if_off_notifier(struct notifier_block *nb,
					 unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_DLG_POWER_OFF) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: 0x%lx\n", pdrv->index, __func__, event);
	lcd_dlg_power_if_off(pdrv);

	return NOTIFY_OK;
}

static struct notifier_block lcd_dlg_power_if_off_nb = {
	.notifier_call = lcd_dlg_power_if_off_notifier,
	.priority = LCD_PRIORITY_DLG_POWER_IF_OFF,
};

static int lcd_power_if_on_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_IF_ON) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: 0x%lx\n", pdrv->index, __func__, event);

	if (pdrv->status & LCD_STATUS_IF_ON) {
		LCDPR("[%d]: lcd interface is already enabled\n", pdrv->index);
		return NOTIFY_OK;
	}

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
		LCDPR("[%d]: %s: force power on controller ahead\n", pdrv->index, __func__);
		lcd_power_encl_on(pdrv);
	}

	lcd_power_if_on(pdrv);

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_if_on_nb = {
	.notifier_call = lcd_power_if_on_notifier,
	.priority = LCD_PRIORITY_POWER_IF_ON,
};

static int lcd_power_if_off_notifier(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_IF_OFF) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: 0x%lx\n", pdrv->index, __func__, event);

	lcd_power_if_off(pdrv);

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_if_off_nb = {
	.notifier_call = lcd_power_if_off_notifier,
	.priority = LCD_PRIORITY_POWER_IF_OFF,
};

static int lcd_power_screen_black_notifier(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	unsigned long long local_time[3];

	local_time[0] = sched_clock();

	if ((event & LCD_EVENT_SCREEN_BLACK) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: 0x%lx\n", pdrv->index, __func__, event);
	lcd_power_screen_black(pdrv);
	local_time[1] = sched_clock();

	pdrv->config.cus_ctrl.mute_time = local_time[1] - local_time[0];
	return NOTIFY_OK;
}

static struct notifier_block lcd_power_screen_black_nb = {
	.notifier_call = lcd_power_screen_black_notifier,
	.priority = LCD_PRIORITY_SCREEN_BLACK,
};

static int lcd_power_screen_restore_notifier(struct notifier_block *nb,
					     unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	unsigned long long local_time[3];

	local_time[0] = sched_clock();

	if ((event & LCD_EVENT_SCREEN_RESTORE) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: 0x%lx\n", pdrv->index, __func__, event);
	lcd_power_screen_restore(pdrv);
	local_time[1] = sched_clock();

	pdrv->config.cus_ctrl.unmute_time = local_time[1] - local_time[0];

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_screen_restore_nb = {
	.notifier_call = lcd_power_screen_restore_notifier,
	.priority = LCD_PRIORITY_SCREEN_RESTORE,
};

static int lcd_vlock_param_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv;
	int index;
	unsigned int *param;

	if ((event & LCD_EVENT_VLOCK_PARAM) == 0)
		return NOTIFY_DONE;
	if (!data) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}

	param = (unsigned int *)data;
	index = param[LCD_VLOCK_PARAM_NUM];
	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: 0x%lx\n", index, __func__, event);

	memcpy(param, pdrv->config.vlock_param,
	       (LCD_VLOCK_PARAM_NUM * sizeof(unsigned int)));

	return NOTIFY_OK;
}

static struct notifier_block lcd_vlock_param_nb = {
	.notifier_call = lcd_vlock_param_notifier,
};

static int lcd_notifier_init(void)
{
	int ret = 0;

	ret = aml_lcd_notifier_register(&lcd_power_encl_on_nb);
	if (ret)
		LCDERR("register lcd_power_encl_on_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_encl_off_nb);
	if (ret)
		LCDERR("register lcd_power_encl_off_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_dlg_switch_mode_nb);
	if (ret)
		LCDERR("register lcd_dlg_switch_mode_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_dlg_power_if_on_nb);
	if (ret)
		LCDERR("register lcd_dlg_power_if_on_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_dlg_power_if_off_nb);
	if (ret)
		LCDERR("register lcd_dlg_power_if_off_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_if_on_nb);
	if (ret)
		LCDERR("register lcd_power_if_on_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_if_off_nb);
	if (ret)
		LCDERR("register lcd_power_if_off_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_screen_black_nb);
	if (ret)
		LCDERR("register lcd_power_screen_black_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_screen_restore_nb);
	if (ret)
		LCDERR("register lcd_power_screen_restore_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_vlock_param_nb);
	if (ret)
		LCDERR("register lcd_vlock_param_nb failed\n");

	return 0;
}

static void lcd_notifier_remove(void)
{
	aml_lcd_notifier_unregister(&lcd_power_screen_restore_nb);
	aml_lcd_notifier_unregister(&lcd_power_screen_black_nb);
	aml_lcd_notifier_unregister(&lcd_dlg_switch_mode_nb);
	aml_lcd_notifier_unregister(&lcd_dlg_power_if_off_nb);
	aml_lcd_notifier_unregister(&lcd_dlg_power_if_on_nb);
	aml_lcd_notifier_unregister(&lcd_power_if_off_nb);
	aml_lcd_notifier_unregister(&lcd_power_if_on_nb);
	aml_lcd_notifier_unregister(&lcd_power_encl_off_nb);
	aml_lcd_notifier_unregister(&lcd_power_encl_on_nb);

	aml_lcd_notifier_unregister(&lcd_vlock_param_nb);
}

/* **************************************** */

/* ************************************************************* */
/* lcd ioctl                                                     */
/* ************************************************************* */
static int lcd_io_open(struct inode *inode, struct file *file)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = container_of(inode->i_cdev, struct aml_lcd_drv_s, cdev);
	file->private_data = pdrv;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	return 0;
}

static int lcd_io_release(struct inode *inode, struct file *file)
{
	struct aml_lcd_drv_s *pdrv;

	if (!file->private_data)
		return 0;

	pdrv = (struct aml_lcd_drv_s *)file->private_data;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	file->private_data = NULL;
	return 0;
}

static long lcd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp;
	int mcd_nr = -1;
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)file->private_data;
	struct lcd_optical_info_s *opt_info;
	union lcd_ctrl_config_u *pctrl;
	struct lcd_config_s *pconf;
	struct phy_config_s *phy_cfg;
	struct ioctl_phy_config_s *ioctl_phy_cfg = &ioctl_phy_config, ioctl_phy_usr;
	unsigned int ss_level = 0xffffffff, ss_freq = 0xffffffff, ss_mode = 0xffffffff;
	struct aml_lcd_ss_ctl_s ss_ctl = {0xffffffff, 0xffffffff, 0xffffffff};
	unsigned int temp, i = 0, lane_num;
	int ret = 0;

	if (!pdrv)
		return -EFAULT;

	pconf = &pdrv->config;
	pctrl = &pdrv->config.control;
	opt_info = &pdrv->config.optical;
	mcd_nr = _IOC_NR(cmd);
	LCDPR("[%d]: %s: cmd_dir = 0x%x, cmd_nr = 0x%x\n",
	      pdrv->index, __func__, _IOC_DIR(cmd), mcd_nr);

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case LCD_IOC_NR_GET_HDR_INFO:
		if (copy_to_user(argp, opt_info, sizeof(struct lcd_optical_info_s)))
			ret = -EFAULT;
		break;
	case LCD_IOC_NR_SET_HDR_INFO:
		if (copy_from_user(opt_info, argp, sizeof(struct lcd_optical_info_s))) {
			ret = -EFAULT;
		} else {
			lcd_optical_vinfo_update(pdrv);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: set optical info:\n"
					"hdr_support          %d\n"
					"features             %d\n"
					"primaries_r_x        %d\n"
					"primaries_r_y        %d\n"
					"primaries_g_x        %d\n"
					"primaries_g_y        %d\n"
					"primaries_b_x        %d\n"
					"primaries_b_y        %d\n"
					"white_point_x        %d\n"
					"white_point_y        %d\n"
					"luma_max             %d\n"
					"luma_min             %d\n"
					"luma_avg             %d\n\n",
					pdrv->index,
					opt_info->hdr_support,
					opt_info->features,
					opt_info->primaries_r_x,
					opt_info->primaries_r_y,
					opt_info->primaries_g_x,
					opt_info->primaries_g_y,
					opt_info->primaries_b_x,
					opt_info->primaries_b_y,
					opt_info->white_point_x,
					opt_info->white_point_y,
					opt_info->luma_max,
					opt_info->luma_min,
					opt_info->luma_avg);
			}
		}
		break;
	case LCD_IOC_GET_TCON_BIN_MAX_CNT_INFO:
	case LCD_IOC_SET_TCON_DATA_INDEX_INFO:
	case LCD_IOC_GET_TCON_BIN_PATH_INFO:
	case LCD_IOC_SET_TCON_BIN_DATA_INFO:
		lcd_tcon_ioctl_handler(pdrv, mcd_nr, arg);
		break;
	case LCD_IOC_POWER_CTRL:
		if (copy_from_user((void *)&temp, argp, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}

		if (temp) {
			mutex_lock(&lcd_power_mutex);
			aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_ON, (void *)pdrv);
			lcd_if_enable_retry(pdrv);
			mutex_unlock(&lcd_power_mutex);
		} else {
			mutex_lock(&lcd_power_mutex);
			aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_OFF, (void *)pdrv);
			mutex_unlock(&lcd_power_mutex);
		}
		break;
	case LCD_IOC_MUTE_CTRL:
		if (copy_from_user((void *)&temp, argp, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}

		if (temp)
			lcd_screen_black(pdrv);
		else
			lcd_screen_restore(pdrv);

		break;
	case LCD_IOC_GET_PHY_PARAM:
		phy_cfg = &pconf->phy_cfg;
		ioctl_phy_cfg->flag = phy_cfg->flag;
		ioctl_phy_cfg->vswing = phy_cfg->vswing;
		ioctl_phy_cfg->vcm = phy_cfg->vcm;
		ioctl_phy_cfg->odt = phy_cfg->odt;
		ioctl_phy_cfg->ref_bias = phy_cfg->ref_bias;
		ioctl_phy_cfg->mode = phy_cfg->mode;
		ioctl_phy_cfg->weakly_pull_down = phy_cfg->weakly_pull_down;
		ioctl_phy_cfg->lane_num = phy_cfg->lane_num;
		ioctl_phy_cfg->ext_pullup = phy_cfg->ext_pullup;
		ioctl_phy_cfg->vswing_level = phy_cfg->vswing_level;
		ioctl_phy_cfg->preem_level = phy_cfg->preem_level;
		lane_num = phy_cfg->lane_num > CH_LANE_MAX ? CH_LANE_MAX : phy_cfg->lane_num;
		for (i = 0; i < lane_num; i++) {
			ioctl_phy_cfg->ioctl_lane[i].preem = phy_cfg->lane[i].preem;
			ioctl_phy_cfg->ioctl_lane[i].amp = phy_cfg->lane[i].amp;
		}
		if (copy_to_user(argp, (const void *)ioctl_phy_cfg,
		    sizeof(struct ioctl_phy_config_s)))
			ret = -EFAULT;

		break;
	case LCD_IOC_SET_PHY_PARAM:
		memset(&ioctl_phy_usr, 0, sizeof(struct ioctl_phy_config_s));
		if (copy_from_user((void *)&ioctl_phy_usr, argp,
		    sizeof(struct ioctl_phy_config_s))) {
			ret = -EFAULT;
			break;
		}

		phy_cfg = &pconf->phy_cfg;
		if (ioctl_phy_usr.ioctl_mode == 0) {
			switch (pdrv->config.basic.lcd_type) {
			case LCD_LVDS:
				pctrl->lvds_cfg.phy_vswing = ioctl_phy_usr.vswing_level;
				pctrl->lvds_cfg.phy_preem  = ioctl_phy_usr.preem_level;
				break;
			case LCD_VBYONE:
				pctrl->vbyone_cfg.phy_vswing = ioctl_phy_usr.vswing_level;
				pctrl->vbyone_cfg.phy_preem  = ioctl_phy_usr.preem_level;
				break;
			case LCD_MLVDS:
				pctrl->mlvds_cfg.phy_vswing = ioctl_phy_usr.vswing_level;
				pctrl->mlvds_cfg.phy_preem  = ioctl_phy_usr.preem_level;
				break;
			case LCD_P2P:
				pctrl->p2p_cfg.phy_vswing = ioctl_phy_usr.vswing_level;
				pctrl->p2p_cfg.phy_preem  = ioctl_phy_usr.preem_level;
				break;
			case LCD_EDP:
				pctrl->edp_cfg.phy_vswing_preset = ioctl_phy_usr.vswing_level;
				pctrl->edp_cfg.phy_preem_preset  = ioctl_phy_usr.preem_level;
				break;
			default:
				LCDERR("%s: not support lcd_type: %s\n",
				       __func__,
				       lcd_type_type_to_str(pdrv->config.basic.lcd_type));
				return -EINVAL;
			}
			phy_cfg->flag = ioctl_phy_usr.flag;
			phy_cfg->vswing = ioctl_phy_usr.vswing;
			phy_cfg->vcm = ioctl_phy_usr.vcm;
			phy_cfg->odt = ioctl_phy_usr.odt;
			phy_cfg->ref_bias = ioctl_phy_usr.ref_bias;
			phy_cfg->mode = ioctl_phy_usr.mode;
			phy_cfg->weakly_pull_down = ioctl_phy_usr.weakly_pull_down;
			phy_cfg->lane_num = ioctl_phy_usr.lane_num;
			phy_cfg->ext_pullup = ioctl_phy_usr.ext_pullup;
			phy_cfg->vswing_level = ioctl_phy_usr.vswing_level;
			phy_cfg->preem_level = ioctl_phy_usr.preem_level;
			phy_cfg->vswing =
				lcd_phy_vswing_level_to_value(pdrv, phy_cfg->vswing_level);
			temp = lcd_phy_preem_level_to_value(pdrv, phy_cfg->preem_level);
			lane_num = phy_cfg->lane_num > CH_LANE_MAX ?
				CH_LANE_MAX : phy_cfg->lane_num;
			for (i = 0; i < lane_num; i++) {
				phy_cfg->lane[i].preem = temp;
				phy_cfg->lane[i].amp = ioctl_phy_usr.ioctl_lane[i].amp;
			}
		}

		if (pdrv->status & LCD_STATUS_IF_ON)
			lcd_phy_set(pdrv, 1);

		break;
	case LCD_IOC_GET_SS:
		lcd_get_ss_num(pdrv, &ss_ctl.level, &ss_ctl.freq, &ss_ctl.mode);

		if (copy_to_user(argp, (const void *)&ss_ctl, sizeof(struct aml_lcd_ss_ctl_s)))
			ret = -EFAULT;

		break;
	case LCD_IOC_SET_SS:
		if (copy_from_user((void *)&ss_ctl, argp, sizeof(struct aml_lcd_ss_ctl_s))) {
			ret = -EFAULT;
			break;
		}

		if (ss_ctl.level != 0xffffffff)
			ss_level = ss_ctl.level & 0xff;
		if (ss_ctl.freq != 0xffffffff)
			ss_freq  = ss_ctl.freq & 0xff;
		if (ss_ctl.mode != 0xffffffff)
			ss_mode  = ss_ctl.mode & 0xff;
		ret = lcd_set_ss(pdrv, ss_level, ss_freq, ss_mode);
		if (ret == 0) {
			if (ss_level <= 0xff)
				pdrv->config.timing.ss_level = ss_level;
			if (ss_freq <= 0xff)
				pdrv->config.timing.ss_freq = ss_freq;
			if (ss_mode <= 0xff)
				pdrv->config.timing.ss_mode = ss_mode;
		}

		break;
	default:
		LCDERR("[%d]: not support ioctl cmd_nr: 0x%x\n",
		       pdrv->index, mcd_nr);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long lcd_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = lcd_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations lcd_fops = {
	.owner          = THIS_MODULE,
	.open           = lcd_io_open,
	.release        = lcd_io_release,
	.unlocked_ioctl = lcd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = lcd_compat_ioctl,
#endif
};

static int lcd_cdev_add(struct aml_lcd_drv_s *pdrv, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!pdrv) {
		LCDERR("%s: pdrv is null\n", __func__);
		return -1;
	}
	if (!lcd_cdev) {
		ret = 1;
		goto lcd_cdev_add_failed;
	}

	devno = MKDEV(MAJOR(lcd_cdev->devno), pdrv->index);

	cdev_init(&pdrv->cdev, &lcd_fops);
	pdrv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&pdrv->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto lcd_cdev_add_failed;
	}

	pdrv->dev = device_create(lcd_cdev->class, parent,
				  devno, NULL, "lcd%d", pdrv->index);
	if (IS_ERR_OR_NULL(pdrv->dev)) {
		ret = 3;
		goto lcd_cdev_add_failed1;
	}

	dev_set_drvdata(pdrv->dev, pdrv);
	pdrv->dev->of_node = parent->of_node;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s OK\n", pdrv->index, __func__);
	return 0;

lcd_cdev_add_failed1:
	cdev_del(&pdrv->cdev);
lcd_cdev_add_failed:
	LCDERR("[%d]: %s: failed: %d\n", pdrv->index, __func__, ret);
	return -1;
}

static void lcd_cdev_remove(struct aml_lcd_drv_s *pdrv)
{
	dev_t devno;

	if (!lcd_cdev || !pdrv)
		return;

	devno = MKDEV(MAJOR(lcd_cdev->devno), pdrv->index);
	device_destroy(lcd_cdev->class, devno);
	cdev_del(&pdrv->cdev);
}

static int lcd_global_init_once(void)
{
	int ret;

	if (lcd_global_init_flag) {
		lcd_global_init_flag++;
		return 0;
	}
	lcd_global_init_flag++;

	lcd_debug_print_flag = lcd_debug_ctrl_config.debug_print_flag;

	mutex_init(&lcd_vout_mutex);
	mutex_init(&lcd_power_mutex);
	spin_lock_init(&lcd_reg_spinlock);
	lcd_clk_init();

	lcd_notifier_init();
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	lcd_extern_init();
#endif

	/* init workqueue */
	lcd_workqueue = create_workqueue("lcd_work_queue");
	if (!lcd_workqueue) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDERR("can't create lcd workqueue\n");
	}

	lcd_cdev = kzalloc(sizeof(*lcd_cdev), GFP_KERNEL);
	if (!lcd_cdev)
		return -1;

	ret = alloc_chrdev_region(&lcd_cdev->devno, 0,
				  LCD_MAX_DRV, LCD_CDEV_NAME);
	if (ret) {
		ret = 1;
		goto lcd_cdev_init_once_err;
	}

	lcd_cdev->class = class_create(THIS_MODULE, "aml_lcd");
	if (IS_ERR_OR_NULL(lcd_cdev->class)) {
		ret = 2;
		goto lcd_cdev_init_once_err_1;
	}

	return 0;

lcd_cdev_init_once_err_1:
	unregister_chrdev_region(lcd_cdev->devno, LCD_MAX_DRV);
lcd_cdev_init_once_err:
	kfree(lcd_cdev);
	lcd_cdev = NULL;
	LCDERR("%s: failed: %d\n", __func__, ret);
	return -1;
}

static void lcd_global_remove_once(void)
{
	if (lcd_global_init_flag > 1) {
		lcd_global_init_flag--;
		return;
	}
	lcd_global_init_flag--;

	lcd_notifier_remove();

	if (!lcd_cdev)
		return;

	class_destroy(lcd_cdev->class);
	unregister_chrdev_region(lcd_cdev->devno, LCD_MAX_DRV);
	kfree(lcd_cdev);
	lcd_cdev = NULL;
}

/* ************************************************************* */
static int lcd_vsync_irq_init(struct aml_lcd_drv_s *pdrv)
{
	init_completion(&pdrv->vsync_done);

	if (pdrv->res_vsync_irq[0]) {
		snprintf(pdrv->vsync_isr_name[0], 15, "lcd%d_vsync", pdrv->index);
		if (request_irq(pdrv->res_vsync_irq[0]->start,
				lcd_vsync_isr, IRQF_SHARED,
				pdrv->vsync_isr_name[0], (void *)pdrv)) {
			LCDERR("[%d]: can't request %s\n",
			       pdrv->index, pdrv->vsync_isr_name[0]);
		} else {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: request %s successful\n",
				      pdrv->index, pdrv->vsync_isr_name[0]);
			}
		}
	}

	if (pdrv->res_vsync_irq[1]) {
		snprintf(pdrv->vsync_isr_name[1], 15, "lcd%d_vsync2", pdrv->index);
		if (request_irq(pdrv->res_vsync_irq[1]->start,
				lcd_vsync2_isr, IRQF_SHARED,
				pdrv->vsync_isr_name[1], (void *)pdrv)) {
			LCDERR("[%d]: can't request %s\n",
			       pdrv->index, pdrv->vsync_isr_name[1]);
		} else {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: request %s successful\n",
				      pdrv->index, pdrv->vsync_isr_name[1]);
			}
		}
	}

	if (pdrv->res_vsync_irq[2]) {
		snprintf(pdrv->vsync_isr_name[2], 15, "lcd%d_vsync3", pdrv->index);
		if (request_irq(pdrv->res_vsync_irq[2]->start,
				lcd_vsync3_isr, IRQF_SHARED,
				pdrv->vsync_isr_name[2], (void *)pdrv)) {
			LCDERR("[%d]: can't request %s\n",
			       pdrv->index, pdrv->vsync_isr_name[2]);
		} else {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("[%d]: request %s successful\n",
				      pdrv->index, pdrv->vsync_isr_name[2]);
			}
		}
	}

	/* add timer to monitor hpll frequency */
	timer_setup(&pdrv->vs_none_timer, &lcd_vsync_none_timer_handler, 0);
	/* pdrv->vs_none_timer.data = NULL; */
	pdrv->vs_none_timer.expires = jiffies + LCD_VSYNC_NONE_INTERVAL;
	/*add_timer(&pdrv->vs_none_timer);*/
	/*LCDPR("add vs_none_timer handler\n"); */

	return 0;
}

static void lcd_vsync_irq_remove(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->res_vsync_irq[0])
		free_irq(pdrv->res_vsync_irq[0]->start, (void *)pdrv);
	if (pdrv->res_vsync_irq[1])
		free_irq(pdrv->res_vsync_irq[1]->start, (void *)pdrv);
	if (pdrv->res_vsync_irq[2])
		free_irq(pdrv->res_vsync_irq[2]->start, (void *)pdrv);

	if (pdrv->vsync_none_timer_flag) {
		del_timer_sync(&pdrv->vs_none_timer);
		pdrv->vsync_none_timer_flag = 0;
	}
}

static void lcd_init_vout(struct aml_lcd_drv_s *pdrv)
{
	switch (pdrv->mode) {
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_vout_server_init(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_vout_server_init(pdrv);
		break;
#endif
	default:
		LCDERR("[%d]: invalid lcd mode: %d\n",
		       pdrv->index, pdrv->mode);
		break;
	}
}

static int lcd_mode_init(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv)
		return -1;

	switch (pdrv->mode) {
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MODE_TV:
		lcd_mode_tv_init(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_mode_tablet_init(pdrv);
		break;
#endif
	default:
		LCDERR("[%d]: invalid lcd mode: %d\n",
		       pdrv->index, pdrv->mode);
		break;
	}

	return 0;
}

static int lcd_mode_probe(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	if (!pdrv)
		return -1;

	ret = lcd_get_config(pdrv);
	if (ret)
		return -1;
	/* must behind lcd_get_config, for phy will probe by interface type */
	lcd_phy_probe(pdrv);
	lcd_debug_probe(pdrv);
	lcd_mode_init(pdrv);
	pdrv->probe_done = 1;

	lcd_vsync_irq_init(pdrv);

	if (pdrv->init_flag) {
		LCDPR("[%d]: power on for init_flag\n", pdrv->index);
		mutex_lock(&lcd_power_mutex);
		aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_ON, (void *)pdrv);
		lcd_if_enable_retry(pdrv);
		mutex_unlock(&lcd_power_mutex);
	}

	/* add notifier for video sync_duration info refresh */
	lcd_vout_notify_mode_change(pdrv);

	if (pdrv->auto_test)
		lcd_auto_test_func(pdrv);

	lcd_drm_add(pdrv->dev);

	return 0;
}

static int lcd_config_remove(struct aml_lcd_drv_s *pdrv)
{
	switch (pdrv->mode) {
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_vout_server_remove(pdrv);
		lcd_mode_tv_remove(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_vout_server_remove(pdrv);
		lcd_mode_tablet_remove(pdrv);
		break;
#endif
	default:
		LCDPR("[%d]: invalid lcd mode\n", pdrv->index);
		break;
	}

	lcd_clk_config_remove(pdrv);

	return 0;
}

static void lcd_vout_server_remove(struct aml_lcd_drv_s *pdrv)
{
	switch (pdrv->mode) {
#ifdef CONFIG_AMLOGIC_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_vout_server_remove(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_vout_server_remove(pdrv);
		break;
#endif
	default:
		LCDPR("[%d]: %s: invalid lcd mode\n", pdrv->index, __func__);
		break;
	}
}

static void lcd_config_probe_work(struct work_struct *p_work)
{
	struct delayed_work *d_work;
	struct aml_lcd_drv_s *pdrv;
	bool is_init;
	int ret;

	d_work = container_of(p_work, struct delayed_work, work);
	pdrv = container_of(d_work, struct aml_lcd_drv_s, config_probe_dly_work);

	is_init = lcd_unifykey_init_get();
	if (!is_init) {
		if (pdrv->retry_cnt++ < LCD_UNIFYKEY_WAIT_TIMEOUT) {
			lcd_queue_delayed_work(&pdrv->config_probe_dly_work,
				LCD_UNIFYKEY_RETRY_INTERVAL);
			return;
		}
		LCDERR("[%d]: %s: key_init_flag=%d, exit\n", pdrv->index, __func__, is_init);
		goto lcd_config_probe_work_failed;
	}
	LCDPR("[%d]: key_init_flag=%d, retry_cnt=%d\n", pdrv->index, is_init, pdrv->retry_cnt);

	ret = lcd_mode_probe(pdrv);
	if (ret) {
		LCDERR("[%d]: %s: mode_probe failed, exit\n", pdrv->index, __func__);
		goto lcd_config_probe_work_failed;
	}

	return;

lcd_config_probe_work_failed:
	lcd_vout_server_remove(pdrv);
	lcd_driver[pdrv->index] = NULL;
	kfree(pdrv);
}

static void lcd_config_default(struct aml_lcd_drv_s *pdrv)
{
	unsigned int init_state;

	pdrv->init_flag = 0;
	init_state = lcd_get_venc_init_config(pdrv);
	if (init_state) {
		switch (pdrv->boot_ctrl->init_level) {
		case LCD_INIT_LEVEL_NORMAL:
			pdrv->status = LCD_STATUS_ON;
			pdrv->resume_flag = (LCD_RESUME_PREPARE | LCD_RESUME_ENABLE);
			break;
		case LCD_INIT_LEVEL_PWR_OFF:
			pdrv->status = LCD_STATUS_ENCL_ON;
			pdrv->resume_flag = LCD_RESUME_PREPARE;
			break;
		case LCD_INIT_LEVEL_KERNEL_ON:
			pdrv->init_flag = 1;
			pdrv->status = LCD_STATUS_ENCL_ON;
			pdrv->resume_flag = LCD_RESUME_PREPARE;
			break;
		default:
			pdrv->status = LCD_STATUS_ON;
			pdrv->resume_flag = (LCD_RESUME_PREPARE | LCD_RESUME_ENABLE);
			break;
		}
	} else {
		pdrv->status = 0;
		pdrv->resume_flag = 0;
	}
	LCDPR("[%d]: base_fr: %d, status: 0x%x, init_flag: %d\n",
		pdrv->index,
		pdrv->config.timing.dft_timing.frame_rate,
		pdrv->status, pdrv->init_flag);
}

static void lcd_bootup_config_init(struct aml_lcd_drv_s *pdrv)
{
	unsigned int val;

	pdrv->mute_state = 0;
	pdrv->mute_flag = 0;
	pdrv->mute_count = 0;
	pdrv->mute_count_test = 0;
	pdrv->unmute_count_test = 0;
	pdrv->tcon_isr_bypass = 0;
	pdrv->fr_mode = 0;
	pdrv->viu_sel = LCD_VIU_SEL_NONE;
	pdrv->vsync_none_timer_flag = 0;
	pdrv->module_reset = lcd_module_reset;

	pdrv->test_flag = pdrv->debug_ctrl->debug_test_pattern;
	pdrv->test_state = pdrv->test_flag;

	strcpy(pdrv->config.basic.model_name, lcd_panel_name[pdrv->index]);
	pdrv->config.custom_pinmux = pdrv->boot_ctrl->custom_pinmux;

	pdrv->config.basic.lcd_type = pdrv->boot_ctrl->lcd_type;
	pdrv->config.timing.dft_timing.frame_rate = pdrv->boot_ctrl->base_frame_rate;
	pdrv->config.timing.base_timing.frame_rate = pdrv->boot_ctrl->base_frame_rate;
	pdrv->config.timing.act_timing.frame_rate = pdrv->boot_ctrl->base_frame_rate;

	val = pdrv->boot_ctrl->advanced_flag;
	switch (pdrv->config.basic.lcd_type) {
	case LCD_RGB:
		pdrv->config.basic.lcd_bits = pdrv->boot_ctrl->lcd_bits;
		pdrv->config.control.rgb_cfg.de_valid = val & 0x1;
		pdrv->config.control.rgb_cfg.sync_valid = (val >> 1) & 0x1;
		break;
	case LCD_P2P:
		pdrv->config.control.p2p_cfg.p2p_type = val;
		break;
	case LCD_EDP:
		pdrv->config.control.edp_cfg.link_rate = 1 + (val & 0xc0);
		pdrv->config.control.edp_cfg.lane_count = 1 + (val & 0x30);
		pdrv->config.control.edp_cfg.timing_idx = val & 0x0f;
		break;
	default:
		break;
	}
}

static int lcd_config_probe(struct aml_lcd_drv_s *pdrv, struct platform_device *pdev)
{
	int ret = 0;

	lcd_bootup_config_init(pdrv);

	ret = lcd_base_config_load_from_dts(pdrv);
	if (ret)
		return -1;

	pdrv->res_vsync_irq[0] = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync");
	pdrv->res_vsync_irq[1] = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync2");
	pdrv->res_vsync_irq[2] = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync3");
	pdrv->res_vx1_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vbyone");
	pdrv->res_tcon_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "tcon");

	lcd_clk_config_probe(pdrv);
	lcd_phy_config_init(pdrv);
	lcd_venc_probe(pdrv);
	lcd_config_default(pdrv);
	lcd_init_vout(pdrv);

	/* lock pinmux as earlier as possible if lcd in on */
	if (pdrv->status & LCD_STATUS_IF_ON) {
		switch (pdrv->config.basic.lcd_type) {
		case LCD_RGB:
			lcd_rgb_pinmux_set(pdrv, 1);
			break;
		case LCD_BT656:
		case LCD_BT1120:
			lcd_bt_pinmux_set(pdrv, 1);
			break;
		case LCD_VBYONE:
			lcd_vbyone_pinmux_set(pdrv, 1);
			break;
		case LCD_MLVDS:
			lcd_mlvds_pinmux_set(pdrv, 1);
			break;
		case LCD_P2P:
			lcd_p2p_pinmux_set(pdrv, 1);
			break;
		case LCD_EDP:
			lcd_edp_pinmux_set(pdrv, 1);
			break;
		default:
			break;
		}
	}

	if (pdrv->key_valid) {
		lcd_queue_delayed_work(&pdrv->config_probe_dly_work, 0);
	} else {
		ret = lcd_mode_probe(pdrv);
		if (ret) {
			lcd_vout_server_remove(pdrv);
			LCDERR("[%d]: probe exit\n", pdrv->index);
			return -1;
		}
	}

	return 0;
}

#ifdef CONFIG_OF
static struct lcd_data_s lcd_data_axg = {
	.chip_type = LCD_CHIP_AXG,
	.chip_name = "axg",
	.reg_map_table = &lcd_reg_g12a[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_g12a = {
	.chip_type = LCD_CHIP_G12A,
	.chip_name = "g12a",
	.reg_map_table = &lcd_reg_g12a[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_g12b = {
	.chip_type = LCD_CHIP_G12B,
	.chip_name = "g12b",
	.reg_map_table = &lcd_reg_g12a[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_tl1 = {
	.chip_type = LCD_CHIP_TL1,
	.chip_name = "tl1",
	.reg_map_table = &lcd_reg_tl1[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_sm1 = {
	.chip_type = LCD_CHIP_SM1,
	.chip_name = "sm1",
	.reg_map_table = &lcd_reg_g12a[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_tm2 = {
	.chip_type = LCD_CHIP_TM2,
	.chip_name = "tm2",
	.reg_map_table = &lcd_reg_tl1[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_t5 = {
	.chip_type = LCD_CHIP_T5,
	.chip_name = "t5",
	.reg_map_table = &lcd_reg_t5[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_t5d = {
	.chip_type = LCD_CHIP_T5D,
	.chip_name = "t5d",
	.reg_map_table = &lcd_reg_t5[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_t7 = {
	.chip_type = LCD_CHIP_T7,
	.chip_name = "t7",
	.reg_map_table = &lcd_reg_t7[0],
	.drv_max = 3,
	.offset_venc = {0x0, 0x600, 0x800},
	.offset_venc_if = {0x0, 0x500, 0x600},
	.offset_venc_data = {0x0, 0x100, 0x200},
};

static struct lcd_data_s lcd_data_t3 = {
	.chip_type = LCD_CHIP_T3,
	.chip_name = "t3",
	.reg_map_table = &lcd_reg_t5[0],
	.drv_max = 2,
	.offset_venc = {0x0, 0x600, 0x0},
	.offset_venc_if = {0x0, 0x500, 0x0},
	.offset_venc_data = {0x0, 0x100, 0x0},
};

static struct lcd_data_s lcd_data_t5w = {
	.chip_type = LCD_CHIP_T5W,
	.chip_name = "t5w",
	.reg_map_table = &lcd_reg_t5[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_a4 = {
	.chip_type = LCD_CHIP_A4,
	.chip_name = "a4",
	.reg_map_table = &lcd_reg_a4[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static const struct of_device_id lcd_dt_match_table[] = {
	{
		.compatible = "amlogic, lcd-axg",
		.data = &lcd_data_axg,
	},
	{
		.compatible = "amlogic, lcd-g12a",
		.data = &lcd_data_g12a,
	},
	{
		.compatible = "amlogic, lcd-g12b",
		.data = &lcd_data_g12b,
	},
	{
		.compatible = "amlogic, lcd-tl1",
		.data = &lcd_data_tl1,
	},
	{
		.compatible = "amlogic, lcd-sm1",
		.data = &lcd_data_sm1,
	},
	{
		.compatible = "amlogic, lcd-tm2",
		.data = &lcd_data_tm2,
	},
	{
		.compatible = "amlogic, lcd-t5",
		.data = &lcd_data_t5,
	},
	{
		.compatible = "amlogic, lcd-t5d",
		.data = &lcd_data_t5d,
	},
	{
		.compatible = "amlogic, lcd-t7",
		.data = &lcd_data_t7,
	},
	{
		.compatible = "amlogic, lcd-t3",
		.data = &lcd_data_t3,
	},
	{
		.compatible = "amlogic, lcd-t5w",
		.data = &lcd_data_t5w,
	},
	{
		.compatible = "amlogic, lcd-a4",
		.data = &lcd_data_a4,
	},
	{}
};
#endif

static int lcd_probe(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *pdrv;
	const struct of_device_id *match;
	struct lcd_data_s *pdata;
	unsigned int index = 0;
	int ret = 0;

	lcd_global_init_once();

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: no index exist, default to 0\n", __func__);
		index = 0;
	}
	if (index >= LCD_MAX_DRV) {
		LCDERR("%s: invalid index %d\n", __func__, index);
		return -1;
	}
	if (lcd_drv_init_state & (1 << index)) {
		LCDERR("%s: index %d driver already registered\n",
		       __func__, index);
		return -1;
	}
	lcd_drv_init_state |= (1 << index);

	match = of_match_device(lcd_dt_match_table, &pdev->dev);
	if (!match) {
		LCDERR("%s: no match table\n", __func__);
		return -1;
	}
	pdata = (struct lcd_data_s *)match->data;
	LCDPR("[%d]: driver version: %s(%d-%s)\n",
	      index,
	      LCD_DRV_VERSION,
	      pdata->chip_type,
	      pdata->chip_name);
	if (index >= pdata->drv_max) {
		LCDERR("[%d]: %s: invalid index\n", index, __func__);
		return -1;
	}

	pdrv = lcd_driver_add(index);
	if (!pdrv)
		goto lcd_probe_err_0;
	/* set drvdata */
	lcd_driver[index] = pdrv;
	pdrv->data = pdata;
	//pdrv->of_node = pdev->dev.of_node;
	platform_set_drvdata(pdev, pdrv);

#ifdef CONFIG_AMLOGIC_VPU
	/*vpu dev register for lcd*/
	pdrv->lcd_vpu_dev = vpu_dev_register(VPU_VENCL, LCD_CDEV_NAME);
#endif

	ret = lcd_ioremap(pdrv, pdev);
	if (ret)
		goto lcd_probe_err_1;

	spin_lock_init(&pdrv->isr_lock);
	INIT_DELAYED_WORK(&pdrv->config_probe_dly_work, lcd_config_probe_work);
	INIT_WORK(&pdrv->late_resume_work, lcd_lata_resume_work);
	INIT_WORK(&pdrv->screen_restore_work, lcd_screen_restore_work);
	INIT_DELAYED_WORK(&pdrv->test_delayed_work, lcd_auto_test_delayed);

	ret = lcd_cdev_add(pdrv, &pdev->dev);
	if (ret)
		goto lcd_probe_err_2;
	ret = lcd_config_probe(pdrv, pdev);
	if (ret)
		goto lcd_probe_err_2;

	LCDPR("[%d]: %s ok, init_state:0x%x\n", index, __func__, lcd_drv_init_state);

	return 0;

lcd_probe_err_2:
	lcd_cdev_remove(pdrv);
lcd_probe_err_1:
	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	lcd_driver[index] = NULL;
	/* free drv */
	kfree(pdrv);
lcd_probe_err_0:
	lcd_drv_init_state &= ~(1 << index);
	LCDPR("[%d]: %s failed\n", index, __func__);
	return ret;
}

static int lcd_remove(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *pdrv = platform_get_drvdata(pdev);
	int index;

	if (!pdrv)
		return 0;

	lcd_drm_remove(pdrv->dev);

	index = pdrv->index;

	cancel_work_sync(&pdrv->late_resume_work);
	cancel_work_sync(&pdrv->screen_restore_work);
	cancel_delayed_work(&pdrv->config_probe_dly_work);
	if (lcd_workqueue)
		destroy_workqueue(lcd_workqueue);

	lcd_vsync_irq_remove(pdrv);
	lcd_cdev_remove(pdrv);
	lcd_debug_remove(pdrv);
	lcd_config_remove(pdrv);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);

	kfree(pdrv->reg_map);
	kfree(pdrv);
	lcd_driver[index] = NULL;
	lcd_drv_init_state &= ~(1 << index);
	lcd_global_remove_once();

	LCDPR("[%d]: %s, init_state:0x%x\n", index, __func__, lcd_drv_init_state);
	return 0;
}

static int lcd_resume(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *pdrv = platform_get_drvdata(pdev);

	if (!pdrv)
		return 0;

	if ((pdrv->status & LCD_STATUS_VMODE_ACTIVE) == 0)
		return 0;

	mutex_lock(&lcd_power_mutex);
	LCDPR("[%d]: %s\n", pdrv->index, __func__);
	pdrv->resume_flag |= LCD_RESUME_PREPARE;
	aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)pdrv);
	LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	mutex_unlock(&lcd_power_mutex);

	return 0;
}

static int lcd_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct aml_lcd_drv_s *pdrv = platform_get_drvdata(pdev);

	if (!pdrv)
		return 0;

	mutex_lock(&lcd_power_mutex);
	if (pdrv->status & LCD_STATUS_IF_ON)
		LCDERR("[%d]: %s: lcd interface is still enabled!\n", pdrv->index, __func__);

	pdrv->resume_flag &= ~LCD_RESUME_PREPARE;
	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		aml_lcd_notifier_call_chain(LCD_EVENT_UNPREPARE, (void *)pdrv);
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	}
	mutex_unlock(&lcd_power_mutex);
	return 0;
}

static void lcd_shutdown(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *pdrv = platform_get_drvdata(pdev);

	if (!pdrv)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);

	if (pdrv->status & LCD_STATUS_ENCL_ON)
		aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, (void *)pdrv);
}

static struct platform_driver lcd_platform_driver = {
	.probe = lcd_probe,
	.remove = lcd_remove,
	.suspend = lcd_suspend,
	.resume = lcd_resume,
	.shutdown = lcd_shutdown,
	.driver = {
		.name = "mesonlcd",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(lcd_dt_match_table),
#endif
	},
};

int __init lcd_init(void)
{
	if (platform_driver_register(&lcd_platform_driver)) {
		LCDERR("failed to register lcd driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit lcd_exit(void)
{
	platform_driver_unregister(&lcd_platform_driver);
}

static int lcd_panel_name_para_setup(char *str)
{
	if (str)
		sprintf(lcd_panel_name[0], "%s", str);

	LCDPR("panel_name: %s\n", lcd_panel_name[0]);
	return 0;
}

static int lcd1_panel_name_para_setup(char *str)
{
	if (str)
		sprintf(lcd_panel_name[1], "%s", str);

	LCDPR("panel_name: %s\n", lcd_panel_name[1]);
	return 0;
}

static int lcd2_panel_name_para_setup(char *str)
{
	if (str)
		sprintf(lcd_panel_name[2], "%s", str);

	LCDPR("panel_name: %s\n", lcd_panel_name[2]);
	return 0;
}

static int lcd_panel_type_para_setup(char *str)
{
	if (str)
		sprintf(lcd_propname[0], "%s", str);

	LCDPR("panel_type: %s\n", lcd_propname[0]);
	return 0;
}

static int lcd1_panel_type_para_setup(char *str)
{
	if (str)
		sprintf(lcd_propname[1], "%s", str);

	LCDPR("panel1_type: %s\n", lcd_propname[1]);
	return 0;
}

static int lcd2_panel_type_para_setup(char *str)
{
	if (str)
		sprintf(lcd_propname[2], "%s", str);

	LCDPR("panel2_type: %s\n", lcd_propname[2]);
	return 0;
}

static int lcd_boot_ctrl_setup(char *str)
{
	int ret = 0;
	unsigned int data32 = 0;
	struct lcd_boot_ctrl_s *boot_ctrl = &lcd_boot_ctrl_config[0];

	if (!str)
		return -EINVAL;

	ret = kstrtouint(str, 16, &data32);
	if (ret) {
		LCDERR("%s:invalid data\n", __func__);
		return -EINVAL;
	}

	LCDPR("lcd_ctrl: 0x%08x\n", data32);
	boot_ctrl->lcd_type = data32 & 0xf;
	boot_ctrl->lcd_bits = (data32 >> 4) & 0xf;
	boot_ctrl->advanced_flag = (data32 >> 8) & 0xff;
	boot_ctrl->custom_pinmux = (data32 >> 16) & 0x1;
	boot_ctrl->init_level = (data32 >> 18) & 0x3;
	boot_ctrl->base_frame_rate = (data32 >> 24) & 0xff;
	return 0;
}

static int lcd1_boot_ctrl_setup(char *str)
{
	int ret = 0;
	unsigned int data32 = 0;
	struct lcd_boot_ctrl_s *boot_ctrl = &lcd_boot_ctrl_config[1];

	if (!str)
		return -EINVAL;

	ret = kstrtouint(str, 16, &data32);
	if (ret) {
		LCDERR("%s:invalid data\n", __func__);
		return -EINVAL;
	}

	LCDPR("lcd1_ctrl: 0x%08x\n", data32);
	boot_ctrl->lcd_type = data32 & 0xf;
	boot_ctrl->lcd_bits = (data32 >> 4) & 0xf;
	boot_ctrl->advanced_flag = (data32 >> 8) & 0xff;
	boot_ctrl->custom_pinmux = (data32 >> 16) & 0x1;
	boot_ctrl->init_level = (data32 >> 18) & 0x3;
	boot_ctrl->base_frame_rate = (data32 >> 24) & 0xff;
	return 0;
}

static int lcd2_boot_ctrl_setup(char *str)
{
	int ret = 0;
	unsigned int data32 = 0;
	struct lcd_boot_ctrl_s *boot_ctrl = &lcd_boot_ctrl_config[2];

	if (!str)
		return -EINVAL;

	ret = kstrtouint(str, 16, &data32);
	if (ret) {
		LCDERR("%s:invalid data\n", __func__);
		return -EINVAL;
	}

	LCDPR("lcd2_ctrl: 0x%08x\n", data32);
	boot_ctrl->lcd_type = data32 & 0xf;
	boot_ctrl->lcd_bits = (data32 >> 4) & 0xf;
	boot_ctrl->advanced_flag = (data32 >> 8) & 0xff;
	boot_ctrl->custom_pinmux = (data32 >> 16) & 0x1;
	boot_ctrl->init_level = (data32 >> 18) & 0x3;
	boot_ctrl->base_frame_rate = (data32 >> 24) & 0xff;
	return 0;
}

static int lcd_debug_ctrl_setup(char *str)
{
	int ret = 0;
	unsigned int data32 = 0;
	struct lcd_debug_ctrl_s *debug_ctrl = &lcd_debug_ctrl_config;

	if (!str)
		return -EINVAL;

	ret = kstrtouint(str, 16, &data32);
	if (ret) {
		LCDERR("%s:invalid data\n", __func__);
		return -EINVAL;
	}

	LCDPR("debug_ctrl: 0x%08x\n", data32);
	debug_ctrl->debug_print_flag = data32 & 0xff;
	debug_ctrl->debug_test_pattern = (data32 >> 8) & 0xff;
	debug_ctrl->debug_para_source = (data32 >> 28) & 0x3;
	debug_ctrl->debug_lcd_mode = (data32 >> 30) & 0x3;
	return 0;
}

__setup("panel_name=", lcd_panel_name_para_setup);
__setup("panel1_name=", lcd1_panel_name_para_setup);
__setup("panel2_name=", lcd2_panel_name_para_setup);
__setup("panel_type=", lcd_panel_type_para_setup);
__setup("panel1_type=", lcd1_panel_type_para_setup);
__setup("panel2_type=", lcd2_panel_type_para_setup);
__setup("lcd_ctrl=", lcd_boot_ctrl_setup);
__setup("lcd1_ctrl=", lcd1_boot_ctrl_setup);
__setup("lcd2_ctrl=", lcd2_boot_ctrl_setup);
__setup("lcd_debug=", lcd_debug_ctrl_setup);

//MODULE_DESCRIPTION("Meson LCD Panel Driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Amlogic, Inc.");
