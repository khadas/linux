/*
 * drivers/amlogic/efuse/efuse.c
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
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>

#include <linux/amlogic/secmon.h>
#include "efuse.h"

#define EFUSE_MODULE_NAME   "efuse"
#define EFUSE_DRIVER_NAME	"efuse"
#define EFUSE_DEVICE_NAME   "efuse"
#define EFUSE_CLASS_NAME    "efuse"
#define EFUSE_IS_OPEN           (0x01)

struct efuse_dev_t {
	struct cdev         cdev;
	unsigned int        flags;
};

static struct efuse_dev_t *efuse_devp;
/* static struct class *efuse_clsp; */
static dev_t efuse_devno;

void __iomem *sharemem_input_base;
void __iomem *sharemem_output_base;
unsigned efuse_read_cmd;
unsigned efuse_write_cmd;

static int efuse_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct efuse_dev_t *devp;

	devp = container_of(inode->i_cdev, struct efuse_dev_t, cdev);
	file->private_data = devp;

	return ret;
}

static int efuse_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct efuse_dev_t *devp;
	unsigned long efuse_status = 0;

	devp = file->private_data;
	efuse_status &= ~EFUSE_IS_OPEN;
	return ret;
}

loff_t efuse_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;

	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = EFUSE_BYTES + off;
		break;

	default: /* can't happen */
		return -EINVAL;
	}

	if (newpos < 0)
		return -EINVAL;
	filp->f_pos = newpos;
		return newpos;
}

static long efuse_unlocked_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct efuseinfo_item_t *info;
	switch (cmd) {
	case EFUSE_INFO_GET:
		info = (struct efuseinfo_item_t *)arg;
		if (efuse_getinfo_byID(info->id, info) < 0)
			return  -EFAULT;
		break;

	default:
		return -ENOTTY;
	}
	return 0;
}


static ssize_t efuse_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	int ret;
	int local_count = 0;
	unsigned char *local_buf = kzalloc(sizeof(char)*count, GFP_KERNEL);
	if (!local_buf) {
		pr_info("memory not enough\n");
		return -ENOMEM;
	}

	local_count = efuse_read_item(local_buf, count, ppos);
	if (local_count < 0) {
		ret =  -EFAULT;
		goto error_exit;
	}

	if (copy_to_user((void *)buf, (void *)local_buf, local_count)) {
		ret =  -EFAULT;
		goto error_exit;
	}
	ret = local_count;

error_exit:
	/*if (local_buf)*/
		kfree(local_buf);
	return ret;
}

static ssize_t efuse_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned int  pos = (unsigned int)*ppos;
	int ret, size;
	unsigned char *contents = NULL;

	if (pos >= EFUSE_BYTES)
		return 0;       /* Past EOF */
	if (count > EFUSE_BYTES - pos)
		count = EFUSE_BYTES - pos;
	if (count > EFUSE_BYTES)
		return -EFAULT;

	ret = check_if_efused(pos, count);
	if (ret) {
		pr_info("check if has been efused failed\n");
		if (ret == 1)
			return -EROFS;
		else if (ret < 0)
			return ret;
	}

	contents = kzalloc(sizeof(unsigned char)*EFUSE_BYTES, GFP_KERNEL);
	if (!contents) {
		pr_info("memory not enough\n");
		return -ENOMEM;
	}
	size = sizeof(contents);
	memset(contents, 0, size);
	if (copy_from_user(contents, buf, count)) {
		/*if (contents)*/
			kfree(contents);
		return -EFAULT;
	}

	if (efuse_write_item(contents, count, ppos) < 0) {
		kfree(contents);
		return -EFAULT;
	}

	kfree(contents);
	return count;
}


static const struct file_operations efuse_fops = {
	.owner      = THIS_MODULE,
	.llseek     = efuse_llseek,
	.open       = efuse_open,
	.release    = efuse_release,
	.read       = efuse_read,
	.write      = efuse_write,
	.unlocked_ioctl      = efuse_unlocked_ioctl,
};

/* Sysfs Files */
static ssize_t mac_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	char dec_mac[6] = {0};
	struct efuseinfo_item_t info;

	if (efuse_getinfo_byID(EFUSE_MAC_ID, &info) < 0) {
		pr_err("ID is not found\n");
		return -EFAULT;
	}

	if (efuse_read_item(dec_mac, info.data_len, (loff_t *)&info.offset) < 0)
		return -EFAULT;

	return sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			dec_mac[0], dec_mac[1], dec_mac[2],
			dec_mac[3], dec_mac[4], dec_mac[5]);
}

