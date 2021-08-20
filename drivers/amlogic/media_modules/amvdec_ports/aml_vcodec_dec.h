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
#ifndef _AML_VCODEC_DEC_H_
#define _AML_VCODEC_DEC_H_

#include <linux/kref.h>
#include <linux/scatterlist.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include "aml_vcodec_util.h"
#include "aml_task_chain.h"

#define VCODEC_CAPABILITY_4K_DISABLED	0x10
#define VCODEC_DEC_4K_CODED_WIDTH	4096U
#define VCODEC_DEC_4K_CODED_HEIGHT	2304U
#define AML_VDEC_MAX_W			2048U
#define AML_VDEC_MAX_H			1088U

#define AML_VDEC_IRQ_STATUS_DEC_SUCCESS	0x10000
#define V4L2_BUF_FLAG_LAST		0x00100000

#define VDEC_GATHER_MEMORY_TYPE		0
#define VDEC_SCATTER_MEMORY_TYPE	1

#define META_DATA_SIZE			(256)
#define MD_BUF_SIZE			(1024)
#define COMP_BUF_SIZE			(8196)
#define SEI_BUF_SIZE			(2 * 12 * 1024)
#define SEI_TYPE	(1)
#define DV_TYPE		(2)


/*
 * struct vdec_v4l2_buffer - decoder frame buffer
 * @mem_type	: gather or scatter memory.
 * @num_planes	: used number of the plane
 * @mem[4]	: array mem for used planes,
 *		  mem[0]: Y, mem[1]: C/U, mem[2]: V
 * @vf_fd	: the file handle of video frame
 * @status      : frame buffer status (vdec_fb_status)
 * @buf_idx	: the index from vb2 index.
 * @vframe	: store the vframe that get from caller.
 * @task	: the context of task chain manager.
 */

struct vdec_v4l2_buffer {
	int	mem_type;
	int	num_planes;
	union {
		struct	aml_vcodec_mem mem[4];
		u32	vf_fd;
	} m;
	u32	status;
	u32	buf_idx;
	void	*vframe;

	struct task_chain_s *task;
};

/**
 * struct aml_video_dec_buf - Private data related to each VB2 buffer.
 * @b:		VB2 buffer
 * @list:	link list
 * @used:	Capture buffer contain decoded frame data and keep in
 *			codec data structure
 * @lastframe:		Intput buffer is last buffer - EOS
 * @error:		An unrecoverable error occurs on this buffer.
 * @frame_buffer:	Decode status, and buffer information of Capture buffer
 *
 * Note : These status information help us track and debug buffer state
 */
struct aml_video_dec_buf {
	struct vb2_v4l2_buffer vb;
	struct list_head list;

	struct vdec_v4l2_buffer frame_buffer;
	struct file_private_data privdata;
	struct codec_mm_s *mem[2];
	char mem_onwer[32];
	bool used;
	bool que_in_m2m;
	bool lastframe;
	bool error;

	/* internal compressed buffer */
	unsigned int internal_index;

	ulong vpp_buf_handle;
	ulong ge2d_buf_handle;

	/*4 bytes data for data len*/
	char meta_data[META_DATA_SIZE + 4];

	struct sg_table *out_sgt;
	struct sg_table *cap_sgt;
};

extern const struct v4l2_ioctl_ops aml_vdec_ioctl_ops;
extern const struct v4l2_m2m_ops aml_vdec_m2m_ops;

/*
 * aml_vdec_lock/aml_vdec_unlock are for ctx instance to
 * get/release lock before/after access decoder hw.
 * aml_vdec_lock get decoder hw lock and set curr_ctx
 * to ctx instance that get lock
 */
void aml_vdec_unlock(struct aml_vcodec_ctx *ctx);
void aml_vdec_lock(struct aml_vcodec_ctx *ctx);
int aml_vcodec_dec_queue_init(void *priv, struct vb2_queue *src_vq,
			   struct vb2_queue *dst_vq);
void aml_vcodec_dec_set_default_params(struct aml_vcodec_ctx *ctx);
void aml_vcodec_dec_release(struct aml_vcodec_ctx *ctx);
int aml_vcodec_dec_ctrls_setup(struct aml_vcodec_ctx *ctx);
void wait_vcodec_ending(struct aml_vcodec_ctx *ctx);
void vdec_frame_buffer_release(void *data);
void aml_vdec_dispatch_event(struct aml_vcodec_ctx *ctx, u32 changes);
void* v4l_get_vf_handle(int fd);
void aml_v4l_ctx_release(struct kref *kref);
void dmabuff_recycle_worker(struct work_struct *work);
void aml_buffer_status(struct aml_vcodec_ctx *ctx);
void aml_vdec_basic_information(struct aml_vcodec_ctx *ctx);

void aml_alloc_buffer(struct aml_vcodec_ctx *ctx, int flag);
void aml_free_buffer(struct aml_vcodec_ctx *ctx, int flag);
void aml_free_one_sei_buffer(struct aml_vcodec_ctx *ctx, char **addr, int *size, int idx);
void aml_bind_sei_buffer(struct aml_vcodec_ctx *v4l, char **addr, int *size, int *idx);
void aml_bind_dv_buffer(struct aml_vcodec_ctx *v4l, char **comp_buf, char **md_buf);

int aml_canvas_cache_init(struct aml_vcodec_dev *dev);
void aml_canvas_cache_put(struct aml_vcodec_dev *dev);
int aml_canvas_cache_get(struct aml_vcodec_dev *dev, char *usr);

#endif /* _AML_VCODEC_DEC_H_ */
