// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

#define VSYNC_INFO_FREQUENT        300

static int global_on_flag;
static unsigned short vsync_cnt;

static int global_hw_init_on(struct ldim_dev_driver_s *dev_drv)
{
	ldim_set_duty_pwm(&dev_drv->ldim_pwm_config);
	ldim_set_duty_pwm(&dev_drv->analog_pwm_config);
	dev_drv->pinmux_ctrl(dev_drv, 1);
	mdelay(2);
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_on);
	mdelay(20);

	return 0;
}

static int global_hw_init_off(struct ldim_dev_driver_s *dev_drv)
{
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_off);
	dev_drv->pinmux_ctrl(dev_drv, 0);
	ldim_pwm_off(&dev_drv->ldim_pwm_config);
	ldim_pwm_off(&dev_drv->analog_pwm_config);

	return 0;
}

static int global_smr(struct aml_ldim_driver_s *ldim_drv,
		      unsigned int *buf, unsigned int len)
{
	unsigned int dim_max, dim_min;
	unsigned int level, val;

	if (vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		vsync_cnt = 0;

	if (global_on_flag == 0) {
		if (vsync_cnt == 0)
			LDIMPR("%s: on_flag=%d\n", __func__, global_on_flag);
		return 0;
	}

	if (len != 1) {
		if (vsync_cnt == 0)
			LDIMERR("%s: data len %d invalid\n", __func__, len);
		return -1;
	}

	dim_max = ldim_drv->dev_drv->dim_max;
	dim_min = ldim_drv->dev_drv->dim_min;
	level = buf[0];
	val = dim_min + ((level * (dim_max - dim_min)) / LD_DATA_MAX);

	ldim_drv->dev_drv->ldim_pwm_config.pwm_duty = val;
	ldim_set_duty_pwm(&ldim_drv->dev_drv->ldim_pwm_config);

	return 0;
}

static void global_dim_range_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->dim_max = dev_drv->ldim_pwm_config.pwm_duty_max;
	dev_drv->dim_min = dev_drv->ldim_pwm_config.pwm_duty_min;
}

static int global_power_on(struct aml_ldim_driver_s *ldim_drv)
{
	if (global_on_flag) {
		LDIMPR("%s: global is already on, exit\n", __func__);
		return 0;
	}

	global_hw_init_on(ldim_drv->dev_drv);
	global_on_flag = 1;
	vsync_cnt = 0;

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int global_power_off(struct aml_ldim_driver_s *ldim_drv)
{
	global_on_flag = 0;
	global_hw_init_off(ldim_drv->dev_drv);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t global_show(struct class *class, struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	if (!strcmp(attr->attr.name, "status")) {
		ret = sprintf(buf, "global status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"dim_max        = %d\n"
				"dim_min        = %d\n"
				"pwm_duty       = %d%%\n\n",
				ldim_drv->dev_drv->index, global_on_flag,
				ldim_drv->dev_drv->en_gpio_on,
				ldim_drv->dev_drv->en_gpio_off,
				ldim_drv->dev_drv->dim_max,
				ldim_drv->dev_drv->dim_min,
				ldim_drv->dev_drv->ldim_pwm_config.pwm_duty);
	}

	return ret;
}

static struct class_attribute global_class_attrs[] = {
	__ATTR(status, 0644, global_show, NULL)
};

static int global_ldim_driver_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->ldim_pwm_config.pwm_duty_max = dev_drv->dim_max;
	dev_drv->ldim_pwm_config.pwm_duty_min = dev_drv->dim_min;

	dev_drv->dim_range_update = global_dim_range_update;
	dev_drv->power_on = global_power_on;
	dev_drv->power_off = global_power_off;
	dev_drv->dev_smr = global_smr;

	return 0;
}

int ldim_dev_global_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int i;

	global_on_flag = 0;
	vsync_cnt = 0;

	global_ldim_driver_update(dev_drv);

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(global_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &global_class_attrs[i])) {
				LDIMERR("create ldim_dev class attribute %s fail\n",
					global_class_attrs[i].attr.name);
			}
		}
	}

	global_on_flag = 1; /* default enable in uboot */
	LDIMPR("%s ok\n", __func__);

	return 0;
}

int ldim_dev_global_remove(struct aml_ldim_driver_s *ldim_drv)
{
	return 0;
}

