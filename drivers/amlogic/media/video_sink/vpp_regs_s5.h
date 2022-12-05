/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_sink/vpp_reg_s5.h
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

#ifndef VPP_REGS_S5_HEADER_
#define VPP_REGS_S5_HEADER_

//------------------------------------------------------------------------------
// VIU registers 0x20 -0x3f
//------------------------------------------------------------------------------
#define S5_VIU_VD1_MISC                               0x1a10
#define S5_VIU_VD2_MISC                               0x1a11
#define S5_VIU_VD3_MISC                               0x1a12
#define S5_VIU_VD4_MISC                               0x1a13
#define S5_VIU_VD5_MISC                               0x1a14
#define S5_VIU_OSD1_MISC                              0x1a15
#define S5_VIU_OSD2_MISC                              0x1a16
#define S5_VIU_OSD3_MISC                              0x1a17
#define S5_VIU_OSD4_MISC                              0x1a18
#define VIU_VIU0_MISC                              0x1a19
#define VIU_VIU1_MISC                              0x1a1a
#define VIU_VIU2_MISC                              0x1a1b
#define VIU_FRC_MISC                               0x1a1c
#define VIU_LINE_INT0                              0x1a1d
#define VIU_LINE_INT1                              0x1a1e
#define VIU_LINE_INT2                              0x1a1f
#define VIU_PROBE_CTRL                             0x1a20
#define VIU_PROBE_POS                              0x1a21
#define VIU_PROBE_HL                               0x1a22
#define VIU_RO_PROBE0                              0x1a23
#define VIU_RO_PROBE1                              0x1a24
#define VIU_WRBAK_CTRL                             0x1a25
#define VIU_RO_CFG_ERR                             0x1a26
#define VIU_SLEEP_CTRL                             0x1a27
#define VIU_DMA_CTRL0                              0x1a28
#define VIU_DMA_CTRL1                              0x1a29

// Closing file:  ./vpu_top_regs.h
//
//===========================================================================
// -----------------------------------------------
// REG_BASE:  VD1_SLICE1_VCBUS_BASE = 0x28
// -----------------------------------------------
//===========================================================================
//
// Reading file:  ./vpp_vd_proc_reg.h
//
#define VD_PROC_SR0_CTRL                           0x2800
//Bit 31    sr0_pps_dpsel                         //unsigned, RW, default = 0, 0:SR0 after PPS 1:SR0 before PPS
//Bit 30    vd1_sr0_dpsel                         //unsigned, RW, default = 0, 0:SR0 in vd1 slice0 1:SR0 in vd1 slice1
//Bit 29:0  reg_sr0_ctrl                          //unsigned, RW, default = 0, use for hsvsharp
#define VD_PROC_SR1_CTRL                           0x2801
//Bit 31:30 reserved
//Bit 29:0  reg_sr1_ctrl                          //unsigned, RW, default = 0, use for hsvsharp
#define VD_PROC_S0_IN_SIZE                         0x2802
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s0_in_hsize                   //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s0_in_vsize                   //unsigned, RW, default = 0
#define VD_PROC_S0_OUT_SIZE                        0x2803
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s0_out_hsize                  //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s0_out_vsize                  //unsigned, RW, default = 0
#define VD_PROC_S0_PPS_IN_SIZE                     0x2804
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s0_pps_in_hsize               //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s0_pps_in_vsize               //unsigned, RW, default = 0
#define VD_PROC_S0_SR0_IN_SIZE                     0x2805
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s0_sr0_in_hsize               //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s0_sr0_in_vsize               //unsigned, RW, default = 0
#define VD_PROC_S0_SR1_IN_SIZE                     0x2806
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s0_sr1_in_hsize               //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s0_sr1_in_vsize               //unsigned, RW, default = 0
#define VD_PROC_S1_IN_SIZE                         0x2807
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s1_in_hsize                   //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s1_in_vsize                   //unsigned, RW, default = 0
#define VD_PROC_S1_OUT_SIZE                        0x2808
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s1_out_hsize                  //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s1_out_vsize                  //unsigned, RW, default = 0
#define VD_PROC_S1_SR0_IN_SIZE                     0x2809
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s1_sr0_in_hsize               //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s1_sr0_in_vsize               //unsigned, RW, default = 0
#define VD_PROC_S2_IN_SIZE                         0x280a
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s2_in_hsize                   //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s2_in_vsize                   //unsigned, RW, default = 0
#define VD_PROC_S2_OUT_SIZE                        0x280b
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s2_out_hsize                  //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s2_out_vsize                  //unsigned, RW, default = 0
#define VD_PROC_S3_IN_SIZE                         0x280c
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s3_in_hsize                   //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s3_in_vsize                   //unsigned, RW, default = 0
#define VD_PROC_S3_OUT_SIZE                        0x280d
//Bit 31:29 reserved
//Bit 28:16 vd_proc_s3_out_hsize                  //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd_proc_s3_out_vsize                  //unsigned, RW, default = 0
#define VD_PROC_GCLK_CTRL                          0x280e
//Bit 25:18 sr1_gclk_ctrl                         //unsigned, RW, default = 0
//Bit 17:10 sr0_gclk_ctrl                         //unsigned, RW, default = 0
//Bit 9:8   vd_s3_top_gclk_ctrl                   //unsigned, RW, default = 0
//Bit 7:6   vd_s2_top_gclk_ctrl                   //unsigned, RW, default = 0
//Bit 5:4   vd_s1_top_gclk_ctrl                   //unsigned, RW, default = 0
//Bit 3:2   vd_s0_top_gclk_ctrl                   //unsigned, RW, default = 0
//Bit 1     reg_gclk_ctrl_h                       //unsigned, RW, default = 0
//Bit 0     reserved
#define VD_PROC_BYPASS_CTRL                        0x280f
//Bit 5      reg_bypass_prebld1                   //unsigned, RW, default = 1, 0:use vd prebld 1:bypass vd prebld
//Bit 4      vd_s3_bypass_ve                      //unsigned, RW, default = 1, 0:use ve 1:bypass ve
//Bit 3      vd_s2_bypass_ve                      //unsigned, RW, default = 1, 0:use ve 1:bypass ve
//Bit 2      vd_s1_bypass_ve                      //unsigned, RW, default = 1, 0:use ve 1:bypass ve
//Bit 1      vd_s0_bypass_ve                      //unsigned, RW, default = 1, 0:use ve 1:bypass ve
//Bit 0      reg_bypass_prebld0                   //unsigned, RW, default = 1, 0:use vd prebld 1:bypass vd prebld
#define VD_S0_HWIN_CUT                             0x2810
//Bit 31:30  reserved
//Bit 29     vd_s0_hwin_en                        //unsigned, RW, default = 0, win_en, cut the overlap size
//Bit 28:16  vd_s0_hwin_bgn                       //unsigned, RW, default = 240
//Bit 15:13 reserved
//Bit 12:0   vd_s0_hwin_end                       //unsigned, RW, default = 320
#define VD_S1_HWIN_CUT                             0x2811
//Bit 31:30  reserved
//Bit 29     vd_s1_hwin_en                        //unsigned, RW, default = 0, win_en, cut the overlap size
//Bit 28:16  vd_s1_hwin_bgn                       //unsigned, RW, default = 240
//Bit 15:13 reserved
//Bit 12:0   vd_s1_hwin_end                       //unsigned, RW, default = 320
#define VD_S2_HWIN_CUT                             0x2812
//Bit 31:30  reserved
//Bit 29     vd_s2_hwin_en                        //unsigned, RW, default = 0, win_en, cut the overlap size
//Bit 28:16  vd_s2_hwin_bgn                       //unsigned, RW, default = 240
//Bit 15:13 reserved
//Bit 12:0   vd_s2_hwin_end                       //unsigned, RW, default = 320
#define VD_S3_HWIN_CUT                             0x2813
//Bit 31:30  reserved
//Bit 29     vd_s3_hwin_en                        //unsigned, RW, default = 0, win_en, cut the overlap size
//Bit 28:16  vd_s3_hwin_bgn                       //unsigned, RW, default = 240
//Bit 15:13 reserved
//Bit 12:0   vd_s3_hwin_end                       //unsigned, RW, default = 320
#define VD1_S0_CLIP_MISC0                          0x2814
//Bit 31:30  reserved
//Bit 29:0   reg_vd1_s0_clip_misc0                //unsigned, RW, default = 30'h3fff_ffff
#define VD1_S0_CLIP_MISC1                          0x2815
//Bit 31:30  reserved
//Bit 29:0   reg_vd1_s0_clip_misc1                //unsigned, RW, default = 0
#define VD1_S1_CLIP_MISC0                          0x2816
//Bit 31:30  reserved
//Bit 29:0   reg_vd1_s1_clip_misc0                //unsigned, RW, default = 30'h3fff_ffff
#define VD1_S1_CLIP_MISC1                          0x2817
//Bit 31:30  reserved
//Bit 29:0   reg_vd1_s1_clip_misc1                //unsigned, RW, default = 0
#define VD1_S2_CLIP_MISC0                          0x2818
//Bit 31:30  reserved
//Bit 29:0   reg_vd1_s2_clip_misc0                //unsigned, RW, default = 30'h3fff_ffff
#define VD1_S2_CLIP_MISC1                          0x2819
//Bit 31:30  reserved
//Bit 29:0   reg_vd1_s2_clip_misc1                //unsigned, RW, default = 0
#define VD1_S3_CLIP_MISC0                          0x281a
//Bit 31:30  reserved
//Bit 29:0   reg_vd1_s3_clip_misc0                //unsigned, RW, default = 30'h3fff_ffff
#define VD1_S3_CLIP_MISC1                          0x281b
//Bit 31:30  reserved
//Bit 29:0   reg_vd1_s3_clip_misc1                //unsigned, RW, default = 0
#define VD1_CLIP_EXT_MODE                          0x281c
//Bit 3      reg_vd1_s3_ext_mode                  //unsigned, RW, default = 0
//Bit 2      reg_vd1_s2_ext_mode                  //unsigned, RW, default = 0
//Bit 1      reg_vd1_s1_ext_mode                  //unsigned, RW, default = 0
//Bit 0      reg_vd1_s0_ext_mode                  //unsigned, RW, default = 0
#define VD1_S0_PPS_DUMMY_DATA                      0x281d
//Bit 31:24  reserved
//Bit 23:0   vd1_s0_pps_dummy_data                //unsigned, RW, default = 0
#define VD1_S1_PPS_DUMMY_DATA                      0x281e
//Bit 31:24  reserved
//Bit 23:0   vd1_s1_pps_dummy_data                //unsigned, RW, default = 0
#define VD1_S2_PPS_DUMMY_DATA                      0x281f
//Bit 31:24  reserved
//Bit 23:0   vd1_s2_pps_dummy_data                //unsigned, RW, default = 0
#define VD1_S3_PPS_DUMMY_DATA                      0x2820
//Bit 31:24  reserved
//Bit 23:0   vd1_s3_pps_dummy_data                //unsigned, RW, default = 0
#define VD1_S0_DETUNL_CTRL                         0x2821
//Bit 31:0   vd1_s0_dtunl_ctrl                    //unsigned, RW, default = 0
#define VD1_S0_DV_BYPASS_CTRL                      0x2822
//Bit 31:2   reserved
//Bit 1      vd1_s0_dv_ext_mode                   //unsigned, RW, default = 0
//Bit 0      vd1_s0_dv_en                         //unsigned, RW, default = 0
#define VD1_S1_DETUNL_CTRL                         0x2823
//Bit 31:0   vd1_s1_dtunl_ctrl                    //unsigned, RW, default = 0
#define VD1_S1_DV_BYPASS_CTRL                      0x2824
//Bit 31:2   reserved
//Bit 1      vd1_s1_dv_ext_mode                   //unsigned, RW, default = 0
//Bit 0      vd1_s1_dv_en                         //unsigned, RW, default = 0
#define VD1_S2_DETUNL_CTRL                         0x2825
//Bit 31:0   vd1_s2_dtunl_ctrl                    //unsigned, RW, default = 0
#define VD1_S2_DV_BYPASS_CTRL                      0x2826
//Bit 31:2   reserved
//Bit 1      vd1_s2_dv_ext_mode                   //unsigned, RW, default = 0
//Bit 0      vd1_s2_dv_en                         //unsigned, RW, default = 0
#define VD1_S3_DETUNL_CTRL                         0x2827
//Bit 31:0   vd1_s3_dtunl_ctrl                    //unsigned, RW, default = 0
#define VD1_S3_DV_BYPASS_CTRL                      0x2828
//Bit 31:2   reserved
//Bit 1      vd1_s3_dv_ext_mode                   //unsigned, RW, default = 0
//Bit 0      vd1_s3_dv_en                         //unsigned, RW, default = 0
//
// Closing file:  ./vpp_vd_proc_reg.h
//
//
//
// Reading file:  ./vpp_vd2_proc_reg.h
//
#define VD2_PROC_IN_SIZE                           0x3880
//Bit 31:29 reserved
//Bit 28:16 vd2_proc_in_hsize                   //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd2_proc_in_vsize                   //unsigned, RW, default = 0
#define VD2_PROC_OUT_SIZE                          0x3881
//Bit 31:29 reserved
//Bit 28:16 vd2_proc_out_hsize                  //unsigned, RW, default = 0
//Bit 15:13 reserved
//Bit 12:0  vd2_proc_out_vsize                  //unsigned, RW, default = 0
#define VD2_PROC_GCLK_CTRL                         0x3882
//Bit 31:4  reserved
//Bit 3:2   vd2_top_gclk_ctrl                   //unsigned, RW, default = 0
//Bit 1     reg_gclk_ctrl_h                     //unsigned, RW, default = 0
//Bit 0     reserved
#define VD2_CLIP_MISC0                             0x3883
//Bit 31:30  reserved
//Bit 29:0   reg_vd2_clip_misc0                 //unsigned, RW, default = 30'h3fff_ffff
#define VD2_CLIP_MISC1                             0x3884
//Bit 31:30  reserved
//Bit 29:0   reg_vd2_clip_misc1                 //unsigned, RW, default = 0
#define VD2_CLIP_EXT_MODE                          0x3885
//Bit 0      reg_vd2_ext_mode                   //unsigned, RW, default = 0
#define VD2_PPS_DUMMY_DATA                         0x3886
//Bit 31:24  reserved
//Bit 23:0   vd2_pps_dummy_data                 //unsigned, RW, default = 0
#define VD2_DETUNL_CTRL                            0x3887
//Bit 31:0   vd2_dtunl_ctrl                    //unsigned, RW, default = 0
#define VD2_DV_BYPASS_CTRL                         0x3888
//Bit 31:2   reserved
//Bit 1      vd2_dv_ext_mode                    //unsigned, RW, default = 0
//Bit 0      vd2_dv_en                          //unsigned, RW, default = 0
#define VD2_PILITE_CTRL                            0x3889
//Bit 31:1   reserved
//Bit 0      reg_pilite_en                      //unsigned, RW, default = 0
//
// Closing file:  ./vpp_vd2_proc_reg.h
//
#define  SRSHARP0_OFFSET   (0x000)
#define  SRSHARP1_OFFSET   (0x200)

#define SHARP_SR2_CTRL                             0x5057
#define SHARP_SR2_CTRL2                            0x5164
#define SHARP_SYNC_CTRL                            0x50b0
#define DEMO_MODE_WINDOW_CTRL0                     0x5180
//Bit 31:30        reserved
//Bit 29           reg_demo_mode_enable      // unsigned ,    RW, default = 0  enable demo mode
//Bit 28           reg_demo_mode_hf_remove_enable // unsigned ,    RW, default = 0  enable DNN hf remove inside window
//Bit 27:16        reg_demo_mode_window_0    // unsigned ,    RW, default = 0  horizontal window start, included in demo window
//Bit 15:14        reg_demo_mode_inner_sel   // unsigned ,    RW, default = 1  signal selection inside window
//Bit 13:12        reg_demo_mode_outer_sel   // unsigned ,    RW, default = 0  signal selection outside window
//Bit 11: 0        reg_demo_mode_window_1    // unsigned ,    RW, default = 1920  horizontal window end,not included in demo window
#define DEMO_MODE_WINDOW_CTRL1                     0x5181
//Bit 31:28        reserved
//Bit 27:16        reg_demo_mode_window_2    // unsigned ,    RW, default = 0  vertical window start, included in demo window
//Bit 15:12        reserved
//Bit 11: 0        reg_demo_mode_window_3    // unsigned ,    RW, default = 2160  vertical window end, not included in demo window
#define NN_POST_TOP                                0x5182

#define S5_SRSHARP0_SHARP_SR2_CTRL                    (SRSHARP0_OFFSET + SHARP_SR2_CTRL)
#define S5_SRSHARP1_SHARP_SR2_CTRL                    (SRSHARP1_OFFSET + SHARP_SR2_CTRL)
#define S5_SRSHARP0_SHARP_SR2_CTRL2                   (SRSHARP0_OFFSET + SHARP_SR2_CTRL2)
#define S5_SRSHARP1_SHARP_SR2_CTRL2                   (SRSHARP1_OFFSET + SHARP_SR2_CTRL2)
#define S5_SRSHARP1_DEMO_MODE_WINDOW_CTRL0            (SRSHARP1_OFFSET + DEMO_MODE_WINDOW_CTRL0) // 0x80  //
#define S5_SRSHARP1_DEMO_MODE_WINDOW_CTRL1            (SRSHARP1_OFFSET + DEMO_MODE_WINDOW_CTRL1) // 0x81  //
#define S5_SRSHARP0_SHARP_SYNC_CTRL                   (SRSHARP0_OFFSET + SHARP_SYNC_CTRL)
#define S5_SRSHARP1_SHARP_SYNC_CTRL                   (SRSHARP1_OFFSET + SHARP_SYNC_CTRL)
#define S5_SRSHARP1_NN_POST_TOP                       (SRSHARP1_OFFSET + NN_POST_TOP) // 0x82  //

//===========================================================================
// -----------------------------------------------
// REG_BASE:  VPP_VCBUS_BASE = 0x1d
// -----------------------------------------------
//===========================================================================
//
// Reading file:  ./vpp_post_reg.h
//
//===========================================================================
// Vpp0 postprocessing Registers
//===========================================================================
//====================top====================
#define S5_VPP_POST_GCLK_CTRL                         0x1d00
//Bit 31:8   reserved
//Bit 7:6    reg_4s4p_gc_ctrl                  //unsigned, RW, default = 0, 4slice4ppc clk gating ctrl
//Bit 5:4    reg_4p4s_gc_ctrl                  //unsigned, RW, default = 0, 4ppc4slice clk gating ctrl
//Bit 3:2    reg_2p2s_gc_ctrl                  //unsigned, RW, default = 0, 2ppc2slice clk gating ctrl
//Bit 0      reg_gclk_ctrl_h                   //unsigned, RW, default = 0, reg_gclk_ctrl high bit, low bit is 0
#define S5_VPP_POSTBLEND_H_V_SIZE                      0x1d01
//Bit 31:30  reserved
//Bit 29:16  vpp_postblend_vsize               //unsigned, RW, default = 1080
//Bit 15:14  reserved
//Bit 13:0   vpp_postblend_hsize               //unsigned, RW, default = 1920
#define S5_VPP_POSTBLEND_CTRL                          0x1d02
//Bit 31:10  reserved
//Bit 9      reg_cbus_mode                     //unsigned, RW, default = 0
//Bit 8      vpp_post_blend_en                 //unsigned, RW, default = 0
//Bit 7 :0   reg_ovlp_size                     //unsigned, RW, default = 0
#define S5_VPP_POSTBLEND_VD1_H_START_END              0x1d03
//Bit 31:29  reserved
//Bit 28:16  vd1xn_scope_hs                    //unsigned, RW, default = 1920, vd1 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1xn_scope_he                    //unsigned, RW, default = 0, vd1 scope horizontal ending
#define S5_VPP_POSTBLEND_VD1_V_START_END              0x1d04
//Bit 31:29  reserved
//Bit 28:16  vd1xn_scope_vs                    //unsigned, RW, default = 1080, vd1 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1xn_scope_ve                    //unsigned, RW, default = 0, vd1 scope vertical ending
#define S5_VPP_POSTBLEND_VD2_H_START_END              0x1d05
//Bit 31:29  reserved
//Bit 28:16  vd2xn_scope_hs                    //unsigned, RW, default = 1920, vd2 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd2xn_scope_he                    //unsigned, RW, default = 0, vd2 scope horizontal ending
#define S5_VPP_POSTBLEND_VD2_V_START_END              0x1d06
//Bit 31:29  reserved
//Bit 28:16  vd2xn_scope_vs                    //unsigned, RW, default = 1080, vd2 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd2xn_scope_ve                    //unsigned, RW, default = 0, vd2 scope vertical ending
#define S5_VPP_POSTBLEND_VD3_H_START_END              0x1d07
//Bit 31:29  reserved
//Bit 28:16  vd3xn_scope_hs                    //unsigned, RW, default = 1920, vd3 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd3xn_scope_he                    //unsigned, RW, default = 0, vd3 scope horizontal ending
#define S5_VPP_POSTBLEND_VD3_V_START_END              0x1d08
//Bit 31:29  reserved
//Bit 28:16  vd3xn_scope_vs                    //unsigned, RW, default = 1080, vd3 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd3xn_scope_ve                    //unsigned, RW, default = 0, vd3 scope vertical ending
#define S5_VPP_OSD1_BLD_H_SCOPE                       0x1d09
//Bit 31:29  reserved
//Bit 28:16  osd1xn_scope_hs                    //unsigned, RW, default = 1920, osd1 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   osd1xn_scope_he                    //unsigned, RW, default = 0, osd1 scope horizontal ending
#define S5_VPP_OSD1_BLD_V_SCOPE                       0x1d0a
//Bit 31:29  reserved
//Bit 28:16  osd1xn_scope_vs                    //unsigned, RW, default = 1080, osd1 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   osd1xn_scope_ve                    //unsigned, RW, default = 0, osd1 scope vertical ending
#define S5_VPP_OSD2_BLD_H_SCOPE                       0x1d0b
//Bit 31:29  reserved
//Bit 28:16  osd2xn_scope_hs                    //unsigned, RW, default = 1920, osd2 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   osd2xn_scope_he                    //unsigned, RW, default = 0, osd2 scope horizontal ending
#define S5_VPP_OSD2_BLD_V_SCOPE                       0x1d0c
//Bit 31:29  reserved
//Bit 28:16  osd2xn_scope_vs                    //unsigned, RW, default = 1080, osd2 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   osd2xn_scope_ve                    //unsigned, RW, default = 0, osd2 scope vertical ending
#define S5_VD1_BLEND_SRC_CTRL                         0x1d0d
//Bit 31:5   reserved
//Bit 4      vd1_postbld_premult                //unsigned, RW, default = 0, vd1 premult en
//Bit 3:0    vd1_postbld_src                    //unsigned, RW, default = 0, vd1 source sel
#define S5_VD2_BLEND_SRC_CTRL                         0x1d0e
//Bit 31:5   reserved
//Bit 4      vd2_postbld_premult                //unsigned, RW, default = 0, vd2 premult en
//Bit 3:0    vd2_postbld_src                    //unsigned, RW, default = 0, vd2 source sel
#define S5_VD3_BLEND_SRC_CTRL                         0x1d0f
//Bit 31:5   reserved
//Bit 4      vd3_postbld_premult                //unsigned, RW, default = 0, vd3 premult en
//Bit 3:0    vd3_postbld_src                    //unsigned, RW, default = 0, vd3 source sel
#define S5_OSD1_BLEND_SRC_CTRL                        0x1d10
//Bit 31:5   reserved
//Bit 4      osd1_postbld_premult                //unsigned, RW, default = 0, osd1 premult en
//Bit 3:0    osd1_postbld_src                    //unsigned, RW, default = 0, osd1 source sel
#define S5_OSD2_BLEND_SRC_CTRL                        0x1d11
//Bit 31:5   reserved
//Bit 4      osd2_postbld_premult                //unsigned, RW, default = 0, osd2 premult en
//Bit 3:0    osd2_postbld_src                    //unsigned, RW, default = 0, osd2 source sel
#define S5_VD1_POSTBLEND_ALPHA                        0x1d12
//Bit 31:9   reserved
//Bit 8:0    vd1_postbld_alpha                   //unsigned, RW, default = 9'h100
#define S5_VD2_POSTBLEND_ALPHA                        0x1d13
//Bit 31:9   reserved
//Bit 8:0    vd2_postbld_alpha                   //unsigned, RW, default = 9'h100
#define S5_VD3_POSTBLEND_ALPHA                        0x1d14
//Bit 31:9   reserved
//Bit 8:0    vd3_postbld_alpha                   //unsigned, RW, default = 9'h100
#define S5_VPP_OBUF_RAM_CTRL                          0x1d15
//Bit 31:2   reserved
//Bit 1:0    obuf_ram_slice_mode                 //unsigned, RW, default = 0, 0: 1x2k ram 1: 2x2k ram 3:4x2k ram
#define S5_VPP_4S4P_CTRL                              0x1d16
//Bit 31:2   reserved
//Bit 1:0    reg_4s4p_mode                       //unsigned, RW, default = 0, 0: 4slice to 4ppc  1: 2 slice to 4ppc 2:1 slice to 4ppc  3:disable
#define S5_VPP_4P4S_CTRL                              0x1d17
//Bit 31:2   reserved
//Bit 1:0    reg_4p4s_mode                       //unsigned, RW, default = 0, 0: 4ppc to 4slice 1: 4ppc to 2slice 2:4ppc to 1slice 3:disable
#define S5_VPP_POST_WIN_CUT_CTRL                      0x1d18
//Bit 31     reg_win_en                          //unsigned, RW, default = 0
//Bit 30:29  reserved
//Bit 28:16  reg_win_end_h                       //unsigned, RW, default = 1920
//Bit 15:13  reserved
//Bit 12:0   reg_win_bgn_h                       //unsigned, RW, default = 0
#define S5_VPP_POST_HOLD_CTRL                         0x1d1f
//Bit 31     reg_hold_en                      //unsigned ,RW, default = 0
//Bit 30     reg_hold_mode                    //unsigned ,RW, default = 1
//Bit 29     reserved
//Bit 28:16  hold_hsizem1                     //unsigned ,RW, default = 13'h77f
//Bit 15:14  reserved
//Bit 13:8   hold_vsizem1                     //unsigned ,RW, default = 6'h3
//Bit 7 :4   reg_pass_num                     //unsigned ,RW, default = 4'h1
//Bit 3 :0   reg_hold_num                     //unsigned ,RW, default = 4'h3
#define S5_VPP_POST_VD1_WIN_CUT_CTRL                  0x1df0
//Bit 31     reg_vd1_win_en                   //unsigned, RW, default = 0
//Bit 30:13  reserved
//Bit 12:0   reg_vd1_win_hsize                //unsigned, RW, default = 0
#define S5_VPP_POST_PAD_CTRL                          0x1df1
//Bit 31     reg_pad_enable                   //unsigned, RW, default = 0
//Bit 30     reg_pad_rpt_lcol                 //unsigned, RW, default = 0
//Bit 29:28  reg_pad_gc_ctrl                  //unsigned, RW, default = 0
//Bit 27:24  reserved
//Bit 23:0   reg_pad_dummy                    //unsigned, RW, default = 0
#define S5_VPP_POST_PAD_HSIZE                         0x1df2
//Bit 31:14  reserved
//Bit 13:0   reg_pad_hsize                    //unsigned, RW, default = 0
//====================postblend====================
#define S5_VPP_POST_BLEND_CTRL                        0x1d19
//Bit 31:28  reserved
//Bit 27:20  hold_lines                        //unsigned ,RW, default = 4
//Bit 19:2   reserved
//Bit 1 :0   gclk_ctrl                         //unsigned ,RW, default = 0
#define S5_VPP_POST_BLEND_BLEND_DUMMY_DATA            0x1d1a
//Bit 31:24  reserved
//Bit 23:16  blend0_dummy_data_y               //unsigned ,RW, default = 8'h00
//Bit 15:8   blend0_dummy_data_cb              //unsigned ,RW, default = 8'h80
//Bit 7 :0   blend0_dummy_data_cr              //unsigned ,RW, default = 8'h80
#define S5_VPP_POST_BLEND_DUMMY_ALPHA                 0x1d1b
//Bit 31:25  reserved
//Bit 24:16  blend1_dummy_alpha                //unsigned ,RW, default = 9'h0
//Bit 15:9   reserved
//Bit 8 :0   blend0_dummy_alpha                //unsigned ,RW, default = 9'h0
#define S5_VPP_POST_BLEND_RO_CURRENT_XY               0x1d1c
//Bit 31:0   ro_blend_current_xy               //unsigned ,RO, default = 32'h0
#define S5_VPP_POST_BLEND_DUMMY_ALPHA1                0x1d1d
//Bit 31:25  reserved
//Bit 24:16  blend3_dummy_alpha                //unsigned ,RW, default = 9'h0
//Bit 15:9   reserved
//Bit 8 :0   blend2_dummy_alpha                //unsigned ,RW, default = 9'h0
#define S5_VPP_POST_BLEND_OSD_FLAG_CTRL               0x1d1e
//Bit 31:23  reg_osd_flag_thd
//Bit 22:16  reserved                         //unsigned ,RW, default = 9'h80
//Bit 15:8   reg_osd_flag_premult_mask        //unsigned ,RW, default = 8'h01
//Bit 7 :5   reserved
//Bit 4 :0   reg_default_osd_flag             //unsigned ,RW, default = 5'h18
//
// Closing file:  ./vpp_post_reg.h
//
//
// Reading file:  ./pip_alph_gen_regs.h
//
//
// Reading file:  ./vd1_pip_alph_gen_regs.h
#define S5_VD1_PIP_ALPH_CTRL                          0x1d20
#define S5_VD1_PIP_ALPH_SCP_H_0                       0x1d21
#define S5_VD1_PIP_ALPH_SCP_H_1                       0x1d22
#define S5_VD1_PIP_ALPH_SCP_H_2                       0x1d23
#define S5_VD1_PIP_ALPH_SCP_H_3                       0x1d24
#define S5_VD1_PIP_ALPH_SCP_H_4                       0x1d25
#define S5_VD1_PIP_ALPH_SCP_H_5                       0x1d26
#define S5_VD1_PIP_ALPH_SCP_H_6                       0x1d27
#define S5_VD1_PIP_ALPH_SCP_H_7                       0x1d28
#define S5_VD1_PIP_ALPH_SCP_H_8                       0x1d29
#define S5_VD1_PIP_ALPH_SCP_H_9                       0x1d2a
#define S5_VD1_PIP_ALPH_SCP_H_10                      0x1d2b
#define S5_VD1_PIP_ALPH_SCP_H_11                      0x1d2c
#define S5_VD1_PIP_ALPH_SCP_H_12                      0x1d2d
#define S5_VD1_PIP_ALPH_SCP_H_13                      0x1d2e
#define S5_VD1_PIP_ALPH_SCP_H_14                      0x1d2f
#define S5_VD1_PIP_ALPH_SCP_H_15                      0x1d30
#define S5_VD1_PIP_ALPH_SCP_V_0                       0x1d31
#define S5_VD1_PIP_ALPH_SCP_V_1                       0x1d32
#define S5_VD1_PIP_ALPH_SCP_V_2                       0x1d33
#define S5_VD1_PIP_ALPH_SCP_V_3                       0x1d34
#define S5_VD1_PIP_ALPH_SCP_V_4                       0x1d35
#define S5_VD1_PIP_ALPH_SCP_V_5                       0x1d36
#define S5_VD1_PIP_ALPH_SCP_V_6                       0x1d37
#define S5_VD1_PIP_ALPH_SCP_V_7                       0x1d38
#define S5_VD1_PIP_ALPH_SCP_V_8                       0x1d39
#define S5_VD1_PIP_ALPH_SCP_V_9                       0x1d3a
#define S5_VD1_PIP_ALPH_SCP_V_10                      0x1d3b
#define S5_VD1_PIP_ALPH_SCP_V_11                      0x1d3c
#define S5_VD1_PIP_ALPH_SCP_V_12                      0x1d3d
#define S5_VD1_PIP_ALPH_SCP_V_13                      0x1d3e
#define S5_VD1_PIP_ALPH_SCP_V_14                      0x1d3f
#define S5_VD1_PIP_ALPH_SCP_V_15                      0x1d40

