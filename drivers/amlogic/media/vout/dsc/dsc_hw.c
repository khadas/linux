// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vout/dsc/dsc.h>
#include "dsc_reg.h"
#include "dsc_drv.h"
#include "dsc_config.h"
#include "dsc_debug.h"

unsigned int dsc_tmg[DSC_ENCODE_MAX][11] = {
{0x42c, 0x51, 0x51 + 4320, 0x400, 0x410, 1019, 1020, 0x11, 0x25, 4399, 1661}, //0--
{0x42c, 0x52, 0x1, 0x400, 0x410, 1019, 1020, 0x11, 0x25, 4399, 1247}, //5-- 8k 444 30hz bpp9.9375
{58, 83, 3, 24, 32, 22, 22, 0x13, 0x27, 4399, 1247}, //7-- 8k 420 60hz bpp7.4375
};

unsigned int R_DSC_REG(unsigned int reg)
{
	struct aml_dsc_drv_s *dsc_drv = NULL;

	dsc_drv = dsc_drv_get();
	if (dsc_drv && dsc_drv->dsc_reg_base)
		return readl(dsc_drv->dsc_reg_base + (reg << 2));
	return 0;
}

void W_DSC_REG(unsigned int reg, unsigned int val)
{
	struct aml_dsc_drv_s *dsc_drv = NULL;

	dsc_drv = dsc_drv_get();
	if (dsc_drv && dsc_drv->dsc_reg_base)
		writel(val, (dsc_drv->dsc_reg_base + (reg << 2)));
}

unsigned int R_DSC_BIT(u32 reg, const u32 start, const u32 len)
{
	return ((R_DSC_REG(reg) >> (start)) & ((1L << (len)) - 1));
}

