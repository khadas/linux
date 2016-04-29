/*
 * ../../drivers/amlogic/video_dev/amlvideo2.c
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <media/videobuf-res.h>
#include <media/videobuf-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <linux/types.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include "common/vfp-queue.h"
#include <linux/amlogic/ge2d/ge2d.h>
#include <linux/amlogic/amports/timestamp.h>
#include <linux/kernel.h>
#include <linux/amlogic/tvin/tvin_v4l2.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include "amlvideo2.h"

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
/* #include <mach/mod_gate.h> */
/* #endif */

#include <linux/of.h>
#include <linux/of_fdt.h>


#include <linux/semaphore.h>
#include <linux/sched/rt.h>

#define AVMLVIDEO2_MODULE_NAME "amlvideo2"
#define AMLVIDEO2_RES_CANVAS 0xc0

/* #define USE_SEMA_QBUF */
/* #define USE_VDIN_PTS */

/* #define MUTLI_NODE */
#ifdef MUTLI_NODE
#define MAX_SUB_DEV_NODE 2
#else
#define MAX_SUB_DEV_NODE 1
#endif

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001

#define AMLVIDEO2_MAJOR_VERSION 0
#define AMLVIDEO2_MINOR_VERSION 7
#define AMLVIDEO2_RELEASE 1
#define AMLVIDEO2_VERSION \
KERNEL_VERSION(\
	       AMLVIDEO2_MAJOR_VERSION, AMLVIDEO2_MINOR_VERSION,\
	       AMLVIDEO2_RELEASE)

#define MAGIC_SG_MEM 0x17890714
#define MAGIC_DC_MEM 0x0733ac61
#define MAGIC_VMAL_MEM 0x18221223
#define MAGIC_RE_MEM 0x123039dc

#ifdef MUTLI_NODE
#define RECEIVER_NAME0 "amlvideo2_0"
#define RECEIVER_NAME1 "amlvideo2_1"
#else
#define RECEIVER_NAME "amlvideo2"
#define DEVICE_NAME   "amlvideo2"
#endif

#define VM_RES0_CANVAS_INDEX AMLVIDEO2_RES_CANVAS
#define VM_RES0_CANVAS_INDEX_U (AMLVIDEO2_RES_CANVAS+1)
#define VM_RES0_CANVAS_INDEX_V (AMLVIDEO2_RES_CANVAS+2)
#define VM_RES0_CANVAS_INDEX_UV (VM_RES0_CANVAS_INDEX_U)

#ifdef MUTLI_NODE
#define VM_RES1_CANVAS_INDEX (AMLVIDEO2_RES_CANVAS+3)
#define VM_RES1_CANVAS_INDEX_U (AMLVIDEO2_RES_CANVAS+4)
#define VM_RES1_CANVAS_INDEX_V (AMLVIDEO2_RES_CANVAS+5)
#define VM_RES1_CANVAS_INDEX_UV VM_RES1_CANVAS_INDEX_U
#endif

#define DUR2PTS(x) ((x) - ((x) >> 4))
#define DUR2PTS_RM(x) ((x) & 0xf)

#define CMA_ALLOC_SIZE 24

MODULE_DESCRIPTION(
	"pass a frame of amlogic video2 codec device  to user in style of v4l2");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL");
static unsigned video_nr = 11;
/* module_param(video_nr, uint, 0644); */
/* MODULE_PARM_DESC(video_nr, "videoX start number, 10 is defaut"); */

static unsigned debug;
/* module_param(debug, uint, 0644); */
/* MODULE_PARM_DESC(debug, "activates debug info"); */

static int amlvideo2_pool_size;

static struct vfq_s q_ready;
/*void vf_inqueue(struct vframe_s *vf, const char *receiver);*/

struct vframe_s *amlvideo2_pool_ready;
static bool video_blocking;
struct timeval thread_ts1;
struct timeval thread_ts2;
static int frameInv_adjust;
static int frameInv;
static struct vframe_s *tmp_vf;
static int frame_inittime = 1;
#define DEF_FRAMERATE 30

static unsigned int vid_limit = 16;
module_param(vid_limit, uint, 0644);
MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static struct v4l2_fract amlvideo2_frmintervals_active = {
	.numerator = 1, .denominator = DEF_FRAMERATE, };

/* supported controls */
static struct v4l2_queryctrl amlvideo2_node_qctrl[] = {{
	.id = V4L2_CID_ROTATE, .type = V4L2_CTRL_TYPE_INTEGER, .name = "Rotate",
	.minimum = 0, .maximum = 270, .step = 90, .default_value = 0, .flags =
		V4L2_CTRL_FLAG_SLIDER, } };

static struct v4l2_frmivalenum amlvideo2_frmivalenum[] = {{
	.index = 0, .pixel_format = V4L2_PIX_FMT_NV21, .width = 1920, .height =
		1080, .type = V4L2_FRMIVAL_TYPE_DISCRETE, {.discrete = {
		.numerator = 1, .denominator = 30, } } }, {
	.index = 1, .pixel_format = V4L2_PIX_FMT_NV21, .width = 1600, .height =
		1200, .type = V4L2_FRMIVAL_TYPE_DISCRETE, {.discrete = {
		.numerator = 1, .denominator = 5, } } }, };
#define dpr_err(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
 Basic structures
 ------------------------------------------------------------------*/

enum aml_provider_type_e {
	AML_PROVIDE_NONE = 0,
	AML_PROVIDE_MIRROCAST_VDIN0 = 1,
	AML_PROVIDE_MIRROCAST_VDIN1 = 2,
	AML_PROVIDE_HDMIIN_VDIN0 = 3,
	AML_PROVIDE_HDMIIN_VDIN1 = 4,
	AML_PROVIDE_DECODE = 5,
	AML_PROVIDE_PPMGR = 6,
	AML_PROVIDE_MAX = 7
};

enum aml_receiver_type_e {
	AML_RECEIVER_NONE = 0,
	AML_RECEIVER_PPMGR,
	AML_RECEIVER_DI,
	AML_RECEIVER_MAX
};

struct amlvideo2_fmt {
	char *name;
	u32 fourcc; /* v4l2 format id */
	int depth;
};

static struct amlvideo2_fmt formats[] = {
{.name = "RGB565 (BE)",
.fourcc = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
.depth = 16, },

{.name = "RGB888 (24)",
.fourcc = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
.depth = 24, },

{.name = "BGR888 (24)",
.fourcc = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
.depth = 24, },

{.name = "12  Y/CbCr 4:2:0",
.fourcc = V4L2_PIX_FMT_NV12,
.depth = 12, },

{.name = "12  Y/CbCr 4:2:0",
.fourcc = V4L2_PIX_FMT_NV21,
.depth = 12, },

{.name = "YUV420P",
.fourcc = V4L2_PIX_FMT_YUV420,
.depth = 12, },

{.name = "YVU420P",
.fourcc = V4L2_PIX_FMT_YVU420,
.depth = 12, }

#if 0
	{
		.name = "RGBA8888 (32)",
		.fourcc = V4L2_PIX_FMT_RGB32, /* 24  RGBA-8-8-8-8 */
		.depth = 32,
	},
	{
		.name = "RGB565 (BE)",
		.fourcc = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth = 16,
	},
	{
		.name = "BGR888 (24)",
		.fourcc = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
		.depth = 24,
	},
	{
		.name = "YUV420P",
		.fourcc = V4L2_PIX_FMT_YUV420,
		.depth = 12,
	},
#endif
};

static struct amlvideo2_fmt *get_format(struct v4l2_format *f)
{
	struct amlvideo2_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

/* buffer for one video frame */
struct amlvideo2_frame_info {
	int x;
	int y;
	int w;
	int h;
};

struct amlvideo2_node_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct amlvideo2_fmt *fmt;
	int canvas_id;
	struct amlvideo2_frame_info axis;
};

struct amlvideo2_node_dmaqueue {
	struct list_head active;

	/* thread for generating video stream*/
	struct task_struct *kthread;
	wait_queue_head_t wq;
	unsigned char task_running;
#ifdef USE_SEMA_QBUF
	struct completion qbuf_comp;
#endif
};

struct amlvideo2_device {
struct mutex mutex;
struct v4l2_device v4l2_dev;
struct platform_device *pdev;
struct amlvideo2_node *node[MAX_SUB_DEV_NODE];
int node_num;
resource_size_t buffer_start;
unsigned int buffer_size;
struct page *cma_pages;
struct resource memobj;
int cma_mode;
bool use_reserve;

};

struct amlvideo2_fh;
struct amlvideo2_node {

spinlock_t slock;
struct mutex mutex;
int vid;
int users;

struct amlvideo2_device *vid_dev;

/* various device info */
struct video_device *vfd;

struct amlvideo2_node_dmaqueue vidq;

/* Control 'registers' */
int qctl_regs[ARRAY_SIZE(amlvideo2_node_qctrl)];

struct videobuf_res_privdata res;
struct vframe_receiver_s recv;
struct vframe_receiver_s *sub_recv;
struct vframe_provider_s *provider;
enum aml_provider_type_e p_type;
enum aml_receiver_type_e r_type;
int provide_ready;

struct amlvideo2_fh *fh;
unsigned int input; /* 0:mirrocast; 1:hdmiin */
enum tvin_port_e porttype;
unsigned int start_vdin_flag;
struct ge2d_context_s *context;
struct vdin_v4l2_ops_s vops;
int vdin_device_num;
};

struct amlvideo2_fh {
struct amlvideo2_node *node;

/* video capture */
struct amlvideo2_fmt *fmt;
unsigned int width, height;
struct videobuf_queue vb_vidq;
unsigned int is_streamed_on;

suseconds_t frm_save_time_us; /* us */
unsigned int f_flags;
enum v4l2_buf_type type;
};

struct amlvideo2_output {
int canvas_id;
void *vbuf;
int width;
int height;
u32 v4l2_format;
int angle;
struct amlvideo2_frame_info *frame;
};

static struct v4l2_frmsize_discrete amlvideo2_prev_resolution[] = {{160, 120}, {
320, 240}, {640, 480}, {1280, 720}, };

static struct v4l2_frmsize_discrete amlvideo2_pic_resolution[] = {{1280, 960} };

static unsigned print_cvs_idx;
module_param(print_cvs_idx, uint, 0644);
MODULE_PARM_DESC(print_cvs_idx, "print canvas index\n");

