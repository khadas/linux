/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __REG_DECONTOUR_T3_H__
#define __REG_DECONTOUR_T3_H__

/********************************
 * note: for t3
 *	base on t5:
 *		0x49xx -> 0x4bxx
 *		0x4axx not change
 ********************************/
#define DCTR_T3_PATH                                  (0x4b00)
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
#define INTRP_T3_PARAM                                (0x4b01)
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
#define DCTR_T3_BGRID_PARAM1                          (0x4b02)
//Bit 31:26        reserved
//Bit 25:16        reg_grd_xnum
	// unsigned ,    RW, default = 80  number of grid in horizontal
	//dimension, value = [0-80]
//Bit 15:10        reserved
//Bit  9: 0        reg_grd_ynum
	// unsigned ,    RW, default = 45  number of grid in vertical
	//dimension, value = [0-45]
#define DCTR_T3_BGRID_PARAM2                          (0x4b03)
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
#define DCTR_T3_BGRID_PARAM3                          (0x4b04)
//Bit 31:26        reserved
//Bit 25:16        reg_grd_xnum_use
	// unsigned ,    RW, default = 80  number of grid in horizontal
	//dimension, value = [0-80]
//Bit 15:10        reserved
//Bit  9: 0        reg_grd_ynum_use
	// unsigned ,    RW, default = 45  number of grid in vertical
	//dimension, value = [0-45]
#define DCTR_T3_BGRID_PARAM4                          (0x4b05)
//Bit 31:24        reserved
//Bit 23:16        reg_grd_xsize_ds
	// unsigned ,    RW, default = 12
//Bit 15: 8        reserved
//Bit  7: 0        reg_grd_ysize_ds
	// unsigned ,    RW, default = 12
#define DCTR_T3_BGRID_PARAM5                          (0x4b06)
//Bit 31:24        reserved
//Bit 23:17        reserved
//Bit 16: 0        reg_grd_xidx_div
	// unsigned ,    RW, default = 0  controlled by SW
#define DCTR_T3_BGRID_PARAM6                          (0x4b07)
//Bit 31:24        reserved
//Bit 23:17        reserved
//Bit 16: 0        reg_grd_yidx_div
	// unsigned ,    RW, default = 0  controlled by SW
#define DCTR_T3_BGRID_PARAM7                          (0x4b08)
//Bit 31:24        reserved
//Bit 23:18        reserved
//Bit 17: 8        reg_slc_prt_th
	// unsigned ,    RW, default = 200  slicing protection threshold
//Bit  7: 1        reserved
//Bit  0           reg_slc_prt_en
	// unsigned ,    RW, default = 0  slicing protection enable bit
#define DCTR_T3_BGRID_PARAM8_0                        (0x4b09)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_0         // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_1         // unsigned ,    RW, default = 48
#define DCTR_T3_BGRID_PARAM8_1                        (0x4b0a)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_2         // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_3         // unsigned ,    RW, default = 48
#define DCTR_T3_BGRID_PARAM8_2                        (0x4b0b)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_4         // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_5         // unsigned ,    RW, default = 48
#define DCTR_T3_BGRID_PARAM8_3                        (0x4b0c)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_6         // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_7         // unsigned ,    RW, default = 48
#define DCTR_T3_BGRID_PARAM8_4                        (0x4b0d)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_8         // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_9         // unsigned ,    RW, default = 48
#define DCTR_T3_BGRID_PARAM8_5                        (0x4b0e)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_10        // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_11        // unsigned ,    RW, default = 48
#define DCTR_T3_BGRID_PARAM8_6                        (0x4b0f)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_12        // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_13        // unsigned ,    RW, default = 48
#define DCTR_T3_BGRID_PARAM8_7                        (0x4b10)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_14        // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_15        // unsigned ,    RW, default = 48
#define DCTR_T3_BGRID_PARAM8_8                        (0x4b11)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_16        // unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_17
	// unsigned ,    RW, default = 48
#define DCTR_T3_BGRID_PARAM8_9                        (0x4b12)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_18
	// unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_19
	// unsigned ,    RW, default = 48
