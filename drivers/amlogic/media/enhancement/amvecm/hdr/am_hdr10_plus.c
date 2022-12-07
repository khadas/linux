// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/amcsc.c
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

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/amvecm/cuva_alg.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>

#include "am_hdr10_plus.h"
#include "am_hdr10_plus_ootf.h"
#include "../amcsc.h"

uint debug_hdr;
#define pr_hdr(fmt, args...)\
	do {\
		if (debug_hdr)\
			pr_info(fmt, ## args);\
	} while (0)

#define HDR10_PLUS_VERSION  "hdr10_plus v1_20181024"

struct hdr_plus_bits_s sei_md_bits = {
	.len_itu_t_t35_country_code = 8,
	.len_itu_t_t35_terminal_provider_code = 16,
	.len_itu_t_t35_terminal_provider_oriented_code = 16,
	.len_application_identifier = 8,
	.len_application_version = 8,
	.len_num_windows = 2,
	.len_window_upper_left_corner_x = 16,
	.len_window_upper_left_corner_y = 16,
	.len_window_lower_right_corner_x = 16,
	.len_window_lower_right_corner_y = 16,
	.len_center_of_ellipse_x = 16,
	.len_center_of_ellipse_y = 16,
	.len_rotation_angle = 8,
	.len_semimajor_axis_internal_ellipse = 16,
	.len_semimajor_axis_external_ellipse = 16,
	.len_semiminor_axis_external_ellipse = 16,
	.len_overlap_process_option = 1,
	.len_tgt_sys_disp_max_lumi = 27,
	.len_tgt_sys_disp_act_pk_lumi_flag = 1,
	.len_num_rows_tgt_sys_disp_act_pk_lumi = 5,
	.len_num_cols_tgt_sys_disp_act_pk_lumi = 5,
	.len_tgt_sys_disp_act_pk_lumi = 4,
	.len_maxscl = 17,
	.len_average_maxrgb = 17,
	.len_num_distribution_maxrgb_percentiles = 4,
	.len_distribution_maxrgb_percentages = 7,
	.len_distribution_maxrgb_percentiles = 17,
	.len_fraction_bright_pixels = 10,
	.len_mast_disp_act_pk_lumi_flag = 1,
	.len_num_rows_mast_disp_act_pk_lumi = 5,
	.len_num_cols_mast_disp_act_pk_lumi = 5,
	.len_mast_disp_act_pk_lumi = 4,
	.len_tone_mapping_flag = 1,
	.len_knee_point_x = 12,
	.len_knee_point_y = 12,
	.len_num_bezier_curve_anchors = 4,
	.len_bezier_curve_anchors = 10,
	.len_color_saturation_mapping_flag = 1,
	.len_color_saturation_weight = 6
};

struct vframe_hdr_plus_sei hdr_plus_sei;
#define NAL_UNIT_SEI 39
#define NAL_UNIT_SEI_SUFFIX 40

int getbits(char buffer[], int totbitoffset, int *info, int bytecount,
	    int numbits)
{
	int inf;
	int  bitoffset  = (totbitoffset & 0x07);/*bit from start of byte*/
	long byteoffset = (totbitoffset >> 3);/*byte from start of buffer*/
	int  bitcounter = numbits;
	static char *curbyte;

	if ((byteoffset) + ((numbits + bitoffset) >> 3) > bytecount)
		return -1;

	curbyte = &buffer[byteoffset];
	bitoffset = 7 - bitoffset;
	inf = 0;

	while (numbits--) {
		inf <<= 1;
		inf |= ((*curbyte) >> (bitoffset--)) & 0x01;

		if (bitoffset < 0) {
			curbyte++;
			bitoffset = 7;
		}
	/*curbyte   -= (bitoffset >> 3);*/
	/*bitoffset &= 0x07;*/
	/*curbyte   += (bitoffset == 7);*/
	}

	*info = inf;
	return bitcounter;
}

void parser_hdr10_plus_medata(char *metadata, uint32_t size)
{
	int totbitoffset = 0;
	int value = 0;
	int i = 0;
	int j = 0;
	int num_win;
	unsigned int num_col_tsdapl = 0, num_row_tsdapl = 0;
	unsigned int tar_sys_disp_act_pk_lumi_flag = 0;
	unsigned int num_d_m_p = 0;
	unsigned int m_d_a_p_l_flag = 0;
	unsigned int num_row_m_d_a_p_l = 0, num_col_m_d_a_p_l = 0;
	unsigned int tone_mapping_flag = 0;
	unsigned int num_bezier_curve_anchors = 0;
	unsigned int color_saturation_mapping_flag = 0;

	memset(&hdr_plus_sei, 0, sizeof(struct vframe_hdr_plus_sei));

	getbits(metadata, totbitoffset,
		&value, size, sei_md_bits.len_itu_t_t35_country_code);
	hdr_plus_sei.itu_t_t35_country_code = (u16)value;
	totbitoffset += sei_md_bits.len_itu_t_t35_country_code;

	getbits(metadata, totbitoffset,
		&value, size, sei_md_bits.len_itu_t_t35_terminal_provider_code);
	hdr_plus_sei.itu_t_t35_terminal_provider_code = (u16)value;
	totbitoffset += sei_md_bits.len_itu_t_t35_terminal_provider_code;

	getbits(metadata, totbitoffset,
		&value, size,
		sei_md_bits.len_itu_t_t35_terminal_provider_oriented_code);
	hdr_plus_sei.itu_t_t35_terminal_provider_oriented_code =
		(u16)value;
	totbitoffset +=
		sei_md_bits.len_itu_t_t35_terminal_provider_oriented_code;

	getbits(metadata, totbitoffset,
		&value, size,
		sei_md_bits.len_application_identifier);
	hdr_plus_sei.application_identifier = (u16)value;
	totbitoffset += sei_md_bits.len_application_identifier;

	getbits(metadata, totbitoffset,
		&value, size,
		sei_md_bits.len_application_version);
	hdr_plus_sei.application_version = (u16)value;
	totbitoffset += sei_md_bits.len_application_version;

	getbits(metadata, totbitoffset,
		&value, size,
		sei_md_bits.len_num_windows);
	hdr_plus_sei.num_windows = (u16)value;
	totbitoffset += sei_md_bits.len_num_windows;

	num_win = value;

	if (value > 1) {
		for (i = 1; i < num_win; i++) {
			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_window_upper_left_corner_x);
			hdr_plus_sei.window_upper_left_corner_x[i] = (u16)value;
			totbitoffset +=
				sei_md_bits.len_window_upper_left_corner_x;

			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_window_upper_left_corner_y);
			hdr_plus_sei.window_upper_left_corner_y[i] = (u16)value;
			totbitoffset +=
				sei_md_bits.len_window_upper_left_corner_y;

			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_window_lower_right_corner_x);
			hdr_plus_sei.window_lower_right_corner_x[i] =
				(u16)value;
			totbitoffset +=
				sei_md_bits.len_window_lower_right_corner_x;

			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_window_lower_right_corner_y);
			hdr_plus_sei.window_lower_right_corner_y[i] =
				(u16)value;
			totbitoffset +=
				sei_md_bits.len_window_lower_right_corner_y;

			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_center_of_ellipse_x);
			hdr_plus_sei.center_of_ellipse_x[i] = (u16)value;
			totbitoffset += sei_md_bits.len_center_of_ellipse_x;

			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_center_of_ellipse_y);
			hdr_plus_sei.center_of_ellipse_y[i] = (u16)value;
			totbitoffset += sei_md_bits.len_center_of_ellipse_y;

			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_rotation_angle);
			hdr_plus_sei.rotation_angle[i] = (u16)value;
			totbitoffset += sei_md_bits.len_rotation_angle;

			getbits(metadata, totbitoffset,
				&value, size,
			sei_md_bits.len_semimajor_axis_internal_ellipse);
			hdr_plus_sei.semimajor_axis_internal_ellipse[i] =
				(u16)value;
			totbitoffset +=
				sei_md_bits.len_semimajor_axis_internal_ellipse;

			getbits(metadata, totbitoffset,
				&value, size,
			sei_md_bits.len_semimajor_axis_external_ellipse);
			hdr_plus_sei.semimajor_axis_external_ellipse[i] =
				(u16)value;
			totbitoffset +=
				sei_md_bits.len_semimajor_axis_external_ellipse;

			getbits(metadata, totbitoffset,
				&value, size,
			sei_md_bits.len_semiminor_axis_external_ellipse);
			hdr_plus_sei.semiminor_axis_external_ellipse[i] =
				(u16)value;
			totbitoffset +=
				sei_md_bits.len_semiminor_axis_external_ellipse;

			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_overlap_process_option);
			hdr_plus_sei.overlap_process_option[i] = (u16)value;
			totbitoffset += sei_md_bits.len_overlap_process_option;
		}
	}

	getbits(metadata, totbitoffset,
		&value, size,
		sei_md_bits.len_tgt_sys_disp_max_lumi);
		hdr_plus_sei.tgt_sys_disp_max_lumi = value;
	totbitoffset +=
		sei_md_bits.len_tgt_sys_disp_max_lumi;

	getbits(metadata, totbitoffset,
		&value, size,
		sei_md_bits.len_tgt_sys_disp_act_pk_lumi_flag);
	hdr_plus_sei.tgt_sys_disp_act_pk_lumi_flag =
		(u16)value;
	totbitoffset +=
		sei_md_bits.len_tgt_sys_disp_act_pk_lumi_flag;

	tar_sys_disp_act_pk_lumi_flag = value;

	if (tar_sys_disp_act_pk_lumi_flag) {
		getbits(metadata, totbitoffset,
			&value, size,
		sei_md_bits.len_num_rows_tgt_sys_disp_act_pk_lumi);
		hdr_plus_sei.num_rows_tgt_sys_disp_act_pk_lumi =
			(u16)value;
		totbitoffset +=
		sei_md_bits.len_num_rows_tgt_sys_disp_act_pk_lumi;

		num_row_tsdapl = value;

		getbits(metadata, totbitoffset,
			&value, size,
		sei_md_bits.len_num_cols_tgt_sys_disp_act_pk_lumi);
		hdr_plus_sei.num_cols_tgt_sys_disp_act_pk_lumi =
			(u16)value;
		totbitoffset +=
		sei_md_bits.len_num_cols_tgt_sys_disp_act_pk_lumi;

		num_col_tsdapl = value;

		for (i = 0; i < num_row_tsdapl; i++) {
			for (j = 0; j < num_col_tsdapl; j++) {
				getbits(metadata, totbitoffset,
					&value, size,
				sei_md_bits.len_tgt_sys_disp_act_pk_lumi);
				hdr_plus_sei.tgt_sys_disp_act_pk_lumi[i][j] =
					(u16)value;
				totbitoffset +=
				sei_md_bits.len_tgt_sys_disp_act_pk_lumi;
			}
		}
	}
	for (i = 0; i < num_win; i++) {
		for (j = 0; j < 3; j++) {
			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_maxscl);
			hdr_plus_sei.maxscl[i][j] = value;
			totbitoffset += sei_md_bits.len_maxscl;
		}
		getbits(metadata, totbitoffset,
			&value, size,
			sei_md_bits.len_average_maxrgb);
		hdr_plus_sei.average_maxrgb[i] = value;
		totbitoffset += sei_md_bits.len_average_maxrgb;

		getbits(metadata, totbitoffset,
			&value, size,
			sei_md_bits.len_num_distribution_maxrgb_percentiles);
		hdr_plus_sei.num_distribution_maxrgb_percentiles[i] =
			(u16)value;
		totbitoffset +=
			sei_md_bits.len_num_distribution_maxrgb_percentiles;

		num_d_m_p = value;
		for (j = 0; j < num_d_m_p; j++) {
			getbits(metadata, totbitoffset,
				&value, size,
			sei_md_bits.len_distribution_maxrgb_percentages);
			hdr_plus_sei.distribution_maxrgb_percentages[i][j] =
				(u16)value;
			totbitoffset +=
				sei_md_bits.len_distribution_maxrgb_percentages;

			getbits(metadata, totbitoffset,
				&value, size,
			sei_md_bits.len_distribution_maxrgb_percentiles);
			hdr_plus_sei.distribution_maxrgb_percentiles[i][j] =
				value;
			totbitoffset +=
				sei_md_bits.len_distribution_maxrgb_percentiles;
		}

		getbits(metadata, totbitoffset,
			&value, size,
			sei_md_bits.len_fraction_bright_pixels);
		hdr_plus_sei.fraction_bright_pixels[i] = (u16)value;
		totbitoffset += sei_md_bits.len_fraction_bright_pixels;
	}

	getbits(metadata, totbitoffset,
		&value, size,
		sei_md_bits.len_mast_disp_act_pk_lumi_flag);
	hdr_plus_sei.mast_disp_act_pk_lumi_flag =
		(u16)value;
	totbitoffset +=
		sei_md_bits.len_mast_disp_act_pk_lumi_flag;

	m_d_a_p_l_flag = value;
	if (m_d_a_p_l_flag) {
		getbits(metadata, totbitoffset,
			&value, size,
		sei_md_bits.len_num_rows_mast_disp_act_pk_lumi);
		hdr_plus_sei.num_rows_mast_disp_act_pk_lumi =
			(u16)value;
		totbitoffset +=
			sei_md_bits.len_num_rows_mast_disp_act_pk_lumi;

		num_row_m_d_a_p_l = value;

		getbits(metadata, totbitoffset,
			&value, size,
		sei_md_bits.len_num_cols_mast_disp_act_pk_lumi);
		hdr_plus_sei.num_cols_mast_disp_act_pk_lumi =
			(u16)value;
		totbitoffset +=
		sei_md_bits.len_num_cols_mast_disp_act_pk_lumi;

		num_col_m_d_a_p_l = value;

		for (i = 0; i < num_row_m_d_a_p_l; i++) {
			for (j = 0; j < num_col_m_d_a_p_l; j++) {
				getbits(metadata, totbitoffset,
					&value, size,
				sei_md_bits.len_mast_disp_act_pk_lumi);
				hdr_plus_sei.mast_disp_act_pk_lumi[i][j] =
					(u16)value;
				totbitoffset +=
					sei_md_bits.len_mast_disp_act_pk_lumi;
			}
		}
	}

	for (i = 0; i < num_win; i++) {
		getbits(metadata, totbitoffset,
			&value, size,
			sei_md_bits.len_tone_mapping_flag);
		hdr_plus_sei.tone_mapping_flag[i] = (u16)value;
		totbitoffset += sei_md_bits.len_tone_mapping_flag;

		tone_mapping_flag = value;

		if (tone_mapping_flag) {
			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_knee_point_x);
			hdr_plus_sei.knee_point_x[i] = (u16)value;
			totbitoffset += sei_md_bits.len_knee_point_x;

			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_knee_point_y);
			hdr_plus_sei.knee_point_y[i] = (u16)value;
			totbitoffset += sei_md_bits.len_knee_point_y;

			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_num_bezier_curve_anchors);
			hdr_plus_sei.num_bezier_curve_anchors[i] =
				(u16)value;
			totbitoffset +=
				sei_md_bits.len_num_bezier_curve_anchors;

			num_bezier_curve_anchors = value;

			for (j = 0; j < num_bezier_curve_anchors; j++) {
				getbits(metadata, totbitoffset,
					&value, size,
					sei_md_bits.len_bezier_curve_anchors);
				hdr_plus_sei.bezier_curve_anchors[i][j] =
					(u16)value;
				totbitoffset +=
					sei_md_bits.len_bezier_curve_anchors;
			}
		}

		getbits(metadata, totbitoffset,
			&value, size,
			sei_md_bits.len_color_saturation_mapping_flag);
		hdr_plus_sei.color_saturation_mapping_flag[i] =
			(u16)value;
		totbitoffset +=
			sei_md_bits.len_color_saturation_mapping_flag;

		color_saturation_mapping_flag = value;
		if (color_saturation_mapping_flag) {
			getbits(metadata, totbitoffset,
				&value, size,
				sei_md_bits.len_color_saturation_weight);
			hdr_plus_sei.color_saturation_weight[i] =
				(u16)value;
			totbitoffset +=
				sei_md_bits.len_color_saturation_weight;
		}
	}
}