int get_amlvideo2_canvas_index(struct amlvideo2_output *output,
				int start_canvas)
{
int canvas = start_canvas;
int v4l2_format = output->v4l2_format;
void *buf = (void *)output->vbuf;
int width = output->width;
int height = output->height;
int canvas_height = height;
if (canvas_height % 16 != 0)
	canvas_height = ((canvas_height + 15) >> 4) << 4;

switch (v4l2_format) {
case V4L2_PIX_FMT_RGB565X:
case V4L2_PIX_FMT_VYUY:
	canvas = start_canvas;
	canvas_config(canvas, (unsigned long)buf, width * 2, canvas_height,
	CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	break;
case V4L2_PIX_FMT_YUV444:
case V4L2_PIX_FMT_BGR24:
case V4L2_PIX_FMT_RGB24:
	canvas = start_canvas;
	canvas_config(canvas, (unsigned long)buf, width * 3, canvas_height,
	CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	break;
case V4L2_PIX_FMT_NV12:
case V4L2_PIX_FMT_NV21:
	canvas_config(start_canvas, (unsigned long)buf, width, canvas_height,
	CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	canvas_config(start_canvas + 1, (unsigned long)(buf + width * height),
			width, canvas_height / 2,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	canvas = start_canvas | ((start_canvas + 1) << 8);
	break;
case V4L2_PIX_FMT_YVU420:
case V4L2_PIX_FMT_YUV420:
	canvas_config(start_canvas, (unsigned long)buf, width, canvas_height,
	CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	canvas_config(start_canvas + 1, (unsigned long)(buf + width * height),
			width / 2, canvas_height / 2,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	canvas_config(start_canvas + 2,
			(unsigned long)(buf + width * height * 5 / 4),
			width / 2, canvas_height / 2,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_LINEAR);
	if (V4L2_PIX_FMT_YUV420 == v4l2_format) {
		canvas = start_canvas | ((start_canvas + 1) << 8)
				| ((start_canvas + 2) << 16);
	} else {
		canvas = start_canvas | ((start_canvas + 2) << 8)
				| ((start_canvas + 1) << 16);
	}
	break;
default:
	break;
}
if (print_cvs_idx == 1) {
	pr_err("v4l2_format=%x, canvas=%x\n", v4l2_format, canvas);
	print_cvs_idx = 0;
}
return canvas;
}
/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
int convert_canvas_index(struct amlvideo2_output *output, int start_canvas)
{
	int canvas = start_canvas;
	int v4l2_format = output->v4l2_format;

	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_VYUY:
		canvas = start_canvas;
		break;
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
		canvas = start_canvas;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		canvas = start_canvas | ((start_canvas + 1) << 8);
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
		if (V4L2_PIX_FMT_YUV420 == v4l2_format) {
			canvas = start_canvas | ((start_canvas + 1) << 8)
					| ((start_canvas + 2) << 16);
		} else {
			canvas = start_canvas | ((start_canvas + 2) << 8)
					| ((start_canvas + 1) << 16);
		}
		break;
	default:
		break;
	}
	return canvas;
}
/* #endif */

static unsigned int print_ifmt;
module_param(print_ifmt, uint, 0644);
MODULE_PARM_DESC(print_ifmt, "print input format\n");

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;
	if (vf->type & VIDTYPE_VIU_422) {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM) {
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		} else if (vf->type & VIDTYPE_INTERLACE_TOP) {
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		} else {
			format = GE2D_FORMAT_S16_YUV422;
		}
	} else if (vf->type & VIDTYPE_VIU_NV21) {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM) {
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		} else if (vf->type & VIDTYPE_INTERLACE_TOP) {
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		} else {
			format = GE2D_FORMAT_M24_NV21;
		}
	} else {
		if (vf->type & VIDTYPE_INTERLACE_BOTTOM) {
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420B & (3 << 3));
		} else if (vf->type & VIDTYPE_INTERLACE_TOP) {
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		} else {
			format = GE2D_FORMAT_M24_YUV420;
		}
	}
	if (print_ifmt == 1) {
		pr_err(
		"vf->type=%x, format=%x, w*h=%dx%d, canvas0=%x, canvas1=%x\n",
		vf->type, format, vf->width, vf->height, vf->canvas0Addr,
		vf->canvas1Addr);

		pr_err(
	"vf->type=%x, VIDTYPE_INTERLACE_BOTTOM=%x, VIDTYPE_INTERLACE_TOP=%x\n",
	vf->type, VIDTYPE_INTERLACE_BOTTOM, VIDTYPE_INTERLACE_TOP);
		print_ifmt = 0;
	}
	return format;
}

static int get_input_format_no_interlace(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;
	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422;
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21;
	else
		format = GE2D_FORMAT_M24_YUV420;

	if (print_ifmt == 1) {
		pr_err(
		"vf->type=%x, format=%x, w*h=%dx%d, canvas0=%x, canvas1=%x\n",
		vf->type, format, vf->width, vf->height, vf->canvas0Addr,
		vf->canvas1Addr);

		pr_err(
	"vf->type=%x, VIDTYPE_INTERLACE_BOTTOM=%x, VIDTYPE_INTERLACE_TOP=%x\n",
	vf->type, VIDTYPE_INTERLACE_BOTTOM, VIDTYPE_INTERLACE_TOP);
		print_ifmt = 0;
	}
	return format;
}

static int get_interlace_input_format(struct vframe_s *vf,
					struct amlvideo2_output *output)
{
	int format = GE2D_FORMAT_M24_NV21;
	if (vf->type & VIDTYPE_VIU_422) {
		format = GE2D_FORMAT_S16_YUV422;
		if (vf->height >= output->height << 2) {
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		}
	} else if (vf->type & VIDTYPE_VIU_NV21) {
		format = GE2D_FORMAT_M24_NV21;
		if (vf->height >= output->height << 2) {
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		}
	} else {
		format = GE2D_FORMAT_M24_YUV420;
		if (vf->height >= output->height << 2) {
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		}
	}
	if (print_ifmt == 1) {
		pr_err(
		"vf->type=%x, format=%x, w*h=%dx%d, canvas0=%x, canvas1=%x\n",
		vf->type, format, vf->width, vf->height, vf->canvas0Addr,
		vf->canvas1Addr);
		print_ifmt = 0;
	}
	return format;
}

static inline int get_top_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;
	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422 |
			(GE2D_FORMAT_S16_YUV422T & (3 << 3));
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21 |
			(GE2D_FORMAT_M24_NV21T & (3 << 3));
	else
		format = GE2D_FORMAT_M24_YUV420 |
			(GE2D_FORMAT_M24_YUV420T & (3 << 3));

	if (print_ifmt == 1) {
		pr_err(
		"vf->type=%x, format=%x, w*h=%dx%d, canvas0=%x, canvas1=%x\n",
		vf->type, format, vf->width, vf->height, vf->canvas0Addr,
		vf->canvas1Addr);
		print_ifmt = 0;
	}
	return format;
}

static inline int get_bottom_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;
	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422 |
			(GE2D_FORMAT_S16_YUV422B & (3 << 3));
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21 |
			(GE2D_FORMAT_M24_NV21B & (3 << 3));
	else
		format =
			GE2D_FORMAT_M24_YUV420 |
			(GE2D_FMT_M24_YUV420B & (3 << 3));


	if (print_ifmt == 1) {
		pr_err(
		"vf->type=%x, format=%x, w*h=%dx%d, canvas0=%x, canvas1=%x\n",
		vf->type, format, vf->width, vf->height, vf->canvas0Addr,
		vf->canvas1Addr);
		print_ifmt = 0;
	}
	return format;
}

static unsigned int print_ofmt;
module_param(print_ofmt, uint, 0644);
MODULE_PARM_DESC(print_ofmt, "print output format\n");

static int get_output_format(int v4l2_format)
{
	int format = GE2D_FORMAT_S24_YUV444;
	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
		format = GE2D_FORMAT_S16_RGB_565;
		break;
	case V4L2_PIX_FMT_YUV444:
		format = GE2D_FORMAT_S24_YUV444;
		break;
	case V4L2_PIX_FMT_VYUY:
		format = GE2D_FORMAT_S16_YUV422;
		break;
	case V4L2_PIX_FMT_BGR24:
		format = GE2D_FORMAT_S24_RGB;
		break;
	case V4L2_PIX_FMT_RGB24:
		format = GE2D_FORMAT_S24_BGR;
		break;
	case V4L2_PIX_FMT_NV12:
		format = GE2D_FORMAT_M24_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
		format = GE2D_FORMAT_M24_NV21;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		format = GE2D_FORMAT_S8_Y;
		break;
	default:
		break;
	}
	if (print_ofmt == 1) {
		pr_err("outputformat, v4l2_format=%x, format=%x\n", v4l2_format,
			format);
		print_ofmt = 0;
	}
	return format;
}

static void output_axis_adjust(int src_w, int src_h, int *dst_w, int *dst_h,
				int angle)
{
	int w = 0, h = 0, disp_w = 0, disp_h = 0;
	disp_w = *dst_w;
	disp_h = *dst_h;
	if (angle % 180 != 0) {
		h = min_t(int, (int)src_w, disp_h);
		w = src_h * h / src_w;
		if (w > disp_w) {
			h = (h * disp_w) / w;
			w = disp_w;
		}
	} else {
		if ((src_w < disp_w) && (src_h < disp_h)) {
			w = src_w;
			h = src_h;
		} else if ((src_w * disp_h) > (disp_w * src_h)) {
			w = disp_w;
			h = disp_w * src_h / src_w;
		} else {
			h = disp_h;
			w = disp_h * src_w / src_h;
		}
	}
	*dst_w = w;
	*dst_h = h;
}

int amlvideo2_ge2d_interlace_two_canvasAddr_process(
struct vframe_s *vf, struct ge2d_context_s *context,
struct config_para_ex_s *ge2d_config, struct amlvideo2_output *output,
enum aml_provider_type_e p_type)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	int output_canvas = vf->canvas0Addr;

	/* ============================ */
	/* top field */
	/* ============================ */
	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	/* src_height = vf->height/2; */
	src_height = vf->height;

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if ((p_type == AML_PROVIDE_MIRROCAST_VDIN0) ||
		(p_type == AML_PROVIDE_MIRROCAST_VDIN1)) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	output_axis_adjust(
		src_width, src_height, &dst_width, &dst_height, cur_angle);
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	canvas_read(vf->canvas0Addr & 0xff, &cs0);
	canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
	canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	ge2d_config->src_planes[1].addr = cs1.addr;
	ge2d_config->src_planes[1].w = cs1.width;
	ge2d_config->src_planes[1].h = cs1.height;
	ge2d_config->src_planes[2].addr = cs2.addr;
	ge2d_config->src_planes[2].w = cs2.width;
	ge2d_config->src_planes[2].h = cs2.height;
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format(vf);
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height); */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = (get_output_format(output->v4l2_format)
		| GE2D_LITTLE_ENDIAN)
					| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height / 2;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(
		context, src_left, src_top / 2, src_width, src_height / 2,
		output->frame->x, output->frame->y / 2,
		output->frame->w, output->frame->h / 2);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 4, output->frame->w / 2,
			output->frame->h / 4);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 4, output->frame->w / 2,
			output->frame->h / 4);
	}

	/* ============================ */
	/* bottom field */
	/* ============================ */
	output_canvas = vf->canvas1Addr;
	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	src_height = vf->height;

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if (src_width < src_height)
		cur_angle = (cur_angle + 90) % 360;

	output_axis_adjust(
		src_width, src_height,
		&dst_width, &dst_height, cur_angle);
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	canvas_read(vf->canvas1Addr & 0xff, &cs0);
	canvas_read((vf->canvas1Addr >> 8) & 0xff, &cs1);
	canvas_read((vf->canvas1Addr >> 16) & 0xff, &cs2);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	ge2d_config->src_planes[1].addr = cs1.addr;
	ge2d_config->src_planes[1].w = cs1.width;
	ge2d_config->src_planes[1].h = cs1.height;
	ge2d_config->src_planes[2].addr = cs2.addr;
	ge2d_config->src_planes[2].w = cs2.width;
	ge2d_config->src_planes[2].h = cs2.height;
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = vf->canvas1Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format(vf);
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;
	/* pr_err("vf_width is %d ,
	 *  vf_height is %d\n",vf->width ,vf->height); */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = (get_output_format(output->v4l2_format)
		| GE2D_LITTLE_ENDIAN)
					| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height / 2;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(
		context, src_left, src_top / 2, src_width, src_height / 2,
		output->frame->x, output->frame->y / 2,
		output->frame->w, output->frame->h / 2);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 4, output->frame->w / 2,
			output->frame->h / 4);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 4, output->frame->w / 2,
			output->frame->h / 4);
	}

	return output_canvas;
}

