// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/pwm.h>
#include <linux/amlogic/pwm-meson.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_bl.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

struct bl_pwm_init_config_s {
	unsigned int *pwm_vs_reg;
	unsigned int pwm_vs_reg_cnt;
	unsigned int pwm_vs_flag;
};

static struct bl_pwm_init_config_s pwm_init_cfg = {
	.pwm_vs_reg = NULL,
	.pwm_vs_reg_cnt = 0,
	.pwm_vs_flag = 0,
};

static char *bl_pwm_name[] = {
	"PWM_A",
	"PWM_B",
	"PWM_C",
	"PWM_D",
	"PWM_E",
	"PWM_F",
	"PWM_G",
	"PWM_H",
	"PWM_I",
	"PWM_J",
	"PWM_K",
	"PWM_L",
	"PWM_M",
	"PWM_N"
};

static char *bl_pwm_ao_name[] = {
	"PWM_AO_A",
	"PWM_AO_B",
	"PWM_AO_C",
	"PWM_AO_D",
	"PWM_AO_E",
	"PWM_AO_F",
	"PWM_AO_G",
	"PWM_AO_H"
};

static char *bl_pwm_vs_name[] = {"PWM_VS"};
static char *bl_pwm_invalid_name[] = {"invalid"};

enum bl_pwm_port_e bl_pwm_str_to_num(const char *str)
{
	enum bl_pwm_port_e pwm_port = BL_PWM_MAX;
	int i, cnt;

	cnt = ARRAY_SIZE(bl_pwm_name);
	for (i = 0; i < cnt; i++) {
		if (strcmp(str, bl_pwm_name[i]) == 0) {
			pwm_port = i + BL_PWM_A;
			return pwm_port;
		}
	}

	cnt = ARRAY_SIZE(bl_pwm_ao_name);
	for (i = 0; i < cnt; i++) {
		if (strcmp(str, bl_pwm_ao_name[i]) == 0) {
			pwm_port = i + BL_PWM_AO_A;
			return pwm_port;
		}
	}

	if (strcmp(str, bl_pwm_vs_name[0]) == 0) {
		pwm_port = BL_PWM_VS;
		return pwm_port;
	}

	return BL_PWM_MAX;
}

char *bl_pwm_num_to_str(unsigned int num)
{
	unsigned int temp, cnt;

	if (num < BL_PWM_AO_A) {
		temp = num - BL_PWM_A;
		cnt = ARRAY_SIZE(bl_pwm_name);
		if (temp >= cnt)
			return bl_pwm_invalid_name[0];
		return bl_pwm_name[temp];
	} else if (num < BL_PWM_VS) {
		temp = num - BL_PWM_AO_A;
		cnt = ARRAY_SIZE(bl_pwm_ao_name);
		if (temp >= cnt)
			return bl_pwm_invalid_name[0];
		return bl_pwm_ao_name[temp];
	} else if (num == BL_PWM_VS) {
		return bl_pwm_vs_name[0];
	}

	return bl_pwm_invalid_name[0];
}

static unsigned int bl_level_mapping(struct aml_bl_drv_s *bdrv, unsigned int level)
{
	unsigned int mid = bdrv->bconf.level_mid;
	unsigned int mid_map = bdrv->bconf.level_mid_mapping;
	unsigned int max = bdrv->bconf.level_max;
	unsigned int min = bdrv->bconf.level_min;

	if (mid == mid_map)
		return level;

	level = level > max ? max : level;
	if (level >= mid && level <= max) {
		level = (((level - mid) * (max - mid_map)) / (max - mid)) +
			mid_map;
	} else if (level >= min && level < mid) {
		level = (((level - min) * (mid_map - min)) / (mid - min)) + min;
	} else {
		level = 0;
	}
	return level;
}

