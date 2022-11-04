// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/version.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <uapi/linux/sched/types.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/vdetect/vdetect.h>
#include <linux/amlogic/media/utils/am_com.h>

#define MAX_INST 1
#define RECEIVER_NAME "vdetect"
#define PROVIDER_NAME "vdetect"

#define VDETECT_MAJOR_VERSION 0
#define VDETECT_MINOR_VERSION 7
#define VDETECT_RELEASE 1
#define VDETECT_VERSION \
	KERNEL_VERSION(VDETECT_MAJOR_VERSION, \
		       VDETECT_MINOR_VERSION, \
		       VDETECT_RELEASE)

#define CMA_ALLOC_SIZE 1
#define MAGIC_RE_MEM 0x123039dc
#define VDETECT_MODULE_NAME "vdetect"

#define V4L2_AMLOGIC_VDETECT_BASE  (V4L2_CID_USER_BASE | 0x1200)

#define PRINT_ERROR		0X0
#define PRINT_CAPTUREINFO	0X0001
#define PRINT_BUFFERINFO	0X0002
#define PRINT_THREADINFO	0X0004
#define PRINT_PATHINFO		0X0008
#define PRINT_TIMESTAMP		0X0010
#define PRINT_MN_SET_FLOW	0X0020
#define PRINT_PATH_OTHER	0X0040
#define PRINT_CAP_OTHER		0X0080

static int detect_debug;
static int detect_dump;
static int detect_thread;
static int tv_add_vdetect;
static int aipq_enable;
static int post_get_count;
static int post_put_count;
static int pre_put_count;
static DEFINE_SPINLOCK(lock);

static u32 vid_limit = 32;
static unsigned int vdetect_base = 40;

static u32 frame_number;
static struct vdetect_frame_nn_info nn_info[MAX_NN_INFO_BUF_COUNT];
static u32 nn_frame_num;
int vdetect_print(int index, int debug_flag, const char *fmt, ...)
{
	if ((detect_debug & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "vdetect.%d ", index);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static char *result_vnn[] = {
	"faceskin",
	"bluesky",
	"flowers",
	"foods",
	"breads",
	"fruits",
	"meats",
	"document",
	"ocean",
	"pattern",
	"group",
	"animals",
	"grass",
	"iceskate",
	"leaves",
	"racetrack",
	"fireworks",
	"waterfall",
	"beach",
	"waterside",
	"snows",
	"nightscop",
	"sunset"
};

static struct vdetect_fmt formats[] = {
	{
		.name = "RGB565 (BE)",
		.fourcc = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth = 16,
	},

	{
		.name = "RGB888 (24)",
		.fourcc = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
		.depth = 24,
	},

	{
		.name = "BGR888 (24)",
		.fourcc = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
		.depth = 24,
	},

	{
		.name = "RGBA888 (32)",
		.fourcc = V4L2_PIX_FMT_RGB32, /* 32  RGBA-8-8-8 */
		.depth = 32,
	},

	{
		.name = "12  Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.depth = 12,
	},

	{
		.name = "12  Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV21,
		.depth = 12,
	},

	{
		.name = "YUV420P",
		.fourcc = V4L2_PIX_FMT_YUV420,
		.depth = 12,
	},

	{
		.name = "YVU420P",
		.fourcc = V4L2_PIX_FMT_YVU420,
		.depth = 12,
	}
};

static struct vdetect_fmt *get_format(struct v4l2_format *f)
{
	struct vdetect_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		fmt = NULL;

	return fmt;
}

static int alloc_canvas(struct vdetect_dev *dev)
{
	const char *keep_owner = "vdetect";

	if (dev->src_canvas[0] < 0)
		dev->src_canvas[0] =
			canvas_pool_map_alloc_canvas(keep_owner);
	if (dev->src_canvas[1] < 0)
		dev->src_canvas[1] =
			canvas_pool_map_alloc_canvas(keep_owner);
	if (dev->src_canvas[2] < 0)
		dev->src_canvas[2] =
			canvas_pool_map_alloc_canvas(keep_owner);
	if (dev->dst_canvas < 0)
		dev->dst_canvas =
			canvas_pool_map_alloc_canvas(keep_owner);

	if (dev->src_canvas[0] < 0 ||
	    dev->src_canvas[1] < 0 ||
	    dev->src_canvas[2] < 0 ||
	    dev->dst_canvas < 0) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "canvas alloc failed!\n");
		return -1;
	}
	return 0;
}

static int free_canvas(struct vdetect_dev *dev)
{
	if (dev->src_canvas[0] >= 0) {
		canvas_pool_map_free_canvas(dev->src_canvas[0]);
		dev->src_canvas[0] = -1;
	}

	if (dev->src_canvas[1] >= 0) {
		canvas_pool_map_free_canvas(dev->src_canvas[1]);
		dev->src_canvas[1] = -1;
	}

	if (dev->src_canvas[2] >= 0) {
		canvas_pool_map_free_canvas(dev->src_canvas[2]);
		dev->src_canvas[2] = -1;
	}

	if (dev->dst_canvas >= 0) {
		canvas_pool_map_free_canvas(dev->dst_canvas);
		dev->dst_canvas = -1;
	}

	if (dev->src_canvas[0] >= 0 ||
	    dev->src_canvas[1] >= 0 ||
	    dev->src_canvas[2] >= 0 ||
	    dev->dst_canvas >= 0) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "canvas free failed!\n");
		return -1;
	}

	return 0;
}

static int vdetect_cma_buf_init(struct vdetect_dev *dev)
{
	int flags;
	int ret;

	flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;
	ret = alloc_canvas(dev);
	if (ret < 0) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "alloc cma failed!\n");
		return -1;
	}

	dev->buffer_start =
		codec_mm_alloc_for_dma(dev->recv_name,
				       (CMA_ALLOC_SIZE * SZ_1M) / PAGE_SIZE,
				       0, flags);

	if (!(dev->buffer_start)) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "alloc cma buffer failed\n");
		return -1;
	}

	dev->buffer_size = CMA_ALLOC_SIZE * SZ_1M;

	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "vdetect video %d cma memory is %x , size is  %x\n",
		      dev->inst, (unsigned int)dev->buffer_start,
		      (unsigned int)dev->buffer_size);

	return 0;
}

static int vdetect_cma_buf_uninit(struct vdetect_dev *dev)
{
	int ret;

	ret = free_canvas(dev);
	if (ret < 0)
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s free canvas fail! line: %d",
			      __func__, __LINE__);
	if (dev->buffer_start != 0) {
		codec_mm_free_for_dma("vdetect", dev->buffer_start);
		dev->buffer_start = 0;
		dev->buffer_size = 0;
		vdetect_print(dev->inst, PRINT_CAPTUREINFO,
			      "vdetect video %d cma memory release succeed!\n",
			      dev->inst);
	}
	return 0;
}

static DEFINE_SPINLOCK(devlist_lock);

static unsigned long vdetect_devlist_lock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&devlist_lock, flags);
	return flags;
}

static void vdetect_devlist_unlock(unsigned long flags)
{
	spin_unlock_irqrestore(&devlist_lock, flags);
}

static struct vframe_s *vdetect_vf_peek(void *op_arg)
{
	struct vdetect_dev *dev = (struct vdetect_dev *)op_arg;

	return vf_peek(dev->recv_name);
}

static struct vframe_s *vdetect_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct vdetect_dev *dev = (struct vdetect_dev *)op_arg;
	unsigned long flags;
	bool bypass_flag = false;

	vf = vf_get(dev->recv_name);

	if (!vf)
		return NULL;

	vf->nn_value[AI_PQ_TOP - 1].maxclass = dev->vf_num;
	dev->vf_num++;
	post_get_count++;

	spin_lock_irqsave(&lock, flags);
	if (vf->flag & VFRAME_FLAG_GAME_MODE ||
		vf->width > 3840 ||
		vf->height > 2160) {
		dev->last_vf = NULL;
		bypass_flag = true;
	} else {
		vf->flag |= VFRAME_FLAG_THROUGH_VDETECT;
	}

	if (!bypass_flag &&
	    (atomic_read(&dev->vdect_status) != VDETECT_PREPARE))
		dev->last_vf = vf;
	spin_unlock_irqrestore(&lock, flags);

	if (atomic_read(&dev->is_playing) == 0) {
		atomic_set(&dev->is_playing, 1);
		if (detect_thread)
			up(&dev->thread_sem);
	}

	return vf;
}

