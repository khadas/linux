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
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/sched/clock.h>
#include <linux/sync_file.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/vfm/amlogic_fbc_hook_v1.h>
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
#include <linux/amlogic/media/di/di_interface.h>
#endif

#include "videodisplay.h"
#define VIDEO_COMPOSER_VERSION "1.0"

#define VIDEO_COMPOSER_NAME_SIZE 32

#define VIDEO_COMPOSER_DEVICE_NAME   "video_composer-dev"

#define WAIT_THREAD_STOPPED_TIMEOUT 20

#define WAIT_READY_Q_TIMEOUT 100

static u32 use_low_latency;
MODULE_PARM_DESC(use_low_latency, "\n use_low_latency\n");
module_param(use_low_latency, uint, 0664);

static u32 video_composer_instance_num;
static unsigned int force_composer;
static unsigned int force_composer_pip;
static unsigned int transform;
static unsigned int vidc_debug;
static unsigned int vidc_pattern_debug;
static int last_index[MAX_VD_LAYERS][MXA_LAYER_COUNT];
static int last_omx_index;
static u32 print_flag;
static u32 full_axis = 1;
static u32 print_close;
static u32 receive_wait = 15;
static u32 margin_time = 2000;
static u32 max_width = 2560;
static u32 max_height = 1440;
static u32 pic_mode_max_width = 3840;
static u32 pic_mode_max_height = 2160;
static u32 rotate_width = 1280;
static u32 rotate_height = 720;
static u32 dewarp_rotate_width = 3840;
static u32 dewarp_rotate_height = 2160;
static u32 vicp_max_width = 3840;
static u32 vicp_max_height = 2160;
static u32 close_black;
static u32 debug_axis_pip;
static u32 debug_crop_pip;
static u32 composer_use_444;
static u32 reset_drop;
static u32 drop_cnt;
static u32 drop_cnt_pip;
static u32 receive_count;
static u32 receive_count_pip;
static u32 receive_new_count;
static u32 receive_new_count_pip;
static u32 total_get_count;
static u32 total_put_count;
static u64 nn_need_time = 15000;
static u64 nn_margin_time = 9000;
static u32 nn_bypass;
static u32 tv_fence_creat_count;
static u32 dump_vframe;
static u32 vicp_output_dev = 2; /*1 mif, 2 fbc, 3 fbc+mif*/
static u32 vicp_shrink_mode = 1; /*0 2x, 1 4x, 2 8x*/
static u32 force_comp_w;
static u32 force_comp_h;

u32 vd_pulldown_level = 2;
u32 vd_max_hold_count = 300;
u32 vd_set_frame_delay[MAX_VIDEO_COMPOSER_INSTANCE_NUM];
u32 vd_dump_vframe;
u32 vpp_drop_count;
static u32 composer_dev_choice; /*1 ge2d, 2 dewarp, 3 vicp*/
struct vframe_s *current_display_vf;
u32 vd_test_fps;
u32 vd_test_fps_pip;
u64 vd_test_fps_val[MAX_VD_LAYERS];
u64 vd_test_vsync_val[MAX_VD_LAYERS];
u32 dewarp_load_flag; /*0 dynamic load, 1 load bin file*/
#define to_dst_buf(vf)	\
	container_of(vf, struct dst_buf_t, frame)

static void vd_dump_afbc_vf(u8 *data_y, u8 *data_uv, struct vframe_s *vf, int flag)
{
	struct file *fp = NULL;
	char name_buf[32];
	int data_size_y, data_size_uv;
	mm_segment_t fs;
	loff_t pos;

	if (!vf)
		return;

	/*use flag to distinguish src and dst vframe*/
	if (flag == 0)
		snprintf(name_buf, sizeof(name_buf), "/sdcard/src_afbc_vframe.yuv");
	else
		snprintf(name_buf, sizeof(name_buf), "/sdcard/dst_afbc_vframe.yuv");

	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;

	data_size_y = vf->compWidth * vf->compHeight;
	data_size_uv = vf->compWidth * vf->compHeight / 2;
	pr_info("dump: data_size_y =%d, data_size_uv=%d\n", data_size_y, data_size_uv);

	if (!data_y || !data_uv) {
		pr_err("%s: vmap failed.\n", __func__);
		return;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;
	vfs_write(fp, data_y, data_size_y, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_y, data_y);
	pos = fp->f_pos;
	vfs_write(fp, data_uv, data_size_uv, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_uv, data_uv);
	set_fs(fs);
	filp_close(fp, NULL);
}

static void vd_dump_vf(struct vframe_s *vf)
{
	struct file *fp = NULL;
	char name_buf[32];
	int data_size_y, data_size_uv;
	u8 *data_y;
	u8 *data_uv;
	mm_segment_t fs;
	loff_t pos;

	if (!vf)
		return;

	snprintf(name_buf, sizeof(name_buf), "/sdcard/dst_vframe.yuv");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	data_size_y = vf->canvas0_config[0].width *
			vf->canvas0_config[0].height;
	data_size_uv = vf->canvas0_config[1].width *
			vf->canvas0_config[1].height;
	data_y = codec_mm_vmap(vf->canvas0_config[0].phy_addr, data_size_y);
	data_uv = codec_mm_vmap(vf->canvas0_config[1].phy_addr, data_size_uv);
	if (!data_y || !data_uv) {
		pr_err("%s: vmap failed.\n", __func__);
		return;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;
	vfs_write(fp, data_y, data_size_y, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_y, data_y);
	codec_mm_unmap_phyaddr(data_y);
	pos = fp->f_pos;
	vfs_write(fp, data_uv, data_size_uv, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_uv, data_uv);
	codec_mm_unmap_phyaddr(data_uv);
	set_fs(fs);
	filp_close(fp, NULL);
}

int vd_vframe_afbc_soft_decode(struct vframe_s *vf, int flag)
{
	int i, j, ret, y_size, free_cnt;
	short *planes[4];
	short *y_src, *u_src, *v_src, *s2c, *s2c1;
	u8 *tmp, *tmp1;
	u8 *y_dst, *vu_dst;
	int bit_10;
	struct timeval start, end;
	unsigned long time_use = 0;

	if ((vf->bitdepth & BITDEPTH_YMASK)  == BITDEPTH_Y10)
		bit_10 = 1;
	else
		bit_10 = 0;

	u32 p_data_size = vf->compWidth * vf->compHeight  * 3 / 2;
	u8 *p = vmalloc(p_data_size);
	if (!p) {
		pr_err("p_data_size vmalloc fail in %s\n", __func__);
		return -1;
	}

	y_size = vf->compWidth * vf->compHeight * sizeof(short);
	pr_info("width: %d, height: %d, compWidth: %u, compHeight: %u.\n",
		 vf->width, vf->height, vf->compWidth, vf->compHeight);
	for (i = 0; i < 4; i++) {
		planes[i] = vmalloc(y_size);
		if (!planes[i]) {
			free_cnt = i;
			pr_err("vmalloc fail in %s\n", __func__);
			vfree(p);
			goto free;
		}
		pr_info("plane %d size: %d, vmalloc addr: %p.\n",
			i, y_size, planes[i]);
	}
	free_cnt = 4;

	do_gettimeofday(&start);
	ret = AMLOGIC_FBC_vframe_decoder_v1((void **)planes, vf, 0, 0);
	if (ret < 0) {
		pr_err("amlogic_fbc_lib.ko error %d", ret);
		vfree(p);
		goto free;
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	pr_debug("FBC Decompress time: %ldms\n", time_use);

	y_src = planes[0];
	u_src = planes[1];
	v_src = planes[2];

	y_dst = p;
	vu_dst = p + vf->compWidth * vf->compHeight;

	do_gettimeofday(&start);
	for (i = 0; i < vf->compHeight; i++) {
		for (j = 0; j < vf->compWidth; j++) {
			s2c = y_src + j;
			tmp = (u8 *)(s2c);
			if (bit_10)
				*(y_dst + j) = *s2c >> 2;
			else
				*(y_dst + j) = tmp[0];
		}

			y_dst += vf->compWidth;
			y_src += vf->compWidth;
	}

	for (i = 0; i < (vf->compHeight / 2); i++) {
		for (j = 0; j < vf->compWidth; j += 2) {
			s2c = v_src + j / 2;
			s2c1 = u_src + j / 2;
			tmp = (u8 *)(s2c);
			tmp1 = (u8 *)(s2c1);

			if (bit_10) {
				*(vu_dst + j) = *s2c >> 2;
				*(vu_dst + j + 1) = *s2c1 >> 2;
			} else {
				*(vu_dst + j) = tmp[0];
				*(vu_dst + j + 1) = tmp1[0];
			}
		}
		vu_dst += vf->compWidth;
		u_src += (vf->compWidth / 2);
		v_src += (vf->compWidth / 2);
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	pr_debug("bitblk time: %ldms\n", time_use);

	y_dst = p;
	vu_dst = p + vf->compWidth * vf->compHeight;
	vd_dump_afbc_vf(y_dst, vu_dst, vf, flag);
	vfree(p);
	for (i = 0; i < free_cnt; i++)
		vfree(planes[i]);
	return 0;

free:
	for (i = 0; i < free_cnt; i++)
		vfree(planes[i]);
	return -1;
}

void ext_controls(void)
{
	if (current_display_vf->type & VIDTYPE_COMPRESS) {
		vd_vframe_afbc_soft_decode(current_display_vf, 1);
	} else {
		vd_dump_vf(current_display_vf);
	}
}

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

struct video_composer_port_s ports[] = {
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
	{
		.name = "video_composer.2",
		.index = 2,
		.open_count = 0,
	},
};

struct video_composer_port_s *video_composer_get_port(u32 index)
{
	int i = 0;

	if (index >= video_composer_instance_num) {
		vc_print(index, PRINT_ERROR,
			"%s: invalid index.\n",
			__func__);
		return NULL;
	}

	for (i = 0; i < video_composer_instance_num; i++) {
		if (index == ports[i].index)
			break;
	}

	if (i == video_composer_instance_num) {
		vc_print(index, PRINT_ERROR,
			"%s: don't find port[%d].\n",
			__func__, index);
		return NULL;
	} else {
		return &ports[i];
	}
}

static void *video_timeline_create(struct composer_dev *dev)
{
	const char *tl_name = "videocomposer_timeline_0";

	if (dev->index == 0)
		tl_name = "videocomposer_timeline_0";
	else if (dev->index == 1)
		tl_name = "videocomposer_timeline_1";
	else if (dev->index == 2)
		tl_name = "videocomposer_timeline_2";

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
	vc_print(dev->index, PRINT_FENCE, "pt_val %d", pt_val);

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
		"receive_cnt=%lld,new_cnt=%lld,fen_creat_cnt=%lld,fen_release_cnt=%lld\n",
		dev->received_count,
		dev->received_new_count,
		dev->fence_creat_count,
		dev->fence_release_count);
}

static int vc_init_ge2d_buffer(struct composer_dev *dev, bool is_tvp, size_t usage)
{
	int i, flags;
	u32 buf_width, buf_height, buf_size;
	struct vinfo_s *video_composer_vinfo;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };
	u64 output_duration;

	video_composer_vinfo = get_current_vinfo();
	if (IS_ERR_OR_NULL(video_composer_vinfo))
		video_composer_vinfo = &vinfo;

	dev->vinfo_w = video_composer_vinfo->width;
	dev->vinfo_h = video_composer_vinfo->height;
	buf_width = (video_composer_vinfo->width + 0x1f) & ~0x1f;
	buf_height = video_composer_vinfo->height;

	output_duration = div64_u64(video_composer_vinfo->sync_duration_num,
		video_composer_vinfo->sync_duration_den);
	vc_print(dev->index, PRINT_OTHER,
		"%s: output_duration is %lld.\n", __func__, output_duration);
	if (output_duration > 60) {
		buf_width = 1920;
		buf_height = 1080;
	}

	vc_print(dev->index, PRINT_OTHER, "%s: usage: %lld\n", __func__, usage);
	if (usage == UVM_USAGE_IMAGE_PLAY) {
		if (buf_width > pic_mode_max_width)
			buf_width = pic_mode_max_width;
		if (buf_height > pic_mode_max_height)
			buf_height = pic_mode_max_height;
	} else {
		if (dev->need_rotate) {
			buf_width = rotate_width;
			buf_height = rotate_height;
		}

		if (buf_width > max_width)
			buf_width = max_width;
		if (buf_height > max_height)
			buf_height = max_height;
	}

	if (composer_use_444)
		buf_size = buf_width * buf_height * 3;
	else
		buf_size = buf_width * buf_height * 3 / 2;

	buf_size = PAGE_ALIGN(buf_size);
	dev->composer_buf_w = buf_width;
	dev->composer_buf_h = buf_height;
	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr == 0)
			dev->dst_buf[i].phy_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
				buf_size / PAGE_SIZE, 0, flags);
		vc_print(dev->index, PRINT_ERROR,
			 "%s: cma memory is %x , size is  %x\n",
			 ports[dev->index].name,
			 (unsigned int)dev->dst_buf[i].phy_addr,
			 (unsigned int)buf_size);

		if (dev->dst_buf[i].phy_addr == 0) {
			dev->buffer_status = INIT_ERROR;
			vc_print(dev->index, PRINT_ERROR, "cma memory config fail\n");
			return -1;
		}
		dev->dst_buf[i].index = i;
		dev->dst_buf[i].dirty = true;
		dev->dst_buf[i].buf_w = buf_width;
		dev->dst_buf[i].buf_h = buf_height;
		dev->dst_buf[i].buf_size = buf_size;
		dev->dst_buf[i].is_tvp = is_tvp;

		if (!kfifo_put(&dev->free_q, &dev->dst_buf[i].frame))
			vc_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
	}

	return 0;
}

static int vc_init_dewarp_buffer(struct composer_dev *dev, bool is_tvp, size_t usage)
{
	int i, flags;
	u32 buf_width, buf_height, buf_size;
	struct vinfo_s *video_composer_vinfo;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };
	u64 output_duration;

	video_composer_vinfo = get_current_vinfo();
	if (IS_ERR_OR_NULL(video_composer_vinfo))
		video_composer_vinfo = &vinfo;
	dev->vinfo_w = video_composer_vinfo->width;
	dev->vinfo_h = video_composer_vinfo->height;
	buf_width = (video_composer_vinfo->width + 0x1f) & ~0x1f;
	buf_height = video_composer_vinfo->height;

	output_duration = div64_u64(video_composer_vinfo->sync_duration_num,
		video_composer_vinfo->sync_duration_den);
	vc_print(dev->index, PRINT_OTHER,
		"%s: output_duration is %lld.\n", __func__, output_duration);
	if (output_duration > 60) {
		buf_width = 1920;
		buf_height = 1080;
	}

	vc_print(dev->index, PRINT_OTHER, "%s: usage: %lld\n", __func__, usage);
	if (usage == UVM_USAGE_IMAGE_PLAY) {
		if (buf_width > pic_mode_max_width)
			buf_width = pic_mode_max_width;
		if (buf_height > pic_mode_max_height)
			buf_height = pic_mode_max_height;
	} else {
		if (dev->need_rotate) {
			buf_width = dewarp_rotate_width;
			buf_height = dewarp_rotate_height;
		}
	}

	if (composer_use_444)
		buf_size = buf_width * buf_height * 3;
	else
		buf_size = buf_width * buf_height * 3 / 2;

	buf_size = PAGE_ALIGN(buf_size);
	dev->composer_buf_w = buf_width;
	dev->composer_buf_h = buf_height;
	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr == 0)
			dev->dst_buf[i].phy_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
				buf_size / PAGE_SIZE, 0, flags);
		vc_print(dev->index, PRINT_ERROR,
			 "%s: cma memory is %x , size is  %x\n",
			 ports[dev->index].name,
			 (unsigned int)dev->dst_buf[i].phy_addr,
			 (unsigned int)buf_size);

		if (dev->dst_buf[i].phy_addr == 0) {
			dev->buffer_status = INIT_ERROR;
			vc_print(dev->index, PRINT_ERROR, "cma memory config fail\n");
			return -1;
		}
		dev->dst_buf[i].index = i;
		dev->dst_buf[i].dirty = true;
		dev->dst_buf[i].buf_w = buf_width;
		dev->dst_buf[i].buf_h = buf_height;
		dev->dst_buf[i].buf_size = buf_size;
		dev->dst_buf[i].is_tvp = is_tvp;

		if (!kfifo_put(&dev->free_q, &dev->dst_buf[i].frame))
			vc_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
	}

	return 0;
}

