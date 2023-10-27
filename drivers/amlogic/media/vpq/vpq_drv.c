// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpq_drv.h"
#include "vpq_printk.h"
#include "vpq_debug.h"
#include "vpq_ioctl.h"
#include "vpq_vfm.h"
#include "vpq_table_logic.h"

struct vpq_dev_s *vpq_dev;

struct vpq_dev_s *get_vpq_dev(void)
{
	return vpq_dev;
}

const struct class_attribute vpq_class_attr[] = {
	__ATTR(debug, 0644,
		vpq_debug_cmd_show, vpq_debug_cmd_store),

	__ATTR(log_lev, 0644,
		vpq_log_lev_show, vpq_log_lev_store),

	__ATTR(picture_mode, 0644,
		vpq_picture_mode_item_show, vpq_picture_mode_item_store),

	__ATTR(pq_module, 0644,
		vpq_pq_module_status_show, vpq_pq_module_status_store),

	__ATTR(src_infor, 0644,
		vpq_src_infor_show, vpq_src_infor_store),

	__ATTR(his_avg, 0644,
		vpq_src_hist_avg_show, vpq_src_hist_avg_store),

	__ATTR(debug_other, 0644,
		vpq_debug_other_show, vpq_debug_other_store),

	__ATTR(dump_table, 0644,
		vpq_dump_show, vpq_dump_store),

	__ATTR_NULL,
};

const struct vpq_match_data_s vpq_t5w_match = {
	.chip_id = VPQ_CHIP_T5W,
};

const struct vpq_match_data_s vpq_t3_match = {
	.chip_id = VPQ_CHIP_T3,
};

const struct of_device_id vpq_dts_match[] = {
	{
		.compatible = "amlogic, vpq-t5w",
		.data = &vpq_t5w_match,
	},
	{
		.compatible = "amlogic, vpq-t3",
		.data = &vpq_t3_match,
	},
	{},
};

static int vpq_open(struct inode *inode, struct file *file)
{
	struct vpq_dev_s *vpq_devp;

	VPQ_PR_DRV("%s\n", __func__);

	vpq_devp = container_of(inode->i_cdev, struct vpq_dev_s, vpq_cdev);
	file->private_data = vpq_devp;
	return 0;
}

static int vpq_release(struct inode *inode, struct file *file)
{
	VPQ_PR_DRV("%s\n", __func__);

	file->private_data = NULL;
	return 0;
}

static long vpq_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;

	ret = vpq_ioctl_process(file, cmd, arg);
	return ret;
}

#ifdef CONFIG_COMPAT
static long vpq_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	//VPQ_PR_DRV("%s file:%px\n", __func__, file);

	arg = (unsigned long)compat_ptr(arg);
	ret = vpq_ioctl(file, cmd, arg);
	return ret;
}
#endif

static unsigned int vpq_poll(struct file *file, poll_table *wait)
{
	struct vpq_dev_s *devp = file->private_data;
	unsigned int mask = 0;

	VPQ_PR_DRV("%s start\n", __func__);

	poll_wait(file, &devp->queue, wait);
	VPQ_PR_DRV("%s poll_wait is over\n", __func__);

	mask = (POLLIN | POLLRDNORM);

	return mask;
}

const struct file_operations vpq_fops = {
	.owner = THIS_MODULE,
	.open = vpq_open,
	.release = vpq_release,
	.unlocked_ioctl = vpq_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vpq_compat_ioctl,
#endif
	.poll = vpq_poll,
};

static int vpq_dts_parse(struct vpq_dev_s *vpq_devp)
{
	int ret = 0;
	const struct of_device_id *of_id;
	struct platform_device *pdev = vpq_devp->pdev;

	VPQ_PR_DRV("%s\n", __func__);

	of_id = of_match_device(vpq_dts_match, &pdev->dev);
	if (of_id) {
		VPQ_PR_DRV("%s compatible\n", of_id->compatible);

		vpq_devp->pm_data = (struct vpq_match_data_s *)of_id->data;
	}
	return ret;
}

static void vpq_event_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct vpq_dev_s *devp =
		container_of(dwork, struct vpq_dev_s, event_dwork);

	VPQ_PR_DRV("%s run to here\n", __func__);

	if (!devp) {
		VPQ_PR_DRV("%s dwork error\n", __func__);
		return;
	}
}

