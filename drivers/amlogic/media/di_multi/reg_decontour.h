/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/reg_decontour.h
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

#ifndef __REG_DECONTOUR_H__
#define __REG_DECONTOUR_H__

// Reading file:  dcntr_post_regs.h
//
//`define DCNTR1_VCBUS_BASE      8'h4a
// 0x00-0x7f
//
// Reading file:  dcntr_post_regs0.h
//
// synopsys translate_off
// synopsys translate_on
#define DCTR_PATH                                  (0x4900)
//Bit 31            reserved
//Bit 30:29        reg_decontour_enable_0
	// unsigned ,    RW, default = 3  enable of the decountour processing,
	//yuv separately; 0: org, 1, bilgrd, 2: pavgbgrd_blender,
	//3 final_all_blender
//Bit 28:27        reg_decontour_enable_1
	// unsigned ,    RW, default = 3  enable of the decountour processing,
	//yuv separately; 0: org, 1, bilgrd, 2: pavgbgrd_blender,
	//3 final_all_blender
//Bit 26           reg_map_path
	// unsigned ,    RW, default = 0  input selection of grid build,
	//0:DS input, 1: Ori input
//Bit 25           reg_grd_path
	// unsigned ,    RW, default = 0  input selection of grid build,
	//0:DS input, 1: Ori input
//Bit 24           reg_sig_path
	// unsigned ,    RW, default = 0  input selection for SiG flt,
	//value = [0,1], 0:DS in,   1:Ori in
//Bit 23            reserved
//Bit 22:19        reg_bit_in
	// unsigned , RW, default = 10  10bit or 12bit source input bit-width
//Bit 18           reg_dc_en
	// unsigned ,    RW, default = 1  general enable bit
//Bit 17:16        reg_in_ds_rate_x
	// unsigned ,    RW, default = 2  Input down-sample registers,
	//normally AVG. value = [0,1,2], change according to input resolution.
	//real rate is 2^reg_in_ds_rate
//Bit 15            reserved
//Bit 14:13        reg_in_ds_phs_x
	// unsigned ,    RW, default = 0
	//Input down-sample x phase on decimation mode
//Bit 12:11        reg_in_ds_phs_y
	// unsigned ,    RW, default = 0
	//Input down-sample y phase on decimation mode
//Bit 10: 9        reg_intep_phs_x
	// unsigned ,    RW, default = 0
	//Interpolation x phase on decimation mode
//Bit  8: 7        reg_intep_phs_y
	// unsigned ,    RW, default = 0
	//Interpolation y phase on decimation mode
//Bit  6: 3        reg_in_ds_bit
	// unsigned ,    RW, default = 8
	//Input down-sample register, value = [8,10], 8:8bits, 10: 10bits
//Bit  2           reg_in_ds_mode
	// unsigned ,    RW, default = 0
	//Input down-sample registers, normally AVG. 0: AVG, 1:decimation
//Bit  1: 0        reg_in_ds_rate_y
	// unsigned ,    RW, default = 2  Input down-sample registers, normally
	//AVG. value = [0,1,2], change according to input resolution.
	//real rate is 2^reg_in_ds_rate
#define INTRP_PARAM                                (0x4901)
//Bit 31:26        reserved
//Bit 25:21        reg_intep_phs_x_rtl
	// signed ,    RW, default = 0  Interpolation x phase used,
	//could be negative num, set by SW
//Bit 20:16        reg_intep_phs_x_use
	// signed ,    RW, default = 0  Interpolation x phase used,
	//could be negative num, set by SW
//Bit 15:10        reserved
//Bit  9: 5        reg_intep_phs_y_rtl
	// signed ,    RW, default = 0  Interpolation x phase used,
	//could be negative num, set by SW
//Bit  4: 0        reg_intep_phs_y_use
	// signed ,    RW, default = 0  Interpolation y phase used,
	//could be negative num, set by SW
#define DCTR_BGRID_PARAM1                          (0x4902)
//Bit 31:26        reserved
//Bit 25:16        reg_grd_xnum
	// unsigned ,    RW, default = 80  number of grid in horizontal
	//dimension, value = [0-80]
//Bit 15:10        reserved
//Bit  9: 0        reg_grd_ynum
	// unsigned ,    RW, default = 45  number of grid in vertical
	//dimension, value = [0-45]
#define DCTR_BGRID_PARAM2                          (0x4903)
//Bit 31:24        reg_grd_xsize
	// unsigned ,    RW, default = 48  horizontal_size of each grid
	//in pixels,   value = calculated by FW
//Bit 23:16        reg_grd_ysize
	// unsigned ,    RW, default = 48  vertical_size mod of each grid
	//in pixels, value = calculated by FW
//Bit 15: 8        reg_grd_valsz
	// unsigned ,    RW, default = 48  pixel value mod of each grid of
	//10bits value ,  value = calculated by FW
//Bit  7: 0        reg_grd_vnum
	// unsigned ,    RW, default = 22  number of grid in luminance
	//dimension, value = 22
#define DCTR_BGRID_PARAM3                          (0x4904)
//Bit 31:26        reserved
//Bit 25:16        reg_grd_xnum_use
	// unsigned ,    RW, default = 80  number of grid in horizontal
	//dimension, value = [0-80]
//Bit 15:10        reserved
//Bit  9: 0        reg_grd_ynum_use
	// unsigned ,    RW, default = 45  number of grid in vertical
	//dimension, value = [0-45]
#define DCTR_BGRID_PARAM4                          (0x4905)
//Bit 31:24        reserved
//Bit 23:16        reg_grd_xsize_ds
	// unsigned ,    RW, default = 12
//Bit 15: 8        reserved
//Bit  7: 0        reg_grd_ysize_ds
	// unsigned ,    RW, default = 12
#define DCTR_BGRID_PARAM5                          (0x4906)
//Bit 31:24        reserved
//Bit 23:17        reserved
//Bit 16: 0        reg_grd_xidx_div
	// unsigned ,    RW, default = 0  controlled by SW
#define DCTR_BGRID_PARAM6                          (0x4907)
//Bit 31:24        reserved
//Bit 23:17        reserved
//Bit 16: 0        reg_grd_yidx_div
	// unsigned ,    RW, default = 0  controlled by SW
#define DCTR_BGRID_PARAM7                          (0x4908)
//Bit 31:24        reserved
//Bit 23:18        reserved
//Bit 17: 8        reg_slc_prt_th
	// unsigned ,    RW, default = 200  slicing protection threshold
//Bit  7: 1        reserved
//Bit  0           reg_slc_prt_en
	// unsigned ,    RW, default = 0  slicing protection enable bit
