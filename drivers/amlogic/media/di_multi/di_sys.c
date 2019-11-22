/*
 * drivers/amlogic/media/di_multi/di_sys.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
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
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/of_device.h>

#include <linux/amlogic/media/vfm/vframe.h>

/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "deinterlace_dbg.h"
#include "deinterlace.h"
#include "di_data_l.h"
#include "di_data.h"
#include "di_dbg.h"
#include "di_vframe.h"
#include "di_task.h"
#include "di_prc.h"
#include "di_sys.h"
#include "di_api.h"

#include "register.h"
#include "nr_downscale.h"

static di_dev_t *di_pdev;

struct di_dev_s *get_dim_de_devp(void)
{
	return di_pdev;
}

unsigned int di_get_dts_nrds_en(void)
{
	return get_dim_de_devp()->nrds_enable;
}

/********************************************
 * mem
 *******************************************/

/********************************************/
static ssize_t
show_config(struct device *dev,
	    struct device_attribute *attr, char *buf)
{
	int pos = 0;

	return pos;
}

static ssize_t show_tvp_region(struct device *dev,
			       struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;
	struct di_dev_s *de_devp = get_dim_de_devp();

	len = sprintf(buff, "segment DI:%lx - %lx (size:0x%x)\n",
		      de_devp->mem_start,
		      de_devp->mem_start + de_devp->mem_size - 1,
		      de_devp->mem_size);
	return len;
}

static
ssize_t
show_log(struct device *dev, struct device_attribute *attr, char *buf)
{
	return dim_read_log(buf);
}

static ssize_t
show_frame_format(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned int channel = get_current_channel();	/*debug only*/
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (get_init_flag(channel))
		ret += sprintf(buf + ret, "%s\n",
			ppre->cur_prog_flag
			? "progressive" : "interlace");

	else
		ret += sprintf(buf + ret, "%s\n", "null");

	return ret;
}

static DEVICE_ATTR(frame_format, 0444, show_frame_format, NULL);
static DEVICE_ATTR(config, 0640, show_config, store_config);
static DEVICE_ATTR(debug, 0200, NULL, store_dbg);
static DEVICE_ATTR(dump_pic, 0200, NULL, store_dump_mem);
static DEVICE_ATTR(log, 0640, show_log, store_log);
static DEVICE_ATTR(provider_vframe_status, 0444, show_vframe_status, NULL);
static DEVICE_ATTR(tvp_region, 0444, show_tvp_region, NULL);

/********************************************/
static int di_open(struct inode *node, struct file *file)
{
	di_dev_t *di_in_devp;

/* Get the per-device structure that contains this cdev */
	di_in_devp = container_of(node->i_cdev, di_dev_t, cdev);
	file->private_data = di_in_devp;

	return 0;
}

static int di_release(struct inode *node, struct file *file)
{
/* di_dev_t *di_in_devp = file->private_data; */

/* Reset file pointer */

/* Release some other fields */
	file->private_data = NULL;
	return 0;
}

static long di_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (_IOC_TYPE(cmd) != _DI_) {
		PR_ERR("%s invalid command: %u\n", __func__, cmd);
		return -EFAULT;
	}

#if 0
	dbg_reg("no pq\n");
	return 0;
#endif
	switch (cmd) {
	case AMDI_IOC_SET_PQ_PARM:
		ret = dim_pq_load_io(arg);

		break;
	default:
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long di_compat_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = di_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations di_fops = {
	.owner		= THIS_MODULE,
	.open		= di_open,
	.release	= di_release,
	.unlocked_ioctl	= di_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= di_compat_ioctl,
#endif
};

static int dim_rev_mem(struct di_dev_s *di_devp)
{
	unsigned int ch;
	unsigned int o_size;
	unsigned long rmstart;
	unsigned int rmsize;
	unsigned int flg_map;

	if (di_devp && !di_devp->flag_cma) {
		dil_get_rev_mem(&rmstart, &rmsize);
		dil_get_flg(&flg_map);
		if (!rmstart) {
			PR_ERR("%s:reserved mem start add is 0\n", __func__);
			return -1;
		}
		di_devp->mem_start = rmstart;
		di_devp->mem_size = rmsize;

		if (!flg_map)
			di_devp->flags |= DI_MAP_FLAG;

		o_size = rmsize / DI_CHANNEL_NUB;

		for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
			di_set_mem_info(ch,
					di_devp->mem_start + (o_size * ch),
					o_size);
			PR_INF("rmem:ch[%d]:start:0x%lx, size:%uB\n",
			       ch,
			       (di_devp->mem_start + (o_size * ch)),
			       o_size);
		}
		PR_INF("rmem:0x%lx, size %uMB.\n",
		       di_devp->mem_start, (di_devp->mem_size >> 20));

		di_devp->mem_flg = true;
		return 0;
	}
	PR_INF("%s:no dev or no rev mem\n", __func__);
	return -1;
}

