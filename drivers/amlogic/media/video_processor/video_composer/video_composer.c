// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/video_composer/video_composer.c
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

#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <uapi/linux/sched/types.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include <linux/amlogic/aml_sync_api.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#endif
#include <linux/dma-buf.h>
#include <linux/ion.h>
#include "video_composer.h"
#include <linux/amlogic/media/utils/am_com.h>
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_VIDEO_COMPOSER
#ifdef CONFIG_AMLOGIC_DEBUG_ATRACE
#include <trace/events/meson_atrace.h>
#endif
#define VIDEO_COMPOSER_VERSION "1.0"

#define VIDEO_COMPOSER_NAME_SIZE 32

#define VIDEO_COMPOSER_DEVICE_NAME   "video_composer-dev"

#define MAX_VIDEO_COMPOSER_INSTANCE_NUM ARRAY_SIZE(ports)

#define WAIT_THREAD_STOPPED_TIMEOUT 20

#define WAIT_READY_Q_TIMEOUT 100

static u32 video_composer_instance_num;
static unsigned int force_composer;
static unsigned int vc_active[2];
static unsigned int countinue_vsync_count[2];
static unsigned int force_composer_pip;
static unsigned int transform;
static unsigned int vidc_debug;
static unsigned int vidc_pattern_debug;
static u32 print_flag;
static u32 full_axis;
static u32 print_close;
static u32 receive_wait = 15;
static u32 margin_time = 2000;
static u32 max_width = 2560;
static u32  max_height = 1440;
static u32 rotate_width = 1280;
static u32  rotate_height = 720;
static u32  close_black;
static u32  axis_pip;
static u32  crop_pip;

#define PRINT_ERROR		0X0
#define PRINT_QUEUE_STATUS	0X0001
#define PRINT_FENCE		0X0002
#define PRINT_PERFORMANCE	0X0004
#define PRINT_AXIS		0X0008
#define PRINT_INDEX_DISP	0X0010
#define PRINT_PATTERN	        0X0020
#define PRINT_OTHER		0X0040

#define to_dst_buf(vf)	\
	container_of(vf, struct dst_buf_t, frame)

int vc_print(int index, int debug_flag, const char *fmt, ...)
{
	if (index + 1 == print_close)
		return 0;

	if ((print_flag & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "vc:[%d]", index);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static DEFINE_MUTEX(video_composer_mutex);

static struct video_composer_port_s ports[] = {
	{
		.name = "video_composer.0",
		.index = 0,
		.open_count = 0,
	},
	{
		.name = "video_composer.1",
		.index = 1,
		.open_count = 0,
	},
};

static struct timeval vsync_time;
static int get_count[MAX_VIDEO_COMPOSER_INSTANCE_NUM];

void vsync_notify_video_composer(void)
{
	int i;
	int count = MAX_VIDEO_COMPOSER_INSTANCE_NUM;

	countinue_vsync_count[0]++;
	countinue_vsync_count[1]++;
	if ((vidc_pattern_debug & 1) && vc_active[0])
		pr_info("vc: get_count[0]=%d\n", get_count[0]);
	else if ((vidc_pattern_debug & 2) && vc_active[1])
		pr_info("vc: get_count[1]=%d\n", get_count[1]);

	for (i = 0; i < count; i++)
		get_count[i] = 0;
	do_gettimeofday(&vsync_time);
}

static void *video_timeline_create(struct composer_dev *dev)
{
	const char *tl_name = "videocomposer_timeline_0";

	if (dev->index == 0)
		tl_name = "videocomposer_timeline_0";
	else if (dev->index == 1)
		tl_name = "videocomposer_timeline_1";

	if (IS_ERR_OR_NULL(dev->video_timeline)) {
		dev->cur_streamline_val = 0;
		dev->video_timeline = aml_sync_create_timeline(tl_name);
		vc_print(dev->index, PRINT_FENCE,
			 "timeline create tlName =%s, video_timeline=%p\n",
			 tl_name, dev->video_timeline);
	}

	return dev->video_timeline;
}

static int video_timeline_create_fence(struct composer_dev *dev)
{
	int out_fence_fd = -1;
	u32 pt_val = 0;

	pt_val = dev->cur_streamline_val + 1;
	out_fence_fd = aml_sync_create_fence(dev->video_timeline, pt_val);
	if (out_fence_fd >= 0) {
		dev->cur_streamline_val++;
		dev->fence_creat_count++;
	} else {
		vc_print(dev->index, PRINT_ERROR,
			 "create fence returned %d", out_fence_fd);
	}
	return out_fence_fd;
}

static void video_timeline_increase(struct composer_dev *dev,
				    unsigned int value)
{
	aml_sync_inc_timeline(dev->video_timeline, value);
	dev->fence_release_count += value;
	vc_print(dev->index, PRINT_FENCE,
		 "receive_cnt=%lld,fen_creat_cnt=%lld,fen_release_cnt=%lld\n",
		 dev->received_count,
		 dev->fence_creat_count,
		 dev->fence_release_count);
}

static int video_composer_init_buffer(struct composer_dev *dev)
{
	int i, ret = 0;
	u32 buf_width, buf_height;
	u32 buf_size;
	struct vinfo_s *video_composer_vinfo;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };
	int flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	switch (dev->buffer_status) {
	case UNINITIAL:/*not config*/
		break;
	case INIT_SUCCESS:/*config before , return ok*/
		return 0;
	case INIT_ERROR:/*config fail, won't retry , return failure*/
		return -1;
	default:
		return -1;
	}

	video_composer_vinfo = get_current_vinfo();

	if (IS_ERR_OR_NULL(video_composer_vinfo))
		video_composer_vinfo = &vinfo;
	dev->vinfo_w = video_composer_vinfo->width;
	dev->vinfo_h = video_composer_vinfo->height;
	buf_width = (video_composer_vinfo->width + 0x1f) & ~0x1f;
	buf_height = video_composer_vinfo->height;
	if (dev->need_rotate) {
		buf_width = rotate_width;
		buf_height = rotate_height;
	}
	if (buf_width > max_width)
		buf_width = max_width;
	if (buf_height > max_height)
		buf_height = max_height;
	buf_size = buf_width * buf_height * 3;
	buf_size = PAGE_ALIGN(buf_size);
	dev->composer_buf_w = buf_width;
	dev->composer_buf_h = buf_height;

	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr == 0)
			dev->dst_buf[i].phy_addr =
				codec_mm_alloc_for_dma("video_composer",
						       buf_size / PAGE_SIZE,
						       0, flags);
		vc_print(dev->index, PRINT_OTHER,
			 "video_composer: cma memory is %x , size is  %x\n",
			 (unsigned int)dev->dst_buf[i].phy_addr,
			 (unsigned int)buf_size);

		if (dev->dst_buf[i].phy_addr == 0) {
			dev->buffer_status = INIT_ERROR;
			vc_print(dev->index, PRINT_ERROR,
				 "cma memory config fail\n");
			return -1;
		}
		dev->dst_buf[i].index = i;
		dev->dst_buf[i].dirty = true;
		dev->dst_buf[i].buf_w = buf_width;
		dev->dst_buf[i].buf_h = buf_height;
		dev->dst_buf[i].buf_size = buf_size;
		if (!kfifo_put(&dev->free_q, &dev->dst_buf[i].frame))
			vc_print(dev->index, PRINT_ERROR,
				 "init buffer free_q is full\n");
	}

	if (IS_ERR_OR_NULL(dev->ge2d_para.context))
		ret = init_ge2d_composer(&dev->ge2d_para);

	if (ret < 0)
		vc_print(dev->index, PRINT_ERROR,
			 "create ge2d composer fail!\n");
	//vf_local_init(dev);
	dev->buffer_status = INIT_SUCCESS;

	return 0;
}

static void video_composer_uninit_buffer(struct composer_dev *dev)
{
	int i;
	int ret = 0;

	if (dev->buffer_status == UNINITIAL) {
		vc_print(dev->index, PRINT_OTHER,
			 "%s buffer have uninit already finished!\n", __func__);
		return;
	}
	dev->buffer_status = UNINITIAL;
	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr != 0) {
			pr_info("video_composer: cma free addr is %x\n",
				(unsigned int)dev->dst_buf[i].phy_addr);
			codec_mm_free_for_dma("video_composer",
					      dev->dst_buf[i].phy_addr);
			dev->dst_buf[i].phy_addr = 0;
		}
	}

	if (dev->ge2d_para.context)
		ret = uninit_ge2d_composer(&dev->ge2d_para);
	dev->ge2d_para.context = NULL;
	if (ret < 0)
		vc_print(dev->index, PRINT_ERROR,
			 "uninit ge2d composer failed!\n");
}

static void frames_put_file(struct composer_dev *dev,
			    struct received_frames_t *current_frames)
{
	struct file *file_vf;
	int current_count;
	int i;

	current_count = current_frames->frames_info.frame_count;
	for (i = 0; i < current_count; i++) {
		file_vf = current_frames->file_vf[i];
		fput(file_vf);
	}
	dev->fput_count++;
}