static int vc_init_vicp_buffer(struct composer_dev *dev, bool is_tvp, size_t usage)
{
	int i, j, flags;
	u32 buf_addr = 0;
	u32 buf_width, buf_height, buf_size;
	struct vinfo_s *video_composer_vinfo;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };
	int dw_size = 0, afbc_body_size = 0, afbc_head_size = 0, afbc_table_size = 0;
	u32 *virt_addr = NULL, *temp_addr = NULL;
	u32 temp_body_addr;
	u64 output_duration;
	ulong buf_handle, buf_phy_addr;

	video_composer_vinfo = get_current_vinfo();
	if (IS_ERR_OR_NULL(video_composer_vinfo))
		video_composer_vinfo = &vinfo;
	dev->vinfo_w = video_composer_vinfo->width;
	dev->vinfo_h = video_composer_vinfo->height;
	buf_width = (video_composer_vinfo->width + 0x1f) & ~0x1f;
	buf_height = video_composer_vinfo->height;

	output_duration = div64_u64(video_composer_vinfo->sync_duration_num,
		video_composer_vinfo->sync_duration_den);
	vc_print(dev->index, PRINT_OTHER,
		"%s: output_duration is %lld.\n", __func__, output_duration);
	if (output_duration > 60) {
		buf_width = 1920;
		buf_height = 1080;
	}

	vc_print(dev->index, PRINT_OTHER, "%s: usage: %lld\n", __func__, usage);
	if (usage == UVM_USAGE_IMAGE_PLAY) {
		if (buf_width > pic_mode_max_width)
			buf_width = pic_mode_max_width;
		if (buf_height > pic_mode_max_height)
			buf_height = pic_mode_max_height;
	} else {
		if (dev->need_rotate) {
			buf_width = rotate_width;
			buf_height = rotate_height;
		}

		if (buf_width > vicp_max_width)
			buf_width = vicp_max_width;
		if (buf_height > vicp_max_height)
			buf_height = vicp_max_height;
	}

	if (vicp_output_dev == 1) {
		buf_size = buf_width * buf_height * 3;
		if (composer_use_444 == 0)
			buf_size = buf_size * 3 / 2;
	} else {
		if (vicp_output_dev == 3)//mif
			dw_size = roundup(buf_width >> 2, 32) * roundup(buf_height >> 2, 2);

		afbc_body_size = buf_width * buf_height + (1024 * 1658);
		if (composer_use_444 == 0) {
			dw_size = dw_size * 3 / 2;
			afbc_body_size = afbc_body_size * 3 / 2;
		}

		dw_size = PAGE_ALIGN(dw_size);
		afbc_body_size = roundup(PAGE_ALIGN(afbc_body_size), PAGE_SIZE);
		afbc_head_size = (roundup(buf_width, 64) * roundup(buf_height, 64)) / 32;
		afbc_head_size = PAGE_ALIGN(afbc_head_size);
		afbc_table_size = PAGE_ALIGN((afbc_body_size * 4) / PAGE_SIZE);
		buf_size = dw_size + afbc_body_size + afbc_head_size;
	}

	buf_size = PAGE_ALIGN(buf_size);
	dev->composer_buf_w = buf_width;
	dev->composer_buf_h = buf_height;

	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr == 0)
			buf_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
				buf_size / PAGE_SIZE, 0, flags);

		if (buf_addr == 0) {
			dev->buffer_status = INIT_ERROR;
			vc_print(dev->index, PRINT_ERROR, "cma memory config fail\n");
			return -1;
		}

		dev->dst_buf[i].phy_addr = buf_addr;
		vc_print(dev->index, PRINT_ERROR,
			"%s: cma memory is 0x%x , size is 0x%x.\n",
			ports[dev->index].name, dev->dst_buf[i].phy_addr, buf_size);

		dev->dst_buf[i].index = i;
		dev->dst_buf[i].dirty = true;
		dev->dst_buf[i].buf_w = buf_width;
		dev->dst_buf[i].buf_h = buf_height;
		dev->dst_buf[i].buf_size = buf_size;
		dev->dst_buf[i].is_tvp = is_tvp;

		if (vicp_output_dev != 1) {
			dev->dst_buf[i].dw_size = dw_size;
			dev->dst_buf[i].afbc_body_addr = dev->dst_buf[i].phy_addr + dw_size;
			dev->dst_buf[i].afbc_body_size = afbc_body_size;
			dev->dst_buf[i].afbc_head_addr = dev->dst_buf[i].afbc_body_addr +
				afbc_body_size;
			dev->dst_buf[i].afbc_head_size = afbc_head_size;

			virt_addr = (u32 *)codec_mm_dma_alloc_coherent(&buf_handle, &buf_phy_addr,
				afbc_table_size, dev->port->name);
			dev->dst_buf[i].afbc_table_handle = buf_handle;
			dev->dst_buf[i].afbc_table_addr = buf_phy_addr;
			dev->dst_buf[i].afbc_table_size = afbc_table_size;
			if (dev->dst_buf[i].afbc_table_addr == 0) {
				dev->buffer_status = INIT_ERROR;
				vc_print(dev->index, PRINT_ERROR, "alloc table buf fail.\n");
				return -1;
			}
			temp_body_addr = dev->dst_buf[i].afbc_body_addr & 0xffffffff;
			memset(virt_addr, 0, afbc_table_size);
			temp_addr = virt_addr;
			for (j = 0; j < afbc_body_size; j += 4096) {
				*virt_addr = ((j + temp_body_addr) >> 12) & 0x000fffff;
				virt_addr++;
			}

			vc_print(dev->index, PRINT_VICP, "dw_size = %d.\n", dw_size);
			vc_print(dev->index, PRINT_VICP, "HeadAddr = 0x%lx, headsize = %d.\n",
				dev->dst_buf[i].afbc_head_addr, afbc_head_size);
			vc_print(dev->index, PRINT_VICP, "BodyAddr = 0x%lx, bodysize = %d.\n",
				dev->dst_buf[i].afbc_body_addr, afbc_body_size);
			vc_print(dev->index, PRINT_VICP, "tableAddr = 0x%lx, tablesize = %d.\n",
				dev->dst_buf[i].afbc_table_addr, afbc_table_size);
		}

		if (!kfifo_put(&dev->free_q, &dev->dst_buf[i].frame))
			vc_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
	}

	return 0;
}

static int video_composer_init_buffer(struct composer_dev *dev, bool is_tvp, size_t usage)
{
	int ret = 0;

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

	if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
		ret = vc_init_dewarp_buffer(dev, is_tvp, usage);
		ret |= init_dewarp_composer(&dev->dewarp_para);
	} else if (dev->dev_choice == COMPOSER_WITH_VICP) {
		ret = vc_init_vicp_buffer(dev, is_tvp, usage);
	} else if (dev->dev_choice == COMPOSER_WITH_GE2D) {
		ret = vc_init_ge2d_buffer(dev, is_tvp, usage);
		if (IS_ERR_OR_NULL(dev->ge2d_para.context))
			ret |= init_ge2d_composer(&dev->ge2d_para);
	} else {
		vc_print(dev->index, PRINT_ERROR, "composer device choice error!\n");
		ret = -1;
	}

	if (ret < 0) {
		vc_print(dev->index, PRINT_ERROR, "config vc buf failed!\n");
		return -1;
	} else {
		dev->buffer_status = INIT_SUCCESS;
		return 0;
	}
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
			pr_info("%s: cma free addr is %x\n",
				ports[dev->index].name,
				(unsigned int)dev->dst_buf[i].phy_addr);
			codec_mm_free_for_dma(ports[dev->index].name,
					      dev->dst_buf[i].phy_addr);
			dev->dst_buf[i].phy_addr = 0;
			if (vicp_output_dev != 1 && dev->dev_choice == COMPOSER_WITH_VICP) {
				codec_mm_dma_free_coherent(dev->dst_buf[i].afbc_table_handle);
				dev->dst_buf[i].afbc_table_addr = 0;
			}
		}
	}

	if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
		ret = uninit_dewarp_composer(&dev->dewarp_para);
		if (ret < 0)
			vc_print(dev->index, PRINT_ERROR, "uninit dewarp composer fail!\n");
	}

	if (dev->dev_choice == COMPOSER_WITH_GE2D) {
		if (!IS_ERR_OR_NULL(dev->ge2d_para.context))
			ret = uninit_ge2d_composer(&dev->ge2d_para);
		dev->ge2d_para.context = NULL;
		if (ret < 0)
			vc_print(dev->index, PRINT_ERROR, "uninit ge2d composer failed!\n");
	}

	dev->last_dst_vf = NULL;
	INIT_KFIFO(dev->free_q);
	kfifo_reset(&dev->free_q);
}

static struct file_private_data *vc_get_file_private(struct composer_dev *dev,
						      struct file *file_vf)
{
	struct file_private_data *file_private_data;
	bool is_v4lvideo_fd = false;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		pr_err("vc: get_file_private_data fail\n");
		return NULL;
	}

	if (is_v4lvideo_buf_file(file_vf))
		is_v4lvideo_fd = true;

	if (is_v4lvideo_fd) {
		file_private_data =
			(struct file_private_data *)(file_vf->private_data);
		return file_private_data;
	}

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 VF_PROCESS_V4LVIDEO);
	if (!uhmod) {
		vc_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL\n");
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vc_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL\n");
		return NULL;
	}
	file_private_data = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
			 VF_PROCESS_V4LVIDEO);

	return file_private_data;
}

static struct vf_nn_sr_t *vc_get_hfout_data(struct composer_dev *dev,
						     struct file *file_vf)
{
	struct vf_nn_sr_t *srout_data;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		vc_print(dev->index, PRINT_ERROR, "vc get hfout data fail\n");
		return NULL;
	}

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 PROCESS_NN);
	if (!uhmod) {
		vc_print(dev->index, PRINT_OTHER, "dma file file_private_data is NULL 1\n");
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vc_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL 2\n");
		return NULL;
	}
	srout_data = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
			 PROCESS_NN);

	return srout_data;
}

static struct vf_dalton_t *vc_get_dalton_info(struct composer_dev *dev,
						     struct file *file_vf)
{
	struct vf_dalton_t *dalton_info;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		vc_print(dev->index, PRINT_ERROR, "vc get dalton_info fail\n");
		return NULL;
	}

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 PROCESS_DALTON);
	if (!uhmod) {
		vc_print(dev->index, PRINT_OTHER, "dma file file_private_data is NULL 1\n");
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vc_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL 2\n");
		return NULL;
	}
	dalton_info = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
			 PROCESS_DALTON);

	return dalton_info;
}

static struct vf_aiface_t *vc_get_aiface_info(struct composer_dev *dev,
						struct file *file_vf, int omx_index)
{
	struct vf_aiface_t *aiface_info;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		vc_print(dev->index, PRINT_ERROR, "NULL param: omx_index=%d.\n", omx_index);

		return NULL;
	}
	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data), PROCESS_AIFACE);
	if (!uhmod) {
		vc_print(dev->index, PRINT_OTHER, "NULL uhmod: omx_index=%d\n", omx_index);
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vc_print(dev->index, PRINT_ERROR, "invalid uhmod param:omx_index=%d\n", omx_index);
		return NULL;
	}
	aiface_info = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data), PROCESS_AIFACE);

	return aiface_info;
}

static struct vf_aicolor_t *vc_get_aicolor_info(struct composer_dev *dev,
		struct file *file_vf)
{
	struct vf_aicolor_t *aicolor_info;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		vc_print(dev->index, PRINT_ERROR, "vc get aicolor_info fail.\n");
		return NULL;
	}
	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 PROCESS_AICOLOR);
	if (!uhmod) {
		vc_print(dev->index, PRINT_OTHER, "dma file file_private_data is NULL 1\n");
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vc_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL 2\n");
		return NULL;
	}
	aicolor_info = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
			 PROCESS_AICOLOR);

	return aicolor_info;
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
		total_put_count++;
		dev->fput_count++;
	}
}

void vc_private_q_init(struct composer_dev *dev)
{
	int i;

	INIT_KFIFO(dev->vc_private_q);
	kfifo_reset(&dev->vc_private_q);

	for (i = 0; i < COMPOSER_READY_POOL_SIZE; i++) {
		dev->vc_private[i].index = i;
		dev->vc_private[i].flag = 0;
		dev->vc_private[i].srout_data = NULL;
		dev->vc_private[i].src_vf = NULL;
		dev->vc_private[i].vsync_index = 0;
		if (!kfifo_put(&dev->vc_private_q, &dev->vc_private[i]))
			vc_print(dev->index, PRINT_ERROR,
				"q_init: vc_private_q is full!\n");
	}
}

void vc_private_q_recycle(struct composer_dev *dev,
	struct video_composer_private *vc_private)
{
	if (!vc_private)
		return;

	memset(vc_private, 0, sizeof(struct video_composer_private));

	if (!kfifo_put(&dev->vc_private_q, vc_private))
		vc_print(dev->index, PRINT_ERROR,
			"vc_private_q is full!\n");
}

struct video_composer_private *vc_private_q_pop(struct composer_dev *dev)
{
	struct video_composer_private *vc_private = NULL;

	if (!kfifo_get(&dev->vc_private_q, &vc_private)) {
		vc_print(dev->index, PRINT_ERROR,
			 "task: get vc_private_q failed\n");
		vc_private = NULL;
	} else {
		vc_private->flag = 0;
		vc_private->srout_data = NULL;
		vc_private->src_vf = NULL;
	}

	return vc_private;
}

