 /*
  * drivers/amlogic/defendkey/defendkey.c
  *
  * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/cma.h>
#include <linux/io-mapping.h>
#include <linux/sysfs.h>
#include <asm/cacheflush.h>
#include "securekey.h"

#define DEFENDKEY_DEVICE_NAME	"defendkey"
#define DEFENDKEY_CLASS_NAME "defendkey"

struct defendkey_dev_t {
	struct cdev cdev;
	unsigned int flags;
};

static struct defendkey_dev_t *defendkey_devp;
static dev_t defendkey_devno;
static struct device *defendkey_device;
struct platform_device *defendkey_pdev = NULL;

static int defendkey_open(struct inode *inode, struct file *file)
{
	struct defendkey_dev_t *devp;
	devp = container_of(inode->i_cdev, struct defendkey_dev_t, cdev);
	file->private_data = devp;
	return 0;
}
static int defendkey_release(struct inode *inode, struct file *file)
{
	struct defendkey_dev_t *devp;
	devp = file->private_data;
	return 0;
}

static loff_t defendkey_llseek(struct file *filp, loff_t off, int whence)
{
	return 0;
}

static long defendkey_unlocked_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long defendkey_compat_ioctl(struct file *filp,
			      unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = defendkey_unlocked_ioctl(filp, cmd, args);

	return ret;
}
#endif

static ssize_t defendkey_read(struct file *file,
	char __user *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t defendkey_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	int value = -1;
	int ret = -EINVAL;

	struct page cma_pages;
	struct page *pcma;
	unsigned long mem_base_phy;
	unsigned long mem_size;
	void __iomem *mem_base_virt;

	pcma =  &cma_pages;
	pcma = dma_alloc_from_contiguous(
					&defendkey_pdev->dev,
					(32 * SZ_1M) >> PAGE_SHIFT, 0);

	if (!pcma) {
		pr_err("defendkey:%s: CMA alloc fail\n", __func__);
		return -1;
	}
	mem_base_phy = page_to_phys(pcma);
	mem_size = 32 * SZ_1M;

	mem_base_virt = phys_to_virt(mem_base_phy);

	if (!mem_base_phy || !mem_size) {
		pr_err("bad secure check memory!\nmem_base_phy:%lx,size:%lx\n",
			mem_base_phy, mem_size);
		return ret;
	}
	pr_info("defendkey: mem_base_phy:%lx mem_size:%lx mem_base_virt:%p\n",
		mem_base_phy, mem_size, mem_base_virt);

	ret = copy_from_user(mem_base_virt, buf, count);
	if (ret) {
		pr_err("defendkey:copy_from_user fail! ret : %d\n", ret);
		ret =  -EFAULT;
		goto exit;
	}
	pr_info("defendkey: copy_from_user OK! count %Zd\n", count);
	__dma_flush_range(mem_base_virt, mem_base_virt+count);
	pr_info("defendkey: firmware checking...\n");
	ret = aml_sec_boot_check(AML_D_P_UPGRADE_CHECK,
		(unsigned long) mem_base_phy, count, 0);
	if (ret > 0) {
		pr_err("defendkey:write defendkey error,%s:%d: ret %d\n",
			__func__, __LINE__, ret);
		value = -2;
		goto exit;
	}
	if (!ret) {
		pr_info("defendkey: aml_sec_boot_check ok!\n");
		value = 1;
	}
exit:
	dma_release_from_contiguous(&defendkey_pdev->dev,
		pcma, (32 * SZ_1M) >> PAGE_SHIFT);
	return value;
}

static ssize_t version_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	ssize_t n = 0;
	n += sprintf(&buf[n], "version:2.00");
	buf[n] = 0;
	return n;
}
static ssize_t secure_check_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	ssize_t n = 0;
	int ret;
	ret = aml_is_secure_set();
	if (ret < 0)
		n += sprintf(&buf[n], "fail");
	else if (ret == 0)
		n += sprintf(&buf[n], "raw");
	else if (ret > 0)
		n += sprintf(&buf[n], "encrypt");

	return n;
}
static ssize_t secure_verify_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	ssize_t n = 0;
	return n;
}

static const struct file_operations defendkey_fops = {
	.owner      = THIS_MODULE,
	.llseek     = defendkey_llseek,
	.open       = defendkey_open,
	.release    = defendkey_release,
	.read       = defendkey_read,
	.write      = defendkey_write,
	.unlocked_ioctl      = defendkey_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = defendkey_compat_ioctl,
#endif
};

static struct class_attribute defendkey_class_attrs[] = {
	__ATTR_RO(version),
	__ATTR_RO(secure_check),
	__ATTR_RO(secure_verify),
	__ATTR_NULL
};
static struct class defendkey_class = {
	.name = DEFENDKEY_CLASS_NAME,
	.class_attrs = defendkey_class_attrs,
};

static int aml_defendkey_probe(struct platform_device *pdev)
{
	int r;
	int ret =  -1;

	struct device *devp;

	defendkey_pdev = pdev;
	ret = alloc_chrdev_region(&defendkey_devno, 0, 1,
		DEFENDKEY_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "defendkey: failed to allocate major number\n ");
		ret = -ENODEV;
		goto out;
	}
	dev_info(&pdev->dev, "defendkey_devno:%x\n", defendkey_devno);
	ret = class_register(&defendkey_class);
	if (ret) {
		dev_err(&pdev->dev, "defendkey: failed to register class\n ");
		goto error1;
	}
	defendkey_devp = kzalloc(sizeof(struct defendkey_dev_t), GFP_KERNEL);
	if (!defendkey_devp) {
		dev_err(&pdev->dev, "defendkey: failed to allocate memory\n ");
		ret = -ENOMEM;
		goto error2;
	}
	/* connect the file operations with cdev */
	cdev_init(&defendkey_devp->cdev, &defendkey_fops);
	defendkey_devp->cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&defendkey_devp->cdev, defendkey_devno, 1);
	if (ret) {
		dev_err(&pdev->dev, "defendkey: failed to add device\n");
		goto error3;
	}
	devp = device_create(&defendkey_class, NULL, defendkey_devno,
		NULL, DEFENDKEY_DEVICE_NAME);
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "defendkey: failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}

	defendkey_device = devp;

	r = of_reserved_mem_device_init(&pdev->dev);
	if (r) {
		dev_err(&pdev->dev, "of_reserved_mem_device_init fail!\n");
		goto error4;
	}

	dev_info(&pdev->dev, "defendkey: device %s created ok\n",
			DEFENDKEY_DEVICE_NAME);
	return 0;