static void vf_pop_display_q(struct composer_dev *dev, struct vframe_s *vf)
{
	struct vframe_s *dis_vf = NULL;

	int k = kfifo_len(&dev->display_q);

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &dis_vf)) {
			if (dis_vf == vf)
				break;
			if (!kfifo_put(&dev->display_q, dis_vf))
				vc_print(dev->index, PRINT_ERROR,
					 "display_q is full!\n");
		}
		k--;
		if (k < 0) {
			vc_print(dev->index, PRINT_ERROR,
				 "can find vf in display_q\n");
			break;
		}
	}
}

static void display_q_uninit(struct composer_dev *dev)
{
	struct vframe_s *dis_vf = NULL;
	int repeat_count;
	int i;

	vc_print(dev->index, PRINT_QUEUE_STATUS, "vc: unit display_q len=%d\n",
		 kfifo_len(&dev->display_q));

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &dis_vf)) {
			if (dis_vf->flag
			    & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS) {
				repeat_count = dis_vf->repeat_count[dev->index];
				vc_print(dev->index, PRINT_FENCE,
					 "vc: unit repeat_count=%d, omx_index=%d\n",
					 repeat_count,
					 dis_vf->omx_index);
				for (i = 0; i <= repeat_count; i++) {
					fput(dis_vf->file_vf);
					dev->fput_count++;
				}
			} else if (!(dis_vf->flag
				     & VFRAME_FLAG_VIDEO_COMPOSER)) {
				vc_print(dev->index, PRINT_ERROR,
					 "vc: unit display_q flag is null, omx_index=%d\n",
					 dis_vf->omx_index);
			}
		}
	}
}

static void receive_q_uninit(struct composer_dev *dev)
{
	int i = 0;
	struct received_frames_t *received_frames = NULL;

	vc_print(dev->index, PRINT_QUEUE_STATUS, "vc: unit receive_q len=%d\n",
		 kfifo_len(&dev->receive_q));
	while (kfifo_len(&dev->receive_q) > 0) {
		if (kfifo_get(&dev->receive_q, &received_frames))
			frames_put_file(dev, received_frames);
	}

	for (i = 0; i < FRAMES_INFO_POOL_SIZE; i++) {
		atomic_set(&dev->received_frames[i].on_use,
			   false);
	}
}

static void ready_q_uninit(struct composer_dev *dev)
{
	struct vframe_s *dis_vf = NULL;
	int repeat_count;
	int i;

	vc_print(dev->index, PRINT_QUEUE_STATUS, "vc: unit ready_q len=%d\n",
		 kfifo_len(&dev->ready_q));

	while (kfifo_len(&dev->ready_q) > 0) {
		if (kfifo_get(&dev->ready_q, &dis_vf)) {
			if (dis_vf->flag
			    & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS) {
				repeat_count = dis_vf->repeat_count[dev->index];
				for (i = 0; i <= repeat_count; i++) {
					fput(dis_vf->file_vf);
					dev->fput_count++;
				}
			}
		}
	}
}

static void videocom_vf_put(struct vframe_s *vf, struct composer_dev *dev)
{
	struct dst_buf_t *dst_buf;

	if (IS_ERR_OR_NULL(vf)) {
		vc_print(dev->index, PRINT_ERROR, "vf is NULL\n");
		return;
	}

	dst_buf = to_dst_buf(vf);
	if (IS_ERR_OR_NULL(dst_buf)) {
		vc_print(dev->index, PRINT_ERROR, "dst_buf is NULL\n");
		return;
	}

	if (IS_ERR_OR_NULL(dev)) {
		vc_print(dev->index, PRINT_ERROR, "dev is NULL\n");
		return;
	}

	if (!kfifo_put(&dev->free_q, vf))
		vc_print(dev->index, PRINT_ERROR, "put free_q is full\n");
	vc_print(dev->index, PRINT_OTHER,
		 "%s free buffer count: %d %d\n",
		 __func__, kfifo_len(&dev->free_q), __LINE__);

	if (kfifo_is_full(&dev->free_q)) {
		dev->need_free_buffer = true;
		vc_print(dev->index, PRINT_ERROR,
			 "free_q is full, could uninit buffer!\n");
	}
	wake_up_interruptible(&dev->wq);
}

static unsigned long get_dma_phy_addr(int fd)
{
	unsigned long phy_addr = 0;
	struct dma_buf *dbuf = NULL;
	struct sg_table *table = NULL;
	struct page *page = NULL;
	struct ion_buffer *buffer = NULL;

	dbuf = dma_buf_get(fd);
	buffer = dbuf->priv;
	table = buffer->sg_table;
	page = sg_page(table->sgl);
	phy_addr = PFN_PHYS(page_to_pfn(page));
	dma_buf_put(dbuf);
	return phy_addr;
}

static struct vframe_s *get_dst_vframe_buffer(struct composer_dev *dev)
{
	struct vframe_s *dst_vf;

	if (!kfifo_get(&dev->free_q, &dst_vf)) {
		vc_print(dev->index, PRINT_QUEUE_STATUS, "free q is empty\n");
		return NULL;
	}
	return dst_vf;
}

static void check_window_change(struct composer_dev *dev,
				struct frames_info_t *cur_frame_info)
{
	int last_width, last_height, current_width, current_height;
	int cur_pos_x, cur_pos_y, cur_pos_w, cur_pos_h;
	int last_pos_x, last_pos_y, last_pos_w, last_pos_h;
	struct frames_info_t last_frame_info;
	int last_zorder, cur_zorder;
	bool window_changed = false;
	int i;

	last_frame_info = dev->last_frames.frames_info;
	if (cur_frame_info->frame_count != last_frame_info.frame_count) {
		window_changed = true;
		vc_print(dev->index, PRINT_ERROR,
			 "last count=%d, current count=%d\n",
			 last_frame_info.frame_count,
			 cur_frame_info->frame_count);
	} else {
		for (i = 0; i < cur_frame_info->frame_count; i++) {
			current_width = cur_frame_info->frame_info[i].crop_w;
			current_height = cur_frame_info->frame_info[i].crop_h;
			last_width = last_frame_info.frame_info[i].crop_w;
			last_height = last_frame_info.frame_info[i].crop_h;

			if (current_width * last_height !=
				current_height * last_width) {
				vc_print(dev->index, PRINT_ERROR,
					 "frame width or height changed!");
				window_changed = true;
				break;
			}

			cur_pos_x = cur_frame_info->frame_info[i].dst_x;
			cur_pos_y = cur_frame_info->frame_info[i].dst_y;
			cur_pos_w = cur_frame_info->frame_info[i].dst_w;
			cur_pos_h = cur_frame_info->frame_info[i].dst_h;
			last_pos_x = last_frame_info.frame_info[i].dst_x;
			last_pos_y = last_frame_info.frame_info[i].dst_y;
			last_pos_w = last_frame_info.frame_info[i].dst_w;
			last_pos_h = last_frame_info.frame_info[i].dst_h;

			if (cur_pos_x != last_pos_x ||
			    cur_pos_y != last_pos_y ||
			    cur_pos_w != last_pos_w ||
			    cur_pos_h != last_pos_h) {
				vc_print(dev->index, PRINT_OTHER,
					 "frame axis changed!");
				window_changed = true;
				break;
			}

			cur_zorder = cur_frame_info->frame_info[i].zorder;
			last_zorder = last_frame_info.frame_info[i].zorder;
			if (cur_zorder != last_zorder) {
				vc_print(dev->index, PRINT_OTHER,
					 "frame zorder changed!");
				window_changed = true;
				break;
			}
		}
	}

	if (!window_changed)
		return;

	for (i = 0; i < BUFFER_LEN; i++)
		dev->dst_buf[i].dirty = true;
}

static struct output_axis output_axis_adjust(struct composer_dev *dev,
					     struct frame_info_t *vframe_info,
					     struct ge2d_composer_para *para)
{
	int picture_width = 0, picture_height = 0;
	int render_w = 0, render_h = 0;
	int disp_w, disp_h;
	struct output_axis axis;
	int tmp;

	picture_width = vframe_info->crop_w;
	picture_height = vframe_info->crop_h;
	disp_w = para->position_width;
	disp_h = para->position_height;

	if (para->angle & 1) {
		tmp = picture_height;
		picture_height = picture_width;
		picture_width = tmp;
	}

	render_w = disp_w;
	render_h = disp_w * picture_height / picture_width;
	if (render_h > disp_h) {
		render_h = disp_h;
		render_w = disp_h * picture_width / picture_height;
	}

	axis.left = para->position_left + (disp_w - render_w) / 2;
	axis.top = para->position_top + (disp_h - render_h) / 2;
	axis.width = render_w;
	axis.height = render_h;
	vc_print(dev->index, PRINT_AXIS,
		 "position left top width height: %d %d %d %d\n",
		 para->position_left,
		 para->position_top,
		 para->position_width,
		 para->position_height);

