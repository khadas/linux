/*
 * drivers/amlogic/display/vout/vout_serve.c
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


/* Linux Headers */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/of.h>
#include <linux/major.h>
#include <linux/uaccess.h>

/* Amlogic Headers */
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>

/* Local Headers */
#include "vout_log.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend early_suspend;
static int early_suspend_flag;
#endif
#ifdef CONFIG_SCREEN_ON_EARLY
static int early_resume_flag;
#endif

#define VOUT_CDEV_NAME  "display"
#define VOUT_CLASS_NAME "display"
#define	MAX_NUMBER_PARA 10

static struct class *vout_class;
static DEFINE_MUTEX(vout_mutex);
static char vout_mode_uboot[64] __nosavedata;
static char vout_mode[64] __nosavedata;
static char vout_axis[64] __nosavedata;
static u32 vout_init_vmode = VMODE_INIT_NULL;
static int uboot_display;

struct vout_cdev_s {
	dev_t           devno;
	struct cdev     cdev;
	struct device   *dev;
};

static struct vout_cdev_s *vout_cdev;

void update_vout_mode_attr(const struct vinfo_s *vinfo)
{
	if (vinfo == NULL) {
		pr_info("error vinfo is null\n");
		return;
	} else
		pr_info("vinfo mode is: %s\n", vinfo->name);
	snprintf(vout_mode, 40, "%s", vinfo->name);
}

static int set_vout_mode(char *name);
static void set_vout_axis(char *axis);
static ssize_t mode_show(struct class *class, struct class_attribute *attr,
			 char *buf);
static ssize_t mode_store(struct class *class, struct class_attribute *attr,
			  const char *buf, size_t count);
static ssize_t axis_show(struct class *class, struct class_attribute *attr,
			 char *buf);
static ssize_t axis_store(struct class *class, struct class_attribute *attr,
			  const char *buf, size_t count);
static ssize_t fr_policy_show(struct class *class,
		struct class_attribute *attr, char *buf);
static ssize_t fr_policy_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count);
static CLASS_ATTR(mode, S_IWUSR | S_IRUGO, mode_show, mode_store);
static CLASS_ATTR(axis, S_IWUSR | S_IRUGO, axis_show, axis_store);
static CLASS_ATTR(fr_policy, S_IWUSR | S_IRUGO,
		fr_policy_show, fr_policy_store);

static ssize_t mode_show(struct class *class, struct class_attribute *attr,
			 char *buf)
{
	int ret = 0;

	ret = snprintf(buf, 64, "%s\n", vout_mode);

	return ret;
}

static ssize_t mode_store(struct class *class, struct class_attribute *attr,
			  const char *buf, size_t count)
{
	char mode[64];

	mutex_lock(&vout_mutex);
	snprintf(mode, 64, "%s", buf);
	if (set_vout_mode(mode) == 0)
		strcpy(vout_mode, mode);
	mutex_unlock(&vout_mutex);
	return count;
}

static ssize_t axis_show(struct class *class, struct class_attribute *attr,
			 char *buf)
{
	int ret = 0;
	ret = snprintf(buf, 64, "%s\n", vout_axis);
	return ret;
}

static ssize_t axis_store(struct class *class, struct class_attribute *attr,
			  const char *buf, size_t count)
{
	mutex_lock(&vout_mutex);
	snprintf(vout_axis, 64, "%s", buf);
	set_vout_axis(vout_axis);
	mutex_unlock(&vout_mutex);
	return count;
}

static ssize_t fr_policy_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int policy;
	int ret = 0;

	policy = get_vframe_rate_policy();
	ret = sprintf(buf, "%d\n", policy);

	return ret;
}