#define DCTR_BGRID_PARAM8_0                        (0x4909)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_0         // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_1         // unsigned ,    RW, default = 48
#define DCTR_BGRID_PARAM8_1                        (0x490a)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_2         // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_3         // unsigned ,    RW, default = 48
#define DCTR_BGRID_PARAM8_2                        (0x490b)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_4         // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_5         // unsigned ,    RW, default = 48
#define DCTR_BGRID_PARAM8_3                        (0x490c)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_6         // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_7         // unsigned ,    RW, default = 48
#define DCTR_BGRID_PARAM8_4                        (0x490d)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_8         // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_9         // unsigned ,    RW, default = 48
#define DCTR_BGRID_PARAM8_5                        (0x490e)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_10        // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_11        // unsigned ,    RW, default = 48
#define DCTR_BGRID_PARAM8_6                        (0x490f)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_12        // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_13        // unsigned ,    RW, default = 48
#define DCTR_BGRID_PARAM8_7                        (0x4910)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_14        // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_15        // unsigned ,    RW, default = 48
#define DCTR_BGRID_PARAM8_8                        (0x4911)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_16        // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_17
	// unsigned ,    RW, default = 48
#define DCTR_BGRID_PARAM8_9                        (0x4912)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_18
	// unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_19
	// unsigned ,    RW, default = 48
#define DCTR_BGRID_PARAM8_10                       (0x4913)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_20
	// unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_21
	// unsigned ,    RW, default = 48
#define DCTR_SIGFIL                                (0x4914)
//Bit 31:24        reserved
//Bit 23:16        reg_sig_thr
	// unsigned ,    RW, default = 64  of sigma filtering
//Bit 15            reserved
//Bit 14: 8        reg_sig_win_h
	// unsigned ,    RW, default = 2  window of sigma filtering
	//= 2*reg_sig_win_h+1, value = 2;
//Bit  7: 4        reg_sig_win_v
	// unsigned ,    RW, default = 1  window of sigma filtering
	// = 2*reg_sig_win_v+1, value = 1;
//Bit  3: 2        reg_sig_ds_r_x
	// unsigned ,    RW, default = 0  ratio for AVG 0 ,1 or 2,
	//value = calculated by FW
//Bit  1: 0        reg_sig_ds_r_y
	// unsigned ,    RW, default = 0  ratio for AVG 0 ,1 or 2,
	//value = calculated by FW
#define DCTR_DIVR1                                 (0x4915)
//Bit 31            reserved
//Bit 30           reg_divrsmap_enable
	// unsigned ,    RW, default = 1  enable of the
	//diversity map detection;
//Bit 29:27        reg_divrsmap_bit1
	// unsigned ,    RW, default = 3  shift bits1
//Bit 26:24        reg_divrsmap_bit2
	// unsigned ,    RW, default = 3  shift bits2
//Bit 23:16        reserved
//Bit 15: 8        reg_divrsmap_gain1
	// unsigned ,    RW, default = 96  magnitude. u2.6
//Bit  7: 0        reg_divrsmap_gain2
	// unsigned ,    RW, default = 64  magnitude. u2.6
#define DCTR_DIVR2                                 (0x4916)
//Bit 31:25        reserved
//Bit 24:16        reg_divrsmap_mag1
	// signed ,    RW, default = 8  value = [-128~127];
//Bit 15: 9        reserved
//Bit  8: 0        reg_divrsmap_mag2
	// signed ,    RW, default = -16  value = [-128~127];
#define DCTR_DIVR3                                 (0x4917)
//Bit 31:24        reserved
//Bit 23:16        reserved
//Bit 15: 8        reg_divrsmap_clp1
	// unsigned ,    RW, default = 30
//Bit  7: 0        reg_divrsmap_clp2
	// unsigned ,    RW, default = 64
#define DCTR_DIVR4                                 (0x4918)
//Bit 31            reserved
//Bit 30:28        reg_divrsmap_blk0_sft
	// unsigned ,    RW, default = 1  ds block 0 is
	//(1 left_sft reg_divrsmap_blk0_sft),value = [0,1,2];
//Bit 27            reserved
//Bit 26:24        reg_divrsmap_blk1_sft
	// unsigned ,    RW, default = 2  ds block 1 is
	///(1 left_sft reg_divrsmap_blk1_sft),value = [1,2,3];
//Bit 23            reserved
//Bit 22:20        reg_divrsmap_blk2_sft
	// unsigned ,    RW, default = 3  ds block 2 is
	//(1 left_sft reg_divrsmap_blk2_sft),value = [2,3,4];
//Bit 19            reserved
//Bit 18:16        reg_divrsmap_win
	// unsigned ,    RW, default = 4  diversity detection window,value = 4
//Bit 15: 8        reserved
//Bit  7: 4        reserved
//Bit  3: 2        reg_divrsmap_sel1
	// unsigned ,    RW, default = 0  map selection of map based on
	//blk0 and blk1, 0: bld, 1: map0 2:map1,value = [0,1,2]
//Bit  1: 0        reg_divrsmap_sel2
	// unsigned ,    RW, default = 0  map selection of map based on
	//blk1 and blk2, 0: bld, 1: map1 2:map2,value = [0,1,2]
#define DCTR_DIRMAP                                (0x4919)
//Bit 31           reg_dirmap_enable         // unsigned ,    RW, default = 1
//Bit 30           reg_dir_smgrd_en          // unsigned ,    RW, default = 0
//Bit 29:24        reg_dir_noise_th
	// unsigned ,    RW, default = 0  noise threshold
//Bit 23:16        reg_dir_gain
	// unsigned ,    RW, default = 128  u2.6
//Bit 15:12        reg_dir_win
	// unsigned ,    RW, default = 8  window  2*reg_dir_win,value =[5-8];
//Bit 11: 9        reserved
//Bit  8: 0        reg_dir_mag
	// signed,  RW, default = 0  for 1/2 DS 720p mode,value = [-128~127];
