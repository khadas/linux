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
#include <linux/amlogic/secmon.h>
#include <linux/compat.h>
#define EFUSE_DEVICE_NAME   "efuse"
#define EFUSE_CLASS_NAME    "efuse"

static struct aml_efuse_key efuse_key = {
	.num      = 0,
	.infos    = NULL,
};

static struct aml_efuse_lockable_check efusecheck = {
	.main_cmd = 0,
	.item_num = 0,
	.infos = NULL,
};

static struct efuse_obj_field_t efuse_field;

struct aml_efuse_cmd efuse_cmd;
unsigned int efuse_pattern_size;
void __iomem *sharemem_input_base;
void __iomem *sharemem_output_base;
unsigned int efuse_obj_cmd_status;

#define  DEFINE_EFUSEKEY_SHOW_ATTR(keyname)	\
	static ssize_t  keyname##_show(struct class *cla, \
					  struct class_attribute *attr,	\
						char *buf)	\
	{	\
		ssize_t ret;	\
		\
		ret = efuse_user_attr_show(#keyname, buf); \
		return ret; \
	}
DEFINE_EFUSEKEY_SHOW_ATTR(mac)
DEFINE_EFUSEKEY_SHOW_ATTR(mac_bt)
DEFINE_EFUSEKEY_SHOW_ATTR(mac_wifi)
DEFINE_EFUSEKEY_SHOW_ATTR(usid)

#ifndef EFUSE_READ_ONLY
#define  DEFINE_EFUSEKEY_STORE_ATTR(keyname)	\
	static ssize_t  keyname##_store(struct class *cla, \
					  struct class_attribute *attr,	\
						const char *buf,	\
						size_t count)	\
	{	\
		ssize_t ret;	\
		\
		ret = efuse_user_attr_store(#keyname, buf, count); \
		return ret; \
	}
DEFINE_EFUSEKEY_STORE_ATTR(mac)
DEFINE_EFUSEKEY_STORE_ATTR(mac_bt)
DEFINE_EFUSEKEY_STORE_ATTR(mac_wifi)
DEFINE_EFUSEKEY_STORE_ATTR(usid)
#endif

int efuse_getinfo(char *item, struct efusekey_info *info)
{
	int i;
	int ret = -EINVAL;

	for (i = 0; i < efuse_key.num; i++) {
		if (strcmp(efuse_key.infos[i].keyname, item) == 0) {
			strcpy(info->keyname, efuse_key.infos[i].keyname);
			info->offset = efuse_key.infos[i].offset;
			info->size = efuse_key.infos[i].size;
			ret = 0;
			break;
		}
	}
	if (ret < 0)
		pr_err("%s item not found.\n", item);
	return ret;
}

/*return: 0:is configurated, -1: don't cfg*/
int efuse_burn_lockable_is_cfg(char *itemname)
{
	int ret = -1;
	int i;

	for (i = 0; i < efusecheck.item_num; i++) {
		if (strcmp(itemname, efusecheck.infos[i].itemname) == 0) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/*
 * return: 1:burned(written), 0: not write, -1: fail
 */
int efuse_burn_check_burned(char *itemname)
{
	int i;
	int ret = -1;
	int subcmd;

	for (i = 0; i < efusecheck.item_num; i++) {
		if (strcmp(itemname, efusecheck.infos[i].itemname) == 0) {
			subcmd = efusecheck.infos[i].subcmd;
			ret = efuse_amlogic_check_lockable_item(subcmd);
			break;
		}
	}
	return ret;
}

static int efuse_open(struct inode *inode, struct file *file)
{
	struct aml_efuse_dev *devp;

	devp = container_of(inode->i_cdev, struct aml_efuse_dev, cdev);
	file->private_data = devp;

	return 0;
}

static int efuse_release(struct inode *inode, struct file *file)
{
	return 0;
}

static loff_t efuse_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;
	ssize_t max_size;

	max_size = efuse_get_max();
	if (max_size <= 0)
		return -EINVAL;

	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = max_size + off;
		break;

	default: /* can't happen */
		return -EINVAL;
	}

	if (newpos < 0 || newpos >= max_size)
		return -EINVAL;

	filp->f_pos = newpos;
	return newpos;
}

static long efuse_unlocked_ioctl(struct file *file,
				 unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct efusekey_info info;

	switch (cmd) {
	case EFUSE_INFO_GET:
		if (copy_from_user((void *)&info, argp, sizeof(info))) {
			pr_err("%s: copy_from_user fail\n", __func__);
			return -EFAULT;
		}

		if (efuse_getinfo(info.keyname, &info) < 0) {
			pr_err("efuse: %s is not found\n", info.keyname);
			return -EFAULT;
		}

		if (copy_to_user(argp, &info, sizeof(info))) {
			pr_err("%s: copy_to_user fail\n", __func__);
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
	long ret;

	args = (unsigned long)compat_ptr(args);
	ret = efuse_unlocked_ioctl(filp, cmd, args);

	return ret;
}
#endif

static ssize_t efuse_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	ssize_t ret;
	unsigned int  pos = (unsigned int)*ppos;
	unsigned char *op = NULL;
	unsigned int max_size;

	ret = efuse_get_max();
	if (ret < 0) {
		pr_err("efuse: failed to get userdata max size\n");
		goto exit;
	}
	max_size = (unsigned int)ret;

	if (pos >= max_size || count > max_size || count > max_size - pos) {
		ret = -EINVAL;
		pr_err("efuse: data range over userdata range\n");
		goto exit;
	}
	op = kzalloc((sizeof(char) * count), GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse: failed to allocate memory\n");
		goto exit;
	}

	memset(op, 0, count);

	ret = efuse_read_usr(op, count, ppos);
	if (ret < 0) {
		kfree(op);
		pr_err("efuse: read user data fail!\n");
		goto exit;
	}

	ret = copy_to_user((void *)buf, (void *)op, count);
	kfree(op);

	if (ret) {
		ret = -EFAULT;
		pr_err("%s: copy_to_user fail!!\n", __func__);
		goto exit;
	}

	ret = count;

exit:
	return ret;
}

static ssize_t efuse_write(struct file *file,
			   const char __user *buf, size_t count, loff_t *ppos)
{
#ifndef EFUSE_READ_ONLY
	ssize_t ret;
	unsigned int  pos = (unsigned int)*ppos;
	unsigned char *op = NULL;
	unsigned int max_size;

	ret = efuse_get_max();
	if (ret < 0) {
		pr_err("efuse: failed to get userdata max size\n");
		goto exit;
	}
	max_size = (unsigned int)ret;

	if (pos >= max_size || count > max_size || count > max_size - pos) {
		ret = -EINVAL;
		pr_err("efuse: data range over userdata range\n");
		goto exit;
	}

	op = kzalloc((sizeof(char) * count), GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse: failed to allocate memory\n");
		goto exit;
	}

	memset(op, 0, count);

	if (copy_from_user((void *)op, (void *)buf, count)) {
		kfree(op);
		ret = -EFAULT;
		pr_err("%s: copy_from_user fail!!\n", __func__);
		goto exit;
	}

	ret = efuse_write_usr(op, count, ppos);
	kfree(op);

	if (ret < 0) {
		pr_err("efuse: write user area %d bytes data fail\n",
		       (unsigned int)count);
		goto exit;
	}

	pr_info("efuse: write user area %d bytes data OK\n", (unsigned int)ret);

	ret = count;

exit:
	return ret;
#else
	pr_err("no permission to write!!\n");
	return -EPERM;
#endif
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
	char *op = NULL;
	ssize_t ret;
	int i;
	const char *c, *s;
	struct efusekey_info info;
	unsigned int uint_val;
	loff_t pos;

	if (efuse_getinfo(name, &info) < 0) {
		ret = -EINVAL;
		pr_err("efuse: %s is not found\n", name);
		goto exit;
	}

	op = kzalloc(sizeof(char) * count, GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse: failed to allocate memory\n");
		goto exit;
	}

	memset(op, 0, count);
	memcpy(op, buf, count);

	c = ":";
	s = op;
	if (strstr(s, c)) {
		for (i = 0; i < info.size; i++) {
			uint_val = 0;
			if (i != info.size - 1) {
				ret = sscanf(s, "%x:", &uint_val);
				if (ret != 1  || uint_val > 0xff)
					ret = -EINVAL;
			} else {
				ret = kstrtou8(s, 16,
					       (unsigned char *)&uint_val);
			}

			if (ret < 0) {
				kfree(op);
				pr_err("efuse: get key input data fail\n");
				goto exit;
			}

			op[i] = uint_val;

			s += 2;
			if (!strncmp(s, c, 1))
				s++;
		}
	} else if ((op[count - 1] != 0x0A && count != info.size) ||
		   count - 1 > info.size || count < info.size) {
		kfree(op);
		ret = -EINVAL;
		pr_err("efuse: key data length not match\n");
		goto exit;
	}

	pos = ((loff_t)(info.offset)) & 0xffffffff;
	ret = efuse_write_usr(op, info.size, &pos);
	kfree(op);

	if (ret < 0) {
		pr_err("efuse: write user area %d bytes data fail\n",
		       (unsigned int)info.size);
		goto exit;
	}

	pr_info("efuse: write user area %d bytes data OK\n",
		(unsigned int)ret);

	ret = count;

exit:
	return ret;
#else
	pr_err("no permission to write!!\n");
	return -EPERM;
#endif
}
EXPORT_SYMBOL(efuse_user_attr_store);

ssize_t efuse_user_attr_show(char *name, char *buf)
{
	char *op = NULL;
	ssize_t ret;
	ssize_t len = 0;
	struct efusekey_info info;
	int i;
	loff_t pos;

	if (efuse_getinfo(name, &info) < 0) {
		ret = -EINVAL;
		pr_err("efuse: %s is not found\n", name);
		goto exit;
	}

	op = kzalloc(sizeof(char) * info.size, GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse: failed to allocate memory\n");
		goto exit;
	}

	memset(op, 0, info.size);

	pos = ((loff_t)(info.offset)) & 0xffffffff;
	ret = efuse_read_usr(op, info.size, &pos);
	if (ret < 0) {
		kfree(op);
		pr_err("efuse: read user data fail!\n");
		goto exit;
	}

	for (i = 0; i < info.size; i++) {
		if (i != 0 && (i % 16 == 0))
			len += sprintf(buf + len, "\n");
		if (i % 16 == 0)
			len += sprintf(buf + len, "0x%02x: ", i);

		len += sprintf(buf + len, "%02x ", op[i]);
	}
	len += sprintf(buf + len, "\n");
	kfree(op);

	ret = len;

exit:
	return ret;
}

ssize_t efuse_user_attr_read(char *name, char *buf)
{
	char *op = NULL;
	ssize_t ret;
	struct efusekey_info info;
	loff_t pos;

	if (efuse_getinfo(name, &info) < 0) {
		ret = -EINVAL;
		pr_err("efuse: %s is not found\n", name);
		goto exit;
	}

	op = kzalloc(sizeof(char) * info.size, GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse: failed to allocate memory\n");
		goto exit;
	}

	memset(op, 0, info.size);

	pos = ((loff_t)(info.offset)) & 0xffffffff;
	ret = efuse_read_usr(op, info.size, &pos);
	if (ret < 0) {
		kfree(op);
		pr_err("efuse: read user data fail!\n");
		goto exit;
	}

	memcpy(buf, op, info.size);
	kfree(op);

	ret = (ssize_t)info.size;

exit:
	return ret;
}

static ssize_t userdata_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	char *op = NULL;
	ssize_t ret;
	ssize_t len = 0;
	int i;
	loff_t offset = 0;
	unsigned int max_size;

	ret = efuse_get_max();
	if (ret < 0) {
		pr_err("efuse: failed to get userdata max size\n");
		goto exit;
	}
	max_size = (unsigned int)ret;

	op = kcalloc(max_size, sizeof(char), GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse: failed to allocate memory\n");
		goto exit;
	}

	memset(op, 0, max_size);

	ret = efuse_read_usr(op, max_size, &offset);
	if (ret < 0) {
		kfree(op);
		pr_err("efuse: read user data error!!\n");
		goto exit;
	}

	for (i = 0; i < ret; i++) {
		if (i != 0 && (i % 16 == 0))
			len += sprintf(buf + len, "\n");
		if (i % 16 == 0)
			len += sprintf(buf + len, "0x%02x: ", i);

		len += sprintf(buf + len, "%02x ", op[i]);
	}
	len += sprintf(buf + len, "\n");
	kfree(op);

	ret = len;

exit:
	return ret;
}

#ifndef EFUSE_READ_ONLY
static ssize_t userdata_store(struct class *cla,
			      struct class_attribute *attr,
			      const char *buf, size_t count)
{
	ssize_t ret;
	loff_t offset = 0;
	char *op = NULL;
	unsigned int max_size;

	ret = efuse_get_max();
	if (ret < 0) {
		pr_err("efuse: failed to get userdata max size\n");
		goto exit;
	}
	max_size = (unsigned int)ret;

	if (count > max_size) {
		ret = -EINVAL;
		pr_err("efuse: data length over userdata max size\n");
		goto exit;
	}

	op = kzalloc(sizeof(char) * count, GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse: failed to allocate memory\n");
		goto exit;
	}

	memset(op, 0, count);
	memcpy(op, buf, count);

	ret = efuse_write_usr(op, count, &offset);
	kfree(op);

	if (ret < 0) {
		pr_err("efuse: write user area %d bytes data fail\n",
		       (unsigned int)count);
		goto exit;
	}

	pr_info("efuse: write user area %d bytes data OK\n",
		(unsigned int)ret);

	ret = count;

exit:
	return ret;
}
#endif

static ssize_t amlogic_set_store(struct class *cla,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	ssize_t ret;
	char *op = NULL;

	if (count != efuse_pattern_size) {
		ret = -EINVAL;
		pr_err("efuse: bad pattern size, only support size %d!\n",
		       efuse_pattern_size);
		goto exit;
	}

	op = kzalloc(sizeof(char) * count, GFP_KERNEL);
	if (!op) {
		ret = -ENOMEM;
		pr_err("efuse: failed to allocate memory!\n");
		goto exit;
	}

	memset(op, 0, count);
	memcpy(op, buf, count);

	ret = efuse_amlogic_set(op, count);
	kfree(op);

	if (ret) {
		pr_err("efuse: pattern programming fail! ret: %d\n",
		       (unsigned int)ret);
		ret = -EINVAL;
		goto exit;
	}

	pr_info("efuse: pattern programming success!\n");

	ret = count;

exit:
	return ret;
}

static ssize_t secureboot_check_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	ssize_t n = 0;
	int ret;

	struct aml_efuse_dev *efuse_dev;

	efuse_dev = container_of(cla, struct aml_efuse_dev, cls);
	if (!efuse_dev->reg_base)
		ret = -EINVAL;
	else
		ret = readl(efuse_dev->reg_base) & efuse_dev->secureboot_mask;
	if (ret < 0)
		n = sprintf(buf, "fail");
	else if (ret == 0)
		n = sprintf(buf, "raw");
	else
		n = sprintf(buf, "encrypt");

	return n;
}

