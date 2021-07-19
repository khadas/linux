// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include "efuse.h"
#include <linux/amlogic/efuse.h>
#include <linux/io.h>
#include "efuse_burn.h"

#define EFUSE_BURN_DEVICE_NAME   "efuse_burn"
#define EFUSE_BURN_CLASS_NAME    "efuse_burn"

static struct aml_efuse_burn_dev *pefuse_burn_dev;

static int efuse_burn_open(struct inode *inode, struct file *file)
{
	struct aml_efuse_burn_dev *devp;

	devp = container_of(inode->i_cdev, struct aml_efuse_burn_dev, cdev);
	file->private_data = devp;

	//printk(KERN_NOTICE "%s:%d\n", __func__, __LINE__);
	pr_notice("%s:%d\n", __func__, __LINE__);

	return 0;
}

static int efuse_burn_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static loff_t efuse_burn_llseek(struct file *filp, loff_t off, int whence)
{
	return 0;
}

static long efuse_burn_unlocked_ioctl(struct file *file,
				 unsigned int cmd, unsigned long arg)
{
	long ret = -ENOTTY;
	void __user *argp = (void __user *)arg;
	struct efuse_burn_info info;

	pr_notice("%s:%d\n", __func__, __LINE__);
	switch (cmd) {
	case EFUSE_BURN_CHECK_KEY:
		if (copy_from_user((void *)&info, argp, sizeof(info))) {
			pr_err("%s: copy_from_user fail\n", __func__);
			return -EFAULT;
		}
		if (efuse_burn_lockable_is_cfg(info.itemname) == 0) {
			info.status = efuse_burn_check_burned(info.itemname);
		} else {
			pr_err("%s: efuse_burn check item not cfg\n", __func__);
			return -EFAULT;
		}
		if (copy_to_user(argp, &info, sizeof(info))) {
			pr_err("%s: copy_to_user fail\n", __func__);
			return -EFAULT;
		}
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long efuse_burn_compat_ioctl(struct file *filp,
			       unsigned int cmd, unsigned long args)
{
	long ret;

	args = (unsigned long)compat_ptr(args);
	ret = efuse_burn_unlocked_ioctl(filp, cmd, args);

	return ret;
}
#endif

static ssize_t efuse_burn_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	pr_notice("%s:%d\n", __func__, __LINE__);
	return 0;
}

static ssize_t efuse_burn_write(struct file *file,
			   const char __user *buf, size_t count, loff_t *ppos)
{
	struct aml_efuse_burn_dev *devp;
	ssize_t ret;
	char *op = NULL;

	devp = file->private_data;
	//pr_notice("%s:%d, sizeof(loff_t):%d\n",
	//	__func__, __LINE__, sizeof(loff_t));
	//pr_notice("pos=%lld,count=%d\n", *ppos, count);
	if (count != devp->efuse_pattern_size) {
		ret = -EINVAL;
		pr_err("efuse burn: bad pattern size, only support size %d!\n",
		       devp->efuse_pattern_size);
		goto exit;
	}
	op = kzalloc(sizeof(char) * count, GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse burn: failed to allocate memory!\n");
		goto exit;
	}

	memset(op, 0, count);
	if (copy_from_user(op, buf, count)) {
		pr_err("%s: copy_from_user fail\n", __func__);
		kfree(op);
		ret = -EFAULT;
		goto exit;
	}

	ret = efuse_amlogic_set(op, count);
	kfree(op);

	if (ret) {
		pr_err("efuse burn: pattern programming fail! ret: %d\n",
		       (unsigned int)ret);
		ret = -EINVAL;
		goto exit;
	}

	pr_info("efuse burn: pattern programming success!\n");

	ret = count;

exit:
	return ret;
}

static const struct file_operations efuse_burn_fops = {
	.owner      = THIS_MODULE,
	.llseek     = efuse_burn_llseek,
	.open       = efuse_burn_open,
	.release    = efuse_burn_release,
	.read       = efuse_burn_read,
	.write      = efuse_burn_write,
	.unlocked_ioctl      = efuse_burn_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = efuse_burn_compat_ioctl,
#endif
};

static ssize_t version_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n = sprintf(buf, "ver1.0");
	return n;
}

