/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_OSD_SCALER_H_
#define _MESON_OSD_SCALER_H_

#define HW_OSD_SCALER_NUM 4

/*vpp osd scaler*/
#define VPP_OSD_VSC_PHASE_STEP                     0x1dc0
#define VPP_OSD_VSC_INI_PHASE                      0x1dc1
#define VPP_OSD_VSC_CTRL0                          0x1dc2
#define VPP_OSD_HSC_PHASE_STEP                     0x1dc3
#define VPP_OSD_HSC_INI_PHASE                      0x1dc4
#define VPP_OSD_HSC_CTRL0                          0x1dc5
#define VPP_OSD_HSC_INI_PAT_CTRL                   0x1dc6
#define VPP_OSD_SC_DUMMY_DATA                      0x1dc7
#define VPP_OSD_SC_CTRL0                           0x1dc8
#define VPP_OSD_SCI_WH_M1                          0x1dc9
#define VPP_OSD_SCO_H_START_END                    0x1dca
#define VPP_OSD_SCO_V_START_END                    0x1dcb
#define VPP_OSD_SCALE_COEF_IDX                     0x1dcc
#define VPP_OSD_SCALE_COEF                         0x1dcd

/* vpp osd2 scaler */
#define OSD2_VSC_PHASE_STEP                        0x3d00
#define OSD2_VSC_INI_PHASE                         0x3d01
#define OSD2_VSC_CTRL0                             0x3d02
#define OSD2_HSC_PHASE_STEP                        0x3d03
#define OSD2_HSC_INI_PHASE                         0x3d04
#define OSD2_HSC_CTRL0                             0x3d05
#define OSD2_HSC_INI_PAT_CTRL                      0x3d06
#define OSD2_SC_DUMMY_DATA                         0x3d07
#define OSD2_SC_CTRL0                              0x3d08
#define OSD2_SCI_WH_M1                             0x3d09
#define OSD2_SCO_H_START_END                       0x3d0a
#define OSD2_SCO_V_START_END                       0x3d0b
#define OSD2_SCALE_COEF_IDX                        0x3d18
#define OSD2_SCALE_COEF                            0x3d19

/* vpp osd34 scaler */
#define OSD34_VSC_PHASE_STEP                       0x3d20
#define OSD34_VSC_INI_PHASE                        0x3d21
#define OSD34_VSC_CTRL0                            0x3d22
#define OSD34_HSC_PHASE_STEP                       0x3d23
#define OSD34_HSC_INI_PHASE                        0x3d24
#define OSD34_HSC_CTRL0                            0x3d25
#define OSD34_HSC_INI_PAT_CTRL                     0x3d26
#define OSD34_SC_DUMMY_DATA                        0x3d27
#define OSD34_SC_CTRL0                             0x3d28
#define OSD34_SCI_WH_M1                            0x3d29
#define OSD34_SCO_H_START_END                      0x3d2a
#define OSD34_SCO_V_START_END                      0x3d2b
#define OSD34_SCALE_COEF_IDX                       0x3d1e
#define OSD34_SCALE_COEF                           0x3d1f

/* for t7 osd scaler */
#define T7_VPP_OSD_VSC_PHASE_STEP                  0x5a00
#define T7_VPP_OSD_VSC_INI_PHASE                   0x5a01
#define T7_VPP_OSD_VSC_CTRL0                       0x5a02
#define T7_VPP_OSD_HSC_PHASE_STEP                  0x5a03
#define T7_VPP_OSD_HSC_INI_PHASE                   0x5a04
#define T7_VPP_OSD_HSC_CTRL0                       0x5a05
#define T7_VPP_OSD_HSC_INI_PAT_CTRL                0x5a06
#define T7_VPP_OSD_SC_DUMMY_DATA                   0x5a07
#define T7_VPP_OSD_SC_CTRL0                        0x5a08
#define T7_VPP_OSD_SCI_WH_M1                       0x5a09
#define T7_VPP_OSD_SCO_H_START_END                 0x5a0a
#define T7_VPP_OSD_SCO_V_START_END                 0x5a0b
#define T7_VPP_OSD_SCALE_COEF_IDX                  0x5a0c
#define T7_VPP_OSD_SCALE_COEF                      0x5a0d