static void vdetect_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdetect_dev *dev = (struct vdetect_dev *)op_arg;
	unsigned long lock_flag;

	spin_lock_irqsave(&lock, lock_flag);
	if (atomic_read(&dev->vdect_status) == VDETECT_PREPARE ||
	    (vf->flag & VFRAME_FLAG_VIDEO_VDETECT)) {
		vf->flag |= VFRAME_FLAG_VIDEO_VDETECT_PUT;
		spin_unlock_irqrestore(&lock, lock_flag);
	} else {
		spin_unlock_irqrestore(&lock, lock_flag);
		vf_put(vf, dev->recv_name);
		pre_put_count++;
	}
	post_put_count++;
}

static int get_source_type(struct vframe_s *vf)
{
	enum vframe_source_type ret;
	int interlace_mode;

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
	    vf->source_type == VFRAME_SOURCE_TYPE_CVBS) {
		if ((vf->bitdepth & BITDEPTH_Y10) &&
		    (!(vf->type & VIDTYPE_COMPRESS)) &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL))
			ret = VDIN_10BIT_NORMAL;
		else
			ret = VDIN_8BIT_NORMAL;
	} else {
		if ((vf->bitdepth & BITDEPTH_Y10) &&
		    (!(vf->type & VIDTYPE_COMPRESS)) &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)) {
			if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_10BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_10BIT_BOTTOM;
			else
				ret = DECODER_10BIT_NORMAL;
		} else {
			if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_8BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_8BIT_BOTTOM;
			else
				ret = DECODER_8BIT_NORMAL;
		}
	}
	return ret;
}

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_YUV420;
	enum vframe_source_type soure_type;

	soure_type = get_source_type(vf);
	switch (soure_type) {
	case DECODER_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case DECODER_8BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444B & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420B & (3 << 3));
		break;
	case DECODER_8BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444T & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420T & (3 << 3));
		break;
	case DECODER_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	case DECODER_10BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422B
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422B
					& (3 << 3));
		}
		break;
	case DECODER_10BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422T
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422T
					& (3 << 3));
		}
		break;
	case VDIN_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case VDIN_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	default:
		format = GE2D_FORMAT_M24_YUV420;
	}
	return format;
}

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
	case V4L2_PIX_FMT_RGB32:
		format = GE2D_FORMAT_S32_ABGR;
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
	return format;
}

int get_vdetect_canvas_index(struct vdetect_output *output,
			     int start_canvas)
{
	int canvas = start_canvas;
	int v4l2_format = output->v4l2_format;
	void *buf = (void *)output->vbuf;
	int width = output->width;
	int height = output->height;
	int canvas_height = height;

	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_VYUY:
		canvas = start_canvas;
		canvas_config(canvas, (unsigned long)buf, width * 2,
			      canvas_height, CANVAS_ADDR_NOWRAP,
			      CANVAS_BLKMODE_LINEAR);
		break;
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
		canvas = start_canvas;
		canvas_config(canvas, (unsigned long)buf, width * 3,
			      canvas_height, CANVAS_ADDR_NOWRAP,
			      CANVAS_BLKMODE_LINEAR);
		break;
	case V4L2_PIX_FMT_RGB32:
		canvas = start_canvas;
		canvas_config(canvas, (unsigned long)buf, width * 4,
			      canvas_height, CANVAS_ADDR_NOWRAP,
			      CANVAS_BLKMODE_LINEAR);
		break;
	default:
		break;
	}
	return canvas;
}

static int copy_phybuf_to_file(ulong phys, u32 size,
			       struct file *fp, loff_t pos)
{
	u32 span = SZ_1M;
	u8 *p;
	int remain_size = 0;
	ssize_t ret;

	remain_size = size;
	while (remain_size > 0) {
		if (remain_size < span)
			span = remain_size;
		p = codec_mm_vmap(phys, PAGE_ALIGN(span));
		if (!p) {
			pr_info("vdetect: vmap failed\n");
			return -1;
		}
		codec_mm_dma_flush(p, span, DMA_FROM_DEVICE);
		ret = vfs_write(fp, (char *)p, span, &pos);
		if (ret <= 0)
			pr_info("vdetect: vfs write failed!\n");
		phys += span;
		codec_mm_unmap_phyaddr(p);
		remain_size -= span;

		pr_info("pos: %lld, phys: %lx, remain_size: %d\n",
			pos, phys, remain_size);
	}
	return 0;
}

static int vdetect_ge2d_process(struct config_para_ex_s *ge2d_config,
				struct vdetect_output *output,
				struct vdetect_dev *dev)
{
	struct vframe_s *vf = dev->ge2d_vf;
	struct ge2d_context_s *context = dev->context;
	struct canvas_s cs0, cs1, cs2, cd;
	int interlace_mode;
	int output_canvas = output->canvas_id;
	int input_width, input_height;
	unsigned long lock_flag;
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);
	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	mutex_lock(&dev->vf_mutex);
	if (atomic_read(&dev->vdect_status) == VDETECT_DROP_VF) {
		mutex_unlock(&dev->vf_mutex);
		atomic_set(&dev->vdect_status, VDETECT_INIT);
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s exit\n", __func__);
		return -2;
	}
	if (IS_ERR_OR_NULL(vf)) {
		vdetect_print(dev->inst, PRINT_ERROR, "%s: vf is NULL!\n", __func__);
		return -2;
	}

	input_width = vf->width;
	input_height = vf->height;
	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(dev->src_canvas[0],
				     &vf->canvas0_config[0]);
		if (vf->plane_num == 2) {
			canvas_config_config(dev->src_canvas[1],
					     &vf->canvas0_config[1]);
		} else if (vf->plane_num == 3) {
			canvas_config_config(dev->src_canvas[2],
					     &vf->canvas0_config[2]);
		}
		ge2d_config->src_para.canvas_index =
			dev->src_canvas[0]
			| (dev->src_canvas[1] << 8)
			| (dev->src_canvas[2] << 16);
		ge2d_config->src_planes[0].addr =
				vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				vf->canvas0_config[0].width;
		ge2d_config->src_planes[0].h =
				vf->canvas0_config[0].height;
		ge2d_config->src_planes[1].addr =
				vf->canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				vf->canvas0_config[1].width;
		ge2d_config->src_planes[1].h =
				vf->canvas0_config[1].height >> 1;
		if (vf->plane_num == 3) {
			ge2d_config->src_planes[2].addr =
				vf->canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas0_config[2].height >> 1;
		}
	} else {
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
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "src canvas 0x%x 0x%x 0x%x\n",
		      dev->src_canvas[0],
		      dev->src_canvas[1],
		      dev->src_canvas[2]);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "src width: %d, height: %d\n",
		      input_width, input_height);
	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM ||
	    interlace_mode == VIDTYPE_INTERLACE_TOP)
		input_height >>= 1;
	ge2d_config->src_para.format = get_input_format(vf);
	mutex_unlock(&dev->vf_mutex);
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = input_width;
	ge2d_config->src_para.height = input_height;
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format =
		get_output_format(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = dev->dst_width;
	ge2d_config->dst_para.height = dev->dst_height;
	ge2d_config->dst_xy_swap = 0;

	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "dst canvas 0X%x\n", dev->dst_canvas);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "dst width: %d, height: %d\n",
		      dev->dst_width, dev->dst_height);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "dst_buffer addr: 0X%lx\n", cd.addr);
	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "++ge2d configing error.\n");
		return -1;
	}

	stretchblt_noalpha(context, 0, 0, input_width, input_height,
			   0, 0, output->width, output->height);

	mutex_lock(&dev->vf_mutex);
	if (atomic_read(&dev->vdect_status) == VDETECT_DROP_VF) {
		mutex_unlock(&dev->vf_mutex);
		atomic_set(&dev->vdect_status, VDETECT_INIT);
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s line: %d\n", __func__, __LINE__);
		return -2;
	}

	spin_lock_irqsave(&lock, lock_flag);
	if (vf->flag & VFRAME_FLAG_VIDEO_VDETECT)
		vf->flag &= ~VFRAME_FLAG_VIDEO_VDETECT;

	if (vf->flag & VFRAME_FLAG_VIDEO_VDETECT_PUT) {
		vf->flag &= ~VFRAME_FLAG_VIDEO_VDETECT_PUT;
		spin_unlock_irqrestore(&lock, lock_flag);
		vf_put(vf, dev->recv_name);
		pre_put_count++;
		vdetect_print(dev->inst, PRINT_CAPTUREINFO,
			"%s put vf buffer!\n", __func__);
	} else {
		spin_unlock_irqrestore(&lock, lock_flag);
		vdetect_print(dev->inst, PRINT_CAPTUREINFO,
			"ge2d don't put!\n");
	}
	mutex_unlock(&dev->vf_mutex);

	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s end line %d\n", __func__, __LINE__);
	return 0;
}