#define DCTR_DIVR_CURVE1_0                         (0x491a)
//Bit 31:24        reg_divr_curvr_x1_0       //unsigned ,    RW, default = 32
//Bit 23:16        reg_divr_curvr_y1_0       //unsigned ,    RW, default = 32
//Bit 15: 0        reg_divr_curvr_slp1_0     //unsigned ,    RW, default = 1024
#define DCTR_DIVR_CURVE2_0                         (0x491b)
//Bit 31:24        reg_divr_curvr_x2_0       //unsigned ,    RW, default = 32
//Bit 23:16        reg_divr_curvr_y2_0       //unsigned ,    RW, default = 32
//Bit 15: 0        reg_divr_curvr_slp2_0     //unsigned ,    RW, default = 1024
#define DCTR_DIR_CURVE1_0                          (0x491c)
//Bit 31:24        reg_dir_curvr_x_0         //unsigned ,    RW, default = 30
//Bit 23:16        reg_dir_curvr_y_0         //unsigned ,    RW, default = 0
//Bit 15: 0        reg_dir_curvr_slp_0       //unsigned ,    RW, default = 0
#define DCTR_DIVR_CURVE1_1                         (0x491d)
//Bit 31:24        reg_divr_curvr_x1_1       //unsigned ,    RW, default = 64
//Bit 23:16        reg_divr_curvr_y1_1       //unsigned ,    RW, default = 64
//Bit 15: 0        reg_divr_curvr_slp1_1     //unsigned ,    RW, default = 1024
#define DCTR_DIVR_CURVE2_1                         (0x491e)
//Bit 31:24        reg_divr_curvr_x2_1       //unsigned ,    RW, default = 64
//Bit 23:16        reg_divr_curvr_y2_1       //unsigned ,    RW, default = 64
//Bit 15: 0        reg_divr_curvr_slp2_1     //unsigned ,    RW, default = 1024
#define DCTR_DIR_CURVE1_1                          (0x491f)
//Bit 31:24        reg_dir_curvr_x_1         //unsigned ,    RW, default = 45
//Bit 23:16        reg_dir_curvr_y_1         //unsigned ,    RW, default = 20
//Bit 15: 0        reg_dir_curvr_slp_1       //unsigned ,    RW, default = 1365
#define DCTR_DIVR_CURVE1_2                         (0x4920)
//Bit 31:24        reg_divr_curvr_x1_2       //unsigned ,    RW, default = 96
//Bit 23:16        reg_divr_curvr_y1_2       //unsigned ,    RW, default = 96
//Bit 15: 0        reg_divr_curvr_slp1_2     //unsigned ,    RW, default = 1024
#define DCTR_DIVR_CURVE2_2                         (0x4921)
//Bit 31:24        reg_divr_curvr_x2_2       //unsigned ,    RW, default = 96
//Bit 23:16        reg_divr_curvr_y2_2       //unsigned ,    RW, default = 96
//Bit 15: 0        reg_divr_curvr_slp2_2     //unsigned ,    RW, default = 1024
#define DCTR_DIR_CURVE1_2                          (0x4922)
//Bit 31:24        reg_dir_curvr_x_2         //unsigned ,    RW, default = 60
//Bit 23:16        reg_dir_curvr_y_2         //unsigned ,    RW, default = 90
//Bit 15: 0        reg_dir_curvr_slp_2       //unsigned ,    RW, default = 4779
#define DCTR_DIVR_CURVE1_3                         (0x4923)
//Bit 31:24        reg_divr_curvr_x1_3       //unsigned ,    RW, default = 128
//Bit 23:16        reg_divr_curvr_y1_3       //unsigned ,    RW, default = 128
//Bit 15: 0        reg_divr_curvr_slp1_3     //unsigned ,    RW, default = 1024
#define DCTR_DIVR_CURVE2_3                         (0x4924)
//Bit 31:24        reg_divr_curvr_x2_3       //unsigned ,    RW, default = 128
//Bit 23:16        reg_divr_curvr_y2_3       //unsigned ,    RW, default = 128
//Bit 15: 0        reg_divr_curvr_slp2_3     //unsigned ,    RW, default = 1024
#define DCTR_DIR_CURVE1_3                          (0x4925)
//Bit 31:24        reg_dir_curvr_x_3
	// unsigned ,    RW, default = 100
//Bit 23:16        reg_dir_curvr_y_3
	// unsigned ,    RW, default = 160
//Bit 15: 0        reg_dir_curvr_slp_3
	// unsigned ,    RW, default = 1792
#define DCTR_DIVR_CURVE1_4                         (0x4926)
//Bit 31:24        reg_divr_curvr_x1_4
	// unsigned ,    RW, default = 150
//Bit 23:16        reg_divr_curvr_y1_4
	// unsigned ,    RW, default = 150
//Bit 15: 0        reg_divr_curvr_slp1_4
	// unsigned ,    RW, default = 1024
#define DCTR_DIVR_CURVE2_4                         (0x4927)
//Bit 31:24        reg_divr_curvr_x2_4
	// unsigned ,    RW, default = 150
//Bit 23:16        reg_divr_curvr_y2_4
	// unsigned ,    RW, default = 150
//Bit 15: 0        reg_divr_curvr_slp2_4
	// unsigned ,    RW, default = 1024
#define DCTR_DIR_CURVE1_4                          (0x4928)
//Bit 31:24        reg_dir_curvr_x_4
	// unsigned ,    RW, default = 180
//Bit 23:16        reg_dir_curvr_y_4
	// unsigned ,    RW, default = 220
//Bit 15: 0        reg_dir_curvr_slp_4
	// unsigned ,    RW, default = 768
#define DCTR_DIVR_CURVE1_5                         (0x4929)
//Bit 31:24        reg_divr_curvr_x1_5
	// unsigned ,    RW, default = 255
//Bit 23:16        reg_divr_curvr_y1_5
	// unsigned ,    RW, default = 255
//Bit 15: 0        reg_divr_curvr_slp1_5
	// unsigned ,    RW, default = 1024
#define DCTR_DIVR_CURVE2_5                         (0x492a)
//Bit 31:24        reg_divr_curvr_x2_5
	// unsigned ,    RW, default = 255
//Bit 23:16        reg_divr_curvr_y2_5
	// unsigned ,    RW, default = 255
//Bit 15: 0        reg_divr_curvr_slp2_5
	// unsigned ,    RW, default = 1024
#define DCTR_DIR_CURVE1_5                          (0x492b)
//Bit 31:24        reg_dir_curvr_x_5
	// unsigned ,    RW, default = 200
//Bit 23:16        reg_dir_curvr_y_5
	// unsigned ,    RW, default = 255
//Bit 15: 0        reg_dir_curvr_slp_5
	// unsigned ,    RW, default = 1792
#define DCTR_BLENDING1                             (0x492c)
//Bit 31:24        reg_avg_cor_th
	// unsigned ,    RW, default = 10  AVG de-contouring threshold
//Bit 23:16        reg_big_cor_th
	// unsigned ,    RW, default = 18  BiG de-contouring threshold
//Bit 15           reg_flt_cor_en
	// unsigned ,    RW, default = 1  AVG and BiG coring enable
//Bit 14:13        reg_map_bld_mode
	// unsigned ,    RW, default = 2  Blending mode of diversmap and final
	//detail map, 2bits,  1:final map,1:diversity map, 2:MIN, 3:blend
//Bit 12: 8        reg_map_bld_alpha
	// unsigned ,    RW, default = 10  blending alpha of map, [0,16]
//Bit  7: 0        reserved
#define DCTR_BLENDING2                             (0x492d)
//Bit 31:25        reserved
//Bit 24:16        reg_final_bld_gain
	// unsigned ,    RW, default = 0  0~256   TODO
//Bit 15:14        reserved
//Bit 13:12        reg_bld1_mode
	// unsigned ,    RW, default = 2  blending 1 mode, 0: AVG flt,
	//1: BiG flt, 2:blending value = [0,1,2];
//Bit 11: 0        reg_pmap_manual_alp
	// unsigned ,    RW, default = 0  manual setting for decontour
	//alpha blender, the larger the more filter
#define DCTR_PMEM_MAP1                             (0x492e)
//Bit 31           reg_pmap_detail_enable
	// unsigned ,    RW, default = 1  enable of the detail map for
	//post map detection
