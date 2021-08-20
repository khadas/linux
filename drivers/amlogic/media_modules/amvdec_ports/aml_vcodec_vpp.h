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
#ifndef _AML_VCODEC_VPP_H_
#define _AML_VCODEC_VPP_H_

#define SUPPORT_V4L_VPP

#include <linux/kfifo.h>
#ifdef SUPPORT_V4L_VPP
#include <linux/amlogic/media/di/di_interface.h>
#endif
#include "aml_vcodec_drv.h"
#include "aml_vcodec_dec.h"

enum vpp_work_mode {
	VPP_MODE_DI,
	VPP_MODE_COLOR_CONV,
	VPP_MODE_NOISE_REDUC,
	VPP_MODE_DI_LOCAL = 0x81,
	VPP_MODE_COLOR_CONV_LOCAL = 0x82,
	VPP_MODE_NOISE_REDUC_LOCAL = 0x83,
	VPP_MODE_S4_DW_MMU = 0x91,
	VPP_MODE_MAX = 0xff
};

#define VPP_FRAME_SIZE 64

struct aml_v4l2_vpp_buf {
#ifdef SUPPORT_V4L_VPP
	struct di_buffer di_buf;
	struct di_buffer *di_local_buf;
#endif
	struct aml_video_dec_buf *aml_buf;
	struct aml_v4l2_vpp_buf *inbuf;
};

struct aml_v4l2_vpp {
	int di_handle; /* handle of DI */
	struct aml_vcodec_ctx *ctx;
	u32 buf_size; /* buffer size for vpp */
	u32 work_mode; /* enum vpp_work_mode */
	u32 buffer_mode;

	DECLARE_KFIFO_PTR(input, typeof(struct aml_v4l2_vpp_buf*));
	DECLARE_KFIFO_PTR(output, typeof(struct aml_v4l2_vpp_buf*));
	DECLARE_KFIFO_PTR(processing, typeof(struct aml_v4l2_vpp_buf*));
	DECLARE_KFIFO_PTR(frame, typeof(struct vframe_s *));
	DECLARE_KFIFO(out_done_q, struct aml_v4l2_vpp_buf *, VPP_FRAME_SIZE);
	DECLARE_KFIFO(in_done_q, struct aml_v4l2_vpp_buf *, VPP_FRAME_SIZE);

	struct vframe_s *vfpool;
	struct aml_v4l2_vpp_buf *ovbpool;
	struct aml_v4l2_vpp_buf *ivbpool;
	struct task_struct *task;
	bool running;
	struct semaphore sem_in, sem_out;

	/* In p to i transition, output/frame can be multi writer */
	struct mutex output_lock;

	/* for debugging */
	/*
	 * in[0] --> vpp <-- in[1]
	 * out[0]<-- vpp --> out[1]
	 */
	int in_num[2];
	int out_num[2];
	ulong fb_token;

	bool is_prog;
	bool is_bypass_p;
	int di_ibuf_num;
	int di_obuf_num;
};

struct task_ops_s *get_vpp_ops(void);

#ifdef SUPPORT_V4L_VPP
/* get number of buffer needed for a working mode */
int aml_v4l2_vpp_get_buf_num(u32 mode);
int aml_v4l2_vpp_init(
		struct aml_vcodec_ctx *ctx,
		struct aml_vpp_cfg_infos *cfg,
		struct aml_v4l2_vpp** vpp_handle);
int aml_v4l2_vpp_destroy(struct aml_v4l2_vpp* vpp);
int aml_v4l2_vpp_reset(struct aml_v4l2_vpp *vpp);
#else
static inline int aml_v4l2_vpp_get_buf_num(u32 mode) { return -1; }
static inline int aml_v4l2_vpp_init(
		struct aml_vcodec_ctx *ctx,
		struct aml_vpp_cfg_infos *cfg,
		struct aml_v4l2_vpp** vpp_handle) { return -1; }
static inline int aml_v4l2_vpp_destroy(struct aml_v4l2_vpp* vpp) { return -1; }
#endif

#endif
