/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef __SYSTEM_AM_MD_H__
#define __SYSTEM_AM_MD_H__
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "acamera.h"
#include "acamera_fw.h"
#include <linux/kfifo.h>
#include <linux/ioport.h>

#define ISP_MD_REG_START                       0xfe023e00

#define MD_TOP_GCLK                            ISP_MD_REG_START + 4*0x0  //
//Bit 31: 0        reg_md_gclk               // unsigned ,    RW, default = 0  clock gate control
#define MD_TOP_CTRL                            ISP_MD_REG_START + 4*0x1  //
//Bit 31: 0        reg_md_top_ctrl           // unsigned ,    RW, default = 32'h204  md top ntrol
#define MD_WR_CTRL0                            ISP_MD_REG_START + 4*0x2  //
//Bit 31           reg_wr_axi_wr_en          // unsigned ,    RW, default = 1  axi wr enable
//Bit 30           reg_wr_axi_req_en         // unsigned ,    RW, default = 1  axi request enable
//Bit 29           reg_wr_axi_bypass         // unsigned ,    RW, default = 0  bypass axi wr
//Bit 28           reg_wr_frame_int          // unsigned ,    RW, default = 0  init wr frame
//Bit 27:24        reserved
//Bit 23: 0        reg_wr_total_size         // unsigned ,    RW, default = 57600  pixels hsize * vsize
#define MD_WR_CTRL1                            ISP_MD_REG_START + 4*0x3  //
//Bit 31: 0        reg_wr_base_addr          // unsigned ,    RW, default = 32'h80000000  wr axi base address
#define MD_WR_CTRL2                            ISP_MD_REG_START + 4*0x4  //
//Bit 31:23        reserved
//Bit 22:20        reg_wr_burst_lens         // unsigned ,    RW, default = 2  burst_lens limiation, burst_size: 0: 1x128, 1: 2x128, 2~3: 4x128
//Bit 19:16        reg_wr_req_th             // unsigned ,    RW, default = 4  fifo depth req_th * 8 *128 bits in fifo.
//Bit 15: 0        reg_wr_urgent_ctrl        // unsigned ,    RW, default = 0  urgent control
#define MD_WR_CTRL3                            ISP_MD_REG_START + 4*0x5  //
//Bit 31:24        reg_wr_awid               // unsigned ,    RW, default = 0  wr id
//Bit 23:21        reg_wr_awprot             // unsigned ,    RW, default = 0  awprot for security control
//Bit 20: 0        reserved
#define MD_RO_WR_ST0                           ISP_MD_REG_START + 4*0x6  //
//Bit 31: 0        ro_wr_st0                 // unsigned ,    RW, default = 0
#define MD_RO_WR_ST1                           ISP_MD_REG_START + 4*0x7  //
//Bit 31: 0        ro_wr_st1                 // unsigned ,    RW, default = 0
#define MD_RO_WR_ST2                           ISP_MD_REG_START + 4*0x8  //
//Bit 31: 0        ro_wr_st2                 // unsigned ,    RW, default = 0
#define MD_RD_CTRL0                            ISP_MD_REG_START + 4*0x9  //
//Bit 31           reg_rd_axi_rd_en          // unsigned ,    RW, default = 1  axi rd enable
//Bit 30           reg_rd_axi_req_en         // unsigned ,    RW, default = 1  axi request enable
//Bit 29:24        reserved
//Bit 23: 0        reg_rd_total_size         // unsigned ,    RW, default = 57600  total size hsize*vsize, <= 1024*1024
#define MD_RD_CTRL1                            ISP_MD_REG_START + 4*0xa  //
//Bit 31: 0        reg_rd_base_addr          // unsigned ,    RW, default = 32'h80000000
#define MD_RD_CTRL2                            ISP_MD_REG_START + 4*0xb  //
//Bit 31:23        reserved
//Bit 22:20        reg_rd_burst_lens         // unsigned ,    RW, default = 2  burst_lens limiation, burst_size: 0: 1x128, 1: 2x128, 2~3: 4x128
//Bit 19:16        reg_rd_req_th             // unsigned ,    RW, default = 4  fifo depth req_th * 8 *128 bits in fifo.
//Bit 15: 0        reg_rd_urgent_ctrl        // unsigned ,    RW, default = 0  urgent control
#define MD_RD_CTRL3                            ISP_MD_REG_START + 4*0xc  //
//Bit 31:24        reg_rd_arid               // unsigned ,    RW, default = 0  wr id
//Bit 23:21        reg_rd_arprot             // unsigned ,    RW, default = 0  awprot for security control
//Bit 20: 0        reserved
#define MD_RO_RD_ST0                           ISP_MD_REG_START + 4*0xd  //
//Bit 31: 0        ro_rd_st0                 // unsigned ,    RW, default = 0
#define MD_RO_RD_ST1                           ISP_MD_REG_START + 4*0xe  //
//Bit 31: 0        ro_rd_st1                 // unsigned ,    RW, default = 0
#define MD_INPUT_CTRL0                         ISP_MD_REG_START + 4*0xf  //
//Bit 31           reg_md_enable             // unsigned ,    RW, default = 1  lp motion detection enable, 0: disable;  1: enable
//Bit 30           reg_md_raw_xphase_ofst    // unsigned ,    RW, default = 0  horizontal phase of the raw data, 0: start wz R-col (RGGB or GBRG); 1: start wz B-col (GRBG or BGGR)
//Bit 29           reg_md_raw_yphase_ofst    // unsigned ,    RW, default = 0  vertical phase of the raw data,   0: start wz R-row (RGGB or GRBG); 1: start wz B-row (GRBG or BGGR)
//Bit 28           reg_md_is_on_raw          // unsigned ,    RW, default = 1  the md_sel is on raw data, set together with md_sel.    0: YUV/RGB;  1:RAW
//Bit 27:24        reg_md_input_sel          // unsigned ,    RW, default = 1  data selection for the lp motion detection. 0: raw sensor input; 1: raw WDR stitch; 2:raw fed_out(nr_in); 3:raw mirror_in; 4£º RGB after dms; 5: IRout; 6:RGB/YUV after gamma; 7: fe_o yuv bypass , default=1 
//Bit 23:20        reg_md_input_ls           // unsigned ,    RW, default = 0  E domain data left shift bit num to align with u20 Odomain data and will clip to u10 by droping 10lsb, e.g. yuv is u12, then ls = 8;
//Bit 19: 0        reserved
#define MD_INPUT_SIZE                         ISP_MD_REG_START + 4*0x10  //
//Bit 31:28        reserved
//Bit 27:16        reg_md_input_xsize        // unsigned ,    RW, default = 1920  xsize of the input in pixels, set to image xsize
//Bit 15:12        reserved
//Bit 11: 0        reg_md_input_ysize        // unsigned ,    RW, default = 1080  ysize of the input in pixels, set to image ysize
#define MD_WINXY_0                            ISP_MD_REG_START + 4*0x11  //
//Bit 31:28        reserved
//Bit 27:16        reg_md_inpc_winxxyy_0     // unsigned ,    RW, default = 0  input combined window for processing.
//Bit 15:12        reserved
//Bit 11: 0        reg_md_inpc_winxxyy_1     // unsigned ,    RW, default = 960  input combined window for processing.
#define MD_WINXY_1                            ISP_MD_REG_START + 4*0x12  //
//Bit 31:28        reserved
//Bit 27:16        reg_md_inpc_winxxyy_2     // unsigned ,    RW, default = 0  input combined window for processing.
//Bit 15:12        reserved
//Bit 11: 0        reg_md_inpc_winxxyy_3     // unsigned ,    RW, default = 540  input combined window for processing.
#define MD_RGGB_OFSET_0                       ISP_MD_REG_START + 4*0x13  //
//Bit 31:20        reserved
//Bit 19: 0        reg_md_rggb_ofset_0       // signed ,    RW, default = 0  ofset to the components, same as the ISP mission mode ofset
#define MD_RGGB_OFSET_1                       ISP_MD_REG_START + 4*0x14  //
//Bit 31:20        reserved
//Bit 19: 0        reg_md_rggb_ofset_1       // signed ,    RW, default = 0  ofset to the components, same as the ISP mission mode ofset
#define MD_RGGB_OFSET_2                       ISP_MD_REG_START + 4*0x15  //
//Bit 31:20        reserved
//Bit 19: 0        reg_md_rggb_ofset_2       // signed ,    RW, default = 0  ofset to the components, same as the ISP mission mode ofset
#define MD_RGGB_OFSET_3                       ISP_MD_REG_START + 4*0x16  //
//Bit 31:20        reserved
//Bit 19: 0        reg_md_rggb_ofset_3       // signed ,    RW, default = 0  ofset to the components, same as the ISP mission mode ofset
#define MD_RGGB_GAIN_0                        ISP_MD_REG_START + 4*0x17  //
//Bit 31:28        reserved
//Bit 27:16        reg_md_rggb_gain_0        // unsigned ,    RW, default = 1024  gain to different channel, normalize to 1024 as "1.0"
//Bit 15:12        reserved
//Bit 11: 0        reg_md_rggb_gain_1        // unsigned ,    RW, default = 1024  gain to different channel, normalize to 1024 as "1.0"
#define MD_RGGB_GAIN_1                        ISP_MD_REG_START + 4*0x18  //
//Bit 31:28        reserved
//Bit 27:16        reg_md_rggb_gain_2        // unsigned ,    RW, default = 1024  gain to different channel, normalize to 1024 as "1.0"
//Bit 15:12        reserved
//Bit 11: 0        reg_md_rggb_gain_3        // unsigned ,    RW, default = 1024  gain to different channel, normalize to 1024 as "1.0"
#define MD_RGGB_COEF                          ISP_MD_REG_START + 4*0x19  //
//Bit 31:24        reg_md_bld_coefs_0        // unsigned ,    RW, default = 32  blender coef of the components to Y, normalize to 128 as "1"
//Bit 23:16        reg_md_bld_coefs_1        // unsigned ,    RW, default = 32  blender coef of the components to Y, normalize to 128 as "1"
//Bit 15: 8        reg_md_bld_coefs_2        // unsigned ,    RW, default = 32  blender coef of the components to Y, normalize to 128 as "1"
//Bit  7: 0        reg_md_bld_coefs_3        // unsigned ,    RW, default = 32  blender coef of the components to Y, normalize to 128 as "1"
#define MD_COMB_EOTF_0                        ISP_MD_REG_START + 4*0x1a  //
//Bit 31:22        reg_md_comb_eotf_0        // unsigned ,    RW, default = 0  piec wise lut
//Bit 21:12        reg_md_comb_eotf_1        // unsigned ,    RW, default = 0  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_2        // unsigned ,    RW, default = 1  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_1                        ISP_MD_REG_START + 4*0x1b  //
//Bit 31:22        reg_md_comb_eotf_3        // unsigned ,    RW, default = 1  piec wise lut
//Bit 21:12        reg_md_comb_eotf_4        // unsigned ,    RW, default = 1  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_5        // unsigned ,    RW, default = 1  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_2                        ISP_MD_REG_START + 4*0x1c  //
//Bit 31:22        reg_md_comb_eotf_6        // unsigned ,    RW, default = 2  piec wise lut
//Bit 21:12        reg_md_comb_eotf_7        // unsigned ,    RW, default = 2  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_8        // unsigned ,    RW, default = 2  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_3                        ISP_MD_REG_START + 4*0x1d  //
//Bit 31:22        reg_md_comb_eotf_9        // unsigned ,    RW, default = 3  piec wise lut
//Bit 21:12        reg_md_comb_eotf_10       // unsigned ,    RW, default = 3  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_11       // unsigned ,    RW, default = 4  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_4                        ISP_MD_REG_START + 4*0x1e  //
//Bit 31:22        reg_md_comb_eotf_12       // unsigned ,    RW, default = 4  piec wise lut
//Bit 21:12        reg_md_comb_eotf_13       // unsigned ,    RW, default = 5  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_14       // unsigned ,    RW, default = 6  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_5                        ISP_MD_REG_START + 4*0x1f  //
//Bit 31:22        reg_md_comb_eotf_15       // unsigned ,    RW, default = 7  piec wise lut
//Bit 21:12        reg_md_comb_eotf_16       // unsigned ,    RW, default = 8  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_17       // unsigned ,    RW, default = 10  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_6                        ISP_MD_REG_START + 4*0x20  //
//Bit 31:22        reg_md_comb_eotf_18       // unsigned ,    RW, default = 12  piec wise lut
//Bit 21:12        reg_md_comb_eotf_19       // unsigned ,    RW, default = 14  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_20       // unsigned ,    RW, default = 16  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_7                        ISP_MD_REG_START + 4*0x21  //
//Bit 31:22        reg_md_comb_eotf_21       // unsigned ,    RW, default = 20  piec wise lut
//Bit 21:12        reg_md_comb_eotf_22       // unsigned ,    RW, default = 24  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_23       // unsigned ,    RW, default = 28  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_8                        ISP_MD_REG_START + 4*0x22  //
//Bit 31:22        reg_md_comb_eotf_24       // unsigned ,    RW, default = 32  piec wise lut
//Bit 21:12        reg_md_comb_eotf_25       // unsigned ,    RW, default = 40  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_26       // unsigned ,    RW, default = 48  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_9                        ISP_MD_REG_START + 4*0x23  //
//Bit 31:22        reg_md_comb_eotf_27       // unsigned ,    RW, default = 56  piec wise lut
//Bit 21:12        reg_md_comb_eotf_28       // unsigned ,    RW, default = 64  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_29       // unsigned ,    RW, default = 80  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_10                       ISP_MD_REG_START + 4*0x24  //
//Bit 31:22        reg_md_comb_eotf_30       // unsigned ,    RW, default = 96  piec wise lut
//Bit 21:12        reg_md_comb_eotf_31       // unsigned ,    RW, default = 112  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_32       // unsigned ,    RW, default = 128  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_11                       ISP_MD_REG_START + 4*0x25  //
//Bit 31:22        reg_md_comb_eotf_33       // unsigned ,    RW, default = 160  piec wise lut
//Bit 21:12        reg_md_comb_eotf_34       // unsigned ,    RW, default = 192  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_35       // unsigned ,    RW, default = 223  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_12                       ISP_MD_REG_START + 4*0x26  //
//Bit 31:22        reg_md_comb_eotf_36       // unsigned ,    RW, default = 256  piec wise lut
//Bit 21:12        reg_md_comb_eotf_37       // unsigned ,    RW, default = 288  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_38       // unsigned ,    RW, default = 320  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_13                       ISP_MD_REG_START + 4*0x27  //
//Bit 31:22        reg_md_comb_eotf_39       // unsigned ,    RW, default = 352  piec wise lut
//Bit 21:12        reg_md_comb_eotf_40       // unsigned ,    RW, default = 384  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_41       // unsigned ,    RW, default = 416  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_14                       ISP_MD_REG_START + 4*0x28  //
//Bit 31:22        reg_md_comb_eotf_42       // unsigned ,    RW, default = 448  piec wise lut
//Bit 21:12        reg_md_comb_eotf_43       // unsigned ,    RW, default = 480  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_44       // unsigned ,    RW, default = 512  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_15                       ISP_MD_REG_START + 4*0x29  //
//Bit 31:22        reg_md_comb_eotf_45       // unsigned ,    RW, default = 544  piec wise lut
//Bit 21:12        reg_md_comb_eotf_46       // unsigned ,    RW, default = 576  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_47       // unsigned ,    RW, default = 608  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_16                       ISP_MD_REG_START + 4*0x2a  //
//Bit 31:22        reg_md_comb_eotf_48       // unsigned ,    RW, default = 640  piec wise lut
//Bit 21:12        reg_md_comb_eotf_49       // unsigned ,    RW, default = 672  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_50       // unsigned ,    RW, default = 704  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_17                       ISP_MD_REG_START + 4*0x2b  //
//Bit 31:22        reg_md_comb_eotf_51       // unsigned ,    RW, default = 736  piec wise lut
//Bit 21:12        reg_md_comb_eotf_52       // unsigned ,    RW, default = 768  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_53       // unsigned ,    RW, default = 800  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_18                       ISP_MD_REG_START + 4*0x2c  //
//Bit 31:22        reg_md_comb_eotf_54       // unsigned ,    RW, default = 832  piec wise lut
//Bit 21:12        reg_md_comb_eotf_55       // unsigned ,    RW, default = 864  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_56       // unsigned ,    RW, default = 896  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_19                       ISP_MD_REG_START + 4*0x2d  //
//Bit 31:22        reg_md_comb_eotf_57       // unsigned ,    RW, default = 928  piec wise lut
//Bit 21:12        reg_md_comb_eotf_58       // unsigned ,    RW, default = 960  piec wise lut
//Bit 11: 2        reg_md_comb_eotf_59       // unsigned ,    RW, default = 992  piec wise lut
//Bit  1: 0        reserved
#define MD_COMB_EOTF_20                       ISP_MD_REG_START + 4*0x2e  //
//Bit 31:10        reserved
//Bit  9: 0        reg_md_comb_eotf_60       // unsigned ,    RW, default = 1023  piec wise lut
#define MD_EOTF                               ISP_MD_REG_START + 4*0x2f  //
//Bit 31: 2        reserved
//Bit  1           reg_md_bld_use_max        // unsigned ,    RW, default = 0  use max of the components for the blender, 0: no max, 1:max (more sensitive)
//Bit  0           reg_md_eotf_en            // unsigned ,    RW, default = 1  piece wise lut enable
#define MD_DS_STEP                            ISP_MD_REG_START + 4*0x30  //
//Bit 31:20        reserved
//Bit 19:16        reg_ds_hstep              // unsigned ,    RW, default = 3  integer pixel ratio for horizontal scalar, o2i_ratio= 1/step, 1~15
//Bit 15: 4        reserved
//Bit  3: 0        reg_ds_vstep              // unsigned ,    RW, default = 3  integer pixel ratio for vertical scalar,   o2i_ratio= 1/step£¬1~15
#define MD_DS_OSIZE                           ISP_MD_REG_START + 4*0x31  //
//Bit 31:28        reserved
//Bit 27:16        reg_ds_ocol               // unsigned ,    RW, default = 320  actual downscaled field buffer col number.
//Bit 15:12        reserved
//Bit 11: 0        reg_ds_orow               // unsigned ,    RW, default = 180  actual downscaled field buffer row number.
#define MD_DS_CTRL                            ISP_MD_REG_START + 4*0x32  //
//Bit 31:11        reserved
//Bit 10           reg_ds_inp8b              // unsigned ,    RW, default = 0  input to ds accum cell use 8bits instead of 10bits,only set to 1 when (v_step*h_step>64) 0: input 10bits, 1: input 8bits
//Bit  9: 0        reg_ds_norm               // unsigned ,    RW, default = 256  normalization gain to the accum to get the 8bits data for ddr, norm = (inp8b? 4096:1024)/pixelnum(v_step*h_step)
#define MD_CORE_CTRL0                         ISP_MD_REG_START + 4*0x33  //
//Bit 31:28        reserved
//Bit 27:25        reg_md_sad_mode           // unsigned ,    RW, default = 2  window size for sad, 0: no sad, 1: 1x7,  2:3x3; 3:3x5, else 3x7
//Bit 24           reg_md_edge_mode          // unsigned ,    RW, default = 0  mode for the pre and cur edge calculation, 0: min(edge_pre, edge_cur);  1: avg(edge_pre, edge_cur)
//Bit 23:16        reg_md_coring             // unsigned ,    RW, default = 6  coring for sad for noise robustness. sad_core = max(sad-coring,0);
//Bit 15:14        reserved
//Bit 13: 8        reg_md_edge_ratio         // unsigned ,    RW, default = 4  ratio to edge as dynamic coring part, final_coring or 1b_thrd = (edge*ratio + coring); norm to 16 as '1'
//Bit  7: 0        reserved
#define MD_WIN_XXYY0                          ISP_MD_REG_START + 4*0x34  //
//Bit 31:26        reserved
//Bit 25:16        reg_md_win_xxyy_0         // unsigned ,    RW, default = 0  for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]*4
//Bit 15:10        reserved
//Bit  9: 0        reg_md_win_xxyy_1         // unsigned ,    RW, default = 1023  for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]*4
#define MD_WIN_XXYY1                          ISP_MD_REG_START + 4*0x35  //
//Bit 31:26        reserved
//Bit 25:16        reg_md_win_xxyy_2         // unsigned ,    RW, default = 0  for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]*4
//Bit 15:10        reserved
//Bit  9: 0        reg_md_win_xxyy_3         // unsigned ,    RW, default = 1023  for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]*4
#define RO_MD_SAD_SUM                         ISP_MD_REG_START + 4*0x36  //
//Bit 31:28        reserved
//Bit 27: 0        ro_md_sad_sum             // unsigned ,    RW, default = 0  sum of the motion sad for all the pixels within the window;
#define RO_MD_1BM_SUM                         ISP_MD_REG_START + 4*0x37  //
//Bit 31:20        reserved
//Bit 19: 0        ro_md_1bm_sum             // unsigned ,    RW, default = 0  sum of the 1bit motion for all the pixels within the window;
#define RO_MD_EDG_SUM                         ISP_MD_REG_START + 4*0x38  //
//Bit 31:28        reserved
//Bit 27: 0        ro_md_edg_sum             // unsigned ,    RW, default = 0  sum of the edge info for all the pixels within the window;
#define MD_ISMOT_SAD_THRD                     ISP_MD_REG_START + 4*0x39  //
//Bit 31:24        reserved
//Bit 23: 0        reg_ismot_sad_thrd        // unsigned ,    RW, default = 65536  threshold to sum_sad for current frame is motion
#define MD_ISMOT_1BM_THRD                     ISP_MD_REG_START + 4*0x3a  //
//Bit 31:16        reserved
//Bit 15: 0        reg_ismot_1bm_thrd        // unsigned ,    RW, default = 4096  threshold to sum_1bm for current frame consider as motion
#define MD_WR_P_BADDR                        ISP_MD_REG_START + 4*0x3b  //
//Bit 31: 0        reg_rd_base_addr_p          // unsigned ,    RW, default = 32'h80000000
#define MD_RD_P_BADDR                        ISP_MD_REG_START + 4*0x3c  //
//Bit 31: 0        reg_wr_base_addr_p          // unsigned ,    RW, default = 32'h80000000  wr axi base address  

