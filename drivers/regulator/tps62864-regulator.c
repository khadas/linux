// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/of_regulator.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/tps62864.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>

/* Register definitions */
#define TPS62864_REG_VOUT1		0x1
#define TPS62864_REG_VOUT2		0x2
#define TPS62864_REG_CONTROL	0x3
#define TPS62864_REG_STATUS		0x5
#define TPS62864_MAX_REG		TPS62864_REG_STATUS
#define TPS62864_MAX_VOUT_REG	2

/* TPS62864 CONTROL register (TPS62864_REG_CONTROL 0x3) */
#define TPS62864_RESET							BIT(7)
#define TPS62864_FPWM_ENABLE_DURNING_OUT_CHG	BIT(6)
#define TPS62864_SW_ENABLE						BIT(5)
#define TPS62864_FPWM_ENABLE					BIT(4)
#define TPS62864_DISCH_ENABLE					BIT(3)
#define TPS62864_HICCUP_ENABLE					BIT(2)

#define TPS62864_RAMP_20mV_PER_US			0x0
#define TPS62864_RAMP_10mV_PER_US			0x1
#define TPS62864_RAMP_5mV_PER_US			0x2
#define TPS62864_RAMP_1mV_PER_US			0x3
#define TPS62864_RAMP_MASK					0x3

/* Output voltage settings */
#define TPS62864_BASE_VOLTAGE	400000
#define TPS62864_N_VOLTAGES	256
#define TPS62864_UV_STEP	5000

enum chips {TPS62864, TPS62866};

/* tps 62864 chip information */
struct tps62864_chip {
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct regmap *regmap;

	int curr_vout_reg_val[TPS62864_MAX_VOUT_REG];
	int curr_vout_reg_id;
};

static int tps62864_dcdc_get_voltage_sel(struct regulator_dev *dev)
{
	struct tps62864_chip *tps = rdev_get_drvdata(dev);
	unsigned int data = 0;
	int ret;

	ret = regmap_read(tps->regmap, tps->curr_vout_reg_id, &data);
	if (ret < 0) {
		dev_err(tps->dev, "%s(): register %d read failed with err %d\n",
			__func__, tps->curr_vout_reg_id, ret);
		return ret;
	}

	return (int)data;
}

static int tps62864_dcdc_set_voltage_sel(struct regulator_dev *dev,
					 unsigned int selector)
{
	struct tps62864_chip *tps = rdev_get_drvdata(dev);
	int ret;

	ret = regmap_write(tps->regmap, tps->curr_vout_reg_id, selector);
	if (ret < 0) {
		dev_err(tps->dev, "%s(): register %d write failed with err %d\n",
			 __func__, tps->curr_vout_reg_id, ret);
		return ret;
	}
	tps->curr_vout_reg_val[tps->curr_vout_reg_id] = selector;
	return 0;
}

static int tps62864_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct tps62864_chip *tps = rdev_get_drvdata(rdev);
	int val;
	int ret;

	/* Enable force PWM mode in FAST mode only. */
	switch (mode) {
	case REGULATOR_MODE_FAST:
		val = TPS62864_FPWM_ENABLE;
		break;

	case REGULATOR_MODE_NORMAL:
		val = 0;
		break;

	default:
		return -EINVAL;
	}

	ret = regmap_update_bits(tps->regmap, TPS62864_REG_CONTROL, TPS62864_FPWM_ENABLE, val);
	if (ret < 0) {
		dev_err(tps->dev, "%s(): register %d update failed with err %d\n",
			__func__, TPS62864_REG_CONTROL, ret);
	}
	return ret;
}

static unsigned int tps62864_get_mode(struct regulator_dev *rdev)
{
	struct tps62864_chip *tps = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(tps->regmap, TPS62864_REG_CONTROL, &data);
	if (ret < 0) {
		dev_err(tps->dev, "%s(): register %d read failed with err %d\n",
			__func__, TPS62864_REG_CONTROL, ret);
		return ret;
	}
	return (data & TPS62864_FPWM_ENABLE) ? REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
}