void cuva_metadata_bits_parse(struct cuva_md_bits_s *md_bits)
{
	md_bits->system_start_code_bits = 8;
	md_bits->min_maxrgb_pq_bits = 12;
	md_bits->avg_maxrgb_pq_bits = 12;
	md_bits->var_maxrgb_pq_bits = 12;
	md_bits->max_maxrgb_pq_bits = 12;
	md_bits->tm_enable_mode_flag_bits = 1;
	md_bits->tm_param_en_num_bits = 1;
	md_bits->tgt_system_dsp_max_luma_pq_bits = 12;
	md_bits->base_en_flag_bits = 1;
	md_bits->base_param_m_p_bits = 14;
	md_bits->base_param_m_m_bits = 6;
	md_bits->base_param_m_a_bits = 10;
	md_bits->base_param_m_b_bits = 10;
	md_bits->base_param_m_n_bits = 6;
	md_bits->base_param_m_k1_bits = 2;
	md_bits->base_param_m_k2_bits = 2;
	md_bits->base_param_m_k3_bits = 4;
	md_bits->base_param_delta_en_mode_bits = 3;
	md_bits->base_param_en_delta_bits = 7;
	md_bits->spline_en_flag_bits = 1;
	md_bits->spline_en_num_bits = 1;
	md_bits->spline_th_en_mode_bits = 2;
	md_bits->spline_th_en_mb_bits = 8;
	md_bits->spline_th_enable_bits = 12;
	md_bits->spline_th_en_delta1_bits = 10;
	md_bits->spline_th_en_delta2_bits = 10;
	md_bits->spline_en_strength_bits = 8;
	md_bits->color_sat_mapping_flag_bits = 1;
	md_bits->color_sat_num_bits = 3;
	md_bits->clor_sat_gain_bits = 8;
}

