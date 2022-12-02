// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/amlogic/media/vout/dsc/dsc.h>
#include "dsc_reg.h"
#include "dsc_hw.h"
#include "dsc_drv.h"
#include "dsc_config.h"
#include "dsc_debug.h"
//===========================================================================
// For DSC Debug
//===========================================================================
static const char *dsc_debug_usage_str = {
"Usage:\n"
"    echo state > /sys/class/aml_dsc/dsc0/status; dump dsc status\n"
"    echo dump_reg > /sys/class/aml_dsc/dsc0/debug; dump dsc register\n"
"    echo read addr >/sys/class/aml_dsc/dsc0/debug\n"
"    echo write addr value >/sys/class/aml_dsc/dsc0/debug\n"
"\n"
};

static ssize_t dsc_debug_help(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", dsc_debug_usage_str);
}

static ssize_t dsc_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	return len;
}

static void dsc_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static inline void dsc_print_state(struct aml_dsc_drv_s *dsc_drv)
{
	int i;

	/* Picture Parameter Set Start*/
	DSC_PR("------Picture Parameter Set Start------\n");
	DSC_PR("dsc_version_major:%d\n", dsc_drv->pps_data.dsc_version_major);
	DSC_PR("dsc_version_minor:%d\n", dsc_drv->pps_data.dsc_version_minor);
	DSC_PR("pps_identifier:%d\n", dsc_drv->pps_data.pps_identifier);
	DSC_PR("bits_per_component:%d\n", dsc_drv->pps_data.bits_per_component);
	DSC_PR("line_buf_depth:%d\n", dsc_drv->pps_data.line_buf_depth);
	DSC_PR("block_pred_enable:%d\n", dsc_drv->pps_data.block_pred_enable);
	DSC_PR("convert_rgb:%d\n", dsc_drv->pps_data.convert_rgb);
	DSC_PR("simple_422:%d\n", dsc_drv->pps_data.simple_422);
	DSC_PR("vbr_enable:%d\n", dsc_drv->pps_data.vbr_enable);
	DSC_PR("bits_per_pixel:%d\n", dsc_drv->pps_data.bits_per_pixel);
	DSC_PR("pic_height:%d\n", dsc_drv->pps_data.pic_height);
	DSC_PR("pic_width:%d\n", dsc_drv->pps_data.pic_width);
	DSC_PR("slice_height:%d\n", dsc_drv->pps_data.slice_height);
	DSC_PR("slice_width:%d\n", dsc_drv->pps_data.slice_width);
	DSC_PR("chunk_size:%d\n", dsc_drv->pps_data.chunk_size);
	DSC_PR("initial_xmit_delay:%d\n", dsc_drv->pps_data.initial_xmit_delay);
	DSC_PR("initial_dec_delay:%d\n", dsc_drv->pps_data.initial_dec_delay);
	DSC_PR("initial_scale_value:%d\n", dsc_drv->pps_data.initial_scale_value);
	DSC_PR("scale_increment_interval:%d\n", dsc_drv->pps_data.scale_increment_interval);
	DSC_PR("scale_decrement_interval:%d\n", dsc_drv->pps_data.scale_decrement_interval);
	DSC_PR("first_line_bpg_offset:%d\n", dsc_drv->pps_data.first_line_bpg_offset);
	DSC_PR("nfl_bpg_offset:%d\n", dsc_drv->pps_data.nfl_bpg_offset);
	DSC_PR("slice_bpg_offset:%d\n", dsc_drv->pps_data.slice_bpg_offset);
	DSC_PR("initial_offset:%d\n", dsc_drv->pps_data.initial_offset);
	DSC_PR("final_offset:%d\n", dsc_drv->pps_data.final_offset);
	DSC_PR("flatness_min_qp:%d\n", dsc_drv->pps_data.flatness_min_qp);
	DSC_PR("flatness_max_qp:%d\n", dsc_drv->pps_data.flatness_max_qp);
	DSC_PR("rc_model_size:%d\n", dsc_drv->pps_data.rc_parameter_set.rc_model_size);
	DSC_PR("rc_edge_factor:%d\n", dsc_drv->pps_data.rc_parameter_set.rc_edge_factor);
	DSC_PR("rc_quant_incr_limit0:%d\n",
		dsc_drv->pps_data.rc_parameter_set.rc_quant_incr_limit0);
	DSC_PR("rc_quant_incr_limit1:%d\n",
		dsc_drv->pps_data.rc_parameter_set.rc_quant_incr_limit1);
	DSC_PR("rc_tgt_offset_hi:%d\n", dsc_drv->pps_data.rc_parameter_set.rc_tgt_offset_hi);
	DSC_PR("rc_tgt_offset_lo:%d\n", dsc_drv->pps_data.rc_parameter_set.rc_tgt_offset_lo);
	for (i = 0; i < RC_BUF_THRESH_NUM; i++) {
		DSC_PR("rc_buf_thresh[%d]:%d(%d)\n",
			i, dsc_drv->pps_data.rc_parameter_set.rc_buf_thresh[i],
			dsc_drv->pps_data.rc_parameter_set.rc_buf_thresh[i] << 6);
	}
	for (i = 0; i < RC_RANGE_PARAMETERS_NUM; i++) {
		DSC_PR("rc_range_parameters[%d].range_min_qp:%d\n",
		i, dsc_drv->pps_data.rc_parameter_set.rc_range_parameters[i].range_min_qp);
		DSC_PR("rc_range_parameters[%d].range_max_qp:%d\n",
			i, dsc_drv->pps_data.rc_parameter_set.rc_range_parameters[i].range_max_qp);
		DSC_PR("rc_range_parameters[%d].range_bpg_offset:%d(0x%x)\n",
		i, dsc_drv->pps_data.rc_parameter_set.rc_range_parameters[i].range_bpg_offset,
		dsc_drv->pps_data.rc_parameter_set.rc_range_parameters[i].range_bpg_offset & 0x3f);
	}
	DSC_PR("native_420:%d\n", dsc_drv->pps_data.native_420);
	DSC_PR("native_422:%d\n", dsc_drv->pps_data.native_422);
	DSC_PR("very_flat_qp:%d\n", dsc_drv->very_flat_qp);
	DSC_PR("somewhat_flat_qp_thresh:%d\n", dsc_drv->somewhat_flat_qp_thresh);
	DSC_PR("somewhat_flat_qp_delta:%d\n", dsc_drv->somewhat_flat_qp_delta);
	DSC_PR("second_line_bpg_offset:%d\n", dsc_drv->pps_data.second_line_bpg_offset);
	DSC_PR("nsl_bpg_offset:%d\n", dsc_drv->pps_data.nsl_bpg_offset);
	DSC_PR("second_line_offset_adj:%d\n", dsc_drv->pps_data.second_line_offset_adj);
	DSC_PR("rcb_bits:%d\n", dsc_drv->rcb_bits);
	DSC_PR("flatness_det_thresh:%d\n", dsc_drv->flatness_det_thresh);
	DSC_PR("mux_word_size:%d\n", dsc_drv->mux_word_size);
	DSC_PR("full_ich_err_precision:%d\n", dsc_drv->full_ich_err_precision);
	DSC_PR("hc_active_bytes:%d\n", dsc_drv->pps_data.hc_active_bytes);
	DSC_PR("------Picture Parameter Set End------\n");
	DSC_PR("slice_num_m1:%d\n", dsc_drv->slice_num_m1);
	DSC_PR("clr_ssm_fifo_sts:%d\n", dsc_drv->clr_ssm_fifo_sts);
	DSC_PR("dsc_enc_en:%d\n", dsc_drv->dsc_enc_en);
	DSC_PR("dsc_enc_frm_latch_en:%d\n", dsc_drv->dsc_enc_frm_latch_en);
	DSC_PR("pix_per_clk:%d\n", dsc_drv->pix_per_clk);
	DSC_PR("inbuf_rd_dly0:%d\n", dsc_drv->inbuf_rd_dly0);
	DSC_PR("inbuf_rd_dly1:%d\n", dsc_drv->inbuf_rd_dly1);
	DSC_PR("c3_clk_en:%x\n", dsc_drv->c3_clk_en);
	DSC_PR("c2_clk_en:%x\n", dsc_drv->c2_clk_en);
	DSC_PR("c1_clk_en:%x\n", dsc_drv->c1_clk_en);
	DSC_PR("c0_clk_en:%x\n", dsc_drv->c0_clk_en);
	DSC_PR("slices_in_core:%d\n", dsc_drv->slices_in_core);
	DSC_PR("slice_group_number:%d\n", dsc_drv->slice_group_number);
	DSC_PR("partial_group_pix_num:%d\n", dsc_drv->partial_group_pix_num);
	DSC_PR("chunk_6byte_num_m1:%d\n", dsc_drv->chunk_6byte_num_m1);
	DSC_PR("------timing start------\n");
	DSC_PR("tmg_havon_begin:%d\n", dsc_drv->tmg_ctrl.tmg_havon_begin);
	DSC_PR("tmg_vavon_bline:%d\n", dsc_drv->tmg_ctrl.tmg_vavon_bline);
	DSC_PR("tmg_vavon_eline:%d\n", dsc_drv->tmg_ctrl.tmg_vavon_eline);
	DSC_PR("tmg_hso_begin:%d\n", dsc_drv->tmg_ctrl.tmg_hso_begin);
	DSC_PR("tmg_hso_end:%d\n", dsc_drv->tmg_ctrl.tmg_hso_end);
	DSC_PR("tmg_vso_begin:%d\n", dsc_drv->tmg_ctrl.tmg_vso_begin);
	DSC_PR("tmg_vso_end:%d\n", dsc_drv->tmg_ctrl.tmg_vso_end);
	DSC_PR("tmg_vso_bline:%d\n", dsc_drv->tmg_ctrl.tmg_vso_bline);
	DSC_PR("tmg_vso_eline:%d\n", dsc_drv->tmg_ctrl.tmg_vso_eline);
	DSC_PR("hc_vtotal_m1:%d\n", dsc_drv->hc_vtotal_m1);
	DSC_PR("hc_htotal_m1:%d\n", dsc_drv->hc_htotal_m1);
	DSC_PR("encp bist timing------\n");
	DSC_PR("encp_dvi_hso_begin:%d\n", dsc_drv->encp_timing_ctrl.encp_dvi_hso_begin);
	DSC_PR("encp_dvi_hso_end:%d\n", dsc_drv->encp_timing_ctrl.encp_dvi_hso_end);
	DSC_PR("encp_vso_bline:%d\n", dsc_drv->encp_timing_ctrl.encp_vso_bline);
	DSC_PR("encp_vso_eline:%d\n", dsc_drv->encp_timing_ctrl.encp_vso_eline);
	DSC_PR("encp_vso_begin:%d\n", dsc_drv->encp_timing_ctrl.encp_vso_begin);
	DSC_PR("encp_vso_end:%d\n", dsc_drv->encp_timing_ctrl.encp_vso_end);
	DSC_PR("encp_de_h_begin:%d\n", dsc_drv->encp_timing_ctrl.encp_de_h_begin);
	DSC_PR("encp_de_h_end:%d\n", dsc_drv->encp_timing_ctrl.encp_de_h_end);
	DSC_PR("encp_de_v_end:%d\n", dsc_drv->encp_timing_ctrl.encp_de_v_end);
	DSC_PR("encp_de_v_begin:%d\n", dsc_drv->encp_timing_ctrl.encp_de_v_begin);
	DSC_PR("encp video timing------\n");
	DSC_PR("encp_hso_end:%d\n", dsc_drv->encp_timing_ctrl.encp_hso_end);
	DSC_PR("encp_hso_begin:%d\n", dsc_drv->encp_timing_ctrl.encp_hso_begin);
	DSC_PR("encp_vso_bline:%d\n", dsc_drv->encp_timing_ctrl.encp_vso_bline);
	DSC_PR("encp_vso_eline:%d\n", dsc_drv->encp_timing_ctrl.encp_vso_eline);
	DSC_PR("encp_vso_begin:%d\n", dsc_drv->encp_timing_ctrl.encp_vso_begin);
	DSC_PR("encp_vso_end:%d\n", dsc_drv->encp_timing_ctrl.encp_vso_end);
	DSC_PR("encp_vavon_bline:d%d\n", dsc_drv->encp_timing_ctrl.encp_vavon_bline);
	DSC_PR("encp_vavon_eline:%d\n", dsc_drv->encp_timing_ctrl.encp_vavon_eline);
	DSC_PR("encp_havon_begin:%d\n", dsc_drv->encp_timing_ctrl.encp_havon_begin);
	DSC_PR("encp_havon_end:%d\n", dsc_drv->encp_timing_ctrl.encp_havon_end);
	DSC_PR("h_total:%d\n", dsc_drv->encp_timing_ctrl.h_total);
	DSC_PR("v_total:%d\n", dsc_drv->encp_timing_ctrl.v_total);
	DSC_PR("------timing end------\n");
	DSC_PR("pix_in_swap0:0x%x\n", dsc_drv->pix_in_swap0);
	DSC_PR("pix_in_swap1:0x%x\n", dsc_drv->pix_in_swap1);
	DSC_PR("gclk_ctrl:%d\n", dsc_drv->gclk_ctrl);
	DSC_PR("out_swap:0x%x\n", dsc_drv->out_swap);
	DSC_PR("------odd_mode start------\n");
	DSC_PR("hs_odd_no_tail:%d\n", dsc_drv->hs_odd_no_tail);
	DSC_PR("hs_odd_no_head:%d\n", dsc_drv->hs_odd_no_head);
	DSC_PR("hs_even_no_tail:%d\n", dsc_drv->hs_even_no_tail);
	DSC_PR("hs_even_no_head:%d\n", dsc_drv->hs_even_no_head);
	DSC_PR("vs_odd_no_tail:%d\n", dsc_drv->vs_odd_no_tail);
	DSC_PR("vs_odd_no_head:%d\n", dsc_drv->vs_odd_no_head);
	DSC_PR("vs_even_no_tail:%d\n", dsc_drv->vs_even_no_tail);
	DSC_PR("vs_even_no_head:%d\n", dsc_drv->vs_even_no_head);
	DSC_PR("hc_htotal_offs_oddline:%d\n", dsc_drv->hc_htotal_offs_oddline);
	DSC_PR("hc_htotal_offs_evenline:%d\n", dsc_drv->hc_htotal_offs_evenline);
	DSC_PR("hc_active_odd_mode:%d\n", dsc_drv->hc_active_odd_mode);
	DSC_PR("------odd_mode end------\n");
	DSC_PR("fps:%d\n", dsc_drv->fps);
	DSC_PR("color_format:%d\n", dsc_drv->color_format);
	DSC_PR("dsc_mode:%d\n", dsc_drv->dsc_mode);
	DSC_PR("bits_per_pixel_int:%d\n", dsc_drv->bits_per_pixel_int);
	DSC_PR("bits_per_pixel_remain:%d\n", dsc_drv->bits_per_pixel_remain);
	DSC_PR("bits_per_pixel_multiple:%d\n", dsc_drv->bits_per_pixel_multiple);
	DSC_PR("bits_per_pixel_really_value:%d\n", dsc_drv->bits_per_pixel_really_value);
	DSC_PR("manual_set_select:0x%x\n", dsc_drv->dsc_debug.manual_set_select);
}

