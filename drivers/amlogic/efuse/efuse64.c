/*
 * drivers/amlogic/efuse/efuse64.c
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
#include <linux/reset.h>

#include <linux/amlogic/secmon.h>
#include "efuse.h"
#ifdef CONFIG_ARM64
#include <linux/amlogic/efuse-amlogic.h>
#endif

#define EFUSE_MODULE_NAME   "efuse"
#define EFUSE_DRIVER_NAME	"efuse"
#define EFUSE_DEVICE_NAME   "efuse"
#define EFUSE_CLASS_NAME    "efuse"
#define EFUSE_IS_OPEN           (0x01)

struct efusekey_info *efusekey_infos = NULL;
int efusekeynum =  -1;

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
unsigned efuse_get_max_cmd;

#define  DEFINE_EFUEKEY_SHOW_ATTR(keyname)	\
	static ssize_t  show_##keyname(struct class *cla, \
					  struct class_attribute *attr,	\
						char *buf)	\
	{	\
		ssize_t ret;	\
		int i = 0; \
		\
		ret = efuse_user_attr_show(#keyname, buf); \
		pr_info("efuse read data\n"); \
		while (i < ret) { \
			pr_info("%02x%s", buf[i], \
				((i+1)%16 == 0)?"\n":":"); \
			i++; \
		} \
		pr_info("\n"); \
		return ret; \
	}
DEFINE_EFUEKEY_SHOW_ATTR(mac)
DEFINE_EFUEKEY_SHOW_ATTR(mac_bt)
DEFINE_EFUEKEY_SHOW_ATTR(mac_wifi)
DEFINE_EFUEKEY_SHOW_ATTR(usid)


#define  DEFINE_EFUEKEY_STORE_ATTR(keyname)	\
	static ssize_t  store_##keyname(struct class *cla, \
					  struct class_attribute *attr,	\
						const char *buf,	\
						size_t count)	\
	{	\
		ssize_t ret;	\
		\
		ret = efuse_user_attr_store(#keyname, buf, count); \
		return ret; \
	}
DEFINE_EFUEKEY_STORE_ATTR(mac)
DEFINE_EFUEKEY_STORE_ATTR(mac_bt)
DEFINE_EFUEKEY_STORE_ATTR(mac_wifi)
DEFINE_EFUEKEY_STORE_ATTR(usid)

int efuse_getinfo(char *item, struct efusekey_info *info)
{
	int i;
	int ret = -1;

	for (i = 0; i < efusekeynum; i++) {
		if (strcmp(efusekey_infos[i].keyname, item) == 0) {
			strcpy(info->keyname, efusekey_infos[i].keyname);
			info->offset = efusekey_infos[i].offset;
			info->size = efusekey_infos[i].size;
			ret = 0;
			break;
		}
	}
	if (ret < 0)
		pr_err("%s item not found.\n", item);
	return ret;
}

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
	struct efusekey_info *info;

	switch (cmd) {
	case EFUSE_INFO_GET:
		info = (struct efusekey_info *)arg;
		if (efuse_getinfo(info->keyname, info) < 0) {
			pr_err("%s if not found\n", info->keyname);
			return -EFAULT;
		}
		break;

	default:
		return -ENOTTY;
	}
	return 0;
}


#ifdef CONFIG_COMPAT
static long efuse_compat_ioctl(struct file *filp,
			      unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = efuse_unlocked_ioctl(filp, cmd, args);

	return ret;
}
#endif

static ssize_t efuse_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	int ret;
	int local_count = 0;
	unsigned char *local_buf;

	if (count > efuse_get_max()) {
		pr_err("%s: can not read %Zd bytes from efuse\n",
			   __func__, count);
		return -EINVAL;
	}

	local_buf = kzalloc(sizeof(char)*count, GFP_KERNEL);
	if (!local_buf) {
		pr_info("memory not enough\n");
		return -ENOMEM;
	}

	local_count = efuse_read_usr(local_buf, count, ppos);
	if (local_count < 0) {
		ret =  -EFAULT;
		pr_err("%s: read user error!!--%d\n", __func__, __LINE__);
		goto error_exit;
	}

	if (copy_to_user((void *)buf, (void *)local_buf, local_count)) {
		ret =  -EFAULT;
		pr_err("%s: copy_to_user error!!--%d\n", __func__, __LINE__);
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

	contents = kzalloc(sizeof(unsigned char)*EFUSE_BYTES, GFP_KERNEL);
	if (!contents) {
		pr_info("memory not enough\n");
		return -ENOMEM;
	}
	size = sizeof(contents);
	memset(contents, 0, size);
	if (copy_from_user(contents, buf, count)) {
		pr_err("efuse_write copy_from_user ERROR!!\n");
			kfree(contents);
		return -EFAULT;
	}

	ret = efuse_write_usr(contents, count, ppos);
	if (ret < 0) {
		pr_err("write user area %Zd bytes data, fail!!!\n", count);
		kfree(contents);
		return -EFAULT;
	}

	pr_info("user area %d bytes data, write OK\n", ret);
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
#ifdef CONFIG_COMPAT
	.compat_ioctl   = efuse_compat_ioctl,
#endif
};


ssize_t efuse_user_attr_store(char *name, const char *buf, size_t count)
{
#ifndef EFUSE_READ_ONLY
	char *local_buf;
	ssize_t ret;
	int i;
	const char *c, *s;
	struct efusekey_info info;
	unsigned uint_val;

	if (efuse_getinfo(name, &info) < 0) {
		pr_err("%s is not found\n", name);
		return -EFAULT;
	}

	local_buf = kzalloc(sizeof(char)*(count), GFP_KERNEL);

	memcpy(local_buf, buf, strlen(buf));

	c = ":";
	s = local_buf;
	if (NULL != strstr(s, c)) {
			for (i = 0; i < info.size; i++) {
				uint_val = 0;
				ret = sscanf(s, "%x", &uint_val);
				if (ret < 0) {
					pr_err("ERROR: efuse get user data fail!\n");
					goto error_exit;
				} else
					local_buf[i] = uint_val;
				s += 2;
				if (!strncmp(s, c, 1))
					s++;
				pr_debug("local_buf[%d]: 0x%x\n",
					i, local_buf[i]);
			}
	}

	ret = efuse_write_usr(local_buf, info.size, (loff_t *)&(info.offset));
	if (ret == -1) {
		pr_err("ERROR: efuse write user data fail!\n");
		goto error_exit;
	}
	if (ret != info.size)
		pr_err("ERROR: write %zd byte(s) not %d byte(s) data\n",
			ret, info.size);

	pr_info("efuse write %zd data OK\n", ret);

error_exit:
	kfree(local_buf);
	return count;
#else
	pr_err("no permission to write!!\n");
	return -1;
#endif
}

ssize_t efuse_user_attr_show(char *name, char *buf)
{
	char *local_buf;
	ssize_t ret;
	int i;
	struct efusekey_info info;

	if (efuse_getinfo(name, &info) < 0) {
		pr_err("%s is not found\n", name);
		return -EFAULT;
	}

	local_buf = kzalloc(sizeof(char)*(info.size), GFP_KERNEL);
	memset(local_buf, 0, info.size);

	ret = efuse_read_usr(local_buf, info.size, (loff_t *)&(info.offset));
	if (ret == -1) {
		pr_err("ERROR: efuse read user data fail!\n");
		goto error_exit;
	}
	if (ret != info.size)
		pr_err("ERROR: read %zd byte(s) not %d byte(s) data\n",
			ret, info.size);

	for (i = 0; i < info.size; i++)
		buf[i] = local_buf[i];

error_exit:
	kfree(local_buf);
	return ret;
}

int efuse_get_mac(char *addr)
{
	char buf[6];
	int ret;

	/* Copy H/W MAC address from eFuse programmed on production.
	 * If missing or an error, C0:FF:EE:00:01:9F will be used.
	 */
	ret = efuse_user_attr_show("mac", buf);
	if (ret < 0) {
		pr_err("%s: failed to read MAC\n", __func__);
	} else {
		printk("%s: %pM\n", __func__, buf);
		memcpy(addr, buf, 6);
	}

	return ret;
}
EXPORT_SYMBOL(efuse_get_mac);