struct cuva_hdr_dynamic_metadata_s cuva_metadata;

void cuva_hdr_metadata_parse(char *metadata, uint32_t size)
{
	struct cuva_md_bits_s md_bits;
	int totbitoffset = 0;
	int value;
	int num_wds;
	int i, j;
	int tm_param_num;
	int spline_num;

	cuva_metadata_bits_parse(&md_bits);

	pr_hdr("cuva metadata: size: %d\n", size);
	for (i = 0; i < size / 8; i++)
		pr_hdr("%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
			metadata[8 * i], metadata[8 * i + 1],
			metadata[8 * i + 2], metadata[8 * i + 3],
			metadata[8 * i + 4], metadata[8 * i + 5],
			metadata[8 * i + 6], metadata[8 * i + 7]);
	if (size % 8 > 0) {
		for (j = 0; j < size % 8; j++)
			pr_hdr("%02x\n", metadata[8 * i + j]);
	}
	pr_hdr("cuva metadata print end\n");

	metadata += 5;
	size -= 5;

	memset(&cuva_metadata, 0,
		sizeof(struct cuva_hdr_dynamic_metadata_s));

	getbits(metadata, totbitoffset,
		&value, size,
		md_bits.system_start_code_bits);
	cuva_metadata.system_start_code = value;
	totbitoffset += md_bits.system_start_code_bits;
	pr_hdr("system_start_code = %d\n", cuva_metadata.system_start_code);

	if (cuva_metadata.system_start_code == 0x1) {
		num_wds = 1;
		/*maxrgb_pq*/
		getbits(metadata, totbitoffset,
			&value, size,
			md_bits.min_maxrgb_pq_bits);
		cuva_metadata.min_maxrgb_pq = value;
		totbitoffset += md_bits.min_maxrgb_pq_bits;
		pr_hdr("min_maxrgb_pq = %d\n", cuva_metadata.min_maxrgb_pq);

		getbits(metadata, totbitoffset,
			&value, size,
			md_bits.avg_maxrgb_pq_bits);
		cuva_metadata.avg_maxrgb_pq = value;
		totbitoffset += md_bits.avg_maxrgb_pq_bits;
		pr_hdr("avg_maxrgb_pq = %d\n", cuva_metadata.avg_maxrgb_pq);

		getbits(metadata, totbitoffset,
			&value, size,
			md_bits.var_maxrgb_pq_bits);
		cuva_metadata.var_maxrgb_pq = value;
		totbitoffset += md_bits.var_maxrgb_pq_bits;
		pr_hdr("var_maxrgb_pq = %d\n", cuva_metadata.var_maxrgb_pq);

		getbits(metadata, totbitoffset,
			&value, size,
			md_bits.max_maxrgb_pq_bits);
		cuva_metadata.max_maxrgb_pq = value;
		totbitoffset += md_bits.max_maxrgb_pq_bits;
		pr_hdr("max_maxrgb_pq = %d\n", cuva_metadata.max_maxrgb_pq);
		/*maxrgb_pq end*/

		getbits(metadata, totbitoffset,
			&value, size,
			md_bits.tm_enable_mode_flag_bits);
		cuva_metadata.tm_enable_mode_flag = value;
		totbitoffset += md_bits.tm_enable_mode_flag_bits;
		pr_hdr("tm_enable_mode_flag = %d\n", cuva_metadata.tm_enable_mode_flag);

		if (cuva_metadata.tm_enable_mode_flag == 1) {
			getbits(metadata, totbitoffset,
				&value, size,
				md_bits.tm_param_en_num_bits);
			cuva_metadata.tm_param_en_num = value;
			totbitoffset += md_bits.tm_param_en_num_bits;
			pr_hdr("tm_param_en_num = %d\n", cuva_metadata.tm_param_en_num);

			if (cuva_metadata.tm_param_en_num > 1)
				cuva_metadata.tm_param_en_num = 1;
			tm_param_num = cuva_metadata.tm_param_en_num + 1;
			for (i = 0; i < tm_param_num; i++) {
				getbits(metadata, totbitoffset,
					&value, size,
					md_bits.tgt_system_dsp_max_luma_pq_bits);
				cuva_metadata.tgt_system_dsp_max_luma_pq[i] = value;
				totbitoffset += md_bits.tgt_system_dsp_max_luma_pq_bits;
				pr_hdr("tgt_system_dsp_max_luma_pq[%d] = %d\n",
					i, cuva_metadata.tgt_system_dsp_max_luma_pq[i]);

				getbits(metadata, totbitoffset,
					&value, size,
					md_bits.base_en_flag_bits);
				cuva_metadata.base_en_flag[i] = value;
				totbitoffset += md_bits.base_en_flag_bits;
				pr_hdr("base_en_flag[%d] = %d\n",
					i, cuva_metadata.base_en_flag[i]);

				if (cuva_metadata.base_en_flag[i]) {
					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.base_param_m_p_bits);
					cuva_metadata.base_param_m_p[i] = value;
					totbitoffset += md_bits.base_param_m_p_bits;
					pr_hdr("base_param_m_p[%d] = %d\n",
						i, cuva_metadata.base_param_m_p[i]);

					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.base_param_m_m_bits);
					cuva_metadata.base_param_m_m[i] = value;
					totbitoffset += md_bits.base_param_m_m_bits;
					pr_hdr("base_param_m_m[%d] = %d\n",
						i, cuva_metadata.base_param_m_m[i]);

					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.base_param_m_a_bits);
					cuva_metadata.base_param_m_a[i] = value;
					totbitoffset += md_bits.base_param_m_a_bits;
					pr_hdr("base_param_m_a[%d] = %d\n",
						i, cuva_metadata.base_param_m_a[i]);

					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.base_param_m_b_bits);
					cuva_metadata.base_param_m_b[i] = value;
					totbitoffset += md_bits.base_param_m_b_bits;
					pr_hdr("base_param_m_b[%d] = %d\n",
						i, cuva_metadata.base_param_m_b[i]);

					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.base_param_m_n_bits);
					cuva_metadata.base_param_m_n[i] = value;
					totbitoffset += md_bits.base_param_m_n_bits;
					pr_hdr("base_param_m_n[%d] = %d\n",
						i, cuva_metadata.base_param_m_n[i]);

					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.base_param_m_k1_bits);
					cuva_metadata.base_param_m_k1[i] = value;
					totbitoffset += md_bits.base_param_m_k1_bits;
					pr_hdr("base_param_m_k1[%d] = %d\n",
						i, cuva_metadata.base_param_m_k1[i]);

					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.base_param_m_k2_bits);
					cuva_metadata.base_param_m_k2[i] = value;
					totbitoffset += md_bits.base_param_m_k2_bits;
					pr_hdr("base_param_m_k2[%d] = %d\n",
						i, cuva_metadata.base_param_m_k2[i]);

					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.base_param_m_k3_bits);
					cuva_metadata.base_param_m_k3[i] = value;
					totbitoffset += md_bits.base_param_m_k3_bits;
					pr_hdr("base_param_m_k3[%d] = %d\n",
						i, cuva_metadata.base_param_m_k3[i]);

					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.base_param_delta_en_mode_bits);
					cuva_metadata.base_param_delta_en_mode[i] = value;
					totbitoffset += md_bits.base_param_delta_en_mode_bits;
					pr_hdr("base_param_delta_en_mode[%d] = %d\n",
						i, cuva_metadata.base_param_delta_en_mode[i]);

					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.base_param_en_delta_bits);
					cuva_metadata.base_param_en_delta[i] = value;
					totbitoffset += md_bits.base_param_en_delta_bits;
					pr_hdr("base_param_en_delta[%d] = %d\n",
						i, cuva_metadata.base_param_en_delta[i]);
				}

				getbits(metadata, totbitoffset,
					&value, size,
					md_bits.spline_en_flag_bits);
				cuva_metadata.spline_en_flag[i] = value;
				totbitoffset += md_bits.spline_en_flag_bits;
				pr_hdr("spline_en_flag[%d] = %d\n",
					i, cuva_metadata.spline_en_flag[i]);

				if (cuva_metadata.spline_en_flag[i]) {
					getbits(metadata, totbitoffset,
						&value, size,
						md_bits.spline_en_num_bits);
					cuva_metadata.spline_en_num[i] = value;
					totbitoffset += md_bits.spline_en_num_bits;
					pr_hdr("spline_en_num[%d] = %d\n",
						i, cuva_metadata.spline_en_num[i]);

					if (cuva_metadata.spline_en_num[i] > 1)
						cuva_metadata.spline_en_num[i] = 1;

					spline_num = cuva_metadata.spline_en_num[i] + 1;
					for (j = 0; j < spline_num; j++) {
						getbits(metadata, totbitoffset,
							&value, size,
							md_bits.spline_th_en_mode_bits);
						cuva_metadata.spline_th_en_mode[j][i] = value;
						totbitoffset += md_bits.spline_th_en_mode_bits;
						pr_hdr("spline_th_en_mode[%d][%d] = %d\n",
						j, i, cuva_metadata.spline_th_en_mode[j][i]);

						if (cuva_metadata.spline_th_en_mode[j][i]
							== 0 ||
							cuva_metadata.spline_th_en_mode[j][i]
							== 2) {
							getbits(metadata, totbitoffset,
								&value, size,
								md_bits.spline_th_en_mb_bits);
							cuva_metadata.spline_th_en_mb[j][i] = value;
							totbitoffset +=
								md_bits.spline_th_en_mb_bits;
							pr_hdr("spline_th_en_mb[%d][%d] = %d\n",
							j, i, cuva_metadata.spline_th_en_mb[j][i]);
						}

						getbits(metadata, totbitoffset,
							&value, size,
							md_bits.spline_th_enable_bits);
						cuva_metadata.spline_th_enable[j][i] = value;
						totbitoffset += md_bits.spline_th_enable_bits;
						pr_hdr("spline_th_enable[%d][%d] = %d\n",
						j, i, cuva_metadata.spline_th_enable[j][i]);

						getbits(metadata, totbitoffset,
							&value, size,
							md_bits.spline_th_en_delta1_bits);
						cuva_metadata.spline_th_en_delta1[j][i] = value;
						totbitoffset += md_bits.spline_th_en_delta1_bits;
						pr_hdr("spline_th_en_delta1[%d][%d] = %d\n",
						j, i, cuva_metadata.spline_th_en_delta1[j][i]);

						getbits(metadata, totbitoffset,
							&value, size,
							md_bits.spline_th_en_delta2_bits);
						cuva_metadata.spline_th_en_delta2[j][i] = value;
						totbitoffset += md_bits.spline_th_en_delta2_bits;
						pr_hdr("spline_th_en_delta2[%d][%d] = %d\n",
						j, i, cuva_metadata.spline_th_en_delta2[j][i]);

						getbits(metadata, totbitoffset,
							&value, size,
							md_bits.spline_en_strength_bits);
						cuva_metadata.spline_en_strength[j][i] = value;
						totbitoffset += md_bits.spline_en_strength_bits;
						pr_hdr("spline_en_strengch[%d][%d] = %d\n",
						j, i, cuva_metadata.spline_en_strength[j][i]);
					}
				}
			}
		}

		getbits(metadata, totbitoffset,
			&value, size,
			md_bits.color_sat_mapping_flag_bits);
		cuva_metadata.color_sat_mapping_flag = value;
		totbitoffset += md_bits.color_sat_mapping_flag_bits;
		pr_hdr("color_sat_mapping_flag = %d\n",
			cuva_metadata.color_sat_mapping_flag);

		if (cuva_metadata.color_sat_mapping_flag) {
			getbits(metadata, totbitoffset,
				&value, size,
				md_bits.color_sat_num_bits);
			cuva_metadata.color_sat_num = value;
			totbitoffset += md_bits.color_sat_num_bits;
			pr_hdr("color_sat_num = %d\n", cuva_metadata.color_sat_num);

			if (cuva_metadata.color_sat_num > 8)
				cuva_metadata.color_sat_num = 8;
			for (i = 0; i < cuva_metadata.color_sat_num; i++) {
				getbits(metadata, totbitoffset,
					&value, size,
					md_bits.clor_sat_gain_bits);
				cuva_metadata.clor_sat_gain[i] = value;
				totbitoffset += md_bits.clor_sat_gain_bits;
				pr_hdr("clor_sat_gain[%d] = %d\n",
					i, cuva_metadata.clor_sat_gain[i]);
			}
		}
	}
}

