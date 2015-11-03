/*
 * drivers/amlogic/display/backlight/aml_bl.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
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
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/vout/aml_bl.h>
#ifdef CONFIG_AML_LCD
#include <linux/amlogic/vout/lcd_notify.h>
#endif
#ifdef CONFIG_AML_BL_EXTERN
#include <linux/amlogic/vout/aml_bl_extern.h>
#endif
#include "aml_bl_reg.h"

/* #define AML_BACKLIGHT_DEBUG */
static int bl_debug_print_flag;

static enum bl_chip_type_e bl_chip_type = BL_CHIP_MAX;
static struct aml_bl_drv_s *bl_drv;

static DEFINE_MUTEX(bl_power_mutex);
static DEFINE_MUTEX(bl_level_mutex);

const char *bl_chip_table[] = {
	"M6",
	"M8",
	"M8b",
	"M8M2",
	"G9TV",
	"G9BB",
	"GXTVBB",
	"invalid",
};

const char *bl_ctrl_method_table[] = {
	"gpio",
	"pwm",
	"local_diming",
	"extern",
	"null"
};
static struct bl_config_s bl_config = {
	.level_default = 128,
	.level_mid = 128,
	.level_mid_mapping = 128,
	.level_min = 10,
	.level_max = 255,
	.power_on_delay = 100,
	.power_off_delay = 10,
	.method = BL_CTRL_MAX,
};
#if 0
static unsigned int pwm_misc[6][5] = {
	/* pwm_reg,         div bit, clk_sel bit, clk_en bit, pwm_en bit*/
	{PWM_MISC_REG_AB,   8,       4,           15,         0,},
	{PWM_MISC_REG_AB,   16,      6,           23,         1,},
	{PWM_MISC_REG_CD,   8,       4,           15,         0,},
	{PWM_MISC_REG_CD,   16,      6,           23,         1,},
	{PWM_MISC_REG_EF,   8,       4,           15,         0,},
	{PWM_MISC_REG_EF,   16,      6,           23,         1,},
};
#endif
static unsigned int pwm_reg[6] = {
	PWM_PWM_A,
	PWM_PWM_B,
	PWM_PWM_C,
	PWM_PWM_D,
	PWM_PWM_E,
	PWM_PWM_F,
};

static enum bl_chip_type_e aml_bl_check_chip(void)
{
	unsigned int cpu_type;
	enum bl_chip_type_e bl_chip = BL_CHIP_MAX;

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
	case MESON_CPU_MAJOR_ID_MG9TV:
		bl_chip = BL_CHIP_G9TV;
		break;
	case MESON_CPU_MAJOR_ID_GXTVBB:
		bl_chip = BL_CHIP_GXTVBB;
		break;
	default:
		bl_chip = BL_CHIP_MAX;
	}

	if (bl_debug_print_flag)
		BLPR("BL driver check chip : %s\n", bl_chip_table[bl_chip]);
	return bl_chip;
}

static int aml_bl_check_driver(void)
{
	if (bl_drv == NULL) {
		BLPR("no bl driver\n");
		return -1;
	}

	return 0;
}

struct aml_bl_drv_s *aml_bl_get_driver(void)
{
	if (bl_drv == NULL)
		BLPR("no bl driver");

	return bl_drv;
}

static void bl_power_ctrl_gpio(struct gpio_desc *gpio, unsigned int value)
{
	switch (value) {
	case BL_GPIO_OUTPUT_LOW:
	case BL_GPIO_OUTPUT_HIGH:
		bl_gpio_output(gpio, value);
		break;
	case BL_GPIO_INPUT:
		bl_gpio_input(gpio);
		break;
	default:
		break;
	}
}
#if 0
static void bl_power_on_pwm(unsigned int port, unsigned int pre_div)
{
	switch (port) {
	case BL_PWM_A:
	case BL_PWM_B:
	case BL_PWM_C:
	case BL_PWM_D:
		/* pwm clk_div */
		bl_cbus_setb(pwm_misc[port][0], pre_div, pwm_misc[port][1], 7);
		/* pwm clk_sel */
		bl_cbus_setb(pwm_misc[port][0], 0, pwm_misc[port][2], 2);
		/* pwm clk_en */
		bl_cbus_setb(pwm_misc[port][0], 1, pwm_misc[port][3], 1);
		/* pwm enable */
		bl_cbus_setb(pwm_misc[port][0], 1, pwm_misc[port][4], 1);
		break;
	case BL_PWM_E:
	case BL_PWM_F:
		if (bl_chip_type >= BL_CHIP_M8) {
			/* pwm clk_div */
			bl_cbus_setb(pwm_misc[port][0], pre_div,
				pwm_misc[port][1], 7);
			/* pwm clk_sel */
			bl_cbus_setb(pwm_misc[port][0], 0,
				pwm_misc[port][2], 2);
			/* pwm clk_en */
			bl_cbus_setb(pwm_misc[port][0], 1,
				pwm_misc[port][3], 1);
			/* pwm enable */
			bl_cbus_setb(pwm_misc[port][0], 1,
				pwm_misc[port][4], 1);
		}
		break;
	default:
		break;
	}
}

