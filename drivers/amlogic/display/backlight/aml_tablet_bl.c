/*
 * AMLOGIC backlight driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 *
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 * Modify:  Evoke Zhang <evoke.zhang@amlogic.com>
 *
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
#include <linux/amlogic/vout/aml_tablet_bl.h>
#include <linux/amlogic/vout/aml_bl.h>
#include <linux/workqueue.h>
#include "aml_bl_reg.h"
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>
#ifdef CONFIG_AML_TABLET_BL_EXTERN
#include <linux/amlogic/vout/aml_bl_extern.h>
#endif


/* #define MESON_BACKLIGHT_DEBUG */
#ifdef MESON_BACKLIGHT_DEBUG
#define PRINT(...) pr_info(KERN_INFO __VA_ARGS__)
#define DTRACE()    pr_info(KERN_INFO "%s()\n", __func__)
static const char * const bl_ctrl_method_table[] = {
	"gpio",
	"pwm_negative",
	"pwm_positive",
	"pwm_combo",
	"extern",
	"null"
};
#else
#define PRINT(...)
#define DTRACE()
#endif

enum bl_chip_type {
	BL_CHIP_M6 = 0,
	BL_CHIP_M8,
	BL_CHIP_M8B,
	BL_CHIP_M8M2,
	BL_CHIP_MAX,
};


enum bl_chip_type chip_type = BL_CHIP_MAX;
const char *bl_chip_table[] = {
	"M6",
	"M8",
	"M8b",
	"M8M2",
	"invalid",
};

/* for lcd backlight power */
enum bl_ctrl_method_t {
	BL_CTL_GPIO = 0,
	BL_CTL_PWM_NEGATIVE = 1,
	BL_CTL_PWM_POSITIVE = 2,
	BL_CTL_PWM_COMBO = 3,
	BL_CTL_EXTERN = 4,
	BL_CTL_MAX = 5,
};

enum bl_pwm_port_t {
	BL_PWM_A = 0,
	BL_PWM_B,
	BL_PWM_C,
	BL_PWM_D,
	BL_PWM_E,
	BL_PWM_F,
	BL_PWM_MAX,
};

struct lcd_bl_config_t {
	u32 level_default;
	u32 level_mid;
	u32 level_mid_mapping;
	u32 level_min;
	u32 level_max;
	unsigned short power_on_delay;
	unsigned char method;

	struct gpio_desc *gpio;
	u32 gpio_on;
	u32 gpio_off;
	u32 dim_max;
	u32 dim_min;
	unsigned char pwm_port;
	unsigned char pwm_gpio_used;
	u32 pwm_cnt;
	u32 pwm_pre_div;
	u32 pwm_max;
	u32 pwm_min;

	u32 combo_level_switch;
	unsigned char combo_high_port;
	unsigned char combo_high_method;
	unsigned char combo_low_port;
	unsigned char combo_low_method;
	u32 combo_high_cnt;
	u32 combo_high_pre_div;
	u32 combo_high_duty_max;
	u32 combo_high_duty_min;
	u32 combo_low_cnt;
	u32 combo_low_pre_div;
	u32 combo_low_duty_max;
	u32 combo_low_duty_min;

	struct pinctrl *p;
	struct workqueue_struct *workqueue;
	struct delayed_work bl_delayed_work;
};

static struct lcd_bl_config_t bl_config = {
	.level_default = 128,
	.level_mid = 128,
	.level_mid_mapping = 128,
	.level_min = 10,
	.level_max = 255,
	.power_on_delay = 100,
	.method = BL_CTL_MAX,
};
static u32 bl_level = BL_LEVEL_DEFAULT;
static u32 bl_status = 3; /*  bit[0]:lcd, bit[1]:bl */
static u32 bl_real_status = 1;

#define FIN_FREQ				(24 * 1000)

void get_bl_ext_level(struct bl_extern_config_t *bl_ext_cfg)
{
	bl_ext_cfg->level_min = bl_config.level_min;
	bl_ext_cfg->level_max = bl_config.level_max;
}