#define S5_VD2_PIP_ALPH_CTRL                          0x1d41
#define S5_VD2_PIP_ALPH_SCP_H_0                       0x1d42
#define S5_VD2_PIP_ALPH_SCP_H_1                       0x1d43
#define S5_VD2_PIP_ALPH_SCP_H_2                       0x1d44
#define S5_VD2_PIP_ALPH_SCP_H_3                       0x1d45
#define S5_VD2_PIP_ALPH_SCP_H_4                       0x1d46
#define S5_VD2_PIP_ALPH_SCP_H_5                       0x1d47
#define S5_VD2_PIP_ALPH_SCP_H_6                       0x1d48
#define S5_VD2_PIP_ALPH_SCP_H_7                       0x1d49
#define S5_VD2_PIP_ALPH_SCP_H_8                       0x1d4a
#define S5_VD2_PIP_ALPH_SCP_H_9                       0x1d4b
#define S5_VD2_PIP_ALPH_SCP_H_10                      0x1d4c
#define S5_VD2_PIP_ALPH_SCP_H_11                      0x1d4d
#define S5_VD2_PIP_ALPH_SCP_H_12                      0x1d4e
#define S5_VD2_PIP_ALPH_SCP_H_13                      0x1d4f
#define S5_VD2_PIP_ALPH_SCP_H_14                      0x1d50
#define S5_VD2_PIP_ALPH_SCP_H_15                      0x1d51
#define S5_VD2_PIP_ALPH_SCP_V_0                       0x1d52
#define S5_VD2_PIP_ALPH_SCP_V_1                       0x1d53
#define S5_VD2_PIP_ALPH_SCP_V_2                       0x1d54
#define S5_VD2_PIP_ALPH_SCP_V_3                       0x1d55
#define S5_VD2_PIP_ALPH_SCP_V_4                       0x1d56
#define S5_VD2_PIP_ALPH_SCP_V_5                       0x1d57
#define S5_VD2_PIP_ALPH_SCP_V_6                       0x1d58
#define S5_VD2_PIP_ALPH_SCP_V_7                       0x1d59
#define S5_VD2_PIP_ALPH_SCP_V_8                       0x1d5a
#define S5_VD2_PIP_ALPH_SCP_V_9                       0x1d5b
#define S5_VD2_PIP_ALPH_SCP_V_10                      0x1d5c
#define S5_VD2_PIP_ALPH_SCP_V_11                      0x1d5d
#define S5_VD2_PIP_ALPH_SCP_V_12                      0x1d5e
#define S5_VD2_PIP_ALPH_SCP_V_13                      0x1d5f
#define S5_VD2_PIP_ALPH_SCP_V_14                      0x1d60
#define S5_VD2_PIP_ALPH_SCP_V_15                      0x1d61

// synopsys translate_off
// synopsys translate_on
// Reading file:  ./vpp_vd_sys_top_regs.h
//
//===========================================================================
// Video sys top egisters
//===========================================================================
//====================top====================
#define VPP_VD_SYS_GCLK_CTRL                       0x3200
//Bit 31:5   reserved
//Bit 5:4    reg_pad_gc_ctrl                     //unsigned, RW, default = 0, padding module gating ctrl
//Bit 3:2    reg_4s4p_gc_ctrl                    //unsigned, RW, default = 0, 4slice4ppc clk gating ctrl
//Bit 0      reg_gclk_ctrl_h                     //unsigned, RW, default = 0, reg_gclk_ctrl high bit, low bit is 0
#define VPP_VD_PREBLEND_H_V_SIZE                    0x3201
//Bit 31:30  reserved
//Bit 29:16  vpp_vd_preblend_vsize                //unsigned, RW, default = 1080
//Bit 15:14  reserved
//Bit 13:0   vpp_vd_preblend_hsize                //unsigned, RW, default = 1920
#define VPP_VD_PREBLEND_CTRL                        0x3202
//Bit 31:1   reserved
//Bit 0      vpp_vd_preblend_en                  //unsigned, RW, default = 0
#define S5_VPP_PREBLEND_VD1_H_START_END               0x3203
//Bit 31:29  reserved
//Bit 28:16  vd1_prebld_scope_hs                 //unsigned, RW, default = 0, vd1 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_prebld_scope_he                 //unsigned, RW, default = 1920, vd1 scope horizontal ending
#define S5_VPP_PREBLEND_VD1_V_START_END               0x3204
//Bit 31:29  reserved
//Bit 28:16  vd1_prebld_scope_vs                 //unsigned, RW, default = 0, vd1 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_prebld_scope_ve                 //unsigned, RW, default = 1080, vd1 scope vertical ending
#define S5_VPP_PREBLEND_VD2_H_START_END               0x3205
//Bit 31:29  reserved
//Bit 28:16  vd2_prebld_scope_hs                 //unsigned, RW, default = 0, vd2 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd2_prebld_scope_he                 //unsigned, RW, default = 1920, vd2 scope horizontal ending
#define S5_VPP_PREBLEND_VD2_V_START_END               0x3206
//Bit 31:29  reserved
//Bit 28:16  vd2_prebld_scope_vs                 //unsigned, RW, default = 0, vd2 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd2_prebld_scope_ve                 //unsigned, RW, default = 1080, vd2 scope vertical ending
#define VD1_PREBLEND_SRC_CTRL                      0x3207
//Bit 31:5   reserved
//Bit 4      vd1_prebld_premult                  //unsigned, RW, default = 0, vd1 premult en
//Bit 3:0    vd1_prebld_src                      //unsigned, RW, default = 0, vd1 source sel
#define VD2_PREBLEND_SRC_CTRL                      0x3208
//Bit 31:5   reserved
//Bit 4      vd2_prebld_premult                  //unsigned, RW, default = 0, vd2 premult en
//Bit 3:0    vd2_prebld_src                      //unsigned, RW, default = 0, vd2 source sel
#define VPP_VD_BLND_H_V_SIZE                       0x3209
//Bit 31:30  reserved
//Bit 29:16  vpp_vd_blend_vsize                  //unsigned, RW, default = 1080
//Bit 15:14  reserved
//Bit 13:0   vpp_vd_blend_hsize                  //unsigned, RW, default = 1920
#define VPP_VD_BLND_CTRL                           0x320a
//Bit 31:1   reserved
//Bit 0      vpp_vd_blend_en                     //unsigned, RW, default = 0
#define VPP_BLEND_VD1_S0_H_START_END               0x320b
//Bit 31:29  reserved
//Bit 28:16  vd1_s0_blend_scope_hs               //unsigned, RW, default = 0, vd1 slice0 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s0_blend_scope_he               //unsigned, RW, default = 1920, vd1 slice0 scope horizontal ending
#define VPP_BLEND_VD1_S0_V_START_END               0x320c
//Bit 31:29  reserved
//Bit 28:16  vd1_s0_blend_scope_vs               //unsigned, RW, default = 0, vd1 slice0 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s0_blend_scope_ve               //unsigned, RW, default = 1080, vd1 slice0 scope vertical ending
#define VPP_BLEND_VD1_S1_H_START_END               0x320d
//Bit 31:29  reserved
//Bit 28:16  vd1_s1_blend_scope_hs               //unsigned, RW, default = 0, vd1 slice1 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s1_blend_scope_he               //unsigned, RW, default = 1920, vd1 slice1 scope horizontal ending
#define VPP_BLEND_VD1_S1_V_START_END               0x320e
//Bit 31:29  reserved
//Bit 28:16  vd1_s1_blend_scope_vs               //unsigned, RW, default = 0, vd1 slice1 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s1_blend_scope_ve               //unsigned, RW, default = 1080, vd1 slice1 scope vertical ending
#define VPP_BLEND_VD1_S2_H_START_END               0x320f
//Bit 31:29  reserved
//Bit 28:16  vd1_s2_blend_scope_hs               //unsigned, RW, default = 0, vd1 slice2 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s2_blend_scope_he               //unsigned, RW, default = 1920, vd1 slice2 scope horizontal ending
#define VPP_BLEND_VD1_S2_V_START_END               0x3210
//Bit 31:29  reserved
//Bit 28:16  vd1_s2_blend_scope_vs               //unsigned, RW, default = 0, vd1 slice2 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s2_blend_scope_ve               //unsigned, RW, default = 1080, vd1 slice2 scope vertical ending
#define VPP_BLEND_VD1_S3_H_START_END               0x3211
//Bit 31:29  reserved
//Bit 28:16  vd1_s3_blend_scope_hs               //unsigned, RW, default = 0, vd1 slice3 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s3_blend_scope_he               //unsigned, RW, default = 1920, vd1 slice3 scope horizontal ending
#define VPP_BLEND_VD1_S3_V_START_END               0x3212
//Bit 31:29  reserved
//Bit 28:16  vd1_s3_blend_scope_vs               //unsigned, RW, default = 0, vd1 slice3 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s3_blend_scope_ve               //unsigned, RW, default = 1080, vd1 slice3 scope vertical ending
#define VD1_S0_BLEND_SRC_CTRL                      0x3213
//Bit 31:5   reserved
//Bit 4      vd1_s0_blend_premult                //unsigned, RW, default = 0, vd1 s0 premult en
//Bit 3:0    vd1_s0_blend_src                    //unsigned, RW, default = 0, vd1 s0 source sel
#define VD1_S1_BLEND_SRC_CTRL                      0x3214
//Bit 31:5   reserved
//Bit 4      vd1_s1_blend_premult                //unsigned, RW, default = 0, vd1 s1 premult en
//Bit 3:0    vd1_s1_blend_src                    //unsigned, RW, default = 0, vd1 s1 source sel
#define VD1_S2_BLEND_SRC_CTRL                      0x3215
//Bit 31:5   reserved
//Bit 4      vd1_s2_blend_premult                //unsigned, RW, default = 0, vd1 s2 premult en
//Bit 3:0    vd1_s2_blend_src                    //unsigned, RW, default = 0, vd1 s2 source sel
#define VD1_S3_BLEND_SRC_CTRL                      0x3216
//Bit 31:5   reserved
//Bit 4      vd1_s3_blend_premult                //unsigned, RW, default = 0, vd1 s3 premult en
//Bit 3:0    vd1_s3_blend_src                    //unsigned, RW, default = 0, vd1 s3 source sel
#define VPP_VD_SYS_CTRL                            0x3217
//Bit 31:2   reserved
//Bit 2      reg_vd2_dpsel                       //unsigned, RW, default = 0, 0:vd2_proc outputinclude pi_lite 1:slice2ppc output
//Bit 1:0    reg_vd1_dpsel                       //unsigned, RW, default = 1, 0:pi or s2p output 1:4slice4ppc output 2:2slice2ppc output
#define SLICE2PPC_H_V_SIZE                         0x3218
//Bit 31:29  reserved
//Bit 28:16  slice2ppc_vsize                     //unsigned, RW, default = 1080
//Bit 15:13  reserved
//Bit 12:0   slice2ppc_hsize                     //unsigned, RW, default = 1920
#define VD1_PI_CTRL                                0x3219
//Bit 31:1   reserved
//Bit 0      reg_pi_en                           //unsigned, RW, default = 0
#define VD1_PREBLEND_ALPHA                         0x321a
//Bit 31:9   reserved
//Bit 8:0    vd1_prebld_alpha                    //unsigned, RW, default = 9'h100
#define VD2_PREBLEND_ALPHA                         0x321b
//Bit 31:9   reserved
//Bit 8:0    vd2_prebld_alpha                    //unsigned, RW, default = 9'h100
#define VD1_S0S1_BLEND_ALPHA                       0x321c
//Bit 31:25  reserved
//Bit 24:16  vd1_s1_bld_alpha                    //unsigned, RW, default = 9'h100
//Bit 15:9   reserved
//Bit 8:0    vd1_s0_bld_alpha                    //unsigned, RW, default = 9'h100
#define VD1_S2S3_BLEND_ALPHA                       0x321d
//Bit 31:25  reserved
//Bit 24:16  vd1_s3_bld_alpha                    //unsigned, RW, default = 9'h100
//Bit 15:9   reserved
//Bit 8:0    vd1_s2_bld_alpha                    //unsigned, RW, default = 9'h100
#define VD1_PAD_CTRL                               0x321e
//Bit 31:29  reserved
//Bit 28:16  vd_proc_end_vsize                   //unsigned, RW, default = 2160
//Bit 15:8   reserved
//Bit 7      reg_s3_pad_rpt_lcol                 //unsigned, RW, default = 0, s3 col padding enable, let hsize be divided by 32
//Bit 6      reg_s2_pad_rpt_lcol                 //unsigned, RW, default = 0, s2 col padding enable, let hsize be divided by 32
//Bit 5      reg_s1_pad_rpt_lcol                 //unsigned, RW, default = 0, s1 col padding enable, let hsize be divided by 32
//Bit 4      reg_s0_pad_rpt_lcol                 //unsigned, RW, default = 0, s0 col padding enable, let hsize be divided by 32
//Bit 3      reg_s3_pad_enable                   //unsigned, RW, default = 0, s3 padding module enable
//Bit 2      reg_s2_pad_enable                   //unsigned, RW, default = 0, s2 padding module enable
//Bit 1      reg_s1_pad_enable                   //unsigned, RW, default = 0, s1 padding module enable
//Bit 0      reg_s0_pad_enable                   //unsigned, RW, default = 0, s0 padding module enable
#define VD1_PAD_DUMMY_DATA                         0x321f
//Bit 31:24  reserved
//Bit 23:16  vd1_pad_dummy_y                     //unsigned ,RW, default = 8'h00
//Bit 15:8   vd1_pad_dummy_cb                    //unsigned ,RW, default = 8'h80
//Bit 7 :0   vd1_pad_dummy_cr                    //unsigned ,RW, default = 8'h80
#define VD1_S0_PAD_H_SIZE0                         0x3220
//Bit 31:29  reserved
//Bit 28:16  vd1_s0_pad_hs0                      //unsigned, RW, default = 0, vd1 slice0 padding module horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s0_pad_he0                      //unsigned, RW, default = 1920, vd1 slice0 padding module horizontal ending
#define VD1_S0_PAD_V_SIZE0                         0x3221
//Bit 31:29  reserved
//Bit 28:16  vd1_s0_pad_vs0                      //unsigned, RW, default = 0, vd1 slice0 padding module vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s0_pad_ve0                      //unsigned, RW, default = 1080, vd1 slice0 padding module vertical ending
#define VD1_S1_PAD_H_SIZE0                         0x3222
//Bit 31:29  reserved
//Bit 28:16  vd1_s1_pad_hs0                      //unsigned, RW, default = 0, vd1 slice1 padding module horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s1_pad_he0                      //unsigned, RW, default = 1920, vd1 slice1 padding module horizontal ending
#define VD1_S1_PAD_V_SIZE0                         0x3223
//Bit 31:29  reserved
//Bit 28:16  vd1_s1_pad_vs0                      //unsigned, RW, default = 0, vd1 slice1 padding module vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s1_pad_ve0                      //unsigned, RW, default = 1080, vd1 slice1 padding module vertical ending
#define VD1_S2_PAD_H_SIZE0                         0x3224
//Bit 31:29  reserved
//Bit 28:16  vd1_s2_pad_hs0                      //unsigned, RW, default = 0, vd1 slice2 padding module horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s2_pad_he0                      //unsigned, RW, default = 1920, vd1 slice2 padding module horizontal ending
#define VD1_S2_PAD_V_SIZE0                         0x3225
//Bit 31:29  reserved
//Bit 28:16  vd1_s2_pad_vs0                      //unsigned, RW, default = 0, vd1 slice2 padding module vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s2_pad_ve0                      //unsigned, RW, default = 1080, vd1 slice2 padding module vertical ending
#define VD1_S3_PAD_H_SIZE0                         0x3226
//Bit 31:29  reserved
//Bit 28:16  vd1_s3_pad_hs0                      //unsigned, RW, default = 0, vd1 slice3 padding module horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s3_pad_he0                      //unsigned, RW, default = 1920, vd1 slice3 padding module horizontal ending
#define VD1_S3_PAD_V_SIZE0                         0x3227
//Bit 31:29  reserved
//Bit 28:16  vd1_s3_pad_vs0                      //unsigned, RW, default = 0, vd1 slice3 padding module vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s3_pad_ve0                      //unsigned, RW, default = 1080, vd1 slice3 padding module vertical ending
#define VD1_S0_PAD_H_SIZE1                         0x3228
//Bit 31:29  reserved
//Bit 28:16  vd1_s0_pad_hs1                      //unsigned, RW, default = 0, vd1 slice0 padding module horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s0_pad_he1                      //unsigned, RW, default = 1920, vd1 slice0 padding module horizontal ending
#define VD1_S0_PAD_V_SIZE1                         0x3229
//Bit 31:29  reserved
//Bit 28:16  vd1_s0_pad_vs1                      //unsigned, RW, default = 0, vd1 slice0 padding module vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s0_pad_ve1                      //unsigned, RW, default = 1080, vd1 slice0 padding module vertical ending
#define VD1_S1_PAD_H_SIZE1                         0x322a
//Bit 31:29  reserved
//Bit 28:16  vd1_s1_pad_hs1                      //unsigned, RW, default = 0, vd1 slice1 padding module horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s1_pad_he1                      //unsigned, RW, default = 1920, vd1 slice1 padding module horizontal ending
#define VD1_S1_PAD_V_SIZE1                         0x322b
//Bit 31:29  reserved
//Bit 28:16  vd1_s1_pad_vs1                      //unsigned, RW, default = 0, vd1 slice1 padding module vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s1_pad_ve1                      //unsigned, RW, default = 1080, vd1 slice1 padding module vertical ending
#define VD1_S2_PAD_H_SIZE1                         0x322c
//Bit 31:29  reserved
//Bit 28:16  vd1_s2_pad_hs1                      //unsigned, RW, default = 0, vd1 slice2 padding module horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s2_pad_he1                      //unsigned, RW, default = 1920, vd1 slice2 padding module horizontal ending
#define VD1_S2_PAD_V_SIZE1                         0x322d
//Bit 31:29  reserved
//Bit 28:16  vd1_s2_pad_vs1                      //unsigned, RW, default = 0, vd1 slice2 padding module vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s2_pad_ve1                      //unsigned, RW, default = 1080, vd1 slice2 padding module vertical ending
#define VD1_S3_PAD_H_SIZE1                         0x322e
//Bit 31:29  reserved
//Bit 28:16  vd1_s3_pad_hs1                      //unsigned, RW, default = 0, vd1 slice3 padding module horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s3_pad_he1                      //unsigned, RW, default = 1920, vd1 slice3 padding module horizontal ending
#define VD1_S3_PAD_V_SIZE1                         0x322f
//Bit 31:29  reserved
//Bit 28:16  vd1_s3_pad_vs1                      //unsigned, RW, default = 0, vd1 slice3 padding module vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1_s3_pad_ve1                      //unsigned, RW, default = 1080, vd1 slice3 padding module vertical ending
#define VD1_S2S3_S2P_SIZE                          0x3230
//Bit 31:29  reserved
//Bit 28:16  vd1_s2s3_s2p_vsize                  //unsigned, RW, default = 1080
//Bit 15:13  reserved
//Bit 12:0   vd1_s2s3_s2p_hsize                  //unsigned, RW, default = 1920
//====================preblend====================
#define VPP_VD_PRE_BLEND_CTRL                      0x3240
//Bit 31:28  reserved
//Bit 27:20  hold_lines                        //unsigned ,RW, default = 4
//Bit 19:2   reserved
//Bit 1 :0   gclk_ctrl                         //unsigned ,RW, default = 0
#define VPP_VD_PRE_BLEND_DUMMY_DATA                0x3241
//Bit 31:24  reserved
//Bit 23:16  blend0_dummy_data_y               //unsigned ,RW, default = 8'h00
//Bit 15:8   blend0_dummy_data_cb              //unsigned ,RW, default = 8'h80
//Bit 7 :0   blend0_dummy_data_cr              //unsigned ,RW, default = 8'h80
#define VPP_VD_PRE_BLEND_DUMMY_ALPHA               0x3242
//Bit 31:25  reserved
//Bit 24:16  blend1_dummy_alpha                //unsigned ,RW, default = 9'h0
//Bit 15:9   reserved
//Bit 8 :0   blend0_dummy_alpha                //unsigned ,RW, default = 9'h0
#define VPP_VD_PRE_BLEND_RO_CURRENT_XY             0x3243
//Bit 31:0   ro_blend_current_xy               //unsigned ,RO, default = 32'h0
#define VPP_VD_PRE_BLEND_DUMMY_ALPHA1              0x3244
//Bit 31:25  reserved
//Bit 24:16  blend3_dummy_alpha                //unsigned ,RW, default = 9'h0
//Bit 15:9   reserved
//Bit 8 :0   blend2_dummy_alpha                //unsigned ,RW, default = 9'h0
#define VPP_VD_PRE_BLEND_OSD_FLAG_CTRL             0x3245
//Bit 31:23  reg_osd_flag_thd
//Bit 22:16  reserved                         //unsigned ,RW, default = 9'h80
//Bit 15:8   reg_osd_flag_premult_mask        //unsigned ,RW, default = 8'h01
//Bit 7 :5   reserved
//Bit 4 :0   reg_default_osd_flag             //unsigned ,RW, default = 5'h18
//====================vd blnd====================
#define VPP_VD_BLEND_CTRL                          0x3246
//Bit 31:28  reserved
//Bit 27:20  hold_lines                        //unsigned ,RW, default = 4
//Bit 19:2   reserved
//Bit 1 :0   gclk_ctrl                         //unsigned ,RW, default = 0
#define VPP_VD_BLEND_DUMMY_DATA                    0x3247
//Bit 31:24  reserved
//Bit 23:16  blend0_dummy_data_y               //unsigned ,RW, default = 8'h00
//Bit 15:8   blend0_dummy_data_cb              //unsigned ,RW, default = 8'h80
//Bit 7 :0   blend0_dummy_data_cr              //unsigned ,RW, default = 8'h80
#define VPP_VD_BLEND_DUMMY_ALPHA                   0x3248
//Bit 31:25  reserved
//Bit 24:16  blend1_dummy_alpha                //unsigned ,RW, default = 9'h0
//Bit 15:9   reserved
//Bit 8 :0   blend0_dummy_alpha                //unsigned ,RW, default = 9'h0
#define VPP_VD_BLEND_RO_CURRENT_XY                 0x3249
//Bit 31:0   ro_blend_current_xy               //unsigned ,RO, default = 32'h0
#define VPP_VD_BLEND_DUMMY_ALPHA1                  0x324a
//Bit 31:25  reserved
//Bit 24:16  blend3_dummy_alpha                //unsigned ,RW, default = 9'h0
//Bit 15:9   reserved
//Bit 8 :0   blend2_dummy_alpha                //unsigned ,RW, default = 9'h0
#define VPP_VD_BLEND_OSD_FLAG_CTRL                 0x324b
//Bit 31:23  reg_osd_flag_thd
//Bit 22:16  reserved                         //unsigned ,RW, default = 9'h80
//Bit 15:8   reg_osd_flag_premult_mask        //unsigned ,RW, default = 8'h01
//Bit 7 :5   reserved
//Bit 4 :0   reg_default_osd_flag             //unsigned ,RW, default = 5'h18
//
// Closing file:  ./vpp_vd_sys_top_regs.h
//
//===========================================================================
// -----------------------------------------------
// REG_BASE:  S5_VPP_POST_UNIT0_VCBUS_BASE = 0x25
// -----------------------------------------------
//===========================================================================
//`include "disp_if_regs.h"
//
// Reading file:  ./vpp_post_unit_reg.h
//
//===========================================================================
// Video postprocessing Registers
//===========================================================================
//====================top====================
#define S5_VPP_GCLK_CTRL                              0x2500
//Bit 31:8   reserved
//Bit 7:6    align_fifo_gclk_ctrl//unsigned, RW, default = 0
//Bit 5:4    ofifo_gclk_ctrl     //unsigned, RW, default = 0
//Bit 3:2    top_gclk_ctrl       //unsigned, RW, default = 0
//Bit 1      reg_gclk_ctrl       //unsigned, RW, default = 0, reg_gclk_ctrl high bit, low bit is 0
//Bit 0      reserved
#define S5_VPP_DOLBY_CTRL                             0x2501
//Bit 31:11  reserved
//Bit 10     vpp_clip_ext_mode   //unsigned, RW, default = 0
//Bit 3      vpp_dolby3_en       //unsigned, RW, default = 0
//Bit 2      vpp_dpath_sel       //unsigned, RW, default = 0
//Bit 1:0    reserved
#define S5_VPP_SYNC_SEL0                              0x2502
//Bit 31:11  reserved
//Bit 10     vpp_clip_ext_mode_sync_sel   //unsigned, RW, default = 0
//Bit 3      vpp_dolby3_en_sync_sel       //unsigned, RW, default = 0
//Bit 2      vpp_dpath_sel_sync_sel       //unsigned, RW, default = 0
//Bit 1:0    reserved
#define S5_VPP_OUT_H_V_SIZE                           0x2503
//Bit 31:29  reserved
//Bit 28:16  vpp_post_hsize       //unsigned, RW, default = 1920
//Bit 15:13  reserved
//Bit 12:0   vpp_post_vsize       //unsigned, RW, default = 1080
#define S5_VPP_OFIFO_SIZE                             0x2504
//Bit 31:20 ofifo_line_len       //unsigned, RW, default = 0
//Bit 19:14 reserved
//Bit 13:0  ofifo_size           //unsigned, RW, default = 4096
#define S5_VPP_OFIFO_URG_CTRL                         0x2505
//Bit 31:16  reg_urg_ofifo_hold_ctrl  //unsigned, RW, default = 0
//Bit 15:0   reg_ofifo_urg_ctrl  //unsigned, RW, default = 0
#define S5_VPP_FIFO_STATUS                            0x2506
//Bit 31:13  reserved
//Bit 12:0   vpp_fifo_status    //unsigned, RO, default = 0, 12:0 ofifo_buf_count
#define S5_VPP_CLIP_MISC0                             0x2507
//Bit 29:20  reg_clip_top_r     //unsigned, RW, default = 1023, final clip r channel top
//Bit 19:10  reg_clip_top_g     //unsigned, RW, default = 1023, final clip g channel top
//Bit  9: 0  reg_clip_top_b     //unsigned, RW, default = 1023, final clip b channel top
#define S5_VPP_CLIP_MISC1                             0x2508
//Bit 29:20  reg_clip_bottom_r  //unsigned, RW, default = 0, final clip r channel bottom
//Bit 19:10  reg_clip_bottom_g  //unsigned, RW, default = 0, final clip g channel bottom
//Bit  9: 0  reg_clip_bottom_b  //unsigned, RW, default = 0, final clip b channel bottom
#define S5_VPP_MISC                                   0x2509
//Bit 31:4   reserved
//Bit 3:2    wm_out_bits_sel    //unsigned, RW, default = 0
//Bit 1      wm_inp_bits_sel    //unsigned, RW, default = 0
//Bit 0      gamma_path_sel     //unsigned, RW, default = 0
#define S5_VPP_CRC_CHK                                0x250a
//Bit 31     crc_man_start      //unsigned, RW, default = 0
//Bit 30:2   reserved
//Bit 1      crc_man_enable     //unsigned, RW, default = 0
//Bit 0      crc_enable         //unsigned, RW, default = 0
#define S5_VPP_RO_CRCSUM                              0x250b
//Bit 31:0   ro_vpp_crc         //unsigned, RW, default = 0
#define S5_VPP_SLC_DEAL_CTRL                          0x250c
//Bit 31:4   reserved
//Bit 3      slc_deal_hwin_en   //unsigned, RW, default = 0
//Bit 2:0    slc_deal_out_swap  //unsigned, RW, default = 0
#define S5_VPP_HWIN_SIZE                              0x250d
//Bit 31:29  reserved
//Bit 28:16  slc_deal_hwin_end_h               //unsigned, RW, default = 1919
//Bit 15:13  reserved
//Bit 12:0   slc_deal_hwin_bgn_h               //unsigned, RW, default = 0
#define S5_VPP_ALIGN_FIFO_SIZE                        0x250e
//Bit 31:20 align_fifo_line_len       //unsigned, RW, default = 0
//Bit 19:14 reserved
//Bit 13:0  align_fifo_size           //unsigned, RW, default = 2048
//====================gamma====================
#define S5_VPP_GAMMA_CTRL                             0x2530
#define S5_VPP_GAMMA_BIN_ADDR                         0x2531
#define S5_VPP_GAMMA_BIN_DATA                         0x2532
//====================lut3d====================
#define S5_VPP_LUT3D_CTRL                             0x2540
#define S5_VPP_LUT3D_CBUS2RAM_CTRL                    0x2541
#define S5_VPP_LUT3D_RAM_ADDR                         0x2542
#define S5_VPP_LUT3D_RAM_DATA                         0x2543
//====================gainoff====================
#define S5_VPP_GAINOFF_CTRL0                          0x2550
#define S5_VPP_GAINOFF_CTRL1                          0x2551
#define S5_VPP_GAINOFF_CTRL2                          0x2552
#define S5_VPP_GAINOFF_CTRL3                          0x2553
#define S5_VPP_GAINOFF_CTRL4                          0x2554
#define S5_VPP_GAINOFF_GCLK_CTRL                      0x2555
//====================csc====================
#define S5_VPP_POST2_MATRIX_COEF00_01                 0x2560
//Bit 31:29 reserved
//Bit 28:16 coef00                      //signed , default = 0
//Bit 15:13 reserved
//Bit 12:0  coef01                      //signed , default = 0
#define S5_VPP_POST2_MATRIX_COEF02_10                 0x2561
//Bit 31:29 reserved
//Bit 28:16 coef02                      //signed , default = 0
//Bit 15:13 reserved
//Bit 12:0  coef10                      //signed , default = 0
#define S5_VPP_POST2_MATRIX_COEF11_12                 0x2562
//Bit 31:29 reserved
//Bit 28:16 coef11                      //signed , default = 0
//Bit 15:13 reserved
//Bit 12:0  coef12                      //signed , default = 0
#define S5_VPP_POST2_MATRIX_COEF20_21                 0x2563
//Bit 31:29 reserved
//Bit 28:16 coef20                      //signed , default = 0
//Bit 15:13 reserved
//Bit 12:0  coef21                      //signed , default = 0
#define S5_VPP_POST2_MATRIX_COEF22                    0x2564
//Bit 31:13 reserved
//Bit 12:0  coef22                      //signed , default = 0
#define S5_VPP_POST2_MATRIX_COEF13_14                 0x2565
//Bit 31:29 reserved
//Bit 28:16 coef13                      //signed , default = 0
//Bit 15:13 reserved
//Bit 12:0  coef14                      //signed , default = 0
#define S5_VPP_POST2_MATRIX_COEF23_24                 0x2566
//Bit 31:29 reserved
//Bit 28:16 coef23                      //signed , default = 0
//Bit 15:13 reserved
//Bit 12:0  coef24                      //signed , default = 0
#define S5_VPP_POST2_MATRIX_COEF15_25                 0x2567
//Bit 31:29 reserved
//Bit 28:16 coef15                      //signed , default = 0
//Bit 15:13 reserved
//Bit 12:0  coef25                      //signed , default = 0
#define S5_VPP_POST2_MATRIX_CLIP                      0x2568
//Bit 31:22  reserved
//Bit 21:8   comp_thrd0                 //  signed ,default == 0,   mat clip enable
//Bit 7:5    conv_rs                    //  unsigned ,default == 0,   mat rs
//Bit 4:3    clmod                      //  unsigned ,default == 0,   mat clmod
#define S5_VPP_POST2_MATRIX_OFFSET0_1                 0x2569
//Bit 31:29 reserved
//Bit 28:16 offset0                     //signed , default = 0
//Bit 15:13 reserved
//Bit 12:0  offset1                     //signed , default = 0
#define S5_VPP_POST2_MATRIX_OFFSET2                   0x256a
//Bit 31:13 reserved
//Bit 12:0  offset2                     //signed , default = 0
#define S5_VPP_POST2_MATRIX_PRE_OFFSET0_1             0x256b
//Bit 31:29 reserved
//Bit 28:16 pre_offset0                 //signed , default = 0
//Bit 15:13 reserved
//Bit 12:0  pre_offset1                 //signed , default = 0
#define S5_VPP_POST2_MATRIX_PRE_OFFSET2               0x256c
//Bit 31:13 reserved
//Bit 12:0  pre_offset2  //signed , default = 0
#define S5_VPP_POST2_MATRIX_EN_CTRL                   0x256d