	vc_print(dev->index, PRINT_AXIS,
		 "frame out data axis left top width height: %d %d %d %d\n",
		 axis.left, axis.top, axis.width, axis.height);
	return axis;
}

static void vframe_composer(struct composer_dev *dev)
{
	struct received_frames_t *received_frames = NULL;
	struct received_frames_t *received_frames_tmp = NULL;
	struct frames_info_t *frames_info = NULL;
	struct vframe_s *scr_vf = NULL;
	struct file *file_vf = NULL;
	int vf_dev[MXA_LAYER_COUNT];
	struct frame_info_t *vframe_info[MXA_LAYER_COUNT];
	int i, j, tmp;
	u32 zd1, zd2;
	struct file_private_data *file_private_data;
	struct config_para_ex_s ge2d_config;
	struct timeval begin_time;
	struct timeval end_time;
	int cost_time;
	int ret = 0;
	struct vframe_s *dst_vf = NULL;
	int count;
	struct dst_buf_t *dst_buf = NULL;
	u32 cur_transform = 0;
	struct src_data_para src_data;
	u32 drop_count = 0;
	struct output_axis dst_axis;
	int min_left = 0, min_top = 0;
	int max_right = 0, max_bottom = 0;

	do_gettimeofday(&begin_time);

	dev->ge2d_para.ge2d_config = &ge2d_config;
	ret = video_composer_init_buffer(dev);
	if (ret != 0) {
		vc_print(dev->index, PRINT_ERROR, "vc: init buffer failed!\n");
		video_composer_uninit_buffer(dev);
	} else {
		dst_vf = get_dst_vframe_buffer(dev);
	}
	if (IS_ERR_OR_NULL(dst_vf)) {
		vc_print(dev->index, PRINT_ERROR, "dst vf is NULL\n");
		return;
	}
	memset(dst_vf, 0, sizeof(struct vframe_s));
	while (1) {
		if (!kfifo_get(&dev->receive_q, &received_frames)) {
			vc_print(dev->index, PRINT_ERROR,
				 "com: get failed\n");
			return;
		}
		if (!kfifo_peek(&dev->receive_q, &received_frames_tmp))
			break;
		drop_count++;
		frames_put_file(dev, received_frames);
		vc_print(dev->index, PRINT_PERFORMANCE, "com: drop frame\n");
		atomic_set(&received_frames->on_use, false);
	}

	frames_info = &received_frames->frames_info;
	count = frames_info->frame_count;
	check_window_change(dev, &received_frames->frames_info);

	dst_buf = to_dst_buf(dst_vf);
	dev->ge2d_para.format = GE2D_FORMAT_S24_YUV444;
	dev->ge2d_para.phy_addr[0] = dst_buf->phy_addr;
	dev->ge2d_para.buffer_w = dst_buf->buf_w;
	dev->ge2d_para.buffer_h = dst_buf->buf_h;
	dev->ge2d_para.canvas0Addr = -1;
	dev->ge2d_para.plane_num = 1;

	if (dst_buf->dirty && !close_black) {
		ret = fill_vframe_black(&dev->ge2d_para);
		if (ret < 0) {
			vc_print(dev->index, PRINT_ERROR,
				 "ge2d fill black failed\n");
		}
		vc_print(dev->index, PRINT_OTHER, "fill black\n");
		dst_buf->dirty = false;
	}

	for (i = 0; i < count; i++) {
		vf_dev[i] = i;
		vframe_info[i] = &frames_info->frame_info[i];
	}

	for (i = 0; i < count - 1; i++) {
		for (j = 0; j < count - 1 - i; j++) {
			zd1 = vframe_info[vf_dev[j]]->zorder;
			zd2 = vframe_info[vf_dev[j + 1]]->zorder;
			if (zd1 > zd2) {
				tmp = vf_dev[j];
				vf_dev[j] = vf_dev[j + 1];
				vf_dev[j + 1] = tmp;
			}
		}
	}
	min_left = vframe_info[0]->dst_x;
	min_top = vframe_info[0]->dst_y;
	for (i = 0; i < count; i++) {
		if (vframe_info[vf_dev[i]]->type == 1) {
			src_data.canvas0Addr = -1;
			src_data.canvas1Addr = -1;
			src_data.canvas0_config[0].phy_addr =
				(u32)(received_frames->phy_addr[vf_dev[i]]);
			src_data.canvas0_config[0].width =
				(vframe_info[vf_dev[i]]->buffer_w
				 + 0x1f) & ~0x1f;
			src_data.canvas0_config[0].height =
				vframe_info[vf_dev[i]]->buffer_h;
			src_data.canvas0_config[0].block_mode =
				CANVAS_BLKMODE_LINEAR;
			src_data.canvas0_config[0].endian = 0;
			src_data.canvas0_config[1].phy_addr =
				(u32)(received_frames->phy_addr[vf_dev[i]]
				      + (src_data.canvas0_config[0].width)
				      * (src_data.canvas0_config[0].height));

			src_data.canvas0_config[1].width =
				(vframe_info[vf_dev[i]]->buffer_w
				 + 0x1f) & ~0x1f;
			src_data.canvas0_config[1].height =
				(vframe_info[vf_dev[i]]->buffer_h) / 2;
			src_data.canvas0_config[1].block_mode =
				CANVAS_BLKMODE_LINEAR;
			src_data.canvas0_config[1].endian = 0;

			src_data.bitdepth = BITDEPTH_Y8
					    | BITDEPTH_U8
					    | BITDEPTH_V8;
			src_data.source_type = 0;
			src_data.type = VIDTYPE_PROGRESSIVE
					| VIDTYPE_VIU_FIELD
					| VIDTYPE_VIU_NV21;
			src_data.plane_num = 2;
			src_data.width = vframe_info[vf_dev[i]]->buffer_w;
			src_data.height = vframe_info[vf_dev[i]]->buffer_h;
			src_data.is_vframe = false;
		} else {
			file_vf = received_frames->file_vf[vf_dev[i]];
			file_private_data =
			(struct file_private_data *)(file_vf->private_data);
			scr_vf = &file_private_data->vf;

			src_data.canvas0Addr = scr_vf->canvas0Addr;
			src_data.canvas1Addr = scr_vf->canvas1Addr;
			src_data.canvas0_config[0] = scr_vf->canvas0_config[0];
			src_data.canvas0_config[1] = scr_vf->canvas0_config[1];
			src_data.canvas0_config[2] = scr_vf->canvas0_config[2];
			src_data.canvas1_config[0] = scr_vf->canvas1_config[0];
			src_data.canvas1_config[1] = scr_vf->canvas1_config[1];
			src_data.canvas1_config[2] = scr_vf->canvas1_config[2];
			src_data.bitdepth = scr_vf->bitdepth;
			src_data.source_type = scr_vf->source_type;
			src_data.type = scr_vf->type;
			src_data.plane_num = scr_vf->plane_num;
			src_data.width = scr_vf->width;
			src_data.height = scr_vf->height;
			if (scr_vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
				src_data.is_vframe = false;
			else
				src_data.is_vframe = true;
		}
		cur_transform = vframe_info[vf_dev[i]]->transform;
		dev->ge2d_para.position_left =
			vframe_info[vf_dev[i]]->dst_x;
		dev->ge2d_para.position_top =
			vframe_info[vf_dev[i]]->dst_y;
		dev->ge2d_para.position_width =
			vframe_info[vf_dev[i]]->dst_w;
		dev->ge2d_para.position_height =
			vframe_info[vf_dev[i]]->dst_h;

		if (cur_transform == VC_TRANSFORM_ROT_90)
			dev->ge2d_para.angle = 1;
		else if (cur_transform == VC_TRANSFORM_ROT_180)
			dev->ge2d_para.angle = 2;
		else if (cur_transform == VC_TRANSFORM_ROT_270)
			dev->ge2d_para.angle = 3;
		else if (cur_transform != 0)
			vc_print(dev->index, PRINT_ERROR,
				 "not support transform=%d\n", cur_transform);
		if (!full_axis) {
			dst_axis =
				output_axis_adjust(dev, vframe_info[vf_dev[i]],
						   &dev->ge2d_para);
			dev->ge2d_para.position_left =
				dst_axis.left * dst_buf->buf_w / dev->vinfo_w;
			dev->ge2d_para.position_top =
				dst_axis.top * dst_buf->buf_h / dev->vinfo_h;
			dev->ge2d_para.position_width =
				dst_axis.width * dst_buf->buf_w / dev->vinfo_w;
			dev->ge2d_para.position_height = dst_axis.height
				* dst_buf->buf_h / dev->vinfo_h;

			if (min_left > dst_axis.left)
				min_left = dst_axis.left;
			if (min_top > dst_axis.top)
				min_top = dst_axis.top;
			if (max_right < (dst_axis.left + dst_axis.width))
				max_right = dst_axis.left + dst_axis.width;
			if (max_bottom < (dst_axis.top + dst_axis.height))
				max_bottom = dst_axis.top + dst_axis.height;
		}
		dev->ge2d_para.plane_num = 1;

		ret = ge2d_data_composer(&src_data, &dev->ge2d_para);
		if (ret < 0)
			vc_print(dev->index, PRINT_ERROR,
				 "ge2d composer failed\n");
	}

	frames_put_file(dev, received_frames);

	do_gettimeofday(&end_time);
	cost_time = (1000000 * (end_time.tv_sec - begin_time.tv_sec)
		+ (end_time.tv_usec - begin_time.tv_usec)) / 1000;
	vc_print(dev->index, PRINT_PERFORMANCE,
		 "ge2d cost: %d ms\n",
		 cost_time);

	dst_vf->flag |=
		VFRAME_FLAG_VIDEO_COMPOSER
		| VFRAME_FLAG_COMPOSER_DONE;

	dst_vf->bitdepth =
		BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;

	dst_vf->type =
		VIDTYPE_VIU_444
		| VIDTYPE_VIU_SINGLE_PLANE
		| VIDTYPE_VIU_FIELD;
	if (axis_pip) {
		dst_vf->axis[0] = 0;
		dst_vf->axis[1] = 0;
		dst_vf->axis[2] = 0;
		dst_vf->axis[3] = 0;
	} else {
		dst_vf->axis[0] = min_left;
		dst_vf->axis[1] = min_top;
		dst_vf->axis[2] = max_right;
		dst_vf->axis[3] = max_bottom;
	}
	if (crop_pip) {
		dst_vf->crop[0] = 0;
		dst_vf->crop[1] = 0;
		dst_vf->crop[2] = 0;
		dst_vf->crop[3] = 0;
	} else {
		dst_vf->crop[0] = min_top * dst_buf->buf_h / dev->vinfo_h;
		dst_vf->crop[1] = min_left * dst_buf->buf_w / dev->vinfo_w;
		dst_vf->crop[2] = dst_buf->buf_h -
			max_bottom * dst_buf->buf_h / dev->vinfo_h;
		dst_vf->crop[3] = dst_buf->buf_w -
			max_right * dst_buf->buf_w / dev->vinfo_w;
	}
	vc_print(dev->index, PRINT_AXIS,
		 "min_top,min_left,max_bottom,max_right: %d %d %d %d\n",
		 min_top, min_left, max_bottom, max_right);

	dst_vf->zorder = frames_info->disp_zorder;
	dst_vf->canvas0_config[0].phy_addr = dst_buf->phy_addr;
	dst_vf->canvas0Addr = -1;
	dst_vf->canvas1Addr = -1;
	dst_vf->plane_num = 1;
	dst_vf->width = dst_buf->buf_w;
	dst_vf->height = dst_buf->buf_h;
	dst_vf->canvas0_config[0].width = dst_vf->width * 3;
	dst_vf->canvas0_config[0].height = dst_vf->height;
	dst_vf->canvas0_config[0].block_mode = 0;
	dst_vf->repeat_count[dev->index] = 0;

	if (dev->last_dst_vf)
		dev->last_dst_vf->repeat_count[dev->index] += drop_count;
	else
		dst_vf->repeat_count[dev->index] += drop_count;
	dev->last_dst_vf = dst_vf;
	dev->last_frames = *received_frames;
	dev->fake_vf = *dev->last_dst_vf;

	if (!kfifo_put(&dev->ready_q, (const struct vframe_s *)dst_vf))
		vc_print(dev->index, PRINT_ERROR, "ready_q is full\n");

	vc_print(dev->index, PRINT_PERFORMANCE,
		 "ready len=%d\n", kfifo_len(&dev->ready_q));

	atomic_set(&received_frames->on_use, false);
}

static void empty_ready_queue(struct composer_dev *dev)
{
	int repeat_count;
	int omx_index;
	bool is_composer;
	int i;
	struct file *file_vf;
	bool is_dma_buf;
	struct vframe_s *vf = NULL;

	vc_print(dev->index, PRINT_OTHER, "vc: empty ready_q len=%d\n",
		 kfifo_len(&dev->ready_q));

	while (kfifo_len(&dev->ready_q) > 0) {
		if (kfifo_get(&dev->ready_q, &vf)) {
			if (!vf)
				break;
			repeat_count = vf->repeat_count[dev->index];
			omx_index = vf->omx_index;
			is_composer = vf->flag & VFRAME_FLAG_COMPOSER_DONE;
			file_vf = vf->file_vf;
			is_dma_buf = vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_DMA;
			vc_print(dev->index, PRINT_OTHER,
				 "empty: repeat_count =%d, omx_index=%d\n",
				 repeat_count, omx_index);
			video_timeline_increase(dev, repeat_count + 1);
			if (!is_composer) {
				for (i = 0; i <= repeat_count; i++) {
					fput(file_vf);
					dev->fput_count++;
				}
			} else {
				videocom_vf_put(vf, dev);
			}
			if (is_dma_buf) {
				if (!kfifo_put(&dev->dma_free_q, vf))
					vc_print(dev->index, PRINT_ERROR,
						 "dma_free is full!\n");
			}
		}
	}
}

static void video_composer_task(struct composer_dev *dev)
{
	struct vframe_s *vf = NULL;
	struct file *file_vf = NULL;
	struct frame_info_t *frame_info = NULL;
	struct file_private_data *file_private_data;
	struct received_frames_t *received_frames = NULL;
	struct frames_info_t *frames_info = NULL;
	int count;
	bool need_composer = false;
	int ready_count = 0;
	unsigned long phy_addr;
	u64 time_us64;

	if (!kfifo_peek(&dev->receive_q, &received_frames)) {
		vc_print(dev->index, PRINT_ERROR, "task: peek failed\n");
		return;
	}

	if (IS_ERR_OR_NULL(received_frames)) {
		vc_print(dev->index, PRINT_ERROR,
			 "task: get received_frames is NULL\n");
		return;
	}

	count = received_frames->frames_info.frame_count;
	time_us64 = received_frames->time_us64;

	if (count == 1) {
		if (transform)
			received_frames->frames_info.frame_info[0].transform =
				transform;
		if ((dev->index == 0 && force_composer) ||
		    (dev->index == 1 && force_composer_pip))
			need_composer = true;
		if (received_frames->frames_info.frame_info[0].transform) {
			need_composer = true;
			dev->need_rotate = true;
		}
	} else {
		need_composer = true;
	}
	if (!need_composer) {
		frames_info = &received_frames->frames_info;
		frame_info = frames_info->frame_info;
		phy_addr = received_frames->phy_addr[0];
		vc_print(dev->index, PRINT_OTHER,
			 "task:frame_cnt=%d,z=%d,index=%d,receive_q len=%d\n",
			 frames_info->frame_count,
			 frames_info->disp_zorder,
			 dev->index,
			 kfifo_len(&dev->receive_q));
		file_vf = received_frames->file_vf[0];
		if (frame_info->type == 0) {
			file_private_data =
			(struct file_private_data *)(file_vf->private_data);
			vf = &file_private_data->vf;
		} else if (frame_info->type == 1) {
			if (!kfifo_get(&dev->dma_free_q, &vf)) {
				vc_print(dev->index, PRINT_ERROR,
					 "task: get dma_free_q failed\n");
				return;
			}
			memset(vf, 0, sizeof(struct vframe_s));
		}
		if (!kfifo_get(&dev->receive_q, &received_frames)) {
			vc_print(dev->index, PRINT_ERROR,
				 "task: get failed\n");
			return;
		}
		if (!vf) {
			vc_print(dev->index, PRINT_ERROR,
				 "vf is NULL\n");
			return;
		}
		vf->axis[0] = frame_info->dst_x;
		vf->axis[1] = frame_info->dst_y;
		vf->axis[2] = frame_info->dst_w + frame_info->dst_x - 1;
		vf->axis[3] = frame_info->dst_h + frame_info->dst_y - 1;
		vf->crop[0] = 0;
		vf->crop[1] = 0;
		vf->crop[2] = 0;
		vf->crop[3] = 0;
		vf->zorder = frames_info->disp_zorder;
		vf->file_vf = file_vf;
		//vf->zorder = 1;
		vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
			| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
		vf->pts_us64 = time_us64;
		vf->disp_pts = 0;

		if (frame_info->type == 1) {
			vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER_DMA;
			vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
			vf->canvas0Addr = -1;
			vf->canvas0_config[0].phy_addr = phy_addr;
			vf->canvas0_config[0].width = (frame_info->buffer_w
						       + 0x1f) & ~0x1f;
			vf->canvas0_config[0].height = frame_info->buffer_h;
			vf->canvas1Addr = -1;
			vf->canvas0_config[1].phy_addr = phy_addr
				+ vf->canvas0_config[0].width
				* vf->canvas0_config[0].height;
			vf->canvas0_config[1].width = (frame_info->buffer_w
						       + 0x1f) & ~0x1f;
			vf->canvas0_config[1].height = frame_info->buffer_h;
			vf->width = frame_info->buffer_w;
			vf->height = frame_info->buffer_h;
			vf->plane_num = 2;
			vf->type = VIDTYPE_PROGRESSIVE
					| VIDTYPE_VIU_FIELD
					| VIDTYPE_VIU_NV21;
			vf->bitdepth =
				BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
			vf->crop[0] = frame_info->crop_y;
			vf->crop[1] = frame_info->crop_x;
			vf->crop[2] = frame_info->buffer_h
				- frame_info->crop_h
				- frame_info->crop_y;
			vf->crop[3] = frame_info->buffer_w
				- frame_info->crop_w
				- frame_info->crop_x;
		}
		vc_print(dev->index, PRINT_AXIS,
			 "axis: %d %d %d %d\ncrop: %d %d %d %d\n",
			 vf->axis[0], vf->axis[1], vf->axis[2], vf->axis[3],
			 vf->crop[0], vf->crop[1], vf->crop[2], vf->crop[3]);
		vc_print(dev->index, PRINT_AXIS,
			 "vf_width: %d, vf_height: %d\n",
			 vf->width, vf->height);
		vc_print(dev->index, PRINT_AXIS,
			 "=========frame info:==========\n");
		vc_print(dev->index, PRINT_AXIS,
			 "frame aixs x,y,w,h: %d %d %d %d\n",
			 frame_info->dst_x, frame_info->dst_y,
			 frame_info->dst_w, frame_info->dst_h);
		vc_print(dev->index, PRINT_AXIS,
			 "frame crop t,l,b,r: %d %d %d %d\n",
			 frame_info->crop_y, frame_info->crop_x,
			 frame_info->crop_h, frame_info->crop_w);
		vc_print(dev->index, PRINT_AXIS,
			 "frame buffer Width X Height: %d X %d\n",
			 frame_info->buffer_w, frame_info->buffer_h);
		vc_print(dev->index, PRINT_AXIS,
			 "===============================\n");
		if (dev->last_file == file_vf && frame_info->type == 0) {
			vf->repeat_count[dev->index]++;
			vc_print(dev->index, PRINT_FENCE,
				 "repeat =%d, omx_index=%d\n",
				 vf->repeat_count[dev->index],
				 vf->omx_index);
		} else {
			dev->last_file = file_vf;
			vf->repeat_count[dev->index] = 0;
			if (!kfifo_put(&dev->ready_q,
				       (const struct vframe_s *)vf))
				vc_print(dev->index, PRINT_ERROR,
					 "by_pass ready_q is full\n");
			ready_count = kfifo_len(&dev->ready_q);
			if (ready_count > 2)
				vc_print(dev->index, PRINT_ERROR,
					 "ready len=%d\n", ready_count);
			else if (ready_count > 1)
				vc_print(dev->index, PRINT_PATTERN,
					 "ready len=%d\n", ready_count);
			vc_print(dev->index, PRINT_QUEUE_STATUS,
				 "ready len=%d\n", kfifo_len(&dev->ready_q));
		}
		dev->fake_vf = *vf;
		atomic_set(&received_frames->on_use, false);
	} else {
		vframe_composer(dev);
		dev->last_file = NULL;
	}
}

static void video_composer_wait_event(struct composer_dev *dev)
{
	wait_event_interruptible_timeout(dev->wq,
					 (kfifo_len(&dev->receive_q) > 0 &&
					  dev->enable_composer) ||
					 dev->need_free_buffer ||
					 dev->need_unint_receive_q ||
					 dev->need_empty_ready,
					 msecs_to_jiffies(5000));
}

static int video_composer_thread(void *data)
{
	struct composer_dev *dev = data;

	vc_print(dev->index, PRINT_OTHER, "thread: started\n");
	init_waitqueue_head(&dev->wq);

	dev->thread_stopped = 0;
	while (1) {
		if (kthread_should_stop())
			break;

		if (kfifo_len(&dev->receive_q) == 0)
			video_composer_wait_event(dev);

		if (dev->need_empty_ready) {
			vc_print(dev->index, PRINT_OTHER,
				 "empty_ready_queue\n");
			dev->need_empty_ready = false;
			empty_ready_queue(dev);
			dev->fake_vf.flag |= VFRAME_FLAG_FAKE_FRAME;
			if (!kfifo_put(&dev->ready_q,
				       (const struct vframe_s *)&dev->fake_vf))
				vc_print(dev->index, PRINT_ERROR,
					 "by_pass ready_q is full\n");
		}

		if (dev->need_free_buffer) {
			dev->need_free_buffer = false;
			video_composer_uninit_buffer(dev);
			vc_print(dev->index, PRINT_OTHER,
				 "%s video composer release!\n", __func__);
			continue;
		}
		if (kthread_should_stop())
			break;

		if (!dev->enable_composer && dev->need_unint_receive_q) {
			receive_q_uninit(dev);
			dev->need_unint_receive_q = false;
			ready_q_uninit(dev);
			complete(&dev->task_done);
			continue;
		}
		if (kfifo_len(&dev->receive_q) > 0 && dev->enable_composer)
			video_composer_task(dev);
	}
	dev->thread_stopped = 1;
	vc_print(dev->index, PRINT_OTHER, "thread: exit\n");
	return 0;
}

static int video_composer_open(struct inode *inode, struct file *file)
{
	struct composer_dev *dev;
	struct video_composer_port_s *port = &ports[iminor(inode)];
	int i;
	struct sched_param param = {.sched_priority = 2};

	pr_err("%s iminor(inode) =%d\n", __func__, iminor(inode));
	if (iminor(inode) >= video_composer_instance_num)
		return -ENODEV;

	mutex_lock(&video_composer_mutex);

	if (port->open_count > 0) {
		mutex_unlock(&video_composer_mutex);
		pr_err("video_composer: instance %d is aleady opened",
		       port->index);
		return -EBUSY;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		mutex_unlock(&video_composer_mutex);
		pr_err("video_composer: instance %d alloc dev failed",
		       port->index);
		return -ENOMEM;
	}
	dev->ge2d_para.context = NULL;

	dev->ge2d_para.count = 0;
	dev->ge2d_para.canvas_dst[0] = -1;
	dev->ge2d_para.canvas_dst[1] = -1;
	dev->ge2d_para.canvas_dst[2] = -1;
	dev->ge2d_para.canvas_scr[0] = -1;
	dev->ge2d_para.canvas_scr[1] = -1;
	dev->ge2d_para.canvas_scr[2] = -1;
	dev->ge2d_para.plane_num = 1;

	dev->buffer_status = UNINITIAL;

	dev->port = port;
	file->private_data = dev;
	dev->index = port->index;
	dev->need_free_buffer = false;
	dev->last_frames.frames_info.frame_count = 0;
	dev->is_sideband = false;
	dev->need_empty_ready = false;

	memcpy(dev->vf_provider_name, port->name,
	       strlen(port->name) + 1);

	port->open_count++;
	do_gettimeofday(&dev->start_time);

	mutex_unlock(&video_composer_mutex);
	dev->kthread = kthread_create(video_composer_thread,
				      dev, dev->port->name);
	if (IS_ERR(dev->kthread)) {
		pr_err("video_composer_thread creat failed\n");
		return -ENOMEM;
	}

	if (sched_setscheduler(dev->kthread, SCHED_FIFO, &param))
		pr_err("vc:Could not set realtime priority.\n");

	wake_up_process(dev->kthread);
	//mutex_init(&dev->mutex_input);

	for (i = 0; i < FRAMES_INFO_POOL_SIZE; i++)
		dev->received_frames[i].index = i;

	video_timeline_create(dev);

	return 0;
}

static int video_composer_release(struct inode *inode, struct file *file)
{
	struct composer_dev *dev = file->private_data;
	struct video_composer_port_s *port = dev->port;
	int i = 0;

	if (iminor(inode) >= video_composer_instance_num)
		return -ENODEV;

	if (dev->kthread) {
		kthread_stop(dev->kthread);
		wake_up_interruptible(&dev->wq);
		dev->kthread = NULL;
	}

	mutex_lock(&video_composer_mutex);

	port->open_count--;

	mutex_unlock(&video_composer_mutex);
	while (1) {
		i++;
		if (dev->thread_stopped)
			break;
		usleep_range(9000, 10000);
		if (i > WAIT_THREAD_STOPPED_TIMEOUT) {
			pr_err("wait thread timeout\n");
			break;
		}
	}
	kfree(dev);
	return 0;
}

static void disable_video_layer(struct composer_dev *dev, int val)
{
	pr_info("dev->index =%d, val=%d", dev->index, val);
	if (dev->index == 0)
		_video_set_disable(val);
	else if (dev->index == 1)
		_videopip_set_disable(val);
}

static void set_frames_info(struct composer_dev *dev,
			    struct frames_info_t *frames_info)
{
	u32 fence_fd;
	int i = 0;
	int j = 0;
	struct file *file_vf = NULL;
	struct vframe_s *vf = NULL;
	struct file_private_data *file_private_data;
	struct timeval time1;
	struct timeval time2;
	u64 time_us64;
	u64 time_vsync;
	int axis[4];
	int crop[4];
	int ready_len = 0;
	bool current_is_sideband = false;

	for (j = 0; j < frames_info->frame_count; j++) {
		if (frames_info->frame_info[j].type == 2) {
			vc_print(dev->index, PRINT_OTHER,
				 "sideband:i=%d,z=%d\n",
				 i,
				 frames_info->disp_zorder);
			ready_len = kfifo_len(&dev->ready_q);
			vc_print(dev->index, PRINT_OTHER,
				 "sideband: ready_len =%d\n",
				 ready_len);
			frames_info->frame_info[j].composer_fen_fd = -1;
			axis[0] = frames_info->frame_info[j].dst_x;
			axis[1] = frames_info->frame_info[j].dst_y;
			axis[2] = frames_info->frame_info[j].dst_w
				+ axis[0] - 1;
			axis[3] = frames_info->frame_info[j].dst_h
				+ axis[1] - 1;
			crop[0] = frames_info->frame_info[j].crop_x;
			crop[1] = frames_info->frame_info[j].crop_y;
			crop[2] = frames_info->frame_info[j].crop_w;
			crop[3] = frames_info->frame_info[j].crop_h;
			set_video_window_ext(dev->index, axis);
			set_video_crop_ext(dev->index, crop);
			set_video_zorder_ext(dev->index,
					     frames_info->disp_zorder);
			if (!dev->is_sideband && dev->received_count > 0) {
				vc_print(dev->index, PRINT_OTHER,
					 "non change to sideband:wake_up\n");
				dev->need_empty_ready = true;
				wake_up_interruptible(&dev->wq);
			}
			if (!dev->is_sideband) {
				set_blackout_policy(0);
				dev->select_path_done = false;
			}
			dev->is_sideband = true;
			current_is_sideband = true;
		}
	}
	if (!dev->select_path_done) {
		if (current_is_sideband) {
			if (dev->received_count > 0) {
				if (dev->index == 0)
					set_video_path_select("amvideo", 0);
				else if (dev->index == 1)
					set_video_path_select("pipvideo", 1);
			} else {
				if (dev->index == 0)
					set_video_path_select("auto", 0);
				else if (dev->index == 1)
					set_video_path_select("pipvideo", 1);
			}
		}
		dev->select_path_done = true;
	}
	if (current_is_sideband) {
		if (frames_info->frame_count > 1)
			vc_print(dev->index, PRINT_ERROR,
				 "sideband count not 1\n");
		return;
	}

	if ((dev->is_sideband && !current_is_sideband) ||
	    dev->received_count == 0) {
		if (dev->is_sideband && !current_is_sideband) {
			set_blackout_policy(1);
			vc_print(dev->index, PRINT_OTHER,
				 "sideband to non\n");
		}
		dev->is_sideband = false;
		if (dev->index == 0)
			set_video_path_select("video_render.0", 0);
		else if (dev->index == 1)
			set_video_path_select("video_render.1", 1);
	}
	dev->is_sideband = false;

	time1 = dev->start_time;
	do_gettimeofday(&time2);
	time_us64 = (u64)1000000 * (time2.tv_sec - time1.tv_sec)
			+ time2.tv_usec - time1.tv_usec;

	time_vsync = (u64)1000000 * (time2.tv_sec - vsync_time.tv_sec)
					+ time2.tv_usec - vsync_time.tv_usec;

	if (frames_info->frame_count > MXA_LAYER_COUNT ||
	    frames_info->frame_count < 1) {
		vc_print(dev->index, PRINT_ERROR,
			 "vc: layer count %d\n", frames_info->frame_count);
		return;
	}

	while (1) {
		j = 0;
		for (i = 0; i < FRAMES_INFO_POOL_SIZE; i++) {
			if (!atomic_read(&dev->received_frames[i].on_use))
				break;
		}
		if (i == FRAMES_INFO_POOL_SIZE) {
			j++;
			if (j > WAIT_READY_Q_TIMEOUT) {
				pr_err("receive_q is full, wait timeout!\n");
				return;
			}
			usleep_range(1000 * receive_wait,
				     1000 * (receive_wait + 1));
			pr_err("receive_q is full!!! need wait =%d\n", j);
			continue;
		} else {
			break;
		}
	}

	fence_fd = video_timeline_create_fence(dev);
	dev->received_frames[i].frames_info = *frames_info;
	dev->received_frames[i].frames_num = dev->received_count;
	dev->received_frames[i].time_us64 = time_us64;

	vc_print(dev->index, PRINT_PERFORMANCE,
		 "len =%d,frame_count=%d,i=%d,z=%d,time_us64=%lld,fd=%d, time_vsync=%lld\n",
		 kfifo_len(&dev->receive_q),
		 frames_info->frame_count,
		 i,
		 frames_info->disp_zorder,
		 time_us64,
		 fence_fd,
		 time_vsync);

	for (j = 0; j < frames_info->frame_count; j++) {
		frames_info->frame_info[j].composer_fen_fd = fence_fd;
		file_vf = fget(frames_info->frame_info[j].fd);
		if (!file_vf) {
			vc_print(dev->index, PRINT_ERROR, "fget fd fail\n");
			return;
		}
		dev->received_frames[i].file_vf[j] = file_vf;
		if (frames_info->frame_info[j].type == 0) {
			file_private_data =
			(struct file_private_data *)(file_vf->private_data);
			vf = &file_private_data->vf;
			vc_print(dev->index, PRINT_FENCE | PRINT_PATTERN,
				 "received_cnt=%lld,i=%d,z=%d,omx_index=%d, fence_fd=%d, fc_no=%d, index_disp=%d,pts=%lld\n",
				 dev->received_count + 1,
				 i,
				 frames_info->frame_info[j].zorder,
				 vf->omx_index,
				 fence_fd,
				 dev->cur_streamline_val,
				 vf->index_disp,
				 vf->pts_us64);
#ifdef CONFIG_AMLOGIC_DEBUG_ATRACE
			ATRACE_COUNTER("video_composer", vf->index_disp);
#endif
		} else if (frames_info->frame_info[j].type == 1) {
			vc_print(dev->index, PRINT_FENCE,
				 "received_cnt=%lld,i=%d,z=%d,DMA_fd=%d\n",
				 dev->received_count + 1,
				 i,
				 frames_info->frame_info[j].zorder,
				 frames_info->frame_info[j].fd);
			dev->received_frames[i].phy_addr[j] =
			get_dma_phy_addr(frames_info->frame_info[j].fd);
		} else {
			vc_print(dev->index, PRINT_ERROR,
				 "unsupport type=%d\n",
				 frames_info->frame_info[j].type);
		}
	}
	atomic_set(&dev->received_frames[i].on_use, true);

	dev->received_count++;
	if (!kfifo_put(&dev->receive_q, &dev->received_frames[i]))
		vc_print(dev->index, PRINT_ERROR, "put ready fail\n");
	wake_up_interruptible(&dev->wq);

	//vc_print(dev->index, PRINT_PERFORMANCE, "set_frames_info_out\n");
}

/* -----------------------------------------------------------------
 *           provider opeations
 * -----------------------------------------------------------------
 */
static struct vframe_s *vc_vf_peek(void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vframe_s *vf = NULL;
	struct timeval time1;
	struct timeval time2;
	u64 time_vsync;
	int interval_time;

	time1 = dev->start_time;
	time2 = vsync_time;
	if (kfifo_peek(&dev->ready_q, &vf)) {
		if (vf && get_count[dev->index] > 0) {
			time_vsync = (u64)1000000
				* (time2.tv_sec - time1.tv_sec)
				+ time2.tv_usec - time1.tv_usec;
			interval_time = abs(time_vsync - vf->pts_us64);
			vc_print(dev->index, PRINT_PERFORMANCE,
				 "time_vsync=%lld, vf->pts_us64=%lld\n",
				 time_vsync, vf->pts_us64);
			if (interval_time < margin_time) {
				vc_print(dev->index, PRINT_PATTERN,
					 "display next vsync\n");
				return NULL;
			}
		}
		return vf;
	} else {
		return NULL;
	}
}

static struct vframe_s *vc_vf_get(void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vframe_s *vf = NULL;

	if (kfifo_get(&dev->ready_q, &vf)) {
		if (!vf)
			return NULL;

		if (vf->flag & VFRAME_FLAG_FAKE_FRAME)
			return vf;

		if (!kfifo_put(&dev->display_q, vf))
			vc_print(dev->index, PRINT_ERROR,
				 "display_q is full!\n");
		if (!(vf->flag
		      & VFRAME_FLAG_VIDEO_COMPOSER)) {
			pr_err("vc: vf_get: flag is null\n");
		}

		get_count[dev->index]++;

		vc_print(dev->index, PRINT_OTHER | PRINT_PATTERN,
			 "get: omx_index=%d, get_count=%d, vsync_count=%d\n",
			 vf->omx_index,
			 get_count[dev->index],
			 countinue_vsync_count[dev->index]);
		countinue_vsync_count[dev->index] = 0;

		return vf;
	} else {
		return NULL;
	}
}

static void vc_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	int repeat_count;
	int omx_index;
	bool rendered;
	bool is_composer;
	int i;
	struct file *file_vf;
	bool is_dma_buf;

	if (!vf)
		return;

	repeat_count = vf->repeat_count[dev->index];
	omx_index = vf->omx_index;
	rendered = vf->rendered;
	is_composer = vf->flag & VFRAME_FLAG_COMPOSER_DONE;
	file_vf = vf->file_vf;
	is_dma_buf = vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_DMA;

	if (vf->flag & VFRAME_FLAG_FAKE_FRAME) {
		vc_print(dev->index, PRINT_OTHER,
			 "put: fake frame\n");
		return;
	}

	vc_print(dev->index, PRINT_FENCE,
		 "put: repeat_count =%d, omx_index=%d\n",
		 repeat_count, omx_index);

	vf_pop_display_q(dev, vf);

	if (rendered) {
		video_timeline_increase(dev, repeat_count
					+ 1 + dev->drop_frame_count);
		dev->drop_frame_count = 0;
	} else {
		dev->drop_frame_count += repeat_count + 1;
		vc_print(dev->index, PRINT_PERFORMANCE | PRINT_FENCE,
			 "put: drop repeat_count=%d\n", repeat_count);
	}

	if (!is_composer) {
		for (i = 0; i <= repeat_count; i++) {
			if (file_vf)
				fput(file_vf);
			dev->fput_count++;
		}
	} else {
		videocom_vf_put(vf, dev);
	}
	if (is_dma_buf) {
		if (!kfifo_put(&dev->dma_free_q, vf))
			vc_print(dev->index, PRINT_ERROR,
				 "dma_free is full!\n");
	}
}

