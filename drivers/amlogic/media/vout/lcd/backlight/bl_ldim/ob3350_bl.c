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

static int ob3350_on_flag;

struct ob3350 {
	unsigned char cur_addr;
};

struct ob3350 *bl_ob3350;

static int ob3350_hw_init_on(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		      ldim_drv->ldev_conf->en_gpio_on);
	mdelay(2);

	ldim_set_duty_pwm(&ldim_drv->ldev_conf->ldim_pwm_config);
	ldim_set_duty_pwm(&ldim_drv->ldev_conf->analog_pwm_config);
	ldim_drv->pinmux_ctrl(1);
	mdelay(20);

	return 0;
}

static int ob3350_hw_init_off(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	ldim_gpio_set(ldim_drv->ldev_conf->en_gpio,
		      ldim_drv->ldev_conf->en_gpio_off);
	ldim_drv->pinmux_ctrl(0);
	ldim_pwm_off(&ldim_drv->ldev_conf->ldim_pwm_config);
	ldim_pwm_off(&ldim_drv->ldev_conf->analog_pwm_config);

	return 0;
}

static int ob3350_smr(unsigned int *buf, unsigned int len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int dim_max, dim_min;
	unsigned int level, val;

	if (ob3350_on_flag == 0) {
		if (ldim_debug_print)
			LDIMPR("%s: on_flag=%d\n", __func__, ob3350_on_flag);
		return 0;
	}

	if (len != 1) {
		LDIMERR("%s: data len %d invalid\n", __func__, len);
		return -1;
	}

	dim_max = ldim_drv->ldev_conf->dim_max;
	dim_min = ldim_drv->ldev_conf->dim_min;
	level = buf[0];
	val = dim_min + ((level * (dim_max - dim_min)) / LD_DATA_MAX);

	ldim_drv->ldev_conf->ldim_pwm_config.pwm_duty = val;
	ldim_set_duty_pwm(&ldim_drv->ldev_conf->ldim_pwm_config);

	return 0;
}

static void ob3350_dim_range_update(void)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_config_s *ldim_dev;

	ldim_dev = ldim_drv->ldev_conf;
	ldim_dev->dim_max = ldim_dev->ldim_pwm_config.pwm_duty_max;
	ldim_dev->dim_min = ldim_dev->ldim_pwm_config.pwm_duty_min;
}

static int ob3350_power_on(void)
{
	if (ob3350_on_flag) {
		LDIMPR("%s: ob3350 is already on, exit\n", __func__);
		return 0;
	}

	ob3350_hw_init_on();
	ob3350_on_flag = 1;

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int ob3350_power_off(void)
{
	ob3350_on_flag = 0;
	ob3350_hw_init_off();

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t ob3350_show(struct class *class,
			   struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	if (!strcmp(attr->attr.name, "status")) {
		ret = sprintf(buf, "ob3350 status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"dim_max        = %d\n"
				"dim_min        = %d\n"
				"pwm_duty       = %d%%\n\n",
				ldim_drv->conf->dev_index, ob3350_on_flag,
				ldim_drv->ldev_conf->en_gpio_on,
				ldim_drv->ldev_conf->en_gpio_off,
				ldim_drv->ldev_conf->dim_max,
				ldim_drv->ldev_conf->dim_min,
				ldim_drv->ldev_conf->ldim_pwm_config.pwm_duty);
	}

	return ret;
}

static struct class_attribute ob3350_class_attrs[] = {
	__ATTR(status, 0644, ob3350_show, NULL),
	__ATTR_NULL
};

static int ob3350_ldim_driver_update(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_config_s *ldim_dev = ldim_drv->ldev_conf;

	ldim_dev->ldim_pwm_config.pwm_duty_max = ldim_dev->dim_max;
	ldim_dev->ldim_pwm_config.pwm_duty_min = ldim_dev->dim_min;
	ldim_dev->dim_range_update = ob3350_dim_range_update;

	ldim_drv->device_power_on = ob3350_power_on;
	ldim_drv->device_power_off = ob3350_power_off;
	ldim_drv->device_bri_update = ob3350_smr;

	return 0;
}

int ldim_dev_ob3350_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct class *dev_class;
	int i;

	ob3350_on_flag = 0;
	bl_ob3350 = kzalloc(sizeof(*bl_ob3350), GFP_KERNEL);
	if (!bl_ob3350)
		return -1;

	ob3350_ldim_driver_update(ldim_drv);

	if (ldim_drv->ldev_conf->dev_class) {
		dev_class = ldim_drv->ldev_conf->dev_class;
		for (i = 0; i < ARRAY_SIZE(ob3350_class_attrs); i++) {
			if (class_create_file(dev_class,
					      &ob3350_class_attrs[i])) {
				LDIMERR("create ldim_dev class attribute %s fail\n",
				 ob3350_class_attrs[i].attr.name);
			}
		}
	}

	ob3350_on_flag = 1; /* default enable in uboot */
	LDIMPR("%s ok\n", __func__);

	return 0;
}

int ldim_dev_ob3350_remove(struct aml_ldim_driver_s *ldim_drv)
{
	kfree(bl_ob3350);
	bl_ob3350 = NULL;

	return 0;
}