int amlvideo2_ge2d_interlace_vdindata_process(
struct vframe_s *vf, struct ge2d_context_s *context,
struct config_para_ex_s *ge2d_config, struct amlvideo2_output *output,
enum aml_provider_type_e p_type)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	src_height = vf->height;

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if ((p_type == AML_PROVIDE_MIRROCAST_VDIN0) ||
		(p_type == AML_PROVIDE_MIRROCAST_VDIN1)) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	output_axis_adjust(
		src_width, src_height, &dst_width, &dst_height, cur_angle);
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	src_height = vf->height / 2;
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	canvas_read(vf->canvas0Addr & 0xff, &cs0);
	canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
	canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	ge2d_config->src_planes[1].addr = cs1.addr;
	ge2d_config->src_planes[1].w = cs1.width;
	ge2d_config->src_planes[1].h = cs1.height;
	ge2d_config->src_planes[2].addr = cs2.addr;
	ge2d_config->src_planes[2].w = cs2.width;
	ge2d_config->src_planes[2].h = cs2.height;
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_interlace_input_format(vf, output);
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = src_height;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height); */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(
		output->v4l2_format)|GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(
		context, src_left, src_top, src_width, src_height,
		output->frame->x, output->frame->y, output->frame->w,
		output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top, src_width, src_height,
			output->frame->x / 2, output->frame->y / 2,
			output->frame->w / 2, output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top, src_width, src_height,
			output->frame->x / 2, output->frame->y / 2,
			output->frame->w / 2, output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_ge2d_interlace_one_canvasAddr_process(
struct vframe_s *vf, struct ge2d_context_s *context,
struct config_para_ex_s *ge2d_config, struct amlvideo2_output *output,
enum aml_provider_type_e p_type)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	/* src_height = vf->height/2; */
	src_height = vf->height;

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if ((p_type == AML_PROVIDE_MIRROCAST_VDIN0) ||
		(p_type == AML_PROVIDE_MIRROCAST_VDIN1)) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	output_axis_adjust(
		src_width, src_height,
		&dst_width, &dst_height, cur_angle);
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	canvas_read(vf->canvas0Addr & 0xff, &cs0);
	canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
	canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	ge2d_config->src_planes[1].addr = cs1.addr;
	ge2d_config->src_planes[1].w = cs1.width;
	ge2d_config->src_planes[1].h = cs1.height;
	ge2d_config->src_planes[2].addr = cs2.addr;
	ge2d_config->src_planes[2].w = cs2.width;
	ge2d_config->src_planes[2].h = cs2.height;
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format_no_interlace(vf);
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height); */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(
		output->v4l2_format)|GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(
		context, src_left, src_top / 2, src_width, src_height / 2,
		output->frame->x, output->frame->y, output->frame->w,
		output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 2, output->frame->w / 2,
			output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
			src_height / 2, output->frame->x / 2,
			output->frame->y / 2, output->frame->w / 2,
			output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_ge2d_pre_process(struct vframe_s *vf,
				struct ge2d_context_s *context,
				struct config_para_ex_s *ge2d_config,
				struct amlvideo2_output *output,
				enum aml_provider_type_e p_type)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror = 0;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	src_height = vf->height;

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = 0;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if ((p_type == AML_PROVIDE_MIRROCAST_VDIN0) ||
		(p_type == AML_PROVIDE_MIRROCAST_VDIN1)) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	output_axis_adjust(
		src_width, src_height,
		&dst_width, &dst_height, cur_angle);
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if ((dst_left != output->frame->x) || (dst_top != output->frame->y)
		|| (dst_width != output->frame->w)
		|| (dst_height != output->frame->h)) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->width;
		ge2d_config->src_para.height = output->height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format(
			output->v4l2_format)|GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->width;
		ge2d_config->dst_para.height = output->height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		fillrect(
			context,
			0,
			0,
			output->width,
			output->height,
			(ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
				0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	canvas_read(vf->canvas0Addr & 0xff, &cs0);
	canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
	canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	ge2d_config->src_planes[1].addr = cs1.addr;
	ge2d_config->src_planes[1].w = cs1.width;
	ge2d_config->src_planes[1].h = cs1.height;
	ge2d_config->src_planes[2].addr = cs2.addr;
	ge2d_config->src_planes[2].w = cs2.width;
	ge2d_config->src_planes[2].h = cs2.height;
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format(vf);
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height); */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if ((output->v4l2_format != V4L2_PIX_FMT_YUV420) && (output->v4l2_format
		!= V4L2_PIX_FMT_YVU420))
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(
		output->v4l2_format)|GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, src_left, src_top, src_width, src_height,
		output->frame->x, output->frame->y, output->frame->w,
		output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if ((output->v4l2_format == V4L2_PIX_FMT_YUV420) || (output->v4l2_format
		== V4L2_PIX_FMT_YVU420)) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(
			context, src_left, src_top, src_width, src_height,
			output->frame->x / 2, output->frame->y / 2,
			output->frame->w / 2, output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context,
			src_left, src_top, src_width, src_height,
			output->frame->x / 2, output->frame->y / 2,
			output->frame->w / 2, output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_sw_post_process(int canvas, void *addr)
{
	return 0;
}

static int amlvideo2_fillbuff(struct amlvideo2_fh *fh,
				struct amlvideo2_node_buffer *buf,
				struct vframe_s *vf)
{
	struct config_para_ex_s ge2d_config;
	struct amlvideo2_output output;
	struct amlvideo2_node *node = fh->node;
	void *vbuf = NULL;
	int src_canvas = -1;
	int magic = 0;
	int ge2d_proc = 0;
	int sw_proc = 0;

	vbuf = (void *)videobuf_to_res(&buf->vb);

	dpr_err(node->vid_dev, 1, "%s\n", __func__);
	if (!vbuf)
		return -1;

	/* memset(&ge2d_config,0,sizeof(struct config_para_ex_s)); */
	memset(&output, 0, sizeof(struct amlvideo2_output));

	output.v4l2_format = fh->fmt->fourcc;
	output.vbuf = vbuf;
	output.width = buf->vb.width;
	output.height = buf->vb.height;
	output.canvas_id = buf->canvas_id;
	output.angle = node->qctl_regs[0];
	output.frame = &buf->axis;

	magic = MAGIC_RE_MEM;
	switch (magic) {
	case MAGIC_RE_MEM:
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	#if 1
		if (output.canvas_id == 0) {
			output.canvas_id = get_amlvideo2_canvas_index(
				&output, VM_RES0_CANVAS_INDEX + buf->vb.i * 3);
			buf->canvas_id = output.canvas_id;
		}
	#else
	#ifdef MUTLI_NODE
		output.canvas_id =
			get_amlvideo2_canvas_index(
				&output, (node->vid == 0)
				? VM_RES0_CANVAS_INDEX
				: VM_RES1_CANVAS_INDEX);
	#else
		output.canvas_id =
			get_amlvideo2_canvas_index(
				&output, VM_RES0_CANVAS_INDEX);
	#endif
	#endif
		break;
	case MAGIC_VMAL_MEM:
		/* canvas_index =
		 * get_amlvideo2_canvas_index(
		 * v4l2_format,&depth); */
		/* sw_proc = 1; */
		/* break; */
	case MAGIC_DC_MEM:
	case MAGIC_SG_MEM:
	default:
		return -1;
	}

	switch (output.v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		ge2d_proc = 1;
		break;
	default:
		break;
	}
	src_canvas = vf->canvas0Addr;
	if (ge2d_proc) {
		if ((vf->type & VIDTYPE_INTERLACE_BOTTOM) || (vf->type
			& VIDTYPE_INTERLACE_TOP)) {
			if (vf->canvas0Addr == vf->canvas1Addr) {
				if ((AML_PROVIDE_MIRROCAST_VDIN1
					== node->p_type)
					|| (AML_PROVIDE_MIRROCAST_VDIN0
					== node->p_type)) {
					src_canvas =
				amlvideo2_ge2d_interlace_vdindata_process(
					vf, node->context, &ge2d_config,
					&output, node->p_type);
				} else {
					src_canvas =
			amlvideo2_ge2d_interlace_one_canvasAddr_process(
				vf, node->context, &ge2d_config,
				&output, node->p_type);
				}
			} else {
				src_canvas =
			amlvideo2_ge2d_interlace_two_canvasAddr_process(
				vf, node->context, &ge2d_config,
				&output, node->p_type);
			}
		} else {
			src_canvas = amlvideo2_ge2d_pre_process(
				vf, node->context,
				&ge2d_config, &output, node->p_type);
		}
	}

	if ((sw_proc) && (src_canvas > 0))
		amlvideo2_sw_post_process(src_canvas, vbuf);

	buf->vb.state = VIDEOBUF_DONE;
	/* do_gettimeofday(&buf->vb.ts); */
	return 0;
}

static unsigned print_ivals;
module_param(print_ivals, uint, 0644);
MODULE_PARM_DESC(print_ivals, "print current intervals!!");

#define TEST_LATENCY
#ifdef TEST_LATENCY
static unsigned print_latecny;
module_param(print_latecny, uint, 0644);
MODULE_PARM_DESC(print_latecny, "print current latency!!");
static int total_latency;
static int total_latency_out;
static long long cur_time;
static long long cur_time_out;
static int frame_num;
static int frame_num_out;
static struct timeval test_time;
#endif

static int amlvideo2_thread_tick(struct amlvideo2_fh *fh)
{
	struct amlvideo2_node_buffer *buf = NULL;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	unsigned diff = 0;
	bool no_frame = false;
	struct vframe_s *vf = NULL;
	unsigned long flags = 0;
	int active_duration = 0;
	int i_ret = 0;

	dpr_err(node->vid_dev, 1, "Thread tick\n");

	if (kthread_should_stop())
		return 0;

	i_ret = wait_event_interruptible_timeout(
		dma_q->wq,
		((vf_peek(node->recv.name) != NULL)
		&& (node->provide_ready)) || (node->vidq.task_running == 0),
		msecs_to_jiffies(5000));

	if (i_ret == 0 || node->vidq.task_running == 0)
		return 0;

	if ((AML_RECEIVER_NONE != node->r_type) && vfq_full(&q_ready))
		return -1;

	if ((video_blocking) && (amlvideo2_pool_ready != NULL)) {
		vfq_init(&q_ready,
		amlvideo2_pool_size + 1,
		(struct vframe_s **)&amlvideo2_pool_ready[0]);
		video_blocking = false;
		pr_err("video blocking need to reset@!!!!!\n");
		return 0;
	}

	if (!fh->is_streamed_on) {
		dpr_err(node->vid_dev, 1, "dev doesn't stream on\n");
		while (vf_peek(node->recv.name) && (!vfq_full(&q_ready))) {
			vf = vf_get(node->recv.name);
			vf_inqueue(vf, node->recv.name);
		}
		return 0;
	}

	if (!node->provide_ready) {
		dpr_err(node->vid_dev, 1, "provide is not ready\n");
		return -1;
	}

	spin_lock_irqsave(&node->slock, flags);
	if (list_empty(&dma_q->active)) {
		dpr_err(node->vid_dev, 1, "No active queue to serve\n");
		while (vf_peek(node->recv.name) && (!vfq_full(&q_ready))) {
			vf = vf_get(node->recv.name);
			vf_inqueue(vf, node->recv.name);
		}
		goto unlock;
	}

	buf = list_entry(
		dma_q->active.next,
		struct amlvideo2_node_buffer,
		vb.queue);

	if (vf_peek(node->recv.name) == NULL) {
		no_frame = true;
	} else {
		/* drop the frame to get the last one */
		if (!vfq_full(&q_ready)) {
			vf = vf_get(node->recv.name);
			while (vf_peek(node->recv.name) != NULL) {
				vf_inqueue(vf, node->recv.name);
				if (!vfq_full(&q_ready))
					vf = vf_get(node->recv.name);
				else
					break;
			}
			if ((node->p_type == AML_PROVIDE_HDMIIN_VDIN0) ||
				(node->p_type == AML_PROVIDE_HDMIIN_VDIN1) ||
				(node->p_type == AML_PROVIDE_MIRROCAST_VDIN1)) {
				if (((vf->type & VIDTYPE_TYPEMASK)
					== VIDTYPE_INTERLACE_BOTTOM) &&
					(vf->canvas0Addr == vf->canvas1Addr)) {
					vf_inqueue(vf, node->recv.name);
					no_frame = true;
					vf = NULL;
				}
			}
		}
	}

	#ifdef USE_VDIN_PTS
	if (no_frame)
		goto unlock;
	if (frame_inittime == 1) {
		if (tmp_vf) {
			vf_inqueue(tmp_vf, node->recv.name);
			tmp_vf = NULL;
		}
		frameInv_adjust = 0;
		frameInv = 0;
		thread_ts1.tv_sec = vf->pts_us64 & 0xFFFFFFFF;
		thread_ts1.tv_usec = vf->pts;
		frame_inittime = 0;
	} else {
		diff = (vf->pts_us64 & 0xFFFFFFFF) - thread_ts1.tv_sec;
		diff = diff*1000000 + vf->pts - thread_ts1.tv_usec;
		if (diff < (int) fh->frm_save_time_us) {
			vf_inqueue(vf, node->recv.name);
			goto unlock;
		}
	}
	#else
	if (frame_inittime == 1) {
		if (tmp_vf) {
			vf_inqueue(tmp_vf, node->recv.name);
			tmp_vf = NULL;
		}
		if (no_frame)
			goto unlock;
		frameInv_adjust = 0;
		frameInv = 0;
		do_gettimeofday(&thread_ts1);
		frame_inittime = 0;
	} else {
		do_gettimeofday(&thread_ts2);
		/* thread_ts2.tv_sec = vf->pts_us64& 0xFFFFFFFF; */
		/* thread_ts2.tv_usec = vf->pts; */
		diff = thread_ts2.tv_sec - thread_ts1.tv_sec;
		diff = diff * 1000000 + thread_ts2.tv_usec - thread_ts1.tv_usec;
		frameInv += diff;
		memcpy(&thread_ts1, &thread_ts2, sizeof(struct timeval));
		active_duration = frameInv - frameInv_adjust;
		/* Fill buffer */
		if (active_duration + 5000 > (int)fh->frm_save_time_us) {
			/* ||(active_duration  > (int)fh->frm_save_time_us) */
			if (vf) {
				if (tmp_vf != NULL)
					vf_inqueue(tmp_vf, node->recv.name);

				tmp_vf = NULL;
			} else if (tmp_vf) {
				vf = tmp_vf;
				tmp_vf = NULL;
			} else {
				goto unlock;
			}
		} else {
			if (vf) {
				if (tmp_vf != NULL)
					vf_inqueue(tmp_vf, node->recv.name);

				tmp_vf = vf;
			}
			goto unlock;
		}
	}
	while (active_duration >= (int)fh->frm_save_time_us)
		active_duration -= fh->frm_save_time_us;

	if ((active_duration + 5000) > fh->frm_save_time_us)
		frameInv_adjust = fh->frm_save_time_us - active_duration;
	else
		frameInv_adjust = -active_duration;
	frameInv = 0;
	#endif
	buf->vb.state = VIDEOBUF_ACTIVE;
	list_del(&buf->vb.queue);

	spin_unlock_irqrestore(&node->slock, flags);

	/* test latency */
	#ifdef TEST_LATENCY
	if (print_latecny) {
		int timeNow64, timePts;
		do_gettimeofday(&test_time);
		timeNow64 = ((test_time.tv_sec & 0xFFFFFFFF) * 1000 * 1000)
			+ (test_time.tv_usec);
		/* timePts =  ((vf->pts_us64
		 * & 0xFFFFFFFF)*1000*1000) + (vf->pts); */
		timePts = ((thread_ts2.tv_sec & 0xFFFFFFFF) * 1000 * 1000)
			+ (thread_ts2.tv_usec);
		/* pr_err("amlvideo2 in  num:%d delay:%d\n",
		 * timePts, (timeNow64 - timePts)/1000); */
		if (cur_time == test_time.tv_sec) {
			total_latency += (int)((timeNow64 - timePts) / 1000);
			frame_num++;
		} else {
			total_latency += (int)((timeNow64 - timePts) / 1000);
			frame_num++;
			pr_err("amvideo2 in latency:%d frame_num:%d\n",
				total_latency / frame_num, frame_num);
			total_latency = 0;
			frame_num = 0;
			cur_time = test_time.tv_sec;
		}
	}
	#endif

	amlvideo2_fillbuff(fh, buf, vf);
	#ifdef USE_VDIN_PTS
	buf->vb.ts.tv_sec = vf->pts_us64 & 0xFFFFFFFF;
	buf->vb.ts.tv_usec = vf->pts;
	thread_ts1.tv_sec = vf->pts_us64 & 0xFFFFFFFF;
	thread_ts1.tv_usec = vf->pts;
	#else
	buf->vb.ts.tv_sec = thread_ts2.tv_sec & 0xFFFFFFFF;
	buf->vb.ts.tv_usec = thread_ts2.tv_usec;
	#endif
	vf_inqueue(vf, node->recv.name);

	while ((vf_peek(node->recv.name) != NULL) && (!vfq_full(&q_ready))) {
		vf = vf_get(node->recv.name);
		vf_inqueue(vf, node->recv.name);
	}
	dpr_err(node->vid_dev, 1, "filled buffer %p\n", buf);

	if (waitqueue_active(&buf->vb.done))
		wake_up(&buf->vb.done);


	/* test latency */
	#ifdef TEST_LATENCY
	if (print_latecny) {
		int timeNow64_out, timePts_out;
		do_gettimeofday(&test_time);
		timeNow64_out = ((test_time.tv_sec & 0xFFFFFFFF) * 1000 * 1000)
			+ (test_time.tv_usec);
		timePts_out = ((buf->vb.ts.tv_sec & 0xFFFFFFFF) * 1000 * 1000)
			+ (buf->vb.ts.tv_usec);
		/* pr_err("amlvideo2 out num:%d delay:%d\n",
		 * timePts_out, (timeNow64_out - timePts_out)/1000); */
		if (cur_time_out == test_time.tv_sec) {
			total_latency_out +=
				(timeNow64_out - timePts_out) / 1000;
			frame_num_out++;
		} else {
			total_latency_out +=
				(timeNow64_out - timePts_out) / 1000;
			frame_num_out++;
			pr_err("amvideo2 out latency:%d frame_num_out:%d\n",
				total_latency_out / frame_num_out,
				frame_num_out);
			total_latency_out = 0;
			frame_num_out = 0;
			cur_time_out = test_time.tv_sec;
		}
	}
	#endif
	dpr_err(node->vid_dev, 2, "[%p/%d] wakeup\n", buf, buf->vb.i);
	return 0;

unlock: spin_unlock_irqrestore(&node->slock, flags);
	return 0;
}

static void amlvideo2_sleep(struct amlvideo2_fh *fh)
{
	/* struct amlvideo2_node *node = fh->node; */
	/* struct amlvideo2_node_dmaqueue *dma_q = &node->vidq; */

	/* DECLARE_WAITQUEUE(wait, current); */

	/* dpr_err(node->vid_dev, 1, "%s dma_q=0x%08lx\n", __func__, */
	/* (unsigned long)dma_q); */

	/* add_wait_queue(&dma_q->wq, &wait); */
	/* if (kthread_should_stop()) */
	/* goto stop_task; */

	/* Calculate time to wake up */
	/* timeout = msecs_to_jiffies(frames_to_ms(1)); */

	if (amlvideo2_thread_tick(fh) < 0)
		schedule_timeout_interruptible(1);

	/* stop_task: */
	/* remove_wait_queue(&dma_q->wq, &wait); */
	try_to_freeze();
}

static int amlvideo2_thread(void *data)
{
	struct amlvideo2_fh *fh = data;
	struct amlvideo2_node *node = fh->node;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};
	int ret = 0;
	sched_setscheduler(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);

	dpr_err(node->vid_dev, 1, "thread started\n");

	set_freezable();

	while (1) {
		if (kthread_should_stop())
			break;

	#ifdef USE_SEMA_QBUF
		ret = wake_up_interruptible(&node->vidq.qbuf_comp);
	#endif
		if (!node->vidq.task_running)
			break;

		amlvideo2_sleep(fh);
		if (kthread_should_stop())
			break;
	}
	while (!kthread_should_stop())
		usleep_range(9000, 10000);
		/*msleep(10);*/

	tmp_vf = NULL;
	dpr_err(node->vid_dev, 1, "thread: exit\n");
	return ret;
}

static int amlvideo2_start_thread(struct amlvideo2_fh *fh)
{
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	#ifdef USE_SEMA_QBUF
	init_completion(&dma_q->qbuf_comp);
	#endif
	dpr_err(node->vid_dev, 1, "%s\n", __func__);

	if (dma_q->task_running)
		return 0;

	tmp_vf = NULL;
	dma_q->task_running = 1;

	#ifdef MUTLI_NODE
	dma_q->kthread =
		kthread_run(amlvideo2_thread, fh,
		(node->vid == 0)?"amlvideo2-0":"amlvideo2-1");
	#else
	dma_q->kthread = kthread_run(amlvideo2_thread, fh, "amlvideo2");
	#endif

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&node->vid_dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	/* wake_up_interruptible(&dma_q->wq); */

	dpr_err(node->vid_dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void amlvideo2_stop_thread(struct amlvideo2_node_dmaqueue *dma_q)
{
	struct amlvideo2_node *node =
		container_of(dma_q, struct amlvideo2_node, vidq);
	dpr_err(node->vid_dev, 1, "%s\n", __func__);
	/* shutdown control thread */
	if (dma_q->kthread) {
		dma_q->task_running = 0;
		send_sig(SIGTERM, dma_q->kthread, 1);
	#ifdef USE_SEMA_QBUF
		complete(&dma_q->qbuf_comp);
	#endif
		wake_up_interruptible(&dma_q->wq);
		kthread_stop(dma_q->kthread);
		dma_q->kthread = NULL;
	}
}

enum aml_provider_type_e get_provider_type(const char *name, int input)
{
	enum aml_provider_type_e type = AML_PROVIDE_NONE;
	if (!name)
		return type;
	if (0 == strncasecmp(name, "vdin0", 5)) {
		if (0 == input)
			type = AML_PROVIDE_MIRROCAST_VDIN0;
		else
			type = AML_PROVIDE_HDMIIN_VDIN0;
	} else if (0 == strncasecmp(name, "vdin1", 5)) {
		if (1 == input)
			type = AML_PROVIDE_MIRROCAST_VDIN1;
		else
			type = AML_PROVIDE_HDMIIN_VDIN1;
	} else if (0 == strncasecmp(name, "decoder", 7)) {
		type = AML_PROVIDE_DECODE;
	} else if (0 == strncasecmp(name, "ppmgr", 5)) {
		type = AML_PROVIDE_PPMGR;
	} else {
		type = AML_PROVIDE_MAX;
	}
	return type;
}

enum aml_provider_type_e get_sub_receiver_type(const char *name)
{
	enum aml_provider_type_e type = AML_RECEIVER_NONE;
	if (!name)
		return type;
	if (0 == strncasecmp(name, "ppmgr", 5)) {
		type = AML_RECEIVER_PPMGR;
		pr_err("type is ppmgr");
	} else if (0 == strncasecmp(name, "deinterlace", 11)) {
		type = AML_RECEIVER_DI;
		pr_err("type is deinterlace");
	} else {
		type = AML_RECEIVER_MAX;
		pr_err("type is not certain\n");
	}
	return type;
}
/* ------------------------------------------------------------------
 *           provider operations
 *-----------------------------------------------------------------*/
static struct vframe_s *amlvideo2_vf_peek(void *op_arg)
{
	if (video_blocking)
		return NULL;
	return vfq_peek(&q_ready);
}

static struct vframe_s *amlvideo2_vf_get(void *op_arg)
{
	if (video_blocking)
		return NULL;
	return vfq_pop(&q_ready);
}

static void amlvideo2_vf_put(struct vframe_s *vf, void *op_arg)
{
	if (video_blocking)
		return;
	vf_put(vf, DEVICE_NAME);
}

static int amlvideo2_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_PUT) {
		/* pr_err("video put, avail=%d\n", vfq_level(&q_ready) ); */
	} else if (type & VFRAME_EVENT_RECEIVER_GET) {
		/* pr_err("video get, avail=%d\n", vfq_level(&q_ready) ); */
	} else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
		/* up(&thread_sem); */
		pr_err("receiver is waiting\n");
	} else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
		pr_err("frame wait\n");
	}
	return 0;
}