//===========================================================================
// -----------------------------------------------
// REG_BASE:  VPP_INTF_AFBCD01_VCBUS_BASE = 0x43
// -----------------------------------------------
//===========================================================================
//`include "afbcd_mult_regs.h"
//
// Reading file:  ./afbcd_vd12_regs.h
//
// synopsys translate_off
// synopsys translate_on
////===============================////
//   reg addr map
//   0 -3f :  rdmif
//   40-6f :  afbcd
//   70-7f :  fgrain
////===============================////
#define S5_VD1_IF0_GEN_REG                            0x4300
#define S5_VD1_IF0_CANVAS0                            0x4301
#define S5_VD1_IF0_CANVAS1                            0x4302
#define S5_VD1_IF0_LUMA_X0                            0x4303
#define S5_VD1_IF0_LUMA_Y0                            0x4304
#define S5_VD1_IF0_CHROMA_X0                          0x4305
#define S5_VD1_IF0_CHROMA_Y0                          0x4306
#define S5_VD1_IF0_LUMA_X1                            0x4307
#define S5_VD1_IF0_LUMA_Y1                            0x4308
#define S5_VD1_IF0_CHROMA_X1                          0x4309
#define S5_VD1_IF0_CHROMA_Y1                          0x430a
#define S5_VD1_IF0_RPT_LOOP                           0x430b
#define S5_VD1_IF0_LUMA0_RPT_PAT                      0x430c
#define S5_VD1_IF0_CHROMA0_RPT_PAT                    0x430d
#define S5_VD1_IF0_LUMA1_RPT_PAT                      0x430e
#define S5_VD1_IF0_CHROMA1_RPT_PAT                    0x430f
#define S5_VD1_IF0_LUMA_PSEL                          0x4310
#define S5_VD1_IF0_CHROMA_PSEL                        0x4311
#define S5_VD1_IF0_DUMMY_PIXEL                        0x4312
#define S5_VD1_IF0_LUMA_FIFO_SIZE                     0x4313
#define S5_VD1_IF0_AXI_CMD_CNT                        0x4314
#define S5_VD1_IF0_AXI_RDAT_CNT                       0x4315
#define S5_VD1_IF0_RANGE_MAP_Y                        0x4316
#define S5_VD1_IF0_RANGE_MAP_CB                       0x4317
#define S5_VD1_IF0_RANGE_MAP_CR                       0x4318
#define S5_VD1_IF0_GEN_REG2                           0x4319
#define S5_VD1_IF0_PROT                               0x431a
#define S5_VD1_IF0_URGENT_CTRL                        0x431b
#define S5_VD1_IF0_GEN_REG3                           0x431c
#define VIU_S5_VD1_FMT_CTRL                           0x431d
#define VIU_S5_VD1_FMT_W                              0x431e
#define S5_VD1_IF0_BADDR_Y                            0x4320
#define S5_VD1_IF0_BADDR_CB                           0x4321
#define S5_VD1_IF0_BADDR_CR                           0x4322
#define S5_VD1_IF0_STRIDE_0                           0x4323
#define S5_VD1_IF0_STRIDE_1                           0x4324
#define S5_VD1_IF0_BADDR_Y_F1                         0x4325
#define S5_VD1_IF0_BADDR_CB_F1                        0x4326
#define S5_VD1_IF0_BADDR_CR_F1                        0x4327
#define S5_VD1_IF0_STRIDE_0_F1                        0x4328
#define S5_VD1_IF0_STRIDE_1_F1                        0x4329
//8'h28-8'h6f   for vd1_afbcd
//==========================================================================
// VD_TOP
//==========================================================================
#define S5_VD1_AFBCDM_VDTOP_CTRL0                         0x4338
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512
//==========================================================================
// AFBC_DEC
//==========================================================================
#define S5_VD1_AFBCDM_ENABLE                              0x4340
//Bit   31:29     reserved
//Bit   28:23     reg_gclk_ctrl_core     unsigned, default = 0
//Bit   22        reg_fmt_size_sw_mode   unsigned, default = 0, 0:hw mode 1:sw mode for format size
//Bit   21        reg_addr_link_en  unsigned, default = 1, 1:enable
//Bit   20        reg_fmt444_comb   unsigned, default = 0, 0: 444 8bit uncomb
//Bit   19        reg_dos_uncomp_mode   unsigned  , default = 0
//Bit   18:16     soft_rst          unsigned  , default = 4
//Bit   15:14     reserved
//Bit   13:12     ddr_blk_size      unsigned  , default = 1
//Bit   11:9      cmd_blk_size      unsigned  , default = 3
//Bit   8         dec_enable        unsigned  , default = 0
//Bit   7:2       reserved
//Bit   1         head_len_sel      unsigned  , default = 1
//Bit   0         reserved          unsigned  , pulse dec_frm_start
#define S5_VD1_AFBCDM_MODE                                0x4341
//Bit   31:30     reserved
//Bit   29        ddr_sz_mode       unsigned, default = 0 , 0: fixed block ddr size 1 : unfixed block ddr size;
//Bit   28        blk_mem_mode      unsigned, default = 0 , 0: fixed 16x128 size; 1 : fixed 12x128 size
//Bit   27:26     rev_mode          unsigned, default = 0 , reverse mode
//Bit   25:24     mif_urgent        unsigned, default = 3 , info mif and data mif urgent
//Bit   23        reserved
//Bit   22:16     hold_line_num     unsigned, default = 0 ,
//Bit   15:14     burst_len         unsigned, default = 2, 0: burst1 1:burst2 2:burst4
//Bit   13:8      compbits_yuv      unsigned, default = 0 ,
//                                  bit 1:0,: y  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 3:2,: u  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 5:4,: v  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//Bit   7:6       vert_skip_y       unsigned, default = 0 , luma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   5:4       horz_skip_y       unsigned, default = 0 , luma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   3:2       vert_skip_uv      unsigned, default = 0 , chroma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   1:0       horz_skip_uv      unsigned, default = 0 , chroma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
#define S5_VD1_AFBCDM_SIZE_IN                             0x4342
//Bit   31:29     reserved
//Bit   28:16     hsize_in          unsigned, default = 1920 , pic horz size in  unit: pixel
//Bit   15:13     reserved
//Bit   12:0      vsize_in          unsigned, default = 1080 , pic vert size in  unit: pixel
#define S5_VD1_AFBCDM_DEC_DEF_COLOR                       0x4343
//Bit   31:30    reserved
//Bit   29:20    def_color_y        unsigned, default = 255, afbc dec y default setting value
//Bit   19:10    def_color_u        unsigned, default = 128, afbc dec u default setting value
//Bit    9: 0    def_color_v        unsigned, default = 128, afbc dec v default setting value
#define S5_VD1_AFBCDM_CONV_CTRL                           0x4344
//Bit   31:14    reserved
//Bit   13:12    fmt_mode            unsigned, default = 2, 0:yuv444 1:yuv422 2:yuv420
//Bit   11: 0    conv_lbuf_len       unsigned, default = 256, unit=16 pixel need to set = 2^n
#define S5_VD1_AFBCDM_LBUF_DEPTH                          0x4345
//Bit   31:28    reserved
//Bit   27:16    dec_lbuf_depth      unsigned, default = 128; // unit= 8 pixel
//Bit   15:12    reserved
//Bit   11:0     mif_lbuf_depth      unsigned, default = 128;
#define S5_VD1_AFBCDM_HEAD_BADDR                          0x4346
//Bit   31:0    mif_info_baddr      unsigned, default = 32'h0;
#define S5_VD1_AFBCDM_BODY_BADDR                          0x4347
//Bit   31:0    mif_data_baddr      unsigned, default = 32'h00010000;
#define S5_VD1_AFBCDM_SIZE_OUT                            0x4348
//Bit   31:29    reserved
//Bit   28:16    hsize_out           unsigned, default = 1920 ; // unit: 1 pixel
//Bit   15:13    reserved
//Bit    12:0    vsize_out           unsigned, default = 1080 ; // unit: 1 pixel
#define S5_VD1_AFBCDM_OUT_YSCOPE                          0x4349
//Bit   31:29    reserved
//Bit   28:16    out_vert_bgn        unsigned, default = 0 ; // unit: 1 pixel
//Bit   15:13    reserved
//Bit    12:0    out_vert_end        unsigned, default = 1079 ; // unit: 1 pixel
#define S5_VD1_AFBCDM_STAT                                0x434a
//Bit   31:18   ro_dbg_axi_cnt_d8    unsigned,RO, default = 0
//Bit   17:16   ro_dbg_axi_idel      unsigned,RO, default = 0
//Bit   15:14   reserved
//Bit   13:8    ro_dbg_frm_cnt       unsigned,RO, default = 0
//Bit   7 :1    ro_dbg_go_line_cnt   unsigned,RO, default = 0
//Bit      0    frm_end_stat         unsigned, default = 0 frame end status
#define S5_VD1_AFBCDM_VD_CFMT_CTRL                        0x434b
//Bit 31    cfmt_gclk_bit_dis      unsigned, default = 0 ; //  it true, disable clock, otherwise enable clock
//Bit 30    cfmt_soft_rst_bit      unsigned, default = 0 ; //  soft rst bit
//Bit 29    reserved
//Bit 28    chfmt_rpt_pix          unsigned, default = 0 ; //  if true, horizontal formatter use repeating to generate pixel, otherwise use bilinear interpolation
//Bit 27:24 chfmt_ini_phase        unsigned, default = 0 ; //  horizontal formatter initial phase
//Bit 23    chfmt_rpt_p0_en        unsigned, default = 0 ; //  horizontal formatter repeat pixel 0 enable
//Bit 22:21 chfmt_yc_ratio         unsigned, default = 0 ; //  horizontal Y/C ratio, 00: 1:1, 01: 2:1, 10: 4:1
//Bit 20    chfmt_en               unsigned, default = 0 ; //  horizontal formatter enable
//Bit 19    cvfmt_phase0_always_en unsigned, default = 0 ; //if true, always use phase0 while vertical formater, meaning always //repeat data, no interpolation
//Bit 18    cvfmt_rpt_last_dis     unsigned, default = 0 ; //if true, disable vertical formatter chroma repeat last line
//Bit 17    cvfmt_phase0_nrpt_en   unsigned, default = 0 ; //vertical formatter dont need repeat line on phase0, 1: enable, 0: disable
//Bit 16    cvfmt_rpt_line0_en     unsigned, default = 0 ; //vertical formatter repeat line 0 enable
//Bit 15:12 cvfmt_skip_line_num    unsigned, default = 0 ; //vertical formatter skip line num at the beginning
//Bit 11:8  cvfmt_ini_phase        unsigned, default = 0 ; //vertical formatter initial phase
//Bit 7:1   cvfmt_phase_step       unsigned, default = 0 ; //vertical formatter phase step (3.4)
//Bit 0     cvfmt_en               unsigned, default = 0 ; //vertical formatter enable
#define S5_VD1_AFBCDM_VD_CFMT_W                           0x434c
//Bit 31:29 reserved
//Bit 28:16 chfmt_w                unsigned, default = 0 ;horizontal formatter width
//Bit 15:13 reserved
//Bit 12:0  cvfmt_w                unsigned, default = 0 ;vertical formatter width
#define S5_VD1_AFBCDM_MIF_HOR_SCOPE                       0x434d
//Bit   31:26   reserved
//Bit   25:16   mif_blk_bgn_h        unsigned, default = 0  ; // unit: 32 pixel/block hor
//Bit   15:10   reserved
//Bit    9: 0   mif_blk_end_h        unsigned, default = 59 ; // unit: 32 pixel/block hor
#define S5_VD1_AFBCDM_MIF_VER_SCOPE                       0x434e
//Bit   31:28   reserved
//Bit   27:16   mif_blk_bgn_v          unsigned, default = 0  ; // unit: 32 pixel/block ver
//Bit   15:12   reserved
//Bit   11: 0   mif_blk_end_v          unsigned, default = 269; // unit: 32 pixel/block ver
#define S5_VD1_AFBCDM_PIXEL_HOR_SCOPE                     0x434f
//Bit   31:29   reserved
//Bit   28:16   dec_pixel_bgn_h        unsigned, default = 0  ; // unit: pixel
//Bit   15:13   reserved
//Bit   12: 0   dec_pixel_end_h        unsigned, default = 1919 ; // unit: pixel
#define S5_VD1_AFBCDM_PIXEL_VER_SCOPE                     0x4350
//Bit   31:29   reserved
//Bit   28:16   dec_pixel_bgn_v        unsigned, default = 0  ; // unit: pixel
//Bit   15:13   reserved
//Bit   12: 0   dec_pixel_end_v        unsigned, default = 1079 ; // unit: pixel
#define S5_VD1_AFBCDM_VD_CFMT_H                           0x4351
//Bit 31:13     reserved
//Bit 12:0      cfmt_h                 unsigned, default = 142  ; //vertical formatter height
#define S5_VD1_AFBCDM_IQUANT_ENABLE                       0x4352
//Bit 31:12        reserved
//Bit  11          reg_quant_expand_en_1     //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  10          reg_quant_expand_en_0     //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  9: 8        reg_bcleav_ofst           //signed ,       RW, default = 0  bcleave ofset to get lower range, especially under lossy, for v1/v2, x=0 is equivalent, default = -1;
//Bit  7: 5        reserved
//Bit  4           reg_quant_enable_1        // unsigned ,    RW, default = 0  enable for quant to get some lossy
//Bit  3: 1        reserved
//Bit  0           reg_quant_enable_0        // unsigned ,    RW, default = 0  enable for quant to get some lossy
#define S5_VD1_AFBCDM_IQUANT_LUT_1                        0x4353
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_0_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_0_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_0_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_0_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD1_AFBCDM_IQUANT_LUT_2                        0x4354
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD1_AFBCDM_IQUANT_LUT_3                        0x4355
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_1_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_1_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_1_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_1_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD1_AFBCDM_IQUANT_LUT_4                        0x4356
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD1_AFBCDM_ROT_CTRL                            0x4360
//Bit   31:30   reg_rot_ohds2_mode                  //unsigned, RW, default = 0 , rot output format down hor drop mode,0:average 1:use 0 2:use 1
//Bit   29:28   reg_rot_ovds2_mode                  //unsigned, RW, default = 0 , rot output format down ver drop mode,0:average 1:use 0 2:use 1
//Bit   27      reg_pip_mode                        //unsigned, RW, default = 0 , 0:dec_src from vdin/dos  1:dec_src from pip
//Bit   26:24   reg_rot_uv_vshrk_drop_mode          //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   23      reserved
//Bit   22:20   reg_rot_uv_hshrk_drop_mode          //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   19:18   reg_rot_uv_vshrk_ratio              //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   17:16   reg_rot_uv_hshrk_ratio              //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   15      reserved
//Bit   14:12   reg_rot_y_vshrk_drop_mode           //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   11      reserved
//Bit   10:8    reg_rot_y_hshrk_drop_mode           //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   7:6     reg_rot_y_vshrk_ratio               //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   5:4     reg_rot_y_hshrk_ratio               //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   3:2     reg_rot_uv422_drop_mode             //unsigned, RW, default = 0 , 0:average 1:left 2:right
//Bit   1       reg_rot_uv422_omode                 //unsigned, RW, default = 0 , when rot input fmt422, 0:output_uv422  1:output_uv420
//Bit   0       reg_rot_enable                      //unsigned, RW, default = 0 , rotation enable
#define S5_VD1_AFBCDM_ROT_SCOPE                           0x4361
//Bit   31:26   reserved
//Bit   25:20   reg_rot_debug_probe      //unsigned, RW , default = 0, y:[2:0] uv:[5:3]; 0:iblk_size 1:oblk_size 2:iblk_cnt 3:oblk_cnt 4:hsize_in 5:vsize_in 6:vstep
//Bit   19      reg_rot_dout_ds_mode_sw  //unsigned, RW , default = 0, 0:use hardware mode 1:use software mode
//Bit   18:17   reg_rot_dout_ds_mode     //unsigned, RW , default = 0, rot output fmt_down mode: [0]:h_downscale [1]:v_downscale
//Bit   16      reg_rot_ifmt_force444    //unsigned, RW , default = 1, 1: rot input fmt force as 444
//Bit   15:14   reg_rot_ofmt_mode        //unsigned, RW , default = 0, rot output fmt mode
//Bit   13:12   reg_rot_compbits_out_y   //unsigned, RW , default = 0, rot output compbit y
//Bit   11:10   reg_rot_compbits_out_uv  //unsigned, RW , default = 0, rot output compbit uv
//Bit   9:8     reg_rot_wrbgn_v          //unsigned, RW , default = 0, rot pic vert size window begin pixel
//Bit   7:5     reserved
//Bit   4:0     reg_rot_wrbgn_h          //unsigned, RW , default = 0, rot pic hori size window begin pixel
#define S5_VD1_AFBCDM_RPLC_CTRL                           0x4362
//Bit   31        reg_rplc_inter_corr_en //unsigned, RW , default = 0   , //pip replace inte-frame edge correct enable
//Bit   30        reg_rplc_dummy_corr_en //unsigned, RW , default = 0   , //pip replace outsize of real-pipframe edge correct enable
//Bit   29        reg_rplc_byps          //unsigned, RW , default = 1   , //pip replace module bypass
//Bit   28:16     reg_rplc_vsize_in      //unsigned, RW , default = 1080, //
//Bit   15:13     reserved
//Bit   12:0      reg_rplc_hsize_in      //unsigned, RW , default = 1920,
#define S5_VD1_AFBCDM_RPLC_PICEN                          0x4363
//Bit  31:28      reserved
//Bit  27:16      reg_rplc_def_color_y    //unsigned, RW , default =0        , //pip replace def_color_y
//Bit  15:0       reg_rplc_pic_enable     //unsigned, RW , default =16'hffff , //pip replace pip_picure enbale
#define S5_VD1_AFBCDM_RPLC_DEFCOL                         0x4364
//Bit  31:24     reserved
//Bit  23:12     reg_rplc_def_color_v    //unsigned, RW , default =0        , //pip replace def_color_v
//Bit  11:0      reg_rplc_def_color_u    //unsigned, RW , default =0        , //pip replace def_color_u
#define S5_VD1_AFBCDM_RPLC_SCPXN_ADDR                     0x4365
//Bit  31:5      reserved
//Bit  4:0       reg_rplc_scpxn_addr     //unsigned, RW , default =0        , //pip replace scopx16 addr
#define S5_VD1_AFBCDM_RPLC_SCPXN_DATA                     0x4366
//Bit  31:26     reserved
//Bit  25:0      reg_rplc_scpxn_data     //unsigned, RW , default =0        , //pip replace scopx16 data
#define S5_VD1_AFBCDM_ROT_RO_STAT                         0x4367
//Bit   31:0     ro_rot_debug           //unsigned, RO , default = 0, rot some status

