/*
 * drivers/amlogic/drm/vpu-hw/meson_osd_scaler.c
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

#include "meson_vpu_pipeline.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"
#include "meson_osd_scaler.h"

static struct osd_scaler_reg_s osd_scaler_reg[HW_OSD_SCALER_NUM] = {
	{
		VPP_OSD_SCALE_COEF_IDX,
		VPP_OSD_SCALE_COEF,
		VPP_OSD_VSC_PHASE_STEP,
		VPP_OSD_VSC_INI_PHASE,
		VPP_OSD_VSC_CTRL0,
		VPP_OSD_HSC_PHASE_STEP,
		VPP_OSD_HSC_INI_PHASE,
		VPP_OSD_HSC_CTRL0,
		VPP_OSD_HSC_INI_PAT_CTRL,
		VPP_OSD_SC_DUMMY_DATA,
		VPP_OSD_SC_CTRL0,
		VPP_OSD_SCI_WH_M1,
		VPP_OSD_SCO_H_START_END,
		VPP_OSD_SCO_V_START_END,
	},
	{
		OSD2_SCALE_COEF_IDX,
		OSD2_SCALE_COEF,
		OSD2_VSC_PHASE_STEP,
		OSD2_VSC_INI_PHASE,
		OSD2_VSC_CTRL0,
		OSD2_HSC_PHASE_STEP,
		OSD2_HSC_INI_PHASE,
		OSD2_HSC_CTRL0,
		OSD2_HSC_INI_PAT_CTRL,
		OSD2_SC_DUMMY_DATA,
		OSD2_SC_CTRL0,
		OSD2_SCI_WH_M1,
		OSD2_SCO_H_START_END,
		OSD2_SCO_V_START_END,
	},
	{
		OSD34_SCALE_COEF_IDX,
		OSD34_SCALE_COEF,
		OSD34_VSC_PHASE_STEP,
		OSD34_VSC_INI_PHASE,
		OSD34_VSC_CTRL0,
		OSD34_HSC_PHASE_STEP,
		OSD34_HSC_INI_PHASE,
		OSD34_HSC_CTRL0,
		OSD34_HSC_INI_PAT_CTRL,
		OSD34_SC_DUMMY_DATA,
		OSD34_SC_CTRL0,
		OSD34_SCI_WH_M1,
		OSD34_SCO_H_START_END,
		OSD34_SCO_V_START_END,
	}
};

static unsigned int __osd_filter_coefs_bicubic[] = { /* bicubic	coef0 */
	0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300, 0xfd7e0500, 0xfc7e0600,
	0xfb7d0800, 0xfb7c0900, 0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe, 0xf76f1dfd, 0xf76d1ffd,
	0xf76b21fd, 0xf76824fd, 0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa, 0xf8523cfa, 0xf8503ff9,
	0xf84d42f9, 0xf84a45f9, 0xf84848f8
};

static unsigned int __osd_filter_coefs_2point_binilear[] = {
	/* 2 point bilinear, bank_length == 2	coef2 */
	0x80000000, 0x7e020000, 0x7c040000, 0x7a060000, 0x78080000, 0x760a0000,
	0x740c0000, 0x720e0000, 0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
	0x68180000, 0x661a0000, 0x641c0000, 0x621e0000, 0x60200000, 0x5e220000,
	0x5c240000, 0x5a260000, 0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
	0x50300000, 0x4e320000, 0x4c340000, 0x4a360000, 0x48380000, 0x463a0000,
	0x443c0000, 0x423e0000, 0x40400000
};

static unsigned int __osd_filter_coefs_4point_triangle[] = {
	0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
	0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
	0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
	0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
	0x18382808, 0x18382808, 0x17372909, 0x17372909,
	0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
	0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
	0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
	0x10303010
};

static unsigned int *osd_scaler_filter_table[] = {
	__osd_filter_coefs_bicubic,
	__osd_filter_coefs_2point_binilear,
	__osd_filter_coefs_4point_triangle
};

