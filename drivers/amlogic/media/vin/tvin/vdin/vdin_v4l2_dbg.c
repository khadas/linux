// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux Headers */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>

/* Local Headers */
/*#include "../tvin_format_table.h"*/
#include "vdin_drv.h"
#include "vdin_ctl.h"
/*#include "vdin_regs.h"*/
/*#include "vdin_afbce.h"*/

#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include "vdin_canvas.h"
#include "vdin_v4l2_dbg.h"
#include "vdin_v4l2_if.h"

static void vdin_v4l2_status(struct vdin_dev_s *devp)
{
	pr_info("vdin_v4l_ver:0x%x\n", VDIN_DEV_VER);
	pr_info("ver2:%s\n", VDIN_DEV_VER2);
	pr_info("work_mode:%d\n", devp->work_mode);
	pr_info("type:%d\n", devp->vb_queue.type);
	pr_info("memory:%d(%s)\n", devp->vb_queue.memory,
		vb2_memory_sts_to_str(devp->vb_queue.memory));
	pr_info("num_buffers:%d\n", devp->vb_queue.num_buffers);
	pr_info("queued_count:%d\n", devp->vb_queue.queued_count);
	pr_info("streaming:%d\n", devp->vb_queue.streaming);
	pr_info("buf_struct_size:%d\n", devp->vb_queue.buf_struct_size);
	pr_info("min_buffers_needed:%d\n", devp->vb_queue.min_buffers_needed);
	pr_info("v4l_en:%d\n", devp->dts_config.v4l_en);
	pr_info("secure_flg:%d\n", devp->vdin_v4l2.secure_flg);

	/* vdin v4l2 stats */
	pr_info("done_cnt:%u\n", devp->vdin_v4l2.stats.done_cnt);
	pr_info("dque_cnt:%u\n", devp->vdin_v4l2.stats.dque_cnt);
	pr_info("que_cnt:%u\n", devp->vdin_v4l2.stats.que_cnt);
	pr_info("drop_divide:%u\n", devp->vdin_v4l2.stats.drop_divide);
}

static ssize_t v4l_dbg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/*struct vdin_dev_s *devp = dev_get_drvdata(dev);*/
	ssize_t len = 0;

	len += sprintf(buf + len, "\n v4l debug interface");
	len += sprintf(buf + len, "\n5 start 0/1\n");
	len += sprintf(buf + len, "\n5 dbg_en 0/1 x\n");
	len += sprintf(buf + len, "\n5 status x\n");
	len += sprintf(buf + len, "\n5 pause 0/1 x\n");
	return len;
}

static ssize_t v4l_dbg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	char *parm[47] = {NULL};
	char *buf_orig = kstrdup(buf, GFP_KERNEL);
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	long val = 0;
	/*unsigned int temp;*/
	/*unsigned int mode = 0, flag = 0;*/

	if (!buf)
		return len;
	vdin_parse_param(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "start", 5)) {
		if (kstrtol(parm[1], 10, &val) == 0)
			devp->work_mode = val;
		devp->work_mode = val;//VDIN_WORK_MD_V4L;
		pr_info("vdin v4l start %d\n", devp->work_mode);
	} else if (!strncmp(parm[0], "dbg_en", 5)) {
		if (kstrtol(parm[1], 16, &val) == 0)
			vdin_v4l_debug = val;
	} else if (!strncmp(parm[0], "status", 6)) {
		vdin_v4l2_status(devp);
	} else if (!strncmp(parm[0], "pause", 5)) {
		if (kstrtol(parm[1], 10, &val) == 0)
			devp->dbg_v4l_pause = val;
		pr_info("v4l pause %d\n", devp->dbg_v4l_pause);
	} else if (!strcmp(parm[0], "no_vdin_ioctl")) {/* for v4l2 test app */
		if (kstrtol(parm[1], 0, &val) == 0)
			devp->dbg_v4l_no_vdin_ioctl = val;
		pr_info("v4l dbg_v4l_no_vdin_ioctl %d\n", devp->dbg_v4l_no_vdin_ioctl);
	} else if (!strcmp(parm[0], "no_vdin_event")) {/* for v4l2 test app */
		if (kstrtol(parm[1], 0, &val) == 0)
			devp->dbg_v4l_no_vdin_event = val;
		pr_info("v4l dbg_v4l_no_vdin_event %d\n", devp->dbg_v4l_no_vdin_event);
	} else if (!strcmp(parm[0], "pixfmt")) {/* for v4l2 test app */
		if (kstrtol(parm[1], 0, &val) == 0)
			devp->v4l2_dbg_ctl.dbg_pix_fmt = val;
		pr_info("dbg_pix_fmt %#x\n", devp->v4l2_dbg_ctl.dbg_pix_fmt);
		pr_info("V4L2_PIX_FMT_UYVY:%#x\n", V4L2_PIX_FMT_UYVY);
		pr_info("V4L2_PIX_FMT_NV21:%#x\n", V4L2_PIX_FMT_NV21);
		pr_info("V4L2_PIX_FMT_NV12:%#x\n", V4L2_PIX_FMT_NV12);
		pr_info("V4L2_PIX_FMT_NV21M:%#x\n", V4L2_PIX_FMT_NV21M);
		pr_info("V4L2_PIX_FMT_NV12M:%#x\n", V4L2_PIX_FMT_NV12M);
	} else {
		pr_info("%s unknown cmd\n", __func__);
	}

	return len;
}
static DEVICE_ATTR_RW(v4l_dbg);

static ssize_t v4l_work_mode_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct vdin_dev_s *devp = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n", devp->work_mode);
}

static ssize_t v4l_work_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t len)
{
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	int cnt, val;

	cnt = kstrtoint(buf, 16, &val);
	if (cnt < 0)
		return -EINVAL;

	devp->work_mode = val;

	return len;
}
static DEVICE_ATTR_RW(v4l_work_mode);

void vdin_v4l2_create_device_files(struct device *dev)
{
	device_create_file(dev, &dev_attr_v4l_dbg);
	device_create_file(dev, &dev_attr_v4l_work_mode);
}

void vdin_v4l2_remove_device_files(struct device *dev)
{
	device_remove_file(dev, &dev_attr_v4l_dbg);
	device_remove_file(dev, &dev_attr_v4l_work_mode);
}