static int vdetect_fill_buffer(struct vdetect_dev *dev)
{
	struct vdetect_buffer *buf = NULL;
	struct config_para_ex_s ge2d_config;
	struct vdetect_output output;
	struct vdetect_dmaqueue *dma_q = &dev->dma_q;
	int ret = 0;
	void *vbuf = NULL;
	unsigned long flags = 0;
	unsigned long flag;
	mm_segment_t old_fs;
	struct file *filp_scr = NULL;
	struct canvas_s cd;
	int result = 0;
	ulong phys;
	u32 width, height;
	struct timeval time_begin;
	struct timeval time_end;
	int cost_time;

	if (dev->ge2d_vf == dev->last_vf) {
		vdetect_print(dev->inst, PRINT_CAPTUREINFO,
			"%s: the same frame!\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(dev->last_vf)) {
		vdetect_print(dev->inst, PRINT_ERROR,
			"%s: vf is NULL!\n", __func__);
		return -1;
	}

	spin_lock_irqsave(&dev->slock, flags);

	if (list_empty(&dma_q->active)) {
		spin_unlock_irqrestore(&dev->slock, flags);
		vdetect_print(dev->inst, PRINT_ERROR,
			"%s: dma queue is inactive!\n", __func__);
		return -1;
	}

	buf = list_entry(dma_q->active.next, struct vdetect_buffer, vb.queue);
	buf->vb.state = VIDEOBUF_ACTIVE;
	list_del(&buf->vb.queue);
	spin_unlock_irqrestore(&dev->slock, flags);

	vbuf = (void *)videobuf_to_res(&buf->vb);

	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s buf_addr: 0x%lx\n", __func__, (unsigned long)vbuf);
	if (!vbuf) {
		vdetect_print(dev->inst, PRINT_ERROR, "vbuf is invalue!\n");
		goto fail;
	}
	/* memset(&ge2d_config,0,sizeof(struct config_para_ex_s)); */
	memset(&output, 0, sizeof(struct vdetect_output));

	output.v4l2_format = dev->fmt->fourcc;
	output.vbuf = vbuf;
	output.width = buf->vb.width;
	output.height = buf->vb.height;

	output.canvas_id =
		get_vdetect_canvas_index(&output, dev->dst_canvas);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);
	mutex_lock(&dev->vf_mutex);
	if (atomic_read(&dev->vdect_status) == VDETECT_DROP_VF) {
		mutex_unlock(&dev->vf_mutex);
		atomic_set(&dev->vdect_status, VDETECT_INIT);
		vdetect_print(dev->inst, PRINT_CAPTUREINFO,
			      "%s line: %d\n", __func__, __LINE__);
		goto fail;
	}

	spin_lock_irqsave(&lock, flag);
	atomic_set(&dev->vdect_status, VDETECT_PREPARE);
	dev->last_vf->flag |= VFRAME_FLAG_VIDEO_VDETECT;
	dev->ge2d_vf = dev->last_vf;
	atomic_set(&dev->vdect_status, VDETECT_INIT);
	spin_unlock_irqrestore(&lock, flag);

	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);

	if (detect_dump & 1) {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		filp_scr = filp_open("/data/temp/screen_capture",
				     O_RDWR | O_CREAT, 0666);
		if (IS_ERR(filp_scr)) {
			pr_info("open /data/temp/screen_capture failed\n");
		} else {
			if (dev->ge2d_vf->canvas0Addr == (u32)-1) {
				phys = (ulong)
				dev->ge2d_vf->canvas0_config[0].phy_addr;
				width = dev->ge2d_vf->canvas0_config[0].width;
				height =
					dev->ge2d_vf->canvas0_config[0].height;
			} else {
				canvas_read(dev->ge2d_vf->canvas0Addr & 0xff,
					    &cd);
				phys = cd.addr;
				width = cd.width;
				height = cd.height;
			}
			result = copy_phybuf_to_file(phys, width * height,
						     filp_scr, 0);
			if (result < 0)
				pr_info("write screen_capture failed\n");
			pr_info("scr addr: %0lx, width: %d, height: %d\n",
				phys, width, height);
			pr_info("dump source type: %d\n",
				get_input_format(dev->ge2d_vf));
			vfs_fsync(filp_scr, 0);
			filp_close(filp_scr, NULL);
			set_fs(old_fs);
		}
	}
	mutex_unlock(&dev->vf_mutex);
	do_gettimeofday(&time_begin);
	ret = vdetect_ge2d_process(&ge2d_config, &output, dev);
	if (ret < 0) {
		if ((ret == -2) || atomic_read(&dev->is_playing) == 0) {
			dev->ge2d_vf = NULL;
			vdetect_print(dev->inst, PRINT_ERROR,
				      "%s exit\n", __func__);
			goto fail;
		}
		mutex_lock(&dev->vf_mutex);
		if (atomic_read(&dev->vdect_status) == VDETECT_DROP_VF) {
			mutex_unlock(&dev->vf_mutex);
			dev->ge2d_vf = NULL;
			atomic_set(&dev->vdect_status, VDETECT_INIT);
			vdetect_print(dev->inst, PRINT_ERROR,
				      "%s line: %d\n", __func__, __LINE__);
			goto fail;
		}

		if (dev->ge2d_vf->flag & VFRAME_FLAG_VIDEO_VDETECT)
			dev->ge2d_vf->flag &= ~VFRAME_FLAG_VIDEO_VDETECT;
		if (dev->ge2d_vf->flag & VFRAME_FLAG_VIDEO_VDETECT_PUT) {
			dev->ge2d_vf->flag &= ~VFRAME_FLAG_VIDEO_VDETECT_PUT;
			vf_put(dev->ge2d_vf, dev->recv_name);
			pre_put_count++;
		}
		mutex_unlock(&dev->vf_mutex);
		dev->ge2d_vf = NULL;
		vdetect_print(dev->inst, PRINT_ERROR, "ge2d deal failed!\n");
		goto fail;
	}
	do_gettimeofday(&time_end);
	cost_time = (time_end.tv_sec * 1000 * 1000 + time_end.tv_usec
		     - time_begin.tv_sec * 1000 * 1000
		     - time_begin.tv_usec) / 1000;
	vdetect_print(dev->inst, PRINT_TIMESTAMP,
		      "ge2d cost time: %d ms\n", cost_time);
	buf->vb.state = VIDEOBUF_DONE;
	/*wait up vb done buffer workqueue*/
	if (waitqueue_active(&buf->vb.done))
		wake_up(&buf->vb.done);
	return ret;
fail:
	spin_lock_irqsave(&dev->slock, flags);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &dma_q->active);
	spin_unlock_irqrestore(&dev->slock, flags);
	return ret;
}

static int vdetect_thread(void *data)
{
	struct vdetect_dev *dev = data;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};
	int ret = 0;

	sched_setscheduler(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "start %s thread.\n", dev->recv_name);

	while (down_interruptible(&dev->thread_sem) == 0) {
		if (atomic_read(&dev->is_playing) == 0) {
			vdetect_print(dev->inst, PRINT_CAP_OTHER,
				      "%s has no vf source\n", __func__);
			continue;
		}
		if (dev->begin_time.tv_sec == 0 &&
		    dev->begin_time.tv_usec == 0)
			do_gettimeofday(&dev->begin_time);
		if (kthread_should_stop()) {
			vdetect_print(dev->inst, PRINT_CAPTUREINFO,
				      "%s kthread stop 1.\n", dev->recv_name);
			break;
		}

	#ifdef USE_SEMA_QBUF
		ret = wake_up_interruptible(&dev->dma_q.qbuf_comp);
	#endif
		vdetect_print(dev->inst, PRINT_THREADINFO,
			      "%s task_running = %d\n", dev->recv_name,
			      dev->dma_q.task_running);
		if (!dev->dma_q.task_running) {
			vdetect_print(dev->inst, PRINT_CAPTUREINFO,
				      "%s here break.\n", dev->recv_name);
			break;
		}
		vdetect_fill_buffer(dev);
		if (kthread_should_stop()) {
			vdetect_print(dev->inst, PRINT_CAPTUREINFO,
				      "%s kthread stop 2.\n", dev->recv_name);
			break;
		}
		vdetect_print(dev->inst, PRINT_THREADINFO,
			      "%s_thread while 1 .\n", dev->recv_name);
	}
	while (!kthread_should_stop()) {
		vdetect_print(dev->inst, PRINT_CAPTUREINFO,
			      "%s_thread while 2 .\n", dev->recv_name);
		usleep_range(9000, 10000);
	}
		/*msleep(10);*/
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "thread: exit\n");
	return ret;
}

/* ------------------------------------------------------------------
 * Videobuf operations
 * ------------------------------------------------------------------
 */
