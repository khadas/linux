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
#ifndef _AML_VCODEC_GE2D_H_
#define _AML_VCODEC_GE2D_H_

#include <linux/kfifo.h>
#include "aml_vcodec_drv.h"
#include "aml_vcodec_dec.h"

/* define ge2d work mode. */
#define GE2D_MODE_CONVERT_NV12		(1 << 0)
#define GE2D_MODE_CONVERT_NV21		(1 << 1)
#define GE2D_MODE_CONVERT_LE		(1 << 2)
#define GE2D_MODE_CONVERT_BE		(1 << 3)
#define GE2D_MODE_SEPARATE_FIELD	(1 << 4)
#define GE2D_MODE_422_TO_420		(1 << 5)

#define GE2D_FRAME_SIZE 64

struct aml_v4l2_ge2d_buf {
	u32			flag;
	struct vframe_s		*vf;
	void			*caller_data;
	struct aml_video_dec_buf *aml_buf;
};

struct aml_v4l2_ge2d {
	struct ge2d_context_s	*ge2d_context; /* handle of GE2D */
	u32			buf_size; /* buffer size for ge2d */
	u32			work_mode; /* enum ge2d_work_mode */
	u32			buffer_mode;
	struct aml_vcodec_ctx	*ctx;

	DECLARE_KFIFO_PTR(input, typeof(struct aml_v4l2_ge2d_buf*));
	DECLARE_KFIFO_PTR(output, typeof(struct aml_v4l2_ge2d_buf*));
	DECLARE_KFIFO_PTR(frame, typeof(struct vframe_s *));
	DECLARE_KFIFO(out_done_q, struct aml_v4l2_ge2d_buf *, GE2D_FRAME_SIZE);
	DECLARE_KFIFO(in_done_q, struct aml_v4l2_ge2d_buf *, GE2D_FRAME_SIZE);

	struct vframe_s		*vfpool;
	struct aml_v4l2_ge2d_buf *ovbpool;
	struct aml_v4l2_ge2d_buf *ivbpool;
	struct task_struct	*task;
	bool			running;
	struct semaphore	sem_in, sem_out;

	/* In p to i transition, output/frame can be multi writer */
	struct mutex		output_lock;

	/* for debugging */
	/*
	 * in[0] --> ge2d <-- in[1]
	 * out[0]<-- ge2d --> out[1]
	 */
	int			in_num[2];
	int			out_num[2];
	ulong			fb_token;
};

struct task_ops_s *get_ge2d_ops(void);

int aml_v4l2_ge2d_init(
		struct aml_vcodec_ctx *ctx,
		struct aml_ge2d_cfg_infos *cfg,
		struct aml_v4l2_ge2d** ge2d_handle);

int aml_v4l2_ge2d_destroy(struct aml_v4l2_ge2d* ge2d);

#endif