static ssize_t mac_wifi_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	char dec_mac[6] = {0};
	struct efuseinfo_item_t info;

	if (efuse_getinfo_byID(EFUSE_MAC_WIFI_ID, &info) < 0) {
		pr_err("ID is not found\n");
		return -EFAULT;
	}

	if (efuse_read_item(dec_mac, info.data_len, (loff_t *)&info.offset) < 0)
		return -EFAULT;

	return sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			dec_mac[0], dec_mac[1], dec_mac[2],
			dec_mac[3], dec_mac[4], dec_mac[5]);
}


static ssize_t mac_bt_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	char dec_mac[6] = {0};
	struct efuseinfo_item_t info;

	if (efuse_getinfo_byID(EFUSE_MAC_BT_ID, &info) < 0) {
		pr_info("ID is not found\n");
		return -EFAULT;
	}
	if (efuse_read_item(dec_mac, info.data_len, (loff_t *)&info.offset) < 0)
		return -EFAULT;

	return sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			dec_mac[0], dec_mac[1], dec_mac[2],
			dec_mac[3], dec_mac[4], dec_mac[5]);
}

static int efuse_device_match(struct device *dev, const void *data)
{
	int ret;
	ret = strcmp(dev->kobj.name, (const char *)data);
	return !ret;
}

struct device *efuse_class_to_device(struct class *cla)
{
	struct device		*dev;

	dev = class_find_device(cla, NULL, (void *)cla->name,
				efuse_device_match);
	if (!dev)
		pr_info("%s no matched device found!/n", __func__);
	return dev;
}

static ssize_t userdata_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	char *op;
	bool ret = true;
	int i, size;
	struct efuseinfo_item_t info;
	char tmp[5];

	if (efuse_getinfo_byID(EFUSE_USID_ID, &info) < 0) {
		pr_err("ID is not found\n");
		return -1;
	}

	op = kmalloc(sizeof(char)*info.data_len, GFP_KERNEL);
	if (!op) {
		pr_err("efuse: failed to allocate memory\n");
		ret = -ENOMEM;
		return ret;
	}

	size = sizeof(op);
	memset(op, 0, size);
	if (efuse_read_item(op, info.data_len, (loff_t *)&info.offset) < 0) {
		/*if (op)*/
		kfree(op);
		return -1;
	}

	for (i = 0; i < info.data_len; i++) {
		memset(tmp, 0, 5);
		sprintf(tmp, "%02x:", op[i]);
		strcat(buf, tmp);
	}
	buf[3*info.data_len - 1] = 0; /* delete the last ':' */
	return 3*info.data_len - 1;
}

#ifndef EFUSE_READ_ONLY
static ssize_t userdata_write(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	struct efuseinfo_item_t info;
	int i, size;
	unsigned local_count = count;
	struct efuse_platform_data *data = NULL;
	struct device	*dev = NULL;
	bool ret = true;
	char *op = NULL;
	dev = efuse_class_to_device(cla);
	data = dev->platform_data;

	if (!data) {
		pr_err("%s error!no platform_data!\n", __func__);
		return -1;
	}

	if (local_count > data->count)
		local_count = data->count;
	if (efuse_getinfo_byID(EFUSE_USID_ID, &info) < 0) {
		pr_info("ID is not found\n");
		return -1;
	}

	if (check_if_efused(info.offset, info.data_len)) {
		pr_err("%s error!data_verify failed!\n", __func__);
		return -1;
	}

	op = kzalloc((sizeof(char)*(info.data_len)), GFP_KERNEL);
	if (!op) {
		pr_err("efuse: failed to allocate memory\n");
		ret = -ENOMEM;
		return ret;
	}

	size = sizeof(op);
	memset(op, 0, size);
	for (i = 0; i < local_count; i++)
		op[i] = buf[i];

	if (efuse_write_item(op, info.data_len, (loff_t *)&info.offset) < 0)
		return -1;

/* if (op) */
		kfree(op);
	return local_count;
}
#endif

static struct class_attribute efuse_class_attrs[] = {

	__ATTR_RO(mac),

	__ATTR_RO(mac_wifi),

	__ATTR_RO(mac_bt),

	#ifndef EFUSE_READ_ONLY
	/*make the efuse can not be write through sysfs */
	__ATTR(userdata, S_IRWXU, userdata_show, userdata_write),

	#else
	__ATTR_RO(userdata),

	#endif
	__ATTR_NULL

};

static struct class efuse_class = {

	.name = EFUSE_CLASS_NAME,

	.class_attrs = efuse_class_attrs,

};