#define DCTR_T3_BGRID_PARAM8_10                       (0x4b13)
//Bit 31:27        reserved
//Bit 26:16        reg_grd_vbin_gs_20
	// unsigned ,    RW, default = 48
//Bit 15:11        reserved
//Bit 10: 0        reg_grd_vbin_gs_21
	// unsigned ,    RW, default = 48
#define DCTR_T3_SIGFIL                                (0x4b14)
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
#define DCTR_T3_DIVR1                                 (0x4b15)
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
#define DCTR_T3_DIVR2                                 (0x4b16)
//Bit 31:25        reserved
//Bit 24:16        reg_divrsmap_mag1
	// signed ,    RW, default = 8  value = [-128~127];
//Bit 15: 9        reserved
//Bit  8: 0        reg_divrsmap_mag2
	// signed ,    RW, default = -16  value = [-128~127];
#define DCTR_T3_DIVR3                                 (0x4b17)
//Bit 31:24        reserved
//Bit 23:16        reserved
//Bit 15: 8        reg_divrsmap_clp1
	// unsigned ,    RW, default = 30
//Bit  7: 0        reg_divrsmap_clp2
	// unsigned ,    RW, default = 64
#define DCTR_T3_DIVR4                                 (0x4b18)
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
#define DCTR_T3_DIRMAP                                (0x4b19)
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
#define DCTR_T3_DIVR_CURVE1_0                         (0x4b1a)
//Bit 31:24        reg_divr_curvr_x1_0       //unsigned ,    RW, default = 32
//Bit 23:16        reg_divr_curvr_y1_0       //unsigned ,    RW, default = 32
//Bit 15: 0        reg_divr_curvr_slp1_0     //unsigned ,    RW, default = 1024
#define DCTR_T3_DIVR_CURVE2_0                         (0x4b1b)
//Bit 31:24        reg_divr_curvr_x2_0       //unsigned ,    RW, default = 32
//Bit 23:16        reg_divr_curvr_y2_0       //unsigned ,    RW, default = 32
//Bit 15: 0        reg_divr_curvr_slp2_0     //unsigned ,    RW, default = 1024
#define DCTR_T3_DIR_CURVE1_0                          (0x4b1c)
//Bit 31:24        reg_dir_curvr_x_0         //unsigned ,    RW, default = 30
//Bit 23:16        reg_dir_curvr_y_0         //unsigned ,    RW, default = 0
//Bit 15: 0        reg_dir_curvr_slp_0       //unsigned ,    RW, default = 0
#define DCTR_T3_DIVR_CURVE1_1                         (0x4b1d)
//Bit 31:24        reg_divr_curvr_x1_1       //unsigned ,    RW, default = 64
//Bit 23:16        reg_divr_curvr_y1_1       //unsigned ,    RW, default = 64
//Bit 15: 0        reg_divr_curvr_slp1_1     //unsigned ,    RW, default = 1024
#define DCTR_T3_DIVR_CURVE2_1                         (0x4b1e)
//Bit 31:24        reg_divr_curvr_x2_1       //unsigned ,    RW, default = 64
//Bit 23:16        reg_divr_curvr_y2_1       //unsigned ,    RW, default = 64
//Bit 15: 0        reg_divr_curvr_slp2_1     //unsigned ,    RW, default = 1024
#define DCTR_T3_DIR_CURVE1_1                          (0x4b1f)
//Bit 31:24        reg_dir_curvr_x_1         //unsigned ,    RW, default = 45
//Bit 23:16        reg_dir_curvr_y_1         //unsigned ,    RW, default = 20
//Bit 15: 0        reg_dir_curvr_slp_1       //unsigned ,    RW, default = 1365
#define DCTR_T3_DIVR_CURVE1_2                         (0x4b20)
//Bit 31:24        reg_divr_curvr_x1_2       //unsigned ,    RW, default = 96
//Bit 23:16        reg_divr_curvr_y1_2       //unsigned ,    RW, default = 96
//Bit 15: 0        reg_divr_curvr_slp1_2     //unsigned ,    RW, default = 1024
#define DCTR_T3_DIVR_CURVE2_2                         (0x4b21)
//Bit 31:24        reg_divr_curvr_x2_2       //unsigned ,    RW, default = 96
//Bit 23:16        reg_divr_curvr_y2_2       //unsigned ,    RW, default = 96
//Bit 15: 0        reg_divr_curvr_slp2_2     //unsigned ,    RW, default = 1024
#define DCTR_T3_DIR_CURVE1_2                          (0x4b22)
//Bit 31:24        reg_dir_curvr_x_2         //unsigned ,    RW, default = 60
//Bit 23:16        reg_dir_curvr_y_2         //unsigned ,    RW, default = 90
//Bit 15: 0        reg_dir_curvr_slp_2       //unsigned ,    RW, default = 4779
#define DCTR_T3_DIVR_CURVE1_3                         (0x4b23)
//Bit 31:24        reg_divr_curvr_x1_3       //unsigned ,    RW, default = 128
//Bit 23:16        reg_divr_curvr_y1_3       //unsigned ,    RW, default = 128
//Bit 15: 0        reg_divr_curvr_slp1_3     //unsigned ,    RW, default = 1024
#define DCTR_T3_DIVR_CURVE2_3                         (0x4b24)
//Bit 31:24        reg_divr_curvr_x2_3       //unsigned ,    RW, default = 128
//Bit 23:16        reg_divr_curvr_y2_3       //unsigned ,    RW, default = 128
//Bit 15: 0        reg_divr_curvr_slp2_3     //unsigned ,    RW, default = 1024
#define DCTR_T3_DIR_CURVE1_3                          (0x4b25)
//Bit 31:24        reg_dir_curvr_x_3
	// unsigned ,    RW, default = 100