/* ****************************************************** */
static int bl_pwm_out_level_check(struct bl_pwm_config_s *bl_pwm)
{
	int out_level = 0xff;
	unsigned int pwm_range;

	if (bl_pwm->pwm_duty_max > 255)
		pwm_range = 4095;
	else if (bl_pwm->pwm_duty_max > 100)
		pwm_range = 255;
	else
		pwm_range = 100;

	switch (bl_pwm->pwm_method) {
	case BL_PWM_POSITIVE:
		if (bl_pwm->pwm_duty == 0)
			out_level = 0;
		else if (bl_pwm->pwm_duty == pwm_range)
			out_level = 1;
		else
			out_level = 0xff;
		break;
	case BL_PWM_NEGATIVE:
		if (bl_pwm->pwm_duty == 0)
			out_level = 1;
		else if (bl_pwm->pwm_duty == pwm_range)
			out_level = 0;
		else
			out_level = 0xff;
		break;
	default:
		BLERR("%s: port %d: invalid pwm_method %d\n",
		      __func__, bl_pwm->pwm_port, bl_pwm->pwm_method);
		break;
	}

	return out_level;
}

static void bl_set_pwm_vs(struct bl_pwm_config_s *bl_pwm,
			  unsigned int pol, unsigned int out_level)
{
	unsigned int pwm_hi, n, sw;
	unsigned int reg[5], vs[8], ve[8];
	int i;

	if (bl_pwm->pwm_cnt == 0) {
		BLERR("%s: pwm_cnt is 0\n", __func__);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("%s: pwm_duty=%d, out_level=%d\n",
		     __func__, bl_pwm->pwm_duty, out_level);
	}

	if (out_level == 0) {
		for (i = 0; i < 8; i++) {
			vs[i] = 0x1fff;
			ve[i] = 0;
		}
	} else if (out_level == 1) {
		for (i = 0; i < 8; i++) {
			vs[i] = 0;
			ve[i] = 0x1fff;
		}
	} else {
		pwm_hi = bl_pwm->pwm_level;
		n = bl_pwm->pwm_freq;
		sw = (bl_pwm->pwm_cnt * 10 / n + 5) / 10;
		pwm_hi = (pwm_hi * 10 / n + 5) / 10;
		pwm_hi = (pwm_hi > 1) ? pwm_hi : 1;
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
			BLPR("pwm_vs: n=%d, sw=%d, pwm_high=%d\n",
			     n, sw, pwm_hi);
		}
		for (i = 0; i < n; i++) {
			vs[i] = 1 + (sw * i);
			ve[i] = vs[i] + pwm_hi - 1;
		}
		for (i = n; i < 8; i++) {
			vs[i] = 0x1fff;
			ve[i] = 0x1fff;
		}
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		for (i = 0; i < 8; i++) {
			BLPR("pwm_vs: vs[%d]=%d, ve[%d]=%d\n",
			     i, vs[i], i, ve[i]);
		}
	}

	if (pwm_init_cfg.pwm_vs_reg_cnt < 5) {
		BLERR("%s: pwm_vs reg error\n", __func__);
		return;
	}
	for (i = 0; i < pwm_init_cfg.pwm_vs_reg_cnt; i++)
		reg[i] = pwm_init_cfg.pwm_vs_reg[i];

	lcd_vcbus_write(reg[0], (pol << 31) |
		       (2 << 14) | /* vsync latch */
		       (ve[0] << 16) | (vs[0]));
	lcd_vcbus_write(reg[1], (ve[1] << 16) | (vs[1]));
	lcd_vcbus_write(reg[2], (ve[2] << 16) | (vs[2]));
	lcd_vcbus_write(reg[3], (ve[3] << 16) | (vs[3]));
	if (pwm_init_cfg.pwm_vs_flag) {
		lcd_vcbus_setb(reg[4], 1, 31, 1);
		lcd_vcbus_setb(reg[0], vs[4], 0, 13);
		lcd_vcbus_setb(reg[0], ve[4], 16, 13);
		lcd_vcbus_write(reg[1], (ve[5] << 16) | (vs[5]));
		lcd_vcbus_write(reg[2], (ve[6] << 16) | (vs[6]));
		lcd_vcbus_write(reg[3], (ve[7] << 16) | (vs[7]));
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
			BLPR("VPU_VPU_PWM_V4=0x%08x\n", lcd_vcbus_read(reg[0]));
			BLPR("VPU_VPU_PWM_V5=0x%08x\n", lcd_vcbus_read(reg[1]));
			BLPR("VPU_VPU_PWM_V6=0x%08x\n", lcd_vcbus_read(reg[2]));
			BLPR("VPU_VPU_PWM_V7=0x%08x\n", lcd_vcbus_read(reg[3]));
		}
		lcd_vcbus_setb(reg[4], 0, 31, 1);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("VPU_VPU_PWM_V0=0x%08x\n", lcd_vcbus_read(reg[0]));
		BLPR("VPU_VPU_PWM_V1=0x%08x\n", lcd_vcbus_read(reg[1]));
		BLPR("VPU_VPU_PWM_V2=0x%08x\n", lcd_vcbus_read(reg[2]));
		BLPR("VPU_VPU_PWM_V3=0x%08x\n", lcd_vcbus_read(reg[3]));
	}
}