static ssize_t checkburn_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	ssize_t n = 0;
	struct aml_efuse_dev *efuse_dev;

	efuse_dev = container_of(cla, struct aml_efuse_dev, cls);
	if (efuse_dev->name[0]) {
		if (efuse_burn_lockable_is_cfg(efuse_dev->name) == 0) {
			n = efuse_burn_check_burned(efuse_dev->name);
			if (n == 1)
				n = sprintf(buf, "written");
			else if (n == 0)
				n = sprintf(buf, "not write");
			else
				n = sprintf(buf, "error");
		} else {
			n = sprintf(buf, "nocfg");
		}
	} else {
		n = sprintf(buf, "unknown");
	}
	return n;
}

static ssize_t checkburn_store(struct class *cla,
			      struct class_attribute *attr,
			      const char *buf, size_t count)
{
	ssize_t n = 0;
	struct aml_efuse_dev *efuse_dev;

	efuse_dev = container_of(cla, struct aml_efuse_dev, cls);

	n = strlen(buf);
	pr_notice("%s:%d %s,n=%zd\n", __func__, __LINE__, buf, n);

	n--; //discard '\n'
	if (n >= EFUSE_CHECK_NAME_LEN)
		n = EFUSE_CHECK_NAME_LEN - 1;

	memset(efuse_dev->name, 0, sizeof(efuse_dev->name));
	memcpy(efuse_dev->name, buf, n);

	return count;
}

