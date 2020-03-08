// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/ppmgr/ppmgr_drv.c
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

#include <linux/compat.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/ppmgr/ppmgr.h>
#include <linux/amlogic/media/ppmgr/ppmgr_status.h>
#include <linux/platform_device.h>
/*#include <linux/amlogic/ge2d/ge2d_main.h>*/
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/media/utils/amports_config.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include "ppmgr_log.h"
#include "ppmgr_pri.h"
#include "ppmgr_dev.h"

#define PPMGRDRV_INFO(fmt, args...) pr_info("PPMGRDRV: info: " fmt " ", ## args)
#define PPMGRDRV_DBG(fmt, args...) pr_debug("PPMGRDRV: dbg: " fmt " ", ## args)
#define PPMGRDRV_WARN(fmt, args...) pr_warn("PPMGRDRV: warn: " fmt " ", ## args)
#define PPMGRDRV_ERR(fmt, args...) pr_err("PPMGRDRV: err: " fmt " ", ## args)

/***********************************************************************
 *
 * global status.
 *
 ************************************************************************/
static int ppmgr_enable_flag;
static int ppmgr_flag_change;
static int property_change;
static int buff_change;

static int inited_ppmgr_num;
static enum platform_type_t platform_type = PLATFORM_MID;
/*static struct platform_device *ppmgr_core_device = NULL;*/
struct ppmgr_device_t ppmgr_device;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
static bool scaler_pos_reset;
#endif

static struct ppmgr_dev_reg_s ppmgr_dev_reg;

enum platform_type_t get_platform_type(void)
{
	return platform_type;
}

int get_bypass_mode(void)
{
	return ppmgr_device.bypass;
}

int get_property_change(void)
{
	return property_change;
}

void set_property_change(int flag)
{
	property_change = flag;
}

int get_buff_change(void)
{
	return buff_change;
}

void set_buff_change(int flag)
{
	buff_change = flag;
}

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
bool get_scaler_pos_reset(void)
{
	return scaler_pos_reset;
}

void set_scaler_pos_reset(bool flag)
{
	scaler_pos_reset = flag;
}
#endif

int get_ppmgr_status(void)
{
	return ppmgr_enable_flag;
}

void set_ppmgr_status(int flag)
{
	if (flag != ppmgr_enable_flag)
		ppmgr_flag_change = 1;

	if (flag >= 0)
		ppmgr_enable_flag = flag;
	else
		ppmgr_enable_flag = 0;
}

/***********************************************************************
 *
 * Utilities.
 *
 ************************************************************************/
static ssize_t _ppmgr_angle_store(unsigned long val)
{
	unsigned long angle = val;

	if (angle > 3) {
		if (angle == 90) {
			angle = 1;
		} else if (angle == 180) {
			angle = 2;
		} else if (angle == 270) {
			angle = 3;
		} else {
			PPMGRDRV_ERR("invalid orientation value\n");
			PPMGRDRV_ERR("you should set 0 for 0 clockwise\n");
			PPMGRDRV_ERR("1 or 90 for 90 clockwise\n");
			PPMGRDRV_ERR("2 or 180 for 180 clockwise\n");
			PPMGRDRV_ERR("3 or 270 for 270 clockwise\n");
			return -EINVAL;
		}
	}

	ppmgr_device.global_angle = angle;
	ppmgr_device.videoangle = (angle + ppmgr_device.orientation) % 4;
	if (!ppmgr_device.use_prot) {
		if (angle != ppmgr_device.angle) {
			property_change = 1;
			PPMGRDRV_INFO("ppmgr angle:%x\n", ppmgr_device.angle);
			PPMGRDRV_INFO("orient:%x\n", ppmgr_device.orientation);
			PPMGRDRV_INFO("vidangl:%x\n", ppmgr_device.videoangle);
		}
	} else {
		if (angle != ppmgr_device.angle) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
			set_video_angle(angle);
#endif
			PPMGRDRV_INFO("prot angle:%ld\n", angle);
		}
	}
	ppmgr_device.angle = angle;
	return 0;
}

/***********************************************************************
 *
 * class property info.
 *
 ************************************************************************/