static ssize_t fr_policy_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int policy;
	int ret = 0;

	mutex_lock(&vout_mutex);
	ret = sscanf(buf, "%d", &policy);
	if (ret == 1) {
		ret = set_vframe_rate_policy(policy);
		if (ret)
			pr_info("%s: %d failed\n", __func__, policy);
	} else {
		pr_info("%s: invalid data\n", __func__);
		return -EINVAL;
	}
	mutex_unlock(&vout_mutex);
	return count;
}

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token)
				|| !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if ((!token) || (*token == '\n') || (len == 0))
			break;
		ret = kstrtoint(token, 0, &res);
		if (ret < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

#ifdef CONFIG_PM
static int  meson_vout_suspend(struct platform_device *pdev,
			       pm_message_t state);
static int  meson_vout_resume(struct platform_device *pdev);
#endif

static char local_name[32] = {0};

static int set_vout_mode(char *name)
{
	struct hdmitx_dev *hdmitx_device = get_hdmitx_device();
	enum vmode_e mode;
	int ret = 0;

	vout_log_info("vmode set to %s\n", name);

	if (strcmp(name, local_name)) {
		memset(local_name, 0, sizeof(local_name));
		strcpy(local_name, name);
		mode = validate_vmode(name);
		goto next;
	} else {
		vout_log_info("don't set the same mode as current\n");
		return -1;
	}

	mode = validate_vmode(name);
	if (VMODE_MAX == mode) {
		vout_log_info("no matched vout mode\n");
		return -1;
	}
next:
	if (hdmitx_device->hdtx_dev) {
		/*
		 * On kernel booting, there happens call phy_pll_off() a time,
		 * this will cause hdmitx disable output for a short while.
		 * So, just using flag to prevent the first error call.
		 * [    5.038667@0] vout_serve: vmode set to 1080p60hz
		 * [    5.038711@0] vout_serve: disable HDMI PHY as soon as
		*/
		static int flag = 1;
		if (flag == 1)
			flag = 0;
		else {
			phy_pll_off();
			vout_log_info("disable HDMI PHY as soon as possible\n");
		}
	}
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	ret = set_current_vmode(mode);
	if (ret)
		vout_log_info("new mode %s set error\n", name);
	else
		vout_log_info("new mode %s set ok\n", name);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);
	return ret;
}

static int set_vout_init_mode(void)
{
	enum vmode_e vmode;
	int ret = 0;

	vout_init_vmode = validate_vmode(vout_mode_uboot);
	if (vout_init_vmode >= VMODE_MAX) {
		vout_log_info("no matched vout_init mode %s\n",
			vout_mode_uboot);
		return -1;
	}
#if 1
	if (uboot_display)
		vmode = vout_init_vmode | VMODE_INIT_BIT_MASK;
	else
		vmode = vout_init_vmode;
#else
	/* force to init display, for clk-gating disabled */
	vmode = vout_init_vmode;
#endif

	memset(local_name, 0, sizeof(local_name));
	strcpy(local_name, vout_mode_uboot);
	ret = set_current_vmode(vmode);
	vout_log_info("init mode %s\n", vout_mode_uboot);

	return ret;
}

enum vmode_e get_logo_vmode(void)
{
	return vout_init_vmode;
}
EXPORT_SYMBOL(get_logo_vmode);

int set_logo_vmode(enum vmode_e mode)
{
	const char *tmp;
	char name[32] = {0};
	int ret = 0;

	if (mode == VMODE_INIT_NULL)
		return -1;
	tmp = vmode_mode_to_name(mode);
	strncpy(&name[0], tmp, 32);
	vout_init_vmode = mode;
	set_vout_mode(name);

	return ret;
}
EXPORT_SYMBOL(set_logo_vmode);

#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
void update_vmode_status(char *name)
{
	snprintf(vout_mode, 64, "%s\n", name);
}
EXPORT_SYMBOL(update_vmode_status);
#endif

char *get_vout_mode_internal(void)
{
	return vout_mode;
}
EXPORT_SYMBOL(get_vout_mode_internal);

char *get_vout_mode_uboot(void)
{
	return vout_mode_uboot;
}
EXPORT_SYMBOL(get_vout_mode_uboot);

static void set_vout_axis(char *para)
{
#define OSD_COUNT 2
	static struct disp_rect_s disp_rect[OSD_COUNT];
	char count = OSD_COUNT * 4;
	int *pt = &disp_rect[0].x;
	int parsed[MAX_NUMBER_PARA] = {};

	/* parse window para */
	if (parse_para(para, 8, parsed) >= 4)
		memcpy(pt, parsed, sizeof(struct disp_rect_s) * OSD_COUNT);
	if ((count >= 4) && (count < 8))
		disp_rect[1] = disp_rect[0];

	vout_log_info("osd0=> x:%d,y:%d,w:%d,h:%d\nosd1=> x:%d,y:%d,w:%d,h:%d\n",
			*pt, *(pt + 1), *(pt + 2), *(pt + 3),
			*(pt + 4), *(pt + 5), *(pt + 6), *(pt + 7));
	vout_notifier_call_chain(VOUT_EVENT_OSD_DISP_AXIS, &disp_rect[0]);
}