error4:
	cdev_del(&defendkey_devp->cdev);
error3:
	kfree(defendkey_devp);
error2:
	class_unregister(&defendkey_class);
error1:
	unregister_chrdev_region(defendkey_devno, 1);
out:
	return ret;
}

static int aml_defendkey_remove(struct platform_device *pdev)
{
	/* if (pdev->dev.of_node) { */
	/* unifykey_dt_release(pdev); */
	/* } */
	unregister_chrdev_region(defendkey_devno, 1);
	device_destroy(&defendkey_class, defendkey_devno);
	/* device_destroy(&defend_class, defendkey_devno); */
	cdev_del(&defendkey_devp->cdev);
	kfree(defendkey_devp);
	class_unregister(&defendkey_class);
	return 0;
}

static const struct of_device_id meson_defendkey_dt_match[] = {
	{	.compatible = "amlogic, defendkey",
	},
	{},
};

static struct platform_driver aml_defendkey_driver = {
	.probe = aml_defendkey_probe,
	.remove = aml_defendkey_remove,
	.driver = {
		.name = DEFENDKEY_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = meson_defendkey_dt_match,
	},
};

static int __init defendkey_init(void)
{
	int ret = -1;

	ret = platform_driver_register(&aml_defendkey_driver);

	if (ret != 0) {
		pr_err("defendkey:failed to register driver, error %d\n", ret);
		return -ENODEV;
	}
	pr_info("defendkey: driver init\n");

	return ret;
}
static void __exit defendkey_exit(void)
{
	platform_driver_unregister(&aml_defendkey_driver);
}

module_init(defendkey_init);
module_exit(defendkey_exit);

MODULE_DESCRIPTION("AMLOGIC defendkey driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("benlong zhou <benlong.zhou@amlogic.com>");