// Reading file:  isp_md2_sta_regs.h
//
// synopsys translate_off
// synopsys translate_on
#define ISP_MD2_BASE                               0xfe3b2800
#define ISP_MD2_COMMON0                            ((0x0000  << 2) + 0xfe3b2800)
//Bit 31: 0        reg_ctrl0                 // unsigned ,    RW, default = 4  latch control
#define ISP_MD2_COMMON1                            ((0x0001  << 2) + 0xfe3b2800)
//Bit 31: 0        reg_ctrl1                 // unsigned ,    RW, default = 32'hffffffff  latch control
#define ISP_MD2_COMMON2                            ((0x0002  << 2) + 0xfe3b2800)
//Bit 31: 0        reg_ctrl2                 // unsigned ,    RW, default = 32'hc000010  latch control
#define ISP_MD2_COMMON3                            ((0x0003  << 2) + 0xfe3b2800)
//Bit 31:28        reg_num_cmp               // unsigned ,    RW, default = 4  number cmp
//Bit 27:20        reg_bit_depth             // unsigned ,    RW, default = 20  bit_depth
//Bit 19:12        reg_gclk                  // unsigned ,    RW, default = 0  clk gate
//Bit 11: 8        reg_insel                 // unsigned ,    RW, default = 0  input data channel select
//Bit  7           reg_bypass                // unsigned ,    RW, default = 0  axi wr bypass
//Bit  6: 2        reserved
//Bit  1           reg_soft_rst              // unsigned ,    RW, default = 0  soft_rst
//Bit  0            reserved
#define ISP_MD2_COMMON_SIZE                        ((0x0004  << 2) + 0xfe3b2800)
//Bit 31:30        reserved
//Bit 29:16        reg_lXSizeIn              // unsigned ,    RW, default = 4096  horizontal size
//Bit 15:14        reserved
//Bit 13: 0        reg_lYSizeIn              // unsigned ,    RW, default = 2160  vertical size
#define ISP_MD2AXI_CTRL                            ((0x0005  << 2) + 0xfe3b2800)
//Bit 31:24        reg_axi_bsize             // unsigned ,    RW, default = 4  each framesize buffer_size
//Bit 23:22        reg_wr_burst_lens         // unsigned ,    RW, default = 2  busrt lens reg_wr_burst_lens   0: 1x128, 1: 2x128, 2â€?: 4x128
//Bit 21:18        reg_wr_req_th             // unsigned ,    RW, default = 4  fifo depth req_th * 8 *128 bits in fifo.
//Bit 17:10        reg_wr_awid               // unsigned ,    RW, default = 0  wid
//Bit  9: 7        reg_wr_awport             // unsigned ,    RW, default = 0  urgent control
//Bit  6: 0        reg_axi_reserved          // unsigned ,    RW, default = 0  reserved
#define ISP_MD2AXI_CTRL0                           ((0x0006  << 2) + 0xfe3b2800)
//Bit 31: 0        reg_wr_urgent_ctrl        // unsigned ,    RW, default = 0  urgent control
#define ISP_MD2AXI_BUFFSIZE                        ((0x0007  << 2) + 0xfe3b2800)
//Bit 31:30        reg_axi_mode              // unsigned ,    RW, default = 0  0:always axi_addr, 1: axi_addr-axi_addrp pingpong, 2~3:buffer_mode, start with axi_addr
//Bit 29           reg_axi_buff_rst          // unsigned ,    RW, default = 4  frame buffer reset
//Bit 28: 0        reg_axi_fbuf              // unsigned ,    RW, default = 4096  axi_buffer size
#define ISP_MD2AXI_FRAMESIZE                       ((0x0008  << 2) + 0xfe3b2800)
//Bit 31:29        reserved
//Bit 28: 0        reg_axi_fsize             // unsigned ,    RW, default = 4096  axi_fsize
#define ISP_MD2AXI_ADDR                            ((0x0009  << 2) + 0xfe3b2800)
//Bit 31: 0        reg_axi_addr              // unsigned ,    RW, default = 268435456  axi_addr
#define ISP_MD2AXI_ADDRP                           ((0x000a  << 2) + 0xfe3b2800)
//Bit 31: 0        reg_axi_addrp             // unsigned ,    RW, default = 268435456  axi_addrp
#define MD2_CTRL                                   ((0x000b  << 2) + 0xfe3b2800)
//Bit 31:14        reserved
//Bit 13: 8        reg_md2_sta_in_rs         // signed ,    RW, default = (BIT_DEPTH-10)  input data righ shift before pre-shrink, x= BL-10,  range -2~6
//Bit  7            reserved
//Bit  6           reg_md2_sta_enable        // unsigned ,    RW, default = 0  enable of the md2 statistics. 0: disable 1: enable
//Bit  5           reg_md2_sta_yuvin         // unsigned ,    RW, default = 0  yuv or rgb input selection, 0: rgb; 1:yuv, default = 0
//Bit  4           reg_md2_sta_y_by_ir       // unsigned ,    RW, default = 0  enable of replace y by ir components. 0: disable 1: enable
//Bit  3: 0        reserved
#define MD2_INPUT_SIZE                             ((0x000c  << 2) + 0xfe3b2800)
//Bit 31:16        reg_md2_input_xsize       // unsigned ,    RW, default = 1920  xsize of the input in pixels, set to image xsize
//Bit 15: 0        reg_md2_input_ysize       // unsigned ,    RW, default = 1080  ysize of the input in pixels, set to image ysize
#define MD2_WIND_XSTED                             ((0x000d  << 2) + 0xfe3b2800)
//Bit 31:30        reserved
//Bit 29:16        reg_md2_win_xxyy_0        // unsigned ,    RW, default = 0  window for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]
//Bit 15:14        reserved
//Bit 13: 0        reg_md2_win_xxyy_1        // unsigned ,    RW, default = 1920
#define MD2_WIND_YSTED                             ((0x000e  << 2) + 0xfe3b2800)
//Bit 31:30        reserved
//Bit 29:16        reg_md2_win_xxyy_2        // unsigned ,    RW, default = 0
//Bit 15:14        reserved
//Bit 13: 0        reg_md2_win_xxyy_3        // unsigned ,    RW, default = 1080
#define MD2_PRE_STEP_NORM                          ((0x000f  << 2) + 0xfe3b2800)
//Bit 31:25        reserved
//Bit 24:20        reg_md2_preds_hstp        // unsigned ,    RW, default = 3  integer pixel ratio for pre horizontal shrink, o2i_ratio= 1/step, 1~31
//Bit 19:16        reg_md2_preds_vstp        // unsigned ,    RW, default = 2  integer pixel ratio for pre vertical shrink,   o2i_ratio= 1/stepÂ£Â¬1~15
//Bit 15:12        reserved
//Bit 11: 0        reg_md2_preds_norm        // unsigned ,    RW, default = 682  normalization gain to the accum to get average of the block, norm = 4096/pixelnum(v_step*h_step)
#define MD2_PRE_OUT_SIZE                           ((0x0010  << 2) + 0xfe3b2800)
//Bit 31:26        reserved
//Bit 25:16        reg_md2_preds_ocol        // unsigned ,    RW, default = 512  pre-shrink output image horizontal size, maximum 512, floor((win_xxyy[1]-win_xxyy[0])/hstp).
//Bit 15:10        reserved
//Bit  9: 0        reg_md2_preds_orow        // unsigned ,    RW, default = 512  pre-shrink output image vertical size, maximum 512
#define MD2_OFFSET_INP                             ((0x0011  << 2) + 0xfe3b2800)
//Bit 31:27        reserved
//Bit 26:16        reg_md2_offset_inp_1      // signed ,    RW, default = 0  pre-offset before the 3x3 matrix.
//Bit 15:11        reserved
//Bit 10: 0        reg_md2_offset_inp_0      // signed ,    RW, default = 0  pre-offset before the 3x3 matrix.
#define MD2_MATRIX_0                               ((0x0012  << 2) + 0xfe3b2800)
//Bit 31:28        reserved
//Bit 27:16        reg_md2_3x3matrix_1       // signed ,    RW, default = 150  3x3 matrix coefs.
//Bit 15:12        reserved
//Bit 11: 0        reg_md2_3x3matrix_0       // signed ,    RW, default = 77  3x3 matrix coefs.
#define MD2_MATRIX_1                               ((0x0013  << 2) + 0xfe3b2800)
//Bit 31:28        reserved
//Bit 27:16        reg_md2_3x3matrix_3       // signed ,    RW, default = -43  3x3 matrix coefs.
//Bit 15:12        reserved
//Bit 11: 0        reg_md2_3x3matrix_2       // signed ,    RW, default = 29  3x3 matrix coefs.
#define MD2_MATRIX_2                               ((0x0014  << 2) + 0xfe3b2800)
//Bit 31:28        reserved
//Bit 27:16        reg_md2_3x3matrix_5       // signed ,    RW, default = 128  3x3 matrix coefs.
//Bit 15:12        reserved
//Bit 11: 0        reg_md2_3x3matrix_4       // signed ,    RW, default = -85  3x3 matrix coefs.
#define MD2_MATRIX_3                               ((0x0015  << 2) + 0xfe3b2800)
//Bit 31:28        reserved
//Bit 27:16        reg_md2_3x3matrix_7       // signed ,    RW, default = -107  3x3 matrix coefs.
//Bit 15:12        reserved
//Bit 11: 0        reg_md2_3x3matrix_6       // signed ,    RW, default = 128  3x3 matrix coefs.
#define MD2_MATRIX_4                               ((0x0016  << 2) + 0xfe3b2800)
//Bit 31:18        reserved
//Bit 17:16        reg_md2_3x3mtrx_rs        // unsigned ,    RW, default = 0  coefs precision mode. 0 s3.8; 1: s2.9; 2: s1.10 3:s0.11
//Bit 15:12        reserved
//Bit 11: 0        reg_md2_3x3matrix_8       // signed ,    RW, default = -21  3x3 matrix coefs.
#define MD2_OFFSET                                 ((0x0017  << 2) + 0xfe3b2800)
//Bit 31:27        reserved
//Bit 26:16        reg_md2_offset_inp_2      // signed ,    RW, default = 0  pre-offset before the 3x3 matrix.
//Bit 15:11        reserved
//Bit 10: 0        reg_md2_offset_oup_2      // signed ,    RW, default = 0  post-offset after the 3x3 matrix.
#define MD2_OFFSET_OUP                             ((0x0018  << 2) + 0xfe3b2800)
//Bit 31:27        reserved
//Bit 26:16        reg_md2_offset_oup_1      // signed ,    RW, default = 0  post-offset after the 3x3 matrix.
//Bit 15:11        reserved
//Bit 10: 0        reg_md2_offset_oup_0      // signed ,    RW, default = 0  post-offset after the 3x3 matrix.
#define MD2_POST_STEP_NORM                         ((0x0019  << 2) + 0xfe3b2800)
//Bit 31:24        reserved
//Bit 23:20        reg_md2_pstds_hstp        // unsigned ,    RW, default = 4  integer pixel ratio for post horizontal shrink, o2i_ratio= 1/step, 1~15
//Bit 19:16        reg_md2_pstds_vstp        // unsigned ,    RW, default = 3  integer pixel ratio for post vertical shrink,   o2i_ratio= 1/stepÂ£Â¬1~15
//Bit 15:12        reserved
//Bit 11: 0        reg_md2_pstds_norm        // unsigned ,    RW, default = 512  normalization gain to the accum to get average in block, norm = 4096/pixelnum(v_step*h_step)
#define MD2_POST_OUT_SIZE                          ((0x001a  << 2) + 0xfe3b2800)
//Bit 31:25        reserved
//Bit 24:16        reg_md2_pstds_ocol        // unsigned ,    RW, default = 256  post-shrink output image horizontal size, maximum 256
//Bit 15: 9        reserved
//Bit  8: 0        reg_md2_pstds_orow        // unsigned ,    RW, default = 128  post-shrink output image vertical size, maximum 256
#define MD2_TH                                     ((0x001b  << 2) + 0xfe3b2800)
//Bit 31:20        reserved
//Bit 19:18        reg_md2_ratio_mod         // unsigned ,    RW, default = 0  ration mode selection. 0: R/(R+G+B+1), 1: R/G;  2ï¼?(R+G+B)/R; 3: G/R
//Bit 17:16        reg_md2_ratio_nrm         // unsigned ,    RW, default = 2  normalizatin of ratio for: mode=0, range=(1, 0]/(1<<x), dft=2;  mode>0, precision = u(2+x).(6-x), dft=1
//Bit 15: 8        reg_md2_satur_thr         // unsigned ,    RW, default = 240  saturation threshold of u8
//Bit  7: 0        reg_md2_under_thr         // unsigned ,    RW, default = 10  under threshold of u8
#define ISP_MD2_STA_INFO0                          ((0x001c  << 2) + 0xfe3b2800)
//Bit 31: 0        ro_md2_ro0                // unsigned ,    RO, default = 0  debug info0
#define ISP_MD2_STA_INFO1                          ((0x001d  << 2) + 0xfe3b2800)
//Bit 31: 0        ro_md2_ro1                // unsigned ,    RO, default = 0  debug info1
#define ISP_MD2_STA_INFO2                          ((0x001e  << 2) + 0xfe3b2800)
//Bit 31: 0        ro_md2_ro2                // unsigned ,    RO, default = 0  debug info2
#define ISP_MD2_STA_INFO3                          ((0x001f  << 2) + 0xfe3b2800)
//Bit 31: 0        ro_md2_ro3                // unsigned ,    RO, default = 0  debug info3
#define ISP_MD2_STA_INFO4                          ((0x0020  << 2) + 0xfe3b2800)
//Bit 31: 0        ro_md2_ro4                // unsigned ,    RO, default = 0  debug info4
#define ISP_MD2_STA_INFO5                          ((0x0021  << 2) + 0xfe3b2800)
//Bit 31: 0        ro_md2_ro5                // unsigned ,    RO, default = 0  debug info5
#define ISP_MD2_STA_INFO6                          ((0x0022  << 2) + 0xfe3b2800)
//Bit 31: 0        ro_md2_ro6                // unsigned ,    RO, default = 0  debug info6
#define ISP_MD2_STA_INFO7                          ((0x0023  << 2) + 0xfe3b2800)
//Bit 31: 0        ro_md2_ro7                // unsigned ,    RO, default = 0  debug info7
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  isp_md2_sta_regs.h


