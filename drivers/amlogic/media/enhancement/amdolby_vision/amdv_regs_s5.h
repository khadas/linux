/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef AMDV_REGS_S5_HEADER_
#define AMDV_REGS_S5_HEADER_
//------------------------------------------------------------------------------
// VIU registers 0x20 -0x3f
//------------------------------------------------------------------------------
#define VIU_VD1_MISC                               0x1a10
#define VIU_VD2_MISC                               0x1a11
#define VIU_VD3_MISC                               0x1a12
#define VIU_VD4_MISC                               0x1a13
#define VIU_VD5_MISC                               0x1a14
#define VIU_OSD1_MISC                              0x1a15
#define VIU_OSD2_MISC                              0x1a16
#define VIU_OSD3_MISC                              0x1a17
#define VIU_OSD4_MISC                              0x1a18
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
//Bit 31    sr0_pps_dpsel //unsigned, RW, default = 0, 0:SR0 after PPS 1:SR0 before PPS
//Bit 30    vd1_sr0_dpsel //unsigned, RW, default = 0, 0:SR0 in vd1 slice0 1:SR0 in vd1 slice1
//Bit 29:0  reg_sr0_ctrl  //unsigned, RW, default = 0, use for hsvsharp
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
//Bit 5      reg_bypass_prebld1   //unsigned, RW, default = 1, 0:use vd prebld 1:bypass vd prebld
//Bit 4      vd_s3_bypass_ve      //unsigned, RW, default = 1, 0:use ve 1:bypass ve
//Bit 3      vd_s2_bypass_ve      //unsigned, RW, default = 1, 0:use ve 1:bypass ve
//Bit 2      vd_s1_bypass_ve      //unsigned, RW, default = 1, 0:use ve 1:bypass ve
//Bit 1      vd_s0_bypass_ve      //unsigned, RW, default = 1, 0:use ve 1:bypass ve
//Bit 0      reg_bypass_prebld0   //unsigned, RW, default = 1, 0:use vd prebld 1:bypass vd prebld
#define VD_S0_HWIN_CUT                             0x2810
//Bit 31:30  reserved
//Bit 29     vd_s0_hwin_en              //unsigned, RW, default = 0, win_en, cut the overlap size
//Bit 28:16  vd_s0_hwin_bgn             //unsigned, RW, default = 240
//Bit 15:13 reserved
//Bit 12:0   vd_s0_hwin_end                       //unsigned, RW, default = 320
#define VD_S1_HWIN_CUT                             0x2811
//Bit 31:30  reserved
//Bit 29     vd_s1_hwin_en              //unsigned, RW, default = 0, win_en, cut the overlap size
//Bit 28:16  vd_s1_hwin_bgn             //unsigned, RW, default = 240
//Bit 15:13 reserved
//Bit 12:0   vd_s1_hwin_end                       //unsigned, RW, default = 320
#define VD_S2_HWIN_CUT                             0x2812
//Bit 31:30  reserved
//Bit 29     vd_s2_hwin_en              //unsigned, RW, default = 0, win_en, cut the overlap size
//Bit 28:16  vd_s2_hwin_bgn             //unsigned, RW, default = 240
//Bit 15:13 reserved
//Bit 12:0   vd_s2_hwin_end                       //unsigned, RW, default = 320
#define VD_S3_HWIN_CUT                             0x2813
//Bit 31:30  reserved
//Bit 29     vd_s3_hwin_en              //unsigned, RW, default = 0, win_en, cut the overlap size
//Bit 28:16  vd_s3_hwin_bgn             //unsigned, RW, default = 240
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
#define VD1_S0_DETUNNEL_CTRL                       0x2821
//Bit 31:0   vd1_s0_detunnel_ctrl                    //unsigned, RW, default = 0
#define VD1_S0_DV_BYPASS_CTRL                      0x2822
//Bit 31:2   reserved
//Bit 1      vd1_s0_dv_ext_mode                   //unsigned, RW, default = 0
//Bit 0      vd1_s0_dv_en                         //unsigned, RW, default = 0
#define VD1_S1_DETUNNEL_CTRL                         0x2823
//Bit 31:0   vd1_s1_detunnel_ctrl                    //unsigned, RW, default = 0
#define VD1_S1_DV_BYPASS_CTRL                      0x2824
//Bit 31:2   reserved
//Bit 1      vd1_s1_dv_ext_mode                   //unsigned, RW, default = 0
//Bit 0      vd1_s1_dv_en                         //unsigned, RW, default = 0
#define VD1_S2_DETUNNEL_CTRL                         0x2825
//Bit 31:0   vd1_s2_detunnel_ctrl                    //unsigned, RW, default = 0
#define VD1_S2_DV_BYPASS_CTRL                      0x2826
//Bit 31:2   reserved
//Bit 1      vd1_s2_dv_ext_mode                   //unsigned, RW, default = 0
//Bit 0      vd1_s2_dv_en                         //unsigned, RW, default = 0
#define VD1_S3_DETUNNEL_CTRL                         0x2827
//Bit 31:0   vd1_s3_detunnel_ctrl                    //unsigned, RW, default = 0
#define VD1_S3_DV_BYPASS_CTRL                      0x2828
//Bit 31:2   reserved
//Bit 1      vd1_s3_dv_ext_mode                   //unsigned, RW, default = 0
//Bit 0      vd1_s3_dv_en                         //unsigned, RW, default = 0
//
// Closing file:  ./vpp_vd_proc_reg.h
//
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
//Bit 7:6    reg_4s4p_gc_ctrl  //unsigned, RW, default = 0, 4slice4ppc clk gating ctrl
//Bit 5:4    reg_4p4s_gc_ctrl  //unsigned, RW, default = 0, 4ppc4slice clk gating ctrl
//Bit 3:2    reg_2p2s_gc_ctrl  //unsigned, RW, default = 0, 2ppc2slice clk gating ctrl
//Bit 0      reg_gclk_ctrl_h   //unsigned, RW, default = 0, reg_gclk_ctrl high bit, low bit is 0
#define S5_VPP_POSTBLEND_H_V_SIZE                      0x1d01
//Bit 31:30  reserved
//Bit 29:16  vpp_postblend_vsize                //unsigned, RW, default = 1080
//Bit 15:14  reserved
//Bit 13:0   vpp_postblend_hsize                //unsigned, RW, default = 1920
#define S5_VPP_POSTBLEND_CTRL                          0x1d02
//Bit 31:10  reserved
//Bit 9      reg_cbus_mode                     //unsigned, RW, default = 0
//Bit 8      vpp_post_blend_en                 //unsigned, RW, default = 0
//Bit 7 :0   reg_ovlp_size                     //unsigned, RW, default = 0
#define S5_VPP_POSTBLEND_VD1_H_START_END              0x1d03
//Bit 31:29  reserved
//Bit 28:16  vd1xn_scope_hs  //unsigned, RW, default = 1920, vd1 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd1xn_scope_he  //unsigned, RW, default = 0, vd1 scope horizontal ending
#define S5_VPP_POSTBLEND_VD1_V_START_END              0x1d04
//Bit 31:29  reserved
//Bit 28:16  vd1xn_scope_vs  //unsigned, RW, default = 1080, vd1 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd1xn_scope_ve  //unsigned, RW, default = 0, vd1 scope vertical ending
#define S5_VPP_POSTBLEND_VD2_H_START_END              0x1d05
//Bit 31:29  reserved
//Bit 28:16  vd2xn_scope_hs     //unsigned, RW, default = 1920, vd2 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd2xn_scope_he    //unsigned, RW, default = 0, vd2 scope horizontal ending
#define S5_VPP_POSTBLEND_VD2_V_START_END              0x1d06
//Bit 31:29  reserved
//Bit 28:16  vd2xn_scope_vs    //unsigned, RW, default = 1080, vd2 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd2xn_scope_ve    //unsigned, RW, default = 0, vd2 scope vertical ending
#define S5_VPP_POSTBLEND_VD3_H_START_END              0x1d07
//Bit 31:29  reserved
//Bit 28:16  vd3xn_scope_hs     //unsigned, RW, default = 1920, vd3 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   vd3xn_scope_he     //unsigned, RW, default = 0, vd3 scope horizontal ending
#define S5_VPP_POSTBLEND_VD3_V_START_END              0x1d08
//Bit 31:29  reserved
//Bit 28:16  vd3xn_scope_vs    //unsigned, RW, default = 1080, vd3 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   vd3xn_scope_ve    //unsigned, RW, default = 0, vd3 scope vertical ending
#define S5_VPP_OSD1_BLD_H_SCOPE                       0x1d09
//Bit 31:29  reserved
//Bit 28:16  osd1xn_scope_hs   //unsigned, RW, default = 1920, osd1 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   osd1xn_scope_he   //unsigned, RW, default = 0, osd1 scope horizontal ending
#define S5_VPP_OSD1_BLD_V_SCOPE                       0x1d0a
//Bit 31:29  reserved
//Bit 28:16  osd1xn_scope_vs    //unsigned, RW, default = 1080, osd1 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   osd1xn_scope_ve    //unsigned, RW, default = 0, osd1 scope vertical ending
#define S5_VPP_OSD2_BLD_H_SCOPE                       0x1d0b
//Bit 31:29  reserved
//Bit 28:16  osd2xn_scope_hs   //unsigned, RW, default = 1920, osd2 scope horizontal starting
//Bit 15:13  reserved
//Bit 12:0   osd2xn_scope_he   //unsigned, RW, default = 0, osd2 scope horizontal ending
#define S5_VPP_OSD2_BLD_V_SCOPE                       0x1d0c
//Bit 31:29  reserved
//Bit 28:16  osd2xn_scope_vs   //unsigned, RW, default = 1080, osd2 scope vertical starting
//Bit 15:13  reserved
//Bit 12:0   osd2xn_scope_ve   //unsigned, RW, default = 0, osd2 scope vertical ending
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
//Bit 1:0    obuf_ram_slice_mode  //unsigned, RW, default = 0, 0: 1x2k ram 1: 2x2k ram 3:4x2k ram
#define S5_VPP_4S4P_CTRL                              0x1d16
//Bit 31:2   reserved
//Bit 1:0    reg_4s4p_mode //unsigned,RW,default = 0,
//0:4slice to 4ppc, 1:2 slice to 4ppc, 2:1 slice to 4ppc 3:disable
#define S5_VPP_4P4S_CTRL                              0x1d17
//Bit 31:2   reserved
//Bit 1:0    reg_4p4s_mode //unsigned,RW,default = 0,
//0:4ppc to 4slice, 1:4 ppc to 2slice, 2:4ppc to 1slice 3:disable
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
#define VD2_DETUNNEL_CTRL                            0x3887
//Bit 31:0   vd2_detunnel_ctrl                    //unsigned, RW, default = 0
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