//Bit 30           reg_pmap_luma_scl_enable
	//unsigned ,    RW, default = 0  enable of luma based scale for DC
	//strength, 0: no luma scl on dc_map; 1: enable of luma scl on dc_map
//Bit 29           reg_pmap_colorprt_enable
	//unsigned, RW, default = 0 enable of color
	//protection based on the color
//Bit 28           reg_pmap_manual_enable
	// unsigned ,    RW, default = 0  enable for manual alpha for dc filter
	//result blender
//Bit 27:26        reg_pmap_luma_scl_sel
	// unsigned ,    RW, default = 2  selection of the luma for luma based
	//scale 0: original; 1 avg; 2/up: bilateral_grid results
//Bit 25:24        reg_pmap_colorprotect_sel
	// unsigned ,    RW, default = 1  selection of the UV for color
	//protection desion, 0: original; 1: avg; 2/up: bilateral_grid results
//Bit 23:16        reg_pmap_luma_gain
	// unsigned ,    RW, default = 64  u2.6
//Bit 15: 9        reserved
//Bit  8: 0        reg_pmap_luma_mag
	// signed ,    RW, default = 0  luma map magnitude
#define DCTR_PMEM_MAP2                             (0x492f)
//Bit 31:24        reserved
//Bit 23:16        reg_pmap_colorprotect_gain
	// unsigned ,    RW, default = 64  scale to the color protection
	//strength, the larger the more protection, norm to 64 as 1.0
//Bit 15: 9        reserved
//Bit  8: 0        reg_pmap_colorprotect_mag
	// signed ,    RW, default = 0  magnitude of the color
	//protection module
#define DCTR_PMEM_SCL_LUT_0                        (0x4930)
//Bit 31:30        reserved
//Bit 29:24        reg_pmap_luma_scl_lut_0
	// unsigned ,    RW, default = 8  luma based scale on dc_map,
	//normalized to 32 as 1.0. the larger, the more DC filtered
//Bit 23:22        reserved
//Bit 21:16        reg_pmap_luma_scl_lut_1
	// unsigned ,    RW, default = 16  luma based scale on dc_map,
	//normalized to 32 as 1.0. the larger, the more DC filtered
//Bit 15:14        reserved
//Bit 13: 8        reg_pmap_luma_scl_lut_2
	// unsigned ,    RW, default = 32  luma based scale on dc_map,
	//normalized to 32 as 1.0. the larger, the more DC filtered
//Bit  7: 6        reserved
//Bit  5: 0        reg_pmap_luma_scl_lut_3
	// unsigned ,    RW, default = 32  luma based scale on dc_map,
	//normalized to 32 as 1.0. the larger, the more DC filtered
#define DCTR_PMEM_SCL_LUT_1                        (0x4931)
//Bit 31:30        reserved
//Bit 29:24        reg_pmap_luma_scl_lut_4
	// unsigned ,    RW, default = 32  luma based scale on dc_map,
	//normalized to 32 as 1.0. the larger, the more DC filtered
//Bit 23:22        reserved
//Bit 21:16        reg_pmap_luma_scl_lut_5
	// unsigned ,    RW, default = 32  luma based scale on dc_map,
	//normalized to 32 as 1.0. the larger, the more DC filtered
//Bit 15:14        reserved
//Bit 13: 8        reg_pmap_luma_scl_lut_6
	// unsigned ,    RW, default = 32  luma based scale on dc_map,
	//normalized to 32 as 1.0. the larger, the more DC filtered
//Bit  7: 6        reserved
//Bit  5: 0        reg_pmap_luma_scl_lut_7
	// unsigned ,    RW, default = 32  luma based scale on dc_map,
	//normalized to 32 as 1.0. the larger, the more DC filtered
#define DCTR_PMEM_SCL_LUT_8                        (0x4932)
//Bit 31: 6        reserved
//Bit  5: 0        reg_pmap_luma_scl_lut_8
	// unsigned ,    RW, default = 32  luma based scale on dc_map,
	//normalized to 32 as 1.0. the larger, the more DC filtered
#define DCTR_RAND_SEED_0                           (0x4933)
//Bit 31: 0        reg_rand_seeds_0
	// unsigned ,    RW, default = 32'ha0a52f23  seeds of the rand PRBS_4
#define DCTR_RAND_SEED_1                           (0x4934)
//Bit 31: 0        reg_rand_seeds_1
	// unsigned ,    RW, default = 32'h70a52f27  seeds of the rand PRBS_4
#define DCTR_RAND_SEED_2                           (0x4935)
//Bit 31: 0        reg_rand_seeds_2
	// unsigned ,    RW, default = 32'h63a52f22  seeds of the rand PRBS_4
#define DCTR_RAND_EN                               (0x4936)
//Bit 31           reg_rand_xorlut_0         // unsigned ,    RW, default = 0
	//for whether do xor
//Bit 30           reg_rand_xorlut_1         // unsigned ,    RW, default = 1
	//for whether do xor
//Bit 29           reg_rand_xorlut_2         // unsigned ,    RW, default = 0
	//for whether do xor
//Bit 28           reg_rand_xorlut_3         // unsigned ,    RW, default = 1
	//for whether do xor
//Bit 27           reg_rand_xorlut_4         // unsigned ,    RW, default = 1
	//for whether do xor
//Bit 26           reg_rand_xorlut_5         // unsigned ,    RW, default = 0
	//for whether do xor
//Bit 25           reg_rand_xorlut_6         // unsigned ,    RW, default = 0
	//for whether do xor
//Bit 24           reg_rand_xorlut_7         // unsigned ,    RW, default = 1
	//for whether do xor
//Bit 23           reg_rand_xorlut_8         // unsigned ,    RW, default = 0
	//for whether do xor
//Bit 22           reg_rand_xorlut_9         // unsigned ,    RW, default = 0
	//for whether do xor
//Bit 21           reg_rand_xorlut_10        // unsigned ,    RW, default = 1
	//for whether do xor
//Bit 20           reg_rand_xorlut_11        // unsigned ,    RW, default = 1
	//for whether do xor
//Bit 19           reg_rand_xorlut_12        // unsigned ,    RW, default = 1
	//for whether do xor
//Bit 18           reg_rand_xorlut_13        // unsigned ,    RW, default = 1
	//for whether do xor
//Bit 17           reg_rand_xorlut_14        // unsigned ,    RW, default = 0
	//for whether do xor
//Bit 16           reg_rand_xorlut_15        // unsigned ,    RW, default = 0
	//for whether do xor
//Bit 15:14        reserved
//Bit 13           reg_rand_frm_rst          // unsigned,     RW, default = 0
	//pulse, rst frm_cnt
//Bit 12           reg_rand_enable_0         // unsigned ,    RW, default = 0
	//enable to add random noise for luma
//Bit 11           reg_rand_enable_1         // unsigned ,    RW, default = 0
	//enable to add random noise for u