static void bl_power_off_pwm(unsigned char port)
{
	switch (port) {
	case BL_PWM_A:
	case BL_PWM_B:
	case BL_PWM_C:
	case BL_PWM_D:
		/* pwm disable */
		bl_cbus_setb(pwm_misc[port][0], 0, pwm_misc[port][4], 1);
		break;
	case BL_PWM_E:
	case BL_PWM_F:
		if (bl_chip_type >= BL_CHIP_M8) {
			bl_cbus_setb(pwm_misc[port][0], 0,
				pwm_misc[port][4], 1);
		}
		break;
	default:
		break;
	}
}
#endif
static void bl_power_on(void)
{
	struct bl_config_s *bconf;
#ifdef CONFIG_AML_BL_EXTERN
	struct aml_bl_extern_driver_s *bl_ext;
	int ret;
#endif

	if (aml_bl_check_driver())
		return;

	mutex_lock(&bl_power_mutex);

	bconf = bl_drv->bconf;
	if ((bl_drv->level == 0) ||
		(bl_drv->state & BL_STATE_BL_ON)) {
		goto exit_power_on_bl;
	}

	switch (bconf->method) {
	case BL_CTRL_GPIO:
		if (!IS_ERR(bconf->gpio))
			bl_power_ctrl_gpio(bconf->gpio, bconf->gpio_on);
		break;
	case BL_CTRL_PWM:
#if 0
		bl_power_on_pwm(bconf->pwm_port, bconf->pwm_pre_div);

		if (bconf->pwm_gpio)
			devm_gpiod_put(bl_drv->dev, bconf->pwm_gpio);
		if (bconf->pwm_port == BL_PWM_VS) {
			bconf->pin = devm_pinctrl_get_select(bl_drv->dev,
				"pwm_vs");
		} else {
			bconf->pin = devm_pinctrl_get_select(bl_drv->dev,
				"default");
		}
		if (IS_ERR(bconf->pin))
			BLPR("set backlight pinmux error\n");
		if (bconf->pwm_on_delay > 0)
			mdelay(bconf->pwm_on_delay);
		if (bconf->gpio_used) {
			if (!IS_ERR(bconf->gpio))
				bl_power_ctrl_gpio(bconf->gpio, bconf->gpio_on);
		}
#endif
		break;
#ifdef CONFIG_AML_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMING:
		aml_bl_on_local_diming();
		if (bconf->gpio_used) {
			if (!IS_ERR(bconf->gpio))
				bl_power_ctrl_gpio(bconf->gpio, bconf->gpio_on);
		}
		break;
#endif
#ifdef CONFIG_AML_BL_EXTERN
	case BL_CTRL_EXTERN:
		bl_ext = aml_bl_extern_get_driver();
		if (bl_ext == NULL) {
			BLPR("no bl_extern driver\n");
		} else {
			if (bl_ext->power_on) {
				ret = bl_ext->power_on();
				if (ret) {
					BLPR("bl_extern: power on error\n");
					goto exit_power_on_bl;
				}
			} else {
				BLPR("bl_extern: power on is null\n");
			}
		}
		break;
#endif
	default:
		BLPR("wrong backlight control method\n");
		goto exit_power_on_bl;
		break;
	}
	bl_drv->state |= BL_STATE_BL_ON;
	BLPR("backlight power on\n");

exit_power_on_bl:
	mutex_unlock(&bl_power_mutex);
}