//8'h70-8'h7f   for vd1_fgrain
//
// Reading file:  ./vd1s0_fgrain_regs.h
//
#define S5_VD1_FGRAIN_CTRL                            0x4370
//Bit 31:19      reserved
//Bit 18,        reg_dma_st_clr,            .usigned  , clear DMA error status
//Bit 17,        reg_hold4dma_scale,        .usigned  , wait DMA scale data ready before accept input data        default = 0
//Bit 16,        reg_hold4dma_tbl,          .usigned  , wait DMA grain table data ready before accept input data  default = 0
//Bit 15,        reserved
//Bit 14,        reg_cin_uv_swap,           .unsigned , swap uv input data order. default = 0
//Bit 13,        reg_cin_rev,               .unsigned , reverse c order.  default = 0
//Bit 12,        reg_yin_rev,               .unsigned , reverse y orderdefault = 0
//Bit 11,        reserved
//Bit 10,        reg_fgrain_use_sat4bp      .unsigned , only for debug. default = 0
//Bit  9,        reg_fgrain_tbl_ext_mode    .unsigned , default = 1
//Bit  8,        reg_fgrain_ext_imode       .unsigned , default = 1
//Bit  7: 6      reg_fmt_mode               .unsigned , default = 2, 0:444; 1:422; 2:420; 3:reserved
//Bit  5: 4      reg_comp_bits              .unsigned , default = 1, 0:8bits; 1:10bits, else 12 bits
//Bit  3: 2      reg_rev_mode               .unsigned . default = 0, 0:h_rev; 1:v_rev;
//Bit  1,        reg_block_mode             .unsigned , default = 1
//Bit  0,        reg_fgrain_en              .unsigned , default = 0
#define S5_VD1_FGRAIN_WIN_H                           0x4371
//Bit  31:16     reg_win_end_h      .unsigned , default = 3812
//Bit  15: 0,    reg_win_bgn_h      .unsigned , default = 0
#define S5_VD1_FGRAIN_WIN_V                           0x4372
//Bit  31:16     reg_win_end_v      .unsigned , default = 2156
//Bit  15: 0,    reg_win_bgn_v      .unsigned , default = 0
#define S5_VD1_FGRAIN_SW_Y_RANNGE                     0x4373
//Bit 31,        reg_fgrain_sw_yrange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_ymax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_ymin        .unsigned , default = 0
#define S5_VD1_FGRAIN_SW_C_RANNGE                     0x4374
//Bit 31,        reg_fgrain_sw_crange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_cmax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_cmin        .unsigned , default = 0
#define S5_VD1_FGRAIN_GCLK_CTRL_0                     0x4375
//Bit 31:0,      reg_fgrain_gclk_ctrl0  .unsigned , default = 0
#define S5_VD1_FGRAIN_GCLK_CTRL_1                     0x4376
//Bit 31:0,      reg_fgrain_gclk_ctrl1  .unsigned , default = 0
#define S5_VD1_FGRAIN_GCLK_CTRL_2                     0x4377
//Bit 31:0,      reg_fgrain_gclk_ctrl2  .unsigned , default = 0
#define S5_VD1_FGRAIN_PARAM_ADDR                      0x4378
#define S5_VD1_FGRAIN_PARAM_DATA                      0x4379
#define S5_VD1_FGRAIN_SLICE_WIN_H                     0x437a
//
// Closing file:  ./vd1s0_fgrain_regs.h
//
#define S5_VD2_IF0_GEN_REG                            0x4380
#define S5_VD2_IF0_CANVAS0                            0x4381
#define S5_VD2_IF0_CANVAS1                            0x4382
#define S5_VD2_IF0_LUMA_X0                            0x4383
#define S5_VD2_IF0_LUMA_Y0                            0x4384
#define S5_VD2_IF0_CHROMA_X0                          0x4385
#define S5_VD2_IF0_CHROMA_Y0                          0x4386
#define S5_VD2_IF0_LUMA_X1                            0x4387
#define S5_VD2_IF0_LUMA_Y1                            0x4388
#define S5_VD2_IF0_CHROMA_X1                          0x4389
#define S5_VD2_IF0_CHROMA_Y1                          0x438a
#define S5_VD2_IF0_RPT_LOOP                           0x438b
#define S5_VD2_IF0_LUMA0_RPT_PAT                      0x438c
#define S5_VD2_IF0_CHROMA0_RPT_PAT                    0x438d
#define S5_VD2_IF0_LUMA1_RPT_PAT                      0x438e
#define S5_VD2_IF0_CHROMA1_RPT_PAT                    0x438f
#define S5_VD2_IF0_LUMA_PSEL                          0x4390
#define S5_VD2_IF0_CHROMA_PSEL                        0x4391
#define S5_VD2_IF0_DUMMY_PIXEL                        0x4392
#define S5_VD2_IF0_LUMA_FIFO_SIZE                     0x4393
#define S5_VD2_IF0_AXI_CMD_CNT                        0x4394
#define S5_VD2_IF0_AXI_RDAT_CNT                       0x4395
#define S5_VD2_IF0_RANGE_MAP_Y                        0x4396
#define S5_VD2_IF0_RANGE_MAP_CB                       0x4397
#define S5_VD2_IF0_RANGE_MAP_CR                       0x4398
#define S5_VD2_IF0_GEN_REG2                           0x4399
#define S5_VD2_IF0_PROT                               0x439a
#define S5_VD2_IF0_URGENT_CTRL                        0x439b
#define S5_VD2_IF0_GEN_REG3                           0x439c
#define VIU_S5_VD2_FMT_CTRL                           0x439d
#define VIU_S5_VD2_FMT_W                              0x439e
#define S5_VD2_IF0_BADDR_Y                            0x43a0
#define S5_VD2_IF0_BADDR_CB                           0x43a1
#define S5_VD2_IF0_BADDR_CR                           0x43a2
#define S5_VD2_IF0_STRIDE_0                           0x43a3
#define S5_VD2_IF0_STRIDE_1                           0x43a4
#define S5_VD2_IF0_BADDR_Y_F1                         0x43a5
#define S5_VD2_IF0_BADDR_CB_F1                        0x43a6
#define S5_VD2_IF0_BADDR_CR_F1                        0x43a7
#define S5_VD2_IF0_STRIDE_0_F1                        0x43a8
#define S5_VD2_IF0_STRIDE_1_F1                        0x43a9
//8'hb8-8'hef   for vd2_afbcd

//==========================================================================
// VD_TOP
//==========================================================================
#define S5_VD2_AFBCDM_VDTOP_CTRL0                         0x43b8
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512
//==========================================================================
// AFBC_DEC
//==========================================================================
#define S5_VD2_AFBCDM_ENABLE                              0x43c0
//Bit   31:29     reserved
//Bit   28:23     reg_gclk_ctrl_core     unsigned, default = 0
//Bit   22        reg_fmt_size_sw_mode   unsigned, default = 0, 0:hw mode 1:sw mode for format size
//Bit   21        reg_addr_link_en  unsigned, default = 1, 1:enable
//Bit   20        reg_fmt444_comb   unsigned, default = 0, 0: 444 8bit uncomb
//Bit   19        reg_dos_uncomp_mode   unsigned  , default = 0
//Bit   18:16     soft_rst          unsigned  , default = 4
//Bit   15:14     reserved
//Bit   13:12     ddr_blk_size      unsigned  , default = 1
//Bit   11:9      cmd_blk_size      unsigned  , default = 3
//Bit   8         dec_enable        unsigned  , default = 0
//Bit   7:2       reserved
//Bit   1         head_len_sel      unsigned  , default = 1
//Bit   0         reserved          unsigned  , pulse dec_frm_start
#define S5_VD2_AFBCDM_MODE                                0x43c1
//Bit   31:30     reserved
//Bit   29        ddr_sz_mode       unsigned, default = 0 , 0: fixed block ddr size 1 : unfixed block ddr size;
//Bit   28        blk_mem_mode      unsigned, default = 0 , 0: fixed 16x128 size; 1 : fixed 12x128 size
//Bit   27:26     rev_mode          unsigned, default = 0 , reverse mode
//Bit   25:24     mif_urgent        unsigned, default = 3 , info mif and data mif urgent
//Bit   23        reserved
//Bit   22:16     hold_line_num     unsigned, default = 0 ,
//Bit   15:14     burst_len         unsigned, default = 2, 0: burst1 1:burst2 2:burst4
//Bit   13:8      compbits_yuv      unsigned, default = 0 ,
//                                  bit 1:0,: y  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 3:2,: u  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 5:4,: v  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//Bit   7:6       vert_skip_y       unsigned, default = 0 , luma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   5:4       horz_skip_y       unsigned, default = 0 , luma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   3:2       vert_skip_uv      unsigned, default = 0 , chroma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   1:0       horz_skip_uv      unsigned, default = 0 , chroma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
#define S5_VD2_AFBCDM_SIZE_IN                             0x43c2
//Bit   31:29     reserved
//Bit   28:16     hsize_in          unsigned, default = 1920 , pic horz size in  unit: pixel
//Bit   15:13     reserved
//Bit   12:0      vsize_in          unsigned, default = 1080 , pic vert size in  unit: pixel
#define S5_VD2_AFBCDM_DEC_DEF_COLOR                       0x43c3
//Bit   31:30    reserved
//Bit   29:20    def_color_y        unsigned, default = 255, afbc dec y default setting value
//Bit   19:10    def_color_u        unsigned, default = 128, afbc dec u default setting value
//Bit    9: 0    def_color_v        unsigned, default = 128, afbc dec v default setting value
#define S5_VD2_AFBCDM_CONV_CTRL                           0x43c4
//Bit   31:14    reserved
//Bit   13:12    fmt_mode            unsigned, default = 2, 0:yuv444 1:yuv422 2:yuv420
//Bit   11: 0    conv_lbuf_len       unsigned, default = 256, unit=16 pixel need to set = 2^n
#define S5_VD2_AFBCDM_LBUF_DEPTH                          0x43c5
//Bit   31:28    reserved
//Bit   27:16    dec_lbuf_depth      unsigned, default = 128; // unit= 8 pixel
//Bit   15:12    reserved
//Bit   11:0     mif_lbuf_depth      unsigned, default = 128;
#define S5_VD2_AFBCDM_HEAD_BADDR                          0x43c6
//Bit   31:0    mif_info_baddr      unsigned, default = 32'h0;
#define S5_VD2_AFBCDM_BODY_BADDR                          0x43c7
//Bit   31:0    mif_data_baddr      unsigned, default = 32'h00010000;
#define S5_VD2_AFBCDM_SIZE_OUT                            0x43c8
//Bit   31:29    reserved
//Bit   28:16    hsize_out           unsigned, default = 1920 ; // unit: 1 pixel
//Bit   15:13    reserved
//Bit    12:0    vsize_out           unsigned, default = 1080 ; // unit: 1 pixel
#define S5_VD2_AFBCDM_OUT_YSCOPE                          0x43c9
//Bit   31:29    reserved
//Bit   28:16    out_vert_bgn        unsigned, default = 0 ; // unit: 1 pixel
//Bit   15:13    reserved
//Bit    12:0    out_vert_end        unsigned, default = 1079 ; // unit: 1 pixel
#define S5_VD2_AFBCDM_STAT                                0x43ca
//Bit   31:18   ro_dbg_axi_cnt_d8    unsigned,RO, default = 0
//Bit   17:16   ro_dbg_axi_idel      unsigned,RO, default = 0
//Bit   15:14   reserved
//Bit   13:8    ro_dbg_frm_cnt       unsigned,RO, default = 0
//Bit   7 :1    ro_dbg_go_line_cnt   unsigned,RO, default = 0
//Bit      0    frm_end_stat         unsigned, default = 0 frame end status
#define S5_VD2_AFBCDM_VD_CFMT_CTRL                        0x43cb
//Bit 31    cfmt_gclk_bit_dis      unsigned, default = 0 ; //  it true, disable clock, otherwise enable clock
//Bit 30    cfmt_soft_rst_bit      unsigned, default = 0 ; //  soft rst bit
//Bit 29    reserved
//Bit 28    chfmt_rpt_pix          unsigned, default = 0 ; //  if true, horizontal formatter use repeating to generate pixel, otherwise use bilinear interpolation
//Bit 27:24 chfmt_ini_phase        unsigned, default = 0 ; //  horizontal formatter initial phase
//Bit 23    chfmt_rpt_p0_en        unsigned, default = 0 ; //  horizontal formatter repeat pixel 0 enable
//Bit 22:21 chfmt_yc_ratio         unsigned, default = 0 ; //  horizontal Y/C ratio, 00: 1:1, 01: 2:1, 10: 4:1
//Bit 20    chfmt_en               unsigned, default = 0 ; //  horizontal formatter enable
//Bit 19    cvfmt_phase0_always_en unsigned, default = 0 ; //if true, always use phase0 while vertical formater, meaning always //repeat data, no interpolation
//Bit 18    cvfmt_rpt_last_dis     unsigned, default = 0 ; //if true, disable vertical formatter chroma repeat last line
//Bit 17    cvfmt_phase0_nrpt_en   unsigned, default = 0 ; //vertical formatter dont need repeat line on phase0, 1: enable, 0: disable
//Bit 16    cvfmt_rpt_line0_en     unsigned, default = 0 ; //vertical formatter repeat line 0 enable
//Bit 15:12 cvfmt_skip_line_num    unsigned, default = 0 ; //vertical formatter skip line num at the beginning
//Bit 11:8  cvfmt_ini_phase        unsigned, default = 0 ; //vertical formatter initial phase
//Bit 7:1   cvfmt_phase_step       unsigned, default = 0 ; //vertical formatter phase step (3.4)
//Bit 0     cvfmt_en               unsigned, default = 0 ; //vertical formatter enable
#define S5_VD2_AFBCDM_VD_CFMT_W                           0x43cc
//Bit 31:29 reserved
//Bit 28:16 chfmt_w                unsigned, default = 0 ;horizontal formatter width
//Bit 15:13 reserved
//Bit 12:0  cvfmt_w                unsigned, default = 0 ;vertical formatter width
#define S5_VD2_AFBCDM_MIF_HOR_SCOPE                       0x43cd
//Bit   31:26   reserved
//Bit   25:16   mif_blk_bgn_h        unsigned, default = 0  ; // unit: 32 pixel/block hor
//Bit   15:10   reserved
//Bit    9: 0   mif_blk_end_h        unsigned, default = 59 ; // unit: 32 pixel/block hor
#define S5_VD2_AFBCDM_MIF_VER_SCOPE                       0x43ce
//Bit   31:28   reserved
//Bit   27:16   mif_blk_bgn_v          unsigned, default = 0  ; // unit: 32 pixel/block ver
//Bit   15:12   reserved
//Bit   11: 0   mif_blk_end_v          unsigned, default = 269; // unit: 32 pixel/block ver
#define S5_VD2_AFBCDM_PIXEL_HOR_SCOPE                     0x43cf
//Bit   31:29   reserved
//Bit   28:16   dec_pixel_bgn_h        unsigned, default = 0  ; // unit: pixel
//Bit   15:13   reserved
//Bit   12: 0   dec_pixel_end_h        unsigned, default = 1919 ; // unit: pixel
#define S5_VD2_AFBCDM_PIXEL_VER_SCOPE                     0x43d0
//Bit   31:29   reserved
//Bit   28:16   dec_pixel_bgn_v        unsigned, default = 0  ; // unit: pixel
//Bit   15:13   reserved
//Bit   12: 0   dec_pixel_end_v        unsigned, default = 1079 ; // unit: pixel
#define S5_VD2_AFBCDM_VD_CFMT_H                           0x43d1
//Bit 31:13     reserved
//Bit 12:0      cfmt_h                 unsigned, default = 142  ; //vertical formatter height
#define S5_VD2_AFBCDM_IQUANT_ENABLE                       0x43d2
//Bit 31:12        reserved
//Bit  11          reg_quant_expand_en_1     //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  10          reg_quant_expand_en_0     //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  9: 8        reg_bcleav_ofst           //signed ,       RW, default = 0  bcleave ofset to get lower range, especially under lossy, for v1/v2, x=0 is equivalent, default = -1;
//Bit  7: 5        reserved
//Bit  4           reg_quant_enable_1        // unsigned ,    RW, default = 0  enable for quant to get some lossy
//Bit  3: 1        reserved
//Bit  0           reg_quant_enable_0        // unsigned ,    RW, default = 0  enable for quant to get some lossy
#define S5_VD2_AFBCDM_IQUANT_LUT_1                        0x43d3
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_0_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_0_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_0_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_0_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD2_AFBCDM_IQUANT_LUT_2                        0x43d4
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD2_AFBCDM_IQUANT_LUT_3                        0x43d5
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_1_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_1_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_1_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_1_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD2_AFBCDM_IQUANT_LUT_4                        0x43d6
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD2_AFBCDM_ROT_CTRL                            0x43e0
//Bit   31:30   reg_rot_ohds2_mode                  //unsigned, RW, default = 0 , rot output format down hor drop mode,0:average 1:use 0 2:use 1
//Bit   29:28   reg_rot_ovds2_mode                  //unsigned, RW, default = 0 , rot output format down ver drop mode,0:average 1:use 0 2:use 1
//Bit   27      reg_pip_mode                        //unsigned, RW, default = 0 , 0:dec_src from vdin/dos  1:dec_src from pip
//Bit   26:24   reg_rot_uv_vshrk_drop_mode          //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   23      reserved
//Bit   22:20   reg_rot_uv_hshrk_drop_mode          //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   19:18   reg_rot_uv_vshrk_ratio              //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   17:16   reg_rot_uv_hshrk_ratio              //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   15      reserved
//Bit   14:12   reg_rot_y_vshrk_drop_mode           //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   11      reserved
//Bit   10:8    reg_rot_y_hshrk_drop_mode           //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   7:6     reg_rot_y_vshrk_ratio               //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   5:4     reg_rot_y_hshrk_ratio               //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   3:2     reg_rot_uv422_drop_mode             //unsigned, RW, default = 0 , 0:average 1:left 2:right
//Bit   1       reg_rot_uv422_omode                 //unsigned, RW, default = 0 , when rot input fmt422, 0:output_uv422  1:output_uv420
//Bit   0       reg_rot_enable                      //unsigned, RW, default = 0 , rotation enable
#define S5_VD2_AFBCDM_ROT_SCOPE                           0x43e1
//Bit   31:26   reserved
//Bit   25:20   reg_rot_debug_probe      //unsigned, RW , default = 0, y:[2:0] uv:[5:3]; 0:iblk_size 1:oblk_size 2:iblk_cnt 3:oblk_cnt 4:hsize_in 5:vsize_in 6:vstep
//Bit   19      reg_rot_dout_ds_mode_sw  //unsigned, RW , default = 0, 0:use hardware mode 1:use software mode
//Bit   18:17   reg_rot_dout_ds_mode     //unsigned, RW , default = 0, rot output fmt_down mode: [0]:h_downscale [1]:v_downscale
//Bit   16      reg_rot_ifmt_force444    //unsigned, RW , default = 1, 1: rot input fmt force as 444
//Bit   15:14   reg_rot_ofmt_mode        //unsigned, RW , default = 0, rot output fmt mode
//Bit   13:12   reg_rot_compbits_out_y   //unsigned, RW , default = 0, rot output compbit y
//Bit   11:10   reg_rot_compbits_out_uv  //unsigned, RW , default = 0, rot output compbit uv
//Bit   9:8     reg_rot_wrbgn_v          //unsigned, RW , default = 0, rot pic vert size window begin pixel
//Bit   7:5     reserved
//Bit   4:0     reg_rot_wrbgn_h          //unsigned, RW , default = 0, rot pic hori size window begin pixel
#define S5_VD2_AFBCDM_RPLC_CTRL                           0x43e2
//Bit   31        reg_rplc_inter_corr_en //unsigned, RW , default = 0   , //pip replace inte-frame edge correct enable
//Bit   30        reg_rplc_dummy_corr_en //unsigned, RW , default = 0   , //pip replace outsize of real-pipframe edge correct enable
//Bit   29        reg_rplc_byps          //unsigned, RW , default = 1   , //pip replace module bypass
//Bit   28:16     reg_rplc_vsize_in      //unsigned, RW , default = 1080, //
//Bit   15:13     reserved
//Bit   12:0      reg_rplc_hsize_in      //unsigned, RW , default = 1920,
#define S5_VD2_AFBCDM_RPLC_PICEN                          0x43e3
//Bit  31:28      reserved
//Bit  27:16      reg_rplc_def_color_y    //unsigned, RW , default =0        , //pip replace def_color_y
//Bit  15:0       reg_rplc_pic_enable     //unsigned, RW , default =16'hffff , //pip replace pip_picure enbale
#define S5_VD2_AFBCDM_RPLC_DEFCOL                         0x43e4
//Bit  31:24     reserved
//Bit  23:12     reg_rplc_def_color_v    //unsigned, RW , default =0        , //pip replace def_color_v
//Bit  11:0      reg_rplc_def_color_u    //unsigned, RW , default =0        , //pip replace def_color_u
#define S5_VD2_AFBCDM_RPLC_SCPXN_ADDR                     0x43e5
//Bit  31:5      reserved
//Bit  4:0       reg_rplc_scpxn_addr     //unsigned, RW , default =0        , //pip replace scopx16 addr
#define S5_VD2_AFBCDM_RPLC_SCPXN_DATA                     0x43e6
//Bit  31:26     reserved
//Bit  25:0      reg_rplc_scpxn_data     //unsigned, RW , default =0        , //pip replace scopx16 data
#define S5_VD2_AFBCDM_ROT_RO_STAT                         0x43e7
//Bit   31:0     ro_rot_debug           //unsigned, RO , default = 0, rot some status