#define T7_OSD2_VSC_PHASE_STEP                     0x5a40
#define T7_OSD2_VSC_INI_PHASE                      0x5a41
#define T7_OSD2_VSC_CTRL0                          0x5a42
#define T7_OSD2_HSC_PHASE_STEP                     0x5a43
#define T7_OSD2_HSC_INI_PHASE                      0x5a44
#define T7_OSD2_HSC_CTRL0                          0x5a45
#define T7_OSD2_HSC_INI_PAT_CTRL                   0x5a46
#define T7_OSD2_SC_DUMMY_DATA                      0x5a47
#define T7_OSD2_SC_CTRL0                           0x5a48
#define T7_OSD2_SCI_WH_M1                          0x5a49
#define T7_OSD2_SCO_H_START_END                    0x5a4a
#define T7_OSD2_SCO_V_START_END                    0x5a4b
#define T7_OSD2_SCALE_COEF_IDX                     0x5a4c
#define T7_OSD2_SCALE_COEF                         0x5a4d

#define T7_OSD34_VSC_PHASE_STEP                    0x5a80
#define T7_OSD34_VSC_INI_PHASE                     0x5a81
#define T7_OSD34_VSC_CTRL0                         0x5a82
#define T7_OSD34_HSC_PHASE_STEP                    0x5a83
#define T7_OSD34_HSC_INI_PHASE                     0x5a84
#define T7_OSD34_HSC_CTRL0                         0x5a85
#define T7_OSD34_HSC_INI_PAT_CTRL                  0x5a86
#define T7_OSD34_SC_DUMMY_DATA                     0x5a87
#define T7_OSD34_SC_CTRL0                          0x5a88
#define T7_OSD34_SCI_WH_M1                         0x5a89
#define T7_OSD34_SCO_H_START_END                   0x5a8a
#define T7_OSD34_SCO_V_START_END                   0x5a8b
#define T7_OSD34_SCALE_COEF_IDX                    0x5a8c
#define T7_OSD34_SCALE_COEF                        0x5a8d

#define T7_OSD4_VSC_PHASE_STEP                     0x5ac0
#define T7_OSD4_VSC_INI_PHASE                      0x5ac1
#define T7_OSD4_VSC_CTRL0                          0x5ac2
#define T7_OSD4_HSC_PHASE_STEP                     0x5ac3
#define T7_OSD4_HSC_INI_PHASE                      0x5ac4
#define T7_OSD4_HSC_CTRL0                          0x5ac5
#define T7_OSD4_HSC_INI_PAT_CTRL                   0x5ac6
#define T7_OSD4_SC_DUMMY_DATA                      0x5ac7
#define T7_OSD4_SC_CTRL0                           0x5ac8
#define T7_OSD4_SCI_WH_M1                          0x5ac9
#define T7_OSD4_SCO_H_START_END                    0x5aca
#define T7_OSD4_SCO_V_START_END                    0x5acb
#define T7_OSD4_SCALE_COEF_IDX                     0x5acc
#define T7_OSD4_SCALE_COEF                         0x5acd