static ssize_t vout_attr_vinfo_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	const struct vinfo_s *info = NULL;
	ssize_t len = 0;

	info = get_current_vinfo();
	if (info == NULL) {
		pr_info("current vinfo is null\n");
		return sprintf(buf, "\n");
	}

	len = sprintf(buf, "current vinfo:\n"
		"    name:                  %s\n"
		"    mode:                  %d\n"
		"    width:                 %d\n"
		"    height:                %d\n"
		"    field_height:          %d\n"
		"    aspect_ratio_num:      %d\n"
		"    aspect_ratio_den:      %d\n"
		"    sync_duration_num:     %d\n"
		"    sync_duration_den:     %d\n"
		"    screen_real_width:     %d\n"
		"    screen_real_height:    %d\n"
		"    video_clk:             %d\n"
		"    viu_color_fmt:         %d\n\n",
		info->name, info->mode,
		info->width, info->height, info->field_height,
		info->aspect_ratio_num, info->aspect_ratio_den,
		info->sync_duration_num, info->sync_duration_den,
		info->screen_real_width, info->screen_real_height,
		info->video_clk, info->viu_color_fmt);
	len += sprintf(buf+len, "hdr_info:\n"
		"    present_flag          %d\n"
		"    features              0x%x\n"
		"    primaries             0x%x, 0x%x\n"
		"                          0x%x, 0x%x\n"
		"                          0x%x, 0x%x\n"
		"    white_point           0x%x, 0x%x\n"
		"    luminance             %d, %d\n\n",
		info->master_display_info.present_flag,
		info->master_display_info.features,
		info->master_display_info.primaries[0][0],
		info->master_display_info.primaries[0][1],
		info->master_display_info.primaries[1][0],
		info->master_display_info.primaries[1][1],
		info->master_display_info.primaries[2][0],
		info->master_display_info.primaries[2][1],
		info->master_display_info.white_point[0],
		info->master_display_info.white_point[1],
		info->master_display_info.luminance[0],
		info->master_display_info.luminance[1]);
	return len;
}

static struct  class_attribute  class_attr_vinfo =
	__ATTR(vinfo, S_IRUGO|S_IWUSR|S_IWGRP, vout_attr_vinfo_show, NULL);

static int create_vout_attr(void)
{
	int ret = 0;
	struct vinfo_s *init_mode;
	/* create vout class */
	vout_class = class_create(THIS_MODULE, VOUT_CLASS_NAME);

	if (IS_ERR(vout_class)) {
		vout_log_err("create vout class fail\n");
		return  -1;
	}

	/* create vout class attr files */
	ret = class_create_file(vout_class, &class_attr_mode);
	if (ret != 0)
		vout_log_err("create class attr mode failed!\n");

	ret = class_create_file(vout_class, &class_attr_axis);
	if (ret != 0)
		vout_log_err("create class attr axis failed!\n");

	ret = class_create_file(vout_class, &class_attr_fr_policy);
	if (ret != 0)
		vout_log_err("create class attr fr_policy failed!\n");

	ret = class_create_file(vout_class, &class_attr_vinfo);
	if (ret != 0)
		vout_log_err("create class attr vinfo failed!\n");

	/*
	 * init /sys/class/display/mode
	 */
	init_mode = (struct vinfo_s *)get_current_vinfo();

	if (init_mode)
		strcpy(vout_mode, init_mode->name);

	return ret;
}

/* ************************************************************* */
/* vout ioctl                                                    */
/* ************************************************************* */
static int vout_io_open(struct inode *inode, struct file *file)
{
	struct vout_cdev_s *vcdev;

	vout_log_info("%s\n", __func__);
	vcdev = container_of(inode->i_cdev, struct vout_cdev_s, cdev);
	file->private_data = vcdev;
	return 0;
}

static int vout_io_release(struct inode *inode, struct file *file)
{
	vout_log_info("%s\n", __func__);
	file->private_data = NULL;
	return 0;
}