static DEFINE_MUTEX(bl_power_mutex);
static void power_on_bl(int bl_flag)
{
	struct pinctrl_state *s;
#ifdef CONFIG_AML_TABLET_BL_EXTERN
	struct aml_bl_extern_driver_t *bl_extern_driver;
#endif
	int ret;

	mutex_lock(&bl_power_mutex);
	if (bl_flag == LCD_BL_FLAG) {
		if (bl_status > 0)
			bl_status = 3;
		else
			goto exit_power_on_bl;
	}

	PRINT("%s(bl_flag=%s):bl_level=%u,bl_status=%u,bl_real_status=%u\n",
		__func__, (bl_flag ? "LCD_BL_FLAG" : "DRV_BL_FLAG"),
		bl_level, bl_status, bl_real_status);
	if ((bl_level == 0) || (bl_status != 3) || (bl_real_status == 1))
		goto exit_power_on_bl;
	switch (bl_config.method) {
	case BL_CTL_GPIO:
		if (chip_type == BL_CHIP_M6)
			bl_reg_setb(LED_PWM_REG0, 1, 12, 2);
		mdelay(20);
		bl_gpio_output(bl_config.gpio, bl_config.gpio_on);
		break;
	case BL_CTL_PWM_NEGATIVE:
	case BL_CTL_PWM_POSITIVE:
		switch (bl_config.pwm_port) {
		case BL_PWM_A:
			/* pwm_a_clk_div */
			bl_reg_setb(PWM_MISC_REG_AB,
				bl_config.pwm_pre_div, 8, 7);
			/* pwm_a_clk_sel */
			bl_reg_setb(PWM_MISC_REG_AB, 0, 4, 2);
			/* pwm_a_clk_en */
			bl_reg_setb(PWM_MISC_REG_AB, 1, 15, 1);
			/* enable pwm_a */
			bl_reg_setb(PWM_MISC_REG_AB, 1, 0, 1);
			break;
		case BL_PWM_B:
			bl_reg_setb(PWM_MISC_REG_AB,
				bl_config.pwm_pre_div, 16, 7);
			bl_reg_setb(PWM_MISC_REG_AB, 0, 6, 2);
			bl_reg_setb(PWM_MISC_REG_AB, 1, 23, 1);
			bl_reg_setb(PWM_MISC_REG_AB, 1, 1, 1);
			break;
		case BL_PWM_C:
			bl_reg_setb(PWM_MISC_REG_CD,
				bl_config.pwm_pre_div, 8, 7);
			bl_reg_setb(PWM_MISC_REG_CD, 0, 4, 2);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 15, 1);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 0, 1);
			break;
		case BL_PWM_D:
			bl_reg_setb(PWM_MISC_REG_CD,
				bl_config.pwm_pre_div, 16, 7);
			bl_reg_setb(PWM_MISC_REG_CD, 0, 6, 2);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 23, 1);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 1, 1);
			break;
		case BL_PWM_E:
			if (chip_type >= BL_CHIP_M8) {
				bl_reg_setb(PWM_MISC_REG_EF,
					bl_config.pwm_pre_div, 8, 7);
				bl_reg_setb(PWM_MISC_REG_EF, 0, 4, 2);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 15, 1);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 0, 1);
			}
			break;
		case BL_PWM_F:
			if (chip_type >= BL_CHIP_M8) {
				bl_reg_setb(PWM_MISC_REG_EF,
					bl_config.pwm_pre_div, 16, 7);
				bl_reg_setb(PWM_MISC_REG_EF, 0, 6, 2);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 23, 1);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 1, 1);
			}
			break;
		default:
			break;
		}

		if (IS_ERR(bl_config.p)) {
			PRINT("set backlight pinmux error.\n");
			goto exit_power_on_bl;
		}
		 /* select pinctrl */
		s = pinctrl_lookup_state(bl_config.p, "default");
		if (IS_ERR(s)) {
			PRINT("set backlight pinmux error.\n");
			devm_pinctrl_put(bl_config.p);
			goto exit_power_on_bl;
		}
		/* set pinmux and lock pins */
		ret = pinctrl_select_state(bl_config.p, s);
		if (ret < 0) {
			PRINT("set backlight pinmux error.\n");
			devm_pinctrl_put(bl_config.p);
			goto exit_power_on_bl;
		}
		mdelay(20);
		if (bl_config.pwm_gpio_used) {
			if (bl_config.gpio)
				bl_gpio_output(bl_config.gpio,
					bl_config.gpio_on);
		}
		break;
	case BL_CTL_PWM_COMBO:
		switch (bl_config.combo_high_port) {
		case BL_PWM_A:
			bl_reg_setb(PWM_MISC_REG_AB,
				bl_config.combo_high_pre_div, 8, 7);
			bl_reg_setb(PWM_MISC_REG_AB, 0, 4, 2);
			bl_reg_setb(PWM_MISC_REG_AB, 1, 15, 1);
			bl_reg_setb(PWM_MISC_REG_AB, 1, 0, 1);
			break;
		case BL_PWM_B:
			bl_reg_setb(PWM_MISC_REG_AB,
				bl_config.combo_high_pre_div, 16, 7);
			bl_reg_setb(PWM_MISC_REG_AB, 0, 6, 2);
			bl_reg_setb(PWM_MISC_REG_AB, 1, 23, 1);
			bl_reg_setb(PWM_MISC_REG_AB, 1, 1, 1);
			break;
		case BL_PWM_C:
			bl_reg_setb(PWM_MISC_REG_CD,
				bl_config.combo_high_pre_div, 8, 7);
			bl_reg_setb(PWM_MISC_REG_CD, 0, 4, 2);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 15, 1);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 0, 1);
			break;
		case BL_PWM_D:
			bl_reg_setb(PWM_MISC_REG_CD,
				bl_config.combo_high_pre_div, 16, 7);
			bl_reg_setb(PWM_MISC_REG_CD, 0, 6, 2);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 23, 1);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 1, 1);
			break;
		case BL_PWM_E:
			if (chip_type >= BL_CHIP_M8) {
				bl_reg_setb(PWM_MISC_REG_EF,
					bl_config.combo_high_pre_div, 8, 7);
				bl_reg_setb(PWM_MISC_REG_EF, 0, 4, 2);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 15, 1);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 0, 1);
			}
			break;
		case BL_PWM_F:
			if (chip_type >= BL_CHIP_M8) {
				bl_reg_setb(PWM_MISC_REG_EF,
					bl_config.combo_high_pre_div, 16, 7);
				bl_reg_setb(PWM_MISC_REG_EF, 0, 6, 2);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 23, 1);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 1, 1);
			}
			break;
		default:
			break;
		}
		switch (bl_config.combo_low_port) {
		case BL_PWM_A:
			bl_reg_setb(PWM_MISC_REG_AB,
				bl_config.combo_low_pre_div, 8, 7);
			bl_reg_setb(PWM_MISC_REG_AB, 0, 4, 2);
			bl_reg_setb(PWM_MISC_REG_AB, 1, 15, 1);
			bl_reg_setb(PWM_MISC_REG_AB, 1, 0, 1);
			break;
		case BL_PWM_B:
			bl_reg_setb(PWM_MISC_REG_AB,
				bl_config.combo_low_pre_div, 16, 7);
			bl_reg_setb(PWM_MISC_REG_AB, 0, 6, 2);
			bl_reg_setb(PWM_MISC_REG_AB, 1, 23, 1);
			bl_reg_setb(PWM_MISC_REG_AB, 1, 1, 1);
			break;
		case BL_PWM_C:
			bl_reg_setb(PWM_MISC_REG_CD,
				bl_config.combo_low_pre_div, 8, 7);
			bl_reg_setb(PWM_MISC_REG_CD, 0, 4, 2);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 15, 1);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 0, 1);
			break;
		case BL_PWM_D:
			bl_reg_setb(PWM_MISC_REG_CD,
				bl_config.combo_low_pre_div, 16, 7);
			bl_reg_setb(PWM_MISC_REG_CD, 0, 6, 2);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 23, 1);
			bl_reg_setb(PWM_MISC_REG_CD, 1, 1, 1);
			break;
		case BL_PWM_E:
			if (chip_type >= BL_CHIP_M8) {
				bl_reg_setb(PWM_MISC_REG_EF,
					bl_config.combo_low_pre_div, 8, 7);
				bl_reg_setb(PWM_MISC_REG_EF, 0, 4, 2);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 15, 1);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 0, 1);
			}
			break;
		case BL_PWM_F:
			if (chip_type >= BL_CHIP_M8) {
				bl_reg_setb(PWM_MISC_REG_EF,
					bl_config.combo_low_pre_div, 16, 7);
				bl_reg_setb(PWM_MISC_REG_EF, 0, 6, 2);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 23, 1);
				bl_reg_setb(PWM_MISC_REG_EF, 1, 1, 1);
			}
			break;
		default:
			break;
		}

		if (IS_ERR(bl_config.p)) {
			pr_err("set backlight pinmux error.\n");
			goto exit_power_on_bl;
		}
		s = pinctrl_lookup_state(bl_config.p, "pwm_combo");
		if (IS_ERR(s)) {
			pr_err("set backlight pinmux error.\n");
			devm_pinctrl_put(bl_config.p);
			goto exit_power_on_bl;
		}

		ret = pinctrl_select_state(bl_config.p, s);
		if (ret < 0) {
			pr_err("set backlight pinmux error.\n");
			devm_pinctrl_put(bl_config.p);
			goto exit_power_on_bl;
		}
		break;
#ifdef CONFIG_AML_TABLET_BL_EXTERN
	case BL_CTL_EXTERN:
		bl_extern_driver = aml_bl_extern_get_driver();
		if (bl_extern_driver == NULL) {
			pr_err("no bl_extern driver\n");
		} else {
			if (bl_extern_driver->power_on) {
				ret = bl_extern_driver->power_on();
				if (ret) {
					pr_err("[bl_extern] power on error\n");
					goto exit_power_on_bl;
				}
			} else {
				PRINT("[bl_extern] power on is null\n");
			}
		}
		break;
#endif
	default:
		pr_err("wrong backlight control method\n");
		goto exit_power_on_bl;
		break;
	}
	bl_real_status = 1;
	pr_info("backlight power on\n");

exit_power_on_bl:
	mutex_unlock(&bl_power_mutex);
}

 /* bl_delayed_work for LCD_BL_FLAG control */
static void bl_delayd_on(struct work_struct *work)
{
	power_on_bl(LCD_BL_FLAG);
}