//8'hf0-8'hff   for vd2_fgrain
//
// Reading file:  ./vd1s1_fgrain_regs.h
//
#define S5_VD2_FGRAIN_CTRL                            0x43f0
//Bit 31:19      reserved
//Bit 18,        reg_dma_st_clr,            .usigned  , clear DMA error status
//Bit 17,        reg_hold4dma_scale,        .usigned  , wait DMA scale data ready before accept input data        default = 0
//Bit 16,        reg_hold4dma_tbl,          .usigned  , wait DMA grain table data ready before accept input data  default = 0
//Bit 15,        reserved
//Bit 14,        reg_cin_uv_swap,           .unsigned , swap uv input data order. default = 0
//Bit 13,        reg_cin_rev,               .unsigned , reverse c order.  default = 0
//Bit 12,        reg_yin_rev,               .unsigned , reverse y orderdefault = 0
//Bit 11,        reserved
//Bit 10,        reg_fgrain_use_sat4bp      .unsigned , only for debug. default = 0
//Bit  9,        reg_fgrain_tbl_ext_mode    .unsigned , default = 1
//Bit  8,        reg_fgrain_ext_imode       .unsigned , default = 1
//Bit  7: 6      reg_fmt_mode               .unsigned , default = 2, 0:444; 1:422; 2:420; 3:reserved
//Bit  5: 4      reg_comp_bits              .unsigned , default = 1, 0:8bits; 1:10bits, else 12 bits
//Bit  3: 2      reg_rev_mode               .unsigned . default = 0, 0:h_rev; 1:v_rev;
//Bit  1,        reg_block_mode             .unsigned , default = 1
//Bit  0,        reg_fgrain_en              .unsigned , default = 0
#define S5_VD2_FGRAIN_WIN_H                           0x43f1
//Bit  31:16     reg_win_end_h      .unsigned , default = 3812
//Bit  15: 0,    reg_win_bgn_h      .unsigned , default = 0
#define S5_VD2_FGRAIN_WIN_V                           0x43f2
//Bit  31:16     reg_win_end_v      .unsigned , default = 2156
//Bit  15: 0,    reg_win_bgn_v      .unsigned , default = 0
#define S5_VD2_FGRAIN_SW_Y_RANNGE                     0x43f3
//Bit 31,        reg_fgrain_sw_yrange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_ymax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_ymin        .unsigned , default = 0
#define S5_VD2_FGRAIN_SW_C_RANNGE                     0x43f4
//Bit 31,        reg_fgrain_sw_crange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_cmax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_cmin        .unsigned , default = 0
#define S5_VD2_FGRAIN_GCLK_CTRL_0                     0x43f5
//Bit 31:0,      reg_fgrain_gclk_ctrl0  .unsigned , default = 0
#define S5_VD2_FGRAIN_GCLK_CTRL_1                     0x43f6
//Bit 31:0,      reg_fgrain_gclk_ctrl1  .unsigned , default = 0
#define S5_VD2_FGRAIN_GCLK_CTRL_2                     0x43f7
//Bit 31:0,      reg_fgrain_gclk_ctrl2  .unsigned , default = 0
#define S5_VD2_FGRAIN_PARAM_ADDR                      0x43f8
#define S5_VD2_FGRAIN_PARAM_DATA                      0x43f9
#define S5_VD2_FGRAIN_SLICE_WIN_H                     0x43fa
//
// Closing file:  ./vd1s1_fgrain_regs.h
//
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ./afbcd_vd12_regs.h
//
//`include "fgrain_regs.h"      //nouse
//===========================================================================
// -----------------------------------------------
// REG_BASE:  VPP_INTF_AFBCD23_VCBUS_BASE = 0x44
// -----------------------------------------------
//===========================================================================
//`include "afbcd_mult_regs.h"
//
// Reading file:  ./afbcd_vd34_regs.h
//
// synopsys translate_off
// synopsys translate_on
////===============================////
//   reg addr map
//   0 -3f :  rdmif
//   40-6f :  afbcd
//   70-7f :  fgrain
////===============================////
#define S5_VD3_IF0_GEN_REG                            0x4400
#define S5_VD3_IF0_CANVAS0                            0x4401
#define S5_VD3_IF0_CANVAS1                            0x4402
#define S5_VD3_IF0_LUMA_X0                            0x4403
#define S5_VD3_IF0_LUMA_Y0                            0x4404
#define S5_VD3_IF0_CHROMA_X0                          0x4405
#define S5_VD3_IF0_CHROMA_Y0                          0x4406
#define S5_VD3_IF0_LUMA_X1                            0x4407
#define S5_VD3_IF0_LUMA_Y1                            0x4408
#define S5_VD3_IF0_CHROMA_X1                          0x4409
#define S5_VD3_IF0_CHROMA_Y1                          0x440a
#define S5_VD3_IF0_RPT_LOOP                           0x440b
#define S5_VD3_IF0_LUMA0_RPT_PAT                      0x440c
#define S5_VD3_IF0_CHROMA0_RPT_PAT                    0x440d
#define S5_VD3_IF0_LUMA1_RPT_PAT                      0x440e
#define S5_VD3_IF0_CHROMA1_RPT_PAT                    0x440f
#define S5_VD3_IF0_LUMA_PSEL                          0x4410
#define S5_VD3_IF0_CHROMA_PSEL                        0x4411
#define S5_VD3_IF0_DUMMY_PIXEL                        0x4412
#define S5_VD3_IF0_LUMA_FIFO_SIZE                     0x4413
#define S5_VD3_IF0_AXI_CMD_CNT                        0x4414
#define S5_VD3_IF0_AXI_RDAT_CNT                       0x4415
#define S5_VD3_IF0_RANGE_MAP_Y                        0x4416
#define S5_VD3_IF0_RANGE_MAP_CB                       0x4417
#define S5_VD3_IF0_RANGE_MAP_CR                       0x4418
#define S5_VD3_IF0_GEN_REG2                           0x4419
#define S5_VD3_IF0_PROT                               0x441a
#define S5_VD3_IF0_URGENT_CTRL                        0x441b
#define S5_VD3_IF0_GEN_REG3                           0x441c
#define VIU_S5_VD3_FMT_CTRL                           0x441d
#define VIU_S5_VD3_FMT_W                              0x441e
#define S5_VD3_IF0_BADDR_Y                            0x4420
#define S5_VD3_IF0_BADDR_CB                           0x4421
#define S5_VD3_IF0_BADDR_CR                           0x4422
#define S5_VD3_IF0_STRIDE_0                           0x4423
#define S5_VD3_IF0_STRIDE_1                           0x4424
#define S5_VD3_IF0_BADDR_Y_F1                         0x4425
#define S5_VD3_IF0_BADDR_CB_F1                        0x4426
#define S5_VD3_IF0_BADDR_CR_F1                        0x4427
#define S5_VD3_IF0_STRIDE_0_F1                        0x4428
#define S5_VD3_IF0_STRIDE_1_F1                        0x4429
//8'h28-8'h6f   for vd3_afbcd
//==========================================================================
// VD_TOP
//==========================================================================
#define S5_VD3_AFBCDM_VDTOP_CTRL0                         0x4438
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512
//==========================================================================
// AFBC_DEC
//==========================================================================
#define S5_VD3_AFBCDM_ENABLE                              0x4440
//Bit   31:29     reserved
//Bit   28:23     reg_gclk_ctrl_core     unsigned, default = 0
//Bit   22        reg_fmt_size_sw_mode   unsigned, default = 0, 0:hw mode 1:sw mode for format size
//Bit   21        reg_addr_link_en  unsigned, default = 1, 1:enable
//Bit   20        reg_fmt444_comb   unsigned, default = 0, 0: 444 8bit uncomb
//Bit   19        reg_dos_uncomp_mode   unsigned  , default = 0
//Bit   18:16     soft_rst          unsigned  , default = 4
//Bit   15:14     reserved
//Bit   13:12     ddr_blk_size      unsigned  , default = 1
//Bit   11:9      cmd_blk_size      unsigned  , default = 3
//Bit   8         dec_enable        unsigned  , default = 0
//Bit   7:2       reserved
//Bit   1         head_len_sel      unsigned  , default = 1
//Bit   0         reserved          unsigned  , pulse dec_frm_start
#define S5_VD3_AFBCDM_MODE                                0x4441
//Bit   31:30     reserved
//Bit   29        ddr_sz_mode       unsigned, default = 0 , 0: fixed block ddr size 1 : unfixed block ddr size;
//Bit   28        blk_mem_mode      unsigned, default = 0 , 0: fixed 16x128 size; 1 : fixed 12x128 size
//Bit   27:26     rev_mode          unsigned, default = 0 , reverse mode
//Bit   25:24     mif_urgent        unsigned, default = 3 , info mif and data mif urgent
//Bit   23        reserved
//Bit   22:16     hold_line_num     unsigned, default = 0 ,
//Bit   15:14     burst_len         unsigned, default = 2, 0: burst1 1:burst2 2:burst4
//Bit   13:8      compbits_yuv      unsigned, default = 0 ,
//                                  bit 1:0,: y  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 3:2,: u  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 5:4,: v  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//Bit   7:6       vert_skip_y       unsigned, default = 0 , luma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   5:4       horz_skip_y       unsigned, default = 0 , luma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   3:2       vert_skip_uv      unsigned, default = 0 , chroma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   1:0       horz_skip_uv      unsigned, default = 0 , chroma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
#define S5_VD3_AFBCDM_SIZE_IN                             0x4442
//Bit   31:29     reserved
//Bit   28:16     hsize_in          unsigned, default = 1920 , pic horz size in  unit: pixel
//Bit   15:13     reserved
//Bit   12:0      vsize_in          unsigned, default = 1080 , pic vert size in  unit: pixel
#define S5_VD3_AFBCDM_DEC_DEF_COLOR                       0x4443
//Bit   31:30    reserved
//Bit   29:20    def_color_y        unsigned, default = 255, afbc dec y default setting value
//Bit   19:10    def_color_u        unsigned, default = 128, afbc dec u default setting value
//Bit    9: 0    def_color_v        unsigned, default = 128, afbc dec v default setting value
#define S5_VD3_AFBCDM_CONV_CTRL                           0x4444
//Bit   31:14    reserved
//Bit   13:12    fmt_mode            unsigned, default = 2, 0:yuv444 1:yuv422 2:yuv420
//Bit   11: 0    conv_lbuf_len       unsigned, default = 256, unit=16 pixel need to set = 2^n
#define S5_VD3_AFBCDM_LBUF_DEPTH                          0x4445
//Bit   31:28    reserved
//Bit   27:16    dec_lbuf_depth      unsigned, default = 128; // unit= 8 pixel
//Bit   15:12    reserved
//Bit   11:0     mif_lbuf_depth      unsigned, default = 128;
#define S5_VD3_AFBCDM_HEAD_BADDR                          0x4446
//Bit   31:0    mif_info_baddr      unsigned, default = 32'h0;
#define S5_VD3_AFBCDM_BODY_BADDR                          0x4447
//Bit   31:0    mif_data_baddr      unsigned, default = 32'h00010000;
#define S5_VD3_AFBCDM_SIZE_OUT                            0x4448
//Bit   31:29    reserved
//Bit   28:16    hsize_out           unsigned, default = 1920 ; // unit: 1 pixel
//Bit   15:13    reserved
//Bit    12:0    vsize_out           unsigned, default = 1080 ; // unit: 1 pixel
#define S5_VD3_AFBCDM_OUT_YSCOPE                          0x4449
//Bit   31:29    reserved
//Bit   28:16    out_vert_bgn        unsigned, default = 0 ; // unit: 1 pixel
//Bit   15:13    reserved
//Bit    12:0    out_vert_end        unsigned, default = 1079 ; // unit: 1 pixel
#define S5_VD3_AFBCDM_STAT                                0x444a
//Bit   31:18   ro_dbg_axi_cnt_d8    unsigned,RO, default = 0
//Bit   17:16   ro_dbg_axi_idel      unsigned,RO, default = 0
//Bit   15:14   reserved
//Bit   13:8    ro_dbg_frm_cnt       unsigned,RO, default = 0
//Bit   7 :1    ro_dbg_go_line_cnt   unsigned,RO, default = 0
//Bit      0    frm_end_stat         unsigned, default = 0 frame end status
#define S5_VD3_AFBCDM_VD_CFMT_CTRL                        0x444b
//Bit 31    cfmt_gclk_bit_dis      unsigned, default = 0 ; //  it true, disable clock, otherwise enable clock
//Bit 30    cfmt_soft_rst_bit      unsigned, default = 0 ; //  soft rst bit
//Bit 29    reserved
//Bit 28    chfmt_rpt_pix          unsigned, default = 0 ; //  if true, horizontal formatter use repeating to generate pixel, otherwise use bilinear interpolation
//Bit 27:24 chfmt_ini_phase        unsigned, default = 0 ; //  horizontal formatter initial phase
//Bit 23    chfmt_rpt_p0_en        unsigned, default = 0 ; //  horizontal formatter repeat pixel 0 enable
//Bit 22:21 chfmt_yc_ratio         unsigned, default = 0 ; //  horizontal Y/C ratio, 00: 1:1, 01: 2:1, 10: 4:1
//Bit 20    chfmt_en               unsigned, default = 0 ; //  horizontal formatter enable
//Bit 19    cvfmt_phase0_always_en unsigned, default = 0 ; //if true, always use phase0 while vertical formater, meaning always //repeat data, no interpolation
//Bit 18    cvfmt_rpt_last_dis     unsigned, default = 0 ; //if true, disable vertical formatter chroma repeat last line
//Bit 17    cvfmt_phase0_nrpt_en   unsigned, default = 0 ; //vertical formatter dont need repeat line on phase0, 1: enable, 0: disable
//Bit 16    cvfmt_rpt_line0_en     unsigned, default = 0 ; //vertical formatter repeat line 0 enable
//Bit 15:12 cvfmt_skip_line_num    unsigned, default = 0 ; //vertical formatter skip line num at the beginning
//Bit 11:8  cvfmt_ini_phase        unsigned, default = 0 ; //vertical formatter initial phase
//Bit 7:1   cvfmt_phase_step       unsigned, default = 0 ; //vertical formatter phase step (3.4)
//Bit 0     cvfmt_en               unsigned, default = 0 ; //vertical formatter enable
#define S5_VD3_AFBCDM_VD_CFMT_W                           0x444c
//Bit 31:29 reserved
//Bit 28:16 chfmt_w                unsigned, default = 0 ;horizontal formatter width
//Bit 15:13 reserved
//Bit 12:0  cvfmt_w                unsigned, default = 0 ;vertical formatter width
#define S5_VD3_AFBCDM_MIF_HOR_SCOPE                       0x444d
//Bit   31:26   reserved
//Bit   25:16   mif_blk_bgn_h        unsigned, default = 0  ; // unit: 32 pixel/block hor
//Bit   15:10   reserved
//Bit    9: 0   mif_blk_end_h        unsigned, default = 59 ; // unit: 32 pixel/block hor
#define S5_VD3_AFBCDM_MIF_VER_SCOPE                       0x444e
//Bit   31:28   reserved
//Bit   27:16   mif_blk_bgn_v          unsigned, default = 0  ; // unit: 32 pixel/block ver
//Bit   15:12   reserved
//Bit   11: 0   mif_blk_end_v          unsigned, default = 269; // unit: 32 pixel/block ver
#define S5_VD3_AFBCDM_PIXEL_HOR_SCOPE                     0x444f
//Bit   31:29   reserved
//Bit   28:16   dec_pixel_bgn_h        unsigned, default = 0  ; // unit: pixel
//Bit   15:13   reserved
//Bit   12: 0   dec_pixel_end_h        unsigned, default = 1919 ; // unit: pixel
#define S5_VD3_AFBCDM_PIXEL_VER_SCOPE                     0x4450
//Bit   31:29   reserved
//Bit   28:16   dec_pixel_bgn_v        unsigned, default = 0  ; // unit: pixel
//Bit   15:13   reserved
//Bit   12: 0   dec_pixel_end_v        unsigned, default = 1079 ; // unit: pixel
#define S5_VD3_AFBCDM_VD_CFMT_H                           0x4451
//Bit 31:13     reserved
//Bit 12:0      cfmt_h                 unsigned, default = 142  ; //vertical formatter height
#define S5_VD3_AFBCDM_IQUANT_ENABLE                       0x4452
//Bit 31:12        reserved
//Bit  11          reg_quant_expand_en_1     //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  10          reg_quant_expand_en_0     //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  9: 8        reg_bcleav_ofst           //signed ,       RW, default = 0  bcleave ofset to get lower range, especially under lossy, for v1/v2, x=0 is equivalent, default = -1;
//Bit  7: 5        reserved
//Bit  4           reg_quant_enable_1        // unsigned ,    RW, default = 0  enable for quant to get some lossy
//Bit  3: 1        reserved
//Bit  0           reg_quant_enable_0        // unsigned ,    RW, default = 0  enable for quant to get some lossy
#define S5_VD3_AFBCDM_IQUANT_LUT_1                        0x4453
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_0_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_0_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_0_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_0_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD3_AFBCDM_IQUANT_LUT_2                        0x4454
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD3_AFBCDM_IQUANT_LUT_3                        0x4455
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_1_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_1_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_1_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_1_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD3_AFBCDM_IQUANT_LUT_4                        0x4456
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD3_AFBCDM_ROT_CTRL                            0x4460
//Bit   31:30   reg_rot_ohds2_mode                  //unsigned, RW, default = 0 , rot output format down hor drop mode,0:average 1:use 0 2:use 1
//Bit   29:28   reg_rot_ovds2_mode                  //unsigned, RW, default = 0 , rot output format down ver drop mode,0:average 1:use 0 2:use 1
//Bit   27      reg_pip_mode                        //unsigned, RW, default = 0 , 0:dec_src from vdin/dos  1:dec_src from pip
//Bit   26:24   reg_rot_uv_vshrk_drop_mode          //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   23      reserved
//Bit   22:20   reg_rot_uv_hshrk_drop_mode          //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   19:18   reg_rot_uv_vshrk_ratio              //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   17:16   reg_rot_uv_hshrk_ratio              //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   15      reserved
//Bit   14:12   reg_rot_y_vshrk_drop_mode           //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   11      reserved
//Bit   10:8    reg_rot_y_hshrk_drop_mode           //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   7:6     reg_rot_y_vshrk_ratio               //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   5:4     reg_rot_y_hshrk_ratio               //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   3:2     reg_rot_uv422_drop_mode             //unsigned, RW, default = 0 , 0:average 1:left 2:right
//Bit   1       reg_rot_uv422_omode                 //unsigned, RW, default = 0 , when rot input fmt422, 0:output_uv422  1:output_uv420
//Bit   0       reg_rot_enable                      //unsigned, RW, default = 0 , rotation enable
#define S5_VD3_AFBCDM_ROT_SCOPE                           0x4461
//Bit   31:26   reserved
//Bit   25:20   reg_rot_debug_probe      //unsigned, RW , default = 0, y:[2:0] uv:[5:3]; 0:iblk_size 1:oblk_size 2:iblk_cnt 3:oblk_cnt 4:hsize_in 5:vsize_in 6:vstep
//Bit   19      reg_rot_dout_ds_mode_sw  //unsigned, RW , default = 0, 0:use hardware mode 1:use software mode
//Bit   18:17   reg_rot_dout_ds_mode     //unsigned, RW , default = 0, rot output fmt_down mode: [0]:h_downscale [1]:v_downscale
//Bit   16      reg_rot_ifmt_force444    //unsigned, RW , default = 1, 1: rot input fmt force as 444
//Bit   15:14   reg_rot_ofmt_mode        //unsigned, RW , default = 0, rot output fmt mode
//Bit   13:12   reg_rot_compbits_out_y   //unsigned, RW , default = 0, rot output compbit y
//Bit   11:10   reg_rot_compbits_out_uv  //unsigned, RW , default = 0, rot output compbit uv
//Bit   9:8     reg_rot_wrbgn_v          //unsigned, RW , default = 0, rot pic vert size window begin pixel
//Bit   7:5     reserved
//Bit   4:0     reg_rot_wrbgn_h          //unsigned, RW , default = 0, rot pic hori size window begin pixel
#define S5_VD3_AFBCDM_RPLC_CTRL                           0x4462
//Bit   31        reg_rplc_inter_corr_en //unsigned, RW , default = 0   , //pip replace inte-frame edge correct enable
//Bit   30        reg_rplc_dummy_corr_en //unsigned, RW , default = 0   , //pip replace outsize of real-pipframe edge correct enable
//Bit   29        reg_rplc_byps          //unsigned, RW , default = 1   , //pip replace module bypass
//Bit   28:16     reg_rplc_vsize_in      //unsigned, RW , default = 1080, //
//Bit   15:13     reserved
//Bit   12:0      reg_rplc_hsize_in      //unsigned, RW , default = 1920,
#define S5_VD3_AFBCDM_RPLC_PICEN                          0x4463
//Bit  31:28      reserved
//Bit  27:16      reg_rplc_def_color_y    //unsigned, RW , default =0        , //pip replace def_color_y
//Bit  15:0       reg_rplc_pic_enable     //unsigned, RW , default =16'hffff , //pip replace pip_picure enbale
#define S5_VD3_AFBCDM_RPLC_DEFCOL                         0x4464
//Bit  31:24     reserved
//Bit  23:12     reg_rplc_def_color_v    //unsigned, RW , default =0        , //pip replace def_color_v
//Bit  11:0      reg_rplc_def_color_u    //unsigned, RW , default =0        , //pip replace def_color_u
#define S5_VD3_AFBCDM_RPLC_SCPXN_ADDR                     0x4465
//Bit  31:5      reserved
//Bit  4:0       reg_rplc_scpxn_addr     //unsigned, RW , default =0        , //pip replace scopx16 addr
#define S5_VD3_AFBCDM_RPLC_SCPXN_DATA                     0x4466
//Bit  31:26     reserved
//Bit  25:0      reg_rplc_scpxn_data     //unsigned, RW , default =0        , //pip replace scopx16 data
#define S5_VD3_AFBCDM_ROT_RO_STAT                         0x4467
//Bit   31:0     ro_rot_debug           //unsigned, RO , default = 0, rot some status

//8'h70-8'h7f   for vd3_fgrain
#define S5_VD3_FGRAIN_CTRL                            0x4470
//Bit 31:19      reserved
//Bit 18,        reg_dma_st_clr,            .usigned  , clear DMA error status
//Bit 17,        reg_hold4dma_scale,        .usigned  , wait DMA scale data ready before accept input data        default = 0
//Bit 16,        reg_hold4dma_tbl,          .usigned  , wait DMA grain table data ready before accept input data  default = 0
//Bit 15,        reserved
//Bit 14,        reg_cin_uv_swap,           .unsigned , swap uv input data order. default = 0
//Bit 13,        reg_cin_rev,               .unsigned , reverse c order.  default = 0
//Bit 12,        reg_yin_rev,               .unsigned , reverse y orderdefault = 0
//Bit 11,        reserved
//Bit 10,        reg_fgrain_use_sat4bp      .unsigned , only for debug. default = 0
//Bit  9,        reg_fgrain_tbl_ext_mode    .unsigned , default = 1
//Bit  8,        reg_fgrain_ext_imode       .unsigned , default = 1
//Bit  7: 6      reg_fmt_mode               .unsigned , default = 2, 0:444; 1:422; 2:420; 3:reserved
//Bit  5: 4      reg_comp_bits              .unsigned , default = 1, 0:8bits; 1:10bits, else 12 bits
//Bit  3: 2      reg_rev_mode               .unsigned . default = 0, 0:h_rev; 1:v_rev;
//Bit  1,        reg_block_mode             .unsigned , default = 1
//Bit  0,        reg_fgrain_en              .unsigned , default = 0
#define S5_VD3_FGRAIN_WIN_H                           0x4471
//Bit  31:16     reg_win_end_h      .unsigned , default = 3812
//Bit  15: 0,    reg_win_bgn_h      .unsigned , default = 0
#define S5_VD3_FGRAIN_WIN_V                           0x4472
//Bit  31:16     reg_win_end_v      .unsigned , default = 2156
//Bit  15: 0,    reg_win_bgn_v      .unsigned , default = 0
#define S5_VD3_FGRAIN_SW_Y_RANNGE                     0x4473
//Bit 31,        reg_fgrain_sw_yrange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_ymax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_ymin        .unsigned , default = 0
#define S5_VD3_FGRAIN_SW_C_RANNGE                     0x4474
//Bit 31,        reg_fgrain_sw_crange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_cmax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_cmin        .unsigned , default = 0
#define S5_VD3_FGRAIN_GCLK_CTRL_0                     0x4475
//Bit 31:0,      reg_fgrain_gclk_ctrl0  .unsigned , default = 0
#define S5_VD3_FGRAIN_GCLK_CTRL_1                     0x4476
//Bit 31:0,      reg_fgrain_gclk_ctrl1  .unsigned , default = 0
#define S5_VD3_FGRAIN_GCLK_CTRL_2                     0x4477
//Bit 31:0,      reg_fgrain_gclk_ctrl2  .unsigned , default = 0
#define S5_VD3_FGRAIN_PARAM_ADDR                      0x4478
#define S5_VD3_FGRAIN_PARAM_DATA                      0x4479
#define S5_VD3_FGRAIN_SLICE_WIN_H                     0x447a

#define S5_VD4_IF0_GEN_REG                            0x4480
#define S5_VD4_IF0_CANVAS0                            0x4481
#define S5_VD4_IF0_CANVAS1                            0x4482
#define S5_VD4_IF0_LUMA_X0                            0x4483
#define S5_VD4_IF0_LUMA_Y0                            0x4484
#define S5_VD4_IF0_CHROMA_X0                          0x4485
#define S5_VD4_IF0_CHROMA_Y0                          0x4486
#define S5_VD4_IF0_LUMA_X1                            0x4487
#define S5_VD4_IF0_LUMA_Y1                            0x4488
#define S5_VD4_IF0_CHROMA_X1                          0x4489
#define S5_VD4_IF0_CHROMA_Y1                          0x448a
#define S5_VD4_IF0_RPT_LOOP                           0x448b
#define S5_VD4_IF0_LUMA0_RPT_PAT                      0x448c
#define S5_VD4_IF0_CHROMA0_RPT_PAT                    0x448d
#define S5_VD4_IF0_LUMA1_RPT_PAT                      0x448e
#define S5_VD4_IF0_CHROMA1_RPT_PAT                    0x448f
#define S5_VD4_IF0_LUMA_PSEL                          0x4490
#define S5_VD4_IF0_CHROMA_PSEL                        0x4491
#define S5_VD4_IF0_DUMMY_PIXEL                        0x4492
#define S5_VD4_IF0_LUMA_FIFO_SIZE                     0x4493
#define S5_VD4_IF0_AXI_CMD_CNT                        0x4494
#define S5_VD4_IF0_AXI_RDAT_CNT                       0x4495
#define S5_VD4_IF0_RANGE_MAP_Y                        0x4496
#define S5_VD4_IF0_RANGE_MAP_CB                       0x4497
#define S5_VD4_IF0_RANGE_MAP_CR                       0x4498
#define S5_VD4_IF0_GEN_REG2                           0x4499
#define S5_VD4_IF0_PROT                               0x449a
#define S5_VD4_IF0_URGENT_CTRL                        0x449b
#define S5_VD4_IF0_GEN_REG3                           0x449c
#define VIU_S5_VD4_FMT_CTRL                           0x449d
#define VIU_S5_VD4_FMT_W                              0x449e
#define S5_VD4_IF0_BADDR_Y                            0x44a0
#define S5_VD4_IF0_BADDR_CB                           0x44a1
#define S5_VD4_IF0_BADDR_CR                           0x44a2
#define S5_VD4_IF0_STRIDE_0                           0x44a3
#define S5_VD4_IF0_STRIDE_1                           0x44a4
#define S5_VD4_IF0_BADDR_Y_F1                         0x44a5
#define S5_VD4_IF0_BADDR_CB_F1                        0x44a6
#define S5_VD4_IF0_BADDR_CR_F1                        0x44a7
#define S5_VD4_IF0_STRIDE_0_F1                        0x44a8
#define S5_VD4_IF0_STRIDE_1_F1                        0x44a9
//8'ha8-8'hef   for vd3_afbcd