//Bit 10           reg_rand_enable_2         // unsigned ,    RW, default = 0
	//enable to add random noise for v
//Bit  9           reg_rand_reset_0          // unsigned ,    RW, default = 1
	//reset of the rand seeds for luma
//Bit  8           reg_rand_reset_1          // unsigned ,    RW, default = 1
	//reset of the rand seeds for u
//Bit  7           reg_rand_reset_2          // unsigned ,    RW, default = 1
	//reset of the rand seeds for v
//Bit  6           reg_dth_en_0              // unsigned ,    RW, default = 1
	//enable of the dither for y
//Bit  5           reg_dth_en_1              // unsigned ,    RW, default = 1
	//enable of the dither for u
//Bit  4           reg_dth_en_2              // unsigned ,    RW, default = 1
	//enable of the dither for v
//Bit  3: 2        reg_dth_mod               // unsigned ,    RW, default = 1
	//mode for yuv
//Bit  1: 0        reg_dth_mod_BiG           // unsigned ,    RW, default = 3
	//mode for BiG
#define DCTR_RAND_GAIN1                            (0x4937)
//Bit 31:24        reg_rand_noise_gain_0     // unsigned ,    RW, default = 8
	//gain to the random before added to the signal, normalize to 8 as 1.0
//Bit 23:16        reg_rand_noise_gain_1     // unsigned ,    RW, default = 8
	//gain to the random before added to the signal, normalize to 8 as 1.0
//Bit 15: 8        reg_randgain_dcmap_thrd_0 // unsigned ,    RW, default = 80
	//thrd to mapu8(finmap_u12 right_sft 4) for strength , mapu8<th[0],
	//strn=0; mapu8<th[1], strn=1,
//Bit  7: 0        reg_randgain_dcmap_thrd_1 // unsigned ,    RW, default = 128
	//thrd to mapu8(finmap_u12 right_sft 4) for strength ,
	//mapu8<th[0], strn=0;
	//mapu8<th[1], strn=1,
#define DCTR_RAND_GAIN2                            (0x4938)
//Bit 31:24        reg_randgain_dcmap_thrd_2 // unsigned ,    RW, default = 150
	//thrd to mapu8(finmap_u12 right_sft 4) for strength ,
	//mapu8<th[0], strn=0;
	//mapu8<th[1], strn=1,
//Bit 23:16        reg_randgain_dcmap_thrd_3 // unsigned ,    RW, default = 180
	//thrd to mapu8(finmap_u12 right_sft 4) for strength ,
	//mapu8<th[0], strn=0;
	//mapu8<th[1], strn=1,
//Bit 15: 8        reg_randgain_dcmap_thrd_4 // unsigned ,    RW, default = 208
	//thrd to mapu8(finmap_u12 right_sft 4) for strength ,
	//mapu8<th[0], strn=0;
	//mapu8<th[1], strn=1,
//Bit  7: 0        reg_randgain_dcmap_thrd_5 // unsigned ,    RW, default = 230
	//thrd to mapu8(finmap_u12 right_sft 4) for strength ,
	//mapu8<th[0], strn=0;
	//mapu8<th[1], strn=1,
#define DCTR_STA_HIST_0                            (0x4939)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_0             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_1                            (0x493a)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_1             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_2                            (0x493b)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_2             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_3                            (0x493c)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_3             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_4                            (0x493d)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_4             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_5                            (0x493e)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_5             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_6                            (0x493f)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_6             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_7                            (0x4940)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_7             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_8                            (0x4941)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_8             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_9                            (0x4942)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_9             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_10                           (0x4943)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_10            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_11                           (0x4944)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_11            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_12                           (0x4945)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_12            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_13                           (0x4946)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_13            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_14                           (0x4947)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_14            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_15                           (0x4948)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_15            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_16                           (0x4949)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_16            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_17                           (0x494a)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_17            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_18                           (0x494b)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_18            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_19                           (0x494c)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_19            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_20                           (0x494d)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_20            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_21                           (0x494e)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_21            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_22                           (0x494f)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_22            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_23                           (0x4950)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_23            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_24                           (0x4951)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_24            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_25                           (0x4952)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_25            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_26                           (0x4953)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_26            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_27                           (0x4954)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_27            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_28                           (0x4955)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_28            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_29                           (0x4956)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_29            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_30                           (0x4957)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_30            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_STA_HIST_31                           (0x4958)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_31            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_MAP_HIST_0                            (0x4959)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_0             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_1                            (0x495a)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_1             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_2                            (0x495b)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_2             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_3                            (0x495c)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_3             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_4                            (0x495d)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_4             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_5                            (0x495e)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_5             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_6                            (0x495f)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_6             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_7                            (0x4960)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_7             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_8                            (0x4961)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_8             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_9                            (0x4962)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_9             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_10                           (0x4963)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_10            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_11                           (0x4964)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_11            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_12                           (0x4965)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_12            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_13                           (0x4966)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_13            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_14                           (0x4967)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_14            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_MAP_HIST_15                           (0x4968)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_15            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_FLAT_COUNT                            (0x4969)
//Bit 31:24        reserved
//Bit 23: 0        ro_flat_c                 // unsigned ,    RO, default = 0
	//frame flat pixel cont, for per-frame SW use
#define DCTR_FIF_COUNT                             (0x496a)
//Bit 31:24        reserved
//Bit 23: 0        ro_fif_c                  // unsigned ,    RO, default = 0
	//frame flat in flat pixel cont, for per-frame SW use
#define DCTR_STA                                   (0x496b)
//Bit 31:16        reserved
//Bit 15:10        reserved
//Bit  9: 8        reg_sta_map_opt           // unsigned ,    RW, default = 0
		//0:dir map, 1: divr_map, 2:divr_map_final
//Bit  7: 0        reg_sta_flat_th           // unsigned ,    RW, default = 160
		//threshold for flat area
#define DCTR_DEMO                                  (0x496c)
//Bit 31:24        reg_map_debug_sel         // unsigned ,    RW, default = 2
		//debug map false color/level to show on image, 0: no debug
//Bit 23:22        reg_map_debug_mod         // unsigned ,    RW, default = 1
		//debug mode, 0: output, 1:luma only, 2:false color
//Bit 21:18        reserved
//Bit 17:16        reg_demo_mode             // unsigned ,    RW, default = 0
		//0: off, 1: left on, right off, 2: left off, right on