static int vc_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_PUT)
		;
	else if (type & VFRAME_EVENT_RECEIVER_GET)
		;
	else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT)
		;
	return 0;
}

static int vc_vf_states(struct vframe_states *states, void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;

	states->vf_pool_size = COMPOSER_READY_POOL_SIZE;
	states->buf_recycle_num = 0;
	states->buf_free_num = COMPOSER_READY_POOL_SIZE
		- kfifo_len(&dev->ready_q);
	states->buf_avail_num = kfifo_len(&dev->ready_q);
	return 0;
}

static const struct vframe_operations_s vc_vf_provider = {
	.peek = vc_vf_peek,
	.get = vc_vf_get,
	.put = vc_vf_put,
	.event_cb = vc_event_cb,
	.vf_states = vc_vf_states,
};

static int video_composer_creat_path(struct composer_dev *dev)
{
	if (dev->index == 0)
		snprintf(dev->vfm_map_chain, VCOM_MAP_NAME_SIZE,
			 "%s %s", dev->vf_provider_name,
			 "video_render.0");
	else if (dev->index == 1)
		snprintf(dev->vfm_map_chain, VCOM_MAP_NAME_SIZE,
			 "%s %s", dev->vf_provider_name,
			 "video_render.1");

	snprintf(dev->vfm_map_id, VCOM_MAP_NAME_SIZE,
		 "vcom-map-%d", dev->index);

	if (vfm_map_add(dev->vfm_map_id,
			dev->vfm_map_chain) < 0) {
		pr_err("vcom pipeline map creation failed %s.\n",
		       dev->vfm_map_id);
		dev->vfm_map_id[0] = 0;
		return -ENOMEM;
	}

	vf_provider_init(&dev->vc_vf_prov, dev->vf_provider_name,
			 &vc_vf_provider, dev);

	vf_reg_provider(&dev->vc_vf_prov);

	vf_notify_receiver(dev->vf_provider_name,
			   VFRAME_EVENT_PROVIDER_START, NULL);
	return 0;
}

