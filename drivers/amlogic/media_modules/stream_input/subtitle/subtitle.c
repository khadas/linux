/*
 * drivers/amlogic/media/subtitle/subtitle.c
 *
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
 */

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/uaccess.h>
//#include "amports_priv.h"

//#include "amlog.h"
//MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0, LOG_DEFAULT_LEVEL_DESC,
			 //LOG_DEFAULT_MASK_DESC);
#define DEVICE_NAME "amsubtitle"
/* Dev name as it appears in /proc/devices   */
#define DEVICE_CLASS_NAME "subtitle"

static int subdevice_open;

#define MAX_SUBTITLE_PACKET 10
static DEFINE_MUTEX(amsubtitle_mutex);

struct subtitle_data_s {
	int subtitle_size;
	int subtitle_pts;
	char *data;
};
static struct subtitle_data_s subtitle_data[MAX_SUBTITLE_PACKET];
static int subtitle_enable = 1;
static int subtitle_total;
static int subtitle_width;
static int subtitle_height;
static int subtitle_type = -1;
static int subtitle_current;	/* no subtitle */
/* sub_index node will be modified by libplayer; amlogicplayer will use */
/* it to detect wheather libplayer switch sub finished or not */
static int subtitle_index;	/* no subtitle */
/* static int subtitle_size = 0; */
/* static int subtitle_read_pos = 0; */
static int subtitle_write_pos;
static int subtitle_start_pts;
static int subtitle_fps;
static int subtitle_subtype;
static int subtitle_reset;
/* static int *subltitle_address[MAX_SUBTITLE_PACKET]; */

enum subinfo_para_e {
	SUB_NULL = -1,
	SUB_ENABLE = 0,
	SUB_TOTAL,
	SUB_WIDTH,
	SUB_HEIGHT,
	SUB_TYPE,
	SUB_CURRENT,
	SUB_INDEX,
	SUB_WRITE_POS,
	SUB_START_PTS,
	SUB_FPS,
	SUB_SUBTYPE,
	SUB_RESET,
	SUB_DATA_T_SIZE,
	SUB_DATA_T_DATA
};

struct subinfo_para_s {
	enum subinfo_para_e subinfo_type;
	int subtitle_info;
	char *data;
};

/* total */
/* curr */
/* bimap */
/* text */
/* type */
/* info */
/* pts */
/* duration */
/* color pallete */
/* width/height */

static ssize_t show_curr(struct class *class, struct class_attribute *attr,
						 char *buf)
{
	return sprintf(buf, "%d: current\n", subtitle_current);
}

static ssize_t store_curr(struct class *class, struct class_attribute *attr,
						  const char *buf, size_t size)
{
	unsigned int curr;
	ssize_t r;

	r = kstrtoint(buf, 0, &curr);
	if (r < 0)
		return -EINVAL;


	subtitle_current = curr;

	return size;
}

static ssize_t show_index(struct class *class, struct class_attribute *attr,
						  char *buf)
{
	return sprintf(buf, "%d: current\n", subtitle_index);
}

static ssize_t store_index(struct class *class, struct class_attribute *attr,
						   const char *buf, size_t size)
{
	unsigned int curr;
	ssize_t r;

	r = kstrtoint(buf, 0, &curr);
	if (r < 0)
		return -EINVAL;

	subtitle_index = curr;

	return size;
}

static ssize_t show_reset(struct class *class, struct class_attribute *attr,
						  char *buf)
{
	return sprintf(buf, "%d: current\n", subtitle_reset);
}

static ssize_t store_reset(struct class *class, struct class_attribute *attr,
						   const char *buf, size_t size)
{
	unsigned int reset;
	ssize_t r;

	r = kstrtoint(buf, 0, &reset);

	pr_info("reset is %d\n", reset);
	if (r < 0)
		return -EINVAL;


	subtitle_reset = reset;

	return size;
}

static ssize_t show_type(struct class *class, struct class_attribute *attr,
						 char *buf)
{
	return sprintf(buf, "%d: type\n", subtitle_type);
}

static ssize_t store_type(struct class *class, struct class_attribute *attr,
						  const char *buf, size_t size)
{
	unsigned int type;
	ssize_t r;

	r = kstrtoint(buf, 0, &type);
	if (r < 0)
		return -EINVAL;

	subtitle_type = type;

	return size;
}

static ssize_t show_width(struct class *class, struct class_attribute *attr,
						  char *buf)
{
	return sprintf(buf, "%d: width\n", subtitle_width);
}

static ssize_t store_width(struct class *class, struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int width;
	ssize_t r;

	r = kstrtoint(buf, 0, &width);
	if (r < 0)
		return -EINVAL;

	subtitle_width = width;

	return size;
}