#define	PPMGR_CLASS_NAME				"ppmgr"
static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	ssize_t len;
	int count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	if (!params) {
		PPMGRDRV_INFO("para kstrdup failed\n");
		return 0;
	}
	params_base = params;
	token = params;
	len = strlen(token);
	if (len <= 0) {
		PPMGRDRV_INFO("token is NULL\n");
		return 0;
	}
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token) || !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if (len == 0 || !token)
			break;
		ret = kstrtoint(token, 0, &res);
		if (ret < 0) {
			PPMGRDRV_ERR("ERR convert %s to long int!\n", token);
			break;
		}

		len = strlen(token);
		*out++ = res;
		count++;
	} while (token && count < para_num && len > 0);

	kfree(params_base);
	return count;
}

static ssize_t ppmgr_info_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	unsigned int bstart;
	unsigned int bsize;

	get_ppmgr_buf_info(&bstart, &bsize);
/* return snprintf(buf, 80, "buffer:\n start:%x.\tsize:%d\n", */
/* (unsigned int)bstart, bsize / (1024 * 1024)); */
	return snprintf(buf, 80, "buffer:\n start:%x.\tsize:%d\n",
			bstart, bsize / (1024 * 1024));
}

static ssize_t angle_show(struct class *cla, struct class_attribute *attr,
			  char *buf)
{
	return snprintf(buf, 80, "current angel is %d\n",
			ppmgr_device.global_angle);
}

static ssize_t angle_store(struct class *cla, struct class_attribute *attr,
			   const char *buf, size_t count)
{
	long angle;
	int ret = kstrtoul(buf, 0, &angle);

	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	if (angle > 3 || angle < 0) {
		/* size = endp - buf; */
		return count;
	}

	if (_ppmgr_angle_store(angle) < 0)
		return -EINVAL;

	/* size = endp - buf; */
	return count;
}

int get_use_prot(void)
{
	return ppmgr_device.use_prot;
}
EXPORT_SYMBOL(get_use_prot);

static ssize_t disable_prot_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 40, "%d\n", ppmgr_device.disable_prot);
}

static ssize_t disable_prot_store(struct class *cla,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	size_t r;
	u32 s_value;

	r = kstrtoint(buf, 0, &s_value);
	if (r != 0)
		return -EINVAL;

	ppmgr_device.disable_prot = s_value;
	return strnlen(buf, count);
}

static ssize_t orientation_show(struct class *cla,
				struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80, "current orientation is %d\n",
			ppmgr_device.orientation * 90);
}

/* set the initial orientation for video,
 * it should be set before video start.
 */
static ssize_t orientation_store(struct class *cla,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	ssize_t ret = -EINVAL; /* , size; */
	unsigned long tmp;
	/* unsigned angle = simple_strtoul(buf, &endp, 0); */
	unsigned int angle;

	ret = kstrtoul(buf, 0, &tmp);
	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	angle = tmp;
	/*if (property_change) return ret;*/
	if (angle > 3) {
		if (angle == 90) {
			angle = 1;
		} else if (angle == 180) {
			angle = 2;
		} else if (angle == 270) {
			angle = 3;
		} else {
			PPMGRDRV_ERR("invalid orientation value\n");
			PPMGRDRV_ERR("you should set 0 for 0 clockwise\n");
			PPMGRDRV_ERR("1 or 90 for 90 clockwise\n");
			PPMGRDRV_ERR("2 or 180 for 180 clockwise\n");
			PPMGRDRV_ERR("3 or 270 for 270 clockwise\n");
			return ret;
		}
	}
	ppmgr_device.orientation = angle;
	ppmgr_device.videoangle =
		(ppmgr_device.angle + ppmgr_device.orientation) % 4;
	PPMGRDRV_INFO("angle:%d,orientation:%d,videoangle:%d\n",
		      ppmgr_device.angle, ppmgr_device.orientation,
		      ppmgr_device.videoangle);
	/* size = endp - buf; */
	return count;
}

static ssize_t bypass_show(struct class *cla, struct class_attribute *attr,
			   char *buf)
{
	return snprintf(buf, 80, "current bypass is %d\n",
			ppmgr_device.bypass);
}

static ssize_t bypass_store(struct class *cla, struct class_attribute *attr,
			    const char *buf, size_t count)
{
	long tmp;

	/* ppmgr_device.bypass = simple_strtoul(buf, &endp, 0); */
	int ret = kstrtol(buf, 0, &tmp);

	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	ppmgr_device.bypass = tmp;
	/* size = endp - buf; */
	return count;
}

