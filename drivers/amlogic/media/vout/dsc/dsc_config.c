// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vout/dsc/dsc.h>
#include "dsc_config.h"
#include "dsc_hw.h"
#include "dsc_drv.h"
#include "dsc_debug.h"
#include "rc_tables.h"

//{120000, 330, 83, 3, 138, 182, 22, 22, 19, 39, 4399, 2249}, //8k 444 bpp12
unsigned int dsc_timing[DSC_TIMING_MAX][DSC_TIMING_VALUE_MAX] = {
//horizontal_valye bpp_value, havon_begin,vavon_bline,vavon_eline,hso_begin,hs_end
//vs_begin,vs_end,vs_bline,vs_eline,hc_vtotal_m1,hc_htotal_m1
{3840, 70625, 52, 94, 4, 16, 24, 22, 22, 12, 22, 2249, 616}, //4k 422 bpp7.0625
{7680, 120000, 280, 92, 2, 88, 132, 22, 22, 10, 20, 2249, 2199}, //8k bpp12
{7680, 99375, 70, 83, 3, 29, 38, 22, 22, 19, 39, 4399, 1660}, //8k bpp9.9375
{7680, 74375, 58, 83, 3, 24, 32, 22, 22, 19, 39, 4399, 1247},//8k bpp7.4375
};

unsigned int encp_timing_bist[DSC_ENCODE_MAX][11] = {
//de_v_end,de_v_begin,vso_bline,vso_eline,de_h_begin,de_h_end,h_total
//hso_len,hback_len
{7680, 60, 0x0, 0x50, 0x10, 0x24, 0x49, 0x7c9, 9000, 176, 592}, //8k 60hz
};

unsigned int encp_timing_video[DSC_ENCODE_MAX][9] = {
//vavon_bline,vavon_eline,havon_begin,havon_end,h_total,hso_len,hback_len,vso_bline,vso_eline
{0x42c, 0x51, 0x51 + 4320, 0x400, 0x410, 1019, 592, 0, 0},//4k 444 120hz bpp7.0625
{0x42c, 0x51, 0x51 + 4320, 0x400, 0x410, 1019, 9000, 176, 592},//4k 422 120hz bpp12
{0x50, 0x112f, 0x47, 0x7c6, 9000, 176, 592, 16, 36},//8k 444 60hz bpp9.9375
};

static inline void range_check(char *s, u32 a, u32 b, u32 c)
{
	if (((a) < (b)) || ((a) > (c)))
		DSC_ERR("%s out of range, needs to be between %d and %d\n", s, b, c);
}

static inline u8 dsc_clamp(u8 x, u8 min, u8 max)
{
	return ((x) > (max) ? (max) : ((x) < (min) ? (min) : (x)));
}

/*!
 ************************************************************************
 * \brief
 *    compute_offset() - Compute offset value at a specific group
 *
 * \param dsc_drv
 *    Pointer to DSC configuration structure
 * \param pixels_per_group
 *    Number of pixels per group
 * \param groups_per_line
 *    Number of groups per line
 * \param grp_cnt
 *    Group to compute offset for
 * \return
 *    Offset value for the group grp_cnt
 ************************************************************************
 */
static int compute_offset(struct aml_dsc_drv_s *dsc_drv, int pixels_per_group,
				int groups_per_line, int grp_cnt)
{
	int offset = 0;
	int grp_cnt_id;
	int bits_per_pixel_multiple = dsc_drv->bits_per_pixel_multiple;

	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("[%s]bits_per_pixel_multiple:%d\n", __func__, bits_per_pixel_multiple);

	grp_cnt_id = DIV_ROUND_UP(dsc_drv->pps_data.initial_xmit_delay, pixels_per_group);
	if (grp_cnt <= grp_cnt_id)
		offset = DIV_ROUND_UP(grp_cnt * pixels_per_group * bits_per_pixel_multiple,
						BPP_FRACTION);
	else
		offset = DIV_ROUND_UP(grp_cnt_id * pixels_per_group * bits_per_pixel_multiple,
				BPP_FRACTION) -
				(((grp_cnt - grp_cnt_id) * dsc_drv->pps_data.slice_bpg_offset) >>
				OFFSET_FRACTIONAL_BITS);
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("[%s]offset:%d\n", __func__, offset);

	if (grp_cnt <= groups_per_line)
		offset += grp_cnt * dsc_drv->pps_data.first_line_bpg_offset;
	else
		offset += groups_per_line * dsc_drv->pps_data.first_line_bpg_offset -
			(((grp_cnt - groups_per_line) * dsc_drv->pps_data.nfl_bpg_offset) >>
			OFFSET_FRACTIONAL_BITS);
	if (dsc_drv->pps_data.native_420) {
		if (grp_cnt <= groups_per_line)
			offset -= (grp_cnt * dsc_drv->pps_data.nsl_bpg_offset) >>
					OFFSET_FRACTIONAL_BITS;
		else if (grp_cnt <= 2 * groups_per_line)
			offset += (grp_cnt - groups_per_line) *
				dsc_drv->pps_data.second_line_bpg_offset -
				((groups_per_line * dsc_drv->pps_data.nsl_bpg_offset) >>
					OFFSET_FRACTIONAL_BITS);
		else
			offset += (grp_cnt - groups_per_line) *
				dsc_drv->pps_data.second_line_bpg_offset -
				(((grp_cnt - groups_per_line) * dsc_drv->pps_data.nsl_bpg_offset) >>
				OFFSET_FRACTIONAL_BITS);
	}
	return offset;
}

/*!
 ************************************************************************
 * \brief
 *    compute_rc_parameters() - Compute rate control parameters
 *
 * \param dsc_drv
 *    Pointer to DSC configuration structure
 * \param pixels_per_group
 *    Number of pixels per group
 * \param groups_per_line
 *    Number of groups per line
 * \param groups per cnt
 *    Group to compute offset for
 * \return
 *    0 = success; 1 = error with configuration
 ************************************************************************
 */