static inline void bl_pwm_normal_state_print(struct pwm_state *state)
{
	BLPR("pwm state: polarity=%d, duty_cycle=%lld, period=%lld, enabled=%d\n",
	     state->polarity, state->duty_cycle,
	     state->period, state->enabled);
}

static void bl_set_pwm_normal(struct bl_pwm_config_s *bl_pwm,
			      unsigned int pol, unsigned int out_level)
{
	if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
		BLERR("%s: invalid bl_pwm_ch\n", __func__);
		return;
	}
	if (bl_pwm->pwm_cnt == 0) {
		BLERR("%s: pwm_cnt is 0\n", __func__);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("pwm: pwm=0x%p, port=%d, meson_index=%d\n",
		     bl_pwm->pwm_data.pwm, bl_pwm->pwm_port,
		     bl_pwm->pwm_data.meson_index);
	}

	bl_pwm->pwm_data.state.polarity = pol;
	bl_pwm->pwm_data.state.duty_cycle = bl_pwm->pwm_level;
	bl_pwm->pwm_data.state.period =  bl_pwm->pwm_cnt;
	bl_pwm->pwm_data.state.enabled = true;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
		bl_pwm_normal_state_print(&bl_pwm->pwm_data.state);

	if (out_level == 0xff) {
		pwm_constant_disable(bl_pwm->pwm_data.meson,
				     bl_pwm->pwm_data.meson_index);
	} else {
		/* pwm duty 100% or 0% special control */
		pwm_constant_enable(bl_pwm->pwm_data.meson,
				    bl_pwm->pwm_data.meson_index);
	}
	pwm_apply_state(bl_pwm->pwm_data.pwm, &bl_pwm->pwm_data.state);
}