static int amlvideo2_vf_states(struct vframe_states *states, void *op_arg)
{
	/* unsigned long flags; */
	/* spin_lock_irqsave(&lock, flags); */
	states->vf_pool_size = amlvideo2_pool_size;
	states->buf_recycle_num = 0;
	states->buf_free_num = amlvideo2_pool_size - vfq_level(&q_ready);
	states->buf_avail_num = vfq_level(&q_ready);
	/* spin_unlock_irqrestore(&lock, flags); */
	return 0;
}

static const struct vframe_operations_s amlvideo2_vf_provider = {
.peek = amlvideo2_vf_peek,
.get = amlvideo2_vf_get,
.put = amlvideo2_vf_put,
.event_cb = amlvideo2_event_cb,
.vf_states = amlvideo2_vf_states, };

static struct vframe_provider_s amlvideo2_vf_prov;

/* ------------------------------------------------------------------
 queue operations
 ------------------------------------------------------------------*/

void vf_inqueue(struct vframe_s *vf, const char *receiver)
{
	struct vframe_receiver_s *vfp;

	if (vf == NULL)
		return;

	vfp = vf_get_receiver(DEVICE_NAME);
	if (NULL == vfp) {
		vf_put(vf, receiver);
		return;
	}

	vfq_push(&q_ready, vf);
	vf_notify_receiver(
		DEVICE_NAME,
		VFRAME_EVENT_PROVIDER_VFRAME_READY,
		NULL);
}