static void bl_power_off(void)
{
	struct bl_config_s *bconf;
#ifdef CONFIG_AML_BL_EXTERN
	struct aml_bl_extern_driver_s *bl_ext;
	int ret;
#endif

	if (aml_bl_check_driver())
		return;
	mutex_lock(&bl_power_mutex);

	bconf = bl_drv->bconf;
	if ((bl_drv->state & BL_STATE_BL_ON) == 0) {
		mutex_unlock(&bl_power_mutex);
		return;
	}

	if (bconf->power_off_delay > 0)
		mdelay(bconf->power_off_delay);
	switch (bconf->method) {
	case BL_CTRL_GPIO:
		if (!IS_ERR(bconf->gpio))
			bl_gpio_output(bconf->gpio, bconf->gpio_off);
		break;
	case BL_CTRL_PWM:
#if 0
		BLPR("test gpio: %p\n", bconf->gpio);
		if (bconf->gpio_used) {
			if (!IS_ERR(bconf->gpio)) {
				bl_power_ctrl_gpio(bconf->gpio,
					bconf->gpio_off);
			}
		}
		if (bconf->pwm_off_delay > 0)
			mdelay(bconf->pwm_off_delay);
		bl_power_off_pwm(bconf->pwm_port);
		if (!IS_ERR(bconf->pin))
			devm_pinctrl_put(bconf->pin);
		bconf->pwm_gpio = devm_gpiod_get(bl_drv->dev, "bl_pwm");
		BLPR("test pwm_gpio: %p\n", bconf->pwm_gpio);
		if (!IS_ERR(bconf->pwm_gpio)) {
			bl_power_ctrl_gpio(bconf->pwm_gpio,
				bconf->pwm_gpio_off);
		}
#endif
		break;
#ifdef CONFIG_AML_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMING:
		if (bconf->gpio_used) {
			if (!IS_ERR(bconf->gpio)) {
				bl_power_ctrl_gpio(bconf->gpio,
					bconf->gpio_off);
			}
		}
		aml_bl_off_local_diming();
		break;
#endif
#ifdef CONFIG_AML_BL_EXTERN
	case BL_CTRL_EXTERN:
		bl_ext = aml_bl_extern_get_driver();
		if (bl_ext == NULL) {
			BLPR("no bl_extern driver\n");
		} else {
			if (bl_ext->power_off) {
				ret = bl_ext->power_off();
				if (ret)
					BLPR("bl_extern: power off error\n");
			} else {
				BLPR("bl_extern: power off is null\n");
			}
		}
		break;
#endif
	default:
		BLPR("wrong backlight control method\n");
		break;
	}
	bl_drv->state &= ~BL_STATE_BL_ON;
	BLPR("backlight power off\n");
	mutex_unlock(&bl_power_mutex);
}

static void bl_set_level_pwm(unsigned int level)
{
	unsigned int pwm_hi = 0, pwm_lo = 0;
	unsigned int min = bl_drv->bconf->level_min;
	unsigned int max = bl_drv->bconf->level_max;
	unsigned int pwm_max = bl_drv->bconf->pwm_max;
	unsigned int pwm_min = bl_drv->bconf->pwm_min;
	unsigned int port;
	unsigned int vs[4], ve[4], sw, n, i;
	struct bl_config_s *bconf = bl_drv->bconf;

	level = (pwm_max - pwm_min) * (level - min) / (max - min) + pwm_min;
	switch (bconf->pwm_method) {
	case BL_PWM_POSITIVE:
		pwm_hi = level;
		pwm_lo = bconf->pwm_cnt - level;
		break;
	case BL_PWM_NEGATIVE:
		pwm_lo = level;
		pwm_hi = bconf->pwm_cnt - level;
		break;
	default:
		break;
	}

	port = bl_drv->bconf->pwm_port;
	switch (port) {
	case BL_PWM_A:
	case BL_PWM_B:
	case BL_PWM_C:
	case BL_PWM_D:
		bl_cbus_write(pwm_reg[port], (pwm_hi << 16) | pwm_lo);
		break;
	case BL_PWM_E:
	case BL_PWM_F:
		if (bl_chip_type >= BL_CHIP_M8)
			bl_cbus_write(pwm_reg[port], (pwm_hi << 16) | pwm_lo);
		break;
	case BL_PWM_VS:
		memset(vs, 0, sizeof(unsigned int) * 4);
		memset(ve, 0, sizeof(unsigned int) * 4);
		n = bconf->pwm_freq;
		sw = (bconf->pwm_cnt * 10 / n + 5) / 10;
		pwm_hi = (pwm_hi * 10 / n + 5) / 10;
		pwm_hi = (pwm_hi > 1) ? pwm_hi : 1;
		if (bl_debug_print_flag)
			BLPR("n=%d, sw=%d, pwm_high=%d\n", n, sw, pwm_hi);
		for (i = 0; i < n; i++) {
			vs[i] = 1 + (sw * i);
			ve[i] = vs[i] + pwm_hi - 1;
			if (bl_debug_print_flag) {
				BLPR("vs[%d]=%d, ve[%d]=%d\n",
					i, vs[i], i, ve[i]);
			}
		}
		bl_vcbus_write(VPU_VPU_PWM_V0, (ve[0] << 16) | (vs[0]));
		bl_vcbus_write(VPU_VPU_PWM_V1, (ve[1] << 16) | (vs[1]));
		bl_vcbus_write(VPU_VPU_PWM_V2, (ve[2] << 16) | (vs[2]));
		bl_vcbus_write(VPU_VPU_PWM_V3, (ve[3] << 16) | (vs[3]));
		break;
	default:
		break;
	}
}