static ssize_t checklist_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	int i;
	ssize_t n = 0;

	for (i = 0; i < efusecheck.item_num; i++) {
		n += sprintf(&buf[n], efusecheck.infos[i].itemname);
		n += sprintf(&buf[n], "\n");
	}
	return n;
}

static int key_item_parse_dt(const struct device_node *np_efusekey,
			     int index, struct efusekey_info *infos)
{
	const phandle *phandle;
	struct device_node *np_key;
	char *propname;
	const char *keyname;
	int ret;
	int name_size;

	propname = kasprintf(GFP_KERNEL, "key%d", index);

	phandle = of_get_property(np_efusekey, propname, NULL);
	if (!phandle) {
		ret = -EINVAL;
		pr_err("failed to find match %s\n", propname);
		goto exit;
	}
	np_key = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (!np_key) {
		ret = -EINVAL;
		pr_err("failed to find device node %s\n", propname);
		goto exit;
	}

	ret = of_property_read_string(np_key, "keyname", &keyname);
	if (ret < 0) {
		pr_err("failed to get keyname item\n");
		goto exit;
	}

	name_size = EFUSE_KEY_NAME_LEN - 1;
	memcpy(infos[index].keyname, keyname,
	       strlen(keyname) > name_size ? name_size : strlen(keyname));

	ret = of_property_read_u32(np_key, "offset",
				   &infos[index].offset);
	if (ret < 0) {
		pr_err("failed to get key offset item\n");
		goto exit;
	}

	ret = of_property_read_u32(np_key, "size",
				   &infos[index].size);
	if (ret < 0) {
		pr_err("failed to get key size item\n");
		goto exit;
	}

	pr_debug("efusekey name: %15s\toffset: %5d\tsize: %5d\n",
		infos[index].keyname,
		infos[index].offset,
		infos[index].size);

	ret = 0;

exit:
	kfree(propname);
	return ret;
}