typedef struct
{
    int reg_ctrl0 ;
    int reg_ctrl1 ;
    int reg_ctrl2 ;
    int reg_num_cmp ;
    int reg_bit_depth ;
    int reg_gclk ;
    int reg_insel ;
    int reg_bypass ;
    int reg_soft_rst ;
    int reg_lXSizeIn ;
    int reg_lYSizeIn ;
    int reg_axi_bsize ;
    int reg_wr_burst_lens ;
    int reg_wr_req_th ;
    int reg_wr_awid ;
    int reg_wr_awport ;
    int reg_axi_reserved ;
    int reg_wr_urgent_ctrl ;
    int reg_axi_mode ;
    int reg_axi_buff_rst ;
    int reg_axi_fbuf ;
    int reg_axi_fsize ;
    int reg_axi_addr ;
    int reg_axi_addrp ;
    int reg_md2_sta_enable ;
    int reg_md2_sta_yuvin ;
    int reg_md2_sta_y_by_ir ;
    int reg_md2_sta_in_rs ;
    int reg_md2_input_xsize ;
    int reg_md2_input_ysize ;
    int reg_md2_win_xxyy_0 ;
    int reg_md2_win_xxyy_1 ;
    int reg_md2_win_xxyy_2 ;
    int reg_md2_win_xxyy_3 ;
    int reg_md2_preds_hstp ;
    int reg_md2_preds_vstp ;
    int reg_md2_preds_norm ;
    int reg_md2_preds_ocol ;
    int reg_md2_preds_orow ;
    int reg_md2_offset_inp_1 ;
    int reg_md2_offset_inp_0 ;
    int reg_md2_3x3matrix_1 ;
    int reg_md2_3x3matrix_0 ;
    int reg_md2_3x3matrix_3 ;
    int reg_md2_3x3matrix_2 ;
    int reg_md2_3x3matrix_5 ;
    int reg_md2_3x3matrix_4 ;
    int reg_md2_3x3matrix_7 ;
    int reg_md2_3x3matrix_6 ;
    int reg_md2_3x3mtrx_rs ;
    int reg_md2_3x3matrix_8 ;
    int reg_md2_offset_inp_2 ;
    int reg_md2_offset_oup_2 ;
    int reg_md2_offset_oup_1 ;
    int reg_md2_offset_oup_0 ;
    int reg_md2_pstds_hstp ;
    int reg_md2_pstds_vstp ;
    int reg_md2_pstds_norm ;
    int reg_md2_pstds_ocol ;
    int reg_md2_pstds_orow ;
    int reg_md2_ratio_mod ;
    int reg_md2_ratio_nrm ;
    int reg_md2_satur_thr ;
    int reg_md2_under_thr ;
    int ro_md2_ro0 ;
    int ro_md2_ro1 ;
    int ro_md2_ro2 ;
    int ro_md2_ro3 ;
    int ro_md2_ro4 ;
    int ro_md2_ro5 ;
    int ro_md2_ro6 ;
    int ro_md2_ro7 ;
} T_MD2_PRM;

