// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mtd/mtd.h>
#include <linux/amlogic/aml_rsv.h>
#include <linux/module.h>

static int __meson_rsv_open(struct inode *node, struct file *file)
{
	struct meson_rsv_user_t *user;

	user = container_of(node->i_cdev, struct meson_rsv_user_t, cdev);
	file->private_data = user;

	return 0;
}

static loff_t __meson_rsv_llseek(struct file *file, loff_t off, int whence)
{
	struct meson_rsv_user_t *user = file->private_data;
	struct meson_rsv_info_t *info = user->info;
	loff_t newpos;

	if (!info->valid) {
		pr_info("%s: invalid %s\n", __func__, info->name);
		return -EFAULT;
	}

	mutex_lock(&user->lock);
	switch (whence) {
	case 0: /* SEEK_SET (start postion)*/
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = file->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = info->size - 1;
		newpos = newpos - off;
		break;

	default: /* can't happen */
		mutex_unlock(&user->lock);
		return -EINVAL;
	}

	if (newpos < 0 || newpos >= info->size) {
		mutex_unlock(&user->lock);
		return -EINVAL;
	}

	file->f_pos = newpos;
	mutex_unlock(&user->lock);

	return newpos;
}

static ssize_t __meson_rsv_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct meson_rsv_user_t *user = file->private_data;
	struct meson_rsv_info_t *info = user->info;
	u8 *vbuf;
	size_t count_backup = count;
	int ret = 0;

	if (!info->valid) {
		pr_info("%s: %s: invalid\n", __func__, info->name);
		return -EFAULT;
	}

	if (*ppos == info->size)
		return 0;

	if (*ppos > info->size) {
		pr_info("%s: %s: out of space\n", __func__, info->name);
		return -EFAULT;
	}

	vbuf = vmalloc(info->size + info->mtd->writesize);
	if (!vbuf)
		return -ENOMEM;

	mutex_lock(&user->lock);
	ret = info->read(vbuf, info->size);
	if (ret) {
		pr_info("%s: %s: failed %d\n", __func__, info->name, ret);
		goto exit;
	}

	if ((*ppos + count) > info->size)
		count = info->size - *ppos;

	ret = copy_to_user(buf, vbuf + *ppos, count);
	*ppos += count;
exit:
	mutex_unlock(&user->lock);
	vfree(vbuf);
	return count_backup;
}

static ssize_t __meson_rsv_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct meson_rsv_user_t *user = file->private_data;
	struct meson_rsv_info_t *info = user->info;
	u8 *vbuf;
	size_t count_backup = count;
	int ret = 0;

	if (!info->valid) {
		pr_info("%s: %s: invalid\n", __func__, info->name);
		return -EFAULT;
	}

	if (*ppos == info->size)
		return 0;

	if (*ppos > info->size) {
		pr_info("%s: %s: out of space\n", __func__, info->name);
		return -EFAULT;
	}

	vbuf = vmalloc(info->size + info->mtd->writesize);
	if (!vbuf)
		return -ENOMEM;

	mutex_lock(&user->lock);
	if (info->valid) {
		ret = info->read(vbuf, info->size);
		if (ret) {
			pr_info("%s: %s: failed %d\n", __func__,
				info->name, ret);
			goto exit;
		}
	}
	if ((*ppos + count) > info->size)
		count = info->size - *ppos;

	ret = copy_from_user(vbuf + *ppos, buf, count);
	ret = info->write(vbuf, info->size);
	if (ret) {
		pr_info("%s: %s: failed %d\n", __func__, info->name, ret);
		goto exit;
	}
	*ppos += count;
exit:
	mutex_unlock(&user->lock);
	vfree(vbuf);
	return count_backup;
}

static long __meson_rsv_ioctl(struct file *file, u32 cmd, unsigned long args)
{
	return 0;
}

static const struct file_operations meson_rsv_ops = {
	.open = __meson_rsv_open,
	.read = __meson_rsv_read,
	.write = __meson_rsv_write,
	.unlocked_ioctl = __meson_rsv_ioctl,
	.llseek	= __meson_rsv_llseek,
};

int meson_rsv_register_cdev(struct meson_rsv_info_t *info, char *name)
{
	struct meson_rsv_user_t *user;
	int ret = 0;

	if (!info) {
		pr_info("%s: rsv not init yet!\n", __func__);
		return -EFAULT;
	}

	user = kzalloc(sizeof(*user), GFP_KERNEL);
	if (!user)
		return -ENOMEM;

	user->info = info;
	mutex_init(&user->lock);

	ret = alloc_chrdev_region(&user->devt, 0, 1, name);
	if (ret < 0) {
		pr_info("%s: alloc dev_t failed\n", __func__);
		goto exit_err;
	}

	cdev_init(&user->cdev, &meson_rsv_ops);
	user->cdev.owner = THIS_MODULE;
	ret = cdev_add(&user->cdev, user->devt, 1);
	if (ret) {
		pr_info("%s: add cdev failed\n", __func__);
		goto exit_err1;
	}

	user->cls = class_create(THIS_MODULE, name);
	if (IS_ERR(user->cls)) {
		pr_info("%s: create class failed\n", __func__);
		goto exit_err2;
	}

	user->dev = device_create(user->cls, NULL, user->devt, NULL, name);
	if (IS_ERR(user->dev)) {
		pr_info("%s: create device failed\n", __func__);
		goto exit_err3;
	}

	pr_info("%s: register %s device OK\n", __func__, name);
	return 0;

exit_err3:
	class_destroy(user->cls);
exit_err2:
	cdev_del(&user->cdev);
exit_err1:
	unregister_chrdev_region(user->devt, 1);
exit_err:
	kfree(user);
	return ret;
}

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("xianjun.liu <xianjun.liu@amlogic.com>");
MODULE_DESCRIPTION("Amlogic's Meson MTD RESVER Management driver");