#ifdef CONFIG_AML_BL_EXTERN
static void bl_set_level_extern(unsigned int level)
{
	struct aml_bl_extern_driver_s *bl_ext;
	int ret;

	bl_ext = aml_bl_extern_get_driver();
	if (bl_ext == NULL) {
		BLPR("no bl_extern driver\n");
	} else {
		if (bl_ext->set_level) {
			ret = bl_ext->set_level(level);
			if (ret)
				BLPR("bl_extern: set_level error\n");
		} else {
			BLPR("bl_extern: set_level is null\n");
		}
	}
}
#endif

static void aml_bl_set_level(unsigned int level)
{
	if (aml_bl_check_driver())
		return;

	if (bl_debug_print_flag) {
		BLPR("aml_bl_set_level=%u, last level=%u, state=0x%x\n",
			level, bl_drv->level, bl_drv->state);
	}

	/* level range check */
	if (level > bl_drv->bconf->level_max)
		level = bl_drv->bconf->level_max;
	if (level < bl_drv->bconf->level_min) {
		if (level < BL_LEVEL_OFF)
			level = 0;
		else
			level = bl_drv->bconf->level_min;
	}
	bl_drv->level = level;

	if (level == 0)
		return;

	switch (bl_drv->bconf->method) {
	case BL_CTRL_GPIO:
		break;
	case BL_CTRL_PWM:
		bl_set_level_pwm(level);
		break;
#ifdef CONFIG_AML_LOCAL_DIMMING
	case BL_CTRL_LOCAL_DIMING:
		bl_set_level_local_diming(level);
		break;
#endif
#ifdef CONFIG_AML_BL_TABLET_EXTERN
	case BL_CTRL_EXTERN:
		bl_set_level_extern(level);
		break;
#endif
	default:
		break;
	}
}

static unsigned int aml_bl_get_level(void)
{
	if (aml_bl_check_driver())
		return 0;

	BLPR("aml bl state: 0x%x\n", bl_drv->state);
	return bl_drv->level;
}

static int aml_bl_update_status(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	mutex_lock(&bl_level_mutex);
	if (brightness < 0)
		brightness = 0;
	else if (brightness > 255)
		brightness = 255;

	if ((bl_drv->state & BL_STATE_LCD_ON) == 0)
		brightness = 0;

	BLPR("aml_bl_update_status: %u, real brightness: %u, state: 0x%x\n",
		bd->props.brightness, brightness, bl_drv->state);
	if (brightness == 0) {
		if (bl_drv->state & BL_STATE_BL_ON)
			bl_power_off();
	} else {
		aml_bl_set_level(brightness);
		if ((bl_drv->state & BL_STATE_BL_ON) == 0)
			bl_power_on();
	}
	mutex_unlock(&bl_level_mutex);
	return 0;
}

static int aml_bl_get_brightness(struct backlight_device *bd)
{
	return aml_bl_get_level();
}

static const struct backlight_ops aml_bl_ops = {
	.get_brightness = aml_bl_get_brightness,
	.update_status  = aml_bl_update_status,
};

#ifdef CONFIG_OF
#if 0
static unsigned char bl_gpio_str_to_value(const char *str)
{
	unsigned char value;

	if ((strcmp(str, "output_low") == 0) || (strcmp(str, "0") == 0))
		value = BL_GPIO_OUTPUT_LOW;
	else if ((strcmp(str, "output_high") == 0) || (strcmp(str, "1") == 0))
		value = BL_GPIO_OUTPUT_HIGH;
	else if ((strcmp(str, "input") == 0) || (strcmp(str, "2") == 0))
		value = BL_GPIO_INPUT;

	return value;
}
#endif

static enum bl_pwm_port_e bl_pwm_str_to_pwm(const char *str)
{
	enum bl_pwm_port_e pwm_port;

	if (strcmp(str, "PWM_A") == 0) {
		pwm_port = BL_PWM_A;
	} else if (strcmp(str, "PWM_B") == 0) {
		pwm_port = BL_PWM_B;
	} else if (strcmp(str, "PWM_C") == 0) {
		pwm_port = BL_PWM_C;
	} else if (strcmp(str, "PWM_D") == 0) {
		pwm_port = BL_PWM_D;
	} else if (strcmp(str, "PWM_E") == 0) {
		if (bl_chip_type >= BL_CHIP_M8)
			pwm_port = BL_PWM_E;
		else
			pwm_port = BL_PWM_MAX;
	} else if (strcmp(str, "PWM_F") == 0) {
		if (bl_chip_type >= BL_CHIP_M8)
			pwm_port = BL_PWM_F;
		else
			pwm_port = BL_PWM_MAX;
	} else if (strcmp(str, "PWM_VS") == 0) {
		pwm_port = BL_PWM_VS;
	} else {
		pwm_port = BL_PWM_MAX;
	}

