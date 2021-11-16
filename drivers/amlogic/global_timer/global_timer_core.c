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
#include <linux/hwspinlock.h>
#include <linux/amlogic/glb_timer.h>

#define DRIVER_NAME	"global_timer_core"
#define GLOBAL_TIMER_SNAPSHOT      _IOR('Z', 11, __u64)
#define HWSPIN_LOCK_TIMEOUT	   5

enum meson_glb_topctl_reg {
	TOP_CTRL0			= 0x00 << 2,
	TOP_CTRL1			= 0x01 << 2,
	TOP_CTRL2			= 0x02 << 2,
	TOP_CTRL3			= 0x03 << 2,
	TOP_CTRL4			= 0x04 << 2,
	TOP_TS0				= 0x08 << 2,
	TOP_TS1				= 0x09 << 2,
};

static struct regmap_config meson_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.fast_io = true,
};

struct meson_glb_timer_core_pdata {
	void __iomem *reg;
	struct regmap *regmap;
	struct clk *glb_clk;
	struct platform_device *pdev;
	struct cdev chrdev;
	dev_t chr_devno;
	struct hwspinlock *hwlock;
};

struct meson_glb_timer_core_pdata *glb_pdata;

/**
 * ns_to_global_timer_count
 *
 * @time_in_ns: input time in ns
 * @returns: time in global timer ticks
 */
u64 ns_to_global_timer_count(u64 time_in_ns)
{
	do_div(time_in_ns, 42);
	return time_in_ns;
}
EXPORT_SYMBOL_GPL(ns_to_global_timer_count);

/**
 * global_timer_count_to_ns
 *
 * @time_in_global_timer_ticks: input time in global timer ticks
 * @returns: time in ns
 */
u64 global_timer_count_to_ns(u64 time_in_global_timer_ticks)
{
	return time_in_global_timer_ticks * 42;
}
EXPORT_SYMBOL_GPL(global_timer_count_to_ns);

/**
 * meson_global_timer_global_snapshot
 * @returns the global timer 64 bit snapshot value
 */
u64 meson_global_timer_global_snapshot(void)
{
	u32 ts_l;
	u32 ts_h;
	u64 ts;
	struct regmap *regmap;

	if (!glb_pdata) {
		pr_err("gbl timer not probe yet\n");
		return -ENODEV;
	}

	regmap = glb_pdata->regmap;

	hwspin_lock_timeout(glb_pdata->hwlock, HWSPIN_LOCK_TIMEOUT);
	regmap_read(regmap, TOP_TS0, &ts_l);
	regmap_read(regmap, TOP_TS1, &ts_h);
	hwspin_unlock(glb_pdata->hwlock);

	ts = ts_h;

	return (ts << 32) | ts_l;
}
EXPORT_SYMBOL_GPL(meson_global_timer_global_snapshot);

/**
 * meson_global_timer_reset
 * @returns 0 if successful or appropriate error code
 */
int meson_global_timer_reset(void)
{
	struct regmap *regmap;

	if (!glb_pdata) {
		pr_err("gbl timer not probe yet\n");
		return -ENODEV;
	}

	regmap = glb_pdata->regmap;
	regmap_update_bits(regmap, TOP_CTRL0, BIT(2), BIT(2));
	regmap_update_bits(regmap, TOP_CTRL0, BIT(2), 0);

	return 0;
}
EXPORT_SYMBOL_GPL(meson_global_timer_reset);

static int meson_glb_timer_core_mmio(struct platform_device *pdev)
{
	resource_size_t size;
	const char *name = NULL;
	struct meson_glb_timer_core_pdata *glb_timer;

	glb_timer = platform_get_drvdata(pdev);

	glb_timer->reg = devm_of_iomap(&pdev->dev, pdev->dev.of_node, 0, &size);
	if (IS_ERR(glb_timer->reg))
		return PTR_ERR(glb_timer->reg);

	of_property_read_string_index(pdev->dev.of_node, "reg-names", 0, &name);
	meson_regmap_config.max_register = size - 4;
	meson_regmap_config.name = devm_kasprintf(&pdev->dev, GFP_KERNEL,
						  "%pOFn-%s",
						  pdev->dev.of_node, name);
	if (!meson_regmap_config.name)
		return -ENOMEM;

	glb_timer->regmap = devm_regmap_init_mmio(&pdev->dev, glb_timer->reg,
						     &meson_regmap_config);
	if (IS_ERR(glb_timer->regmap))
		return PTR_ERR(glb_timer->regmap);
	return 0;
}

static ssize_t value_show(struct class *class, struct class_attribute *attr,
		   char *buf)
{
	u64 ts;

	ts = meson_global_timer_global_snapshot();

	return sprintf(buf, "%lld\n", ts);
}
static CLASS_ATTR_RO(value);

static ssize_t reset_store(struct class *class, struct class_attribute *attr,
			      const char *buf, size_t len)
{
	meson_global_timer_reset();
	return len;
}
static CLASS_ATTR_WO(reset);

static ssize_t value_bin_read(struct file *filp, struct kobject *kobj,
			      struct bin_attribute *attr, char *buf,
			      loff_t off, size_t count)
{
	u64 ts;
	int ret;

	ts = meson_global_timer_global_snapshot();
	ret = snprintf(buf, sizeof(ts), "%lld\n", ts);

	return ret;
}
static BIN_ATTR_RO(value_bin, sizeof(u64));

static struct bin_attribute *glb_timer_core_class_bin_attrs[] = {
	&bin_attr_value_bin,
	NULL,
};