#define OSD1_PROC_VSC_PHASE_STEP                   0x6410
#define OSD1_PROC_VSC_INI_PHASE                    0x6411
#define OSD1_PROC_VSC_CTRL0                        0x6412
#define OSD1_PROC_HSC_PHASE_STEP                   0x6413
#define OSD1_PROC_HSC_INI_PHASE                    0x6414
#define OSD1_PROC_HSC_CTRL0                        0x6415
#define OSD1_PROC_HSC_INI_PAT_CTRL                 0x6416
#define OSD1_PROC_SC_DUMMY_DATA                    0x6417
#define OSD1_PROC_SC_CTRL0                         0x6418
#define OSD1_PROC_SCI_WH_M1                        0x6419
#define OSD1_PROC_SCO_H_START_END                  0x641a
#define OSD1_PROC_SCO_V_START_END                  0x641b
#define OSD1_PROC_SCALE_COEF_IDX                   0x641c
#define OSD1_PROC_SCALE_COEF                       0x641d
#define OSD1_PROC_DB_FLT_CTRL                      0x6420
#define OSD1_PROC_DB_FLT_CTRL1                     0x6421
#define OSD1_PROC_DB_FLT_LUMA_THRD                 0x6422
#define OSD1_PROC_DB_FLT_CHRM_THRD                 0x6423
#define OSD1_PROC_DB_FLT_RANDLUT                   0x6424
#define OSD1_PROC_DB_FLT_PXI_THRD                  0x6425
#define OSD1_PROC_DB_FLT_SEED_Y                    0x6426
#define OSD1_PROC_DB_FLT_SEED_U                    0x6427
#define OSD1_PROC_DB_FLT_SEED_V                    0x6428
#define OSD1_PROC_DB_FLT_SEED3                     0x6429
#define OSD1_PROC_DB_FLT_SEED4                     0x642a
#define OSD1_PROC_DB_FLT_SEED5                     0x642b

#define OSD2_PROC_VSC_PHASE_STEP                   0x6810
#define OSD2_PROC_VSC_INI_PHASE                    0x6811
#define OSD2_PROC_VSC_CTRL0                        0x6812
#define OSD2_PROC_HSC_PHASE_STEP                   0x6813
#define OSD2_PROC_HSC_INI_PHASE                    0x6814
#define OSD2_PROC_HSC_CTRL0                        0x6815
#define OSD2_PROC_HSC_INI_PAT_CTRL                 0x6816
#define OSD2_PROC_SC_DUMMY_DATA                    0x6817
#define OSD2_PROC_SC_CTRL0                         0x6818
#define OSD2_PROC_SCI_WH_M1                        0x6819
#define OSD2_PROC_SCO_H_START_END                  0x681a
#define OSD2_PROC_SCO_V_START_END                  0x681b
#define OSD2_PROC_SCALE_COEF_IDX                   0x681c
#define OSD2_PROC_SCALE_COEF                       0x681d
#define OSD2_PROC_DB_FLT_CTRL                      0x6820
#define OSD2_PROC_DB_FLT_CTRL1                     0x6821
#define OSD2_PROC_DB_FLT_LUMA_THRD                 0x6822
#define OSD2_PROC_DB_FLT_CHRM_THRD                 0x6823
#define OSD2_PROC_DB_FLT_RANDLUT                   0x6824
#define OSD2_PROC_DB_FLT_PXI_THRD                  0x6825
#define OSD2_PROC_DB_FLT_SEED_Y                    0x6826
#define OSD2_PROC_DB_FLT_SEED_U                    0x6827
#define OSD2_PROC_DB_FLT_SEED_V                    0x6828
#define OSD2_PROC_DB_FLT_SEED3                     0x6829
#define OSD2_PROC_DB_FLT_SEED4                     0x682a
#define OSD2_PROC_DB_FLT_SEED5                     0x682b

#define OSD3_PROC_VSC_PHASE_STEP                   0x6c10
#define OSD3_PROC_VSC_INI_PHASE                    0x6c11
#define OSD3_PROC_VSC_CTRL0                        0x6c12
#define OSD3_PROC_HSC_PHASE_STEP                   0x6c13
#define OSD3_PROC_HSC_INI_PHASE                    0x6c14
#define OSD3_PROC_HSC_CTRL0                        0x6c15
#define OSD3_PROC_HSC_INI_PAT_CTRL                 0x6c16
#define OSD3_PROC_SC_DUMMY_DATA                    0x6c17
#define OSD3_PROC_SC_CTRL0                         0x6c18
#define OSD3_PROC_SCI_WH_M1                        0x6c19
#define OSD3_PROC_SCO_H_START_END                  0x6c1a
#define OSD3_PROC_SCO_V_START_END                  0x6c1b
#define OSD3_PROC_SCALE_COEF_IDX                   0x6c1c
#define OSD3_PROC_SCALE_COEF                       0x6c1d
#define OSD3_PROC_DB_FLT_CTRL                      0x6c20
#define OSD3_PROC_DB_FLT_CTRL1                     0x6c21
#define OSD3_PROC_DB_FLT_LUMA_THRD                 0x6c22
#define OSD3_PROC_DB_FLT_CHRM_THRD                 0x6c23
#define OSD3_PROC_DB_FLT_RANDLUT                   0x6c24
#define OSD3_PROC_DB_FLT_PXI_THRD                  0x6c25
#define OSD3_PROC_DB_FLT_SEED_Y                    0x6c26
#define OSD3_PROC_DB_FLT_SEED_U                    0x6c27
#define OSD3_PROC_DB_FLT_SEED_V                    0x6c28
#define OSD3_PROC_DB_FLT_SEED3                     0x6c29
#define OSD3_PROC_DB_FLT_SEED4                     0x6c2a
#define OSD3_PROC_DB_FLT_SEED5                     0x6c2b