	return pwm_port;
}

static void bl_pwm_parameters_init(struct bl_config_s *bconf)
{
	unsigned int freq, cnt, pre_div;
	int i;

	freq = bconf->pwm_freq;
	switch (bconf->pwm_port) {
	case BL_PWM_VS:
		cnt = bl_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;
		bconf->pwm_cnt = cnt;
		bconf->pwm_pre_div = 0;
		break;
	default:
		freq = ((freq >= (BL_FIN_FREQ / 2)) ? (BL_FIN_FREQ / 2) : freq);
		for (i = 0; i < 0x7f; i++) {
			pre_div = i;
			cnt = BL_FIN_FREQ / (freq * (pre_div + 1)) - 2;
			if (cnt <= 0xffff)
				break;
		}
		bconf->pwm_freq = freq;
		bconf->pwm_cnt = cnt;
		bconf->pwm_pre_div = pre_div;
		break;
	}
	bconf->pwm_max = (bconf->pwm_cnt * bconf->pwm_duty_max / 100);
	bconf->pwm_min = (bconf->pwm_cnt * bconf->pwm_duty_min / 100);
}

static void aml_bl_config_print(struct bl_config_s *bconf)
{
	if (bl_debug_print_flag == 0)
		return;

	BLPR("bl level default kernel=%u\n", bconf->level_default);
	BLPR("bl level max=%u, min=%u\n",
		bconf->level_max, bconf->level_min);
	BLPR("bl control_method: %s(%u)\n",
		bl_ctrl_method_table[bconf->method], bconf->method);
	BLPR("bl power on_delay: %ums, off_delay: %ums\n",
		bconf->power_on_delay, bconf->power_off_delay);

	BLPR("bl pwm_method: %d\n", bconf->pwm_method);
	BLPR("bl pwm_port: %u\n", bconf->pwm_port);
	switch (bconf->pwm_port) {
	case BL_PWM_VS:
		BLPR("bl pwm_freq: %u x vfreq\n", bconf->pwm_freq);
		BLPR("pwm_cnt = %u\n", bconf->pwm_cnt);
		break;
	default:
		BLPR("bl pwm_freq: %uHz\n", bconf->pwm_freq);
		BLPR("bl pwm_cnt=%u, pre_div=%u\n",
			bconf->pwm_cnt, bconf->pwm_pre_div);
		break;
	}
	BLPR("bl pwm_duty max: %u%%, min: %u%%\n",
		bconf->pwm_duty_max, bconf->pwm_duty_min);
	BLPR("bl pwm_gpio_off: %u\n", bconf->pwm_gpio_off);
	BLPR("bl pwm on_delay: %ums, off_delay: %ums\n",
		bconf->pwm_on_delay, bconf->pwm_off_delay);
}

static int aml_bl_get_config(struct bl_config_s *bconf,
		struct platform_device *pdev)
{
	int ret = 0;
	int val;
	const char *str;
	unsigned int bl_para[3];
	char bl_propname[20];
	int index;
	struct device_node *child;

	if (pdev->dev.of_node == NULL) {
		BLPR("no backlight of_node exist\n");
		return -1;
	}

	ret = of_property_read_string(pdev->dev.of_node, "mode", &str);
	if (ret) {
		BLPR("failed to get mode\n");
		str = "invalid";
	}
	if (strcmp(str, "tv") == 0)
		bl_drv->mode = BL_MODE_TV;
	else if (strcmp(str, "tablet") == 0)
		bl_drv->mode = BL_MODE_TABLET;
	else
		bl_drv->mode = BL_MODE_MAX;
	/* BLPR("mode: %d\n", bl_drv->mode); */

	/* select backlight by index */
	index = BL_INDEX_DEFAULT;
#ifdef CONFIG_AML_LCD
	aml_lcd_notifier_call_chain(LCD_EVENT_BACKLIGHT_SEL, &index);
#endif
	bl_drv->index = index;
	sprintf(bl_propname, "backlight_%d", index);
	child = of_get_child_by_name(pdev->dev.of_node, bl_propname);
	if (child == NULL) {
		BLPR("error: failed to get %s\n", bl_propname);
		return -1;
	}

