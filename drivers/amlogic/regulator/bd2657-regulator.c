// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/rohm-bd2657.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>

static const struct regulator_linear_range bd2657_buck_volts[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x01, 0xab, 5000),
	REGULATOR_LINEAR_RANGE(1350000, 0xac, 0xff, 0),
};

/*
 * BUCK3 has different voltages when the I2C address selection pin is
 * connected to GND/HIGH. We check the I2C address and decide the voltage
 * table for BUCK3 based on it
 */
static const struct regulator_linear_range bd2657_buck3_volts_2[] = {
	REGULATOR_LINEAR_RANGE(1100000, 0x01, 0x97, 5000),
	REGULATOR_LINEAR_RANGE(1850000, 0x98, 0xff, 0),
};

#define BD2675_I2C_2 0x6b

static unsigned int bd2657_ramp_table[] = {
	2500, 5000, 2500, 2500,
};

struct bd2657_data {
	struct regulator_desc desc;
	int vsel_cache;
	bool enabled;
};

static int bd2656_get_voltage_sel(struct regulator_dev *rdev)
{
	struct bd2657_data *d;

	d = container_of(rdev->desc, struct bd2657_data, desc);
	return d->vsel_cache;
}

static int bd2656_set_voltage_sel(struct regulator_dev *rdev, unsigned int sel)
{
	int ret = 0;
	struct bd2657_data *d;

	d = container_of(rdev->desc, struct bd2657_data, desc);
	if (d->enabled)
		ret = regulator_set_voltage_sel_regmap(rdev, sel);

	if (!ret)
		d->vsel_cache = sel;

	return ret;
}

static int bd2656_is_enabled(struct regulator_dev *rdev)
{
	struct bd2657_data *d;

	d = container_of(rdev->desc, struct bd2657_data, desc);

	return d->enabled;
}

static int bd2656_disable(struct regulator_dev *rdev)
{
	int ret;
	struct bd2657_data *d;

	d = container_of(rdev->desc, struct bd2657_data, desc);
	if (!d->enabled)
		return 0;

	ret = regmap_write(rdev->regmap, rdev->desc->vsel_reg, 0);
	if (!ret)
		d->enabled = false;

	return ret;
}

static int bd2656_enable(struct regulator_dev *rdev)
{
	struct bd2657_data *d;
	int ret;

	d = container_of(rdev->desc, struct bd2657_data, desc);
	if (d->enabled)
		return 0;

	if (!d->vsel_cache) {
		dev_err(&rdev->dev, "Failed to enable '%s'\n",
			rdev->desc->name);
		return -EINVAL;
	}

	ret = regmap_write(rdev->regmap, rdev->desc->vsel_reg, d->vsel_cache);
	if (!ret)
		d->enabled = true;

	return ret;
}

static const struct regulator_ops bd2657_buck_ops = {
	.enable = bd2656_enable,
	.disable = bd2656_disable,
	.is_enabled = bd2656_is_enabled,
	//.list_voltage = regulator_list_voltage_linear_range, //
	.list_voltage = regulator_list_voltage_linear_range, //
	.set_voltage_sel = bd2656_set_voltage_sel,
	.get_voltage_sel = bd2656_get_voltage_sel,
	//.set_voltage_time_sel = regulator_set_voltage_time_sel,  //
	//.set_ramp_delay = regulator_set_ramp_delay_regmap,  //
};