static ssize_t userdata_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	char *op;
	int ret;
	int i;
	loff_t offset;
	unsigned int max_size;

	max_size = efuse_get_max();

	op = kmalloc(sizeof(char)*max_size, GFP_KERNEL);
	if (!op) {
		pr_err("efuse: failed to allocate memory\n");
		ret = -ENOMEM;
		return ret;
	}

	memset(op, 0, max_size);
	offset = 0;

	ret = efuse_read_usr(op, max_size, &offset);
	if (ret < 0) {
		pr_err("%s: efuse read user data error!!\n", __func__);
		kfree(op);
		return -1;
	}

	pr_debug("user area %d bytes data,", max_size);
	pr_info("%d bytes data read:\n", ret);
	for (i = 0; i < ret; i++) {
		if ((i != 0) && (i%16 == 0))
			pr_info("\n");
		if (i%16 == 0)
			pr_info("0x%x: ", i);
		pr_info("%02x ", op[i]);
	}

	kfree(op);
	return ret;
}

#ifndef EFUSE_READ_ONLY
static ssize_t userdata_write(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	int i;
	int ret;
	loff_t offset;
	char *op = NULL;
	unsigned int max_size;

	max_size = efuse_get_max();
	op = kzalloc((sizeof(char)*max_size), GFP_KERNEL);
	if (!op) {
		pr_err("efuse: failed to allocate memory\n");
		ret = -ENOMEM;
		return ret;
	}

	memset(op, 0, max_size);
	for (i = 0; i < (count-1); i++)
		op[i] = buf[i];

	offset = 0;
	pr_debug("user area %d bytes data, write...\n", max_size);
	ret = efuse_write_usr(op, count, &offset);
	if (ret < 0) {
		kfree(op);
		pr_err("write user area %d bytes data, fail!!!\n", max_size);
		return -1;
	}
	pr_info("user area %d bytes data, write OK\n", ret);
	kfree(op);
	return count;
}
#endif