static int compute_rc_parameters(struct aml_dsc_drv_s *dsc_drv, int pixels_per_group, int num_s_sps)
{
	int groups_per_line;
	int num_extra_mux_bits;
	int slice_bits;
	int final_value;
	int final_scale;
	int invalid = 0;
	int groups_total;
	int max_offset;
	int rbs_min;
	int hrd_delay;
	int slice_w;
	int uncompressed_bpg_rate;
	int initial_xmit_multi_bpp;
	int bits_per_pixel_multiple = dsc_drv->bits_per_pixel_multiple;
	int rc_model_size = dsc_drv->pps_data.rc_parameter_set.rc_model_size;

	slice_w = dsc_drv->pps_data.slice_width >> (dsc_drv->pps_data.native_420 ||
					dsc_drv->pps_data.native_422);
	if (dsc_drv->pps_data.native_422)
		uncompressed_bpg_rate = 3 * dsc_drv->pps_data.bits_per_component * 4;
	else
		uncompressed_bpg_rate = (3 * dsc_drv->pps_data.bits_per_component +
					(!dsc_drv->pps_data.convert_rgb ? 0 : 2)) * 3;

	if (dsc_drv->pps_data.slice_height >= 8)
		dsc_drv->pps_data.first_line_bpg_offset = 12 +
			(9 * BPP_FRACTION / 100 * MIN(34, dsc_drv->pps_data.slice_height - 8)) /
									BPP_FRACTION;
	else
		dsc_drv->pps_data.first_line_bpg_offset = 2 * (dsc_drv->pps_data.slice_height - 1);
	dsc_drv->pps_data.first_line_bpg_offset = dsc_clamp(dsc_drv->pps_data.first_line_bpg_offset,
			0, ((uncompressed_bpg_rate * BPP_FRACTION - 3 * bits_per_pixel_multiple) /
									BPP_FRACTION));
	range_check("first_line_bpg_offset", dsc_drv->pps_data.first_line_bpg_offset, 0, 31);
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("first_line_bpg_offset:%d\n", dsc_drv->pps_data.first_line_bpg_offset);

	dsc_drv->pps_data.second_line_bpg_offset = dsc_drv->pps_data.native_420 ? 12 : 0;
	dsc_drv->pps_data.second_line_bpg_offset =
				dsc_clamp(dsc_drv->pps_data.second_line_bpg_offset,
					0, ((uncompressed_bpg_rate * BPP_FRACTION - 3 *
					bits_per_pixel_multiple) / BPP_FRACTION));
	range_check("second_line_bpg_offset", dsc_drv->pps_data.second_line_bpg_offset, 0, 31);
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("second_line_bpg_offset:%d\n", dsc_drv->pps_data.second_line_bpg_offset);

	groups_per_line = (slice_w + pixels_per_group - 1) / pixels_per_group;
	dsc_drv->pps_data.chunk_size = (int)DIV_ROUND_UP(slice_w * bits_per_pixel_multiple,
									(8 * BPP_FRACTION));
	range_check("chunk_size", dsc_drv->pps_data.chunk_size, 0, 65535);
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("chunk_size:%d\n", dsc_drv->pps_data.chunk_size);

	if (dsc_drv->pps_data.convert_rgb)
		num_extra_mux_bits = (num_s_sps * (dsc_drv->mux_word_size +
			(4 * dsc_drv->pps_data.bits_per_component + 4) - 2));
	else if (!dsc_drv->pps_data.native_422) // YCbCr
		num_extra_mux_bits = (num_s_sps * dsc_drv->mux_word_size +
			(4 * dsc_drv->pps_data.bits_per_component + 4) +
			2 * (4 * dsc_drv->pps_data.bits_per_component) - 2);
	else
		num_extra_mux_bits = (num_s_sps * dsc_drv->mux_word_size +
			(4 * dsc_drv->pps_data.bits_per_component + 4) + 3 *
			(4 * dsc_drv->pps_data.bits_per_component) - 2);
	slice_bits = 8 * dsc_drv->pps_data.chunk_size * dsc_drv->pps_data.slice_height;
	while ((num_extra_mux_bits > 0) &&
	       ((slice_bits - num_extra_mux_bits) % dsc_drv->mux_word_size))
		num_extra_mux_bits--;

	dsc_drv->pps_data.initial_scale_value = 8 * rc_model_size /
				(rc_model_size - dsc_drv->pps_data.initial_offset);
	if (groups_per_line < dsc_drv->pps_data.initial_scale_value - 8)
		dsc_drv->pps_data.initial_scale_value = groups_per_line + 8;
	range_check("initial_scale_value", dsc_drv->pps_data.initial_scale_value, 0, 63);

	if (dsc_drv->pps_data.initial_scale_value > 8)
		dsc_drv->pps_data.scale_decrement_interval = groups_per_line /
						(dsc_drv->pps_data.initial_scale_value - 8);
	else
		dsc_drv->pps_data.scale_decrement_interval = 4095;
	range_check("scale_decrement_interval",
		dsc_drv->pps_data.scale_decrement_interval, 0, 4095);

	final_value = rc_model_size -
		((dsc_drv->pps_data.initial_xmit_delay *
			dsc_drv->pps_data.bits_per_pixel + 8) >> 4) + num_extra_mux_bits;
	dsc_drv->pps_data.final_offset = final_value;
	range_check("final_offset", dsc_drv->pps_data.final_offset, 0, 65535);

	if (final_value >= rc_model_size)
		DSC_ERR("final_offset must be less than rc_model_size add initial_xmit_delay.\n");
	final_scale = 8 * rc_model_size /
			(rc_model_size - final_value);
	if (final_scale > 63)
		DSC_ERR("WARNING: final scale value > than 63/8 increasing initial_xmit_delay.\n");
	if (dsc_drv->pps_data.slice_height > 1)
		dsc_drv->pps_data.nfl_bpg_offset =
			(int)DIV_ROUND_UP(dsc_drv->pps_data.first_line_bpg_offset <<
				OFFSET_FRACTIONAL_BITS, (dsc_drv->pps_data.slice_height - 1));
	else
		dsc_drv->pps_data.nfl_bpg_offset = 0;

	if (dsc_drv->pps_data.nfl_bpg_offset > 65535) {
		DSC_ERR("nfl_bpg_offset is too large for this slice height\n");
		invalid = 1;
	}
	if (dsc_drv->pps_data.slice_height > 2)
		dsc_drv->pps_data.nsl_bpg_offset =
			(int)DIV_ROUND_UP(dsc_drv->pps_data.second_line_bpg_offset <<
					OFFSET_FRACTIONAL_BITS,
					(dsc_drv->pps_data.slice_height - 1));
	else
		dsc_drv->pps_data.nsl_bpg_offset = 0;

	if (dsc_drv->pps_data.nsl_bpg_offset > 65535) {
		DSC_ERR("nsl_bpg_offset is too large for this slice height\n");
		invalid = 1;
	}
	groups_total = groups_per_line * dsc_drv->pps_data.slice_height;
	dsc_drv->pps_data.slice_bpg_offset = (int)DIV_ROUND_UP((1 << OFFSET_FRACTIONAL_BITS) *
				(rc_model_size -
				dsc_drv->pps_data.initial_offset + num_extra_mux_bits),
				groups_total);
	range_check("slice_bpg_offset", dsc_drv->pps_data.slice_bpg_offset, 0, 65535);
	if (dsc_drv->pps_data.slice_height == 1) {
		if (dsc_drv->pps_data.first_line_bpg_offset > 0)
			DSC_ERR("For slice_height == 1, the FIRST_LINE_BPG_OFFSET must be 0\n");
	}

	// BEGIN scale_increment_interval fix
	if (final_scale > 9) {
		dsc_drv->pps_data.scale_increment_interval = (int)((1 << OFFSET_FRACTIONAL_BITS) *
					dsc_drv->pps_data.final_offset /
					((final_scale - 9) * (dsc_drv->pps_data.nfl_bpg_offset +
					 dsc_drv->pps_data.slice_bpg_offset +
					 dsc_drv->pps_data.nsl_bpg_offset)));
		if (dsc_drv->pps_data.scale_increment_interval > 65535) {
			DSC_ERR("scale_increment_interval is too large for this slice height.\n");
			invalid = 1;
		}
	} else {
		dsc_drv->pps_data.scale_increment_interval = 0;
	}
	// END scale_increment_interval fix

	if (dsc_drv->pps_data.dsc_version_minor == 2 &&
	   (dsc_drv->pps_data.native_420 || dsc_drv->pps_data.native_422)) {
		max_offset = compute_offset(dsc_drv, pixels_per_group, groups_per_line,
			DIV_ROUND_UP(dsc_drv->pps_data.initial_xmit_delay, pixels_per_group));
		max_offset = MAX(max_offset, compute_offset(dsc_drv, pixels_per_group,
						groups_per_line, groups_per_line));
		max_offset = MAX(max_offset, compute_offset(dsc_drv, pixels_per_group,
						groups_per_line, 2 * groups_per_line));
		rbs_min = rc_model_size - dsc_drv->pps_data.initial_offset + max_offset;
	} else {  // DSC 1.1 method
		initial_xmit_multi_bpp =
			DIV_ROUND_UP(dsc_drv->pps_data.initial_xmit_delay * bits_per_pixel_multiple,
									BPP_FRACTION);
		//DSC_ERR("initial_xmit_multi_bpp:%d\n", initial_xmit_multi_bpp);
		rbs_min = (int)(rc_model_size - dsc_drv->pps_data.initial_offset +
			initial_xmit_multi_bpp +
			groups_per_line * dsc_drv->pps_data.first_line_bpg_offset);
		if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
			DSC_PR("initial_xmit_multi_bpp:%d rbs_min:%d\n",
				initial_xmit_multi_bpp, rbs_min);
	}

	hrd_delay = DIV_ROUND_UP(rbs_min * BPP_FRACTION, bits_per_pixel_multiple);
	dsc_drv->rcb_bits = DIV_ROUND_UP(hrd_delay * bits_per_pixel_multiple, BPP_FRACTION);
	dsc_drv->pps_data.initial_dec_delay = hrd_delay - dsc_drv->pps_data.initial_xmit_delay;
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("hrd_delay:%d rcb_bits:%d\n", hrd_delay, dsc_drv->rcb_bits);

	return invalid;
}

