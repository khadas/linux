/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <uapi/linux/major.h>
#include "video_framerate_adapter.h"

#define CLASS_NAME	"framerate_adapter"
#define DEV_NAME	"framerate_dev"

#ifndef VIDEOFRAME_MAJOR
#define VIDEOFRAME_MAJOR AMSTREAM_MAJOR
#endif

struct frame_rate_dev_s* frame_rate_dev;

 void vframe_rate_uevent(int duration)
{
	char *configured[2];
	char framerate[40] = {0};

	sprintf(framerate, "FRAME_RATE_HINT=%lu",
		(unsigned long)duration);
	configured[0] = framerate;
	configured[1] = NULL;
	kobject_uevent_env(&frame_rate_dev->dev->kobj,
		KOBJ_CHANGE, configured);

	pr_info("%s: sent uevent %s\n", __func__, configured[0]);
}

EXPORT_SYMBOL(vframe_rate_uevent);

static const struct file_operations frame_rate_fops = {
	.owner = THIS_MODULE
};

static struct attribute *frame_rate_class_attrs[] = {
	NULL
};

ATTRIBUTE_GROUPS(frame_rate_class);

static struct class frame_rate_class = {
	.name = CLASS_NAME,
	.class_groups = frame_rate_class_groups,
};

static int frame_rate_driver_init(void)
{
	int ret = -1;

	frame_rate_dev = kzalloc(sizeof(struct frame_rate_dev_s), GFP_KERNEL);
	if (IS_ERR_OR_NULL(frame_rate_dev))
		return -ENOMEM;

	frame_rate_dev->dev_no = MKDEV(VIDEOFRAME_MAJOR, 101);

	ret = register_chrdev_region(frame_rate_dev->dev_no, 1, DEV_NAME);
	if (ret < 0) {
		pr_err("Can't get major number %d.\n", VIDEOFRAME_MAJOR);
		goto err_4;
	}

	cdev_init(&frame_rate_dev->cdev, &frame_rate_fops);
	frame_rate_dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&frame_rate_dev->cdev, frame_rate_dev->dev_no, 1);
	if (ret) {
		pr_err("Error %d adding cdev fail.\n", ret);
		goto err_3;
	}

	ret = class_register(&frame_rate_class);
	if (ret < 0) {
		pr_err("Failed in creating class.\n");
		goto err_2;
	}

	frame_rate_dev->dev = device_create(&frame_rate_class, NULL,
		frame_rate_dev->dev_no, NULL, DEV_NAME);
	if (IS_ERR_OR_NULL(frame_rate_dev->dev)) {
		pr_err("Create device failed.\n");
		ret = -ENODEV;
		goto err_1;
	}
	pr_info("Registered frame rate driver success.\n");
	return 0;

err_1:
	device_destroy(&frame_rate_class, frame_rate_dev->dev_no);
err_2:
	class_unregister(&frame_rate_class);
err_3:
	cdev_del(&frame_rate_dev->cdev);
err_4:
	unregister_chrdev_region(frame_rate_dev->dev_no, 1);
	kfree(frame_rate_dev);
	return ret;
}

static void frame_rate_driver_exit(void)
{
	device_destroy(&frame_rate_class, frame_rate_dev->dev_no);
	class_unregister(&frame_rate_class);
	cdev_del(&frame_rate_dev->cdev);
	unregister_chrdev_region(frame_rate_dev->dev_no, 1);
	kfree(frame_rate_dev);
}

static int __init frame_rate_module_init(void)
{
	int ret = -1;

	ret = frame_rate_driver_init();
	if (ret) {
		pr_info("Error %d frame_rate_module_init init fail.\n", ret);
	}
	return ret;
}

static void __exit frame_rate_module_exit(void)
{
	frame_rate_driver_exit();
	pr_info("frame_rate_module_exit\n");
}

module_init(frame_rate_module_init);
module_exit(frame_rate_module_exit);

MODULE_AUTHOR("<shilong.yang@amlogic.com>");
MODULE_DESCRIPTION("framerate adapter");
MODULE_LICENSE("GPL");