//==========================================================================
// VD_TOP
//==========================================================================
#define S5_VD4_AFBCDM_VDTOP_CTRL0                         0x44b8
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512
//==========================================================================
// AFBC_DEC
//==========================================================================
#define S5_VD4_AFBCDM_ENABLE                              0x44c0
//Bit   31:29     reserved
//Bit   28:23     reg_gclk_ctrl_core     unsigned, default = 0
//Bit   22        reg_fmt_size_sw_mode   unsigned, default = 0, 0:hw mode 1:sw mode for format size
//Bit   21        reg_addr_link_en  unsigned, default = 1, 1:enable
//Bit   20        reg_fmt444_comb   unsigned, default = 0, 0: 444 8bit uncomb
//Bit   19        reg_dos_uncomp_mode   unsigned  , default = 0
//Bit   18:16     soft_rst          unsigned  , default = 4
//Bit   15:14     reserved
//Bit   13:12     ddr_blk_size      unsigned  , default = 1
//Bit   11:9      cmd_blk_size      unsigned  , default = 3
//Bit   8         dec_enable        unsigned  , default = 0
//Bit   7:2       reserved
//Bit   1         head_len_sel      unsigned  , default = 1
//Bit   0         reserved          unsigned  , pulse dec_frm_start
#define S5_VD4_AFBCDM_MODE                                0x44c1
//Bit   31:30     reserved
//Bit   29        ddr_sz_mode       unsigned, default = 0 , 0: fixed block ddr size 1 : unfixed block ddr size;
//Bit   28        blk_mem_mode      unsigned, default = 0 , 0: fixed 16x128 size; 1 : fixed 12x128 size
//Bit   27:26     rev_mode          unsigned, default = 0 , reverse mode
//Bit   25:24     mif_urgent        unsigned, default = 3 , info mif and data mif urgent
//Bit   23        reserved
//Bit   22:16     hold_line_num     unsigned, default = 0 ,
//Bit   15:14     burst_len         unsigned, default = 2, 0: burst1 1:burst2 2:burst4
//Bit   13:8      compbits_yuv      unsigned, default = 0 ,
//                                  bit 1:0,: y  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 3:2,: u  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 5:4,: v  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//Bit   7:6       vert_skip_y       unsigned, default = 0 , luma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   5:4       horz_skip_y       unsigned, default = 0 , luma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   3:2       vert_skip_uv      unsigned, default = 0 , chroma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   1:0       horz_skip_uv      unsigned, default = 0 , chroma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
#define S5_VD4_AFBCDM_SIZE_IN                             0x44c2
//Bit   31:29     reserved
//Bit   28:16     hsize_in          unsigned, default = 1920 , pic horz size in  unit: pixel
//Bit   15:13     reserved
//Bit   12:0      vsize_in          unsigned, default = 1080 , pic vert size in  unit: pixel
#define S5_VD4_AFBCDM_DEC_DEF_COLOR                       0x44c3
//Bit   31:30    reserved
//Bit   29:20    def_color_y        unsigned, default = 255, afbc dec y default setting value
//Bit   19:10    def_color_u        unsigned, default = 128, afbc dec u default setting value
//Bit    9: 0    def_color_v        unsigned, default = 128, afbc dec v default setting value
#define S5_VD4_AFBCDM_CONV_CTRL                           0x44c4
//Bit   31:14    reserved
//Bit   13:12    fmt_mode            unsigned, default = 2, 0:yuv444 1:yuv422 2:yuv420
//Bit   11: 0    conv_lbuf_len       unsigned, default = 256, unit=16 pixel need to set = 2^n
#define S5_VD4_AFBCDM_LBUF_DEPTH                          0x44c5
//Bit   31:28    reserved
//Bit   27:16    dec_lbuf_depth      unsigned, default = 128; // unit= 8 pixel
//Bit   15:12    reserved
//Bit   11:0     mif_lbuf_depth      unsigned, default = 128;
#define S5_VD4_AFBCDM_HEAD_BADDR                          0x44c6
//Bit   31:0    mif_info_baddr      unsigned, default = 32'h0;
#define S5_VD4_AFBCDM_BODY_BADDR                          0x44c7
//Bit   31:0    mif_data_baddr      unsigned, default = 32'h00010000;
#define S5_VD4_AFBCDM_SIZE_OUT                            0x44c8
//Bit   31:29    reserved
//Bit   28:16    hsize_out           unsigned, default = 1920 ; // unit: 1 pixel
//Bit   15:13    reserved
//Bit    12:0    vsize_out           unsigned, default = 1080 ; // unit: 1 pixel
#define S5_VD4_AFBCDM_OUT_YSCOPE                          0x44c9
//Bit   31:29    reserved
//Bit   28:16    out_vert_bgn        unsigned, default = 0 ; // unit: 1 pixel
//Bit   15:13    reserved
//Bit    12:0    out_vert_end        unsigned, default = 1079 ; // unit: 1 pixel
#define S5_VD4_AFBCDM_STAT                                0x44ca
//Bit   31:18   ro_dbg_axi_cnt_d8    unsigned,RO, default = 0
//Bit   17:16   ro_dbg_axi_idel      unsigned,RO, default = 0
//Bit   15:14   reserved
//Bit   13:8    ro_dbg_frm_cnt       unsigned,RO, default = 0
//Bit   7 :1    ro_dbg_go_line_cnt   unsigned,RO, default = 0
//Bit      0    frm_end_stat         unsigned, default = 0 frame end status
#define S5_VD4_AFBCDM_VD_CFMT_CTRL                        0x44cb
//Bit 31    cfmt_gclk_bit_dis      unsigned, default = 0 ; //  it true, disable clock, otherwise enable clock
//Bit 30    cfmt_soft_rst_bit      unsigned, default = 0 ; //  soft rst bit
//Bit 29    reserved
//Bit 28    chfmt_rpt_pix          unsigned, default = 0 ; //  if true, horizontal formatter use repeating to generate pixel, otherwise use bilinear interpolation
//Bit 27:24 chfmt_ini_phase        unsigned, default = 0 ; //  horizontal formatter initial phase
//Bit 23    chfmt_rpt_p0_en        unsigned, default = 0 ; //  horizontal formatter repeat pixel 0 enable
//Bit 22:21 chfmt_yc_ratio         unsigned, default = 0 ; //  horizontal Y/C ratio, 00: 1:1, 01: 2:1, 10: 4:1
//Bit 20    chfmt_en               unsigned, default = 0 ; //  horizontal formatter enable
//Bit 19    cvfmt_phase0_always_en unsigned, default = 0 ; //if true, always use phase0 while vertical formater, meaning always //repeat data, no interpolation
//Bit 18    cvfmt_rpt_last_dis     unsigned, default = 0 ; //if true, disable vertical formatter chroma repeat last line
//Bit 17    cvfmt_phase0_nrpt_en   unsigned, default = 0 ; //vertical formatter dont need repeat line on phase0, 1: enable, 0: disable
//Bit 16    cvfmt_rpt_line0_en     unsigned, default = 0 ; //vertical formatter repeat line 0 enable
//Bit 15:12 cvfmt_skip_line_num    unsigned, default = 0 ; //vertical formatter skip line num at the beginning
//Bit 11:8  cvfmt_ini_phase        unsigned, default = 0 ; //vertical formatter initial phase
//Bit 7:1   cvfmt_phase_step       unsigned, default = 0 ; //vertical formatter phase step (3.4)
//Bit 0     cvfmt_en               unsigned, default = 0 ; //vertical formatter enable
#define S5_VD4_AFBCDM_VD_CFMT_W                           0x44cc
//Bit 31:29 reserved
//Bit 28:16 chfmt_w                unsigned, default = 0 ;horizontal formatter width
//Bit 15:13 reserved
//Bit 12:0  cvfmt_w                unsigned, default = 0 ;vertical formatter width
#define S5_VD4_AFBCDM_MIF_HOR_SCOPE                       0x44cd
//Bit   31:26   reserved
//Bit   25:16   mif_blk_bgn_h        unsigned, default = 0  ; // unit: 32 pixel/block hor
//Bit   15:10   reserved
//Bit    9: 0   mif_blk_end_h        unsigned, default = 59 ; // unit: 32 pixel/block hor
#define S5_VD4_AFBCDM_MIF_VER_SCOPE                       0x44ce
//Bit   31:28   reserved
//Bit   27:16   mif_blk_bgn_v          unsigned, default = 0  ; // unit: 32 pixel/block ver
//Bit   15:12   reserved
//Bit   11: 0   mif_blk_end_v          unsigned, default = 269; // unit: 32 pixel/block ver
#define S5_VD4_AFBCDM_PIXEL_HOR_SCOPE                     0x44cf
//Bit   31:29   reserved
//Bit   28:16   dec_pixel_bgn_h        unsigned, default = 0  ; // unit: pixel
//Bit   15:13   reserved
//Bit   12: 0   dec_pixel_end_h        unsigned, default = 1919 ; // unit: pixel
#define S5_VD4_AFBCDM_PIXEL_VER_SCOPE                     0x44d0
//Bit   31:29   reserved
//Bit   28:16   dec_pixel_bgn_v        unsigned, default = 0  ; // unit: pixel
//Bit   15:13   reserved
//Bit   12: 0   dec_pixel_end_v        unsigned, default = 1079 ; // unit: pixel
#define S5_VD4_AFBCDM_VD_CFMT_H                           0x44d1
//Bit 31:13     reserved
//Bit 12:0      cfmt_h                 unsigned, default = 142  ; //vertical formatter height
#define S5_VD4_AFBCDM_IQUANT_ENABLE                       0x44d2
//Bit 31:12        reserved
//Bit  11          reg_quant_expand_en_1     //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  10          reg_quant_expand_en_0     //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  9: 8        reg_bcleav_ofst           //signed ,       RW, default = 0  bcleave ofset to get lower range, especially under lossy, for v1/v2, x=0 is equivalent, default = -1;
//Bit  7: 5        reserved
//Bit  4           reg_quant_enable_1        // unsigned ,    RW, default = 0  enable for quant to get some lossy
//Bit  3: 1        reserved
//Bit  0           reg_quant_enable_0        // unsigned ,    RW, default = 0  enable for quant to get some lossy
#define S5_VD4_AFBCDM_IQUANT_LUT_1                        0x44d3
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_0_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_0_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_0_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_0_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD4_AFBCDM_IQUANT_LUT_2                        0x44d4
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD4_AFBCDM_IQUANT_LUT_3                        0x44d5
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_1_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_1_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_1_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_1_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD4_AFBCDM_IQUANT_LUT_4                        0x44d6
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD4_AFBCDM_ROT_CTRL                            0x44e0
//Bit   31:30   reg_rot_ohds2_mode                  //unsigned, RW, default = 0 , rot output format down hor drop mode,0:average 1:use 0 2:use 1
//Bit   29:28   reg_rot_ovds2_mode                  //unsigned, RW, default = 0 , rot output format down ver drop mode,0:average 1:use 0 2:use 1
//Bit   27      reg_pip_mode                        //unsigned, RW, default = 0 , 0:dec_src from vdin/dos  1:dec_src from pip
//Bit   26:24   reg_rot_uv_vshrk_drop_mode          //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   23      reserved
//Bit   22:20   reg_rot_uv_hshrk_drop_mode          //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   19:18   reg_rot_uv_vshrk_ratio              //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   17:16   reg_rot_uv_hshrk_ratio              //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   15      reserved
//Bit   14:12   reg_rot_y_vshrk_drop_mode           //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   11      reserved
//Bit   10:8    reg_rot_y_hshrk_drop_mode           //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   7:6     reg_rot_y_vshrk_ratio               //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   5:4     reg_rot_y_hshrk_ratio               //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   3:2     reg_rot_uv422_drop_mode             //unsigned, RW, default = 0 , 0:average 1:left 2:right
//Bit   1       reg_rot_uv422_omode                 //unsigned, RW, default = 0 , when rot input fmt422, 0:output_uv422  1:output_uv420
//Bit   0       reg_rot_enable                      //unsigned, RW, default = 0 , rotation enable
#define S5_VD4_AFBCDM_ROT_SCOPE                           0x44e1
//Bit   31:26   reserved
//Bit   25:20   reg_rot_debug_probe      //unsigned, RW , default = 0, y:[2:0] uv:[5:3]; 0:iblk_size 1:oblk_size 2:iblk_cnt 3:oblk_cnt 4:hsize_in 5:vsize_in 6:vstep
//Bit   19      reg_rot_dout_ds_mode_sw  //unsigned, RW , default = 0, 0:use hardware mode 1:use software mode
//Bit   18:17   reg_rot_dout_ds_mode     //unsigned, RW , default = 0, rot output fmt_down mode: [0]:h_downscale [1]:v_downscale
//Bit   16      reg_rot_ifmt_force444    //unsigned, RW , default = 1, 1: rot input fmt force as 444
//Bit   15:14   reg_rot_ofmt_mode        //unsigned, RW , default = 0, rot output fmt mode
//Bit   13:12   reg_rot_compbits_out_y   //unsigned, RW , default = 0, rot output compbit y
//Bit   11:10   reg_rot_compbits_out_uv  //unsigned, RW , default = 0, rot output compbit uv
//Bit   9:8     reg_rot_wrbgn_v          //unsigned, RW , default = 0, rot pic vert size window begin pixel
//Bit   7:5     reserved
//Bit   4:0     reg_rot_wrbgn_h          //unsigned, RW , default = 0, rot pic hori size window begin pixel
#define S5_VD4_AFBCDM_RPLC_CTRL                           0x44e2
//Bit   31        reg_rplc_inter_corr_en //unsigned, RW , default = 0   , //pip replace inte-frame edge correct enable
//Bit   30        reg_rplc_dummy_corr_en //unsigned, RW , default = 0   , //pip replace outsize of real-pipframe edge correct enable
//Bit   29        reg_rplc_byps          //unsigned, RW , default = 1   , //pip replace module bypass
//Bit   28:16     reg_rplc_vsize_in      //unsigned, RW , default = 1080, //
//Bit   15:13     reserved
//Bit   12:0      reg_rplc_hsize_in      //unsigned, RW , default = 1920,
#define S5_VD4_AFBCDM_RPLC_PICEN                          0x44e3
//Bit  31:28      reserved
//Bit  27:16      reg_rplc_def_color_y    //unsigned, RW , default =0        , //pip replace def_color_y
//Bit  15:0       reg_rplc_pic_enable     //unsigned, RW , default =16'hffff , //pip replace pip_picure enbale
#define S5_VD4_AFBCDM_RPLC_DEFCOL                         0x44e4
//Bit  31:24     reserved
//Bit  23:12     reg_rplc_def_color_v    //unsigned, RW , default =0        , //pip replace def_color_v
//Bit  11:0      reg_rplc_def_color_u    //unsigned, RW , default =0        , //pip replace def_color_u
#define S5_VD4_AFBCDM_RPLC_SCPXN_ADDR                     0x44e5
//Bit  31:5      reserved
//Bit  4:0       reg_rplc_scpxn_addr     //unsigned, RW , default =0        , //pip replace scopx16 addr
#define S5_VD4_AFBCDM_RPLC_SCPXN_DATA                     0x44e6
//Bit  31:26     reserved
//Bit  25:0      reg_rplc_scpxn_data     //unsigned, RW , default =0        , //pip replace scopx16 data
#define S5_VD4_AFBCDM_ROT_RO_STAT                         0x44e7
//Bit   31:0     ro_rot_debug           //unsigned, RO , default = 0, rot some status

//8'hf0-8'hff   for vd3_fgrain
#define S5_VD4_FGRAIN_CTRL                            0x44f0
//Bit 31:19      reserved
//Bit 18,        reg_dma_st_clr,            .usigned  , clear DMA error status
//Bit 17,        reg_hold4dma_scale,        .usigned  , wait DMA scale data ready before accept input data        default = 0
//Bit 16,        reg_hold4dma_tbl,          .usigned  , wait DMA grain table data ready before accept input data  default = 0
//Bit 15,        reserved
//Bit 14,        reg_cin_uv_swap,           .unsigned , swap uv input data order. default = 0
//Bit 13,        reg_cin_rev,               .unsigned , reverse c order.  default = 0
//Bit 12,        reg_yin_rev,               .unsigned , reverse y orderdefault = 0
//Bit 11,        reserved
//Bit 10,        reg_fgrain_use_sat4bp      .unsigned , only for debug. default = 0
//Bit  9,        reg_fgrain_tbl_ext_mode    .unsigned , default = 1
//Bit  8,        reg_fgrain_ext_imode       .unsigned , default = 1
//Bit  7: 6      reg_fmt_mode               .unsigned , default = 2, 0:444; 1:422; 2:420; 3:reserved
//Bit  5: 4      reg_comp_bits              .unsigned , default = 1, 0:8bits; 1:10bits, else 12 bits
//Bit  3: 2      reg_rev_mode               .unsigned . default = 0, 0:h_rev; 1:v_rev;
//Bit  1,        reg_block_mode             .unsigned , default = 1
//Bit  0,        reg_fgrain_en              .unsigned , default = 0
#define S5_VD4_FGRAIN_WIN_H                           0x44f1
//Bit  31:16     reg_win_end_h      .unsigned , default = 3812
//Bit  15: 0,    reg_win_bgn_h      .unsigned , default = 0
#define S5_VD4_FGRAIN_WIN_V                           0x44f2
//Bit  31:16     reg_win_end_v      .unsigned , default = 2156
//Bit  15: 0,    reg_win_bgn_v      .unsigned , default = 0
#define S5_VD4_FGRAIN_SW_Y_RANNGE                     0x44f3
//Bit 31,        reg_fgrain_sw_yrange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_ymax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_ymin        .unsigned , default = 0
#define S5_VD4_FGRAIN_SW_C_RANNGE                     0x44f4
//Bit 31,        reg_fgrain_sw_crange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_cmax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_cmin        .unsigned , default = 0
#define S5_VD4_FGRAIN_GCLK_CTRL_0                     0x44f5
//Bit 31:0,      reg_fgrain_gclk_ctrl0  .unsigned , default = 0
#define S5_VD4_FGRAIN_GCLK_CTRL_1                     0x44f6
//Bit 31:0,      reg_fgrain_gclk_ctrl1  .unsigned , default = 0
#define S5_VD4_FGRAIN_GCLK_CTRL_2                     0x44f7
//Bit 31:0,      reg_fgrain_gclk_ctrl2  .unsigned , default = 0
#define S5_VD4_FGRAIN_PARAM_ADDR                      0x44f8
#define S5_VD4_FGRAIN_PARAM_DATA                      0x44f9
#define S5_VD4_FGRAIN_SLICE_WIN_H                     0x44fa

// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ./afbcd_vd34_regs.h
//
//`include "fgrain_regs.h"      //nouse
//===========================================================================
// -----------------------------------------------
// REG_BASE:  VPP_INTF_AFBCD45_VCBUS_BASE = 0x45
// -----------------------------------------------
//===========================================================================
//`include "afbcd_mult_regs.h"
//
// Reading file:  ./afbcd_vd56_regs.h
//
// synopsys translate_off
// synopsys translate_on
////===============================////
//   reg addr map
//   0 -3f :  rdmif
//   40-6f :  afbcd
//   70-7f :  fgrain
////===============================////
#define S5_VD5_IF0_GEN_REG                            0x4500
#define S5_VD5_IF0_CANVAS0                            0x4501
#define S5_VD5_IF0_CANVAS1                            0x4502
#define S5_VD5_IF0_LUMA_X0                            0x4503
#define S5_VD5_IF0_LUMA_Y0                            0x4504
#define S5_VD5_IF0_CHROMA_X0                          0x4505
#define S5_VD5_IF0_CHROMA_Y0                          0x4506
#define S5_VD5_IF0_LUMA_X1                            0x4507
#define S5_VD5_IF0_LUMA_Y1                            0x4508
#define S5_VD5_IF0_CHROMA_X1                          0x4509
#define S5_VD5_IF0_CHROMA_Y1                          0x450a
#define S5_VD5_IF0_RPT_LOOP                           0x450b
#define S5_VD5_IF0_LUMA0_RPT_PAT                      0x450c
#define S5_VD5_IF0_CHROMA0_RPT_PAT                    0x450d
#define S5_VD5_IF0_LUMA1_RPT_PAT                      0x450e
#define S5_VD5_IF0_CHROMA1_RPT_PAT                    0x450f
#define S5_VD5_IF0_LUMA_PSEL                          0x4510
#define S5_VD5_IF0_CHROMA_PSEL                        0x4511
#define S5_VD5_IF0_DUMMY_PIXEL                        0x4512
#define S5_VD5_IF0_LUMA_FIFO_SIZE                     0x4513
#define S5_VD5_IF0_AXI_CMD_CNT                        0x4514
#define S5_VD5_IF0_AXI_RDAT_CNT                       0x4515
#define S5_VD5_IF0_RANGE_MAP_Y                        0x4516
#define S5_VD5_IF0_RANGE_MAP_CB                       0x4517
#define S5_VD5_IF0_RANGE_MAP_CR                       0x4518
#define S5_VD5_IF0_GEN_REG2                           0x4519
#define S5_VD5_IF0_PROT                               0x451a
#define S5_VD5_IF0_URGENT_CTRL                        0x451b
#define S5_VD5_IF0_GEN_REG3                           0x451c
#define VIU_S5_VD5_FMT_CTRL                           0x451d
#define VIU_S5_VD5_FMT_W                              0x451e
#define S5_VD5_IF0_BADDR_Y                            0x4520
#define S5_VD5_IF0_BADDR_CB                           0x4521
#define S5_VD5_IF0_BADDR_CR                           0x4522
#define S5_VD5_IF0_STRIDE_0                           0x4523
#define S5_VD5_IF0_STRIDE_1                           0x4524
#define S5_VD5_IF0_BADDR_Y_F1                         0x4525
#define S5_VD5_IF0_BADDR_CB_F1                        0x4526
#define S5_VD5_IF0_BADDR_CR_F1                        0x4527
#define S5_VD5_IF0_STRIDE_0_F1                        0x4528
#define S5_VD5_IF0_STRIDE_1_F1                        0x4529
//8'h28-8'h6f   for vd3_afbcd
//==========================================================================
// VD_TOP
//==========================================================================
#define S5_VD5_AFBCDM_VDTOP_CTRL0                         0x4538
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512
//==========================================================================
// AFBC_DEC
//==========================================================================
#define S5_VD5_AFBCDM_ENABLE                              0x4540
//Bit   31:29     reserved
//Bit   28:23     reg_gclk_ctrl_core     unsigned, default = 0
//Bit   22        reg_fmt_size_sw_mode   unsigned, default = 0, 0:hw mode 1:sw mode for format size
//Bit   21        reg_addr_link_en  unsigned, default = 1, 1:enable
//Bit   20        reg_fmt444_comb   unsigned, default = 0, 0: 444 8bit uncomb
//Bit   19        reg_dos_uncomp_mode   unsigned  , default = 0
//Bit   18:16     soft_rst          unsigned  , default = 4
//Bit   15:14     reserved
//Bit   13:12     ddr_blk_size      unsigned  , default = 1
//Bit   11:9      cmd_blk_size      unsigned  , default = 3
//Bit   8         dec_enable        unsigned  , default = 0
//Bit   7:2       reserved
//Bit   1         head_len_sel      unsigned  , default = 1
//Bit   0         reserved          unsigned  , pulse dec_frm_start
#define S5_VD5_AFBCDM_MODE                                0x4541
//Bit   31:30     reserved
//Bit   29        ddr_sz_mode       unsigned, default = 0 , 0: fixed block ddr size 1 : unfixed block ddr size;
//Bit   28        blk_mem_mode      unsigned, default = 0 , 0: fixed 16x128 size; 1 : fixed 12x128 size
//Bit   27:26     rev_mode          unsigned, default = 0 , reverse mode
//Bit   25:24     mif_urgent        unsigned, default = 3 , info mif and data mif urgent
//Bit   23        reserved
//Bit   22:16     hold_line_num     unsigned, default = 0 ,
//Bit   15:14     burst_len         unsigned, default = 2, 0: burst1 1:burst2 2:burst4
//Bit   13:8      compbits_yuv      unsigned, default = 0 ,
//                                  bit 1:0,: y  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 3:2,: u  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 5:4,: v  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//Bit   7:6       vert_skip_y       unsigned, default = 0 , luma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   5:4       horz_skip_y       unsigned, default = 0 , luma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   3:2       vert_skip_uv      unsigned, default = 0 , chroma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   1:0       horz_skip_uv      unsigned, default = 0 , chroma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
#define S5_VD5_AFBCDM_SIZE_IN                             0x4542
//Bit   31:29     reserved
//Bit   28:16     hsize_in          unsigned, default = 1920 , pic horz size in  unit: pixel
//Bit   15:13     reserved
//Bit   12:0      vsize_in          unsigned, default = 1080 , pic vert size in  unit: pixel
#define S5_VD5_AFBCDM_DEC_DEF_COLOR                       0x4543
//Bit   31:30    reserved
//Bit   29:20    def_color_y        unsigned, default = 255, afbc dec y default setting value
//Bit   19:10    def_color_u        unsigned, default = 128, afbc dec u default setting value
//Bit    9: 0    def_color_v        unsigned, default = 128, afbc dec v default setting value
#define S5_VD5_AFBCDM_CONV_CTRL                           0x4544
//Bit   31:14    reserved
//Bit   13:12    fmt_mode            unsigned, default = 2, 0:yuv444 1:yuv422 2:yuv420
//Bit   11: 0    conv_lbuf_len       unsigned, default = 256, unit=16 pixel need to set = 2^n
#define S5_VD5_AFBCDM_LBUF_DEPTH                          0x4545
//Bit   31:28    reserved
//Bit   27:16    dec_lbuf_depth      unsigned, default = 128; // unit= 8 pixel
//Bit   15:12    reserved
//Bit   11:0     mif_lbuf_depth      unsigned, default = 128;
#define S5_VD5_AFBCDM_HEAD_BADDR                          0x4546
//Bit   31:0    mif_info_baddr      unsigned, default = 32'h0;
#define S5_VD5_AFBCDM_BODY_BADDR                          0x4547
//Bit   31:0    mif_data_baddr      unsigned, default = 32'h00010000;
#define S5_VD5_AFBCDM_SIZE_OUT                            0x4548
//Bit   31:29    reserved
//Bit   28:16    hsize_out           unsigned, default = 1920 ; // unit: 1 pixel
//Bit   15:13    reserved
//Bit    12:0    vsize_out           unsigned, default = 1080 ; // unit: 1 pixel
#define S5_VD5_AFBCDM_OUT_YSCOPE                          0x4549
//Bit   31:29    reserved
//Bit   28:16    out_vert_bgn        unsigned, default = 0 ; // unit: 1 pixel
//Bit   15:13    reserved
//Bit    12:0    out_vert_end        unsigned, default = 1079 ; // unit: 1 pixel
#define S5_VD5_AFBCDM_STAT                                0x454a
//Bit   31:18   ro_dbg_axi_cnt_d8    unsigned,RO, default = 0
//Bit   17:16   ro_dbg_axi_idel      unsigned,RO, default = 0
//Bit   15:14   reserved
//Bit   13:8    ro_dbg_frm_cnt       unsigned,RO, default = 0
//Bit   7 :1    ro_dbg_go_line_cnt   unsigned,RO, default = 0
//Bit      0    frm_end_stat         unsigned, default = 0 frame end status
#define S5_VD5_AFBCDM_VD_CFMT_CTRL                        0x454b
//Bit 31    cfmt_gclk_bit_dis      unsigned, default = 0 ; //  it true, disable clock, otherwise enable clock
//Bit 30    cfmt_soft_rst_bit      unsigned, default = 0 ; //  soft rst bit
//Bit 29    reserved
//Bit 28    chfmt_rpt_pix          unsigned, default = 0 ; //  if true, horizontal formatter use repeating to generate pixel, otherwise use bilinear interpolation
//Bit 27:24 chfmt_ini_phase        unsigned, default = 0 ; //  horizontal formatter initial phase
//Bit 23    chfmt_rpt_p0_en        unsigned, default = 0 ; //  horizontal formatter repeat pixel 0 enable
//Bit 22:21 chfmt_yc_ratio         unsigned, default = 0 ; //  horizontal Y/C ratio, 00: 1:1, 01: 2:1, 10: 4:1
//Bit 20    chfmt_en               unsigned, default = 0 ; //  horizontal formatter enable
//Bit 19    cvfmt_phase0_always_en unsigned, default = 0 ; //if true, always use phase0 while vertical formater, meaning always //repeat data, no interpolation
//Bit 18    cvfmt_rpt_last_dis     unsigned, default = 0 ; //if true, disable vertical formatter chroma repeat last line
//Bit 17    cvfmt_phase0_nrpt_en   unsigned, default = 0 ; //vertical formatter dont need repeat line on phase0, 1: enable, 0: disable
//Bit 16    cvfmt_rpt_line0_en     unsigned, default = 0 ; //vertical formatter repeat line 0 enable
//Bit 15:12 cvfmt_skip_line_num    unsigned, default = 0 ; //vertical formatter skip line num at the beginning
//Bit 11:8  cvfmt_ini_phase        unsigned, default = 0 ; //vertical formatter initial phase
//Bit 7:1   cvfmt_phase_step       unsigned, default = 0 ; //vertical formatter phase step (3.4)
//Bit 0     cvfmt_en               unsigned, default = 0 ; //vertical formatter enable
#define S5_VD5_AFBCDM_VD_CFMT_W                           0x454c
//Bit 31:29 reserved
//Bit 28:16 chfmt_w                unsigned, default = 0 ;horizontal formatter width
//Bit 15:13 reserved
//Bit 12:0  cvfmt_w                unsigned, default = 0 ;vertical formatter width
#define S5_VD5_AFBCDM_MIF_HOR_SCOPE                       0x454d
//Bit   31:26   reserved
//Bit   25:16   mif_blk_bgn_h        unsigned, default = 0  ; // unit: 32 pixel/block hor
//Bit   15:10   reserved
//Bit    9: 0   mif_blk_end_h        unsigned, default = 59 ; // unit: 32 pixel/block hor
#define S5_VD5_AFBCDM_MIF_VER_SCOPE                       0x454e
//Bit   31:28   reserved
//Bit   27:16   mif_blk_bgn_v          unsigned, default = 0  ; // unit: 32 pixel/block ver
//Bit   15:12   reserved
//Bit   11: 0   mif_blk_end_v          unsigned, default = 269; // unit: 32 pixel/block ver
#define S5_VD5_AFBCDM_PIXEL_HOR_SCOPE                     0x454f
//Bit   31:29   reserved
//Bit   28:16   dec_pixel_bgn_h        unsigned, default = 0  ; // unit: pixel
//Bit   15:13   reserved
//Bit   12: 0   dec_pixel_end_h        unsigned, default = 1919 ; // unit: pixel
#define S5_VD5_AFBCDM_PIXEL_VER_SCOPE                     0x4550
//Bit   31:29   reserved
//Bit   28:16   dec_pixel_bgn_v        unsigned, default = 0  ; // unit: pixel
//Bit   15:13   reserved
//Bit   12: 0   dec_pixel_end_v        unsigned, default = 1079 ; // unit: pixel
#define S5_VD5_AFBCDM_VD_CFMT_H                           0x4551
//Bit 31:13     reserved
//Bit 12:0      cfmt_h                 unsigned, default = 142  ; //vertical formatter height
#define S5_VD5_AFBCDM_IQUANT_ENABLE                       0x4552
//Bit 31:12        reserved
//Bit  11          reg_quant_expand_en_1     //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  10          reg_quant_expand_en_0     //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  9: 8        reg_bcleav_ofst           //signed ,       RW, default = 0  bcleave ofset to get lower range, especially under lossy, for v1/v2, x=0 is equivalent, default = -1;
//Bit  7: 5        reserved
//Bit  4           reg_quant_enable_1        // unsigned ,    RW, default = 0  enable for quant to get some lossy
//Bit  3: 1        reserved
//Bit  0           reg_quant_enable_0        // unsigned ,    RW, default = 0  enable for quant to get some lossy
#define S5_VD5_AFBCDM_IQUANT_LUT_1                        0x4553
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_0_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_0_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_0_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_0_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD5_AFBCDM_IQUANT_LUT_2                        0x4554
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD5_AFBCDM_IQUANT_LUT_3                        0x4555
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_1_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_1_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_1_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_1_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD5_AFBCDM_IQUANT_LUT_4                        0x4556
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define S5_VD5_AFBCDM_ROT_CTRL                            0x4560
//Bit   31:30   reg_rot_ohds2_mode                  //unsigned, RW, default = 0 , rot output format down hor drop mode,0:average 1:use 0 2:use 1
//Bit   29:28   reg_rot_ovds2_mode                  //unsigned, RW, default = 0 , rot output format down ver drop mode,0:average 1:use 0 2:use 1
//Bit   27      reg_pip_mode                        //unsigned, RW, default = 0 , 0:dec_src from vdin/dos  1:dec_src from pip
//Bit   26:24   reg_rot_uv_vshrk_drop_mode          //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   23      reserved
//Bit   22:20   reg_rot_uv_hshrk_drop_mode          //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   19:18   reg_rot_uv_vshrk_ratio              //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   17:16   reg_rot_uv_hshrk_ratio              //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   15      reserved
//Bit   14:12   reg_rot_y_vshrk_drop_mode           //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   11      reserved
//Bit   10:8    reg_rot_y_hshrk_drop_mode           //unsigned, RW, default = 0 , 0:average (1/2: 1:left 2:right) (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   7:6     reg_rot_y_vshrk_ratio               //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   5:4     reg_rot_y_hshrk_ratio               //unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   3:2     reg_rot_uv422_drop_mode             //unsigned, RW, default = 0 , 0:average 1:left 2:right
//Bit   1       reg_rot_uv422_omode                 //unsigned, RW, default = 0 , when rot input fmt422, 0:output_uv422  1:output_uv420
//Bit   0       reg_rot_enable                      //unsigned, RW, default = 0 , rotation enable
#define S5_VD5_AFBCDM_ROT_SCOPE                           0x4561
//Bit   31:26   reserved
//Bit   25:20   reg_rot_debug_probe      //unsigned, RW , default = 0, y:[2:0] uv:[5:3]; 0:iblk_size 1:oblk_size 2:iblk_cnt 3:oblk_cnt 4:hsize_in 5:vsize_in 6:vstep
//Bit   19      reg_rot_dout_ds_mode_sw  //unsigned, RW , default = 0, 0:use hardware mode 1:use software mode
//Bit   18:17   reg_rot_dout_ds_mode     //unsigned, RW , default = 0, rot output fmt_down mode: [0]:h_downscale [1]:v_downscale
//Bit   16      reg_rot_ifmt_force444    //unsigned, RW , default = 1, 1: rot input fmt force as 444
//Bit   15:14   reg_rot_ofmt_mode        //unsigned, RW , default = 0, rot output fmt mode
//Bit   13:12   reg_rot_compbits_out_y   //unsigned, RW , default = 0, rot output compbit y
//Bit   11:10   reg_rot_compbits_out_uv  //unsigned, RW , default = 0, rot output compbit uv
//Bit   9:8     reg_rot_wrbgn_v          //unsigned, RW , default = 0, rot pic vert size window begin pixel
//Bit   7:5     reserved
//Bit   4:0     reg_rot_wrbgn_h          //unsigned, RW , default = 0, rot pic hori size window begin pixel
#define S5_VD5_AFBCDM_RPLC_CTRL                           0x4562
//Bit   31        reg_rplc_inter_corr_en //unsigned, RW , default = 0   , //pip replace inte-frame edge correct enable
//Bit   30        reg_rplc_dummy_corr_en //unsigned, RW , default = 0   , //pip replace outsize of real-pipframe edge correct enable
//Bit   29        reg_rplc_byps          //unsigned, RW , default = 1   , //pip replace module bypass
//Bit   28:16     reg_rplc_vsize_in      //unsigned, RW , default = 1080, //
//Bit   15:13     reserved
//Bit   12:0      reg_rplc_hsize_in      //unsigned, RW , default = 1920,
#define S5_VD5_AFBCDM_RPLC_PICEN                          0x4563
//Bit  31:28      reserved
//Bit  27:16      reg_rplc_def_color_y    //unsigned, RW , default =0        , //pip replace def_color_y
//Bit  15:0       reg_rplc_pic_enable     //unsigned, RW , default =16'hffff , //pip replace pip_picure enbale
#define S5_VD5_AFBCDM_RPLC_DEFCOL                         0x4564
//Bit  31:24     reserved
//Bit  23:12     reg_rplc_def_color_v    //unsigned, RW , default =0        , //pip replace def_color_v
//Bit  11:0      reg_rplc_def_color_u    //unsigned, RW , default =0        , //pip replace def_color_u
#define S5_VD5_AFBCDM_RPLC_SCPXN_ADDR                     0x4565
//Bit  31:5      reserved
//Bit  4:0       reg_rplc_scpxn_addr     //unsigned, RW , default =0        , //pip replace scopx16 addr
#define S5_VD5_AFBCDM_RPLC_SCPXN_DATA                     0x4566
//Bit  31:26     reserved
//Bit  25:0      reg_rplc_scpxn_data     //unsigned, RW , default =0        , //pip replace scopx16 data
#define S5_VD5_AFBCDM_ROT_RO_STAT                         0x4567
//Bit   31:0     ro_rot_debug           //unsigned, RO , default = 0, rot some status

//8'h70-8'h7f   for vd3_fgrain
//8'h80-8'hff   for reserved
#define S5_VD5_FGRAIN_CTRL                            0x4570
//Bit 31:19      reserved
//Bit 18,        reg_dma_st_clr,            .usigned  , clear DMA error status
//Bit 17,        reg_hold4dma_scale,        .usigned  , wait DMA scale data ready before accept input data        default = 0
//Bit 16,        reg_hold4dma_tbl,          .usigned  , wait DMA grain table data ready before accept input data  default = 0
//Bit 15,        reserved
//Bit 14,        reg_cin_uv_swap,           .unsigned , swap uv input data order. default = 0
//Bit 13,        reg_cin_rev,               .unsigned , reverse c order.  default = 0
//Bit 12,        reg_yin_rev,               .unsigned , reverse y orderdefault = 0
//Bit 11,        reserved
//Bit 10,        reg_fgrain_use_sat4bp      .unsigned , only for debug. default = 0
//Bit  9,        reg_fgrain_tbl_ext_mode    .unsigned , default = 1
//Bit  8,        reg_fgrain_ext_imode       .unsigned , default = 1
//Bit  7: 6      reg_fmt_mode               .unsigned , default = 2, 0:454; 1:422; 2:420; 3:reserved
//Bit  5: 4      reg_comp_bits              .unsigned , default = 1, 0:8bits; 1:10bits, else 12 bits
//Bit  3: 2      reg_rev_mode               .unsigned . default = 0, 0:h_rev; 1:v_rev;
//Bit  1,        reg_block_mode             .unsigned , default = 1
//Bit  0,        reg_fgrain_en              .unsigned , default = 0
#define S5_VD5_FGRAIN_WIN_H                           0x4571
//Bit  31:16     reg_win_end_h      .unsigned , default = 3812
//Bit  15: 0,    reg_win_bgn_h      .unsigned , default = 0
#define S5_VD5_FGRAIN_WIN_V                           0x4572
//Bit  31:16     reg_win_end_v      .unsigned , default = 2156
//Bit  15: 0,    reg_win_bgn_v      .unsigned , default = 0
#define S5_VD5_FGRAIN_SW_Y_RANNGE                     0x4573
//Bit 31,        reg_fgrain_sw_yrange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_ymax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_ymin        .unsigned , default = 0
#define S5_VD5_FGRAIN_SW_C_RANNGE                     0x4574
//Bit 31,        reg_fgrain_sw_crange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_cmax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_cmin        .unsigned , default = 0
#define S5_VD5_FGRAIN_GCLK_CTRL_0                     0x4575
//Bit 31:0,      reg_fgrain_gclk_ctrl0  .unsigned , default = 0
#define S5_VD5_FGRAIN_GCLK_CTRL_1                     0x4576
//Bit 31:0,      reg_fgrain_gclk_ctrl1  .unsigned , default = 0
#define S5_VD5_FGRAIN_GCLK_CTRL_2                     0x4577
//Bit 31:0,      reg_fgrain_gclk_ctrl2  .unsigned , default = 0
#define S5_VD5_FGRAIN_PARAM_ADDR                      0x4578
#define S5_VD5_FGRAIN_PARAM_DATA                      0x4579
#define S5_VD5_FGRAIN_SLICE_WIN_H                     0x457a

// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ./afbcd_vd56_regs.h
//
//`include "fgrain_regs.h"      //nouse
// Reading file:  ./vpp_vd1_scale_regs.h
//
#define VPP_VD1_SCALE_COEF_IDX                     0x1da0
#define VPP_VD1_SCALE_COEF                         0x1da1
#define VPP_VD1_VSC_REGION12_STARTP                0x1da2
#define VPP_VD1_VSC_REGION34_STARTP                0x1da3
#define VPP_VD1_VSC_REGION4_ENDP                   0x1da4
#define VPP_VD1_VSC_START_PHASE_STEP               0x1da5
#define VPP_VD1_VSC_REGION0_PHASE_SLOPE            0x1da6
#define VPP_VD1_VSC_REGION1_PHASE_SLOPE            0x1da7
#define VPP_VD1_VSC_REGION3_PHASE_SLOPE            0x1da8
#define VPP_VD1_VSC_REGION4_PHASE_SLOPE            0x1da9
#define VPP_VD1_VSC_PHASE_CTRL                     0x1daa
#define VPP_VD1_VSC_INI_PHASE                      0x1dab
#define VPP_VD1_HSC_REGION12_STARTP                0x1dac
#define VPP_VD1_HSC_REGION34_STARTP                0x1dad
#define VPP_VD1_HSC_REGION4_ENDP                   0x1dae
#define VPP_VD1_HSC_START_PHASE_STEP               0x1daf
#define VPP_VD1_HSC_REGION0_PHASE_SLOPE            0x1db0
#define VPP_VD1_HSC_REGION1_PHASE_SLOPE            0x1db1
#define VPP_VD1_HSC_REGION3_PHASE_SLOPE            0x1db2
#define VPP_VD1_HSC_REGION4_PHASE_SLOPE            0x1db3
#define VPP_VD1_HSC_PHASE_CTRL                     0x1db4
#define VPP_VD1_SC_MISC                            0x1db5
#define VPP_VD1_SCO_FIFO_CTRL                      0x1db6
#define VPP_VD1_HSC_PHASE_CTRL1                    0x1db7
#define VPP_VD1_HSC_INI_PAT_CTRL                   0x1db8
#define VPP_VD1_SC_GCLK_CTRL                       0x1db9
#define VPP_VD1_PREHSC_COEF                        0x1dba
#define VPP_VD1_PRE_SCALE_CTRL                     0x1dbb
#define VPP_VD1_PREVSC_COEF                        0x1dbc
#define VPP_VD1_PREHSC_COEF1                       0x1dbd
//
// Closing file:  ./vpp_vd1_scale_regs.h
// Reading file:  ./vpp_vd1_scale1_regs.h
//
#define VPP_SLICE1_VD1_SCALE_COEF_IDX              0x28b0
#define VPP_SLICE1_VD1_SCALE_COEF                  0x28b1
#define VPP_SLICE1_VD1_VSC_REGION12_STARTP         0x28b2
#define VPP_SLICE1_VD1_VSC_REGION34_STARTP         0x28b3
#define VPP_SLICE1_VD1_VSC_REGION4_ENDP            0x28b4
#define VPP_SLICE1_VD1_VSC_START_PHASE_STEP        0x28b5
#define VPP_SLICE1_VD1_VSC_REGION0_PHASE_SLOPE     0x28b6
#define VPP_SLICE1_VD1_VSC_REGION1_PHASE_SLOPE     0x28b7
#define VPP_SLICE1_VD1_VSC_REGION3_PHASE_SLOPE     0x28b8
#define VPP_SLICE1_VD1_VSC_REGION4_PHASE_SLOPE     0x28b9
#define VPP_SLICE1_VD1_VSC_PHASE_CTRL              0x28ba
#define VPP_SLICE1_VD1_VSC_INI_PHASE               0x28bb
#define VPP_SLICE1_VD1_HSC_REGION12_STARTP         0x28bc
#define VPP_SLICE1_VD1_HSC_REGION34_STARTP         0x28bd
#define VPP_SLICE1_VD1_HSC_REGION4_ENDP            0x28be
#define VPP_SLICE1_VD1_HSC_START_PHASE_STEP        0x28bf
#define VPP_SLICE1_VD1_HSC_REGION0_PHASE_SLOPE     0x28c0
#define VPP_SLICE1_VD1_HSC_REGION1_PHASE_SLOPE     0x28c1
#define VPP_SLICE1_VD1_HSC_REGION3_PHASE_SLOPE     0x28c2
#define VPP_SLICE1_VD1_HSC_REGION4_PHASE_SLOPE     0x28c3
#define VPP_SLICE1_VD1_HSC_PHASE_CTRL              0x28c4
#define VPP_SLICE1_VD1_SC_MISC                     0x28c5
#define VPP_SLICE1_VD1_SCO_FIFO_CTRL               0x28c6
#define VPP_SLICE1_VD1_HSC_PHASE_CTRL1             0x28c7
#define VPP_SLICE1_VD1_HSC_INI_PAT_CTRL            0x28c8
#define VPP_SLICE1_VD1_SC_GCLK_CTRL                0x28c9
#define VPP_SLICE1_VD1_PREHSC_COEF                 0x28ca
#define VPP_SLICE1_VD1_PRE_SCALE_CTRL              0x28cb
#define VPP_SLICE1_VD1_PREVSC_COEF                 0x28cc
#define VPP_SLICE1_VD1_PREHSC_COEF1                0x28cd
//
// Closing file:  ./vpp_vd1_scale1_regs.h
//
// Reading file:  ./vpp_vd1_scale2_regs.h
//
#define VPP_SLICE2_VD1_SCALE_COEF_IDX              0x29b0
#define VPP_SLICE2_VD1_SCALE_COEF                  0x29b1
#define VPP_SLICE2_VD1_VSC_REGION12_STARTP         0x29b2
#define VPP_SLICE2_VD1_VSC_REGION34_STARTP         0x29b3
#define VPP_SLICE2_VD1_VSC_REGION4_ENDP            0x29b4
#define VPP_SLICE2_VD1_VSC_START_PHASE_STEP        0x29b5
#define VPP_SLICE2_VD1_VSC_REGION0_PHASE_SLOPE     0x29b6
#define VPP_SLICE2_VD1_VSC_REGION1_PHASE_SLOPE     0x29b7
#define VPP_SLICE2_VD1_VSC_REGION3_PHASE_SLOPE     0x29b8
#define VPP_SLICE2_VD1_VSC_REGION4_PHASE_SLOPE     0x29b9
#define VPP_SLICE2_VD1_VSC_PHASE_CTRL              0x29ba
#define VPP_SLICE2_VD1_VSC_INI_PHASE               0x29bb
#define VPP_SLICE2_VD1_HSC_REGION12_STARTP         0x29bc
#define VPP_SLICE2_VD1_HSC_REGION34_STARTP         0x29bd
#define VPP_SLICE2_VD1_HSC_REGION4_ENDP            0x29be
#define VPP_SLICE2_VD1_HSC_START_PHASE_STEP        0x29bf
#define VPP_SLICE2_VD1_HSC_REGION0_PHASE_SLOPE     0x29c0
#define VPP_SLICE2_VD1_HSC_REGION1_PHASE_SLOPE     0x29c1
#define VPP_SLICE2_VD1_HSC_REGION3_PHASE_SLOPE     0x29c2
#define VPP_SLICE2_VD1_HSC_REGION4_PHASE_SLOPE     0x29c3
#define VPP_SLICE2_VD1_HSC_PHASE_CTRL              0x29c4
#define VPP_SLICE2_VD1_SC_MISC                     0x29c5
#define VPP_SLICE2_VD1_SCO_FIFO_CTRL               0x29c6
#define VPP_SLICE2_VD1_HSC_PHASE_CTRL1             0x29c7
#define VPP_SLICE2_VD1_HSC_INI_PAT_CTRL            0x29c8
#define VPP_SLICE2_VD1_SC_GCLK_CTRL                0x29c9
#define VPP_SLICE2_VD1_PREHSC_COEF                 0x29ca
#define VPP_SLICE2_VD1_PRE_SCALE_CTRL              0x29cb
#define VPP_SLICE2_VD1_PREVSC_COEF                 0x29cc
#define VPP_SLICE2_VD1_PREHSC_COEF1                0x29cd
//
// Closing file:  ./vpp_vd1_scale2_regs.h
//
//===========================================================================
// Reading file:  ./vpp_vd1_scale3_regs.h
//
#define VPP_SLICE3_VD1_SCALE_COEF_IDX              0x2ab0
#define VPP_SLICE3_VD1_SCALE_COEF                  0x2ab1
#define VPP_SLICE3_VD1_VSC_REGION12_STARTP         0x2ab2
#define VPP_SLICE3_VD1_VSC_REGION34_STARTP         0x2ab3
#define VPP_SLICE3_VD1_VSC_REGION4_ENDP            0x2ab4
#define VPP_SLICE3_VD1_VSC_START_PHASE_STEP        0x2ab5
#define VPP_SLICE3_VD1_VSC_REGION0_PHASE_SLOPE     0x2ab6
#define VPP_SLICE3_VD1_VSC_REGION1_PHASE_SLOPE     0x2ab7
#define VPP_SLICE3_VD1_VSC_REGION3_PHASE_SLOPE     0x2ab8
#define VPP_SLICE3_VD1_VSC_REGION4_PHASE_SLOPE     0x2ab9
#define VPP_SLICE3_VD1_VSC_PHASE_CTRL              0x2aba
#define VPP_SLICE3_VD1_VSC_INI_PHASE               0x2abb
#define VPP_SLICE3_VD1_HSC_REGION12_STARTP         0x2abc
#define VPP_SLICE3_VD1_HSC_REGION34_STARTP         0x2abd
#define VPP_SLICE3_VD1_HSC_REGION4_ENDP            0x2abe
#define VPP_SLICE3_VD1_HSC_START_PHASE_STEP        0x2abf
#define VPP_SLICE3_VD1_HSC_REGION0_PHASE_SLOPE     0x2ac0
#define VPP_SLICE3_VD1_HSC_REGION1_PHASE_SLOPE     0x2ac1
#define VPP_SLICE3_VD1_HSC_REGION3_PHASE_SLOPE     0x2ac2
#define VPP_SLICE3_VD1_HSC_REGION4_PHASE_SLOPE     0x2ac3
#define VPP_SLICE3_VD1_HSC_PHASE_CTRL              0x2ac4
#define VPP_SLICE3_VD1_SC_MISC                     0x2ac5
#define VPP_SLICE3_VD1_SCO_FIFO_CTRL               0x2ac6
#define VPP_SLICE3_VD1_HSC_PHASE_CTRL1             0x2ac7
#define VPP_SLICE3_VD1_HSC_INI_PAT_CTRL            0x2ac8
#define VPP_SLICE3_VD1_SC_GCLK_CTRL                0x2ac9
#define VPP_SLICE3_VD1_PREHSC_COEF                 0x2aca
#define VPP_SLICE3_VD1_PRE_SCALE_CTRL              0x2acb
#define VPP_SLICE3_VD1_PREVSC_COEF                 0x2acc
#define VPP_SLICE3_VD1_PREHSC_COEF1                0x2acd
//
// Closing file:  ./vpp_vd1_scale3_regs.h
//
//===========================================================================
// Reading file:  ./vpp_vd2_scale_regs.h
//
// synopsys translate_off
// synopsys translate_on
#define VPP_VD2_SCALE_COEF_IDX                         0x3850
#define VPP_VD2_SCALE_COEF                             0x3851
#define VPP_VD2_VSC_REGION12_STARTP                    0x3852
#define VPP_VD2_VSC_REGION34_STARTP                    0x3853
#define VPP_VD2_VSC_REGION4_ENDP                       0x3854
#define VPP_VD2_VSC_START_PHASE_STEP                   0x3855
#define VPP_VD2_VSC_REGION0_PHASE_SLOPE                0x3856
#define VPP_VD2_VSC_REGION1_PHASE_SLOPE                0x3857
#define VPP_VD2_VSC_REGION3_PHASE_SLOPE                0x3858
#define VPP_VD2_VSC_REGION4_PHASE_SLOPE                0x3859
#define VPP_VD2_VSC_PHASE_CTRL                         0x385a
#define VPP_VD2_VSC_INI_PHASE                          0x385b
#define VPP_VD2_HSC_REGION12_STARTP                    0x385c
#define VPP_VD2_HSC_REGION34_STARTP                    0x385d
#define VPP_VD2_HSC_REGION4_ENDP                       0x385e
#define VPP_VD2_HSC_START_PHASE_STEP                   0x385f
#define VPP_VD2_HSC_REGION0_PHASE_SLOPE                0x3860
#define VPP_VD2_HSC_REGION1_PHASE_SLOPE                0x3861
#define VPP_VD2_HSC_REGION3_PHASE_SLOPE                0x3862
#define VPP_VD2_HSC_REGION4_PHASE_SLOPE                0x3863
#define VPP_VD2_HSC_PHASE_CTRL                         0x3864
#define VPP_VD2_SC_MISC                                0x3865
#define VPP_VD2_SCO_FIFO_CTRL                          0x3866
#define VPP_VD2_HSC_PHASE_CTRL1                        0x3867
#define VPP_VD2_HSC_INI_PAT_CTRL                       0x3868
#define VPP_VD2_SC_GCLK_CTRL                           0x3869
#define VPP_VD2_PREHSC_COEF                            0x386a
#define VPP_VD2_PRE_SCALE_CTRL                         0x386b
#define VPP_VD2_PREVSC_COEF                            0x386c
#define VPP_VD2_PREHSC_COEF1                           0x386d
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ./vpp_vd2_scale_regs.h
// Reading file:  ./aisr_post_reg.h
//
//VCBUS_BASE                   8'h41
// 0x40-0x6f
//
// Reading file:  ./aisr_reshap_regs.h
//
// synopsys translate_off
// synopsys translate_on
//VCBUS_BASE                   8'h41
#define S5_AISR_RESHAP_CTRL0                          0x4140
//Bit 31:15 reserved
//Bit 14:12 reg_cmd_intr_len  // unsigned , default = 1, interrupt send cmd when how many series axi cmd,
                              // 0=12 1=16 2=24 3=32 4=40 5=48 6=56 7=64