void bl_power_on(int bl_flag)
{
	PRINT("%s(bl_flag=%s): bl_level=%u, bl_status=%s, bl_real_status=%s\n",
		__func__, (bl_flag ? "LCD_BL_FLAG" : "DRV_BL_FLAG"),
			bl_level, (bl_status ? "ON" : "OFF"),
				(bl_real_status ? "ON" : "OFF"));
	if (bl_flag == LCD_BL_FLAG)
		bl_status = 1;

	if (bl_config.method < BL_CTL_MAX) {
		if (bl_flag == LCD_BL_FLAG) {
			if (bl_config.workqueue) {
				queue_delayed_work(bl_config.workqueue,
				&bl_config.bl_delayed_work,
				msecs_to_jiffies(bl_config.power_on_delay));
			} else {
				PRINT("[Warning]: no bl workqueue\n");
				msleep(bl_config.power_on_delay);
				power_on_bl(bl_flag);
			}
		} else {
			power_on_bl(bl_flag);
		}
	} else {
		PRINT("wrong backlight control method\n");
	}
	pr_info("bl_power_on...\n");
}

void bl_power_off(int bl_flag)
{
#ifdef CONFIG_AML_TABLET_BL_EXTERN
	struct aml_bl_extern_driver_t *bl_extern_driver;
#endif
	int ret;

	mutex_lock(&bl_power_mutex);

	if (bl_flag == LCD_BL_FLAG)
		bl_status = 0;
	PRINT("%s(bl_flag=%s): bl_level=%u, bl_status=%u, bl_real_status=%u\n",
		__func__, (bl_flag ? "LCD_BL_FLAG" : "DRV_BL_FLAG"),
			bl_level, bl_status, bl_real_status);
	if (bl_real_status == 0) {
		mutex_unlock(&bl_power_mutex);
		return;
	}

	switch (bl_config.method) {
	case BL_CTL_GPIO:
		bl_gpio_output(bl_config.gpio, bl_config.gpio_off);
		break;
	case BL_CTL_PWM_NEGATIVE:
	case BL_CTL_PWM_POSITIVE:
		if (bl_config.pwm_gpio_used) {
			if (bl_config.gpio)
				bl_gpio_output(bl_config.gpio,
						bl_config.gpio_off);
		}
		switch (bl_config.pwm_port) {
		case BL_PWM_A:
			bl_reg_setb(PWM_MISC_REG_AB, 0, 0, 1);
			break;
		case BL_PWM_B:
			bl_reg_setb(PWM_MISC_REG_AB, 0, 1, 1);
			break;
		case BL_PWM_C:
			bl_reg_setb(PWM_MISC_REG_CD, 0, 0, 1);
			break;
		case BL_PWM_D:
			bl_reg_setb(PWM_MISC_REG_CD, 0, 1, 1);
			break;
		case BL_PWM_E:
			if (chip_type >= BL_CHIP_M8)
				bl_reg_setb(PWM_MISC_REG_EF, 0, 0, 1);
			break;
		case BL_PWM_F:
			if (chip_type >= BL_CHIP_M8)
				bl_reg_setb(PWM_MISC_REG_EF, 0, 1, 1);
			break;
		default:
			break;
		}
		break;
	case BL_CTL_PWM_COMBO:
		switch (bl_config.combo_high_port) {
		case BL_PWM_A:
			bl_reg_setb(PWM_MISC_REG_AB, 0, 0, 1);
			break;
		case BL_PWM_B:
			bl_reg_setb(PWM_MISC_REG_AB, 0, 1, 1);
			break;
		case BL_PWM_C:
			bl_reg_setb(PWM_MISC_REG_CD, 0, 0, 1);
			break;
		case BL_PWM_D:
			bl_reg_setb(PWM_MISC_REG_CD, 0, 1, 1);
			break;
		case BL_PWM_E:
			if (chip_type >= BL_CHIP_M8)
				bl_reg_setb(PWM_MISC_REG_EF, 0, 0, 1);
			break;
		case BL_PWM_F:
			if (chip_type >= BL_CHIP_M8)
				bl_reg_setb(PWM_MISC_REG_EF, 0, 1, 1);
			break;
		default:
			break;
		}
		switch (bl_config.combo_low_port) {
		case BL_PWM_A:
			bl_reg_setb(PWM_MISC_REG_AB, 0, 0, 1);
			break;
		case BL_PWM_B:
			bl_reg_setb(PWM_MISC_REG_AB, 0, 1, 1);
			break;
		case BL_PWM_C:
			bl_reg_setb(PWM_MISC_REG_CD, 0, 0, 1);
			break;
		case BL_PWM_D:
			bl_reg_setb(PWM_MISC_REG_CD, 0, 1, 1);
			break;
		case BL_PWM_E:
			if (chip_type >= BL_CHIP_M8)
				bl_reg_setb(PWM_MISC_REG_EF, 0, 0, 1);
			break;
		case BL_PWM_F:
			if (chip_type >= BL_CHIP_M8)
				bl_reg_setb(PWM_MISC_REG_EF, 0, 1, 1);
			break;
		default:
			break;
		}
		break;
#ifdef CONFIG_AML_TABLET_BL_EXTERN
	case BL_CTL_EXTERN:
		bl_extern_driver = aml_bl_extern_get_driver();
		if (bl_extern_driver == NULL) {
			PRINT("no bl_extern driver\n");
		} else {
			if (bl_extern_driver->power_off) {
				ret = bl_extern_driver->power_off();
				if (ret)
					PRINT("[bl_extern] power off error\n");
			} else {
				PRINT("[bl_extern] power off is null\n");
			}
		}
		break;
#endif
	default:
		break;
	}
	bl_real_status = 0;
	pr_info("backlight power off\n");
	mutex_unlock(&bl_power_mutex);
}

u32 level_range_check(u32 level, struct lcd_bl_config_t bl_config)
{
	u32 max, min;
	max = bl_config.level_max;
	min = bl_config.level_min;
	min = (level < min ? (level < BL_LEVEL_OFF ? 0 : min) : level);
	level = level > max ? max : min;
	return level;
}

u32 level_mapping(u32 level, struct lcd_bl_config_t bl_config)
{
	u32 level_mid = bl_config.level_mid;
	u32 level_mid_map = bl_config.level_mid_mapping;
	u32 max = bl_config.level_max;
	u32 min = bl_config.level_min;
	if (level > level_mid) {
		level = ((level - level_mid) * (max - level_mid_map)) /
			(max - level_mid) + level_mid_map;
	} else {
		level = ((level - min) * (level_mid_map - min)) /
			(level_mid - min) + min;
	}
	return level;
}

u32 level_to_gpio_level(u32 level, struct lcd_bl_config_t bl_config)
{
	u32 dim_min = bl_config.dim_min;
	u32 level_min = bl_config.level_min;
	u32 dim_max = bl_config.dim_max;
	u32 level_max = bl_config.level_max;
	level = dim_min - ((level - level_min) * (dim_min - dim_max)) /
		(level_max - level_min);
	return level;
}

u32 level_transfer_to_pwm_ctrl(u32 level,
					struct lcd_bl_config_t bl_config)
{
	u32 pwm_max = bl_config.pwm_max;
	u32 pwm_min = bl_config.pwm_min;
	u32 level_min = bl_config.level_min;
	u32 level_max = bl_config.level_max;
	level = (pwm_max - pwm_min) * (level - level_min) /
		(level_max - level_min) + pwm_min;
	return level;
}