static int get_efusekey_info(struct device_node *np)
{
	const phandle *phandle;
	struct device_node *np_efusekey = NULL;
	int index;
	int ret;

	phandle = of_get_property(np, "key", NULL);
	if (!phandle) {
		ret = -EINVAL;
		pr_err("failed to find match efuse key\n");
		goto exit;
	}
	np_efusekey = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (!np_efusekey) {
		ret = -EINVAL;
		pr_err("failed to find device node efusekey\n");
		goto exit;
	}

	ret = of_property_read_u32(np_efusekey, "keynum", &efuse_key.num);
	if (ret < 0) {
		pr_err("failed to get efusekey num item\n");
		goto exit;
	}

	if (efuse_key.num <= 0) {
		ret = -EINVAL;
		pr_err("efusekey num config error\n");
		goto exit;
	}
	pr_debug("efusekey num: %d\n", efuse_key.num);

	efuse_key.infos = kzalloc((sizeof(struct efusekey_info))
		* efuse_key.num, GFP_KERNEL);
	if (!efuse_key.infos) {
		ret = -ENOMEM;
		pr_err("fail to alloc enough mem for efusekey_infos\n");
		goto exit;
	}

	for (index = 0; index < efuse_key.num; index++) {
		ret = key_item_parse_dt(np_efusekey, index, efuse_key.infos);
		if (ret < 0) {
			kfree(efuse_key.infos);
			goto exit;
		}
	}

	return 0;

exit:
	return ret;
}

