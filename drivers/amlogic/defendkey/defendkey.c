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
void __iomem *mem_base_virt;
unsigned long mem_size;
unsigned long random_virt;

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
	ssize_t value = -1;
	int ret = -EINVAL;

	unsigned long mem_base_phy, copy_base, copy_size, random;
	unsigned long option = 0;
	int i;

	mem_size = 1 * SZ_1M;

	mem_base_phy = virt_to_phys(mem_base_virt);

	if (!mem_base_phy || !mem_size) {
		pr_err("bad secure check memory!\nmem_base_phy:%lx,size:%lx\n",
			mem_base_phy, mem_size);
		return ret;
	}
	pr_info("defendkey: mem_base_phy:%lx mem_size:%lx mem_base_virt:%p\n",
		mem_base_phy, mem_size, mem_base_virt);
	random = readl((void *)random_virt);

	for (i = 0; i <= count/mem_size; i++) {
		copy_size = mem_size;
		copy_base = (unsigned long)buf+i*mem_size;
		if ((i+1)*mem_size > count)
			copy_size = count - mem_size*i;
		ret = copy_from_user(mem_base_virt,
			(const void __user *)copy_base, copy_size);
		if (ret) {
			pr_err("defendkey:copy_from_user fail! ret:%d\n", ret);
			ret =  -EFAULT;
			goto exit;
		}
		/*pr_info("defendkey: copy_from_user OK!");
		pr_info("user copy_base: 0x%lx copy_size:0x%lx\n",
			copy_base, copy_size);*/
		__dma_flush_range((const void *)mem_base_virt,
			(const void *)(mem_base_virt+copy_size));
		/*pr_info("defendkey: firmware checking...\n");*/

		if (i == 0) {
			option = 1;
			if (count <= mem_size) {
				/*just transfer data to BL31 one time*/
				option = 1|2|4;
			}
			option |= (random<<32);
		} else if ((i > 0) && (i < (count/mem_size))) {
			option = 2|(random<<32);
			if ((count%mem_size == 0) &&
				(i == (count/mem_size - 1)))
				option = 4|(random<<32);
		} else if (i == (count/mem_size)) {
			if (count%mem_size == 0)
				break;
			else
				option = 4|(random<<32);
		}
		/*pr_info("defendkey:%d: copy_size:0x%lx, option:0x%lx\n",
			__LINE__, copy_size, option);*/
		ret = aml_sec_boot_check(AML_D_P_UPGRADE_CHECK,
			(unsigned long) mem_base_phy, copy_size, option);
		if (ret) {
			value = 0;
			pr_err("defendkey: aml_sec_boot_check error,%s:%d: ret %d\n",
			__func__, __LINE__, ret);
			goto exit;
		}
	}

	if (!ret) {
		pr_info("defendkey: aml_sec_boot_check ok!\n");
		value = 1;
	}
exit:
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
	int ret =  -1;
	u64 val64;
	struct resource *res;

	struct device *devp;

	defendkey_pdev = pdev;
	ret = of_property_read_u64(pdev->dev.of_node, "mem_size", &val64);
	if (ret) {
		dev_err(&pdev->dev, "please config mem_size in dts\n");
		return -1;
	}
	mem_size = val64;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR(res)) {
		dev_err(&pdev->dev, "reg: cannot obtain I/O memory region");
		return PTR_ERR(res);
	}
	random_virt = (unsigned long)devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR((void *)random_virt))
		return PTR_ERR((void *)random_virt);
	mem_base_virt = kmalloc(mem_size, GFP_KERNEL);
	if (!mem_base_virt) {
		dev_err(&pdev->dev, "defendkey: failed to allocate memory\n ");
		return -ENOMEM;
	}
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



