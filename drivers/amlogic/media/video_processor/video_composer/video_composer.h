/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_processor/video_composer/video_composer.h
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

#ifndef VIDEO_COMPOSER_H
#define VIDEO_COMPOSER_H

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
#include <linux/amlogic/media/video_processor/video_composer_ext.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vfm_ext.h>

#include <linux/kfifo.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/dma-mapping.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include "vfq.h"
#include <linux/amlogic/media/ge2d/ge2d.h>
#include "vframe_ge2d_composer.h"

#define MXA_LAYER_COUNT 9
#define COMPOSER_READY_POOL_SIZE 32
#define FRAMES_INFO_POOL_SIZE 32

/* disable video_composer mode */
#define VIDEO_COMPOSER_ENABLE_NONE    0
#define VIDEO_COMPOSER_ENABLE_NORMAL  1
#define BUFFER_LEN 4
#define DMA_BUF_COUNT 4
#define PATTEN_FACTOR_MAX 5

#define VCOM_MAP_NAME_SIZE 90
#define VCOM_MAP_STRUCT_SIZE 120

#define VCOM_PROVIDER_NAME_SIZE 32

#define PRINT_ERROR		0X0
#define PRINT_QUEUE_STATUS	0X0001
#define PRINT_FENCE		0X0002
#define PRINT_PERFORMANCE	0X0004
#define PRINT_AXIS		0X0008
#define PRINT_INDEX_DISP	0X0010
#define PRINT_PATTERN	        0X0020
#define PRINT_OTHER		0X0040
#define PRINT_NN		0X0080

#define SOURCE_DTV_FIX_TUNNEL		0x1
#define SOURCE_HWC_CREAT_ION		0x2
#define SOURCE_PIC_MODE		0x4

enum vc_transform_t {
	/* flip source image horizontally */
	VC_TRANSFORM_FLIP_H = 1,
	/* flip source image vertically */
	VC_TRANSFORM_FLIP_V = 2,
	/* rotate source image 90 degrees clock-wise */
	VC_TRANSFORM_ROT_90 = 4,
	/* rotate source image 180 degrees */
	VC_TRANSFORM_ROT_180 = 3,
	/* rotate source image 270 degrees clock-wise */
	VC_TRANSFORM_ROT_270 = 7,
	/* flip source image horizontally, the rotate 90 degrees clock-wise */
	VC_TRANSFORM_FLIP_H_ROT_90 = VC_TRANSFORM_FLIP_H | VC_TRANSFORM_ROT_90,
	/* flip source image vertically, the rotate 90 degrees clock-wise */
	VC_TRANSFORM_FLIP_V_ROT_90 = VC_TRANSFORM_FLIP_V | VC_TRANSFORM_ROT_90,
};

struct frame_info_t {
	u32 fd;
	u32 composer_fen_fd;
	u32 disp_fen_fd;
	u32 buffer_w;
	u32 buffer_h;
	u32 dst_x;
	u32 dst_y;
	u32 dst_w;
	u32 dst_h;
	u32 crop_x;
	u32 crop_y;
	u32 crop_w;
	u32 crop_h;
	u32 zorder;
	u32 transform;
	u32 type;
	u32 sideband_type;
	u32 reserved[2];
	u32 source_type;
};

struct frames_info_t {
	u32 frame_count;
	struct frame_info_t frame_info[MXA_LAYER_COUNT];
	u32 layer_index;
	u32 disp_zorder;
	u32 reserved[4];
};

enum com_buffer_status {
	UNINITIAL = 0,
	INIT_SUCCESS,
	INIT_ERROR,
};

struct video_composer_port_s {
	const char *name;
	u32 index;
	u32 open_count;
	struct device *class_dev;
	struct device *pdev;
};

struct videocom_frame_s {
	struct vframe_s frame;
	int index;
};

struct vidc_buf_status {
	int index;
	int dirty;
};

struct dst_buf_t {
	int index;
	struct vframe_s frame;
	struct componser_info_t componser_info;
	bool dirty;
	u32 phy_addr;
	u32 buf_w;
	u32 buf_h;
	u32 buf_size;
	bool is_tvp;
};

struct output_axis {
	int left;
	int top;
	int width;
	int height;
};

struct received_frames_t {
	int index;
	atomic_t on_use;
	struct frames_info_t frames_info;
	unsigned long long frames_num;
	struct file *file_vf[MXA_LAYER_COUNT];
	unsigned long phy_addr[MXA_LAYER_COUNT];
	u64 time_us64;
	bool is_tvp;
};

struct vd_prepare_s {
	struct vframe_s *src_frame;
	struct vframe_s dst_frame;
	void *release_fence;
};

