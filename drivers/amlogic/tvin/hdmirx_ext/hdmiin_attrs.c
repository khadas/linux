/*
 * drivers/amlogic/tvin/hdmirx_ext/hdmiin_attrs.c
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include "hdmiin_drv.h"

#include "hdmiin_hw_iface.h"
#include "hdmiin_attrs.h"
#include "vdin_iface.h"
#include "platform_iface.h"

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static ssize_t hdmiin_debug_print_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "hdmiin_debug_print: %d\n", hdmiin_debug_print);
}

static ssize_t hdmiin_debug_print_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int temp = 0;
	int ret;

	ret = sscanf(buf, "%d", &temp);
	if (ret == 1) {
		hdmiin_debug_print = temp;
		pr_info("set hdmiin_debug_print: %d\n", temp);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t hdmiin_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "todo: debug iface for read\n");
}

static ssize_t hdmiin_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	__hw_debug((char *)buf);

	return count;
}

static ssize_t hdmiin_port_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char port = 0xff;

	port = __hw_get_input_port();

	return sprintf(buf, "current hdmiin input port = %d\n", port);
}

static ssize_t hdmiin_port_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned char port = 0;
	unsigned long temp;
	int ret;

	ret = kstrtoul(buf, 10, &temp);
	port = (unsigned char)temp;

	if (port < 4) {
		__hw_set_input_port(port);
		HDMIIN_PD("set hdmiin input port = %d\n", port);
	}
	return count;
}

static ssize_t hdmiin_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hdmiin_drv_s *hdrv = hdmiin_get_driver();

	return sprintf(buf, "hdmiin tvin enable = %d\n", hdrv->user_cmd);
}

static ssize_t hdmiin_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmiin_drv_s *hdrv = hdmiin_get_driver();
	int argn = 0;
	char *p = NULL, *para = NULL;
	char *argv[5] = {NULL, NULL, NULL, NULL, NULL};
	char str[3] = {' ', '\n', '\0'};
	unsigned int mode = 0, enable = 0;
	/*unsigned int height = 0, width = 0, frame_rate = 0, field_flag = 0;*/

	p = kstrdup(buf, GFP_KERNEL);
	while (1) {
		para = strsep(&p, str);
		if (para == NULL)
			break;
		if (*para == '\0')
			continue;
		argv[argn++] = para;
	}

	/*pr_info("argn = %d, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"\n",
		argn, argv[0], argv[1], argv[2], argv[3], argv[4]);*/
	if (!strcmp(argv[0], "0")) { /* disable */
		enable = 0;
	} else if (!strcmp(argv[0], "1")) {
		/* enable, driver will trigger to vdin-stop */
		enable = 1;
	} else if (!strcmp(argv[0], "2")) {
		/* enable, driver will trigger to vdin-start */
		enable = 2;
	} else if (!strcmp(argv[0], "3")) {
		/* enable, driver will trigger to vdin-start/vdin-stop */
		enable = 3;
	} else if (!strcmp(argv[0], "4")) {
		/* enable, driver will not trigger to vdin-start/vdin-stop */
		enable = 4;
	} else {
		/* enable, driver will trigger specified mode */
		enable = 0xff;
	}

	hdrv->user_cmd = enable;

	/*if ((enable==1) && (argn!=5) && (argn!=1)) {
		pr_info("invalid parameters to enable cmd !\n");
		kfree(p);
		return count;
	}*/

	if ((enable == 0) && (hdrv->vdin.started == 1)) {
		hdmiin_stop_vdin(&(hdrv->vdin));
		pr_info("hdmiin disable dvin\n");
	} else if (((enable == 1) || (enable == 2) ||
		(enable == 3) || (enable == 4)) && (hdrv->vdin.started == 0)) {
		mode = __hw_get_video_mode();
		hdmiin_start_vdin_mode(mode);
		pr_info("hdmiin enable(0x%x) dvin, mode = %d\n", enable, mode);
	} else if ((enable == 0xff) && (hdrv->vdin.started == 0)) {
		mode = __plat_get_video_mode_by_name(argv[0]);
		hdmiin_start_vdin_mode(mode);
		pr_info("hdmiin enable(0x%x) dvin, mode = %d\n", enable, mode);
	} else {
		pr_info("error: enable=0x%x, mode=%d, vdin_started=%d\n",
			enable, mode, hdrv->vdin.started);
	}

	kfree(p);
	return count;
}

