// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk.h>

//#include "vlock_proc.h"
#include "frame_lock_proc.h"
#include <linux/amlogic/media/vout/vout_notify.h>

enum vlock_chip_type_e		vlk_chip_type_id;
enum vlock_chip_cls_e		vlk_chip_cls_id;
struct vlock_dev_s			vlock_dev;

static ssize_t framelock_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	return framelock_debug_show(cla, attr, buf);
}

static ssize_t framelock_store(struct class *cla,
				  struct class_attribute *attr,
		const char *buf, size_t count)
{
	return framelock_debug_store(cla, attr, buf, count);
}

static const struct class_attribute vlock_class_attr[] = {
	__ATTR(vlock_dbg, 0644,
		vlock_proc_debug_show,
		vlock_proc_debug_store),
	__ATTR(framelock_dbg, 0644,
		framelock_show,
		framelock_store),
	__ATTR_NULL
};

static const struct vlock_match_data_s vlock_dts_xxx = {
	.chip_id			= vlk_chip_id_other,
	.chip_cls			= VLK_OTHER_CLS,
	.vlk_chip			= vlk_chip_t3,
	.vlk_hw_ver			= vlk_hw_tm2verb,
	.vlk_chip_pll_sel	= vlock_chip_pll_sel_tcon,
	.vlk_support		= true,
	.vlk_new_fsm		= 1,
	.vlk_phlock_en		= true,
	.vlk_ctl_for_frc	= 0,
	.vrr_support_flag	= 0,
};

static const struct vlock_match_data_s vlock_dts_t3 = {
	.chip_id			= vlk_chip_id_t3,
	.chip_cls			= VLK_TV_CHIP,
	.vlk_chip			= vlk_chip_t3,
	.vlk_hw_ver			= vlk_hw_tm2verb,
	.vlk_chip_pll_sel	= vlock_chip_pll_sel_tcon,
	.vlk_support		= true,
	.vlk_new_fsm		= 1,
	.vlk_phlock_en		= true,
	.vlk_ctl_for_frc	= 1,
	.vrr_support_flag	= 1,
};

const struct of_device_id vlock_dts_match[] = {
	{
		.compatible = "amlogic, vlock-t3",
		.data = &vlock_dts_t3,
	},
	{
		.compatible = "amlogic, vlock",
		.data = &vlock_dts_xxx,
	},
};

static struct notifier_block vlock_proc_notifier_nb = {
	.notifier_call	= vlock_proc_notify_callback,
};

static struct notifier_block framelock_vdin_vrr_en_notifier_nb = {
	.notifier_call	= framelock_vrr_nfy_callback,
};

void vlock_vid_lock_clk_config(struct vlock_dev_s *devp, struct device *dev)
{
	int clk_frq = 0;
	//if (!vlock_en)
	//	return;

	vlock_proc_hiu_reg_config(dev);
	/*need set clock tree */
	devp->vlock_clk = devm_clk_get(dev, "cts_vid_lock_clk");
	if (!IS_ERR(devp->vlock_clk)) {
		clk_set_rate(devp->vlock_clk, 24000000);
		if (clk_prepare_enable(devp->vlock_clk) < 0)
			pr_info("vlock vid clk enable fail\n");
		clk_frq = clk_get_rate(devp->vlock_clk);
		pr_info("vlock vid cts_vid_lock_clk:%d\n", clk_frq);
		vlock_hw_clk_ok = 1;
	} else {
		pr_err("vlock vid clk not cfg\n");
		vlock_hw_clk_ok = 0;
	}
}

static void vlock_match_init(struct vlock_match_data_s *pdata)
{
	vlk_chip_type_id = pdata->chip_id;
	vlk_chip_cls_id = pdata->chip_cls;
	pr_info("vlk_chip id: %d, vlk_chip_cls : %d\n", vlk_chip_type_id, vlk_chip_cls_id);
}

void vlock_dts_parse(struct vlock_dev_s *vlock_pdev)
{
	struct device_node *node;
	const struct of_device_id *of_id = NULL;
	struct vlock_match_data_s *matchdata;
	struct platform_device *pdev = vlock_pdev->pdev;

	if (!vlock_pdev) {
		pr_info("vlock_pdev is NULL\n");
		return;
	}

	if (!vlock_pdev->pdev) {
		pr_info("vlock_pdev->pdev is NULL\n");
		return;
	}

	if (!pdev->dev.of_node) {
		pr_info("pdev->dev.of_node is NULL\n");
		return;
	}

	of_id = of_match_device(vlock_dts_match, &pdev->dev);
	if (!of_id) {
		pr_info("of_id is NULL\n");
		return;
	}

	if (of_id) {
		pr_info("%s compatible\n", of_id->compatible);
		matchdata = (struct vlock_match_data_s *)of_id->data;
	} else {
		pr_info("%s unable to get matched device\n", of_id->compatible);
		matchdata = (struct vlock_match_data_s *)&vlock_dts_xxx;
	}
	pr_info("%d %d %d %d %d %d %d %d %d %d\n",
		matchdata->chip_id, matchdata->chip_cls, matchdata->vlk_chip, matchdata->vlk_hw_ver,
		matchdata->vlk_chip_pll_sel, matchdata->vlk_support, matchdata->vlk_new_fsm,
		matchdata->vlk_phlock_en, matchdata->vlk_ctl_for_frc, matchdata->vrr_support_flag);

	node = pdev->dev.of_node;

	//vlock param config
	vlock_proc_dts_config(node);

	vlock_match_init(matchdata);
	vlock_proc_dts_match_init(matchdata);
	vlock_vid_lock_clk_config(vlock_pdev, &pdev->dev);
	vlock_proc_status_init();
}