static int parse_sei(char *sei_buf, uint32_t size)
{
	char *p = sei_buf;
	char *p_sei;
	u16 header;
	u16 nal_unit_type;
	u16 payload_type, payload_size;

	if (size < 2)
		return 0;
	header = *p++;
	header <<= 8;
	header += *p++;
	nal_unit_type = header >> 9;
	if (nal_unit_type != NAL_UNIT_SEI &&
	    nal_unit_type != NAL_UNIT_SEI_SUFFIX)
		return 0;
	while (p + 4 <= sei_buf + size) {
		payload_type = *p++;
		/*unsupport type for the current*/
		if (payload_type == 0xff)
			payload_type += *p++;
		payload_size = *p++;
		if (payload_size == 0xff)
			payload_size += *p++;
		if (p + payload_size <= sei_buf + size) {
			switch (payload_type) {
			case SEI_SYNTAX:
				p_sei = p;
				if (p_sei[0] == 0xB5 &&
					p_sei[1] == 0x00 &&
					p_sei[2] == 0x3C &&
					p_sei[3] == 0x00 &&
					p_sei[4] == 0x01 &&
					p_sei[5] == 0x04)
					parser_hdr10_plus_medata(p_sei, payload_size);
				else if (p_sei[0] == 0x26 &&
					p_sei[1] == 0x00 &&
					p_sei[2] == 0x04 &&
					p_sei[3] == 0x00 &&
					p_sei[4] == 0x05)
					cuva_hdr_metadata_parse(p_sei, payload_size);
				break;
			default:
				break;
			}
		}
		p += payload_size;
	}
	return 0;
}

/*av1 hdr10p not contain nal_unit_type + payload type +  payload size  these 4bytes*/
static int parse_sei_av1(char *sei_buf, uint32_t size)
{
	if (size < 6)
		return 0;

	if (check_av1_hdr10p(sei_buf))
		parser_hdr10_plus_medata(sei_buf, size);

	return 0;
}