/*********vsc config begin**********/
/*vsc phase_step=(v_in << 20)/v_out */
void osd_vsc_phase_step_set(struct osd_scaler_reg_s *reg, u32 phase_step)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_phase_step,
		phase_step, 0, 28);
}
/*vsc init phase*/
void osd_vsc_init_phase_set(struct osd_scaler_reg_s *reg,
	u32 bottom_init_phase, u32 top_init_phase)
{
	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd_vsc_ini_phase,
		(bottom_init_phase << 16) |
		(top_init_phase << 0));
}
/*vsc control*/
/*vsc enable last line repeate*/
void osd_vsc_repate_last_line_enable_set(
	struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, flag, 25, 1);
}
/*vsc enable*/
void osd_vsc_enable_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, flag, 24, 1);
}
/*vsc input Interlaced or Progressive:0->P;1->I*/
void osd_vsc_output_format_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, flag, 23, 1);
}
/*
 *vsc double line mode
 *bit0:change line buffer becomes 2 lines
 *bit1:double input width and half input height
 */
void osd_vsc_double_line_mode_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, data, 21, 2);
}
/*vsc phase always on*/
void osd_vsc_phase_always_on_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, flag, 20, 1);
}
/*vsc nearest en*/
void osd_vsc_nearest_en_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, flag, 19, 1);
}
/*vsc repeate bottom field line0 num*/
void osd_vsc_bot_rpt_l0_num_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, data, 16, 2);
}
/*vsc bottom field init receive num??*/
void osd_vsc_bot_ini_rcv_num_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, data, 11, 4);
}
/*vsc repeate top field line0 num*/
void osd_vsc_top_rpt_l0_num_set(struct osd_scaler_reg_s *reg, u32 flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, flag, 8, 2);
}
/*vsc top field init receive num??*/
void osd_vsc_top_ini_rcv_num_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, data, 3, 4);
}
/*vsc bank length??*/
void osd_vsc_bank_length_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_vsc_ctrl0, data, 0, 2);
}
/*********vsc config end**********/

/*********hsc config begin**********/
/*hsc phase_step=(v_in << 20)/v_out */
void osd_hsc_phase_step_set(struct osd_scaler_reg_s *reg, u32 phase_step)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_hsc_phase_step,
		phase_step, 0, 28);
}
/*vsc init phase*/
void osd_hsc_init_phase_set(struct osd_scaler_reg_s *reg,
	u32 init_phase0, u32 init_phase1)
{
	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd_hsc_ini_phase,
		(init_phase1 << 16) | (init_phase0 << 0));
}
/*hsc control*/
/*hsc enable*/
void osd_hsc_enable_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_hsc_ctrl0, flag, 22, 1);
}
/* hsc double pixel mode */
void osd_hsc_double_line_mode_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_hsc_ctrl0, flag, 21, 1);
}
/*hsc phase always on*/
void osd_hsc_phase_always_on_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_hsc_ctrl0, flag, 20, 1);
}
/*hsc nearest en*/
void osd_hsc_nearest_en_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_hsc_ctrl0, flag, 19, 1);
}
/*hsc repeate pixel0 num1??*/
void osd_hsc_rpt_p0_num1_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_hsc_ctrl0, data, 16, 2);
}
/*hsc init receive num1*/
void osd_vsc_ini_rcv_num1_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_hsc_ctrl0, data, 11, 4);
}
/*hsc repeate pixel0 num0*/
void osd_hsc_rpt_p0_num0_set(struct osd_scaler_reg_s *reg, u32 flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_hsc_ctrl0, flag, 8, 2);
}
/*hsc init receive num0*/
void osd_hsc_ini_rcv_num0_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_hsc_ctrl0, data, 3, 4);
}
/*hsc bank length*/
void osd_hsc_bank_length_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_hsc_ctrl0, data, 0, 2);
}
/*
 *hsc init pattern
 *[15:8]pattern
 *[6:4]pattern start
 *[2:0]pattern end
 */
void osd_hsc_ini_pat_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd_hsc_ini_pat_ctrl, data);
}
/*********hsc config end**********/

/*********sc top ctrl start**********/
/*
 *dummy data:
 *[31:24]componet0
 *[23:16]componet1
 *[15:8]componet2
 *[7:0]alpha
 */
void osd_sc_dummy_data_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd_sc_dummy_data, data);
}
/*sc gate clock*/
void osd_sc_gclk_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_sc_ctrl0, data, 16, 12);
}
/*
 *sc input data alpha mode
 *0:(alpha>=128)?alpha-1:alpha
 *1:(alpha>=1)?alpha-1:alpha
 */
