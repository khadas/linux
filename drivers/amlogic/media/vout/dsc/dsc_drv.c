// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/dsc/dsc.h>
#include "dsc_drv.h"
#include "dsc_reg.h"
#include "dsc_config.h"
#include "dsc_hw.h"
#include "dsc_debug.h"

#include <linux/amlogic/gki_module.h>

#define DSC_CDEV_NAME  "aml_dsc"

static struct dsc_cdev_s *dsc_cdev;
static struct aml_dsc_drv_s *dsc_drv_local;

struct aml_dsc_drv_s *dsc_drv_get(void)
{
	return dsc_drv_local;
}

static inline int dsc_ioremap(struct platform_device *pdev, struct aml_dsc_drv_s *dsc_drv)
{
	struct resource *res;
	int size_io_reg;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		size_io_reg = resource_size(res);
		DSC_PR("%s: dsc reg base=0x%p,size=0x%x\n",
			__func__, (void *)res->start, size_io_reg);
		dsc_drv->dsc_reg_base =
			devm_ioremap_nocache(&pdev->dev, res->start, size_io_reg);
		if (!dsc_drv->dsc_reg_base) {
			dev_err(&pdev->dev, "dsc ioremap failed\n");
			return -ENOMEM;
		}
		DSC_PR("%s: dsc maped reg_base =0x%p, size=0x%x\n",
				__func__, dsc_drv->dsc_reg_base, size_io_reg);
	} else {
		dev_err(&pdev->dev, "missing dsc_reg_base memory resource\n");
		dsc_drv->dsc_reg_base = NULL;
		return -1;
	}

	return 0;
}

/* ************************************************************* */
static int dsc_io_open(struct inode *inode, struct file *file)
{
	struct aml_dsc_drv_s *dsc_drv;

	dsc_drv = container_of(inode->i_cdev, struct aml_dsc_drv_s, cdev);
	file->private_data = dsc_drv;

	return 0;
}

static int dsc_io_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations dsc_fops = {
	.owner          = THIS_MODULE,
	.open           = dsc_io_open,
	.release        = dsc_io_release,
};

static int dsc_cdev_add(struct aml_dsc_drv_s *dsc_drv, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!dsc_cdev || !dsc_drv) {
		ret = 1;
		DSC_ERR("%s: dsc_drv is null\n", __func__);
		return -1;
	}

	devno = MKDEV(MAJOR(dsc_cdev->devno), 0);

	cdev_init(&dsc_drv->cdev, &dsc_fops);
	dsc_drv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dsc_drv->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto dsc_cdev_add_failed;
	}

	dsc_drv->dev = device_create(dsc_cdev->class, parent,
				  devno, NULL, "dsc%d", 0);
	if (IS_ERR_OR_NULL(dsc_drv->dev)) {
		ret = 3;
		goto dsc_cdev_add_failed1;
	}

	dev_set_drvdata(dsc_drv->dev, dsc_drv);
	dsc_drv->dev->of_node = parent->of_node;

	DSC_PR("%s OK\n", __func__);
	return 0;

dsc_cdev_add_failed1:
	cdev_del(&dsc_drv->cdev);
dsc_cdev_add_failed:
	DSC_ERR("%s: failed: %d\n", __func__, ret);
	return -1;
}

static void dsc_cdev_remove(struct aml_dsc_drv_s *dsc_drv)
{
	dev_t devno;

	if (!dsc_cdev || !dsc_drv)
		return;

	devno = MKDEV(MAJOR(dsc_cdev->devno), 0);
	device_destroy(dsc_cdev->class, devno);
	cdev_del(&dsc_drv->cdev);
}

/* todo: removed */
static inline void dsc_get_notify_value(struct aml_dsc_drv_s *dsc_drv,
				struct dsc_notifier_data_s *vdata)
{
	if (dsc_drv && vdata) {
		if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_PIC_W_H)) {
			dsc_drv->pps_data.pic_width = vdata->pic_width;
			dsc_drv->pps_data.pic_height = vdata->pic_height;
		}

		if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_COLOR_FORMAT))
			dsc_drv->color_format = vdata->color_format;
		if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_VIDEO_FPS))
			dsc_drv->fps = vdata->fps;
		if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_BITS_PER_COMPONENT))
			dsc_drv->pps_data.bits_per_component = vdata->bits_per_component;
	}
}