static void display_q_uninit(struct composer_dev *dev)
{
	struct vframe_s *dis_vf = NULL;
	int repeat_count;
	int i;
	bool is_mosaic_22 = false;
	struct file *file_vf;

	vc_print(dev->index, PRINT_QUEUE_STATUS, "vc: unit display_q len=%d\n",
		 kfifo_len(&dev->display_q));

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &dis_vf)) {
			is_mosaic_22 = dis_vf->type_ext & VIDTYPE_EXT_MOSAIC_22;

			if (dis_vf->flag
			    & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS && !is_mosaic_22) {
				repeat_count = dis_vf->repeat_count;
				vc_print(dev->index, PRINT_FENCE,
					 "vc: unit repeat_count=%d, omx_index=%d\n",
					 repeat_count,
					 dis_vf->omx_index);
				for (i = 0; i <= repeat_count; i++) {
					fput(dis_vf->file_vf);
					total_put_count++;
					dev->fput_count++;
				}
			} else if (is_mosaic_22) {
				for (i = 0; i < 4; i++) {
					file_vf = dis_vf->vc_private->mosaic_vf[i]->file_vf;
					if (file_vf) {
						fput(file_vf);
						total_put_count++;
						dev->fput_count++;
					} else {
						vc_print(dev->index, PRINT_ERROR,
							"%s error!!!: i=%d fput fail\n",
							__func__, i);
					}
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
			if (!dis_vf) {
				vc_print(dev->index, PRINT_ERROR, "%s: dis_vf is NULL.\n",
					__func__);
				break;
			}

			if (dis_vf->vc_private)
				if (dis_vf->vc_private->srout_data) {
					if (dis_vf->vc_private->srout_data->nn_status == NN_DONE)
						dis_vf->vc_private->srout_data->nn_status =
							NN_DISPLAYED;
				}

			if (dis_vf->flag
			    & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS) {
				repeat_count = dis_vf->repeat_count;
				for (i = 0; i <= repeat_count; i++) {
					fput(dis_vf->file_vf);
					total_put_count++;
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
	vc_print(dev->index, PRINT_PATTERN, "put: vf=%p\n", vf);
	wake_up_interruptible(&dev->wq);
}

struct vframe_s *videocomposer_vf_peek(void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vframe_s *vf = NULL;
	struct timeval now_time;
	struct timeval nn_start_time;
	u64 nn_used_time;
	bool canbe_peek = true;
	u32 nn_status;
	u32 nn_mode;
	bool bypass_nn = false;

	if (kfifo_peek(&dev->ready_q, &vf)) {
		if (!vf)
			return NULL;

		if (!vf->vc_private) {
			vc_print(dev->index, PRINT_OTHER,
				"peek: vf->vc_private is NULL\n");
			return vf;
		}

		if ((vf->vc_private->flag & VC_FLAG_AI_SR) == 0)
			return vf;

		nn_status = vf->vc_private->srout_data->nn_status;
		nn_mode = vf->vc_private->srout_data->nn_mode;

		vc_print(dev->index, PRINT_NN,
			"peek:nn_status=%d, nn_index=%d, nn_mode=%d, PHY=%llx, nn out:%d*%d, hf:%d*%d,hf_align:%d*%d\n",
			vf->vc_private->srout_data->nn_status,
			vf->vc_private->srout_data->nn_index,
			vf->vc_private->srout_data->nn_mode,
			vf->vc_private->srout_data->nn_out_phy_addr,
			vf->vc_private->srout_data->nn_out_width,
			vf->vc_private->srout_data->nn_out_height,
			vf->vc_private->srout_data->hf_width,
			vf->vc_private->srout_data->hf_height,
			vf->vc_private->srout_data->hf_align_w,
			vf->vc_private->srout_data->hf_align_h);
		if (nn_status != NN_DONE) {
			if (nn_status == NN_INVALID) {
				vf->vc_private->flag &= ~VC_FLAG_AI_SR;
				vc_print(dev->index, PRINT_NN | PRINT_OTHER,
					"nn status is invalid, need bypass");
				return vf;
			} else if (nn_status == NN_WAIT_DOING) {
				vc_print(dev->index, PRINT_FENCE | PRINT_NN,
					"peek: nn wait doing, nn_index =%d, omx_index=%d, nn_status=%d,srout_data=%px\n",
					vf->vc_private->srout_data->nn_index,
					vf->omx_index,
					vf->vc_private->srout_data->nn_status,
					vf->vc_private->srout_data);
				return NULL;
			} else if (nn_status == NN_DISPLAYED) {
				vc_print(dev->index, PRINT_ERROR,
					"peek: nn_status err, nn_index =%d, omx_index=%d, nn_status=%d\n",
					vf->vc_private->srout_data->nn_index,
					vf->omx_index,
					vf->vc_private->srout_data->nn_status);
				return vf;
			}

			if (!(vf->type_original & VIDTYPE_INTERLACE)) {
				if (!(dev->nn_mode_flag & 0x1) && nn_mode == 1) {
					dev->nn_mode_flag |= 0x1;
					bypass_nn = true;
				} else if (!(dev->nn_mode_flag & 0x2) && nn_mode == 2) {
					dev->nn_mode_flag |= 0x2;
					bypass_nn = true;
				} else if (!(dev->nn_mode_flag & 0x4) && nn_mode == 3) {
					dev->nn_mode_flag |= 0x4;
					bypass_nn = true;
				}
			} else {
				if (!(dev->nn_mode_flag & 0x100) && nn_mode == 1) {
					dev->nn_mode_flag |= 0x100;
					bypass_nn = true;
				} else if (!(dev->nn_mode_flag & 0x200) && nn_mode == 2) {
					dev->nn_mode_flag |= 0x200;
					bypass_nn = true;
				} else if (!(dev->nn_mode_flag & 0x400) && nn_mode == 3) {
					dev->nn_mode_flag |= 0x400;
					bypass_nn = true;
				}
			}

			if (bypass_nn) {
				vf->vc_private->flag &= ~VC_FLAG_AI_SR;
				vc_print(dev->index, PRINT_NN,
					"nn mode change, bypass first frame\n");
				return vf;
			}

			do_gettimeofday(&now_time);
			nn_start_time = vf->vc_private->srout_data->start_time;
			nn_used_time = (u64)1000000 *
				(now_time.tv_sec - nn_start_time.tv_sec)
				+ now_time.tv_usec - nn_start_time.tv_usec;

			if (nn_used_time < (nn_need_time - nn_margin_time))
				canbe_peek = false;
			vc_print(dev->index, PRINT_FENCE | PRINT_NN,
				"peek: nn not done, nn_index = %d, omx_index = %d, nn_status = %d, nn_used_time = %lld, canbe_peek = %d.\n",
				vf->vc_private->srout_data->nn_index,
				vf->omx_index,
				vf->vc_private->srout_data->nn_status,
				nn_used_time,
				canbe_peek);
			if (!canbe_peek) {
				vc_print(dev->index, PRINT_FENCE | PRINT_NN,
				"peek:fail: nn not done, nn_index =%d, omx_index=%d, nn_status=%d, nn_used_time=%lld canbe_peek=%d\n",
					vf->vc_private->srout_data->nn_index,
					vf->omx_index,
					vf->vc_private->srout_data->nn_status,
					nn_used_time,
					canbe_peek);
				return NULL;
			}
		}

		return vf;
	} else {
		return NULL;
	}
}

void videocomposer_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	int repeat_count;
	int omx_index;
	int index_disp;
	bool rendered;
	bool is_composer;
	bool is_mosaic_22;
	int i;
	struct file *file_vf;
	struct vd_prepare_s *vd_prepare;

	if (!vf)
		return;

	repeat_count = vf->repeat_count;
	omx_index = vf->omx_index;
	index_disp = vf->index_disp;
	rendered = vf->rendered;
	is_composer = vf->flag & VFRAME_FLAG_COMPOSER_DONE;
	is_mosaic_22 = vf->type_ext & VIDTYPE_EXT_MOSAIC_22;

	if (vf->flag & VFRAME_FLAG_FAKE_FRAME) {
		vc_print(dev->index, PRINT_OTHER,
			 "put: fake frame\n");
		return;
	}

	if (vf->vc_private && vf->vc_private->srout_data) {
		if (vf->vc_private->srout_data->nn_status == NN_DONE)
			vf->vc_private->srout_data->nn_status = NN_DISPLAYED;
	}

	if (vf->vc_private && !is_mosaic_22) {
		vc_private_q_recycle(dev, vf->vc_private);
		vf->vc_private = NULL;
	}

	vc_print(dev->index, PRINT_FENCE,
		 "put: repeat_count =%d, omx_index=%d, index_disp=%x\n",
		 repeat_count, omx_index, index_disp);

	if (rendered) {
		video_timeline_increase(dev, repeat_count
					+ 1 + dev->drop_frame_count);
		dev->drop_frame_count = 0;
	} else {
		dev->drop_frame_count += repeat_count + 1;
		vc_print(dev->index, PRINT_PERFORMANCE | PRINT_FENCE,
			 "put: drop repeat_count=%d\n", repeat_count);
	}

	if (!is_composer && !is_mosaic_22) {
		vd_prepare = container_of(vf, struct vd_prepare_s, dst_frame);
		if (IS_ERR_OR_NULL(vd_prepare)) {
			vc_print(dev->index, PRINT_ERROR,
				"%s: prepare is NULL.\n",
				__func__);
			return;
		}
		file_vf = vd_prepare->src_frame->file_vf;
		for (i = 0; i <= repeat_count; i++) {
			if (file_vf) {
				fput(file_vf);
				total_put_count++;
				dev->fput_count++;
			} else {
				vc_print(dev->index, PRINT_ERROR,
					"%s error:src_index=%d,dst_index=%d.\n",
					__func__,
					vd_prepare->src_frame->omx_index,
					vd_prepare->dst_frame.omx_index);
			}
		}
		vd_prepare_data_q_put(dev, vd_prepare);
	} else if (is_mosaic_22) {
		for (i = 0; i < 4; i++) {
			if (!vf->vc_private) {
				vc_print(dev->index, PRINT_ERROR, "put mosaic no priv!!!\n");
				break;
			}
			file_vf = vf->vc_private->mosaic_vf[i]->file_vf;
			if (file_vf) {
				fput(file_vf);
				total_put_count++;
				dev->fput_count++;
			} else {
				vc_print(dev->index, PRINT_ERROR,
					"%s error: i=%d,src_index=%d,dst_index=%d.\n",
					__func__,
					i,
					vf->vc_private->mosaic_src_vf[i]->omx_index,
					vf->vc_private->mosaic_dst_vf[i].omx_index);
			}
		}
		if (vf->vc_private) {
			vc_private_q_recycle(dev, vf->vc_private);
			vf->vc_private = NULL;
		}

		vd_prepare = container_of(vf, struct vd_prepare_s, dst_frame);
		if (IS_ERR_OR_NULL(vd_prepare)) {
			vc_print(dev->index, PRINT_ERROR,
				"%s: prepare is NULL.\n",
				__func__);
			return;
		}
		vd_prepare_data_q_put(dev, vd_prepare);
	} else {
		videocom_vf_put(vf, dev);
	}
}

static unsigned long get_dma_phy_addr(int fd, int index)
{
	unsigned long phy_addr = 0;
	struct dma_buf *dbuf = NULL;
	struct sg_table *table = NULL;
	struct page *page = NULL;
	struct dma_buf_attachment *attach = NULL;

	dbuf = dma_buf_get(fd);
	attach = dma_buf_attach(dbuf, ports[index].pdev);
	if (IS_ERR(attach))
		return 0;

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	page = sg_page(table->sgl);
	phy_addr = PFN_PHYS(page_to_pfn(page));
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dbuf, attach);
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

static u32 need_switch_buffer(struct dst_buf_t *buf, bool is_tvp, struct composer_dev *dev)
{
	int flags, ret;
	bool vicp_fbc_out_en = false;

	if (IS_ERR_OR_NULL(buf) || IS_ERR_OR_NULL(dev)) {
		vc_print(dev->index, PRINT_ERROR, "%s: NULL param.\n", __func__);
		return -1;
	}

	vc_print(dev->index, PRINT_OTHER, "%s: is about %s buffer\n", __func__,
		is_tvp ? "none tvp" : "tvp");
	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	if (vicp_output_dev != 1 && dev->dev_choice == COMPOSER_WITH_VICP)
		vicp_fbc_out_en = true;

	if (buf->phy_addr > 0) {
		vc_print(dev->index, PRINT_OTHER, "free buffer 0x%x\n", buf->phy_addr);
		codec_mm_free_for_dma(ports[dev->index].name, buf->phy_addr);
	}

	buf->phy_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
					buf->buf_size / PAGE_SIZE, 0, flags);
	vc_print(dev->index, PRINT_ERROR, "%s: alloc buffer 0x%x\n", __func__, buf->phy_addr);

	if (vicp_fbc_out_en) {
		buf->afbc_body_addr = buf->phy_addr + buf->dw_size;
		buf->afbc_head_addr = buf->afbc_body_addr + buf->afbc_body_size;

		if (buf->afbc_table_addr > 0) {
			vc_print(dev->index, PRINT_OTHER, "%s: free table buffer 0x%lx\n", __func__,
				buf->afbc_table_addr);
			codec_mm_free_for_dma(ports[dev->index].name, buf->afbc_table_addr);
		}
		buf->afbc_table_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
					buf->afbc_table_size / PAGE_SIZE, 0, flags);
		vc_print(dev->index, PRINT_ERROR, "%s: alloc buffer 0x%lx\n", __func__,
			buf->afbc_table_addr);
	}

	buf->is_tvp = is_tvp;

	if (buf->phy_addr == 0 || (vicp_fbc_out_en && buf->afbc_table_addr == 0))
		ret = 0;
	else
		ret = 1;

	return ret;
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
	struct frame_info_t *vframe_info)
{
	int picture_width = 0, picture_height = 0;
	int render_w = 0, render_h = 0;
	int disp_w, disp_h;
	struct output_axis axis;
	int tmp;

	memset(&axis, 0, sizeof(struct output_axis));
	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(vframe_info)) {
		pr_info("%s: invalid param.\n", __func__);
		return axis;
	}

	picture_width = vframe_info->crop_w;
	picture_height = vframe_info->crop_h;
	disp_w = vframe_info->dst_w;
	disp_h = vframe_info->dst_h;

	if (vframe_info->transform == VC_TRANSFORM_ROT_90 ||
		vframe_info->transform == VC_TRANSFORM_ROT_270) {
		tmp = picture_height;
		picture_height = picture_width;
		picture_width = tmp;
	}
	if (!full_axis) {
		render_w = disp_w;
		render_h = disp_w * picture_height / picture_width;
		if (render_h > disp_h) {
			render_h = disp_h;
			render_w = disp_h * picture_width / picture_height;
		}
	} else {
		render_w = disp_w;
		render_h = disp_h;
	}
	axis.left = vframe_info->dst_x + (disp_w - render_w) / 2;
	axis.top = vframe_info->dst_y + (disp_h - render_h) / 2;
	axis.width = render_w;
	axis.height = render_h;

	vc_print(dev->index, PRINT_AXIS,
		 "frame out data axis left top width height: %d %d %d %d\n",
		 axis.left, axis.top, axis.width, axis.height);
	return axis;
}

bool vf_is_pre_link(struct vframe_s *vf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (vf && pvpp_check_vf(vf) > 0 && vf->vf_ext)
		return true;
#endif
	return false;
}

static struct vframe_s *get_vf_from_file(struct composer_dev *dev,
					 struct file *file_vf, bool need_dw)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *di_vf = NULL;
	bool is_dec_vf = false;
	struct file_private_data *file_private_data = NULL;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(file_vf)) {
		vc_print(dev->index, PRINT_ERROR,
			"%s: invalid param.\n",
			__func__);
		return vf;
	}

	is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);

	if (is_dec_vf) {
		vc_print(dev->index, PRINT_OTHER, "vf is from decoder\n");
		vf =
		dmabuf_get_vframe((struct dma_buf *)(file_vf->private_data));
		if (!vf) {
			vc_print(dev->index, PRINT_ERROR, "vf is NULL.\n");
			return vf;
		}

		di_vf = vf->vf_ext;
		vc_print(dev->index, PRINT_OTHER,
			"vframe_type = 0x%x, vframe_flag = 0x%x.\n",
			vf->type,
			vf->flag);
		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			vc_print(dev->index, PRINT_OTHER,
				"di_vf->type = 0x%x, di_vf->org = 0x%x.\n",
				di_vf->type,
				di_vf->type_original);
			if (!need_dw ||
			    (need_dw && di_vf->width != 0 &&
				di_vf->canvas0_config[0].phy_addr != 0 &&
				!vf_is_pre_link(di_vf))) {
				vc_print(dev->index, PRINT_OTHER,
					"use di vf\n");
				/* link uvm vf into di_vf->vf_ext */
				if (!di_vf->vf_ext)
					di_vf->vf_ext = vf;
				/* link uvm vf into vf->uvm_vf */
				di_vf->uvm_vf = vf;
				vf = di_vf;
			}
		}
		dmabuf_put_vframe((struct dma_buf *)(file_vf->private_data));
		if (vf->omx_index == 0 && vf->index_disp != 0)
			vf->omx_index = vf->index_disp;
	} else {
		vc_print(dev->index, PRINT_OTHER, "vf is from v4lvideo\n");
		file_private_data = vc_get_file_private(dev, file_vf);
		if (!file_private_data) {
			vc_print(dev->index, PRINT_ERROR,
				 "invalid fd: no uvm, no v4lvideo!!\n");
		} else {
			vf = &file_private_data->vf;
			if (need_dw && (vf->flag & VFRAME_FLAG_DOUBLE_FRAM) && vf->vf_ext) {
				if (vf->width == 0 ||
					vf->canvas0_config[0].phy_addr == 0 ||
					vf_is_pre_link(vf)) {
					vf = vf->vf_ext;
					vc_print(dev->index, PRINT_OTHER, "use dec vf.\n");
				}
			}
		}
	}
	return vf;
}

static void dump_vf(int vc_index, struct vframe_s *vf, int flag)
{
	struct file *fp = NULL;
	char name_buf[32];
	int data_size_y, data_size_uv;
	u8 *data_y;
	u8 *data_uv;
	mm_segment_t fs;
	loff_t pos;

	/*use flag to distinguish src and dst vframe*/
	if (!vf)
		return;

	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
		vc_print(vc_index, PRINT_ERROR, "%s: security vf.\n", __func__);
		return;
	}

	if (flag == 0)
		snprintf(name_buf, sizeof(name_buf),
			"/data/src_vframe_%d.yuv", dump_vframe);
	else
		snprintf(name_buf, sizeof(name_buf),
			"/data/dst_vframe_%d.yuv", dump_vframe);
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	data_size_y = vf->canvas0_config[0].width *
			vf->canvas0_config[0].height;
	data_size_uv = vf->canvas0_config[1].width *
			vf->canvas0_config[1].height;
	data_y = codec_mm_vmap(vf->canvas0_config[0].phy_addr, data_size_y);
	data_uv = codec_mm_vmap(vf->canvas0_config[1].phy_addr, data_size_uv);
	if (!data_y || !data_uv) {
		vc_print(vc_index, PRINT_ERROR, "%s: vmap failed.\n", __func__);
		return;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;
	vfs_write(fp, data_y, data_size_y, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	vc_print(vc_index, PRINT_ERROR, "%s: write %u size to addr%p\n",
		__func__, data_size_y, data_y);
	codec_mm_unmap_phyaddr(data_y);
	pos = fp->f_pos;
	vfs_write(fp, data_uv, data_size_uv, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	vc_print(vc_index, PRINT_ERROR, "%s: write %u size to addr%p\n",
		__func__, data_size_uv, data_uv);
	codec_mm_unmap_phyaddr(data_uv);
	set_fs(fs);
	filp_close(fp, NULL);
}

static bool check_dewarp_support_status(struct composer_dev *dev,
	struct received_frames_t *received_frames)
{
	struct frame_info_t frame_info;
	struct composer_vf_para vframe_para;
	struct vframe_s *src_vf = NULL;
	struct file *file_vf = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(received_frames)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return false;
	}

	memset(&vframe_para, 0, sizeof(struct composer_vf_para));
	memset(&frame_info, 0, sizeof(struct frame_info_t));
	frame_info = received_frames->frames_info.frame_info[0];
	vframe_para.src_vf_width = frame_info.crop_w;
	vframe_para.src_vf_height = frame_info.crop_h;
	vframe_para.dst_vf_width = dewarp_rotate_width;
	vframe_para.dst_vf_height = dewarp_rotate_height;
	vframe_para.src_vf_angle = frame_info.transform;
	file_vf = received_frames->file_vf[0];
	is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf || is_v4l_vf) {
		src_vf = get_vf_from_file(dev, file_vf, true);
		if (!src_vf) {
			vc_print(dev->index, PRINT_ERROR, "get vf NULL\n");
			vframe_para.src_vf_format = NV12;
		} else {
			vframe_para.src_vf_format = get_dewarp_format(dev->index, src_vf);
		}
	} else {
		vframe_para.src_vf_format = NV12;
	}
	dev->dewarp_para.vf_para = &vframe_para;
	if (dev->need_rotate && is_dewarp_supported(dev->index, dev->dewarp_para.vf_para))
		return true;
	else
		return false;
}

static void choose_composer_device(struct composer_dev *dev,
	struct received_frames_t *received_frames)
{
	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(received_frames)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		dev->dev_choice = COMPOSER_WITH_GE2D;
		return;
	}

	if (composer_dev_choice == 0) {
		if (check_dewarp_support_status(dev, received_frames))
			dev->dev_choice = COMPOSER_WITH_DEWARP;
		else if (is_vicp_supported() && !dev->need_rotate)
			dev->dev_choice = COMPOSER_WITH_VICP;
		else
			dev->dev_choice = COMPOSER_WITH_GE2D;
	} else {
		if (check_dewarp_support_status(dev, received_frames) && composer_dev_choice == 2)
			dev->dev_choice = COMPOSER_WITH_DEWARP;
		else if (is_vicp_supported() && composer_dev_choice == 3 && !dev->need_rotate)
			dev->dev_choice = COMPOSER_WITH_VICP;
		else
			dev->dev_choice = COMPOSER_WITH_GE2D;
	}

	if (dev->dev_choice == COMPOSER_WITH_VICP)
		vicp_process_enable(1);
}

static void check_vicp_skip_mode(struct composer_dev *dev, struct frames_info_t *frames_info,
	enum vicp_skip_mode_e *buf)
{
	int i = 0, input_4k_count = 0;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(frames_info) || IS_ERR_OR_NULL(buf)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return;
	}

	for (i = 0; i < frames_info->frame_count; i++) {
		if (frames_info->frame_info[i].buffer_w > 1920) {
			input_4k_count++;
			buf[i] = VICP_SKIP_MODE_ALL;
		} else {
			buf[i] = VICP_SKIP_MODE_OFF;
		}
	}

	if (input_4k_count <= 2) {
		vc_print(dev->index, PRINT_VICP, "%s: no need skip.\n", __func__);
		memset(buf, VICP_SKIP_MODE_OFF, frames_info->frame_count);
	}
}

static void video_wait_aiface_ready(struct composer_dev *dev,
				struct vf_aiface_t *aiface_info, int omx_index)
{
	int wait_count = 0;

	while (1) {
		if (aiface_info->nn_status != NN_DONE &&
			(aiface_info->nn_status == NN_WAIT_DOING ||
			aiface_info->nn_status == NN_START_DOING)) {
			usleep_range(2000, 2100);
			wait_count++;
			vc_print(dev->index, PRINT_PATTERN | PRINT_AIFACE,
				"aiface wait count %d, omx_index=%d, nn_status=%d\n",
				wait_count, omx_index, aiface_info->nn_status);
		} else {
			break;
		}
	}
	vc_print(dev->index, PRINT_PATTERN | PRINT_AIFACE,
		 "aiface wait %dms, omx_index=%d, nn_status=%d\n",
		 2 * wait_count, omx_index, aiface_info->nn_status);
}