static int buffer_setup(struct videobuf_queue *vq, unsigned int *count,
			unsigned int *size)
{
	struct videobuf_res_privdata *res =
		(struct videobuf_res_privdata *)vq->priv_data;
	struct vdetect_dev *dev = (struct vdetect_dev *)res->priv;
	int height = dev->height;

	vdetect_print(dev->inst, PRINT_BUFFERINFO,
		      "%s line: %d\n", __func__, __LINE__);
	if (height % 16 != 0)
		height = ((height + 15) >> 4) << 4;

	*size = (dev->width * height * dev->fmt->depth) >> 3;
	vdetect_print(dev->inst, PRINT_BUFFERINFO,
		      "%s, width=%d, height=%d\n",
		      __func__, dev->width, height);
	if (*count == 0)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;
	vdetect_print(dev->inst, PRINT_BUFFERINFO,
		      "%s, count=%d, size=%d\n", __func__, *count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct vdetect_buffer *buf)
{
	struct videobuf_res_privdata *res =
		(struct videobuf_res_privdata *)vq->priv_data;
	struct vdetect_dev *dev = (struct vdetect_dev *)res->priv;

	vdetect_print(dev->inst, PRINT_BUFFERINFO,
		      "%s state:%i\n", __func__, buf->vb.state);
	videobuf_waiton(vq, &buf->vb, 0, 0);
	if (in_interrupt())
		WARN_ON(1);
	videobuf_res_free(vq, &buf->vb);
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 2000
#define norm_maxh() 1600

static int buffer_prepare(struct videobuf_queue *vq,
			  struct videobuf_buffer *videobuf,
			  enum v4l2_field field)
{
	struct videobuf_res_privdata *res =
		(struct videobuf_res_privdata *)vq->priv_data;
	struct vdetect_dev *dev = (struct vdetect_dev *)res->priv;
	struct vdetect_buffer *buf =
		container_of(videobuf, struct vdetect_buffer, vb);
	void *vbuf;
	int rc;

	vdetect_print(dev->inst, PRINT_BUFFERINFO,
		      "%s field: %d\n", __func__, field);
	if (dev->width < 16 || dev->width > norm_maxw() ||
	    dev->height < 16 || dev->height > norm_maxh()) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s line: %d\n", __func__, __LINE__);
		return -EINVAL;
	}
	buf->vb.size = (dev->width * dev->height * dev->fmt->depth) >> 3;
	if (buf->vb.baddr != 0 && buf->vb.bsize < buf->vb.size) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s line: %d\n", __func__, __LINE__);
		return -EINVAL;
	}
	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt = dev->fmt;
	buf->vb.width = dev->width;
	buf->vb.height = dev->height;
	buf->vb.field = field;
	vbuf = (void *)videobuf_to_res(&buf->vb);
	if (buf->vb.state == VIDEOBUF_NEEDS_INIT) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		vdetect_print(dev->inst, PRINT_BUFFERINFO,
			      "%s addr: 0x%lx, i: %d, magic:%d", __func__,
			      (unsigned long)vbuf, buf->vb.i, buf->vb.magic);
		if (rc < 0) {
			vdetect_print(dev->inst, PRINT_ERROR,
				      "%s line: %d\n", __func__, __LINE__);
			goto fail;
		}
	}
	buf->vb.state = VIDEOBUF_PREPARED;
	return 0;

fail: free_buffer(vq, buf);
	return rc;
}

static void buffer_queue(struct videobuf_queue *vq,
			 struct videobuf_buffer *videobuf)
{
	struct vdetect_buffer *buf =
		container_of(videobuf, struct vdetect_buffer, vb);
	struct videobuf_res_privdata *res =
		(struct videobuf_res_privdata *)vq->priv_data;
	struct vdetect_dev *dev = (struct vdetect_dev *)res->priv;
	struct vdetect_dmaqueue *dma_q = &dev->dma_q;

	vdetect_print(dev->inst, PRINT_BUFFERINFO,
		      "%s line: %d\n", __func__, __LINE__);

	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &dma_q->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *videobuf)
{
	struct vdetect_buffer *buf =
		container_of(videobuf, struct vdetect_buffer, vb);
	struct videobuf_res_privdata *res =
		(struct videobuf_res_privdata *)vq->priv_data;
	struct vdetect_dev *dev = (struct vdetect_dev *)res->priv;

	vdetect_print(dev->inst, PRINT_BUFFERINFO,
		      "%s line: %d\n", __func__, __LINE__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops vdetect_qops = {
	.buf_setup = buffer_setup,
	.buf_prepare = buffer_prepare,
	.buf_queue = buffer_queue,
	.buf_release = buffer_release,
};

static int vdetect_event_cb(int type, void *data, void *private_data)
{
	struct vdetect_dev *dev = (struct vdetect_dev *)private_data;

	if (type & VFRAME_EVENT_RECEIVER_PUT)
		;
	else if (type & VFRAME_EVENT_RECEIVER_GET)
		;
	else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT)
		vdetect_print(dev->inst, PRINT_PATHINFO,
			      "receiver is waiting\n");
	else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT)
		vdetect_print(dev->inst, PRINT_PATHINFO,
			      "frame wait\n");
	return 0;
}

static int vdetect_vf_states(struct vframe_states *states, void *op_arg)
{
	states->vf_pool_size = 0;
	states->buf_recycle_num = 0;
	states->buf_free_num = 0;
	states->buf_avail_num = 0;
	/* spin_unlock_irqrestore(&lock, flags); */
	return 0;
}

static const struct vframe_operations_s vdetect_vf_prov = {
		.peek = vdetect_vf_peek,
		.get = vdetect_vf_get,
		.put = vdetect_vf_put,
		.event_cb = vdetect_event_cb,
		.vf_states = vdetect_vf_states,
};

static int video_receiver_event_fun(int type, void *data, void *private_data)
{
	struct vdetect_dev *dev = (struct vdetect_dev *)private_data;
	struct vframe_states states;
	int i = 0;

	switch (type) {
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		vf_notify_receiver(dev->prov_name, type, NULL);
		vdetect_print(dev->inst, PRINT_PATH_OTHER, "ready\n");
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		vdetect_print(dev->inst, PRINT_PATH_OTHER, "state\n");
		vdetect_vf_states(&states, dev);
		if (states.buf_avail_num >= 0)
			return RECEIVER_ACTIVE;
		if (vf_notify_receiver(dev->prov_name, type, NULL)
				       == RECEIVER_ACTIVE) {
			return RECEIVER_ACTIVE;
		}
		return RECEIVER_INACTIVE;
	case VFRAME_EVENT_PROVIDER_START:
		vf_notify_receiver(dev->prov_name, type, NULL);
		vdetect_print(dev->inst, PRINT_PATH_OTHER, "start\n");
		break;
	case VFRAME_EVENT_PROVIDER_REG:
		vf_reg_provider(&dev->prov);
		vdetect_print(dev->inst, PRINT_PATHINFO, "reg\n");
		memset(nn_info, 0, sizeof(nn_info));
		dev->vf_num = 0;
		for (i = 0; i < AI_PQ_TOP; i++) {
			dev->nn_value[i].maxclass = 0;
			dev->nn_value[i].maxprob = 0;
		}
		mutex_lock(&dev->vf_mutex);
		atomic_set(&dev->is_dev_reged, 1);
		mutex_unlock(&dev->vf_mutex);
		post_get_count = 0;
		pre_put_count = 0;
		post_put_count = 0;
		break;
	case VFRAME_EVENT_PROVIDER_UNREG:
		mutex_lock(&dev->vf_mutex);
		atomic_set(&dev->vdect_status, VDETECT_DROP_VF);
		atomic_set(&dev->is_dev_reged, 0);
		mutex_unlock(&dev->vf_mutex);
		atomic_set(&dev->is_playing, 0);
		vf_unreg_provider(&dev->prov);
		vdetect_print(dev->inst, PRINT_PATHINFO, "unreg\n");
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		mutex_lock(&dev->vf_mutex);
		atomic_set(&dev->vdect_status, VDETECT_DROP_VF);
		mutex_unlock(&dev->vf_mutex);
		atomic_set(&dev->is_playing, 0);
		vf_light_unreg_provider(&dev->prov);
		vdetect_print(dev->inst, PRINT_PATHINFO, "light unreg\n");
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		mutex_lock(&dev->vf_mutex);
		atomic_set(&dev->vdect_status, VDETECT_DROP_VF);
		mutex_unlock(&dev->vf_mutex);
		atomic_set(&dev->is_playing, 0);
		vdetect_print(dev->inst, PRINT_PATHINFO, "reset\n");
		vf_notify_receiver(dev->prov_name, type, NULL);
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
		vf_notify_receiver(dev->prov_name, type, data);
		break;
	default:
		break;
	}
	return 0;
}