static ssize_t version_store(struct class *cla,
			      struct class_attribute *attr,
			      const char *buf, size_t count)
{
	pr_notice("%s:%d,buf=0x%x,count=%d\n",
		__func__, __LINE__, (int)buf, count);
	return count;
}

static EFUSE_CLASS_ATTR(version);

static struct attribute *efuse_burn_class_attrs[] = {
	&class_attr_version.attr,
	NULL,
};
ATTRIBUTE_GROUPS(efuse_burn_class);

static int efuse_burn_probe(struct platform_device *pdev)
{
	int ret;
	struct device *devp;
	struct aml_efuse_burn_dev *efuse_burn_dev;
	struct device_node *np = pdev->dev.of_node;

	efuse_burn_dev = devm_kzalloc(&pdev->dev, sizeof(*efuse_burn_dev),
						GFP_KERNEL);
	if (!efuse_burn_dev) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "failed to alloc enough mem for efuse_dev\n");
		goto out;
	}

	efuse_burn_dev->pdev = pdev;
	platform_set_drvdata(pdev, efuse_burn_dev);

	of_node_get(np);

	ret = of_property_read_u32(np, "efuse_pattern_size",
				   &efuse_burn_dev->efuse_pattern_size);
	if (ret) {
		pr_err("can't get efuse_pattern_size, please configurate size\n");
		goto error1;
	}

	ret = alloc_chrdev_region(&efuse_burn_dev->devno, 0, 1,
			EFUSE_BURN_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "fail to allocate major number\n ");
		goto error1;
	}

	efuse_burn_dev->cls.name = EFUSE_BURN_CLASS_NAME;
	efuse_burn_dev->cls.owner = THIS_MODULE;
	efuse_burn_dev->cls.class_groups = efuse_burn_class_groups;
	ret = class_register(&efuse_burn_dev->cls);
	if (ret)
		goto error2;

	cdev_init(&efuse_burn_dev->cdev, &efuse_burn_fops);
	efuse_burn_dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&efuse_burn_dev->cdev, efuse_burn_dev->devno, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to add device\n");
		goto error3;
	}

	devp = device_create(&efuse_burn_dev->cls, NULL,
			 efuse_burn_dev->devno, efuse_burn_dev, "efuse_burn");
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}
	pefuse_burn_dev = efuse_burn_dev;

	dev_info(&pdev->dev, "device %s created OK\n", EFUSE_BURN_DEVICE_NAME);

	return 0;

error4:
	cdev_del(&efuse_burn_dev->cdev);
error3:
	class_unregister(&efuse_burn_dev->cls);
error2:
	unregister_chrdev_region(efuse_burn_dev->devno, 1);
error1:
	devm_kfree(&pdev->dev, efuse_burn_dev);
out:
	return ret;
}

static int efuse_burn_remove(struct platform_device *pdev)
{
	struct aml_efuse_burn_dev *efuse_burn_dev;

	efuse_burn_dev = platform_get_drvdata(pdev);
	unregister_chrdev_region(efuse_burn_dev->devno, 1);
	device_destroy(&efuse_burn_dev->cls, efuse_burn_dev->devno);
	cdev_del(&efuse_burn_dev->cdev);
	class_unregister(&efuse_burn_dev->cls);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, efuse_burn_dev);
	return 0;
}

static const struct of_device_id efuse_burn_dt_match[] = {
	{	.compatible = "amlogic, efuseburn",
	},
	{},
};

static struct platform_driver efuse_burn_driver = {
	.probe = efuse_burn_probe,
	.remove = efuse_burn_remove,
	.driver = {
		.name = EFUSE_BURN_DEVICE_NAME,
		.of_match_table = efuse_burn_dt_match,
	.owner = THIS_MODULE,
	},
};

int __init aml_efuse_burn_init(void)
{
	return platform_driver_register(&efuse_burn_driver);
}

void aml_efuse_burn_exit(void)
{
	platform_driver_unregister(&efuse_burn_driver);
}