static int vpq_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct vpq_dev_s *vpq_devp = NULL;

	VPQ_PR_DRV("%s start\n", __func__);

	vpq_dev = kzalloc(sizeof(*vpq_dev), GFP_KERNEL);
	if (!vpq_dev) {
		VPQ_PR_ERR("%s vpq dev kzalloc error\n", __func__);

		ret = -1;
		goto fail_alloc_dev;
	}
	memset(vpq_dev, 0, sizeof(struct vpq_dev_s));
	vpq_devp = vpq_dev;

	ret = alloc_chrdev_region(&vpq_devp->devno, 0, VPQ_DEVNO, VPQ_NAME);
	if (ret < 0) {
		VPQ_PR_ERR("%s vpq devno alloc failed\n", __func__);

		goto fail_alloc_region;
	}

	vpq_devp->clsp = class_create(THIS_MODULE, VPQ_CLS_NAME);
	if (IS_ERR(vpq_devp->clsp)) {
		VPQ_PR_ERR("%s vpq class create failed\n", __func__);

		ret = -1;
		goto fail_create_class;
	}

	for (i = 0; vpq_class_attr[i].attr.name; i++) {
		ret = class_create_file(vpq_devp->clsp, &vpq_class_attr[i]);
		if (ret < 0) {
			VPQ_PR_ERR("%s vpq class create file failed\n", __func__);

			goto fail_create_class_file;
		}
	}

	/* create cdev and register with sysfs */
	cdev_init(&vpq_devp->vpq_cdev, &vpq_fops);

	vpq_devp->vpq_cdev.owner = THIS_MODULE;
	ret = cdev_add(&vpq_devp->vpq_cdev, vpq_devp->devno, VPQ_DEVNO);
	if (ret < 0) {
		VPQ_PR_ERR("%s vpq add cdev failed\n", __func__);

		goto fail_add_cdev;
	}

	vpq_devp->dev = device_create(vpq_devp->clsp, NULL, vpq_devp->devno, vpq_devp, VPQ_NAME);
	if (!vpq_devp->dev) {
		VPQ_PR_ERR("%s vpq device_create failed\n", __func__);

		ret = -1;
		goto fail_create_dev;
	}

	dev_set_drvdata(vpq_devp->dev, vpq_devp);
	platform_set_drvdata(pdev, vpq_devp);
	vpq_devp->pdev = pdev;

	vpq_dts_parse(vpq_devp);

	/*vdin event*/
	INIT_DELAYED_WORK(&vpq_devp->event_dwork, vpq_event_work);

	/*init queue*/
	init_waitqueue_head(&vpq_devp->queue);

	VPQ_PR_DRV("%s end\n", __func__);

	return ret;

fail_create_dev:
	cdev_del(&vpq_devp->vpq_cdev);
fail_add_cdev:
	for (i = 0; vpq_class_attr[i].attr.name; i++)
		class_remove_file(vpq_devp->clsp, &vpq_class_attr[i]);
fail_create_class_file:
	class_destroy(vpq_devp->clsp);
fail_create_class:
	unregister_chrdev_region(vpq_devp->devno, VPQ_DEVNO);
fail_alloc_region:
	kfree(vpq_dev);
	vpq_dev = NULL;
fail_alloc_dev:
	return ret;
}

static int vpq_remove(struct platform_device *pdev)
{
	int i;
	struct vpq_dev_s *vpq_devp = get_vpq_dev();

	VPQ_PR_DRV("%s\n", __func__);

	cancel_delayed_work(&vpq_devp->event_dwork);

	device_destroy(vpq_devp->clsp, vpq_devp->devno);
	cdev_del(&vpq_devp->vpq_cdev);
	for (i = 0; vpq_class_attr[i].attr.name; i++)
		class_remove_file(vpq_devp->clsp, &vpq_class_attr[i]);
	class_destroy(vpq_devp->clsp);
	unregister_chrdev_region(vpq_devp->devno, VPQ_DEVNO);

	vpq_table_deinit();

	kfree(vpq_dev);
	vpq_dev = NULL;

	return 0;
}

static void vpq_shutdown(struct platform_device *pdev)
{
	int i;
	struct vpq_dev_s *vpq_devp = get_vpq_dev();

	VPQ_PR_DRV("%s\n", __func__);

	device_destroy(vpq_devp->clsp, vpq_devp->devno);
	cdev_del(&vpq_devp->vpq_cdev);
	for (i = 0; vpq_class_attr[i].attr.name; i++)
		class_remove_file(vpq_devp->clsp, &vpq_class_attr[i]);
	class_destroy(vpq_devp->clsp);
	unregister_chrdev_region(vpq_devp->devno, VPQ_DEVNO);
	kfree(vpq_dev);
	vpq_dev = NULL;
}

static struct platform_driver vpq_driver = {
	.probe = vpq_probe,
	.remove = vpq_remove,
	.shutdown = vpq_shutdown,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "aml_vpq",
		.owner = THIS_MODULE,
		.of_match_table = vpq_dts_match,
	},
};

int __init vpq_init(void)
{
	VPQ_PR_DRV("%s module init\n", __func__);

	if (platform_driver_register(&vpq_driver)) {
		VPQ_PR_ERR("%s module init failed\n", __func__);
		return -ENODEV;
	}

	vpq_vfm_init();

	vpq_table_init();

	return 0;
}

void __exit vpq_exit(void)
{
	VPQ_PR_DRV("%s:module exit\n", __func__);

	platform_driver_unregister(&vpq_driver);
}

