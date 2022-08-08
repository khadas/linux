/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_processor/v4lvideo/v4lvideo.h
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

#ifndef _v4lvideo_H
#define _v4lvideo_H

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>

#include <linux/mm.h>
/* #include <mach/mod_gate.h> */

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas.h>

#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#endif
#define MAX_WIDTH 4096
#define MAX_HEIGHT 4096

#define V4LVIDEO_POOL_SIZE 18
#define ION_VF_RECEIVER_NAME_SIZE 32

#define V4LVID_INFO(fmt, args...) pr_info("v4lvid: info: " fmt, ## args)
#define V4LVID_DBG(fmt, args...) pr_debug("v4lvid: dbg: " fmt, ## args)
#define V4LVID_ERR(fmt, args...) pr_err("v4lvid: err: " fmt, ## args)

#define dprintk(dev, level, fmt, arg...)                    \
v4l2_dbg(level, debug, &(dev)->v4l2_dev, fmt, ## arg)

struct v4l2q_s {
	int rp;
	int wp;
	int size;
	int pre_rp;
	int pre_wp;
	void **pool;
};

static inline void v4l2q_lookup_start(struct v4l2q_s *q)
{
	q->pre_rp = q->rp;
	q->pre_wp = q->wp;
}

static inline void v4l2q_lookup_end(struct v4l2q_s *q)
{
	q->rp = q->pre_rp;
	q->wp = q->pre_wp;
}

static inline void v4l2q_init(struct v4l2q_s *q, u32 size,
			      void **pool)
{
	q->rp = 0;
	q->wp = 0;
	q->size = size;
	q->pool = pool;
}

static inline bool v4l2q_empty(struct v4l2q_s *q)
{
	return q->rp == q->wp;
}

static inline int v4l2q_level(struct v4l2q_s *q)
{
	int level = q->wp - q->rp;

	if (level < 0)
		level += q->size;

	return level;
}

static inline void v4l2q_push(struct v4l2q_s *q, void *vf)
{
	int wp = q->wp;

	/*ToDo*/
	smp_mb();

	q->pool[wp] = vf;

	/*ToDo*/
	smp_wmb();

	q->wp = (wp == (q->size - 1)) ? 0 : (wp + 1);
}

static inline void *v4l2q_pop(struct v4l2q_s *q)
{
	void *vf;
	int rp;

	if (v4l2q_empty(q))
		return NULL;

	rp = q->rp;

	/*ToDo*/
	smp_rmb();

	vf = q->pool[rp];

	/*ToDo*/
	smp_mb();

	q->rp = (rp == (q->size - 1)) ? 0 : (rp + 1);

	return vf;
}

static inline void *v4l2q_peek(struct v4l2q_s *q)
{
	return (v4l2q_empty(q)) ? NULL : q->pool[q->rp];
}

static inline bool v4l2q_pop_specific(struct v4l2q_s *q, void *vf)
{
	void *vf_tmp = NULL;
	int i = v4l2q_level(q);

	if (i <= 0) {
		pr_err("%s fail i =%d\n", __func__, i);
		return false;
	}

	while (i > 0) {
		i--;
		vf_tmp = v4l2q_pop(q);
		if (vf_tmp != vf) {
			v4l2q_push(q, vf_tmp);
			if (i < 1) {
				pr_err("%s fail\n", __func__);
				return false;
			}
		} else {
			break;
		}
	}
	return true;
}

/* ------------------------------------------------------------------
 * Basic structures
 * ------------------------------------------------------------------
 */
struct v4lvideo_fmt {
	char *name;
	u32 fourcc; /* v4l2 format id */
	u8 depth;
	bool is_yuv;
};

/* v4l2_amlogic_parm must < u8[200] */
struct v4l2_amlogic_parm {
		u32	signal_type;
		struct vframe_master_display_colour_s
		 master_display_colour;
		u32 hdr10p_data_size;
		char hdr10p_data_buf[128];
	};

struct v4lvideo_file_s {
	atomic_t on_use;
	struct file_private_data *private_data_p;
	struct vframe_s *vf_p;
	struct vframe_s *vf_ext_p;
	u32 flag;
	u32 vf_type;
	bool free_before_unreg;
	u32 inst_id;
	struct v4lvideo_dev *dev;
};

struct v4lvideo_active_file {
	int index;
	atomic_t on_use;
	struct file_private_data *p;
};

struct v4lvideo_dev {
	struct list_head v4lvideo_devlist;
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	int fd_num;

	/* video capture */
	const struct v4lvideo_fmt *fmt;
	unsigned int width, height;
	struct vframe_receiver_s video_vf_receiver;
	u64 last_pts_us64;

	char vf_receiver_name[ION_VF_RECEIVER_NAME_SIZE];
	int inst;
	bool mapped;
	int vf_wait_cnt;
	bool receiver_register;
	u32 frame_num;

	struct v4l2q_s input_queue;
	struct v4l2q_s display_queue;
	struct v4l2_buffer *v4lvideo_input_queue[V4LVIDEO_POOL_SIZE];
	struct v4lvideo_file_s *v4lvideo_display_queue[V4LVIDEO_POOL_SIZE];
	/* mutex_input */
	struct mutex mutex_input;
	/* mutex_opened */
	struct mutex mutex_opened;
	struct v4l2_buffer v4lvideo_input[V4LVIDEO_POOL_SIZE];
	struct v4l2_amlogic_parm am_parm;
	u8 first_frame;
	char *provider_name;
	struct v4lvideo_file_s v4lvideo_file[V4LVIDEO_POOL_SIZE];
	bool opened;
	struct v4lvideo_active_file active_file[V4LVIDEO_POOL_SIZE];
	int dv_inst;
};

enum vframe_source_type {
	DECODER_8BIT_NORMAL = 0,
	DECODER_8BIT_BOTTOM,
	DECODER_8BIT_TOP,
	DECODER_10BIT_NORMAL,
	DECODER_10BIT_BOTTOM,
	DECODER_10BIT_TOP,
	VDIN_8BIT_NORMAL,
	VDIN_10BIT_NORMAL,
};

struct video_info_t {
	u32 width;
	u32 height;
};

unsigned int get_v4lvideo_debug(void);

#define V4LVIDEO_IOC_MAGIC  'I'
#define V4LVIDEO_IOCTL_ALLOC_ID   _IOW(V4LVIDEO_IOC_MAGIC, 0x00, int)
#define V4LVIDEO_IOCTL_FREE_ID    _IOW(V4LVIDEO_IOC_MAGIC, 0x01, int)
#define V4LVIDEO_IOCTL_ALLOC_FD   _IOW(V4LVIDEO_IOC_MAGIC, 0x02, int)
#define V4LVIDEO_IOCTL_LINK_FD    _IOW(V4LVIDEO_IOC_MAGIC, 0x03, int)

#endif