static void hdr10_plus_vf_md_parse(struct vframe_s *vf)
{
	int i;
	struct hdr10plus_para hdr10p_md_param;
	struct vframe_hdr_plus_sei *p;

	memset(&hdr10p_md_param, 0, sizeof(struct hdr10plus_para));
	memset(&hdr_plus_sei, 0, sizeof(struct vframe_hdr_plus_sei));
	p = &hdr_plus_sei;

	hdr10p_md_param.application_version =
		vf->prop.hdr10p_data.pb4_st.app_ver;
	pr_hdr("vf:app_ver = 0x%x\n", vf->prop.hdr10p_data.pb4_st.app_ver);

	hdr10p_md_param.targeted_max_lum =
		vf->prop.hdr10p_data.pb4_st.max_lumin;
	pr_hdr("vf:target_max_lumin = 0x%x\n",
	       vf->prop.hdr10p_data.pb4_st.max_lumin);

	hdr10p_md_param.average_maxrgb =
		vf->prop.hdr10p_data.average_maxrgb;
	pr_hdr("vf:average_maxrgb = 0x%x\n",
	       vf->prop.hdr10p_data.average_maxrgb);

	/*distribution value*/
	memcpy(&hdr10p_md_param.distribution_values[0],
	       &vf->prop.hdr10p_data.distrib_valus0,
		sizeof(uint8_t) * 9);
	for (i = 0; i < 9; i++)
		pr_hdr("vf:hdr10p_md_param.distribution_values[%d] = 0x%x\n",
		       i, hdr10p_md_param.distribution_values[i]);

	hdr10p_md_param.num_bezier_curve_anchors =
	vf->prop.hdr10p_data.pb15_18_st.num_bezier_curve_anchors;
	pr_hdr("vf:num_bezier_curve_anchors = 0x%x\n",
	       vf->prop.hdr10p_data.pb15_18_st.num_bezier_curve_anchors);

	/*if (!hdr10p_md_param.num_bezier_curve_anchors) {*/
	/*	hdr10p_md_param.num_bezier_curve_anchors = 9;*/
	/*	pr_hdr("hdr10p_md_param.num_bezier_curve_anchors = 0\n");*/
	/*}*/

	hdr10p_md_param.knee_point_x =
	(vf->prop.hdr10p_data.pb15_18_st.knee_point_x_9_6 << 6) |
		vf->prop.hdr10p_data.pb15_18_st.knee_point_x_5_0;
	pr_hdr("vf:knee_point_x_5_0 = 0x%x, knee_point_x_9_6 = 0x%x\n",
	       vf->prop.hdr10p_data.pb15_18_st.knee_point_x_5_0,
	      vf->prop.hdr10p_data.pb15_18_st.knee_point_x_9_6);

	hdr10p_md_param.knee_point_y =
	(vf->prop.hdr10p_data.pb15_18_st.knee_point_y_9_8 << 8) |
		vf->prop.hdr10p_data.pb15_18_st.knee_point_y_7_0;
	pr_hdr("vf:knee_point_y_7_0 = 0x%x, knee_point_y_9_8 = 0x%x\n",
	       vf->prop.hdr10p_data.pb15_18_st.knee_point_y_7_0,
	      vf->prop.hdr10p_data.pb15_18_st.knee_point_y_9_8);

	/*bezier curve*/
	hdr10p_md_param.bezier_curve_anchors[0] =
		vf->prop.hdr10p_data.pb15_18_st.bezier_curve_anchors0;

	memcpy(&hdr10p_md_param.bezier_curve_anchors[1],
	       &vf->prop.hdr10p_data.bezier_curve_anchors1,
	       sizeof(uint8_t) * 8);
	for (i = 0; i < 9; i++)
		pr_hdr("vf:hdr10p_md_param.bezier_curve_anchors[%d] = 0x%x\n",
		       i, hdr10p_md_param.bezier_curve_anchors[i]);

	hdr10p_md_param.graphics_overlay_flag =
		vf->prop.hdr10p_data.pb27_st.overlay_flag;
	hdr10p_md_param.no_delay_flag =
		vf->prop.hdr10p_data.pb27_st.no_delay_flag;
	pr_hdr("vf:overlay_flag = 0x%x\n",
	       vf->prop.hdr10p_data.pb27_st.overlay_flag);
	pr_hdr("vf:no_delay_flag = 0x%x\n",
	       vf->prop.hdr10p_data.pb27_st.no_delay_flag);

	hdr_plus_sei.application_identifier =
		hdr10p_md_param.application_version;

	pr_hdr("hdr_plus_sei.application_identifier = %d\n",
	       hdr_plus_sei.application_identifier);

	/*hdr10 plus default one window*/
	hdr_plus_sei.num_windows = 1;

	hdr_plus_sei.tgt_sys_disp_max_lumi =
		hdr10p_md_param.targeted_max_lum << 5;
	pr_hdr("hdr_plus_sei.tgt_sys_disp_max_lumi = %d\n",
	       hdr_plus_sei.tgt_sys_disp_max_lumi);

	hdr_plus_sei.average_maxrgb[0] =
		(hdr10p_md_param.average_maxrgb << 4) * 10;
	pr_hdr("hdr_plus_sei.average_maxrgb[0] = %d\n",
	       hdr_plus_sei.average_maxrgb[0]);

	for (i = 0; i < 9; i++) {
		if (i == 2) {
			hdr_plus_sei.distribution_maxrgb_percentiles[0][2] =
				hdr10p_md_param.distribution_values[2];
			pr_hdr("hdr_plus_sei.distribution_");
			pr_hdr("maxrgb_percentiles[0][%d] = %d\n",
			       i, p->distribution_maxrgb_percentiles[0][i]);
			continue;
		}
		hdr_plus_sei.distribution_maxrgb_percentiles[0][i] =
			(hdr10p_md_param.distribution_values[i] << 4) * 10;
		pr_hdr("hdr_plus_sei.distribution_");
		pr_hdr("maxrgb_percentiles[0][%d] = %d\n",
		       i, p->distribution_maxrgb_percentiles[0][i]);
	}

	hdr_plus_sei.num_bezier_curve_anchors[0] =
		hdr10p_md_param.num_bezier_curve_anchors;
	pr_hdr("hdr_plus_sei.num_bezier_curve_anchors[0] = %d\n",
	       hdr_plus_sei.num_bezier_curve_anchors[0]);

	hdr_plus_sei.knee_point_x[0] =
		hdr10p_md_param.knee_point_x << 2;
	hdr_plus_sei.knee_point_y[0] =
		hdr10p_md_param.knee_point_y << 2;
	pr_hdr("hdr_plus_sei.knee_point_x[0] = %d\n",
	       hdr_plus_sei.knee_point_x[0]);
	pr_hdr("hdr_plus_sei.knee_point_y[0] = %d\n",
	       hdr_plus_sei.knee_point_y[0]);

	for (i = 0; i < 9; i++) {
		hdr_plus_sei.bezier_curve_anchors[0][i] =
			hdr10p_md_param.bezier_curve_anchors[i] << 2;
		pr_hdr("hdr_plus_sei.bezier_curve_anchors[0][%d] = %d\n",
		       i, hdr_plus_sei.bezier_curve_anchors[0][i]);
	}

	for (i = 0; i < 9; i++) {
		if (hdr_plus_sei.bezier_curve_anchors[0][i] != 0) {
			hdr_plus_sei.tone_mapping_flag[0] = 1;
			break;
		}
	}

	hdr_plus_sei.color_saturation_mapping_flag[0] = 0;
}

void parser_dynamic_metadata(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	struct provider_aux_req_s req_avs2;
	char *p;
	unsigned int size = 0;
	unsigned int type = 0;
	int i;
	int j;
	char *meta_buf;
	int len;

	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		hdr10_plus_vf_md_parse(vf);
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;
		req.low_latency = 0;

		req_avs2.vf = vf;
		req_avs2.bot_flag = 0;
		req_avs2.aux_buf = NULL;
		req_avs2.aux_size = 0;
		req_avs2.dv_enhance_exist = 0;
		req_avs2.low_latency = 0;
		if (get_vframe_src_fmt(vf) == VFRAME_SIGNAL_FMT_HDR10PLUS ||
			get_vframe_src_fmt(vf) == VFRAME_SIGNAL_FMT_CUVA_HDR ||
			get_vframe_src_fmt(vf) == VFRAME_SIGNAL_FMT_CUVA_HLG) {
			size = 0;
			if (vf->codec_vfmt == VFORMAT_AVS2 ||
				vf->codec_vfmt == VFORMAT_AVS3) {
				if (debug_hdr)
					pr_info("avs2/avs3 metadata\n");
				req_avs2.aux_buf = (char *)get_sei_from_src_fmt(vf, &size);
				req_avs2.aux_size = size;
			} else {
				if (debug_hdr)
					pr_info("hevc_md\n");
				req.aux_buf = (char *)get_sei_from_src_fmt(vf, &size);
				req.aux_size = size;
			}
		} else {
			vf_notify_provider_by_name("vdec.h265.00",
						   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
						   (void *)&req);
			if (!req.aux_buf)
				vf_notify_provider_by_name("decoder",
							   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
							   (void *)&req);
			vf_notify_provider_by_name("vdec.avs2.00",
				VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				(void *)&req_avs2);
		}
		if (req.aux_buf && req.aux_size &&
			(debug_csc & 0x10)) {
			meta_buf = req.aux_buf;
			pr_csc(0x10,
				"hdr10 source_type = %d src_fmt = %d metadata(%d):\n",
				get_vframe_src_fmt(vf),
				vf->source_type, req.aux_size);
			for (i = 0; i < req.aux_size + 8; i += 8) {
				len = req.aux_size - i;
				if (len < 8) {
					pr_csc(0x10, "\t i = %02d", i);
					for (j = 0; j < len; j++)
						pr_csc(0x10,
						"%02x ", meta_buf[i + j]);
					pr_csc(0x10, "\n");
				} else {
					pr_csc(0x10,
						"\t i = %02d: %02x %02x %02x %02x %02x %02x %02x %02x\n",
						i,
						meta_buf[i],
						meta_buf[i + 1],
						meta_buf[i + 2],
						meta_buf[i + 3],
						meta_buf[i + 4],
						meta_buf[i + 5],
						meta_buf[i + 6],
						meta_buf[i + 7]);
				}
			}
		}
		if (req.aux_buf && req.aux_size) {
			u32 count = 0;
			u32 offest = 0;

			p = req.aux_buf;
			while (p < req.aux_buf
				+ req.aux_size - 8) {
				size = *p++;
				size = (size << 8) | *p++;
				size = (size << 8) | *p++;
				size = (size << 8) | *p++;
				type = *p++;
				type = (type << 8) | *p++;
				type = (type << 8) | *p++;
				type = (type << 8) | *p++;
				offest += 8;
				if (offest + size > req.aux_size) {
					pr_err("%s exception: t:%x, s:%d, p:%px, c:%d, offt:%d, aux:%px, s:%d\n",
						__func__,
						type, size, p, count, offest,
						req.aux_buf, req.aux_size);
					break;
				}
				if (type == 0x02000000)
					parse_sei(p, size);

				if ((type & 0xffff0000) == 0x14000000)/*av1 hdr10p*/
					parse_sei_av1(p, size);

				count++;
				offest += size;
				p += size;
			}
		} else if (req_avs2.aux_buf && req_avs2.aux_size) {
			p = req_avs2.aux_buf;
			size = req_avs2.aux_size;
			cuva_hdr_metadata_parse(p, size);
		}
	}

	if (debug_hdr >= 1)
		debug_hdr--;
}

struct hdr10plus_para dbg_hdr10plus_pkt;

void hdr10_plus_hdmitx_vsif_parser(struct hdr10plus_para
				   *hdmitx_hdr10plus_param,
				   struct vframe_s *vf)
{
	int vsif_tds_max_l;
	int ave_maxrgb;
	int distribution_values[9];
	int i;
	int kpx, kpy;
	int bz_cur_anchors[9];
	u32 maxrgb_99_percentiles = 0;

	memset(hdmitx_hdr10plus_param, 0, sizeof(struct hdr10plus_para));

	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		hdmitx_hdr10plus_param->application_version =
			vf->prop.hdr10p_data.pb4_st.app_ver;
		hdmitx_hdr10plus_param->targeted_max_lum =
			vf->prop.hdr10p_data.pb4_st.max_lumin;
		hdmitx_hdr10plus_param->average_maxrgb =
			vf->prop.hdr10p_data.average_maxrgb;
		/*distribution value*/
		memcpy(&hdmitx_hdr10plus_param->distribution_values[0],
		       &vf->prop.hdr10p_data.distrib_valus0,
			sizeof(uint8_t) * 9);

