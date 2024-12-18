/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip Video Decoder 2 driver
 *
 * Copyright (C) 2024 Collabora, Ltd.
 *  Detlev Casanova <detlev.casanova@collabora.com>
 *
 * Based on rkvdec driver by Boris Brezillon <boris.brezillon@collabora.com>
 */
#ifndef RKVDEC_H_
#define RKVDEC_H_

#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>

#include "rkvdec2-regs.h"

#define RKVDEC2_RCB_COUNT	10

struct rkvdec2_ctx;

enum rkvdec2_alloc_type {
	RKVDEC2_ALLOC_SRAM,
	RKVDEC2_ALLOC_DMA,
};

struct rkvdec2_aux_buf {
	void *cpu;
	dma_addr_t dma;
	size_t size;
	enum rkvdec2_alloc_type type;
};

struct rkvdec2_ctrl_desc {
	struct v4l2_ctrl_config cfg;
};

struct rkvdec2_ctrls {
	const struct rkvdec2_ctrl_desc *ctrls;
	unsigned int num_ctrls;
};

struct rkvdec2_run {
	struct {
		struct vb2_v4l2_buffer *src;
		struct vb2_v4l2_buffer *dst;
	} bufs;
};

struct rkvdec2_decoded_buffer {
	/* Must be the first field in this struct. */
	struct v4l2_m2m_buffer base;
};

static inline struct rkvdec2_decoded_buffer *
vb2_to_rkvdec2_decoded_buf(struct vb2_buffer *buf)
{
	return container_of(buf, struct rkvdec2_decoded_buffer,
			    base.vb.vb2_buf);
}

struct rkvdec2_coded_fmt_ops {
	int (*adjust_fmt)(struct rkvdec2_ctx *ctx,
			  struct v4l2_format *f);
	int (*start)(struct rkvdec2_ctx *ctx);
	void (*stop)(struct rkvdec2_ctx *ctx);
	int (*run)(struct rkvdec2_ctx *ctx);
	void (*done)(struct rkvdec2_ctx *ctx, struct vb2_v4l2_buffer *src_buf,
		     struct vb2_v4l2_buffer *dst_buf,
		     enum vb2_buffer_state result);
	int (*try_ctrl)(struct rkvdec2_ctx *ctx, struct v4l2_ctrl *ctrl);
};

struct rkvdec2_coded_fmt_desc {
	u32 fourcc;
	struct v4l2_frmsize_stepwise frmsize;
	const struct rkvdec2_ctrls *ctrls;
	const struct rkvdec2_coded_fmt_ops *ops;
	unsigned int num_decoded_fmts;
	const u32 *decoded_fmts;
	u32 subsystem_flags;
};

struct rkvdec2_dev {
	struct v4l2_device v4l2_dev;
	struct media_device mdev;
	struct video_device vdev;
	struct v4l2_m2m_dev *m2m_dev;
	struct device *dev;
	struct clk_bulk_data *clocks;
	void __iomem *regs;
	struct gen_pool *sram_pool;
	struct mutex vdev_lock; /* serializes ioctls */
	struct delayed_work watchdog_work;
};

struct rkvdec2_ctx {
	struct v4l2_fh fh;
	struct v4l2_format coded_fmt;
	struct v4l2_format decoded_fmt;
	const struct rkvdec2_coded_fmt_desc *coded_fmt_desc;
	struct v4l2_ctrl_handler ctrl_hdl;
	struct rkvdec2_dev *dev;
	struct rkvdec2_aux_buf rcb_bufs[RKVDEC2_RCB_COUNT];

	u32 colmv_offset;

	void *priv;
};

static inline struct rkvdec2_ctx *fh_to_rkvdec2_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct rkvdec2_ctx, fh);
}

void rkvdec2_run_preamble(struct rkvdec2_ctx *ctx, struct rkvdec2_run *run);
void rkvdec2_run_postamble(struct rkvdec2_ctx *ctx, struct rkvdec2_run *run);

extern const struct rkvdec2_coded_fmt_ops rkvdec2_h264_fmt_ops;

#endif /* RKVDEC_H_ */