static ssize_t show_height(struct class *class, struct class_attribute *attr,
						   char *buf)
{
	return sprintf(buf, "%d: height\n", subtitle_height);
}

static ssize_t store_height(struct class *class, struct class_attribute *attr,
				const char *buf, size_t size)
{
	unsigned int height;
	ssize_t r;

	r = kstrtoint(buf, 0, &height);
	if (r < 0)
		return -EINVAL;

	subtitle_height = height;

	return size;
}

static ssize_t show_total(struct class *class, struct class_attribute *attr,
						  char *buf)
{
	return sprintf(buf, "%d: num\n", subtitle_total);
}

static ssize_t store_total(struct class *class, struct class_attribute *attr,
				const char *buf, size_t size)
{
	unsigned int total;
	ssize_t r;

	r = kstrtoint(buf, 0, &total);
	if (r < 0)
		return -EINVAL;
	pr_info("subtitle num is %d\n", total);
	subtitle_total = total;

	return size;
}

static ssize_t show_enable(struct class *class, struct class_attribute *attr,
						   char *buf)
{
	if (subtitle_enable)
		return sprintf(buf, "1: enabled\n");

	return sprintf(buf, "0: disabled\n");
}

static ssize_t store_enable(struct class *class, struct class_attribute *attr,
				const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	r = kstrtoint(buf, 0, &mode);
	if (r < 0)
		return -EINVAL;
	pr_info("subtitle enable is %d\n", mode);
	subtitle_enable = mode ? 1 : 0;

	return size;
}

static ssize_t show_size(struct class *class, struct class_attribute *attr,
						 char *buf)
{
	if (subtitle_enable)
		return sprintf(buf, "1: size\n");

	return sprintf(buf, "0: size\n");
}

static ssize_t store_size(struct class *class, struct class_attribute *attr,
						  const char *buf, size_t size)
{
	unsigned int ssize;
	ssize_t r;

	r = kstrtoint(buf, 0, &ssize);
	if (r < 0)
		return -EINVAL;
	pr_info("subtitle size is %d\n", ssize);
	subtitle_data[subtitle_write_pos].subtitle_size = ssize;

	return size;
}

static ssize_t show_startpts(struct class *class, struct class_attribute *attr,
							 char *buf)
{
	return sprintf(buf, "%d: pts\n", subtitle_start_pts);
}

static ssize_t store_startpts(struct class *class, struct class_attribute *attr,
					const char *buf, size_t size)
{
	unsigned int spts;
	ssize_t r;

	r = kstrtoint(buf, 0, &spts);
	if (r < 0)
		return -EINVAL;
	pr_info("subtitle start pts is %x\n", spts);
	subtitle_start_pts = spts;

	return size;
}

static ssize_t show_data(struct class *class, struct class_attribute *attr,
						 char *buf)
{
#if 0
	if (subtitle_data[subtitle_write_pos].data)
		return sprintf(buf, "%lld\n",
		(unsigned long)(subtitle_data[subtitle_write_pos].data));
#endif
	return sprintf(buf, "0: disabled\n");
}

static ssize_t store_data(struct class *class, struct class_attribute *attr,
						  const char *buf, size_t size)
{
	unsigned int address;
	ssize_t r;

	r = kstrtoint(buf, 0, &address);
	if ((r == 0))
		return -EINVAL;
#if 0
	if (subtitle_data[subtitle_write_pos].subtitle_size > 0) {
		subtitle_data[subtitle_write_pos].data = vmalloc((
			subtitle_data[subtitle_write_pos].subtitle_size));
		if (subtitle_data[subtitle_write_pos].data)
			memcpy(subtitle_data[subtitle_write_pos].data,
			(unsigned long *)address,
			subtitle_data[subtitle_write_pos].subtitle_size);
	}
	pr_info("subtitle data address is %x",
			(unsigned int)(subtitle_data[subtitle_write_pos].data));
#endif
	subtitle_write_pos++;
	if (subtitle_write_pos >= MAX_SUBTITLE_PACKET)
		subtitle_write_pos = 0;
	return 1;
}

static ssize_t show_fps(struct class *class, struct class_attribute *attr,
						char *buf)
{
	return sprintf(buf, "%d: fps\n", subtitle_fps);
}

static ssize_t store_fps(struct class *class, struct class_attribute *attr,
						 const char *buf, size_t size)
{
	unsigned int ssize;
	ssize_t r;

	r = kstrtoint(buf, 0, &ssize);
	if (r < 0)
		return -EINVAL;
	pr_info("subtitle fps is %d\n", ssize);
	subtitle_fps = ssize;

	return size;
}