//Bit 23:16        reg_dir_curvr_y_3
	// unsigned ,    RW, default = 160
//Bit 15: 0        reg_dir_curvr_slp_3
	// unsigned ,    RW, default = 1792
#define DCTR_T3_DIVR_CURVE1_4                         (0x4b26)
//Bit 31:24        reg_divr_curvr_x1_4
	// unsigned ,    RW, default = 150
//Bit 23:16        reg_divr_curvr_y1_4
	// unsigned ,    RW, default = 150
//Bit 15: 0        reg_divr_curvr_slp1_4
	// unsigned ,    RW, default = 1024
#define DCTR_T3_DIVR_CURVE2_4                         (0x4b27)
//Bit 31:24        reg_divr_curvr_x2_4
	// unsigned ,    RW, default = 150
//Bit 23:16        reg_divr_curvr_y2_4
	// unsigned ,    RW, default = 150
//Bit 15: 0        reg_divr_curvr_slp2_4
	// unsigned ,    RW, default = 1024
#define DCTR_T3_DIR_CURVE1_4                          (0x4b28)
//Bit 31:24        reg_dir_curvr_x_4
	// unsigned ,    RW, default = 180
//Bit 23:16        reg_dir_curvr_y_4
	// unsigned ,    RW, default = 220
//Bit 15: 0        reg_dir_curvr_slp_4
	// unsigned ,    RW, default = 768
#define DCTR_T3_DIVR_CURVE1_5                         (0x4b29)
//Bit 31:24        reg_divr_curvr_x1_5
	// unsigned ,    RW, default = 255
//Bit 23:16        reg_divr_curvr_y1_5
	// unsigned ,    RW, default = 255
//Bit 15: 0        reg_divr_curvr_slp1_5
	// unsigned ,    RW, default = 1024
#define DCTR_T3_DIVR_CURVE2_5                         (0x4b2a)
//Bit 31:24        reg_divr_curvr_x2_5
	// unsigned ,    RW, default = 255
//Bit 23:16        reg_divr_curvr_y2_5
	// unsigned ,    RW, default = 255
//Bit 15: 0        reg_divr_curvr_slp2_5
	// unsigned ,    RW, default = 1024
#define DCTR_T3_DIR_CURVE1_5                          (0x4b2b)
//Bit 31:24        reg_dir_curvr_x_5
	// unsigned ,    RW, default = 200
//Bit 23:16        reg_dir_curvr_y_5
	// unsigned ,    RW, default = 255