	ret = of_property_read_string(child, "bl_name", &str);
	if (ret) {
		BLPR("failed to get bl_name\n");
		str = "backlight";
	}
	strcpy(bconf->name, str);
	BLPR("bl_name: %s\n", bconf->name);

	ret = of_property_read_u32_array(child, "bl_level_default_uboot_kernel",
		&bl_para[0], 2);
	if (ret) {
		BLPR("failed to get bl_level_default_uboot_kernel\n");
		bconf->level_default = BL_LEVEL_DEFAULT;
	} else {
		bconf->level_default = bl_para[1];
	}
	ret = of_property_read_u32_array(child, "bl_level_max_min",
		&bl_para[0], 2);
	if (ret) {
		BLPR("failed to get bl_level_max_min\n");
		bconf->level_min = BL_LEVEL_MIN;
		bconf->level_max = BL_LEVEL_MAX;
	} else {
		bconf->level_max = bl_para[0];
		bconf->level_min = bl_para[1];
	}

	ret = of_property_read_u32(child, "bl_ctrl_method", &val);
	if (ret) {
		BLPR("failed to get bl_ctrl_method\n");
		bconf->method = BL_CTRL_MAX;
	} else {
		val = (val >= BL_CTRL_MAX) ? BL_CTRL_MAX : val;
		bconf->method = (unsigned char)val;
	}
	ret = of_property_read_u32(child, "bl_en_gpio_used", &val);
	if (ret) {
		BLPR("failed to get bl_en_gpio_used\n");
		bconf->gpio_used = 0;
	} else {
		bconf->gpio_used = (unsigned char)val;
	}
	ret = of_property_read_u32_array(child, "bl_en_gpio_on_off",
		&bl_para[0], 2);
	if (ret) {
		BLPR("failed to get bl_en_gpio_on_off\n");
		bconf->gpio_on = BL_GPIO_OUTPUT_HIGH;
		bconf->gpio_on = BL_GPIO_OUTPUT_LOW;
	} else {
		bconf->gpio_on = bl_para[0];
		bconf->gpio_on = bl_para[1];
	}
	ret = of_property_read_u32_array(child, "bl_power_on_off_delay",
		&bl_para[0], 2);
	if (ret) {
		BLPR("failed to get bl_power_on_off_delay\n");
		bconf->power_on_delay = 100;
		bconf->power_off_delay = 100;
	} else {
		bconf->power_on_delay = bl_para[0];
		bconf->power_on_delay = bl_para[1];
	}

	ret = of_property_read_u32(child, "bl_pwm_method", &val);
	if (ret) {
		BLPR("failed to get bl_pwm_method\n");
		bconf->pwm_method = BL_PWM_METHOD_MAX;
	} else {
		val = (val >= BL_PWM_METHOD_MAX) ? BL_PWM_METHOD_MAX : val;
		bconf->pwm_method = (unsigned char)val;
	}
	ret = of_property_read_string_index(child, "bl_pwm_port", 0, &str);
	if (ret) {
		BLPR("failed to get bl_pwm_port\n");
		bconf->pwm_port = BL_PWM_MAX;
	} else {
		bconf->pwm_port = bl_pwm_str_to_pwm(str);
		BLPR("bl pwm_port: %s(%u)\n", str, bconf->pwm_port);
	}
	ret = of_property_read_u32(child, "bl_pwm_freq", &val);
	if (ret) {
		bconf->pwm_freq = BL_PWM_FREQ_DEFAULT;
		BLPR("failed to get bl_pwm_freq, default set to %uHz\n",
			bconf->pwm_freq);
	} else {
		bconf->pwm_freq = val;
	}
	ret = of_property_read_u32_array(child, "bl_pwm_duty_max_min",
		&bl_para[0], 2);
	if (ret) {
		BLPR("failed to get bl_pwm_duty_max_min\n");
		bconf->pwm_duty_max = 100;
		bconf->pwm_duty_min = 20;
	} else {
		bconf->pwm_duty_max = bl_para[0];
		bconf->pwm_duty_min = bl_para[1];
	}
	bl_pwm_parameters_init(bconf);

	ret = of_property_read_u32_array(child, "bl_pwm_on_off_delay",
		&bl_para[0], 2);
	if (ret) {
		BLPR("failed to get bl_pwm_on_off_delay\n");
		bconf->pwm_on_delay = 0;
		bconf->pwm_off_delay = 0;
	} else {
		bconf->pwm_on_delay = bl_para[0];
		bconf->pwm_off_delay = bl_para[1];
	}

	/* get & request pin ctrl */
	if (bconf->method == BL_CTRL_PWM) {
		if (bconf->pwm_port == BL_PWM_VS) {
			bconf->pin = devm_pinctrl_get_select(bl_drv->dev,
				"pwm_vs");
		} else {
			bconf->pin = devm_pinctrl_get_select(bl_drv->dev,
				"default");
		}
	}

