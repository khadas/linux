// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
/*#define DEBUG*/

#include <linux/device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/clk-provider.h>
#include <linux/of_platform.h>


#include "../common/iomapres.h"

#include "audio_io.h"
#include "regs.h"
#include "audio_controller.h"

#define DRV_NAME "aml-audio-controller"

unsigned int chip_id;

static unsigned int aml_audio_mmio_read(struct regmap *regmap, unsigned int reg)
{
	return mmio_read(regmap, reg);
}

static int aml_audio_mmio_write(struct regmap *regmap, unsigned int reg, unsigned int value)
{
	return mmio_write(regmap, reg, value);
}

static int aml_audio_mmio_update_bits(struct regmap *regmap,
				      unsigned int reg, unsigned int mask,
				      unsigned int value)
{
	return mmio_update_bits(regmap, reg, mask, value);
}

int aml_return_chip_id(void)
{
	return chip_id;
}

struct aml_audio_ctrl_ops aml_actrl_mmio_ops = {
	.read		= aml_audio_mmio_read,
	.write		= aml_audio_mmio_write,
	.update_bits	= aml_audio_mmio_update_bits,
};

struct gate_info {
	bool clk1_gate_off;
};

static struct gate_info axg_info = {
	.clk1_gate_off = true,
};

static const struct of_device_id amlogic_audio_controller_of_match[] = {
	{
		.compatible = "amlogic, audio-controller"
	},
	{
		.compatible = "amlogic, axg-audio-controller",
		.data       = &axg_info,
	},
	{},
};

static int register_audio_controller(struct platform_device *pdev,
				     struct aml_audio_controller *actrl)
{
	struct regmap *audioio_regmap, *acc_regmap;
	struct gate_info *info = (struct gate_info *)of_device_get_match_data(&pdev->dev);

	audioio_regmap = regmap_resource(&pdev->dev, "audio_bus");
	acc_regmap = regmap_resource(&pdev->dev, "audio_acc");
	if (IS_ERR(audioio_regmap))
		return PTR_ERR(audioio_regmap);

	/* init aml audio bus mmio controller */
	actrl->audioio_regmap = audioio_regmap;
	if (!IS_ERR(acc_regmap)) {
		actrl->acc_regmap = acc_regmap;
		mmio_write(acc_regmap, AUDIO_ACC_CLK_GATE_EN, 0xff);
	} else {
		actrl->acc_regmap = NULL;
	}
	actrl->ops = &aml_actrl_mmio_ops;

	platform_set_drvdata(pdev, actrl);

	/* gate on all clks on bringup stage, need gate separately */
	aml_audiobus_write(actrl, EE_AUDIO_CLK_GATE_EN0, 0xffffffff);

	if (info && !info->clk1_gate_off)
		aml_audiobus_update_bits(actrl, EE_AUDIO_CLK_GATE_EN1, 0xffffffff, 0xffffffff);

	return 0;
}

static int aml_audio_controller_probe(struct platform_device *pdev)
{
	struct aml_audio_controller *actrl;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	pr_info("asoc debug: %s-%d\n", __func__, __LINE__);
	actrl = devm_kzalloc(&pdev->dev, sizeof(*actrl), GFP_KERNEL);
	if (!actrl)
		return -ENOMEM;

	ret = of_property_read_u32(node, "chip_id", &chip_id);
	if (ret < 0)
		/* default set 0 */
		chip_id = 0;

	return register_audio_controller(pdev, actrl);
}

static struct platform_driver aml_audio_controller_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = amlogic_audio_controller_of_match,
	},
	.probe = aml_audio_controller_probe,
};

int __init audio_controller_init(void)
{
	return platform_driver_register(&aml_audio_controller_driver);
}

void __exit audio_controller_exit(void)
{
	platform_driver_unregister(&aml_audio_controller_driver);
}

#ifndef MODULE
module_init(audio_controller_init);
module_exit(audio_controller_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic audio controller ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, amlogic_audio_controller_of_match);
#endif
