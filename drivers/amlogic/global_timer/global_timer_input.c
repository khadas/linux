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
#include <linux/amlogic/glb_timer.h>

#define DRIVER_NAME	"global_timer_input"

static struct regmap_config meson_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.fast_io = true,
};

struct meson_glb_timer_input_pdata {
	void __iomem *reg;
	struct regmap *regmap;
	struct platform_device *pdev;
};

struct meson_glb_timer_input_pdata *glb_timer_input_padata;

/**
 * global_timer_input_gpio_get_source_index
 *
 * @virq: the vitural number from gpiod_to_irq used to find out the global
 * timer input source ID
 * @returns: the input source id for given virq. or error code
 */
int meson_global_timer_input_gpio_get_source_index(int virq)
{
	return gpio_irq_get_channel_idx(virq);
}
EXPORT_SYMBOL_GPL(meson_global_timer_input_gpio_get_source_index);

/**
 * global_timer_input_gpio_configure
 *
 * @id: the input source id
 * @trigger_type: the trigger type, rising edge, falling edge
 * (can be found on include/linux/global_timer.h)
 * @returns: 0 on success or appropriate error code
 */
int meson_global_timer_input_gpio_configure(u8 id,
					enum meson_glb_srcsel_flag trigger_type)
{
	struct regmap *regmap = glb_timer_input_padata->regmap;

	if (!glb_timer_input_padata) {
		pr_err("glb timer input driver no probe yet\n");
		return -ENODEV;
	}

	/* hold high bit when read low bit */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, id),
			   BIT(30), BIT(30));

	/* set trigger method */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, id),
			   GENMASK(17, 16),
			   (trigger_type & GENMASK(1, 0)) << 16);

	/* set if oneshot mode */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, id),
			   BIT(29), ((trigger_type & BIT(2)) << 29));

	/* enable source */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, id),
			   BIT(31), BIT(31));
	return 0;
}
EXPORT_SYMBOL_GPL(meson_global_timer_input_gpio_configure);

/**
 * global_timer_input_gpio_get_snapshot
 *
 * @id: the input source id
 * @returns: the global timer 64 bit snapshot value
 */
u64 meson_global_timer_input_gpio_get_snapshot(int id)
{
	u32 ts_l;
	u32 ts_h;
	u64 ts;

	struct regmap *regmap = glb_timer_input_padata->regmap;

	regmap_read(regmap, SRC_OFFSET(TRIG_SRC0_TS_L, id), &ts_l);
	regmap_read(regmap, SRC_OFFSET(TRIG_SRC0_TS_H, id), &ts_h);

	ts = ts_h;

	return (ts << 32) | ts_l;
}
EXPORT_SYMBOL_GPL(meson_global_timer_input_gpio_get_snapshot);

static int meson_glb_timer_input_probe(struct platform_device *pdev)
{
	resource_size_t size;
	const char *name = NULL;
	struct meson_glb_timer_input_pdata *glb_timer_input;

	glb_timer_input = devm_kzalloc(&pdev->dev, sizeof(*glb_timer_input),
				       GFP_KERNEL);

	if (!glb_timer_input)
		return -ENOMEM;

	platform_set_drvdata(pdev, glb_timer_input);
	glb_timer_input->pdev = pdev;

	glb_timer_input->reg = devm_of_iomap(&pdev->dev, pdev->dev.of_node, 0,
					     &size);
	if (IS_ERR(glb_timer_input->reg))
		return PTR_ERR(glb_timer_input->reg);

	of_property_read_string_index(pdev->dev.of_node, "reg-names", 0, &name);
	meson_regmap_config.max_register = size - 4;
	meson_regmap_config.name = devm_kasprintf(&pdev->dev, GFP_KERNEL,
						  "%pOFn-%s",
						  pdev->dev.of_node, name);
	if (!meson_regmap_config.name)
		return -ENOMEM;

	glb_timer_input->regmap = devm_regmap_init_mmio(&pdev->dev,
							glb_timer_input->reg,
							&meson_regmap_config);
	if (IS_ERR(glb_timer_input->regmap))
		return PTR_ERR(glb_timer_input->regmap);

	glb_timer_input_padata = glb_timer_input;

	return 0;
}

static int meson_glb_timer_input_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id meson_glb_timer_input_dt_match[] = {
	{
		.compatible     = "amlogic,meson-glb-timer-gpio-input",
	},
	{}
};

static struct platform_driver meson_glb_timer_input_driver = {
	.probe = meson_glb_timer_input_probe,
	.remove = meson_glb_timer_input_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = meson_glb_timer_input_dt_match,
	},
};

module_platform_driver(meson_glb_timer_input_driver);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("AMLOGIC GLOBAL TIMER DRIVER");
MODULE_LICENSE("GPL v2");

