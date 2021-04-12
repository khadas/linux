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

#ifndef _di_v4l_H
#define _di_v4l_H

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
#include <linux/mm.h>
#include <linux/kfifo.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/di/di_interface.h>

#define DI_INPUT_SIZE 16
#define DI_OUTPUT_SIZE 16

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

struct crc_info_t {
	unsigned int crcdata[11];
	bool done_flag[11];
};

struct yuv_info_t {
	unsigned int format;
	unsigned int width;
	unsigned int height;
	unsigned int buffer_w;
	unsigned int buffer_h;
};

struct in_buf_t {
	int index;
	u32 phy_addr;
	u32 buf_w;
	u32 buf_h;
	u32 buf_size;
	struct vframe_s frame;
	struct di_buffer di_buf;
};

struct di_v4l_dev {
	u32 index;
	int di_index;
	struct yuv_info_t yuv_info;
	struct di_init_parm di_parm;
	struct di_buffer di_input[DI_INPUT_SIZE];
	DECLARE_KFIFO(di_input_free_q, struct di_buffer *, DI_INPUT_SIZE);
	DECLARE_KFIFO(di_output_di_q, struct di_buffer *, DI_OUTPUT_SIZE);
	struct in_buf_t in_buf[DI_INPUT_SIZE];
	bool in_buf_ready;
	u32 empty_done_count;
	u32 fill_done_count;
	u32 fill_count;
	u32 fill_frame_count;
	u32 fill_crc_count;
};

#define DI_V4L_IOC_MAGIC  'I'
#define DI_V4L_IOCTL_GET_CRC   _IOR(DI_V4L_IOC_MAGIC, 0x00, int)
#define DI_V4L_IOCTL_INIT      _IOW(DI_V4L_IOC_MAGIC, 0x01, int)

#endif

