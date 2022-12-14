/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_sink/vpp_post_s5.h
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

#ifndef VPP_POST_S5_HH
#define VPP_POST_S5_HH
#include "video_reg_s5.h"

/* VPP POST input src: 3VD, 2 OSD */
/* S5 only VD1, VD2 */
#define VPP_POST_VD_NUM   3
#define VPP_POST_OSD_NUM  2
#define VPP_POST_NUM (VPP_POST_VD_NUM + VPP_POST_OSD_NUM)

#define POST_SLICE_NUM    4
struct vpp_post_blend_s {
	u32 bld_out_en;
	u32 bld_out_w;
	u32 bld_out_h;
	u32 bld_out_premult;

	u32 bld_src1_sel;	  //1:din0	2:din1 3:din2 4:din3 5:din4 else :close
	u32 bld_src2_sel;	  //1:din0	2:din1 3:din2 4:din3 5:din4 else :close
	u32 bld_src3_sel;	  //1:din0	2:din1 3:din2 4:din3 5:din4 else :close
	u32 bld_src4_sel;	  //1:din0	2:din1 3:din2 4:din3 5:din4 else :close
	u32 bld_src5_sel;	  //1:din0	2:din1 3:din2 4:din3 5:din4 else :close
	u32 bld_dummy_data;

	//usually the bottom layer set 1, for example postbld_src1_sel = 1,set 0x1
	u32 bld_din0_premult_en;
	u32 bld_din1_premult_en;
	u32 bld_din2_premult_en;
	u32 bld_din3_premult_en;
	u32 bld_din4_premult_en;

	//u32 vd1_index;//VPP_VD1/VPP_VD2/VPP_VD3
	u32 bld_din0_h_start;
	u32 bld_din0_h_end;
	u32 bld_din0_v_start;
	u32 bld_din0_v_end;
	u32 bld_din0_alpha;

	//u32 vd2_index;//VPP_VD1/VPP_VD2/VPP_VD3
	u32 bld_din1_h_start;
	u32 bld_din1_h_end;
	u32 bld_din1_v_start;
	u32 bld_din1_v_end;
	u32 bld_din1_alpha;

	//u32 vd3_index;//VPP_VD1/VPP_VD2/VPP_VD3
	u32 bld_din2_h_start;
	u32 bld_din2_h_end;
	u32 bld_din2_v_start;
	u32 bld_din2_v_end;
	u32 bld_din2_alpha;

	//u32 osd1_index;//VPP_OSD1/VPP_OSD2/VPP_OSD3/VPP_OSD4
	u32 bld_din3_h_start;
	u32 bld_din3_h_end;
	u32 bld_din3_v_start;
	u32 bld_din3_v_end;

	//u32 osd2_index;//VPP_OSD1/VPP_OSD2/VPP_OSD3/VPP_OSD4
	u32 bld_din4_h_start;
	u32 bld_din4_h_end;
	u32 bld_din4_v_start;
	u32 bld_din4_v_end;
};

struct vd1_hwin_s {
	u32 vd1_hwin_en;
	u32 vd1_hwin_in_hsize;
	/* hwin cut out before to blend */
	u32 vd1_hwin_out_hsize;
};

struct vpp_post_pad_s {
	u32 vpp_post_pad_en;
	u32 vpp_post_pad_hsize;
	u32 vpp_post_pad_dummy;
	/* 1: padding with last colum */
	/* 0: padding with vpp_post_pad_dummy val */
	u32 vpp_post_pad_rpt_lcol;
};

struct vpp_post_hwin_s {
	u32 vpp_post_hwin_en;
	u32 vpp_post_dout_hsize;
	u32 vpp_post_dout_vsize;
};

struct vpp_post_proc_slice_s {
	u32 hsize[POST_SLICE_NUM];
	u32 vsize[POST_SLICE_NUM];
};

struct vpp_post_proc_hwin_s {
	u32 hwin_en[POST_SLICE_NUM];
	u32 hwin_bgn[POST_SLICE_NUM];
	u32 hwin_end[POST_SLICE_NUM];
};

struct vpp_post_proc_s {
	struct vpp_post_proc_slice_s vpp_post_proc_slice;
	struct vpp_post_proc_hwin_s vpp_post_proc_hwin;
	u32 align_fifo_size[POST_SLICE_NUM];
	u32 gamma_bypass;
	u32 ccm_bypass;
	u32 vadj2_bypass;
	u32 lut3d_bypass;
	u32 gain_off_bypass;
	u32 vwm_bypass;
};

struct vpp_post_s {
	u32 slice_num;
	u32 overlap_hsize;
	struct vd1_hwin_s vd1_hwin;
	struct vpp_post_blend_s vpp_post_blend;
	struct vpp_post_pad_s vpp_post_pad;
	struct vpp_post_hwin_s vpp_post_hwin;
	struct vpp_post_proc_s vpp_post_proc;
};

struct vpp_post_input_s {
	u32 slice_num;
	u32 overlap_hsize;
	u32 din_hsize[VPP_POST_NUM];
	u32 din_vsize[VPP_POST_NUM];
	u32 din_x_start[VPP_POST_NUM];
	u32 din_y_start[VPP_POST_NUM];
	u32 bld_out_hsize;
	u32 bld_out_vsize;
	/* means vd1 4s4p padding */
	u32 vd1_padding_en;
	u32 vd1_size_before_padding;
	u32 vd1_size_after_padding;
};

struct vpp_post_reg_s {
	struct vpp_post_blend_reg_s vpp_post_blend_reg;
	struct vpp_post_misc_reg_s vpp_post_misc_reg;
};

int get_vpp_slice_num(const struct vinfo_s *info);
int vpp_post_param_set(struct vpp_post_input_s *vpp_input,
	struct vpp_post_s *vpp_post);
void vpp_post_set(u32 vpp_index, struct vpp_post_s *vpp_post);
int update_vpp_input_info(const struct vinfo_s *info);
struct vpp_post_input_s *get_vpp_input_info(void);
void dump_vpp_post_reg(void);

#endif