	if (bconf->gpio_used)
		bconf->gpio = devm_gpiod_get(&pdev->dev, "bl_en");
	ret = of_property_read_u32(child, "bl_pwm_gpio_off", &val);
	if (ret) {
		BLPR("failed to get bl_pwm_gpio_off\n");
		bconf->pwm_gpio_off = BL_GPIO_OUTPUT_LOW;
	} else {
		bconf->pwm_gpio_off = val;
	}

	aml_bl_config_print(bconf);

	return ret;
}
#endif

/* ****************************************
 * lcd notify
 * **************************************** */
static void aml_bl_delayd_on(struct work_struct *work)
{
	if (aml_bl_check_driver())
		return;

	/* lcd power on backlight flag */
	bl_drv->state |= BL_STATE_LCD_ON;
	BLPR("%s: bl_level=%u, state=0x%x\n",
		__func__, bl_drv->level, bl_drv->state);
	aml_bl_update_status(bl_drv->bldev);
}

static int aml_bl_on_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct bl_config_s *bconf;

	BLPR("%s: 0x%lx\n", __func__, event);
	if ((event & LCD_EVENT_BL_ON) == 0)
		return NOTIFY_DONE;

	if (aml_bl_check_driver())
		return NOTIFY_DONE;

	bconf = bl_drv->bconf;
	/* lcd power on sequence control */
	if (bconf->method < BL_CTRL_MAX) {
		if (bl_drv->workqueue) {
			queue_delayed_work(bl_drv->workqueue,
				&bl_drv->bl_delayed_work,
				msecs_to_jiffies(bconf->power_on_delay));
		} else {
			BLPR("Warning: no bl workqueue\n");
			msleep(bconf->power_on_delay);
			/* lcd power on backlight flag */
			bl_drv->state |= BL_STATE_LCD_ON;
			aml_bl_update_status(bl_drv->bldev);
		}
	} else {
		BLPR("wrong backlight control method\n");
	}

	return NOTIFY_OK;
}

static int aml_bl_off_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	BLPR("%s: 0x%lx\n", __func__, event);
	if ((event & LCD_EVENT_BL_OFF) == 0)
		return NOTIFY_DONE;

	if (aml_bl_check_driver())
		return NOTIFY_DONE;

	bl_drv->state &= ~BL_STATE_LCD_ON;
	aml_bl_update_status(bl_drv->bldev);

	return NOTIFY_OK;
}

static struct notifier_block aml_bl_on_nb = {
	.notifier_call = aml_bl_on_notifier,
	.priority = LCD_PRIORITY_POWER_BL_ON,
};

static struct notifier_block aml_bl_off_nb = {
	.notifier_call = aml_bl_off_notifier,
	.priority = LCD_PRIORITY_POWER_BL_OFF,
};
/* **************************************** */

/* ****************************************
 * bl debug calss
 * **************************************** */
struct class *bl_debug_class;
static ssize_t bl_status_read(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct bl_config_s *bconf = bl_drv->bconf;

	return sprintf(buf, "read backlight status:\n"
		"index:        %d\n"
		"mode:         %d\n"
		"state:        0x%x\n"
		"level:        %d\n"
		"level_min:    %d\n"
		"level_max:    %d\n\n"
		"method:           %d\n"
		"power_on_delay:   %d\n"
		"power_off_delay:  %d\n"
		"gpio_on:          %d\n"
		"gpio_off:         %d\n"
		"pwm_method:       %d\n"
		"pwm_port:         %d\n"
		"pwm_freq:         %d\n"
		"pwm_duty_max:     %d\n"
		"pwm_duty_min:     %d\n"
		"pwm_gpio_off:     %d\n"
		"pwm_on_delay:     %d\n"
		"pwm_off_delay:    %d\n\n",
		bl_drv->index, bl_drv->mode, bl_drv->state, bl_drv->level,
		bconf->level_min, bconf->level_max,
		bconf->method, bconf->power_on_delay, bconf->power_off_delay,
		bconf->gpio_on, bconf->gpio_off,
		bconf->pwm_method, bconf->pwm_port, bconf->pwm_freq,
		bconf->pwm_duty_max, bconf->pwm_duty_min,
		bconf->pwm_gpio_off, bconf->pwm_on_delay, bconf->pwm_off_delay);
}

static ssize_t bl_debug_print_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "bl_debug_print_flag: %d\n", bl_debug_print_flag);
}