void W_DSC_BIT(u32 reg, const u32 value, const u32 start, const u32 len)
{
	W_DSC_REG(reg, ((R_DSC_REG(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

unsigned int dsc_vcbus_reg_read(unsigned int _reg)
{
#ifdef CONFIG_AMLOGIC_VPU
	return vpu_vcbus_read(_reg);
#else
	return aml_read_vcbus(_reg);
#endif
};

void dsc_vcbus_reg_write(unsigned int _reg, unsigned int _value)
{
#ifdef CONFIG_AMLOGIC_VPU
	return vpu_vcbus_write(_reg, _value);
#else
	return aml_write_vcbus(_reg, _value);
#endif
};

void dsc_vcbus_reg_setb(unsigned int reg, unsigned int value,
		       unsigned int _start, unsigned int _len)
{
	dsc_vcbus_reg_write(reg, ((dsc_vcbus_reg_read(reg) &
		(~(((1L << _len) - 1) << _start))) |
		((value & ((1L << _len) - 1)) << _start)));
}

unsigned int dsc_vcbus_reg_getb(unsigned int reg,
			       unsigned int _start, unsigned int _len)
{
	return (dsc_vcbus_reg_read(reg) >> _start) & ((1L << _len) - 1);
}

void set_dsc_en(unsigned int enable)
{
	if (enable) {
		dsc_vcbus_reg_setb(ENCP_VIDEO_SYNC_MODE, 1, 4, 1);
		dsc_vcbus_reg_setb(VPU_HDMI_SETTING, 1, 31, 1);
		dsc_vcbus_reg_setb(VPU_HDMI_SETTING, 0, 7, 1);
		W_DSC_BIT(DSC_ASIC_CTRL0, 1, DSC_ENC_EN, DSC_ENC_EN_WID);
		W_DSC_BIT(DSC_ASIC_CTRL4, 1, TMG_EN, TMG_EN_WID);
	} else {
		dsc_vcbus_reg_setb(ENCP_VIDEO_SYNC_MODE, 0, 4, 1);
		dsc_vcbus_reg_setb(VPU_HDMI_SETTING, 0, 31, 1);
		W_DSC_BIT(DSC_ASIC_CTRL0, 0, DSC_ENC_EN, DSC_ENC_EN_WID);
		W_DSC_BIT(DSC_ASIC_CTRL4, 0, TMG_EN, TMG_EN_WID);
	}
}

static void dsc_config_rc_parameter_register(struct dsc_rc_parameter_set *rc_parameter_set)
{
	int i;
	unsigned char range_bpg_offset;

	for (i = 0; i < RC_BUF_THRESH_NUM; i++) {
		W_DSC_BIT(DSC_COMP_RC_BUF_THRESH_0 + i, rc_parameter_set->rc_buf_thresh[i] << 6,
			RC_BUF_THRESH_0, RC_BUF_THRESH_0_WID);
	}

	for (i = 0; i < RC_RANGE_PARAMETERS_NUM; i++) {
		range_bpg_offset =
			rc_parameter_set->rc_range_parameters[i].range_bpg_offset & 0x3f;
		W_DSC_BIT(DSC_COMP_RC_RANGE_0 + i, range_bpg_offset,
			RANGE_BPG_OFFSET_0, RANGE_BPG_OFFSET_0_WID);
		W_DSC_BIT(DSC_COMP_RC_RANGE_0 + i,
			rc_parameter_set->rc_range_parameters[i].range_max_qp,
			RANGE_MAX_QP_0, RANGE_MAX_QP_0_WID);
		W_DSC_BIT(DSC_COMP_RC_RANGE_0 + i,
			rc_parameter_set->rc_range_parameters[i].range_min_qp,
			RANGE_MIN_QP_0, RANGE_MIN_QP_0_WID);
	}
}

static void dsc_config_timing_register(struct aml_dsc_drv_s *dsc_drv)
{
	struct dsc_timing_gen_ctrl tmg_ctrl = dsc_drv->tmg_ctrl;
	struct encp_timing_gen_ctrl encp_timing_ctrl = dsc_drv->encp_timing_ctrl;

	W_DSC_BIT(DSC_ASIC_CTRL4, tmg_ctrl.tmg_havon_begin, TMG_HAVON_BEGIN, TMG_HAVON_BEGIN_WID);
	W_DSC_BIT(DSC_ASIC_CTRL5, tmg_ctrl.tmg_vavon_bline, TMG_VAVON_BLINE, TMG_VAVON_BLINE_WID);
	W_DSC_BIT(DSC_ASIC_CTRL5, tmg_ctrl.tmg_vavon_eline, TMG_VAVON_ELINE, TMG_VAVON_ELINE_WID);
	W_DSC_BIT(DSC_ASIC_CTRL6, tmg_ctrl.tmg_hso_begin, TMG_HSO_BEGIN, TMG_HSO_BEGIN_WID);
	W_DSC_BIT(DSC_ASIC_CTRL6, tmg_ctrl.tmg_hso_end, TMG_HSO_END, TMG_HSO_END_WID);
	W_DSC_BIT(DSC_ASIC_CTRL7, tmg_ctrl.tmg_vso_begin, TMG_VSO_BEGIN, TMG_VSO_BEGIN_WID);
	W_DSC_BIT(DSC_ASIC_CTRL7, tmg_ctrl.tmg_vso_end, TMG_VSO_END, TMG_VSO_END_WID);
	W_DSC_BIT(DSC_ASIC_CTRL8, tmg_ctrl.tmg_vso_bline, TMG_VSO_BLINE, TMG_VSO_BLINE_WID);
	W_DSC_BIT(DSC_ASIC_CTRL8, tmg_ctrl.tmg_vso_eline, TMG_VSO_ELINE, TMG_VSO_ELINE_WID);
	W_DSC_BIT(DSC_ASIC_CTRL18, dsc_drv->hc_vtotal_m1, HC_VTOTAL_M1, HC_VTOTAL_M1_WID);
	W_DSC_BIT(DSC_ASIC_CTRL18, dsc_drv->hc_htotal_m1, HC_HTOTAL_M1, HC_HTOTAL_M1_WID);

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_VPU_BIST_TMG_CTRL)) {
		//bist set
		dsc_vcbus_reg_write(ENCP_DVI_HSO_BEGIN, encp_timing_ctrl.encp_dvi_hso_begin);
		dsc_vcbus_reg_write(ENCP_DVI_HSO_END, encp_timing_ctrl.encp_dvi_hso_end);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_BLINE_EVN, encp_timing_ctrl.encp_vso_bline);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_ELINE_EVN, encp_timing_ctrl.encp_vso_eline);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_BEGIN_EVN, encp_timing_ctrl.encp_dvi_hso_begin);
		dsc_vcbus_reg_write(ENCP_DVI_VSO_END_EVN, encp_timing_ctrl.encp_dvi_hso_end);
		dsc_vcbus_reg_write(ENCP_DE_H_BEGIN, encp_timing_ctrl.encp_de_h_begin);
		dsc_vcbus_reg_write(ENCP_DE_H_END, encp_timing_ctrl.encp_de_h_end);
		dsc_vcbus_reg_write(ENCP_DE_V_END_EVEN, encp_timing_ctrl.encp_de_v_end);
		dsc_vcbus_reg_write(ENCP_DE_V_BEGIN_EVEN, encp_timing_ctrl.encp_de_v_begin);
	}

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_VPU_VIDEO_TMG_CTRL)) {
		//video set
		dsc_vcbus_reg_write(ENCP_VIDEO_HSO_BEGIN, encp_timing_ctrl.encp_hso_begin);
		dsc_vcbus_reg_write(ENCP_VIDEO_HSO_END, encp_timing_ctrl.encp_hso_end);
		dsc_vcbus_reg_write(ENCP_VIDEO_VSO_BLINE, encp_timing_ctrl.encp_video_vso_bline);
		dsc_vcbus_reg_write(ENCP_VIDEO_VSO_ELINE, encp_timing_ctrl.encp_video_vso_eline);
		dsc_vcbus_reg_write(ENCP_VIDEO_VSO_BEGIN, 0);
		dsc_vcbus_reg_write(ENCP_VIDEO_VSO_END, 0);
		dsc_vcbus_reg_write(ENCP_VIDEO_HAVON_BEGIN, encp_timing_ctrl.encp_havon_begin);
		dsc_vcbus_reg_write(ENCP_VIDEO_HAVON_END, encp_timing_ctrl.encp_havon_end);
		dsc_vcbus_reg_write(ENCP_VIDEO_VAVON_BLINE, encp_timing_ctrl.encp_vavon_bline);
		dsc_vcbus_reg_write(ENCP_VIDEO_VAVON_ELINE, encp_timing_ctrl.encp_vavon_eline);
		dsc_vcbus_reg_write(ENCP_VIDEO_MAX_PXCNT, encp_timing_ctrl.v_total);
		dsc_vcbus_reg_write(ENCP_VIDEO_MAX_LNCNT, encp_timing_ctrl.h_total);
	}
}