/* ------------------------------------------------------------------
 Videobuf operations
 ------------------------------------------------------------------*/
static int buffer_setup(struct videobuf_queue *vq, unsigned int *count,
			unsigned int *size)
{
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct amlvideo2_fh *fh = (struct amlvideo2_fh *)res->priv;
	struct amlvideo2_node *node = fh->node;
	int height = fh->height;

	if (height % 16 != 0)
		height = ((height + 15) >> 4) << 4;

	*size = (fh->width * height * fh->fmt->depth) >> 3;
	if (0 == *count)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	dpr_err(
		node->vid_dev, 1,
		"%s, count=%d, size=%d\n", __func__, *count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq,
			struct amlvideo2_node_buffer *buf)
{
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct amlvideo2_fh *fh = (struct amlvideo2_fh *)res->priv;
	struct amlvideo2_node *node = fh->node;

	dpr_err(node->vid_dev, 1, "%s, state: %i\n", __func__, buf->vb.state);
	videobuf_waiton(vq, &buf->vb, 0, 0);
	if (in_interrupt())
		BUG();
	videobuf_res_free(vq, &buf->vb);
	dpr_err(node->vid_dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 2000
#define norm_maxh() 1600

static int buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
				enum v4l2_field field)
{
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct amlvideo2_fh *fh = (struct amlvideo2_fh *)res->priv;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_buffer *buf = container_of(
		vb, struct amlvideo2_node_buffer, vb);
	int rc;
	dpr_err(node->vid_dev, 1, "%s, field=%d\n", __func__, field);

	BUG_ON(NULL == fh->fmt);

	if (fh->width < 48 || fh->width > norm_maxw() ||
	fh->height < 32 || fh->height > norm_maxh())
		return -EINVAL;

	buf->vb.size = (fh->width * fh->height * fh->fmt->depth) >> 3;
	if (0 != buf->vb.baddr && buf->vb.bsize < buf->vb.size)
		return -EINVAL;
	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt = fh->fmt;
	buf->vb.width = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field = field;
	if (VIDEOBUF_NEEDS_INIT == buf->vb.state) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0)
			goto fail;
	}
	buf->vb.state = VIDEOBUF_PREPARED;
	return 0;

fail: free_buffer(vq, buf);
	return rc;
}

static void buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct amlvideo2_node_buffer *buf = container_of(
		vb, struct amlvideo2_node_buffer, vb);
	struct videobuf_res_privdata *res = (struct videobuf_res_privdata *)vq
		->priv_data;
	struct amlvideo2_fh *fh = (struct amlvideo2_fh *)res->priv;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *vidq = &node->vidq;
	dpr_err(node->vid_dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
				struct videobuf_buffer *vb)
{
	struct amlvideo2_node_buffer *buf = container_of(
		vb, struct amlvideo2_node_buffer, vb);
	free_buffer(vq, buf);
}

static struct videobuf_queue_ops amlvideo2_qops = {
.buf_setup = buffer_setup,
.buf_prepare = buffer_prepare,
.buf_queue = buffer_queue,
.buf_release = buffer_release, };

/* ------------------------------------------------------------------
 IOCTL vidioc handling
 ------------------------------------------------------------------*/