static const struct vframe_receiver_op_s vdetect_vf_recv = {
	.event_cb = video_receiver_event_fun
};

static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct vdetect_dev *dev = NULL;

	dev = video_drvdata(file);

	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);

	strcpy(cap->driver, "vdetect");
	strcpy(cap->card, "vdetect");
	strlcpy(cap->bus_info,
		dev->v4l2_dev.name,
		sizeof(cap->bus_info));
	cap->version = VDETECT_VERSION;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
			| V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct vdetect_dev *dev = NULL;
#ifdef USE_SEMA_QBUF
	struct vdetect_dmaqueue *dma_q = NULL;
#endif
	dev = video_drvdata(file);
#ifdef USE_SEMA_QBUF
	dma_q = &dev->dma_q;
	init_completion(&dma_q->qbuf_comp);
#endif
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);
	return videobuf_reqbufs(&dev->vbuf_queue, p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct vdetect_dev *dev = NULL;
	struct vdetect_output output;
	int ret = 0;

	dev = video_drvdata(file);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);

	ret = videobuf_querybuf(&dev->vbuf_queue, p);

	if (ret == 0) {
		memset(&output, 0, sizeof(struct vdetect_output));

		output.v4l2_format = dev->fmt->fourcc;
		output.vbuf = NULL;
		output.width = dev->width;
		output.height = dev->height;
		output.canvas_id = -1;
		p->reserved = dev->dst_canvas;
	} else {
		p->reserved = 0;
	}

	return ret;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct vdetect_dev *dev = NULL;
	#ifdef USE_SEMA_QBUF
	struct vdetect_dmaqueue *dma_q = NULL;
	#endif
	int ret = 0;

	dev = video_drvdata(file);

	#ifdef USE_SEMA_QBUF
	dma_q = &dev->dma_q;
	complete(&dma_q->qbuf_comp);
	#endif
	if (detect_thread) {
		dev->begin_time.tv_sec = 0;
		dev->begin_time.tv_usec = 0;
		up(&dev->thread_sem);
	}
	ret = videobuf_qbuf(&dev->vbuf_queue, p);
	vdetect_print(dev->inst, PRINT_CAP_OTHER,
		      "%s ret: %d line: %d\n", __func__, ret, __LINE__);
	return ret;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct vdetect_dev *dev = video_drvdata(file);
	struct timeval time_begin;
	struct timeval time_end;
	int use_time;
	int ret;
	unsigned long flags;

	if (atomic_read(&dev->is_playing) == 0) {
		vdetect_print(dev->inst, PRINT_CAP_OTHER,
			      "%s has no vf source\n", __func__);
		return -ENOMEM;
	}

	if (!aipq_enable)  {
		vdetect_print(dev->inst, PRINT_CAP_OTHER,
			      "%s aipq function stop\n", __func__);
		return -EAGAIN;
	}

	if (!dev->last_vf) {
		vdetect_print(dev->inst, PRINT_CAP_OTHER,
			      "%s: no need ai pq.\n", __func__);
		return -EAGAIN;
	}

	if (!detect_thread) {
		do_gettimeofday(&time_begin);
		vdetect_fill_buffer(dev);
	}

	ret = videobuf_dqbuf(&dev->vbuf_queue, p, file->f_flags & O_NONBLOCK);
	if (ret >= 0) {
		mutex_lock(&dev->vf_mutex);
		if (atomic_read(&dev->is_dev_reged) == 1) {
			spin_lock_irqsave(&lock, flags);
			if (IS_ERR_OR_NULL(dev->last_vf))
				frame_number = 0;
			else
				frame_number = dev->last_vf->nn_value[AI_PQ_TOP - 1].maxclass;
			spin_unlock_irqrestore(&lock, flags);
			vdetect_print(dev->inst, PRINT_MN_SET_FLOW,
					"%d: sdk get nn info from %d frame\n",
					__LINE__, frame_number);
		}
		mutex_unlock(&dev->vf_mutex);
		vdetect_print(dev->inst, PRINT_CAPTUREINFO,
			      "%s ret: %d, index: %d\n",
			      __func__, ret, p->index);
	} else {
		if (detect_thread)
			up(&dev->thread_sem);
		vdetect_print(dev->inst, PRINT_CAP_OTHER,
			      "%s ret: %d\n", __func__, ret);
	}
	do_gettimeofday(&time_end);
	if (!detect_thread) {
		use_time = (time_end.tv_sec * 1000 * 1000 + time_end.tv_usec
			    - time_begin.tv_sec * 1000 * 1000
			    - time_begin.tv_usec) / 1000;
		vdetect_print(dev->inst, PRINT_TIMESTAMP,
			      "%s block mode all cost time: %d ms line: %d\n",
			      __func__, use_time, __LINE__);
	} else {
		use_time = (time_end.tv_sec * 1000 * 1000 + time_end.tv_usec
			    - dev->begin_time.tv_sec * 1000 * 1000
			    - dev->begin_time.tv_usec) / 1000;
		vdetect_print(dev->inst, PRINT_TIMESTAMP,
			      "%s thread mode all cost time: %d ms line: %d\n",
			      __func__, use_time, __LINE__);
	}
	return ret;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct vdetect_dev *dev = NULL;

	dev = video_drvdata(file);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);
	if (dev->set_format_flag) {
		f->fmt.pix.width = dev->width;
		f->fmt.pix.height = dev->height;
		f->fmt.pix.field = dev->vbuf_queue.field;
		f->fmt.pix.pixelformat = dev->fmt->fourcc;
		f->fmt.pix.bytesperline =
		(f->fmt.pix.width * dev->fmt->depth) >> 3;
		f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;
	} else {
		f->fmt.pix.width = 0;
		f->fmt.pix.height = 0;
	}

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct vdetect_fmt *fmt = NULL;
	struct vdetect_dev *dev = NULL;
	enum v4l2_field field;
	unsigned int maxw, maxh;

	dev = video_drvdata(file);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);
	fmt = get_format(f);
	if (!fmt) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "Fourcc format (0x%08x) invalid.\n",
			      f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	field = f->fmt.pix.field;

	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	} else if (field != V4L2_FIELD_INTERLACED) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "Field type invalid.\n");
		return -EINVAL;
	}

	maxw = norm_maxw();
	maxh = norm_maxh();

	f->fmt.pix.field = field;
	v4l_bound_align_image(&f->fmt.pix.width, 16, maxw, 2,
			      &f->fmt.pix.height, 16, maxh, 0, 0);
	f->fmt.pix.bytesperline = (f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;
	return 0;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	int ret = 0;
	struct vdetect_dev *dev = NULL;
	struct videobuf_queue *q = NULL;

	dev = video_drvdata(file);
	q = &dev->vbuf_queue;
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);
	dev->dst_width = f->fmt.pix.width;
	dev->dst_height = f->fmt.pix.height;
	f->fmt.pix.width = (f->fmt.pix.width + 31) & (~31);
	if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_YVU420 ||
	    f->fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420) {
		f->fmt.pix.width =
			(f->fmt.pix.width + 63) & (~63);
	}

	ret = vidioc_try_fmt_vid_cap(file, priv, f);
	if (ret < 0) {
		vdetect_print(dev->inst, PRINT_CAPTUREINFO,
			      "%s line: %d\n", __func__, __LINE__);
		return ret;
	}
	mutex_lock(&q->vb_lock);

	if (videobuf_queue_is_busy(&dev->vbuf_queue)) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s queue busy\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	dev->fmt = get_format(f);
	dev->width = f->fmt.pix.width;
	dev->height = f->fmt.pix.height;
	dev->vbuf_queue.field = f->fmt.pix.field;
	dev->type = f->type;
	dev->set_format_flag = true;
	ret = 0;
out: mutex_unlock(&q->vb_lock);
	return ret;
}

static int vidioc_g_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	struct vdetect_dev *dev = NULL;

	dev = video_drvdata(file);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);
	return 0;
}