static struct vf_aiface_t *aiface_info_adjust(struct composer_dev *dev, struct file *file_vf,
	int omx_index, int display_x, int display_y, int display_w, int display_h)
{
	struct vf_aiface_t *aiface_info = NULL;
	struct face_value_t face_value_tmp;
	int nn_width, nn_height, count = 0;
	int i = 0;

	aiface_info = vc_get_aiface_info(dev, file_vf, omx_index);
	if (aiface_info) {
		nn_width = aiface_info->nn_frame_width;
		nn_height = aiface_info->nn_frame_height;
		video_wait_aiface_ready(dev, aiface_info, omx_index);
		if (aiface_info->nn_status == NN_DONE) {
			memset(&face_value_tmp, 0, sizeof(struct face_value_t));
			count = aiface_info->aiface_value_count;
			for (i = 0; i < count; i++) {
				vc_print(dev->index, PRINT_AIFACE,
					"src: omx_index=%d, x=%d, y=%d, w=%d, h=%d, score=%d.\n",
					omx_index,
					aiface_info->face_value[i].x,
					aiface_info->face_value[i].y,
					aiface_info->face_value[i].w,
					aiface_info->face_value[i].h,
					aiface_info->face_value[i].score);
				face_value_tmp.x = display_x +
					display_w * aiface_info->face_value[i].x / nn_width;
				face_value_tmp.y = display_y +
					display_h * aiface_info->face_value[i].y / nn_height;
				face_value_tmp.w = display_w *
					aiface_info->face_value[i].w / nn_width;
				face_value_tmp.h = display_h *
					aiface_info->face_value[i].h / nn_height;
				face_value_tmp.score = aiface_info->face_value[i].score;

				aiface_info->face_value[i] = face_value_tmp;
				vc_print(dev->index, PRINT_AIFACE,
					"dst: omx_index=%d, x=%d, y=%d, w=%d, h=%d, score=%d.\n",
					omx_index,
					aiface_info->face_value[i].x,
					aiface_info->face_value[i].y,
					aiface_info->face_value[i].w,
					aiface_info->face_value[i].h,
					aiface_info->face_value[i].score);
			}

			aiface_info->nn_status = NN_DISPLAYED;
		} else {
			vc_print(dev->index, PRINT_AIFACE,
				"%s: omx_index=%d, aiface not ready.\n",
				__func__, omx_index);
		}
	} else {
		//vc_print(dev->index, PRINT_ERROR, "%s: get aiface info failed.\n", __func__);
		aiface_info = NULL;
	}

	return aiface_info;
}

