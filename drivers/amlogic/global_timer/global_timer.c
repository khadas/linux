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
#include <linux/amlogic/glb_timer.h>
#include "global_timer.h"

//#define GLB_TIMER_TEST_CASE

static struct meson_glb_timer *gbt;

static struct regmap_config meson_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static int meson_glb_timer_dt_parser(struct platform_device *pdev)
{
	int i;
	const char *name = NULL;
	struct meson_glb_timer *glb_timer;
	resource_size_t size;

	glb_timer = platform_get_drvdata(pdev);

	/* map reg  */
	for (i = 0; i < REG_NUM; i++) {
		glb_timer->reg[i] = devm_of_iomap(&pdev->dev, pdev->dev.of_node, i, &size);
		if (IS_ERR(glb_timer->reg[i]))
			return PTR_ERR(glb_timer->reg[i]);

		of_property_read_string_index(pdev->dev.of_node, "reg-names", i, &name);
		meson_regmap_config.max_register = size - 4;
		meson_regmap_config.name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%pOFn-%s",
							  pdev->dev.of_node, name);
		if (!meson_regmap_config.name)
			return -ENOMEM;

		glb_timer->regmap[i] = devm_regmap_init_mmio(&pdev->dev, glb_timer->reg[i],
							     &meson_regmap_config);
		if (IS_ERR(glb_timer->regmap[i]))
			return PTR_ERR(glb_timer->regmap[i]);
	}

	return 0;
}

/* Timer Module TopCtrl*/
static inline void meson_glb_topctl(struct meson_glb_timer *glb, unsigned int ops,
				    unsigned long long ts)
{
	unsigned int ts_l;
	unsigned int ts_h;
	struct regmap *regmap = glb->regmap[REG_TOPCTL];

	switch (ops) {
	case RESET_TIMER:
		regmap_update_bits(regmap, TOP_CTRL0, BIT(2), BIT(2));
		regmap_update_bits(regmap, TOP_CTRL0, BIT(2), 0);
		break;
	case HOLD_COUNTER:
		regmap_update_bits(regmap, TOP_CTRL0, BIT(1), BIT(1));
		break;
	case CLEAR_TIMER:
		regmap_update_bits(regmap, TOP_CTRL0, BIT(3), BIT(3));
		break;
	case SET_FORCE_COUNT:
		ts_l = ts & GENMASK_ULL(31, 0);
		ts_h = (ts >> 32) & GENMASK_ULL(31, 0);

		regmap_write(regmap, TOP_CTRL1, ts_l);
		regmap_write(regmap, TOP_CTRL2, ts_h);
		regmap_update_bits(regmap, TOP_CTRL0, BIT(0), BIT(0));

		break;
	default:
		dev_err(&glb->pdev->dev, "wrong ops\n");
		break;
	}
}

static unsigned long long meson_glb_get_cur_count(struct meson_glb_timer *glb)
{
	unsigned int ts_l;
	unsigned int ts_h;
	unsigned long long ts;
	struct regmap *regmap = glb->regmap[REG_TOPCTL];

	regmap_read(regmap, TOP_TS0, &ts_l);
	regmap_read(regmap, TOP_TS1, &ts_h);

	ts = ts_h;

	return (ts << 32) | ts_l;
}

/* Trigger Select Source Module */
static inline void meson_glb_trig_src_enable(struct meson_glb_timer *glb, u8 srcn, bool on)
{
	regmap_update_bits(glb->regmap[REG_SRCSEL], SRC_OFFSET(TRIG_SRC0_CTRL0, srcn), BIT(31),
			   on << 31);
}

static inline int meson_glb_trig_src_config(struct meson_glb_timer *glb, u8 srcn,
					    unsigned int func)
{
	struct regmap *regmap = glb->regmap[REG_SRCSEL];

	/* hold high bit when read low bit */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, srcn), BIT(30), BIT(30));

	/* set trigger method */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, srcn), GENMASK(17, 16),
					      (func & GENMASK(1, 0)) << 16);
	/* set if oneshot mode */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, srcn), BIT(29),
					      ((func & BIT(2)) << 29));

	return 0;
}

static inline void meson_glb_trig_src_oneshot(struct meson_glb_timer *glb, u8 srcn)
{
	regmap_update_bits(glb->regmap[REG_SRCSEL], SRC_OFFSET(TRIG_SRC0_CTRL0, srcn),
			   BIT(1), BIT(1));
}

static inline void meson_glb_trig_src_swtrigger(struct meson_glb_timer *glb, u8 srcn)
{
	regmap_update_bits(glb->regmap[REG_SRCSEL], SRC_OFFSET(TRIG_SRC0_CTRL0, srcn),
			   BIT(0), BIT(0));
}