static int check_item_parse_dt(const struct device_node *np_efusecheck,
			     int index, struct lockable_info *infos)
{
	const phandle *phandle;
	struct device_node *np_check;
	char *propname;
	const char *checkname;
	int ret;
	int name_size;

	propname = kasprintf(GFP_KERNEL, "check%d", index);

	phandle = of_get_property(np_efusecheck, propname, NULL);
	if (!phandle) {
		ret = -EINVAL;
		pr_err("failed to find match %s\n", propname);
		goto exit;
	}
	np_check = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (!np_check) {
		ret = -EINVAL;
		pr_err("failed to find device node %s\n", propname);
		goto exit;
	}

	ret = of_property_read_string(np_check, "checkname", &checkname);
	if (ret < 0) {
		pr_err("failed to get checkname item\n");
		goto exit;
	}

	name_size = EFUSE_CHECK_NAME_LEN - 1;
	memcpy(infos[index].itemname, checkname,
	       strlen(checkname) > name_size ? name_size : strlen(checkname));

	ret = of_property_read_u32(np_check, "subcmd",
				   &infos[index].subcmd);
	if (ret < 0) {
		pr_err("failed to get subcmd item\n");
		goto exit;
	}

	pr_debug("efusecheck name: %15s subcmd: 0x%16x\n",
		infos[index].itemname,
		infos[index].subcmd);

	ret = 0;

exit:
	kfree(propname);
	return ret;
}

static int get_efusecheck_info(struct device_node *np)
{
	const phandle *phandle;
	struct device_node *np_ec = NULL;
	int index;
	int ret;

	phandle = of_get_property(np, "check", NULL);
	if (!phandle) {
		ret = -EINVAL;
		pr_err("failed to find match efuse key\n");
		goto exit;
	}
	np_ec = of_find_node_by_phandle(be32_to_cpup(phandle));
	if (!np_ec) {
		ret = -EINVAL;
		pr_err("failed to find device node efusekey\n");
		goto exit;
	}

	ret = of_property_read_u32(np_ec, "maincmd",
			&efusecheck.main_cmd);
	if (ret < 0) {
		efusecheck.main_cmd = EFUSE_READ_CALI_ITEM;
		pr_err("don't cfg efusecheck maincmd, used default:0x%x\n",
				efusecheck.main_cmd);
		pr_notice("don't cfg efusecheck maincmd, used default:0x%x\n",
				efusecheck.main_cmd);
	} else {
		pr_debug("efuse check maincmd:0x%x\n",
			efusecheck.main_cmd);
	}

	ret = of_property_read_u32(np_ec, "checknum", &efusecheck.item_num);
	if (ret < 0) {
		pr_err("failed to get efusecheck num item\n");
		goto exit;
	}

	if (efusecheck.item_num <= 0) {
		ret = -EINVAL;
		pr_err("efusecheck num config error\n");
		goto exit;
	}
	pr_debug("efusecheck num: %d\n", efusecheck.item_num);

	efusecheck.infos = kzalloc((sizeof(struct lockable_info))
		* efusecheck.item_num, GFP_KERNEL);
	if (!efusecheck.infos) {
		ret = -ENOMEM;
		pr_err("fail to alloc enough mem for efusecheck_item\n");
		goto exit;
	}

	for (index = 0; index < efusecheck.item_num; index++) {
		ret = check_item_parse_dt(np_ec, index, efusecheck.infos);
		if (ret < 0) {
			kfree(efusecheck.infos);
			goto exit;
		}
	}

	return 0;

exit:
	return ret;
}