static int video_composer_release_path(struct composer_dev *dev)
{
	vf_unreg_provider(&dev->vc_vf_prov);

	if (dev->vfm_map_id[0]) {
		vfm_map_remove(dev->vfm_map_id);
		dev->vfm_map_id[0] = 0;
	}
	return 0;
}

static int video_composer_init(struct composer_dev *dev)
{
	int ret;
	int i;

	if (!dev)
		return -1;

	INIT_KFIFO(dev->ready_q);
	INIT_KFIFO(dev->receive_q);
	INIT_KFIFO(dev->free_q);
	INIT_KFIFO(dev->display_q);
	INIT_KFIFO(dev->dma_free_q);
	kfifo_reset(&dev->ready_q);
	kfifo_reset(&dev->receive_q);
	kfifo_reset(&dev->free_q);
	kfifo_reset(&dev->display_q);
	kfifo_reset(&dev->dma_free_q);

	for (i = 0; i < DMA_BUF_COUNT; i++)
		kfifo_put(&dev->dma_free_q, &dev->dma_vf[i]);

	dev->received_count = 0;
	dev->fence_creat_count = 0;
	dev->fence_release_count = 0;
	dev->fput_count = 0;
	dev->last_dst_vf = NULL;
	dev->drop_frame_count = 0;
	dev->is_sideband = false;
	dev->need_empty_ready = false;
	dev->last_file = NULL;
	dev->select_path_done = false;
	init_completion(&dev->task_done);

	disable_video_layer(dev, 2);
	video_set_global_output(dev->index, 1);

	ret = video_composer_creat_path(dev);
	if (dev->index == 0)
		set_video_path_select("video_render.0", 0);
	else if (dev->index == 1)
		set_video_path_select("video_render.1", 1);

	vc_active[dev->index] = 1;

	return ret;
}