bool dim_rev_mem_check(void)/*tmp*/
{
	di_dev_t *di_devp = get_dim_de_devp();

	if (di_devp && !di_devp->flag_cma && di_devp->mem_flg)
		return true;

	if (!di_devp) {
		PR_ERR("%s:no dev\n", __func__);
		return false;
	}
	PR_INF("%s\n", __func__);
	dim_rev_mem(di_devp);

	return true;
}

#define ARY_MATCH (1)
#ifdef ARY_MATCH

static const struct di_meson_data  data_g12a = {
	.name = "dim_g12a",
};

static const struct di_meson_data  data_sm1 = {
	.name = "dim_sm1",
};

/* #ifdef CONFIG_USE_OF */
static const struct of_device_id amlogic_deinterlace_dt_match[] = {
	/*{ .compatible = "amlogic, deinterlace", },*/
	{	.compatible = "amlogic, dim-g12a",
		.data = &data_g12a,
	}, {	.compatible = "amlogic, dim-g12b",
		.data = &data_sm1,
	}, {	.compatible = "amlogic, dim-sm1",
		.data = &data_sm1,
	}, {}
};
#endif
static int dim_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct di_dev_s *di_devp = NULL;
	int i;
#ifdef ARY_MATCH
	const struct of_device_id *match;
	struct di_data_l_s *pdata;
#endif
	PR_INF("%s:\n", __func__);

