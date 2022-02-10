// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/major.h>
#include <linux/list.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/gpio/consumer.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/hrtimer.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/amlogic/glb_timer.h>

#define DRIVER_NAME	"global_timer_output"

enum glb_output_reg {
	REG_OUTPUT_CTRL	= 0,
	REG_OUTPUT_SEL	= 1,
	REG_NUM,
};

struct meson_glb_timer_output_dev {
	struct list_head node;
	void __iomem *reg[REG_NUM];
	struct regmap *regmap[REG_NUM];
	struct platform_device *pdev;
	struct global_timer_output_gpio glb_output_gpio;
	int source;
};

static struct regmap_config meson_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.fast_io = true,
};

/**
 * global_timer_output_gpio_get_from_index
 *
 * @dev: pointer to the device struct for the consumer driver (this will be used
 * to parse the global-timer-output-gpio-snapshot property and get access to the
 * input GPIO). This access is assumed exclusive and a 2nd consumer can't get
 * access to the same global timer output GPIO
 * @index: an index to the phandle index in the global-timer-input-gpio-snapshot
 * property
 * @returns: the pointer to the struct global_timer_output_gpio is success or
 * relevant error value.
 */
struct global_timer_output_gpio*
global_timer_output_gpio_get_from_index(struct device *dev, int index)
{
	struct global_timer_output_gpio *gtod = NULL;
	struct device_node *np;
	struct device_node *np_glb_output;
	struct platform_device *pdev;
	struct meson_glb_timer_output_dev *glb_timer_output;
	char *propname;
	int size, i;
	const __be32 *list;
	struct property *prop;
	phandle phd;
	int ret;

	if (IS_ERR_OR_NULL(dev)) {
		pr_err("dev not found !!\n");
		return ERR_PTR(-ENODEV);
	}

	np = dev->of_node;

	/* parser rt_gpiox */
	propname = kasprintf(GFP_KERNEL, "global-timer-output-gpio-trigger");
	prop = of_find_property(np, propname, &size);
	kfree(propname);
	if (!prop)
		return ERR_PTR(-ENODEV);

	list = prop->value;
	size /= sizeof(*list);

	if (index + 1 > size)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < index; i++)
		list++;

	phd = be32_to_cpup(list);
	np_glb_output = of_find_node_by_phandle(phd);
	pdev = of_find_device_by_node(np_glb_output);

	if (!pdev)
		return ERR_PTR(-ENODEV);

	glb_timer_output = platform_get_drvdata(pdev);
	gtod = &glb_timer_output->glb_output_gpio;

	propname = kasprintf(GFP_KERNEL,
			     "global-timer-output-gpio-trigger-names");
	ret = of_property_read_string_index(np, propname, index, &gtod->name);
	kfree(propname);
	if (ret)
		return ERR_PTR(-EINVAL);

	if (!gtod->owner_dev) {
		gtod->owner_dev = dev;
	} else {
		dev_err(&pdev->dev, "This GPIO is requested by driver %s\n",
			dev_name(gtod->owner_dev));
		return ERR_PTR(-EBUSY);
	}

	return gtod;
}
EXPORT_SYMBOL_GPL(global_timer_output_gpio_get_from_index);

/**
 * global_timer_output_gpio_setup
 *
 * @gtod: the pointer to struct global_timer_output
 * @oneshot: if true then one shot pulse or continuous
 * @pulse_width: pulse with in global timer clock cycles
 * @interval: pulse interval in global timer cycles
 * @init_val: decides the initial value of the gpio state and the pulse would
 * be inverse of that init value
 * @returns: 0 on success and error code on failure
 */
int global_timer_output_gpio_setup(struct global_timer_output_gpio *gtod,
				   bool oneshot, u64 pulse_width, u64 interval,
				   u8 init_val)
{
	struct meson_glb_timer_output_dev *glb_timer_output;
	struct regmap *regmap;
	int reg;

	if (!gtod)
		return -EINVAL;

	glb_timer_output = gtod->glb_output_dev;
	regmap = glb_timer_output->regmap[REG_OUTPUT_CTRL];

	/* reset state */
	regmap_update_bits(regmap, OUTPUT_CTRL0, BIT(0), BIT(0));
	for (reg = 0; reg < (OUTPUT_ST1 >> 2); reg++)
		regmap_write(regmap, reg << 2, 0);

	if (init_val)
		regmap_update_bits(regmap, OUTPUT_CTRL0, BIT(28), BIT(28));

	/* config pulse type */
	regmap_write(regmap, OUTPUT_PULSE_WDITH, pulse_width);
	regmap_write(regmap, OUTPUT_INTERVAL, interval);
	regmap_write(regmap, OUTPUT_STOP_NUM, 0);

	/* config mode */
	if (oneshot)
		regmap_update_bits(regmap, OUTPUT_CTRL0, BIT(3), BIT(3));
	else
		regmap_update_bits(regmap, OUTPUT_CTRL0, BIT(2), BIT(2));

	return 0;
}
EXPORT_SYMBOL_GPL(global_timer_output_gpio_setup);