static ssize_t bl_debug_print_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp = 0;

	ret = sscanf(buf, "%d", &temp);
	bl_debug_print_flag = temp;

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static struct class_attribute bl_debug_class_attrs[] = {
	__ATTR(status, S_IRUGO | S_IWUSR, bl_status_read, NULL),
	__ATTR(print, S_IRUGO | S_IWUSR, bl_debug_print_show,
			bl_debug_print_store),
};

static int aml_bl_creat_class(void)
{
	int i;

	bl_debug_class = class_create(THIS_MODULE, "aml_bl");
	if (IS_ERR(bl_debug_class)) {
		BLPR("create aml_bl debug class fail\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(bl_debug_class_attrs); i++) {
		if (class_create_file(bl_debug_class,
				&bl_debug_class_attrs[i])) {
			BLPR("create aml_bl debug attribute %s fail\n",
				bl_debug_class_attrs[i].attr.name);
		}
	}
	return 0;
}

static int aml_bl_remove_class(void)
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
/* **************************************** */
static int aml_bl_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct backlight_device *bldev;
	struct bl_config_s *bconf;
	int ret;

#ifdef AML_BACKLIGHT_DEBUG
	bl_debug_print_flag = 1;
#else
	bl_debug_print_flag = 0;
#endif

	bl_chip_type = aml_bl_check_chip();
	bl_drv = kzalloc(sizeof(struct aml_bl_drv_s), GFP_KERNEL);
	if (!bl_drv) {
		BLPR("driver malloc error\n");
		return -ENOMEM;
	}

	bconf = &bl_config;
	bl_drv->dev = &pdev->dev;
	bl_drv->bconf = bconf;
#ifdef CONFIG_OF
	aml_bl_get_config(bconf, pdev);
#endif

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK; /* full on */
	props.max_brightness = (bconf->level_max > 0 ?
			bconf->level_max : BL_LEVEL_MAX);
	props.brightness = (bconf->level_default > 0 ?
			bconf->level_default : BL_LEVEL_DEFAULT);

	bldev = backlight_device_register(AML_BL_NAME, &pdev->dev,
					bl_drv, &aml_bl_ops, &props);
	if (IS_ERR(bldev)) {
		BLPR("failed to register backlight\n");
		ret = PTR_ERR(bldev);
		goto err;
	}
	bl_drv->bldev = bldev;
	/* platform_set_drvdata(pdev, bl_drv); */

	/* init workqueue */
	INIT_DELAYED_WORK(&bl_drv->bl_delayed_work, aml_bl_delayd_on);
	bl_drv->workqueue = create_workqueue("bl_power_on_queue");
	if (bl_drv->workqueue == NULL)
		BLPR("can't create bl work queue\n");

#ifdef CONFIG_AML_LCD
	ret = aml_lcd_notifier_register(&aml_bl_on_nb);
	if (ret)
		BLPR("register aml_bl_on_notifier failed\n");
	ret = aml_lcd_notifier_register(&aml_bl_off_nb);
	if (ret)
		BLPR("register aml_bl_off_notifier failed\n");
#endif
	aml_bl_creat_class();

	/* update bl status */
	bl_drv->state = (BL_STATE_LCD_ON | BL_STATE_BL_ON);
	aml_bl_update_status(bl_drv->bldev);

	BLPR("probe OK\n");
	return 0;
err:
	kfree(bl_drv);
	return ret;
}

static int __exit aml_bl_remove(struct platform_device *pdev)
{
	/*struct aml_bl *bl_drv = platform_get_drvdata(pdev);*/

	aml_bl_remove_class();

	if (bl_drv->workqueue)
		destroy_workqueue(bl_drv->workqueue);

	backlight_device_unregister(bl_drv->bldev);
#ifdef CONFIG_AML_LCD
	aml_lcd_notifier_unregister(&aml_bl_on_nb);
	aml_lcd_notifier_unregister(&aml_bl_off_nb);
#endif
	/* platform_set_drvdata(pdev, NULL); */
	kfree(bl_drv);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_bl_dt_match[] = {
	{
		.compatible = "amlogic, backlight",
	},
	{},
};
#endif

static struct platform_driver aml_bl_driver = {
	.driver = {
		.name = AML_BL_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_bl_dt_match,
#endif
	},
	.probe = aml_bl_probe,
	.remove = __exit_p(aml_bl_remove),
};

static int __init aml_bl_init(void)
{
	if (platform_driver_register(&aml_bl_driver)) {
		BLPR("failed to register bl driver module\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit aml_bl_exit(void)
{
	platform_driver_unregister(&aml_bl_driver);
}

module_init(aml_bl_init);
module_exit(aml_bl_exit);

MODULE_DESCRIPTION("AML Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