void osd_sc_din_alpha_mode_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_sc_ctrl0, flag, 13, 1);
}
/*
 *sc output data alpha mode
 *0:(alpha>=128)?alpha+1:alpha
 *1:(alpha>=1)?alpha+1:alpha
 */
void osd_sc_dout_alpha_mode_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_sc_ctrl0, flag, 12, 1);
}
/*sc alpha*/
void osd_sc_alpha_set(struct osd_scaler_reg_s *reg, u32 data)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_sc_ctrl0, data, 4, 8);
}
/*sc path en*/
void osd_sc_path_en_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_sc_ctrl0, flag, 3, 1);
}
/*sc en*/
void osd_sc_en_set(struct osd_scaler_reg_s *reg, bool flag)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_sc_ctrl0, flag, 2, 1);
}
/*sc input width minus 1*/
void osd_sc_in_w_set(struct osd_scaler_reg_s *reg, u32 size)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_sci_wh_m1, (size - 1), 16, 13);
}
/*sc input height minus 1*/
void osd_sc_in_h_set(struct osd_scaler_reg_s *reg, u32 size)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->vpp_osd_sci_wh_m1, (size - 1), 0, 13);
}
/*sc output horizontal size = end - start + 1*/
void osd_sc_out_horz_set(struct osd_scaler_reg_s *reg, u32 start, u32 end)
{
	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd_sco_h_start_end,
		(start & 0xfff << 16) | (end & 0xfff));
}

/*sc output vertical size = end - start + 1*/
void osd_sc_out_vert_set(struct osd_scaler_reg_s *reg, u32 start, u32 end)
{
	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd_sco_v_start_end,
		(start & 0xfff << 16) | (end & 0xfff));
}

/*
 *sc h/v coef
 *1:config horizontal coef
 *0:config vertical coef
 */