/**
 * global_timer_output_start
 *
 * @gtod: the pointer to struct global_timer_output
 * @expires: the 64 bit future expires value for when the pulse must start
 * @returns: 0 on success and error code on failure
 */
int global_timer_output_start(struct global_timer_output_gpio *gtod,
			      u64 expires)
{
	struct meson_glb_timer_output_dev *glb_timer_output;
	struct regmap *regmap;
	u32 ts_l;
	u32 ts_h;

	if (!gtod)
		return -EINVAL;

	glb_timer_output = gtod->glb_output_dev;
	regmap = glb_timer_output->regmap[REG_OUTPUT_CTRL];

	ts_l = expires & GENMASK_ULL(31, 0);
	ts_h = (expires >> 32) & GENMASK_ULL(31, 0);

	regmap_write(regmap, OUTPUT_EXPIRES_TS_L, ts_l);
	regmap_write(regmap, OUTPUT_EXPIRES_TS_H, ts_h);

	/* enable output */
	regmap_update_bits(regmap, OUTPUT_CTRL0, BIT(31), BIT(31));

	/* enable generator and rt */
	regmap_update_bits(glb_timer_output->regmap[REG_OUTPUT_SEL],
			   0, BIT(31) | GENMASK(3, 0),
			   BIT(31) | glb_timer_output->source);

	return 0;
}
EXPORT_SYMBOL_GPL(global_timer_output_start);

/**
 * global_timer_output_stop
 *
 * @gtod: the pointer to struct global_timer_output
 * @returns: 0 on success and error code on failure
 */
int global_timer_output_stop(struct global_timer_output_gpio *gtod)
{
	struct meson_glb_timer_output_dev *glb_timer_output;
	struct regmap *regmap;

	glb_timer_output = gtod->glb_output_dev;
	regmap = glb_timer_output->regmap[REG_OUTPUT_CTRL];

	if (!gtod)
		return -EINVAL;

	/* disable output */
	regmap_update_bits(regmap, OUTPUT_CTRL0, BIT(31), 0);

	/* disable generator and rt */
	regmap_update_bits(glb_timer_output->regmap[REG_OUTPUT_SEL],
			   0, BIT(31), 0);
	return 0;
}

static int meson_glb_timer_output_probe(struct platform_device *pdev)
{
	struct meson_glb_timer_output_dev *glb_timer_output;
	resource_size_t size;
	const char *name = NULL;
	int ret = 0;
	int i;

	glb_timer_output = devm_kzalloc(&pdev->dev, sizeof(*glb_timer_output),
					GFP_KERNEL);

	if (!glb_timer_output)
		return -ENOMEM;

	ret = of_property_read_u32(pdev->dev.of_node,
				   "amlogic,output-source",
				   &glb_timer_output->source);

	for (i = 0; i < REG_NUM; i++) {
		void __iomem *reg;
		struct regmap *regmap;

		reg = devm_of_iomap(&pdev->dev, pdev->dev.of_node, i, &size);
		glb_timer_output->reg[i] = reg;
		if (IS_ERR(reg))
			return PTR_ERR(glb_timer_output->reg[i]);

		of_property_read_string_index(pdev->dev.of_node, "reg-names",
					      i, &name);
		meson_regmap_config.max_register = size - 4;
		meson_regmap_config.name = devm_kasprintf(&pdev->dev,
							  GFP_KERNEL,
							  "%pOFn-%s",
							  pdev->dev.of_node,
							  name);
		if (!meson_regmap_config.name)
			return -ENOMEM;

		regmap = devm_regmap_init_mmio(&pdev->dev,
					       glb_timer_output->reg[i],
					       &meson_regmap_config);

		glb_timer_output->regmap[i] = regmap;

		if (IS_ERR(regmap))
			return PTR_ERR(glb_timer_output->regmap[i]);
	}

	platform_set_drvdata(pdev, glb_timer_output);
	glb_timer_output->glb_output_gpio.glb_output_dev = glb_timer_output;

	return ret;
}

static int meson_glb_timer_output_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id meson_glb_timer_output_dt_match[] = {
	{
		.compatible     = "amlogic,meson-glb-timer-gpio-output",
	},
	{}
};

static struct platform_driver meson_glb_timer_output_driver = {
	.probe = meson_glb_timer_output_probe,
	.remove = meson_glb_timer_output_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = meson_glb_timer_output_dt_match,
	},
};

module_platform_driver(meson_glb_timer_output_driver);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("AMLOGIC GLOBAL TIMER DRIVER");
MODULE_LICENSE("GPL v2");