//Bit 15:12        reserved
//Bit 11: 0        reg_demo_mode_mid         // unsigned ,    RW, default = 1920
		//mid line of the demo,[1-3840]
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  dcntr_post_regs0.h
//
// 0x80-0x8f
//
// Reading file:  dcntr_post_regs1.h
//
// synopsys translate_off
// synopsys translate_on
#define DCTR_COLOR_LUT_ADDR                        (0x4980)
#define DCTR_COLOR_LUT_DATA                        (0x4981)
#define DCTR_DITHER_LUT_ADDR                       (0x4982)
#define DCTR_DITHER_LUT_DATA                       (0x4983)
#define DCTR_DIVR_CURVE_ADDR                       (0x4984)
#define DCTR_DIVR_CURVE_DATA                       (0x4985)
#define DCTR_DIVRLUT_ADDR                          (0x4986)
#define DCTR_DIVRLUT_DATA                          (0x4987)
#define REG_DCTR_GCLK_CTRL0                        (0x4988)
//Bit 31:30       reg_pstmem_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 29:24       reg_slicing_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 23:22       reg_dcntr_core_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 21:20       reg_avg_flt_y_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 19:18       reg_avg_flt_uv_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 17:16       reg_dir_det_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 17:16       reg_dir_flt_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 15:14       reg_sta_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 13:12       reg_divrmap_wrap_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 11:10       reg_divrmap_calc2_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 9:8         reg_divrmap_calc1_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 7:6         reg_divrmap_calc0_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 5:4         reg_divrmap_unit2_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 3:2         reg_divrmap_unit1_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 1:0         reg_divrmap_unit0_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
#define REG_DCTR_GCLK_CTRL1                        (0x4989)
//Bit 31:8        reserved
//Bit 7:6         reg_reg_wrap_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 5:4         reg_divrmap_extra_ds2	// unsigned,   RW, default = 0
//Bit 3:2         reg_divrmap_extra_ds1	// unsigned,   RW, default = 0
//Bit 1:0         reg_divrmap_extra_ds0	// unsigned,   RW, default = 0
#define REG_DCTR_BUF_DEP0                          (0x498a)
//Bit 31:26       reserved
//Bit 25:13       reg_divrmap_buf_dep1	// unsigned,   RW, default = 1600
//Bit 12:0        reg_divrmap_buf_dep0	// unsigned,   RW, default = 6000
#define REG_DCTR_BUF_DEP1                          (0x498b)
//Bit 31:26       reserved
//Bit 25:13       reg_dir_det_buf_dep	// unsigned,   RW, default = 5200
//Bit 12:0        reg_avg_flt_buf_dep	// unsigned,   RW, default = 1200
#define REG_DCTR_UV_HW_CTRL0                       (0x498c)
//Bit 31          reserved
//Bit 30          reg_uv_hw_ctrl_en	// unsigned,   RW, default = 0
//Bit 29:28       reg_uv_shrk_mode_v	// unsigned,   RW, default = 0
//Bit 27:26       reg_uv_shrk_mode_h	// unsigned,   RW, default = 0
//Bit 25:13       reg_uv_in_vsize	// unsigned,   RW, default = 0
//Bit 12:0        reg_uv_in_hsize	// unsigned,   RW, default = 0
#define REG_DCTR_WIN_CTRL0                         (0x498d)
//Bit 31:28       reserved
//Bit 27          reg_debug_out_en	// unsigned,   RW, default = 0
//Bit 26          reg_win_en		// unsigned,   RW, default = 0
//Bit 25:13       reg_win_bgn_h		// unsigned,   RW, default = 0
//Bit 12:0        reg_win_end_h		// unsigned,   RW, default = 1919
#define REG_DCTR_WIN_CTRL1                         (0x498e)
//Bit 31:26       reserved
//Bit 25:13       reg_win_bgn_v		// unsigned,   RW, default = 0
//Bit 12:0        reg_win_end_v		// unsigned,   RW, default = 1079
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  dcntr_post_regs1.h
//
// 0x90-0xdf
//
// Reading file:  dcntr_post_mif.h
//
// synopsys translate_off
// synopsys translate_on

//ary#define DCNTR_DIVR_RDMIF_BADDR 0x90
//ary#define DCNTR_YFLT_RDMIF_BADDR 0xa0
//ary#define DCNTR_CFLT_RDMIF_BADDR 0xb0
//ary#define DCNTR_GRID_RDMIF_BADDR 0xc0
//ary#define DCNTR_AXIRD_BADDR      0xd0

#define DCNTR_DIVR_RMIF_CTRL1                      (0x4990)
#define DCNTR_DIVR_RMIF_CTRL2                      (0x4991)
#define DCNTR_DIVR_RMIF_CTRL3                      (0x4992)
#define DCNTR_DIVR_RMIF_CTRL4                      (0x4993)
#define DCNTR_DIVR_RMIF_SCOPE_X                    (0x4994)
#define DCNTR_DIVR_RMIF_SCOPE_Y                    (0x4995)
#define DCNTR_DIVR_RMIF_RO_STAT                    (0x4996)
#define DCNTR_YFLT_RMIF_CTRL1                      (0x49a0)
#define DCNTR_YFLT_RMIF_CTRL2                      (0x49a1)
#define DCNTR_YFLT_RMIF_CTRL3                      (0x49a2)
#define DCNTR_YFLT_RMIF_CTRL4                      (0x49a3)
#define DCNTR_YFLT_RMIF_SCOPE_X                    (0x49a4)
#define DCNTR_YFLT_RMIF_SCOPE_Y                    (0x49a5)
#define DCNTR_YFLT_RMIF_RO_STAT                    (0x49a6)
#define DCNTR_CFLT_RMIF_CTRL1                      (0x49b0)
#define DCNTR_CFLT_RMIF_CTRL2                      (0x49b1)
#define DCNTR_CFLT_RMIF_CTRL3                      (0x49b2)
#define DCNTR_CFLT_RMIF_CTRL4                      (0x49b3)
#define DCNTR_CFLT_RMIF_SCOPE_X                    (0x49b4)
#define DCNTR_CFLT_RMIF_SCOPE_Y                    (0x49b5)
#define DCNTR_CFLT_RMIF_RO_STAT                    (0x49b6)
#define DCNTR_GRID_RMIF_CTRL1                      (0x49c0)
#define DCNTR_GRID_RMIF_CTRL2                      (0x49c1)
#define DCNTR_GRID_RMIF_CTRL3                      (0x49c2)
#define DCNTR_GRID_RMIF_CTRL4                      (0x49c3)
#define DCNTR_GRID_RMIF_SCOPE_X                    (0x49c4)
#define DCNTR_GRID_RMIF_SCOPE_Y                    (0x49c5)
#define DCNTR_GRID_RMIF_RO_STAT                    (0x49c6)
#define DCNTR_POST_FMT_CTRL                        (0x49ca)
#define DCNTR_POST_FMT_W                           (0x49cb)
#define DCNTR_POST_FMT_H                           (0x49cc)
#define DCNTR_POST_ARB_MODE                        (0x49d0)
#define DCNTR_POST_ARB_REQEN_SLV                   (0x49d1)
#define DCNTR_POST_ARB_WEIGH0_SLV                  (0x49d2)
#define DCNTR_POST_ARB_WEIGH1_SLV                  (0x49d3)
#define DCNTR_POST_ARB_UGT                         (0x49d4)
#define DCNTR_POST_ARB_LIMT0                       (0x49d5)
#define DCNTR_POST_ARB_STATUS                      (0x49d6)
#define DCNTR_POST_ARB_DBG_CTRL                    (0x49d7)
#define DCNTR_POST_ARB_PROT                        (0x49d8)
#define DCNTR_POST_ARB_PROT_STAT                   (0x49d9)
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  dcntr_post_mif.h
//
//
// Closing file:  dcntr_post_regs.h
//
// -----------------------------------------------
// CBUS_BASE:  DCNTR1_VCBUS_BASE = 0x4a
// -----------------------------------------------
//
// Reading file:  dcntr_pre_regs.h
//
// synopsys translate_off
// synopsys translate_on
//`define DCNTR0_VCBUS_BASE              8'h49
#define DCTR_BGRID_PATH_PRE                        (0x4a00)
//Bit 31: 5        reserved
//Bit  4           reg_grd_path              // unsigned ,    RW, default = 0
	//input selection of grid build, 0:DS input, 1: Ori input