static int tps62864_set_ramp_delay(struct regulator_dev *rdev,
		int ramp_delay)
{
	struct tps62864_chip *tps = rdev_get_drvdata(rdev);
	unsigned int control;
	int ret;

	/* Set ramp delay */
	if (ramp_delay <= 1000)
		control = TPS62864_RAMP_1mV_PER_US;
	else if (ramp_delay <= 5000)
		control = TPS62864_RAMP_5mV_PER_US;
	else if (ramp_delay <= 10000)
		control = TPS62864_RAMP_10mV_PER_US;
	else if (ramp_delay <= 20000)
		control = TPS62864_RAMP_20mV_PER_US;
	else
		return -EINVAL;

	ret = regmap_update_bits(tps->regmap, TPS62864_REG_CONTROL,
			TPS62864_RAMP_MASK, control);
	if (ret < 0) {
		dev_err(tps->dev, "%s(): register %d update failed with err %d",
				__func__, TPS62864_REG_CONTROL, ret);
	}
	return ret;
}

static struct regulator_ops tps62864_dcdc_ops = {
	.get_voltage_sel	= tps62864_dcdc_get_voltage_sel,
	.set_voltage_sel	= tps62864_dcdc_set_voltage_sel,
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
	.set_mode		= tps62864_set_mode,
	.get_mode		= tps62864_get_mode,
	.set_ramp_delay		= tps62864_set_ramp_delay,
};

static int tps62864_init_dcdc(struct tps62864_chip *tps,
		struct tps62864_regulator_platform_data *pdata)
{
	int ret;
	unsigned int ramp_speed;
	unsigned int data;

	/* Get voltage ramp speed from control register */
	ret = regmap_read(tps->regmap, TPS62864_REG_CONTROL, &data);
	if (ret < 0) {
		dev_err(tps->dev, "%s(): register %d read failed with err %d\n",
			__func__, TPS62864_REG_CONTROL, ret);
		return ret;
	}

	ramp_speed = data & TPS62864_RAMP_MASK;
	switch (ramp_speed) {
	case TPS62864_RAMP_20mV_PER_US:
		tps->desc.ramp_delay = 20000;
		break;
	case TPS62864_RAMP_10mV_PER_US:
		tps->desc.ramp_delay = 10000;
		break;
	case TPS62864_RAMP_5mV_PER_US:
		tps->desc.ramp_delay = 5000;
		break;
	case TPS62864_RAMP_1mV_PER_US:
	default:
		tps->desc.ramp_delay = 1000;
		break;
	}

	return ret;
}

static const struct regmap_config tps62864_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= TPS62864_MAX_REG,
	.cache_type		= REGCACHE_RBTREE,
};

static struct tps62864_regulator_platform_data *
	of_get_tps62864_platform_data(struct device *dev,
				      const struct regulator_desc *desc)
{
	struct tps62864_regulator_platform_data *pdata;
	struct device_node *np = dev->of_node;
	u32 vout_reg_id;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->reg_init_data = of_get_regulator_init_data(dev, dev->of_node,
							  desc);
	if (!pdata->reg_init_data) {
		dev_err(dev, "Not able to get OF regulator init data\n");
		return NULL;
	}

	of_property_read_u32(np, "ti,vout-reg-id", &vout_reg_id);
	pdata->vout_reg_id = (vout_reg_id >= (TPS62864_MAX_VOUT_REG - 1)) ?
		(TPS62864_MAX_VOUT_REG - 1) : vout_reg_id;

	return pdata;
}

#if defined(CONFIG_OF)
static const struct of_device_id tps62864_of_match[] = {
	 { .compatible = "ti,tps62864", .data = (void *)TPS62864},
	 { .compatible = "ti,tps62866", .data = (void *)TPS62866},
	{},
};
MODULE_DEVICE_TABLE(of, tps62864_of_match);
#endif