int qlevel_luma_8bpc[] = {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5, 6, 7};
int qlevel_chroma_8bpc[] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8};
int qlevel_luma_10bpc[] = {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7, 8, 9};
int qlevel_chroma_10bpc[] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 10, 10, 10};
int qlevel_luma_12bpc[] = {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9,
				10, 11};
int qlevel_chroma_12bpc[] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
				12, 12, 12};
int qlevel_luma_14bpc[] =   {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,  9,  9,
				10, 10, 11, 11, 11, 12, 13};
int qlevel_chroma_14bpc[] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
				11, 12, 12, 13, 14, 14, 14};
int qlevel_luma_16bpc[] =   {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,  9,  9,
				10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 15};
int qlevel_chroma_16bpc[] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
				11, 12, 12, 13, 13, 14, 14, 15, 16, 16, 16};

static int Qp2Qlevel(struct aml_dsc_drv_s *dsc_drv, int qp, int cpnt)
{
	int qlevel = 0;

	if (((cpnt % 3) == 0) || (dsc_drv->pps_data.native_420 && cpnt == 1)) {
		switch (dsc_drv->pps_data.bits_per_component) {
		case 8:
			qlevel = qlevel_luma_8bpc[qp];
			break;
		case 10:
			qlevel = qlevel_luma_10bpc[qp];
			break;
		case 12:
			qlevel = qlevel_luma_12bpc[qp];
			break;
		case 14:
			qlevel = qlevel_luma_14bpc[qp];
			break;
		case 16:
			qlevel = qlevel_luma_16bpc[qp];
			break;
		}
	} else {
		switch (dsc_drv->pps_data.bits_per_component) {
		case 8:
			qlevel = qlevel_chroma_8bpc[qp];
			break;
		case 10:
			qlevel = qlevel_chroma_10bpc[qp];
			break;
		case 12:
			qlevel = qlevel_chroma_12bpc[qp];
			break;
		case 14:
			qlevel = qlevel_chroma_14bpc[qp];
			break;
		case 16:
			qlevel = qlevel_chroma_16bpc[qp];
			break;
		}
		if (dsc_drv->pps_data.dsc_version_minor == 2 && !dsc_drv->pps_data.convert_rgb)
			qlevel = MAX(0, qlevel - 1);
	}

	return (qlevel);
}