struct composer_dev {
	u32 index;
	struct video_composer_port_s *port;
	u32 enable_composer;
	DECLARE_KFIFO(ready_q, struct vframe_s *, COMPOSER_READY_POOL_SIZE);
	DECLARE_KFIFO(receive_q, struct received_frames_t *,
		      FRAMES_INFO_POOL_SIZE);
	DECLARE_KFIFO(free_q, struct vframe_s *, BUFFER_LEN);
	DECLARE_KFIFO(display_q, struct vframe_s *,
			COMPOSER_READY_POOL_SIZE);
	DECLARE_KFIFO(vc_private_q, struct video_composer_private *,
			COMPOSER_READY_POOL_SIZE);
	DECLARE_KFIFO(vc_prepare_data_q, struct vd_prepare_s *,
			COMPOSER_READY_POOL_SIZE);
	char vf_provider_name[VCOM_PROVIDER_NAME_SIZE];
	char vfm_map_id[VCOM_MAP_STRUCT_SIZE];
	char vfm_map_chain[VCOM_MAP_STRUCT_SIZE];
	struct vframe_provider_s vc_vf_prov;
	void *video_timeline;
	u32 cur_streamline_val;
	struct file *last_file;
	enum com_buffer_status buffer_status;
	struct ge2d_composer_para ge2d_para;
	struct task_struct *kthread;
	struct received_frames_t received_frames[FRAMES_INFO_POOL_SIZE];
	unsigned long long received_count;
	unsigned long long received_new_count;
	unsigned long long fence_creat_count;
	unsigned long long fence_release_count;
	unsigned long long fput_count;
	unsigned long long fget_count;
	bool need_free_buffer;
	//struct mutex mutex_input;
	wait_queue_head_t wq;
	bool thread_stopped;
	struct vframe_s *last_dst_vf;
	bool need_unint_receive_q;
	struct completion task_done;
	struct dst_buf_t dst_buf[BUFFER_LEN];
	struct vframe_s dma_vf[DMA_BUF_COUNT];
	u32 drop_frame_count;
	struct received_frames_t last_frames;
	struct timeval start_time;
	u32 vinfo_w;
	u32 vinfo_h;
	u32 composer_buf_w;
	u32 composer_buf_h;
	bool need_rotate;
	bool is_sideband;
	bool need_empty_ready;
	struct vframe_s fake_vf;
	struct vframe_s fake_back_vf;
	struct video_composer_private vc_private[COMPOSER_READY_POOL_SIZE];
	struct vd_prepare_s vd_prepare[COMPOSER_READY_POOL_SIZE];
	struct vd_prepare_s *vd_prepare_last;
	bool select_path_done;
	bool composer_enabled;
	bool thread_need_stop;
	bool is_drm_enable;
	u32 video_render_index;
	u32 vframe_dump_flag;
	u32 pre_pat_trace;
	u32 pattern[5];
	u32 pattern_enter_cnt;
	u32 pattern_exit_cnt;
	u32 pattern_detected;
	u32 continue_hold_count;
	u32 last_hold_index;
	u32 last_vsync_index;
	u32 last_vf_index;
	bool enable_pulldown;
	u32 patten_factor[PATTEN_FACTOR_MAX];
	u32 patten_factor_index;
	u32 next_factor;
};

#define VIDEO_COMPOSER_IOC_MAGIC  'V'
#define VIDEO_COMPOSER_IOCTL_SET_FRAMES		\
	_IOW(VIDEO_COMPOSER_IOC_MAGIC, 0x00, struct frames_info_t)
#define VIDEO_COMPOSER_IOCTL_SET_ENABLE		\
	_IOW(VIDEO_COMPOSER_IOC_MAGIC, 0x01, int)
#define VIDEO_COMPOSER_IOCTL_SET_DISABLE	\
	_IOW(VIDEO_COMPOSER_IOC_MAGIC, 0x02, int)
#define VIDEO_COMPOSER_IOCTL_GET_PANEL_CAPABILITY	\
	_IOR(VIDEO_COMPOSER_IOC_MAGIC, 0x03, int)

int video_composer_set_enable(struct composer_dev *dev, u32 val);
struct video_composer_port_s *video_composer_get_port(u32 index);
int vc_print(int index, int debug_flag, const char *fmt, ...);
void videocomposer_vf_put(struct vframe_s *vf, void *op_arg);
struct vframe_s *videocomposer_vf_peek(void *op_arg);
void video_dispaly_push_ready(struct composer_dev *dev, struct vframe_s *vf);
void vc_private_q_init(struct composer_dev *dev);
void vc_private_q_recycle(struct composer_dev *dev,
	struct video_composer_private *vc_private);
struct video_composer_private *vc_private_q_pop(struct composer_dev *dev);

#endif /* VIDEO_COMPOSER_H */