#if 1	/*move from init to here*/

	di_pdev = kzalloc(sizeof(*di_pdev), GFP_KERNEL);
	if (!di_pdev) {
		PR_ERR("%s fail to allocate memory.\n", __func__);
		goto fail_kmalloc_dev;
	}

	/******************/
	ret = alloc_chrdev_region(&di_pdev->devno, 0, DI_COUNT, DEVICE_NAME);
	if (ret < 0) {
		PR_ERR("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	PR_INF("%s: major %d\n", __func__, MAJOR(di_pdev->devno));
	di_pdev->pclss = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(di_pdev->pclss)) {
		ret = PTR_ERR(di_pdev->pclss);
		PR_ERR("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}
#endif

	di_devp = di_pdev;
	/* *********new********* */
	di_pdev->data_l = NULL;
	di_pdev->data_l = kzalloc(sizeof(struct di_data_l_s), GFP_KERNEL);
	if (!di_pdev->data_l) {
		PR_ERR("%s fail to allocate data l.\n", __func__);
		goto fail_kmalloc_datal;
	}
	/*memset(di_pdev->data_l, 0, sizeof(struct di_data_l_s));*/
	/*pr_info("\tdata size: %ld\n", sizeof(struct di_data_l_s));*/
	/************************/
	if (!dip_prob())
		goto fail_cdev_add;

	di_devp->flags |= DI_SUSPEND_FLAG;
	cdev_init(&di_devp->cdev, &di_fops);
	di_devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&di_devp->cdev, di_devp->devno, DI_COUNT);
	if (ret)
		goto fail_cdev_add;

	di_devp->devt = MKDEV(MAJOR(di_devp->devno), 0);
	di_devp->dev = device_create(di_devp->pclss, &pdev->dev,
		di_devp->devt, di_devp, "di%d", 0);

	if (!di_devp->dev) {
		pr_error("device_create create error\n");
		goto fail_cdev_add;
	}
	dev_set_drvdata(di_devp->dev, di_devp);
	platform_set_drvdata(pdev, di_devp);

#ifdef ARY_MATCH
	/************************/
	match = of_match_device(amlogic_deinterlace_dt_match,
				&pdev->dev);
	if (!match) {
		PR_ERR("%s,no matched table\n", __func__);
		goto fail_cdev_add;
	}
	pdata = (struct di_data_l_s *)di_pdev->data_l;
	pdata->mdata = match->data;
	PR_INF("match name: %s\n", pdata->mdata->name);
#endif

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0)
		PR_INF("no reserved mem.\n");

	ret = of_property_read_u32(pdev->dev.of_node,
				   "flag_cma", &di_devp->flag_cma);
	if (ret)
		PR_ERR("DI-%s: get flag_cma error.\n", __func__);
	else
		PR_INF("flag_cma=%d\n", di_devp->flag_cma);

	dim_rev_mem(di_devp);

	ret = of_property_read_u32(pdev->dev.of_node,
				   "nrds-enable", &di_devp->nrds_enable);
	ret = of_property_read_u32(pdev->dev.of_node,
				   "pps-enable", &di_devp->pps_enable);

	/*di pre h scaling down :sm1 tm2*/
	/*pre_hsc_down_en;*/
	di_devp->h_sc_down_en = di_mp_uit_get(eDI_MP_pre_hsc_down_en);

	if (di_devp->flag_cma >= 1) {
#ifdef CONFIG_CMA
		di_devp->pdev = pdev;
		di_devp->flags |= DI_MAP_FLAG;
		#if 0
		di_devp->mem_size = dma_get_cma_size_int_byte(&pdev->dev);
		#else
		if (di_devp->flag_cma == 1	||
		    di_devp->flag_cma == 2) {
			di_devp->mem_size
			= dma_get_cma_size_int_byte(&pdev->dev);
			PR_INF("mem size from dts:0x%x\n", di_devp->mem_size);
		}

		if (di_devp->mem_size <= 0x800000) {/*need check??*/
			di_devp->mem_size = 0x2800000;
			/*(flag_cma ? 3) reserved in*/
			/*codec mm : cma in codec mm*/
			if (di_devp->flag_cma != 3) {
				/*no di cma, try use*/
				/*cma from codec mm*/
				di_devp->flag_cma = 4;
			}
		}
		#endif
		pr_info("DI: CMA size 0x%x.\n", di_devp->mem_size);
		if (di_devp->flag_cma == 2) {
			if (dim_cma_alloc_total(di_devp))
				dip_cma_st_set_ready_all();
		}
#endif
	} else {
			dip_cma_st_set_ready_all();
	}
	/* mutex_init(&di_devp->cma_mutex); */
	INIT_LIST_HEAD(&di_devp->pq_table_list);

	atomic_set(&di_devp->pq_flag, 0);

	di_devp->pre_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	pr_info("pre_irq:%d\n",
		di_devp->pre_irq);
	di_devp->post_irq = irq_of_parse_and_map(pdev->dev.of_node, 1);
	pr_info("post_irq:%d\n",
		di_devp->post_irq);

	di_pr_info("%s allocate rdma channel %d.\n", __func__,
		   di_devp->rdma_handle);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		dim_get_vpu_clkb(&pdev->dev, di_devp);
		#ifdef CLK_TREE_SUPPORT
		clk_prepare_enable(di_devp->vpu_clkb);
		pr_info("DI:enable vpu clkb.\n");
		#else
		aml_write_hiubus(HHI_VPU_CLKB_CNTL, 0x1000100);
		#endif
	}
	di_devp->flags &= (~DI_SUSPEND_FLAG);
	ret = of_property_read_u32(pdev->dev.of_node,
				   "buffer-size", &di_devp->buffer_size);
	if (ret)
		PR_ERR("DI-%s: get buffer size error.\n", __func__);

	/* set flag to indicate that post_wr is supportted */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "post-wr-support",
				   &di_devp->post_wr_support);
	if (ret)
		dimp_set(eDI_MP_post_wr_support, 0);/*post_wr_support = 0;*/
	else	/*post_wr_support = di_devp->post_wr_support;*/
		dimp_set(eDI_MP_post_wr_support, di_devp->post_wr_support);

	ret = of_property_read_u32(pdev->dev.of_node,
				   "nr10bit-support",
				   &di_devp->nr10bit_support);
	if (ret)
		dimp_set(eDI_MP_nr10bit_support, 0);/*nr10bit_support = 0;*/
	else	/*nr10bit_support = di_devp->nr10bit_support;*/
		dimp_set(eDI_MP_nr10bit_support, di_devp->nr10bit_support);

