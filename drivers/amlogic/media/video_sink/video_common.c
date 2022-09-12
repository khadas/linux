// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_sink/video_common.c
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/amlogic/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/arm-smccc.h>
#include <linux/debugfs.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/sched.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include "video_priv.h"
#include "video_hw_s5.h"
#include "video_reg_s5.h"
#include "vpp_post_s5.h"

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <linux/amlogic/media/utils/vdec_reg.h>

#include <linux/amlogic/media/registers/register.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "videolog.h"

#include <linux/amlogic/media/video_sink/vpp.h>
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include "linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h"
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include "../common/vfm/vfm.h"
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include "video_receiver.h"
#ifdef CONFIG_AMLOGIC_MEDIA_LUT_DMA
#include <linux/amlogic/media/lut_dma/lut_dma.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
#include <linux/amlogic/media/di/di_interface.h>
#endif

u32 is_crop_left_odd(struct vpp_frame_par_s *frame_par)
{
	int crop_left_odd;

	/* odd, not even aligned*/
	if (frame_par->VPP_hd_start_lines_ & 0x01)
		crop_left_odd = 1;
	else
		crop_left_odd = 0;
	return crop_left_odd;
}

void get_pre_hscaler_para(u8 layer_id, int *ds_ratio, int *flt_num)
{
	switch (pre_scaler[layer_id].pre_hscaler_ntap) {
	case PRE_HSCALER_2TAP:
		*ds_ratio = pre_scaler[layer_id].pre_hscaler_rate;
		*flt_num = 2;
		break;
	case PRE_HSCALER_4TAP:
		*ds_ratio = pre_scaler[layer_id].pre_hscaler_rate;
		*flt_num = 4;
		break;
	case PRE_HSCALER_6TAP:
		*ds_ratio = pre_scaler[layer_id].pre_hscaler_rate;
		*flt_num = 6;
		break;
	case PRE_HSCALER_8TAP:
		*ds_ratio = pre_scaler[layer_id].pre_hscaler_rate;
		*flt_num = 8;
		break;
	}
}

void get_pre_vscaler_para(u8 layer_id, int *ds_ratio, int *flt_num)
{
	switch (pre_scaler[layer_id].pre_vscaler_ntap) {
	case PRE_VSCALER_2TAP:
		*ds_ratio = pre_scaler[layer_id].pre_vscaler_rate;
		*flt_num = 2;
		break;
	case PRE_VSCALER_4TAP:
		*ds_ratio = pre_scaler[layer_id].pre_vscaler_rate;
		*flt_num = 4;
		break;
	}
}

void get_pre_hscaler_coef(u8 layer_id, int *pre_hscaler_table)
{
	if (pre_scaler[layer_id].pre_hscaler_coef_set) {
		pre_hscaler_table[0] = pre_scaler[layer_id].pre_hscaler_coef[0];
		pre_hscaler_table[1] = pre_scaler[layer_id].pre_hscaler_coef[1];
		pre_hscaler_table[2] = pre_scaler[layer_id].pre_hscaler_coef[2];
		pre_hscaler_table[3] = pre_scaler[layer_id].pre_hscaler_coef[3];
	} else {
		switch (pre_scaler[layer_id].pre_hscaler_ntap) {
		case PRE_HSCALER_2TAP:
			pre_hscaler_table[0] = 0x100;
			pre_hscaler_table[1] = 0x0;
			pre_hscaler_table[2] = 0x0;
			pre_hscaler_table[3] = 0x0;
			break;
		case PRE_HSCALER_4TAP:
			pre_hscaler_table[0] = 0xc0;
			pre_hscaler_table[1] = 0x40;
			pre_hscaler_table[2] = 0x0;
			pre_hscaler_table[3] = 0x0;
			break;
		case PRE_HSCALER_6TAP:
			pre_hscaler_table[0] = 0x9c;
			pre_hscaler_table[1] = 0x44;
			pre_hscaler_table[2] = 0x20;
			pre_hscaler_table[3] = 0x0;
			break;
		case PRE_HSCALER_8TAP:
			pre_hscaler_table[0] = 0x90;
			pre_hscaler_table[1] = 0x40;
			pre_hscaler_table[2] = 0x20;
			pre_hscaler_table[3] = 0x10;
			break;
		}
	}
}

void get_pre_vscaler_coef(u8 layer_id, int *pre_hscaler_table)
{
	if (pre_scaler[layer_id].pre_vscaler_coef_set) {
		pre_hscaler_table[0] = pre_scaler[layer_id].pre_vscaler_coef[0];
		pre_hscaler_table[1] = pre_scaler[layer_id].pre_vscaler_coef[1];
	} else {
		switch (pre_scaler[layer_id].pre_vscaler_ntap) {
		case PRE_VSCALER_2TAP:
			if (has_pre_hscaler_8tap(0)) {
				pre_hscaler_table[0] = 0x100;
				pre_hscaler_table[1] = 0x0;
			} else {
				pre_hscaler_table[0] = 0x40;
				pre_hscaler_table[1] = 0x0;
			}
			break;
		case PRE_VSCALER_4TAP:
			if (has_pre_hscaler_8tap(0)) {
				pre_hscaler_table[0] = 0xc0;
				pre_hscaler_table[1] = 0x40;
			} else {
				pre_hscaler_table[0] = 0xf8;
				pre_hscaler_table[1] = 0x48;
			}
			break;
		}
	}
}

u32 viu_line_stride(u32 buffr_width)
{
	u32 line_stride;

	/* input: buffer width not hsize */
	/* 1 stride = 16 byte */
	line_stride = (buffr_width + 15) / 16;
	return line_stride;
}

void init_layer_canvas(struct video_layer_s *layer,
			      u32 start_canvas)
{
	u32 i, j;

	if (!layer)
		return;

	for (i = 0; i < CANVAS_TABLE_CNT; i++) {
		for (j = 0; j < 6; j++)
			layer->canvas_tbl[i][j] =
				start_canvas++;
		layer->disp_canvas[i][0] =
			(layer->canvas_tbl[i][2] << 16) |
			(layer->canvas_tbl[i][1] << 8) |
			layer->canvas_tbl[i][0];
		layer->disp_canvas[i][1] =
			(layer->canvas_tbl[i][5] << 16) |
			(layer->canvas_tbl[i][4] << 8) |
			layer->canvas_tbl[i][3];
	}
}

void vframe_canvas_set(struct canvas_config_s *config,
	u32 planes,
	u32 *index)
{
	int i;
	u32 *canvas_index = index;

	struct canvas_config_s *cfg = config;

	for (i = 0; i < planes; i++, canvas_index++, cfg++)
		canvas_config_config(*canvas_index, cfg);
}

bool is_layer_aisr_supported(struct video_layer_s *layer)
{
	/* only vd1 has aisr for t3 */
	if (!layer || layer->layer_id != 0)
		return false;
	else
		return true;
}