void bl_pwm_ctrl(struct bl_pwm_config_s *bl_pwm, int status)
{
	struct pwm_state pstate;
	unsigned int pol = 0, out_level = 0xff;

	if (bl_pwm->pwm_method == BL_PWM_NEGATIVE)
		pol = 1;

	if (status) {
		/* enable pwm */
		out_level = bl_pwm_out_level_check(bl_pwm);
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
			BLPR("port %d: pwm_duty=%d, out_level=%d, pol=%s\n",
			     bl_pwm->pwm_port, bl_pwm->pwm_duty, out_level,
			     (pol ? "negative" : "positive"));
		}

		switch (bl_pwm->pwm_port) {
		case BL_PWM_A:
		case BL_PWM_B:
		case BL_PWM_C:
		case BL_PWM_D:
		case BL_PWM_E:
		case BL_PWM_F:
		case BL_PWM_G:
		case BL_PWM_H:
		case BL_PWM_I:
		case BL_PWM_J:
		case BL_PWM_K:
		case BL_PWM_L:
		case BL_PWM_M:
		case BL_PWM_N:
		case BL_PWM_AO_A:
		case BL_PWM_AO_B:
		case BL_PWM_AO_C:
		case BL_PWM_AO_D:
		case BL_PWM_AO_E:
		case BL_PWM_AO_F:
		case BL_PWM_AO_G:
		case BL_PWM_AO_H:
			bl_set_pwm_normal(bl_pwm, pol, out_level);
			break;
		case BL_PWM_VS:
			bl_set_pwm_vs(bl_pwm, pol, out_level);
			break;
		default:
			BLERR("%s: invalid pwm_port: 0x%x\n",
			      __func__, bl_pwm->pwm_port);
			break;
		}
	} else {
		/* disable pwm */
		switch (bl_pwm->pwm_port) {
		case BL_PWM_A:
		case BL_PWM_B:
		case BL_PWM_C:
		case BL_PWM_D:
		case BL_PWM_E:
		case BL_PWM_F:
		case BL_PWM_G:
		case BL_PWM_H:
		case BL_PWM_I:
		case BL_PWM_J:
		case BL_PWM_K:
		case BL_PWM_L:
		case BL_PWM_M:
		case BL_PWM_N:
		case BL_PWM_AO_A:
		case BL_PWM_AO_B:
		case BL_PWM_AO_C:
		case BL_PWM_AO_D:
		case BL_PWM_AO_E:
		case BL_PWM_AO_F:
		case BL_PWM_AO_G:
		case BL_PWM_AO_H:
			if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
				BLERR("%s: invalid bl_pwm_ch\n", __func__);
				return;
			}

			pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
			pstate.duty_cycle = 0;
			pstate.enabled = 0;
			pstate.period = bl_pwm->pwm_data.state.period;
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
				bl_pwm_normal_state_print(&pstate);
			pwm_apply_state(bl_pwm->pwm_data.pwm, &(pstate));
			break;
		case BL_PWM_VS:
			bl_set_pwm_vs(bl_pwm, pol, 0);
			break;
		default:
			BLERR("%s: invalid pwm_port: 0x%x\n",
			      __func__, bl_pwm->pwm_port);
			break;
		}
	}
}

static void bl_set_pwm(struct aml_bl_drv_s *bdrv, struct bl_pwm_config_s *bl_pwm)
{
	if ((bdrv->state & BL_STATE_BL_INIT_ON) == 0)
		bl_pwm->pwm_duty_save = bl_pwm->pwm_duty;

	if (bdrv->state & BL_STATE_BL_ON ||
	    bdrv->state & BL_STATE_BL_INIT_ON) {
		bl_pwm_ctrl(bl_pwm, 1);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLERR("%s: bl_drv state is off\n", __func__);
	}
}

void bl_pwm_set_duty(struct aml_bl_drv_s *bdrv, struct bl_pwm_config_s *bl_pwm)
{
	unsigned long long temp;

	if (bdrv->pwm_bypass)
		return;
	if (bl_pwm->pwm_cnt == 0) {
		BLERR("%s: pwm_cnt is 0\n", __func__);
		return;
	}

	if (bdrv->pwm_duty_free) {
		if (bl_pwm->pwm_duty_max > 255) {
			if (bl_pwm->pwm_duty > 4095) {
				BLERR("pwm_duty %d > 4095, reset to 4095\n",
				      bl_pwm->pwm_duty);
				bl_pwm->pwm_duty = 4095;
			}
		} else if (bl_pwm->pwm_duty_max > 100) {
			if (bl_pwm->pwm_duty > 255) {
				BLERR("pwm_duty %d > 255, reset to 255\n",
				      bl_pwm->pwm_duty);
				bl_pwm->pwm_duty = 255;
			}
		} else {
			if (bl_pwm->pwm_duty > 100) {
				BLERR("pwm_duty %d%% > 100%%, reset to 100%%\n",
				      bl_pwm->pwm_duty);
				bl_pwm->pwm_duty = 100;
			}
		}
	} else {
		if (bl_pwm->pwm_duty > bl_pwm->pwm_duty_max) {
			BLERR("pwm_duty %d > duty_max %d, reset to duty_max\n",
			      bl_pwm->pwm_duty, bl_pwm->pwm_duty_max);
			bl_pwm->pwm_duty = bl_pwm->pwm_duty_max;
		} else if (bl_pwm->pwm_duty < bl_pwm->pwm_duty_min) {
			BLERR("pwm_duty %d is < %d, reset to duty_min\n",
			      bl_pwm->pwm_duty, bl_pwm->pwm_duty_min);
			bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
		}
	}

	temp = bl_pwm->pwm_cnt;
	if (bl_pwm->pwm_duty_max > 255) {
		bl_pwm->pwm_level =
			bl_do_div(((temp * bl_pwm->pwm_duty) + 2048), 4095);
	} else if (bl_pwm->pwm_duty_max > 100) {
		bl_pwm->pwm_level =
			bl_do_div(((temp * bl_pwm->pwm_duty) + 127), 255);
	} else {
		bl_pwm->pwm_level =
			bl_do_div(((temp * bl_pwm->pwm_duty) + 50), 100);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("pwm port %d: pwm_max=%d, pwm_min=%d, pwm_level=%d\n",
		     bl_pwm->pwm_port,
		     bl_pwm->pwm_max, bl_pwm->pwm_min, bl_pwm->pwm_level);
		if (bl_pwm->pwm_duty_max > 100) {
			BLPR("duty=%d(%d%%), duty_max=%d, duty_min=%d\n",
			     bl_pwm->pwm_duty,
			     bl_pwm->pwm_duty * 100 / bl_pwm->pwm_duty_max,
			     bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min);
		} else {
			BLPR("duty=%d%%, duty_max=%d%%, duty_min=%d%%\n",
			     bl_pwm->pwm_duty,
			     bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min);
		}
	}
	bl_set_pwm(bdrv, bl_pwm);
}