static inline unsigned long long meson_glb_trig_src_get_count(struct meson_glb_timer *glb, u8 srcn)
{
	unsigned int ts_l;
	unsigned int ts_h;
	unsigned long long ts;
	struct regmap *regmap = glb->regmap[REG_SRCSEL];

	regmap_read(regmap, SRC_OFFSET(TRIG_SRC0_TS_L, srcn), &ts_l);
	regmap_read(regmap, SRC_OFFSET(TRIG_SRC0_TS_H, srcn), &ts_h);

	ts = ts_h;

	return (ts << 32) | ts_l;
}

/* Output Module */
static inline void meson_glb_output_enable(struct meson_glb_timer *glb, u8 srcn, bool on)
{
	regmap_update_bits(glb->regmap[REG_OUTCTL], SRC_OFFSET(OUTPUT_CTRL0, srcn), BIT(31),
			   on << 31);
}

static inline void meson_glb_output_mode_config(struct meson_glb_timer *glb, u8 srcn,
						unsigned int mode)
{
	struct regmap *regmap = glb->regmap[REG_OUTCTL];

	if (mode & SOFTWARE_MODE) {
		regmap_update_bits(regmap, SRC_OFFSET(OUTPUT_CTRL0, srcn), BIT(4), BIT(4));
		regmap_update_bits(regmap, SRC_OFFSET(OUTPUT_CTRL0, srcn), BIT(2), BIT(2));
	}

	if (mode & ONESHOT_MODE)
		regmap_update_bits(regmap, SRC_OFFSET(OUTPUT_CTRL0, srcn), BIT(3), BIT(3));
}

static inline void meson_glb_output_sw_start(struct meson_glb_timer *glb, u8 srcn)
{
	regmap_update_bits(glb->regmap[REG_OUTCTL], SRC_OFFSET(OUTPUT_CTRL0, srcn), BIT(1), BIT(1));
}

static inline void meson_glb_output_sw_stop(struct meson_glb_timer *glb, u8 srcn)
{
	regmap_update_bits(glb->regmap[REG_OUTCTL], SRC_OFFSET(OUTPUT_CTRL0, srcn), BIT(0), BIT(0));
}

static inline void meson_glb_output_fource_output(struct meson_glb_timer *glb, u8 srcn,
						  enum output_level level)
{
	struct regmap *regmap = glb->regmap[REG_OUTCTL];

	/* output level */
	regmap_update_bits(regmap, SRC_OFFSET(OUTPUT_CTRL0, srcn), GENMASK(17, 16),
			   BIT(17) | (level << 16));
	/* change output dir */
	regmap_update_bits(regmap, SRC_OFFSET(OUTPUT_CTRL0, srcn), GENMASK(19, 18), BIT(19));
}

static inline void meson_glb_output_fource_input(struct meson_glb_timer *glb, u8 srcn)
{
	regmap_update_bits(glb->regmap[REG_OUTCTL], SRC_OFFSET(OUTPUT_CTRL0, srcn),
			   GENMASK(19, 18), GENMASK(19, 18));
}

static inline void meson_glb_output_pulse_config(struct meson_glb_timer *glb, u8 srcn,
						 bool pulse_polarity, u32 pulse_width,
						 u32 pulse_interval, unsigned int pulse_num)
{
	struct regmap *regmap = glb->regmap[REG_OUTCTL];

	if (pulse_polarity)
		regmap_update_bits(regmap, SRC_OFFSET(OUTPUT_CTRL0, srcn), BIT(28), BIT(28));

	regmap_write(regmap, SRC_OFFSET(OUTPUT_PULSE_WDITH, srcn), pulse_width);
	regmap_write(regmap, SRC_OFFSET(OUTPUT_INTERVAL, srcn), pulse_interval);
	regmap_write(regmap, SRC_OFFSET(OUTPUT_ST0, srcn), pulse_num - 1);
}

static inline void meson_glb_output_set_expire(struct meson_glb_timer *glb, u8 srcn,
					       unsigned long long ts)
{
	unsigned int ts_l;
	unsigned int ts_h;
	struct regmap *regmap = glb->regmap[REG_OUTCTL];

	ts_l = ts & GENMASK_ULL(31, 0);
	ts_h = (ts >> 32) & GENMASK_ULL(31, 0);

	regmap_write(regmap, SRC_OFFSET(OUTPUT_EXPIRES_TS_L, srcn), ts_l);
	regmap_write(regmap, SRC_OFFSET(OUTPUT_EXPIRES_TS_H, srcn), ts_h);
}

/* Output Source Select */
static inline void meson_glb_set_output_src(struct meson_glb_timer *glb, u8 srcn)
{
	regmap_update_bits(glb->regmap[REG_OUTSEL], (srcn << 2),
			   BIT(31) | GENMASK(3, 0), srcn | BIT(31));
}

int glb_timer_mipi_config(u8 srcn, unsigned int trig)
{
	if (!gbt) {
		pr_err("gbl timer not probe yet\n");
		return -ENODEV;
	}

	meson_glb_trig_src_config(gbt, srcn, trig);
	meson_glb_trig_src_enable(gbt, srcn, 1);

	return 0;
}
EXPORT_SYMBOL(glb_timer_mipi_config);