static ssize_t hdmiin_video_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int mode = 0xff;
	char hdmi_mode_str[16], *mode_str;
	unsigned char value;

	value = __hw_is_hdmi_mode();

	memset(hdmi_mode_str, 0x00, 16);

	strcpy(hdmi_mode_str, (value == 0) ? "DVI:" : "HDMI:");
	mode = __hw_get_video_mode();

	mode_str = __plat_get_video_mode_name(mode);

	if (strcmp(mode_str, "invalid") != 0)
		strcat(hdmi_mode_str, mode_str);
	else
		strcpy(hdmi_mode_str, mode_str);
	return sprintf(buf, "%s\n", hdmi_mode_str);
}

static ssize_t hdmiin_cable_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hdmiin_drv_s *hdrv = hdmiin_get_driver();

	hdrv->status.cable = __hw_get_cable_status();
	return sprintf(buf, "%d\n", hdrv->status.cable);
}

static ssize_t hdmiin_signal_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hdmiin_drv_s *hdrv = hdmiin_get_driver();

	hdrv->status.signal = __hw_get_cable_status();
	return sprintf(buf, "%d\n", hdrv->status.signal);
}

static ssize_t hdmiin_audio_sr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int audio_sr;
	char *audio_sr_array[] = {
		"44.1 kHz",		/* 0x0 */
		"Not indicated",	/* 0x1 */
		"48 kHz",		/* 0x2 */
		"32 kHz",		/* 0x3 */
		"22.05 kHz",		/* 0x4 */
		"reserved",		/* 0x5 */
		"24 kHz",		/* 0x6 */
		"reserved",		/* 0x7 */
		"88.2 kHz",		/* 0x8 */
		"768 kHz (192*4)",	/* 0x9 */
		"96 kHz",		/* 0xa */
		"reserved",		/* 0xb */
		"176.4 kHz",		/* 0xc */
		"reserved",		/* 0xd */
		"192 kHz",		/* 0xe */
		"reserved"		/* 0xf */
	};

	audio_sr = __hw_get_audio_sample_rate();
	if ((audio_sr < 0) || (audio_sr > sizeof(audio_sr_array)))
		audio_sr = 0;

	return sprintf(buf, "%s\n", audio_sr_array[audio_sr]);
}

static DEVICE_ATTR(print,		S_IRUGO|S_IWUSR,
		hdmiin_debug_print_show,	hdmiin_debug_print_store);
static DEVICE_ATTR(debug,		S_IRUGO|S_IWUSR,
		hdmiin_debug_show,		hdmiin_debug_store);
static DEVICE_ATTR(port,		S_IRUGO|S_IWUSR,
		hdmiin_port_show,		hdmiin_port_store);
static DEVICE_ATTR(enable,		S_IRUGO|S_IWUSR,
		hdmiin_enable_show,		hdmiin_enable_store);
static DEVICE_ATTR(video_mode,		S_IRUGO,
		hdmiin_video_mode_show,		NULL);
static DEVICE_ATTR(cable_status,	S_IRUGO,
		hdmiin_cable_status_show,	NULL);
static DEVICE_ATTR(signal_status,	S_IRUGO,
		hdmiin_signal_status_show,	NULL);
static DEVICE_ATTR(audio_sample_rate,	S_IRUGO,
		hdmiin_audio_sr_show,		NULL);

int hdmiin_create_device_attrs(struct device *pdevice)
{
	int ret = -1;

	ret = device_create_file(pdevice, &dev_attr_print);
	ret = device_create_file(pdevice, &dev_attr_debug);
	ret |= device_create_file(pdevice, &dev_attr_port);
	ret |= device_create_file(pdevice, &dev_attr_enable);
	ret |= device_create_file(pdevice, &dev_attr_video_mode);
	ret |= device_create_file(pdevice, &dev_attr_cable_status);
	ret |= device_create_file(pdevice, &dev_attr_signal_status);
	ret |= device_create_file(pdevice, &dev_attr_audio_sample_rate);

	return ret;
}

void hdmiin_destroy_device_attrs(struct device *pdevice)
{
	device_remove_file(pdevice, &dev_attr_print);
	device_remove_file(pdevice, &dev_attr_debug);
	device_remove_file(pdevice, &dev_attr_port);
	device_remove_file(pdevice, &dev_attr_enable);
	device_remove_file(pdevice, &dev_attr_video_mode);
	device_remove_file(pdevice, &dev_attr_cable_status);
	device_remove_file(pdevice, &dev_attr_signal_status);
	device_remove_file(pdevice, &dev_attr_audio_sample_rate);

	return;
}