/*!
 ************************************************************************
 * \brief
 *    check_qp_for_overflow() - Ensure max QP's are programmed to avoid overflow
 *
 * \param dsc_cfg
 *    Pointer to DSC configuration structure
 * \param pixel_per_group
 *    Number of pixels per group
 * \return
 *    0 = success; 1 = error with configuration
 ************************************************************************
 */
void check_qp_for_overflow(struct aml_dsc_drv_s *dsc_drv, int pixel_per_group)
{
	int p_mode_bits, max_bits;
	int cpnt, max_res_size;
	int cpntBitDepth[NUM_COMPONENTS];
	struct dsc_rc_parameter_set *rc_parameter_set;

	rc_parameter_set = &dsc_drv->pps_data.rc_parameter_set;
	for (cpnt = 0; cpnt < NUM_COMPONENTS; ++cpnt)
		cpntBitDepth[cpnt] = (dsc_drv->pps_data.convert_rgb && (cpnt == 1 || cpnt == 2)) ?
			(dsc_drv->pps_data.bits_per_component + 1) :
			dsc_drv->pps_data.bits_per_component;

	// MPP group when predicted size is 0
	p_mode_bits = 1;  // extra bit for luma prefix/ICH switch
	for (cpnt = 0; cpnt < (dsc_drv->pps_data.native_422 ? 4 : 3); ++cpnt) {
		max_res_size = cpntBitDepth[cpnt] -
			Qp2Qlevel(dsc_drv, rc_parameter_set->rc_range_parameters[14].range_max_qp,
				  cpnt);
		p_mode_bits += max_res_size * 4;  // prefix + residuals
	}

	// Followed by predicted group (where predicted size is max size-1)
	max_bits = p_mode_bits;
	p_mode_bits = 1;
	for (cpnt = 0; cpnt < (dsc_drv->pps_data.native_422 ? 4 : 3); ++cpnt) {
		max_res_size = cpntBitDepth[cpnt] -
		Qp2Qlevel(dsc_drv, rc_parameter_set->rc_range_parameters[14].range_max_qp,
										cpnt) - 1;
		p_mode_bits += 1 + max_res_size * 3;  // prefix (1bit) + residuals
	}
	max_bits += p_mode_bits;
}

/*!
 * \brief
 *    generate_rc_parameters() - Generate RC parameters
 *
 * \param dsc_drv
 *    PPS data structure
 ************************************************************************
 */
