// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#include "cpucore_cooling.h"
#include <linux/amlogic/gpucore_cooling.h>
#include <linux/amlogic/gpu_cooling.h>
#include <linux/amlogic/ddr_cooling.h>
#include <linux/amlogic/meson_cooldev.h>
#include <linux/cpu.h>

enum cluster_type {
	CLUSTER_BIG = 0,
	CLUSTER_LITTLE,
	NUM_CLUSTERS
};

enum cool_dev_type {
	COOL_DEV_TYPE_CPU_CORE = 0,
	COOL_DEV_TYPE_GPU_FREQ,
	COOL_DEV_TYPE_GPU_CORE,
	COOL_DEV_TYPE_DDR,
	COOL_DEV_TYPE_MAX
};

struct meson_cooldev {
	int cool_dev_num;
	struct mutex lock;/*cooling devices mutexlock*/
	struct cool_dev *cool_devs;
	struct thermal_zone_device    *tzd;
};

static struct meson_cooldev *meson_gcooldev;

static int get_cool_dev_type(char *type)
{
	if (!strcmp(type, "cpucore"))
		return COOL_DEV_TYPE_CPU_CORE;
	if (!strcmp(type, "gpufreq"))
		return COOL_DEV_TYPE_GPU_FREQ;
	if (!strcmp(type, "gpucore"))
		return COOL_DEV_TYPE_GPU_CORE;
	if (!strcmp(type, "ddr"))
		return COOL_DEV_TYPE_DDR;
	return COOL_DEV_TYPE_MAX;
}

int meson_gcooldev_min_update(struct thermal_cooling_device *cdev)
{
	return 0;
}
EXPORT_SYMBOL(meson_gcooldev_min_update);

static int register_cool_dev(struct platform_device *pdev,
			     int index, struct device_node *child)
{
	struct meson_cooldev *mcooldev = platform_get_drvdata(pdev);
	struct cool_dev *cool = &mcooldev->cool_devs[index];
	struct device_node *node;
	int c_id, pp, coeff;
	const char *node_name;

	pr_debug("meson_cdev index: %d\n", index);

	if (of_property_read_string(child, "node_name", &node_name)) {
		pr_err("thermal: read node_name failed\n");
		return -EINVAL;
	}

	switch (get_cool_dev_type(cool->device_type)) {
	case COOL_DEV_TYPE_CPU_CORE:
		node = of_find_node_by_name(NULL, node_name);
		if (!node) {
			pr_err("thermal: can't find node\n");
			return -EINVAL;
		}
		cool->np = node;
		if (of_property_read_u32(child, "cluster_id", &c_id)) {
			pr_err("thermal: read cluster_id failed\n");
			return -EINVAL;
		}
		cool->cooling_dev = cpucore_cooling_register(cool->np,
							     c_id);
		break;

	case COOL_DEV_TYPE_DDR:
		node = of_find_node_by_name(NULL, node_name);
		if (!node) {
			pr_err("thermal: can't find node\n");
			return -EINVAL;
		}
		cool->np = node;

		if (of_property_read_u32(child, "ddr_reg", &cool->ddr_reg)) {
			pr_err("thermal: read ddr reg_addr failed\n");
			return -EINVAL;
		}

		if (of_property_read_u32(child, "ddr_status", &cool->ddr_status)) {
			pr_err("thermal: read ddr reg_status failed\n");
			return -EINVAL;
		}

		if (of_property_read_u32_array(child, "ddr_bits",
					       &cool->ddr_bits[0], 2)) {
			pr_err("thermal: read ddr_bits failed\n");
			return -EINVAL;
		}

		if (of_property_read_u32_array(child, "ddr_data",
					       &cool->ddr_data[0], cool->ddr_status)) {
			pr_err("thermal: read ddr_data failed\n");
			return -EINVAL;
		}

		cool->cooling_dev = ddr_cooling_register(cool->np, cool);
		break;
	/* GPU is KO, just save these parameters */
	case COOL_DEV_TYPE_GPU_FREQ:
		node = of_find_node_by_name(NULL, node_name);
		if (!node) {
			pr_err("thermal: can't find node\n");
			return -EINVAL;
		}
		cool->np = node;
		if (of_property_read_u32(cool->np, "num_of_pp", &pp)) {
			pr_err("thermal: read num_of_pp failed\n");
			return -EINVAL;
		}

		if (of_property_read_u32(child, "dyn_coeff", &coeff)) {
			pr_err("thermal: read dyn_coeff failed\n");
			return -EINVAL;
		}
		save_gpu_cool_para(coeff, cool->np, pp);
		return 0;

	case COOL_DEV_TYPE_GPU_CORE:
		node = of_find_node_by_name(NULL, node_name);
		if (!node) {
			pr_err("thermal: can't find node\n");
			return -EINVAL;
		}
		cool->np = node;
		save_gpucore_thermal_para(cool->np);
		return 0;

	default:
		pr_err("thermal: unknown type:%s\n", cool->device_type);
		return -EINVAL;
	}

	if (IS_ERR(cool->cooling_dev)) {
		pr_err("thermal: register %s failed\n", cool->device_type);
		cool->cooling_dev = NULL;
		return -EINVAL;
	}
	return 0;
}