static long vout_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp;
	int mcd_nr;
	struct vinfo_s *info = NULL;

	mcd_nr = _IOC_NR(cmd);
	vout_log_info("%s: cmd_dir = 0x%x, cmd_nr = 0x%x\n",
		__func__, _IOC_DIR(cmd), mcd_nr);

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case VOUT_IOC_NR_GET_VINFO:
		info = get_current_vinfo();
		if (info == NULL)
			ret = -EFAULT;
		else if (info->mode == VMODE_INIT_NULL)
			ret = -EFAULT;
		else if (copy_to_user(argp, info, sizeof(struct vinfo_s)))
			ret = -EFAULT;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long vout_compat_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vout_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations vout_fops = {
	.owner          = THIS_MODULE,
	.open           = vout_io_open,
	.release        = vout_io_release,
	.unlocked_ioctl = vout_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = vout_compat_ioctl,
#endif
};

static int create_vout_fops(void)
{
	int ret = 0;

	vout_cdev = kmalloc(sizeof(struct vout_cdev_s), GFP_KERNEL);
	if (!vout_cdev) {
		vout_log_info("error: failed to allocate vout_cdev\n");
		return -1;
	}

	ret = alloc_chrdev_region(&vout_cdev->devno, 0, 1, VOUT_CDEV_NAME);
	if (ret < 0) {
		vout_log_info("error: faild to alloc devno\n");
		kfree(vout_cdev);
		vout_cdev = NULL;
		return -1;
	}

	cdev_init(&vout_cdev->cdev, &vout_fops);
	vout_cdev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&vout_cdev->cdev, vout_cdev->devno, 1);
	if (ret) {
		vout_log_info("error: failed to add device\n");
		unregister_chrdev_region(vout_cdev->devno, 1);
		kfree(vout_cdev);
		vout_cdev = NULL;
		return -1;
	}

	vout_cdev->dev = device_create(vout_class, NULL, vout_cdev->devno,
			NULL, VOUT_CDEV_NAME);
	if (IS_ERR(vout_cdev->dev)) {
		ret = PTR_ERR(vout_cdev->dev);
		cdev_del(&vout_cdev->cdev);
		unregister_chrdev_region(vout_cdev->devno, 1);
		kfree(vout_cdev);
		vout_cdev = NULL;
		return -1;
	}

	vout_log_info("%s OK\n", __func__);
	return 0;
}
/* ************************************************************* */

#ifdef CONFIG_PM
static int  meson_vout_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifdef CONFIG_HAS_EARLYSUSPEND

	if (early_suspend_flag)
		return 0;

#endif
	vout_suspend();
	return 0;
}

static int  meson_vout_resume(struct platform_device *pdev)
{
#ifdef CONFIG_SCREEN_ON_EARLY

	if (early_resume_flag) {
		early_resume_flag = 0;
		return 0;
	}

#endif
#ifdef CONFIG_HAS_EARLYSUSPEND

	if (early_suspend_flag)
		return 0;

#endif
	vout_resume();
	return 0;
}
#endif

#ifdef CONFIG_HIBERNATION
static int  meson_vout_freeze(struct device *dev)
{
	return 0;
}

static int  meson_vout_thaw(struct device *dev)
{
	return 0;
}

static int  meson_vout_restore(struct device *dev)
{
	enum vmode_e mode;
	mode = get_current_vmode();
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	return 0;
}
static int meson_vout_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	return meson_vout_suspend(pdev, PMSG_SUSPEND);
}
static int meson_vout_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	return meson_vout_resume(pdev);
}
#endif

#ifdef CONFIG_SCREEN_ON_EARLY
void resume_vout_early(void)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend_flag = 0;
	early_resume_flag = 1;
	vout_resume();
#endif
	return;
}
EXPORT_SYMBOL(resume_vout_early);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void meson_vout_early_suspend(struct early_suspend *h)
{
	if (early_suspend_flag)
		return;

	vout_suspend();
	early_suspend_flag = 1;
}

static void meson_vout_late_resume(struct early_suspend *h)
{
	if (!early_suspend_flag)
		return;

	early_suspend_flag = 0;
	vout_resume();
}
#endif

/*****************************************************************
**
**	vout driver interface
**
******************************************************************/
static int meson_vout_probe(struct platform_device *pdev)
{
	int ret = -1;

#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	early_suspend.suspend = meson_vout_early_suspend;
	early_suspend.resume = meson_vout_late_resume;
	register_early_suspend(&early_suspend);
#endif

	set_vout_init_mode();

	vout_class = NULL;
	ret = create_vout_attr();
	ret = create_vout_fops();

	if (ret == 0)
		vout_log_info("create vout attribute OK\n");
	else
		vout_log_info("create vout attribute FAILED\n");

	vout_log_info("%s OK\n", __func__);
	return ret;
}