		hdmitx_hdr10plus_param->num_bezier_curve_anchors =
		vf->prop.hdr10p_data.pb15_18_st.num_bezier_curve_anchors;

		hdmitx_hdr10plus_param->knee_point_x =
		(vf->prop.hdr10p_data.pb15_18_st.knee_point_x_9_6 << 6) |
			vf->prop.hdr10p_data.pb15_18_st.knee_point_x_5_0;
		hdmitx_hdr10plus_param->knee_point_y =
		(vf->prop.hdr10p_data.pb15_18_st.knee_point_y_9_8 << 8) |
			vf->prop.hdr10p_data.pb15_18_st.knee_point_y_7_0;
		/*bezier curve*/
		hdmitx_hdr10plus_param->bezier_curve_anchors[0] =
			vf->prop.hdr10p_data.pb15_18_st.bezier_curve_anchors0;
		memcpy(&hdmitx_hdr10plus_param->bezier_curve_anchors[1],
		       &vf->prop.hdr10p_data.bezier_curve_anchors1,
			sizeof(uint8_t) * 8);

		hdmitx_hdr10plus_param->graphics_overlay_flag =
			vf->prop.hdr10p_data.pb27_st.overlay_flag;
		hdmitx_hdr10plus_param->no_delay_flag =
			vf->prop.hdr10p_data.pb27_st.no_delay_flag;

		memcpy(&dbg_hdr10plus_pkt, hdmitx_hdr10plus_param,
		       sizeof(struct hdr10plus_para));

		return;
	}

	hdmitx_hdr10plus_param->application_version =
		(u8)hdr_plus_sei.application_version;

	if (hdr_plus_sei.tgt_sys_disp_max_lumi < 1024) {
		vsif_tds_max_l =
			(hdr_plus_sei.tgt_sys_disp_max_lumi + (1 << 4)) >> 5;
		if (vsif_tds_max_l > 31)
			vsif_tds_max_l = 31;
	} else {
		vsif_tds_max_l = 31;
	}
	hdmitx_hdr10plus_param->targeted_max_lum = (u8)vsif_tds_max_l;

	ave_maxrgb = hdr_plus_sei.average_maxrgb[0] / 10;
	if (ave_maxrgb < (1 << 12)) {
		ave_maxrgb = (ave_maxrgb + (1 << 3)) >> 4;
		if (ave_maxrgb > 255)
			ave_maxrgb = 255;
	} else {
		ave_maxrgb = 255;
	}
	hdmitx_hdr10plus_param->average_maxrgb = (u8)ave_maxrgb;

	for (i = 0; i < 9; i++) {
		if (i == 2 &&
		    /* V0 sei update */
		    hdmitx_hdr10plus_param->application_version != 0) {
			distribution_values[i] =
			hdr_plus_sei.distribution_maxrgb_percentiles[0][i];
			hdmitx_hdr10plus_param->distribution_values[i] =
				(u8)distribution_values[i];
			continue;
		}
		distribution_values[i] =
			hdr_plus_sei.distribution_maxrgb_percentiles[0][i] / 10;
		/* V0 sei update */
		if (hdr_plus_sei.num_distribution_maxrgb_percentiles[0] == 10 &&
		    hdmitx_hdr10plus_param->application_version == 0 &&
		    i == 8) {
			maxrgb_99_percentiles =
			hdr_plus_sei.distribution_maxrgb_percentiles[0][i + 1];
			distribution_values[i] = maxrgb_99_percentiles / 10;
		}
		if (distribution_values[i] < (1 << 12)) {
			distribution_values[i] =
				(distribution_values[i] + (1 << 3)) >> 4;
			if (distribution_values[i] > 255)
				distribution_values[i] = 255;
		} else {
			distribution_values[i] = 255;
		}
		hdmitx_hdr10plus_param->distribution_values[i] =
			(u8)distribution_values[i];
	}

	if (hdr_plus_sei.tone_mapping_flag[0] == 0) {
		hdmitx_hdr10plus_param->num_bezier_curve_anchors = 0;
		hdmitx_hdr10plus_param->knee_point_x = 0;
		hdmitx_hdr10plus_param->knee_point_y = 0;
		for (i = 0; i < 9; i++)
			hdmitx_hdr10plus_param->bezier_curve_anchors[0] = 0;
	} else {
		hdmitx_hdr10plus_param->num_bezier_curve_anchors =
			(u8)hdr_plus_sei.num_bezier_curve_anchors[0];

		if (hdmitx_hdr10plus_param->num_bezier_curve_anchors > 9)
			hdmitx_hdr10plus_param->num_bezier_curve_anchors = 9;
		kpx = hdr_plus_sei.knee_point_x[0];
		kpx = (kpx + (1 << 1)) >> 2;
		if (kpx > 1023)
			kpx = 1023;
		hdmitx_hdr10plus_param->knee_point_x = kpx;

		kpy = hdr_plus_sei.knee_point_y[0];
		kpy = (kpy + (1 << 1)) >> 2;
		if (kpy > 1023)
			kpy = 1023;
		hdmitx_hdr10plus_param->knee_point_y = kpy;

		for (i = 0; i < 9; i++) {
			if (i ==
			hdmitx_hdr10plus_param->num_bezier_curve_anchors)
				break;
			bz_cur_anchors[i] =
				hdr_plus_sei.bezier_curve_anchors[0][i];

			bz_cur_anchors[i] = (bz_cur_anchors[i] + (1 << 1)) >> 2;
			if (bz_cur_anchors[i] > 255)
				bz_cur_anchors[i] = 255;
			hdmitx_hdr10plus_param->bezier_curve_anchors[i] =
				(u8)bz_cur_anchors[i];
		}
	}
	/*only video, don't include graphic*/
	hdmitx_hdr10plus_param->graphics_overlay_flag = 0;
	/*metadata and video have no delay*/
	hdmitx_hdr10plus_param->no_delay_flag = 1;

	memcpy(&dbg_hdr10plus_pkt, hdmitx_hdr10plus_param,
	       sizeof(struct hdr10plus_para));
}

struct cuva_hdr_vsif_para dbg_cuva_vsif_pkt;
struct cuva_hdr_vs_emds_para dbg_cuva_emds_pkt;

void cuva_hdr_vsif_pkt_update(struct cuva_hdr_vsif_para *vsif_para)
{
	memset(vsif_para, 0, sizeof(struct cuva_hdr_vsif_para));

	vsif_para->system_start_code = (u8)cuva_metadata.system_start_code;
	vsif_para->version_code = 0x5;
	vsif_para->monitor_mode_en = 0;/*metadata have no monitor_mode_en*/
	vsif_para->transfer_character = 0;/*metadata have no transfer_character*/

	memcpy(&dbg_cuva_vsif_pkt, vsif_para,
		sizeof(struct cuva_hdr_vsif_para));
}

void cuva_hdr_emds_pkt_update(struct cuva_hdr_vs_emds_para *edms_para)
{
	int i;

	memset(edms_para, 0, sizeof(struct cuva_hdr_vs_emds_para));

	edms_para->system_start_code = (u8)cuva_metadata.system_start_code;
	edms_para->version_code = 0x5;
	edms_para->min_maxrgb_pq = (u16)cuva_metadata.min_maxrgb_pq;
	edms_para->avg_maxrgb_pq = (u16)cuva_metadata.avg_maxrgb_pq;
	edms_para->var_maxrgb_pq = (u16)cuva_metadata.var_maxrgb_pq;
	edms_para->max_maxrgb_pq = (u16)cuva_metadata.max_maxrgb_pq;
	edms_para->targeted_max_lum_pq = (u16)cuva_metadata.tgt_system_dsp_max_luma_pq[0];
	edms_para->transfer_character = 0;/*metadata have no transfer_character*/
	edms_para->base_enable_flag = (u8)cuva_metadata.base_en_flag[0];
	edms_para->base_param_m_p = (u16)cuva_metadata.base_param_m_p[0];
	edms_para->base_param_m_m = (u16)cuva_metadata.base_param_m_m[0];
	edms_para->base_param_m_a = (u16)cuva_metadata.base_param_m_a[0];
	edms_para->base_param_m_n = (u16)cuva_metadata.base_param_m_n[0];
	edms_para->base_param_k[0] = (u8)cuva_metadata.base_param_m_k1[0];
	edms_para->base_param_k[1] = (u8)cuva_metadata.base_param_m_k2[0];
	edms_para->base_param_k[2] = (u8)cuva_metadata.base_param_m_k3[0];
	edms_para->base_param_delta_enable_mode =
		(u8)cuva_metadata.base_param_delta_en_mode[0];
	edms_para->base_param_enable_delta = (u8)cuva_metadata.base_param_en_delta[0];
	edms_para->_3spline_enable_num = (u8)cuva_metadata.spline_en_num[0];
	edms_para->_3spline_enable_flag = (u8)cuva_metadata.spline_en_flag[0];
	if (edms_para->_3spline_enable_flag) {
		for (i = 0; i < 2; i++) {
			edms_para->_3spline_data[i].th_enable_mode =
				(u8)cuva_metadata.spline_th_en_mode[0][i];
			edms_para->_3spline_data[i].th_enable_mb =
				(u8)cuva_metadata.spline_th_en_mb[0][i];
			edms_para->_3spline_data[i].th_enable =
				(u16)cuva_metadata.spline_th_enable[0][i];
			edms_para->_3spline_data[i].th_enable_delta[0] =
				(u16)cuva_metadata.spline_th_en_delta1[0][i];
			edms_para->_3spline_data[i].th_enable_delta[1] =
				(u16)cuva_metadata.spline_th_en_delta2[0][i];
			edms_para->_3spline_data[i].enable_strength =
				(u8)cuva_metadata.spline_en_strength[0][i];
		}
	}

	if (cuva_metadata.color_sat_mapping_flag) {
		edms_para->color_saturation_num = (u8)cuva_metadata.color_sat_num;
		for (i = 0; i < 8; i++)
			edms_para->color_saturation_gain[i] =
				(u8)cuva_metadata.clor_sat_gain[i];
	}

	/*metadata have no graphic_src_display_value*/
	edms_para->graphic_src_display_value = 0;
	/*metadata have no max_display_mastering_lum*/
	edms_para->max_display_mastering_lum = 0;

	memcpy(&dbg_cuva_emds_pkt, edms_para,
		sizeof(struct cuva_hdr_vs_emds_para));
}