static struct class_attribute efuse_class_attrs[] = {

	#ifndef EFUSE_READ_ONLY
	/*make the efuse can not be write through sysfs */
	__ATTR(userdata, S_IRWXU, userdata_show, userdata_write),

	#else
	__ATTR_RO(userdata),

	#endif

	__ATTR(mac, S_IRWXU, show_mac, store_mac),

	__ATTR(mac_bt, S_IRWXU, show_mac_bt, store_mac_bt),

	__ATTR(mac_wifi, S_IRWXU, show_mac_wifi, store_mac_wifi),

	__ATTR(usid, S_IRWXU, show_usid, store_usid),

	__ATTR_NULL

};

static struct class efuse_class = {

	.name = EFUSE_CLASS_NAME,

	.class_attrs = efuse_class_attrs,

};

int get_efusekey_info(struct device_node *np)
{
	const phandle *phandle;
	struct device_node *np_efusekey, *np_key;
	int index;
	char *propname;
	const char *uname;
	int ret;

	phandle = of_get_property(np, "key", NULL);
	if (!phandle) {
		pr_info("%s:don't find match key\n", __func__);
		return -1;
	} else {
		np_efusekey = of_find_node_by_phandle(be32_to_cpup(phandle));
		if (!np_efusekey) {
			pr_err("can't find device node key\n");
			return -1;
		}
	}

	ret = of_property_read_u32(np_efusekey, "keynum", &efusekeynum);
	if (ret) {
		pr_err("please config efusekeynum item\n");
		return -1;
	}
	pr_info("efusekeynum: %d\n", efusekeynum);

	if (efusekeynum > 0) {
		efusekey_infos = kzalloc((sizeof(struct efusekey_info))
			*efusekeynum, GFP_KERNEL);
		if (!efusekey_infos) {
			pr_err("%s efuse_keys alloc err\n", __func__);
			return -1;
		}
	}

	for (index = 0; index < efusekeynum; index++) {
		propname = kasprintf(GFP_KERNEL, "key%d", index);

		phandle = of_get_property(np_efusekey, propname, NULL);
		if (!phandle) {
			pr_err("don't find  match %s\n", propname);
			goto err;
		}
		if (phandle){
			np_key = of_find_node_by_phandle(be32_to_cpup(phandle));
		  if (!np_key) {
				pr_err("can't find device node\n");
		    goto err;
		  }
		}
		ret = of_property_read_string(np_key, "keyname", &uname);
		if (ret) {
			pr_err("please config keyname item\n");
			goto err;
		}
		strcpy(efusekey_infos[index].keyname, uname);
		ret = of_property_read_u32(np_key, "offset",
			&(efusekey_infos[index].offset));
		if (ret) {
			pr_err("please config offset item\n");
			goto err;
		}
		ret = of_property_read_u32(np_key, "size",
			&(efusekey_infos[index].size));
		if (ret) {
			pr_err("please config size item\n");
			goto err;
		}

		pr_info("efusekeyname: %15s\toffset: %5d\tsize: %5d\n",
			efusekey_infos[index].keyname,
			efusekey_infos[index].offset,
			efusekey_infos[index].size);
		kfree(propname);
	}
	return 0;
err:
	kfree(efusekey_infos);
	return -1;

}

static int efuse_probe(struct platform_device *pdev)
{
	int ret;
	struct device *devp;
	struct device_node *np = pdev->dev.of_node;
	struct reset_control *efuse_rst;

	/* open clk gate HHI_GCLK_MPEG0 bit62*/
	efuse_rst = devm_reset_control_get(&pdev->dev, "efuse_clk");
	if (IS_ERR(efuse_rst))
		dev_err(&pdev->dev, " open efuse clk gate error!!\n");
	else
		reset_control_deassert(efuse_rst);

	if (pdev->dev.of_node) {
		of_node_get(np);

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

	  ret = of_property_read_u32(np, "get_max_cmd", &efuse_get_max_cmd);
		if (ret) {
			dev_err(&pdev->dev, "please config get_max_cmd\n");
			return -1;
		}

		get_efusekey_info(np);
	}

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
		dev_err(&pdev->dev, "failed to add device\n");
		goto error3;
	}

	devp = device_create(&efuse_class, NULL, efuse_devno, NULL, "efuse");
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}
	dev_dbg(&pdev->dev, "device %s created\n", EFUSE_DEVICE_NAME);

	sharemem_input_base = get_secmon_sharemem_input_base();
	sharemem_output_base = get_secmon_sharemem_output_base();
	dev_info(&pdev->dev, "probe OK!\n");
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
MODULE_AUTHOR("Cai Yun <yun.cai@amlogic.com>");