//===========================================================================
// -----------------------------------------------
// REG_BASE:  VPP_POST_UNIT1_VCBUS_BASE = 0x26
// -----------------------------------------------
//===========================================================================
//`include "disp_if_regs.h"

// Reading file:  ./vpp_post_unit1_reg.h
//
//===========================================================================
// Video postprocessing Registers
//===========================================================================
//====================top====================
#define VPP_SLICE1_GCLK_CTRL                       0x2600
//Bit 31:8   reserved
//Bit 7:6    align_fifo_gclk_ctrl//unsigned, RW, default = 0
//Bit 5:4    ofifo_gclk_ctrl     //unsigned, RW, default = 0
//Bit 3:2    top_gclk_ctrl       //unsigned, RW, default = 0
//Bit 1      reg_gclk_ctrl       //unsigned, RW, default = 0, reg_gclk_ctrl high bit, low bit is 0
//Bit 0      reserved
#define VPP_SLICE1_DOLBY_CTRL                      0x2601
//Bit 31:11  reserved
//Bit 10     VPP_SLICE1_clip_ext_mode   //unsigned, RW, default = 0
//Bit 3      VPP_SLICE1_dolby3_en       //unsigned, RW, default = 0
//Bit 2      VPP_SLICE1_dpath_sel       //unsigned, RW, default = 0
//Bit 1:0    reserved