//Bit  3: 0        reg_bit_in                // unsigned ,    RW, default = 10
	//10bit or 12bit source input bit-width
#define DCTR_DS_PRE                                (0x4a01)
//Bit 31:13        reserved
//Bit 12:11        reg_in_ds_phs_x           // unsigned ,    RW, default = 0
	//Input down-sample x phase on decimation mode
//Bit 10: 9        reg_in_ds_phs_y           // unsigned ,    RW, default = 0
	//Input down-sample y phase on decimation mode
//Bit  8: 5        reg_in_ds_bit             // unsigned ,    RW, default = 8
	//Input down-sample register, value = [8,10], 8:8bits, 10: 10bits
//Bit  4           reg_in_ds_mode            // unsigned ,    RW, default = 0
	//Input down-sample registers, normally AVG. 0: AVG, 1:decimation
//Bit  3: 2        reg_in_ds_rate_x          // unsigned ,    RW, default = 2
	//Input down-sample registers, normally AVG. value = [0,1,2],
	//change according to input resolution. real rate is 2^reg_in_ds_rate
//Bit  1: 0        reg_in_ds_rate_y          // unsigned ,    RW, default = 2
	//Input down-sample registers, normally AVG. value = [0,1,2],
	//change according to input resolution. real rate is 2^reg_in_ds_rate
#define DCTR_BGRID_PARAM1_PRE                      (0x4a02)
//Bit 31:26        reserved
//Bit 25:16        reg_grd_xnum              // unsigned ,    RW, default = 80
	//number of grid in horizontal dimension, value = [0-80]
//Bit 15:10        reserved
//Bit  9: 0        reg_grd_ynum              // unsigned ,    RW, default = 45
	//number of grid in vertical dimension, value = [0-45]
#define DCTR_BGRID_PARAM2_PRE                      (0x4a03)
//Bit 31:24        reg_grd_xsize             // unsigned ,    RW, default = 48
	//horizontal_size of each grid in pixels,   value = calculated by FW
//Bit 23:16        reg_grd_ysize             // unsigned ,    RW, default = 48
	//vertical_size mod of each grid in pixels, value = calculated by FW
//Bit 15: 8        reg_grd_valsz             // unsigned ,    RW, default = 48
	//pixel value mod of each grid of 10bits value,
	//value = calculated by FW
//Bit  7: 0        reg_grd_vnum              // unsigned ,    RW, default = 22
	//number of grid in luminance dimension, value = 22
#define DCTR_BGRID_PARAM3_PRE                      (0x4a04)
//Bit 31:26        reserved
//Bit 25:16        reg_grd_xnum_use          // unsigned ,    RW, default = 80
	//number of grid in horizontal dimension, value = [0-80]
//Bit 15:10        reserved
//Bit  9: 0        reg_grd_ynum_use          // unsigned ,    RW, default = 45
	//number of grid in vertical dimension, value = [0-45]
#define DCTR_BGRID_PARAM4_PRE                      (0x4a05)
//Bit 31:24        reserved
//Bit 23:16        reg_grd_xsize_ds          // unsigned ,    RW, default = 12
//Bit 15: 8        reserved
//Bit  7: 0        reg_grd_ysize_ds          // unsigned ,    RW, default = 12
#define DCTR_BGRID_PARAM5_PRE_0                    (0x4a06)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_0         // unsigned ,    RW, default = 48
	//Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_1
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_PARAM5_PRE_1                    (0x4a07)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_2
	// unsigned ,    RW, default = 48  Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_3
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_PARAM5_PRE_2                    (0x4a08)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_4
	// unsigned ,    RW, default = 48  Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_5
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_PARAM5_PRE_3                    (0x4a09)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_6
	// unsigned ,    RW, default = 48  Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_7
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_PARAM5_PRE_4                    (0x4a0a)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_8
	// unsigned ,    RW, default = 48  Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_9
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_PARAM5_PRE_5                    (0x4a0b)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_10
	// unsigned ,    RW, default = 48  Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_11
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_PARAM5_PRE_6                    (0x4a0c)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_12
	// unsigned ,    RW, default = 48  Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_13
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_PARAM5_PRE_7                    (0x4a0d)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_14
	// unsigned ,    RW, default = 48  Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_15
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_PARAM5_PRE_8                    (0x4a0e)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_16
	// unsigned ,    RW, default = 48  Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_17
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_PARAM5_PRE_9                    (0x4a0f)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_18
	// unsigned ,    RW, default = 48  Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_19
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_PARAM5_PRE_10                   (0x4a10)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gb_20
	// unsigned ,    RW, default = 48  Controlled by sw
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gb_21
	// unsigned ,    RW, default = 48  Controlled by sw
#define DCTR_BGRID_GCLK_CTRL                       (0x4a11)
//Bit 31:0         reg_gclk_ctrl
	//unsigned, RW, default= 0;
#define DCTR_BGRID_WRAP_CTRL                       (0x4a12)
//Bit 31:1          reserved
//Bit 0             reg_grd_build_en
	//unsigned, RW, default=1;

//ary#define DCNTR_YDS_WRMIF_BADDR      0x20 //8'h20-8'h26
//ary#define DCNTR_CDS_WRMIF_BADDR      0x28 //8'h28-8'h2e
//ary#define DCNTR_GRD_WRMIF_BADDR      0x48 // 8'h48-8'h4e
//ary#define DCNTR_GRID_AXIWR_BADDR     0x50 // 8'h50-8'h59

