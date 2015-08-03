/*
 * drivers/amlogic/display/vout2/vout2_serve.c
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>

/* Amlogic Headers */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/vout/vout_notify.h>

/* Local Headers */
#include "vout/vout_log.h"
#include "vout/vout_io.h"
#include "vout/vout_reg.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
/* static struct early_suspend early_suspend; */
/* static int early_suspend_flag = 0; */
#endif


#define VOUT_CLASS_NAME "display2"
#define	MAX_NUMBER_PARA 10

static struct class *vout_class;
static DEFINE_MUTEX(vout_mutex);
static char vout_mode[64];
static char vout_axis[64];

static int s_venc_mux;

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
static ssize_t venc_mux_show(struct class *class, struct class_attribute *attr,
			     char *buf);
static ssize_t venc_mux_store(struct class *class, struct class_attribute *attr,
			      const char *buf, size_t count);
static CLASS_ATTR(mode, S_IWUSR | S_IRUGO, mode_show, mode_store);
static CLASS_ATTR(axis, S_IWUSR | S_IRUGO, axis_show, axis_store);
static CLASS_ATTR(venc_mux, S_IWUSR | S_IRUGO, venc_mux_show, venc_mux_store);

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
		if (len == 0)
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

#if 0
#ifdef CONFIG_PM
static int  meson_vout_suspend(struct platform_device *pdev,
			       pm_message_t state);
static int  meson_vout_resume(struct platform_device *pdev);
#endif
#endif
static void set_vout_mode(char *name)
{
	enum vmode_e mode;

	vout_log_info("tvmode2 set to %s\n", name);
	mode = validate_vmode2(name);

	if (VMODE_MAX == mode) {
		vout_log_info("no matched vout2 mode\n");
		return;
	}

	if (mode == get_current_vmode2()) {
		vout_log_info("don't set the same mode as current.\n");
		return;
	}

	set_current_vmode2(mode);
	vout_log_info("new mode2 = %s set ok\n", name);
	vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);
}

void  set_vout2_mode_internal(char *name)
{
	set_vout_mode(name);
}
EXPORT_SYMBOL(set_vout2_mode_internal);

char *get_vout2_mode_internal(void)
{
	return vout_mode;
}
EXPORT_SYMBOL(get_vout2_mode_internal);

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

static const char *venc_mux_help = {
	"venc_mux:\n"
	"    0. single display, viu1->panel, viu2->null\n"
	"    1. dual display, viu1->cvbs, viu2->panel\n"
	"    2. dual display, viu1->hdmi, viu2->panel\n"
	"    4. single display, viu1->null, viu2->hdmi\n"
	"    8. dual display, viu1->panel, viu2->hdmi\n"
};

static ssize_t venc_mux_show(struct class *class, struct class_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "%s\ncurrent venc_mux: %d\n",
			venc_mux_help, s_venc_mux);
}

static ssize_t venc_mux_store(struct class *class, struct class_attribute *attr,
			      const char *buf, size_t count)
{
	unsigned int mux_type = 0;
	unsigned int mux = 0;
	unsigned long res = 0;
	int ret = 0;

	ret = kstrtoul(buf, 0, &res);
	mux = res;

	switch (mux) {
	case 0x0:
		mux_type = s_venc_mux;
		vout_vcbus_set_bits(VPU_VIU_VENC_MUX_CTRL, mux_type, 0, 4);
		break;

	case 0x1:
		mux_type = mux | (s_venc_mux << 2);
		vout_vcbus_set_bits(VPU_VIU_VENC_MUX_CTRL, mux_type, 0, 4);
		break;

	case 0x2:
		mux_type = mux | (s_venc_mux << 2);
		vout_vcbus_set_bits(VPU_VIU_VENC_MUX_CTRL, mux_type, 0, 4);
		break;

	case 0x4:
		mux_type = (0x2 << 2);
		vout_vcbus_set_bits(VPU_VIU_VENC_MUX_CTRL, mux_type, 0, 4);
		break;

	case 0x8:
		mux_type = (0x2 << 2) | s_venc_mux;
		vout_vcbus_set_bits(VPU_VIU_VENC_MUX_CTRL, mux_type, 0, 4);
		break;

	default:
		vout_log_err("set venc_mux error\n");
		break;
	}

	vout_log_info("set venc_mux mux_type is %d\n", mux_type);
	return count;
}

