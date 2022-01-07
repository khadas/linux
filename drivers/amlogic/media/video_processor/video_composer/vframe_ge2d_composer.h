/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_processor/video_composer/vframe_ge2d_composer.h
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

#ifndef VFRAME_GE2D_COMPOSER_H
#define VFRAME_GE2D_COMPOSER_H
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
//#include "video_composer.h"

#define VIDEOCOM_INFO(fmt, args...)	\
	pr_info("video_composer: info:" fmt "", ## args)
#define VIDEOCOM_DBG(fmt, args...)	\
	pr_debug("video_composer: dbg:" fmt "", ## args)
#define VIDEOCOM_WARN(fmt, args...)	\
	pr_warn("video_composer: warn:" fmt "", ## args)
#define VIDEOCOM_ERR(fmt, args...)	\
	pr_err("video_composer: err:" fmt "", ## args)

enum videocom_source_type {
	DECODER_8BIT_NORMAL = 0,
	DECODER_8BIT_BOTTOM,
	DECODER_8BIT_TOP,
	DECODER_10BIT_NORMAL,
	DECODER_10BIT_BOTTOM,
	DECODER_10BIT_TOP,
	VDIN_8BIT_NORMAL,
	VDIN_10BIT_NORMAL,
};

enum ge2d_angle_type {
	GE2D_ANGLE_TYPE_ROT_90 = 1,
	GE2D_ANGLE_TYPE_ROT_180,
	GE2D_ANGLE_TYPE_ROT_270,
	GE2D_ANGLE_TYPE_FLIP_H,
	GE2D_ANGLE_TYPE_FLIP_V,
	GE2D_ANGLE_TYPE_MAX,
};

struct ge2d_composer_para {
	int count;
	int format;
	int position_left;
	int position_top;
	int position_width;
	int position_height;
	int buffer_w;
	int buffer_h;
	int plane_num;
	int canvas_scr[3];
	int canvas_dst[3];
	u32 phy_addr[3];
	u32 canvas0Addr;
	struct ge2d_context_s *context;
	struct config_para_ex_s *ge2d_config;
	int angle;
	bool is_tvp;
};

struct src_data_para {
	u32 canvas0Addr;
	u32 canvas1Addr;
	u32 plane_num;
	u32 type;
	u32 posion_x;
	u32 posion_y;
	u32 width;
	u32 height;
	u32 bitdepth;
	struct canvas_config_s canvas0_config[3];
	struct canvas_config_s canvas1_config[3];
	enum vframe_source_type_e source_type;
	bool is_vframe;
};

struct dump_param {
	u32 plane_num;
	struct canvas_config_s canvas0_config[3];
};

enum buffer_data {
	BLACK_BUFFER = 0,
	SCR_BUFFER,
	DST_EMPTY_BUFFER,
	DST_BUFFER_DATA,
	OTHER_BUFFER,
};

int init_ge2d_composer(struct ge2d_composer_para *ge2d_comp_para);

int uninit_ge2d_composer(struct ge2d_composer_para *ge2d_comp_para);

int fill_vframe_black(struct ge2d_composer_para *ge2d_comp_para);

int ge2d_data_composer(struct src_data_para *src_para,
		       struct ge2d_composer_para *ge2d_comp_para);

#endif
