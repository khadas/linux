/*
 * drivers/amlogic/drm/vpu-hw/meson_osd_scaler.h
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

#ifndef _MESON_OSD_SCALER_H_
#define _MESON_OSD_SCALER_H_

#define HW_OSD_SCALER_NUM 3

/*vpp osd scaler*/
#define VPP_OSD_VSC_PHASE_STEP 0x1dc0
#define VPP_OSD_VSC_INI_PHASE 0x1dc1
#define VPP_OSD_VSC_CTRL0 0x1dc2
#define VPP_OSD_HSC_PHASE_STEP 0x1dc3
#define VPP_OSD_HSC_INI_PHASE 0x1dc4
#define VPP_OSD_HSC_CTRL0 0x1dc5
#define VPP_OSD_HSC_INI_PAT_CTRL 0x1dc6
#define VPP_OSD_SC_DUMMY_DATA 0x1dc7
#define VPP_OSD_SC_CTRL0 0x1dc8
#define VPP_OSD_SCI_WH_M1 0x1dc9
#define VPP_OSD_SCO_H_START_END 0x1dca
#define VPP_OSD_SCO_V_START_END 0x1dcb
#define VPP_OSD_SCALE_COEF_IDX 0x1dcc
#define VPP_OSD_SCALE_COEF 0x1dcd

/* vpp osd2 scaler */
#define OSD2_VSC_PHASE_STEP 0x3d00
#define OSD2_VSC_INI_PHASE 0x3d01
#define OSD2_VSC_CTRL0 0x3d02
#define OSD2_HSC_PHASE_STEP 0x3d03
#define OSD2_HSC_INI_PHASE 0x3d04
#define OSD2_HSC_CTRL0 0x3d05
#define OSD2_HSC_INI_PAT_CTRL 0x3d06
#define OSD2_SC_DUMMY_DATA 0x3d07
#define OSD2_SC_CTRL0 0x3d08
#define OSD2_SCI_WH_M1 0x3d09
#define OSD2_SCO_H_START_END 0x3d0a
#define OSD2_SCO_V_START_END 0x3d0b
#define OSD2_SCALE_COEF_IDX 0x3d18
#define OSD2_SCALE_COEF 0x3d19

/* vpp osd34 scaler */
#define OSD34_VSC_PHASE_STEP 0x3d20
#define OSD34_VSC_INI_PHASE 0x3d21
#define OSD34_VSC_CTRL0 0x3d22
#define OSD34_HSC_PHASE_STEP 0x3d23
#define OSD34_HSC_INI_PHASE 0x3d24
#define OSD34_HSC_CTRL0 0x3d25
#define OSD34_HSC_INI_PAT_CTRL 0x3d26
#define OSD34_SC_DUMMY_DATA 0x3d27
#define OSD34_SC_CTRL0 0x3d28
#define OSD34_SCI_WH_M1 0x3d29
#define OSD34_SCO_H_START_END 0x3d2a
#define OSD34_SCO_V_START_END 0x3d2b
#define OSD34_SCALE_COEF_IDX 0x3d1e
#define OSD34_SCALE_COEF 0x3d1f


/*macro define for chip const*/
/*bank length is related to scale fifo:4 line 1920??*/
#define OSD_SCALE_BANK_LENGTH 4
#define OSD_SCALE_LINEBUFFER 1920

#define OSD_ZOOM_WIDTH_BITS 18
#define OSD_ZOOM_HEIGHT_BITS 20
#define OSD_ZOOM_TOTAL_BITS 24
#define OSD_PHASE_BITS 16

#define OSD_SCALER_COEFF_H 1
#define OSD_SCALER_COEFF_V 0

enum scaler_coef_e {
	COEFS_BICUBIC = 0,
	COEFS_2POINT_BINILEAR,
	COEFS_4POINT_TRIANGLE
};

enum f2v_vphase_type_e {
	F2V_IT2IT = 0,
	F2V_IB2IB,
	F2V_IT2IB,
	F2V_IB2IT,
	F2V_P2IT,
	F2V_P2IB,
	F2V_IT2P,
	F2V_IB2P,
	F2V_P2P,
	F2V_TYPE_MAX
};

struct f2v_vphase_s {
	u8 rcv_num;
	u8 rpt_num;
	u16 phase;
};

struct osd_scaler_reg_s {
	u32 vpp_osd_scale_coef_idx;
	u32 vpp_osd_scale_coef;
	u32 vpp_osd_vsc_phase_step;
	u32 vpp_osd_vsc_ini_phase;
	u32 vpp_osd_vsc_ctrl0;
	u32 vpp_osd_hsc_phase_step;
	u32 vpp_osd_hsc_ini_phase;
	u32 vpp_osd_hsc_ctrl0;
	u32 vpp_osd_hsc_ini_pat_ctrl;
	u32 vpp_osd_sc_dummy_data;
	u32 vpp_osd_sc_ctrl0;
	u32 vpp_osd_sci_wh_m1;
	u32 vpp_osd_sco_h_start_end;
	u32 vpp_osd_sco_v_start_end;
};

#endif