static void vframe_do_mosaic_22(struct composer_dev *dev)
{
	struct received_frames_t *received_frames = NULL;
	struct vd_prepare_s *vd_prepare = NULL;
	struct vframe_s *vf = NULL;
	struct vframe_s *scr_vf = NULL;
	struct vframe_s *mosaic_vf = NULL;
	struct vframe_s *vf_ext = NULL;
	struct frames_info_t *frames_info = NULL;
	struct file *file_vf = NULL;
	int i;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct video_composer_private *vc_private;
	struct frame_info_t *frame_info = NULL;
	u32 pic_w;
	u32 pic_h;

	if (IS_ERR_OR_NULL(dev)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return;
	}

	if (!kfifo_peek(&dev->receive_q, &received_frames))
		return;

	vd_prepare = vd_prepare_data_q_get(dev);
	if (!vd_prepare) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s: get prepare_data failed.\n",
			 __func__);
		return;
	}

	vf = &vd_prepare->dst_frame;
	memset(vf, 0, sizeof(struct vframe_s));

	if (!kfifo_get(&dev->receive_q, &received_frames)) {
		vc_print(dev->index, PRINT_ERROR, "com: get failed\n");
		return;
	}

	vc_private = vc_private_q_pop(dev);
	if (!vc_private) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s: get vc_private failed.\n",
			 __func__);
		return;
	}

	vc_private->flag |= VC_FLAG_MOSAIC_22;
	vf->vc_private = vc_private;

	frames_info = &received_frames->frames_info;

	for (i = 0; i < 4; i++) {
		frame_info = &frames_info->frame_info[i];
		scr_vf = NULL;
		file_vf = received_frames->file_vf[i];
		is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
		is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

		if (is_dec_vf || is_v4l_vf) {
			vc_print(dev->index, PRINT_OTHER,
				 "%s dma buffer is vf\n", __func__);
			scr_vf = get_vf_from_file(dev, file_vf, false);
			if (!scr_vf) {
				vc_print(dev->index,
					 PRINT_ERROR, "get vf NULL\n");
				continue;
			}
		} else {
			vc_print(dev->index, PRINT_ERROR, "%s dma buffer not vf\n", __func__);
		}
		if (!scr_vf) {
			vc_print(dev->index, PRINT_ERROR, "%s:no vf\n", __func__);
			return;
		}
		vc_private->mosaic_src_vf[i] = scr_vf;
		vc_private->mosaic_dst_vf[i] = *scr_vf;
		vc_private->mosaic_vf[i] = &vc_private->mosaic_dst_vf[i];
		mosaic_vf = vc_private->mosaic_vf[i];

		mosaic_vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
			| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
		mosaic_vf->axis[0] = frame_info->dst_x;
		mosaic_vf->axis[1] = frame_info->dst_y;
		mosaic_vf->axis[2] = frame_info->dst_w + frame_info->dst_x - 1;
		mosaic_vf->axis[3] = frame_info->dst_h + frame_info->dst_y - 1;
		mosaic_vf->crop[0] = frame_info->crop_y;
		mosaic_vf->crop[1] = frame_info->crop_x;
		if ((mosaic_vf->type & VIDTYPE_COMPRESS) != 0) {
			pic_w = mosaic_vf->compWidth;
			pic_h = mosaic_vf->compHeight;
		} else {
			pic_w = mosaic_vf->width;
			pic_h = mosaic_vf->height;
		}
		mosaic_vf->crop[2] = pic_h - frame_info->crop_h - frame_info->crop_y;
		mosaic_vf->crop[3] = pic_w - frame_info->crop_w - frame_info->crop_x;

		mosaic_vf->zorder = frame_info->zorder;
		mosaic_vf->file_vf = file_vf;

		if (mosaic_vf->flag & VFRAME_FLAG_DOUBLE_FRAM) {
			vf_ext = mosaic_vf->vf_ext;
			if (vf_ext) {
				vf_ext->axis[0] = mosaic_vf->axis[0];
				vf_ext->axis[1] = mosaic_vf->axis[1];
				vf_ext->axis[2] = mosaic_vf->axis[2];
				vf_ext->axis[3] = mosaic_vf->axis[3];
				vf_ext->crop[0] = mosaic_vf->crop[0];
				vf_ext->crop[1] = mosaic_vf->crop[1];
				vf_ext->crop[2] = mosaic_vf->crop[2];
				vf_ext->crop[3] = mosaic_vf->crop[3];
				vf_ext->zorder = mosaic_vf->zorder;
				vf_ext->flag |= VFRAME_FLAG_VIDEO_COMPOSER
					| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
			} else {
				vc_print(dev->index, PRINT_ERROR,
					 "vf_ext is null\n");
			}
		}
	}

	vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
		| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;

	vf->bitdepth = (BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8);

	vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
	vf->type = (VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV21);
	vf->type_ext |= VIDTYPE_EXT_MOSAIC_22;

	vf->axis[0] = 0;
	vf->axis[1] = 0;
	vf->axis[2] = dev->vinfo_w - 1;
	vf->axis[3] = dev->vinfo_h - 1;

	vf->crop[0] = 0;
	vf->crop[1] = 0;
	vf->crop[2] = 0;
	vf->crop[3] = 0;

	vf->zorder = frames_info->disp_zorder;
	vf->canvas0Addr = -1;
	vf->canvas1Addr = -1;

	vf->width = dev->vinfo_w;
	vf->height = dev->vinfo_h;

	vc_print(dev->index, PRINT_DEWARP,
			 "composer:vf_w: %d, vf_h: %d\n", vf->width, vf->height);

	vf->canvas0_config[0].phy_addr = 0;
	vf->canvas0_config[0].width = dev->vinfo_w;
	vf->canvas0_config[0].height = dev->vinfo_h;
	vf->canvas0_config[0].block_mode = 0;

	vf->canvas0_config[1].phy_addr = 0;
	vf->canvas0_config[1].width = dev->vinfo_w;
	vf->canvas0_config[1].height = dev->vinfo_h >> 1;
	vf->canvas0_config[1].block_mode = 0;
	vf->plane_num = 2;

	vf->repeat_count = 0;

	dev->vd_prepare_last = vd_prepare;

	dev->fake_vf = *vf;

	if (!kfifo_put(&dev->ready_q, (const struct vframe_s *)vf))
		vc_print(dev->index, PRINT_ERROR, "ready_q is full\n");

	atomic_set(&received_frames->on_use, false);
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
	size_t usage = 0;
	int i, j, tmp;
	u32 zd1, zd2;
	struct timeval begin_time;
	struct timeval end_time;
	int cost_time;
	int ret = 0;
	struct vframe_s *dst_vf = NULL;
	int count;
	struct dst_buf_t *dst_buf = NULL;
	struct src_data_para src_data;
	u32 drop_count = 0;
	unsigned long addr = 0;
	struct output_axis dst_axis;
	struct output_axis display_axis;
	int min_left = 0, min_top = 0;
	int max_right = 0, max_bottom = 0;
	struct composer_info_t *composer_info;
	bool is_dec_vf = false, is_v4l_vf = false;
	bool is_tvp = false;
	bool need_dw = false;
	bool is_fixtunnel = false;
	int transform = 0;
	struct composer_vf_para vframe_para;
	struct vicp_data_config_s data_config;
	struct crop_info_s crop_info;
	ulong buf_addr[3];
	int fbc_init_ctrl, fbc_pip_mode;
	int mifout_en = 1, fbcout_en = 1;
	u32 src_fmt = 0;
	enum vicp_skip_mode_e skip_mode[MXA_LAYER_COUNT] = {VICP_SKIP_MODE_OFF};
	struct vf_aiface_t *aiface_info_temp = NULL;
	u32 num = 0, last_num = 0, size = 0;
	struct vframe_s *input_vf[MXA_LAYER_COUNT] = {NULL};
	struct output_axis out_axis[MXA_LAYER_COUNT] = {0};
	struct frame_info_t *vframe_info_cur = NULL;

	if (IS_ERR_OR_NULL(dev)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return;
	}

	do_gettimeofday(&begin_time);

	if (!kfifo_peek(&dev->receive_q, &received_frames_tmp))
		return;

	choose_composer_device(dev, received_frames_tmp);
	is_tvp = received_frames_tmp->is_tvp;
	if (meson_uvm_get_usage(received_frames_tmp->file_vf[0]->private_data, &usage) < 0)
		vc_print(dev->index, PRINT_ERROR,
			"%s:meson_uvm_get_usage fail.\n", __func__);
	ret = video_composer_init_buffer(dev, is_tvp, usage);
	if (ret != 0) {
		vc_print(dev->index, PRINT_ERROR, "vc: init buffer failed!\n");
		video_composer_uninit_buffer(dev);
	} else {
		dst_vf = get_dst_vframe_buffer(dev);
	}

	if (IS_ERR_OR_NULL(dst_vf)) {
		vc_print(dev->index, PRINT_PATTERN, "dst vf is NULL\n");
		return;
	}
	/*
	 * IIS_ERR_OR_NULL() is a function to determine
	 * whether it is empty or not, but script doesn't recognize it.
	 */
	/* coverity[var_deref_model:SUPPRESS] */
	memset(dst_vf, 0, sizeof(struct vframe_s));
	dst_buf = to_dst_buf(dst_vf);
	composer_info = &dst_buf->composer_info;
	memset(composer_info, 0, sizeof(struct composer_info_t));

	while (1) {
		if (!kfifo_get(&dev->receive_q, &received_frames)) {
			vc_print(dev->index, PRINT_ERROR, "com: get failed\n");
			return;
		}
		if (!kfifo_peek(&dev->receive_q, &received_frames_tmp))
			break;
		drop_count++;
		frames_put_file(dev, received_frames);
		vc_print(dev->index, PRINT_OTHER, "com: drop frame\n");
		atomic_set(&received_frames->on_use, false);
	}

	frames_info = &received_frames->frames_info;

	if (force_comp_w != 0) {
		frames_info->frame_info[0].dst_x = 0;
		frames_info->frame_info[0].dst_y = 0;
		frames_info->frame_info[0].dst_w = force_comp_w;
		frames_info->frame_info[0].dst_h = force_comp_h;
	}

	count = frames_info->frame_count;
	check_window_change(dev, &received_frames->frames_info);
	is_tvp = received_frames->is_tvp;
	if (is_tvp != dst_buf->is_tvp) {
		ret = need_switch_buffer(dst_buf, is_tvp, dev);
		if (ret == 0) {
			vc_print(dev->index, PRINT_ERROR,
				 "switch buffer from %s to %s failed\n",
				 dst_buf->is_tvp ? "tvp" : "non tvp",
				 is_tvp ? "tvp" : "non tvp");
			return;
		}
	}
	if (composer_use_444) {
		dev->ge2d_para.format = GE2D_FORMAT_S24_YUV444;
		dev->ge2d_para.plane_num = 1;
	} else {
		dev->ge2d_para.format = GE2D_FORMAT_M24_NV21;
		dev->ge2d_para.plane_num = 2;
	}
	dev->ge2d_para.is_tvp = is_tvp;
	dev->ge2d_para.phy_addr[0] = dst_buf->phy_addr;
	dev->ge2d_para.buffer_w = dst_buf->buf_w;
	dev->ge2d_para.buffer_h = dst_buf->buf_h;
	dev->ge2d_para.canvas0_addr = -1;

	if (dev->dev_choice == COMPOSER_WITH_GE2D) {
		if (dst_buf->dirty && !close_black) {
			ret = fill_vframe_black(&dev->ge2d_para);
			if (ret < 0)
				vc_print(dev->index, PRINT_ERROR, "ge2d fill black failed\n");
			else
				vc_print(dev->index, PRINT_OTHER, "fill black\n");
			dst_buf->dirty = false;
		}
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
	check_vicp_skip_mode(dev, frames_info, skip_mode);

	for (i = 0; i < count; i++) {
		file_vf = received_frames->file_vf[vf_dev[i]];
		vframe_info_cur = vframe_info[vf_dev[i]];
		is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
		is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);
		if (vframe_info[i]->transform != 0 || count != 1)
			need_dw = true;

		if (vframe_info[vf_dev[i]]->source_type == SOURCE_DTV_FIX_TUNNEL)
			is_fixtunnel = true;

		src_fmt = vframe_info[vf_dev[i]]->buffer_format;

		if (vframe_info[vf_dev[i]]->type == 1) {
			if (is_dec_vf || is_v4l_vf) {
				vc_print(dev->index, PRINT_OTHER,
					 "%s dma buffer is vf\n", __func__);
				scr_vf = get_vf_from_file(dev, file_vf, need_dw);
				if (!scr_vf) {
					vc_print(dev->index, PRINT_ERROR, "get vf NULL\n");
					continue;
				}
				if (scr_vf->type & VIDTYPE_V4L_EOS) {
					vc_print(dev->index, PRINT_ERROR, "eos vf\n");
					continue;
				}
			} else {
				addr = received_frames->phy_addr[vf_dev[i]];
				vc_print(dev->index, PRINT_OTHER,
					"%s dma buffer not vf\n", __func__);
			}
		} else if (vframe_info[vf_dev[i]]->type == 0) {
			if (is_dec_vf || is_v4l_vf) {
				vc_print(dev->index, PRINT_OTHER, "%s type 0 is vf\n", __func__);
				scr_vf = get_vf_from_file(dev, file_vf, need_dw);
			}
			if (!scr_vf) {
				vc_print(dev->index, PRINT_ERROR, "get vf NULL\n");
				continue;
			}

			if (scr_vf->type & VIDTYPE_V4L_EOS) {
				vc_print(dev->index, PRINT_ERROR, "eos vf\n");
				continue;
			}
		}

		transform = vframe_info[vf_dev[i]]->transform;

		crop_info.left = vframe_info[vf_dev[i]]->crop_x;
		crop_info.top = vframe_info[vf_dev[i]]->crop_y;
		crop_info.width = vframe_info[vf_dev[i]]->crop_w;
		crop_info.height = vframe_info[vf_dev[i]]->crop_h;

		dst_axis = output_axis_adjust(dev, vframe_info[vf_dev[i]]);
		display_axis.left = dst_axis.left * dst_buf->buf_w / dev->vinfo_w;
		display_axis.top = dst_axis.top * dst_buf->buf_h / dev->vinfo_h;
		display_axis.width = dst_axis.width * dst_buf->buf_w / dev->vinfo_w;
		display_axis.height = dst_axis.height * dst_buf->buf_h / dev->vinfo_h;
		vc_print(dev->index, PRINT_AXIS,
			"display_axis: left top width height: %d %d %d %d\n",
			display_axis.left, display_axis.top,
			display_axis.width, display_axis.height);
		if (min_left > dst_axis.left)
			min_left = dst_axis.left;
		if (min_top > dst_axis.top)
			min_top = dst_axis.top;
		if (max_right < (dst_axis.left + dst_axis.width))
			max_right = dst_axis.left + dst_axis.width;
		if (max_bottom < (dst_axis.top + dst_axis.height))
			max_bottom = dst_axis.top + dst_axis.height;

		input_vf[i] = scr_vf;
		out_axis[i] = display_axis;

		if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
			vc_print(dev->index, PRINT_OTHER, "use dewarp composer.\n");
			memset(&vframe_para, 0, sizeof(vframe_para));
			// coverity[var_deref_model] if scr_vf is null code will not run to here
			config_dewarp_vframe(dev->index, transform, scr_vf, dst_buf, &vframe_para);
			dev->dewarp_para.vf_para = &vframe_para;
			ret = load_dewarp_firmware(&dev->dewarp_para);
			if (ret != 0) {
				vc_print(dev->index, PRINT_ERROR, "load firmware failed.\n");
				break;
			}
			ret = dewarp_data_composer(&dev->dewarp_para);
			if (ret < 0)
				vc_print(dev->index, PRINT_ERROR, "dewarp composer failed\n");
		} else if (dev->dev_choice == COMPOSER_WITH_VICP) {
			vc_print(dev->index, PRINT_OTHER, "use vicp composer.\n");
			memset(&data_config, 0, sizeof(struct vicp_data_config_s));
			config_vicp_input_data(scr_vf,
					addr,
					dst_buf->buf_w,
					dst_buf->buf_w,
					dst_buf->buf_h,
					1,
					VICP_COLOR_FORMAT_YUV420,
					8,
					&data_config.input_data);

			if (vicp_output_dev == 1) {
				mifout_en = 1;
				fbcout_en = 0;
			} else if (vicp_output_dev == 2) {
				mifout_en = 0;
				fbcout_en = 1;
			} else {
				mifout_en = 1;
				fbcout_en = 1;
			}

			buf_addr[0] = (ulong)dst_buf->phy_addr;
			if (fbcout_en) {
				buf_addr[1] = dst_buf->afbc_head_addr;
				buf_addr[2] = dst_buf->afbc_table_addr;
			}

			if (count == 1) {
				fbc_init_ctrl = 1;
				fbc_pip_mode = 1;
			} else {
				if (i == 0) {
					fbc_init_ctrl = 1;
					fbc_pip_mode = 1;
				} else {
					fbc_init_ctrl = 0;
					fbc_pip_mode = 1;
				}
			}

			config_vicp_output_data(fbcout_en,
				mifout_en,
				buf_addr,
				dst_buf->buf_w,
				dst_buf->buf_w,
				dst_buf->buf_h,
				1,
				VICP_COLOR_FORMAT_YUV420,
				8,
				VICP_COLOR_FORMAT_YUV420,
				8,
				fbc_init_ctrl,
				fbc_pip_mode,
				&data_config.output_data);
			data_config.data_option.rotation_mode =
				map_rotationmode_from_vc_to_vicp(transform);
			data_config.data_option.crop_info.left = crop_info.left;
			data_config.data_option.crop_info.top = crop_info.top;
			data_config.data_option.crop_info.width = crop_info.width;
			data_config.data_option.crop_info.height = crop_info.height;
			data_config.data_option.output_axis.left = display_axis.left;
			data_config.data_option.output_axis.top = display_axis.top;
			data_config.data_option.output_axis.width = display_axis.width;
			data_config.data_option.output_axis.height = display_axis.height;
			data_config.data_option.shrink_mode =
				(enum vicp_shrink_mode_e)vicp_shrink_mode;

			if (count > 1)
				data_config.data_option.rdma_enable = true;
			else
				data_config.data_option.rdma_enable = false;
			data_config.data_option.input_source_count = count;
			data_config.data_option.input_source_number = i;
			data_config.data_option.security_enable = is_tvp;
			data_config.data_option.skip_mode = skip_mode[vf_dev[i]];

			ret = vicp_data_composer(&data_config);
			if (ret < 0)
				vc_print(dev->index, PRINT_ERROR, "vicp composer failed\n");
		} else {
			vc_print(dev->index, PRINT_OTHER, "use ge2d composer.\n");
			if (src_fmt == YUV444)
				src_data.is_yuv444 = true;
			else
				src_data.is_yuv444 = false;
			ret = config_ge2d_data(scr_vf,
				addr,
				vframe_info[vf_dev[i]]->buffer_w,
				vframe_info[vf_dev[i]]->buffer_h,
				vframe_info[vf_dev[i]]->reserved[0],
				vframe_info[vf_dev[i]]->reserved[1],
				crop_info.left,
				crop_info.top,
				crop_info.width,
				crop_info.height,
				&src_data);
			if (ret < 0)
				continue;

			dev->ge2d_para.angle = transform;
			dev->ge2d_para.position_left = display_axis.left;
			dev->ge2d_para.position_top = display_axis.top;
			dev->ge2d_para.position_width = display_axis.width;
			dev->ge2d_para.position_height = display_axis.height;

			ret = ge2d_data_composer(&src_data, &dev->ge2d_para);
			if (ret < 0)
				vc_print(dev->index, PRINT_ERROR, "ge2d composer failed\n");
		}
	}

	for (i = 0; i < count; i++) {
		if (!input_vf[i] || out_axis[i].width == 0 || out_axis[i].height == 0) {
			vc_print(dev->index, PRINT_ERROR, "invalid aiface param.\n");
			break;
		}

		aiface_info_temp = aiface_info_adjust(dev,
						received_frames->file_vf[vf_dev[i]],
						input_vf[i]->omx_index,
						out_axis[i].left,
						out_axis[i].top,
						out_axis[i].width,
						out_axis[i].height);
		if (aiface_info_temp) {
			vc_print(dev->index, PRINT_AIFACE,
				"omx_index = %d, aiface_value_count = %d.\n",
				input_vf[i]->omx_index, aiface_info_temp->aiface_value_count);
			if (!dev->aiface_buf) {
				size = sizeof(struct vf_aiface_t) * MAX_FACE_COUNT_PER_FRAME;
				dev->aiface_buf = vmalloc(size);
				if (!dev->aiface_buf) {
					vc_print(dev->index, PRINT_ERROR, "vmalloc failed.\n");
					break;
				}
			}

			for (num = 0; num < aiface_info_temp->aiface_value_count; num++)
				dev->aiface_buf->face_value[last_num + num] =
					aiface_info_temp->face_value[num];

			last_num += num;
			if (last_num >= MAX_FACE_COUNT_PER_FRAME) {
				vc_print(dev->index, PRINT_ERROR, "over max face count.\n");
				break;
			}
		} else {
			vc_print(dev->index, PRINT_AIFACE, "get aiface_info failed.\n");
		}
	}

	if (dev->aiface_buf) {
		dev->aiface_buf->aiface_value_count = last_num;
		dst_vf->vc_private = vc_private_q_pop(dev);
		dst_vf->vc_private->aiface_info = dev->aiface_buf;
		dst_vf->vc_private->flag |= VC_FLAG_AI_FACE;
	} else {
		vc_print(dev->index, PRINT_AIFACE, "aiface_info is NULL.\n");
	}

	frames_put_file(dev, received_frames);

	do_gettimeofday(&end_time);
	cost_time = (1000000 * (end_time.tv_sec - begin_time.tv_sec)
		+ (end_time.tv_usec - begin_time.tv_usec));
	vc_print(dev->index, PRINT_PERFORMANCE, "vframe composer cost: %d us\n", cost_time);

	dst_vf->flag |= (VFRAME_FLAG_VIDEO_COMPOSER | VFRAME_FLAG_COMPOSER_DONE);

	dst_vf->bitdepth = (BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8);

	if (!composer_use_444) {
		dst_vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
		dst_vf->type = (VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV21);
	} else {
		dst_vf->type = (VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD);
	}
	if (usage == UVM_USAGE_IMAGE_PLAY)
		dst_vf->type |= VIDTYPE_PIC;

	if (is_tvp)
		dst_vf->flag |= VFRAME_FLAG_VIDEO_SECURE;

	if (is_fixtunnel)
		dst_vf->flag |= VFRAME_FLAG_FIX_TUNNEL;

	if (vframe_info_cur->transform == VC_TRANSFORM_FLIP_H_ROT_90)
		dst_vf->flag |= VFRAME_FLAG_MIRROR_H;
	if (vframe_info_cur->transform == VC_TRANSFORM_FLIP_V_ROT_90)
		dst_vf->flag |= VFRAME_FLAG_MIRROR_V;

	if (debug_axis_pip) {
		dst_vf->axis[0] = 0;
		dst_vf->axis[1] = 0;
		dst_vf->axis[2] = 0;
		dst_vf->axis[3] = 0;
	} else {
		dst_vf->axis[0] = min_left;
		dst_vf->axis[1] = min_top;
		dst_vf->axis[2] = max_right - 1;
		dst_vf->axis[3] = max_bottom - 1;
	}
	composer_info->count = count;
	for (i = 0; i < count; i++) {
		composer_info->axis[i][0] = vframe_info[vf_dev[i]]->dst_x - dst_vf->axis[0];
		composer_info->axis[i][1] = vframe_info[vf_dev[i]]->dst_y - dst_vf->axis[1];
		composer_info->axis[i][2] = vframe_info[vf_dev[i]]->dst_w
			+ composer_info->axis[i][0] - 1;
		composer_info->axis[i][3] = vframe_info[vf_dev[i]]->dst_h
			+ composer_info->axis[i][1] - 1;
		vc_print(dev->index, PRINT_AXIS,
			 "alpha index=%d %d %d %d %d\n",
			 i,
			 composer_info->axis[i][0],
			 composer_info->axis[i][1],
			 composer_info->axis[i][2],
			 composer_info->axis[i][3]);
	}
	if (debug_crop_pip) {
		dst_vf->crop[0] = 0;
		dst_vf->crop[1] = 0;
		dst_vf->crop[2] = 0;
		dst_vf->crop[3] = 0;
	} else {
		if (dev->dev_choice != COMPOSER_WITH_DEWARP) {
			dst_vf->crop[0] = min_top * dst_buf->buf_h / dev->vinfo_h;
			dst_vf->crop[1] = min_left * dst_buf->buf_w / dev->vinfo_w;
			dst_vf->crop[2] = dst_buf->buf_h -
				max_bottom * dst_buf->buf_h / dev->vinfo_h;
			dst_vf->crop[3] = dst_buf->buf_w -
				max_right * dst_buf->buf_w / dev->vinfo_w;
		} else {
			vc_print(dev->index, PRINT_DEWARP, "dewarp no need crop.\n");
		}
	}
	vc_print(dev->index, PRINT_AXIS,
		 "min_top,min_left,max_bottom,max_right: %d %d %d %d\n",
		 min_top, min_left, max_bottom, max_right);

	if (scr_vf && count == 1 && dev->dev_choice != COMPOSER_WITH_VICP) {
		vc_print(dev->index, PRINT_OTHER,
			 "%s: copy hdr info.\n", __func__);
		dst_vf->src_fmt = scr_vf->src_fmt;
		dst_vf->signal_type = scr_vf->signal_type;
		dst_vf->source_type = scr_vf->source_type;
	}

	dst_vf->zorder = frames_info->disp_zorder;
	dst_vf->canvas0Addr = -1;
	dst_vf->canvas1Addr = -1;
	if (dev->dev_choice == COMPOSER_WITH_VICP) {
		if (fbcout_en) {
			dst_vf->type |= (VIDTYPE_COMPRESS | VIDTYPE_SCATTER);
			dst_vf->compWidth = dst_buf->buf_w;
			dst_vf->compHeight = dst_buf->buf_h;
			dst_vf->compHeadAddr = dst_buf->afbc_head_addr;
			dst_vf->compBodyAddr = dst_buf->afbc_body_addr;
		}

		if (vicp_shrink_mode >= VICP_SHRINK_MODE_MAX) {
			dst_vf->width = dst_buf->buf_w;
			dst_vf->height = dst_buf->buf_h;
		} else {
			dst_vf->width = dst_buf->buf_w >> (1 + vicp_shrink_mode);
			dst_vf->height = dst_buf->buf_h >> (1 + vicp_shrink_mode);
		}
	} else {
		dst_vf->width = dst_buf->buf_w;
		dst_vf->height = dst_buf->buf_h;
	}
	vc_print(dev->index, PRINT_DEWARP,
			 "composer:vf_w: %d, vf_h: %d\n", dst_vf->width, dst_vf->height);
	if (composer_use_444) {
		dst_vf->canvas0_config[0].phy_addr = dst_buf->phy_addr;
		dst_vf->canvas0_config[0].width = dst_vf->width * 3;
		dst_vf->canvas0_config[0].height = dst_vf->height;
		dst_vf->canvas0_config[0].block_mode = 0;
		dst_vf->plane_num = 1;
	} else {
		dst_vf->canvas0_config[0].phy_addr = dst_buf->phy_addr;
		dst_vf->canvas0_config[0].width = dst_vf->width;
		dst_vf->canvas0_config[0].height = dst_vf->height;
		dst_vf->canvas0_config[0].block_mode = 0;

		dst_vf->canvas0_config[1].phy_addr = dst_buf->phy_addr
			+ dst_vf->width * dst_vf->height;
		dst_vf->canvas0_config[1].width = dst_vf->width;
		dst_vf->canvas0_config[1].height = dst_vf->height >> 1;
		dst_vf->canvas0_config[1].block_mode = 0;
		dst_vf->plane_num = 2;
	}
	vc_print(dev->index, PRINT_DEWARP,
		"canvas0_addr: 0x%lx, canvas0_w: %d, canvas0_h: %d.\n",
		dst_vf->canvas0_config[0].phy_addr,
		dst_vf->canvas0_config[0].width,
		dst_vf->canvas0_config[0].height);
	vc_print(dev->index, PRINT_DEWARP,
		"canvas1_addr:  0x%lx, canvas1_w: %d, canvas1_h: %d.\n",
		dst_vf->canvas0_config[1].phy_addr,
		dst_vf->canvas0_config[1].width,
		dst_vf->canvas0_config[1].height);
	dst_vf->repeat_count = 0;
	dst_vf->composer_info = composer_info;

	if (dev->last_dst_vf)
		dev->last_dst_vf->repeat_count += drop_count;
	else
		dst_vf->repeat_count += drop_count;
	dev->last_dst_vf = dst_vf;
	dev->last_frames = *received_frames;
	dev->fake_vf = *dev->last_dst_vf;

	if (dump_vframe != dev->vframe_dump_flag) {
		if (scr_vf->type & VIDTYPE_COMPRESS)
			vd_vframe_afbc_soft_decode(scr_vf, 0);
		else
			dump_vf(dev->index, scr_vf, 0);

		if (dst_vf->type & VIDTYPE_COMPRESS)
			vd_vframe_afbc_soft_decode(dst_vf, 1);
		else
			dump_vf(dev->index, dst_vf, 1);

		dev->vframe_dump_flag = dump_vframe;
	}
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
	struct vframe_s *vf = NULL;

	vc_print(dev->index, PRINT_OTHER, "vc: empty ready_q len=%d\n",
		 kfifo_len(&dev->ready_q));

	while (kfifo_len(&dev->ready_q) > 0) {
		if (kfifo_get(&dev->ready_q, &vf)) {
			if (!vf)
				break;
			repeat_count = vf->repeat_count;
			omx_index = vf->omx_index;
			is_composer = vf->flag & VFRAME_FLAG_COMPOSER_DONE;
			file_vf = vf->file_vf;
			vc_print(dev->index, PRINT_OTHER,
				 "empty: repeat_count =%d, omx_index=%d\n",
				 repeat_count, omx_index);
			video_timeline_increase(dev, repeat_count + 1);
			if (!is_composer) {
				for (i = 0; i <= repeat_count; i++) {
					fput(file_vf);
					total_put_count++;
					dev->fput_count++;
				}
			} else {
				videocom_vf_put(vf, dev);
			}
		}
	}
}

static void video_wait_decode_fence(struct composer_dev *dev,
				    struct vframe_s *vf)
{
	if (vf && vf->fence) {
		u64 timestamp = local_clock();
		s32 ret = dma_fence_wait_timeout(vf->fence, false, 2000);

		vc_print(dev->index, PRINT_FENCE,
			 "%s, fence %lx, state: %d, wait cost time: %lld ns\n",
			 __func__, (ulong)vf->fence, ret,
			 local_clock() - timestamp);
		vf->fence = NULL;
	} else {
		vc_print(dev->index, PRINT_FENCE,
			 "decoder fence is NULL\n");
	}
}

static void video_wait_sr_fence(struct composer_dev *dev,
				    struct dma_fence *fence)
{
	if (fence) {
		u64 timestamp = local_clock();
		s32 ret = dma_fence_wait_timeout(fence, false, 2000);

		vc_print(dev->index, PRINT_FENCE,
			 "%s, sr fence %lx, state: %d, wait cost time:%lldns\n",
			 __func__, (ulong)fence, ret,
			 local_clock() - timestamp);
	} else {
		vc_print(dev->index, PRINT_FENCE,
			 "sr fence is NULL\n");
	}
}

static void video_wait_dalton_ready(struct composer_dev *dev,
				    struct vf_dalton_t *dalton_info)
{
	int wait_count = 0;
	u32 nn_status;

	while (1) {
		nn_status = dalton_info->nn_status;
		if (nn_status != NN_DONE &&
			(nn_status == NN_WAIT_DOING || nn_status == NN_START_DOING)) {
			usleep_range(2000, 2100);
			wait_count++;
			vc_print(dev->index, PRINT_PATTERN | PRINT_DALTON,
				"dalton wait count %d, nn_status=%d\n", wait_count, nn_status);
		} else {
			break;
		}
	}
	vc_print(dev->index, PRINT_PATTERN | PRINT_DALTON,
		 "dalton wait %dms nn_status=%d\n", 2 * wait_count, dalton_info->nn_status);
}

static void video_wait_aicolor_ready(struct composer_dev *dev,
				struct vf_aicolor_t *aicolor_info)
{
	int wait_count = 0;