void osd_sc_coef_set(struct osd_scaler_reg_s *reg, bool flag, u32 *coef)
{
	u8 i;

	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd_scale_coef_idx,
		(0 << 15) |/*index increment. 1bits*/
		(0 << 14) |/*read coef enable, 1bits*/
		(0 << 9) |/*coef bit mode 8 or 9. 1bits*/
		(flag << 8) |
		(0 << 0)/*coef index 7bits*/);
	for (i = 0; i < 33; i++)
		VSYNCOSD_WR_MPEG_REG(reg->vpp_osd_scale_coef, coef[i]);
}
/*********sc top ctrl end************/
static void f2v_get_vertical_phase(
	u32 zoom_ratio, enum f2v_vphase_type_e type,
	u8 bank_length, struct f2v_vphase_s *vphase)
{
	u8 f2v_420_in_pos_luma[F2V_TYPE_MAX] = {
		0, 2, 0, 2, 0, 0, 0, 2, 0};
	u8 f2v_420_out_pos[F2V_TYPE_MAX] = {
		0, 2, 2, 0, 0, 2, 0, 0, 0};
	s32 offset_in, offset_out;

	/* luma */
	offset_in = f2v_420_in_pos_luma[type]
		<< OSD_PHASE_BITS;
	offset_out = (f2v_420_out_pos[type] * zoom_ratio)
		>> (OSD_ZOOM_HEIGHT_BITS - OSD_PHASE_BITS);

	vphase->rcv_num = bank_length;
	if (bank_length == 4 || bank_length == 3)
		vphase->rpt_num = 1;
	else
		vphase->rpt_num = 0;

	if (offset_in > offset_out) {
		vphase->rpt_num = vphase->rpt_num + 1;
		vphase->phase =
			((4 << OSD_PHASE_BITS) + offset_out - offset_in)
			>> 2;
	} else {
		while ((offset_in + (4 << OSD_PHASE_BITS))
			<= offset_out) {
			if (vphase->rpt_num == 1)
				vphase->rpt_num = 0;
			else
				vphase->rcv_num++;
			offset_in += 4 << OSD_PHASE_BITS;
		}
		vphase->phase = (offset_out - offset_in) >> 2;
	}
}
void osd_scaler_config(struct osd_scaler_reg_s *reg,
	struct meson_vpu_scaler_state *scaler_state,
	struct meson_vpu_block *vblk)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);
	u32 phase_step_v, phase_step_h, vsc_top_init_rec_num, vsc_bank_length;
	u32 hsc_init_rec_num, hsc_init_rpt_p0_num, hsc_bank_length;
	u32 vsc_bot_init_rec_num, vsc_top_rpt_l0_num, vsc_bot_rpt_l0_num;
	u32 vsc_top_init_phase, vsc_bot_init_phase;
	struct f2v_vphase_s vphase;
	u8 version = vblk->pipeline->osd_version;
	u32 linebuffer = scaler->linebuffer;
	u32 bank_length = scaler->bank_length;
	u32 width_in = scaler_state->input_width;
	u32 height_in = scaler_state->input_height;
	u32 width_out = scaler_state->output_width;
	u32 height_out = scaler_state->output_height;
	u32 scan_mode_out = scaler_state->scan_mode_out;
	u32 vsc_double_line_mode;
	u32 *coef_h, *coef_v;
	bool scaler_enable;

	if (width_in == width_out && height_in == height_out &&
		version > OSD_V2)
		scaler_enable = false;
	else
		scaler_enable = true;

	if (width_in > linebuffer) {
		vsc_bank_length = bank_length >> 1;
		vsc_double_line_mode = 1;
	} else {
		vsc_bank_length = bank_length;
		vsc_double_line_mode = 0;
	}
	hsc_init_rec_num = bank_length;
	hsc_bank_length = bank_length;
	hsc_init_rpt_p0_num = bank_length / 2 - 1;

	if (version <= OSD_V2)
		phase_step_v = ((height_in - 1) << OSD_ZOOM_HEIGHT_BITS) /
				height_out;
	else
		phase_step_v =
			(height_in << OSD_ZOOM_HEIGHT_BITS) / height_out;
	if (scan_mode_out) {
		f2v_get_vertical_phase(phase_step_v, F2V_P2IT,
			vsc_bank_length, &vphase);
		vsc_top_init_rec_num = vphase.rcv_num;
		vsc_top_rpt_l0_num = vphase.rpt_num;
		vsc_top_init_phase = vphase.phase;
		f2v_get_vertical_phase(phase_step_v, F2V_P2IB,
			vsc_bank_length, &vphase);
		vsc_bot_init_rec_num = vphase.rcv_num;
		vsc_bot_rpt_l0_num = vphase.rpt_num;
		vsc_bot_init_phase = vphase.phase;
	} else {
		f2v_get_vertical_phase(
			phase_step_v, F2V_P2P,
			vsc_bank_length, &vphase);
		vsc_top_init_rec_num = vphase.rcv_num;
		vsc_top_rpt_l0_num = vphase.rpt_num;
		vsc_top_init_phase = vphase.phase;
		vsc_bot_init_rec_num = 0;
		vsc_bot_rpt_l0_num = 0;
		vsc_bot_init_phase = 0;
	}
	if (version <= OSD_V2)
		vsc_top_init_rec_num++;
	if (version <= OSD_V2 && scan_mode_out)
		vsc_bot_init_rec_num++;
	phase_step_v <<= (OSD_ZOOM_TOTAL_BITS - OSD_ZOOM_HEIGHT_BITS);
	phase_step_h = (width_in << OSD_ZOOM_WIDTH_BITS) / width_out;
	phase_step_h <<= (OSD_ZOOM_TOTAL_BITS - OSD_ZOOM_WIDTH_BITS);
	/*check coef*/
	if (scan_mode_out && width_out <= 720) {
		coef_h = osd_scaler_filter_table[COEFS_4POINT_TRIANGLE];
		coef_v = osd_scaler_filter_table[COEFS_4POINT_TRIANGLE];
	} else if (vsc_double_line_mode == 1) {
		coef_h = osd_scaler_filter_table[COEFS_BICUBIC];
		coef_v = osd_scaler_filter_table[COEFS_2POINT_BINILEAR];
	} else {
		coef_h = osd_scaler_filter_table[COEFS_BICUBIC];
		coef_v = osd_scaler_filter_table[COEFS_BICUBIC];
	}

	/*input size config*/
	osd_sc_in_h_set(reg, height_in);
	osd_sc_in_w_set(reg, width_in);

	/*output size config*/
	osd_sc_out_horz_set(reg, 0, width_out - 1);
	osd_sc_out_vert_set(reg, 0, height_out - 1);

	/*phase step config*/
	osd_vsc_phase_step_set(reg, phase_step_v);
	osd_hsc_phase_step_set(reg, phase_step_h);

	/*dummy data config*/
	osd_sc_dummy_data_set(reg, 0x80808080);

	/*h/v coef config*/
	osd_sc_coef_set(reg, OSD_SCALER_COEFF_H, coef_h);
	osd_sc_coef_set(reg, OSD_SCALER_COEFF_V, coef_v);

	/*init recv line num*/
	osd_vsc_top_ini_rcv_num_set(reg, vsc_top_init_rec_num);
	osd_vsc_bot_ini_rcv_num_set(reg, vsc_bot_init_rec_num);
	osd_hsc_ini_rcv_num0_set(reg, hsc_init_rec_num);
	osd_vsc_double_line_mode_set(reg, vsc_double_line_mode);

	/*repeate line0 num*/
	osd_vsc_top_rpt_l0_num_set(reg, vsc_top_rpt_l0_num);
	osd_vsc_bot_rpt_l0_num_set(reg, vsc_bot_rpt_l0_num);
	osd_hsc_rpt_p0_num0_set(reg, hsc_init_rpt_p0_num);

	/*init phase*/
	osd_vsc_init_phase_set(reg, vsc_bot_init_phase, vsc_top_init_phase);
	osd_hsc_init_phase_set(reg, 0, 0);

	/*vsc bank length*/
	osd_vsc_bank_length_set(reg, vsc_bank_length);
	osd_hsc_bank_length_set(reg, hsc_bank_length);

	/*out scan mode*/
	osd_vsc_output_format_set(reg, scan_mode_out ? 1:0);

	/*repeate last line*/
	if (version >= OSD_V2)
		osd_vsc_repate_last_line_enable_set(reg, 1);

	/*eanble sc*/
	osd_vsc_enable_set(reg, scaler_enable);
	osd_hsc_enable_set(reg, scaler_enable);
	osd_sc_en_set(reg, scaler_enable);
	osd_sc_path_en_set(reg, scaler_enable);
}