void pwm_set_level_ctrl(u32 level, struct lcd_bl_config_t bl_config)
{
	u32 pwm_hi = 0, pwm_lo = 0;
	if (bl_config.method == BL_CTL_PWM_NEGATIVE) {
		pwm_hi = bl_config.pwm_cnt - level;
		pwm_lo = level;
	} else {
		pwm_hi = level;
		pwm_lo = bl_config.pwm_cnt - level;
	}
	switch (bl_config.pwm_port) {
	case BL_PWM_A:
		bl_write_reg(PWM_PWM_A, (pwm_hi << 16) | (pwm_lo));
		break;
	case BL_PWM_B:
		bl_write_reg(PWM_PWM_B, (pwm_hi << 16) | (pwm_lo));
		break;
	case BL_PWM_C:
		bl_write_reg(PWM_PWM_C, (pwm_hi << 16) | (pwm_lo));
		break;
	case BL_PWM_D:
		bl_write_reg(PWM_PWM_D, (pwm_hi << 16) | (pwm_lo));
		break;
	case BL_PWM_E:
		if (chip_type >= BL_CHIP_M8)
			bl_write_reg(PWM_PWM_E, (pwm_hi << 16) | (pwm_lo));
		break;
	case BL_PWM_F:
		if (chip_type >= BL_CHIP_M8)
			bl_write_reg(PWM_PWM_F, (pwm_hi << 16) | (pwm_lo));
		break;
	default:
		break;
	}
}