static ssize_t ppmgr_debug_show(struct class *cla,
				struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80,
			"current ppmgr_debug is %d\n",
			ppmgr_device.ppmgr_debug);
}

static ssize_t ppmgr_debug_store(struct class *cla,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	ppmgr_device.ppmgr_debug = tmp;
	return count;
}

static ssize_t debug_first_frame_show(struct class *cla,
				      struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80,
			"current debug_first_frame is %d\n",
			ppmgr_device.debug_first_frame);
}

static ssize_t debug_first_frame_store(struct class *cla,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	ppmgr_device.debug_first_frame = tmp;
	return count;
}

static ssize_t rect_show(struct class *cla, struct class_attribute *attr,
			 char *buf)
{
	return snprintf(buf, 80, "rotate rect:\nl:%d,t:%d,w:%d,h:%d\n",
			ppmgr_device.left,
			ppmgr_device.top,
			ppmgr_device.width,
			ppmgr_device.height);
}

static ssize_t rect_store(struct class *cla, struct class_attribute *attr,
			  const char *buf, size_t count)
{
	char *errstr =
		"data error,access string is \"left,top,width,height\"\n";
	char *strp = (char *)buf;
	char *endp = NULL;
	int value_array[4];
	static ssize_t buflen;
	static char *tokenlen;
	int i;
	long tmp;
	int ret;

	buflen = strlen(buf);
	value_array[0] = -1;
	value_array[1] = -1;
	value_array[2] = -1;
	value_array[3] = -1;

	for (i = 0; i < 4; i++) {
		if (buflen == 0) {
			PPMGRDRV_ERR("%s\n", errstr);
			return -EINVAL;
		}
		tokenlen = strnchr(strp, buflen, ',');
		if (!IS_ERR_OR_NULL(tokenlen))
			*tokenlen = '\0';
		/* value_array[i] = simple_strtoul(strp, &endp, 0); */
		ret = kstrtol(strp, 0, &tmp);
		if (ret != 0) {
			PPMGRDRV_ERR("ERROR convert %s to long int!\n", strp);
			return ret;
		}
		value_array[i] = tmp;
		if ((endp - strp) > (tokenlen - strp))
			break;
		if (!IS_ERR_OR_NULL(tokenlen)) {
			*tokenlen = ',';
			strp = tokenlen + 1;
			buflen = strlen(strp);
		} else {
			break;
		}
	}

	if (value_array[0] >= 0)
		ppmgr_device.left = value_array[0];
	if (value_array[1] >= 0)
		ppmgr_device.left = value_array[1];
	if (value_array[2] > 0)
		ppmgr_device.left = value_array[2];
	if (value_array[3] > 0)
		ppmgr_device.left = value_array[3];

	return count;
}

static ssize_t dump_path_show(struct class *cla, struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80,
			"ppmgr dump path is: %s\n",
			ppmgr_device.dump_path);
}

static ssize_t dump_path_store(struct class *cla, struct class_attribute *attr,
			       const char *buf, size_t count)
{
	char *tmp;

	tmp = kstrdup(buf, GFP_KERNEL);
	if (!tmp) {
		PPMGRDRV_INFO("buf kstrdup failed\n");
		return 0;
	}
	if (strlen(tmp) >= sizeof(ppmgr_device.dump_path) - 1) {
		memcpy(ppmgr_device.dump_path, tmp,
		       sizeof(ppmgr_device.dump_path) - 1);
		ppmgr_device.dump_path[sizeof(ppmgr_device.dump_path)
			- 1] = '\0';
	} else {
		strcpy(ppmgr_device.dump_path, tmp);
	}

	return count;
}

static ssize_t disp_show(struct class *cla, struct class_attribute *attr,
			 char *buf)
{
	return snprintf(buf, 80, "disp width is %d ; disp height is %d\n",
			ppmgr_device.disp_width, ppmgr_device.disp_height);
}

static void set_disp_para(const char *para)
{
	int parsed[2];
	int ret;
	int w, h;

	ret = parse_para(para, 2, parsed);
	if (ret <= 0)
		return;
	if (likely(ret == 2)) {
		w = parsed[0];
		h = parsed[1];
		if (ppmgr_device.disp_width != w ||
		    ppmgr_device.disp_height != h)
			buff_change = 1;
		ppmgr_device.disp_width = w;
		ppmgr_device.disp_height = h;
	}
}