static void generate_rc_parameters(struct aml_dsc_drv_s *dsc_drv)
{
	struct dsc_rc_parameter_set *rc_parameter_set;
	int qp_bpc_modifier;
	int i;
	int padding_pixels;
	int sw;
	int bits_per_pixel_int = dsc_drv->bits_per_pixel_int;
	int bits_per_pixel_multiple = dsc_drv->bits_per_pixel_multiple;
	int slices;
	u8 bits_per_component;

	rc_parameter_set = &dsc_drv->pps_data.rc_parameter_set;
	bits_per_component = dsc_drv->pps_data.bits_per_component;
	make_qp_tables();

	qp_bpc_modifier = (bits_per_component - 8) * 2 -
		(!dsc_drv->pps_data.convert_rgb && (dsc_drv->pps_data.dsc_version_minor == 1));
	rc_parameter_set->rc_quant_incr_limit0 = 11 + qp_bpc_modifier;
	rc_parameter_set->rc_quant_incr_limit1 = 11 + qp_bpc_modifier;

	if (dsc_drv->pps_data.native_422) {
		if (bits_per_pixel_int >= 16)
			dsc_drv->pps_data.initial_offset = 2048;
		else if (bits_per_pixel_int >= 14)
			dsc_drv->pps_data.initial_offset = 5632 -
				(int)(((bits_per_pixel_multiple - 14 * BPP_FRACTION) * 1792 +
				5 * BPP_FRACTION / 10) / BPP_FRACTION);
		else if (bits_per_pixel_int >= 12)
			dsc_drv->pps_data.initial_offset = 5632;
		else
			DSC_PR("No auto-generated parameters bitsPerPixel < 6 in 4:2:2 mode\n");
		if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
			DSC_PR("initial_offset:%d\n", dsc_drv->pps_data.initial_offset);
	} else {// 4:4:4 or simple 4:2:2 or native 4:2:0
		if (bits_per_pixel_int >= 12)
			dsc_drv->pps_data.initial_offset = 2048;
		else if (bits_per_pixel_int >= 10)
			dsc_drv->pps_data.initial_offset = 5632 -
					(int)(((bits_per_pixel_multiple - 10 * BPP_FRACTION) *
						1792 + 5 * BPP_FRACTION / 10) / BPP_FRACTION);
		else if (bits_per_pixel_int >= 8)
			dsc_drv->pps_data.initial_offset = 6144 -
					(int)(((bits_per_pixel_multiple - 8 * BPP_FRACTION) * 256 +
						5 * BPP_FRACTION / 10) / BPP_FRACTION);
		else if (bits_per_pixel_int >= 6)
			dsc_drv->pps_data.initial_offset = 6144;
		else
			DSC_PR("No auto-generated parameters bitsPerPixel < 6 in 4:4:4 mode\n");
		if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
			DSC_PR("initial_offset:%d\n", dsc_drv->pps_data.initial_offset);
	}
	/* 409600000000 = 4096*BPP_FRACTION*BPP_FRACTION */
	dsc_drv->pps_data.initial_xmit_delay =
		((409600000000 / bits_per_pixel_multiple + 5 * BPP_FRACTION / 10) / BPP_FRACTION);
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("initial_xmit_delay:%d\n", dsc_drv->pps_data.initial_xmit_delay);

	sw = ((dsc_drv->pps_data.native_422 || dsc_drv->pps_data.native_420) ?
		(dsc_drv->pps_data.slice_width / 2) : dsc_drv->pps_data.slice_width);
	padding_pixels = ((sw % 3) ? (3 - (sw % 3)) : 0) *
			(dsc_drv->pps_data.initial_xmit_delay / sw);
	if (3 * bits_per_pixel_multiple >=
	    (((dsc_drv->pps_data.initial_xmit_delay + 2) / 3) *
	     (dsc_drv->pps_data.native_422 ? 4 : 3) * BPP_FRACTION) &&
	     (((dsc_drv->pps_data.initial_xmit_delay + padding_pixels) % 3) == 1))
		dsc_drv->pps_data.initial_xmit_delay++;
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("initial_xmit_delay:%d\n", dsc_drv->pps_data.initial_xmit_delay);

	dsc_drv->pps_data.flatness_min_qp = 3 + qp_bpc_modifier;
	dsc_drv->pps_data.flatness_max_qp = 12 + qp_bpc_modifier;
	dsc_drv->flatness_det_thresh = 2 << (bits_per_component - 8);
	// The following two lines were added in 1.57e
	if (dsc_drv->pps_data.native_420)
		dsc_drv->pps_data.second_line_offset_adj = 512;
	else
		dsc_drv->pps_data.second_line_offset_adj = 0;

	for (i = 0; i < 15; ++i) {
		int idx;

		if (dsc_drv->pps_data.native_420) {
			int ofs_und4[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12,
					   -12, -12 };
			int ofs_und5[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12,
					   -12, -12 };
			int ofs_und6[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12,
					   -12, -12 };
			int ofs_und8[] = { 10, 8, 6, 4, 2, 0, -2, -4, -6, -8, -10, -10, -12,
					   -12, -12 };

			idx = (int)((bits_per_pixel_multiple - 8 * BPP_FRACTION) / BPP_FRACTION);
			if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
				DSC_PR("idx:%d\n", idx);
			rc_parameter_set->rc_range_parameters[i].range_min_qp =
					min_qp_420[(bits_per_component - 8) / 2][i][idx];
			rc_parameter_set->rc_range_parameters[i].range_max_qp =
					max_qp_420[(bits_per_component - 8) / 2][i][idx];
			if (bits_per_pixel_multiple <= 8 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und4[i];
			else if (bits_per_pixel_multiple <= 10 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und4[i] +
					(int)(((5 * (bits_per_pixel_multiple - 8 * BPP_FRACTION) /
					10) * (ofs_und5[i] - ofs_und4[i]) +
					5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else if (bits_per_pixel_multiple <= 12 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und5[i] +
					(int)(((5 * (bits_per_pixel_multiple - 10 * BPP_FRACTION) /
					10) * (ofs_und6[i] - ofs_und5[i]) +
					5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else if (bits_per_pixel_multiple <= 16 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und6[i] +
					(int)(((25 * (bits_per_pixel_multiple - 12 * BPP_FRACTION) /
					100) * (ofs_und8[i] - ofs_und6[i]) +
					5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und8[i];
			if (dsc_drv->dsc_print_en & DSC_PPS_QP_VALUE)
				DSC_PR("min_pq_420:%d max_pq_420:%d range_bgp_offset:%d\n",
					min_qp_420[(bits_per_component - 8) / 2][i][idx],
					max_qp_420[(bits_per_component - 8) / 2][i][idx],
					rc_parameter_set->rc_range_parameters[i].range_bpg_offset);
		} else if (dsc_drv->pps_data.native_422) {
			int ofs_und6[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12,
					   -12, -12 };
			int ofs_und7[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12,
					   -12, -12 };
			int ofs_und10[] = { 10, 8, 6, 4, 2, 0, -2, -4, -6, -8, -10, -10, -12,
					    -12, -12 };
			idx = (int)((bits_per_pixel_multiple - 12 * BPP_FRACTION) / BPP_FRACTION);
			if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
				DSC_PR("idx:%d\n", idx);
			rc_parameter_set->rc_range_parameters[i].range_min_qp =
					min_qp_422[(bits_per_component - 8) / 2][i][idx];
			rc_parameter_set->rc_range_parameters[i].range_max_qp =
					max_qp_422[(bits_per_component - 8) / 2][i][idx];
			if (bits_per_pixel_multiple <= 12 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und6[i];
			else if (bits_per_pixel_multiple <= 14 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und6[i] +
					(int)(((bits_per_pixel_multiple - 12 * BPP_FRACTION) *
					(ofs_und7[i] - ofs_und6[i]) / 2 +
					5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else if (bits_per_pixel_multiple <= 16 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und7[i];
			else if (bits_per_pixel_multiple <= 20 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und7[i] +
					(int)(((bits_per_pixel_multiple - 16 * BPP_FRACTION) *
					(ofs_und10[i] - ofs_und7[i]) / 4 +
					5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und10[i];
			if (dsc_drv->dsc_print_en & DSC_PPS_QP_VALUE)
				DSC_PR("min_pq_422:%d max_pq_422:%d range_bgp_offset:%d\n",
					min_qp_422[(bits_per_component - 8) / 2][i][idx],
					max_qp_422[(bits_per_component - 8) / 2][i][idx],
					rc_parameter_set->rc_range_parameters[i].range_bpg_offset);
		} else {
			int ofs_und6[] = { 0, -2, -2, -4, -6, -6, -8, -8, -8, -10, -10, -12, -12,
					   -12, -12 };
			int ofs_und8[] = { 2, 0, 0, -2, -4,	-6,	-8,	-8, -8, -10, -10,
					   -10, -12, -12, -12 };
			int ofs_und12[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12,
					    -12, -12 };
			int ofs_und15[] = { 10, 8, 6, 4, 2, 0, -2, -4, -6, -8, -10, -10, -12,
					    -12, -12 };
			idx = (int)(2 * (bits_per_pixel_multiple - 6 * BPP_FRACTION) /
					BPP_FRACTION);
			if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
				DSC_PR("idx:%d\n", idx);
			rc_parameter_set->rc_range_parameters[i].range_min_qp =
				MAX(0, min_qp_444[(bits_per_component - 8) / 2][i][idx] -
					(!dsc_drv->pps_data.convert_rgb &&
					 (dsc_drv->pps_data.dsc_version_minor == 1)));
			rc_parameter_set->rc_range_parameters[i].range_max_qp =
				MAX(0, max_qp_444[(bits_per_component - 8) / 2][i][idx] -
					(!dsc_drv->pps_data.convert_rgb &&
					 (dsc_drv->pps_data.dsc_version_minor == 1)));
			if (bits_per_pixel_multiple <= 6 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und6[i];
			else if (bits_per_pixel_multiple <= 8 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und6[i] +
					(int)(((5 * (bits_per_pixel_multiple - 6 * BPP_FRACTION) /
						10) * (ofs_und8[i] - ofs_und6[i]) +
						5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else if (bits_per_pixel_multiple <= 12 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und8[i];
			else if (bits_per_pixel_multiple <= 15 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und12[i] +
					(int)(((bits_per_pixel_multiple - 12 * BPP_FRACTION) *
						(ofs_und15[i] - ofs_und12[i]) / 3 +
						5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und15[i];
			if (dsc_drv->dsc_print_en & DSC_PPS_QP_VALUE)
				DSC_PR("min_pq_444:%d max_pq_444:%d range_bgp_offset:%d\n",
				min_qp_444[(bits_per_component - 8) / 2][i][idx],
				max_qp_444[(bits_per_component - 8) / 2][i][idx],
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset);
		}
		// The following code was added in 1.57g (parameter sanity check)
		range_check("range_max_qp",
			rc_parameter_set->rc_range_parameters[i].range_max_qp, 0,
				15 + 2 * (bits_per_component - 8));
		if (dsc_drv->pps_data.dsc_version_minor == 1 && !dsc_drv->pps_data.convert_rgb &&
		    rc_parameter_set->rc_range_parameters[i].range_max_qp > 12 +
			2 * (bits_per_component - 8))
			DSC_PR("ERROR:DSC 1.1 mode with YCbCr max QP for range %d less than %d\n",
				i, 12 + 2 * (bits_per_component - 8));
	}
	destroy_qp_tables();

	if (dsc_drv->color_format == HDMI_COLORSPACE_YUV420 ||
	    dsc_drv->color_format == HDMI_COLORSPACE_YUV422)
		bits_per_pixel_multiple = bits_per_pixel_multiple / 2;
	slices = dsc_drv->pps_data.pic_width / dsc_drv->pps_data.slice_width;
	dsc_drv->pps_data.hc_active_bytes = slices * DIV_ROUND_UP(dsc_drv->pps_data.slice_width *
						bits_per_pixel_multiple, 8 * BPP_FRACTION);
}

static void populate_pps(struct aml_dsc_drv_s *dsc_drv)
{
	int i;
	int pixels_per_group, num_s_sps;
	struct dsc_pps_data_s *pps_data = &dsc_drv->pps_data;
	int default_threshold[] = {896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744,
					7872, 8000, 8064};

	for (i = 0; i < RC_BUF_THRESH_NUM; i++)
		pps_data->rc_parameter_set.rc_buf_thresh[i] = default_threshold[i] >> 6;

	if (pps_data->native_422 && pps_data->native_420)
		DSC_PR("NATIVE_420 and NATIVE_422 modes cannot both be enabled at the same time\n");

	if (pps_data->bits_per_component != 8 && pps_data->bits_per_component != 10 &&
	    pps_data->bits_per_component != 12 && pps_data->bits_per_component != 14 &&
	    pps_data->bits_per_component != 16)
		DSC_PR("bits_per_component must be either 8, 10, 12, 14, or 16\n");

	// Removed from PPS in v1.44:
	dsc_drv->very_flat_qp = 1 + (2 * (pps_data->bits_per_component - 8));
	dsc_drv->somewhat_flat_qp_delta = 4;
	dsc_drv->somewhat_flat_qp_thresh = 7 + (2 * (pps_data->bits_per_component - 8));
	if (pps_data->bits_per_component <= 10)
		dsc_drv->mux_word_size = 48;
	else
		dsc_drv->mux_word_size = 64;

	// The following line was added in v1.57g:

	if (pps_data->rc_parameter_set.rc_model_size <= pps_data->initial_offset)
		DSC_PR("INITIAL_OFFSET must be less than RC_MODEL_SIZE\n");
	pps_data->initial_scale_value =
		8 * pps_data->rc_parameter_set.rc_model_size /
			(pps_data->rc_parameter_set.rc_model_size - pps_data->initial_offset);
	range_check("initial_scale_value", pps_data->initial_scale_value, 0, 63);

	// Compute rate buffer size for auto mode
	if (pps_data->native_422) {
		num_s_sps = 4;
		pixels_per_group = 3;
	} else {
		num_s_sps = 3;
		pixels_per_group = 3;
	}

	generate_rc_parameters(dsc_drv);

	if (compute_rc_parameters(dsc_drv, pixels_per_group, num_s_sps))
		DSC_PR("One or more PPS parameters exceeded their allowed bit depth.");
	check_qp_for_overflow(dsc_drv, pixels_per_group);
}

static void dsc_config_timing_value(struct aml_dsc_drv_s *dsc_drv)
{
	int i;
	unsigned int dsc_timing_mode = 0;
	enum dsc_encode_mode dsc_mode = dsc_drv->dsc_mode;
	int slice_num = 4;

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_DSC_TMG_CTRL)) {
		for (i = 0; i < DSC_TIMING_MAX; i++) {
			if (dsc_timing[i][0] == dsc_drv->pps_data.pic_width &&
			    dsc_timing[i][1] == dsc_drv->bits_per_pixel_really_value)
				dsc_timing_mode = i;
		}
		//dsc timing set
		dsc_drv->tmg_ctrl.tmg_havon_begin	= dsc_timing[dsc_timing_mode][2];
		dsc_drv->tmg_ctrl.tmg_vavon_bline	= dsc_timing[dsc_timing_mode][3];
		dsc_drv->tmg_ctrl.tmg_vavon_eline	= dsc_timing[dsc_timing_mode][4];
		dsc_drv->tmg_ctrl.tmg_hso_begin		= dsc_timing[dsc_timing_mode][5];
		dsc_drv->tmg_ctrl.tmg_hso_end		= dsc_timing[dsc_timing_mode][6];
		dsc_drv->tmg_ctrl.tmg_vso_begin		= dsc_timing[dsc_timing_mode][7];
		dsc_drv->tmg_ctrl.tmg_vso_end		= dsc_timing[dsc_timing_mode][8];
		dsc_drv->tmg_ctrl.tmg_vso_bline		= dsc_timing[dsc_timing_mode][9];
		dsc_drv->tmg_ctrl.tmg_vso_eline		= dsc_timing[dsc_timing_mode][10];
		dsc_drv->hc_vtotal_m1			= dsc_timing[dsc_timing_mode][11];
		dsc_drv->hc_htotal_m1			= dsc_timing[dsc_timing_mode][12];
	}

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_VPU_BIST_TMG_CTRL)) {
		//encp bist timing set
		dsc_drv->encp_timing_ctrl.encp_de_v_end = encp_timing_bist[dsc_mode][0];
		dsc_drv->encp_timing_ctrl.encp_de_v_begin = encp_timing_bist[dsc_mode][1];
		dsc_drv->encp_timing_ctrl.encp_vso_bline = encp_timing_bist[dsc_mode][2];
		dsc_drv->encp_timing_ctrl.encp_vso_eline = encp_timing_bist[dsc_mode][3];
		dsc_drv->encp_timing_ctrl.encp_de_h_begin = encp_timing_bist[dsc_mode][4];
		dsc_drv->encp_timing_ctrl.encp_de_h_end	= encp_timing_bist[dsc_mode][5];
		dsc_drv->encp_timing_ctrl.encp_dvi_hso_begin = encp_timing_bist[dsc_mode][4] +
			encp_timing_bist[dsc_mode][6] / slice_num -
			encp_timing_bist[dsc_mode][7] / slice_num -
			encp_timing_bist[dsc_mode][8] / slice_num;
		dsc_drv->encp_timing_ctrl.encp_dvi_hso_end =
				dsc_drv->encp_timing_ctrl.encp_dvi_hso_begin +
				encp_timing_bist[dsc_mode][7] / slice_num;
	}

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_VPU_VIDEO_TMG_CTRL)) {
		//encp video timing set
		dsc_drv->encp_timing_ctrl.encp_vavon_bline = encp_timing_video[dsc_mode][0];
		dsc_drv->encp_timing_ctrl.encp_vavon_eline = encp_timing_video[dsc_mode][1];
		dsc_drv->encp_timing_ctrl.encp_havon_begin = encp_timing_video[dsc_mode][2];
		dsc_drv->encp_timing_ctrl.encp_havon_end = encp_timing_video[dsc_mode][3];

		dsc_drv->encp_timing_ctrl.encp_hso_begin = encp_timing_video[dsc_mode][2] +
			encp_timing_video[dsc_mode][4] / slice_num -
			encp_timing_video[dsc_mode][5] / slice_num -
			encp_timing_video[dsc_mode][6] / slice_num;
		dsc_drv->encp_timing_ctrl.encp_hso_end =
			dsc_drv->encp_timing_ctrl.encp_hso_begin +
			encp_timing_video[dsc_mode][5] / slice_num;

		dsc_drv->encp_timing_ctrl.encp_video_vso_bline = encp_timing_video[dsc_mode][7];
		dsc_drv->encp_timing_ctrl.encp_video_vso_eline = encp_timing_video[dsc_mode][8];
	}

	DSC_PR("[%s] dsc_timing_mode:%d dsc_mode:%d\n", __func__, dsc_timing_mode, dsc_mode);
}

static void calculate_asic_data(struct aml_dsc_drv_s *dsc_drv)
{
	int slice_num;

	slice_num = dsc_drv->pps_data.pic_width / dsc_drv->pps_data.slice_width;
	dsc_drv->slice_num_m1 = slice_num - 1;
	dsc_drv->dsc_enc_frm_latch_en		= 0;
	dsc_drv->pix_per_clk			= 2;
	dsc_drv->inbuf_rd_dly0			= 8;
	dsc_drv->inbuf_rd_dly1			= 44;
	dsc_drv->c3_clk_en			= 1;
	dsc_drv->c2_clk_en			= 1;
	dsc_drv->c1_clk_en			= 1;
	dsc_drv->c0_clk_en			= 1;
	if (slice_num == 8)
		dsc_drv->slices_in_core = 1;

	if (dsc_drv->pps_data.convert_rgb ||
	    (!dsc_drv->pps_data.native_422 && !dsc_drv->pps_data.native_420))
		dsc_drv->slice_group_number =
			dsc_drv->pps_data.slice_width / 3 * dsc_drv->pps_data.slice_height;
	else if (dsc_drv->pps_data.native_422 || dsc_drv->pps_data.native_420)
		dsc_drv->slice_group_number =
			dsc_drv->pps_data.slice_width / 6 * dsc_drv->pps_data.slice_height;

	dsc_drv->partial_group_pix_num		= 3;
	dsc_drv->chunk_6byte_num_m1 = DIV_ROUND_UP(dsc_drv->pps_data.chunk_size, 6) - 1;

	dsc_config_timing_value(dsc_drv);

	dsc_drv->pix_in_swap0			= 0x67534201;
	dsc_drv->pix_in_swap1			= 0xb9a8;
	dsc_drv->last_slice_active_m1		= dsc_drv->pps_data.slice_width - 1;
	dsc_drv->gclk_ctrl			= 0;
	dsc_drv->c0s1_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c0s0_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c1s1_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c1s0_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c2s1_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c2s0_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c3s1_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c3s0_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->out_swap			= 0x543210;
	dsc_drv->intr_maskn			= 0;

	dsc_drv->hs_odd_no_tail			= 0;
	dsc_drv->hs_odd_no_head			= 0;
	dsc_drv->hs_even_no_tail		= 0;
	dsc_drv->hs_even_no_head		= 0;
	dsc_drv->vs_odd_no_tail			= 0;
	dsc_drv->vs_odd_no_head			= 0;
	dsc_drv->vs_even_no_tail		= 0;
	dsc_drv->vs_even_no_head		= 0;
	dsc_drv->hc_active_odd_mode		= 0;
	dsc_drv->hc_htotal_offs_oddline		= 0;
	dsc_drv->hc_htotal_offs_evenline		= 0;
}

void init_pps_data(struct aml_dsc_drv_s *dsc_drv)
{
	struct dsc_pps_data_s *pps_data = &dsc_drv->pps_data;

	dsc_drv->pps_data.dsc_version_minor = 2;

	if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320 &&
	    dsc_drv->fps == 60 && dsc_drv->color_format == HDMI_COLORSPACE_YUV420)
		dsc_drv->dsc_mode = DSC_YUV420_7680X4320_60HZ;
	else if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320 &&
		   dsc_drv->fps == 60 && dsc_drv->color_format == HDMI_COLORSPACE_YUV444)
		dsc_drv->dsc_mode = DSC_YUV444_7680X4320_60HZ;
	else if (pps_data->pic_width == 3840 && pps_data->pic_height == 2160 &&
		   dsc_drv->fps == 60 && dsc_drv->color_format == HDMI_COLORSPACE_YUV444)
		dsc_drv->dsc_mode = DSC_YUV444_3840X2160_60HZ;
	else if (pps_data->pic_width == 3840 && pps_data->pic_height == 2160 &&
		   dsc_drv->fps == 120 && dsc_drv->color_format == HDMI_COLORSPACE_YUV422)
		dsc_drv->dsc_mode = DSC_YUV422_3840X2160_120HZ;
	else
		dsc_drv->dsc_mode = DSC_ENCODE_MAX;

	/* set color_foramt */
	if (dsc_drv->color_format == HDMI_COLORSPACE_YUV444) {
		pps_data->convert_rgb = 0;
		pps_data->simple_422 = 0;
		pps_data->native_422 = 0;
		pps_data->native_420 = 0;
	} else if (dsc_drv->color_format == HDMI_COLORSPACE_YUV422) {
		pps_data->convert_rgb = 0;
		pps_data->simple_422 = 0;
		pps_data->native_422 = 1;
		pps_data->native_420 = 0;
	} else if (dsc_drv->color_format == HDMI_COLORSPACE_YUV420) {
		pps_data->convert_rgb = 0;
		pps_data->simple_422 = 0;
		pps_data->native_422 = 0;
		pps_data->native_420 = 1;
	} else if (dsc_drv->color_format == HDMI_COLORSPACE_RGB) {
		pps_data->convert_rgb = 1;
		pps_data->simple_422 = 0;
		pps_data->native_422 = 0;
		pps_data->native_420 = 0;
	} else {
		DSC_ERR("color_format(%d) is error\n", dsc_drv->color_format);
	}

	/* set slice_width slice_height */
	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_SLICE_W_H) {
		DSC_PR("manual slice slice_width:%d slice_height:%d\n",
			pps_data->slice_width, pps_data->slice_height);
	} else if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320) {
		pps_data->slice_width = pps_data->pic_width / 4;
		pps_data->slice_height = pps_data->pic_height / 40;
	} else {
		DSC_ERR("not support pic_width:%d pic_height:%d\n",
			pps_data->pic_width, pps_data->pic_height);
	}

	/* set bits_per_component */
	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_BITS_PER_COMPONENT)
		DSC_PR("manual bits_per_component:%d\n", pps_data->bits_per_component);
	else
		pps_data->bits_per_component = 12;

	/* set bits_per_pixel */
	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_BITS_PER_PIXEL)
		DSC_PR("manual bits_per_pixel:%d\n", pps_data->bits_per_pixel);
	else
		pps_data->bits_per_pixel = 238;

	/* set line_buf_depth */
	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_LINE_BUF_DEPTH)
		DSC_PR("manual line_buf_depth:%d\n", pps_data->line_buf_depth);
	else
		pps_data->line_buf_depth = 13;

	/* set block_pred_enable vbr_enable */
	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_PREDICTION_MODE) {
		DSC_PR("manual block_pred_enable:%d vbr_enable:%d\n",
			pps_data->block_pred_enable, pps_data->vbr_enable);
	} else {
		pps_data->block_pred_enable = 1;
		pps_data->vbr_enable = 0;
	}

	/* set rc_model_size rc_edge_factor rc_tgt_offset */
	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_SOME_RC_PARAMETER) {
		DSC_PR("manual rc_model_size:%d rc_edge_factor:%d lo:%d hi:%d\n",
			pps_data->rc_parameter_set.rc_model_size,
			pps_data->rc_parameter_set.rc_edge_factor,
			pps_data->rc_parameter_set.rc_tgt_offset_lo,
			pps_data->rc_parameter_set.rc_tgt_offset_hi);
	} else {
		pps_data->rc_parameter_set.rc_model_size = 8192;
		pps_data->rc_parameter_set.rc_tgt_offset_hi = 3;
		pps_data->rc_parameter_set.rc_tgt_offset_lo = 3;
		pps_data->rc_parameter_set.rc_edge_factor = 6;
	}

	dsc_drv->bits_per_pixel_int = dsc_drv->pps_data.bits_per_pixel / 16;
	dsc_drv->bits_per_pixel_remain = (dsc_drv->pps_data.bits_per_pixel -
					16 * dsc_drv->bits_per_pixel_int) * BPP_FRACTION / 16;
	dsc_drv->bits_per_pixel_multiple = dsc_drv->bits_per_pixel_int * BPP_FRACTION +
					dsc_drv->bits_per_pixel_remain;
	if (dsc_drv->color_format == HDMI_COLORSPACE_RGB ||
	    dsc_drv->color_format == HDMI_COLORSPACE_YUV444)
		dsc_drv->bits_per_pixel_really_value = dsc_drv->bits_per_pixel_multiple;
	else if (dsc_drv->color_format == HDMI_COLORSPACE_YUV422 ||
		 dsc_drv->color_format == HDMI_COLORSPACE_YUV420)
		dsc_drv->bits_per_pixel_really_value = dsc_drv->bits_per_pixel_multiple / 2;
}

void calculate_dsc_data(struct aml_dsc_drv_s *dsc_drv)
{
	init_pps_data(dsc_drv);
	populate_pps(dsc_drv);
	calculate_asic_data(dsc_drv);
	compute_offset(dsc_drv, 0, 0, 0);
}

