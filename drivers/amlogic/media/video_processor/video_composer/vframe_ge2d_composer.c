// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/video_composer/vframe_ge2d_composer.c
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

#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/ge2d/ge2d_func.h>
#include "vframe_ge2d_composer.h"
#include "vfq.h"
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/delay.h>

static unsigned int ge2d_com_debug;
MODULE_PARM_DESC(ge2d_com_debug, "\n ge2d_com_debug\n");
module_param(ge2d_com_debug, uint, 0664);

static int get_source_type(struct src_data_para *src_data)
{
	enum videocom_source_type ret;
	int interlace_mode;

	interlace_mode = src_data->type & VIDTYPE_TYPEMASK;
	if (src_data->source_type == VFRAME_SOURCE_TYPE_HDMI ||
	    src_data->source_type == VFRAME_SOURCE_TYPE_CVBS) {
		if ((src_data->bitdepth & BITDEPTH_Y10)  &&
		    (!(src_data->type & VIDTYPE_COMPRESS)) &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL))
			ret = VDIN_10BIT_NORMAL;
		else
			ret = VDIN_8BIT_NORMAL;
	} else {
		if ((src_data->bitdepth & BITDEPTH_Y10)  &&
		    (!(src_data->type & VIDTYPE_COMPRESS)) &&
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

static int get_input_format(struct src_data_para *src_data)
{
	int format = GE2D_FORMAT_M24_YUV420;
	enum videocom_source_type soure_type;

	soure_type = get_source_type(src_data);
	switch (soure_type) {
	case DECODER_8BIT_NORMAL:
		if (src_data->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (src_data->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (src_data->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12;
		else if (src_data->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case DECODER_8BIT_BOTTOM:
		if (src_data->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12
				| (GE2D_FORMAT_M24_NV12B & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444B & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420B & (3 << 3));
		break;
	case DECODER_8BIT_TOP:
		if (src_data->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12
				| (GE2D_FORMAT_M24_NV12T & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444T & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420T & (3 << 3));
		break;
	case DECODER_10BIT_NORMAL:
		if (src_data->type & VIDTYPE_VIU_422) {
			if (src_data->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	case DECODER_10BIT_BOTTOM:
		if (src_data->type & VIDTYPE_VIU_422) {
			if (src_data->bitdepth & FULL_PACK_422_MODE)
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
		if (src_data->type & VIDTYPE_VIU_422) {
			if (src_data->bitdepth & FULL_PACK_422_MODE)
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
		if (src_data->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (src_data->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (src_data->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12;
		else if (src_data->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case VDIN_10BIT_NORMAL:
		if (src_data->type & VIDTYPE_VIU_422) {
			if (src_data->bitdepth & FULL_PACK_422_MODE)
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

static int alloc_src_canvas(struct ge2d_composer_para *ge2d_comp_para)
{
	const char *keep_owner = "ge2d_composer";

	if (ge2d_comp_para->canvas_scr[0] < 0)
		ge2d_comp_para->canvas_scr[0] =
			canvas_pool_map_alloc_canvas(keep_owner);
	if (ge2d_comp_para->canvas_scr[1] < 0)
		ge2d_comp_para->canvas_scr[1] =
			canvas_pool_map_alloc_canvas(keep_owner);
	if (ge2d_comp_para->canvas_scr[2] < 0)
		ge2d_comp_para->canvas_scr[2] =
			canvas_pool_map_alloc_canvas(keep_owner);

	if (ge2d_comp_para->canvas_scr[0] < 0 ||
	    ge2d_comp_para->canvas_scr[1] < 0 ||
	    ge2d_comp_para->canvas_scr[2] < 0) {
		VIDEOCOM_INFO("%s failed\n", __func__);
		return -1;
	}
	return 0;
}

static int free_src_canvas(struct ge2d_composer_para *ge2d_comp_para)
{
	if (ge2d_comp_para->canvas_scr[0] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_scr[0]);
		ge2d_comp_para->canvas_scr[0] = -1;
	}

	if (ge2d_comp_para->canvas_scr[1] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_scr[1]);
		ge2d_comp_para->canvas_scr[1] = -1;
	}

	if (ge2d_comp_para->canvas_scr[2] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_scr[2]);
		ge2d_comp_para->canvas_scr[2] = -1;
	}
	if (ge2d_comp_para->canvas_scr[0] >= 0 ||
	    ge2d_comp_para->canvas_scr[1] >= 0 ||
	    ge2d_comp_para->canvas_scr[2] >= 0) {
		VIDEOCOM_INFO("%s failed!\n", __func__);
		return -1;
	}
	return 0;
}

int init_ge2d_composer(struct ge2d_composer_para *ge2d_comp_para)
{
	const char *keep_owner = "ge2d_dest_comp";

	ge2d_comp_para->context = create_ge2d_work_queue();
	if (IS_ERR_OR_NULL(ge2d_comp_para->context)) {
		VIDEOCOM_INFO("creat ge2d work failed\n");
		return -1;
	}

	if (ge2d_comp_para->plane_num > 0) {
		if (ge2d_comp_para->canvas_dst[0] < 0)
			ge2d_comp_para->canvas_dst[0] =
				canvas_pool_map_alloc_canvas(keep_owner);
		if (ge2d_comp_para->canvas_dst[0] < 0) {
			VIDEOCOM_ERR("ge2d init dst canvas_0 failed!\n");
			return -1;
		}
	}

	if (ge2d_comp_para->plane_num > 1) {
		if (ge2d_comp_para->canvas_dst[1] < 0)
			ge2d_comp_para->canvas_dst[1] =
				canvas_pool_map_alloc_canvas(keep_owner);
		if (ge2d_comp_para->canvas_dst[1] < 0) {
			VIDEOCOM_ERR("ge2d init dst canvas_1 failed!\n");
			return -1;
		}
	}

	if (ge2d_comp_para->plane_num > 2) {
		if (ge2d_comp_para->canvas_dst[2] < 0)
			ge2d_comp_para->canvas_dst[2] =
				canvas_pool_map_alloc_canvas(keep_owner);
		if (ge2d_comp_para->canvas_dst[2] < 0) {
			VIDEOCOM_ERR("ge2d init dst canvas_2 failed!\n");
			return -1;
		}
	}
	return 0;
}

int uninit_ge2d_composer(struct ge2d_composer_para *ge2d_comp_para)
{
	destroy_ge2d_work_queue(ge2d_comp_para->context);
	ge2d_comp_para->context = NULL;

	if (free_src_canvas(ge2d_comp_para) < 0) {
		VIDEOCOM_ERR("free src canvas failed!\n");
		return -1;
	}

	if (ge2d_comp_para->canvas_dst[0] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_dst[0]);
		ge2d_comp_para->canvas_dst[0] = -1;
	}

	if (ge2d_comp_para->canvas_dst[1] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_dst[1]);
		ge2d_comp_para->canvas_dst[1] = -1;
	}

	if (ge2d_comp_para->canvas_dst[2] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_dst[2]);
		ge2d_comp_para->canvas_dst[2] = -1;
	}

	if (ge2d_comp_para->canvas_dst[0] >= 0 ||
	    ge2d_comp_para->canvas_dst[1] >= 0 ||
	    ge2d_comp_para->canvas_dst[2] >= 0) {
		VIDEOCOM_ERR("free dst canvas failed!\n");
		return -1;
	}
	return 0;
}

int fill_vframe_black(struct ge2d_composer_para *ge2d_comp_para)
{
	struct canvas_config_s dst_canvas0_config[3];
	u32 dst_plane_num;

	memset(dst_canvas0_config, 0, sizeof(dst_canvas0_config));
	memset(ge2d_comp_para->ge2d_config, 0, sizeof(struct config_para_ex_s));

	if (ge2d_comp_para->format == GE2D_FORMAT_S24_YUV444) {
		dst_canvas0_config[0].phy_addr = ge2d_comp_para->phy_addr[0];
		dst_canvas0_config[0].width = ge2d_comp_para->buffer_w * 3;
		dst_canvas0_config[0].height = ge2d_comp_para->buffer_h;
		dst_canvas0_config[0].block_mode = 0;
		dst_canvas0_config[0].endian = 0;
	} else if (ge2d_comp_para->format == GE2D_FORMAT_M24_NV21) {
		dst_canvas0_config[0].phy_addr = ge2d_comp_para->phy_addr[0];
		dst_canvas0_config[0].width = ge2d_comp_para->buffer_w;
		dst_canvas0_config[0].height = ge2d_comp_para->buffer_h;
		dst_canvas0_config[0].block_mode = 0;
		dst_canvas0_config[0].endian = 0;
		dst_canvas0_config[1].phy_addr = ge2d_comp_para->phy_addr[0]
			+ ge2d_comp_para->buffer_w * ge2d_comp_para->buffer_h;
		dst_canvas0_config[1].width = ge2d_comp_para->buffer_w;
		dst_canvas0_config[1].height = ge2d_comp_para->buffer_h >> 1;
		dst_canvas0_config[1].block_mode = 0;
		dst_canvas0_config[1].endian = 0;
	}
	dst_plane_num = ge2d_comp_para->plane_num;

	if (ge2d_comp_para->canvas0Addr == (u32)-1) {
		canvas_config_config(ge2d_comp_para->canvas_dst[0],
				     &dst_canvas0_config[0]);
		if (dst_plane_num == 2) {
			canvas_config_config(ge2d_comp_para->canvas_dst[1],
					     &dst_canvas0_config[1]);
		} else if (dst_plane_num == 3) {
			canvas_config_config(ge2d_comp_para->canvas_dst[2],
					     &dst_canvas0_config[2]);
		}
		ge2d_comp_para->ge2d_config->src_para.canvas_index =
			ge2d_comp_para->canvas_dst[0]
			| (ge2d_comp_para->canvas_dst[1] << 8)
			| (ge2d_comp_para->canvas_dst[2] << 16);

	} else {
		ge2d_comp_para->ge2d_config->src_para.canvas_index =
			ge2d_comp_para->canvas0Addr;
	}
	ge2d_comp_para->ge2d_config->alu_const_color = 0;/*0x000000ff;*/
	ge2d_comp_para->ge2d_config->bitmask_en = 0;
	ge2d_comp_para->ge2d_config->src1_gb_alpha = 0;/*0xff;*/
	ge2d_comp_para->ge2d_config->dst_xy_swap = 0;

	ge2d_comp_para->ge2d_config->src_key.key_enable = 0;
	ge2d_comp_para->ge2d_config->src_key.key_mask = 0;
	ge2d_comp_para->ge2d_config->src_key.key_mode = 0;

	ge2d_comp_para->ge2d_config->src_para.mem_type =
		CANVAS_TYPE_INVALID;
	ge2d_comp_para->ge2d_config->src_para.format =
		ge2d_comp_para->format;
	ge2d_comp_para->ge2d_config->src_para.fill_color_en = 0;
	ge2d_comp_para->ge2d_config->src_para.fill_mode = 0;
	ge2d_comp_para->ge2d_config->src_para.x_rev = 0;
	ge2d_comp_para->ge2d_config->src_para.y_rev = 0;
	ge2d_comp_para->ge2d_config->src_para.color = 0;
	ge2d_comp_para->ge2d_config->src_para.top = 0;
	ge2d_comp_para->ge2d_config->src_para.left = 0;
	ge2d_comp_para->ge2d_config->src_para.width =
		ge2d_comp_para->buffer_w;
	ge2d_comp_para->ge2d_config->src_para.height =
		ge2d_comp_para->buffer_h;
	ge2d_comp_para->ge2d_config->src2_para.mem_type =
		CANVAS_TYPE_INVALID;
	ge2d_comp_para->ge2d_config->dst_para.canvas_index =
		ge2d_comp_para->ge2d_config->src_para.canvas_index;
	ge2d_comp_para->ge2d_config->dst_para.mem_type =
		CANVAS_TYPE_INVALID;
	ge2d_comp_para->ge2d_config->dst_para.fill_color_en = 0;
	ge2d_comp_para->ge2d_config->dst_para.fill_mode = 0;
	ge2d_comp_para->ge2d_config->dst_para.x_rev = 0;
	ge2d_comp_para->ge2d_config->dst_para.y_rev = 0;
	ge2d_comp_para->ge2d_config->dst_para.color = 0;
	ge2d_comp_para->ge2d_config->dst_para.top = 0;
	ge2d_comp_para->ge2d_config->dst_para.left = 0;
	ge2d_comp_para->ge2d_config->dst_para.format =
		ge2d_comp_para->format;
	if (ge2d_comp_para->format == GE2D_FORMAT_M24_NV21)
		ge2d_comp_para->ge2d_config->dst_para.format |=
			GE2D_LITTLE_ENDIAN;
	ge2d_comp_para->ge2d_config->dst_para.width =
		ge2d_comp_para->buffer_w;
	ge2d_comp_para->ge2d_config->dst_para.height =
		ge2d_comp_para->buffer_h;

	if (ge2d_context_config_ex(ge2d_comp_para->context,
				   ge2d_comp_para->ge2d_config) < 0) {
		VIDEOCOM_ERR("++ge2d configing error.\n");
		return -1;
	}
	fillrect(ge2d_comp_para->context, 0, 0,
		 ge2d_comp_para->buffer_w,
		 ge2d_comp_para->buffer_h,
		 0x008080ff);
	return 0;
}

int ge2d_data_composer(struct src_data_para *scr_data,
		       struct ge2d_composer_para *ge2d_comp_para)
{
	int ret;
	struct canvas_config_s dst_canvas0_config[3];
	u32 dst_plane_num;
	int src_canvas_id, dst_canvas_id;
	int input_width, input_height;
	int position_left, position_top;
	int position_width, position_height;

	memset(ge2d_comp_para->ge2d_config,
	       0, sizeof(struct config_para_ex_s));

	if (ge2d_comp_para->format == GE2D_FORMAT_S24_YUV444) {
		dst_canvas0_config[0].phy_addr = ge2d_comp_para->phy_addr[0];
		dst_canvas0_config[0].width = ge2d_comp_para->buffer_w * 3;
		dst_canvas0_config[0].height = ge2d_comp_para->buffer_h;
		dst_canvas0_config[0].block_mode = 0;
		dst_canvas0_config[0].endian = 0;
	} else if (ge2d_comp_para->format == GE2D_FORMAT_M24_NV21) {
		dst_canvas0_config[0].phy_addr = ge2d_comp_para->phy_addr[0];
		dst_canvas0_config[0].width = ge2d_comp_para->buffer_w;
		dst_canvas0_config[0].height = ge2d_comp_para->buffer_h;
		dst_canvas0_config[0].block_mode = 0;
		dst_canvas0_config[0].endian = 0;
		dst_canvas0_config[1].phy_addr = ge2d_comp_para->phy_addr[0]
			+ ge2d_comp_para->buffer_w * ge2d_comp_para->buffer_h;
		dst_canvas0_config[1].width = ge2d_comp_para->buffer_w;
		dst_canvas0_config[1].height = ge2d_comp_para->buffer_h >> 1;
		dst_canvas0_config[1].block_mode = 0;
		dst_canvas0_config[1].endian = 0;
	}
	dst_plane_num = ge2d_comp_para->plane_num;

	if (scr_data->canvas0Addr == (u32)-1) {
		ret = alloc_src_canvas(ge2d_comp_para);
		if (ret < 0) {
			VIDEOCOM_ERR("alloc src canvas failed!\n");
			return -1;
		}
		canvas_config_config(ge2d_comp_para->canvas_scr[0],
				     &scr_data->canvas0_config[0]);
		if (scr_data->plane_num == 2) {
			canvas_config_config(ge2d_comp_para->canvas_scr[1],
					     &scr_data->canvas0_config[1]);
		} else if (scr_data->plane_num == 3) {
			canvas_config_config(ge2d_comp_para->canvas_scr[2],
					     &scr_data->canvas0_config[2]);
		}
		src_canvas_id = ge2d_comp_para->canvas_scr[0]
			| (ge2d_comp_para->canvas_scr[1] << 8)
			| (ge2d_comp_para->canvas_scr[2] << 16);
	} else {
		src_canvas_id = scr_data->canvas0Addr;
	}

	if (ge2d_comp_para->canvas0Addr == (u32)-1) {
		canvas_config_config(ge2d_comp_para->canvas_dst[0],
				     &dst_canvas0_config[0]);
		if (dst_plane_num == 2) {
			canvas_config_config(ge2d_comp_para->canvas_dst[1],
					     &dst_canvas0_config[1]);
		} else if (dst_plane_num == 3) {
			canvas_config_config(ge2d_comp_para->canvas_dst[2],
					     &dst_canvas0_config[2]);
		}
		dst_canvas_id = ge2d_comp_para->canvas_dst[0]
			| (ge2d_comp_para->canvas_dst[1] << 8)
			| (ge2d_comp_para->canvas_dst[2] << 16);
	} else {
		dst_canvas_id = ge2d_comp_para->canvas0Addr;
	}
	input_width = scr_data->width;
	input_height = scr_data->height;

	if (scr_data->type & VIDTYPE_INTERLACE)
		input_height = scr_data->height >> 1;
	else
		input_height = scr_data->height;
	ge2d_comp_para->ge2d_config->alu_const_color = 0;
	ge2d_comp_para->ge2d_config->bitmask_en = 0;
	ge2d_comp_para->ge2d_config->src1_gb_alpha = 0;
	ge2d_comp_para->ge2d_config->dst_xy_swap = 0;

	ge2d_comp_para->ge2d_config->src_key.key_enable = 0;
	ge2d_comp_para->ge2d_config->src_key.key_mask = 0;
	ge2d_comp_para->ge2d_config->src_key.key_mode = 0;
	ge2d_comp_para->ge2d_config->src_para.mem_type =
		CANVAS_TYPE_INVALID;
	if (scr_data->is_vframe)
		ge2d_comp_para->ge2d_config->src_para.format =
			get_input_format(scr_data);
	else
		ge2d_comp_para->ge2d_config->src_para.format =
			get_input_format(scr_data) | GE2D_LITTLE_ENDIAN;

	ge2d_comp_para->ge2d_config->src_para.fill_color_en = 0;
	ge2d_comp_para->ge2d_config->src_para.fill_mode = 0;
	ge2d_comp_para->ge2d_config->src_para.x_rev = 0;
	ge2d_comp_para->ge2d_config->src_para.y_rev = 0;
	ge2d_comp_para->ge2d_config->src_para.color =
		0xffffffff;
	ge2d_comp_para->ge2d_config->src_para.top = 0;
	ge2d_comp_para->ge2d_config->src_para.left = 0;
	ge2d_comp_para->ge2d_config->src_para.width = input_width;
	ge2d_comp_para->ge2d_config->src_para.height = input_height;
	ge2d_comp_para->ge2d_config->src_para.canvas_index = src_canvas_id;

	ge2d_comp_para->ge2d_config->src2_para.mem_type =
		CANVAS_TYPE_INVALID;

	ge2d_comp_para->ge2d_config->dst_para.mem_type =
		CANVAS_TYPE_INVALID;

	ge2d_comp_para->ge2d_config->dst_para.fill_color_en = 0;
	ge2d_comp_para->ge2d_config->dst_para.fill_mode = 0;
	ge2d_comp_para->ge2d_config->dst_para.x_rev = 0;
	ge2d_comp_para->ge2d_config->dst_para.y_rev = 0;
	ge2d_comp_para->ge2d_config->dst_xy_swap = 0;

	if (ge2d_comp_para->angle == 1) {
		ge2d_comp_para->ge2d_config->dst_xy_swap = 1;
		ge2d_comp_para->ge2d_config->dst_para.x_rev = 1;
	} else if (ge2d_comp_para->angle == 2) {
		ge2d_comp_para->ge2d_config->dst_para.x_rev = 1;
		ge2d_comp_para->ge2d_config->dst_para.y_rev = 1;
	} else if (ge2d_comp_para->angle == 3) {
		ge2d_comp_para->ge2d_config->dst_xy_swap = 1;
		ge2d_comp_para->ge2d_config->dst_para.y_rev = 1;
	}
	ge2d_comp_para->ge2d_config->dst_para.canvas_index = dst_canvas_id;

	ge2d_comp_para->ge2d_config->dst_para.color = 0;
	ge2d_comp_para->ge2d_config->dst_para.top = 0;
	ge2d_comp_para->ge2d_config->dst_para.left = 0;

	ge2d_comp_para->ge2d_config->dst_para.format =
		ge2d_comp_para->format;
	if (ge2d_comp_para->format == GE2D_FORMAT_M24_NV21)
		ge2d_comp_para->ge2d_config->dst_para.format |=
			GE2D_LITTLE_ENDIAN;
	ge2d_comp_para->ge2d_config->dst_para.width =
		ge2d_comp_para->buffer_w;
	ge2d_comp_para->ge2d_config->dst_para.height =
		ge2d_comp_para->buffer_h;

	if (ge2d_context_config_ex(ge2d_comp_para->context,
				   ge2d_comp_para->ge2d_config) < 0) {
		VIDEOCOM_ERR("++ge2d configing error.\n");
		return -1;
	}

	position_left = ge2d_comp_para->position_left;
	position_top = ge2d_comp_para->position_top;
	position_width = ge2d_comp_para->position_width;
	position_height = ge2d_comp_para->position_height;

	stretchblt_noalpha(ge2d_comp_para->context,
			   0,
			   0,
			   input_width,
			   input_height,
			   position_left,
			   position_top,
			   position_width,
			   position_height);
	if (ge2d_com_debug & 1)
		VIDEOCOM_INFO("scr %0x,dst: %0x!,%d, %d, %d, %d\n",
			      ge2d_comp_para->ge2d_config->src_para.format,
			      ge2d_comp_para->ge2d_config->dst_para.format,
			      position_left,
			      position_top,
			      position_width,
			      position_height);
	return 0;
}

