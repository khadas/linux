// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/aml_demod_common.h>

const char *v4l2_std_to_str(v4l2_std_id std)
{
	switch (std) {
	case V4L2_STD_PAL_B:
		return "V4L2_STD_PAL_B";
	case V4L2_STD_PAL_B1:
		return "V4L2_STD_PAL_B1";
	case V4L2_STD_PAL_G:
		return "V4L2_STD_PAL_G";
	case V4L2_STD_PAL_H:
		return "V4L2_STD_PAL_H";
	case V4L2_STD_PAL_I:
		return "V4L2_STD_PAL_I";
	case V4L2_STD_PAL_D:
		return "V4L2_STD_PAL_D";
	case V4L2_STD_PAL_D1:
		return "V4L2_STD_PAL_D1";
	case V4L2_STD_PAL_K:
		return "V4L2_STD_PAL_K";
	case V4L2_STD_PAL_M:
		return "V4L2_STD_PAL_M";
	case V4L2_STD_PAL_N:
		return "V4L2_STD_PAL_N";
	case V4L2_STD_PAL_Nc:
		return "V4L2_STD_PAL_Nc";
	case V4L2_STD_PAL_60:
		return "V4L2_STD_PAL_60";
	case V4L2_STD_NTSC_M:
		return "V4L2_STD_NTSC_M";
	case V4L2_STD_NTSC_M_JP:
		return "V4L2_STD_NTSC_M_JP";
	case V4L2_STD_NTSC_443:
		return "V4L2_STD_NTSC_443";
	case V4L2_STD_NTSC_M_KR:
		return "V4L2_STD_NTSC_M_KR";
	case V4L2_STD_SECAM_B:
		return "V4L2_STD_SECAM_B";
	case V4L2_STD_SECAM_D:
		return "V4L2_STD_SECAM_D";
	case V4L2_STD_SECAM_G:
		return "V4L2_STD_SECAM_G";
	case V4L2_STD_SECAM_H:
		return "V4L2_STD_SECAM_H";
	case V4L2_STD_SECAM_K:
		return "V4L2_STD_SECAM_K";
	case V4L2_STD_SECAM_K1:
		return "V4L2_STD_SECAM_K1";
	case V4L2_STD_SECAM_L:
		return "V4L2_STD_SECAM_L";
	case V4L2_STD_SECAM_LC:
		return "V4L2_STD_SECAM_LC";
	case V4L2_STD_ATSC_8_VSB:
		return "V4L2_STD_ATSC_8_VSB";
	case V4L2_STD_ATSC_16_VSB:
		return "V4L2_STD_ATSC_16_VSB";
	case V4L2_COLOR_STD_PAL:
		return "V4L2_COLOR_STD_PAL";
	case V4L2_COLOR_STD_NTSC:
		return "V4L2_COLOR_STD_NTSC";
	case V4L2_COLOR_STD_SECAM:
		return "V4L2_COLOR_STD_SECAM";
	case V4L2_STD_MN:
		return "V4L2_STD_MN";
	case V4L2_STD_B:
		return "V4L2_STD_B";
	case V4L2_STD_GH:
		return "V4L2_STD_GH";
	case V4L2_STD_DK:
		return "V4L2_STD_DK";
	case V4L2_STD_PAL_BG:
		return "V4L2_STD_PAL_BG";
	case V4L2_STD_PAL_DK:
		return "V4L2_STD_PAL_DK";
	case V4L2_STD_PAL:
		return "V4L2_STD_PAL";
	case V4L2_STD_NTSC:
		return "V4L2_STD_NTSC";
	case V4L2_STD_SECAM_DK:
		return "V4L2_STD_SECAM_DK";
	case (V4L2_STD_SECAM_B | V4L2_STD_SECAM_G):
		return "V4L2_STD_SECAM_BG";
	case V4L2_STD_SECAM:
		return "V4L2_STD_SECAM";
	case V4L2_STD_525_60:
		return "V4L2_STD_525_60";
	case V4L2_STD_625_50:
		return "V4L2_STD_625_50";
	case V4L2_STD_ATSC:
		return "V4L2_STD_ATSC";
	case V4L2_STD_ALL:
		return "V4L2_STD_ALL";
	default:
		return "V4L2_STD_UNKNOWN";
	}
}
EXPORT_SYMBOL(v4l2_std_to_str);