static int video_composer_uninit(struct composer_dev *dev)
{
	int ret;
	int time_left = 0;

	if (dev->is_sideband) {
		if (dev->index == 0)
			set_video_path_select("auto", 0);
		set_blackout_policy(1);
	}

	disable_video_layer(dev, 1);
	video_set_global_output(dev->index, 0);
	ret = video_composer_release_path(dev);

	dev->need_unint_receive_q = true;

	/* free buffer */
	dev->need_free_buffer = true;
	wake_up_interruptible(&dev->wq);

	time_left = wait_for_completion_timeout(&dev->task_done,
						msecs_to_jiffies(500));
	if (!time_left)
		vc_print(dev->index, PRINT_ERROR, "unreg:wait timeout\n");
	else if (time_left < 100)
		vc_print(dev->index, PRINT_ERROR,
			 "unreg:wait time %d\n", time_left);

	display_q_uninit(dev);

	if (dev->fence_creat_count != dev->fput_count) {
		vc_print(dev->index, PRINT_ERROR,
			 "uninit: fence_r=%lld, fence_c=%lld\n",
			 dev->fence_release_count,
			 dev->fence_creat_count);
		vc_print(dev->index, PRINT_ERROR,
			 "uninit: received=%lld, fput=%lld, drop=%d\n",
			 dev->received_count,
			 dev->fput_count,
			 dev->drop_frame_count);
	}
	video_timeline_increase(dev,
				dev->fence_creat_count
				- dev->fence_release_count);
	dev->is_sideband = false;
	dev->need_empty_ready = false;

	vc_active[dev->index] = 0;

	return ret;
}