typedef struct {
    //RTL Control
    int reg_md_gclk              ;               // u32: clock gate control
    int reg_md_top_ctrl          ;           // u32: md top ntrol
    int reg_wr_axi_wr_en         ;            //u1:   axi wr enable
    int reg_wr_axi_req_en        ;            //u1:   axi request enable
    int reg_wr_axi_bypass        ;            //u1:   bypass axi wr
    int reg_wr_total_size        ;    //u24:  pixels hsize * vsize
    int reg_wr_base_addr ;                       //u32:  wr axi base address
    int reg_wr_burst_lens;                      //u3: burst_lens limiation, burst_size: 0: 1x128, 1: 2x128, 2£þ3: 4x128
    int reg_wr_req_th ;                          //u4: fifo depth req_th * 8 *128 bits in fifo.
    int reg_wr_urgent_ctrl ;                     //u16: urgent control
    int reg_wr_awid ;                            //u8: wr id
    int reg_wr_awprot ;                          //u3: awprot for security control
    int RO_wr_st0 ;	                           //u32
    int RO_wr_st1 ;	                           //u32
    int RO_wr_st2 ;                            //u32

    int reg_rd_axi_rd_en   ;                //u1: axi rd enable
    int reg_rd_axi_req_en  ;                //u1: axi request enable
    int reg_rd_total_size  ;                //u24: total size hsize*vsize, <= 1024*1024
    int reg_rd_base_addr   ;                //u32
    int reg_rd_burst_lens  ;                //u3: burst_lens limiation, burst_size: 0: 1x128, 1: 2x128, 2£þ3: 4x128
    int reg_rd_req_th      ;                //u4: fifo depth req_th * 8 *128 bits in fifo.
    int reg_rd_urgent_ctrl ;                //u16: urgent control
    int reg_rd_arid        ;                //u8: wr id
    int reg_rd_arprot      ;                //u3: awprot for security control
    int RO_rd_st0          ;                //u32
    int RO_rd_st1          ;                //u32

	// the motion detection part
	int reg_md_enable;                  //u1: lp motion detection enable, 0: disable;  1: enable
	int reg_md_raw_xphase_ofst;         //u1: horizontal phase of the raw data, 0: start wz R-col (RGGB or GBRG); 1: start wz B-col (GRBG or BGGR)
	int reg_md_raw_yphase_ofst;         //u1: vertical phase of the raw data,   0: start wz R-row (RGGB or GRBG); 1: start wz B-row (GRBG or BGGR)
	int reg_md_input_sel;               //u4: data selection for the lp motion detection. 0: raw sensor input; 1: raw WDR stitch; 2:raw fed_out(nr_in); 3:raw mirror_in; 4?? RGB after dms; 5: IRout; 6:RGB/YUV after gamma; 7: fe_o yuv bypass , 8: output port, default=1
	int reg_md_is_on_raw;               //u1: the md_sel is on raw data, set together with md_sel.    0: YUV/RGB;  1:RAW
	int reg_md_input_ls;                //u4, E domain data left shift bit num to align with u20 Odomain data and will clip to u10 by droping 10lsb, e.g. yuv is u12, then ls = 8;
	int reg_md_input_xsize;             //u12, xsize of the input in pixels, set to image xsize
	int reg_md_input_ysize;             //u12, ysize of the input in pixels, set to image ysize

	//step-I O domain reg_RGGB data do the black-level sub, gain, OETF
	int reg_md_inpc_winxxyy[4];         //u12x4, input combine window for processing.
	int reg_md_rggb_ofset[4];           //S20x4, ofset to the components, same as the ISP mission mode ofset
	int reg_md_rggb_gain[4];            //u12x4, gain to different channel, normalize to 1024 as "1.0"
	int reg_md_bld_coefs[4];            //u8x4,  blender coef of the components to Y, normalize to 128 as "1"
	int reg_md_comb_eotf[61];	        //u10x61,OETF, piece wise lut
	
	int reg_md_eotf_en;      	        //u1,OETF, piece wise lut enable
	int reg_md_bld_use_max;             //u1, use max of the components for the blender, 0: no max, 1:maxa

	//step-II the downscaled frame buffer size
	int reg_ds_ocol;                    //u12: actual downscaled field buffer col number.(<=1024)
	int reg_ds_orow;                    //u12: actual downscaled field buffer row number.
	int reg_ds_hstep;                   //u4: integer pixel ratio for horizontal scalar, o2i_ratio= 1/step
    int reg_ds_vstep;                   //u4: integer pixel ratio for vertical scalar,   o2i_ratio= 1/step
	int reg_ds_inp8b;                   //u1, input to ds accum cell use 8bits instead of 10bits,only set to 1 when (v_step*h_step>64) 0: input 10bits, 1: input 8bits
	int reg_ds_norm ;                   //u10; normalization gain to the accum to get the 8bits data for ddr, norm = (inp8b)?4096:1024/pixelnum(v_step*h_step)

    //step-III do the sad calculation and sum-up for RO
	int reg_md_sad_mode;                //u3,  window size for sad, 0: no sad, 1: 1x7,  2:3x3; 3:3x5, else 3x7
	int reg_md_edge_mode;               //u1: mode for the pre and cur edge calculation, 0: min(edge_pre, edge_cur);  1: avg(edge_pre, edge_cur)
	int reg_md_coring;                  //u8; coring for sad for noise robustness. sad_core = max(sad-coring,0);
	int reg_md_edge_ratio;              //u6: ratio to edge as dynamic coring part, final_coring or 1b_thrd = (edge*ratio + coring); norm to 16 as '1'
	int reg_md_win_xxyy[4];             //u8x4: window for motion sum statistics on down-scaled image. [x_st x_ed; y_st y_ed]*4

	// the output sum of motion
	int RO_md_sad_sum;                  //u28, sum of the motion sad for all the pixels within the window;
	int RO_md_1bm_sum;                  //u20, sum of the 1bit motion for all the pixels within the window;	
	int RO_md_edg_sum;                  //u28, sum of the edge info for all the pixels within the window;

	// to HW decision on is_motion and send out interupt to CPU
	int reg_ismot_sad_thrd;             //u24, threshold to sum_sad for current frame is motion
	int reg_ismot_1bm_thrd;             //u16, threshold to sum_1bm for current frame consider as motion

    int reg_wr_base_addr_p;
    int reg_rd_base_addr_p;

}T_MD_PRM;

struct am_md {
	struct device_node *of_node;
	struct platform_device *p_dev;
	struct resource reg;
	void __iomem *base_addr;
	int irq;
	struct task_struct *kmd_thread;
	wait_queue_head_t tnr_wq;
	int f_motion;
	int ctx_id;
};

int am_md_parse_dt(struct device_node *node);
int param_md_init(T_MD_PRM *reg);
int param_md_input(T_MD_PRM *reg, int hsize_i, int vsize_i, int in_sel);
int param_md_scale(T_MD_PRM *reg, int hsize_i, int vsize_i, int hsize_s, int vsize_o, int32_t base_axi_addr);
int param_md_detect(int md_sad_thrd, int md_1bm_thrd);
int param_md_set(T_MD_PRM *reg);
void am_md_init(int ctx_id, int width, int height);
void am_md_deinit(void);
void am_md_destroy(void);
void aml_md_get_data(uint32_t* dst);


#endif // __SYSTEM_UART_H__