static ssize_t disp_store(struct class *cla, struct class_attribute *attr,
			  const char *buf, size_t count)
{
	ssize_t buflen;

	buflen = strlen(buf);
	if (buflen <= 0)
		return 0;
	set_disp_para(buf);
	return count;
}

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
/* extern int video_scaler_notify(int flag); */
/* extern void amvideo_set_scaler_para(int x, int y, int w, int h, int flag); */

static ssize_t ppscaler_show(struct class *cla, struct class_attribute *attr,
			     char *buf)
{
	return snprintf(buf, 80, "current ppscaler mode is %s\n",
			(ppmgr_device.ppscaler_flag) ? "enabled" : "disabled");
}

static ssize_t ppscaler_store(struct class *cla, struct class_attribute *attr,
			      const char *buf, size_t count)
{
	long tmp;
	/* int flag simple_strtoul(buf, &endp, 0); */
	int flag;
	int ret = kstrtol(buf, 0, &tmp);

	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	flag = tmp;
	if (flag < 2 && flag != ppmgr_device.ppscaler_flag) {
		if (flag)
			video_scaler_notify(1);
		else
			video_scaler_notify(0);
		ppmgr_device.ppscaler_flag = flag;
		if (ppmgr_device.ppscaler_flag == 0)
			set_scaler_pos_reset(true);
	}
	/* size = endp - buf; */
	return count;
}

static void set_ppscaler_para(const char *para)
{
	int parsed[5];
	int ret;

	ret = parse_para(para, 5, parsed);
	if (ret <= 0)
		return;

	if (likely(ret == 5)) {
		ppmgr_device.scale_h_start = parsed[0];
		ppmgr_device.scale_v_start = parsed[1];
		ppmgr_device.scale_h_end = parsed[2];
		ppmgr_device.scale_v_end = parsed[3];
		amvideo_set_scaler_para(ppmgr_device.scale_h_start,
					ppmgr_device.scale_v_start,
					ppmgr_device.scale_h_end
					- ppmgr_device.scale_h_start + 1,
					ppmgr_device.scale_v_end
					- ppmgr_device.scale_v_start + 1,
					parsed[4]);
	}
}

static ssize_t ppscaler_rect_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "ppscaler rect:\nx:%d,y:%d,w:%d,h:%d\n",
			ppmgr_device.scale_h_start,
			ppmgr_device.scale_v_start,
			ppmgr_device.scale_h_end
			- ppmgr_device.scale_h_start + 1,
			ppmgr_device.scale_v_end
			- ppmgr_device.scale_v_start + 1);
}

static ssize_t ppscaler_rect_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	ssize_t buflen;

	buflen = strlen(buf);
	if (buflen <= 0)
		return 0;
	set_ppscaler_para(buf);
	return count;
}
#endif

static ssize_t ppmgr_receiver_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	if (ppmgr_device.receiver == 1)
		return snprintf(buf, 80, "video stream out to video4linux\n");
	else
		return snprintf(buf, 80, "video stream out to vlayer\n");
}

static ssize_t ppmgr_receiver_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	long tmp;
	int ret;

	if (buf[0] != '0' && buf[0] != '1') {
		PPMGRDRV_ERR("device to whitch the video stream decoded\n");
		PPMGRDRV_ERR("0: to video layer\n");
		PPMGRDRV_ERR("1: to amlogic video4linux /dev/video10\n");
		return 0;
	}
	/* ppmgr_device.receiver = simple_strtoul(buf, &endp, 0); */
	ret = kstrtoul(buf, 0, &tmp);
	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	ppmgr_device.receiver = tmp;
	vf_ppmgr_reset(0);
	/* size = endp - buf; */
	return count;
}

static ssize_t platform_type_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	if (platform_type == PLATFORM_TV)
		return snprintf(buf, 80, "current platform is TV\n");
	else if (platform_type == PLATFORM_MID)
		return snprintf(buf, 80, "current platform is MID\n");
	else if (platform_type == PLATFORM_MID_VERTICAL)
		return snprintf(buf, 80, "current platform is vertical MID\n");
	else
		return snprintf(buf, 80, "current platform is MBX\n");
}