static struct attribute *glb_timer_core_class_attrs[] = {
	&class_attr_value.attr,
	&class_attr_reset.attr,
	NULL,
};

static const struct attribute_group glb_timer_core_class_group = {
	.attrs = glb_timer_core_class_attrs,
	.bin_attrs = glb_timer_core_class_bin_attrs,
};

static const struct attribute_group *glb_timer_core_class_groups[] = {
	&glb_timer_core_class_group,
	NULL,
};

static struct class global_timer_class = {
	.name =         "global_timer",
	.owner =        THIS_MODULE,
	.class_groups = glb_timer_core_class_groups,
};

static int meson_glb_timer_core_open(struct inode *inode, struct file *file)
{
	struct meson_glb_timer_core_pdata *glb;

	glb = container_of(inode->i_cdev, struct meson_glb_timer_core_pdata,
			   chrdev);
	file->private_data = glb;

	return 0;
}

static long meson_glb_timer_core_ioctl(struct file *file, unsigned int cmd,
				       unsigned long arg)
{
	u64 ts;
	void __user *parg = (void __user *)arg;

	if (!parg) {
		pr_err("%s invalid user space pointer\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case GLOBAL_TIMER_SNAPSHOT:
		ts = meson_global_timer_global_snapshot();
		if (copy_to_user(parg, &ts, sizeof(u64)))
			return -EFAULT;

		break;
	default:
		break;
	};

	return 0;
}

static int meson_glb_timer_core_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations meson_glb_timer_core_fops = {
	.owner = THIS_MODULE,
	.open = meson_glb_timer_core_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = meson_glb_timer_core_ioctl,
#endif
	.unlocked_ioctl = meson_glb_timer_core_ioctl,
	.release = meson_glb_timer_core_release,
};

int meson_glb_timer_core_cdev_init(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	struct meson_glb_timer_core_pdata *glb_timer;

	glb_timer = platform_get_drvdata(pdev);

	ret = alloc_chrdev_region(&glb_timer->chr_devno, 0, 1, DRIVER_NAME);

	if (ret < 0) {
		pr_err("failed to allocate major number\n");
		return ret;
	}

	cdev_init(&glb_timer->chrdev, &meson_glb_timer_core_fops);
	glb_timer->chrdev.owner = THIS_MODULE;
	ret = cdev_add(&glb_timer->chrdev, glb_timer->chr_devno, 1);

	if (ret < 0) {
		pr_err("cdev_add failed\n");
		goto fail_cdev_add;
	}

	dev = device_create(&global_timer_class, NULL, glb_timer->chr_devno,
			    glb_timer, DRIVER_NAME);
	if (IS_ERR(dev)) {
		pr_err("dev create failed\n");
		ret =  PTR_ERR(dev);
		goto fail_dev_create;
	}
	return ret;

fail_dev_create:
	cdev_del(&glb_timer->chrdev);
fail_cdev_add:
	unregister_chrdev_region(glb_timer->chr_devno, 1);
	return ret;
}

void meson_glb_timer_core_cdev_deint(struct platform_device *pdev)
{
	struct meson_glb_timer_core_pdata *glb_timer;

	glb_timer = platform_get_drvdata(pdev);

	cdev_del(&glb_timer->chrdev);
	unregister_chrdev_region(glb_timer->chr_devno, 1);
}

static int meson_glb_timer_core_probe(struct platform_device *pdev)
{
	int ret = 0;
	int hwlock_id;
	struct meson_glb_timer_core_pdata *glb_timer;

	glb_timer = devm_kzalloc(&pdev->dev, sizeof(*glb_timer), GFP_KERNEL);
	if (!glb_timer)
		return -ENOMEM;

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

	/* regmap mmio */
	ret = meson_glb_timer_core_mmio(pdev);
	if (ret)
		return ret;
	/* sysfs */
	ret = class_register(&global_timer_class);
	if (ret)
		return ret;

	/* ioctl */
	ret = meson_glb_timer_core_cdev_init(pdev);
	if (ret)
		goto err_ioctl;

	/* hwspinlock */
	hwlock_id = of_hwspin_lock_get_id(pdev->dev.of_node, 0);
	if (hwlock_id < 0)
		goto err_hwspinlock;

	glb_timer->hwlock = hwspin_lock_request_specific(hwlock_id);
	if (!glb_timer->hwlock)
		goto err_hwspinlock;

	glb_pdata = glb_timer;

	return ret;

err_hwspinlock:
	meson_glb_timer_core_cdev_deint(pdev);
err_ioctl:
	class_unregister(&global_timer_class);
	return ret;
}

static int meson_glb_timer_core_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static int meson_glb_timer_core_resume(struct device *dev)
{
	return 0;
}

static int meson_glb_timer_core_suspend(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops meson_glb_timer_core_pm_ops = {
	.suspend = meson_glb_timer_core_suspend,
	.resume = meson_glb_timer_core_resume,
};
#endif

static const struct of_device_id meson_glb_timer_core_dt_match[] = {
	{
		.compatible     = "amlogic,meson-glb-timer-core",
	},
	{}
};

static struct platform_driver meson_glb_timer_core_driver = {
	.probe = meson_glb_timer_core_probe,
	.remove = meson_glb_timer_core_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = meson_glb_timer_core_dt_match,
#ifdef CONFIG_PM
		.pm = &meson_glb_timer_core_pm_ops,
#endif
	},
};

module_platform_driver(meson_glb_timer_core_driver);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("AMLOGIC GLOBAL TIMER DRIVER");
MODULE_LICENSE("GPL v2");

