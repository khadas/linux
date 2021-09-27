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

#define DRIVER_NAME	"global_timer_isp"

static struct regmap_config meson_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.fast_io = true,
};

struct meson_glb_timer_isp_pdata {
	void __iomem *reg;
	struct regmap *regmap;
	struct platform_device *pdev;
};

struct meson_glb_timer_isp_pdata *glb_timer_isp_padata;

/**
 * meson_global_timer_isp_event_snapshot_configure
 * @isp_event_src: the ISP event source number
 * (can be found on include/linux/global_timer.h)
 * @trigger_type: the trigger type, rising edge, falling edge
 * (can be found on include/linux/global_timer.h)
 * @returns: the global timer 64 bit snapshot value
 */
int meson_global_timer_isp_event_snapshot_configure(u8 isp_event_src,
					enum meson_glb_srcsel_flag trigger_type)
{
	struct regmap *regmap;

	if (!glb_timer_isp_padata) {
		pr_err("glb timer isp driver no probe yet\n");
		return -ENODEV;
	}

	regmap = glb_timer_isp_padata->regmap;

	/* hold high bit when read low bit */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, isp_event_src),
			   BIT(30), BIT(30));

	/* set trigger method */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, isp_event_src),
			   GENMASK(17, 16),
			   (trigger_type & GENMASK(1, 0)) << 16);

	/* set if oneshot mode */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, isp_event_src),
			   BIT(29), ((trigger_type & BIT(2)) << 29));

	/* enable source */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, isp_event_src),
			   BIT(31), BIT(31));
	return 0;
}
EXPORT_SYMBOL_GPL(meson_global_timer_isp_event_snapshot_configure);

/**
 * meson_global_timer_isp_event_snapshot
 *
 * @isp_event_src: the ISP event source number
 * (can be found on include/linux/global_timer.h)
 * @returns: the global timer 64 bit snapshot value
 */
u64 meson_global_timer_isp_snapshot(u8 isp_event_src)
{
	u32 ts_l;
	u32 ts_h;
	u64 ts;
	struct regmap *regmap;

	if (!glb_timer_isp_padata) {
		pr_err("glb timer isp driver no probe yet\n");
		return -ENODEV;
	}

	regmap = glb_timer_isp_padata->regmap;

	regmap_read(regmap, SRC_OFFSET(TRIG_SRC0_TS_L, isp_event_src), &ts_l);
	regmap_read(regmap, SRC_OFFSET(TRIG_SRC0_TS_H, isp_event_src), &ts_h);

	ts = ts_h;

	return (ts << 32) | ts_l;
}
EXPORT_SYMBOL_GPL(meson_global_timer_isp_snapshot);

int glb_timer_mipi_config(u8 srcn, unsigned int trig)
{
	return meson_global_timer_isp_event_snapshot_configure(srcn, trig);
}
EXPORT_SYMBOL_GPL(glb_timer_mipi_config);

unsigned long long glb_timer_get_counter(u8 srcn)
{
	return meson_global_timer_isp_snapshot(srcn);
}
EXPORT_SYMBOL_GPL(glb_timer_get_counter);

static int meson_glb_timer_isp_probe(struct platform_device *pdev)
{
	resource_size_t size;
	const char *name = NULL;
	struct meson_glb_timer_isp_pdata *glb_timer_isp;

	glb_timer_isp = devm_kzalloc(&pdev->dev, sizeof(*glb_timer_isp),
				     GFP_KERNEL);

	if (!glb_timer_isp)
		return -ENOMEM;

	platform_set_drvdata(pdev, glb_timer_isp);
	glb_timer_isp->pdev = pdev;

	glb_timer_isp->reg = devm_of_iomap(&pdev->dev, pdev->dev.of_node, 0,
					   &size);
	if (IS_ERR(glb_timer_isp->reg))
		return PTR_ERR(glb_timer_isp->reg);

	of_property_read_string_index(pdev->dev.of_node, "reg-names", 0, &name);
	meson_regmap_config.max_register = size - 4;
	meson_regmap_config.name = devm_kasprintf(&pdev->dev, GFP_KERNEL,
						  "%pOFn-%s",
						  pdev->dev.of_node, name);
	if (!meson_regmap_config.name)
		return -ENOMEM;

	glb_timer_isp->regmap = devm_regmap_init_mmio(&pdev->dev,
						      glb_timer_isp->reg,
						      &meson_regmap_config);
	if (IS_ERR(glb_timer_isp->regmap))
		return PTR_ERR(glb_timer_isp->regmap);
	glb_timer_isp_padata = glb_timer_isp;

	return 0;
}

static int meson_glb_timer_isp_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id meson_glb_timer_isp_dt_match[] = {
	{
		.compatible     = "amlogic,meson-glb-timer-isp",
	},
	{}
};

static struct platform_driver meson_glb_timer_isp_driver = {
	.probe = meson_glb_timer_isp_probe,
	.remove = meson_glb_timer_isp_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = meson_glb_timer_isp_dt_match,
	},
};

module_platform_driver(meson_glb_timer_isp_driver);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("AMLOGIC GLOBAL TIMER DRIVER");
MODULE_LICENSE("GPL v2");

