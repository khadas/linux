// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/gki_module.h>

#define VSPR(fmt, args...)     pr_info("vout sys: " fmt "", ## args)
#define VSERR(fmt, args...)    pr_err("vout sys: error: " fmt "", ## args)

/* ********************************
 * mem map
 * *********************************
 */
struct vs_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

static struct vs_reg_map_s *vs_reg_map;

static int vs_ioremap(struct platform_device *pdev)
{
	//int i;
	//int *table;
	struct resource *res;

	vs_reg_map = kzalloc(sizeof(*vs_reg_map), GFP_KERNEL);
	if (!vs_reg_map)
		return -1;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		VSERR("%s: resource get error\n", __func__);
		kfree(vs_reg_map);
		vs_reg_map = NULL;
		return -1;
	}
	vs_reg_map->base_addr = res->start;
	vs_reg_map->size = resource_size(res);
	vs_reg_map->p =
		devm_ioremap_nocache(&pdev->dev, res->start, vs_reg_map->size);
	if (!vs_reg_map->p) {
		vs_reg_map->flag = 0;
		VSERR("%s: reg map failed: 0x%x\n",
			__func__, vs_reg_map->base_addr);
		kfree(vs_reg_map);
		vs_reg_map = NULL;
		return -1;
	}
	vs_reg_map->flag = 1;
	/*
	 *VCLKPR("%s: reg mapped: 0x%x -> %px\n",
	 *	 __func__, vs_reg_map->base_addr, vs_reg_map->p);
	 */

	return 0;
}

static inline void __iomem *check_vs_reg(void)
{
	if (!vs_reg_map)
		return NULL;
	if (vs_reg_map->flag == 0) {
		VSERR("reg 0x%x mapped error\n", vs_reg_map->base_addr);
		return NULL;
	}

	return vs_reg_map->p;
}

unsigned int vs_reg_read(void)
{
	void __iomem *p;
	unsigned int ret = 0;

	p = check_vs_reg();
	if (p)
		ret = readl(p);
	else
		ret = 0;

	return ret;
};

static void vs_reg_write(unsigned int value)
{
	//unsigned long flags = 0;
	void __iomem *p;

	p = check_vs_reg();
	if (p)
		writel(value, p);
};

static void vout_sys_update_info(void)
{
	const struct vinfo_s *vinfo;
	unsigned int data32;

	vinfo = get_current_vinfo();
	if (!vinfo)
		return;
	data32 = (vinfo->sync_duration_num << 16) |
		vinfo->sync_duration_den;
	vs_reg_write(data32);
	VSPR("update date: 0x%08x\n", vs_reg_read());
}

static int vout_sys_notify_callback(struct notifier_block *block,
				    unsigned long cmd, void *para)
{
	switch (cmd) {
	case VOUT_EVENT_SYS_INIT:
	case VOUT_EVENT_MODE_CHANGE:
		vout_sys_update_info();
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block vout_sys_notifier = {
	.notifier_call = vout_sys_notify_callback,
};

/*****************************************************************
 **
 **	vout driver interface
 **
 ******************************************************************/
static const struct of_device_id vs_match_table[] = {
	{
		.compatible = "amlogic, vout_sys_serve",
	},
	{ }
};

static int aml_vs_probe(struct platform_device *pdev)
{
	vs_ioremap(pdev);

	vout_register_client(&vout_sys_notifier);

	VSPR("%s OK\n", __func__);
	return 0;
}

static int aml_vs_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver aml_vs_driver = {
	.probe     = aml_vs_probe,
	.remove    = aml_vs_remove,
	.driver = {
		.name = "vout_sys",
		.of_match_table = vs_match_table,
	},
};

int __init vout_sys_serve_init(void)
{
	int ret = 0;

	if (platform_driver_register(&aml_vs_driver)) {
		VSERR("failed to register vout_sys_serve driver\n");
		ret = -ENODEV;
	}

	return ret;
}

__exit void vout_sys_serve_exit(void)
{
	platform_driver_unregister(&aml_vs_driver);
}

//MODULE_AUTHOR("Evoke Zhang <evoke.zhang@amlogic.com>");
//MODULE_DESCRIPTION("VOUT_SYS Server Module");
//MODULE_LICENSE("GPL");