#ifdef DI_USE_FIXED_CANVAS_IDX
	if (dim_get_canvas()) {
		pr_dbg("DI get canvas error.\n");
		ret = -EEXIST;
		return ret;
	}
#endif

	device_create_file(di_devp->dev, &dev_attr_config);
	device_create_file(di_devp->dev, &dev_attr_debug);
	device_create_file(di_devp->dev, &dev_attr_dump_pic);
	device_create_file(di_devp->dev, &dev_attr_log);
	device_create_file(di_devp->dev, &dev_attr_provider_vframe_status);
	device_create_file(di_devp->dev, &dev_attr_frame_format);
	device_create_file(di_devp->dev, &dev_attr_tvp_region);

	/*pd_device_files_add*/
	get_ops_pd()->prob(di_devp->dev);

	get_ops_nr()->nr_drv_init(di_devp->dev);

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		set_init_flag(i, false);
		set_reg_flag(i, false);
	}

	set_or_act_flag(true);
	/*PR_INF("\t 11\n");*/
	ret = devm_request_irq(&pdev->dev, di_devp->pre_irq, &dim_irq,
			       IRQF_SHARED,
			       "pre_di", (void *)"pre_di");
	if (di_devp->post_wr_support) {
		ret = devm_request_irq(&pdev->dev, di_devp->post_irq,
				       &dim_post_irq,
				       IRQF_SHARED, "post_di",
				       (void *)"post_di");
	}

	di_devp->sema_flg = 1;	/*di_sema_init_flag = 1;*/
	dimh_hw_init(dimp_get(eDI_MP_pulldown_enable),
		     dimp_get(eDI_MP_mcpre_en));

	dim_set_di_flag();

	task_start();

	post_mif_sw(false);

	dim_debugfs_init();	/*2018-07-18 add debugfs*/

	dimh_patch_post_update_mc_sw(DI_MC_SW_IC, true);

	pr_info("%s:ok\n", __func__);
	return ret;

fail_cdev_add:
	pr_info("%s:fail_cdev_add\n", __func__);
	kfree(di_devp->data_l);

fail_kmalloc_datal:
	pr_info("%s:fail_kmalloc datal\n", __func__);

#if 1	/*move from init*/
/*fail_pdrv_register:*/
	class_destroy(di_pdev->pclss);
fail_class_create:
	unregister_chrdev_region(di_pdev->devno, DI_COUNT);
fail_alloc_cdev_region:
	kfree(di_pdev);
fail_kmalloc_dev:

	return ret;
#endif
	return ret;
}

static int dim_remove(struct platform_device *pdev)
{
	struct di_dev_s *di_devp = NULL;

	PR_INF("%s:\n", __func__);
	di_devp = platform_get_drvdata(pdev);

	dimh_hw_uninit();

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		clk_disable_unprepare(di_devp->vpu_clkb);

	di_devp->di_event = 0xff;

	dim_uninit_buf(1, 0);/*channel 0*/
	di_set_flg_hw_int(false);

	task_stop();

	dim_rdma_exit();

/* Remove the cdev */
	device_remove_file(di_devp->dev, &dev_attr_config);
	device_remove_file(di_devp->dev, &dev_attr_debug);
	device_remove_file(di_devp->dev, &dev_attr_log);
	device_remove_file(di_devp->dev, &dev_attr_dump_pic);
	device_remove_file(di_devp->dev, &dev_attr_provider_vframe_status);
	device_remove_file(di_devp->dev, &dev_attr_frame_format);
	device_remove_file(di_devp->dev, &dev_attr_tvp_region);
	/*pd_device_files_del*/
	get_ops_pd()->remove(di_devp->dev);
	get_ops_nr()->nr_drv_uninit(di_devp->dev);
	cdev_del(&di_devp->cdev);

	if (di_devp->flag_cma == 2) {
		if (dma_release_from_contiguous(&pdev->dev,
						di_devp->total_pages,
				di_devp->mem_size >> PAGE_SHIFT)) {
			di_devp->total_pages = NULL;
			di_devp->mem_start = 0;
			pr_dbg("DI CMA total release ok.\n");
		} else {
			pr_dbg("DI CMA total release fail.\n");
		}
		if (di_pdev->nrds_enable) {
			dim_nr_ds_buf_uninit(di_pdev->flag_cma,
					     &pdev->dev);
		}
	}
	device_destroy(di_devp->pclss, di_devp->devno);

/* free drvdata */

	dev_set_drvdata(&pdev->dev, NULL);
	platform_set_drvdata(pdev, NULL);

#if 1	/*move to remove*/
	class_destroy(di_pdev->pclss);

	dim_debugfs_exit();

	dip_exit();
	unregister_chrdev_region(di_pdev->devno, DI_COUNT);
#endif

	kfree(di_devp->data_l);
	kfree(di_pdev);

	PR_INF("%s:finish\n", __func__);
	return 0;
}