static int meson_vout_remove(struct platform_device *pdev)
{
	if (vout_class == NULL)
		return -1;

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend);
#endif
	class_remove_file(vout_class, &class_attr_mode);
	class_remove_file(vout_class, &class_attr_axis);
	class_remove_file(vout_class, &class_attr_fr_policy);
	class_remove_file(vout_class, &class_attr_vinfo);
	class_destroy(vout_class);

	cdev_del(&vout_cdev->cdev);
	unregister_chrdev_region(vout_cdev->devno, 1);
	kfree(vout_cdev);
	vout_cdev = NULL;

	return 0;
}

static const struct of_device_id meson_vout_dt_match[] = {
	{ .compatible = "amlogic, meson-vout",},
	{ },
};

#ifdef CONFIG_HIBERNATION
const struct dev_pm_ops vout_pm = {
	.freeze		= meson_vout_freeze,
	.thaw		= meson_vout_thaw,
	.restore	= meson_vout_restore,
	.suspend	= meson_vout_pm_suspend,
	.resume		= meson_vout_pm_resume,
};
#endif

static struct platform_driver
	vout_driver = {
	.probe      = meson_vout_probe,
	.remove     = meson_vout_remove,
#ifdef CONFIG_PM
	.suspend  = meson_vout_suspend,
	.resume    = meson_vout_resume,
#endif
	.driver = {
		.name = "meson-vout",
		.of_match_table = meson_vout_dt_match,
#ifdef CONFIG_HIBERNATION
	.pm	= &vout_pm,
#endif
	}
};

static int __init vout_init_module(void)
{
	int ret = 0;
	vout_log_info("%s\n", __func__);

	if (platform_driver_register(&vout_driver)) {
		vout_log_err("failed to register VOUT driver\n");
		ret = -ENODEV;
	}

	return ret;
}

static __exit void vout_exit_module(void)
{
	vout_log_info("%s\n", __func__);
	platform_driver_unregister(&vout_driver);
}

subsys_initcall(vout_init_module);
module_exit(vout_exit_module);

static int str2lower(char *str)
{
	while (*str != '\0') {
		*str = tolower(*str);
		str++;
	}
	return 0;
}

static void vout_init_mode_parse(char *str)
{
	/*enum vmode_e vmode;*/

	/* detect vout mode */
	if (strlen(str) <= 1) {
		vout_log_info("%s: error\n", str);
		return;
	}

	/* detect uboot display */
	if (strncmp(str, "en", 2) == 0) { /* enable */
		uboot_display = 1;
		vout_log_info("%s: %d\n", str, uboot_display);
		return;
	} else if (strncmp(str, "dis", 3) == 0) { /* disable */
		uboot_display = 0;
		vout_log_info("%s: %d\n", str, uboot_display);
		return;
	}

	/* just save the vmode_name,
	convert to vmode when vout sever registered */
	strcpy(vout_mode_uboot, str);
	vout_log_info("%s\n", str);
	/*vmode = vmode_name_to_mode(str);
	if (vmode < VMODE_MAX) {
		vout_init_vmode = vmode;
		vout_log_info("%s: %d\n", str, vout_init_vmode);
		return;
	}*/
	return;

	vout_log_info("%s: error\n", str);
}

static int __init get_vout_init_mode(char *str)
{
	char *ptr = str;
	char sep[2];
	char *option;
	int count = 3;
	char find = 0;

	/* init void vout_mode_uboot name */
	memset(vout_mode_uboot, 0, sizeof(vout_mode_uboot));

	if (NULL == str)
		return -EINVAL;

	do {
		if (!isalpha(*ptr) && !isdigit(*ptr)) {
			find = 1;
			break;
		}
	} while (*++ptr != '\0');
	if (!find)
		return -EINVAL;

	sep[0] = *ptr;
	sep[1] = '\0';
	while ((count--) && (option = strsep(&str, sep))) {
		/* vout_log_info("%s\n", option); */
		str2lower(option);
		vout_init_mode_parse(option);
	}

	return 0;
}
__setup("vout=", get_vout_init_mode);

MODULE_AUTHOR("Platform-BJ <platform.bj@amlogic.com>");
MODULE_DESCRIPTION("VOUT Server Module");
MODULE_LICENSE("GPL");