void dsc_config_register(struct aml_dsc_drv_s *dsc_drv)
{
	struct dsc_pps_data_s *pps_data = &dsc_drv->pps_data;

	/* config pps register begin */
	W_DSC_BIT(DSC_COMP_CTRL, pps_data->native_422, NATIVE_422, NATIVE_422_WID);
	W_DSC_BIT(DSC_COMP_CTRL, pps_data->native_420, NATIVE_420, NATIVE_420_WID);
	W_DSC_BIT(DSC_COMP_CTRL, pps_data->convert_rgb, CONVERT_RGB, CONVERT_RGB_WID);
	W_DSC_BIT(DSC_COMP_CTRL, pps_data->block_pred_enable,
			BLOCK_PRED_ENABLE, BLOCK_PRED_ENABLE_WID);
	W_DSC_BIT(DSC_COMP_CTRL, pps_data->vbr_enable, VBR_ENABLE, VBR_ENABLE_WID);
	W_DSC_BIT(DSC_COMP_CTRL, dsc_drv->full_ich_err_precision,
			FULL_ICH_ERR_PRECISION, FULL_ICH_ERR_PRECISION_WID);
	W_DSC_BIT(DSC_COMP_CTRL, pps_data->dsc_version_minor,
			DSC_VERSION_MINOR, DSC_VERSION_MINOR_WID);

	W_DSC_BIT(DSC_COMP_PIC_SIZE, pps_data->pic_width, PCI_WIDTH, PCI_WIDTH_WID);
	W_DSC_BIT(DSC_COMP_PIC_SIZE, pps_data->pic_height, PCI_HEIGHT, PCI_HEIGHT_WID);
	W_DSC_BIT(DSC_COMP_SLICE_SIZE, pps_data->slice_width, SLICE_WIDTH, SLICE_WIDTH_WID);
	W_DSC_BIT(DSC_COMP_SLICE_SIZE, pps_data->slice_height, SLICE_HEIGHT, SLICE_HEIGHT_WID);

	W_DSC_BIT(DSC_COMP_BIT_DEPTH, pps_data->line_buf_depth, LINE_BUF_DEPTH, LINE_BUF_DEPTH_WID);
	W_DSC_BIT(DSC_COMP_BIT_DEPTH, pps_data->bits_per_component,
			BITS_PER_COMPONENT, BITS_PER_COMPONENT_WID);
	W_DSC_BIT(DSC_COMP_BIT_DEPTH, pps_data->bits_per_pixel, BITS_PER_PIXEL, BITS_PER_PIXEL_WID);

	W_DSC_BIT(DSC_COMP_RC_BITS_SIZE, dsc_drv->rcb_bits, RCB_BITS, RCB_BITS_WID);
	W_DSC_BIT(DSC_COMP_RC_BITS_SIZE, pps_data->rc_parameter_set.rc_model_size,
			RC_MODEL_SIZE, RC_MODEL_SIZE_WID);
	W_DSC_BIT(DSC_COMP_RC_TGT_QUANT, pps_data->rc_parameter_set.rc_tgt_offset_hi,
			RC_TGT_OFFSET_HI, RC_TGT_OFFSET_HI_WID);
	W_DSC_BIT(DSC_COMP_RC_TGT_QUANT, pps_data->rc_parameter_set.rc_tgt_offset_lo,
			RC_TGT_OFFSET_LO, RC_TGT_OFFSET_LO_WID);
	W_DSC_BIT(DSC_COMP_RC_TGT_QUANT, pps_data->rc_parameter_set.rc_edge_factor,
			RC_EDGE_FACTOR, RC_EDGE_FACTOR_WID);
	W_DSC_BIT(DSC_COMP_RC_TGT_QUANT, pps_data->rc_parameter_set.rc_quant_incr_limit1,
			RC_QUANT_INCR_LIMIT1, RC_QUANT_INCR_LIMIT1_WID);
	W_DSC_BIT(DSC_COMP_RC_TGT_QUANT, pps_data->rc_parameter_set.rc_quant_incr_limit0,
			RC_QUANT_INCR_LIMIT0, RC_QUANT_INCR_LIMIT0_WID);

	W_DSC_BIT(DSC_COMP_INITIAL_DELAY, pps_data->initial_xmit_delay,
			INITIAL_XMIT_DELAY, INITIAL_XMIT_DELAY_WID);
	W_DSC_BIT(DSC_COMP_INITIAL_DELAY, pps_data->initial_dec_delay,
			INITIAL_DEC_DELAY, INITIAL_DEC_DELAY_WID);
	W_DSC_BIT(DSC_COMP_INITIAL_OFFSET_SCALE, pps_data->initial_scale_value,
			INITIAL_SCALE_VALUE, INITIAL_SCALE_VALUE_WID);
	W_DSC_BIT(DSC_COMP_INITIAL_OFFSET_SCALE, pps_data->initial_offset,
			INITIAL_OFFSET, INITIAL_OFFSET_WID);
	W_DSC_BIT(DSC_COMP_SEC_OFS_ADJ, pps_data->second_line_offset_adj,
			SECOND_LINE_OFS_ADJ, SECOND_LINE_OFS_ADJ_WID);

	W_DSC_BIT(DSC_COMP_FS_BPG_OFS, pps_data->first_line_bpg_offset,
			FIRST_LINE_BPG_OFS, FIRST_LINE_BPG_OFS_WID);
	W_DSC_BIT(DSC_COMP_FS_BPG_OFS, pps_data->second_line_bpg_offset,
			SECOND_LINE_BPG_OFS, SECOND_LINE_BPG_OFS_WID);
	W_DSC_BIT(DSC_COMP_NFS_BPG_OFFSET, pps_data->nfl_bpg_offset,
			NFL_BPG_OFFSET, NFL_BPG_OFFSET_WID);
	W_DSC_BIT(DSC_COMP_NFS_BPG_OFFSET, pps_data->nsl_bpg_offset,
			NSL_BPG_OFFSET, NSL_BPG_OFFSET_WID);

	dsc_config_rc_parameter_register(&pps_data->rc_parameter_set);

	W_DSC_BIT(DSC_COMP_FLATNESS, pps_data->flatness_min_qp,
			FLATNESS_MIN_QP, FLATNESS_MIN_QP_WID);
	W_DSC_BIT(DSC_COMP_FLATNESS, pps_data->flatness_max_qp,
			FLATNESS_MAX_QP, FLATNESS_MAX_QP_WID);
	W_DSC_BIT(DSC_COMP_FLATNESS, dsc_drv->flatness_det_thresh,
			FLATNESS_DET_THRESH, FLATNESS_DET_THRESH_WID);
	W_DSC_BIT(DSC_COMP_SCALE, pps_data->scale_decrement_interval,
			SCALE_DECREMENT_INTERVAL, SCALE_DECREMENT_INTERVAL_WID);
	W_DSC_BIT(DSC_COMP_SCALE, pps_data->scale_increment_interval,
			SCALE_INCREMENT_INTERVAL, SCALE_INCREMENT_INTERVAL_WID);
	W_DSC_BIT(DSC_COMP_SLICE_FINAL_OFFSET, pps_data->slice_bpg_offset,
			SLICE_BPG_OFFSET, SLICE_BPG_OFFSET_WID);
	W_DSC_BIT(DSC_COMP_SLICE_FINAL_OFFSET, pps_data->final_offset,
			FINAL_OFFSET, FINAL_OFFSET_WID);

	W_DSC_BIT(DSC_COMP_CHUNK_WORD_SIZE, dsc_drv->mux_word_size,
			MUX_WORD_SIZE, MUX_WORD_SIZE_WID);
	W_DSC_BIT(DSC_COMP_CHUNK_WORD_SIZE, pps_data->chunk_size,
			CHUNK_SIZE, CHUNK_SIZE_WID);
	W_DSC_BIT(DSC_COMP_FLAT_QP, dsc_drv->very_flat_qp,
			VERY_FLAT_QP, VERY_FLAT_QP_WID);
	W_DSC_BIT(DSC_COMP_FLAT_QP, dsc_drv->somewhat_flat_qp_delta,
			SOMEWHAT_FLAT_QP_DELTA, SOMEWHAT_FLAT_QP_DELTA_WID);
	W_DSC_BIT(DSC_COMP_FLAT_QP, dsc_drv->somewhat_flat_qp_thresh,
			SOMEWHAT_FLAT_QP_THRESH, SOMEWHAT_FLAT_QP_THRESH_WID);
	/* config pps register end */

	W_DSC_BIT(DSC_ASIC_CTRL0, dsc_drv->slice_num_m1,
			SLICE_NUM_M1, SLICE_NUM_M1_WID);
	W_DSC_BIT(DSC_ASIC_CTRL0, dsc_drv->clr_ssm_fifo_sts,
			CLR_SSM_FIFO_STS, CLR_SSM_FIFO_STS_WID);
	W_DSC_BIT(DSC_ASIC_CTRL0, dsc_drv->dsc_enc_frm_latch_en,
			DSC_ENC_FRM_LATCH_EN, DSC_ENC_FRM_LATCH_EN_WID);
	W_DSC_BIT(DSC_ASIC_CTRL0, dsc_drv->pix_per_clk,
			PIX_PER_CLK, PIX_PER_CLK_WID);
	W_DSC_BIT(DSC_ASIC_CTRL0, dsc_drv->inbuf_rd_dly0,
			INBUF_RD_DLY0, INBUF_RD_DLY0_WID);
	W_DSC_BIT(DSC_ASIC_CTRL0, dsc_drv->inbuf_rd_dly1,
			INBUF_RD_DLY1, INBUF_RD_DLY1_WID);

	W_DSC_BIT(DSC_ASIC_CTRL1, dsc_drv->c3_clk_en, C3_CLK_EN, C3_CLK_EN_WID);
	W_DSC_BIT(DSC_ASIC_CTRL1, dsc_drv->c2_clk_en, C2_CLK_EN, C2_CLK_EN_WID);
	W_DSC_BIT(DSC_ASIC_CTRL1, dsc_drv->c1_clk_en, C1_CLK_EN, C1_CLK_EN_WID);
	W_DSC_BIT(DSC_ASIC_CTRL1, dsc_drv->c0_clk_en, C0_CLK_EN, C0_CLK_EN_WID);
	W_DSC_BIT(DSC_ASIC_CTRL1, dsc_drv->slices_in_core, SLICES_IN_CORE, SLICES_IN_CORE_WID);

	W_DSC_BIT(DSC_ASIC_CTRL1, dsc_drv->slice_group_number,
			SLICES_GROUP_NUMBER, SLICES_GROUP_NUMBER_WID);
	W_DSC_BIT(DSC_ASIC_CTRL2, dsc_drv->partial_group_pix_num,
			PARTIAL_GROUP_PIX_NUM, PARTIAL_GROUP_PIX_NUM_WID);
	W_DSC_BIT(DSC_ASIC_CTRL3, dsc_drv->chunk_6byte_num_m1,
			CHUNK_6BYTE_NUM_M1, CHUNK_6BYTE_NUM_M1_WID);
	W_DSC_BIT(DSC_ASIC_CTRLB, dsc_drv->last_slice_active_m1,
			LAST_SLICE_ACTIVE_M1, LAST_SLICE_ACTIVE_M1_WID);

	/* config timing register */
	dsc_config_timing_register(dsc_drv);

	W_DSC_BIT(DSC_ASIC_CTRL9, dsc_drv->pix_in_swap0,
			PIX_IN_SWAP0, PIX_IN_SWAP0_WID);
	W_DSC_BIT(DSC_ASIC_CTRLA, dsc_drv->pix_in_swap1,
			PIX_IN_SWAP1, PIX_IN_SWAP1_WID);
	W_DSC_BIT(DSC_ASIC_CTRL12, dsc_drv->out_swap,
			OUT_SWAP, OUT_SWAP_WID);
	W_DSC_BIT(DSC_ASIC_CTRLC, dsc_drv->gclk_ctrl,
			GCLK_CTRL, GCLK_CTRL_WID);

	/* config slice overflow threshold value */
	W_DSC_BIT(DSC_ASIC_CTRLD, dsc_drv->c0s1_cb_ovfl_th,
			C0S1_CB_OVER_FLOW_TH, C0S1_CB_OVER_FLOW_TH_WID);
	W_DSC_BIT(DSC_ASIC_CTRLD, dsc_drv->c0s0_cb_ovfl_th,
			C0S0_CB_OVER_FLOW_TH, C0S0_CB_OVER_FLOW_TH_WID);
	W_DSC_BIT(DSC_ASIC_CTRLE, dsc_drv->c1s1_cb_ovfl_th,
			C1S1_CB_OVER_FLOW_TH, C1S1_CB_OVER_FLOW_TH_WID);
	W_DSC_BIT(DSC_ASIC_CTRLE, dsc_drv->c1s0_cb_ovfl_th,
			C1S0_CB_OVER_FLOW_TH, C1S0_CB_OVER_FLOW_TH_WID);
	W_DSC_BIT(DSC_ASIC_CTRLF, dsc_drv->c2s1_cb_ovfl_th,
			C2S1_CB_OVER_FLOW_TH, C2S1_CB_OVER_FLOW_TH_WID);
	W_DSC_BIT(DSC_ASIC_CTRLF, dsc_drv->c2s0_cb_ovfl_th,
			C2S0_CB_OVER_FLOW_TH, C2S0_CB_OVER_FLOW_TH_WID);
	W_DSC_BIT(DSC_ASIC_CTRL10, dsc_drv->c3s1_cb_ovfl_th,
			C3S1_CB_OVER_FLOW_TH, C3S1_CB_OVER_FLOW_TH_WID);
	W_DSC_BIT(DSC_ASIC_CTRL10, dsc_drv->c3s0_cb_ovfl_th,
			C3S0_CB_OVER_FLOW_TH, C3S0_CB_OVER_FLOW_TH_WID);

	/* config odd mode */
	if (dsc_drv->hc_active_odd_mode) {
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->hs_odd_no_tail,
			HS_ODD_NO_TAIL, HS_ODD_NO_TAIL_WID);
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->hs_odd_no_head,
			HS_ODD_NO_HEAD, HS_ODD_NO_HEAD_WID);
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->hs_even_no_tail,
			HS_EVEN_NO_TAIL, HS_EVEN_NO_TAIL_WID);
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->hs_even_no_head,
			HS_EVEN_NO_HEAD, HS_EVEN_NO_HEAD_WID);
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->vs_odd_no_tail,
			VS_ODD_NO_TAIL, VS_ODD_NO_TAIL_WID);
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->vs_odd_no_head,
			VS_ODD_NO_HEAD, VS_ODD_NO_HEAD_WID);
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->vs_even_no_tail,
			VS_EVEN_NO_TAIL, VS_EVEN_NO_TAIL_WID);
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->vs_even_no_head,
			VS_EVEN_NO_HEAD, VS_EVEN_NO_HEAD_WID);
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->hc_htotal_offs_oddline,
			HC_HTOTAL_OFFS_ODDLINE, HC_HTOTAL_OFFS_ODDLINE_WID);
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->hc_htotal_offs_evenline,
			HC_HTOTAL_OFFS_EVENLINE, HC_HTOTAL_OFFS_EVENLINE_WID);
		W_DSC_BIT(DSC_ASIC_CTRL19, dsc_drv->hc_active_odd_mode,
			HC_ACTIVE_ODD_MODE, HC_ACTIVE_ODD_MODE_WID);
	}
}