static char *efuse_obj_err_parse(uint32_t  efuse_obj_err_status)
{
	char *err_char = NULL;

	switch (efuse_obj_err_status) {
	case EFUSE_OBJ_ERR_INVALID_DATA:
		err_char = "invalid data";
		break;
	case EFUSE_OBJ_ERR_NOT_FOUND:
		err_char = "field not found";
		break;
	case EFUSE_OBJ_ERR_SIZE:
		err_char = "size not match";
		break;
	case EFUSE_OBJ_ERR_NOT_SUPPORT:
		err_char = "not support";
		break;
	case EFUSE_OBJ_ERR_ACCESS:
		err_char = "access denied";
		break;
	case EFUSE_OBJ_ERR_WRITE_PROTECTED:
		err_char = "write protected";
		break;
	case EFUSE_OBJ_ERR_INTERNAL:
	case EFUSE_OBJ_ERR_OTHER_INTERNAL:
		err_char = "internal error";
		break;
	default:
		err_char = "unknown error";
		break;
		}

	return err_char;
}

static int char2hex(char *hex, void *bin, size_t binlen)
{
	int i, c, n1, n2, hexlen, k;

	hexlen = strnlen(hex, 64);
	k = 0;
	n1 = -1;
	n2 = -1;
	for (i = 0; i < hexlen; i++) {
		n2 = n1;
		c = hex[i];
		if (c >= '0' && c <= '9') {
			n1 = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			n1 = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			n1 = c - 'A' + 10;
		} else if (c == ' ') {
			n1 = -1;
			continue;
		} else {
			return -1;
		}
		if (n1 >= 0 && n2 >= 0) {
			((u8 *)bin)[k] = (n2 << 4) | n1;
			n1 = -1;
			k++;
		}
	}

	return k;
}