void combo_set_level_ctrl(u32 level, struct lcd_bl_config_t bl_config)
{
	u32 pwm_hi = 0, pwm_lo = 0;
	u32 combo_low_cnt = bl_config.combo_low_cnt;
	u32 combo_high_cnt = bl_config.combo_high_cnt;
	u32 combo_low_duty_max = bl_config.combo_low_duty_max;
	u32 combo_low_duty_min = bl_config.combo_low_duty_min;
	u32 combo_high_duty_max = bl_config.combo_high_duty_max;
	u32 combo_high_duty_min = bl_config.combo_high_duty_min;
	u32 combo_level_switch = bl_config.combo_level_switch;
	u32 level_max = bl_config.level_max;
	u32 level_min = bl_config.level_min;
	if (level >= bl_config.combo_level_switch) {
		if (bl_config.combo_low_method == BL_CTL_PWM_NEGATIVE) {
			pwm_hi = combo_low_cnt - combo_low_duty_max;
			pwm_lo = combo_low_duty_max;
		} else {
			pwm_hi = combo_low_duty_max;
			pwm_lo = combo_low_cnt - combo_low_duty_max;
		}
		switch (bl_config.combo_low_port) {
		case BL_PWM_A:
			bl_write_reg(PWM_PWM_A, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_B:
			bl_write_reg(PWM_PWM_B, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_C:
			bl_write_reg(PWM_PWM_C, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_D:
			bl_write_reg(PWM_PWM_D, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_E:
			if (chip_type >= BL_CHIP_M8) {
				bl_write_reg(PWM_PWM_E,
					(pwm_hi << 16) | (pwm_lo));
			}
			break;
		case BL_PWM_F:
			if (chip_type >= BL_CHIP_M8) {
				bl_write_reg(PWM_PWM_F,
					(pwm_hi << 16) | (pwm_lo));
			}
			break;
		default:
			break;
		}

		/* set combo_high duty */
		level = (combo_high_duty_max - combo_high_duty_min) *
		(level - combo_level_switch) / (level_max - combo_level_switch)
		+ combo_high_duty_min;
		if (bl_config.combo_high_method == BL_CTL_PWM_NEGATIVE) {
			pwm_hi = combo_high_cnt - level;
			pwm_lo = level;
		} else {
			pwm_hi = level;
			pwm_lo = combo_high_cnt - level;
		}
		switch (bl_config.combo_high_port) {
		case BL_PWM_A:
			bl_write_reg(PWM_PWM_A, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_B:
			bl_write_reg(PWM_PWM_B, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_C:
			bl_write_reg(PWM_PWM_C, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_D:
			bl_write_reg(PWM_PWM_D, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_E:
			if (chip_type >= BL_CHIP_M8) {
				bl_write_reg(PWM_PWM_E,
					(pwm_hi << 16) | (pwm_lo));
			}
			break;
		case BL_PWM_F:
			if (chip_type >= BL_CHIP_M8) {
				bl_write_reg(PWM_PWM_F,
					(pwm_hi << 16) | (pwm_lo));
			}
			break;
		default:
			break;
		}
	} else {
		/* pre_set combo_high duty min */
		if (bl_config.combo_high_method == BL_CTL_PWM_NEGATIVE) {
			pwm_hi = combo_high_cnt - combo_high_duty_min;
			pwm_lo = combo_high_duty_min;
		} else {
			pwm_hi = combo_high_duty_min;
			pwm_lo = combo_high_cnt - combo_high_duty_min;
		}
		switch (bl_config.combo_high_port) {
		case BL_PWM_A:
			bl_write_reg(PWM_PWM_A, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_B:
			bl_write_reg(PWM_PWM_B, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_C:
			bl_write_reg(PWM_PWM_C, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_D:
			bl_write_reg(PWM_PWM_D, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_E:
			if (chip_type >= BL_CHIP_M8) {
				bl_write_reg(PWM_PWM_E,
					(pwm_hi << 16) | (pwm_lo));
			}
			break;
		case BL_PWM_F:
			if (chip_type >= BL_CHIP_M8) {
				bl_write_reg(PWM_PWM_F,
					(pwm_hi << 16) | (pwm_lo));
			}
			break;
		default:
			break;
		}
		level = (combo_low_duty_max - combo_low_duty_min) *
			(level - level_min) / (combo_level_switch - level_min)
			+ combo_low_duty_min;
		if (bl_config.combo_low_method == BL_CTL_PWM_NEGATIVE) {
			pwm_hi = combo_low_cnt - level;
			pwm_lo = level;
		} else {
			pwm_hi = level;
			pwm_lo = combo_low_cnt - level;
		}
		switch (bl_config.combo_low_port) {
		case BL_PWM_A:
			bl_write_reg(PWM_PWM_A, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_B:
			bl_write_reg(PWM_PWM_B, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_C:
			bl_write_reg(PWM_PWM_C, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_D:
			bl_write_reg(PWM_PWM_D, (pwm_hi << 16) | (pwm_lo));
			break;
		case BL_PWM_E:
			if (chip_type >= BL_CHIP_M8) {
				bl_write_reg(PWM_PWM_E,
						(pwm_hi << 16) | (pwm_lo));
			}
			break;
		case BL_PWM_F:
			if (chip_type >= BL_CHIP_M8) {
				bl_write_reg(PWM_PWM_F,
					(pwm_hi << 16) | (pwm_lo));
			}
			break;
		default:
			break;
		}
	}
}
#ifdef CONFIG_AML_TABLET_BL_EXTERN
void bl_extern_set_level_ctrl(u32 level)
{
	struct aml_bl_extern_driver_t *bl_extern_driver;
	int ret;
	bl_extern_driver = aml_bl_extern_get_driver();
	if (bl_extern_driver == NULL) {
		PRINT("no bl_extern driver\n");
	} else {
		if (bl_extern_driver->set_level) {
			ret = bl_extern_driver->set_level(level);
			if (ret)
				PRINT("[bl_extern] set_level error\n");
		} else {
			PRINT("bl_extern set_level is null\n");
		}
	}
}
#endif

static DEFINE_MUTEX(bl_level_mutex);
static void set_backlight_level(u32 level)
{
	mutex_lock(&bl_level_mutex);
	PRINT("set_backlight_level: %u, last level: %u.\n", level, bl_level);
	PRINT("bl_status:%u,bl_real_status: %u.\n", bl_status, bl_real_status);
	level = level_range_check(level, bl_config);
	bl_level = level;
	if (bl_level == 0) {
		if (bl_real_status == 1)
			bl_power_off(DRV_BL_FLAG);
	} else {
		level = level_mapping(level, bl_config);
		PRINT("level mapping=%u\n", level);
		switch (bl_config.method) {
		case BL_CTL_GPIO:
			level = level_to_gpio_level(level, bl_config);
			if (chip_type == BL_CHIP_M6)
				bl_reg_setb(LED_PWM_REG0, level, 0, 4);
			break;
		case BL_CTL_PWM_NEGATIVE:
		case BL_CTL_PWM_POSITIVE:
			level = level_transfer_to_pwm_ctrl(level, bl_config);
			pwm_set_level_ctrl(level, bl_config);
			break;
		case BL_CTL_PWM_COMBO:
			combo_set_level_ctrl(level, bl_config);
			break;
#ifdef CONFIG_AML_TABLET_BL_EXTERN
		case BL_CTL_EXTERN:
			bl_extern_set_level_ctrl(level);
			break;
#endif
		default:
			break;
		}
		if ((bl_status == 3) && (bl_real_status == 0))
			bl_power_on(DRV_BL_FLAG);
	}
	mutex_unlock(&bl_level_mutex);
}

u32 get_backlight_level(void)
{
	PRINT("%s: %d\n", __func__, bl_level);
	return bl_level;
}


struct aml_bl {
	const struct bl_platform_data   *pdata;
	struct backlight_device         *bldev;
	struct platform_device          *pdev;
};

static int aml_bl_update_status(struct backlight_device *bd)
{
	struct aml_bl *amlbl = bl_get_data(bd);
	int brightness = bd->props.brightness;
	if (brightness < 0)
		brightness = 0;
	else if (brightness > 255)
		brightness = 255;
	if (amlbl->pdata->set_bl_level)
		amlbl->pdata->set_bl_level(brightness);
	return 0;
}

static int aml_bl_get_brightness(struct backlight_device *bd)
{
	struct aml_bl *amlbl = bl_get_data(bd);
	PRINT("%s() pdata->get_bl_level=%p\n",
		__func__, amlbl->pdata->get_bl_level);
	if (amlbl->pdata->get_bl_level)
		return amlbl->pdata->get_bl_level();
	else
		return 0;
}

static const struct backlight_ops aml_bl_ops = {
	.get_brightness = aml_bl_get_brightness,
	.update_status  = aml_bl_update_status,
};

#ifdef CONFIG_USE_OF
static struct bl_platform_data meson_backlight_platform = {
	.get_bl_level = get_backlight_level,
	.set_bl_level = set_backlight_level,
	.max_brightness = BL_LEVEL_MAX,
	.dft_brightness = BL_LEVEL_DEFAULT,
};

#define AMLOGIC_BL_DRV_DATA ((kernel_ulong_t)&meson_backlight_platform)

static const struct of_device_id backlight_dt_match[] = {
	{
		.compatible = "amlogic, backlight",
		.data = (void *)AMLOGIC_BL_DRV_DATA
	},
	{},
};
#else
#define backlight_dt_match NULL
#endif

#ifdef CONFIG_USE_OF
static struct bl_platform_data *bl_get_driver_data(struct platform_device *pdev)
{
	const struct of_device_id *match;
	if (pdev->dev.of_node) {
		match = of_match_node(backlight_dt_match, pdev->dev.of_node);
		return (struct bl_platform_data *)match->data;
	}
	return NULL;
}
#endif




static inline int _get_backlight_config(struct platform_device *pdev)
{
	int ret = 0;
	int val;
	const char *str;
	u32 bl_para[3];
	u32 pwm_freq = 0, pwm_cnt, pwm_pre_div;
	int i;
	struct gpio_desc *bl_gpio;
	if (pdev->dev.of_node) {
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"bl_level_default_uboot_kernel", &bl_para[0], 2);
		if (ret) {
			PRINT("faild to get bl_level_default_uboot_kernel\n");
			bl_config.level_default = BL_LEVEL_DEFAULT;
		} else {
			bl_config.level_default = bl_para[1];
		}
		PRINT("bl level default kernel=%u\n", bl_config.level_default);
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"bl_level_middle_mapping", &bl_para[0], 2);
		if (ret) {
			PRINT("faild to get bl_level_middle_mapping!\n");
			bl_config.level_mid = BL_LEVEL_MID;
			bl_config.level_mid_mapping = BL_LEVEL_MID_MAPPED;
		} else {
			bl_config.level_mid = bl_para[0];
			bl_config.level_mid_mapping = bl_para[1];
		}
		PRINT("bl level mid=%u, mid_mapping=%u\n",
			bl_config.level_mid, bl_config.level_mid_mapping);
		ret = of_property_read_u32_array(pdev->dev.of_node,
			"bl_level_max_min", &bl_para[0], 2);
		if (ret) {
			pr_err("faild to get bl_level_max_min\n");
			bl_config.level_min = BL_LEVEL_MIN;
			bl_config.level_max = BL_LEVEL_MAX;
		} else {
			bl_config.level_max = bl_para[0];
			bl_config.level_min = bl_para[1];
		}
		PRINT("bl level max=%u, min=%u\n",
			bl_config.level_max, bl_config.level_min);

		ret = of_property_read_u32(pdev->dev.of_node,
			"bl_power_on_delay", &val);
		if (ret) {
			PRINT("faild to get bl_power_on_delay\n");
			bl_config.power_on_delay = 100;
		} else {
			val = val & 0xffff;
			bl_config.power_on_delay = (unsigned short)val;
		}
		PRINT("bl power_on_delay: %ums\n", bl_config.power_on_delay);
		ret = of_property_read_u32(pdev->dev.of_node,
					"bl_ctrl_method", &val);
		if (ret) {
			PRINT("faild to get bl_ctrl_method\n");
			bl_config.method = BL_CTL_MAX;
		} else {
			val = (val >= BL_CTL_MAX) ? BL_CTL_MAX : val;
			bl_config.method = (unsigned char)val;
		}
		PRINT("bl control_method: %s(%u)\n",
		bl_ctrl_method_table[bl_config.method], bl_config.method);

		if (bl_config.method == BL_CTL_GPIO) {
			ret = of_property_read_string_index(pdev->dev.of_node,
						"bl_gpio_port_on_off", 0, &str);
			if (ret) {
				PRINT("faild to get bl_gpio_port_on_off!\n");
				if (chip_type == BL_CHIP_M6)
					str = "GPIOD_1";
				else if ((chip_type == BL_CHIP_M8) ||
						(chip_type == BL_CHIP_M8B))
					str = "GPIODV_28";
			}
			bl_gpio = bl_gpio_request(&pdev->dev, str);
			if (IS_ERR(bl_gpio)) {
				bl_config.gpio = bl_gpio;
				PRINT("bl gpio = %s(%d)\n", str);
			} else {
				bl_config.gpio = NULL;
			}
			ret = of_property_read_string_index(pdev->dev.of_node,
						"bl_gpio_port_on_off", 1, &str);
			if (ret) {
				PRINT("faild to get bl_gpio_port_on!\n");
				bl_config.gpio_on = BL_GPIO_OUTPUT_HIGH;
			} else {
				if (strcmp(str, "2") == 0)
					bl_config.gpio_on = BL_GPIO_INPUT;
				else if (strcmp(str, "0") == 0)
					bl_config.gpio_on = BL_GPIO_OUTPUT_LOW;
				else
					bl_config.gpio_on = BL_GPIO_OUTPUT_HIGH;
			}
			ret = of_property_read_string_index(pdev->dev.of_node,
						"bl_gpio_port_on_off", 2, &str);
			if (ret) {
				pr_err("faild to get bl_gpio_port_off!\n");
				bl_config.gpio_off = BL_GPIO_OUTPUT_LOW;
			} else {
				if (strcmp(str, "2") == 0) {
					bl_config.gpio_off = BL_GPIO_INPUT;
				} else if (strcmp(str, "1") == 0) {
					bl_config.gpio_off =
						BL_GPIO_OUTPUT_HIGH;
				} else {
					bl_config.gpio_off = BL_GPIO_OUTPUT_LOW;
				}
			}
			PRINT("bl gpio_on=%u, bl gpio_off=%u\n",
				bl_config.gpio_on, bl_config.gpio_off);
			ret = of_property_read_u32_array(pdev->dev.of_node,
					"bl_gpio_dim_max_min", &bl_para[0], 2);
			if (ret) {
				pr_err("faild to get bl_gpio_dim_max_min\n");
				bl_config.dim_max = 0x0;
				bl_config.dim_min = 0xf;
			} else {
				bl_config.dim_max = bl_para[0];
				bl_config.dim_min = bl_para[1];
			}
			PRINT("bl dim max=%u, min=%u\n",
				bl_config.dim_max, bl_config.dim_min);
		} else if ((bl_config.method == BL_CTL_PWM_NEGATIVE) ||
				(bl_config.method == BL_CTL_PWM_POSITIVE)) {
			ret = of_property_read_string_index(pdev->dev.of_node,
					"bl_pwm_port_gpio_used", 1, &str);
			if (ret) {
				pr_err("faild to get bl_pwm_port_gpio_used!\n");
				bl_config.pwm_gpio_used = 0;
			} else {
				if (strncmp(str, "1", 1) == 0)
					bl_config.pwm_gpio_used = 1;
				else
					bl_config.pwm_gpio_used = 0;
				PRINT("bl_pwm gpio_used: %u\n",
						bl_config.pwm_gpio_used);
			}
			if (bl_config.pwm_gpio_used == 1) {
				ret = of_property_read_string(pdev->dev.of_node,
					"bl_gpio_port_on_off", &str);
				if (ret) {
					pr_err("faild to get ");
					pr_err("bl_gpio_port_on_off!\n");
					if (chip_type == BL_CHIP_M6)
						str = "GPIOD_1";
					else if (chip_type >= BL_CHIP_M8)
						str = "GPIODV_28";
				}
				bl_gpio = bl_gpio_request(&pdev->dev, str);
				if (IS_ERR(bl_gpio)) {
					bl_config.gpio = bl_gpio;
					PRINT("bl gpio = %s\n", str);
				} else {
					bl_config.gpio = NULL;
				}
				ret = of_property_read_string_index(
					pdev->dev.of_node, "bl_gpio_port_on_off"
					, 1, &str);
				if (ret) {
					pr_err("faild to get ");
					pr_err("bl_gpio_port_on!\n");
					bl_config.gpio_on = BL_GPIO_OUTPUT_HIGH;
				} else {
					if (strcmp(str, "2") == 0) {
						bl_config.gpio_on =
							BL_GPIO_INPUT;
					} else if (strcmp(str, "0") == 0) {
						bl_config.gpio_on =
							BL_GPIO_OUTPUT_LOW;
					} else {
						bl_config.gpio_on =
							BL_GPIO_OUTPUT_HIGH;
					}
				}
				ret = of_property_read_string_index(
					pdev->dev.of_node,
					"bl_gpio_port_on_off", 2, &str);
				if (ret) {
					pr_err("faild to get ");
					pr_err("bl_gpio_port_off!\n");
					bl_config.gpio_off = BL_GPIO_OUTPUT_LOW;
				} else {
					if (strcmp(str, "2") == 0) {
						bl_config.gpio_off =
								BL_GPIO_INPUT;
					} else if (strcmp(str, "1") == 0) {
						bl_config.gpio_off =
							BL_GPIO_OUTPUT_HIGH;
					} else {
						bl_config.gpio_off =
							BL_GPIO_OUTPUT_LOW;
					}
				}
				PRINT("gpio_on=%u, gpio_off=%u\n",
					bl_config.gpio_on, bl_config.gpio_off);
			}
			ret = of_property_read_string_index(pdev->dev.of_node,
					"bl_pwm_port_gpio_used", 0, &str);
			if (ret) {
				pr_err("faild to get bl_pwm_port_gpio_used!\n");
				if (chip_type == BL_CHIP_M6)
					bl_config.pwm_port = BL_PWM_D;
				else if (chip_type == BL_CHIP_M8)
					bl_config.pwm_port = BL_PWM_C;
				else if (chip_type == BL_CHIP_M8B)
					bl_config.pwm_port = BL_PWM_MAX;
			} else {
				if (strcmp(str, "PWM_A") == 0) {
					bl_config.pwm_port = BL_PWM_A;
				} else if (strcmp(str, "PWM_B") == 0) {
					bl_config.pwm_port = BL_PWM_B;
				} else if (strcmp(str, "PWM_C") == 0) {
					bl_config.pwm_port = BL_PWM_C;
				} else if (strcmp(str, "PWM_D") == 0) {
					bl_config.pwm_port = BL_PWM_D;
				} else if (strcmp(str, "PWM_E") == 0) {
					if (chip_type >= BL_CHIP_M8)
						bl_config.pwm_port = BL_PWM_E;
				} else if (strcmp(str, "PWM_F") == 0) {
					if (chip_type >= BL_CHIP_M8)
						bl_config.pwm_port = BL_PWM_F;
				} else {
					bl_config.pwm_port = BL_PWM_MAX;
				}
				PRINT("bl pwm_port: %s(%u)\n",
					str, bl_config.pwm_port);
			}
			ret = of_property_read_u32(pdev->dev.of_node,
					"bl_pwm_freq", &val);
			if (ret) {
				if (chip_type == BL_CHIP_M6)
					pwm_freq = 1000;
				else if (chip_type == BL_CHIP_M8)
					pwm_freq = 300000;
				pr_err("faild to get bl_pwm_freq,");
				pr_err("default set to %u\n", pwm_freq);
			} else {
				pwm_freq = ((val >= (FIN_FREQ * 500)) ?
						(FIN_FREQ * 500) : val);
			}
			for (i = 0; i < 0x7f; i++) {
				pwm_pre_div = i;
				pwm_cnt = FIN_FREQ * 1000 /
					(pwm_freq * (pwm_pre_div + 1)) - 2;
				if (pwm_cnt <= 0xffff)
					break;
			}
			bl_config.pwm_cnt = pwm_cnt;
			bl_config.pwm_pre_div = pwm_pre_div;
			PRINT("bl pwm_frequency=%u, cnt=%u, div=%u\n",
			pwm_freq, bl_config.pwm_cnt, bl_config.pwm_pre_div);
			ret = of_property_read_u32_array(pdev->dev.of_node,
				"bl_pwm_duty_max_min", &bl_para[0], 2);
			if (ret) {
				pr_err("faild to get bl_pwm_duty_max_min\n");
				bl_para[0] = 100;
				bl_para[1] = 20;
			}
			bl_config.pwm_max = (bl_config.pwm_cnt *
							bl_para[0] / 100);
			bl_config.pwm_min = (bl_config.pwm_cnt *
							bl_para[1] / 100);
			PRINT("bl pwm_duty max=%u%%, min=%u%%\n",
				bl_para[0], bl_para[1]);
		} else if (bl_config.method == BL_CTL_PWM_COMBO) {
			ret = of_property_read_u32(pdev->dev.of_node,
				"bl_pwm_combo_high_low_level_switch", &val);
			if (ret) {
				pr_err("faild to get ");
				pr_err("bl_pwm_combo_high_low_level_switch\n");
				val = bl_config.level_mid;
			}
			if (val > bl_config.level_mid) {
				val = ((val - bl_config.level_mid) *
			(bl_config.level_max - bl_config.level_mid_mapping)) /
				(bl_config.level_max - bl_config.level_mid) +
					bl_config.level_mid_mapping;
			} else {
				val = ((val - bl_config.level_min) *
			(bl_config.level_mid_mapping - bl_config.level_min)) /
				(bl_config.level_mid - bl_config.level_min) +
					bl_config.level_min;
			}
			bl_config.combo_level_switch = val;
			PRINT("bl pwm_combo level switch =%u\n",
					bl_config.combo_level_switch);
			ret = of_property_read_string_index(pdev->dev.of_node,
				"bl_pwm_combo_high_port_method", 0, &str);
			if (ret) {
				pr_err("faild to get ");
				pr_err("bl_pwm_combo_high_port_method!\n");
				str = "PWM_C";
				bl_config.combo_high_port = BL_PWM_C;
			} else {
				if (strcmp(str, "PWM_A") == 0) {
					bl_config.combo_high_port = BL_PWM_A;
				} else if (strcmp(str, "PWM_B") == 0) {
					bl_config.combo_high_port = BL_PWM_B;
				} else if (strcmp(str, "PWM_C") == 0) {
					bl_config.combo_high_port = BL_PWM_C;
				} else if (strcmp(str, "PWM_D") == 0) {
					bl_config.combo_high_port = BL_PWM_D;
				} else if (strcmp(str, "PWM_E") == 0) {
					if (chip_type >= BL_CHIP_M8)
						bl_config.pwm_port = BL_PWM_E;
				} else if (strcmp(str, "PWM_F") == 0) {
					if (chip_type >= BL_CHIP_M8)
						bl_config.pwm_port = BL_PWM_F;
				} else {
					bl_config.combo_high_port = BL_PWM_MAX;
				}
			}
			PRINT("bl pwm_combo high port: %s(%u)\n",
				str, bl_config.combo_high_port);
			ret = of_property_read_string_index(pdev->dev.of_node,
				"bl_pwm_combo_high_port_method", 1, &str);
			if (ret) {
				pr_err("faild to get ");
				pr_err("bl_pwm_combo_high_port_method!\n");
				str = "1";
				bl_config.combo_high_method =
						BL_CTL_PWM_NEGATIVE;
			} else {
				if (strncmp(str, "1", 1) == 0)
					bl_config.combo_high_method =
							BL_CTL_PWM_NEGATIVE;
				else
					bl_config.combo_high_method =
							BL_CTL_PWM_POSITIVE;
			}
			PRINT("bl pwm_combo high method: %s(%u)\n",
			bl_ctrl_method_table[bl_config.combo_high_method],
				bl_config.combo_high_method);
			ret = of_property_read_string_index(pdev->dev.of_node,
				"bl_pwm_combo_low_port_method", 0, &str);
			if (ret) {
				pr_err("faild to get ");
				pr_err("bl_pwm_combo_low_port_method!\n");
				str = "PWM_D";
				bl_config.combo_low_port = BL_PWM_D;
			} else {
				if (strcmp(str, "PWM_A") == 0) {
					bl_config.combo_low_port = BL_PWM_A;
				} else if (strcmp(str, "PWM_B") == 0) {
					bl_config.combo_low_port = BL_PWM_B;
				} else if (strcmp(str, "PWM_C") == 0) {
					bl_config.combo_low_port = BL_PWM_C;
				} else if (strcmp(str, "PWM_D") == 0) {
					bl_config.combo_low_port = BL_PWM_D;
				} else if (strcmp(str, "PWM_E") == 0) {
					if (chip_type >= BL_CHIP_M8)
						bl_config.pwm_port = BL_PWM_E;
				} else if (strcmp(str, "PWM_F") == 0) {
					if (chip_type >= BL_CHIP_M8)
						bl_config.pwm_port = BL_PWM_F;
				} else {
					bl_config.combo_low_port = BL_PWM_MAX;
				}
			}
			PRINT("bl pwm_combo high port: %s(%u)\n",
				str, bl_config.combo_low_port);
			ret = of_property_read_string_index(pdev->dev.of_node,
				"bl_pwm_combo_low_port_method", 1, &str);
			if (ret) {
				pr_err("faild to get ");
				pr_err("bl_pwm_combo_low_port_method!\n");
				str = "1";
				bl_config.combo_low_method =
						BL_CTL_PWM_NEGATIVE;
			} else {
				if (strncmp(str, "1", 1) == 0)
					bl_config.combo_low_method =
							BL_CTL_PWM_NEGATIVE;
				else
					bl_config.combo_low_method =
							BL_CTL_PWM_POSITIVE;
			}
			PRINT("bl pwm_combo low method: %s(%u)\n",
			bl_ctrl_method_table[bl_config.combo_low_method],
			bl_config.combo_low_method);
			ret = of_property_read_u32_array(pdev->dev.of_node,
			"bl_pwm_combo_high_freq_duty_max_min", &bl_para[0], 3);
			if (ret) {
				pr_err("faild to get ");
				pr_err("bl_pwm_combo_high_freq_duty_max_min\n");
				bl_para[0] = 300000;
				bl_para[1] = 100;
				bl_para[2] = 50;
			}
			pwm_freq = ((bl_para[0] >=
			(FIN_FREQ * 500)) ? (FIN_FREQ * 500) : bl_para[0]);
			for (i = 0; i < 0x7f; i++) {
				pwm_pre_div = i;
				pwm_cnt = FIN_FREQ * 1000 /
					(pwm_freq * (pwm_pre_div + 1)) - 2;
				if (pwm_cnt <= 0xffff)
					break;
			}
			bl_config.combo_high_cnt = pwm_cnt;
			bl_config.combo_high_pre_div = pwm_pre_div;
			bl_config.combo_high_duty_max =
				(bl_config.combo_high_cnt * bl_para[1] / 100);
			bl_config.combo_high_duty_min =
				(bl_config.combo_high_cnt * bl_para[2] / 100);
			PRINT("bl pwm_combo high freq=%uHz, ", pwm_freq);
			PRINT("duty_max=%u%%, duty_min=%u%%\n",
				bl_para[1], bl_para[2]);
			ret = of_property_read_u32_array(pdev->dev.of_node,
			"bl_pwm_combo_low_freq_duty_max_min", &bl_para[0], 3);
			if (ret) {
				pr_err("faild to get ");
				pr_err("bl_pwm_combo_low_freq_duty_max_min\n");
				bl_para[0] = 1000;
				bl_para[1] = 100;
				bl_para[2] = 50;
			}
			pwm_freq = ((bl_para[0] >= (FIN_FREQ * 500)) ?
					(FIN_FREQ * 500) : bl_para[0]);
			for (i = 0; i < 0x7f; i++) {
				pwm_pre_div = i;
				pwm_cnt = FIN_FREQ * 1000 /
					(pwm_freq * (pwm_pre_div + 1)) - 2;
				if (pwm_cnt <= 0xffff)
					break;
			}
			bl_config.combo_low_cnt = pwm_cnt;
			bl_config.combo_low_pre_div = pwm_pre_div;
			bl_config.combo_low_duty_max =
				(bl_config.combo_low_cnt * bl_para[1] / 100);
			bl_config.combo_low_duty_min =
				(bl_config.combo_low_cnt * bl_para[2] / 100);
			PRINT("bl pwm_combo low freq=%uHz,", pwm_freq);
			PRINT(" duty_max=%u%%, duty_min=%u%%\n",
					bl_para[1], bl_para[2]);
		}
		bl_config.p = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(bl_config.p))
			pr_err("get backlight pinmux error.\n");
	}
	return ret;
}


static enum bl_chip_type bl_check_chip(void)
{
	u32 cpu_type;
	enum bl_chip_type bl_chip = BL_CHIP_MAX;

	cpu_type = get_cpu_type();
	switch (cpu_type) {
	case MESON_CPU_MAJOR_ID_M6:
		bl_chip = BL_CHIP_M6;
		break;
	case MESON_CPU_MAJOR_ID_M8:
		bl_chip = BL_CHIP_M8;
		break;
	case MESON_CPU_MAJOR_ID_M8B:
		bl_chip = BL_CHIP_M8B;
		break;
	case MESON_CPU_MAJOR_ID_M8M2:
		bl_chip = BL_CHIP_M8M2;
		break;
	default:
		bl_chip = BL_CHIP_MAX;
	}

	PRINT("BL driver check chip : %s\n", bl_chip_table[bl_chip]);
	return bl_chip;
}



struct class *bl_debug_class;
static ssize_t bl_status_read(struct class *class,
			struct class_attribute *attr, char *buf)
{
	pr_info("read backlight status: bl_real_status=%s(%d)",
			(bl_real_status ? "ON" : "OFF"), bl_real_status);
	pr_info("bl_status=%s(%d), bl_level=%d",
		((bl_status == 0) ? "OFF" : ((bl_status == 1) ?
		"lcd_on_bl_off" : "lcd_on_bl_on")), bl_status, bl_level);
	return sprintf(buf, "\n");
}

static struct class_attribute bl_debug_class_attrs[] = {
	__ATTR(status,  S_IRUGO | S_IWUSR, bl_status_read, NULL),
};

static int creat_bl_attr(void)
{
	int i;
	bl_debug_class = class_create(THIS_MODULE, "lcd_bl");
	if (IS_ERR(bl_debug_class)) {
		pr_err("create lcd_bl debug class fail\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(bl_debug_class_attrs); i++) {
		if (class_create_file(bl_debug_class,
				&bl_debug_class_attrs[i])) {
			pr_err("create lcd_bl debug attribute %s fail\n",
				bl_debug_class_attrs[i].attr.name);
		}
	}
	return 0;
}

static int remove_bl_attr(void)
{
	int i;
	if (bl_debug_class == NULL)
		return -1;

	for (i = 0; i < ARRAY_SIZE(bl_debug_class_attrs); i++)
		class_remove_file(bl_debug_class, &bl_debug_class_attrs[i]);
	class_destroy(bl_debug_class);
	bl_debug_class = NULL;
	return 0;
}


static int aml_bl_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	const struct bl_platform_data *pdata;
	struct backlight_device *bldev;
	struct aml_bl *amlbl;
	int retval;
	DTRACE();
	chip_type = bl_check_chip();
	amlbl = kzalloc(sizeof(struct aml_bl), GFP_KERNEL);
	if (!amlbl) {
		pr_err(KERN_ERR "%s() kzalloc error\n", __func__);
		return -ENOMEM;
	}
	amlbl->pdev = pdev;
#ifdef CONFIG_USE_OF
	pdata = bl_get_driver_data(pdev);
#else
	pdata = pdev->dev.platform_data;
#endif
	if (!pdata) {
		pr_err("%s() missing platform data\n", __func__);
		retval = -ENODEV;
		goto err;
	}

#ifdef CONFIG_USE_OF
	_get_backlight_config(pdev);
#endif

	amlbl->pdata = pdata;

	PRINT("%s() bl_init=%p\n", __func__, pdata->bl_init);
	PRINT("%s() power_on_bl=%p\n", __func__, pdata->power_on_bl);
	PRINT("%s() power_off_bl=%p\n", __func__, pdata->power_off_bl);
	PRINT("%s() set_bl_level=%p\n", __func__, pdata->set_bl_level);
	PRINT("%s() get_bl_level=%p\n", __func__, pdata->get_bl_level);
	PRINT("%s() max_brightness=%d\n", __func__, pdata->max_brightness);
	PRINT("%s() dft_brightness=%d\n", __func__, pdata->dft_brightness);

	memset(&props, 0, sizeof(struct backlight_properties));
#ifdef CONFIG_USE_OF
	props.max_brightness = (bl_config.level_max > 0 ?
					bl_config.level_max : BL_LEVEL_MAX);
#else
	props.max_brightness = (pdata->max_brightness > 0 ?
					pdata->max_brightness : BL_LEVEL_MAX);
#endif
	props.type = BACKLIGHT_RAW;
	bldev = backlight_device_register("aml-bl", &pdev->dev,
					amlbl, &aml_bl_ops, &props);
	if (IS_ERR(bldev)) {
		pr_err("failed to register backlight\n");
		retval = PTR_ERR(bldev);
		goto err;
	}
	amlbl->bldev = bldev;
	platform_set_drvdata(pdev, amlbl);
	bldev->props.power = FB_BLANK_UNBLANK;
#ifdef CONFIG_USE_OF
	bldev->props.brightness = (bl_config.level_default > 0 ?
				bl_config.level_default : BL_LEVEL_DEFAULT);
#else
	bldev->props.brightness = (pdata->dft_brightness > 0 ?
				pdata->dft_brightness : BL_LEVEL_DEFAULT);
#endif

	INIT_DELAYED_WORK(&bl_config.bl_delayed_work, bl_delayd_on);
	bl_config.workqueue = create_workqueue("bl_power_on_queue");
	if (bl_config.workqueue == NULL)
		pr_err("can't create bl work queue\n");
	if (pdata->bl_init)
		pdata->bl_init();
	if (pdata->power_on_bl)
		pdata->power_on_bl();
	if (pdata->set_bl_level)
		pdata->set_bl_level(bldev->props.brightness);

	creat_bl_attr();

	pr_info("aml bl probe OK.\n");
	return 0;
err:
	kfree(amlbl);
	return retval;
}

static int __exit aml_bl_remove(struct platform_device *pdev)
{
	struct aml_bl *amlbl = platform_get_drvdata(pdev);
	DTRACE();
	remove_bl_attr();

	if (bl_config.workqueue)
		destroy_workqueue(bl_config.workqueue);
	backlight_device_unregister(amlbl->bldev);
	platform_set_drvdata(pdev, NULL);
	kfree(amlbl);
	return 0;
}

static struct platform_driver aml_bl_driver = {
	.driver = {
		.name = "aml-bl",
		.owner = THIS_MODULE,
#ifdef CONFIG_USE_OF
		.of_match_table = backlight_dt_match,
#endif
	},
	.probe = aml_bl_probe,
	.remove = __exit_p(aml_bl_remove),
};

static int __init aml_bl_init(void)
{
	DTRACE();
	if (platform_driver_register(&aml_bl_driver)) {
		PRINT("failed to register bl driver module\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit aml_bl_exit(void)
{
	DTRACE();
	platform_driver_unregister(&aml_bl_driver);
}

module_init(aml_bl_init);
module_exit(aml_bl_exit);

MODULE_DESCRIPTION("Meson Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