static int parse_cool_device(struct platform_device *pdev)
{
	struct meson_cooldev *mcooldev = platform_get_drvdata(pdev);
	struct device_node *np = pdev->dev.of_node;
	int i, ret = 0;
	struct cool_dev *cool;
	struct device_node *child;
	const char *str;

	child = of_get_child_by_name(np, "cooling_devices");
	if (!child) {
		pr_err("meson cooldev: can't found cooling_devices\n");
		return -EINVAL;
	}
	mcooldev->cool_dev_num = of_get_child_count(child);
	i = sizeof(struct cool_dev) * mcooldev->cool_dev_num;
	mcooldev->cool_devs = kzalloc(i, GFP_KERNEL);
	if (!mcooldev->cool_devs) {
		pr_err("meson cooldev: alloc mem failed\n");
		return -ENOMEM;
	}

	child = of_get_next_child(child, NULL);
	for (i = 0; i < mcooldev->cool_dev_num; i++) {
		cool = &mcooldev->cool_devs[i];
		if (!child)
			break;
		if (of_property_read_string(child, "device_type", &str))
			pr_err("thermal: read device_type failed\n");
		else
			cool->device_type = (char *)str;

		ret += register_cool_dev(pdev, i, child);
		child = of_get_next_child(np, child);
	}
	return ret;
}

static int meson_cooldev_probe(struct platform_device *pdev)
{
	struct meson_cooldev *mcooldev;

	pr_debug("meson_cdev probe\n");
	mcooldev = devm_kzalloc(&pdev->dev,
				sizeof(struct meson_cooldev),
				GFP_KERNEL);
	if (!mcooldev)
		return -ENOMEM;
	platform_set_drvdata(pdev, mcooldev);
	mutex_init(&mcooldev->lock);

	if (parse_cool_device(pdev))
		pr_info("meson_cdev one or more cooldev register fail\n");
	/*save pdev for mali ko api*/
	meson_gcooldev = platform_get_drvdata(pdev);

	pr_debug("meson_cdev probe done\n");
	return 0;
}

static int meson_cooldev_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id meson_cooldev_of_match[] = {
	{ .compatible = "amlogic, meson-cooldev" },
	{},
};

static struct platform_driver meson_cooldev_platdrv = {
	.driver = {
		.name		= "meson-cooldev",
		.owner		= THIS_MODULE,
		.of_match_table = meson_cooldev_of_match,
#if IS_ENABLED(CONFIG_AMLOGIC_BOOT_TIME)
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#endif
	},
	.probe	= meson_cooldev_probe,
	.remove	= meson_cooldev_remove,
};

static int __init meson_cooldev_platdrv_init(void)
{
	int ret;

	cpu_hotplug_init();

	ret = platform_driver_register(&(meson_tsensor_driver));
	if (ret)
		return ret;
	return platform_driver_register(&(meson_cooldev_platdrv));
}

static __exit void meson_cooldev_platdrv_exit(void)
{
	platform_driver_unregister(&meson_cooldev_platdrv);
	platform_driver_unregister(&meson_tsensor_driver);
}

late_initcall(meson_cooldev_platdrv_init);
module_exit(meson_cooldev_platdrv_exit);

MODULE_DESCRIPTION("MESON Tsensor Driver");
MODULE_AUTHOR("Huan Biao <huan.biao@amlogic.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:meson-tsensor");