void bl_pwm_mapping_init(struct aml_bl_drv_s *bdrv)
{
	struct bl_pwm_config_s *bl_pwm;

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		bl_pwm = bdrv->bconf.bl_pwm;
		if (bl_pwm) {
			bl_pwm->pwm_mapping[0] = bl_pwm->level_min;
			bl_pwm->pwm_mapping[1] = bl_pwm->level_min +
				(bl_pwm->level_max - bl_pwm->level_min + 2) / 4;
			bl_pwm->pwm_mapping[2] = bl_pwm->level_min +
				2 * ((bl_pwm->level_max - bl_pwm->level_min + 2) / 4);
			bl_pwm->pwm_mapping[3] = bl_pwm->level_min +
				3 * ((bl_pwm->level_max - bl_pwm->level_min + 2) / 4);
			bl_pwm->pwm_mapping[4] = bl_pwm->level_max;
			bl_pwm->pwm_mapping[5] = bl_pwm->level_max;
			bl_pwm->pwm_mapping[6] = bl_pwm->level_max;
			BLPR("pwm curve: %d  %d  %d  %d  %d tp:%d %d", bl_pwm->pwm_mapping[0],
				bl_pwm->pwm_mapping[1], bl_pwm->pwm_mapping[2],
				bl_pwm->pwm_mapping[3], bl_pwm->pwm_mapping[4],
				bl_pwm->pwm_mapping[5], bl_pwm->pwm_mapping[6]);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		bl_pwm = bdrv->bconf.bl_pwm_combo0;
		if (bl_pwm) {
			bl_pwm->pwm_mapping[0] = bl_pwm->level_min;
			bl_pwm->pwm_mapping[1] = bl_pwm->level_min +
				(bl_pwm->level_max - bl_pwm->level_min + 2) / 4;
			bl_pwm->pwm_mapping[2] = bl_pwm->level_min +
				2 * ((bl_pwm->level_max - bl_pwm->level_min + 2) / 4);
			bl_pwm->pwm_mapping[3] = bl_pwm->level_min +
				3 * ((bl_pwm->level_max - bl_pwm->level_min + 2) / 4);
			bl_pwm->pwm_mapping[4] = bl_pwm->level_max;
			bl_pwm->pwm_mapping[5] = bl_pwm->level_max;
			bl_pwm->pwm_mapping[6] = bl_pwm->level_max;
			BLPR("pwm curve: %d  %d  %d  %d  %d tp:%d %d", bl_pwm->pwm_mapping[0],
				bl_pwm->pwm_mapping[1], bl_pwm->pwm_mapping[2],
				bl_pwm->pwm_mapping[3], bl_pwm->pwm_mapping[4],
				bl_pwm->pwm_mapping[5], bl_pwm->pwm_mapping[6]);
		}

		bl_pwm = bdrv->bconf.bl_pwm_combo1;
		if (bl_pwm) {
			bl_pwm->pwm_mapping[0] = bl_pwm->level_min;
			bl_pwm->pwm_mapping[1] = bl_pwm->level_min +
				(bl_pwm->level_max - bl_pwm->level_min + 2) / 4;
			bl_pwm->pwm_mapping[2] = bl_pwm->level_min +
				2 * ((bl_pwm->level_max - bl_pwm->level_min + 2) / 4);
			bl_pwm->pwm_mapping[3] = bl_pwm->level_min +
				3 * ((bl_pwm->level_max - bl_pwm->level_min + 2) / 4);
			bl_pwm->pwm_mapping[4] = bl_pwm->level_max;
			bl_pwm->pwm_mapping[5] = bl_pwm->level_max;
			bl_pwm->pwm_mapping[6] = bl_pwm->level_max;
			BLPR("pwm curve: %d  %d  %d  %d  %d tp:%d %d", bl_pwm->pwm_mapping[0],
				bl_pwm->pwm_mapping[1], bl_pwm->pwm_mapping[2],
				bl_pwm->pwm_mapping[3], bl_pwm->pwm_mapping[4],
				bl_pwm->pwm_mapping[5], bl_pwm->pwm_mapping[6]);
		}
		break;
	default:
		break;
	}
}