void hdmitx_get_dsc_data(struct dsc_offer_tx_data *dsc_data)
{
	struct aml_dsc_drv_s *dsc_drv = dsc_drv_get();

	if (!dsc_drv || !dsc_drv->data || dsc_drv->data->chip_type != DSC_CHIP_S5 || !dsc_data) {
		DSC_ERR("%s not get dsc data\n", __func__);
		return;
	}
	memcpy(&dsc_data->pps_data, &dsc_drv->pps_data, sizeof(dsc_drv->pps_data));
	dsc_data->dsc_mode = dsc_drv->dsc_mode;
	dsc_data->enc0_clk = dsc_drv->enc0_clk;
	dsc_data->cts_hdmi_tx_pixel_clk = dsc_drv->cts_hdmi_tx_pixel_clk;
}
EXPORT_SYMBOL(hdmitx_get_dsc_data);

static void dsc_get_notifier_data(struct aml_dsc_drv_s *dsc_drv,
				 struct dsc_notifier_data_s *notifier_data)
{
	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_PIC_W_H)) {
		dsc_drv->pps_data.pic_width = notifier_data->pic_width;
		dsc_drv->pps_data.pic_height = notifier_data->pic_height;
	}

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_VIDEO_FPS))
		dsc_drv->fps = notifier_data->fps;

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_BITS_PER_COMPONENT))
		dsc_drv->pps_data.bits_per_component = notifier_data->bits_per_component;

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_COLOR_FORMAT))
		dsc_drv->color_format = notifier_data->color_format;
	DSC_PR("hdmitx notify input: %dx%dp%dhz cs:%d,cd:%d\n",
		dsc_drv->pps_data.pic_width,
		dsc_drv->pps_data.pic_height,
		dsc_drv->fps / 1000,
		dsc_drv->color_format,
		dsc_drv->pps_data.bits_per_component);
}

int aml_set_dsc_input_param(struct dsc_notifier_data_s *notifier_data)
{
	struct aml_dsc_drv_s *dsc_drv = dsc_drv_get();

	if (!dsc_drv || !dsc_drv->data || dsc_drv->data->chip_type != DSC_CHIP_S5)
		return -1;

	if (!notifier_data) {
		DSC_ERR("%s: notifier_data is null\n", __func__);
		return -1;
	}

	dsc_get_notifier_data(dsc_drv, notifier_data);
	calculate_dsc_enc_data(dsc_drv);

	return 0;
}
EXPORT_SYMBOL(aml_set_dsc_input_param);

void aml_dsc_enable(bool dsc_en)
{
	struct aml_dsc_drv_s *dsc_drv = dsc_drv_get();

	if (!dsc_drv || !dsc_drv->data || dsc_drv->data->chip_type != DSC_CHIP_S5)
		return;

	if (!dsc_en) {
		set_dsc_en(0);
	} else {
		/* if (dsc_dbg) */
			/* return; */
		dsc_config_register(dsc_drv);
		set_dsc_en(1);
	}
}
EXPORT_SYMBOL(aml_dsc_enable);

void dsc_enc_rst(void)
{
	struct aml_dsc_drv_s *dsc_drv = dsc_drv_get();

	if (!dsc_drv || !dsc_drv->data || dsc_drv->data->chip_type != DSC_CHIP_S5)
		return;

	if (IS_ERR(dsc_drv->rst_control)) {
		DSC_ERR("%s can't get device\n", __func__);
		return;
	}
	reset_control_reset(dsc_drv->rst_control);
}

/* todo: removed */
static int aml_dsc_switch_notify_callback(struct notifier_block *block,
					  unsigned long event, void *para)
{
	struct aml_dsc_drv_s *dsc_drv = dsc_drv_get();
	struct dsc_notifier_data_s *vdata;

	if (!dsc_drv) {
		DSC_ERR("%s: dsc_drv is null\n", __func__);
		return -1;
	}

	switch (event) {
	case DSC_EVENT_ON_MODE:
		if (!para)
			break;
		vdata = (struct dsc_notifier_data_s *)para;
		memset(&dsc_drv->pps_data, 0, sizeof(dsc_drv->pps_data));
		dsc_get_notify_value(dsc_drv, vdata);
		calculate_dsc_enc_data(dsc_drv);
		dsc_config_register(dsc_drv);
		set_dsc_en(1);
		break;
	case DSC_EVENT_OFF_MODE:
		set_dsc_en(0);
		break;
	default:
		break;
	}

	return 0;
}

/* todo: removed */
static struct notifier_block aml_dsc_switch_notifier = {
	.notifier_call = aml_dsc_switch_notify_callback,
};