//Bit 11:10 reg_cmd_req_size  // unsigned , default = 1, how many room fifo have, then axi send series req, 0=16 1=32 2=24 3=64
//Bit 9:8   reg_burst_len     // unsigned , default = 2, burst type: 0-single 1-bst2 2-bst4
//Bit 7     reg_swap_64bit    // unsigned , default = 0, 64bits of 128bit swap enable
//Bit 6     reg_little_endian // unsigned , default = 0, big endian enable
//Bit 5     reg_y_rev         // unsigned , default = 0, vertical reverse enable
//Bit 4     reg_x_rev         // unsigned , default = 0, horizontal reverse enable
//Bit 3     reserved
//Bit 2:0   reg_pack_mode     // unsigned , default = 1, 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit
#define S5_AISR_RESHAP_CTRL1                          0x4141
//Bit 31:27 reserved
//Bit 26:24 reg_hloop_num     // unsigned , default = 1 the hor reshape size
//Bit 23    reserved
//Bit 22:20 reg_vloop_num     // unsigned , default = 1 the hor reshape size
//Bit 19:16 reg_vstep         // unsigned , default = 1
//Bit 15:13 reserved
//Bit 12:0  reg_stride        // unsigned , default = 4096
#define S5_AISR_RESHAP_CTRL2                          0x4142
//Bit 31    reserved
//Bit 30    reg_hold_en       // unsigned , default = 0
//Bit 29:24 reg_pass_num      // unsigned , default = 1
//Bit 23:18 reg_hold_num      // unsigned , default = 0
//Bit 17    reserved
//Bit 16:0  reg_urgent_ctrl   // unsigned , default = 0, urgent control reg :
                              //  16  reg_ugt_init  :  urgent initial value
                              //  15  reg_ugt_en    :  urgent enable
                              //  14  reg_ugt_type  :  1= wrmif 0=rdmif
                              // 7:4  reg_ugt_top_th:  urgent top threshold
                              // 3:0  reg_ugt_bot_th:  urgent bottom threshold
#define S5_AISR_RESHAP_SCOPE_X                        0x4143
//Bit 31:29 reserved
//Bit 28:16 reg_x_end         // unsigned , default = 4095, the canvas hor end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_x_start       // unsigned , default = 0, the canvas hor start pixel position
#define S5_AISR_RESHAP_SCOPE_Y                        0x4144
//Bit 31:29 reserved
//Bit 28:16 reg_y_end         // unsigned , default = 0, the canvas ver end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_y_start       // unsigned , default = 0, the canvas ver start pixel position
#define S5_AISR_RESHAP_BADDR00                        0x4145
//Bit 31:0  reg_baddr00       // unsigned , default = 0, the (0,0) base addr for reshape
#define S5_AISR_RESHAP_BADDR01                        0x4146
//Bit 31:0  reg_baddr01       // unsigned , default = 0, the (0,1) base addr for reshape
#define S5_AISR_RESHAP_BADDR02                        0x4147
//Bit 31:0  reg_baddr02       // unsigned , default = 0, the (0,2) base addr for reshape
#define S5_AISR_RESHAP_BADDR03                        0x4148
//Bit 31:0  reg_baddr03       // unsigned , default = 0, the (0,3) base addr for reshape
#define S5_AISR_RESHAP_BADDR10                        0x4149
//Bit 31:0  reg_baddr10       // unsigned , default = 0, the (1,0) base addr for reshape
#define S5_AISR_RESHAP_BADDR11                        0x414a
//Bit 31:0  reg_baddr11       // unsigned , default = 0, the (1,1) base addr for reshape
#define S5_AISR_RESHAP_BADDR12                        0x414b
//Bit 31:0  reg_baddr12       // unsigned , default = 0, the (1,2) base addr for reshape
#define S5_AISR_RESHAP_BADDR13                        0x414c
//Bit 31:0  reg_baddr13       // unsigned , default = 0, the (1,3) base addr for reshape
#define S5_AISR_RESHAP_BADDR20                        0x414d
//Bit 31:0  reg_baddr20       // unsigned , default = 0, the (2,0) base addr for reshape
#define S5_AISR_RESHAP_BADDR21                        0x414e
//Bit 31:0  reg_baddr21       // unsigned , default = 0, the (2,1) base addr for reshape
#define S5_AISR_RESHAP_BADDR22                        0x414f
//Bit 31:0  reg_baddr22       // unsigned , default = 0, the (2,2) base addr for reshape
#define S5_AISR_RESHAP_BADDR23                        0x4150
//Bit 31:0  reg_baddr23       // unsigned , default = 0, the (2,3) base addr for reshape
#define S5_AISR_RESHAP_BADDR30                        0x4151
//Bit 31:0  reg_baddr30       // unsigned , default = 0, the (3,0) base addr for reshape
#define S5_AISR_RESHAP_BADDR31                        0x4152
//Bit 31:0  reg_baddr31       // unsigned , default = 0, the (3,1) base addr for reshape
#define S5_AISR_RESHAP_BADDR32                        0x4153
//Bit 31:0  reg_baddr32       // unsigned , default = 0, the (3,2) base addr for reshape
#define S5_AISR_RESHAP_BADDR33                        0x4154
//Bit 31:0  reg_baddr33       // unsigned , default = 0, the (3,3) base addr for reshape
#define S5_AISR_RESHAP_MISC                           0x4155
//Bit 31:24 reserved
//Bit 23:16 reg_sw_rst        // unsigned , default = 0,
//Bit 15: 8 reg_int_clr       // unsigned , default = 0,
//Bit  7: 0 reserved
#define S5_AISR_RESHAP_GATE                           0x4156
//Bit 31:0  reg_gclk_ctrl     // unsigned , default = 0,
#define S5_AISR_RESHAP_RO_STAT_0                      0x4157
//Bit 31:16 ro_status_1      // unsigned ,
//Bit 15:0  ro_status_0      // unsigned ,
#define S5_AISR_RESHAP_RO_STAT_1                      0x4158
//Bit 31:16 ro_status_3      // unsigned ,
//Bit 15:0  ro_status_2      // unsigned ,
#define S5_AISR_POST_CTRL                             0x415a
//Bit 31:0 reg_post_ctrl      // unsigned , default = 0,
#define S5_AISR_POST_SIZE                             0x415b
//Bit 31:0 reg_post_size      // unsigned , default = 0x4380780,
#define S5_AISR_AXIRD_BADDR                           0x4160
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ./aisr_reshap_regs.h
//
// 0x70-0x8f
//
// Reading file:  ./vpp_scale_schn_reg.h
//
// synopsys translate_off
// synopsys translate_on
#define S5_SCHN_SCALE_COEF_IDX                        0x4170
//Bit 31:20        reserved
//Bit 19:17        reg_type_index_ext        // unsigned ,    RW, default = 0  default = 0x0 ,type of index, 000: vertical coef, 001: vertical chroma coef: 010: horizontal coef part A, 011: horizontal coef part B, 100: horizontal chroma coef part A, 101: horizontal chroma coef part B
//Bit 16           reg_ctype_ext_mode        // unsigned ,    RW, default = 0  default = 0x0 , if true use type_index_ext rather than reg_type_index
//Bit 15           reg_index_inc             // unsigned ,    RW, default = 0  default	= 0x0 ,default = 0x0 ,index increment, if bit9 == 1  then (0: index increase 1, 1: index increase 2) else (index increase 2)
//Bit 14           reg_rd_cbus_coef_en       // unsigned ,    RW, default = 0  default = 0x0 ,1: read coef through cbus enable, just for debug purpose in case when we wanna check the coef in ram in correct or not
//Bit 13           reg_vf_sep_coef_en        // unsigned ,    RW, default = 0  default = 0x0 ,if true, vertical separated coef enable
//Bit 12:10        reserved
//Bit  9           reg_high_reso_en          // unsigned ,    RW, default = 0  default	= 0x0 ,default = 0x0 ,if true, use 9bit resolution coef, other use 8bit resolution coef
//Bit  8: 7        reg_type_index            // unsigned ,    RW, default = 0  default	= 0x0 ,default = 0x0 ,type of index, 00: vertical coef, 01: vertical chroma coef: 10: horizontal coef, 11: resevered
//Bit  6: 0        reg_coef_index            // unsigned ,    RW, default = 0  default	= 0x0 ,coef	index
#define S5_SCHN_SCALE_COEF                            0x4171
//Bit 31:24        reg_coef0                 // signed ,    RW, default = 0  default	= 0x0 ,	coefficients for vertical filter and horizontal	filter
//Bit 23:16        reg_coef1                 // signed ,    RW, default = 0  default	= 0x0 ,	coefficients for vertical filter and horizontal	filter
//Bit 15: 8        reg_coef2                 // signed ,    RW, default = 0  default	= 0x0 ,	coefficients for vertical filter and horizontal	filter
//Bit  7: 0        reg_coef3                 // signed ,    RW, default = 0  default	= 0x0 ,	coefficients for vertical filter and horizontal	filter
#define S5_SCHN_VSC_REGION12_STARTP                   0x4172
//Bit 31:29        reserved
//Bit 28:16        reg_vsc_region1_startp    // unsigned ,    RW, default = 0  default	= 0	,region1 startp
//Bit 15:13        reserved
//Bit 12: 0        reg_vsc_region2_startp    // unsigned ,    RW, default = 0  default	= 0	,region2 startp
#define S5_SCHN_VSC_REGION34_STARTP                   0x4173
//Bit 31:29        reserved
//Bit 28:16        reg_vsc_region3_startp    // unsigned ,    RW, default = 13'h0438  default	=0x0438,region3	startp
//Bit 15:13        reserved
//Bit 12: 0        reg_vsc_region4_startp    // unsigned ,    RW, default = 13'h0438  default	=0x0438,region4	startp
#define S5_SCHN_VSC_REGION4_ENDP                      0x4174
//Bit 31:13        reserved
//Bit 12: 0        reg_vsc_region4_endp      // unsigned ,    RW, default = 1079  default	=13'd1079,region4 endp
#define S5_SCHN_VSC_START_PHASE_STEP                  0x4175
//Bit 31:28        reserved
//Bit 27:24        reg_vsc_integer_part      // unsigned ,    RW, default = 1  default	=1,vertical	start phase	step,(source/dest)*(2^24),integer part	of	step
//Bit 23: 0        reg_vsc_fraction_part     // unsigned ,    RW, default = 0  default	=0,vertical	start phase	step,(source/dest)*(2^24),fraction part	of	step
#define S5_SCHN_VSC_REGION0_PHASE_SLOPE               0x4176
//Bit 31:25        reserved
//Bit 24: 0        reg_vsc_region0_phase_slope // unsigned ,    RW, default = 0  default	=0,vertical	scaler region0 phase slope,region0 phase slope
#define S5_SCHN_VSC_REGION1_PHASE_SLOPE               0x4177
//Bit 31:25        reserved
//Bit 24: 0        reg_vsc_region1_phase_slope // unsigned ,    RW, default = 0  default	= 0,region1	phase slope
#define S5_SCHN_VSC_REGION3_PHASE_SLOPE               0x4178
//Bit 31:25        reserved
//Bit 24: 0        reg_vsc_region3_phase_slope // unsigned ,    RW, default = 0  default	= 0,region3	phase slope
#define S5_SCHN_VSC_REGION4_PHASE_SLOPE               0x4179
//Bit 31:25        reserved
//Bit 24: 0        reg_vsc_region4_phase_slope // unsigned ,    RW, default = 0  default	= 0,region4	phase slope
#define S5_SCHN_VSC_PHASE_CTRL                        0x417a
//Bit 31:19        reserved
//Bit 18:17        reg_vsc_double_line_mode  // unsigned ,    RW, default = 0  default	= 0,double line	mode, input/output line	width of vscaler becomes 2X, so	only 2 line buffer in this case, use for 3D line by line interlace scaling bit1 true, double the input width and half input height, bit0 true, change line buffer 2 lines instead of 4 lines
//Bit 16           reg_vsc_prog_interlace    // unsigned ,    RW, default = 0  default	= 0,0:progressive	output,	1:interlace	output
//Bit 15           reg_vsc_bot_l0_out_en     // unsigned ,    RW, default = 0  default	= 0,vertical	scaler	output	line0	in	advance	or	not	for	bottom	field
//Bit 14:13        reg_vsc_bot_rpt_l0_num    // unsigned ,    RW, default = 1  default	= 1,vertical	scaler	initial	repeat	line0	number	for	bottom	field
//Bit 12            reserved
//Bit 11: 8        reg_vsc_bot_ini_rcv_num   // unsigned ,    RW, default = 4  default	= 4,vertical	scaler	initial	receiving	number	for	bottom	field
//Bit  7           reg_vsc_top_l0_out_en     // unsigned ,    RW, default = 0  default	= 0,vertical	scaler	output	line0	in	advance	or	not	for	top	field
//Bit  6: 5        reg_vsc_top_rpt_l0_num    // unsigned ,    RW, default = 1  default	= 1,vertical	scaler	initial	repeat	line0	number	for	top	field
//Bit  4            reserved
//Bit  3: 0        reg_vsc_top_ini_rcv_num   // unsigned ,    RW, default = 4  default	= 4,vertical	scaler	initial	receiving	number	for	top	field
#define S5_SCHN_VSC_INI_PHASE                         0x417b
//Bit 31:16        reg_vsc_bot_ini_phase     // unsigned ,    RW, default = 0  default	=	0,vertical	scaler	field	initial	phase	for	bottom	field	Bit
//Bit 15: 0        reg_vsc_top_ini_phase     // unsigned ,    RW, default = 0  default	=	0,vertical	scaler	field	initial	phase	for	top	field
#define S5_SCHN_HSC_REGION12_STARTP                   0x417c
//Bit 31:29        reserved
//Bit 28:16        reg_hsc_region1_startp    // unsigned ,    RW, default = 0  default	=	0,region1	startp
//Bit 15:13        reserved
//Bit 12: 0        reg_hsc_region2_startp    // unsigned ,    RW, default = 0  default	=	0,region2	startp
#define S5_SCHN_HSC_REGION34_STARTP                   0x417d
//Bit 31:29        reserved
//Bit 28:16        reg_hsc_region3_startp    // unsigned ,    RW, default = 13'h780  default	=	0x780,region3	startp
//Bit 15:13        reserved
//Bit 12: 0        reg_hsc_region4_startp    // unsigned ,    RW, default = 13'h780  default	=	0x780,region4	startp
#define S5_SCHN_HSC_REGION4_ENDP                      0x417e
//Bit 31:13        reserved
//Bit 12: 0        reg_hsc_region4_endp      // unsigned ,    RW, default = 1919  default	=	13'd1919,region4	startp
#define S5_SCHN_HSC_START_PHASE_STEP                  0x417f
//Bit 31:28        reserved
//Bit 27:24        reg_hsc_integer_part      // unsigned ,    RW, default = 1  default	=	1,integer	part	of	step
//Bit 23: 0        reg_hsc_fraction_part     // unsigned ,    RW, default = 0  default	=	0,fraction	part	of	step
#define S5_SCHN_HSC_REGION0_PHASE_SLOPE               0x4180
//Bit 31:25        reserved
//Bit 24: 0        reg_hsc_region0_phase_slope // unsigned ,    RW, default = 0  default	=	0,region0	phase	slope
#define S5_SCHN_HSC_REGION1_PHASE_SLOPE               0x4181
//Bit 31:25        reserved
//Bit 24: 0        reg_hsc_region1_phase_slope // unsigned ,    RW, default = 0  default	=	0,region1	phase	slope
#define S5_SCHN_HSC_REGION3_PHASE_SLOPE               0x4182
//Bit 31:25        reserved
//Bit 24: 0        reg_hsc_region3_phase_slope // unsigned ,    RW, default = 0  default	=	0,region3	phase	slope
#define S5_SCHN_HSC_REGION4_PHASE_SLOPE               0x4183
//Bit 31:25        reserved
//Bit 24: 0        reg_hsc_region4_phase_slope // unsigned ,    RW, default = 0  default	=	0,region4	phase	slope
#define S5_SCHN_HSC_PHASE_CTRL                        0x4184
//Bit 31:24        reserved
//Bit 23:20        reg_hsc_rpt_p0_num0       // unsigned ,    RW, default = 1  default	=	1	,horizontal	scaler	initial	repeat	pixel0	number0
//Bit 19:16        reg_hsc_ini_rcv_num0      // unsigned ,    RW, default = 4  default	=	4	,horizontal	scaler	initial	receiving number0
//Bit 15: 0        reg_hsc_ini_phase0        // unsigned ,    RW, default = 0  default	=	0	,horizontal	scaler	top	field initial phase0
#define S5_SCHN_SC_MISC                               0x4185
//Bit 31:26        reserved
//Bit 25           reg_hf_sep_coef_4srnet_en // unsigned ,    RW, default = 0  default = 0x0, if true, horizontal separated coef in normal path for SRNet enable
//Bit 24           reg_repeat_last_line_en   // unsigned ,    RW, default = 0  1,enable	repeat last line 0:disable repeat last line
//Bit 23           reg_old_prehsc_en         // unsigned ,    RW, default = 0  default	=	0	,prehsc_en
//Bit 22           reg_hsc_len_div2_en       // unsigned ,    RW, default = 0  default	=	0	,if	true,divide VSC line length 2 as the HSC input length, othwise VSC line lengthjust for special usage, more flexibility
//Bit 21           reg_prevsc_lbuf_mode      // unsigned ,    RW, default = 0  default	=	0	,if	true, prevsc uses line buffer, otherwise prevsc	does not use line buffer, it should	be same as prevsc_en
//Bit 20           reg_prehsc_en             // unsigned ,    RW, default = 0  default	=	0	,prehsc_en
//Bit 19           reg_prevsc_en             // unsigned ,    RW, default = 0  default	=	0	,prevsc_en
//Bit 18           reg_vsc_en                // unsigned ,    RW, default = 0  default	=	0	,vsc_en
//Bit 17           reg_hsc_en                // unsigned ,    RW, default = 0  default	=	0	,hsc_en
//Bit 16           reg_sc_top_en             // unsigned ,    RW, default = 0  default	=	0	,scale_top_en
//Bit 15           reg_sc_vd_en              // unsigned ,    RW, default = 1  default	=	0	,video1	scale out enable
//Bit 14            reserved
//Bit 13           reg_hsc_nonlinear_4region_en // unsigned ,    RW, default = 0  default	=	1	,if	true, region0,region4 are nonlinear	regions, otherwise they	are	not	scaling	regions, for horizontal	scaler
//Bit 12            reserved
//Bit 11: 8        reg_hsc_bank_length       // unsigned ,    RW, default = 4  default	=	0	,horizontal	scaler	bank	length
//Bit  7: 6        reserved
//Bit  5           reg_vsc_phase_field_mode  // unsigned ,    RW, default = 0  default	=	0	,vertical scaler phase field mode, if true,	disable	the	opposite parity	line output, more bandwith needed if output	1080i
//Bit  4           reg_vsc_nonlinear_4region_en // unsigned ,    RW, default = 0  default	=	0	,if	true, region0,region4 are nonlinear	regions, otherwise they	are	not	scaling	regions, for vertical scaler
//Bit  3            reserved
//Bit  2: 0        reg_vsc_bank_length       // unsigned ,    RW, default = 4  default	=	4	,vertical	scaler	bank	length
#define S5_SCHN_SCO_FIFO_CTRL                         0x4186
//Bit 31:29        reserved
//Bit 28:16        reg_sco_fifo_line_len1   // unsigned ,    RW, default = 13'hfff  default	=	0xfff,	scale	out	fifo	line	length	minus	1
//Bit 15:13        reserved
//Bit 12: 0        reg_sco_fifo_size         // unsigned ,    RW, default = 13'h200  default	=	0x200,	scale	out	fifo	size	(actually	only	bit	11:01	is	valid,	11:1,	max	1024),	always	even	number
#define S5_SCHN_HSC_PHASE_CTRL1                       0x4187
//Bit 31:29        reserved
//Bit 28           reg_hsc_double_pix_mode   // unsigned ,    RW, default = 0  default	=	0,horizontal	scaler	double	pixel	mode
//Bit 27:24        reg_prehsc_mode           // unsigned ,    RW, default = 0  default	=	0,prehsc_mode, bit 3:2, prehsc odd line	interp mode, bit 1:0, prehsc even line interp mode, each 2bit, 00 pix0+pix1/2, average,	01:pix1,10:pix0
//Bit 23:20        reg_hsc_rpt_p0_num1       // unsigned ,    RW, default = 1  default	=	1,horizontal	scaler	initial	repeat	pixel0	number1
//Bit 19:16        reg_hsc_ini_rcv_num1      // unsigned ,    RW, default = 4  default	=	4,horizontal	scaler	initial	receiving	number1
//Bit 15: 0        reg_hsc_ini_phase1        // unsigned ,    RW, default = 0  default	=	0,horizontal	scaler	top	field	initial	phase1
#define S5_SCHN_HSC_INI_PAT_CTRL                      0x4188
//Bit 31:24        reg_prehsc_pattern        // unsigned ,    RW, default = 0  default	=	0,	prehsc	pattern,	each	patten	1	bit,	from	lsb	->	msb
//Bit 23            reserved
//Bit 22:20        reg_prehsc_pat_star       // unsigned ,    RW, default = 0  default	=	0,	prehsc	pattern	start
//Bit 19            reserved
//Bit 18:16        reg_prehsc_pat_end        // unsigned ,    RW, default = 0  default	=	0,	prehsc	pattern	end
//Bit 15: 8        reg_hsc_pattern           // unsigned ,    RW, default = 0  default	=	0,	hsc	pattern,	each	patten	1	bit,	from	lsb	->	msb
//Bit  7            reserved
//Bit  6: 4        reg_hsc_pat_start         // unsigned ,    RW, default = 0  default	=	0,	hsc	pattern	start
//Bit  3            reserved
//Bit  2: 0        reg_hsc_pat_end           // unsigned ,    RW, default = 0  default	=	0,	hsc	pattern	end
#define S5_SCHN_SC_GCLK_CTRL                          0x4189
//Bit 31:16        reserved
//Bit 15: 0        reg_vpp_sc_gclk_ctrl      // unsigned ,    RW, default = 0  default	=	0	,scale	clock	gate
#define S5_SCHN_PREHSC_COEF                           0x418a
//Bit 31:26        reserved
//Bit 25:16        reg_prehsc_coef_1         // signed ,    RW, default = 0  default	=	0x00	,	coefficient0	pre horizontal	filter
//Bit 15:10        reserved
//Bit  9: 0        reg_prehsc_coef_0         // signed ,    RW, default = 256  default	=	0x00	,	coefficient1	pre horizontal	filter
#define S5_SCHN_PREVSC_COEF                           0x418b
//Bit 31:26        reserved
//Bit 25:16        reg_prevsc_coef_1         // signed ,    RW, default = 0  default	=	0x00	,	coefficient2	pre vertical	filter
//Bit 15:10        reserved
//Bit  9: 0        reg_prevsc_coef_0         // signed ,    RW, default = 256  default	=	0x40	,	coefficient3	pre vertical	filter
#define S5_SCHN_PRE_SCALE_CTRL                        0x418c
//Bit 31:29        reserved
//Bit 28:25        reg_preh_hb_num           // unsigned ,    RW, default = 8  default = 8, prehsc rtl h blank number
//Bit 24:21        reg_preh_vb_num           // unsigned ,    RW, default = 8  default = 8, prehsc rtl v blank number
//Bit 20           reg_sc_coef_s11_mode      // unsigned ,    RW, default = 0  default = 0, sc coef bit-width 0:s9, 1:s11
//Bit 19:16        reg_vsc_nor_rs_bits       // unsigned ,    RW, default = 7  default = 7, normalize right shift bits of vsc
//Bit 15:12        reg_hsc_nor_rs_bits       // unsigned ,    RW, default = 7  default = 7, normalize right shift bits of hsc
//Bit 11: 8        reg_prehsc_flt_num        // unsigned ,    RW, default = 2  default = 2, prehsc filter tap num
//Bit  7: 4        reg_prevsc_flt_num        // unsigned ,    RW, default = 2  default = 2, prevsc filter tap num
//Bit  3: 2        reg_prehsc_rate           // unsigned ,    RW, default = 1  default =   0,pre hscale down rate, 0:width,1:width/2,2:width/4,3:width/8
//Bit  1: 0        reg_prevsc_rate           // unsigned ,    RW, default = 1  default =   0,pre vscale down rate, 0:height,1:height/2,2:height/4,3:height/8
#define S5_SCHN_PREHSC_COEF1                          0x418d
//Bit 31:26        reserved
//Bit 25:16        reg_prehsc_coef_3         // signed ,    RW, default = 0  default	=	0x00	,	coefficient2	pre horizontal	filter
//Bit 15:10        reserved
//Bit  9: 0        reg_prehsc_coef_2         // signed ,    RW, default = 0  default	=	0x40	,	coefficient3	pre horizontal	filter
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ./vpp_scale_schn_reg.h
//
//
// Closing file:  ./aisr_post_reg.h
//
#define S5_VD1_SLICE0_HDR2_CTRL                       0x25a0
#define S5_VD1_SLICE1_HDR2_CTRL                       0x26a0
#define S5_VD1_SLICE2_HDR2_CTRL                       0x2ca0
#define S5_VD1_SLICE3_HDR2_CTRL                       0x3ea0
#define S5_VD2_HDR2_CTRL                              0x3800
#define S5_OSD1_PROC_HDR2_CTRL                        0x6450
#define S5_OSD2_PROC_HDR2_CTRL                        0x6850
#define S5_OSD3_PROC_HDR2_CTRL                        0x6c50
#define S5_OSD_HDR2_CTRL                              0x6000

#define VD1_PI_CONTROL                                0x4c54

#define VPP_SLICE1_DNLP_CTRL_01                       0x2865
#define VPP_SLICE2_DNLP_CTRL_01                       0x2965
#define VPP_SLICE3_DNLP_CTRL_01                       0x2a65

#define S5_VPP_PROBE_CTRL                             0x606b
#define S5_VPP_PROBE_POS                              0x606c
#define S5_VPP_HL_COLOR                               0x606d
#define S5_VPP_RO_PROBE_COLOR                         0x6070
#define S5_VPP_RO_PROBE_COLOR1                        0x6071

#define S5_ENCI_VIDEO_EN                              0x1b57
#define S5_VPU_VENCI_STAT                             0x1ceb
#define S5_VPU_VENCL_STAT                             0x1cec
#define S5_VPU_VENCP_STAT                             0x1ced
#define S5_VPU_VENC_CTRL                              0x1cef
#define S5_VPU_VIU_VENC_MUX_CTRL                      0x271a

#define S5_VPU_RDARB_UGT_L2C1                         0x27c2
#define S5_VPP_RDARB_MODE                             0x4120
#define S5_VPU_RDARB_MODE_L2C1                        0x279d

#endif