static int vidioc_s_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	struct vdetect_dev *dev = NULL;
	int i, j;
	struct nn_value_t last_saved_val[AI_PQ_TOP];
	int buf_num, last_saved_num, behind_count;

	dev = video_drvdata(file);

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		vdetect_print(dev->inst, PRINT_CAPTUREINFO,
				"not video capture.\n");
		return 0;
	}
	vdetect_print(dev->inst, PRINT_MN_SET_FLOW,
			"sdk set nn info for %d frame start, buf_num is %d.\n",
			frame_number, nn_frame_num);
	if (nn_frame_num == 0)
		buf_num = MAX_NN_INFO_BUF_COUNT - 1;
	else
		buf_num = nn_frame_num - 1;

	last_saved_num = nn_info[buf_num].nn_frame_num;
	if (last_saved_num != 0) {
		memset(last_saved_val, 0, sizeof(last_saved_val));
		for (i = 0; i < AI_PQ_TOP - 1; i++) {
			last_saved_val[i].maxclass =
				nn_info[buf_num].nn_value[i].maxclass;
			last_saved_val[i].maxprob =
				nn_info[buf_num].nn_value[i].maxprob;
		}

		behind_count = frame_number - last_saved_num;
		for (i = 1; i < behind_count; i++) {
			nn_info[nn_frame_num].nn_frame_num =
				last_saved_num + i;
			for (j = 0; j < AI_PQ_TOP - 1; j++) {
				nn_info[nn_frame_num].nn_value[j].maxclass =
					last_saved_val[j].maxclass;
				nn_info[nn_frame_num].nn_value[j].maxprob =
					last_saved_val[j].maxprob;
			}
			nn_info[nn_frame_num].nn_value[AI_PQ_TOP - 1].maxprob =
				last_saved_num;
			nn_frame_num++;
			if (nn_frame_num == MAX_NN_INFO_BUF_COUNT)
				nn_frame_num = 0;
		}
	}

	nn_info[nn_frame_num].nn_frame_num = frame_number;
	for (i = 0; i < AI_PQ_TOP - 1; i++) {
		j = 4 * i;
		nn_info[nn_frame_num].nn_value[i].maxclass =
			parms->parm.raw_data[j];
		nn_info[nn_frame_num].nn_value[i].maxprob =
			parms->parm.raw_data[j + 1]
			+ (parms->parm.raw_data[j + 2] << 8)
			+ (parms->parm.raw_data[j + 3] << 16);
		vdetect_print(dev->inst, PRINT_CAPTUREINFO,
			"top %d, vnn result: %s, probability: %d.\n",
			i,
			result_vnn[nn_info[nn_frame_num].nn_value[i].maxclass],
			nn_info[nn_frame_num].nn_value[i].maxprob);
	}
	nn_info[nn_frame_num].nn_value[AI_PQ_TOP - 1].maxprob =
		frame_number;
	nn_frame_num++;

	if (nn_frame_num == MAX_NN_INFO_BUF_COUNT)
		nn_frame_num = 0;

	return 0;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct vdetect_dev *dev = NULL;

	dev = video_drvdata(file);

	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);
	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct vdetect_dev *dev = NULL;

	dev = video_drvdata(file);

	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line: %d\n", __func__, __LINE__);
	return 0;
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	int ret;
	struct vdetect_dev *dev = NULL;
	struct vdetect_dmaqueue *dma_q = NULL;

	dev = video_drvdata(file);
	dma_q = &dev->dma_q;
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s begin vdetect.%d\n", __func__, dev->inst);
	if (dev->type != V4L2_BUF_TYPE_VIDEO_CAPTURE || i != dev->type)
		return -EINVAL;
	ret = videobuf_streamon(&dev->vbuf_queue);
	if (ret < 0) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s ret:%d\n", __func__, ret);
		return ret;
	}

#ifdef USE_SEMA_QBUF
	init_completion(&dma_q->qbuf_comp);
#endif
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line %d\n", __func__, __LINE__);

	if (detect_thread) {
		mutex_lock(&dev->vdetect_mutex);
		if (dma_q->task_running) {
			mutex_unlock(&dev->vdetect_mutex);
			return 0;
		}

		dma_q->task_running = 1;
		dma_q->kthread = kthread_run(vdetect_thread,
					     dev, dev->recv_name);

		if (IS_ERR(dma_q->kthread)) {
			pr_err("kernel_thread() failed\n");
			dma_q->task_running = 0;
			dma_q->kthread = NULL;
			mutex_unlock(&dev->vdetect_mutex);
			vdetect_print(dev->inst, PRINT_ERROR,
				      "start thread error.....\n");
			return PTR_ERR(dma_q->kthread);
		}
		mutex_unlock(&dev->vdetect_mutex);
		dev->begin_time.tv_sec = 0;
		dev->begin_time.tv_usec = 0;
		up(&dev->thread_sem);
		vdetect_print(dev->inst, PRINT_THREADINFO,
			      "%s thread start successful!\n", __func__);
	}
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s end vdetect.%d\n", __func__, dev->inst);
	return 0;
}

static int vidioc_streamoff(struct file *file,
			    void *priv, enum v4l2_buf_type i)
{
	int ret;
	struct vdetect_dev *dev = NULL;

	dev = video_drvdata(file);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s begin line %d\n", __func__, __LINE__);
	if (dev->type != V4L2_BUF_TYPE_VIDEO_CAPTURE || i != dev->type)
		return -EINVAL;
	ret = videobuf_streamoff(&dev->vbuf_queue);
	if (ret < 0) {
		vdetect_print(dev->inst, PRINT_ERROR, "%s fail\n", __func__);
		return ret;
	}

	if (IS_ERR_OR_NULL(&dev->dma_q)) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s dma_q is NULL\n", __func__);
		return -1;
	}
	if (detect_thread) {
		vdetect_print(dev->inst, PRINT_THREADINFO,
			      "begin to stop %s thread\n", dev->recv_name);
		mutex_lock(&dev->vdetect_mutex);
		/* shutdown control thread */
		if (!IS_ERR_OR_NULL(dev->dma_q.kthread)) {
			up(&dev->thread_sem);
			dev->dma_q.task_running = 0;
			send_sig(SIGTERM, dev->dma_q.kthread, 1);
#ifdef USE_SEMA_QBUF
			complete(&dev->dma_q.qbuf_comp);
#endif
			wake_up_interruptible(&dev->dma_q.wq);
			vdetect_print(dev->inst, PRINT_THREADINFO,
				      "ready to stop %s thread\n",
				      dev->recv_name);

			ret = kthread_stop(dev->dma_q.kthread);
			if (ret < 0)
				vdetect_print(dev->inst, PRINT_ERROR,
					      "%s, ret = %d .\n",
					      __func__, ret);

			dev->dma_q.kthread = NULL;
		}
		mutex_unlock(&dev->vdetect_mutex);
		vdetect_print(dev->inst, PRINT_THREADINFO,
			      "finish stop %s thread\n", dev->recv_name);
	}
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s end line %d\n", __func__, __LINE__);

	return 0;
}

static const struct v4l2_ioctl_ops vdetect_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
	.vidioc_g_parm = vidioc_g_parm,
	.vidioc_s_parm = vidioc_s_parm,
	.vidioc_s_input = vidioc_s_input,
	.vidioc_g_input = vidioc_g_input,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
};

static int vidioc_open(struct file *file)
{
	struct vdetect_dev *dev = NULL;
	struct videobuf_res_privdata *res = NULL;
	int ret;

	dev = video_drvdata(file);
	mutex_lock(&dev->vdetect_mutex);
	if (dev->fd_num > 0) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s line %d\n", __func__, __LINE__);
		mutex_unlock(&dev->vdetect_mutex);
		return -EBUSY;
	}

	dev->fd_num++;

	ret = vdetect_cma_buf_init(dev);
	if (ret < 0) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s alloc cma buffer fail\n", __func__);
		dev->fd_num--;
		mutex_unlock(&dev->vdetect_mutex);
		return -ENOMEM;
	}

	dev->context = create_ge2d_work_queue();
	if (!dev->context) {
		vdetect_print(dev->inst, PRINT_ERROR,
			      "%s create ge2d fail\n", __func__);
		dev->fd_num--;
		mutex_unlock(&dev->vdetect_mutex);
		return -ENOMEM;
	}
	if (detect_thread)
		sema_init(&dev->thread_sem, 1);
	dev->res.start = dev->buffer_start;
	dev->res.end = dev->buffer_start + dev->buffer_size;
	mutex_unlock(&dev->vdetect_mutex);
	file->private_data = dev;
	dev->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	atomic_set(&dev->vdect_status, VDETECT_INIT);
	dev->fmt = &formats[0];
	dev->width = 224;
	dev->height = 224;
	dev->set_format_flag = false;
	dev->f_flags = file->f_flags;
	dev->dma_q.kthread = NULL;
	dev->ge2d_vf = NULL;
	dev->res.priv = (void *)dev;
	res = &dev->res;

	videobuf_queue_res_init(&dev->vbuf_queue, &vdetect_qops, NULL,
				&dev->slock, dev->type, V4L2_FIELD_INTERLACED,
				sizeof(struct vdetect_buffer), (void *)res,
				NULL);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s vdetect.%d\n", __func__, dev->inst);
	return 0;
}