void hdr10_plus_debug(int csc_type)
{
	int i = 0;
	int j = 0;

	if (csc_type == VPP_MATRIX_BT2020YUV_BT2020RGB_DYNAMIC) {
		pr_info("itu_t_t35_country_code = 0x%x\n",
			hdr_plus_sei.itu_t_t35_country_code);
		pr_info("itu_t_t35_terminal_provider_code = 0x%x\n",
			hdr_plus_sei.itu_t_t35_terminal_provider_code);
		pr_info("itu_t_t35_terminal_provider_oriented_code = 0x%x\n",
			hdr_plus_sei.itu_t_t35_terminal_provider_oriented_code);
		pr_info("application_identifier = 0x%x\n",
			hdr_plus_sei.application_identifier);
		pr_info("application_version = 0x%x\n",
			hdr_plus_sei.application_version);
		pr_info("num_windows = 0x%x\n",
			hdr_plus_sei.num_windows);
		for (i = 1; i < hdr_plus_sei.num_windows; i++) {
			pr_info("window_upper_left_corner_x[%d] = 0x%x\n",
				i, hdr_plus_sei.window_upper_left_corner_x[i]);
			pr_info("window_upper_left_corner_y[%d] = 0x%x\n",
				i, hdr_plus_sei.window_upper_left_corner_y[i]);
			pr_info("window_lower_right_corner_x[%d] = 0x%x\n",
				i, hdr_plus_sei.window_lower_right_corner_x[i]);
			pr_info("window_lower_right_corner_y[%d] = 0x%x\n",
				i, hdr_plus_sei.window_lower_right_corner_y[i]);
			pr_info("center_of_ellipse_x[%d] = 0x%x\n",
				i, hdr_plus_sei.center_of_ellipse_x[i]);
			pr_info("center_of_ellipse_y[%d] = 0x%x\n",
				i, hdr_plus_sei.center_of_ellipse_y[i]);
			pr_info("rotation_angle[%d] = 0x%x\n",
				i, hdr_plus_sei.rotation_angle[i]);
			pr_info("semimajor_axis_internal_ellipse[%d] = 0x%x\n",
				i, hdr_plus_sei.semimajor_axis_internal_ellipse[i]);
			pr_info("semimajor_axis_external_ellipse[%d] = 0x%x\n",
				i, hdr_plus_sei.semimajor_axis_external_ellipse[i]);
			pr_info("semiminor_axis_external_ellipse[%d] = 0x%x\n",
				i, hdr_plus_sei.semiminor_axis_external_ellipse[i]);
			pr_info("overlap_process_option[%d] = 0x%x\n",
				i, hdr_plus_sei.overlap_process_option[i]);
		}
		pr_info("targeted_system_display_maximum_luminance = 0x%x\n",
			hdr_plus_sei.tgt_sys_disp_max_lumi);
		pr_info("targeted_system_display_actual_peak_luminance_flag = 0x%x\n",
			hdr_plus_sei.tgt_sys_disp_act_pk_lumi_flag);
		if (hdr_plus_sei.tgt_sys_disp_act_pk_lumi_flag) {
			for (i = 0;
				i < hdr_plus_sei.num_rows_tgt_sys_disp_act_pk_lumi;
				i++) {
				for (j = 0;
				j < hdr_plus_sei.num_cols_tgt_sys_disp_act_pk_lumi;
				j++) {
					pr_info("tgt_sys_disp_act_pk_lumi");
					pr_info("[%d][%d] = 0x%x\n",
					i, j,
					hdr_plus_sei.tgt_sys_disp_act_pk_lumi[i][j]);
				}
			}
		}

		for (i = 0; i < hdr_plus_sei.num_windows; i++) {
			for (j = 0; j < 3; j++)
				pr_info("maxscl[%d][%d] = 0x%x\n",
					i, j, hdr_plus_sei.maxscl[i][j]);

			pr_info("average_maxrgb[%d] = 0x%x\n",
				i, hdr_plus_sei.average_maxrgb[i]);
			pr_info("num_distribution_maxrgb_percentiles[%d] = 0x%x\n",
				i, hdr_plus_sei.num_distribution_maxrgb_percentiles[i]);
			for (j = 0;
			j < hdr_plus_sei.num_distribution_maxrgb_percentiles[i];
			j++) {
				pr_info("distribution_maxrgb_pcntages[%d][%d] = 0x%x\n",
				i, j,
				hdr_plus_sei.distribution_maxrgb_percentages[i][j]);
				pr_info("distribution_maxrgb_pcntiles[%d][%d] = 0x%x\n",
				i, j,
				hdr_plus_sei.distribution_maxrgb_percentiles[i][j]);
			}
			pr_info("fraction_bright_pixels[%d] = 0x%x\n",
				i, hdr_plus_sei.fraction_bright_pixels[i]);
		}

		pr_info("mast_disp_act_pk_lumi_flag = 0x%x\n",
			hdr_plus_sei.mast_disp_act_pk_lumi_flag);
		if (hdr_plus_sei.mast_disp_act_pk_lumi_flag) {
			pr_info("num_rows_mast_disp_act_pk_lumi = 0x%x\n",
				hdr_plus_sei.num_rows_mast_disp_act_pk_lumi);
			pr_info("num_cols_mast_disp_act_pk_lumi = 0x%x\n",
				hdr_plus_sei.num_cols_mast_disp_act_pk_lumi);
			for (i = 0;
				i < hdr_plus_sei.num_rows_mast_disp_act_pk_lumi;
				i++) {
				for (j = 0;
					j < hdr_plus_sei.num_cols_mast_disp_act_pk_lumi;
					j++)
					pr_info("mast_disp_act_pk_lumi[%d][%d] = 0x%x\n",
					i, j, hdr_plus_sei.mast_disp_act_pk_lumi[i][j]);
			}
		}

		for (i = 0; i < hdr_plus_sei.num_windows; i++) {
			pr_info("tone_mapping_flag[%d] = 0x%x\n",
				i, hdr_plus_sei.tone_mapping_flag[i]);
			pr_info("knee_point_x[%d] = 0x%x\n",
				i, hdr_plus_sei.knee_point_x[i]);
			pr_info("knee_point_y[%d] = 0x%x\n",
				i, hdr_plus_sei.knee_point_y[i]);
			pr_info("num_bezier_curve_anchors[%d] = 0x%x\n",
				i, hdr_plus_sei.num_bezier_curve_anchors[i]);
			for (j = 0; j < hdr_plus_sei.num_bezier_curve_anchors[i]; j++)
				pr_info("bezier_curve_anchors[%d][%d] = 0x%x\n",
					i, j, hdr_plus_sei.bezier_curve_anchors[i][j]);

			pr_info("color_saturation_mapping_flag[%d] = 0x%x\n",
				i, hdr_plus_sei.color_saturation_mapping_flag[i]);
			pr_info("color_saturation_weight[%d] = 0x%x\n",
				i, hdr_plus_sei.color_saturation_weight[i]);
		}

		pr_info("\ntx vsif packet data print begin\n");
		pr_info("application_version = 0x%x\n",
			dbg_hdr10plus_pkt.application_version);
		pr_info("targeted_max_lum = 0x%x\n",
			dbg_hdr10plus_pkt.targeted_max_lum);
		pr_info("average_maxrgb = 0x%x\n",
			dbg_hdr10plus_pkt.average_maxrgb);
		for (i = 0; i < 9; i++)
			pr_info("distribution_values[%d] = 0x%x\n",
			i, dbg_hdr10plus_pkt.distribution_values[i]);
		pr_info("num_bezier_curve_anchors = 0x%x\n",
			dbg_hdr10plus_pkt.num_bezier_curve_anchors);
		pr_info("knee_point_x = 0x%x\n",
			dbg_hdr10plus_pkt.knee_point_x);
		pr_info("knee_point_y = 0x%x\n",
			dbg_hdr10plus_pkt.knee_point_y);

		for (i = 0; i < 9; i++)
			pr_info("bezier_curve_anchors[%d] = 0x%x\n",
			i, dbg_hdr10plus_pkt.bezier_curve_anchors[i]);
		pr_info("graphics_overlay_flag = 0x%x\n",
			dbg_hdr10plus_pkt.graphics_overlay_flag);
		pr_info("no_delay_flag = 0x%x\n",
			dbg_hdr10plus_pkt.no_delay_flag);
		pr_info("\ntx vsif packet data print end\n");

		pr_info(HDR10_PLUS_VERSION);
	} else if (csc_type == VPP_MATRIX_BT2020YUV_BT2020RGB_CUVA) {
		pr_info("-----cuva dynamic metadata------\n");
		pr_info("system_start_code = %d\n",
			cuva_metadata.system_start_code);
		pr_info("min_maxrgb_pq = %d\n",
			cuva_metadata.min_maxrgb_pq);
		pr_info("avg_maxrgb_pq = %d\n",
			cuva_metadata.avg_maxrgb_pq);
		pr_info("var_maxrgb_pq = %d\n",
			cuva_metadata.var_maxrgb_pq);
		pr_info("max_maxrgb_pq = %d\n",
			cuva_metadata.max_maxrgb_pq);
		pr_info("tm_enable_mode_flag = %d\n",
			cuva_metadata.tm_enable_mode_flag);
		pr_info("tm_param_en_num = %d\n",
			cuva_metadata.tm_param_en_num);
		for (i = 0; i < 2; i++) {
			pr_info("tgt_system_dsp_max_luma_pq[%d] = %d\n",
				i, cuva_metadata.tgt_system_dsp_max_luma_pq[i]);
			pr_info("base_en_flag[%d] = %d\n",
				i, cuva_metadata.base_en_flag[i]);
			pr_info("base_param_m_p[%d] = %d\n",
				i, cuva_metadata.base_param_m_p[i]);
			pr_info("base_param_m_m[%d] = %d\n",
				i, cuva_metadata.base_param_m_m[i]);
			pr_info("base_param_m_a[%d] = %d\n",
				i, cuva_metadata.base_param_m_a[i]);
			pr_info("base_param_m_b[%d] = %d\n",
				i, cuva_metadata.base_param_m_b[i]);
			pr_info("base_param_m_n[%d] = %d\n",
				i, cuva_metadata.base_param_m_n[i]);
			pr_info("base_param_m_k1[%d] = %d\n",
				i, cuva_metadata.base_param_m_k1[i]);
			pr_info("base_param_m_k2[%d] = %d\n",
				i, cuva_metadata.base_param_m_k2[i]);
			pr_info("base_param_m_k3[%d] = %d\n",
				i, cuva_metadata.base_param_m_k3[i]);
			pr_info("base_param_delta_en_mode[%d] = %d\n",
				i, cuva_metadata.base_param_delta_en_mode[i]);
			pr_info("base_param_en_delta[%d] = %d\n",
				i, cuva_metadata.base_param_en_delta[i]);
			pr_info("spline_en_flag[%d] = %d\n",
				i, cuva_metadata.spline_en_flag[i]);
			pr_info("spline_en_num[%d] = %d\n",
				i, cuva_metadata.spline_en_num[i]);
			for (j = 0; j < 2; j++) {
				pr_info("spline_th_en_mode[%d][%d] = %d\n",
					i, j, cuva_metadata.spline_th_en_mode[i][j]);
				pr_info("spline_th_en_mb[%d][%d] = %d\n",
					i, j, cuva_metadata.spline_th_en_mb[i][j]);
				pr_info("spline_th_enable[%d][%d] = %d\n",
					i, j, cuva_metadata.spline_th_enable[i][j]);
				pr_info("spline_th_en_delta1[%d][%d] = %d\n",
					i, j, cuva_metadata.spline_th_en_delta1[i][j]);
				pr_info("spline_th_en_delta2[%d][%d] = %d\n",
					i, j, cuva_metadata.spline_th_en_delta2[i][j]);
				pr_info("spline_en_strength[%d][%d] = %d\n",
					i, j, cuva_metadata.spline_en_strength[i][j]);
			}
		}
		pr_info("color_sat_mapping_flag = %d\n",
			cuva_metadata.color_sat_mapping_flag);
		pr_info("color_sat_num = %d\n",
			cuva_metadata.color_sat_num);
		for (i = 0; i < 8; i++)
			pr_info("clor_sat_gain[%d] = %d\n",
			i, cuva_metadata.clor_sat_gain[i]);

		pr_info("\n-----cuva hdmitx vsif pkt------\n");
		pr_info("system_start_code = %d\n",
			dbg_cuva_vsif_pkt.system_start_code);
		pr_info("version_code = %d\n",
			dbg_cuva_vsif_pkt.version_code);
		pr_info("monitor_mode_en = %d\n",
			dbg_cuva_vsif_pkt.monitor_mode_en);
		pr_info("transfer_character = %d\n",
			dbg_cuva_vsif_pkt.transfer_character);

		pr_info("\n-----cuva hdmitx emds pkt------\n");
		pr_info("system_start_code = %d\n",
			dbg_cuva_emds_pkt.system_start_code);
		pr_info("version_code = %d\n",
			dbg_cuva_emds_pkt.version_code);
		pr_info("min_maxrgb_pq = %d\n",
			dbg_cuva_emds_pkt.min_maxrgb_pq);
		pr_info("avg_maxrgb_pq = %d\n",
			dbg_cuva_emds_pkt.avg_maxrgb_pq);
		pr_info("var_maxrgb_pq = %d\n",
			dbg_cuva_emds_pkt.var_maxrgb_pq);
		pr_info("max_maxrgb_pq = %d\n",
			dbg_cuva_emds_pkt.max_maxrgb_pq);
		pr_info("targeted_max_lum_pq = %d\n",
			dbg_cuva_emds_pkt.targeted_max_lum_pq);
		pr_info("transfer_character = %d\n",
			dbg_cuva_emds_pkt.transfer_character);
		pr_info("base_enable_flag = %d\n",
			dbg_cuva_emds_pkt.base_enable_flag);
		pr_info("base_param_m_p = %d\n",
			dbg_cuva_emds_pkt.base_param_m_p);
		pr_info("base_param_m_m = %d\n",
			dbg_cuva_emds_pkt.base_param_m_m);
		pr_info("base_param_m_a = %d\n",
			dbg_cuva_emds_pkt.base_param_m_a);
		pr_info("base_param_m_b = %d\n",
			dbg_cuva_emds_pkt.base_param_m_b);
		pr_info("base_param_m_n = %d\n",
			dbg_cuva_emds_pkt.base_param_m_n);
		pr_info("base_param_k[0] = %d\n",
			dbg_cuva_emds_pkt.base_param_k[0]);
		pr_info("base_param_k[1] = %d\n",
			dbg_cuva_emds_pkt.base_param_k[1]);
		pr_info("base_param_k[2] = %d\n",
			dbg_cuva_emds_pkt.base_param_k[2]);
		pr_info("base_param_delta_enable_mode = %d\n",
			dbg_cuva_emds_pkt.base_param_delta_enable_mode);
		pr_info("base_param_enable_delta = %d\n",
			dbg_cuva_emds_pkt.base_param_enable_delta);
		pr_info("_3spline_enable_num = %d\n",
			dbg_cuva_emds_pkt._3spline_enable_num);
		pr_info("_3spline_enable_flag = %d\n",
			dbg_cuva_emds_pkt._3spline_enable_flag);

		for (i = 0; i < 2; i++) {
			pr_info("****_3spline_data[%d] :\n", i);
			pr_info("th_enable_mode = %d\n",
			dbg_cuva_emds_pkt._3spline_data[i].th_enable_mode);
			pr_info("th_enable_mb = %d\n",
				dbg_cuva_emds_pkt._3spline_data[i].th_enable_mb);
			pr_info("th_enable = %d\n",
				dbg_cuva_emds_pkt._3spline_data[i].th_enable);
			pr_info("th_enable_delta[0] = %d\n",
				dbg_cuva_emds_pkt._3spline_data[i].th_enable_delta[0]);
			pr_info("th_enable_delta[1] = %d\n",
				dbg_cuva_emds_pkt._3spline_data[i].th_enable_delta[1]);
			pr_info("enable_strength = %d\n",
				dbg_cuva_emds_pkt._3spline_data[i].enable_strength);
		}
		pr_info("color_saturation_num = %d\n",
			dbg_cuva_emds_pkt.color_saturation_num);
		for (i = 0; i < 8; i++)
			pr_info("color_saturation_gain[%d] = %d\n",
			i, dbg_cuva_emds_pkt.color_saturation_gain[i]);
		pr_info("graphic_src_display_value = %d\n",
			dbg_cuva_emds_pkt.graphic_src_display_value);
		pr_info("max_display_mastering_lum = %d\n",
			dbg_cuva_emds_pkt.max_display_mastering_lum);
	}
}