static unsigned int bl_pwm_set_mapping(struct bl_pwm_config_s *bl_pwm, unsigned int level)
{
	unsigned int levelout;
	unsigned int p0, p1, p2, p3, p4, delta, step, step2, step3, step4;
	unsigned long long levelin;
	unsigned int tp[2];

	p0 = bl_pwm->pwm_mapping[0];
	p1 = bl_pwm->pwm_mapping[1];
	p2 = bl_pwm->pwm_mapping[2];
	p3 = bl_pwm->pwm_mapping[3];
	p4 = bl_pwm->pwm_mapping[4];
	tp[0] = bl_pwm->pwm_mapping[5];
	tp[1] = bl_pwm->pwm_mapping[6];
	levelin = level;

	if (p0 < bl_pwm->level_min || p4 > bl_pwm->level_max)
		BLERR("pwm mapping curve is out of pwm level range!!!");

	if (tp[1] < p4) {
		if (tp[bl_pwm->index] == p4) { /*pdim*/
			if (levelin >= tp[1] && levelin <= bl_pwm->level_max) {
				levelout = p4;
			} else {
				levelout = bl_do_div(((levelin - bl_pwm->level_min) * (p4 - p0)),
				(tp[1] - bl_pwm->level_min)) + p0;
			}
		} else { /*adim*/
			if (levelin < tp[1]) {
				levelout = p0;
			} else {
				levelout = bl_do_div(((levelin - tp[1]) * (p4 - p0)),
				(bl_pwm->level_max - tp[1])) + p0;
			}
		}
	} else {
		step = (bl_pwm->level_max - bl_pwm->level_min + 2) / 4;
		delta = step / 2;
		step2 = 2 * step;
		step3 = 3 * step;
		step4 = bl_pwm->level_max - step3 - bl_pwm->level_min;

		/*pwm curve mapping*/
		if (levelin < (step + bl_pwm->level_min))
			levelout = bl_do_div(((p1 - p0) * (levelin - bl_pwm->level_min)
				+ delta), step) + p0;
		else if (levelin < (step2 + bl_pwm->level_min))
			levelout = bl_do_div(((p2 - p1) * (levelin - step - bl_pwm->level_min)
				+ delta), step) + p1;
		else if (levelin < (step3 + bl_pwm->level_min))
			levelout = bl_do_div(((p3 - p2) * (levelin - step2 - bl_pwm->level_min)
				+ delta), step) + p2;
		else
			levelout = bl_do_div(((p4 - p3) * (levelin - step3 - bl_pwm->level_min)
				+ (step4 / 2)), step4) + p3;
	}

	if (levelout < bl_pwm->level_min)
		levelout = bl_pwm->level_min;
	else if (levelout > bl_pwm->level_max)
		levelout = bl_pwm->level_max;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
		BLPR("curve: %d %d %d %d %d, tp=%d_%d, levelin=%d, levelout=%d",
			p0, p1, p2, p3, p4, tp[0], tp[1], level, levelout);

	return levelout;
}