static inline void dsc_print_reg_value(struct aml_dsc_drv_s *dsc_drv)
{
	int i;

	/* Picture Parameter Set Start*/
	DSC_PR("------Picture Parameter Set Start------\n");
	DSC_PR("dsc_version_major:%d\n", dsc_drv->pps_data.dsc_version_major);
	DSC_PR("dsc_version_minor:%d\n", R_DSC_BIT(DSC_COMP_CTRL, DSC_VERSION_MINOR,
		DSC_VERSION_MINOR_WID));
	DSC_PR("pps_identifier:%d\n", dsc_drv->pps_data.pps_identifier);
	DSC_PR("bits_per_component:%d\n", R_DSC_BIT(DSC_COMP_BIT_DEPTH,
		BITS_PER_COMPONENT, BITS_PER_COMPONENT_WID));
	DSC_PR("line_buf_depth:%d\n", R_DSC_BIT(DSC_COMP_BIT_DEPTH,
		LINE_BUF_DEPTH, LINE_BUF_DEPTH_WID));
	DSC_PR("block_pred_enable:%d\n", R_DSC_BIT(DSC_COMP_CTRL,
		BLOCK_PRED_ENABLE, BLOCK_PRED_ENABLE_WID));
	DSC_PR("convert_rgb:%d\n", R_DSC_BIT(DSC_COMP_CTRL, CONVERT_RGB, CONVERT_RGB_WID));
	DSC_PR("simple_422:%d\n", dsc_drv->pps_data.simple_422);
	DSC_PR("vbr_enable:%d\n", R_DSC_BIT(DSC_COMP_CTRL, VBR_ENABLE, VBR_ENABLE_WID));
	DSC_PR("bits_per_pixel:%d\n", R_DSC_BIT(DSC_COMP_BIT_DEPTH,
		BITS_PER_PIXEL, BITS_PER_PIXEL_WID));
	DSC_PR("pic_height:%d\n", R_DSC_BIT(DSC_COMP_PIC_SIZE, PCI_HEIGHT, PCI_HEIGHT_WID));
	DSC_PR("pic_width:%d\n", R_DSC_BIT(DSC_COMP_PIC_SIZE, PCI_WIDTH, PCI_WIDTH_WID));
	DSC_PR("slice_height:%d\n", R_DSC_BIT(DSC_COMP_SLICE_SIZE, SLICE_HEIGHT, SLICE_HEIGHT_WID));
	DSC_PR("slice_width:%d\n", R_DSC_BIT(DSC_COMP_SLICE_SIZE, SLICE_WIDTH, SLICE_WIDTH_WID));
	DSC_PR("chunk_size:%d\n", R_DSC_BIT(DSC_COMP_CHUNK_WORD_SIZE, CHUNK_SIZE, CHUNK_SIZE_WID));
	DSC_PR("initial_xmit_delay:%d\n", R_DSC_BIT(DSC_COMP_INITIAL_DELAY,
		INITIAL_XMIT_DELAY, INITIAL_XMIT_DELAY_WID));
	DSC_PR("initial_dec_delay:%d\n", R_DSC_BIT(DSC_COMP_INITIAL_DELAY,
		INITIAL_DEC_DELAY, INITIAL_DEC_DELAY_WID));
	DSC_PR("initial_scale_value:%d\n", R_DSC_BIT(DSC_COMP_INITIAL_OFFSET_SCALE,
		INITIAL_SCALE_VALUE, INITIAL_SCALE_VALUE_WID));
	DSC_PR("scale_increment_interval:%d\n", R_DSC_BIT(DSC_COMP_SCALE,
		SCALE_INCREMENT_INTERVAL, SCALE_INCREMENT_INTERVAL_WID));
	DSC_PR("scale_decrement_interval:%d\n", R_DSC_BIT(DSC_COMP_SCALE,
		SCALE_DECREMENT_INTERVAL, SCALE_DECREMENT_INTERVAL_WID));
	DSC_PR("first_line_bpg_offset:%d\n", R_DSC_BIT(DSC_COMP_FS_BPG_OFS,
		FIRST_LINE_BPG_OFS, FIRST_LINE_BPG_OFS_WID));
	DSC_PR("nfl_bpg_offset:%d\n", R_DSC_BIT(DSC_COMP_NFS_BPG_OFFSET,
		NFL_BPG_OFFSET, NFL_BPG_OFFSET_WID));
	DSC_PR("slice_bpg_offset:%d\n", R_DSC_BIT(DSC_COMP_SLICE_FINAL_OFFSET,
		SLICE_BPG_OFFSET, SLICE_BPG_OFFSET_WID));
	DSC_PR("initial_offset:%d\n", R_DSC_BIT(DSC_COMP_INITIAL_OFFSET_SCALE,
		INITIAL_OFFSET, INITIAL_OFFSET_WID));
	DSC_PR("final_offset:%d\n", R_DSC_BIT(DSC_COMP_SLICE_FINAL_OFFSET,
		FINAL_OFFSET, FINAL_OFFSET_WID));
	DSC_PR("flatness_min_qp:%d\n", R_DSC_BIT(DSC_COMP_FLATNESS,
		FLATNESS_MIN_QP, FLATNESS_MIN_QP_WID));
	DSC_PR("flatness_max_qp:%d\n", R_DSC_BIT(DSC_COMP_FLATNESS,
		FLATNESS_MAX_QP, FLATNESS_MAX_QP_WID));
	DSC_PR("rc_model_size:%d\n", R_DSC_BIT(DSC_COMP_RC_BITS_SIZE,
		RC_MODEL_SIZE, RC_MODEL_SIZE_WID));
	DSC_PR("rc_edge_factor:%d\n", R_DSC_BIT(DSC_COMP_RC_TGT_QUANT,
		RC_EDGE_FACTOR, RC_EDGE_FACTOR_WID));
	DSC_PR("rc_quant_incr_limit0:%d\n", R_DSC_BIT(DSC_COMP_RC_TGT_QUANT,
		RC_QUANT_INCR_LIMIT0, RC_QUANT_INCR_LIMIT0_WID));
	DSC_PR("rc_quant_incr_limit1:%d\n", R_DSC_BIT(DSC_COMP_RC_TGT_QUANT,
		RC_QUANT_INCR_LIMIT1, RC_QUANT_INCR_LIMIT1_WID));
	DSC_PR("rc_tgt_offset_hi:%d\n", R_DSC_BIT(DSC_COMP_RC_TGT_QUANT,
		RC_TGT_OFFSET_HI, RC_TGT_OFFSET_HI_WID));
	DSC_PR("rc_tgt_offset_lo:%d\n", R_DSC_BIT(DSC_COMP_RC_TGT_QUANT,
	RC_TGT_OFFSET_LO, RC_TGT_OFFSET_LO_WID));
	for (i = 0; i < RC_BUF_THRESH_NUM; i++) {
		DSC_PR("rc_buf_thresh[%d]:%d(%d)\n",
			i, R_DSC_BIT(DSC_COMP_RC_BUF_THRESH_0 + i,
			RC_BUF_THRESH_0, RC_BUF_THRESH_0_WID) >> 6,
			R_DSC_BIT(DSC_COMP_RC_BUF_THRESH_0 + i,
			RC_BUF_THRESH_0, RC_BUF_THRESH_0_WID));
	}
	for (i = 0; i < RC_RANGE_PARAMETERS_NUM; i++) {
		DSC_PR("rc_range_parameters[%d].range_min_qp:%d\n",
		i, dsc_drv->pps_data.rc_parameter_set.rc_range_parameters[i].range_min_qp);
		DSC_PR("rc_range_parameters[%d].range_max_qp:%d\n",
			i, dsc_drv->pps_data.rc_parameter_set.rc_range_parameters[i].range_max_qp);
		DSC_PR("rc_range_parameters[%d].range_bpg_offset:%d(0x%x)\n",
		i, dsc_drv->pps_data.rc_parameter_set.rc_range_parameters[i].range_bpg_offset,
		dsc_drv->pps_data.rc_parameter_set.rc_range_parameters[i].range_bpg_offset & 0x3f);
	}
	DSC_PR("native_420:%d\n", R_DSC_BIT(DSC_COMP_CTRL, NATIVE_420, NATIVE_420_WID));
	DSC_PR("native_422:%d\n", R_DSC_BIT(DSC_COMP_CTRL, NATIVE_422, NATIVE_422_WID));
	DSC_PR("very_flat_qp:%d\n", R_DSC_BIT(DSC_COMP_FLAT_QP, VERY_FLAT_QP, VERY_FLAT_QP_WID));
	DSC_PR("somewhat_flat_qp_thresh:%d\n", R_DSC_BIT(DSC_COMP_FLAT_QP, SOMEWHAT_FLAT_QP_THRESH,
		SOMEWHAT_FLAT_QP_THRESH_WID));
	DSC_PR("somewhat_flat_qp_delta:%d\n", R_DSC_BIT(DSC_COMP_FLAT_QP, SOMEWHAT_FLAT_QP_DELTA,
		SOMEWHAT_FLAT_QP_DELTA_WID));
	DSC_PR("second_line_bpg_offset:%d\n", R_DSC_BIT(DSC_COMP_FS_BPG_OFS, SECOND_LINE_BPG_OFS,
		SECOND_LINE_BPG_OFS_WID));
	DSC_PR("nsl_bpg_offset:%d\n", R_DSC_BIT(DSC_COMP_NFS_BPG_OFFSET, NSL_BPG_OFFSET,
		NSL_BPG_OFFSET_WID));
	DSC_PR("second_line_offset_adj:%d\n", R_DSC_BIT(DSC_COMP_SEC_OFS_ADJ, SECOND_LINE_OFS_ADJ,
		SECOND_LINE_OFS_ADJ_WID));
	DSC_PR("rcb_bits:%d\n", R_DSC_BIT(DSC_COMP_RC_BITS_SIZE, RCB_BITS, RCB_BITS_WID));
	DSC_PR("flatness_det_thresh:%d\n", R_DSC_BIT(DSC_COMP_FLATNESS, FLATNESS_DET_THRESH,
		FLATNESS_DET_THRESH_WID));
	DSC_PR("mux_word_size:%d\n", R_DSC_BIT(DSC_COMP_CHUNK_WORD_SIZE, MUX_WORD_SIZE,
		MUX_WORD_SIZE_WID));
	DSC_PR("full_ich_err_precision:%d\n", R_DSC_BIT(DSC_COMP_CTRL, FULL_ICH_ERR_PRECISION,
		FULL_ICH_ERR_PRECISION_WID));
	DSC_PR("------Picture Parameter Set End------\n");
	DSC_PR("slice_num_m1:%d\n", R_DSC_BIT(DSC_ASIC_CTRL0, SLICE_NUM_M1, SLICE_NUM_M1_WID));
	DSC_PR("clr_ssm_fifo_sts:%d\n", R_DSC_BIT(DSC_ASIC_CTRL0, CLR_SSM_FIFO_STS,
		CLR_SSM_FIFO_STS_WID));
	DSC_PR("dsc_enc_en:%d\n", R_DSC_BIT(DSC_ASIC_CTRL0, DSC_ENC_EN, DSC_ENC_EN_WID));
	DSC_PR("dsc_enc_frm_latch_en:%d\n", R_DSC_BIT(DSC_ASIC_CTRL0, DSC_ENC_FRM_LATCH_EN,
		DSC_ENC_FRM_LATCH_EN_WID));
	DSC_PR("pix_per_clk:%d\n", R_DSC_BIT(DSC_ASIC_CTRL0, PIX_PER_CLK, PIX_PER_CLK_WID));
	DSC_PR("inbuf_rd_dly0:%d\n", R_DSC_BIT(DSC_ASIC_CTRL0, INBUF_RD_DLY0, INBUF_RD_DLY0_WID));
	DSC_PR("inbuf_rd_dly1:%d\n", R_DSC_BIT(DSC_ASIC_CTRL0, INBUF_RD_DLY1, INBUF_RD_DLY1_WID));
	DSC_PR("c3_clk_en:%x\n", R_DSC_BIT(DSC_ASIC_CTRL1, C3_CLK_EN, C3_CLK_EN_WID));
	DSC_PR("c2_clk_en:%x\n", R_DSC_BIT(DSC_ASIC_CTRL1, C2_CLK_EN, C2_CLK_EN_WID));
	DSC_PR("c1_clk_en:%x\n", R_DSC_BIT(DSC_ASIC_CTRL1, C1_CLK_EN, C1_CLK_EN_WID));
	DSC_PR("c0_clk_en:%x\n", R_DSC_BIT(DSC_ASIC_CTRL1, C0_CLK_EN, C0_CLK_EN_WID));
	DSC_PR("slices_in_core:%d\n",
		R_DSC_BIT(DSC_ASIC_CTRL1, SLICES_IN_CORE, SLICES_IN_CORE_WID));
	DSC_PR("slice_group_number:%d\n", R_DSC_BIT(DSC_ASIC_CTRL1, SLICES_GROUP_NUMBER,
		SLICES_GROUP_NUMBER_WID));
	DSC_PR("partial_group_pix_num:%d\n", R_DSC_BIT(DSC_ASIC_CTRL2, PARTIAL_GROUP_PIX_NUM,
		PARTIAL_GROUP_PIX_NUM_WID));
	DSC_PR("chunk_6byte_num_m1:%d\n", R_DSC_BIT(DSC_ASIC_CTRL3, CHUNK_6BYTE_NUM_M1,
		CHUNK_6BYTE_NUM_M1_WID));
	DSC_PR("------timing start------\n");
	DSC_PR("tmg_en:%d\n", R_DSC_BIT(DSC_ASIC_CTRL4, TMG_EN, TMG_EN_WID));
	DSC_PR("tmg_havon_begin:%d\n", R_DSC_BIT(DSC_ASIC_CTRL4, TMG_HAVON_BEGIN,
		TMG_HAVON_BEGIN_WID));
	DSC_PR("tmg_vavon_bline:%d\n", R_DSC_BIT(DSC_ASIC_CTRL5, TMG_VAVON_BLINE,
		TMG_VAVON_BLINE_WID));
	DSC_PR("tmg_vavon_eline:%d\n", R_DSC_BIT(DSC_ASIC_CTRL5, TMG_VAVON_ELINE,
		TMG_VAVON_ELINE_WID));
	DSC_PR("tmg_hso_begin:%d\n", R_DSC_BIT(DSC_ASIC_CTRL6, TMG_HSO_BEGIN, TMG_HSO_BEGIN_WID));
	DSC_PR("tmg_hso_end:%d\n", R_DSC_BIT(DSC_ASIC_CTRL6, TMG_HSO_END, TMG_HSO_END_WID));
	DSC_PR("tmg_vso_begin:%d\n", R_DSC_BIT(DSC_ASIC_CTRL7, TMG_VSO_BEGIN, TMG_VSO_BEGIN_WID));
	DSC_PR("tmg_vso_end:%d\n", R_DSC_BIT(DSC_ASIC_CTRL7, TMG_VSO_END, TMG_VSO_END_WID));
	DSC_PR("tmg_vso_bline:%d\n", R_DSC_BIT(DSC_ASIC_CTRL8, TMG_VSO_BLINE, TMG_VSO_BLINE_WID));
	DSC_PR("tmg_vso_eline:%d\n", R_DSC_BIT(DSC_ASIC_CTRL8, TMG_VSO_ELINE, TMG_VSO_ELINE_WID));
	DSC_PR("hc_vtotal_m1:%d\n", R_DSC_BIT(DSC_ASIC_CTRL18, HC_VTOTAL_M1, HC_VTOTAL_M1_WID));
	DSC_PR("hc_htotal_m1:%d\n", R_DSC_BIT(DSC_ASIC_CTRL18, HC_HTOTAL_M1, HC_HTOTAL_M1_WID));
	DSC_PR("encp bist timing------\n");
	DSC_PR("encp_de_v_end(%#x):%d\n",
		ENCP_DE_V_END_EVEN, dsc_vcbus_reg_read(ENCP_DE_V_END_EVEN));
	DSC_PR("encp_de_v_begin(%#x):%d\n",
		ENCP_DE_V_BEGIN_EVEN, dsc_vcbus_reg_read(ENCP_DE_V_BEGIN_EVEN));
	DSC_PR("encp_vso_bline(%#x):%d\n",
		ENCP_DVI_VSO_BLINE_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_BLINE_EVN));
	DSC_PR("encp_vso_eline(%#x):%d\n",
		ENCP_DVI_VSO_ELINE_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_ELINE_EVN));
	DSC_PR("encp_de_h_begin(%#x):%d\n",
		ENCP_DE_H_BEGIN, dsc_vcbus_reg_read(ENCP_DE_H_BEGIN));
	DSC_PR("encp_de_h_end(%#x):%d\n",
		ENCP_DE_H_END, dsc_vcbus_reg_read(ENCP_DE_H_END));
	DSC_PR("encp_dvi_hso_begin(%#x):%d\n",
		ENCP_DVI_HSO_BEGIN, dsc_vcbus_reg_read(ENCP_DVI_HSO_BEGIN));
	DSC_PR("encp_dvi_hso_end(%#x):%d\n",
		ENCP_DVI_HSO_END, dsc_vcbus_reg_read(ENCP_DVI_HSO_END));
	DSC_PR("encp_video_max_pxcnt(%#x):%d\n",
		ENCP_VIDEO_MAX_PXCNT, dsc_vcbus_reg_read(ENCP_VIDEO_MAX_PXCNT));
	DSC_PR("encp_video_max_lncnt(%#x):%d\n",
		ENCP_VIDEO_MAX_LNCNT, dsc_vcbus_reg_read(ENCP_VIDEO_MAX_LNCNT));
	DSC_PR("encp video timing------\n");
	DSC_PR("encp_vavon_bline(%#x):%d\n",
		ENCP_VIDEO_VAVON_BLINE, dsc_vcbus_reg_read(ENCP_VIDEO_VAVON_BLINE));
	DSC_PR("encp_vavon_eline(%#x):%d\n",
		ENCP_VIDEO_VAVON_ELINE, dsc_vcbus_reg_read(ENCP_VIDEO_VAVON_ELINE));
	DSC_PR("encp_havon_begin(%#x):%d\n",
		ENCP_VIDEO_HAVON_BEGIN, dsc_vcbus_reg_read(ENCP_VIDEO_HAVON_BEGIN));
	DSC_PR("encp_havon_end(%#x):%d\n",
		ENCP_VIDEO_HAVON_END, dsc_vcbus_reg_read(ENCP_VIDEO_HAVON_END));
	DSC_PR("encp_hso_end(%#x):%d\n",
		ENCP_VIDEO_HSO_END, dsc_vcbus_reg_read(ENCP_VIDEO_HSO_END));
	DSC_PR("encp_hso_begin(%#x):%d\n",
		ENCP_VIDEO_HSO_BEGIN, dsc_vcbus_reg_read(ENCP_VIDEO_HSO_BEGIN));
	DSC_PR("encp_vso_bline(%#x):%d\n",
		ENCP_VIDEO_VSO_BLINE, dsc_vcbus_reg_read(ENCP_VIDEO_VSO_BLINE));
	DSC_PR("encp_vso_eline(%#x):%d\n",
		ENCP_VIDEO_VSO_ELINE, dsc_vcbus_reg_read(ENCP_VIDEO_VSO_ELINE));
	DSC_PR("------timing end------\n");
	DSC_PR("pix_in_swap0:0x%x\n", R_DSC_BIT(DSC_ASIC_CTRL9, PIX_IN_SWAP0, PIX_IN_SWAP0_WID));
	DSC_PR("pix_in_swap1:0x%x\n", R_DSC_BIT(DSC_ASIC_CTRLA, PIX_IN_SWAP1, PIX_IN_SWAP1_WID));
	DSC_PR("gclk_ctrl:%d\n", R_DSC_BIT(DSC_ASIC_CTRLC, GCLK_CTRL, GCLK_CTRL_WID));
	DSC_PR("out_swap:0x%x\n", R_DSC_BIT(DSC_ASIC_CTRL12, OUT_SWAP, OUT_SWAP_WID));
	DSC_PR("------odd_mode start------\n");
	DSC_PR("hs_odd_no_tail:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, HS_ODD_NO_TAIL,
		HS_ODD_NO_TAIL_WID));
	DSC_PR("hs_odd_no_head:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, HS_ODD_NO_HEAD,
		HS_ODD_NO_HEAD_WID));
	DSC_PR("hs_even_no_tail:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, HS_EVEN_NO_TAIL,
		HS_EVEN_NO_TAIL_WID));
	DSC_PR("hs_even_no_head:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, HS_EVEN_NO_HEAD,
		HS_EVEN_NO_HEAD_WID));
	DSC_PR("vs_odd_no_tail:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, VS_ODD_NO_TAIL,
		VS_ODD_NO_TAIL_WID));
	DSC_PR("vs_odd_no_head:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, VS_ODD_NO_HEAD,
		VS_ODD_NO_HEAD_WID));
	DSC_PR("vs_even_no_tail:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, VS_EVEN_NO_TAIL,
		VS_EVEN_NO_TAIL_WID));
	DSC_PR("vs_even_no_head:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, VS_EVEN_NO_HEAD,
		VS_EVEN_NO_HEAD_WID));
	DSC_PR("hc_htotal_offs_oddline:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, HC_HTOTAL_OFFS_ODDLINE,
		HC_HTOTAL_OFFS_ODDLINE_WID));
	DSC_PR("hc_htotal_offs_evenline:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, HC_HTOTAL_OFFS_EVENLINE,
		HC_HTOTAL_OFFS_EVENLINE_WID));
	DSC_PR("hc_active_odd_mode:%d\n", R_DSC_BIT(DSC_ASIC_CTRL19, HC_ACTIVE_ODD_MODE,
		HC_ACTIVE_ODD_MODE_WID));
	DSC_PR("------odd_mode end------\n");
}