static ssize_t show_subtype(struct class *class, struct class_attribute *attr,
							char *buf)
{
	return sprintf(buf, "%d: subtype\n", subtitle_subtype);
}

static ssize_t store_subtype(struct class *class, struct class_attribute *attr,
			const char *buf, size_t size)
{
	unsigned int ssize;
	ssize_t r;

	r = kstrtoint(buf, 0, &ssize);
	if (r < 0)
		return -EINVAL;
	pr_info("subtitle subtype is %d\n", ssize);
	subtitle_subtype = ssize;

	return size;
}

static struct class_attribute subtitle_class_attrs[] = {
	__ATTR(enable, 0664, show_enable, store_enable),
	__ATTR(total, 0664, show_total, store_total),
	__ATTR(width, 0664, show_width, store_width),
	__ATTR(height, 0664, show_height, store_height),
	__ATTR(type, 0664, show_type, store_type),
	__ATTR(curr, 0664, show_curr, store_curr),
	__ATTR(index, 0664, show_index, store_index),
	__ATTR(size, 0664, show_size, store_size),
	__ATTR(data, 0664, show_data, store_data),
	__ATTR(startpts, 0664, show_startpts,
	store_startpts),
	__ATTR(fps, 0664, show_fps, store_fps),
	__ATTR(subtype, 0664, show_subtype,
	store_subtype),
	__ATTR(reset, 0644, show_reset, store_reset),
	__ATTR_NULL
};


/*********************************************************
 * /dev/amvideo APIs
 *********************************************************/
static int amsubtitle_open(struct inode *inode, struct file *file)
{
	mutex_lock(&amsubtitle_mutex);

	if (subdevice_open) {
		mutex_unlock(&amsubtitle_mutex);
		return -EBUSY;
	}

	subdevice_open = 1;

	try_module_get(THIS_MODULE);

	mutex_unlock(&amsubtitle_mutex);

	return 0;
}

static int amsubtitle_release(struct inode *inode, struct file *file)
{
	mutex_lock(&amsubtitle_mutex);

	subdevice_open = 0;

	module_put(THIS_MODULE);

	mutex_unlock(&amsubtitle_mutex);

	return 0;
}

