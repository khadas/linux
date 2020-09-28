/*
 * Regulator driver for yeker yk618 PMIC
 *
 * Copyright (C) 2014 Beniamino Galvani <b.galvani@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/amlogic/yk618.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

static struct regulator_ops yk618_reg_ops = {
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.list_voltage		= regulator_list_voltage_linear,
};

#define REG(rid, ereg, emask, vreg, vmask, min, max, step)		\
	[YK_##rid] = {						\
		.name		= #rid,					\
		.of_match	= of_match_ptr(#rid),			\
		.regulators_node = of_match_ptr("regulators"),		\
		.id		= YK_##rid,			\
		.type		= REGULATOR_VOLTAGE,			\
		.owner		= THIS_MODULE,				\
		.ops		= &yk618_reg_ops,			\
		.n_voltages	= ((max) - (min)) / (step) + 1,		\
		.min_uV		= (min),				\
		.uV_step	= (step),				\
		.enable_reg	= YK_##ereg,			\
		.enable_mask	= (emask),				\
		.vsel_reg	= YK_##vreg,			\
		.vsel_mask	= (vmask),				\
	}



static struct regulator_desc yk618_regulators[] = {
	/* DCDC */
	REG(DCDC1, DCDC1EN, BIT(1), DCDC1DAC, 0x3f, 1600000, 3500000, 100000),
	REG(DCDC2, DCDC2EN, BIT(2), DCDC2DAC, 0x3f, 600000, 1540000, 20000),
	REG(DCDC3, DCDC3EN, BIT(3), DCDC3DAC, 0x3f, 600000, 1540000, 20000),
	REG(DCDC4, DCDC4EN, BIT(4), DCDC4DAC, 0x3f, 600000, 1540000, 20000),
	REG(DCDC5, DCDC5EN, BIT(5), DCDC5DAC, 0x1f, 1000000, 2550000, 50000),
	/* LDO */
	REG(ALDO1, ALDO1EN, BIT(6), ALDO1DAC, 0x1f, 700000, 3300000, 100000),
	REG(ALDO2, ALDO2EN, BIT(7), ALDO2DAC, 0x1f, 700000, 3300000, 100000),
	REG(ALDO3, ALDO3EN, BIT(5), ALDO3DAC, 0x1f, 700000, 3300000, 100000),
	REG(ELDO1, ELDO1EN, BIT(0), ELDO1DAC, 0x1f, 700000, 3300000, 100000),
	REG(ELDO2, ELDO2EN, BIT(1), ELDO2DAC, 0x1f, 700000, 3300000, 100000),
};

static int yk618_regulator_probe(struct platform_device *pdev)
{
	struct yk618 *yk618 = dev_get_drvdata(pdev->dev.parent);
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	struct regulator_desc *regulators;
	int i;

	regulators = yk618_regulators;

	config.dev = pdev->dev.parent;
	config.regmap = yk618->regmap;


	for (i = 0; i < YK_REG_NUM; i++) {
		if (!regulators[i].name)
			continue;
		rdev = devm_regulator_register(&pdev->dev,
					       &regulators[i],
					       &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "failed to register %s regulator\n",
				regulators[i].name);
			return PTR_ERR(rdev);
		}
	}
	return 0;
}

static struct platform_driver yk618_regulator_driver = {
	.probe = yk618_regulator_probe,
	.driver = {
		.name	= "yk618-regulator",
	},
};

module_platform_driver(yk618_regulator_driver);

MODULE_DESCRIPTION("YK618 regulator driver");
MODULE_LICENSE("GPL v2");