#define OSD4_PROC_VSC_PHASE_STEP                   0x7010
#define OSD4_PROC_VSC_INI_PHASE                    0x7011
#define OSD4_PROC_VSC_CTRL0                        0x7012
#define OSD4_PROC_HSC_PHASE_STEP                   0x7013
#define OSD4_PROC_HSC_INI_PHASE                    0x7014
#define OSD4_PROC_HSC_CTRL0                        0x7015
#define OSD4_PROC_HSC_INI_PAT_CTRL                 0x7016
#define OSD4_PROC_SC_DUMMY_DATA                    0x7017
#define OSD4_PROC_SC_CTRL0                         0x7018
#define OSD4_PROC_SCI_WH_M1                        0x7019
#define OSD4_PROC_SCO_H_START_END                  0x701a
#define OSD4_PROC_SCO_V_START_END                  0x701b
#define OSD4_PROC_SCALE_COEF_IDX                   0x701c
#define OSD4_PROC_SCALE_COEF                       0x701d
#define OSD4_PROC_DB_FLT_CTRL                      0x7020
#define OSD4_PROC_DB_FLT_CTRL1                     0x7021
#define OSD4_PROC_DB_FLT_LUMA_THRD                 0x7022
#define OSD4_PROC_DB_FLT_CHRM_THRD                 0x7023
#define OSD4_PROC_DB_FLT_RANDLUT                   0x7024
#define OSD4_PROC_DB_FLT_PXI_THRD                  0x7025
#define OSD4_PROC_DB_FLT_SEED_Y                    0x7026
#define OSD4_PROC_DB_FLT_SEED_U                    0x7027
#define OSD4_PROC_DB_FLT_SEED_V                    0x7028
#define OSD4_PROC_DB_FLT_SEED3                     0x7029
#define OSD4_PROC_DB_FLT_SEED4                     0x702a
#define OSD4_PROC_DB_FLT_SEED5                     0x702b

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
	COEFS_BICUBIC_SHARP = 0,
	COEFS_BICUBIC,
	COEFS_BILINEAR,
	COEFS_2POINT_BILINEAR,
	COEFS_3POINT_TRIANGLE_SHARP,
	COEFS_3POINT_TRIANGLE,
	COEFS_4POINT_TRIANGLE,
	COEFS_4POINT_BSPLINE,
	COEFS_3POINT_BSPLINE,
	COEFS_REPEATE,
	COEFS_MAX
};

enum osd_scaler_f2v_vphase_type_e {
	OSD_SCALER_F2V_IT2IT = 0,
	OSD_SCALER_F2V_IB2IB,
	OSD_SCALER_F2V_IT2IB,
	OSD_SCALER_F2V_IB2IT,
	OSD_SCALER_F2V_P2IT,
	OSD_SCALER_F2V_P2IB,
	OSD_SCALER_F2V_IT2P,
	OSD_SCALER_F2V_IB2P,
	OSD_SCALER_F2V_P2P,
	OSD_SCALER_F2V_TYPE_MAX
};

struct osd_scaler_f2v_vphase_s {
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