//Bit 15: 0        reg_dir_curvr_slp_5
	// unsigned ,    RW, default = 1792
#define DCTR_T3_BLENDING1                             (0x4b2c)
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
#define DCTR_T3_BLENDING2                             (0x4b2d)
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
#define DCTR_T3_PMEM_MAP1                             (0x4b2e)
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
#define DCTR_T3_PMEM_MAP2                             (0x4b2f)
//Bit 31:24        reserved
//Bit 23:16        reg_pmap_colorprotect_gain
	// unsigned ,    RW, default = 64  scale to the color protection
	//strength, the larger the more protection, norm to 64 as 1.0
//Bit 15: 9        reserved
//Bit  8: 0        reg_pmap_colorprotect_mag
	// signed ,    RW, default = 0  magnitude of the color
	//protection module
#define DCTR_T3_PMEM_SCL_LUT_0                        (0x4b30)
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
#define DCTR_T3_PMEM_SCL_LUT_1                        (0x4b31)
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
#define DCTR_T3_PMEM_SCL_LUT_8                        (0x4b32)
//Bit 31: 6        reserved
//Bit  5: 0        reg_pmap_luma_scl_lut_8
	// unsigned ,    RW, default = 32  luma based scale on dc_map,
	//normalized to 32 as 1.0. the larger, the more DC filtered
#define DCTR_T3_RAND_SEED_0                           (0x4b33)
//Bit 31: 0        reg_rand_seeds_0
	// unsigned ,    RW, default = 32'ha0a52f23  seeds of the rand PRBS_4
#define DCTR_T3_RAND_SEED_1                           (0x4b34)
//Bit 31: 0        reg_rand_seeds_1
	// unsigned ,    RW, default = 32'h70a52f27  seeds of the rand PRBS_4
#define DCTR_T3_RAND_SEED_2                           (0x4b35)
//Bit 31: 0        reg_rand_seeds_2
	// unsigned ,    RW, default = 32'h63a52f22  seeds of the rand PRBS_4
#define DCTR_T3_RAND_EN                               (0x4b36)
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
#define DCTR_T3_RAND_GAIN1                            (0x4b37)
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
#define DCTR_T3_RAND_GAIN2                            (0x4b38)
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
#define DCTR_T3_STA_HIST_0                            (0x4b39)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_0             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_1                            (0x4b3a)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_1             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_2                            (0x4b3b)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_2             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_3                            (0x4b3c)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_3             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_4                            (0x4b3d)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_4             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_5                            (0x4b3e)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_5             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_6                            (0x4b3f)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_6             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_7                            (0x4b40)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_7             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_8                            (0x4b41)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_8             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_9                            (0x4b42)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_9             // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_10                           (0x4b43)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_10            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_11                           (0x4b44)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_11            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_12                           (0x4b45)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_12            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_13                           (0x4b46)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_13            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_14                           (0x4b47)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_14            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_15                           (0x4b48)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_15            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_16                           (0x4b49)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_16            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_17                           (0x4b4a)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_17            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_18                           (0x4b4b)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_18            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_19                           (0x4b4c)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_19            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_20                           (0x4b4d)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_20            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_21                           (0x4b4e)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_21            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_22                           (0x4b4f)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_22            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_23                           (0x4b50)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_23            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_24                           (0x4b51)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_24            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_25                           (0x4b52)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_25            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_26                           (0x4b53)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_26            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_27                           (0x4b54)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_27            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_28                           (0x4b55)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_28            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_29                           (0x4b56)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_29            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_30                           (0x4b57)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_30            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_STA_HIST_31                           (0x4b58)
//Bit 31:24        reserved
//Bit 23: 0        ro_sta_hist_31            // unsigned ,    RO, default = 0
	//frame step sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_0                            (0x4b59)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_0             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_1                            (0x4b5a)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_1             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_2                            (0x4b5b)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_2             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_3                            (0x4b5c)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_3             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_4                            (0x4b5d)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_4             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_5                            (0x4b5e)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_5             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_6                            (0x4b5f)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_6             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_7                            (0x4b60)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_7             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_8                            (0x4b61)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_8             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_9                            (0x4b62)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_9             // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_10                           (0x4b63)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_10            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_11                           (0x4b64)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_11            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_12                           (0x4b65)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_12            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_13                           (0x4b66)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_13            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_14                           (0x4b67)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_14            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_MAP_HIST_15                           (0x4b68)