static ssize_t platform_type_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	long tmp;
	/* platform_type = simple_strtoul(buf, &endp, 0); */
	int ret;

	ret = kstrtoul(buf, 0, &tmp);
	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	platform_type = tmp;
	/* size = endp - buf; */
	return count;
}

static ssize_t tb_detect_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	unsigned int detect = ppmgr_device.tb_detect;

	return snprintf(buf, 80, "current T/B detect mode is %d\n", detect);
}

static ssize_t tb_detect_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned long tmp;
	int ret = kstrtoul(buf, 0, &tmp);

	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	ppmgr_device.tb_detect = tmp;
	/* size = endp - buf; */
	return count;
}

static ssize_t tb_detect_period_show(struct class *cla,
				     struct class_attribute *attr, char *buf)
{
	unsigned int period = ppmgr_device.tb_detect_period;

	return snprintf(buf, 80, "current T/B detect period is %d\n", period);
}

static ssize_t tb_detect_period_store(struct class *cla,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned long tmp;
	/* platform_type = simple_strtoul(buf, &endp, 0); */
	int ret = kstrtoul(buf, 0, &tmp);

	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	ppmgr_device.tb_detect_period = tmp;
	/* size = endp - buf; */
	return count;
}

static ssize_t tb_detect_len_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	unsigned int len = ppmgr_device.tb_detect_buf_len;

	return snprintf(buf, 80, "current T/B detect buff len is %d\n", len);
}

static ssize_t tb_detect_len_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned long tmp;
	/* platform_type = simple_strtoul(buf, &endp, 0); */
	int ret = kstrtoul(buf, 0, &tmp);

	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	if (tmp < 6 || tmp > 16) {
		PPMGRDRV_INFO("tb detect buffer len should be  6~16 (%ld)\n",
			      tmp);
		tmp = 6;
	}
	ppmgr_device.tb_detect_buf_len = tmp;
	/* size = endp - buf; */
	return count;
}

static ssize_t tb_detect_mute_show(struct class *cla,
				   struct class_attribute *attr, char *buf)
{
	unsigned int mute = ppmgr_device.tb_detect_init_mute;

	return snprintf(buf, 80, "current T/B detect init mute is %d\n", mute);
}

static ssize_t tb_detect_mute_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	unsigned long tmp;
	/* platform_type = simple_strtoul(buf, &endp, 0); */
	int ret = kstrtoul(buf, 0, &tmp);

	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	PPMGRDRV_INFO("tb detect init mute is  %ld\n", tmp);
	ppmgr_device.tb_detect_init_mute = tmp;
	/* size = endp - buf; */
	return count;
}

static ssize_t tb_status_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	get_tb_detect_status();
	return snprintf(buf, 80, "#################\n");
}

static ssize_t mirror_show(struct class *cla, struct class_attribute *attr,
			   char *buf)
{
	if (ppmgr_device.mirror_flag == 1)
		return snprintf(buf, 80,
				"currnet mirror mode is l-r. value is: %d.\n",
				ppmgr_device.mirror_flag);
	else if (ppmgr_device.mirror_flag == 2)
		return snprintf(buf, 80,
				"currnet mirror mode is t-b. value is: %d.\n",
				ppmgr_device.mirror_flag);
	else
		return snprintf(buf, 80,
				"currnet is normal mode. value is: %d.\n",
				ppmgr_device.mirror_flag);
}