#define DCNTR_YDS_WMIF_CTRL1                       (0x4a20)
#define DCNTR_YDS_WMIF_CTRL2                       (0x4a21)
#define DCNTR_YDS_WMIF_CTRL3                       (0x4a22)
#define DCNTR_YDS_WMIF_CTRL4                       (0x4a23)
#define DCNTR_YDS_WMIF_SCOPE_X                     (0x4a24)
#define DCNTR_YDS_WMIF_SCOPE_Y                     (0x4a25)
#define DCNTR_YDS_WMIF_RO_STAT                     (0x4a26)
#define DCNTR_CDS_WMIF_CTRL1                       (0x4a28)
#define DCNTR_CDS_WMIF_CTRL2                       (0x4a29)
#define DCNTR_CDS_WMIF_CTRL3                       (0x4a2a)
#define DCNTR_CDS_WMIF_CTRL4                       (0x4a2b)
#define DCNTR_CDS_WMIF_SCOPE_X                     (0x4a2c)
#define DCNTR_CDS_WMIF_SCOPE_Y                     (0x4a2d)
#define DCNTR_CDS_WMIF_RO_STAT                     (0x4a2e)
#define DCNTR_GRD_WMIF_CTRL1                       (0x4a48)
#define DCNTR_GRD_WMIF_CTRL2                       (0x4a49)
#define DCNTR_GRD_WMIF_CTRL3                       (0x4a4a)
#define DCNTR_GRD_WMIF_CTRL4                       (0x4a4b)
#define DCNTR_GRD_WMIF_SCOPE_X                     (0x4a4c)
#define DCNTR_GRD_WMIF_SCOPE_Y                     (0x4a4d)
#define DCNTR_GRD_WMIF_RO_STAT                     (0x4a4e)
#define DCNTR_PRE_ARB_MODE                         (0x4a50)
#define DCNTR_PRE_ARB_REQEN_SLV                    (0x4a51)
#define DCNTR_PRE_ARB_WEIGH0_SLV                   (0x4a52)
#define DCNTR_PRE_ARB_WEIGH1_SLV                   (0x4a53)
#define DCNTR_PRE_ARB_UGT                          (0x4a54)
#define DCNTR_PRE_ARB_LIMT0                        (0x4a55)
#define DCNTR_PRE_ARB_STATUS                       (0x4a56)
#define DCNTR_PRE_ARB_DBG_CTRL                     (0x4a57)
#define DCNTR_PRE_ARB_PROT                         (0x4a58)
#define DCNTR_PRE_ARB_PROT_STAT                    (0x4a59)
#define DCNTR_GRID_GEN_REG                         (0x4a30)
#define DCNTR_GRID_GEN_REG2                        (0x4a31)
#define DCNTR_GRID_CANVAS0                         (0x4a32)
#define DCNTR_GRID_LUMA_X0                         (0x4a33)
#define DCNTR_GRID_LUMA_Y0                         (0x4a34)
#define DCNTR_GRID_CHROMA_X0                       (0x4a35)
#define DCNTR_GRID_CHROMA_Y0                       (0x4a36)
#define DCNTR_GRID_RPT_LOOP                        (0x4a37)
#define DCNTR_GRID_LUMA0_RPT_PAT                   (0x4a38)
#define DCNTR_GRID_CHROMA0_RPT_PAT                 (0x4a39)
#define DCNTR_GRID_DUMMY_PIXEL                     (0x4a3a)
#define DCNTR_GRID_LUMA_FIFO_SIZE                  (0x4a3b)
#define DCNTR_GRID_RANGE_MAP_Y                     (0x4a3c)
#define DCNTR_GRID_RANGE_MAP_CB                    (0x4a3d)
#define DCNTR_GRID_RANGE_MAP_CR                    (0x4a3e)
#define DCNTR_GRID_URGENT_CTRL                     (0x4a3f)
#define DCNTR_GRID_GEN_REG3                        (0x4a40)
#define DCNTR_GRID_AXI_CMD_CNT                     (0x4a41)
#define DCNTR_GRID_AXI_RDAT_CNT                    (0x4a42)
#define DCNTR_GRID_FMT_CTRL                        (0x4a43)
#define DCNTR_GRID_FMT_W                           (0x4a44)

/* t3 */
#define DCNTR_GRID_BADDR_Y                         (0x4a60)
#define DCNTR_GRID_BADDR_CB                        (0x4a61)
#define DCNTR_GRID_BADDR_CR                        (0x4a62)
#define DCNTR_GRID_STRIDE_0                        (0x4a63)
#define DCNTR_GRID_STRIDE_1                        (0x4a64)

#define DCNTR_GRID_BADDR_Y_F1                      (0x4a65)
#define DCNTR_GRID_BADDR_CB_F1                     (0x4a66)
#define DCNTR_GRID_BADDR_CR_F1                     (0x4a67)
#define DCNTR_GRID_STRIDE_0_F1                     (0x4a68)
#define DCNTR_GRID_STRIDE_1_F1                     (0x4a69)

// 0xf0-0xff
//
// Reading file:  dcntr_grid_top_reg.h
//
// synopsys translate_off
// synopsys translate_on
// top reg
#define DCTR_BGRID_TOP_FSIZE                       (0x4af0)
//Bit 31: 26        reserved
//Bit 25: 13        reg_frm_hsize	// unsigned , RW, default = 1280
//Bit 12: 0         reg_frm_vsize	// unsigned , RW, default = 720
//
#define DCTR_BGRID_TOP_HDNUM                       (0x4af1)
//Bit 31: 26        reserved
//Bit 25: 13        reg_hold_hnum               // unsigned , RW, default = 0
//Bit 12: 0         reg_hold_vnum               // unsigned , RW, default = 2
#define DCTR_BGRID_TOP_CTRL0                       (0x4af2)
//Bit 31            reg_frm_rst              //unsigned, RW, default=0
//Bit 30            reg_sw_rst              //unsigned, RW, default=0
//Bit 29            reg_grdbuild_en         //unsigned, RW, default=1
//Bit 28:16         reg_stdly_num           //unsigned, RW, default=2
//Bit 15:5          reserved
//Bit 4             reg_hs_sel              //unsigned, RW, default=0
//Bit 3:2           reg_din_sel
	//unsigned, RW, default=1, dos vidin select
//Bit 1             reg_ds_mif_en
	//unsigned, RW, default=1, downsample switch
//Bit 0             reg_grd_mif_en
	//unsigned, RW, default=1, grid build switch
#define DCTR_BGRID_TOP_FMT                         (0x4af3)
//Bit 31:21         reserved
//Bit 20:19         reg_fmt_mode            //unsigned, RW, default=0
//Bit 18:16         reg_444to422_mode       //unsigned, RW, default=0
//Bit 15:13         reg_422to420_mode       //unsigned, RW, default=0
//Bit 12:0          reg_fmt_ybuf_depth      //unsigned, RW, default=0
//
#define DCTR_BGRID_TOP_GCLK                        (0x4af4)
//Bit 31:0           reg_gclk_ctrl           //unsigned, RW, default=0
#define DCTR_BGRID_TOP_HOLD                        (0x4af5)
//Bit 31:17         reserved
//Bit 16            reg_hold_en             //unsigned, RW, default=0;
//Bit 15:8          reg_pass_num            //unsigned, RW, default=1;
//Bit 7:0           reg_hold_num            //unsigned, RW, default=1;

#endif /*__REG_DECONTOUR_H__*/