//Bit 31:24        reserved
//Bit 23: 0        ro_map_hist_15            // unsigned ,    RO, default = 0
	//output frame map sta, for per-frame SW use
#define DCTR_T3_FLAT_COUNT                            (0x4b69)
//Bit 31:24        reserved
//Bit 23: 0        ro_flat_c                 // unsigned ,    RO, default = 0
	//frame flat pixel cont, for per-frame SW use
#define DCTR_T3_FIF_COUNT                             (0x4b6a)
//Bit 31:24        reserved
//Bit 23: 0        ro_fif_c                  // unsigned ,    RO, default = 0
	//frame flat in flat pixel cont, for per-frame SW use
#define DCTR_T3_STA                                   (0x4b6b)
//Bit 31:16        reserved
//Bit 15:10        reserved
//Bit  9: 8        reg_sta_map_opt           // unsigned ,    RW, default = 0
		//0:dir map, 1: divr_map, 2:divr_map_final
//Bit  7: 0        reg_sta_flat_th           // unsigned ,    RW, default = 160
		//threshold for flat area
#define DCTR_T3_DEMO                                  (0x4b6c)
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
#define DCTR_T3_COLOR_LUT_ADDR                        (0x4b80)
#define DCTR_T3_COLOR_LUT_DATA                        (0x4b81)
#define DCTR_T3_DITHER_LUT_ADDR                       (0x4b82)
#define DCTR_T3_DITHER_LUT_DATA                       (0x4b83)
#define DCTR_T3_DIVR_CURVE_ADDR                       (0x4b84)
#define DCTR_T3_DIVR_CURVE_DATA                       (0x4b85)
#define DCTR_T3_DIVRLUT_ADDR                          (0x4b86)
#define DCTR_T3_DIVRLUT_DATA                          (0x4b87)
#define REG_DCTR_T3_GCLK_CTRL0                        (0x4b88)
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
#define REG_DCTR_T3_GCLK_CTRL1                        (0x4b89)
//Bit 31:8        reserved
//Bit 7:6         reg_reg_wrap_gclk_ctrl
	// unsigned,   RW, default = 0 clk gating ctrl signal
//Bit 5:4         reg_divrmap_extra_ds2	// unsigned,   RW, default = 0
//Bit 3:2         reg_divrmap_extra_ds1	// unsigned,   RW, default = 0
//Bit 1:0         reg_divrmap_extra_ds0	// unsigned,   RW, default = 0
#define REG_DCTR_T3_BUF_DEP0                          (0x4b8a)
//Bit 31:26       reserved
//Bit 25:13       reg_divrmap_buf_dep1	// unsigned,   RW, default = 1600
//Bit 12:0        reg_divrmap_buf_dep0	// unsigned,   RW, default = 6000
#define REG_DCTR_T3_BUF_DEP1                          (0x4b8b)
//Bit 31:26       reserved
//Bit 25:13       reg_dir_det_buf_dep	// unsigned,   RW, default = 5200
//Bit 12:0        reg_avg_flt_buf_dep	// unsigned,   RW, default = 1200
#define REG_DCTR_T3_UV_HW_CTRL0                       (0x4b8c)
//Bit 31          reserved
//Bit 30          reg_uv_hw_ctrl_en	// unsigned,   RW, default = 0
//Bit 29:28       reg_uv_shrk_mode_v	// unsigned,   RW, default = 0
//Bit 27:26       reg_uv_shrk_mode_h	// unsigned,   RW, default = 0
//Bit 25:13       reg_uv_in_vsize	// unsigned,   RW, default = 0
//Bit 12:0        reg_uv_in_hsize	// unsigned,   RW, default = 0
#define REG_DCTR_T3_WIN_CTRL0                         (0x4b8d)
//Bit 31:28       reserved
//Bit 27          reg_debug_out_en	// unsigned,   RW, default = 0
//Bit 26          reg_win_en		// unsigned,   RW, default = 0
//Bit 25:13       reg_win_bgn_h		// unsigned,   RW, default = 0
//Bit 12:0        reg_win_end_h		// unsigned,   RW, default = 1919
#define REG_DCTR_T3_WIN_CTRL1                         (0x4b8e)
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