static int vlock_open(struct inode *inode, struct file *file)
{
	struct vlock_dev_s *vlock_pdev;

	vlock_pdev = container_of(inode->i_cdev, struct vlock_dev_s, vlock_cdev);
	file->private_data = vlock_pdev;

	return 0;
}

static int vlock_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static long vlock_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	return ret;
}

static long vlock_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vlock_ioctl(file, cmd, arg);

	return ret;
}

const struct file_operations vlock_fops = {
	.owner = THIS_MODULE,
	.open = vlock_open,
	.release = vlock_release,
	.unlocked_ioctl = vlock_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vlock_compat_ioctl,
#endif
};

static int vlock_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct vlock_dev_s *vlock_pdev = &vlock_dev;

	memset(vlock_pdev, 0, sizeof(struct vlock_dev_s));
	pr_info("%s:In line:%d", __func__, __LINE__);

	ret = alloc_chrdev_region(&vlock_pdev->devno, 0, VLOCK_DEVNO, VLOCK_NAME);
	if (ret < 0) {
		pr_err("vlock devno alloc fail\n");
		goto fail_alloc_region;
	}
	pr_info("%s:In line:%d", __func__, __LINE__);

	vlock_pdev->cls = class_create(THIS_MODULE, VLOCK_CLS_NAME);
	if (IS_ERR(vlock_pdev->cls)) {
		pr_err("vlock dev class create fail\n");
		ret = -1;
		goto fail_create_class;
	}

	for (i = 0; vlock_class_attr[i].attr.name; i++) {
		ret = class_create_file(vlock_pdev->cls, &vlock_class_attr[i]);
		if (ret < 0) {
			pr_err("vlock class create file fail\n");
			goto fail_create_class_file;
		}
	}

	pr_info("%s:In line:%d", __func__, __LINE__);

	cdev_init(&vlock_pdev->vlock_cdev, &vlock_fops);
	vlock_pdev->vlock_cdev.owner = THIS_MODULE;
	ret = cdev_add(&vlock_pdev->vlock_cdev, vlock_pdev->devno, VLOCK_DEVNO);
	if (ret < 0) {
		pr_info("vlock add cdev fail\n");
		goto fail_add_cdev;
	}
	pr_info("%s:In line:%d", __func__, __LINE__);

	vlock_pdev->dev = device_create(vlock_pdev->cls, NULL,
		vlock_pdev->devno, NULL, VLOCK_NAME);
	if (!vlock_pdev->dev) {
		pr_info("vlock device create fail");
		goto fail_create_dev;
	}
	pr_info("%s:In line:%d", __func__, __LINE__);
	vlock_pdev->pdev = pdev;

	vlock_dts_parse(vlock_pdev);
	framelock_vrr_off_done_init();
	vout_register_client(&vlock_proc_notifier_nb);
	aml_vrr_atomic_notifier_register(&framelock_vdin_vrr_en_notifier_nb);

	pr_info("%s: ok\n", __func__);
	pr_info("%s:In line:%d", __func__, __LINE__);
	return 0;

fail_create_dev:
	pr_info("fail_create_dev.\n");
	cdev_del(&vlock_pdev->vlock_cdev);
	return ret;
fail_add_cdev:
	pr_info("fail_add_cdev.\n");
	for (i = 0; vlock_class_attr[i].attr.name; i++)
		class_remove_file(vlock_pdev->cls, &vlock_class_attr[i]);
	return ret;
fail_create_class_file:
	pr_info("fail_create_class_file.\n");
	class_destroy(vlock_pdev->cls);
	return ret;
fail_create_class:
	pr_info("fail_create_class.\n");
	unregister_chrdev_region(vlock_dev.devno, VLOCK_DEVNO);
	return ret;
fail_alloc_region:
	pr_info("vlock alloc error.\n");
	return ret;
}

static int vlock_drv_remove(struct platform_device *pdev)
{
	vout_unregister_client(&vlock_proc_notifier_nb);
	aml_vrr_atomic_notifier_unregister(&framelock_vdin_vrr_en_notifier_nb);
	return 0;
}

static void vlock_drv_shutdown(struct platform_device *pdev)
{
//	return;
}

static int vlock_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int vlock_drv_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver vlock_driver = {
	.probe = vlock_drv_probe,
	.remove = vlock_drv_remove,
	.shutdown = vlock_drv_shutdown,
	.suspend = vlock_drv_suspend,
	.resume = vlock_drv_resume,
	.driver = {
		.name = "aml_vlock",
		.owner = THIS_MODULE,
		.of_match_table = vlock_dts_match,
	},
};

int __init vlock_drv_init(void)
{
	pr_info("%s:module init\n", __func__);

	if (platform_driver_register(&vlock_driver)) {
		pr_err("vlock:fail to register vlock driver!");
		pr_info("vlock:fail to register vlock driver!");
		return -ENODEV;
	}
	pr_info("%s:module init end\n", __func__);

	return 0;
}

void __exit vlock_drv_exit(void)
{
	pr_info("%s:module exit\n", __func__);

	platform_driver_unregister(&vlock_driver);
}