static int create_vout_attr(void)
{
	int ret = 0;

	/* create vout2 class */
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

	ret = class_create_file(vout_class, &class_attr_venc_mux);

	if (ret != 0)
		vout_log_err("create class attr failed!\n");

	return ret;
}

#if 0
#ifdef CONFIG_PM
static int  meson_vout_suspend(struct platform_device *pdev, pm_message_t state)
{
#if 0 /* def CONFIG_HAS_EARLYSUSPEND */

	if (early_suspend_flag)
		return 0;

#endif
	vout2_suspend();
	return 0;
}

static int  meson_vout_resume(struct platform_device *pdev)
{
#if 0 /* def CONFIG_HAS_EARLYSUSPEND */

	if (early_suspend_flag)
		return 0;

#endif
	vout2_resume();
	return 0;
}
#endif
#endif
#if 0 /* def CONFIG_HAS_EARLYSUSPEND */
static void meson_vout_early_suspend(struct early_suspend *h)
{
	if (early_suspend_flag)
		return 0;

	meson_vout_suspend((struct platform_device *)h->param, PMSG_SUSPEND);
	early_suspend_flag = 1;
}

static void meson_vout_late_resume(struct early_suspend *h)
{
	if (!early_suspend_flag)
		return 0;

	early_suspend_flag = 0;
	meson_vout_resume((struct platform_device *)h->param);
}
#endif

/*****************************************************************
**
**	vout driver interface
**
******************************************************************/
static int
meson_vout_probe(struct platform_device *pdev)
{
	int ret =  -1;

	vout_log_info("start init vout2 module\n");
#if 0 /* def CONFIG_HAS_EARLYSUSPEND */
	early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	early_suspend.suspend = meson_vout_early_suspend;
	early_suspend.resume = meson_vout_late_resume;
	early_suspend.param = pdev;
	register_early_suspend(&early_suspend);
#endif

	ret = create_vout_attr();
	s_venc_mux = vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL) & 0x3;

	if (ret == 0)
		vout_log_info("create vout2 attribute OK\n");
	else
		vout_log_err("create vout2 attribute FAILED\n");

#define VPP_OFIFO_SIZE_MASK         0xfff
#define VPP_OFIFO_SIZE_BIT          0

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		vout_vcbus_set_bits(VPP2_OFIFO_SIZE, 0x800,
				    VPP_OFIFO_SIZE_BIT, 13);
	} else {
		vout_vcbus_set_bits(VPP2_OFIFO_SIZE, 0x780,
				    VPP_OFIFO_SIZE_BIT, 12);
	}

	return ret;
}
static int
meson_vout_remove(struct platform_device *pdev)
{
	if (vout_class == NULL)
		return -1;

#if 0 /* def CONFIG_HAS_EARLYSUSPEND */
	unregister_early_suspend(&early_suspend);
#endif

	class_remove_file(vout_class, &class_attr_venc_mux);
	class_remove_file(vout_class, &class_attr_mode);
	class_remove_file(vout_class, &class_attr_axis);
	class_destroy(vout_class);

	return 0;
}


static const struct of_device_id meson_vout2_dt_match[] = {
	{ .compatible = "amlogic, meson-vout2",},
	{},
};

static struct platform_driver
	vout2_driver = {
	.probe      = meson_vout_probe,
	.remove     = meson_vout_remove,
#if 0 /* def  CONFIG_PM */
	.suspend = meson_vout_suspend,
	.resume = meson_vout_resume,
#endif
	.driver     = {
		.name   = "meson-vout2",
		.of_match_table = meson_vout2_dt_match,
	}
};
static int __init vout2_init_module(void)
{
	int ret = 0;
	vout_log_info("%s enter\n", __func__);

	if (platform_driver_register(&vout2_driver)) {
		vout_log_err("%s fail\n", __func__);
		vout_log_err("failed to register vout2 driver\n");
		ret = -ENODEV;
	}

	return ret;

}
static __exit void vout2_exit_module(void)
{

	vout_log_info("vout2_remove_module.\n");

	platform_driver_unregister(&vout2_driver);
}
module_init(vout2_init_module);
module_exit(vout2_exit_module);

MODULE_DESCRIPTION("vout2 serve  module");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("jianfeng_wang <jianfeng.wang@amlogic.com>");