void bl_pwm_set_level(struct aml_bl_drv_s *bdrv,
		      struct bl_pwm_config_s *bl_pwm, unsigned int level)
{
	unsigned int min = bl_pwm->level_min;
	unsigned int max = bl_pwm->level_max;
	unsigned int pwm_max = bl_pwm->pwm_max;
	unsigned int pwm_min = bl_pwm->pwm_min;
	unsigned long long temp;

	if (bdrv->pwm_bypass)
		return;
	if (bl_pwm->pwm_cnt == 0) {
		BLERR("%s: pwm_cnt is 0\n", __func__);
		return;
	}

	level = bl_level_mapping(bdrv, level);
	level = bl_pwm_set_mapping(bl_pwm, level);
	max = bl_level_mapping(bdrv, max);
	min = bl_level_mapping(bdrv, min);
	if (max <= min || level < min) {
		bl_pwm->pwm_level = pwm_min;
	} else {
		temp = pwm_max - pwm_min;
		bl_pwm->pwm_level =
			bl_do_div((temp * (level - min)), (max - min)) +
				pwm_min;
	}
	temp = bl_pwm->pwm_level;
	if (bl_pwm->pwm_duty_max > 255) {
		bl_pwm->pwm_duty =
			(bl_do_div((temp * 40950), bl_pwm->pwm_cnt) + 5) / 10;
	} else if (bl_pwm->pwm_duty_max > 100) {
		bl_pwm->pwm_duty =
			(bl_do_div((temp * 2550), bl_pwm->pwm_cnt) + 5) / 10;
	} else {
		bl_pwm->pwm_duty =
			(bl_do_div((temp * 1000), bl_pwm->pwm_cnt) + 5) / 10;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("pwm port %d: level=%d, level_max=%d, level_min=%d\n",
		     bl_pwm->pwm_port, level, max, min);
		if (bl_pwm->pwm_duty_max > 100) {
			BLPR("duty=%d(%d%%), pwm max=%d, min=%d, pwm_lvl=%d\n",
			     bl_pwm->pwm_duty,
			     bl_pwm->pwm_duty * 100 / bl_pwm->pwm_duty_max,
			     bl_pwm->pwm_max, bl_pwm->pwm_min,
			     bl_pwm->pwm_level);
		} else {
			BLPR("duty=%d%%, pwm max=%d, min=%d, pwm_level=%d\n",
			     bl_pwm->pwm_duty,
			     bl_pwm->pwm_max, bl_pwm->pwm_min,
			     bl_pwm->pwm_level);
		}
	}

	bl_set_pwm(bdrv, bl_pwm);
}

void bl_pwm_config_init(struct bl_pwm_config_s *bl_pwm)
{
	struct aml_lcd_drv_s *pdrv;
	unsigned long long temp;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("%s pwm_port %d: freq = %u\n",
		     __func__, bl_pwm->pwm_port, bl_pwm->pwm_freq);
	}

	switch (bl_pwm->pwm_port) {
	case BL_PWM_VS:
		pdrv = aml_lcd_get_driver(bl_pwm->drv_index);
		bl_pwm->pwm_cnt = lcd_get_max_line_cnt(pdrv);
		break;
	default:
		/* for pwm api: pwm_period */
		bl_pwm->pwm_cnt = 1000000000 / bl_pwm->pwm_freq;
		break;
	}

	temp = bl_pwm->pwm_cnt;
	if (bl_pwm->pwm_duty_max > 255) {
		bl_pwm->pwm_max = bl_do_div((temp * bl_pwm->pwm_duty_max), 4095);
		bl_pwm->pwm_min = bl_do_div((temp * bl_pwm->pwm_duty_min), 4095);
	} else if (bl_pwm->pwm_duty_max > 100) {
		bl_pwm->pwm_max = bl_do_div((temp * bl_pwm->pwm_duty_max), 255);
		bl_pwm->pwm_min = bl_do_div((temp * bl_pwm->pwm_duty_min), 255);
	} else {
		bl_pwm->pwm_max = bl_do_div((temp * bl_pwm->pwm_duty_max), 100);
		bl_pwm->pwm_min = bl_do_div((temp * bl_pwm->pwm_duty_min), 100);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("pwm_cnt = %u, pwm_max = %u, pwm_min = %u\n",
		     bl_pwm->pwm_cnt, bl_pwm->pwm_max, bl_pwm->pwm_min);
	}
}

