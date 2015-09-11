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


#define VOUT_CLASS_NAME "display"
#define	MAX_NUMBER_PARA 10

static struct class *vout_class;
static DEFINE_MUTEX(vout_mutex);
static char vout_mode[64];
static char vout_axis[64];
static u32 vout_logo_vmode = VMODE_INIT_NULL;


static void set_vout_mode(char *name);
static void set_vout_axis(char *axis);
static ssize_t mode_show(struct class *class, struct class_attribute *attr,
			 char *buf);
static ssize_t mode_store(struct class *class, struct class_attribute *attr,
			  const char *buf, size_t count);
static ssize_t axis_show(struct class *class, struct class_attribute *attr,
			 char *buf);
static ssize_t axis_store(struct class *class, struct class_attribute *attr,
			  const char *buf, size_t count);
static CLASS_ATTR(mode, S_IWUSR | S_IRUGO, mode_show, mode_store);
static CLASS_ATTR(axis, S_IWUSR | S_IRUGO, axis_show, axis_store);

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
	mutex_lock(&vout_mutex);
	snprintf(vout_mode, 64, "%s", buf);
	set_vout_mode(vout_mode);
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

static void set_vout_mode(char *name)
{
	enum vmode_e mode;
	vout_log_info("vmode set to %s\n", name);
	mode = validate_vmode(name);

	if (VMODE_MAX == mode) {
		vout_log_info("no matched vout mode\n");
		return;
	}

	if (mode == get_current_vmode()) {
		vout_log_info("don't set the same mode as current.\n");
		return;
	}
	phy_pll_off();
	vout_log_info("disable HDMI PHY as soon as possible\n");
	set_current_vmode(mode);
	vout_log_info("new mode %s set ok\n", name);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);
}

enum vmode_e get_logo_vmode(void)
{
	return vout_logo_vmode;
}
EXPORT_SYMBOL(get_logo_vmode);

int set_logo_vmode(enum vmode_e mode)
{
	int ret = 0;

	if (mode == VMODE_INIT_NULL)
		return -1;

	vout_logo_vmode = mode;
	set_current_vmode(mode);

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
		vout_log_err("create class attr failed!\n");

	ret = class_create_file(vout_class, &class_attr_axis);

	if (ret != 0)
		vout_log_err("create class attr failed!\n");

	/*
	 * init /sys/class/display/mode
	 */
	init_mode = (struct vinfo_s *)get_current_vinfo();

	if (init_mode)
		strcpy(vout_mode, init_mode->name);

	return ret;
}

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
	vout_log_info("%s\n", __func__);
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	early_suspend.suspend = meson_vout_early_suspend;
	early_suspend.resume = meson_vout_late_resume;
	register_early_suspend(&early_suspend);
#endif
	vout_class = NULL;
	ret = create_vout_attr();

	if (ret == 0)
		vout_log_info("create vout attribute OK\n");
	else
		vout_log_info("create vout attribute FAILED\n");

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
	class_destroy(vout_class);
	return 0;
}

static const struct of_device_id meson_vout_dt_match[] = {
	{ .compatible = "amlogic, meson-vout",},
	{ },
};

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

module_init(vout_init_module);
module_exit(vout_exit_module);

MODULE_AUTHOR("Platform-BJ <platform.bj@amlogic.com>");
MODULE_DESCRIPTION("VOUT Server Module");
MODULE_LICENSE("GPL");