unsigned long long glb_timer_get_counter(u8 srcn)
{
	if (!gbt) {
		pr_err("gbl timer not probe yet\n");
		return -ENODEV;
	}

	return meson_glb_trig_src_get_count(gbt, srcn);
}
EXPORT_SYMBOL(glb_timer_get_counter);

static ssize_t current_val_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct meson_glb_timer *glb_timer = dev_get_drvdata(dev);
	ssize_t	ret = 0;

	ret = sprintf(buf, "%lld\n", meson_glb_get_cur_count(glb_timer));

	return ret;
}

static ssize_t reset_timer_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct meson_glb_timer *glb_timer = dev_get_drvdata(dev);

	meson_glb_topctl(glb_timer, RESET_TIMER, 0);

	return size;
}

static DEVICE_ATTR_RO(current_val);
static DEVICE_ATTR_WO(reset_timer);
static struct attribute *glb_timer_attrs[] = {
	&dev_attr_current_val.attr,
	&dev_attr_reset_timer.attr,
	NULL
};

static const struct attribute_group glb_timer_attribute_group = {
	.attrs = glb_timer_attrs,
	.name = "glb_timer"
};

#ifdef GLB_TIMER_TEST_CASE
static unsigned int test_time;
static enum hrtimer_restart tst_hrtimer_handler(struct hrtimer *hrtimer)
{
	struct meson_glb_timer *glb;

	glb = container_of(hrtimer, struct meson_glb_timer, tst_hrtimer);

	if (++test_time % 5 == 0) {
		dev_info(&glb->pdev->dev, "reset glb timer !!\n");
		meson_glb_topctl(glb, RESET_TIMER, 0);
	} else {
		dev_info(&glb->pdev->dev, "read current count %lld\n",
			 meson_glb_get_cur_count(glb));
	}

	if (test_time != 30)
		hrtimer_start(&glb->tst_hrtimer, ktime_set(0, 100 * 1000 * 1000), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}
#endif

static int meson_glb_timer_probe(struct platform_device *pdev)
{
	int ret;
	struct meson_glb_timer *glb_timer;

	glb_timer = devm_kzalloc(&pdev->dev, sizeof(*glb_timer), GFP_KERNEL);

	/* set clk */
	glb_timer->glb_clk = devm_clk_get(&pdev->dev, "glb_clk");
	if (IS_ERR(glb_timer->glb_clk)) {
		dev_err(&pdev->dev, "Can't find glb_clk\n");
		return PTR_ERR(glb_timer->glb_clk);
	}

	ret = clk_prepare_enable(glb_timer->glb_clk);
	if (ret) {
		dev_err(&pdev->dev, "clk enable failed\n");
		return ret;
	}

	platform_set_drvdata(pdev, glb_timer);
	glb_timer->pdev = pdev;

	ret = meson_glb_timer_dt_parser(pdev);
	if (ret)
		return ret;

	/* /sys/devices/platform/soc/fe000000.apb4/fe08e000.global_timer/glb_timer/ */
	ret = sysfs_create_group(&pdev->dev.kobj, &glb_timer_attribute_group);

#ifdef GLB_TIMER_TEST_CASE
	hrtimer_init(&glb_timer->tst_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	glb_timer->tst_hrtimer.function = tst_hrtimer_handler;
	/* 100ms */
	hrtimer_start(&glb_timer->tst_hrtimer, ktime_set(0, 100 * 1000 * 1000), HRTIMER_MODE_REL);
#endif
	gbt = glb_timer;

	return 0;
}

static int meson_glb_timer_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static int meson_glb_timer_resume(struct device *dev)
{
	return 0;
}

static int meson_glb_timer_suspend(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops meson_glb_timer_pm_ops = {
	.suspend = meson_glb_timer_suspend,
	.resume = meson_glb_timer_resume,
};
#endif

static const struct of_device_id meson_glb_timer_dt_match[] = {
	{
		.compatible     = "amlogic, meson-glb-timer",
	},
	{}
};

static struct platform_driver meson_glb_timer_driver = {
	.probe = meson_glb_timer_probe,
	.remove = meson_glb_timer_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = meson_glb_timer_dt_match,
#ifdef CONFIG_PM
		.pm = &meson_glb_timer_pm_ops,
#endif
	},
};

static int __init meson_glb_timer_driver_init(void)
{
	return platform_driver_register(&meson_glb_timer_driver);
}

static void __exit meson_glb_timer_driver_exit(void)
{
	platform_driver_unregister(&meson_glb_timer_driver);
}

module_init(meson_glb_timer_driver_init);
module_exit(meson_glb_timer_driver_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("AMLOGIC GLOBAL TIMER DRIVER");
MODULE_LICENSE("GPL v2");

