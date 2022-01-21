/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_processor/video_composer/videodisplay.h
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

#ifndef VIDEO_DISPLAY_H
#define VIDEO_DISPLAY_H

#include "video_composer.h"

/* disable video_display mode */
#define VIDEO_DISPLAY_ENABLE_NONE    0
#define VIDEO_DISPLAY_ENABLE_NORMAL  1

extern u32 vd_pulldown_level;
extern u32 vd_max_hold_count;
extern u32 vsync_pts_inc_scale;
extern u32 vsync_pts_inc_scale_base;

struct video_display_frame_info_t {
	struct dma_buf *dmabuf;
	struct dma_fence *input_fence;
	struct dma_fence *release_fence;
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
	u32 reserved[10];
};

int video_display_create_path(struct composer_dev *dev);
int video_display_release_path(struct composer_dev *dev);
int video_display_setenable(int layer_index, int is_enable);
int video_display_setframe(int layer_index,
			struct video_display_frame_info_t *frame_info,
			int flags);
void vd_prepare_data_q_put(struct composer_dev *dev,
			struct vd_prepare_s *vd_prepare);
struct vd_prepare_s *vd_prepare_data_q_get(struct composer_dev *dev);
int vd_render_index_get(struct composer_dev *dev);
#endif