static int vidioc_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;

	strcpy(cap->driver, "amlvideo2");
	strcpy(cap->card, "amlvideo2");
	strlcpy(
		cap->bus_info,
		node->vid_dev->v4l2_dev.name,
		sizeof(cap->bus_info));
	cap->version = AMLVIDEO2_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE
		|V4L2_CAP_STREAMING
		| V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_fmtdesc *f)
{
	struct amlvideo2_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct amlvideo2_fh *fh = priv;
	f->fmt.pix.width = fh->width;
	f->fmt.pix.height = fh->height;
	f->fmt.pix.field = fh->vb_vidq.field;
	f->fmt.pix.pixelformat = fh->fmt->fourcc;
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_fmt *fmt = NULL;
	enum v4l2_field field;
	unsigned int maxw, maxh;

	fmt = get_format(f);
	if (!fmt) {
		dpr_err(node->vid_dev, 1, "Fourcc format (0x%08x) invalid.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	field = f->fmt.pix.field;

	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	} else if (V4L2_FIELD_INTERLACED != field) {
		dpr_err(node->vid_dev, 1, "Field type invalid.\n");
		return -EINVAL;
	}

	maxw = norm_maxw();
	maxh = norm_maxh();

	f->fmt.pix.field = field;
	v4l_bound_align_image(
		&f->fmt.pix.width, 48,
		maxw, 2, &f->fmt.pix.height, 32,
		maxh, 0, 0);
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	return 0;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	int ret = 0;
	struct amlvideo2_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;

	ret = vidioc_try_fmt_vid_cap(file, fh, f);
	if (ret < 0)
		return ret;

	mutex_lock(&q->vb_lock);

	if (videobuf_queue_is_busy(&fh->vb_vidq)) {
		dpr_err(fh->node->vid_dev, 1, "%s queue busy\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	fh->fmt = get_format(f);
	fh->width = f->fmt.pix.width;
	fh->height = f->fmt.pix.height;
	fh->vb_vidq.field = f->fmt.pix.field;
	fh->type = f->type;
	ret = 0;
out: mutex_unlock(&q->vb_lock);
	return ret;
}

/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 * V4L2_CAP_TIMEPERFRAME need to be supported furthermore.
 */
static int vidioc_g_parm(struct file *file, void *priv,
				struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;

	pr_err("vidioc_g_parm\n");
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;

	cp->timeperframe = amlvideo2_frmintervals_active;
	pr_err("g_parm,deno=%d, numerator=%d\n", cp->timeperframe.denominator,
		cp->timeperframe.numerator);

	return 0;
}

static int vidioc_s_parm(struct file *file, void *priv,
				struct v4l2_streamparm *parms)
{
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	suseconds_t def_ival = 1000000 / DEF_FRAMERATE;
	suseconds_t ival; /* us */

	/*save the frame period. */
	if (0 == parms->parm.capture.timeperframe.denominator) {
		fh->frm_save_time_us = def_ival;
		amlvideo2_frmintervals_active.numerator = 1;
		amlvideo2_frmintervals_active.denominator = DEF_FRAMERATE;
		return -EINVAL;
	}

	ival = parms->parm.capture.timeperframe.numerator * 1000000
		/ parms->parm.capture.timeperframe.denominator;

	fh->frm_save_time_us = ival;
	amlvideo2_frmintervals_active = parms->parm.capture.timeperframe;

	dpr_err(
		node->vid_dev, 1,
		"%s,%d,type=%d\n",
		__func__, __LINE__, parms->type);
	dpr_err(node->vid_dev, 1, "setting framerate:%d/%d(fps)\n",
		amlvideo2_frmintervals_active.denominator,
		amlvideo2_frmintervals_active.numerator);

	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
				struct v4l2_requestbuffers *p)
{
	struct amlvideo2_fh *fh = priv;
	#ifdef USE_SEMA_QBUF
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	init_completion(&dma_q->qbuf_comp);
	#endif
	return videobuf_reqbufs(&fh->vb_vidq, p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct amlvideo2_fh *fh = priv;
	int ret = videobuf_querybuf(&fh->vb_vidq, p);
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	#if 1
	if (ret == 0) {
		struct amlvideo2_output output;
		memset(&output, 0, sizeof(struct amlvideo2_output));

		output.v4l2_format = fh->fmt->fourcc;
		output.vbuf = NULL;
		output.width = fh->width;
		output.height = fh->height;
		output.canvas_id = -1;
		p->reserved = convert_canvas_index(
			&output, VM_RES0_CANVAS_INDEX + p->index * 3);
	} else {
		p->reserved = 0;
	}
	#endif
	return ret;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct amlvideo2_fh *fh = priv;
	#ifdef USE_SEMA_QBUF
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	#endif
	int ret = videobuf_qbuf(&fh->vb_vidq, p);
	#ifdef USE_SEMA_QBUF
	complete(&dma_q->qbuf_comp);
	#endif
	return ret;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct amlvideo2_fh *fh = priv;

	return videobuf_dqbuf(&fh->vb_vidq, p, file->f_flags & O_NONBLOCK);
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct amlvideo2_fh *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif
static enum tvin_scan_mode_e vmode2scan_mode(enum vmode_e mode)
{
	enum tvin_scan_mode_e scan_mode =
		TVIN_SCAN_MODE_NULL;/* 1: progressive 2:interlaced */
	switch (mode) {
	case VMODE_480I:
	case VMODE_480CVBS:
	case VMODE_576I:
	case VMODE_576CVBS:
	case VMODE_1080I:
	case VMODE_1080I_50HZ:
		scan_mode = TVIN_SCAN_MODE_INTERLACED;
		break;
	case VMODE_480P:
	case VMODE_576P:
	case VMODE_720P:
	case VMODE_1080P:
	case VMODE_720P_50HZ:
	case VMODE_1080P_50HZ:
	case VMODE_1080P_24HZ:
	case VMODE_4K2K_30HZ:
	case VMODE_4K2K_25HZ:
	case VMODE_4K2K_24HZ:
	case VMODE_4K2K_SMPTE:
	case VMODE_VGA:
	case VMODE_SVGA:
	case VMODE_XGA:
	case VMODE_SXGA:
	case VMODE_LCD:
	case VMODE_4K2K_50HZ:
	case VMODE_4K2K_50HZ_Y420:
	case VMODE_4K2K_60HZ:
	case VMODE_4K2K_60HZ_Y420:
		scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		break;
	default:
		pr_err("unknown mode=%d\n", mode);
		break;
	}
	/* pr_err("mode=%d, scan_mode=%d\n", mode, scan_mode); */
	return scan_mode;
}
static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	int ret;
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	struct vdin_v4l2_ops_s *vops = &node->vops;
	struct vdin_parm_s para;
	const struct vinfo_s *vinfo;
	int dst_w, dst_h;
	vinfo = get_current_vinfo();
	if ((fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) || (i != fh->type))
		return -EINVAL;
	ret = videobuf_streamon(&fh->vb_vidq);
	pr_err("%s, videobuf_streamon() ret: %d\n", __func__, ret);
	pr_err(
		"%s, node->input: %d, node->r_type: %d, node->p_type: %d\n",
		__func__,
		node->input, node->r_type, node->p_type);
	if (ret < 0)
		return ret;

	if ((node->input != 0) && (!node->start_vdin_flag)) /* 0:mirrocast */
		goto start;

	if ((AML_RECEIVER_NONE != node->r_type) && (!node->start_vdin_flag))
		goto start;

	if (AML_PROVIDE_MIRROCAST_VDIN0 == node->p_type)
		node->vdin_device_num = 0;
	else if (AML_PROVIDE_MIRROCAST_VDIN1 == node->p_type)
		node->vdin_device_num = 1;


	memset(&para, 0, sizeof(para));
	para.port = node->porttype;
	para.fmt = TVIN_SIG_FMT_MAX;
	para.frame_rate = 60;
	para.h_active = vinfo->width;
	para.v_active = vinfo->height;
	para.hsync_phase = 0;
	para.vsync_phase = 1;
	para.hs_bp = 0;
	para.vs_bp = 0;
	para.dfmt = TVIN_NV21;/* TVIN_YUV422; */
	para.scan_mode = vmode2scan_mode(
		vinfo->mode);/* TVIN_SCAN_MODE_PROGRESSIVE; */
	if (TVIN_SCAN_MODE_INTERLACED == para.scan_mode)
		para.v_active = para.v_active / 2;

	dst_w = fh->width;
	dst_h = fh->height;
	if (vinfo->width < vinfo->height) {
		if ((vinfo->width <= 768) && (vinfo->height <= 1024)) {
			dst_w = vinfo->width;
			dst_h = vinfo->height;
		} else {
			dst_w = fh->height;
			dst_h = fh->width;
		}
		output_axis_adjust(vinfo->height, vinfo->width, (int *)&dst_h,
					(int *)&dst_w, 0);
	} else {
		if ((vinfo->height <= 768) && (vinfo->width <= 1024)) {
			dst_w = vinfo->width;
			dst_h = vinfo->height;
		}
		output_axis_adjust(vinfo->width, vinfo->height, (int *)&dst_w,
					(int *)&dst_h, 0);
	}
	para.dest_hactive = dst_w;
	para.dest_vactive = dst_h;
	if (TVIN_SCAN_MODE_INTERLACED == para.scan_mode)
		para.dest_vactive = para.dest_vactive / 2;

	pr_err("amlvideo2--vidioc_streamon:");
	pr_err("para.h_active: %d, para.v_active: %d,",
		para.h_active, para.v_active);
	pr_err("para.dest_hactive: %d, para.dest_vactive: %d,",
		para.dest_hactive, para.dest_vactive);
	pr_err("fh->width: %d, fh->height: %d,",
		fh->width, fh->height);
	pr_err("vinfo->mode: %d,para.scan_mode: %d\n",
		vinfo->mode, para.scan_mode);

	vops->start_tvin_service(node->vdin_device_num, &para);

start: frame_inittime = 1;
	fh->is_streamed_on = 1;
	/* frameInv_adjust = 0; */
	/* frameInv = 0; */
	/* tmp_vf = NULL; */
	do_gettimeofday(&thread_ts1);
	#ifdef TEST_LATENCY
	cur_time = cur_time_out = thread_ts1.tv_sec;
	total_latency = 0;
	total_latency_out = 0;
	#endif
	return 0;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	int ret;
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	struct vdin_v4l2_ops_s *vops = &node->vops;

	if ((fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) || (i != fh->type))
		return -EINVAL;
	ret = videobuf_streamoff(&fh->vb_vidq);
	if (ret < 0)
		pr_err("videobuf stream off failed\n");

	pr_err("%s , %d\n", __func__, __LINE__);
	if ((0 == node->input) && (AML_RECEIVER_NONE == node->r_type))
		vops->stop_tvin_service(node->vdin_device_num);

	node->start_vdin_flag = 0;
	fh->is_streamed_on = 0;
	return ret;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,
					struct v4l2_frmsizeenum *fsize)
{
	int ret = 0, i = 0;
	struct amlvideo2_fmt *fmt = NULL;
	struct v4l2_frmsize_discrete *frmsize = NULL;
	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		if (formats[i].fourcc == fsize->pixel_format) {
			fmt = &formats[i];
			break;
		}
	}
	if (fmt == NULL)
		return -EINVAL;
	if ((fmt->fourcc == V4L2_PIX_FMT_NV21)
		|| (fmt->fourcc == V4L2_PIX_FMT_NV12)
		|| (fmt->fourcc == V4L2_PIX_FMT_YUV420)
		|| (fmt->fourcc == V4L2_PIX_FMT_YVU420)) {
		if (fsize->index >= ARRAY_SIZE(amlvideo2_prev_resolution))
			return -EINVAL;
		frmsize = &amlvideo2_prev_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	} else if (fmt->fourcc == V4L2_PIX_FMT_RGB24) {
		if (fsize->index >= ARRAY_SIZE(amlvideo2_pic_resolution))
			return -EINVAL;
		frmsize = &amlvideo2_pic_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	return ret;
}
static int vidioc_enum_frameintervals(struct file *file, void *priv,
					struct v4l2_frmivalenum *fival)
{
	unsigned int k;
	if (fival->index > ARRAY_SIZE(amlvideo2_frmivalenum))
		return -EINVAL;
	for (k = 0; k < ARRAY_SIZE(amlvideo2_frmivalenum); k++) {
		if ((fival->index == amlvideo2_frmivalenum[k].index)
			&& (fival->pixel_format
				== amlvideo2_frmivalenum[k].pixel_format)
			&& (fival->width == amlvideo2_frmivalenum[k].width)
			&& (fival->height
				== amlvideo2_frmivalenum[k].height)) {
			memcpy(fival, &amlvideo2_frmivalenum[k],
				sizeof(struct v4l2_frmivalenum));
			return 0;
		}
	}
	return -EINVAL;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id i)
{
	return 0;
}

/* --- controls ---------------------------------------------- */

static int amlvideo2_setting(struct amlvideo2_node *node, int PROP_ID,
				int value, int index)
{
	int ret = 0;
	switch (PROP_ID) {
	case V4L2_CID_ROTATE:
		if (node->qctl_regs[index] != value)
			node->qctl_regs[index] = value;

		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

static int vidioc_queryctrl(struct file *file, void *priv,
				struct v4l2_queryctrl *qc)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(amlvideo2_node_qctrl); i++)
		if (qc->id && qc->id == amlvideo2_node_qctrl[i].id) {
			memcpy(qc, &(amlvideo2_node_qctrl[i]), sizeof(*qc));
			return 0;
		}
	return -EINVAL;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	*i = node->input;
	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	if (i > 1) {
		node->input = i;
		node->start_vdin_flag = (i >> 28);
		if (node->start_vdin_flag)
			node->porttype = (i & (~(1 << 28)));
		else
			node->porttype = i;
		pr_err("current porttype : %x\n", node->porttype);
	} else {
		node->input = i;
		node->start_vdin_flag = 1;
		node->porttype = TVIN_PORT_VIU;
		node->provider = vf_get_provider(DEVICE_NAME);
		if (NULL == node->provider) {
			node->p_type = AML_PROVIDE_MIRROCAST_VDIN1;
		} else {
			node->p_type =
			 get_provider_type(node->provider->name, node->input);
		}
		pr_err("current input:%d\n", node->input);
	}
	return 0;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	int i;
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	for (i = 0; i < ARRAY_SIZE(amlvideo2_node_qctrl); i++)
		if (ctrl->id == amlvideo2_node_qctrl[i].id) {
			ctrl->value = node->qctl_regs[i];
			return 0;
		}
	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	int i;
	struct amlvideo2_fh *fh = priv;
	struct amlvideo2_node *node = fh->node;
	for (i = 0; i < ARRAY_SIZE(amlvideo2_node_qctrl); i++)
		if (ctrl->id == amlvideo2_node_qctrl[i].id) {
			if (ctrl->value < amlvideo2_node_qctrl[i].minimum
				|| ctrl->value
				> amlvideo2_node_qctrl[i].maximum
				|| amlvideo2_setting(
					node, ctrl->id, ctrl->value, i) < 0) {
				return -ERANGE;
			}
			/* node->qctl_regs[i] = ctrl->value; */
			return 0;
		}
	return -EINVAL;
}

int amlvideo2_cma_buf_init(struct amlvideo2_device *vid_dev)
{
	int flags;
	if (!vid_dev->use_reserve) {
		if (vid_dev->cma_mode == 0) {
			vid_dev->cma_pages = dma_alloc_from_contiguous(
				&(vid_dev->pdev->dev),
				(CMA_ALLOC_SIZE*SZ_1M) >> PAGE_SHIFT, 0);
	    if (vid_dev->cma_pages) {
			vid_dev->buffer_start = page_to_phys(
				vid_dev->cma_pages);
			vid_dev->buffer_size = (CMA_ALLOC_SIZE*SZ_1M);
	    } else {
		pr_err("amlvideo2 alloc cma alone failed\n");
		return -1;
	    }
		} else {
			flags = CODEC_MM_FLAGS_DMA_CPU|CODEC_MM_FLAGS_CMA_CLEAR;
			vid_dev->buffer_start = codec_mm_alloc_for_dma(
				"amlvideo2",
				(CMA_ALLOC_SIZE*SZ_1M)/PAGE_SIZE, 0, flags);
	    if (!(vid_dev->buffer_start)) {
		pr_err("amlvideo2 alloc cma buffer failed\n");
		return -1;
	    } else {
		vid_dev->buffer_size = (CMA_ALLOC_SIZE*SZ_1M);
	    }
		}
		pr_info("amlvideo2 cma memory is %x , size is  %x\n" ,
			(unsigned)vid_dev->buffer_start ,
			(unsigned)vid_dev->buffer_size);
	}

	return 0;
}

int amlvideo2_cma_buf_uninit(struct amlvideo2_device *vid_dev)
{
	if (!vid_dev->use_reserve) {
		if (vid_dev->cma_mode == 0) {
			if (vid_dev->cma_pages) {
				dma_release_from_contiguous(
					&vid_dev->pdev->dev,
					vid_dev->cma_pages,
					(CMA_ALLOC_SIZE*SZ_1M) >> PAGE_SHIFT);
				vid_dev->cma_pages = NULL;
			}
		} else {
			if (vid_dev->buffer_start != 0) {
				codec_mm_free_for_dma(
				"amlvideo2",
				vid_dev->buffer_start);
				vid_dev->buffer_start = 0;
				vid_dev->buffer_size = 0;
				pr_info("amlvideo2 cma memory release succeed\n");
			}
		}
	}
	return 0;
}

/* ------------------------------------------------------------------
 File operations for the device
 ------------------------------------------------------------------*/
static int amlvideo2_open(struct file *file)
{
	struct amlvideo2_node *node = video_drvdata(file);
	struct amlvideo2_fh *fh = NULL;
	struct videobuf_res_privdata *res = NULL;
	struct resource *reserve = NULL;
	int ret;

	mutex_lock(&node->mutex);
	node->users++;
	if (node->users > 1) {
		node->users--;
		mutex_unlock(&node->mutex);
		return -EBUSY;
	}

	#if 0
	node->provider = vf_get_provider(
		(node->vid == 0)?RECEIVER_NAME0:RECEIVER_NAME1);

	if (node->provider == NULL) {
		node->users--;
		mutex_unlock(&node->mutex);
		return -ENODEV;
	}
	node->p_type = get_provider_type(node->provider->name, node->input);
	if ((node->p_type == AML_PROVIDE_NONE)
		|| (node->p_type >= AML_PROVIDE_MAX)) {
		node->users--;
		node->provider = NULL;
		mutex_unlock(&node->mutex);
		return -ENODEV;
	}
	#endif
	ret = amlvideo2_cma_buf_init(node->vid_dev);
	if (ret < 0)
		return -ENOMEM;

	fh = node->fh;
	if (NULL == fh) {
		node->users--;
		/* node->provider  = NULL; */
		mutex_unlock(&node->mutex);
		return -ENOMEM;
	}

	if (node->vid_dev->use_reserve) {
		reserve = &node->vid_dev->memobj;
		if (!reserve) {
			pr_err("alloc reserve buffer failed !\n");
			return -ENOMEM;
		} else {
			node->res.start = reserve->start;
			node->res.end = reserve->end;
		}
	} else {
		node->res.start = node->vid_dev->buffer_start;
		node->res.end = node->vid_dev->buffer_start +
					node->vid_dev->buffer_size;
	}
	mutex_unlock(&node->mutex);
	node->input = 0; /* default input is miracast */
	node->sub_recv = vf_get_receiver(DEVICE_NAME);
	if (node->sub_recv) {
		pr_err("node->sub_recv->name=%s\n", node->sub_recv->name);
		node->provider = vf_get_provider(DEVICE_NAME);
		if (NULL == node->provider) {
			pr_err("node->provider=%p", node->provider);
			node->p_type = AML_PROVIDE_MAX;
		} else {
			pr_err("node->provider=%p", node->provider);
			node->p_type = get_provider_type(node->provider->name,
								node->input);
		}
		pr_err("node->r_type=%d\n", node->r_type);
		node->r_type = get_sub_receiver_type(node->sub_recv->name);
	} else {
		pr_err("as an end receiver\n");
		node->p_type = AML_PROVIDE_MIRROCAST_VDIN1;
		node->provider = vf_get_provider(DEVICE_NAME);
		if (NULL == node->provider) {
			node->p_type = AML_PROVIDE_MIRROCAST_VDIN1;
		} else {
			pr_err("node->provider = %p\n", node->provider);
			node->p_type = get_provider_type(node->provider->name,
								node->input);
		}
		node->r_type = AML_RECEIVER_NONE;
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		/* switch_mod_gate_by_name("ge2d", 1); */
		/* #endif */
	}

	file->private_data = fh;
	fh->node = node;

	fh->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt = &formats[0];
	fh->width = 1920;
	fh->height = 1080;
	fh->f_flags = file->f_flags;

	fh->node->res.priv = (void *)fh;
	res = &fh->node->res;
	videobuf_queue_res_init(&fh->vb_vidq, &amlvideo2_qops,
		NULL,
		&node->slock, fh->type, V4L2_FIELD_INTERLACED,
		sizeof(struct amlvideo2_node_buffer), (void *)res,
		NULL);

	if (AML_RECEIVER_NONE == node->r_type) {
		pr_err("start thread\n");
		amlvideo2_start_thread(fh);
	}

	v4l2_vdin_ops_init(&node->vops);
	fh->frm_save_time_us = 1000000 / DEF_FRAMERATE;
	return 0;
}

static ssize_t amlvideo2_read(struct file *file, char __user *data,
				size_t count, loff_t *ppos)
{
	struct amlvideo2_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
						file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int amlvideo2_poll(struct file *file,
					struct poll_table_struct *wait)
{
	struct amlvideo2_fh *fh = file->private_data;
	struct amlvideo2_node *node = fh->node;
	struct videobuf_queue *q = &fh->vb_vidq;

	dpr_err(node->vid_dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int amlvideo2_close(struct file *file)
{
	struct amlvideo2_fh *fh = file->private_data;
	struct amlvideo2_node *node = fh->node;
	if (AML_RECEIVER_NONE == node->r_type)
		amlvideo2_stop_thread(&node->vidq);

	videobuf_stop(&fh->vb_vidq);
	videobuf_mmap_free(&fh->vb_vidq);
	amlvideo2_cma_buf_uninit(node->vid_dev);
	mutex_lock(&node->mutex);
	if (AML_RECEIVER_NONE == node->r_type) {
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		/* switch_mod_gate_by_name("ge2d", 0); */
		/* #endif */
	}
	node->users--;
	amlvideo2_frmintervals_active.numerator = 1;
	amlvideo2_frmintervals_active.denominator = DEF_FRAMERATE;
	/* node->provider = NULL; */
	mutex_unlock(&node->mutex);
	return 0;
}

static int amlvideo2_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = 0;
	struct amlvideo2_fh *fh = file->private_data;
	struct amlvideo2_node *node = fh->node;

	dpr_err(node->vid_dev, 1,
		"mmap called, vma=0x%08lx\n", (unsigned long)vma);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dpr_err(node->vid_dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end - (unsigned long)vma->vm_start, ret);

	return ret;
}

static const struct v4l2_file_operations amlvideo2_fops = {
.owner = THIS_MODULE,
.open = amlvideo2_open,
.release = amlvideo2_close,
.read = amlvideo2_read,
.poll = amlvideo2_poll,
.ioctl = video_ioctl2, /* V4L2 ioctl handler */
.mmap = amlvideo2_mmap, };

static const struct v4l2_ioctl_ops amlvideo2_ioctl_ops = {
.vidioc_querycap = vidioc_querycap,
.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
.vidioc_g_parm = vidioc_g_parm,
.vidioc_s_parm = vidioc_s_parm,
.vidioc_reqbufs = vidioc_reqbufs,
.vidioc_querybuf = vidioc_querybuf,
.vidioc_qbuf = vidioc_qbuf,
.vidioc_dqbuf = vidioc_dqbuf,
.vidioc_s_std = vidioc_s_std,
.vidioc_queryctrl = vidioc_queryctrl,
.vidioc_g_ctrl = vidioc_g_ctrl,
.vidioc_s_ctrl = vidioc_s_ctrl,
.vidioc_streamon = vidioc_streamon,
.vidioc_streamoff = vidioc_streamoff,
.vidioc_enum_framesizes = vidioc_enum_framesizes,
.vidioc_enum_frameintervals = vidioc_enum_frameintervals,
.vidioc_s_input = vidioc_s_input,
.vidioc_g_input = vidioc_g_input,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
.vidiocgmbuf = vidiocgmbuf,
#endif
};

static struct video_device amlvideo2_template = {
.name = "amlvideo2",
.fops =  &amlvideo2_fops,
.ioctl_ops = &amlvideo2_ioctl_ops,
.release = video_device_release,
.tvnorms = V4L2_STD_525_60,
/* .current_norm = V4L2_STD_NTSC_M, */
};

static int vf_get_states(struct vframe_states *states)
{
	int ret = -1;
	struct vframe_provider_s *vfp;
	vfp = vf_get_provider(DEVICE_NAME);
	if (vfp && vfp->ops && vfp->ops->vf_states)
		ret = vfp->ops->vf_states(states, vfp->op_arg);
	return ret;
}

static int amlvideo2_receiver_event_fun(int type, void *data,
					void *private_data)
{
	struct amlvideo2_node *node = (struct amlvideo2_node *)private_data;
	/* (struct amlvideo2_fh *)container_of(
	 * node, struct amlvideo2_fh, node); */
	struct amlvideo2_fh *fh = node->fh;
	struct vframe_states states;
	struct vframe_states frame_states;

	switch (type) {
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		node->provide_ready = 1;
		video_blocking = false;
		if (vf_peek(node->recv.name) != NULL)
			wake_up_interruptible(&node->vidq.wq);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		amlvideo2_vf_states(&states, NULL);
		if (states.buf_avail_num > 0) {
			return RECEIVER_ACTIVE;
		} else {
			if (vf_notify_receiver(
				DEVICE_NAME,
				VFRAME_EVENT_PROVIDER_QUREY_STATE,
				NULL)
				== RECEIVER_ACTIVE) {
				return RECEIVER_ACTIVE;
			}
				return RECEIVER_INACTIVE;
		}
		break;
	case VFRAME_EVENT_PROVIDER_START:
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		/* switch_mod_gate_by_name("ge2d", 1); */
		/* #endif */
		node->sub_recv = vf_get_receiver(DEVICE_NAME);
		pr_err("sub recv=%p\n", node->sub_recv);
		if (node->sub_recv) {
			pr_err("node->sub_recv->name=%s\n",
			       node->sub_recv->name);
			node->r_type =
				get_sub_receiver_type(node->sub_recv->name);
			pr_err("r_type=%d\n", node->r_type);

			node->provider = vf_get_provider(DEVICE_NAME);
			pr_err("provider=%p\n", node->provider);
			if (node->provider) {
				pr_err("provider=%s\n", node->provider->name);
				node->p_type =
					get_provider_type(node->provider->name,
						node->input);
			}

			fh->node = node;

			vf_reg_provider(&amlvideo2_vf_prov);
			vf_notify_receiver(DEVICE_NAME,
			VFRAME_EVENT_PROVIDER_START,
						NULL);
	    pr_info("start thread\n");
	    amlvideo2_start_thread(fh);
		}

		break;
	case VFRAME_EVENT_PROVIDER_REG:
		amlvideo2_pool_ready = NULL;
		amlvideo2_pool_size = 0;
		if (vf_get_states(&frame_states) == 0)
			amlvideo2_pool_size = frame_states.vf_pool_size;
		else
			amlvideo2_pool_size = 4;

		amlvideo2_pool_ready =
		kmalloc((sizeof(struct vframe_s *) * (amlvideo2_pool_size + 1)),
		GFP_KERNEL);
		if (!amlvideo2_pool_ready)
				pr_err("amlvideo2_pool_ready malloc failed\n");
		if (amlvideo2_pool_ready != NULL) {
				vfq_init(&q_ready,
				amlvideo2_pool_size + 1,
				(struct vframe_s **)&amlvideo2_pool_ready[0]);
		}
		break;
	case VFRAME_EVENT_PROVIDER_UNREG:
		node->provide_ready = 0;
		pr_err("r_type=%d,p_type=%d\n", node->r_type, node->p_type);
		if (AML_RECEIVER_NONE != node->r_type) {
			amlvideo2_stop_thread(&node->vidq);
			pr_err("%s,%dstop thread\n", __func__, __LINE__);
		}

		pr_err("unreg amlvideo2\n");
		vf_unreg_provider(&amlvideo2_vf_prov);

		kfree(amlvideo2_pool_ready);
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		/* switch_mod_gate_by_name("ge2d", 0); */
		/* #endif */
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		pr_err("provider light unreg\n");
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		pr_err("provider reset\n");
		vf_notify_receiver(DEVICE_NAME,
		VFRAME_EVENT_PROVIDER_RESET,
					NULL);
		if (node->vidq.task_running) {
			video_blocking = true;
			wake_up_interruptible(&node->vidq.wq);
		}
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
		vf_notify_receiver(DEVICE_NAME, type, data);
		break;
	default:
		break;
	}
	return 0;
}

static const struct vframe_receiver_op_s video_vf_receiver = {
.event_cb = amlvideo2_receiver_event_fun};
/* -----------------------------------------------------------------
 Initialization and module stuff
 ------------------------------------------------------------------*/

static int amlvideo2_release_node(struct amlvideo2_device *vid_dev)
{
	int i = 0;
	struct video_device *vfd = NULL;

	for (i = 0; i < vid_dev->node_num; i++) {
		if (vid_dev->node[i]) {
			vfd = vid_dev->node[i]->vfd;
			video_device_release(vfd);
			vf_unreg_receiver(&vid_dev->node[i]->recv);
			if (vid_dev->node[i]->context)
				destroy_ge2d_work_queue(
					vid_dev->node[i]->context);
			kfree(vid_dev->node[i]->fh);
			vid_dev->node[i]->fh = NULL;
			kfree(vid_dev->node[i]);
			vid_dev->node[i] = NULL;
		}
	}

	return 0;
}

/* static struct resource memobj; */
static int amlvideo2_create_node(struct platform_device *pdev)
{
	int ret = 0, i = 0, j = 0;
	struct video_device *vfd = NULL;
	struct amlvideo2_node *vid_node = NULL;
	struct amlvideo2_fh *fh = NULL;
	struct resource *res = NULL;
	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);
	struct amlvideo2_device *vid_dev = container_of(v4l2_dev,
							struct amlvideo2_device,
							v4l2_dev);
	pr_info("amlvideo2_create_node");
	vid_dev->node_num = pdev->num_resources;
	if (vid_dev->node_num > MAX_SUB_DEV_NODE)
		vid_dev->node_num = MAX_SUB_DEV_NODE;

	for (i = 0; i < vid_dev->node_num; i++) {
		vid_dev->node[i] = NULL;
		ret = -ENOMEM;
	#if 0
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
	#else

	res = &vid_dev->memobj;
		/*
		 ret = find_reserve_block(pdev->dev.of_node->name,i);
		 if(ret < 0){
		 pr_err("\namlvideo2 memory resource undefined.\n");
		 return -EFAULT;
		 }
		 res->start = (phys_addr_t)get_reserve_block_addr(ret);
		 res->end = res->start +
		 (phys_addr_t)get_reserve_block_size(ret)-1;
		 */
	#endif
		if (!res)
			break;

		vid_node = kzalloc(sizeof(struct amlvideo2_node), GFP_KERNEL);
		if (!vid_node)
			break;

		vid_node->res.magic = MAGIC_RE_MEM;
		vid_node->res.priv = NULL;
		vid_node->context = create_ge2d_work_queue();
		if (!vid_node->context) {
			kfree(vid_node);
			break;
		}
		/* init video dma queues */
		INIT_LIST_HEAD(&vid_node->vidq.active);
		init_waitqueue_head(&vid_node->vidq.wq);

		/* initialize locks */
		spin_lock_init(&vid_node->slock);
		mutex_init(&vid_node->mutex);

		vfd = video_device_alloc();
		if (!vfd) {
			destroy_ge2d_work_queue(vid_node->context);
			kfree(vid_node);
			break;
		}
		*vfd = amlvideo2_template;
		vfd->debug = debug;
		vfd->v4l2_dev = v4l2_dev;
		ret = video_register_device(vfd, VFL_TYPE_GRABBER, video_nr);
		if (ret < 0) {
			ret = -ENODEV;
			video_device_release(vfd);
			destroy_ge2d_work_queue(vid_node->context);
			kfree(vid_node);
			break;
		}

		fh = kzalloc(sizeof(*fh), GFP_KERNEL);
		if (!fh)
			break;

		vid_node->fh = fh;
		video_set_drvdata(vfd, vid_node);

		/* Set all controls to their default value. */
		for (j = 0; j < ARRAY_SIZE(amlvideo2_node_qctrl); j++)
			vid_node->qctl_regs[j] =
				amlvideo2_node_qctrl[j].default_value;

		vid_node->vfd = vfd;
		vid_node->vid = i;
		vid_node->users = 0;
		vid_node->vid_dev = vid_dev;
		video_nr++;
	#ifdef MUTLI_NODE
		vf_receiver_init(&vid_node->recv,
			(i == 0)?RECEIVER_NAME0:RECEIVER_NAME1,
			&video_vf_receiver, (void *)vid_node);
	#else
		vf_receiver_init(&vid_node->recv,
		RECEIVER_NAME,
					&video_vf_receiver, (void *)vid_node);
	#endif
		vf_reg_receiver(&vid_node->recv);
		vf_provider_init(&amlvideo2_vf_prov,
		DEVICE_NAME,
					&amlvideo2_vf_provider,
					NULL);
		vid_dev->node[i] = vid_node;
		v4l2_info(&vid_dev->v4l2_dev, "V4L2 device registered as %s\n",
				video_device_node_name(vfd));
		ret = 0;
	}

	if (ret)
		amlvideo2_release_node(vid_dev);

	return ret;
}

static int amlvideo2_driver_probe(struct platform_device *pdev)
{
	s32 ret;
	struct amlvideo2_device *dev = NULL;

	dev = kzalloc(sizeof(struct amlvideo2_device), GFP_KERNEL);

	if (dev == NULL)
		return -ENOMEM;

	memset(dev, 0, sizeof(struct amlvideo2_device));
	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "%s",
			AVMLVIDEO2_MODULE_NAME);

	pr_err("amlvideo2 probe called\n");

	pdev->num_resources = MAX_SUB_DEV_NODE;
	platform_set_drvdata(pdev, &(dev->v4l2_dev));
	ret = of_reserved_mem_device_init(&pdev->dev);

	if (ret == 0)
		pr_info("amlvideo2_probe done\n");

	ret = of_property_read_u32(pdev->dev.of_node,
		"cma_mode", &(dev->cma_mode));
	if (ret) {
		pr_err("don't find  match cma_mode\n");
		dev->cma_mode = 1;
	}
	dev->pdev = pdev;

	if (v4l2_device_register(&pdev->dev, &dev->v4l2_dev) < 0) {
		dev_err(&pdev->dev, "v4l2_device_register failed\n");
		ret = -ENODEV;
		goto probe_err0;
	}

	mutex_init(&dev->mutex);
	video_nr = 11;

	ret = amlvideo2_create_node(pdev);
	if (ret)
		goto probe_err1;

	return 0;

probe_err1: v4l2_device_unregister(&dev->v4l2_dev);

probe_err0: kfree(dev);
	return ret;

	/* int ret = 0; */
	/* struct amlvideo2_device *dev = NULL; */
	/*  */
	/* if(of_get_property(pdev->dev.of_node, "reserve-memory", NULL)) */
	/* pdev->num_resources = MAX_SUB_DEV_NODE; */
	/*  */
	/* if (pdev->num_resources == 0) */
	/* { */
	/* dev_err(&pdev->dev, "probed for an unknown device\n"); */
	/* return -ENODEV; */
	/* } */
	/*  */
	/* dev = kzalloc(sizeof(struct amlvideo2_device), GFP_KERNEL); */
	/*  */
	/* if (dev == NULL) */
	/* return -ENOMEM; */
	/*  */
	/* memset(dev,0,sizeof(struct amlvideo2_device)); */
	/*  */
	/* snprintf(dev->v4l2_dev.name,
	 sizeof(dev->v4l2_dev.name),
	 "%s", AVMLVIDEO2_MODULE_NAME); */
	/*  */
	/* if (v4l2_device_register(&pdev->dev, &dev->v4l2_dev) < 0) { */
	/* dev_err(&pdev->dev, "v4l2_device_register failed\n"); */
	/* ret = -ENODEV; */
	/* goto probe_err0; */
	/* } */
	/*  */
	/* mutex_init(&dev->mutex); */
	/* video_nr = 11; */
	/*  */
	/* ret = amlvideo2_create_node(pdev); */
	/* if (ret) */
	/* goto probe_err1; */
	/*  */
	/* return 0; */
	/*  */
	/* probe_err1: */
	/* v4l2_device_unregister(&dev->v4l2_dev); */
	/*  */
	/* probe_err0: */
	/* kfree(dev); */
	/* return ret; */
}

static int amlvideo2_mem_device_init(struct reserved_mem *rmem,
					struct device *dev)
{
	struct resource *res = NULL;
	struct platform_device *pdev = container_of(dev,
			struct platform_device, dev);
	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);
	struct amlvideo2_device *vdevp = container_of(v4l2_dev,
			struct amlvideo2_device, v4l2_dev);
	res = &(vdevp->memobj);
	res->start = rmem->base;
	res->end = rmem->base + rmem->size - 1;
	vdevp->use_reserve = 1;
	pr_info("amlvideo2 mem:%lx->%lx\n", (unsigned long)res->start,
		(unsigned long)res->end);
	return 0;
}

static const struct reserved_mem_ops rmem_amlvideo2_ops = {
.device_init = amlvideo2_mem_device_init, };

static int __init amlvideo2_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_amlvideo2_ops;
	pr_info("amlvideo2 share mem setup\n");

	return 0;
}

static int amlvideo2_drv_remove(struct platform_device *pdev)
{
	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);
	struct amlvideo2_device *vid_dev = container_of(v4l2_dev,
							struct amlvideo2_device,
							v4l2_dev);

	amlvideo2_release_node(vid_dev);
	v4l2_device_unregister(v4l2_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(vid_dev);
	return 0;
}

static const struct of_device_id amlvideo2_dt_match[] = {
{
.compatible = "amlogic, amlvideo2",
},
{},
};


/* general interface for a linux driver .*/
static struct platform_driver amlvideo2_drv = {
.probe = amlvideo2_driver_probe,
.remove = amlvideo2_drv_remove,
.driver = {.name = "amlvideo2", .owner = THIS_MODULE, .of_match_table =
amlvideo2_dt_match, } };

static int __init amlvideo2_init(void)
{
	/* amlog_level(LOG_LEVEL_HIGH,"amlvideo2_init\n"); */
	if (platform_driver_register(&amlvideo2_drv)) {
		pr_err(
			"Failed to register amlvideo2 driver\n");
			return -ENODEV;
	}

	return 0;
}

static void __exit amlvideo2_exit(void)
{
	platform_driver_unregister(&amlvideo2_drv);
	/* amlog_level(LOG_LEVEL_HIGH,"amlvideo2 module removed.\n"); */
}

RESERVEDMEM_OF_DECLARE(amlvideo2, "amlogic, amlvideo2_memory",
						 amlvideo2_mem_setup);
module_init(amlvideo2_init);
module_exit(amlvideo2_exit);