static int dsc_global_init_once(void)
{
	int ret;

	dsc_cdev = kzalloc(sizeof(*dsc_cdev), GFP_KERNEL);
	if (!dsc_cdev)
		return -1;

	ret = alloc_chrdev_region(&dsc_cdev->devno, 0,
				  1, DSC_CDEV_NAME);
	if (ret) {
		ret = 1;
		goto dsc_global_init_once_err;
	}

	dsc_cdev->class = class_create(THIS_MODULE, "aml_dsc");
	if (IS_ERR_OR_NULL(dsc_cdev->class)) {
		ret = 2;
		goto dsc_global_init_once_err_1;
	}

	aml_dsc_atomic_notifier_register(&aml_dsc_switch_notifier);

	return 0;

dsc_global_init_once_err_1:
	unregister_chrdev_region(dsc_cdev->devno, 1);
dsc_global_init_once_err:
	kfree(dsc_cdev);
	dsc_cdev = NULL;
	DSC_ERR("%s: failed: %d\n", __func__, ret);
	return -1;
}

static void dsc_global_remove_once(void)
{
	if (!dsc_cdev)
		return;

	aml_dsc_atomic_notifier_unregister(&aml_dsc_switch_notifier);

	class_destroy(dsc_cdev->class);
	unregister_chrdev_region(dsc_cdev->devno, 0);
	kfree(dsc_cdev);
	dsc_cdev = NULL;
}

#ifdef CONFIG_OF

static struct dsc_data_s dsc_data_s5 = {
	.chip_type = DSC_CHIP_S5,
	.chip_name = "s5",
};

static const struct of_device_id dsc_dt_match_table[] = {
	{
		.compatible = "amlogic, dsc-s5",
		.data = &dsc_data_s5,
	},
	{}
};
#endif

static int dsc_probe(struct platform_device *pdev)
{
	struct aml_dsc_drv_s *dsc_drv = NULL;
	const struct of_device_id *match = NULL;
	struct dsc_data_s *vdata = NULL;

	match = of_match_device(dsc_dt_match_table, &pdev->dev);
	if (!match) {
		DSC_ERR("%s: no match table\n", __func__);
		return -1;
	}
	vdata = (struct dsc_data_s *)match->data;
	if (!vdata) {
		DSC_ERR("no matched chip data, exit!\n");
		return -1;
	}

	dsc_global_init_once();

	dsc_drv = kzalloc(sizeof(*dsc_drv), GFP_KERNEL);
	if (!dsc_drv)
		return -ENOMEM;
	dsc_drv_local = dsc_drv;
	dsc_drv->data = vdata;
	DSC_PR("%s: driver version: %s(%d-%s)\n",
	      __func__, DSC_DRV_VERSION,
	      vdata->chip_type, vdata->chip_name);

	/* set drvdata */
	platform_set_drvdata(pdev, dsc_drv);
	dsc_cdev_add(dsc_drv, &pdev->dev);

	dsc_debug_file_create(dsc_drv);

	dsc_ioremap(pdev, dsc_drv);
	dsc_drv->rst_control = devm_reset_control_get(dsc_drv->dev, "dsc_enc_reset");

	DSC_PR("%s ok, init_state\n", __func__);

	return 0;

	return -1;
}

static int dsc_remove(struct platform_device *pdev)
{
	struct aml_dsc_drv_s *dsc_drv = platform_get_drvdata(pdev);

	if (!dsc_drv)
		return 0;

	if (dsc_drv->dsc_reg_base) {
		devm_iounmap(&pdev->dev, dsc_drv->dsc_reg_base);
		dsc_drv->dsc_reg_base = NULL;
	}

	dsc_debug_file_remove(dsc_drv);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	dsc_cdev_remove(dsc_drv);

	kfree(dsc_drv);
	dsc_drv = NULL;

	dsc_global_remove_once();

	return 0;
}

static int dsc_resume(struct platform_device *pdev)
{
	return 0;
}

static int dsc_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static void dsc_shutdown(struct platform_device *pdev)
{
}

static struct platform_driver dsc_platform_driver = {
	.probe = dsc_probe,
	.remove = dsc_remove,
	.suspend = dsc_suspend,
	.resume = dsc_resume,
	.shutdown = dsc_shutdown,
	.driver = {
		.name = "meson_dsc",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(dsc_dt_match_table),
#endif
	},
};

int __init dsc_enc_init(void)
{
	if (platform_driver_register(&dsc_platform_driver)) {
		DSC_ERR("failed to register dsc driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit dsc_exit(void)
{
	platform_driver_unregister(&dsc_platform_driver);
}