	while (1) {
		if (aicolor_info->nn_status != NN_DONE &&
			(aicolor_info->nn_status == NN_WAIT_DOING ||
			aicolor_info->nn_status == NN_START_DOING)) {
			usleep_range(2000, 2100);
			wait_count++;
			vc_print(dev->index, PRINT_PATTERN | PRINT_AIFACE,
				"aicolor wait count %d, nn_status=%d\n",
				wait_count, aicolor_info->nn_status);
		} else {
			break;
		}
	}
	vc_print(dev->index, PRINT_PATTERN | PRINT_AIFACE,
		 "aicolor wait %dms nn_status=%d\n", 2 * wait_count, aicolor_info->nn_status);
}

static bool check_vf_has_afbc(struct composer_dev *dev, struct file *file_vf)
{
	struct vframe_s *vf = NULL;

	vf = get_vf_from_file(dev, file_vf, false);
	if (!vf)
		return false;

	if (vf->type & VIDTYPE_COMPRESS)
		return true;

	return false;
}

bool get_lowlatency_mode(void)
{
	return use_low_latency;
}
EXPORT_SYMBOL(get_lowlatency_mode);

static bool check_mosaic_22(struct composer_dev *dev, struct received_frames_t *received_frames)
{
	struct vinfo_s *video_composer_vinfo;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };
	int a[4];
	int i = 0;
	struct frames_info_t *f = &received_frames->frames_info;
	struct frame_info_t *frame_info;
	int half_w;
	int half_h;

	if (!dev->support_mosaic)
		return false;

	if (received_frames->frames_info.frame_count != 4)
		return false;

	if (dev->vinfo_w == 0) {
		video_composer_vinfo = get_current_vinfo();
		if (IS_ERR_OR_NULL(video_composer_vinfo))
			video_composer_vinfo = &vinfo;

		dev->vinfo_w = video_composer_vinfo->width;
		dev->vinfo_h = video_composer_vinfo->height;
	}

	if (dev->vinfo_w == 0 || dev->vinfo_h == 0)
		return false;

	half_w = dev->vinfo_w >> 1;
	half_h = dev->vinfo_h >> 1;

	for (i = 0; i < 4; i++) {
		frame_info = &f->frame_info[i];
		vc_print(dev->index, PRINT_AXIS,
			"check mosaic: i=%d: %d %d %d %d\n",
			i,
			frame_info->dst_x,
			frame_info->dst_y,
			frame_info->dst_w,
			frame_info->dst_h);
	}

	/*check all w h <= 1/2 vinfo*/
	for (i = 0; i < 4; i++) {
		if (f->frame_info[i].dst_w > half_w || f->frame_info[i].dst_h > half_h)
			return false;
		if (!check_vf_has_afbc(dev, received_frames->file_vf[i])) {
			vc_print(dev->index, PRINT_AXIS, "vf has no afbc\n");
			return false;
		}
	}

	for (i = 0; i < 4; i++) {
		if (f->frame_info[i].dst_x < half_w && f->frame_info[i].dst_y < half_h)
			a[0] = i;
		if (f->frame_info[i].dst_x >= half_w && f->frame_info[i].dst_y < half_h)
			a[1] = i;
		if (f->frame_info[i].dst_x < half_w && f->frame_info[i].dst_y >= half_h)
			a[2] = i;
		if (f->frame_info[i].dst_x >= half_w && f->frame_info[i].dst_y >= half_h)
			a[3] = i;
	}

	/*check quadrant 1*/
	frame_info = &f->frame_info[a[0]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w)
		return false;
	if (frame_info->dst_x % 8 != 0)
		return false;

	/*check quadrant 2*/
	frame_info = &f->frame_info[a[1]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w + dev->vinfo_w)
		return false;
	if ((frame_info->dst_x - half_w) % 8 != 0)
		return false;

	/*check quadrant 3*/
	frame_info = &f->frame_info[a[2]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w)
		return false;
	if (frame_info->dst_x % 8 != 0)
		return false;

	/*check quadrant 4*/
	frame_info = &f->frame_info[a[3]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w + dev->vinfo_w)
		return false;
	if ((frame_info->dst_x - half_w) % 8 != 0)
		return false;

	vc_print(dev->index, PRINT_AXIS, "check mosaic ok\n");

	return true;
}

static void video_composer_task(struct composer_dev *dev)
{
	struct vframe_s *vf = NULL;
	struct file *file_vf = NULL;
	struct frame_info_t *frame_info = NULL;
	struct received_frames_t *received_frames = NULL;
	struct frames_info_t *frames_info = NULL;
	int count;
	u32 frame_transform = 0;
	bool need_composer = false;
	int ready_count = 0;
	unsigned long phy_addr;
	u64 time_us64;
	struct vframe_s *vf_ext = NULL;
	u32 pic_w;
	u32 pic_h;
	bool is_dec_vf = false, is_v4l_vf = false, is_repeat_vf = false;
	struct vf_nn_sr_t *srout_data = NULL;
	struct video_composer_private *vc_private;
	u32 nn_status;
	u64 delay_time1;
	u64 delay_time2;
	u64 now_time;
	struct vd_prepare_s *vd_prepare = NULL;
	size_t usage = 0;
	struct vf_dalton_t *dalton_info = NULL;
	bool do_mosaic_22 = false;
	struct vf_aiface_t *aiface_info = NULL;
	struct vf_aicolor_t *aicolor_info = NULL;

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
		if ((dev->index == 0 && force_composer) ||
		    (dev->index == 1 && force_composer_pip))
			need_composer = true;
		frame_transform =
			received_frames->frames_info.frame_info[0].transform;
		if (frame_transform == VC_TRANSFORM_ROT_90 ||
			frame_transform == VC_TRANSFORM_ROT_180 ||
			frame_transform == VC_TRANSFORM_ROT_270 ||
			frame_transform == VC_TRANSFORM_FLIP_H_ROT_90 ||
			frame_transform == VC_TRANSFORM_FLIP_V_ROT_90) {
			need_composer = true;
			dev->need_rotate = true;
		}
	} else {
		if (check_mosaic_22(dev, received_frames)) {
			need_composer = false;
			do_mosaic_22 = true;
		} else {
			need_composer = true;
		}
	}
	if (!need_composer && !do_mosaic_22) {
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
		if (!file_vf) {
			vc_print(dev->index, PRINT_ERROR, "file_vf is NULL\n");
			return;
		}
		is_dec_vf =
		is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
		is_v4l_vf =
		is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);
		/*check repeat vframe*/
		if (dev->last_file == file_vf && (is_dec_vf || is_v4l_vf))
			is_repeat_vf = true;

		if (frame_info->type == 0) {
			if (is_dec_vf || is_v4l_vf)
				vf = get_vf_from_file(dev, file_vf, false);
			vc_print(dev->index, PRINT_OTHER,
				 "%s type 0 is vf\n", __func__);
			if (!vf) {
				vc_print(dev->index, PRINT_ERROR,
					 "%s get vf is NULL\n", __func__);
				return;
			}
			video_wait_decode_fence(dev, vf);
		} else if (frame_info->type == 1) {
			if (is_dec_vf || is_v4l_vf) {
				vf = get_vf_from_file(dev, file_vf, false);
				vc_print(dev->index, PRINT_OTHER,
					 "%s dma is vf\n", __func__);
				if (!vf) {
					vc_print(dev->index, PRINT_ERROR,
						 "%s get vf is NULL\n",
						 __func__);
					return;
				}
				video_wait_decode_fence(dev, vf);
			} else {
				vc_print(dev->index, PRINT_OTHER,
					 "%s dma buffer not vf\n", __func__);
			}
		}

		if (!kfifo_get(&dev->receive_q, &received_frames)) {
			vc_print(dev->index, PRINT_ERROR,
				 "task: get failed\n");
			return;
		}

		if (is_repeat_vf) {
			vd_prepare = dev->vd_prepare_last;
		} else {
			vd_prepare = vd_prepare_data_q_get(dev);
			if (!vd_prepare) {
				vc_print(dev->index, PRINT_ERROR,
					 "%s: get prepare_data failed.\n",
					 __func__);
				return;
			}

			if (is_dec_vf || is_v4l_vf) {
				if (!vf) {
					vc_print(dev->index, PRINT_ERROR,
						 "vf is NULL\n");
					return;
				}
				vd_prepare->src_frame = vf;
				vd_prepare->src_frame->file_vf = file_vf;
				vd_prepare->dst_frame = *vf;
			} else {/*dma buf*/
				vd_prepare->src_frame = &vd_prepare->dst_frame;
				vd_prepare->src_frame->file_vf = file_vf;
			}
		}

		vf = &vd_prepare->dst_frame;

		vf->axis[0] = frame_info->dst_x;
		vf->axis[1] = frame_info->dst_y;
		vf->axis[2] = frame_info->dst_w + frame_info->dst_x - 1;
		vf->axis[3] = frame_info->dst_h + frame_info->dst_y - 1;

		vc_print(dev->index, PRINT_AXIS,
			"frame_info crop: x y w h %d %d %d %d\n",
			frame_info->crop_x,
			frame_info->crop_y,
			frame_info->crop_w,
			frame_info->crop_h);
		if (is_dec_vf || is_v4l_vf) {
			if ((vf->type & VIDTYPE_COMPRESS) != 0) {
				if (is_src_crop_valid(vf->src_crop)) {
					pic_w = vf->compWidth -
						vf->src_crop.left - vf->src_crop.right;
					pic_h = vf->compHeight -
						vf->src_crop.top - vf->src_crop.bottom;
				} else {
					pic_w = vf->compWidth;
					pic_h = vf->compHeight;
				}
			} else {
				pic_w = vf->width;
				pic_h = vf->height;
			}

			if (frame_info->source_type == SOURCE_DTV_FIX_TUNNEL) {
				vf->flag |= VFRAME_FLAG_FIX_TUNNEL;
				vf->crop[0] = frame_info->crop_x;
				vf->crop[1] = frame_info->crop_y;
				vf->crop[2] = frame_info->crop_x +
					frame_info->crop_w;
				vf->crop[3] = frame_info->crop_y +
					frame_info->crop_h;
				vc_print(dev->index, PRINT_AXIS,
					"tunnel set vf crop:%d %d %d %d\n",
					vf->crop[0],
					vf->crop[1],
					vf->crop[2],
					vf->crop[3]);
			} else if (pic_w > frame_info->buffer_w ||
				pic_h > frame_info->buffer_h) {
			/*omx receive w*h is small than actual w*h;such as 8k*/
				vf->crop[0] = 0;
				vf->crop[1] = 0;
				vf->crop[2] = 0;
				vf->crop[3] = 0;
				vc_print(dev->index, PRINT_AXIS,
					"crop info is error!\n");
			} else {
				vf->crop[0] = frame_info->crop_y;
				vf->crop[1] = frame_info->crop_x;
				vf->crop[2] = pic_h
					- frame_info->crop_h
					- frame_info->crop_y;
				vf->crop[3] = pic_w
					- frame_info->crop_w
					- frame_info->crop_x;
				vc_print(dev->index, PRINT_AXIS,
					"none-tunnel set vf crop:%d %d %d %d\n",
					vf->crop[0],
					vf->crop[1],
					vf->crop[2],
					vf->crop[3]);
			}
		} else {
			if (frame_info->type == 1) {
				vf->crop[0] = frame_info->crop_y;
				vf->crop[1] = frame_info->crop_x;
				vf->crop[2] = frame_info->buffer_h
					- frame_info->crop_h
					- frame_info->crop_y;
				vf->crop[3] = frame_info->buffer_w
					- frame_info->crop_w
					- frame_info->crop_x;
			} else {
				vf->crop[0] = 0;
				vf->crop[1] = 0;
				vf->crop[2] = 0;
				vf->crop[3] = 0;
			}
		}
		vf->zorder = frames_info->disp_zorder;
		vf->file_vf = file_vf;
		//vf->zorder = 1;
		vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
			| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
		//mirror frame
		if (frame_transform == VC_TRANSFORM_FLIP_H)
			vf->flag |= VFRAME_FLAG_MIRROR_H;
		else if (frame_transform == VC_TRANSFORM_FLIP_V)
			vf->flag |= VFRAME_FLAG_MIRROR_V;

		vf->pts_us64 = time_us64;
		vf->disp_pts = 0;

		if (frame_info->type == 1 && !(is_dec_vf || is_v4l_vf)) {
			if (frame_info->source_type == SOURCE_HWC_CREAT_ION)
				vf->source_type = VFRAME_SOURCE_TYPE_HWC;
			vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER_DMA;
			vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
			vf->canvas0Addr = -1;
			vf->canvas0_config[0].phy_addr = phy_addr;
			if (frame_info->buffer_w > frame_info->reserved[0]) {
				if (frame_info->buffer_format == YUV444) {
					vf->canvas0_config[0].width = frame_info->buffer_w * 3;
					vc_print(dev->index, PRINT_OTHER,
						"frame_info->buffer_w * 3\n");
				} else {
					vf->canvas0_config[0].width = frame_info->buffer_w;
				}
				vc_print(dev->index, PRINT_PATTERN,
					 "buffer_w(%d) > deal_w(%d)\n",
					 frame_info->buffer_w,
					 frame_info->reserved[0]);
			} else {
				if (frame_info->buffer_format == YUV444) {
					vf->canvas0_config[0].width = frame_info->reserved[0] * 3;
					vc_print(dev->index, PRINT_OTHER,
						"frame_info->reserved[0] * 3\n");
				} else {
					vf->canvas0_config[0].width = frame_info->reserved[0];
				}
				vc_print(dev->index, PRINT_PATTERN,
					 "buffer_w: %d, deal_w: %d\n",
					 frame_info->buffer_w,
					 frame_info->reserved[0]);
			}
			if (frame_info->buffer_h > frame_info->reserved[1]) {
				vf->canvas0_config[0].height =
						frame_info->buffer_h;
				vc_print(dev->index, PRINT_PATTERN,
					 "buffer_h(%d) > deal_h(%d)\n",
					 frame_info->buffer_h,
					 frame_info->reserved[1]);
			} else {
				vf->canvas0_config[0].height =
						frame_info->reserved[1];
				vc_print(dev->index, PRINT_PATTERN,
					 "buffer_h: %d, deal_h: %d\n",
					 frame_info->buffer_h,
					 frame_info->reserved[1]);
			}
			vf->canvas1Addr = -1;
			vf->canvas0_config[1].phy_addr = phy_addr
				+ vf->canvas0_config[0].width
				* vf->canvas0_config[0].height;
			vf->canvas0_config[1].width =
				vf->canvas0_config[0].width;
			vf->canvas0_config[1].height =
				vf->canvas0_config[0].height;
			vf->width = frame_info->buffer_w;
			vf->height = frame_info->buffer_h;
			if (meson_uvm_get_usage(file_vf->private_data, &usage) < 0)
				vc_print(dev->index, PRINT_ERROR,
					"%s:meson_uvm_get_usage fail.\n", __func__);
			if (frame_info->buffer_format == YUV444) {
				vf->plane_num = 1;
				vf->type = VIDTYPE_VIU_SINGLE_PLANE
					| VIDTYPE_VIU_FIELD
					| VIDTYPE_VIU_444;
				vc_print(dev->index, PRINT_OTHER, "buffer_format_t YUV444\n");
			} else {
				vf->plane_num = 2;
				vf->type = VIDTYPE_PROGRESSIVE
					| VIDTYPE_VIU_FIELD
					| VIDTYPE_VIU_NV21;
			}
			if (usage == UVM_USAGE_IMAGE_PLAY)
				vf->type |= VIDTYPE_PIC;
			vf->bitdepth =
				BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
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
			 vf->canvas0_config[0].width,
			 vf->canvas0_config[0].height);
		vc_print(dev->index, PRINT_AXIS,
			 "===============================\n");

		if (is_dec_vf) {
			/* copy to uvm vf */
			vf_ext = vf->uvm_vf;
		} else if (is_v4l_vf) {
			if (vf->flag & VFRAME_FLAG_DOUBLE_FRAM) {
				vf_ext = vf->vf_ext;
				if (!vf_ext)
					vc_print(dev->index, PRINT_ERROR, "vf_ext is null\n");
			}
		}

		if (vf_ext) {
			vf_ext->axis[0] = vf->axis[0];
			vf_ext->axis[1] = vf->axis[1];
			vf_ext->axis[2] = vf->axis[2];
			vf_ext->axis[3] = vf->axis[3];
			vf_ext->crop[0] = vf->crop[0];
			vf_ext->crop[1] = vf->crop[1];
			vf_ext->crop[2] = vf->crop[2];
			vf_ext->crop[3] = vf->crop[3];
			vf_ext->zorder = vf->zorder;
			vf_ext->flag |= VFRAME_FLAG_VIDEO_COMPOSER
				| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
		}

		if (is_repeat_vf) {
			vf->repeat_count++;
			vc_print(dev->index, PRINT_FENCE,
				 "repeat =%d, omx_index=%d\n",
				 vf->repeat_count,
				 vf->omx_index);
		} else {
			if (is_dec_vf || is_v4l_vf) {
				if (vf->hf_info && !nn_bypass)
					srout_data =
						vc_get_hfout_data(dev, file_vf);
				if (srout_data)
					video_wait_sr_fence(dev, srout_data->fence);
				vc_private = vc_private_q_pop(dev);
				if (srout_data && vc_private && vf->hf_info) {
					nn_status = srout_data->nn_status;
					if (vf->hf_info->phy_addr != 0 &&
						vf->hf_info->width != 0 &&
						vf->hf_info->height != 0 &&
						(nn_status == NN_WAIT_DOING ||
						nn_status == NN_START_DOING ||
						nn_status == NN_DONE)) {
						vc_private->srout_data = srout_data;
						vc_private->flag |= VC_FLAG_AI_SR;
					}
				}
				vc_private->src_vf = vd_prepare->src_frame;
				vf->vc_private = vc_private;

				dalton_info = vc_get_dalton_info(dev, file_vf);
				if (dalton_info) {
					video_wait_dalton_ready(dev, dalton_info);
					vc_print(dev->index, PRINT_DALTON,
						"dalton: %d * %d, phy=%ld\n",
						dalton_info->width, dalton_info->height,
						dalton_info->phy_addr);
					dalton_info->nn_status = NN_DISPLAYED;
					vf->vc_private->dalton_info = dalton_info;
					vc_private->flag |= VC_FLAG_DALTON;
				}

				aiface_info = aiface_info_adjust(dev,
								file_vf,
								vf->omx_index,
								frame_info->dst_x,
								frame_info->dst_y,
								frame_info->dst_w,
								frame_info->dst_h);
				if (aiface_info) {
					vf->vc_private->aiface_info = aiface_info;
					vf->vc_private->flag |= VC_FLAG_AI_FACE;
					vc_print(dev->index, PRINT_AIFACE,
						"omx_index = %d, aiface_value_count = %d.\n",
						vf->omx_index, aiface_info->aiface_value_count);
				}

				aicolor_info = vc_get_aicolor_info(dev, file_vf);
				if (aicolor_info) {
					video_wait_aicolor_ready(dev, aicolor_info);
					if (aicolor_info->nn_status == NN_DONE) {
						vf->vc_private->aicolor_info = aicolor_info;
						aicolor_info->nn_status = NN_DISPLAYED;
						vc_private->flag |= VC_FLAG_AI_COLOR;
					}
				}
			}
			dev->last_file = file_vf;
			vf->repeat_count = 0;
			dev->vd_prepare_last = vd_prepare;
			if (vf->flag & VFRAME_FLAG_GAME_MODE) {
				now_time = ktime_to_us(ktime_get());
				delay_time1 = now_time - vf->disp_pts_us64;
				delay_time2 = now_time - vf->timestamp;
				vc_print(dev->index, PRINT_PATTERN,
						 "total: time1=%lld,  time2=%lld\n",
						 delay_time1, delay_time2);
				if (delay_time1 > 1000)
					vc_print(dev->index, PRINT_PATTERN,
						 "too time1=%lld, time2=%lld\n",
						 delay_time1, delay_time2);
			}
			video_dispaly_push_ready(dev, vf);
			if (!kfifo_put(&dev->ready_q,
				       (const struct vframe_s *)vf))
				vc_print(dev->index, PRINT_ERROR,
					 "by_pass ready_q is full\n");
			ready_count = kfifo_len(&dev->ready_q);
			/* dev->video_render_index == 5 means T7 dual screen mode */
			if (ready_count > 3 && dev->video_render_index == 5)
				vc_print(dev->index, PRINT_OTHER,
					 "ready len=%d\n", ready_count);
			else if (ready_count > 2 && dev->video_render_index != 5)
				vc_print(dev->index, PRINT_OTHER,
					 "ready len=%d\n", ready_count);
			else if (ready_count > 1 && dev->video_render_index != 5)
				vc_print(dev->index, PRINT_OTHER,
					 "ready len=%d\n", ready_count);
			vc_print(dev->index, PRINT_QUEUE_STATUS,
				 "ready len=%d\n", kfifo_len(&dev->ready_q));
		}
		dev->fake_vf = *vf;

		if (dump_vframe != dev->vframe_dump_flag) {
			if (vf->type & VIDTYPE_COMPRESS)
				vd_vframe_afbc_soft_decode(vf, 0);
			else
				dump_vf(dev->index, vf, 0);
			dev->vframe_dump_flag = dump_vframe;
		}

		atomic_set(&received_frames->on_use, false);
		if (use_low_latency && dev->index == 0)
			proc_lowlatency_frame(0);
	} else if (do_mosaic_22) {
		vframe_do_mosaic_22(dev);
		dev->last_file = NULL;
	} else {
		vframe_composer(dev);
		dev->last_file = NULL;
		dev->vd_prepare_last = NULL;
	}
}