static void dim_shutdown(struct platform_device *pdev)
{
	struct di_dev_s *di_devp = NULL;
	int i;

	di_devp = platform_get_drvdata(pdev);

	for (i = 0; i < DI_CHANNEL_NUB; i++)
		set_init_flag(i, false);

	if (is_meson_txlx_cpu())
		dim_top_gate_control(true, true);
	else
		dim_DI_Wr(DI_CLKG_CTRL, 0x2);

	if (!is_meson_txlx_cpu())
		diext_clk_b_sw(false);

	PR_INF("%s.\n", __func__);
}

#ifdef CONFIG_PM

static void di_clear_for_suspend(struct di_dev_s *di_devp)
{
	unsigned int channel = get_current_channel();	/*tmp*/

	pr_info("%s\n", __func__);

	di_vframe_unreg(channel);/*have flag*/

	if (dip_chst_get(channel) != eDI_TOP_STATE_IDLE)
		dim_unreg_process_irq(channel);

	dip_cma_close();
	pr_info("%s end\n", __func__);
}

/* must called after lcd */
static int di_suspend(struct device *dev)
{
	struct di_dev_s *di_devp = NULL;

	di_devp = dev_get_drvdata(dev);
	di_devp->flags |= DI_SUSPEND_FLAG;

	di_clear_for_suspend(di_devp);

	if (!is_meson_txlx_cpu())
		diext_clk_b_sw(false);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
		clk_disable_unprepare(di_devp->vpu_clkb);
	PR_INF("%s\n", __func__);
	return 0;
}

/* must called before lcd */
static int di_resume(struct device *dev)
{
	struct di_dev_s *di_devp = NULL;

	PR_INF("%s\n", __func__);
	di_devp = dev_get_drvdata(dev);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		clk_prepare_enable(di_devp->vpu_clkb);

	di_devp->flags &= ~DI_SUSPEND_FLAG;

	/************/
	PR_INF("%s finish\n", __func__);
	return 0;
}

static const struct dev_pm_ops di_pm_ops = {
	.suspend_late = di_suspend,
	.resume_early = di_resume,
};
#endif
#ifndef ARY_MATCH
/* #ifdef CONFIG_USE_OF */
static const struct of_device_id amlogic_deinterlace_dt_match[] = {
	/*{ .compatible = "amlogic, deinterlace", },*/
	{ .compatible = "amlogic, dim-g12a", },
	{}
};
#endif
/* #else */
/* #define amlogic_deinterlace_dt_match NULL */
/* #endif */

static struct platform_driver di_driver = {
	.probe			= dim_probe,
	.remove			= dim_remove,
	.shutdown		= dim_shutdown,
	.driver			= {
		.name		= DEVICE_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = amlogic_deinterlace_dt_match,
#ifdef CONFIG_PM
		.pm			= &di_pm_ops,
#endif
	}
};

static int __init dim_module_init(void)
{
	int ret = 0;

	PR_INF("%s\n", __func__);

	ret = platform_driver_register(&di_driver);
	if (ret != 0) {
		PR_ERR("%s: failed to register driver\n", __func__);
		/*goto fail_pdrv_register;*/
		return -ENODEV;
	}
	PR_INF("%s finish\n", __func__);
	return 0;
}

static void __exit dim_module_exit(void)
{
	platform_driver_unregister(&di_driver);
	PR_INF("%s: ok.\n", __func__);
}

module_init(dim_module_init);
module_exit(dim_module_exit);

MODULE_DESCRIPTION("AMLOGIC MULTI-DI driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("4.0.0");

