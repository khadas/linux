// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/of_fdt.h>

/*for di_ext_ops*/
/*#include <linux/amlogic/media/video_sink/video.h> */
#include "di_local.h"
/***************************************
 * deinterlace in linux kernel
 **************************************/

#define DEVICE_NAME "di_local"
/*#define CLASS_NAME	"dev_pl_demo" */
#define DEV_COUNT	1

#define PR_ERR(fmt, args ...) pr_err("dil:err:" fmt, ## args)
#define PR_WARN(fmt, args ...) pr_err("dil:warn:" fmt, ## args)
#define PR_INF(fmt, args ...) pr_info("dil:" fmt, ## args)

struct dil_dev_s {
	struct platform_device	*pdev;
	unsigned long	   mem_start;
	unsigned int	   mem_size;
	unsigned int	   flg_map;/*?*/

};

static struct dil_dev_s *pdv;

static const struct di_ext_ops *dil_api;	//temp

static unsigned int diffver_flag;

/***************************************
 * dil api for make a distinction between old/new DI function *
 **************************************/
void dil_set_diffver_flag(unsigned int para)
{
	diffver_flag = para;
}
EXPORT_SYMBOL(dil_set_diffver_flag);
unsigned int dil_get_diffver_flag(void)
{
	return diffver_flag;
}
EXPORT_SYMBOL(dil_get_diffver_flag);

/***************************************
 * di api for other module *
 **************************************/
bool dil_attach_ext_api(const struct di_ext_ops *di_api)
{
	#ifdef MARK_HIS
	if (!di_api) {
		PR_ERR("%s:null\n", __func__);
		return false;
	}

	memcpy(di_api, &di_ext, sizeof(struct di_ext_ops));
	#else

	dil_api = di_api;
	#endif
	return true;
}
EXPORT_SYMBOL(dil_attach_ext_api);

unsigned int DI_POST_REG_RD(unsigned int addr)
{
	#ifdef MARK_HIS
	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		pr_err("[DI] REG 0x%x access prohibited.\n", addr);
		return 0;
	}
	return VSYNC_RD_MPEG_REG(addr);
	#endif
	if (dil_api && dil_api->di_post_reg_rd)
		return dil_api->di_post_reg_rd(addr);

	PR_ERR("%s:not attach\n", __func__);
	return 0;
}
EXPORT_SYMBOL(DI_POST_REG_RD);

int DI_POST_WR_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	#ifdef MARK_HIS
	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		pr_err("[DI] REG 0x%x access prohibited.\n", adr);
		return -1;
	}
	return VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
	#endif
	if (dil_api && dil_api->di_post_wr_reg_bits)
		return dil_api->di_post_wr_reg_bits(adr, val, start, len);

	PR_ERR("%s:not attach\n", __func__);

	return 0;
}
EXPORT_SYMBOL(DI_POST_WR_REG_BITS);

void DI_POST_UPDATE_MC(void)
{
	if (dil_api && dil_api->post_update_mc)
		dil_api->post_update_mc();
}
EXPORT_SYMBOL(DI_POST_UPDATE_MC);

/***************************************
 * reserved mem for di *
 **************************************/
void dil_get_rev_mem(unsigned long *mstart, unsigned int *msize)
{
	if (pdv) {
		*mstart = pdv->mem_start;
		*msize = pdv->mem_size;
	} else {
		*mstart = 0;
		*msize = 0;
	}
}
EXPORT_SYMBOL(dil_get_rev_mem);
void dil_get_flg(unsigned int *flg)
{
	if (pdv)
		*flg = pdv->flg_map;
	else
		*flg = 0;
}
EXPORT_SYMBOL(dil_get_flg);

/***************************************
 * reserved mem for di *
 **************************************/

int __init rmem_dil_init(struct reserved_mem *rmem,
			 struct device *dev)
{
	struct dil_dev_s *devp = dev_get_drvdata(dev);

	if (devp) {
		devp->mem_start = rmem->base;
		devp->mem_size = rmem->size;
		if (!of_get_flat_dt_prop(rmem->fdt_node, "no-map", NULL))
			devp->flg_map = 1;

#ifdef MARK_HIS
		o_size = rmem->size / DI_CHANNEL_NUB;

		for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
			di_set_mem_info(ch,
					di_devp->mem_start + (o_size * ch),
					o_size);
			PR_INF("rmem:ch[%d]:start:0x%lx, size:%uB\n",
			       ch,
				(di_devp->mem_start + (o_size * ch)),
				o_size);
		}
#endif
		PR_INF("%s:0x%lx, size %uMB.\n",
		       __func__,
			devp->mem_start, (devp->mem_size >> 20));
		return 0;
	}
	PR_ERR("%s:no devp\n", __func__);
	return 1;
}

static void rmem_dil_release(struct reserved_mem *rmem,
			     struct device *dev)
{
	struct dil_dev_s *devp = dev_get_drvdata(dev);

	if (devp) {
		devp->mem_start = 0;
		devp->mem_size = 0;
	}
	PR_INF("%s:ok\n", __func__);
}

static const struct reserved_mem_ops rmem_di_ops = {
	.device_init	= rmem_dil_init,
	.device_release = rmem_dil_release,
};

static int __init rmem_dil_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_di_ops;
/* rmem->priv = cma; */

	PR_INF("%s %pa, size %ld MiB\n",
	       __func__,
		&rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}

RESERVEDMEM_OF_DECLARE(di, "amlogic, di-mem", rmem_dil_setup);

/***************************************
 *
 ***************************************/
static int dil_probe(struct platform_device *pdev)
{
	int ret = 0;

	PR_INF("%s.\n", __func__);

	/*alloc data*/
	pdv = kzalloc(sizeof(*pdv), GFP_KERNEL);
	if (!pdv) {
		PR_ERR("%s fail to alloc pdv.\n", __func__);
		return -ENOMEM;/*goto fail_alloc_data;*/
	}
	pdv->pdev = pdev;
	platform_set_drvdata(pdev, pdv);

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0)
		PR_INF("%s no reserved mem.\n", __func__);

	PR_INF("%s ok.\n", __func__);
	return 0;
}

static int dil_remove(struct platform_device *pdev)
{
	PR_INF("%s.\n", __func__);

	/*data*/
	kfree(pdv);

	PR_INF("%s ok.\n", __func__);
	return 0;
}

static void dil_shutdown(struct platform_device *pdev)
{
	PR_INF("%s.\n", __func__);
}

static const struct of_device_id dil_match[] = {
	{
		.compatible	= "amlogic, di-local",
		.data		= NULL,
	},
	{},
};

static struct platform_driver dev_driver_tab = {
	.driver	= {
		.name		= DEVICE_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = dil_match,
	},

	.probe			= dil_probe,
	.remove			= dil_remove,
	.shutdown		= dil_shutdown,

};

int __init dil_init(void)
{
	PR_INF("%s.\n", __func__);
	if (platform_driver_register(&dev_driver_tab)) {
		PR_ERR("%s: can't register\n", __func__);
		return -ENODEV;
	}
	PR_INF("%s ok.\n", __func__);
	return 0;
}

void __exit dil_exit(void)
{
	platform_driver_unregister(&dev_driver_tab);
	PR_INF("%s: ok.\n", __func__);
}

#ifndef MODULE
module_init(dil_init);
module_exit(dil_exit);
#endif

//MODULE_DESCRIPTION("AMLOGIC DI_LOCAL driver");
//MODULE_LICENSE("GPL");
//MODULE_VERSION("4.0.0");