static ssize_t efuse_obj_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t count)
{
	int rc = -EINVAL;
	char argv[3][48];
	char argc;
	u8 buff[32];
	u32 bufflen = sizeof(buff);
	char *name = NULL;
	char *data = NULL;
	int dlen = 0;
	u8 databuf[32] = {0};

	argc = sscanf(buf, "%s %s %s", argv[0], argv[1], argv[2]);
	if (argc < 2) {
		pr_err("Invalid number of arguments %d\n", argc);
		return  -EINVAL;
	}

	memset(&buff[0], 0, sizeof(buff));
	memset(&efuse_field, 0, sizeof(efuse_field));

	if (strcmp(argv[0], "get") == 0) {
		// $0 get field
		if (argc == 2) {
			name = argv[1];
			rc = efuse_obj_read(EFUSE_OBJ_EFUSE_DATA, name, buff, &bufflen);
			if (rc == EFUSE_OBJ_SUCCESS) {
				memset(&efuse_field, 0, sizeof(efuse_field));
				strncpy(efuse_field.name, name, sizeof(efuse_field.name) - 1);
				memcpy(efuse_field.data, buff, bufflen);
				efuse_field.size = bufflen;
				rc = count;
			} else {
				pr_err("Error get efuse object: %s: %d\n",
					efuse_obj_err_parse(rc), rc);
				rc = -EINVAL;
			}
		} else {
			pr_err("Error: too many arguments %d\n", argc);
			rc = -EINVAL;
		}
	} else if (strcmp(argv[0], "set") == 0) {
		// $0 set field data
		if (argc == 3) {
			name = argv[1];
			data = argv[2];
			dlen = strnlen(data, 64);
			dlen = char2hex(data, databuf, dlen);
			if (dlen < 0) {
				pr_err("parse data hex2bin error\n");
				return -EINVAL;
			}
			rc = efuse_obj_write(EFUSE_OBJ_EFUSE_DATA, name, databuf, dlen);
			if (rc == EFUSE_OBJ_SUCCESS) {
				rc = count;
			} else {
				pr_err("Error set efuse object: %s: %d\n",
					efuse_obj_err_parse(rc), rc);
				rc = -EINVAL;
			}
		} else {
			pr_err("Error: too few arguments %d\n", argc);
			rc = -EINVAL;
		}
	} else if (strcmp(argv[0], "lock") == 0) {
		// $0 lock field
		if (argc == 2) {
			name = argv[1];
			rc = efuse_obj_write(EFUSE_OBJ_LOCK_STATUS, name, buff, bufflen);
			if (rc == EFUSE_OBJ_SUCCESS) {
				rc = count;
			} else {
				pr_err("Error lock efuse object: %s: %d\n",
					efuse_obj_err_parse(rc), rc);
				rc = -EINVAL;
			}
		} else {
			pr_err("Error: too many arguments %d\n", argc);
			rc = -EINVAL;
		}
	} else if (strcmp(argv[0], "get_lock") == 0) {
		// $0 get_lock field
		if (argc == 2) {
			name = argv[1];
			rc = efuse_obj_read(EFUSE_OBJ_LOCK_STATUS, name, buff, &bufflen);
			if (rc == EFUSE_OBJ_SUCCESS) {
				memset(&efuse_field, 0, sizeof(efuse_field));
				strncpy(efuse_field.name, name, sizeof(efuse_field.name) - 1);
				memcpy(efuse_field.data, buff, bufflen);
				efuse_field.size = bufflen;
				rc = count;
			} else {
				pr_err("Error get_lock efuse object: %s: %d\n",
					efuse_obj_err_parse(rc), rc);
				rc = -EINVAL;
			}
		} else {
			pr_err("Error: too many arguments %d\n", argc);
			rc = -EINVAL;
		}
	} else {
		pr_err("Error: not support efuse object cmd \"%s\"\n", argv[0]);
		rc = -EINVAL;
	}

	return rc;
}

static ssize_t efuse_obj_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;

	for (i = 0; i < efuse_field.size; i++)
		len += sprintf(buf + len, "%02x%s", efuse_field.data[i],
			((i % 16 == 15) || (i == efuse_field.size - 1) ? "\n" : " "));

	memset(&efuse_field, 0, sizeof(efuse_field));

	return len;
}

static EFUSE_CLASS_ATTR(userdata);
static EFUSE_CLASS_ATTR(mac);
static EFUSE_CLASS_ATTR(mac_bt);
static EFUSE_CLASS_ATTR(mac_wifi);
static EFUSE_CLASS_ATTR(usid);
static EFUSE_CLASS_ATTR(efuse_obj);
static CLASS_ATTR_WO(amlogic_set);
static CLASS_ATTR_RO(secureboot_check);
static EFUSE_CLASS_ATTR(checkburn);
static CLASS_ATTR_RO(checklist);

static struct attribute *efuse_class_attrs[] = {
	&class_attr_userdata.attr,
	&class_attr_mac.attr,
	&class_attr_mac_bt.attr,
	&class_attr_mac_wifi.attr,
	&class_attr_usid.attr,
	&class_attr_efuse_obj.attr,
	&class_attr_amlogic_set.attr,
	&class_attr_secureboot_check.attr,
	&class_attr_checkburn.attr,
	&class_attr_checklist.attr,
	NULL,
};

ATTRIBUTE_GROUPS(efuse_class);