int bl_pwm_channel_register(struct device *dev, phandle pwm_phandle,
			    struct bl_pwm_config_s *bl_pwm)
{
	struct device_node *pnode = NULL;
	struct device_node *child;
	const char *pwm_str;
	unsigned int pwm_port, pwm_index = BL_PWM_MAX;
	int ret = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("%s\n", __func__);

	if (!bl_pwm) {
		BLERR("%s: bl_pwm is null\n", __func__);
		return -1;
	}
	pnode = of_find_node_by_phandle(pwm_phandle);
	if (!pnode) {
		BLERR("%s: can't find bl_pwm_config node\n", __func__);
		return -1;
	}

	/*request for pwm device */
	for_each_child_of_node(pnode, child) {
		ret = of_property_read_string(child, "pwm_port", &pwm_str);
		if (ret) {
			BLERR("%s: failed to get pwm_port\n", __func__);
			return -1;
		}
		pwm_port = bl_pwm_str_to_num(pwm_str);
		if (pwm_port >= BL_PWM_VS)
			continue;

		ret = of_property_read_u32_index(child, "pwms", 1, &pwm_index);
		if (ret) {
			BLERR("failed to get meson_pwm_index\n");
			return ret;
		}

		if (pwm_port != bl_pwm->pwm_port)
			continue;

		bl_pwm->pwm_data.meson_index = pwm_index;
		bl_pwm->pwm_data.pwm = devm_of_pwm_get(dev, child, NULL);
		if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
			ret = PTR_ERR(bl_pwm->pwm_data.pwm);
			BLERR("unable to request %s(%d): 0x%x\n", pwm_str, pwm_port, ret);
			continue;
		}
		bl_pwm->pwm_data.meson = to_meson_pwm(bl_pwm->pwm_data.pwm->chip);
		pwm_init_state(bl_pwm->pwm_data.pwm, &bl_pwm->pwm_data.state);
		BLPR("register %s(%d) 0x%px\n",
		     pwm_str, bl_pwm->pwm_data.meson_index, bl_pwm->pwm_data.pwm);
	}

	return ret;
}

static unsigned int pwm_vs_reg[] = {
	VPU_VPU_PWM_V0,
	VPU_VPU_PWM_V1,
	VPU_VPU_PWM_V2,
	VPU_VPU_PWM_V3,
	VPU_VPU_PWM_H0
};

static unsigned int pwm_vs_reg_t7[] = {
	VPU_VPU_PWM_V0_T7,
	VPU_VPU_PWM_V1_T7,
	VPU_VPU_PWM_V2_T7,
	VPU_VPU_PWM_V3_T7,
	VPU_VPU_PWM_H0_T7
};

int bl_pwm_init_config_probe(struct bl_data_s *bdata)
{
	switch (bdata->chip_type) {
	case LCD_CHIP_T7:
	case LCD_CHIP_T3:
	case LCD_CHIP_T5W:
		pwm_init_cfg.pwm_vs_reg = pwm_vs_reg_t7;
		pwm_init_cfg.pwm_vs_reg_cnt = ARRAY_SIZE(pwm_vs_reg_t7);
		break;
	default:
		pwm_init_cfg.pwm_vs_reg = pwm_vs_reg;
		pwm_init_cfg.pwm_vs_reg_cnt = ARRAY_SIZE(pwm_vs_reg);
		break;
	}

	pwm_init_cfg.pwm_vs_flag = bdata->pwm_vs_flag;

	return 0;
}