#define DCNTR_T3_DIVR_RMIF_CTRL1                      (0x4b90)
#define DCNTR_T3_DIVR_RMIF_CTRL2                      (0x4b91)
#define DCNTR_T3_DIVR_RMIF_CTRL3                      (0x4b92)
#define DCNTR_T3_DIVR_RMIF_CTRL4                      (0x4b93)
#define DCNTR_T3_DIVR_RMIF_SCOPE_X                    (0x4b94)
#define DCNTR_T3_DIVR_RMIF_SCOPE_Y                    (0x4b95)
#define DCNTR_T3_DIVR_RMIF_RO_STAT                    (0x4b96)
#define DCNTR_T3_YFLT_RMIF_CTRL1                      (0x4ba0)
#define DCNTR_T3_YFLT_RMIF_CTRL2                      (0x4ba1)
#define DCNTR_T3_YFLT_RMIF_CTRL3                      (0x4ba2)
#define DCNTR_T3_YFLT_RMIF_CTRL4                      (0x4ba3)
#define DCNTR_T3_YFLT_RMIF_SCOPE_X                    (0x4ba4)
#define DCNTR_T3_YFLT_RMIF_SCOPE_Y                    (0x4ba5)
#define DCNTR_T3_YFLT_RMIF_RO_STAT                    (0x4ba6)
#define DCNTR_T3_CFLT_RMIF_CTRL1                      (0x4bb0)
#define DCNTR_T3_CFLT_RMIF_CTRL2                      (0x4bb1)
#define DCNTR_T3_CFLT_RMIF_CTRL3                      (0x4bb2)
#define DCNTR_T3_CFLT_RMIF_CTRL4                      (0x4bb3)
#define DCNTR_T3_CFLT_RMIF_SCOPE_X                    (0x4bb4)
#define DCNTR_T3_CFLT_RMIF_SCOPE_Y                    (0x4bb5)
#define DCNTR_T3_CFLT_RMIF_RO_STAT                    (0x4bb6)
#define DCNTR_T3_GRID_RMIF_CTRL1                      (0x4bc0)
#define DCNTR_T3_GRID_RMIF_CTRL2                      (0x4bc1)
#define DCNTR_T3_GRID_RMIF_CTRL3                      (0x4bc2)
#define DCNTR_T3_GRID_RMIF_CTRL4                      (0x4bc3)
#define DCNTR_T3_GRID_RMIF_SCOPE_X                    (0x4bc4)
#define DCNTR_T3_GRID_RMIF_SCOPE_Y                    (0x4bc5)
#define DCNTR_T3_GRID_RMIF_RO_STAT                    (0x4bc6)
#define DCNTR_T3_POST_FMT_CTRL                        (0x4bca)
#define DCNTR_T3_POST_FMT_W                           (0x4bcb)
#define DCNTR_T3_POST_FMT_H                           (0x4bcc)
#define DCNTR_T3_POST_ARB_MODE                        (0x4bd0)
#define DCNTR_T3_POST_ARB_REQEN_SLV                   (0x4bd1)
#define DCNTR_T3_POST_ARB_WEIGH0_SLV                  (0x4bd2)
#define DCNTR_T3_POST_ARB_WEIGH1_SLV                  (0x4bd3)
#define DCNTR_T3_POST_ARB_UGT                         (0x4bd4)
#define DCNTR_T3_POST_ARB_LIMT0                       (0x4bd5)
#define DCNTR_T3_POST_ARB_STATUS                      (0x4bd6)
#define DCNTR_T3_POST_ARB_DBG_CTRL                    (0x4bd7)
#define DCNTR_T3_POST_ARB_PROT                        (0x4bd8)
#define DCNTR_T3_POST_ARB_PROT_STAT                   (0x4bd9)

#endif /*__REG_DECONTOUR_T3_H__*/
