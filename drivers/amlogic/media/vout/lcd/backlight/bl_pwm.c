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
	"PWM_J"
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

	if (bl_pwm->pwm_duty_max > 100)
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
	unsigned int vs[4], ve[4];
	int i;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("%s: pwm_duty=%d, out_level=%d\n",
		     __func__, bl_pwm->pwm_duty, out_level);
	}

	if (out_level == 0) {
		for (i = 0; i < 4; i++) {
			vs[i] = 0x1fff;
			ve[i] = 0;
		}
	} else if (out_level == 1) {
		for (i = 0; i < 4; i++) {
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
		for (i = n; i < 4; i++) {
			vs[i] = 0xffff;
			ve[i] = 0xffff;
		}
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		for (i = 0; i < 4; i++) {
			BLPR("pwm_vs: vs[%d]=%d, ve[%d]=%d\n",
			     i, vs[i], i, ve[i]);
		}
	}

	lcd_vcbus_write(VPU_VPU_PWM_V0, (pol << 31) |
		       (2 << 14) | /* vsync latch */
		       (ve[0] << 16) | (vs[0]));
	lcd_vcbus_write(VPU_VPU_PWM_V1, (ve[1] << 16) | (vs[1]));
	lcd_vcbus_write(VPU_VPU_PWM_V2, (ve[2] << 16) | (vs[2]));
	lcd_vcbus_write(VPU_VPU_PWM_V3, (ve[3] << 16) | (vs[3]));

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("VPU_VPU_PWM_V0=0x%08x\n", lcd_vcbus_read(VPU_VPU_PWM_V0));
		BLPR("VPU_VPU_PWM_V1=0x%08x\n", lcd_vcbus_read(VPU_VPU_PWM_V1));
		BLPR("VPU_VPU_PWM_V2=0x%08x\n", lcd_vcbus_read(VPU_VPU_PWM_V2));
		BLPR("VPU_VPU_PWM_V3=0x%08x\n", lcd_vcbus_read(VPU_VPU_PWM_V3));
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
			pwm_constant_enable(bl_pwm->pwm_data.meson,
					    bl_pwm->pwm_data.meson_index);
			if (bl_pwm->pwm_method)
				pstate.polarity = 0;
			else
				pstate.polarity = 1;
			pstate.duty_cycle = 0;
			pstate.enabled = 1;
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

	if (bdrv->pwm_duty_free) {
		if (bl_pwm->pwm_duty_max > 100) {
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
	if (bl_pwm->pwm_duty_max > 100) {
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
			     bl_pwm->pwm_duty * 100 / 255,
			     bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min);
		} else {
			BLPR("duty=%d%%, duty_max=%d%%, duty_min=%d%%\n",
			     bl_pwm->pwm_duty,
			     bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min);
		}
	}
	bl_set_pwm(bdrv, bl_pwm);
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

	level = bl_level_mapping(bdrv, level);
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
	if (bl_pwm->pwm_duty_max > 100) {
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
			     bl_pwm->pwm_duty * 100 / 255,
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
	unsigned int cnt;
	unsigned long long temp;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("%s pwm_port %d: freq = %u\n",
		     __func__, bl_pwm->pwm_port, bl_pwm->pwm_freq);
	}

	switch (bl_pwm->pwm_port) {
	case BL_PWM_VS:
		cnt = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;
		bl_pwm->pwm_cnt = cnt;
		break;
	default:
		/* for pwm api: pwm_period */
		bl_pwm->pwm_cnt = 1000000000 / bl_pwm->pwm_freq;
		break;
	}

	temp = bl_pwm->pwm_cnt;
	if (bl_pwm->pwm_duty_max > 100) {
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

int bl_pwm_channel_register(struct aml_bl_drv_s *bdrv)
{
	struct bl_pwm_config_s *bl_pwm = NULL;
	struct device_node *blnode = bdrv->dev->of_node;
	struct device_node *pnode = NULL;
	struct device_node *child;
	phandle pwm_phandle;
	const char *pwm_str;
	unsigned int pwm_port, pwm_index = BL_PWM_MAX;
	int ret = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("%s\n", __func__);

	ret = of_property_read_u32(blnode, "bl_pwm_config", &pwm_phandle);
	if (ret) {
		BLERR("%s: not match bl_pwm_config node\n", __func__);
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

		bl_pwm = NULL;
		switch (bdrv->bconf.method) {
		case BL_CTRL_PWM:
			if (pwm_port == bdrv->bconf.bl_pwm->pwm_port)
				bl_pwm = bdrv->bconf.bl_pwm;
			break;
		case BL_CTRL_PWM_COMBO:
			if (pwm_port == bdrv->bconf.bl_pwm_combo0->pwm_port)
				bl_pwm = bdrv->bconf.bl_pwm_combo0;
			if (pwm_port == bdrv->bconf.bl_pwm_combo1->pwm_port)
				bl_pwm = bdrv->bconf.bl_pwm_combo1;
			break;
		default:
			break;
		}
		if (!bl_pwm)
			continue;

		bl_pwm->pwm_data.meson_index = pwm_index;
		bl_pwm->pwm_data.pwm = devm_of_pwm_get(bdrv->dev, child, NULL);
		if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
			ret = PTR_ERR(bl_pwm->pwm_data.pwm);
			BLERR("[%d]: unable to request %s(%d): 0x%x\n",
			      bdrv->index, pwm_str, pwm_port, ret);
			continue;
		}
		bl_pwm->pwm_data.meson =
			to_meson_pwm(bl_pwm->pwm_data.pwm->chip);
		pwm_init_state(bl_pwm->pwm_data.pwm, &bl_pwm->pwm_data.state);
		BLPR("[%d]: register %s(%d) 0x%px\n",
		     bdrv->index, pwm_str,
		     bl_pwm->pwm_data.meson_index,
		     bl_pwm->pwm_data.pwm);
	}

	return ret;
}