//===========================================================================
// -----------------------------------------------
// REG_BASE:  VPP_POST_UNIT2_VCBUS_BASE = 0x2c
// -----------------------------------------------
//===========================================================================
//`include "disp_if_regs.h"
//
// Reading file:  ./vpp_post_unit2_reg.h
//
//===========================================================================
// Video postprocessing Registers
//===========================================================================
//====================top====================
#define VPP_SLICE2_GCLK_CTRL                      0x2c00
//Bit 31:8   reserved
//Bit 7:6    align_fifo_gclk_ctrl//unsigned, RW, default = 0
//Bit 5:4    ofifo_gclk_ctrl     //unsigned, RW, default = 0
//Bit 3:2    top_gclk_ctrl       //unsigned, RW, default = 0
//Bit 1      reg_gclk_ctrl       //unsigned, RW, default = 0, reg_gclk_ctrl high bit, low bit is 0
//Bit 0      reserved
#define VPP_SLICE2_DOLBY_CTRL                     0x2c01
//Bit 31:11  reserved
//Bit 10     VPP_SLICE2_clip_ext_mode   //unsigned, RW, default = 0
//Bit 3      VPP_SLICE2_dolby3_en       //unsigned, RW, default = 0
//Bit 2      VPP_SLICE2_dpath_sel       //unsigned, RW, default = 0
//Bit 1:0    reserved