static int tps62864_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct regulator_config config = { };
	struct tps62864_regulator_platform_data *pdata;
	struct regulator_dev *rdev;
	struct tps62864_chip *tps;
	int ret;
	int chip_id;

	pdata = dev_get_platdata(&client->dev);

	tps = devm_kzalloc(&client->dev, sizeof(*tps), GFP_KERNEL);
	if (!tps)
		return -ENOMEM;

	tps->regmap = devm_regmap_init_i2c(client, &tps62864_regmap_config);
	if (IS_ERR(tps->regmap)) {
		ret = PTR_ERR(tps->regmap);
		dev_err(&client->dev,
			"%s(): regmap allocation failed with err %d\n",
			__func__, ret);
		return ret;
	}

	if (client->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(tps62864_of_match),
				&client->dev);
		if (!match) {
			dev_err(&client->dev, "Error: No device match found\n");
			return -ENODEV;
		}
		chip_id = (int)(long)match->data;
		if (!pdata)
			pdata = of_get_tps62864_platform_data(&client->dev,
							      &tps->desc);
	} else if (id) {
		chip_id = id->driver_data;
	} else {
		dev_err(&client->dev, "No device tree match or id table match found\n");
		return -ENODEV;
	}

	if (!pdata) {
		dev_err(&client->dev, "%s(): Platform data not found\n",
						__func__);
		return -EIO;
	}

	i2c_set_clientdata(client, tps);

	tps->dev = &client->dev;
	tps->desc.name = client->name;
	tps->desc.id = 0;
	tps->desc.ops = &tps62864_dcdc_ops;
	tps->desc.type = REGULATOR_VOLTAGE;
	tps->desc.owner = THIS_MODULE;
	tps->desc.min_uV = TPS62864_BASE_VOLTAGE;
	tps->desc.uV_step = TPS62864_UV_STEP;
	tps->desc.n_voltages = TPS62864_N_VOLTAGES;
	tps->curr_vout_reg_id = TPS62864_REG_VOUT1 + pdata->vout_reg_id;

	ret = tps62864_init_dcdc(tps, pdata);
	if (ret < 0) {
		dev_err(tps->dev, "%s(): Init failed with err = %d\n",
				__func__, ret);
		return ret;
	}
	config.dev = &client->dev;
	config.init_data = pdata->reg_init_data;
	config.driver_data = tps;
	config.of_node = client->dev.of_node;
	config.regmap = tps->regmap;

	/* Register the regulators */
	rdev = devm_regulator_register(&client->dev, &tps->desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(tps->dev,
			"%s(): regulator register failed with err %s\n",
			__func__, id->name);
		return PTR_ERR(rdev);
	}
	tps->rdev = rdev;

	return 0;
}

static const struct i2c_device_id tps62864_id[] = {
	{.name = "tps62864", .driver_data = TPS62864},
	{.name = "tps62866", .driver_data = TPS62866},
	{},
};

MODULE_DEVICE_TABLE(i2c, tps62864_id);

static struct i2c_driver tps62864_i2c_driver = {
	.driver = {
		.name = "tps62864",
		.of_match_table = of_match_ptr(tps62864_of_match),
	},
	.probe = tps62864_probe,
	.id_table = tps62864_id,
};

static int __init tps62864_init(void)
{
	return i2c_add_driver(&tps62864_i2c_driver);
}
subsys_initcall(tps62864_init);

static void __exit tps62864_cleanup(void)
{
	i2c_del_driver(&tps62864_i2c_driver);
}
module_exit(tps62864_cleanup);

MODULE_AUTHOR("Edward Ho <Edward.Ho@amlogic.com>");
MODULE_DESCRIPTION("TPS6286x voltage regulator driver");
MODULE_LICENSE("GPL v2");