static long amsubtitle_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	switch (cmd) {
	case AMSTREAM_IOC_GET_SUBTITLE_INFO: {
		struct subinfo_para_s Vstates;
		struct subinfo_para_s *states = &Vstates;

		if (copy_from_user((void *)states,
				(void *)arg, sizeof(Vstates)))
			return -EFAULT;
		switch (states->subinfo_type) {
		case SUB_ENABLE:
			states->subtitle_info = subtitle_enable;
			break;
		case SUB_TOTAL:
			states->subtitle_info = subtitle_total;
			break;
		case SUB_WIDTH:
			states->subtitle_info = subtitle_width;
			break;
		case SUB_HEIGHT:
			states->subtitle_info = subtitle_height;
			break;
		case SUB_TYPE:
			states->subtitle_info = subtitle_type;
			break;
		case SUB_CURRENT:
			states->subtitle_info = subtitle_current;
			break;
		case SUB_INDEX:
			states->subtitle_info = subtitle_index;
			break;
		case SUB_WRITE_POS:
			states->subtitle_info = subtitle_write_pos;
			break;
		case SUB_START_PTS:
			states->subtitle_info = subtitle_start_pts;
			break;
		case SUB_FPS:
			states->subtitle_info = subtitle_fps;
			break;
		case SUB_SUBTYPE:
			states->subtitle_info = subtitle_subtype;
			break;
		case SUB_RESET:
			states->subtitle_info = subtitle_reset;
			break;
		case SUB_DATA_T_SIZE:
			states->subtitle_info =
				subtitle_data[subtitle_write_pos].subtitle_size;
			break;
		case SUB_DATA_T_DATA: {
			if (states->subtitle_info > 0)
				states->subtitle_info =
				(long)subtitle_data[subtitle_write_pos].data;
		}
		break;
		default:
			break;
		}
		if (copy_to_user((void *)arg, (void *)states, sizeof(Vstates)))
			return -EFAULT;
	}

	break;
	case AMSTREAM_IOC_SET_SUBTITLE_INFO: {
		struct subinfo_para_s Vstates;
		struct subinfo_para_s *states = &Vstates;

		if (copy_from_user((void *)states,
				(void *)arg, sizeof(Vstates)))
			return -EFAULT;
		switch (states->subinfo_type) {
		case SUB_ENABLE:
			subtitle_enable = states->subtitle_info;
			break;
		case SUB_TOTAL:
			subtitle_total = states->subtitle_info;
			break;
		case SUB_WIDTH:
			subtitle_width = states->subtitle_info;
			break;
		case SUB_HEIGHT:
			subtitle_height = states->subtitle_info;
			break;
		case SUB_TYPE:
			subtitle_type = states->subtitle_info;
			break;
		case SUB_CURRENT:
			subtitle_current = states->subtitle_info;
			break;
		case SUB_INDEX:
			subtitle_index = states->subtitle_info;
			break;
		case SUB_WRITE_POS:
			subtitle_write_pos = states->subtitle_info;
			break;
		case SUB_START_PTS:
			subtitle_start_pts = states->subtitle_info;
			break;
		case SUB_FPS:
			subtitle_fps = states->subtitle_info;
			break;
		case SUB_SUBTYPE:
			subtitle_subtype = states->subtitle_info;
			break;
		case SUB_RESET:
			subtitle_reset = states->subtitle_info;
			break;
		case SUB_DATA_T_SIZE:
			subtitle_data[subtitle_write_pos].subtitle_size =
				states->subtitle_info;
			break;
		case SUB_DATA_T_DATA: {
			if (states->subtitle_info > 0) {
				subtitle_data[subtitle_write_pos].data =
					vmalloc((states->subtitle_info));
				if (subtitle_data[subtitle_write_pos].data)
					memcpy(
					subtitle_data[subtitle_write_pos].data,
					(char *)states->data,
					states->subtitle_info);
			}

			subtitle_write_pos++;
			if (subtitle_write_pos >= MAX_SUBTITLE_PACKET)
				subtitle_write_pos = 0;
		}
		break;
		default:
			break;
		}

	}

	break;
	default:
		break;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long amsub_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = amsubtitle_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static const struct file_operations amsubtitle_fops = {
	.owner = THIS_MODULE,
	.open = amsubtitle_open,
	.release = amsubtitle_release,
	.unlocked_ioctl = amsubtitle_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amsub_compat_ioctl,
#endif
};

static struct device *amsubtitle_dev;
static dev_t amsub_devno;
static struct class *amsub_clsp;
static struct cdev *amsub_cdevp;
#define AMSUBTITLE_DEVICE_COUNT 1

static void create_amsub_attrs(struct class *class)
{
	int i = 0;

	for (i = 0; subtitle_class_attrs[i].attr.name; i++) {
		if (class_create_file(class, &subtitle_class_attrs[i]) < 0)
			break;
	}
}

static void remove_amsub_attrs(struct class *class)
{
	int i = 0;

	for (i = 0; subtitle_class_attrs[i].attr.name; i++)
		class_remove_file(class, &subtitle_class_attrs[i]);
}

int subtitle_init(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&amsub_devno, 0, AMSUBTITLE_DEVICE_COUNT,
							  DEVICE_NAME);
	if (ret < 0) {
		pr_info("amsub: failed to alloc major number\n");
		ret = -ENODEV;
		return ret;
	}

	amsub_clsp = class_create(THIS_MODULE, DEVICE_CLASS_NAME);
	if (IS_ERR(amsub_clsp)) {
		ret = PTR_ERR(amsub_clsp);
		goto err1;
	}

	create_amsub_attrs(amsub_clsp);

	amsub_cdevp = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!amsub_cdevp) {
		/*pr_info("amsub: failed to allocate memory\n");*/
		ret = -ENOMEM;
		goto err2;
	}

	cdev_init(amsub_cdevp, &amsubtitle_fops);
	amsub_cdevp->owner = THIS_MODULE;
	/* connect the major/minor number to cdev */
	ret = cdev_add(amsub_cdevp, amsub_devno, AMSUBTITLE_DEVICE_COUNT);
	if (ret) {
		pr_info("amsub:failed to add cdev\n");
		goto err3;
	}

	amsubtitle_dev = device_create(amsub_clsp,
		NULL, MKDEV(MAJOR(amsub_devno), 0),
		NULL, DEVICE_NAME);

	if (IS_ERR(amsubtitle_dev)) {
		pr_err("## Can't create amsubtitle device\n");
		goto err4;
	}

	return 0;

err4:
	cdev_del(amsub_cdevp);
err3:
	kfree(amsub_cdevp);
err2:
	remove_amsub_attrs(amsub_clsp);
	class_destroy(amsub_clsp);
err1:
	unregister_chrdev_region(amsub_devno, 1);

	return ret;
}
EXPORT_SYMBOL(subtitle_init);

void subtitle_exit(void)
{
	unregister_chrdev_region(amsub_devno, 1);
	device_destroy(amsub_clsp, MKDEV(MAJOR(amsub_devno), 0));
	cdev_del(amsub_cdevp);
	kfree(amsub_cdevp);
	remove_amsub_attrs(amsub_clsp);
	class_destroy(amsub_clsp);
}
EXPORT_SYMBOL(subtitle_exit);