static void video_composer_wait_event(struct composer_dev *dev)
{
	wait_event_interruptible_timeout(dev->wq,
					 (kfifo_len(&dev->receive_q) > 0 &&
					  dev->composer_enabled) ||
					 dev->need_free_buffer ||
					 dev->need_unint_receive_q ||
					 dev->need_empty_ready ||
					 dev->thread_need_stop,
					 msecs_to_jiffies(5000));
}

static int video_composer_thread(void *data)
{
	struct composer_dev *dev = data;

	vc_print(dev->index, PRINT_OTHER, "thread: started\n");

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
			dev->last_file = NULL;
			dev->fake_vf.flag |= VFRAME_FLAG_FAKE_FRAME;
			dev->fake_vf.vf_ext = NULL;
			dev->fake_vf.uvm_vf = NULL;
			dev->fake_back_vf = dev->fake_vf;
			if (!kfifo_put(&dev->ready_q,
				       &dev->fake_back_vf))
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
	// coverity[illegal_address] I'm ensure it is ok.
	struct composer_dev *dev;
	struct video_composer_port_s *port = &ports[iminor(inode)];
	int i;
	struct sched_param param = {.sched_priority = 2};
	u32 layer_cap = 0;

	pr_info("%s iminor(inode) =%d\n", __func__, iminor(inode));
	if (iminor(inode) >= video_composer_instance_num)
		return -ENODEV;

	mutex_lock(&video_composer_mutex);

	if (port->open_count > 0) {
		mutex_unlock(&video_composer_mutex);
		pr_err("video_composer: instance %d is aleady opened",
		       port->index);
		return -EBUSY;
	}

	dev = vmalloc(sizeof(*dev));
	memset(dev, 0, sizeof(*dev));
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
	dev->ge2d_para.plane_num = 2;

	dev->dewarp_para.vc_index = dev->index;
	dev->dewarp_para.context = NULL;
	dev->dewarp_para.fw_load.size_32bit = 0;
	dev->dewarp_para.fw_load.phys_addr = 0;
	dev->dewarp_para.fw_load.virt_addr = NULL;
	dev->dewarp_para.vf_para = NULL;

	dev->buffer_status = UNINITIAL;

	dev->port = port;
	file->private_data = dev;
	dev->index = port->index;
	dev->need_free_buffer = false;
	dev->last_frames.frames_info.frame_count = 0;
	dev->is_sideband = false;
	dev->need_empty_ready = false;
	dev->thread_need_stop = false;
	dev->vframe_dump_flag = 0;

	memcpy(dev->vf_provider_name, port->name,
	       strlen(port->name) + 1);
	dev->video_render_index = vd_render_index_get(dev);
	port->video_render_index = dev->video_render_index;
	port->open_count++;
	do_gettimeofday(&dev->start_time);

	mutex_unlock(&video_composer_mutex);
	dev->kthread = kthread_create(video_composer_thread,
				      dev, dev->port->name);
	if (IS_ERR(dev->kthread)) {
		pr_err("video_composer_thread creat failed\n");
		return -ENOMEM;
	}
	init_waitqueue_head(&dev->wq);
	if (sched_setscheduler(dev->kthread, SCHED_FIFO, &param))
		pr_err("vc:Could not set realtime priority.\n");

	wake_up_process(dev->kthread);
	//mutex_init(&dev->mutex_input);

	for (i = 0; i < FRAMES_INFO_POOL_SIZE; i++)
		dev->received_frames[i].index = i;

	video_timeline_create(dev);

	if (dev->index == 0) {
		layer_cap = video_get_layer_capability();
		if (layer_cap & MOSAIC_MODE)
			dev->support_mosaic = true;
	}

	return 0;
}

static int video_composer_release(struct inode *inode, struct file *file)
{
	struct composer_dev *dev = file->private_data;
	struct video_composer_port_s *port = dev->port;
	int i = 0;
	int ret = 0;

	pr_info("%s enable=%d\n", __func__, dev->enable_composer);

	if (iminor(inode) >= video_composer_instance_num)
		return -ENODEV;

	if (dev->enable_composer) {
		ret = video_composer_set_enable(dev, 0);
		if (ret != 0)
			pr_err("%s, disable fail\n", __func__);
	}

	if (dev->kthread) {
		dev->thread_need_stop = true;
		kthread_stop(dev->kthread);
		wake_up_interruptible(&dev->wq);
		dev->kthread = NULL;
		dev->thread_need_stop = false;
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
	vfree(dev);
	dev = NULL;
	return 0;
}

static void disable_video_layer(struct composer_dev *dev, int val)
{
	pr_info("dev->index =%d, val=%d", dev->index, val);
	if (dev->index == 0)
		_video_set_disable(val);
	else
		_videopip_set_disable(dev->index, val);
}

static void set_frames_info(struct composer_dev *dev,
			    struct frames_info_t *frames_info)
{
	u32 fence_fd;
	int i = 0;
	int j = 0;
	int type = -1;
	struct file *file_vf = NULL;
	struct vframe_s *vf = NULL;
	struct timeval time1;
	struct timeval time2;
	u64 time_us64;
	int axis[4];
	int ready_len = 0;
	bool current_is_sideband = false;
	bool is_dec_vf = false, is_v4l_vf = false;
	s32 sideband_type = -1;
	bool is_tvp = false;
	bool need_dw = false;
	char render_layer[16] = "";
	struct file *fence_file = NULL;
	struct sync_file *sync_file = NULL;
	struct dma_fence *fence_obj = NULL;

	if (!frames_info ||
	    frames_info->frame_count <= 0 ||
	    frames_info->frame_count > MXA_LAYER_COUNT) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s: param is invalid.\n",
			 __func__);
		return;
	}

	if (!dev->composer_enabled) {
		for (j = 0; j < frames_info->frame_count; j++)
			frames_info->frame_info[j].composer_fen_fd = -1;
		vc_print(dev->index, PRINT_ERROR,
			 "set frame but not enable\n");
		return;
	}

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
			sideband_type = frames_info->frame_info[j].sideband_type;
			axis[0] = frames_info->frame_info[j].dst_x;
			axis[1] = frames_info->frame_info[j].dst_y;
			axis[2] = frames_info->frame_info[j].dst_w
				+ axis[0] - 1;
			axis[3] = frames_info->frame_info[j].dst_h
				+ axis[1] - 1;
			set_video_window_ext(dev->index, axis);
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
			if (dev->index == 0) {
				set_video_path_select("auto", 0);
				set_sideband_type(sideband_type, 0);
			}
		}
		vc_print(dev->index, PRINT_ERROR,
			 "sideband_type =%d\n", sideband_type);
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
		disable_video_layer(dev, 0);
		sprintf(render_layer,
			"video_render.%d",
			dev->video_render_index);
		set_video_path_select(render_layer, dev->index);
	}
	dev->is_sideband = false;

	time1 = dev->start_time;
	do_gettimeofday(&time2);
	time_us64 = (u64)1000000 * (time2.tv_sec - time1.tv_sec)
			+ time2.tv_usec - time1.tv_usec;

	/*time_vsync = (u64)1000000 * (time2.tv_sec - vsync_time.tv_sec)*/
	/*+ time2.tv_usec - vsync_time.tv_usec;*/

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
	if (fence_fd >= 0) {
		fence_file = fget(fence_fd);
		vc_print(dev->index, PRINT_FENCE,
				"create fence_fd=%d, fence_file=%px\n",
				fence_fd, fence_file);

		if (!IS_ERR_OR_NULL(fence_file))
			sync_file = (struct sync_file *)fence_file->private_data;
		else
			vc_print(dev->index, PRINT_FENCE, "fence_file is NULL\n");

		if (!IS_ERR_OR_NULL(sync_file))
			fence_obj = sync_file->fence;
		else
			vc_print(dev->index, PRINT_FENCE, "sync_file is NULL\n");

		if (fence_obj) {
			vc_print(dev->index, PRINT_FENCE, "sync_file=%px, seqno=%lld\n",
					sync_file, fence_obj->seqno);
		}
		if (!IS_ERR_OR_NULL(fence_file))
			fput(fence_file);
	}

	if (transform) {
		for (j = 0; j < frames_info->frame_count; j++)
			frames_info->frame_info[j].transform = transform;
	}

	dev->received_frames[i].frames_info = *frames_info;
	dev->received_frames[i].frames_num = dev->received_count;
	dev->received_frames[i].time_us64 = time_us64;

	vc_print(dev->index, PRINT_PERFORMANCE,
		 "len =%d,frame_count=%d,i=%d,z=%d,time_us64=%lld,fd=%d, transform=%d\n",
		 kfifo_len(&dev->receive_q),
		 frames_info->frame_count,
		 i,
		 frames_info->disp_zorder,
		 time_us64,
		 fence_fd,
		 frames_info->frame_info[0].transform);

	for (j = 0; j < frames_info->frame_count; j++) {
		frames_info->frame_info[j].composer_fen_fd = fence_fd;
		file_vf = fget(frames_info->frame_info[j].fd);
		if (!file_vf) {
			vc_print(dev->index, PRINT_ERROR, "fget fd fail\n");
			return;
		}
		total_get_count++;
		dev->received_frames[i].file_vf[j] = file_vf;
		type = frames_info->frame_info[j].type;
		is_dec_vf =
		is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
		is_v4l_vf =
		is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

		vc_print(dev->index, PRINT_FENCE, "receive:file=%px, dma=%px\n",
			 file_vf,
			 file_vf->private_data);

		if (frames_info->frame_info[j].transform != 0 ||
			frames_info->frame_count != 1)
			need_dw = true;
		if (type == 0 || type == 1) {
			vc_print(dev->index, PRINT_FENCE,
				 "received_cnt=%lld,new_cnt=%lld,i=%d,z=%d,DMA_fd=%d\n",
				 dev->received_count + 1,
				 dev->received_new_count + 1,
				 i,
				 frames_info->frame_info[j].zorder,
				 frames_info->frame_info[j].fd);
			if (!(is_dec_vf || is_v4l_vf)) {
				if (type == 0) {
					vc_print(dev->index, PRINT_ERROR,
						 "%s type is %d but not vf\n",
						 __func__, type);
					return;
				}
				dev->received_frames[i].phy_addr[j] =
				get_dma_phy_addr(frames_info->frame_info[j].fd,
						 dev->index);
				vc_print(dev->index, PRINT_OTHER,
					 "%s dma buffer not vf\n", __func__);
				continue;
			}
			vf = get_vf_from_file(dev, file_vf, need_dw);
			vc_print(dev->index, PRINT_OTHER,
				 "%s type is %d and get vf\n",
				 __func__, type);

			if (!vf) {
				vc_print(dev->index, PRINT_ERROR,
					 "received NULL vf!!\n");
				return;
			}
			if (((reset_drop >> dev->index) & 1) ||
			    last_index[dev->index][j] > vf->omx_index) {
				dev->received_new_count = vf->omx_index;
				dev->received_count = vf->omx_index;
				vpp_drop_count = 0;
				reset_drop ^= 1 << dev->index;
				vc_print(dev->index, PRINT_PATTERN,
					 "drop cnt reset!!\n");
			}

			if (last_index[dev->index][j] != vf->omx_index) {
				dev->received_new_count++;
				last_index[dev->index][j] = vf->omx_index;
			}

			if (dev->index == 0) {
				drop_cnt = vf->omx_index + 1
					    - dev->received_new_count;
#ifdef CONFIG_AMLOGIC_DEBUG_ATRACE
				if (drop_cnt == 0)
					ATRACE_COUNTER("video_composer_drop_cnt", 0);
				if (drop_cnt != dev->last_drop_cnt) {
					dev->last_drop_cnt = drop_cnt;
					ATRACE_COUNTER("video_composer_drop_cnt", drop_cnt);
					ATRACE_COUNTER("video_composer_drop_cnt", 0);
				}
#endif
				receive_new_count = dev->received_new_count;
				receive_count = dev->received_count + 1;
				last_omx_index = vf->omx_index;
			} else if (dev->index == 1) {
				drop_cnt_pip = vf->omx_index + 1
						- dev->received_new_count;
				receive_new_count_pip = dev->received_new_count;
				receive_count_pip = dev->received_count + 1;
				last_omx_index = vf->omx_index;
			}

			if (!is_tvp) {
				if (vf->flag & VFRAME_FLAG_VIDEO_SECURE)
					is_tvp = true;
			}
			if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
				vf->source_type == VFRAME_SOURCE_TYPE_CVBS)
				tv_fence_creat_count++;
			vc_print(dev->index, PRINT_FENCE | PRINT_PATTERN,
				 "received_cnt=%lld,new_cnt=%lld,i=%d,z=%d,omx_index=%d, fence_fd=%d, fc_no=%d, index_disp=%d,pts=%lld, vf=%p\n",
				 dev->received_count + 1,
				 dev->received_new_count,
				 i,
				 frames_info->frame_info[j].zorder,
				 vf->omx_index,
				 fence_fd,
				 dev->cur_streamline_val,
				 vf->index_disp,
				 vf->pts_us64,
				 vf);
#ifdef CONFIG_AMLOGIC_DEBUG_ATRACE
			ATRACE_COUNTER("video_composer_sf_omx_index", vf->omx_index);
			ATRACE_COUNTER("video_composer_sf_omx_index", 0);
#endif
		} else {
			vc_print(dev->index, PRINT_ERROR,
				 "unsupport type=%d\n",
				 frames_info->frame_info[j].type);
		}
	}
	dev->received_frames[i].is_tvp = is_tvp;
	atomic_set(&dev->received_frames[i].on_use, true);
	dev->received_count++;

	if (!kfifo_put(&dev->receive_q, &dev->received_frames[i]))
		vc_print(dev->index, PRINT_ERROR, "put ready fail\n");
	wake_up_interruptible(&dev->wq);

	//vc_print(dev->index, PRINT_PERFORMANCE, "set_frames_info_out\n");
}