static int efuse_probe(struct platform_device *pdev)
{
	int ret;
	struct device *devp;
	struct device_node *np = pdev->dev.of_node;
	struct clk *efuse_clk;
	struct aml_efuse_dev *efuse_dev;
	struct resource *reg_mem = NULL;
	void __iomem *reg_base = NULL;
	unsigned int  secureboot_mask = 0;

	efuse_dev = devm_kzalloc(&pdev->dev, sizeof(*efuse_dev), GFP_KERNEL);
	if (!efuse_dev) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "failed to alloc enough mem for efuse_dev\n");
		goto out;
	}

	efuse_dev->pdev = pdev;
	platform_set_drvdata(pdev, efuse_dev);

	efuse_clk = devm_clk_get(&pdev->dev, "efuse_clk");
	if (IS_ERR(efuse_clk)) {
		dev_dbg(&pdev->dev, "can't get efuse clk gate, use default clk\n");
	} else {
		ret = clk_prepare_enable(efuse_clk);
		if (ret) {
			dev_err(&pdev->dev, "failed to enable efuse clk gate\n");
			goto error1;
		}
	}

	of_node_get(np);

	ret = of_property_read_u32(np, "read_cmd", &efuse_cmd.read_cmd);
	if (ret) {
		dev_err(&pdev->dev, "please config read_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "write_cmd", &efuse_cmd.write_cmd);
	if (ret) {
		dev_err(&pdev->dev, "failed to write_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "get_max_cmd", &efuse_cmd.get_max_cmd);
	if (ret) {
		dev_err(&pdev->dev, "failed to get_max_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "mem_in_base_cmd",
				   &efuse_cmd.mem_in_base_cmd);
	if (ret) {
		dev_err(&pdev->dev, "failed to mem_in_base_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "mem_out_base_cmd",
				   &efuse_cmd.mem_out_base_cmd);
	if (ret) {
		dev_err(&pdev->dev, "failed to mem_out_base_cmd\n");
		goto error1;
	}

	ret = of_property_read_u32(np, "efuse_pattern_size",
				   &efuse_pattern_size);
	if (ret) {
		efuse_pattern_size = EFUSE_PATTERN_SIZE;
		pr_err("can't get efuse_pattern_size, use default size0x%x\n",
			efuse_pattern_size);
	} else {
		pr_debug("efuse_pattern_size:0x%x\n",
			efuse_pattern_size);
	}

	ret = of_property_read_u32(np, "efuse_obj_cmd_status", &efuse_obj_cmd_status);
	if (ret)
		efuse_obj_cmd_status = 0;

	reg_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!IS_ERR_OR_NULL(reg_mem)) {
		reg_base = devm_ioremap_resource(&pdev->dev, reg_mem);
		if (IS_ERR(reg_base)) {
			dev_err(&pdev->dev, "reg0: cannot obtain I/O memory region.\n");
			ret = PTR_ERR(reg_base);
			goto error1;
		} else {
			ret = of_property_read_u32(np, "secureboot_mask",
				   &secureboot_mask);
			if (ret) {
				dev_err(&pdev->dev, "can't get reg secureboot_mask\n");
				goto error1;
			}
		}
	} else {
		dev_err(&pdev->dev, "can't get reg resource\n");
	}

	get_efusekey_info(np);
	get_efusecheck_info(np);

	ret = alloc_chrdev_region(&efuse_dev->devno, 0, 1, EFUSE_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "fail to allocate major number\n ");
		goto error1;
	}

	efuse_dev->reg_base = reg_base;
	efuse_dev->secureboot_mask = secureboot_mask;
	efuse_dev->cls.name = EFUSE_CLASS_NAME;
	efuse_dev->cls.owner = THIS_MODULE;
	efuse_dev->cls.class_groups = efuse_class_groups;
	ret = class_register(&efuse_dev->cls);
	if (ret)
		goto error2;

	cdev_init(&efuse_dev->cdev, &efuse_fops);
	efuse_dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&efuse_dev->cdev, efuse_dev->devno, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to add device\n");
		goto error3;
	}

	devp = device_create(&efuse_dev->cls, NULL,
			     efuse_dev->devno, NULL, "efuse");
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}

	sharemem_input_base = get_meson_sm_input_base();
	sharemem_output_base = get_meson_sm_output_base();

	dev_dbg(&pdev->dev, "device %s created OK\n", EFUSE_DEVICE_NAME);
	return 0;

error4:
	cdev_del(&efuse_dev->cdev);
error3:
	class_unregister(&efuse_dev->cls);
error2:
	unregister_chrdev_region(efuse_dev->devno, 1);
error1:
	devm_kfree(&pdev->dev, efuse_dev);
out:
	return ret;
}

static int efuse_remove(struct platform_device *pdev)
{
	struct aml_efuse_dev *efuse_dev = platform_get_drvdata(pdev);

	unregister_chrdev_region(efuse_dev->devno, 1);
	device_destroy(&efuse_dev->cls, efuse_dev->devno);
	cdev_del(&efuse_dev->cdev);
	class_unregister(&efuse_dev->cls);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id efuse_dt_match[] = {
	{	.compatible = "amlogic, efuse",
	},
	{}
};

static struct platform_driver efuse_driver = {
	.probe = efuse_probe,
	.remove = efuse_remove,
	.driver = {
		.name = EFUSE_DEVICE_NAME,
		.of_match_table = efuse_dt_match,
	.owner = THIS_MODULE,
	},
};

int __init aml_efuse_init(void)
{
	return platform_driver_register(&efuse_driver);
}

void aml_efuse_exit(void)
{
	platform_driver_unregister(&efuse_driver);
}
