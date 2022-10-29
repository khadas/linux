/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DSC_DRV_H__
#define __DSC_DRV_H__

#include <linux/hdmi.h>
#include <linux/cdev.h>

#define DSC_TIMING_MAX			4
#define DSC_TIMING_VALUE_MAX		13

#define DSC_NORMAL_DEBUG		BIT(0)
#define DSC_PPS_PARA_EN			BIT(1)
#define DSC_PPS_QP_VALUE		BIT(2)
#define DSC_PPS_MANUAL_SET		BIT(3)

struct dsc_cdev_s {
	dev_t           devno;
	struct class    *class;
};

enum dsc_chip_e {
	DSC_CHIP_S5 = 0,
	DSC_CHIP_MAX,
};

enum dsc_encode_mode {
	DSC_YUV444_3840X2160_60HZ,
	DSC_YUV422_3840X2160_120HZ,
	DSC_YUV444_7680X4320_60HZ,
	DSC_YUV420_7680X4320_60HZ,
	DSC_ENCODE_MAX,
};

struct dsc_data_s {
	unsigned int chip_type;
	const char *chip_name;
};

struct dsc_timing_gen_ctrl {
	unsigned int tmg_havon_begin;//avon=active
	unsigned int tmg_vavon_bline;//
	unsigned int tmg_vavon_eline;
	unsigned int tmg_hso_begin;//hso=hsync
	unsigned int tmg_hso_end;
	unsigned int tmg_vso_begin;
	unsigned int tmg_vso_end;
	unsigned int tmg_vso_bline;
	unsigned int tmg_vso_eline;
};

struct encp_timing_gen_ctrl {
	//bist timing set
	unsigned int encp_dvi_hso_begin;
	unsigned int encp_dvi_hso_end;
	unsigned int encp_vso_bline;
	unsigned int encp_vso_eline;
	unsigned int encp_vso_begin;
	unsigned int encp_vso_end;
	unsigned int encp_de_h_begin;
	unsigned int encp_de_h_end;
	unsigned int encp_de_v_begin;
	unsigned int encp_de_v_end;//de=active
	//video timing set
	unsigned int encp_hso_begin;//h_de_begin+htotal-hsync_len-hback_len
	unsigned int encp_hso_end;//hso_end=hso_begin+hsync_long
	unsigned int encp_video_vso_bline;
	unsigned int encp_video_vso_eline;
	unsigned int encp_video_vso_begin;
	unsigned int encp_video_vso_end;
	unsigned int encp_havon_begin;
	unsigned int encp_havon_end;//havon_long=pic_width/4
	unsigned int encp_vavon_bline;
	unsigned int encp_vavon_eline;
	unsigned int h_total;
	unsigned int v_total;
};

/*******for debug **********/
struct dsc_debug_s {
	int use_dsc_model_value;
	unsigned int manual_set_select;
};

struct aml_dsc_drv_s {
	struct cdev cdev;
	struct dsc_data_s *data;
	struct device *dev;
	void __iomem *dsc_reg_base;

	struct dsc_debug_s dsc_debug;
	struct dsc_pps_data_s pps_data;
	int bits_per_pixel_int;
	int bits_per_pixel_remain;
	int bits_per_pixel_multiple;
	int bits_per_pixel_really_value;
	unsigned int dsc_print_en;
	bool full_ich_err_precision;
	unsigned int rcb_bits;
	unsigned int flatness_det_thresh;
	unsigned int mux_word_size;
	unsigned int very_flat_qp;
	unsigned int somewhat_flat_qp_delta;
	unsigned int somewhat_flat_qp_thresh;
	unsigned int slice_num_m1;//pic_width/slice_width;
	unsigned int clr_ssm_fifo_sts;//1 to 0 clear ro_cx_sx_cb_under_flow
	unsigned int dsc_enc_en;
	unsigned int dsc_enc_frm_latch_en;// need to check ucode
	unsigned int pix_per_clk;// input 4*36 config 2^2
	unsigned int inbuf_rd_dly0;// const value check ucode
	unsigned int inbuf_rd_dly1;// const value check ucode
	bool c3_clk_en;
	bool c2_clk_en;
	bool c1_clk_en;
	bool c0_clk_en;
	bool slices_in_core;//if slice_num_m1==8 enable
	unsigned int slice_group_number;//444:slice_width/3*slice_height
					//422/420:slice_width/2/3*slice_height
	unsigned int partial_group_pix_num;//register only [14:15] const value check ucode
	unsigned int chunk_6byte_num_m1;//chunk_size/6-1 (need up value)
	struct dsc_timing_gen_ctrl tmg_ctrl;
	struct encp_timing_gen_ctrl encp_timing_ctrl;
	unsigned int pix_in_swap0;//const value check ucode
	unsigned int pix_in_swap1;//const value check ucode
	unsigned int last_slice_active_m1;//slice_width - 1
	unsigned int gclk_ctrl;//0x0a0a0a0a is enable clk
	unsigned int c0s1_cb_ovfl_th;//(8/h_slice_num)*350
	unsigned int c0s0_cb_ovfl_th;
	unsigned int c1s1_cb_ovfl_th;
	unsigned int c1s0_cb_ovfl_th;
	unsigned int c2s1_cb_ovfl_th;
	unsigned int c2s0_cb_ovfl_th;
	unsigned int c3s1_cb_ovfl_th;
	unsigned int c3s0_cb_ovfl_th;
	unsigned int out_swap;
	unsigned int intr_maskn;
	unsigned int hc_vtotal_m1;//Vtotal - 1
	unsigned int hc_htotal_m1;//(hc_active+hc_blank)/2-1
	bool hs_odd_no_tail;
	bool hs_odd_no_head;
	bool hs_even_no_tail;
	bool hs_even_no_head;
	bool vs_odd_no_tail;
	bool vs_odd_no_head;
	bool vs_even_no_tail;
	bool vs_even_no_head;
	bool ro_aff_under_flow;
	bool hc_active_odd_mode;
	unsigned int hc_htotal_offs_oddline;
	unsigned int hc_htotal_offs_evenline;
	unsigned int fps;
	enum hdmi_colorspace color_format;
	enum dsc_encode_mode dsc_mode;
};

//===========================================================================
// For ENCL DSC
//===========================================================================
#define DSC_ENC_INDEX                              0

struct aml_dsc_drv_s *dsc_drv_get(void);

#endif