static inline void dsc_dump_regs(struct aml_dsc_drv_s *dsc_drv)
{
	unsigned int reg;

	DSC_PR("dsc regs start----\n");
	for (reg = DSC_COMP_CTRL; reg <= DSC_ASIC_CTRL23; reg++)
		DSC_PR("0x%04x = 0x%08x\n", reg, R_DSC_REG(reg));
	DSC_PR("mux register\n");
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_SYNC_MODE, dsc_vcbus_reg_read(ENCP_VIDEO_SYNC_MODE));
	DSC_PR("0x%x = 0x%x\n", VPU_HDMI_SETTING, dsc_vcbus_reg_read(VPU_HDMI_SETTING));
	DSC_PR("encp bist register\n");
	DSC_PR("0x%x = 0x%x\n", ENCP_DVI_HSO_BEGIN, dsc_vcbus_reg_read(ENCP_DVI_HSO_BEGIN));
	DSC_PR("0x%x = 0x%x\n", ENCP_DVI_HSO_END, dsc_vcbus_reg_read(ENCP_DVI_HSO_END));
	DSC_PR("0x%x = 0x%x\n", ENCP_DVI_VSO_BLINE_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_BLINE_EVN));
	DSC_PR("0x%x = 0x%x\n", ENCP_DVI_VSO_ELINE_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_ELINE_EVN));
	DSC_PR("0x%x = 0x%x\n", ENCP_DVI_VSO_BEGIN_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_BEGIN_EVN));
	DSC_PR("0x%x = 0x%x\n", ENCP_DVI_VSO_END_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_END_EVN));
	DSC_PR("0x%x = 0x%x\n", ENCP_DE_H_BEGIN, dsc_vcbus_reg_read(ENCP_DE_H_BEGIN));
	DSC_PR("0x%x = 0x%x\n", ENCP_DE_H_END, dsc_vcbus_reg_read(ENCP_DE_H_END));
	DSC_PR("0x%x = 0x%x\n", ENCP_DE_V_BEGIN_EVEN, dsc_vcbus_reg_read(ENCP_DE_V_BEGIN_EVEN));
	DSC_PR("0x%x = 0x%x\n", ENCP_DE_V_END_EVEN, dsc_vcbus_reg_read(ENCP_DE_V_END_EVEN));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_MAX_PXCNT, dsc_vcbus_reg_read(ENCP_VIDEO_MAX_PXCNT));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_MAX_LNCNT, dsc_vcbus_reg_read(ENCP_VIDEO_MAX_LNCNT));
	DSC_PR("encp video register\n");
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_HSO_BEGIN, dsc_vcbus_reg_read(ENCP_VIDEO_HSO_BEGIN));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_HSO_END, dsc_vcbus_reg_read(ENCP_VIDEO_HSO_END));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_VSO_BLINE, dsc_vcbus_reg_read(ENCP_VIDEO_VSO_BLINE));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_VSO_ELINE, dsc_vcbus_reg_read(ENCP_VIDEO_VSO_ELINE));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_VSO_BEGIN, dsc_vcbus_reg_read(ENCP_VIDEO_VSO_BEGIN));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_VSO_END, dsc_vcbus_reg_read(ENCP_VIDEO_VSO_END));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_HAVON_BEGIN, dsc_vcbus_reg_read(ENCP_VIDEO_HAVON_BEGIN));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_HAVON_END, dsc_vcbus_reg_read(ENCP_VIDEO_HAVON_END));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_VAVON_BLINE, dsc_vcbus_reg_read(ENCP_VIDEO_VAVON_BLINE));
	DSC_PR("0x%x = 0x%x\n", ENCP_VIDEO_VAVON_ELINE, dsc_vcbus_reg_read(ENCP_VIDEO_VAVON_ELINE));
	DSC_PR("dsc regs end----\n\n");
}

