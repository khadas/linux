/*
 * MFD core driver for Ricoh RN5T618 PMIC
 *
 * Copyright (C) 2014 Beniamino Galvani <b.galvani@gmail.com>
 * Copyright (C) 2016 Toradex AG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/amlogic/yk618.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>

#include "yk-cfg.h"
#include "yk-mfd.h"

extern struct yk_platform_data yk_pdata;

static int __remove_subdev(struct device *dev, void *unused)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

static int yk_mfd_remove_subdevs(struct yk618 *priv)
{
	return device_for_each_child(priv->dev, NULL, __remove_subdev);
}

static int  yk_mfd_add_subdevs(struct yk618 *priv,
					struct yk_platform_data *pdata)
{
	struct yk_funcdev_info *sply_dev;
	struct platform_device *pdev;
	int i, ret = 0;

	/* register for power supply */
	for (i = 0; i < pdata->num_sply_devs; i++) {
	sply_dev = &pdata->sply_devs[i];
	pdev = platform_device_alloc(sply_dev->name, sply_dev->id);
	pdev->dev.parent = priv->dev;
	pdev->dev.platform_data = sply_dev->platform_data;
	ret = platform_device_add(pdev);
	printk("yk_mfd_add_subdevs test2 supply\n");

	if (ret)
		goto failed;
	}

	return 0;

failed:
	yk_mfd_remove_subdevs(priv);
	return ret;
}

static const struct mfd_cell yk618_cells[] = {
	{ .name = "yk618-regulator" },
};

static bool yk618_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
		return true;
	default:
		return true;
	}
}

static const struct regmap_config yk618_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.volatile_reg	= yk618_volatile_reg,
	.max_register	= YK_MAXREG,
	.cache_type	= REGCACHE_RBTREE,
};

static struct yk618 *yk618_pm_power_off;
static struct notifier_block yk618_restart_handler;

static void yk618_trigger_poweroff_sequence(bool repower)
{
	unsigned int val;
	regmap_update_bits(yk618_pm_power_off->regmap, YK_VOFF_SET, 0x07, 0x7);

	val = 0xff;
	printk("[yk] send power-off command!\n");
	mdelay(20);
	if (repower != 1) {
		regmap_read(yk618_pm_power_off->regmap, YK_STATUS, &val);
		if (val & 0xF0) {
			regmap_read(yk618_pm_power_off->regmap, YK_MODE_CHGSTATUS, &val);
			if (val & 0x20) {
				printk("[yk] set flag!\n");
				regmap_write(yk618_pm_power_off->regmap, YK_BUFFERC, 0x0f);
				mdelay(20);
			}
		}
	}
	regmap_write(yk618_pm_power_off->regmap, YK_BUFFERC, 0x0f);
	mdelay(20);
	regmap_update_bits(yk618_pm_power_off->regmap, YK_OFF_CTL, 0x80, 0x80);
	mdelay(20);
}

static void yk618_power_off(void)
{
	yk618_trigger_poweroff_sequence(false);
}

static int yk618_restart(struct notifier_block *this,
			    unsigned long mode, void *cmd)
{
	yk618_trigger_poweroff_sequence(true);

	/*
	 * Re-power factor detection on PMIC side is not instant. 1ms
	 * proved to be enough time until reset takes effect.
	 */
	mdelay(1);

	return NOTIFY_DONE;
}

static const struct of_device_id yk618_of_match[] = {
	{ .compatible = "amlogic,yk618_mfd",},
	{ }
};
MODULE_DEVICE_TABLE(of, yk618_of_match);

static int yk618_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	const struct of_device_id *of_id;
	struct yk618 *priv;
	int ret;

	of_id = of_match_device(yk618_of_match, &i2c->dev);
	if (!of_id) {
		dev_err(&i2c->dev, "Failed to find matching DT ID\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&i2c->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	i2c_set_clientdata(i2c, priv);
	priv->variant = (long)of_id->data;
	priv->dev = &i2c->dev;
	yk = i2c;

	priv->regmap = devm_regmap_init_i2c(i2c, &yk618_regmap_config);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_err(&i2c->dev, "regmap init failed: %d\n", ret);
		return ret;
	}

	ret = devm_mfd_add_devices(&i2c->dev, -1, yk618_cells,
				   ARRAY_SIZE(yk618_cells), NULL, 0, NULL);
	if (ret) {
		dev_err(&i2c->dev, "failed to add sub-devices: %d\n", ret);
		return ret;
	}

	yk618_pm_power_off = priv;
	if (of_device_is_system_power_controller(i2c->dev.of_node)) {
		if (!pm_power_off)
			pm_power_off = yk618_power_off;
		else
			dev_warn(&i2c->dev, "Poweroff callback already assigned\n");
	}

	yk618_restart_handler.notifier_call = yk618_restart;
	yk618_restart_handler.priority = 192;

	ret = register_restart_handler(&yk618_restart_handler);
	if (ret) {
		dev_err(&i2c->dev, "cannot register restart handler, %d\n", ret);
		return ret;
	}

	yk_mfd_add_subdevs(priv, &yk_pdata);

	return 0;
}

static int yk618_i2c_remove(struct i2c_client *i2c)
{
	struct yk618 *priv = i2c_get_clientdata(i2c);

	yk_mfd_remove_subdevs(priv);

	if (priv == yk618_pm_power_off) {
		yk618_pm_power_off = NULL;
		pm_power_off = NULL;
	}

	return 0;
}

static const struct i2c_device_id yk618_i2c_id[] = {
	{ }
};
MODULE_DEVICE_TABLE(i2c, yk618_i2c_id);

static struct i2c_driver yk618_i2c_driver = {
	.driver = {
		.name = "yk618",
		.of_match_table = of_match_ptr(yk618_of_match),
	},
	.probe = yk618_i2c_probe,
	.remove = yk618_i2c_remove,
	.id_table = yk618_i2c_id,
};

module_i2c_driver(yk618_i2c_driver);

MODULE_DESCRIPTION("YK618 MFD driver");
MODULE_LICENSE("GPL v2");