static ssize_t mirror_store(struct class *cla, struct class_attribute *attr,
			    const char *buf, size_t count)
{
	long tmp;
	int ret = kstrtol(buf, 0, &tmp);

	/* ppmgr_device.mirror_flag = simple_strtoul(buf, &endp, 0); */
	if (ret != 0) {
		PPMGRDRV_ERR("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	ppmgr_device.mirror_flag = tmp;
	if (ppmgr_device.mirror_flag > 2)
		ppmgr_device.mirror_flag = 0;
	/* size = endp - buf; */
	return count;
}

/* *************************************************************
 * 3DTV usage
 * *************************************************************
 */
/* extern int vf_ppmgr_get_states(struct vframe_states *states); */

static ssize_t ppmgr_vframe_states_show(struct class *cla,
					struct class_attribute *attr,
					char *buf)
{
	int ret = 0;
	struct vframe_states states;

	if (vf_ppmgr_get_states(&states) == 0) {
		ret += sprintf(buf + ret, "vframe_pool_size=%d\n",
			       states.vf_pool_size);
		ret += sprintf(buf + ret, "vframe buf_free_num=%d\n",
			       states.buf_free_num);
		ret += sprintf(buf + ret, "vframe buf_recycle_num=%d\n",
			       states.buf_recycle_num);
		ret += sprintf(buf + ret, "vframe buf_avail_num=%d\n",
			       states.buf_avail_num);

	} else {
		ret += sprintf(buf + ret, "vframe no states\n");
	}

	return ret;
}

static CLASS_ATTR_RO(ppmgr_info);
static CLASS_ATTR_RW(angle);
static CLASS_ATTR_RW(rect);
static CLASS_ATTR_RW(bypass);
static CLASS_ATTR_RW(ppmgr_debug);
static CLASS_ATTR_RW(debug_first_frame);
static CLASS_ATTR_RW(dump_path);
static CLASS_ATTR_RW(disp);
static CLASS_ATTR_RW(orientation);
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
static CLASS_ATTR_RW(ppscaler);
static CLASS_ATTR_RW(ppscaler_rect);
#endif
static CLASS_ATTR_RW(ppmgr_receiver);
static CLASS_ATTR_RW(platform_type);
static CLASS_ATTR_RW(mirror);
static CLASS_ATTR_RW(disable_prot);
static CLASS_ATTR_RW(tb_detect);
static CLASS_ATTR_RW(tb_detect_period);
static CLASS_ATTR_RO(ppmgr_vframe_states);
static CLASS_ATTR_RW(tb_detect_len);
static CLASS_ATTR_RW(tb_detect_mute);
static CLASS_ATTR_RO(tb_status);
static struct attribute *ppmgr_class_attrs[] = {
	&class_attr_ppmgr_info.attr,
	&class_attr_angle.attr,
	&class_attr_rect.attr,
	&class_attr_bypass.attr,
	&class_attr_ppmgr_debug.attr,
	&class_attr_debug_first_frame.attr,
	&class_attr_dump_path.attr,
	&class_attr_disp.attr,
	&class_attr_orientation.attr,
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	&class_attr_ppscaler.attr,
	&class_attr_ppscaler_rect.attr,
#endif
	&class_attr_ppmgr_receiver.attr,
	&class_attr_platform_type.attr,
	&class_attr_mirror.attr,
	&class_attr_disable_prot.attr,
	&class_attr_tb_detect.attr,
	&class_attr_tb_detect_period.attr,
	&class_attr_ppmgr_vframe_states.attr,
	&class_attr_tb_detect_len.attr,
	&class_attr_tb_detect_mute.attr,
	&class_attr_tb_status.attr,
	NULL
};

ATTRIBUTE_GROUPS(ppmgr_class);

static struct class ppmgr_class = {
	.name = PPMGR_CLASS_NAME,
	.class_groups = ppmgr_class_groups,
};

struct class *init_ppmgr_cls(void)
{
	int ret = 0;

	ret = class_register(&ppmgr_class);
	if (ret < 0) {
		PPMGRDRV_ERR("error create ppmgr class\n");
		return NULL;
	}
	return &ppmgr_class;
}

/***********************************************************************
 *
 * file op section.
 *
 ************************************************************************/

void set_ppmgr_buf_info(unsigned int start, unsigned int size)
{
	ppmgr_device.buffer_start = start;
	ppmgr_device.buffer_size = size;
}

int ppmgr_set_resource(unsigned long start, unsigned long end, struct device *p)
{
	if (inited_ppmgr_num != 0) {
		PPMGRDRV_ERR("Can't support the change at running\n");
		return -1;
	}

	set_ppmgr_buf_info(start, end - start + 1);
	ppmgr_dev_reg.mem_start = start;
	ppmgr_dev_reg.mem_end = end;
	ppmgr_dev_reg.cma_dev = p;

	return 0;
}

void get_ppmgr_buf_info(unsigned int *start, unsigned int *size)
{
	*start = ppmgr_device.buffer_start;
	*size = ppmgr_device.buffer_size;
}

static int ppmgr_open(struct inode *inode, struct file *file)
{
	ppmgr_device.open_count++;
	return 0;
}

static long ppmgr_ioctl(struct file *file, unsigned int cmd, ulong args)
{
	void __user *argp = (void __user *)args;
	int ret = 0;

	switch (cmd) {
	case PPMGR_IOC_GET_ANGLE:
		put_user(ppmgr_device.angle, (unsigned int *)argp);
		break;
	case PPMGR_IOC_SET_ANGLE:
		ret = _ppmgr_angle_store(args);
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long ppmgr_compat_ioctl(struct file *filp,
			       unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = ppmgr_ioctl(filp, cmd, args);

	return ret;
}
#endif

static int ppmgr_release(struct inode *inode, struct file *file)
{
#ifdef CONFIG_ARCH_MESON
	struct ge2d_context_s *context = (struct ge2d_context_s *)file
		->private_data;

	if (context && (destroy_ge2d_work_queue(context) == 0)) {
		ppmgr_device.open_count--;
		return 0;
	}
	PPMGRDRV_INFO("release one ppmgr device\n");
	return -1;
#else
	return 0;
#endif
}

#ifdef CONFIG_COMPAT
static long ppmgr_compat_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long args);
#endif

/***********************************************************************
 *
 * file op initintg section.
 *
 ************************************************************************/

static const struct file_operations ppmgr_fops = {
	.owner = THIS_MODULE,
	.open = ppmgr_open,
	.unlocked_ioctl = ppmgr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ppmgr_compat_ioctl,
#endif
	.release = ppmgr_release, };

int init_ppmgr_device(void)
{
	int ret = 0;

	strcpy(ppmgr_device.name, "ppmgr");
	ret = register_chrdev(0, ppmgr_device.name, &ppmgr_fops);
	if (ret <= 0) {
		PPMGRDRV_ERR("register ppmgr device error\n");
		return ret;
	}
	ppmgr_device.major = ret;
	ppmgr_device.dbg_enable = 0;

	ppmgr_device.angle = 0;
	ppmgr_device.bypass = 0;
	ppmgr_device.videoangle = 0;
	ppmgr_device.orientation = 0;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER_PPSCALER
	ppmgr_device.ppscaler_flag = 0;
	ppmgr_device.scale_h_start = 0;
	ppmgr_device.scale_h_end = 0;
	ppmgr_device.scale_v_start = 0;
	ppmgr_device.scale_v_end = 0;
	scaler_pos_reset = false;
#endif
	ppmgr_device.receiver = 0;
	ppmgr_device.receiver_format =
		(GE2D_FORMAT_M24_NV21 | GE2D_LITTLE_ENDIAN);
	ppmgr_device.display_mode = 0;
	ppmgr_device.mirror_flag = 0;
	ppmgr_device.canvas_width = 0;
	ppmgr_device.canvas_height = 0;
	ppmgr_device.tb_detect = 0;
	ppmgr_device.tb_detect_period = 0;
	ppmgr_device.tb_detect_buf_len = 8;
	ppmgr_device.tb_detect_init_mute = 0;
	ppmgr_device.ppmgr_debug = 0;
	ppmgr_device.debug_first_frame = 0;
	PPMGRDRV_INFO("ppmgr_dev major:%d\n", ret);

	ppmgr_device.cla = init_ppmgr_cls();
	if (IS_ERR_OR_NULL(ppmgr_device.cla))
		return -1;
	ppmgr_device.dev = device_create(ppmgr_device.cla, NULL,
					 MKDEV(ppmgr_device.major, 0),
					 NULL, ppmgr_device.name);
	if (IS_ERR(ppmgr_device.dev)) {
		PPMGRDRV_ERR("create ppmgr device error\n");
		goto unregister_dev;
	}
	buff_change = 0;
	ppmgr_register();
	/*if (start_vpp_task()<0) return -1;*/
	ppmgr_device.use_prot = 1;
#if HAS_VPU_PROT
	ppmgr_device.disable_prot = 0;
#else
	ppmgr_device.disable_prot = 1;
#endif
	ppmgr_device.global_angle = 0;
	ppmgr_device.started = 0;
	return 0;

unregister_dev:
	class_unregister(ppmgr_device.cla);
	return -1;
}

int uninit_ppmgr_device(void)
{
	stop_ppmgr_task();

	if (ppmgr_device.cla) {
		if (ppmgr_device.dev)
			device_destroy(ppmgr_device.cla,
				       MKDEV(ppmgr_device.major, 0));
		class_unregister(ppmgr_device.cla);
	}

	unregister_chrdev(ppmgr_device.major, ppmgr_device.name);
	return 0;
}

/*******************************************************************
 *
 * interface for Linux driver
 *
 * ******************************************************************/

MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0xff, LOG_LEVEL_DESC, LOG_MASK_DESC);

static struct platform_device *ppmgr_dev0;
/*static struct resource memobj;*/
/* for driver. */
static int ppmgr_driver_probe(struct platform_device *pdev)
{
	s32 r;

	PPMGRDRV_INFO("%s called\n", __func__);
	r = of_reserved_mem_device_init(&pdev->dev);
	ppmgr_device.pdev = pdev;
	init_ppmgr_device();

	if (r == 0)
		PPMGRDRV_INFO("%s done\n", __func__);

	return r;
}

static int ppmgr_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	unsigned long start, end;

	start = rmem->base;
	end = rmem->base + rmem->size - 1;
	PPMGRDRV_INFO("init ppmgr memsource %lx->%lx\n", start, end);

	ppmgr_set_resource(start, end, dev);
	return 0;
}

/* #ifdef CONFIG_USE_OF */
static const struct of_device_id amlogic_ppmgr_dt_match[] = {{.compatible =
	"amlogic, ppmgr", }, {}, };
/* #else */
/* #define amlogic_ppmgr_dt_match NULL */
/* #endif */

static const struct reserved_mem_ops rmem_ppmgr_ops = {.device_init =
	ppmgr_mem_device_init, };

#ifdef AMLOGIC_RMEM_MULTI_USER
static struct rmem_multi_user rmem_ppmgr_muser = {
	.of_match_table = amlogic_ppmgr_dt_match,
	.ops  = &rmem_ppmgr_ops,
};
#endif

static int __init ppmgr_mem_setup(struct reserved_mem *rmem)
{
	PPMGRDRV_DBG("ppmgr share mem setup\n");
	ppmgr_device.use_reserved = 0;
	ppmgr_device.buffer_size = 0;
#ifdef AMLOGIC_RMEM_MULTI_USER
	of_add_rmem_multi_user(rmem, &rmem_ppmgr_muser);
#else
	rmem->ops = &rmem_ppmgr_ops;
#endif
	if (ppmgr_device.buffer_size > 0) {
		ppmgr_device.use_reserved = 1;
		pr_warn("ppmgr use reserved memory\n");
	}
	/* ppmgr_device.buffer_size = 0; */
	return 0;
}

static int ppmgr_drv_remove(struct platform_device *plat_dev)
{
	/*struct rtc_device *rtc = platform_get_drvdata(plat_dev);*/
	/*rtc_device_unregister(rtc);*/
	/*device_remove_file(&plat_dev->dev, &dev_attr_irq);*/
	uninit_ppmgr_device();
	return 0;
}

/* general interface for a linux driver .*/
struct platform_driver ppmgr_drv = {
	.probe = ppmgr_driver_probe,
	.remove = ppmgr_drv_remove,
	.driver = {
		.name = "ppmgr",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_ppmgr_dt_match,
	}
};

int __init ppmgr_init_module(void)
{
	int err;

	PPMGRDRV_WARN("ppmgr module init func called\n");
	amlog_level(LOG_LEVEL_HIGH, "ppmgr_init\n");
	err = platform_driver_register(&ppmgr_drv);
	if (err)
		return err;

	return err;
}

void __exit ppmgr_remove_module(void)
{
	platform_device_put(ppmgr_dev0);
	platform_driver_unregister(&ppmgr_drv);
	amlog_level(LOG_LEVEL_HIGH, "ppmgr module removed.\n");
}

#ifndef MODULE
module_init(ppmgr_init_module);
module_exit(ppmgr_remove_module);
#endif

RESERVEDMEM_OF_DECLARE(ppmgr, "amlogic, idev-mem", ppmgr_mem_setup);
//MODULE_DESCRIPTION("AMLOGIC  ppmgr driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("aml-sh <kasin.li@amlogic.com>");