static inline void set_dsc_pps_data(struct aml_dsc_drv_s *dsc_drv, char **parm)
{
	struct dsc_pps_data_s *pps_data = &dsc_drv->pps_data;
	int temp = 0;

	if (!strcmp(parm[1], "color_format")) {
		if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0)) {
			dsc_drv->dsc_debug.manual_set_select |= MANUAL_COLOR_FORMAT;
			dsc_drv->color_format = temp;
		} else {
			dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_COLOR_FORMAT);
		}
	}

	if (!strcmp(parm[1], "pic_w_h")) {
		if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0))
			dsc_drv->pps_data.pic_width = temp;
		if (parm[3] && (kstrtoint(parm[3], 10, &temp) == 0)) {
			dsc_drv->dsc_debug.manual_set_select |= MANUAL_PIC_W_H;
			pps_data->pic_height = temp;
		} else {
			dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_PIC_W_H);
		}
	}

	if (!strcmp(parm[1], "fps")) {
		if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0)) {
			dsc_drv->dsc_debug.manual_set_select |= MANUAL_VIDEO_FPS;
			dsc_drv->fps = temp;
		} else {
			dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_VIDEO_FPS);
		}
	}

	if (!strcmp(parm[1], "slice_w_h")) {
		if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0))
			pps_data->slice_width = temp;
		if (parm[3] && (kstrtoint(parm[3], 10, &temp) == 0)) {
			dsc_drv->dsc_debug.manual_set_select |= MANUAL_SLICE_W_H;
			pps_data->slice_height = temp;
		} else {
			dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_SLICE_W_H);
		}
	}

	if (!strcmp(parm[1], "bits_per_component")) {
		if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0)) {
			dsc_drv->dsc_debug.manual_set_select |= MANUAL_BITS_PER_COMPONENT;
			pps_data->bits_per_component = temp;
		} else {
			dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_BITS_PER_COMPONENT);
		}
	}

	if (!strcmp(parm[1], "bits_per_pixel")) {
		if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0)) {
			dsc_drv->dsc_debug.manual_set_select |= MANUAL_BITS_PER_PIXEL;
			pps_data->bits_per_pixel = temp;
		} else {
			dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_BITS_PER_PIXEL);
		}
	}

	if (!strcmp(parm[1], "line_buf_depth")) {
		if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0)) {
			dsc_drv->dsc_debug.manual_set_select |= MANUAL_LINE_BUF_DEPTH;
			pps_data->line_buf_depth = temp;
		} else {
			dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_LINE_BUF_DEPTH);
		}
	}

	if (!strcmp(parm[1], "prediction_mode")) {
		if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0))
			pps_data->block_pred_enable = temp;
		if (parm[3] && (kstrtoint(parm[3], 10, &temp) == 0)) {
			dsc_drv->dsc_debug.manual_set_select |= MANUAL_PREDICTION_MODE;
			pps_data->vbr_enable = temp;
		} else {
			dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_PREDICTION_MODE);
		}
	}

	if (!strcmp(parm[1], "some_rc_parameter")) {
		if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0))
			pps_data->rc_parameter_set.rc_model_size = temp;
		if (parm[3] && (kstrtoint(parm[3], 10, &temp) == 0))
			pps_data->rc_parameter_set.rc_edge_factor = temp;
		if (parm[4] && (kstrtoint(parm[4], 10, &temp) == 0))
			pps_data->rc_parameter_set.rc_tgt_offset_lo = temp;
		if (parm[5] && (kstrtoint(parm[5], 10, &temp) == 0)) {
			dsc_drv->dsc_debug.manual_set_select |= MANUAL_SOME_RC_PARAMETER;
			pps_data->rc_parameter_set.rc_tgt_offset_hi = temp;
		} else {
			dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_SOME_RC_PARAMETER);
		}
	}

	if (!strcmp(parm[1], "clear_manual_value"))
		dsc_drv->dsc_debug.manual_set_select = 0;

	DSC_PR("[%s] %s temp:%d manual_set_select:0x%x\n", __func__, parm[1], temp,
		dsc_drv->dsc_debug.manual_set_select);
}