static int video_composer_init(struct composer_dev *dev)
{
	int ret;
	int i, j;
	char render_layer[16] = "";

	if (!dev)
		return -1;

	INIT_KFIFO(dev->ready_q);
	INIT_KFIFO(dev->receive_q);
	INIT_KFIFO(dev->free_q);
	INIT_KFIFO(dev->display_q);
	INIT_KFIFO(dev->vc_prepare_data_q);
	kfifo_reset(&dev->ready_q);
	kfifo_reset(&dev->receive_q);
	kfifo_reset(&dev->free_q);
	kfifo_reset(&dev->display_q);
	kfifo_reset(&dev->vc_prepare_data_q);

	for (i = 0; i < COMPOSER_READY_POOL_SIZE; i++)
		vd_prepare_data_q_put(dev, &dev->vd_prepare[i]);

	vc_private_q_init(dev);

	dev->received_count = 0;
	dev->received_new_count = 0;
	dev->fence_creat_count = 0;
	dev->fence_release_count = 0;
	dev->fput_count = 0;
	dev->last_dst_vf = NULL;
	dev->drop_frame_count = 0;
	dev->is_sideband = false;
	dev->need_empty_ready = false;
	dev->last_file = NULL;
	dev->select_path_done = false;
	dev->vd_prepare_last = NULL;
	dev->dev_choice = COMPOSER_WITH_GE2D;
	dev->aiface_buf = NULL;
	init_completion(&dev->task_done);
	for (i = 0; i < MAX_VD_LAYERS; i++) {
		for (j = 0; j < MXA_LAYER_COUNT; j++)
			last_index[i][j] = -1;
	}
	last_omx_index = -1;
	disable_video_layer(dev, 2);
	video_set_global_output(dev->index, 1);

	ret = video_display_create_path(dev);
	sprintf(render_layer, "video_render.%d", dev->video_render_index);
	set_video_path_select(render_layer, dev->index);

	return ret;
}

static int video_composer_uninit(struct composer_dev *dev)
{
	int ret;
	int time_left = 0;

	if (dev->is_sideband) {
		if (dev->index == 0) {
			set_video_path_select("auto", 0);
			set_blackout_policy(1);
		}
	} else {
		if (dev->index == 0) {
			set_video_path_select("default", 0);
			set_blackout_policy(1);
		}
	}

	disable_video_layer(dev, 1);
	video_set_global_output(dev->index, 0);
	ret = video_display_release_path(dev);

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
			 "uninit: received=%lld, new_cnt=%lld, fput=%lld, drop=%d\n",
			 dev->received_count,
			 dev->received_new_count,
			 dev->fput_count,
			 dev->drop_frame_count);
	}
	video_timeline_increase(dev,
				dev->fence_creat_count
				- dev->fence_release_count);
	dev->is_sideband = false;
	dev->need_empty_ready = false;
	video_display_para_reset(dev->index);

	if (dev->aiface_buf) {
		vfree(dev->aiface_buf);
		dev->aiface_buf = NULL;
	}

	if (dev->dev_choice == COMPOSER_WITH_VICP)
		vicp_process_enable(0);
	return ret;
}

int video_composer_set_enable(struct composer_dev *dev, u32 val)
{
	int ret = 0;

	if (val > VIDEO_COMPOSER_ENABLE_NORMAL)
		return -EINVAL;

	if (val == 0)
		dev->composer_enabled = false;

	vc_print(dev->index, PRINT_ERROR,
		 "vc: set enable index=%d, val=%d\n",
		 dev->index, val);

	if (dev->enable_composer == val) {
		pr_err("vc: set_enable repeat set dev->index =%d,val=%d\n",
		       dev->index, val);
		return ret;
	}
	dev->enable_composer = val;

	if (val == VIDEO_COMPOSER_ENABLE_NORMAL) {
		ret = video_composer_init(dev);
	} else if (val == VIDEO_COMPOSER_ENABLE_NONE) {
		wake_up_interruptible(&dev->wq);
		ret = video_composer_uninit(dev);
	}

	if (ret != 0)
		pr_err("vc: set failed\n");
	else
		if (val)
			dev->composer_enabled = true;
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
	struct capability_info_t capability_info;
	u32 w;
	u32 h;

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
	case VIDEO_COMPOSER_IOCTL_GET_PANEL_CAPABILITY:
		val = video_get_layer_capability();
		ret = copy_to_user(argp, &val, sizeof(u32));
		break;
	case VIDEO_COMPOSER_IOCTL_GET_LAYER_CAPABILITY:
		memset(&capability_info, 0, sizeof(struct capability_info_t));
		capability_info.capability = video_get_layer_capability();
		get_video_src_min_buffer(dev->index, &w, &h);
		capability_info.min_w = w;
		capability_info.min_h = h;
		get_video_src_max_buffer(dev->index, &w, &h);
		capability_info.max_w = w;
		capability_info.max_h = h;
		vc_print(dev->index, PRINT_ERROR,
			"get capability: min %d %d; max %d %d\n",
			 capability_info.min_w, capability_info.min_h, w, h);
		ret = copy_to_user(argp, &capability_info, sizeof(struct capability_info_t));
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

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	if (token) {
		len = strlen(token);
		do {
			token = strsep(&params, " ");
			if (!token)
				break;
			while (token &&
			       (isspace(*token) ||
				!isgraph(*token)) && len) {
				token++;
				len--;
			}
			if (len == 0)
				break;
			ret = kstrtoint(token, 0, &res);
			if (ret < 0)
				break;
			len = strlen(token);
			*out++ = res;
			count++;
		} while ((count < para_num) && (len > 0));
	}

	kfree(params_base);
	return count;
}

static ssize_t debug_crop_pip_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	return snprintf(buf, 80,
			"current debug_crop_pip is %d\n",
			debug_crop_pip);
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
	debug_crop_pip = tmp;
	return count;
}

static ssize_t debug_axis_pip_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	return snprintf(buf, 80,
			"current debug_axis_pip is %d\n",
			debug_axis_pip);
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
	debug_axis_pip = tmp;
	return count;
}

static ssize_t dump_vframe_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	return snprintf(buf, 80,
			"dump_vframe: %d.\n",
			dump_vframe);
}

static ssize_t dump_vframe_store(struct class *cla,
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
	dump_vframe = tmp;
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

static ssize_t dewarp_rotate_width_show(struct class *cla,
				 struct class_attribute *attr,
				 char *buf)
{
	return snprintf(buf, 80,
			"current dewarp_rotate_width is %d\n",
			dewarp_rotate_width);
}

static ssize_t dewarp_rotate_width_store(struct class *cla,
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
	dewarp_rotate_width = tmp;
	return count;
}

static ssize_t dewarp_rotate_height_show(struct class *cla,
				 struct class_attribute *attr,
				 char *buf)
{
	return snprintf(buf, 80,
			"current dewarp_rotate_height is %d\n",
			dewarp_rotate_height);
}

static ssize_t dewarp_rotate_height_store(struct class *cla,
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
	dewarp_rotate_height = tmp;
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

static ssize_t composer_use_444_show(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", composer_use_444);
}

static ssize_t composer_use_444_store(struct class *class,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	composer_use_444 = val;
	pr_info("set composer_use_444:%d\n", composer_use_444);
	return count;
}

static ssize_t reset_drop_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "reset_drop: %d\n", reset_drop);
}

static ssize_t reset_drop_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	reset_drop = val;
	return count;
}

static ssize_t drop_cnt_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	return sprintf(buf,
		"rec_cnt: %d, omx_index: %d, valid_cnt: %d, player_drop_cnt: %d, vpp_drop_cnt: %d, total_drop_cnt: %d\n",
		receive_count,
		last_omx_index,
		receive_new_count,
		drop_cnt,
		vpp_drop_count,
		drop_cnt + vpp_drop_count);
}

static ssize_t drop_cnt_pip_show(struct class *class,
				 struct class_attribute *attr,
				 char *buf)
{
	return sprintf(buf,
		"pip_cnt:%d,omx_index:%d,valid_cnt_pip:%d,drop_cnt_pip:%d\n",
		receive_count_pip,
		last_omx_index,
		receive_new_count_pip,
		drop_cnt_pip);
}

static ssize_t receive_count_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", receive_count);
}

static ssize_t receive_count_pip_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", receive_count_pip);
}

static ssize_t receive_new_count_show(struct class *class,
				      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", receive_new_count);
}

static ssize_t receive_new_count_pip_show(struct class *class,
					  struct class_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "%d\n", receive_new_count_pip);
}

static ssize_t total_get_count_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_get_count);
}

static ssize_t total_put_count_show(struct class *class,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_put_count);
}

static ssize_t nn_need_time_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "nn_need_time: %lld\n", nn_need_time);
}

static ssize_t nn_need_time_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	nn_need_time = val;
	return count;
}

static ssize_t nn_margin_time_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "nn_margin_time: %lld\n", nn_margin_time);
}

static ssize_t nn_margin_time_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	nn_margin_time = val;
	return count;
}

static ssize_t nn_bypass_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "nn_bypass: %d\n", nn_bypass);
}

static ssize_t nn_bypass_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	nn_bypass = val;
	return count;
}

static ssize_t tv_fence_creat_count_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "tv_fence_creat_count: %d\n", tv_fence_creat_count);
}

static ssize_t vd_pulldown_level_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "vd_pulldown_level: %d\n", vd_pulldown_level);
}

static ssize_t vd_pulldown_level_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	vd_pulldown_level = val;
	return count;
}

static ssize_t vd_max_hold_count_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "vd_max_hold_count: %d\n", vd_max_hold_count);
}

static ssize_t vd_max_hold_count_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	vd_max_hold_count = val;
	return count;
}

static ssize_t vd_set_frame_delay_show(struct class *cla,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80, "vd_set_frame_delay: %d,%d,%d\n",
		vd_set_frame_delay[0], vd_set_frame_delay[1],
		vd_set_frame_delay[2]);
}

static ssize_t vd_set_frame_delay_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	if (likely(parse_para(buf, MAX_VIDEO_COMPOSER_INSTANCE_NUM,
		vd_set_frame_delay) == MAX_VIDEO_COMPOSER_INSTANCE_NUM))
		return strnlen(buf, count);

	return -EINVAL;
}

static ssize_t vd_dump_vframe_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "vd_dump_vframe: %d\n", vd_dump_vframe);
}

static ssize_t vd_dump_vframe_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	vd_dump_vframe = val;
	if (vd_dump_vframe == 1 && current_display_vf != NULL) {
		ext_controls();
		vd_dump_vframe = 0;
	}
	return count;
}

static ssize_t actual_delay_count_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	int val = (actual_delay_count[0] | (actual_delay_count[1] << 4)
		| (actual_delay_count[2] << 8));
	return sprintf(buf, "%d\n", val);
}

static ssize_t vicp_output_dev_show(struct class *cla, struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80,
		"1 mif, 2 fbc, 3 mif+fbc. current choice is %d.\n", vicp_output_dev);
}

static ssize_t vicp_output_dev_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vicp_output_dev = tmp;
	return count;
}

static ssize_t vicp_shrink_mode_show(struct class *cla, struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80,
		"0 2x, 1 4x, 2 8x. current choice is %d.\n", vicp_shrink_mode);
}

static ssize_t vicp_shrink_mode_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vicp_shrink_mode = tmp;
	return count;
}

static ssize_t vicp_max_width_show(struct class *cla, struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "current vicp_max_width is %d.\n", vicp_max_width);
}

static ssize_t vicp_max_width_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vicp_max_width = tmp;
	return count;
}

static ssize_t vicp_max_height_show(struct class *cla, struct class_attribute *attr,
	char *buf)
{
	return snprintf(buf, 80, "current vicp_max_height is %d.\n", vicp_max_height);
}

static ssize_t vicp_max_height_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vicp_max_height = tmp;
	return count;
}

static ssize_t composer_dev_choice_show(struct class *cla, struct class_attribute *attr,
	char *buf)
{
	return snprintf(buf, 80,
		"1 ge2d, 2 dewarp, 3 vicp. current choice is %d.\n", composer_dev_choice);
}

static ssize_t composer_dev_choice_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	composer_dev_choice = tmp;
	return count;
}

static ssize_t force_comp_w_show(struct class *cla, struct class_attribute *attr,
	char *buf)
{
	return snprintf(buf, 80,
		"force_comp_w %d.\n", force_comp_w);
}

static ssize_t force_comp_w_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	force_comp_w = tmp;
	return count;
}

static ssize_t force_comp_h_show(struct class *cla, struct class_attribute *attr,
	char *buf)
{
	return snprintf(buf, 80,
		"force_comp_h %d.\n", force_comp_h);
}

static ssize_t force_comp_h_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	force_comp_h = tmp;
	return count;
}

static ssize_t vd_test_fps_store(struct class *cla, struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	vd_test_fps = tmp;
	return count;
}

static ssize_t vd_test_fps_show(struct class *cla, struct class_attribute *attr,
	char *buf)
{
	u64 fps_h, vsync_h = 0;
	u32 fps_l, vsync_l = 0;

	fps_h = div_s64_rem(vd_test_fps_val[0], 100000, &fps_l);
	vsync_h = div_s64_rem(vd_test_vsync_val[0], 100000, &vsync_l);

	return snprintf(buf, 80,
		"test_fps_val=%llu.%u, test_vsyn_val=%llu.%u\n",
		fps_h, fps_l, vsync_h, vsync_l);
}

static ssize_t vd_test_fps_pip_store(struct class *cla, struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	vd_test_fps_pip = tmp;
	return count;
}

static ssize_t vd_test_fps_pip_show(struct class *cla, struct class_attribute *attr,
	char *buf)
{
	u64 fps_h, vsync_h = 0;
	u32 fps_l, vsync_l = 0;

	fps_h = div_s64_rem(vd_test_fps_val[1], 100000, &fps_l);
	vsync_h = div_s64_rem(vd_test_vsync_val[1], 100000, &vsync_l);

	return snprintf(buf, 80,
		"test_fps_pip_val=%llu.%u, test_vsyn_pip_val=%llu.%u\n",
		fps_h, fps_l, vsync_h, vsync_l);
}

static ssize_t dewarp_load_flag_show(struct class *cla, struct class_attribute *attr,
	char *buf)
{
	return snprintf(buf, 80, "current dewarp_load_flag is %d.\n", dewarp_load_flag);
}

static ssize_t dewarp_load_flag_store(struct class *cla, struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	pr_info("set dewarp_load_flag to %ld.\n", tmp);
	dewarp_load_flag = tmp;
	return count;
}

static CLASS_ATTR_RW(debug_axis_pip);
static CLASS_ATTR_RW(debug_crop_pip);
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
static CLASS_ATTR_RW(dewarp_rotate_width);
static CLASS_ATTR_RW(dewarp_rotate_height);
static CLASS_ATTR_RW(close_black);
static CLASS_ATTR_RW(composer_use_444);
static CLASS_ATTR_RW(reset_drop);
static CLASS_ATTR_RO(drop_cnt);
static CLASS_ATTR_RO(drop_cnt_pip);
static CLASS_ATTR_RO(receive_count);
static CLASS_ATTR_RO(receive_count_pip);
static CLASS_ATTR_RO(receive_new_count);
static CLASS_ATTR_RO(receive_new_count_pip);
static CLASS_ATTR_RO(total_get_count);
static CLASS_ATTR_RO(total_put_count);
static CLASS_ATTR_RW(nn_need_time);
static CLASS_ATTR_RW(nn_margin_time);
static CLASS_ATTR_RW(nn_bypass);
static CLASS_ATTR_RO(tv_fence_creat_count);
static CLASS_ATTR_RW(dump_vframe);
static CLASS_ATTR_RW(vd_pulldown_level);
static CLASS_ATTR_RW(vd_max_hold_count);
static CLASS_ATTR_RW(vd_set_frame_delay);
static CLASS_ATTR_RW(vd_dump_vframe);
static CLASS_ATTR_RO(actual_delay_count);
static CLASS_ATTR_RW(vicp_output_dev);
static CLASS_ATTR_RW(vicp_shrink_mode);
static CLASS_ATTR_RW(vicp_max_width);
static CLASS_ATTR_RW(vicp_max_height);
static CLASS_ATTR_RW(composer_dev_choice);
static CLASS_ATTR_RW(force_comp_w);
static CLASS_ATTR_RW(force_comp_h);
static CLASS_ATTR_RW(vd_test_fps);
static CLASS_ATTR_RW(vd_test_fps_pip);
static CLASS_ATTR_RW(dewarp_load_flag);

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
	&class_attr_dewarp_rotate_width.attr,
	&class_attr_dewarp_rotate_height.attr,
	&class_attr_close_black.attr,
	&class_attr_composer_use_444.attr,
	&class_attr_reset_drop.attr,
	&class_attr_drop_cnt.attr,
	&class_attr_drop_cnt_pip.attr,
	&class_attr_receive_count.attr,
	&class_attr_receive_count_pip.attr,
	&class_attr_receive_new_count.attr,
	&class_attr_receive_new_count_pip.attr,
	&class_attr_total_get_count.attr,
	&class_attr_total_put_count.attr,
	&class_attr_nn_need_time.attr,
	&class_attr_nn_margin_time.attr,
	&class_attr_nn_bypass.attr,
	&class_attr_tv_fence_creat_count.attr,
	&class_attr_dump_vframe.attr,
	&class_attr_vd_pulldown_level.attr,
	&class_attr_vd_max_hold_count.attr,
	&class_attr_vd_set_frame_delay.attr,
	&class_attr_vd_dump_vframe.attr,
	&class_attr_actual_delay_count.attr,
	&class_attr_vicp_output_dev.attr,
	&class_attr_vicp_shrink_mode.attr,
	&class_attr_vicp_max_width.attr,
	&class_attr_vicp_max_height.attr,
	&class_attr_composer_dev_choice.attr,
	&class_attr_force_comp_w.attr,
	&class_attr_force_comp_h.attr,
	&class_attr_vd_test_fps.attr,
	&class_attr_vd_test_fps_pip.attr,
	&class_attr_dewarp_load_flag.attr,
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
	if (layer_cap & LAYER2_SCALER)
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
		st->pdev = &pdev->dev;
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

//MODULE_DESCRIPTION("Video Technology Magazine video composer Capture Board");
//MODULE_AUTHOR("Amlogic, Jintao Xu<jintao.xu@amlogic.com>");
//MODULE_LICENSE("GPL");
//MODULE_VERSION(VIDEO_COMPOSER_VERSION);

