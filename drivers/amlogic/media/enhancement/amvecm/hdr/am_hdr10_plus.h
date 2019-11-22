/*
 * drivers/amlogic/media/enhancement/amvecm/hdr/am_hdr.h
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

#ifndef AM_HDR_H
#define AM_HDR_H

struct vframe_hdr_plus_sei {
	u16 present_flag;
	u16 itu_t_t35_country_code;
	u16 itu_t_t35_terminal_provider_code;
	u16 itu_t_t35_terminal_provider_oriented_code;
	u16 application_identifier;
	u16 application_version;
	/*num_windows max is 3*/
	u16 num_windows;
	/*windows xy*/
	u16 window_upper_left_corner_x[3];
	u16 window_upper_left_corner_y[3];
	u16 window_lower_right_corner_x[3];
	u16 window_lower_right_corner_y[3];
	u16 center_of_ellipse_x[3];
	u16 center_of_ellipse_y[3];
	u16 rotation_angle[3];
	u16 semimajor_axis_internal_ellipse[3];
	u16 semimajor_axis_external_ellipse[3];
	u16 semiminor_axis_external_ellipse[3];
	u16 overlap_process_option[3];
	/*target luminance*/
	u32 tgt_sys_disp_max_lumi;
	u16 tgt_sys_disp_act_pk_lumi_flag;
	u16 num_rows_tgt_sys_disp_act_pk_lumi;
	u16 num_cols_tgt_sys_disp_act_pk_lumi;
	u16 tgt_sys_disp_act_pk_lumi[25][25];

	/*num_windows max is 3, e.g maxscl[num_windows][i];*/
	u32 maxscl[3][3];
	u32 average_maxrgb[3];
	u16 num_distribution_maxrgb_percentiles[3];
	u16 distribution_maxrgb_percentages[3][15];
	u32 distribution_maxrgb_percentiles[3][15];
	u16 fraction_bright_pixels[3];

	u16 mast_disp_act_pk_lumi_flag;
	u16 num_rows_mast_disp_act_pk_lumi;
	u16 num_cols_mast_disp_act_pk_lumi;
	u16 mast_disp_act_pk_lumi[25][25];
	/*num_windows max is 3, e.g knee_point_x[num_windows]*/
	u16 tone_mapping_flag[3];
	u16 knee_point_x[3];
	u16 knee_point_y[3];
	u16 num_bezier_curve_anchors[3];
	u16 bezier_curve_anchors[3][15];
	u16 color_saturation_mapping_flag[3];
	u16 color_saturation_weight[3];
};

struct hdr_plus_bits_s {
	u16 len_itu_t_t35_country_code;
	u16 len_itu_t_t35_terminal_provider_code;
	u16 len_itu_t_t35_terminal_provider_oriented_code;
	u16 len_application_identifier;
	u16 len_application_version;
	/*num_windows max is 3*/
	u16 len_num_windows;
	/*windows xy*/
	u16 len_window_upper_left_corner_x;
	u16 len_window_upper_left_corner_y;
	u16 len_window_lower_right_corner_x;
	u16 len_window_lower_right_corner_y;
	u16 len_center_of_ellipse_x;
	u16 len_center_of_ellipse_y;
	u16 len_rotation_angle;
	u16 len_semimajor_axis_internal_ellipse;
	u16 len_semimajor_axis_external_ellipse;
	u16 len_semiminor_axis_external_ellipse;
	u16 len_overlap_process_option;
	/*target luminance*/
	u16 len_tgt_sys_disp_max_lumi;
	u16 len_tgt_sys_disp_act_pk_lumi_flag;
	u16 len_num_rows_tgt_sys_disp_act_pk_lumi;
	u16 len_num_cols_tgt_sys_disp_act_pk_lumi;
	u16 len_tgt_sys_disp_act_pk_lumi;

	/*num_windows max is 3, e.g maxscl[num_windows][i];*/
	u16 len_maxscl;
	u16 len_average_maxrgb;
	u16 len_num_distribution_maxrgb_percentiles;
	u16 len_distribution_maxrgb_percentages;
	u16 len_distribution_maxrgb_percentiles;
	u16 len_fraction_bright_pixels;

	u16 len_mast_disp_act_pk_lumi_flag;
	u16 len_num_rows_mast_disp_act_pk_lumi;
	u16 len_num_cols_mast_disp_act_pk_lumi;
	u16 len_mast_disp_act_pk_lumi;
	/*num_windows max is 3, e.g knee_point_x[num_windows]*/
	u16 len_tone_mapping_flag;
	u16 len_knee_point_x;
	u16 len_knee_point_y;
	u16 len_num_bezier_curve_anchors;
	u16 len_bezier_curve_anchors;
	u16 len_color_saturation_mapping_flag;
	u16 len_color_saturation_weight;
};

extern uint debug_hdr;
#define HDR_PLUS_IEEE_OUI 0x90848B
#define SEI_Syntax 0x4
extern void hdr10_plus_hdmitx_vsif_parser(
	struct hdr10plus_para *hdmitx_hdr10plus_param,
	struct vframe_s *vf);
extern void hdr10_plus_parser_metadata(struct vframe_s *vf);
extern void hdr10_plus_process(struct vframe_s *vf);
extern void hdr10_plus_debug(void);
extern struct vframe_hdr_plus_sei hdr_plus_sei;

#endif /* AM_HDR_H */