static inline void set_dsc_tmg_ctrl(struct aml_dsc_drv_s *dsc_drv, char **parm)
{
	int temp = 0;

	if (parm[1] && (kstrtoint(parm[1], 10, &temp) == 0))
		dsc_drv->tmg_ctrl.tmg_havon_begin = temp;

	if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0))
		dsc_drv->tmg_ctrl.tmg_vavon_bline = temp;

	if (parm[3] && (kstrtoint(parm[3], 10, &temp) == 0))
		dsc_drv->tmg_ctrl.tmg_vavon_eline = temp;

	if (parm[4] && (kstrtoint(parm[4], 10, &temp) == 0))
		dsc_drv->tmg_ctrl.tmg_hso_begin = temp;

	if (parm[5] && (kstrtoint(parm[5], 10, &temp) == 0))
		dsc_drv->tmg_ctrl.tmg_hso_end = temp;

	if (parm[6] && (kstrtoint(parm[6], 10, &temp) == 0))
		dsc_drv->tmg_ctrl.tmg_vso_begin = temp;

	if (parm[7] && (kstrtoint(parm[7], 10, &temp) == 0))
		dsc_drv->tmg_ctrl.tmg_vso_end = temp;

	if (parm[8] && (kstrtoint(parm[8], 10, &temp) == 0))
		dsc_drv->tmg_ctrl.tmg_vso_bline = temp;

	if (parm[9] && (kstrtoint(parm[9], 10, &temp) == 0))
		dsc_drv->tmg_ctrl.tmg_vso_eline = temp - 1;

	if (parm[10] && (kstrtoint(parm[10], 10, &temp) == 0))
		dsc_drv->hc_vtotal_m1 = temp - 1;

	if (parm[11] && (kstrtoint(parm[11], 10, &temp) == 0)) {
		dsc_drv->dsc_debug.manual_set_select |= MANUAL_DSC_TMG_CTRL;
		dsc_drv->hc_htotal_m1 = temp;
	} else {
		DSC_PR("%s manual_tmg_ctrl error\n", __func__);
		dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_DSC_TMG_CTRL);
	}

	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_DSC_TMG_CTRL) {
		W_DSC_BIT(DSC_ASIC_CTRL4, dsc_drv->tmg_ctrl.tmg_havon_begin,
				TMG_HAVON_BEGIN, TMG_HAVON_BEGIN_WID);
		W_DSC_BIT(DSC_ASIC_CTRL5, dsc_drv->tmg_ctrl.tmg_vavon_bline,
				TMG_VAVON_BLINE, TMG_VAVON_BLINE_WID);
		W_DSC_BIT(DSC_ASIC_CTRL5, dsc_drv->tmg_ctrl.tmg_vavon_eline,
				TMG_VAVON_ELINE, TMG_VAVON_ELINE_WID);
		W_DSC_BIT(DSC_ASIC_CTRL6, dsc_drv->tmg_ctrl.tmg_hso_begin,
				TMG_HSO_BEGIN, TMG_HSO_BEGIN_WID);
		W_DSC_BIT(DSC_ASIC_CTRL6, dsc_drv->tmg_ctrl.tmg_hso_end,
				TMG_HSO_END, TMG_HSO_END_WID);
		W_DSC_BIT(DSC_ASIC_CTRL7, dsc_drv->tmg_ctrl.tmg_vso_begin,
				TMG_VSO_BEGIN, TMG_VSO_BEGIN_WID);
		W_DSC_BIT(DSC_ASIC_CTRL7, dsc_drv->tmg_ctrl.tmg_vso_end,
				TMG_VSO_END, TMG_VSO_END_WID);
		W_DSC_BIT(DSC_ASIC_CTRL8, dsc_drv->tmg_ctrl.tmg_vso_bline,
				TMG_VSO_BLINE, TMG_VSO_BLINE_WID);
		W_DSC_BIT(DSC_ASIC_CTRL8, dsc_drv->tmg_ctrl.tmg_vso_eline,
				TMG_VSO_ELINE, TMG_VSO_ELINE_WID);
		W_DSC_BIT(DSC_ASIC_CTRL18, dsc_drv->hc_vtotal_m1,
				HC_VTOTAL_M1, HC_VTOTAL_M1_WID);
		W_DSC_BIT(DSC_ASIC_CTRL18, dsc_drv->hc_htotal_m1,
				HC_HTOTAL_M1, HC_HTOTAL_M1_WID);

		DSC_PR("tmg_en:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL4, TMG_EN, TMG_EN_WID));
		DSC_PR("tmg_havon_begin:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL4, TMG_HAVON_BEGIN, TMG_HAVON_BEGIN_WID));
		DSC_PR("tmg_vavon_bline:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL5, TMG_VAVON_BLINE, TMG_VAVON_BLINE_WID));
		DSC_PR("tmg_vavon_eline:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL5, TMG_VAVON_ELINE, TMG_VAVON_ELINE_WID));
		DSC_PR("tmg_hso_begin:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL6, TMG_HSO_BEGIN, TMG_HSO_BEGIN_WID));
		DSC_PR("tmg_hso_end:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL6, TMG_HSO_END, TMG_HSO_END_WID));
		DSC_PR("tmg_vso_begin:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL7, TMG_VSO_BEGIN, TMG_VSO_BEGIN_WID));
		DSC_PR("tmg_vso_end:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL7, TMG_VSO_END, TMG_VSO_END_WID));
		DSC_PR("tmg_vso_bline:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL8, TMG_VSO_BLINE, TMG_VSO_BLINE_WID));
		DSC_PR("tmg_vso_eline:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL8, TMG_VSO_ELINE, TMG_VSO_ELINE_WID));
		DSC_PR("hc_vtotal_m1:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL18, HC_VTOTAL_M1, HC_VTOTAL_M1_WID));
		DSC_PR("hc_htotal_m1:%d\n",
			R_DSC_BIT(DSC_ASIC_CTRL18, HC_HTOTAL_M1, HC_HTOTAL_M1_WID));
	}
}

static inline void set_vpu_bist_tmg_ctrl(struct aml_dsc_drv_s *dsc_drv, char **parm)
{
	int temp = 0;

	if (parm[1] && (kstrtoint(parm[1], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_dvi_hso_begin = temp;

	if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_dvi_hso_end = temp;

	if (parm[3] && (kstrtoint(parm[3], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_vso_bline = temp;

	if (parm[4] && (kstrtoint(parm[4], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_vso_eline = temp;

	if (parm[5] && (kstrtoint(parm[5], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_vso_begin = temp;

	if (parm[6] && (kstrtoint(parm[6], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_vso_end = temp;

	if (parm[7] && (kstrtoint(parm[7], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_de_h_begin = temp;

	if (parm[8] && (kstrtoint(parm[8], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_de_h_end = temp;

	if (parm[9] && (kstrtoint(parm[9], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_de_v_begin = temp;

	if (parm[10] && (kstrtoint(parm[10], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_de_v_end = temp;

	if (parm[11] && (kstrtoint(parm[11], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.h_total = temp - 1;

	if (parm[12] && (kstrtoint(parm[12], 10, &temp) == 0)) {
		dsc_drv->dsc_debug.manual_set_select |= MANUAL_VPU_BIST_TMG_CTRL;
		dsc_drv->encp_timing_ctrl.v_total = temp - 1;
	} else {
		DSC_PR("%s manual_vpu_bit_tmg error\n", __func__);
		dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_VPU_BIST_TMG_CTRL);
	}

	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_VPU_BIST_TMG_CTRL) {
		dsc_vcbus_reg_write(ENCP_DVI_HSO_BEGIN,
					dsc_drv->encp_timing_ctrl.encp_dvi_hso_begin);
		dsc_vcbus_reg_write(ENCP_DVI_HSO_END,
					dsc_drv->encp_timing_ctrl.encp_dvi_hso_end);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_BLINE_EVN,
					dsc_drv->encp_timing_ctrl.encp_vso_bline);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_ELINE_EVN,
					dsc_drv->encp_timing_ctrl.encp_vso_eline);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_BEGIN_EVN,
					dsc_drv->encp_timing_ctrl.encp_vso_begin);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_END_EVN,
					dsc_drv->encp_timing_ctrl.encp_vso_end);
		dsc_vcbus_reg_write(ENCP_DE_H_BEGIN,
					dsc_drv->encp_timing_ctrl.encp_de_h_begin);
		dsc_vcbus_reg_write(ENCP_DE_H_END,
					dsc_drv->encp_timing_ctrl.encp_de_h_end);
		dsc_vcbus_reg_write(ENCP_DE_V_END_EVEN,
					dsc_drv->encp_timing_ctrl.encp_de_v_end);
		dsc_vcbus_reg_write(ENCP_DE_V_BEGIN_EVEN,
					dsc_drv->encp_timing_ctrl.encp_de_v_begin);
		dsc_vcbus_reg_write(ENCP_VIDEO_MAX_PXCNT, dsc_drv->encp_timing_ctrl.v_total);
		dsc_vcbus_reg_write(ENCP_VIDEO_MAX_LNCNT, dsc_drv->encp_timing_ctrl.h_total);

		DSC_PR("encp_dvi_hso_begin(%#x):%d\n",
				ENCP_DVI_HSO_BEGIN, dsc_vcbus_reg_read(ENCP_DVI_HSO_BEGIN));
		DSC_PR("encp_dvi_hso_end(%#x):%d\n",
				ENCP_DVI_HSO_END, dsc_vcbus_reg_read(ENCP_DVI_HSO_END));
		DSC_PR("encp_vso_bline(%#x):%d\n",
				ENCP_DVI_VSO_BLINE_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_BLINE_EVN));
		DSC_PR("encp_vso_eline(%#x):%d\n",
				ENCP_DVI_VSO_ELINE_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_ELINE_EVN));
		DSC_PR("encp_vso_begin(%#x):%d\n",
				ENCP_DVI_VSO_BEGIN_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_BEGIN_EVN));
		DSC_PR("encp_vso_end(%#x):%d\n",
				ENCP_DVI_VSO_END_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_END_EVN));
		DSC_PR("encp_de_h_begin(%#x):%d\n",
				ENCP_DE_H_BEGIN, dsc_vcbus_reg_read(ENCP_DE_H_BEGIN));
		DSC_PR("encp_de_h_end(%#x):%d\n",
				ENCP_DE_H_END, dsc_vcbus_reg_read(ENCP_DE_H_END));
		DSC_PR("encp_de_v_begin(%#x):%d\n",
				ENCP_DE_V_BEGIN_EVEN, dsc_vcbus_reg_read(ENCP_DE_V_BEGIN_EVEN));
		DSC_PR("encp_de_v_end(%#x):%d\n",
				ENCP_DE_V_END_EVEN, dsc_vcbus_reg_read(ENCP_DE_V_END_EVEN));
		DSC_PR("encp_v_total(%#x):%d\n",
				ENCP_VIDEO_MAX_PXCNT, dsc_vcbus_reg_read(ENCP_VIDEO_MAX_PXCNT));
		DSC_PR("encp_h_total(%#x):%d\n",
				ENCP_VIDEO_MAX_LNCNT, dsc_vcbus_reg_read(ENCP_VIDEO_MAX_LNCNT));
	}
}

static inline void set_vpu_video_tmg_ctrl(struct aml_dsc_drv_s *dsc_drv, char **parm)
{
	int temp = 0;

	if (parm[1] && (kstrtoint(parm[1], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_hso_begin = temp;

	if (parm[2] && (kstrtoint(parm[2], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_hso_end = temp;

	if (parm[3] && (kstrtoint(parm[3], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_video_vso_bline = temp;

	if (parm[4] && (kstrtoint(parm[4], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_video_vso_eline = temp;

	if (parm[5] && (kstrtoint(parm[5], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_video_vso_begin = temp;

	if (parm[6] && (kstrtoint(parm[6], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_video_vso_end = temp;

	if (parm[7] && (kstrtoint(parm[7], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_havon_begin = temp;

	if (parm[8] && (kstrtoint(parm[8], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_havon_end = temp;

	if (parm[9] && (kstrtoint(parm[9], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_vavon_bline = temp;

	if (parm[10] && (kstrtoint(parm[10], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.encp_vavon_eline = temp;

	if (parm[11] && (kstrtoint(parm[11], 10, &temp) == 0))
		dsc_drv->encp_timing_ctrl.h_total = temp - 1;

	if (parm[12] && (kstrtoint(parm[12], 10, &temp) == 0)) {
		dsc_drv->dsc_debug.manual_set_select |= MANUAL_VPU_VIDEO_TMG_CTRL;
		dsc_drv->encp_timing_ctrl.v_total = temp - 1;
	} else {
		DSC_PR("%s manual_vpu_bit_tmg error\n", __func__);
		dsc_drv->dsc_debug.manual_set_select &= (~MANUAL_VPU_VIDEO_TMG_CTRL);
	}

	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_VPU_VIDEO_TMG_CTRL) {
		dsc_vcbus_reg_write(ENCP_DVI_HSO_BEGIN,
					dsc_drv->encp_timing_ctrl.encp_hso_begin);
		dsc_vcbus_reg_write(ENCP_DVI_HSO_END,
					dsc_drv->encp_timing_ctrl.encp_hso_end);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_BLINE_EVN,
					dsc_drv->encp_timing_ctrl.encp_video_vso_bline);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_ELINE_EVN,
					dsc_drv->encp_timing_ctrl.encp_video_vso_eline);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_BEGIN_EVN,
					dsc_drv->encp_timing_ctrl.encp_video_vso_begin);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_END_EVN,
					dsc_drv->encp_timing_ctrl.encp_video_vso_end);
		dsc_vcbus_reg_write(ENCP_DE_H_BEGIN,
					dsc_drv->encp_timing_ctrl.encp_havon_begin);
		dsc_vcbus_reg_write(ENCP_DE_H_END,
					dsc_drv->encp_timing_ctrl.encp_havon_end);
		dsc_vcbus_reg_write(ENCP_DE_V_END_EVEN,
					dsc_drv->encp_timing_ctrl.encp_vavon_bline);
		dsc_vcbus_reg_write(ENCP_DE_V_BEGIN_EVEN,
					dsc_drv->encp_timing_ctrl.encp_vavon_eline);
		dsc_vcbus_reg_write(ENCP_VIDEO_MAX_LNCNT, dsc_drv->encp_timing_ctrl.h_total);
		dsc_vcbus_reg_write(ENCP_VIDEO_MAX_PXCNT, dsc_drv->encp_timing_ctrl.v_total);

		DSC_PR("encp_hso_begin(%#x):%d\n",
				ENCP_DVI_HSO_BEGIN, dsc_vcbus_reg_read(ENCP_DVI_HSO_BEGIN));
		DSC_PR("encp_hso_end(%#x):%d\n",
				ENCP_DVI_HSO_END, dsc_vcbus_reg_read(ENCP_DVI_HSO_END));
		DSC_PR("encp_video_vso_bline(%#x):%d\n",
				ENCP_DVI_VSO_BLINE_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_BLINE_EVN));
		DSC_PR("encp_video_vso_eline(%#x):%d\n",
				ENCP_DVI_VSO_ELINE_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_ELINE_EVN));
		DSC_PR("encp_video_vso_begin(%#x):%d\n",
				ENCP_DVI_VSO_BEGIN_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_BEGIN_EVN));
		DSC_PR("encp_video_vso_end(%#x):%d\n",
				ENCP_DVI_VSO_END_EVN, dsc_vcbus_reg_read(ENCP_DVI_VSO_END_EVN));
		DSC_PR("encp_havon_begin(%#x):%d\n",
				ENCP_DE_H_BEGIN, dsc_vcbus_reg_read(ENCP_DE_H_BEGIN));
		DSC_PR("encp_havon_end(%#x):%d\n",
				ENCP_DE_H_END, dsc_vcbus_reg_read(ENCP_DE_H_END));
		DSC_PR("encp_vavon_bline(%#x):%d\n",
				ENCP_DE_V_BEGIN_EVEN, dsc_vcbus_reg_read(ENCP_DE_V_BEGIN_EVEN));
		DSC_PR("encp_vavon_eline(%#x):%d\n",
				ENCP_DE_V_END_EVEN, dsc_vcbus_reg_read(ENCP_DE_V_END_EVEN));
		DSC_PR("v_total(%#x):%d\n",
				ENCP_VIDEO_MAX_PXCNT, dsc_vcbus_reg_read(ENCP_VIDEO_MAX_PXCNT));
		DSC_PR("h_total(%#x):%d\n",
				ENCP_VIDEO_MAX_LNCNT, dsc_vcbus_reg_read(ENCP_VIDEO_MAX_LNCNT));
	}
}

static ssize_t dsc_debug_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	char *buf_orig, *parm[47] = {NULL};
	struct aml_dsc_drv_s *dsc_drv;
	unsigned int temp = 0;
	unsigned int val = 0;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	dsc_drv = dev_get_drvdata(dev);
	dsc_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "read")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0))
			pr_info("reg:0x%x val:0x%x\n", temp, R_DSC_REG(temp));
	} else if (!strcmp(parm[0], "write")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (parm[1] && (kstrtouint(parm[2], 16, &val) == 0))
				W_DSC_REG(temp, val);
		}
		DSC_PR("reg:0x%x val:0x%x\n", temp, R_DSC_REG(temp));
	} else if (!strcmp(parm[0], "state")) {
		dsc_print_state(dsc_drv);
	} else if (!strcmp(parm[0], "dump_reg")) {
		dsc_dump_regs(dsc_drv);
	} else if (!strcmp(parm[0], "get_reg_config")) {
		dsc_print_reg_value(dsc_drv);
	} else if (!strcmp(parm[0], "get_pps_data")) {
		struct dsc_notifier_data_s notifier_data;

		memset(&notifier_data, 0, sizeof(notifier_data));
		hdmitx_get_pps_data(&notifier_data);
		DSC_PR("major:0x%x minor:0x%x\n", notifier_data.pps_data.dsc_version_major,
			notifier_data.pps_data.dsc_version_minor);
	} else if (!strcmp(parm[0], "use_dsc_model_value")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0))
			dsc_drv->dsc_debug.use_dsc_model_value = temp;
		DSC_PR("use_dsc_model_value:0x%x\n", dsc_drv->dsc_debug.use_dsc_model_value);
	} else if (!strcmp(parm[0], "manual_pps_para")) {
		set_dsc_pps_data(dsc_drv, (char **)&parm);
	} else if (!strcmp(parm[0], "calculate_dsc_data")) {
		calculate_dsc_data(dsc_drv);
	} else if (!strcmp(parm[0], "config_dsc_reg")) {
		dsc_config_register(dsc_drv);
	} else if (!strcmp(parm[0], "is_enable_dsc")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0))
			set_dsc_en(temp);
		DSC_PR("is_enable_dsc value:%d\n", temp);
	} else if (!strcmp(parm[0], "manual_dsc_tmg")) {
		set_dsc_tmg_ctrl(dsc_drv, (char **)&parm);
	} else if (!strcmp(parm[0], "manual_vpu_bist_tmg")) {
		set_vpu_bist_tmg_ctrl(dsc_drv, (char **)&parm);
	} else if (!strcmp(parm[0], "manual_vpu_video_tmg")) {
		set_vpu_video_tmg_ctrl(dsc_drv, (char **)&parm);
	} else if (!strcmp(parm[0], "clr_dsc_status")) {
		W_DSC_BIT(DSC_ASIC_CTRL0, 1, CLR_SSM_FIFO_STS, CLR_SSM_FIFO_STS_WID);
		W_DSC_BIT(DSC_ASIC_CTRLD, 0xff, C0S0_CLR_CB_STS, 8);
		DSC_PR("cx_sx_cb_under_flow:0x%x\n", R_DSC_REG(DSC_ASIC_CTRL11));
		DSC_PR("c0_ssm_fifo_sts:0x%x\n", R_DSC_REG(DSC_ASIC_CTRL13));
		DSC_PR("c1_ssm_fifo_sts:0x%x\n", R_DSC_REG(DSC_ASIC_CTRL14));
		DSC_PR("c2_ssm_fifo_sts:0x%x\n", R_DSC_REG(DSC_ASIC_CTRL15));
		DSC_PR("c3_ssm_fifo_sts:0x%x\n", R_DSC_REG(DSC_ASIC_CTRL16));
		W_DSC_BIT(DSC_ASIC_CTRL0, 0, CLR_SSM_FIFO_STS, CLR_SSM_FIFO_STS_WID);
		W_DSC_BIT(DSC_ASIC_CTRLD, 0x0, C0S0_CLR_CB_STS, 8);
	} else if (!strcmp(parm[0], "read_dsc_status")) {
		DSC_PR("cx_sx_cb_under_flow:0x%x\n", R_DSC_REG(DSC_ASIC_CTRL11));
		DSC_PR("c0_ssm_fifo_sts:0x%x\n", R_DSC_REG(DSC_ASIC_CTRL13));
		DSC_PR("c1_ssm_fifo_sts:0x%x\n", R_DSC_REG(DSC_ASIC_CTRL14));
		DSC_PR("c2_ssm_fifo_sts:0x%x\n", R_DSC_REG(DSC_ASIC_CTRL15));
		DSC_PR("c3_ssm_fifo_sts:0x%x\n", R_DSC_REG(DSC_ASIC_CTRL16));
	} else {
		pr_info("unknown command\n");
	}

	kfree(buf_orig);
	return count;
}

static struct device_attribute dsc_debug_attrs[] = {
	__ATTR(help, 0444, dsc_debug_help, NULL),
	__ATTR(status, 0444, dsc_status_show, NULL),
	__ATTR(debug, 0644, dsc_debug_help, dsc_debug_store),
};

int dsc_debug_file_create(struct aml_dsc_drv_s *dsc_drv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dsc_debug_attrs); i++) {
		if (device_create_file(dsc_drv->dev, &dsc_debug_attrs[i])) {
			DSC_ERR("create lcd debug attribute %s fail\n",
				dsc_debug_attrs[i].attr.name);
		}
	}

	return 0;
}

int dsc_debug_file_remove(struct aml_dsc_drv_s *dsc_drv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dsc_debug_attrs); i++)
		device_remove_file(dsc_drv->dev, &dsc_debug_attrs[i]);
	return 0;
}