static const struct bd2657_data bd2657_desc_template[] = {
	{
		.desc = {
			.name = "buck1",
			.of_match = of_match_ptr("BUCK1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD2657_BUCK0,
			.n_voltages = BD2657_BUCK_NUM_VOLTS,
			.ops = &bd2657_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd2657_buck_volts,
			.n_linear_ranges = ARRAY_SIZE(bd2657_buck_volts),
			.vsel_reg = BD2657_REG_BUCK0_VID,
			.vsel_mask = BD2657_MASK_BUCK_VOLT,
			.ramp_delay_table = bd2657_ramp_table,
			.n_ramp_values = ARRAY_SIZE(bd2657_ramp_table),
			.ramp_mask = BD2657_MASK_RAMP_SLOW,
			.ramp_reg = BD2657_REG_BUCK0_SLEW,
			.owner = THIS_MODULE,
		},
	}, {
		.desc = {
			.name = "buck2",
			.of_match = of_match_ptr("BUCK2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD2657_BUCK1,
			.n_voltages = BD2657_BUCK_NUM_VOLTS,
			.ops = &bd2657_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd2657_buck_volts,
			.n_linear_ranges = ARRAY_SIZE(bd2657_buck_volts),
			.vsel_reg = BD2657_REG_BUCK1_VID,
			.vsel_mask = BD2657_MASK_BUCK_VOLT,
			.owner = THIS_MODULE,
		},
	}, {
		.desc = {
			.name = "buck3",
			.of_match = of_match_ptr("BUCK3"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD2657_BUCK2,
			.n_voltages = BD2657_BUCK_NUM_VOLTS,
			.ops = &bd2657_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd2657_buck_volts,
			.n_linear_ranges = ARRAY_SIZE(bd2657_buck_volts),
			.vsel_reg = BD2657_REG_BUCK2_VID,
			.vsel_mask = BD2657_MASK_BUCK_VOLT,
			.owner = THIS_MODULE,
		},
	}, {
		.desc = {
			.name = "buck4",
			.of_match = of_match_ptr("BUCK4"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD2657_BUCK3,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD2657_BUCK_NUM_VOLTS,
			.ops = &bd2657_buck_ops,
			/*
			 * The voltage ranges may be overwritten
			 * at probe as they
			 * differ depending on I2C slave address
			 * pin connection.
			 */
			.linear_ranges = bd2657_buck_volts,
			.n_linear_ranges = ARRAY_SIZE(bd2657_buck_volts),
			.vsel_reg = BD2657_REG_BUCK3_VID,
			.vsel_mask = BD2657_MASK_BUCK_VOLT,
			.owner = THIS_MODULE,
		},
	},
};

static int bd2657_probe(struct platform_device *pdev)
{
	int i, num_bucks, ret, val;
	struct bd2657_data *d;
	struct regulator_config c = {
		.dev = pdev->dev.parent,
	};
	u32 i2c_addr;

	num_bucks = ARRAY_SIZE(bd2657_desc_template);

	c.regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!c.regmap)
		return -ENODEV;

	d = devm_kzalloc(&pdev->dev, sizeof(*d) * num_bucks, GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	/* Get our I2C address from reg as the BUCK3 voltages depend on that */
	ret = of_property_read_u32(pdev->dev.parent->of_node, "reg", &i2c_addr);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read I2C address\n");
		return ret;
	}

	/* Check the register lock */
	ret = regmap_read(c.regmap, BD2657_REG_REGLOCK, &val);
	if (ret)
		return ret;

	if (val & BD2657_MASK_REGLOCK) {
		dev_warn(&pdev->dev, "PMIC is locked, nothing to do\n");

		return -ENODEV;
	}

	for (i = 0; i < num_bucks; i++) {
		struct regulator_dev *rdev;
		int val;

		d[i] = bd2657_desc_template[i];

		if (i2c_addr == BD2675_I2C_2 && d[i].desc.id == BD2657_BUCK3) {
			d[i].desc.linear_ranges = bd2657_buck3_volts_2;
			d[i].desc.n_linear_ranges =
				ARRAY_SIZE(bd2657_buck3_volts_2);
		}

		/*
		 * BD2657 has special BOOT_VID register which contains voltage
		 * after the PMIC cold reset. Write this to vsel so we can
		 * use the vsel from now on.
		 *
		 * TODO: How to know this was cold-boot for PMIC?
		 */
		ret = regmap_read(c.regmap, d[i].desc.vsel_reg + 1, &val);
		if (!ret)
			ret = regmap_write(c.regmap, d[i].desc.vsel_reg, val);
		else
			return ret;
		if (ret)
			return ret;

		/*
		 * BD2657 does not contain separate enable bit. If non-zero
		 * voltage is written to vsel_reg => regulator is enabled.
		 * In order to support setting voltage without enabling
		 * regulator - or to support enabling without setting voltage
		 * we do cache the voltage value and enable status here.
		 *
		 * If regulator is initially disabled, the we set the
		 * cached voltage to be the smallest supported voltage
		 */
		if (!val) {
			d[i].vsel_cache = 1;
			d[i].enabled = false;
		} else {
			d[i].vsel_cache = val;
			d[i].enabled = true;
		}

		rdev = devm_regulator_register(&pdev->dev, &d[i].desc, &c);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev,
				"failed to register %s regulator\n",
				d[i].desc.name);
			return PTR_ERR(rdev);
		}
	}
	return 0;
}

static struct platform_driver bd2657_regulator = {
	.driver = {
		.name = "bd2657-regulator"
	},
	.probe = bd2657_probe,
};
module_platform_driver(bd2657_regulator);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("BD2657 voltage regulator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bd2657-regulator");