static void scaler_size_check(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
	struct meson_vpu_pipeline *pipeline = vblk->pipeline;
	struct meson_vpu_pipeline_state *pipeline_state;
	struct meson_vpu_scaler_state *scaler_state = to_scaler_state(state);

	pipeline_state = priv_to_pipeline_state(pipeline->obj.state);
	if (!pipeline_state) {
		DRM_DEBUG("pipeline_state is NULL!!\n");
		return;
	}
	if (scaler_state->input_width !=
		pipeline_state->scaler_param[vblk->index].input_width) {
		scaler_state->input_width =
			pipeline_state->scaler_param[vblk->index].input_width;
		scaler_state->state_changed |= SCALER_INPUT_WIDTH_CHANGED;
	}
	if (scaler_state->input_height !=
		pipeline_state->scaler_param[vblk->index].input_height) {
		scaler_state->input_height =
			pipeline_state->scaler_param[vblk->index].input_height;
		scaler_state->state_changed |= SCALER_INPUT_HEIGHT_CHANGED;
	}
	if (scaler_state->output_width !=
		pipeline_state->scaler_param[vblk->index].output_width) {
		scaler_state->output_width =
			pipeline_state->scaler_param[vblk->index].output_width;
		scaler_state->state_changed |= SCALER_OUTPUT_WIDTH_CHANGED;
	}
	if (scaler_state->output_height !=
		pipeline_state->scaler_param[vblk->index].output_height) {
		scaler_state->output_height =
			pipeline_state->scaler_param[vblk->index].output_height;
		scaler_state->state_changed |= SCALER_OUTPUT_HEIGHT_CHANGED;
	}
}

void scan_mode_check(struct meson_vpu_pipeline *pipeline,
	struct meson_vpu_scaler_state *scaler_state)
{
	u32 scan_mode_out = pipeline->mode.flags & DRM_MODE_FLAG_INTERLACE;

	if (scaler_state->scan_mode_out != scan_mode_out) {
		scaler_state->scan_mode_out = scan_mode_out;
		scaler_state->state_changed |=
			SCALER_OUTPUT_SCAN_MODE_CHANGED;
	}
}