int video_composer_set_enable(struct composer_dev *dev, u32 val)
{
	int ret = 0;

	if (val > VIDEO_COMPOSER_ENABLE_NORMAL)
		return -EINVAL;

	vc_print(dev->index, PRINT_ERROR,
		 "vc: set enable index=%d, val=%d\n",
		 dev->index, val);

	if (dev->enable_composer == val) {
		pr_err("vc: set_enable repeat set dev->index =%d,val=%d\n",
		       dev->index, val);
		return ret;
	}
	dev->enable_composer = val;
	wake_up_interruptible(&dev->wq);

	if (val == VIDEO_COMPOSER_ENABLE_NORMAL)
		ret = video_composer_init(dev);
	else if (val == VIDEO_COMPOSER_ENABLE_NONE)
		ret = video_composer_uninit(dev);

	if (ret != 0)
		pr_err("vc: set failed\n");
	return ret;
}

static long video_composer_ioctl(struct file *file,
				 unsigned int cmd, ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	u32 val;
	struct composer_dev *dev = (struct composer_dev *)file->private_data;
	struct frames_info_t frames_info;

	switch (cmd) {
	case VIDEO_COMPOSER_IOCTL_SET_FRAMES:
		if (copy_from_user(&frames_info, argp,
				   sizeof(frames_info)) == 0) {
			set_frames_info(dev, &frames_info);
			ret = copy_to_user(argp, &frames_info,
					   sizeof(struct frames_info_t));

		} else {
			ret = -EFAULT;
		}
		break;
	case VIDEO_COMPOSER_IOCTL_SET_ENABLE:
		if (copy_from_user(&val, argp, sizeof(u32)) == 0)
			ret = video_composer_set_enable(dev, val);
		else
			ret = -EFAULT;
		break;
	case VIDEO_COMPOSER_IOCTL_SET_DISABLE:
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long video_composer_compat_ioctl(struct file *file, unsigned int cmd,
					ulong arg)
{
	long ret = 0;

	ret = video_composer_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static const struct file_operations video_composer_fops = {
	.owner = THIS_MODULE,
	.open = video_composer_open,
	.release = video_composer_release,
	.unlocked_ioctl = video_composer_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = video_composer_compat_ioctl,
#endif
	.poll = NULL,
};

static ssize_t debug_crop_pip_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	return snprintf(buf, 80,
			"current debug_crop_pip is %d\n",
			crop_pip);
}

static ssize_t debug_crop_pip_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	crop_pip = tmp;
	return count;
}

static ssize_t debug_axis_pip_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	return snprintf(buf, 80,
			"current debug_axis_pip is %d\n",
			axis_pip);
}

