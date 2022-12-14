/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_sink/video_common.h
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

#ifndef VIDEO_COMMON_HH
#define VIDEO_COMMON_HH
#include "video_priv.h"

u32 is_crop_left_odd(struct vpp_frame_par_s *frame_par);
void get_pre_hscaler_para(u8 layer_id, int *ds_ratio, int *flt_num);
void get_pre_vscaler_para(u8 layer_id, int *ds_ratio, int *flt_num);
void get_pre_hscaler_coef(u8 layer_id, int *pre_hscaler_table);
void get_pre_vscaler_coef(u8 layer_id, int *pre_hscaler_table);
u32 viu_line_stride(u32 buffr_width);
void init_layer_canvas(struct video_layer_s *layer,
			      u32 start_canvas);
void vframe_canvas_set(struct canvas_config_s *config,
	u32 planes,
	u32 *index);
bool is_layer_aisr_supported(struct video_layer_s *layer);
#endif