static ssize_t vidioc_read(struct file *file, char __user *data,
			   size_t count, loff_t *ppos)
{
	struct vdetect_dev *dev = NULL;

	dev = video_drvdata(file);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line %d\n", __func__, __LINE__);
	if (dev->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&dev->vbuf_queue,
					    data, count, ppos, 0,
					    file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int vidioc_poll(struct file *file,
				struct poll_table_struct *wait)
{
	struct vdetect_dev *dev = NULL;
	struct videobuf_queue *q = &dev->vbuf_queue;

	dev = video_drvdata(file);
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s line %d\n", __func__, __LINE__);

	if (dev->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int vidioc_close(struct file *file)
{
	struct vdetect_dev *dev = NULL;
	struct vdetect_dmaqueue *dma_q = NULL;

	dev = video_drvdata(file);
	dma_q = &dev->dma_q;
	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "%s vdetect.%d\n", __func__, dev->inst);
	atomic_set(&dev->vdect_status, VDETECT_INIT);
	if (dma_q->kthread)
		vidioc_streamoff(file, (void *)dev, dev->type);
	videobuf_stop(&dev->vbuf_queue);
	videobuf_mmap_free(&dev->vbuf_queue);
	mutex_lock(&dev->vdetect_mutex);
	if (dev->context)
		destroy_ge2d_work_queue(dev->context);
	vdetect_cma_buf_uninit(dev);
	dev->fd_num--;
	mutex_unlock(&dev->vdetect_mutex);
	return 0;
}

static int vdetect_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = 0;
	struct vdetect_dev *dev = video_drvdata(file);

	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	ret = videobuf_mmap_mapper(&dev->vbuf_queue, vma);

	vdetect_print(dev->inst, PRINT_CAPTUREINFO,
		      "vma start=0x%08lx, size=%ld, ret=%d\n",
		      (unsigned long)vma->vm_start,
		      (unsigned long)vma->vm_end
		      - (unsigned long)vma->vm_start,
		      ret);

	return ret;
}

static const struct v4l2_file_operations vdetect_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = vidioc_open,
	.release = vidioc_close,
	.read = vidioc_read,
	.poll = vidioc_poll,
	.unlocked_ioctl = video_ioctl2, /* V4L2 ioctl handler */
	.mmap = vdetect_mmap,
};

static struct video_device vdetect_template = {
	.name = "vdetect",
	.fops =  &vdetect_v4l2_fops,
	.ioctl_ops = &vdetect_ioctl_ops,
	.release = video_device_release,
	.tvnorms = V4L2_STD_525_60,
};

static LIST_HEAD(vdetect_devlist);

int vdetect_assign_map(char **receiver_name, int *inst)
{
	unsigned long flags;
	struct vdetect_dev *dev = NULL;
	struct list_head *p;

	flags = vdetect_devlist_lock();

	list_for_each(p, &vdetect_devlist) {
		dev = list_entry(p, struct vdetect_dev, vdetect_devlist);
		if (dev->inst == *inst) {
			*receiver_name = dev->recv_name;
			vdetect_devlist_unlock(flags);
			return 0;
		}
	}

	vdetect_devlist_unlock(flags);

	return -ENODEV;
}
EXPORT_SYMBOL(vdetect_assign_map);

int vdetect_get_frame_nn_info(struct vframe_s *vframe)
{
	int ret = -ENODEV;
	int i, j, work_frame_num, max_saved_frame_num, check_flag;
	int buf_last_num;

	if (IS_ERR_OR_NULL(vframe)) {
		vdetect_print(0, PRINT_MN_SET_FLOW,
			"%s: NULL params.\n",
			__func__);
		return ret;
	}

	if (!(vframe->flag & VFRAME_FLAG_THROUGH_VDETECT)) {
		vframe->ai_pq_enable = false;
		vdetect_print(0, PRINT_MN_SET_FLOW,
			"%s: no need ai-pq.\n",
			__func__);
		return 0;
	}

	work_frame_num = vframe->nn_value[AI_PQ_TOP - 1].maxclass;
	buf_last_num = nn_frame_num - 1;
	if (buf_last_num < 0)
		buf_last_num = MAX_NN_INFO_BUF_COUNT - 1;
	vdetect_print(0, PRINT_MN_SET_FLOW,
		"vpp get nn info for %d frame, buf_num is %d.\n",
		work_frame_num, buf_last_num);
	max_saved_frame_num = nn_info[buf_last_num].nn_frame_num;
	if (work_frame_num >= max_saved_frame_num) {
		vdetect_print(0, PRINT_MN_SET_FLOW,
			"use %d frame nn info for %d frame param.\n",
			max_saved_frame_num, work_frame_num);
		for (i = 0; i < AI_PQ_TOP - 1; i++) {
			vframe->nn_value[i].maxclass =
				nn_info[buf_last_num].nn_value[i].maxclass;
			vframe->nn_value[i].maxprob =
				nn_info[buf_last_num].nn_value[i].maxprob;
		}
	} else {
		for (i = 0; i < MAX_NN_INFO_BUF_COUNT; i++) {
			if (work_frame_num == nn_info[i].nn_frame_num) {
				vdetect_print(0, PRINT_MN_SET_FLOW,
					"buf[%d] save nn for %d frame.\n",
					i, work_frame_num);
				for (j = 0; j < AI_PQ_TOP - 1; j++) {
					vframe->nn_value[j].maxclass =
						nn_info[i].nn_value[j].maxclass;
					vframe->nn_value[j].maxprob =
						nn_info[i].nn_value[j].maxprob;
				}
				ret = 0;
				break;
			}
		}
		check_flag = nn_info[i].nn_value[AI_PQ_TOP - 1].maxprob;
		if (work_frame_num != check_flag) {
			vdetect_print(0, PRINT_MN_SET_FLOW,
				"%d frame no nn param, use %d instead.\n",
				work_frame_num, check_flag);
		}
	}

	if (aipq_enable)
		vframe->ai_pq_enable = true;
	else
		vframe->ai_pq_enable = false;

	return ret;
}
EXPORT_SYMBOL(vdetect_get_frame_nn_info);

static int vdetect_alloc_map(int *inst)
{
	unsigned long flags;
	struct vdetect_dev *dev = NULL;
	struct list_head *p;

	flags = vdetect_devlist_lock();

	list_for_each(p, &vdetect_devlist) {
		dev = list_entry(p, struct vdetect_dev, vdetect_devlist);

		if (dev->inst >= 0 && !dev->mapped) {
			dev->mapped = true;
			*inst = dev->inst;
			vdetect_devlist_unlock(flags);
			return 0;
		}
	}

	vdetect_devlist_unlock(flags);
	return -ENODEV;
}

static void vdetect_release_map(int inst)
{
	unsigned long flags;
	struct vdetect_dev *dev = NULL;
	struct list_head *p;

	flags = vdetect_devlist_lock();

	list_for_each(p, &vdetect_devlist) {
		dev = list_entry(p, struct vdetect_dev, vdetect_devlist);
		if (dev->inst == inst && dev->mapped) {
			dev->mapped = false;
			pr_debug("%s %d OK\n", __func__, dev->inst);
			break;
		}
	}

	vdetect_devlist_unlock(flags);
}

static int vdetect_open(struct inode *inode, struct file *file)
{
	int *vdetect_id = kzalloc(sizeof(int), GFP_KERNEL);

	if (vdetect_id) {
		*vdetect_id = -1;
		file->private_data = vdetect_id;
	}
	return 0;
}

static int vdetect_release(struct inode *inode, struct file *file)
{
	int *vdetect_id = file->private_data;

	if (!IS_ERR_OR_NULL(vdetect_id) && (*vdetect_id) != -1)
		vdetect_release_map(*vdetect_id);

	if (vdetect_id)
		kfree(file->private_data);
	file->private_data = NULL;

	return 0;
}

static long vdetect_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case VDETECT_IOCTL_ALLOC_ID:{
			u32 vdetect_id = 0;
			int *a = (int *)file->private_data;

			ret = vdetect_alloc_map(&vdetect_id);

			if (a)
				*a = vdetect_id;

			if (ret != 0)
				break;

			put_user(vdetect_id, (u32 __user *)argp);
		}
		break;
	case VDETECT_IOCTL_FREE_ID:{
			u32 vdetect_id;
			int *a = (int *)file->private_data;

			get_user(vdetect_id, (u32 __user *)argp);

			if (a)
				*a = -1;

			vdetect_release_map(vdetect_id);
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long vdetect_compat_ioctl(struct file *file,
				 unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = vdetect_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static const struct file_operations vdetect_fops = {
	.owner = THIS_MODULE,
	.open = vdetect_open,
	.release = vdetect_release,
	.unlocked_ioctl = vdetect_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vdetect_compat_ioctl,
#endif
	.poll = NULL,
};

static ssize_t vdetect_debug_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%-18s=%d\n%-18s=%d\n%-18s=%d\n%-18s=%d\n",
			"detect_debug",
			detect_debug,
			"post_get_count",
			post_get_count,
			"post_put_count",
			post_put_count,
			"pre_put_count",
			pre_put_count);
}

static ssize_t vdetect_debug_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		detect_debug = val;
	else
		detect_debug = 0;
	pr_info("set detect_debug val:%d\n", detect_debug);
	return count;
}