static ssize_t debug_axis_pip_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	axis_pip = tmp;
	return count;
}

static ssize_t force_composer_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	return snprintf(buf, 80,
			"current debug_force_composer is %d\n",
			force_composer);
}

static ssize_t force_composer_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	force_composer = tmp;
	return count;
}

static ssize_t force_composer_pip_show(struct class *cla,
				       struct class_attribute *attr,
				       char *buf)
{
	return snprintf(buf, 80,
			"current debug_force_composer_pip is %d\n",
			force_composer_pip);
}

static ssize_t force_composer_pip_store(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	force_composer_pip = tmp;
	return count;
}

static ssize_t transform_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80,
			"current transform is %d\n",
			transform);
}

static ssize_t transform_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	transform = tmp;
	return count;
}

static ssize_t vidc_debug_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current vidc_debug is %d\n",
			vidc_debug);
}

static ssize_t vidc_debug_store(struct class *cla,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vidc_debug = tmp;
	return count;
}

static ssize_t vidc_pattern_debug_show(struct class *cla,
				       struct class_attribute *attr,
				       char *buf)
{
	return snprintf(buf, 80,
			"current vidc_pattern_debug is %d\n",
			vidc_pattern_debug);
}

static ssize_t vidc_pattern_debug_store(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vidc_pattern_debug = tmp;
	return count;
}

static ssize_t print_flag_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current print_flag is %d\n",
			print_flag);
}

static ssize_t print_flag_store(struct class *cla,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	print_flag = tmp;
	return count;
}

static ssize_t full_axis_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80,
			"current full_axis is %d\n",
			full_axis);
}

static ssize_t full_axis_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	full_axis = tmp;
	return count;
}

static ssize_t print_close_show(struct class *cla,
				struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current print_close is %d\n",
			print_close);
}

static ssize_t print_close_store(struct class *cla,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	print_close = tmp;
	return count;
}

static ssize_t receive_wait_show(struct class *cla,
				 struct class_attribute *attr,
				 char *buf)
{
	return snprintf(buf, 80,
			"current receive_wait is %d\n",
			receive_wait);
}

static ssize_t receive_wait_store(struct class *cla,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	receive_wait = tmp;
	return count;
}

static ssize_t margin_time_show(struct class *cla,
				struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current margin_time is %d\n",
			margin_time);
}

static ssize_t margin_time_store(struct class *cla,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	margin_time = tmp;
	return count;
}

static ssize_t max_width_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80,
			"current max_width is %d\n",
			max_width);
}

static ssize_t max_width_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	max_width = tmp;
	return count;
}

static ssize_t max_height_show(struct class *cla,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current max_height is %d\n",
			max_height);
}

static ssize_t max_height_store(struct class *cla,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	max_height = tmp;
	return count;
}

static ssize_t rotate_width_show(struct class *cla,
				 struct class_attribute *attr,
				 char *buf)
{
	return snprintf(buf, 80,
			"current rotate_width is %d\n",
			rotate_width);
}

static ssize_t rotate_width_store(struct class *cla,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	rotate_width = tmp;
	return count;
}

static ssize_t rotate_height_show(struct class *cla,
				  struct class_attribute *attr,
				  char *buf)
{
	return snprintf(buf, 80,
			"current rotate_height is %d\n",
			rotate_height);
}

static ssize_t rotate_height_store(struct class *cla,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	rotate_height = tmp;
	return count;
}

static ssize_t close_black_show(struct class *cla,
				struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current close_black is %d\n",
			close_black);
}

static ssize_t close_black_store(struct class *cla,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	close_black = tmp;
	return count;
}

static CLASS_ATTR_RW(debug_crop_pip);
static CLASS_ATTR_RW(debug_axis_pip);
static CLASS_ATTR_RW(force_composer);
static CLASS_ATTR_RW(force_composer_pip);
static CLASS_ATTR_RW(transform);
static CLASS_ATTR_RW(vidc_debug);
static CLASS_ATTR_RW(vidc_pattern_debug);
static CLASS_ATTR_RW(print_flag);
static CLASS_ATTR_RW(full_axis);
static CLASS_ATTR_RW(print_close);
static CLASS_ATTR_RW(receive_wait);
static CLASS_ATTR_RW(margin_time);
static CLASS_ATTR_RW(max_width);
static CLASS_ATTR_RW(max_height);
static CLASS_ATTR_RW(rotate_width);
static CLASS_ATTR_RW(rotate_height);
static CLASS_ATTR_RW(close_black);

static struct attribute *video_composer_class_attrs[] = {
	&class_attr_debug_crop_pip.attr,
	&class_attr_debug_axis_pip.attr,
	&class_attr_force_composer.attr,
	&class_attr_force_composer_pip.attr,
	&class_attr_transform.attr,
	&class_attr_vidc_debug.attr,
	&class_attr_vidc_pattern_debug.attr,
	&class_attr_print_flag.attr,
	&class_attr_full_axis.attr,
	&class_attr_print_close.attr,
	&class_attr_receive_wait.attr,
	&class_attr_margin_time.attr,
	&class_attr_max_width.attr,
	&class_attr_max_height.attr,
	&class_attr_rotate_width.attr,
	&class_attr_rotate_height.attr,
	&class_attr_close_black.attr,
	NULL
};

ATTRIBUTE_GROUPS(video_composer_class);

static struct class video_composer_class = {
	.name = "video_composer",
	.class_groups = video_composer_class_groups,
};

static const struct of_device_id amlogic_video_composer_dt_match[] = {
	{.compatible = "amlogic, video_composer",
	},
	{},
};

static int video_composer_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	u32 layer_cap = 0;
	struct video_composer_port_s *st;

	layer_cap = video_get_layer_capability();
	video_composer_instance_num = 0;
	if (layer_cap & LAYER0_SCALER)
		video_composer_instance_num++;
	if (layer_cap & LAYER1_SCALER)
		video_composer_instance_num++;
	ret = class_register(&video_composer_class);
	if (ret < 0)
		return ret;
	ret = register_chrdev(VIDEO_COMPOSER_MAJOR,
			      "video_composer", &video_composer_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for video_composer device\n");
		goto error1;
	}

	for (st = &ports[0], i = 0;
	     i < video_composer_instance_num; i++, st++) {
		pr_err("%s:ports[i].name=%s, i=%d\n", __func__,
		       ports[i].name, i);
		st->class_dev = device_create(&video_composer_class, NULL,
					      MKDEV(VIDEO_COMPOSER_MAJOR, i),
					      NULL, ports[i].name);
	}
	pr_err("%s num=%d\n", __func__, video_composer_instance_num);
	return ret;

error1:
	pr_err("%s error\n", __func__);
	unregister_chrdev(VIDEO_COMPOSER_MAJOR, "video_composer");
	class_unregister(&video_composer_class);
	return ret;
}

static int video_composer_remove(struct platform_device *pdev)
{
	int i;
	struct video_composer_port_s *st;

	for (st = &ports[0], i = 0;
	     i < video_composer_instance_num; i++, st++)
		device_destroy(&video_composer_class,
			       MKDEV(VIDEO_COMPOSER_MAJOR, i));

	unregister_chrdev(VIDEO_COMPOSER_MAJOR, VIDEO_COMPOSER_DEVICE_NAME);
	class_destroy(&video_composer_class);
	return 0;
};

static struct platform_driver video_composer_driver = {
	.probe = video_composer_probe,
	.remove = video_composer_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "video_composer",
		.of_match_table = amlogic_video_composer_dt_match,
	}
};

int __init video_composer_module_init(void)
{
	pr_err("video_composer_module_init_1\n");

	if (platform_driver_register(&video_composer_driver)) {
		pr_err("failed to register video_composer module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit video_composer_module_exit(void)
{
	platform_driver_unregister(&video_composer_driver);
}

//MODULE_DESCRIPTION("Video Technology Magazine video composer Capture Boad");
//MODULE_AUTHOR("Amlogic, Jintao Xu<jintao.xu@amlogic.com>");
//MODULE_LICENSE("GPL");
//MODULE_VERSION(VIDEO_COMPOSER_VERSION);

#ifndef MODULE
module_init(video_composer_module_init);
module_exit(video_composer_module_exit);
#endif