static int scaler_check_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);

	if (state->checked)
		return 0;

	state->checked = true;
	DRM_DEBUG("%s check_state called.\n", scaler->base.name);

	return 0;
}

static void scaler_set_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);
	struct meson_vpu_scaler_state *scaler_state = to_scaler_state(state);
	struct osd_scaler_reg_s *reg = scaler->reg;

	if (!scaler_state) {
		DRM_DEBUG("scaler or scaler_state is NULL!!\n");
		return;
	}
	scaler_size_check(vblk, state);
	scan_mode_check(vblk->pipeline, scaler_state);
	DRM_DEBUG("scaler_state=0x%x\n", scaler_state->state_changed);
	if (scaler_state->state_changed) {
		osd_scaler_config(reg, scaler_state, vblk);
		scaler_state->state_changed = 0;
	}
	DRM_DEBUG("scaler%d input/output w/h[%d, %d, %d, %d].\n",
		scaler->base.index,
		scaler_state->input_width, scaler_state->input_height,
		scaler_state->output_width, scaler_state->output_height);
}

static void scaler_hw_enable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);
	struct osd_scaler_reg_s *reg = scaler->reg;

	osd_sc_en_set(reg, 1);
	osd_sc_path_en_set(reg, 1);
	DRM_DEBUG("%s enable done.\n", scaler->base.name);
}

static void scaler_hw_disable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);
	struct osd_scaler_reg_s *reg = scaler->reg;

	/*disable sc*/
	osd_sc_en_set(reg, 0);
	osd_sc_path_en_set(reg, 0);
	DRM_DEBUG("%s disable called.\n", scaler->base.name);
}

static void scaler_dump_register(struct meson_vpu_block *vblk,
					struct seq_file *seq)
{
	int osd_index;
	u32 value;
	char buff[8];
	struct meson_vpu_scaler *scaler;
	struct osd_scaler_reg_s *reg;

	osd_index = vblk->index;
	scaler = to_scaler_block(vblk);
	reg = scaler->reg;

	snprintf(buff, 8, "OSD%d", osd_index + 1);

	value = meson_drm_read_reg(reg->vpp_osd_vsc_phase_step);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "VSC_PHASE_STEP:", value);

	value = meson_drm_read_reg(reg->vpp_osd_vsc_ini_phase);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "VSC_INIT_PHASE:", value);

	value = meson_drm_read_reg(reg->vpp_osd_vsc_ctrl0);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "VSC_CTRL0:", value);

	value = meson_drm_read_reg(reg->vpp_osd_hsc_phase_step);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "HSC_PHASE_STEP:", value);

	value = meson_drm_read_reg(reg->vpp_osd_hsc_ini_phase);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "HSC_INIT_PHASE:", value);

	value = meson_drm_read_reg(reg->vpp_osd_hsc_ctrl0);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "HSC_CTRL0:", value);

	value = meson_drm_read_reg(reg->vpp_osd_sc_dummy_data);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "SC_DUMMY_DATA:", value);

	value = meson_drm_read_reg(reg->vpp_osd_sc_ctrl0);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "SC_CTRL0:", value);

	value = meson_drm_read_reg(reg->vpp_osd_sci_wh_m1);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "SCI_WH_M1:", value);

	value = meson_drm_read_reg(reg->vpp_osd_sco_h_start_end);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "SCO_H_START_END:", value);

	value = meson_drm_read_reg(reg->vpp_osd_sco_v_start_end);
	seq_printf(seq, "%s_%-35s\t0x%08X\n", buff, "SCO_V_START_END:", value);
}

static void scaler_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_scaler *scaler = to_scaler_block(vblk);

	scaler->reg = &osd_scaler_reg[vblk->index];
	scaler->linebuffer = OSD_SCALE_LINEBUFFER;
	scaler->bank_length = OSD_SCALE_BANK_LENGTH;
	DRM_DEBUG("%s hw_init called.\n", scaler->base.name);
}

struct meson_vpu_block_ops scaler_ops = {
	.check_state = scaler_check_state,
	.update_state = scaler_set_state,
	.enable = scaler_hw_enable,
	.disable = scaler_hw_disable,
	.dump_register = scaler_dump_register,
	.init = scaler_hw_init,
};