static ssize_t vdetect_dump_show(struct class *class,
				 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "vdetect debug: %d\n", detect_dump);
}

static ssize_t vdetect_dump_store(struct class *class,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		detect_dump = val;
	else
		detect_dump = 0;
	pr_info("set detect_debug val:%d\n", detect_dump);
	return count;
}

static ssize_t vdetect_use_thread_show(struct class *class,
				       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "vdetect use thread: %d\n", detect_thread);
}

static ssize_t vdetect_use_thread_store(struct class *class,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		detect_thread = val;
	else
		detect_thread = 0;
	pr_info("set vdetect use thread val:%d\n", detect_thread);
	return count;
}

static ssize_t tv_add_vdetect_show(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", tv_add_vdetect);
}

static ssize_t tv_add_vdetect_store(struct class *class,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		tv_add_vdetect = val;
	else
		tv_add_vdetect = 0;
	pr_info("set tv_add_vdetect:%d\n", tv_add_vdetect);
	return count;
}

static ssize_t aipq_enable_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aipq_enable);
}

static ssize_t aipq_enable_store(struct class *class,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	if (val > 0)
		aipq_enable = 1;
	else
		aipq_enable = 0;

	return count;
}

static CLASS_ATTR_RW(vdetect_debug);
static CLASS_ATTR_RW(vdetect_dump);
static CLASS_ATTR_RW(vdetect_use_thread);
static CLASS_ATTR_RW(tv_add_vdetect);
static CLASS_ATTR_RW(aipq_enable);

static struct attribute *vdetect_class_attrs[] = {
	&class_attr_vdetect_debug.attr,
	&class_attr_vdetect_dump.attr,
	&class_attr_vdetect_use_thread.attr,
	&class_attr_tv_add_vdetect.attr,
	&class_attr_aipq_enable.attr,
	NULL
};

ATTRIBUTE_GROUPS(vdetect_class);

static struct class vdetect_class = {
	.name = "vdetect",
	.class_groups = vdetect_class_groups,
};

static int vdetect_create_instance(int inst)
{
	struct vdetect_dev *dev = NULL;
	struct video_device *vfd;
	int ret;
	unsigned long flags;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		vdetect_print(inst, PRINT_ERROR,
			      "%s vdetect.%d device alloc failed!\n",
			      __func__, inst);
		return -ENOMEM;
	}
	dev->res.magic = MAGIC_RE_MEM;
	dev->res.priv = NULL;
	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name),
		 "%s-%03d", VDETECT_MODULE_NAME, inst);
	ret = v4l2_device_register(NULL, &dev->v4l2_dev);
	if (ret) {
		vdetect_print(inst, PRINT_ERROR,
			      "v4l2 device register fail!\n");
		goto free_dev;
	}
	pr_info("vdetect.%d v4l2_dev.name=:%s\n", inst, dev->v4l2_dev.name);
	dev->fmt = &formats[0];
	dev->width = 640;
	dev->height = 480;
	dev->fd_num = 0;
	atomic_set(&dev->is_playing, 0);
	dev->inst = inst;
	atomic_set(&dev->vdect_status, VDETECT_INIT);
	/* init video dma queues */
	INIT_LIST_HEAD(&dev->dma_q.active);
	init_waitqueue_head(&dev->dma_q.wq);

	/* initialize locks */
	spin_lock_init(&dev->slock);
	mutex_init(&dev->vdetect_mutex);
	mutex_init(&dev->vf_mutex);

	vfd = video_device_alloc();
	if (!vfd)
		goto unreg_dev;
	*vfd = vdetect_template;
	vfd->dev_debug = detect_debug;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
			| V4L2_CAP_READWRITE;

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */
	ret = video_register_device(vfd, VFL_TYPE_GRABBER,
				    dev->inst + vdetect_base);
	if (ret < 0)
		goto unreg_dev;

	video_set_drvdata(vfd, dev);
	dev->vdev = vfd;
	dev->src_canvas[0] = -1;
	dev->src_canvas[1] = -1;
	dev->src_canvas[2] = -1;
	dev->dst_canvas = -1;
	dev->context = NULL;
	dev->last_vf = NULL;
	dev->ge2d_vf = NULL;
	snprintf(dev->recv_name, RECEIVER_NAME_SIZE,
		 RECEIVER_NAME ".%x", inst & 0xff);

	vf_receiver_init(&dev->recv, dev->recv_name, &vdetect_vf_recv, dev);
	vf_reg_receiver(&dev->recv);
	snprintf(dev->prov_name, RECEIVER_NAME_SIZE,
		 PROVIDER_NAME ".%x", inst & 0xff);
	vf_provider_init(&dev->prov, dev->prov_name, &vdetect_vf_prov, dev);
	pr_info("vdetect.%d v4l2 device registered as %s\n",
		inst, video_device_node_name(vfd));

	/* add to device list */
	flags = vdetect_devlist_lock();
	list_add_tail(&dev->vdetect_devlist, &vdetect_devlist);
	vdetect_devlist_unlock(flags);

	return 0;
unreg_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
free_dev:
	kfree(dev);
	return ret;
}

static int vdetect_v4l2_release(void)
{
	struct vdetect_dev *dev;
	struct list_head *list;
	unsigned long flags;
	int ret;

	flags = vdetect_devlist_lock();

	while (!list_empty(&vdetect_devlist)) {
		list = vdetect_devlist.next;
		list_del(list);
		vdetect_devlist_unlock(flags);

		dev = list_entry(list, struct vdetect_dev, vdetect_devlist);
		if (dev->context)
			destroy_ge2d_work_queue(dev->context);
		vf_unreg_receiver(&dev->recv);
		ret = free_canvas(dev);
		if (ret < 0)
			pr_info("vdetect free canvas failed!\n");
		pr_info("vdetect unregistering %s\n",
			video_device_node_name(dev->vdev));
		video_unregister_device(dev->vdev);
		video_device_release(dev->vdev);
		v4l2_device_unregister(&dev->v4l2_dev);
		kfree(dev);

		flags = vdetect_devlist_lock();
	}

	vdetect_devlist_unlock(flags);

	return 0;
}

static int vdetect_drv_probe(struct platform_device *pdev)
{
	int ret = -1, i;
	struct device *devp;

	pr_info("%s\n", __func__);
	ret = class_register(&vdetect_class);
	if (ret < 0)
		return ret;

	ret = register_chrdev(VDETECT_MAJOR, "vdetect", &vdetect_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for vdetect device\n");
		goto error1;
	}

	devp = device_create(&vdetect_class, NULL,
			     MKDEV(VDETECT_MAJOR, 0), NULL,
			     VDETECT_MODULE_NAME);
	if (IS_ERR(devp)) {
		pr_err("failed to create vdetect device node\n");
		ret = PTR_ERR(devp);
		return ret;
	}

	for (i = 0; i < MAX_INST; i++) {
		ret = vdetect_create_instance(i);
		if (ret) {
			/* If some instantiations succeeded, keep driver */
			if (i)
				ret = 0;
			break;
		}
	}

	if (ret < 0) {
		pr_err("vdetect: error %d while loading driver\n", ret);
		goto error1;
	}

	return 0;
error1:
	unregister_chrdev(VDETECT_MAJOR, "vdetect");
	class_unregister(&vdetect_class);
	return ret;
}

static int vdetect_drv_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	vdetect_v4l2_release();
	device_destroy(&vdetect_class, MKDEV(VDETECT_MAJOR, 0));
	unregister_chrdev(VDETECT_MAJOR, VDETECT_MODULE_NAME);
	class_unregister(&vdetect_class);
	return 0;
}

static const struct of_device_id vdetect_dt_match[] = {
	{
		.compatible = "amlogic, vdetect",
	},
	{}
};

/* general interface for a linux driver .*/
static struct platform_driver vdetect_drv = {
	.probe = vdetect_drv_probe,
	.remove = vdetect_drv_remove,
	.driver = {
			.name = "vdetect",
			.owner = THIS_MODULE,
			.of_match_table = vdetect_dt_match,
		}
};

int __init vdetect_init(void)
{
	pr_info("%s\n", __func__);
	if (platform_driver_register(&vdetect_drv)) {
		pr_err("Failed to register vdetect driver\n");
			return -ENODEV;
	}
	return 0;
}

void __exit vdetect_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&vdetect_drv);
}