void aml_ktime_get_ts(struct timespec *ts)
{
	ktime_get_ts(ts);
}
EXPORT_SYMBOL(aml_ktime_get_ts);

int aml_gpio_direction_output(int gpio, int value)
{
	return gpio_direction_output(gpio, value);
}
EXPORT_SYMBOL(aml_gpio_direction_output);

int aml_gpio_direction_input(int gpio)
{
	return gpio_direction_input(gpio);
}
EXPORT_SYMBOL(aml_gpio_direction_input);

bool aml_gpio_is_valid(int number)
{
	return gpio_is_valid(number);
}
EXPORT_SYMBOL(aml_gpio_is_valid);

int aml_gpio_get_value(int gpio)
{
	return gpio_get_value(gpio);
}
EXPORT_SYMBOL(aml_gpio_get_value);

void aml_gpio_set_value(int gpio, int value)
{
	gpio_set_value(gpio, value);
}
EXPORT_SYMBOL(aml_gpio_set_value);

void aml_gpio_free(int gpio)
{
	gpio_free(gpio);
}
EXPORT_SYMBOL(aml_gpio_free);

int aml_gpio_request(int gpio, const char *label)
{
	return gpio_request(gpio, label);
}
EXPORT_SYMBOL(aml_gpio_request);

int aml_demod_gpio_set(int gpio, int dir, int value, const char *label)
{
	if (gpio_is_valid(gpio)) {
		aml_gpio_request(gpio, label);
		if (dir == GPIOF_DIR_OUT)
			gpio_direction_output(gpio, value);
		else {
			gpio_direction_input(gpio);
			gpio_set_value(gpio, value);
		}
	} else
		return -1;

	return 0;
}
EXPORT_SYMBOL(aml_demod_gpio_set);

int aml_demod_gpio_config(struct gpio_config *cfg, const char *label)
{
	return aml_demod_gpio_set(cfg->pin, cfg->dir, cfg->value, label);
}
EXPORT_SYMBOL(aml_demod_gpio_config);

struct class *aml_class_create(struct module *owner, const char *name)
{
	return class_create(owner, name);
}
EXPORT_SYMBOL(aml_class_create);

void aml_class_destroy(struct class *cls)
{
	class_destroy(cls);
}
EXPORT_SYMBOL(aml_class_destroy);

int aml_class_create_file(struct class *class,
		const struct class_attribute *attr)
{
	return class_create_file(class, attr);
}
EXPORT_SYMBOL(aml_class_create_file);

int aml_class_register(struct class *class)
{
	return class_register(class);
}
EXPORT_SYMBOL(aml_class_register);

void aml_class_unregister(struct class *class)
{
	return class_unregister(class);
}
EXPORT_SYMBOL(aml_class_unregister);

int aml_platform_driver_register(struct platform_driver *drv)
{
	return platform_driver_register(drv);
}
EXPORT_SYMBOL(aml_platform_driver_register);

void aml_platform_driver_unregister(struct platform_driver *drv)
{
	platform_driver_unregister(drv);
}
EXPORT_SYMBOL(aml_platform_driver_unregister);

int aml_platform_device_register(struct platform_device *pdev)
{
	return platform_device_register(pdev);
}
EXPORT_SYMBOL(aml_platform_device_register);

void aml_platform_device_unregister(struct platform_device *pdev)
{
	platform_device_unregister(pdev);
}
EXPORT_SYMBOL(aml_platform_device_unregister);