static int efuse_probe(struct platform_device *pdev)
{
	int ret;
	struct device *devp;
	struct efuse_platform_data aml_efuse_plat;
	struct device_node *np = pdev->dev.of_node;
	int pos, count, usid_min, usid_max;

	ret = alloc_chrdev_region(&efuse_devno, 0, 1, EFUSE_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "efuse: failed to allocate major number\n");
		ret = -ENODEV;
		goto out;
	}

	ret = class_register(&efuse_class);
	if (ret)
		goto error1;

	efuse_devp = kmalloc(sizeof(struct efuse_dev_t), GFP_KERNEL);
	if (!efuse_devp) {
		dev_err(&pdev->dev, "efuse: failed to allocate memory\n");
		ret = -ENOMEM;
		goto error2;
	}

	/* connect the file operations with cdev */
	cdev_init(&efuse_devp->cdev, &efuse_fops);
	efuse_devp->cdev.owner = THIS_MODULE;
	/* connect the major/minor number to the cdev */
	ret = cdev_add(&efuse_devp->cdev, efuse_devno, 1);
	if (ret) {
		dev_err(&pdev->dev, "efuse: failed to add device\n");
		goto error3;
	}

	devp = device_create(&efuse_class, NULL, efuse_devno, NULL, "efuse");
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "efuse: failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}
	dev_dbg(&pdev->dev, "device %s created\n", EFUSE_DEVICE_NAME);

	if (pdev->dev.of_node) {
		of_node_get(np);

		ret = of_property_read_u32(np, "plat-pos", &pos);
		if (ret) {
			dev_err(&pdev->dev, "please config plat-pos item\n");
			return -1;
		}
		ret = of_property_read_u32(np, "plat-count", &count);
		if (ret) {
			dev_err(&pdev->dev, "please config plat-count item\n");
			return -1;
		}
		ret = of_property_read_u32(np, "usid-min", &usid_min);
		if (ret) {
			dev_err(&pdev->dev, "please config usid-min item\n");
			return -1;
		}
		ret = of_property_read_u32(np, "usid-max", &usid_max);
		if (ret) {
			dev_err(&pdev->dev, "please config usid-max item\n");
			return -1;
		}

		ret = of_property_read_u32(np, "read_cmd", &efuse_read_cmd);
		if (ret) {
			dev_err(&pdev->dev, "please config read_cmd item\n");
			return -1;
		}

		ret = of_property_read_u32(np, "write_cmd", &efuse_write_cmd);
		if (ret) {
			dev_err(&pdev->dev, "please config write_cmd item\n");
			return -1;
		}
		/* todo reserved for user id <usid-min ~ usid max> */
	}
	devp->platform_data = &aml_efuse_plat;

	sharemem_input_base = get_secmon_sharemem_input_base();
	sharemem_output_base = get_secmon_sharemem_output_base();
	dev_info(&pdev->dev, "probe ok!\n");
	return 0;

error4:
	cdev_del(&efuse_devp->cdev);
error3:
	kfree(efuse_devp);
error2:
	/* class_destroy(efuse_clsp); */
	class_unregister(&efuse_class);
error1:
	unregister_chrdev_region(efuse_devno, 1);
out:
	return ret;
}

static int efuse_remove(struct platform_device *pdev)
{
	unregister_chrdev_region(efuse_devno, 1);
	/* device_destroy(efuse_clsp, efuse_devno); */
	device_destroy(&efuse_class, efuse_devno);
	cdev_del(&efuse_devp->cdev);
	kfree(efuse_devp);
	/* class_destroy(efuse_clsp); */
	class_unregister(&efuse_class);
	return 0;
}

static const struct of_device_id amlogic_efuse_dt_match[] = {
	{	.compatible = "amlogic, efuse",
	},
	{},
};

static struct platform_driver efuse_driver = {
	.probe = efuse_probe,
	.remove = efuse_remove,
	.driver = {
		.name = EFUSE_DEVICE_NAME,
		.of_match_table = amlogic_efuse_dt_match,
	.owner = THIS_MODULE,
	},
};

static int __init efuse_init(void)
{
	int ret = -1;
	ret = platform_driver_register(&efuse_driver);
	if (ret != 0) {
		pr_err("failed to register efuse driver, error %d\n", ret);
		return -ENODEV;
	}

	return ret;
}

static void __exit efuse_exit(void)
{
	platform_driver_unregister(&efuse_driver);
}

module_init(efuse_init);
module_exit(efuse_exit);

MODULE_DESCRIPTION("AMLOGIC eFuse driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bo Yang <bo.yang@amlogic.com>");