//===========================================================================
// -----------------------------------------------
// REG_BASE:  VPP_POST_UNIT3_VCBUS_BASE = 0x3e
// -----------------------------------------------
//===========================================================================
//`include "disp_if_regs.h"
//
// Reading file:  ./vpp_post_unit3_reg.h
//
//===========================================================================
// Video postprocessing Registers
//===========================================================================
//====================top====================
#define VPP_SLICE3_GCLK_CTRL                       0x3e00
//Bit 31:8   reserved
//Bit 7:6    align_fifo_gclk_ctrl//unsigned, RW, default = 0
//Bit 5:4    ofifo_gclk_ctrl     //unsigned, RW, default = 0
//Bit 3:2    top_gclk_ctrl       //unsigned, RW, default = 0
//Bit 1      reg_gclk_ctrl       //unsigned, RW, default = 0, reg_gclk_ctrl high bit, low bit is 0
//Bit 0      reserved
#define VPP_SLICE3_DOLBY_CTRL                      0x3e01
//Bit 31:11  reserved
//Bit 10     VPP_SLICE3_clip_ext_mode   //unsigned, RW, default = 0
//Bit 3      VPP_SLICE3_dolby3_en       //unsigned, RW, default = 0
//Bit 2      VPP_SLICE3_dpath_sel       //unsigned, RW, default = 0
//Bit 1:0    reserved

//===========================================================================
// -----------------------------------------------
// REG_BASE:  DV_TOP_VCBUS_BASE = 0x10
// -----------------------------------------------
//===========================================================================
//
// Reading file:  ./vpu_primesl_regs.h
//
// synopsys translate_off
// synopsys translate_on
#define VPU_DOLBY_TOP_CTRL                         0x10fd
//Bit 11  "reg_dv3_en       // RW, default = 0, dolby core3 enable
//Bit 10  "reg_prime_enable // RW, default = 0, primesl enable
//Bit 9   "reg_dv_di_en     // RW, default = 0, di dolby enable
//Bit 8   "reg_dv_tv_en      // RW, default = 0, dolby tv enable
//Bit 7:0 "reg_gclk_ctrl    // RW, default = 0, bit1:0 reg_pm_vd2_ctrl
//bit3:2 reg_arb_ctrl bit5:4 reg_pm_gclk bit7:6 reg_ram_gclk

#define VPU_DOLBY_GATE_CTRL                        0x10fe
//Bit 10   "gclk_pm_en                // unsigned, RW, default = 0
//Bit 9    "gclk_dv3_en               // unsigned, RW, default = 0
//Bit 8    "gclk_osd3_en              // unsigned, RW, default = 0
//Bit 7    "gclk_osd2_en              // unsigned, RW, default = 0
//Bit 6    "gclk_osd1_en              // unsigned, RW, default = 0
//Bit 5    "gclk_vd2_en               // unsigned, RW, default = 0
//Bit 4    "gclk_vd1s3_en             // unsigned, RW, default = 0
//Bit 3    "gclk_vd1s2_en             // unsigned, RW, default = 0
//Bit 2    "gclk_vd1s1_en             // unsigned, RW, default = 0
//Bit 1    "gclk_vd1s0_en             // unsigned, RW, default = 0
//Bit 0    "gclk_dv_tv_en              // unsigned, RW, default = 0

#define VPU_DOLBY_INT_STAT                         0x10ff
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ./vpu_primesl_regs.h

// REG_BASE:  OSD_SYS_VCBUS_BASE = 0x60
// -----------------------------------------------
//==================================================
//
// Reading file:  ./osd_sys_reg.h

#define OSD_DOLBY_BYPASS_EN                        0x6077
//Bit   31:8      reserved
//Bit   7 :7      osd4_dolby_din_ext_mode        //unsigned , RW, default = 0
//Bit   6 :6      osd4_dolby_bypass_en           //unsigned , RW, default = 0
//Bit   5 :5      osd3_dolby_din_ext_mode        //unsigned , RW, default = 0
//Bit   4 :4      osd3_dolby_bypass_en           //unsigned , RW, default = 0
//Bit   3 :3      osd2_dolby_din_ext_mode        //unsigned , RW, default = 0
//Bit   2 :2      osd2_dolby_bypass_en           //unsigned , RW, default = 0
//Bit   1 :1      osd1_dolby_din_ext_mode        //unsigned , RW, default = 0
//Bit   0 :0      osd1_dolby_bypass_en           //unsigned , RW, default = 0
// Closing file:  ./osd_sys_reg.h

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

#endif
