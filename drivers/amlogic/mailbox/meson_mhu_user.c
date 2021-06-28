// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/mailbox_client.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/amlogic/scpi_protocol.h>
#include <dt-bindings/mailbox/amlogic,mbox.h>
#include "meson_mhu_user.h"

#define DRIVER_NAME		"user-mbox"
#define CMD_LEN			4

static struct list_head mbox_devs = LIST_HEAD_INIT(mbox_devs);

static void mhu_cleanup_devs(void)
{
	struct mhu_dev *cur, *n;

	list_for_each_entry_safe(cur, n, &mbox_devs, list) {
		if (cur->dev) {
			cdev_del(&cur->char_cdev);
			device_del(cur->dev);
		}
		list_del(&cur->list);
		kfree(cur->data);
		kfree(cur);
	}
}

static ssize_t mbox_message_write(struct file *filp,
				  const char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	int ret;
	struct mhu_dev *mhu_dev = filp->private_data;
	struct device *dev = mhu_dev->p_dev;
	int dest =  mhu_dev->dest;
	int idx = mhu_dev->chan_idx;
	char data[MBOX_USER_SIZE];
	size_t rx_size;

	if (count > MBOX_USER_SIZE) {
		dev_err(dev, "Msg len %zd over range\n", count);
		return -EINVAL;
	}

	ret = copy_from_user(data, userbuf, count);
	if (ret) {
		ret = -EFAULT;
		goto err_probe0;
	}

	switch (dest) {
	case MAILBOX_AOCPU:
		if (mhu_dev->busy) {
			ret = wait_for_completion_killable(&mhu_dev->complete);
			if (ret)
				break;
		}
		mhu_dev->busy = true;
		mbox_message_send_ao_sync(dev, *(uint32_t *)data, data + CMD_LEN, count,
				mhu_dev->data, count, idx);
		break;
	case MAILBOX_DSP:
		ret = -EINVAL;
		break;
	case MAILBOX_SECPU:
		rx_size = count;
		if (mhu_dev->busy) {
			ret = wait_for_completion_killable(&mhu_dev->complete);
			if (ret)
				break;
		}
		mhu_dev->busy = true;
		mbox_message_send_sec_sync(dev, 0xff, data, count,
				mhu_dev->data, &rx_size, idx);
		mhu_dev->r_size = rx_size;
		break;
	default:
		pr_err("desitation error %d\n", dest);
		ret = -EINVAL;
		break;
	};
err_probe0:
	return ret;
}

static ssize_t mbox_message_read(struct file *filp, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	int ret = -1;
	struct mhu_dev *mhu_dev = filp->private_data;
	int dest =  mhu_dev->dest;
	size_t size;

	if (count > MBOX_USER_SIZE)
		count = MBOX_USER_SIZE;

	size = count > mhu_dev->r_size ? mhu_dev->r_size : count;
	if (dest == MAILBOX_AOCPU || dest == MAILBOX_SECPU) {
		*ppos = 0;
		ret = simple_read_from_buffer(userbuf, size, ppos,
					mhu_dev->data, size);
		memset(mhu_dev->data, 0, size);
		mhu_dev->r_size = 0;
		complete(&mhu_dev->complete);
		mhu_dev->busy = false;
		return ret;
	}

	return ret;
}

static int mbox_message_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev = inode->i_cdev;
	struct mhu_dev *mhu_dev = container_of(cdev, struct mhu_dev, char_cdev);

	filp->private_data = mhu_dev;
	return 0;
}

static const struct file_operations mbox_message_ops = {
	.write		= mbox_message_write,
	.read		= mbox_message_read,
	.open		= mbox_message_open,
};

static int mhu_cdev_init(struct device *dev)
{
	static struct class *mbox_class;
	struct mhu_dev *mhu_dev;
	dev_t char_dev;
	u32 idx;
	int err = 0;
	int mbox_nums = 0;

	of_property_read_u32(dev->of_node,
			     "mbox-nums", &mbox_nums);
	if (!mbox_nums) {
		dev_err(dev, "No mbox-nums\n");
		return -1;
	}

	mbox_class = class_create(THIS_MODULE, "mailbox");
	if (IS_ERR(mbox_class))
		goto err;

	err = alloc_chrdev_region(&char_dev, 0, mbox_nums, DRIVER_NAME);
	if (err < 0) {
		dev_err(dev, "alloc dev_t failed\n");
		err = -1;
		goto class_err;
	}

	for (idx = 0; idx < mbox_nums; idx++) {
		mhu_dev = kzalloc(sizeof(*mhu_dev), GFP_KERNEL);
		if (IS_ERR(mhu_dev)) {
			dev_err(dev, "Failed to alloc mhu_dev\n");
			goto out_err;
		}

		mhu_dev->chan_idx = idx;
		mhu_dev->p_dev = dev;
		mhu_dev->char_no = MKDEV(MAJOR(char_dev), idx);
		cdev_init(&mhu_dev->char_cdev, &mbox_message_ops);
		err = cdev_add(&mhu_dev->char_cdev, mhu_dev->char_no, 1);
		if (err) {
			dev_err(dev, "mbox fail to add cdev\n");
			goto out_err;
		}

		if (of_property_read_string_index(dev->of_node,
						  "mbox-names", idx, &mhu_dev->name)) {
			dev_err(dev, "%s get mbox[%d] name fail\n",
				__func__, idx);
			goto out_err;
		}

		if (of_property_read_u32_index(dev->of_node, "mbox-dests",
						idx, &mhu_dev->dest)) {
			dev_err(dev, "%s get mbox[%d] dest fail\n",
				__func__, idx);
			goto out_err;
		}
		mhu_dev->dev =
			device_create(mbox_class, NULL, mhu_dev->char_no,
				      mhu_dev, "%s", mhu_dev->name);
		if (IS_ERR(mhu_dev->dev)) {
			dev_err(dev, "mbox fail to create device\n");
			goto out_err;
		}
		mhu_dev->data = kzalloc(MBOX_USER_SIZE, GFP_KERNEL);
		if (IS_ERR(mhu_dev->data)) {
			dev_err(dev, "No Mem\n");
			goto out_err;
		}
		mhu_dev->busy = false;
		init_completion(&mhu_dev->complete);
		list_add_tail(&mhu_dev->list, &mbox_devs);
	}
	return 0;
out_err:
	mhu_cleanup_devs();
	unregister_chrdev_region(char_dev, mbox_nums);
class_err:
	class_destroy(mbox_class);
err:
	return err;
}

static int mhu_user_probe(struct platform_device *pdev)
{
	return mhu_cdev_init(&pdev->dev);
}

static int mhu_user_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id mhu_of_match[] = {
	{ .compatible = "amlogic, meson-mbox-user" },
	{},
};

static struct platform_driver mhu_user_driver = {
	.probe = mhu_user_probe,
	.remove = mhu_user_remove,
	.driver = {
		.owner		= THIS_MODULE,
		.name		= DRIVER_NAME,
		.of_match_table = mhu_of_match,
	},
};

int __init aml_mhu_user_init(void)
{
	return platform_driver_register(&mhu_user_driver);
}

void __exit aml_mhu_user_exit(void)
{
	platform_driver_unregister(&mhu_user_driver);
}
