/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_reg_v3.h
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

#ifndef __DI_REG_V3_H__
#define __DI_REG_V3_H__

/**********************************************************
 * register for sc2
 **********************************************************/
#define DI_TOP_PRE_CTRL                            0x17c5
#define DI_TOP_POST_CTRL                           0x17c6

#define DI_SC2_PRE_GL_CTRL                         0x17c7 /* sc2 change add */
#define DI_SC2_PRE_GL_THD                          0x17c8 /* sc2 change add */
#define DI_SC2_POST_GL_CTRL                        0x17c9 /* sc2 change add */
#define DI_SC2_POST_GL_THD                         0x17ca /* sc2 change add */
#define DI_RO_PRE_DBG                              0x17cb
#define DI_RO_POST_DBG                             0x17cc
#define DI_TOP_CTRL                                0x17cd
#define DI_AFBCD_GCLK0                             0x17ce
#define DI_AFBCD_GCLK1                             0x17cf
#define DI_RDMIF_DEPTH0                            0x17d0
#define DI_RDMIF_DEPTH1                            0x17d1
#define DI_RDMIF_DEPTH2                            0x17d2
#define DI_TOP_CTRL1                               0x17d3
#define DI_AFBCE0_HOLD_CTRL                        0x17d4
#define DI_AFBCE1_HOLD_CTRL                        0x17d5

/* pre nr */
#define NRWR_DBG_AXI_CMD_CNT                       0x2090
#define NRWR_DBG_AXI_DAT_CNT                       0x2091
#define DI_NRWR_CANVAS                             0x2092
#define DI_NRWR_URGENT                             0x2093
#define DI_SC2_NRWR_X                                  0x2094
#define DI_SC2_NRWR_Y                                  0x2095
//bit 31:30		  NRWR_words_lim
//bit 29		  NRWR_rev_y
//bit 28:16		  NRWR_start_y
//bit 15		  NRWR_ext_en
//bit 12:0		  NRWR_end_y
#define DI_SC2_NRWR_CTRL                               0x2096
//bit 31		  pending_ddr_wrrsp_NRWR
//bit 30		  NRWR_reg_swap
//bit 29:26		  NRWR_burst_lim
//bit 25		  NRWR_canvas_syncen
//bit 24		  NRWR_no_clk_gate
//bit 23:22		  NRWR_rgb_mode
//0:422 to one canvas;1:4:4:4 to one canvas 2:Y to luma ,
// CBCR to chroma canvas ,for nv12/21; 3 : reserved
//bit 21:20		  NRWR_hconv_mode
//bit 19:18		  NRWR_vconv_mode
//bit 17		  NRWR_swap_cbcr
//bit 16		  NRWR_urgent
//bit 15:8		  NRWR_canvas_index_chroma
//bit 7:0		  NRWR_canvas_index_luma
#define DI_NRWR_SHRK_CTRL                          0x2097
//bit   31:10     reserved
//bit   9:8       reg_vshrk_mode
//unsigned, default = 0, 0:1/2 horizontal
//shrink 1:1/4 horizontal shrink 2:1/8 horizontal shrink
//bit   7:6       reg_hshrk_mode
//unsigned, default = 0, 0:1/2 vertical
//shrink 1:1/4 vertical shrink 2:1/8 vertical shrink
//bit   5:2       reg_gclk_ctrl         //unsigned, default = 0
//bit   1         reg_frm_rst           //unsigned, default = 0
//bit   0         reg_shrk_en           //unsigned, default = 0
#define DI_NRWR_SHRK_SIZE                          0x2098
//bit   31:26     reserved
//bit   25:13     reg_frm_hsize
//unsigned, default = 1920, hsize in
//bit   12:0      reg_frm_vsize
//unsigned, default = 1080, vsize in
#define DI_NRWR_CROP_CTRL                          0x209a
//Bit   31        reg_crop_en
//unsigned  , RW,default = 0,dimm_layer enable signal
//Bit   30:4      reserved
//Bit   3:0       reg_hold_line
//unsigned  , RW,default = 4,dimm_layer data
#define DI_NRWR_CROP_DIMM_CTRL                     0x209b
//Bit   31        reg_dimm_layer_en
//unsigned  , RW,default = 0,dimm_layer enable signal
//Bit   30        reserved
//Bit   29:0      reg_dimm_data
//unsigned  , RW,default = 29'h00080200,dimm_layer data
#define DI_NRWR_CROP_SIZE_IN                       0x209c
//Bit   31:29,    reserved
//Bit   28:16     reg_crop_hsize
//unsigned, default = 1920 , pic horz size in  unit: pixel
//Bit   15:13,    reserved
//Bit   12:0,     reg_crop_vsize
//unsigned, default = 1080 , pic vert size in  unit: pixel

#define DI_NRWR_CROP_HSCOPE                        0x209d
//Bit   31:29,    reserved
//Bit   28:16,    reg_cropwin_end_h      unsigned, default = 1919 ;
//Bit   15:13,    reserved
//Bit   12:0,     reg_cropwin_bgn_h      unsigned, default = 0    ;
#define DI_NRWR_CROP_VSCOPE                        0x209e
//Bit   31:29,    reserved
//Bit   28:16,    reg_cropwin_end_v      unsigned, default = 1079 ;
//Bit   15:13,    reserved
//Bit   12:0,     reg_cropwin_bgn_v      unsigned, default = 0    ;
//t7
#define DI_NRWR_BADDR0                             0x20a0
//Bit   31:0      wmif_baddr_luma        unsigned, default = 0x20000
#define DI_NRWR_STRIDE0                            0x20a1
//Bit   31        canvas_mode_en         unsigned, default = 0    ;
//Bit   30:14     reserved
//Bit   13:0      wmif_stride_luma       unsigned, default = 0x1000;
#define DI_NRWR_BADDR1                             0x20a2
//Bit   31:0      wmif_baddr_chroma      unsigned, default = 0x20000
#define DI_NRWR_STRIDE1                            0x20a3
//Bit   31        canvas_mode_en         unsigned, default = 0    ;
//Bit   30:14     reserved
//Bit   13:0      wmif_stride_chroma     unsigned, default = 0x1000;

/* post wr */
#define DIWR_DBG_AXI_CMD_CNT                       0x20f0
#define DIWR_DBG_AXI_DAT_CNT                       0x20f1
#define DI_DIWR_CANVAS                             0x20f2
#define DI_DIWR_URGENT                             0x20f3
#define DI_SC2_DIWR_X                                  0x20f4
#define DI_SC2_DIWR_Y                                  0x20f5
//bit 31:30		  diwr_words_lim
//bit 29		  diwr_rev_y
//bit 28:16		  diwr_start_y
//bit 15		  diwr_ext_en
//bit 12:0		  diwr_end_y
#define DI_SC2_DIWR_CTRL                               0x20f6
//bit 31		  pending_ddr_wrrsp_diwr
//bit 30		  diwr_reg_swap
//bit 29:26		  diwr_burst_lim
//bit 25		  diwr_canvas_syncen
//bit 24		  diwr_no_clk_gate
//bit 23:22		  diwr_rgb_mode
//0:422 to one canvas;1:4:4:4 to one canvas 2:
//Y to luma , CBCR to chroma canvas ,for nv12/21; 3 : reserved
//bit 21:20		  diwr_hconv_mode
//bit 19:18		  diwr_vconv_mode
//bit 17		  diwr_swap_cbcr
//bit 16		  diwr_urgent
//bit 15:8		  diwr_canvas_index_chroma
//bit 7:0		  diwr_canvas_index_luma
#define DI_DIWR_SHRK_CTRL                          0x20f7
//bit   31:10     reserved
//bit   9:8       reg_vshrk_mode        unsigned, default = 0,
//0:1/2 horizontal shrink 1:1/4 horizontal shrink 2:1/8 horizontal shrink
//bit   7:6       reg_hshrk_mode        unsigned, default = 0,
//0:1/2 vertical shrink 1:1/4 vertical shrink 2:1/8 vertical shrink
//bit   5:2       reg_gclk_ctrl         unsigned, default = 0
//bit   1         reg_frm_rst           unsigned, default = 0
//bit   0         reg_shrk_en           unsigned, default = 0
#define DI_DIWR_SHRK_SIZE                          0x20f8
//bit   31:26     reserved
//bit   25:13     reg_frm_hsize         unsigned, default = 1920, hsize in
//bit   12:0      reg_frm_vsize         unsigned, default = 1080, vsize in
#define DI_DIWR_CROP_CTRL                          0x20fa
//Bit   31        reg_crop_en
//unsigned  , RW,default = 0,dimm_layer enable signal
//Bit   30:4      reserved
//Bit   3:0       reg_hold_line
//unsigned  , RW,default = 4,dimm_layer data
#define DI_DIWR_CROP_DIMM_CTRL                     0x20fb
//Bit   31        reg_dimm_layer_en
//unsigned  , RW,default = 0,dimm_layer enable signal
//Bit   30        reserved
//Bit   29:0      reg_dimm_data
//unsigned  , RW,default = 29'h00080200,dimm_layer data
#define DI_DIWR_CROP_SIZE_IN                       0x20fc
//Bit   31:29,    reserved
//Bit   28:16     reg_crop_hsize
//unsigned, default = 1920 , pic horz size in  unit: pixel
//Bit   15:13,    reserved/
//Bit   12:0,     reg_crop_vsize
//unsigned, default = 1080 , pic vert size in  unit: pixel
#define DI_DIWR_CROP_HSCOPE                        0x20fd
//Bit   31:29,    reserved
//Bit   28:16,    reg_cropwin_end_h      unsigned, default = 1919 ;
//Bit   15:13,    reserved
//Bit   12:0,     reg_cropwin_bgn_h      unsigned, default = 0    ;
#define DI_DIWR_CROP_VSCOPE                        0x20fe
//Bit   31:29,    reserved
//Bit   28:16,    reg_cropwin_end_v      unsigned, default = 1079 ;
//Bit   15:13,    reserved
//Bit   12:0,     reg_cropwin_bgn_v      unsigned, default = 0    ;
//t7
#define DI_DIWR_BADDR0                             0x20a8
//Bit   31:0      wmif_baddr_luma        unsigned, default = 0x20000
#define DI_DIWR_STRIDE0                            0x20a9
//Bit   31        canvas_mode_en         unsigned, default = 0    ;
//Bit   30:14     reserved
//Bit   13:0      wmif_stride_luma       unsigned, default = 0x1000;
#define DI_DIWR_BADDR1                             0x20aa
//Bit   31:0      wmif_baddr_chroma      unsigned, default = 0x20000
#define DI_DIWR_STRIDE1                            0x20ab
//Bit   31        canvas_mode_en         unsigned, default = 0    ;
//Bit   30:14     reserved
//Bit   13:0      wmif_stride_chroma     unsigned, default = 0x1000;

/* mif */
/* need move to here */
#define RDMIFXN_GEN_REG                            0x5400
#define RDMIFXN_CANVAS0                            0x5401
#define RDMIFXN_CANVAS1                            0x5402
#define RDMIFXN_LUMA_X0                            0x5403
#define RDMIFXN_LUMA_Y0                            0x5404
#define RDMIFXN_CHROMA_X0                          0x5405
#define RDMIFXN_CHROMA_Y0                          0x5406
#define RDMIFXN_LUMA_X1                            0x5407
#define RDMIFXN_LUMA_Y1                            0x5408
#define RDMIFXN_CHROMA_X1                          0x5409
#define RDMIFXN_CHROMA_Y1                          0x540a
#define RDMIFXN_RPT_LOOP                           0x540b
#define RDMIFXN_LUMA0_RPT_PAT                      0x540c
#define RDMIFXN_CHROMA0_RPT_PAT                    0x540d
#define RDMIFXN_LUMA1_RPT_PAT                      0x540e
#define RDMIFXN_CHROMA1_RPT_PAT                    0x540f
#define RDMIFXN_LUMA_PSEL                          0x5410
#define RDMIFXN_CHROMA_PSEL                        0x5411
#define RDMIFXN_DUMMY_PIXEL                        0x5412
#define RDMIFXN_LUMA_FIFO_SIZE                     0x5413
#define RDMIFXN_AXI_CMD_CNT                        0x5414
#define RDMIFXN_AXI_RDAT_CNT                       0x5415
#define RDMIFXN_RANGE_MAP_Y                        0x5416
#define RDMIFXN_RANGE_MAP_CB                       0x5417
#define RDMIFXN_RANGE_MAP_CR                       0x5418
#define RDMIFXN_GEN_REG2                           0x5419
#define RDMIFXN_PROT                               0x541a
#define RDMIFXN_URGENT_CTRL                        0x541b
#define RDMIFXN_GEN_REG3                           0x541c
#define RDMIFXN_CFMT_CTRL                          0x541d
#define RDMIFXN_CFMT_W                             0x541e
//t7
#define RDMIFXN_BADDR_Y                            0x5420
//Bit 31:0      cntl_f0_baddr_y                        //unsigned, RW, default = 0
#define RDMIFXN_BADDR_CB                           0x5421
//Bit 31:0      cntl_f0_baddr_cb                       //unsigned, RW, default = 0
#define RDMIFXN_BADDR_CR                           0x5422
//Bit 31:0      cntl_f0_baddr_cr                       //unsigned, RW, default = 0
#define RDMIFXN_STRIDE_0                           0x5423
//Bit 31:29     reserved
//Bit 28:16     cntl_f0_stride_cb                      //unsigned, RW, default = 256
//Bit 15:13     reserved
//Bit 12:0      cntl_f0_stride_y                       //unsigned, RW, default = 256
#define RDMIFXN_STRIDE_1                           0x5424
//Bit 31:17     reserved
//Bit 16        cntl_f0_acc_mode                       //unsigned, RW, default = 0
//Bit 15:13     reserved
//Bit 12:0      cntl_f0_stride_cr                      //unsigned, RW, default = 256
#define DI_SC2_INP_GEN_REG                            0x5400
#define DI_SC2_INP_CANVAS0                            0x5401
#define DI_SC2_INP_CANVAS1                            0x5402
#define DI_SC2_INP_LUMA_X0                            0x5403
#define DI_SC2_INP_LUMA_Y0                            0x5404
#define DI_SC2_INP_CHROMA_X0                          0x5405
#define DI_SC2_INP_CHROMA_Y0                          0x5406
#define DI_SC2_INP_LUMA_X1                            0x5407
#define DI_SC2_INP_LUMA_Y1                            0x5408
#define DI_SC2_INP_CHROMA_X1                          0x5409
#define DI_SC2_INP_CHROMA_Y1                          0x540a
#define DI_SC2_INP_RPT_LOOP                           0x540b
#define DI_SC2_INP_LUMA0_RPT_PAT                      0x540c
#define DI_SC2_INP_CHROMA0_RPT_PAT                    0x540d
#define DI_SC2_INP_LUMA1_RPT_PAT                      0x540e
#define DI_SC2_INP_CHROMA1_RPT_PAT                    0x540f
#define DI_SC2_INP_LUMA_PSEL                          0x5410
#define DI_SC2_INP_CHROMA_PSEL                        0x5411
#define DI_SC2_INP_DUMMY_PIXEL                        0x5412
#define DI_SC2_INP_LUMA_FIFO_SIZE                     0x5413
#define DI_SC2_INP_AXI_CMD_CNT                        0x5414
#define DI_SC2_INP_AXI_RDAT_CNT                       0x5415
#define DI_SC2_INP_RANGE_MAP_Y                        0x5416
#define DI_SC2_INP_RANGE_MAP_CB                       0x5417
#define DI_SC2_INP_RANGE_MAP_CR                       0x5418
#define DI_SC2_INP_GEN_REG2                           0x5419
#define DI_SC2_INP_PROT                               0x541a
#define DI_SC2_INP_URGENT_CTRL                        0x541b
#define DI_SC2_INP_GEN_REG3                           0x541c
#define DI_SC2_INP_CFMT_CTRL                          0x541d
#define DI_SC2_INP_CFMT_W                             0x541e
//t7
#define DI_T7_INP_BADDR_Y                            0x5420
#define DI_T7_INP_BADDR_CB                           0x5421
#define DI_T7_INP_BADDR_CR                           0x5422
#define DI_T7_INP_STRIDE_0                           0x5423
#define DI_T7_INP_STRIDE_1                           0x5424

#define DI_SC2_CHAN2_GEN_REG                            0x5480	//off = 0x80
#define DI_SC2_CHAN2_CANVAS0                            0x5481
#define DI_SC2_CHAN2_CANVAS1                            0x5482
#define DI_SC2_CHAN2_LUMA_X0                            0x5483
#define DI_SC2_CHAN2_LUMA_Y0                            0x5484
#define DI_SC2_CHAN2_CHROMA_X0                          0x5485
#define DI_SC2_CHAN2_CHROMA_Y0                          0x5486
#define DI_SC2_CHAN2_LUMA_X1                            0x5487
#define DI_SC2_CHAN2_LUMA_Y1                            0x5488
#define DI_SC2_CHAN2_CHROMA_X1                          0x5489
#define DI_SC2_CHAN2_CHROMA_Y1                          0x548a
#define DI_SC2_CHAN2_RPT_LOOP                           0x548b
#define DI_SC2_CHAN2_LUMA0_RPT_PAT                      0x548c
#define DI_SC2_CHAN2_CHROMA0_RPT_PAT                    0x548d
#define DI_SC2_CHAN2_LUMA1_RPT_PAT                      0x548e
#define DI_SC2_CHAN2_CHROMA1_RPT_PAT                    0x548f
#define DI_SC2_CHAN2_LUMA_PSEL                          0x5490
#define DI_SC2_CHAN2_CHROMA_PSEL                        0x5491
#define DI_SC2_CHAN2_DUMMY_PIXEL                        0x5492
#define DI_SC2_CHAN2_LUMA_FIFO_SIZE                     0x5493
#define DI_SC2_CHAN2_AXI_CMD_CNT                        0x5494
#define DI_SC2_CHAN2_AXI_RDAT_CNT                       0x5495
#define DI_SC2_CHAN2_RANGE_MAP_Y                        0x5496
#define DI_SC2_CHAN2_RANGE_MAP_CB                       0x5497
#define DI_SC2_CHAN2_RANGE_MAP_CR                       0x5498
#define DI_SC2_CHAN2_GEN_REG2                           0x5499
#define DI_SC2_CHAN2_PROT                               0x549a
#define DI_SC2_CHAN2_URGENT_CTRL                        0x549b
#define DI_SC2_CHAN2_GEN_REG3                           0x549c
#define DI_SC2_CHAN2_CFMT_CTRL                          0x549d
#define DI_SC2_CHAN2_CFMT_W                             0x549e
//t7:
#define DI_T7_CHAN2_BADDR_Y                            0x54a0
#define DI_T7_CHAN2_BADDR_CB                           0x54a1
#define DI_T7_CHAN2_BADDR_CR                           0x54a2
#define DI_T7_CHAN2_STRIDE_0                           0x54a3
#define DI_T7_CHAN2_STRIDE_1                           0x54a4

#define DI_SC2_MEM_GEN_REG                            0x5500 //off = 0x100
#define DI_SC2_MEM_CANVAS0                            0x5501
#define DI_SC2_MEM_CANVAS1                            0x5502
#define DI_SC2_MEM_LUMA_X0                            0x5503
#define DI_SC2_MEM_LUMA_Y0                            0x5504
#define DI_SC2_MEM_CHROMA_X0                          0x5505
#define DI_SC2_MEM_CHROMA_Y0                          0x5506
#define DI_SC2_MEM_LUMA_X1                            0x5507
#define DI_SC2_MEM_LUMA_Y1                            0x5508
#define DI_SC2_MEM_CHROMA_X1                          0x5509
#define DI_SC2_MEM_CHROMA_Y1                          0x550a
#define DI_SC2_MEM_RPT_LOOP                           0x550b
#define DI_SC2_MEM_LUMA0_RPT_PAT                      0x550c
#define DI_SC2_MEM_CHROMA0_RPT_PAT                    0x550d
#define DI_SC2_MEM_LUMA1_RPT_PAT                      0x550e
#define DI_SC2_MEM_CHROMA1_RPT_PAT                    0x550f
#define DI_SC2_MEM_LUMA_PSEL                          0x5510
#define DI_SC2_MEM_CHROMA_PSEL                        0x5511
#define DI_SC2_MEM_DUMMY_PIXEL                        0x5512
#define DI_SC2_MEM_LUMA_FIFO_SIZE                     0x5513
#define DI_SC2_MEM_AXI_CMD_CNT                        0x5514
#define DI_SC2_MEM_AXI_RDAT_CNT                       0x5515
#define DI_SC2_MEM_RANGE_MAP_Y                        0x5516
#define DI_SC2_MEM_RANGE_MAP_CB                       0x5517
#define DI_SC2_MEM_RANGE_MAP_CR                       0x5518
#define DI_SC2_MEM_GEN_REG2                           0x5519
#define DI_SC2_MEM_PROT                               0x551a
#define DI_SC2_MEM_URGENT_CTRL                        0x551b
#define DI_SC2_MEM_GEN_REG3                           0x551c
#define DI_SC2_MEM_CFMT_CTRL                          0x551d
#define DI_SC2_MEM_CFMT_W                             0x551e

//t7:
#define DI_T7_MEM_BADDR_Y                            0x5520
#define DI_T7_MEM_BADDR_CB                           0x5521
#define DI_T7_MEM_BADDR_CR                           0x5522
#define DI_T7_MEM_STRIDE_0                           0x5523
#define DI_T7_MEM_STRIDE_1                           0x5524

#define DI_SC2_IF1_GEN_REG                            0x5580	//off = 0x180
#define DI_SC2_IF1_CANVAS0                            0x5581
#define DI_SC2_IF1_CANVAS1                            0x5582
#define DI_SC2_IF1_LUMA_X0                            0x5583
#define DI_SC2_IF1_LUMA_Y0                            0x5584
#define DI_SC2_IF1_CHROMA_X0                          0x5585
#define DI_SC2_IF1_CHROMA_Y0                          0x5586
#define DI_SC2_IF1_LUMA_X1                            0x5587
#define DI_SC2_IF1_LUMA_Y1                            0x5588
#define DI_SC2_IF1_CHROMA_X1                          0x5589
#define DI_SC2_IF1_CHROMA_Y1                          0x558a
#define DI_SC2_IF1_RPT_LOOP                           0x558b
#define DI_SC2_IF1_LUMA0_RPT_PAT                      0x558c
#define DI_SC2_IF1_CHROMA0_RPT_PAT                    0x558d
#define DI_SC2_IF1_LUMA1_RPT_PAT                      0x558e
#define DI_SC2_IF1_CHROMA1_RPT_PAT                    0x558f
#define DI_SC2_IF1_LUMA_PSEL                          0x5590
#define DI_SC2_IF1_CHROMA_PSEL                        0x5591
#define DI_SC2_IF1_DUMMY_PIXEL                        0x5592
#define DI_SC2_IF1_LUMA_FIFO_SIZE                     0x5593
#define DI_SC2_IF1_AXI_CMD_CNT                        0x5594
#define DI_SC2_IF1_AXI_RDAT_CNT                       0x5595
#define DI_SC2_IF1_RANGE_MAP_Y                        0x5596
#define DI_SC2_IF1_RANGE_MAP_CB                       0x5597
#define DI_SC2_IF1_RANGE_MAP_CR                       0x5598
#define DI_SC2_IF1_GEN_REG2                           0x5599
#define DI_SC2_IF1_PROT                               0x559a
#define DI_SC2_IF1_URGENT_CTRL                        0x559b
#define DI_SC2_IF1_GEN_REG3                           0x559c
#define DI_SC2_IF1_CFMT_CTRL                          0x559d
#define DI_SC2_IF1_CFMT_W                             0x559e
//t7
#define DI_T7_IF1_BADDR_Y                            0x55a0
#define DI_T7_IF1_BADDR_CB                           0x55a1
#define DI_T7_IF1_BADDR_CR                           0x55a2
#define DI_T7_IF1_STRIDE_0                           0x55a3
#define DI_T7_IF1_STRIDE_1                           0x55a4

#define DI_SC2_IF0_GEN_REG                            0x5600	//off = 0x200
#define DI_SC2_IF0_CANVAS0                            0x5601
#define DI_SC2_IF0_CANVAS1                            0x5602
#define DI_SC2_IF0_LUMA_X0                            0x5603
#define DI_SC2_IF0_LUMA_Y0                            0x5604
#define DI_SC2_IF0_CHROMA_X0                          0x5605
#define DI_SC2_IF0_CHROMA_Y0                          0x5606
#define DI_SC2_IF0_LUMA_X1                            0x5607
#define DI_SC2_IF0_LUMA_Y1                            0x5608
#define DI_SC2_IF0_CHROMA_X1                          0x5609
#define DI_SC2_IF0_CHROMA_Y1                          0x560a
#define DI_SC2_IF0_RPT_LOOP                           0x560b
#define DI_SC2_IF0_LUMA0_RPT_PAT                      0x560c
#define DI_SC2_IF0_CHROMA0_RPT_PAT                    0x560d
#define DI_SC2_IF0_LUMA1_RPT_PAT                      0x560e
#define DI_SC2_IF0_CHROMA1_RPT_PAT                    0x560f
#define DI_SC2_IF0_LUMA_PSEL                          0x5610
#define DI_SC2_IF0_CHROMA_PSEL                        0x5611
#define DI_SC2_IF0_DUMMY_PIXEL                        0x5612
#define DI_SC2_IF0_LUMA_FIFO_SIZE                     0x5613
#define DI_SC2_IF0_AXI_CMD_CNT                        0x5614
#define DI_SC2_IF0_AXI_RDAT_CNT                       0x5615
#define DI_SC2_IF0_RANGE_MAP_Y                        0x5616
#define DI_SC2_IF0_RANGE_MAP_CB                       0x5617
#define DI_SC2_IF0_RANGE_MAP_CR                       0x5618
#define DI_SC2_IF0_GEN_REG2                           0x5619
#define DI_SC2_IF0_PROT                               0x561a
#define DI_SC2_IF0_URGENT_CTRL                        0x561b
#define DI_SC2_IF0_GEN_REG3                           0x561c
#define DI_SC2_IF0_CFMT_CTRL                          0x561d
#define DI_SC2_IF0_CFMT_W                             0x561e
//t7:
#define DI_T7_IF0_BADDR_Y                            0x5620
#define DI_T7_IF0_BADDR_CB                           0x5621
#define DI_T7_IF0_BADDR_CR                           0x5622
#define DI_T7_IF0_STRIDE_0                           0x5623
#define DI_T7_IF0_STRIDE_1                           0x5624

#define DI_SC2_IF2_GEN_REG                            0x5680	//off = 0x280
#define DI_SC2_IF2_CANVAS0                            0x5681
#define DI_SC2_IF2_CANVAS1                            0x5682
#define DI_SC2_IF2_LUMA_X0                            0x5683
#define DI_SC2_IF2_LUMA_Y0                            0x5684
#define DI_SC2_IF2_CHROMA_X0                          0x5685
#define DI_SC2_IF2_CHROMA_Y0                          0x5686
#define DI_SC2_IF2_LUMA_X1                            0x5687
#define DI_SC2_IF2_LUMA_Y1                            0x5688
#define DI_SC2_IF2_CHROMA_X1                          0x5689
#define DI_SC2_IF2_CHROMA_Y1                          0x568a
#define DI_SC2_IF2_RPT_LOOP                           0x568b
#define DI_SC2_IF2_LUMA0_RPT_PAT                      0x568c
#define DI_SC2_IF2_CHROMA0_RPT_PAT                    0x568d
#define DI_SC2_IF2_LUMA1_RPT_PAT                      0x568e
#define DI_SC2_IF2_CHROMA1_RPT_PAT                    0x568f
#define DI_SC2_IF2_LUMA_PSEL                          0x5690
#define DI_SC2_IF2_CHROMA_PSEL                        0x5691
#define DI_SC2_IF2_DUMMY_PIXEL                        0x5692
#define DI_SC2_IF2_LUMA_FIFO_SIZE                     0x5693
#define DI_SC2_IF2_AXI_CMD_CNT                        0x5694
#define DI_SC2_IF2_AXI_RDAT_CNT                       0x5695
#define DI_SC2_IF2_RANGE_MAP_Y                        0x5696
#define DI_SC2_IF2_RANGE_MAP_CB                       0x5697
#define DI_SC2_IF2_RANGE_MAP_CR                       0x5698
#define DI_SC2_IF2_GEN_REG2                           0x5699
#define DI_SC2_IF2_PROT                               0x569a
#define DI_SC2_IF2_URGENT_CTRL                        0x569b
#define DI_SC2_IF2_GEN_REG3                           0x569c
#define DI_SC2_IF2_CFMT_CTRL                          0x569d
#define DI_SC2_IF2_CFMT_W                             0x569e
//t7
#define DI_T7_IF2_BADDR_Y                            0x56a0
#define DI_T7_IF2_BADDR_CB                           0x56a1
#define DI_T7_IF2_BADDR_CR                           0x56a2
#define DI_T7_IF2_STRIDE_0                           0x56a3
#define DI_T7_IF2_STRIDE_1                           0x56a4

//==========================================================================
// VD_TOP for t7 each afbc add xx38
//==========================================================================
#define AFBCDM_VDTOP_CTRL0                         0x5438
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512

/*afbcd*/
#define AFBCDM_ENABLE                              0x5440
//Bit   31:29,    reserved
//Bit   28:23,    reg_gclk_ctrl_core     unsigned, default = 0
//Bit   22,       reg_fmt_size_sw_mode
//unsigned, default = 0, 0:hw mode 1:sw mode for format size
//Bit   21,       reg_addr_link_en  unsigned, default = 1, 1:enable
//Bit   20,       reg_fmt444_comb   unsigned, default = 0, 0: 444 8bit uncomb
//Bit   19,       reg_dos_uncomp_mode   unsigned  , default = 0
//Bit   18:16,    soft_rst          unsigned  , default = 4
//Bit   15:14,    reserved
//Bit   13:12,    ddr_blk_size      unsigned  , default = 1
//Bit   11:9,     cmd_blk_size      unsigned  , default = 3
//Bit   8,        dec_enable        unsigned  , default = 0
//Bit   7:2,      reserved
//Bit   1,        head_len_sel      unsigned  , default = 1
//Bit   0,        dec_frm_start     unsigned  , default = 0
#define AFBCDM_MODE                                0x5441
//Bit   31:30,    reserved
//Bit   29,       ddr_sz_mode
//uns, default = 0 , 0: fixed block ddr size 1 : unfixed block ddr size;
//Bit   28,       blk_mem_mode
//uns, default = 0 , 0: fixed 16x128 size; 1 : fixed 12x128 size
//Bit   27:26,    rev_mode
//uns, default = 0 , reverse mode
//Bit   25:24,    mif_urgent
//uns, default = 3 , info mif and data mif urgent
//Bit   23,       reserved
//Bit   22:16,    hold_line_num     uns, default = 0 ,
//Bit   15:14,    burst_len
//uns, default = 2, 0: burst1 1:burst2 2:burst4
//Bit   13:8,     compbits_yuv      uns, default = 0 ,
//                                  bit 1:0,:
//y  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 3:2,:
//u  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//                                  bit 5:4,:
//v  component bitwidth : 00-8bit 01-9bit 10-10bit 11-12bit
//Bit   7:6,      vert_skip_y       uns, default = 0 ,
// luma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   5:4,      horz_skip_y       uns, default = 0 ,
// luma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   3:2,      vert_skip_uv      uns, default = 0 ,
// chroma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   1:0,      horz_skip_uv      uns, default = 0 ,
// chroma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
#define AFBCDM_SIZE_IN                             0x5442
//Bit   31:29,    reserved
//Bit   28:16     hsize_in
//uns, default = 1920 , pic horz size in  unit: pixel
//Bit   15:13,    reserved
//Bit   12:0,     vsize_in
//uns, default = 1080 , pic vert size in  unit: pixel
#define AFBCDM_DEC_DEF_COLOR                       0x5443
//Bit   31:30,   reserved
//Bit   29:20,   def_color_y
//uns, default = 255, afbc dec y default setting value
//Bit   19:10,   def_color_u
//uns, default = 128, afbc dec u default setting value
//Bit    9: 0,   def_color_v
//uns, default = 128, afbc dec v default setting value
#define AFBCDM_CONV_CTRL                           0x5444
//Bit   31:14,   reserved
//Bit   13:12,   fmt_mode
//uns, default = 2, 0:yuv444 1:yuv422 2:yuv420
//Bit   11: 0,   conv_lbuf_len
//uns, default = 256, unit=16 pixel need to set = 2^n
#define AFBCDM_LBUF_DEPTH                          0x5445
//Bit   31:28,   reserved
//Bit   27:16,   dec_lbuf_depth      uns, default = 128; // unit= 8 pixel
//Bit   15:12,   reserved
//Bit   11:0,    mif_lbuf_depth      uns, default = 128;
#define AFBCDM_HEAD_BADDR                          0x5446
//Bit   31:0,   mif_info_baddr      uns, default = 32'h0;
#define AFBCDM_BODY_BADDR                          0x5447
//Bit   31:0,   mif_data_baddr      uns, default = 32'h00010000;
#define AFBCDM_SIZE_OUT                            0x5448
//Bit   31:29,   reserved
//Bit   28:16,   hsize_out           uns, default = 1920    ; // unit: 1 pixel
//Bit   15:13,   reserved
//Bit    12:0,   vsize_out           uns, default = 1080 ; // unit: 1 pixel
#define AFBCDM_OUT_YSCOPE                          0x5449
//Bit   31:29,   reserved
//Bit   28:16,   out_vert_bgn        uns, default = 0    ; // unit: 1 pixel
//Bit   15:13,   reserved
//Bit    12:0,   out_vert_end        uns, default = 1079 ;
// unit: 1 pixel
#define AFBCDM_STAT                                0x544a
//Bit   31:1,   ro_dbg_top_info      uns,   default = 0
//Bit      0,   frm_end_stat         uns, default = 0 frame end status
#define AFBCDM_VD_CFMT_CTRL                        0x544b

//Bit 31    cfmt_gclk_bit_dis      uns, default = 0    ;
//  it true, disable clock, otherwise enable clock
//Bit 30    cfmt_soft_rst_bit      uns, default = 0    ;
//  soft rst bit
//Bit 29    reserved
//Bit 28    chfmt_rpt_pix          uns, default = 0    ;
//  if true, horizontal formatter use repeating to generate pixel,
//otherwise use bilinear interpolation
//Bit 27:24 chfmt_ini_phase        uns, default = 0    ;
//  horizontal formatter initial phase
//Bit 23    chfmt_rpt_p0_en        uns, default = 0    ;
//  horizontal formatter repeat pixel 0 enable
//Bit 22:21 chfmt_yc_ratio         uns, default = 0    ;
//  horizontal Y/C ratio, 00: 1:1, 01: 2:1, 10: 4:1
//Bit 20    chfmt_en               uns, default = 0    ;
//  horizontal formatter enable
//Bit 19    cvfmt_phase0_always_en uns, default = 0    ;
//if true, always use phase0 while vertical formater, meaning always
//          repeat data, no interpolation
//Bit 18    cvfmt_rpt_last_dis     uns, default = 0    ;
//if true, disable vertical formatter chroma repeat last line
//Bit 17    cvfmt_phase0_nrpt_en   uns, default = 0    ;
//vertical formatter dont need repeat line on phase0, 1: enable, 0: disable
//Bit 16    cvfmt_rpt_line0_en     uns, default = 0    ;
//vertical formatter repeat line 0 enable
//Bit 15:12 cvfmt_skip_line_num    uns, default = 0    ;
//vertical formatter skip line num at the beginning
//Bit 11:8  cvfmt_ini_phase        uns, default = 0    ;
//vertical formatter initial phase
//Bit 7:1   cvfmt_phase_step       uns, default = 0    ;
//vertical formatter phase step (3.4)
//Bit 0     cvfmt_en               uns, default = 0    ;
//vertical formatter enable
#define AFBCDM_VD_CFMT_W                           0x544c
//Bit 31:29 reserved
//Bit 28:16 chfmt_w
// uns, default = 0    ;horizontal formatter width
//Bit 15:13 reserved
//Bit 12:0  cvfmt_w
//uns, default = 0    ;vertical formatter width
#define AFBCDM_MIF_HOR_SCOPE                       0x544d
//Bit   31:26,   reserved
//Bit   25:16,   mif_blk_bgn_h        uns, default = 0  ;
// unit: 32 pixel/block hor
//Bit   15:10,   reserved
//Bit    9: 0,   mif_blk_end_h        uns, default = 59 ;
// unit: 32 pixel/block hor
#define AFBCDM_MIF_VER_SCOPE                       0x544e
//Bit   31:28,   reserved
//Bit   27:16,   mif_blk_bgn_v        uns, default = 0  ;
// unit: 32 pixel/block ver
//Bit   15:12,   reserved
//Bit   11: 0,   mif_blk_end_v        uns, default = 269;
// unit: 32 pixel/block ver
#define AFBCDM_PIXEL_HOR_SCOPE                     0x544f
//Bit   31:29,   reserved
//Bit   28:16,   dec_pixel_bgn_h        uns, default = 0  ;
// unit: pixel
//Bit   15:13,   reserved
//Bit   12: 0,   dec_pixel_end_h        uns, default = 1919 ;
// unit: pixel
#define AFBCDM_PIXEL_VER_SCOPE                     0x5450
//Bit   31:29,   reserved
//Bit   28:16,   dec_pixel_bgn_v        uns, default = 0  ;
// unit: pixel
//Bit   15:13,   reserved
//Bit   12: 0,   dec_pixel_end_v        uns, default = 1079 ;
// unit: pixel
#define AFBCDM_VD_CFMT_H                           0x5451
//Bit 31:13,    reserved
//Bit 12:0      cfmt_h  uns, default = 142  ;
//vertical formatter height
#define AFBCDM_IQUANT_ENABLE                       0x5452
//Bit 31:12        reserved
//Bit  11          reg_quant_expand_en_1
//unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  10          reg_quant_expand_en_0
//unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  9: 8        reg_bcleav_ofst
//signed ,       RW, default = 0
//bcleave ofset to get lower range, especially under lossy,
//for v1/v2, x=0 is equivalent, default = -1;
//Bit  7: 5        reserved
//Bit  4           reg_quant_enable_1
// unsigned ,    RW, default = 0  enable for quant to get some lossy
//Bit  3: 1        reserved
//Bit  0           reg_quant_enable_0
// unsigned ,    RW, default = 0  enable for quant to get some lossy
#define AFBCDM_IQUANT_LUT_1                        0x5453
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_0_11
// unsigned ,    RW, default = 0
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_0_10
// unsigned ,    RW, default = 1
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_0_9
// unsigned ,    RW, default = 2
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_0_8
// unsigned ,    RW, default = 3
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved/
//Bit 14:12        reg_iquant_yclut_0_7
// unsigned ,    RW, default = 4
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_6
// unsigned ,    RW, default = 5
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_5
// unsigned ,    RW, default = 5
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_4
// unsigned ,    RW, default = 4
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define AFBCDM_IQUANT_LUT_2                        0x5454
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_3
// unsigned ,    RW, default = 3
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_2
// unsigned ,    RW, default = 2
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_1
// unsigned ,    RW, default = 1
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_0
// unsigned ,    RW, default = 0
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define AFBCDM_IQUANT_LUT_3                        0x5455
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_1_11
// unsigned ,    RW, default = 0
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_1_10
// unsigned ,    RW, default = 1
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_1_9
// unsigned ,    RW, default = 2
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_1_8
// unsigned ,    RW, default = 3
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_7
// unsigned ,    RW, default = 4
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_6
// unsigned ,    RW, default = 5
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_5
// unsigned ,    RW, default = 5
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_4
// unsigned ,    RW, default = 4
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define AFBCDM_IQUANT_LUT_4                        0x5456
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_3
// unsigned ,    RW, default = 3
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_2
// unsigned ,    RW, default = 2
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_1
// unsigned ,    RW, default = 1
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_0
// unsigned ,    RW, default = 0
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//add for t3x
#define AFBCDM_BURST_CTRL                       0x5457//32'hff81515c
//Bit 31:5     reserved
//Bit 4        reg_ofset_burst4_en              //unsigned RW  default = 0
//Bit 3        reg_burst_length_add_en          //unsigned RW  default = 0
//Bit 2:0      reg_burst_length_add_value       //unsigned RW  default = 2
#define AFBCDM_LOSS_CTRL                        0x5458//32'hff815160
//Bit 31:5     reserved
//Bit 4        reg_fix_cr_en                    //unsigned ,RW  default = 0
//Bit 3:0      reg_quant_diff_root_leave        //unsigned ,RW  default = 2
#define AFBCDM_ROT_CTRL                            0x5460
//Bit   31:30   reg_rot_ohds2_mode
//unsigned, RW, default = 0 ,
//rot output format down hor drop mode,0:average 1:use 0 2:use 1
//Bit   29:28   reg_rot_ovds2_mode
//unsigned, RW, default = 0 ,
//rot output format down ver drop mode,0:average 1:use 0 2:use 1
//Bit   27      reg_pip_mode
//unsigned, RW, default = 0 ,
//0:dec_src from vdin/dos  1:dec_src from pip
//Bit   26:24   reg_rot_uv_vshrk_drop_mode
//unsigned, RW, default = 0 , 0:
//average (1/2: 1:left 2:right)
// (1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   23      reserved
//Bit   22:20   reg_rot_uv_hshrk_drop_mode
//unsigned, RW, default = 0 , 0:
//average (1/2: 1:left 2:right)
//(1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   19:18   reg_rot_uv_vshrk_ratio
//unsigned, RW, default = 0 , 0:
//no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   17:16   reg_rot_uv_hshrk_ratio
//unsigned, RW, default = 0 , 0:
//no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   15      reserved
//Bit   14:12   reg_rot_y_vshrk_drop_mode
//unsigned, RW, default = 0 , 0:
//average (1/2: 1:left 2:right)
//(1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   11      reserved
//Bit   10:8    reg_rot_y_hshrk_drop_mode
//unsigned, RW, default = 0 , 0:average
//(1/2: 1:left 2:right)
//(1/4: 1:[0] 2:[1] 3:[2] 4:[3], 5:left_121 6:right_121)
//Bit   7:6     reg_rot_y_vshrk_ratio
//unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   5:4     reg_rot_y_hshrk_ratio
//unsigned, RW, default = 0 , 0:no shrink  1:1/2 shrink  2:1/4 shrink
//Bit   3:2     reg_rot_uv422_drop_mode
//unsigned, RW, default = 0 , 0:average 1:left 2:right
//Bit   1       reg_rot_uv422_omode
//unsigned, RW, default = 0 ,
//when rot input fmt422, 0:output_uv422  1:output_uv420
//Bit   0       reg_rot_enable
//unsigned, RW, default = 0 , rotation enable
#define AFBCDM_ROT_SCOPE                           0x5461
//Bit   31:26   reserved
//Bit   25:20   reg_rot_debug_probe
//unsigned, RW , default = 0,
//y:[2:0] uv:[5:3]; 0:iblk_size 1:oblk_size 2:
//iblk_cnt 3:oblk_cnt 4:hsize_in 5:vsize_in 6:vstep
//Bit   19      reg_rot_dout_ds_mode_sw
//unsigned, RW , default = 0, 0:use hardware mode 1:use software mode
//Bit   18:17   reg_rot_dout_ds_mode
//unsigned, RW , default = 0,
//rot output fmt_down mode: [0]:h_downscale [1]:v_downscale
//Bit   16      reg_rot_ifmt_force444
//unsigned, RW , default = 1, 1: rot input fmt force as 444
//Bit   15:14   reg_rot_ofmt_mode
//unsigned, RW , default = 0, rot output fmt mode
//Bit   13:12   reg_rot_compbits_out_y
//unsigned, RW , default = 0, rot output compbit y
//Bit   11:10   reg_rot_compbits_out_uv
//unsigned, RW , default = 0, rot output compbit uv
//Bit   9:8     reg_rot_wrbgn_v
//unsigned, RW , default = 0, rot pic vert size window begin pixel
//Bit   7:5     reserved
//Bit   4:0     reg_rot_wrbgn_h
//unsigned, RW , default = 0, rot pic hori size window begin pixel
#define AFBCDM_RPLC_CTRL                           0x5462
//Bit   31        reg_rplc_inter_corr_en
//unsigned, RW , default = 0   ,
//pip replace inte-frame edge correct enable
//Bit   30        reg_rplc_dummy_corr_en
//unsigned, RW , default = 0   ,
//pip replace outsize of real-pipframe edge correct enable
//Bit   29        reg_rplc_byps
//unsigned, RW , default = 1   , //pip replace module bypass
//Bit   28:16     reg_rplc_vsize_in
//unsigned, RW , default = 1080, //
//Bit   15:13     reserved
//Bit   12:0      reg_rplc_hsize_in
//unsigned, RW , default = 1920,
#define AFBCDM_RPLC_PICEN                          0x5463
//Bit  31:28      reserved
//Bit  27:16      reg_rplc_def_color_y
//unsigned, RW , default =0        , //pip replace def_color_y
//Bit  15:0       reg_rplc_pic_enable
//unsigned, RW , default =16'hffff , //pip replace pip_picure enbale
#define AFBCDM_RPLC_DEFCOL                         0x5464
//Bit  31:24     reserved
//Bit  23:12     reg_rplc_def_color_v
//unsigned, RW , default =0        , //pip replace def_color_v
//Bit  11:0      reg_rplc_def_color_u
//unsigned, RW , default =0        , //pip replace def_color_u
#define AFBCDM_RPLC_SCPXN_ADDR                     0x5465
//Bit  31:0      reg_rplc_scpxn_addr
//unsigned, RW , default =0        , //pip replace scopx16 addr
#define AFBCDM_RPLC_SCPXN_DATA                     0x5466
//Bit  31:0      reg_rplc_scpxn_data
//unsigned, RW , default =0        , //pip replace scopx16 data
#define AFBCDM_ROT_RO_STAT                         0x5467
//Bit   31:0     ro_rot_debug
//unsigned, RO , default = 0, rot some status
#define AFBCDM_FGRAIN_CTRL                         0x5470
//Bit 31:26     reserved
//Bit 25:24     reg_sync_ctrl
// unsigned , RW, default = 0
//Bit 23        reserved
//Bit 22        reg_dma_st_clr
// unsigned , RW, default = 0 clear DMA error status
//Bit 21        reg_hold4dma_scale
// unsigned , RW, default = 0 1 to wait DMA scale data
//ready before accept input data        default = 0
//Bit 20        reg_hold4dma_tbl
// unsigned , RW, default = 0 1 to wait DMA grain table
//data ready before accept input data  default = 0
//Bit 19        reg_cin_uv_swap
// unsigned , RW, default = 0 1 to swap U/V input
//Bit 18        reg_cin_rev
// unsigned , RW, default = 0 1 to reverse the U/V input order
//Bit 17        reg_yin_rev
// unsigned , RW, default = 0 1 to reverse the Y input order
//Bit 16        reg_fgrain_ext_imode
// unsigned , RW, default = 1 0 to
//indicate the input data is *4 in 8bit mode
//Bit 15        reg_use_par_apply_fgrain
// unsigned , RW, default = 0 1 to use apply_fgrain from DMA table
//Bit 14        reg_fgrain_last_ln_mode
// unsigned , RW, default = 0 1 to keep fgrain noise generator
//though the input is finished for rdmif.
//Bit 13        reg_fgrain_use_sat4bp
// unsigned , RW, default = 0 1 to use fgain_max/min
//for sat not {DW{1'b1}}/0
//Bit 12        reg_apply_c_mode
// unsigned , RW, default = 1 0 to following C
//Bit 11        reg_fgrain_tbl_sign_mode
// unsigned , RW, default = 1 0 to indicate signed bit
//is not extended in 8bit mode
//Bit 10        reg_fgrain_tbl_ext_mode
// unsigned , RW, default = 1 0 to indicate the grain table is *4 in 8bit mode
//Bit  9: 8     reg_fmt_mode
// unsigned , RW, default = 2 0:444; 1:422; 2:420; 3:reserved
//Bit  7: 6     reg_comp_bits
// unsigned , RW, default = 1 0:8bits; 1:10bits, else 12 bits
//Bit  5: 4     reg_rev_mode
// unsigned . RW, default = 0 0:h_rev; 1:v_rev;
//Bit  3        reserved
//Bit  2        reg_block_mode
// unsigned , RW, default = 1
//Bit  1        reg_fgrain_loc_en
// unsigned , RW, default = 0 frame-based  fgrain enable
//Bit  0        reg_fgrain_glb_en
// unsigned , RW, default = 0 global-based fgrain enable
#define AFBCDM_FGRAIN_WIN_H                        0x5471
//Bit  31:16     reg_win_end_h      .unsigned , default = 3812
//Bit  15: 0,    reg_win_bgn_h      .unsigned , default = 0
#define AFBCDM_FGRAIN_WIN_V                        0x5472
//Bit  31:16     reg_win_end_v      .unsigned , default = 2156
//Bit  15: 0,    reg_win_bgn_v      .unsigned , default = 0
#define AFBCDM_FGRAIN_SW_Y_RANNGE                  0x5473
//Bit 31,        reg_fgrain_sw_yrange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_ymax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_ymin        .unsigned , default = 0
#define AFBCDM_FGRAIN_SW_C_RANNGE                  0x5474
//Bit 31,        reg_fgrain_sw_crange   .unsigned , default = 0
//Bit 30:26,     reserved
//Bit 25:16,     reg_fgrain_cmax        .unsigned , default = 1023
//Bit 15:10,     reserved
//Bit  9: 0,     reg_fgrain_cmin        .unsigned , default = 0
#define AFBCDM_FGRAIN_GCLK_CTRL_0                  0x5475
//Bit 31:0,      reg_fgrain_gclk_ctrl0  .unsigned , default = 0
#define AFBCDM_FGRAIN_GCLK_CTRL_1                  0x5476
//Bit 31:0,      reg_fgrain_gclk_ctrl1  .unsigned , default = 0
#define AFBCDM_FGRAIN_GCLK_CTRL_2                  0x5477
//Bit 31:0,      reg_fgrain_gclk_ctrl2  .unsigned , default = 0
#define AFBCDM_FGRAIN_PARAM_ADDR                   0x5478
#define AFBCDM_FGRAIN_PARAM_DATA                   0x5479

/*afbcd for vd1*/
#define AFBCDM_VD1_ENABLE                              0x4840 //-0xc00
#define AFBCDM_VD1_MODE                                0x4841
#define AFBCDM_VD1_SIZE_IN                             0x4842
#define AFBCDM_VD1_DEC_DEF_COLOR                       0x4843
#define AFBCDM_VD1_CONV_CTRL                           0x4844
#define AFBCDM_VD1_LBUF_DEPTH                          0x4845
#define AFBCDM_VD1_HEAD_BADDR                          0x4846
#define AFBCDM_VD1_BODY_BADDR                          0x4847
#define AFBCDM_VD1_SIZE_OUT                            0x4848
#define AFBCDM_VD1_OUT_YSCOPE                          0x4849
#define AFBCDM_VD1_STAT                                0x484a
#define AFBCDM_VD1_VD_CFMT_CTRL                        0x484b
#define AFBCDM_VD1_VD_CFMT_W                           0x484c
#define AFBCDM_VD1_MIF_HOR_SCOPE                       0x484d
#define AFBCDM_VD1_MIF_VER_SCOPE                       0x484e
#define AFBCDM_VD1_PIXEL_HOR_SCOPE                     0x484f
#define AFBCDM_VD1_PIXEL_VER_SCOPE                     0x4850
#define AFBCDM_VD1_VD_CFMT_H                           0x4851
#define AFBCDM_VD1_IQUANT_ENABLE                       0x4852
#define AFBCDM_VD1_IQUANT_LUT_1                        0x4853
#define AFBCDM_VD1_IQUANT_LUT_2                        0x4854
#define AFBCDM_VD1_IQUANT_LUT_3                        0x4855
#define AFBCDM_VD1_IQUANT_LUT_4                        0x4856
#define AFBCDM_VD1_ROT_CTRL                            0x4860
#define AFBCDM_VD1_ROT_SCOPE                           0x4861
#define AFBCDM_VD1_RPLC_CTRL                           0x4862
#define AFBCDM_VD1_RPLC_PICEN                          0x4863
#define AFBCDM_VD1_RPLC_DEFCOL                         0x4864
#define AFBCDM_VD1_RPLC_SCPXN_ADDR                     0x4865
#define AFBCDM_VD1_RPLC_SCPXN_DATA                     0x4866
#define AFBCDM_VD1_ROT_RO_STAT                         0x4867
#define AFBCDM_VD1_FGRAIN_CTRL                         0x4870
#define AFBCDM_VD1_FGRAIN_WIN_H                        0x4871
#define AFBCDM_VD1_FGRAIN_WIN_V                        0x4872
#define AFBCDM_VD1_FGRAIN_SW_Y_RANNGE                  0x4873
#define AFBCDM_VD1_FGRAIN_SW_C_RANNGE                  0x4874
#define AFBCDM_VD1_FGRAIN_GCLK_CTRL_0                  0x4875
#define AFBCDM_VD1_FGRAIN_GCLK_CTRL_1                  0x4876
#define AFBCDM_VD1_FGRAIN_GCLK_CTRL_2                  0x4877
#define AFBCDM_VD1_FGRAIN_PARAM_ADDR                   0x4878
#define AFBCDM_VD1_FGRAIN_PARAM_DATA                   0x4879

//for t7:
#define AFBCDM_INP_CTRL0                         0x5438
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512

/*afbcd add*/
#define AFBCDM_INP_ENABLE                              0x5440
#define AFBCDM_INP_MODE                                0x5441
#define AFBCDM_INP_SIZE_IN                             0x5442
#define AFBCDM_INP_DEC_DEF_COLOR                       0x5443
#define AFBCDM_INP_CONV_CTRL                           0x5444
#define AFBCDM_INP_LBUF_DEPTH                          0x5445
#define AFBCDM_INP_HEAD_BADDR                          0x5446
#define AFBCDM_INP_BODY_BADDR                          0x5447
#define AFBCDM_INP_SIZE_OUT                            0x5448
#define AFBCDM_INP_OUT_YSCOPE                          0x5449
#define AFBCDM_INP_STAT                                0x544a
#define AFBCDM_INP_VD_CFMT_CTRL                        0x544b
#define AFBCDM_INP_VD_CFMT_W                           0x544c
#define AFBCDM_INP_MIF_HOR_SCOPE                       0x544d
#define AFBCDM_INP_MIF_VER_SCOPE                       0x544e
#define AFBCDM_INP_PIXEL_HOR_SCOPE                     0x544f
#define AFBCDM_INP_PIXEL_VER_SCOPE                     0x5450
#define AFBCDM_INP_VD_CFMT_H                           0x5451
#define AFBCDM_INP_IQUANT_ENABLE                       0x5452
#define AFBCDM_INP_IQUANT_LUT_1                        0x5453
#define AFBCDM_INP_IQUANT_LUT_2                        0x5454
#define AFBCDM_INP_IQUANT_LUT_3                        0x5455
#define AFBCDM_INP_IQUANT_LUT_4                        0x5456
#define AFBCDM_INP_ROT_CTRL                            0x5460
#define AFBCDM_INP_ROT_SCOPE                           0x5461
#define AFBCDM_INP_RPLC_CTRL                           0x5462
#define AFBCDM_INP_RPLC_PICEN                          0x5463
#define AFBCDM_INP_RPLC_DEFCOL                         0x5464
#define AFBCDM_INP_RPLC_SCPXN_ADDR                     0x5465
#define AFBCDM_INP_RPLC_SCPXN_DATA                     0x5466
#define AFBCDM_INP_ROT_RO_STAT                         0x5467
#define AFBCDM_INP_FGRAIN_CTRL                         0x5470
#define AFBCDM_INP_FGRAIN_WIN_H                        0x5471
#define AFBCDM_INP_FGRAIN_WIN_V                        0x5472
#define AFBCDM_INP_FGRAIN_SW_Y_RANNGE                  0x5473
#define AFBCDM_INP_FGRAIN_SW_C_RANNGE                  0x5474
#define AFBCDM_INP_FGRAIN_GCLK_CTRL_0                  0x5475
#define AFBCDM_INP_FGRAIN_GCLK_CTRL_1                  0x5476
#define AFBCDM_INP_FGRAIN_GCLK_CTRL_2                  0x5477
#define AFBCDM_INP_FGRAIN_PARAM_ADDR                   0x5478
#define AFBCDM_INP_FGRAIN_PARAM_DATA                   0x5479

//for t7:
#define AFBCDM_CHAN2_CTRL0                         0x54b8
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512

#define AFBCDM_CHAN2_ENABLE                              0x54c0	//0x80
#define AFBCDM_CHAN2_MODE                                0x54c1
#define AFBCDM_CHAN2_SIZE_IN                             0x54c2
#define AFBCDM_CHAN2_DEC_DEF_COLOR                       0x54c3
#define AFBCDM_CHAN2_CONV_CTRL                           0x54c4
#define AFBCDM_CHAN2_LBUF_DEPTH                          0x54c5
#define AFBCDM_CHAN2_HEAD_BADDR                          0x54c6
#define AFBCDM_CHAN2_BODY_BADDR                          0x54c7
#define AFBCDM_CHAN2_SIZE_OUT                            0x54c8
#define AFBCDM_CHAN2_OUT_YSCOPE                          0x54c9
#define AFBCDM_CHAN2_STAT                                0x54ca
#define AFBCDM_CHAN2_VD_CFMT_CTRL                        0x54cb
#define AFBCDM_CHAN2_VD_CFMT_W                           0x54cc
#define AFBCDM_CHAN2_MIF_HOR_SCOPE                       0x54cd
#define AFBCDM_CHAN2_MIF_VER_SCOPE                       0x54ce
#define AFBCDM_CHAN2_PIXEL_HOR_SCOPE                     0x54cf
#define AFBCDM_CHAN2_PIXEL_VER_SCOPE                     0x54d0
#define AFBCDM_CHAN2_VD_CFMT_H                           0x54d1
#define AFBCDM_CHAN2_IQUANT_ENABLE                       0x54d2
#define AFBCDM_CHAN2_IQUANT_LUT_1                        0x54d3
#define AFBCDM_CHAN2_IQUANT_LUT_2                        0x54d4
#define AFBCDM_CHAN2_IQUANT_LUT_3                        0x54d5
#define AFBCDM_CHAN2_IQUANT_LUT_4                        0x54d6
#define AFBCDM_CHAN2_ROT_CTRL                            0x54e0
#define AFBCDM_CHAN2_ROT_SCOPE                           0x54e1
#define AFBCDM_CHAN2_RPLC_CTRL                           0x54e2
#define AFBCDM_CHAN2_RPLC_PICEN                          0x54e3
#define AFBCDM_CHAN2_RPLC_DEFCOL                         0x54e4
#define AFBCDM_CHAN2_RPLC_SCPXN_ADDR                     0x54e5
#define AFBCDM_CHAN2_RPLC_SCPXN_DATA                     0x54e6
#define AFBCDM_CHAN2_ROT_RO_STAT                         0x54e7
#define AFBCDM_CHAN2_FGRAIN_CTRL                         0x54f0
#define AFBCDM_CHAN2_FGRAIN_WIN_H                        0x54f1
#define AFBCDM_CHAN2_FGRAIN_WIN_V                        0x54f2
#define AFBCDM_CHAN2_FGRAIN_SW_Y_RANNGE                  0x54f3
#define AFBCDM_CHAN2_FGRAIN_SW_C_RANNGE                  0x54f4
#define AFBCDM_CHAN2_FGRAIN_GCLK_CTRL_0                  0x54f5
#define AFBCDM_CHAN2_FGRAIN_GCLK_CTRL_1                  0x54f6
#define AFBCDM_CHAN2_FGRAIN_GCLK_CTRL_2                  0x54f7
#define AFBCDM_CHAN2_FGRAIN_PARAM_ADDR                   0x54f8
#define AFBCDM_CHAN2_FGRAIN_PARAM_DATA                   0x54f9

//for t7:
#define AFBCDM_MEM_CTRL0                         0x5538
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512

#define AFBCDM_MEM_ENABLE                              0x5540	/*0x100*/
#define AFBCDM_MEM_MODE                                0x5541
#define AFBCDM_MEM_SIZE_IN                             0x5542
#define AFBCDM_MEM_DEC_DEF_COLOR                       0x5543
#define AFBCDM_MEM_CONV_CTRL                           0x5544
#define AFBCDM_MEM_LBUF_DEPTH                          0x5545
#define AFBCDM_MEM_HEAD_BADDR                          0x5546
#define AFBCDM_MEM_BODY_BADDR                          0x5547
#define AFBCDM_MEM_SIZE_OUT                            0x5548
#define AFBCDM_MEM_OUT_YSCOPE                          0x5549
#define AFBCDM_MEM_STAT                                0x554a
#define AFBCDM_MEM_VD_CFMT_CTRL                        0x554b
#define AFBCDM_MEM_VD_CFMT_W                           0x554c
#define AFBCDM_MEM_MIF_HOR_SCOPE                       0x554d
#define AFBCDM_MEM_MIF_VER_SCOPE                       0x554e
#define AFBCDM_MEM_PIXEL_HOR_SCOPE                     0x554f
#define AFBCDM_MEM_PIXEL_VER_SCOPE                     0x5550
#define AFBCDM_MEM_VD_CFMT_H                           0x5551
#define AFBCDM_MEM_IQUANT_ENABLE                       0x5552
#define AFBCDM_MEM_IQUANT_LUT_1                        0x5553
#define AFBCDM_MEM_IQUANT_LUT_2                        0x5554
#define AFBCDM_MEM_IQUANT_LUT_3                        0x5555
#define AFBCDM_MEM_IQUANT_LUT_4                        0x5556
#define AFBCDM_MEM_ROT_CTRL                            0x5560
#define AFBCDM_MEM_ROT_SCOPE                           0x5561
#define AFBCDM_MEM_RPLC_CTRL                           0x5562
#define AFBCDM_MEM_RPLC_PICEN                          0x5563
#define AFBCDM_MEM_RPLC_DEFCOL                         0x5564
#define AFBCDM_MEM_RPLC_SCPXN_ADDR                     0x5565
#define AFBCDM_MEM_RPLC_SCPXN_DATA                     0x5566
#define AFBCDM_MEM_ROT_RO_STAT                         0x5567
#define AFBCDM_MEM_FGRAIN_CTRL                         0x5570
#define AFBCDM_MEM_FGRAIN_WIN_H                        0x5571
#define AFBCDM_MEM_FGRAIN_WIN_V                        0x5572
#define AFBCDM_MEM_FGRAIN_SW_Y_RANNGE                  0x5573
#define AFBCDM_MEM_FGRAIN_SW_C_RANNGE                  0x5574
#define AFBCDM_MEM_FGRAIN_GCLK_CTRL_0                  0x5575
#define AFBCDM_MEM_FGRAIN_GCLK_CTRL_1                  0x5576
#define AFBCDM_MEM_FGRAIN_GCLK_CTRL_2                  0x5577
#define AFBCDM_MEM_FGRAIN_PARAM_ADDR                   0x5578
#define AFBCDM_MEM_FGRAIN_PARAM_DATA                   0x5579

//for t7:
#define AFBCDM_IF1_CTRL0                         0x55b8
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512

#define AFBCDM_IF1_ENABLE                              0x55c0	/*0x180*/
#define AFBCDM_IF1_MODE                                0x55c1
#define AFBCDM_IF1_SIZE_IN                             0x55c2
#define AFBCDM_IF1_DEC_DEF_COLOR                       0x55c3
#define AFBCDM_IF1_CONV_CTRL                           0x55c4
#define AFBCDM_IF1_LBUF_DEPTH                          0x55c5
#define AFBCDM_IF1_HEAD_BADDR                          0x55c6
#define AFBCDM_IF1_BODY_BADDR                          0x55c7
#define AFBCDM_IF1_SIZE_OUT                            0x55c8
#define AFBCDM_IF1_OUT_YSCOPE                          0x55c9
#define AFBCDM_IF1_STAT                                0x55ca
#define AFBCDM_IF1_VD_CFMT_CTRL                        0x55cb
#define AFBCDM_IF1_VD_CFMT_W                           0x55cc
#define AFBCDM_IF1_MIF_HOR_SCOPE                       0x55cd
#define AFBCDM_IF1_MIF_VER_SCOPE                       0x55ce
#define AFBCDM_IF1_PIXEL_HOR_SCOPE                     0x55cf
#define AFBCDM_IF1_PIXEL_VER_SCOPE                     0x55d0
#define AFBCDM_IF1_VD_CFMT_H                           0x55d1
#define AFBCDM_IF1_IQUANT_ENABLE                       0x55d2
#define AFBCDM_IF1_IQUANT_LUT_1                        0x55d3
#define AFBCDM_IF1_IQUANT_LUT_2                        0x55d4
#define AFBCDM_IF1_IQUANT_LUT_3                        0x55d5
#define AFBCDM_IF1_IQUANT_LUT_4                        0x55d6
#define AFBCDM_IF1_ROT_CTRL                            0x55e0
#define AFBCDM_IF1_ROT_SCOPE                           0x55e1
#define AFBCDM_IF1_RPLC_CTRL                           0x55e2
#define AFBCDM_IF1_RPLC_PICEN                          0x55e3
#define AFBCDM_IF1_RPLC_DEFCOL                         0x55e4
#define AFBCDM_IF1_RPLC_SCPXN_ADDR                     0x55e5
#define AFBCDM_IF1_RPLC_SCPXN_DATA                     0x55e6
#define AFBCDM_IF1_ROT_RO_STAT                         0x55e7
#define AFBCDM_IF1_FGRAIN_CTRL                         0x55f0
#define AFBCDM_IF1_FGRAIN_WIN_H                        0x55f1
#define AFBCDM_IF1_FGRAIN_WIN_V                        0x55f2
#define AFBCDM_IF1_FGRAIN_SW_Y_RANNGE                  0x55f3
#define AFBCDM_IF1_FGRAIN_SW_C_RANNGE                  0x55f4
#define AFBCDM_IF1_FGRAIN_GCLK_CTRL_0                  0x55f5
#define AFBCDM_IF1_FGRAIN_GCLK_CTRL_1                  0x55f6
#define AFBCDM_IF1_FGRAIN_GCLK_CTRL_2                  0x55f7
#define AFBCDM_IF1_FGRAIN_PARAM_ADDR                   0x55f8
#define AFBCDM_IF1_FGRAIN_PARAM_DATA                   0x55f9

//for t7:
#define AFBCDM_IF0_CTRL0                         0x5638
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512

#define AFBCDM_IF0_ENABLE                              0x5640	/*0x200*/
#define AFBCDM_IF0_MODE                                0x5641
#define AFBCDM_IF0_SIZE_IN                             0x5642
#define AFBCDM_IF0_DEC_DEF_COLOR                       0x5643
#define AFBCDM_IF0_CONV_CTRL                           0x5644
#define AFBCDM_IF0_LBUF_DEPTH                          0x5645
#define AFBCDM_IF0_HEAD_BADDR                          0x5646
#define AFBCDM_IF0_BODY_BADDR                          0x5647
#define AFBCDM_IF0_SIZE_OUT                            0x5648
#define AFBCDM_IF0_OUT_YSCOPE                          0x5649
#define AFBCDM_IF0_STAT                                0x564a
#define AFBCDM_IF0_VD_CFMT_CTRL                        0x564b
#define AFBCDM_IF0_VD_CFMT_W                           0x564c
#define AFBCDM_IF0_MIF_HOR_SCOPE                       0x564d
#define AFBCDM_IF0_MIF_VER_SCOPE                       0x564e
#define AFBCDM_IF0_PIXEL_HOR_SCOPE                     0x564f
#define AFBCDM_IF0_PIXEL_VER_SCOPE                     0x5650
#define AFBCDM_IF0_VD_CFMT_H                           0x5651
#define AFBCDM_IF0_IQUANT_ENABLE                       0x5652
#define AFBCDM_IF0_IQUANT_LUT_1                        0x5653
#define AFBCDM_IF0_IQUANT_LUT_2                        0x5654
#define AFBCDM_IF0_IQUANT_LUT_3                        0x5655
#define AFBCDM_IF0_IQUANT_LUT_4                        0x5656
#define AFBCDM_IF0_ROT_CTRL                            0x5660
#define AFBCDM_IF0_ROT_SCOPE                           0x5661
#define AFBCDM_IF0_RPLC_CTRL                           0x5662
#define AFBCDM_IF0_RPLC_PICEN                          0x5663
#define AFBCDM_IF0_RPLC_DEFCOL                         0x5664
#define AFBCDM_IF0_RPLC_SCPXN_ADDR                     0x5665
#define AFBCDM_IF0_RPLC_SCPXN_DATA                     0x5666
#define AFBCDM_IF0_ROT_RO_STAT                         0x5667
#define AFBCDM_IF0_FGRAIN_CTRL                         0x5670
#define AFBCDM_IF0_FGRAIN_WIN_H                        0x5671
#define AFBCDM_IF0_FGRAIN_WIN_V                        0x5672
#define AFBCDM_IF0_FGRAIN_SW_Y_RANNGE                  0x5673
#define AFBCDM_IF0_FGRAIN_SW_C_RANNGE                  0x5674
#define AFBCDM_IF0_FGRAIN_GCLK_CTRL_0                  0x5675
#define AFBCDM_IF0_FGRAIN_GCLK_CTRL_1                  0x5676
#define AFBCDM_IF0_FGRAIN_GCLK_CTRL_2                  0x5677
#define AFBCDM_IF0_FGRAIN_PARAM_ADDR                   0x5678
#define AFBCDM_IF0_FGRAIN_PARAM_DATA                   0x5679

//for t7:
#define AFBCDM_IF2_CTRL0                         0x56b8
//Bit  31:22       reserved              //
//Bit  21:16       reg_afbc_gclk_ctrl    // unsigned, RW, default = 0
//Bit  15          reg_frm_start_sel     // unsigned, RW, default = 0
//Bit  14          reg_use_4kram         // unsigned, RW, default = 0
//Bit  13          reg_afbc_vd_sel       // unsigned, RW, default = 0, 0:nor_rdmif 1:afbc_dec
//Bit  12          reg_rdmif_lbuf_bypas  // unsigned, RW, default = 1, 1:rdmif lbuf bypass
//Bit  11:0        reg_rdmif_lbuf_depth  // unsigned, RW, default = 512

#define AFBCDM_IF2_ENABLE                              0x56c0
#define AFBCDM_IF2_MODE                                0x56c1
#define AFBCDM_IF2_SIZE_IN                             0x56c2
#define AFBCDM_IF2_DEC_DEF_COLOR                       0x56c3
#define AFBCDM_IF2_CONV_CTRL                           0x56c4
#define AFBCDM_IF2_LBUF_DEPTH                          0x56c5
#define AFBCDM_IF2_HEAD_BADDR                          0x56c6
#define AFBCDM_IF2_BODY_BADDR                          0x56c7
#define AFBCDM_IF2_SIZE_OUT                            0x56c8
#define AFBCDM_IF2_OUT_YSCOPE                          0x56c9
#define AFBCDM_IF2_STAT                                0x56ca
#define AFBCDM_IF2_VD_CFMT_CTRL                        0x56cb
#define AFBCDM_IF2_VD_CFMT_W                           0x56cc
#define AFBCDM_IF2_MIF_HOR_SCOPE                       0x56cd
#define AFBCDM_IF2_MIF_VER_SCOPE                       0x56ce
#define AFBCDM_IF2_PIXEL_HOR_SCOPE                     0x56cf
#define AFBCDM_IF2_PIXEL_VER_SCOPE                     0x56d0
#define AFBCDM_IF2_VD_CFMT_H                           0x56d1
#define AFBCDM_IF2_IQUANT_ENABLE                       0x56d2
#define AFBCDM_IF2_IQUANT_LUT_1                        0x56d3
#define AFBCDM_IF2_IQUANT_LUT_2                        0x56d4
#define AFBCDM_IF2_IQUANT_LUT_3                        0x56d5
#define AFBCDM_IF2_IQUANT_LUT_4                        0x56d6
#define AFBCDM_IF2_ROT_CTRL                            0x56e0
#define AFBCDM_IF2_ROT_SCOPE                           0x56e1
#define AFBCDM_IF2_RPLC_CTRL                           0x56e2
#define AFBCDM_IF2_RPLC_PICEN                          0x56e3
#define AFBCDM_IF2_RPLC_DEFCOL                         0x56e4
#define AFBCDM_IF2_RPLC_SCPXN_ADDR                     0x56e5
#define AFBCDM_IF2_RPLC_SCPXN_DATA                     0x56e6
#define AFBCDM_IF2_ROT_RO_STAT                         0x56e7
#define AFBCDM_IF2_FGRAIN_CTRL                         0x56f0
#define AFBCDM_IF2_FGRAIN_WIN_H                        0x56f1
#define AFBCDM_IF2_FGRAIN_WIN_V                        0x56f2
#define AFBCDM_IF2_FGRAIN_SW_Y_RANNGE                  0x56f3
#define AFBCDM_IF2_FGRAIN_SW_C_RANNGE                  0x56f4
#define AFBCDM_IF2_FGRAIN_GCLK_CTRL_0                  0x56f5
#define AFBCDM_IF2_FGRAIN_GCLK_CTRL_1                  0x56f6
#define AFBCDM_IF2_FGRAIN_GCLK_CTRL_2                  0x56f7
#define AFBCDM_IF2_FGRAIN_PARAM_ADDR                   0x56f8
#define AFBCDM_IF2_FGRAIN_PARAM_DATA                   0x56f9
/* afbce */
#ifdef DEF_AFBCE
#define DI_AFBCE_ENABLE                            0x2060
//Bit   31:20,    gclk_ctrl        unsigned  , default = 0,
//Bit   19:16,    di_afbce_sync_sel   unsigned  , default = 0,
//Bit   15:14,    reserved
//Bit   13,       enc_rst_mode     unsigned  , default = 0,
//Bit   12,       enc_en_mode      unsigned  , default = 0,
//Bit   11:9,     reserved
//Bit   8,        enc_enable       unsigned  , default = 0,
//Bit   7:1,      reserved
//Bit   0,        reserved         enc_frm_start pulse use this bit don't use
#define DI_AFBCE_MODE                              0x2061
//Bit   31:29,    soft_rst         unsigned, default = 0 ,the use as go_field
//Bit   28,       reserved
//Bit   27:26,    rev_mode         unsigned, default = 0 , reverse mode
//Bit   25:24,    mif_urgent
//unsigned, default = 3 , info mif and data mif urgent
//Bit   23,       reserved
//Bit   22:16,    hold_line_num
//unsigned, default = 4, 0: burst1 1:burst2 2:burst4
//Bit   15:14,    burst_mode
//unsigned, default = 1, 0: burst1 1:burst2 2:burst4
//Bit   13:1,     reserved
//Bit      0,     reg_fmt444_comb
//unsigned, default = 0, 0: 444 8bit uncomb
#define DI_AFBCE_SIZE_IN                           0x2062
//Bit   31:29,    reserved
//Bit   28:16     hsize_in
//unsigned, default = 1920 , pic horz size in  unit: pixel
//Bit   15:13,    reserved
//Bit   12:0,     vsize_in
//unsigned, default = 1080 , pic vert size in  unit: pixel
#define DI_AFBCE_BLK_SIZE_IN                       0x2063
//Bit   31:29,    reserved
//Bit   28:16     hblk_size
//unsigned, default = 60 , pic horz size in  unit: pixel
//Bit   15:13,    reserved
//Bit   12:0,     vblk_size
//unsigned, default = 270, pic vert size in  unit: pixel
#define DI_AFBCE_HEAD_BADDR                        0x2064
//Bit   31:0,     head_baddr         unsigned, default = 32'h00;
#define DI_AFBCE_MIF_SIZE                          0x2065
//Bit   31:30,  reserved
//Bit   29:28,  ddr_blk_size       unsigned, default = 1;
//Bit   27,     reserved
//Bit   26:24,  cmd_blk_size       unsigned, default = 3;
//Bit   23:21,  reserved
//Bit   20:16,  uncmp_size         unsigned, default = 20;
//Bit   15:0,   mmu_page_size      unsigned, default = 4096;
#define DI_AFBCE_PIXEL_IN_HOR_SCOPE                0x2066
//Bit   31:29,   reserved
//Bit   28:16,   enc_win_end_h     unsigned, default = 1919 ; //
//Bit   15:13,   reserved
//Bit   12:0,    enc_win_bgn_h     unsigned, default = 0    ; //
#define DI_AFBCE_PIXEL_IN_VER_SCOPE                0x2067
//Bit   31:29,   reserved
//Bit   28:16,   enc_win_end_v     unsigned, default = 1079 ; //
//Bit   15:13,   reserved
//Bit   12:0,    enc_win_bgn_v     unsigned, default = 0    ; //
#define DI_AFBCE_CONV_CTRL                         0x2068
//Bit   31:29,   reserved
//Bit   28:16,   fmt_ybuf_depth    unsigned, default = 2048
//Bit   15:12,   reserved
//Bit   11: 0,   lbuf_depth
//unsigned, default = 256, unit=16 pixel need to set = 2^n
#define DI_AFBCE_MIF_HOR_SCOPE                     0x2069
//Bit   31:26,   reserved
//Bit   25:16,   blk_end_h         unsigned, default = 0    ; //
//Bit   15:10,   reserved
//Bit   9:0,     blk_bgn_h         unsigned, default = 59    ; //
#define DI_AFBCE_MIF_VER_SCOPE                     0x206a
//Bit   31:28,   reserved
//Bit   27:16,   blk_end_v         unsigned, default = 0    ; //
//Bit   15:12,   reserved
//Bit   11:0,    blk_bgn_v         unsigned, default = 269    ; //
#define DI_AFBCE_STAT1                             0x206b
//Bit   31,     ro_frm_end_pulse1   unsigned, RO,default = 0  ;frame end status
//Bit   30:0,   ro_dbg_top_info1    unsigned, RO,default = 0  ;
#define DI_AFBCE_STAT2                             0x206c
//Bit   31,     reserved
//Bit   30:0,   ro_dbg_top_info2    unsigned, RO,default = 0  ;
#define DI_AFBCE_FORMAT                            0x206d
//Bit 31:12        reserved
//Bit 11:10        reserved

//Bit  9: 8        reg_format_mode
// unsigned ,    RW, default = 2  data format;0 :YUV444,1:YUV422,2:YUV420,3:RGB
//Bit  7: 4        reg_compbits_c
// unsigned ,    RW, default = 10  chroma bitwidth
//Bit  3: 0        reg_compbits_y
// unsigned ,    RW, default = 10  luma bitwidth
#define DI_AFBCE_MODE_EN                           0x206e
//Bit 31:28        reserved
//Bit 27:26        reserved
//Bit 25           reg_adpt_interleave_ymode
// unsigned ,    RW, default = 0  force 0 to disable it: no  HW implementation
//Bit 24           reg_adpt_interleave_cmode
// unsigned ,    RW, default = 0  force 0 to disable it: not HW implementation
//Bit 23           reg_adpt_yinterleave_luma_ride
// unsigned ,    RW, default = 1  vertical interleave piece luma reorder ride;
//   0: no reorder ride; 1: w/4 as ride
//Bit 22           reg_adpt_yinterleave_chrm_ride
// unsigned ,    RW, default = 1  vertical interleave piece chroma reorder ride;
//0: no reorder ride; 1: w/2 as ride
//Bit 21           reg_adpt_xinterleave_luma_ride
// unsigned ,    RW, default = 1  vertical interleave piece luma reorder ride;
//0: no reorder ride; 1: w/4 as ride
//Bit 20           reg_adpt_xinterleave_chrm_ride
// unsigned ,    RW, default = 1  vertical interleave piece chroma reorder ride;
//0: no reorder ride; 1: w/2 as ride
//Bit 19            reserved
//Bit 18           reg_disable_order_mode_i_6
// unsigned ,    RW, default = 0  disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 17           reg_disable_order_mode_i_5
// unsigned ,    RW, default = 0  disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 16           reg_disable_order_mode_i_4
// unsigned ,    RW, default = 0  disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 15           reg_disable_order_mode_i_3
// unsigned ,    RW, default = 0  disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 14           reg_disable_order_mode_i_2
// unsigned ,    RW, default = 0  disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 13           reg_disable_order_mode_i_1
// unsigned ,    RW, default = 0  disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 12           reg_disable_order_mode_i_0
// unsigned ,    RW, default = 0  disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 11            reserved
//Bit 10           reg_minval_yenc_en
// unsigned ,    RW, default = 0
//force disable, final decision to remove this ws 1% performance loss
//Bit  9           reg_16x4block_enable
// unsigned ,    RW, default = 0  block as mission, but permit 16x4 block
//Bit  8           reg_uncompress_split_mode
// unsigned ,    RW, default = 0  0: no split; 1: split
//Bit  7: 6        reserved
//Bit  5           reg_input_padding_uv128
// unsigned ,    RW, default = 0
//input picture 32x4 block gap mode: 0:  pad uv=0; 1: pad uv=128
//Bit  4           reg_dwds_padding_uv128
// unsigned ,    RW, default = 0
//downsampled image for double write 32x gap mode:
// 0:  pad uv=0; 1: pad uv=128
//Bit  3: 1        reg_force_order_mode_value
// unsigned ,    RW, default = 0  force order mode 0~7
//Bit  0           reg_force_order_mode_en
// unsigned ,    RW, default = 0
//force order mode enable:
//0: no force; 1: forced to force_value
#define DI_AFBCE_DWSCALAR                          0x206f
//Bit 31: 8        reserved
//Bit  7: 6        reg_dwscalar_w0
// unsigned ,    RW, default = 3  horizontal 1st step scalar mode:
//0: 1:1 no scalar;
//1: 2:1 data drop (0,2,4, 6) pixel kept;
//2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg
//Bit  5: 4        reg_dwscalar_w1
// unsigned ,    RW, default = 0  horizontal 2nd step scalar mode:
//0: 1:1 no scalar;
//1: 2:1 data drop (0,2,4, 6) pixel kept;
//2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg
//Bit  3: 2        reg_dwscalar_h0
// unsigned ,    RW, default = 2  vertical 1st step scalar mode:
//0: 1:1 no scalar;
//1: 2:1 data drop (0,2,4, 6) pixel kept;
//2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg
//Bit  1: 0        reg_dwscalar_h1
// unsigned ,
//RW, default = 3  vertical 2nd step scalar mode:
//0: 1:1 no scalar;
//1: 2:1 data drop (0,2,4, 6) pixel kept;
//2: 2:1 data drop (1, 3, 5,7..) //pixels kept; 3: avg
#define DI_AFBCE_DEFCOLOR_1                        0x2070
//Bit 31:24        reserved
//Bit 23:12        reg_enc_default_color_3
// unsigned ,
//RW, default = 4095  Picture wise default color value in [YCbCr]
//Bit 11: 0        reg_enc_default_color_0
// unsigned ,
//RW, default = 4095  Picture wise default color value in [YCbCr]
#define DI_AFBCE_DEFCOLOR_2                        0x2071
//Bit 31:24        reserved
//Bit 23:12        reg_enc_default_color_2
// unsigned ,
//RW, default = 2048  wise default color value in [Y Cb Cr]
//Bit 11: 0        reg_enc_default_color_1
// unsigned ,
//RW, default = 2048  wise default color value in [Y Cb Cr]
#define DI_AFBCE_QUANT_ENABLE                      0x2072
//Bit 31:12        reserved
//Bit 11           reg_quant_expand_en_1
// unsigned ,    //RW, default = 0  enable for quantization value expansion
//Bit 10           reg_quant_expand_en_0
// unsigned ,    //RW, default = 0  enable for quantization value expansion
//Bit  9: 8        reg_bcleav_ofst
// signed ,    RW, default = 0  bcleave ofset to get lower range,
// especially under lossy, for v1/v2, x=0 is equivalent, default = -1;
//Bit  7: 5        reserved
//Bit  4           reg_quant_enable_1
// unsigned ,    RW, default = 0  enable for quant to get some lossy
//Bit  3: 1        reserved
//Bit  0           reg_quant_enable_0
// unsigned ,    RW, default = 0  enable for quant to get some lossy
#define DI_AFBCE_IQUANT_LUT_1                      0x2073
//Bit 31            reserved
//Bit 30:28        reg_iquant_yclut_0_11
// unsigned ,    RW, default = 0  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 27            reserved
//Bit 26:24        reg_iquant_yclut_0_10
// unsigned ,    RW, default = 1  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 23            reserved
//Bit 22:20        reg_iquant_yclut_0_9
// unsigned ,    RW, default = 2  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 19            reserved
//Bit 18:16        reg_iquant_yclut_0_8
// unsigned ,    RW, default = 3  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 15            reserved
//Bit 14:12        reg_iquant_yclut_0_7
// unsigned ,    RW, default = 4  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 11            reserved
//Bit 10: 8        reg_iquant_yclut_0_6
// unsigned ,    RW, default = 5  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit  7            reserved
//Bit  6: 4        reg_iquant_yclut_0_5
// unsigned ,    RW, default = 5  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit  3            reserved
//Bit  2: 0        reg_iquant_yclut_0_4
// unsigned ,    RW, default = 4  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
#define DI_AFBCE_IQUANT_LUT_2                      0x2074
//Bit 31:16        reserved
//Bit 15            reserved
//Bit 14:12        reg_iquant_yclut_0_3
// unsigned ,    RW, default = 3  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 11            reserved
//Bit 10: 8        reg_iquant_yclut_0_2
// unsigned ,    RW, default = 2  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit  7            reserved
//Bit  6: 4        reg_iquant_yclut_0_1
// unsigned ,    RW, default = 1  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit  3            reserved
//Bit  2: 0        reg_iquant_yclut_0_0
// unsigned ,    RW, default = 0  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
#define DI_AFBCE_IQUANT_LUT_3                      0x2075
//Bit 31            reserved
//Bit 30:28        reg_iquant_yclut_1_11
// unsigned ,    RW, default = 0  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 27            reserved
//Bit 26:24        reg_iquant_yclut_1_10
// unsigned ,    RW, default = 1  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 23            reserved
//Bit 22:20        reg_iquant_yclut_1_9
// unsigned ,    RW, default = 2  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 19            reserved
//Bit 18:16        reg_iquant_yclut_1_8
// unsigned ,    RW, default = 3  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 15            reserved
//Bit 14:12        reg_iquant_yclut_1_7
// unsigned ,    RW, default = 4  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 11            reserved
//Bit 10: 8        reg_iquant_yclut_1_6
// unsigned ,    RW, default = 5  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit  7            reserved
//Bit  6: 4        reg_iquant_yclut_1_5
// unsigned ,    RW, default = 5  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit  3            reserved
//Bit  2: 0        reg_iquant_yclut_1_4
// unsigned ,    RW, default = 4  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
#define DI_AFBCE_IQUANT_LUT_4                      0x2076
//Bit 31:16        reserved
//Bit 15            reserved
//Bit 14:12        reg_iquant_yclut_1_3
// unsigned ,    RW, default = 3  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit 11            reserved
//Bit 10: 8        reg_iquant_yclut_1_2
// unsigned ,    RW, default = 2  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit  7            reserved
//Bit  6: 4        reg_iquant_yclut_1_1
// unsigned ,    RW, default = 1  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
//Bit  3            reserved
//Bit  2: 0        reg_iquant_yclut_1_0
// unsigned ,    RW, default = 0  quantization lut for mintree leavs,
//iquant=2^lut(bc_leav_q+1)
#define DI_AFBCE_RQUANT_LUT_1                      0x2077
//Bit 31            reserved
//Bit 30:28        reg_rquant_yclut_0_11
// unsigned ,    RW, default = 5  quantization lut for bctree leavs,
//quant=2^lut(bc_leav_r+1),
//can be calculated from iquant_yclut(fw_setting)
//Bit 27            reserved
//Bit 26:24        reg_rquant_yclut_0_10
// unsigned ,    RW, default = 5
//Bit 23            reserved
//Bit 22:20        reg_rquant_yclut_0_9
// unsigned ,    RW, default = 4
//Bit 19            reserved
//Bit 18:16        reg_rquant_yclut_0_8
// unsigned ,    RW, default = 4
//Bit 15            reserved
//Bit 14:12        reg_rquant_yclut_0_7
// unsigned ,    RW, default = 3
//Bit 11            reserved
//Bit 10: 8        reg_rquant_yclut_0_6
// unsigned ,    RW, default = 3
//Bit  7            reserved
//Bit  6: 4        reg_rquant_yclut_0_5
// unsigned ,    RW, default = 2
//Bit  3            reserved
//Bit  2: 0        reg_rquant_yclut_0_4
// unsigned ,    RW, default = 2
#define DI_AFBCE_RQUANT_LUT_2                      0x2078
//Bit 31:16        reserved
//Bit 15            reserved
//Bit 14:12        reg_rquant_yclut_0_3
// unsigned ,    RW, default = 1
//Bit 11            reserved
//Bit 10: 8        reg_rquant_yclut_0_2
// unsigned ,    RW, default = 1
//Bit  7            reserved
//Bit  6: 4        reg_rquant_yclut_0_1
// unsigned ,    RW, default = 0
//Bit  3            reserved
//Bit  2: 0        reg_rquant_yclut_0_0
// unsigned ,    RW, default = 0
#define DI_AFBCE_RQUANT_LUT_3                      0x2079
//Bit 31            reserved
//Bit 30:28        reg_rquant_yclut_1_11
// unsigned ,    RW, default = 5
//quantization lut for bctree leavs, quant=2^lut(bc_leav_r+1),
//can be calculated from iquant_yclut(fw_setting)
//Bit 27            reserved
//Bit 26:24        reg_rquant_yclut_1_10
// unsigned ,    RW, default = 5
//Bit 23            reserved
//Bit 22:20        reg_rquant_yclut_1_9
// unsigned ,    RW, default = 4
//Bit 19            reserved
//Bit 18:16        reg_rquant_yclut_1_8
// unsigned ,    RW, default = 4
//Bit 15            reserved
//Bit 14:12        reg_rquant_yclut_1_7
// unsigned ,    RW, default = 3
//Bit 11            reserved
//Bit 10: 8        reg_rquant_yclut_1_6
// unsigned ,    RW, default = 3
//Bit  7            reserved
//Bit  6: 4        reg_rquant_yclut_1_5
// unsigned ,    RW, default = 2
//Bit  3            reserved
//Bit  2: 0        reg_rquant_yclut_1_4
// unsigned ,    RW, default = 2
#define DI_AFBCE_RQUANT_LUT_4                      0x207a
//Bit 31:16        reserved
//Bit 15            reserved
//Bit 14:12        reg_rquant_yclut_1_3
// unsigned ,    RW, default = 1
//Bit 11            reserved
//Bit 10: 8        reg_rquant_yclut_1_2
// unsigned ,    RW, default = 1
//Bit  7            reserved
//Bit  6: 4        reg_rquant_yclut_1_1
// unsigned ,    RW, default = 0
//Bit  3            reserved
//Bit  2: 0        reg_rquant_yclut_1_0
// unsigned ,    RW, default = 0
#define DI_AFBCE_YUV_FORMAT_CONV_MODE              0x207b
//Bit 31: 8        reserved
//Bit  7           reserved
//Bit  6: 4        reg_444to422_mode
// unsigned ,    RW, default = 0
//Bit  3           reserved
//Bit  2: 0        reg_422to420_mode
// unsigned ,    RW, default = 0
#define DI_AFBCE_DUMMY_DATA                        0x207c
//Bit 31:30        reserved
//Bit 29: 0        reg_dummy_data
// unsigned ,  default = 0  ;
#define DI_AFBCE_CLR_FLAG                          0x207d
//Bit 31:0         reg_di_afbce_clr_flag
// unsigned, default = 0  ;
#define DI_AFBCE_STA_FLAGT                         0x207e
//Bit 31:0         ro_di_afbce__sta_flag
// unsigned, RO,default = 0  ;
#define DI_AFBCE_MMU_NUM                           0x207f
//Bit 31:16        reserved
//Bit 15: 0        ro_frm_mmu_num
// unsigned, RO,default = 0  ;
#define DI_AFBCE_MMU_RMIF_CTRL1                    0x2080
//Bit 31:26 reserved
//Bit 25:24 reg_sync_sel
// unsigned , default = 0, axi canvas id sync with frm rst
//Bit 23:16 reg_canvas_id
// unsigned , default = 0, axi canvas id num
//Bit 15    reserved
//Bit 14:12 reg_cmd_intr_len
// unsigned , default = 1,
//interrupt send cmd when how many series axi cmd,
// 0=12 1=16 2=24 3=32 4=40 5=48 6=56 7=64
//Bit 11:10 reg_cmd_req_size
// unsigned , default = 1,
// how many room fifo have, then axi send series req, 0=16 1=32 2=24 3=64
//Bit 9:8   reg_burst_len
// unsigned , default = 2, burst type: 0-single 1-bst2 2-bst4
//Bit 7     reg_swap_64bit
// unsigned , default = 0, 64bits of 128bit swap enable
//Bit 6     reg_little_endian
// unsigned , default = 0, big endian enable
//Bit 5     reg_y_rev
// unsigned , default = 0, vertical reverse enable
//Bit 4     reg_x_rev
// unsigned , default = 0, horizontal reverse enable
//Bit 3     reserved
//Bit 2:0   reg_pack_mode
// unsigned , default = 3, 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit
#define DI_AFBCE_MMU_RMIF_CTRL2                    0x2081
//Bit 31:30 reg_sw_rst
// unsigned , default = 0,
//Bit 29:24 reserved
//Bit 23:18 reg_gclk_ctrl
//Bit 17    reserved
//Bit 16:0  reg_urgent_ctrl
// unsigned , default = 0, urgent control reg :
//  16  reg_ugt_init  :  urgent initial value
//  15  reg_ugt_en    :  urgent enable
//  14  reg_ugt_type  :  1= wrmif 0=rdmif
// 7:4  reg_ugt_top_th:  urgent top threshold
// 3:0  reg_ugt_bot_th:  urgent bottom threshold
#define DI_AFBCE_MMU_RMIF_CTRL3                    0x2082
//Bit 31:17 reserved
//Bit 16    reg_acc_mode      // unsigned , default = 1,
//Bit 15:13 reserved
//Bit 12:0  reg_stride        // unsigned , default = 4096,
#define DI_AFBCE_MMU_RMIF_CTRL4                    0x2083
//Bit 31:0  reg_baddr        // unsigned , default = 0,
#define DI_AFBCE_MMU_RMIF_SCOPE_X                  0x2084
//Bit 31:29 reserved
//Bit 28:16 reg_x_end
// unsigned , default = 4095, the canvas hor end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_x_start
// unsigned , default = 0, the canvas hor start pixel position
#define DI_AFBCE_MMU_RMIF_SCOPE_Y                  0x2085
//Bit 31:29 reserved
//Bit 28:16 reg_y_end
// unsigned , default = 0, the canvas ver end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_y_start
// unsigned , default = 0, the canvas ver start pixel position
#define DI_AFBCE_MMU_RMIF_RO_STAT                  0x2086
//Bit 15:0  reg_status        // unsigned ,

#endif
#define DI_AFBCE_PIP_CTRL			0x208a
//Bit   31:3      reserved
//Bit   2         reg_enc_align_en
//unsigned  , RW,default = 1,
//Bit   1         reg_pip_ini_ctrl
//unsigned  , RW,default = 0,
//Bit   0         reg_pip_mode
//unsigned  , RW,default = 0,
#define DI_AFBCE_ROT_CTRL                          0x208b
//Bit   31:5      reserved
//Bit   4         reg_rot_en
//unsigned  , RW,default = 0, rotation enable
//Bit   3:0       reg_vstep
//unsigned  , RW,default = 8,
//rotation vstep ,setting acorrding rotation shrink mode
#define DI_AFBCE_DIMM_CTRL                         0x208c
//Bit   31        reg_dimm_layer_en
//unsigned  , RW,default = 0,dimm_layer enable signal
//Bit   30        reserved
//Bit   29:0      reg_dimm_data
//unsigned  , RW,default = 29'h00080200,dimm_layer data
#define DI_AFBCE_BND_DEC_MISC                      0x208d
//Bit 31:28  reserved
//Bit 27:26  bnd_dec_rev_mode
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 25:24  bnd_dec_mif_urgent
//unsigned , RW,default = 3
//only pip mode use those bits,usually don't need configure
//Bit 23:22  bnd_dec_burst_len
//unsigned , RW,default = 2
//only pip mode use those bits,usually don't need configure
//Bit 21:20  bnd_dec_ddr_blk_size
//unsigned , RW,default = 1
//only pip mode use those bits,usually don't need configure
//Bit 19     reserved
//Bit 18:16  bnd_dec_cmd_blk_size
//unsigned , RW,default = 3
//only pip mode use those bits,usually don't need configure
//Bit 15     reserved
//Bit 14     bnd_dec_blk_mem_mode
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 13     bnd_dec_addr_link_en
//unsigned , RW,default = 1
//only pip mode use those bits,usually don't need configure
//Bit 12     bnd_dec_always_body_rden
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 11:0   bnd_dec_mif_lbuf_depth
//unsigned , RW,default = 128
//only pip mode use those bits,usually don't need configure
#define DI_AFBCE_RD_ARB_MISC                       0x208e
//Bit 31:13  reserved
//Bit 12     reg_arb_sw_rst
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 11:10  reserved
//Bit 9      reg_arb_arblk_last1
//unsigned , RW,default = 0
// only pip mode use those bits,usually don't need configure
//Bit 8      reg_arb_arblk_last0
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 7:4    reg_arb_weight_ch1
//unsigned , RW,default = 4
//only pip mode use those bits,usually don't need configure
//Bit 3:0    reg_arb_weight_ch0
//unsigned , RW,default = 10
//only pip mode use those bits,usually don't need configure
/* afbce */

#define DI_AFBCE1_ENABLE                           0x20c0
//Bit   31:20,    gclk_ctrl        unsigned  , default = 0,
//Bit   19:16,    di_AFBCE1_sync_sel   unsigned  , default = 0,
//Bit   15:14,    reserved
//Bit   13,       enc_rst_mode     unsigned  , default = 0,
//Bit   12,       enc_en_mode      unsigned  , default = 0,
//Bit   11:9,     reserved
//Bit   8,        enc_enable       unsigned  , default = 0,
//Bit   7:1,      reserved

//Bit   0,
//reserved         enc_frm_start pulse use this bit don't use
#define DI_AFBCE1_MODE                             0x20c1
//Bit   31:29,    soft_rst
//unsigned, default = 0 ,the use as go_field
//Bit   28,       reserved
//unsigned, default = 0 , enable signal of crop
//Bit   27:26,    rev_mode
//unsigned, default = 0 , reverse mode
//Bit   25:24,    mif_urgent
//unsigned, default = 3 , info mif and data mif urgent
//Bit   23,       reserved
//Bit   22:16,    hold_line_num
//unsigned, default = 4, 0: burst1 1:burst2 2:burst4
//Bit   15:14,    burst_mode
//unsigned, default = 1, 0: burst1 1:burst2 2:burst4
//Bit   13:1,     reserved
//Bit      0,     reg_fmt444_comb
//unsigned, default = 0, 0: 444 8bit uncomb
#define DI_AFBCE1_SIZE_IN                          0x20c2
//Bit   31:29,    reserved
//Bit   28:16     hsize_in
//unsigned, default = 1920 , pic horz size in  unit: pixel
//Bit   15:13,    reserved
//Bit   12:0,     vsize_in
//unsigned, default = 1080 , pic vert size in  unit: pixel
#define DI_AFBCE1_BLK_SIZE_IN                      0x20c3
//Bit   31:29,    reserved
//Bit   28:16     hblk_size
//unsigned, default = 60 , pic horz size in  unit: pixel
//Bit   15:13,    reserved
//Bit   12:0,     vblk_size
//unsigned, default = 270, pic vert size in  unit: pixel
#define DI_AFBCE1_HEAD_BADDR                       0x20c4
//Bit   31:0,     head_baddr         unsigned, default = 32'h00;
#define DI_AFBCE1_MIF_SIZE                         0x20c5
//Bit   31:30,  reserved
//Bit   29:28,  ddr_blk_size       unsigned, default = 1;
//Bit   27,     reserved
//Bit   26:24,  cmd_blk_size       unsigned, default = 3;
//Bit   23:21,  reserved
//Bit   20:16,  uncmp_size         unsigned, default = 20;
//Bit   15:0,   mmu_page_size      unsigned, default = 4096;
#define DI_AFBCE1_PIXEL_IN_HOR_SCOPE               0x20c6
//Bit   31:29,   reserved
//Bit   28:16,   enc_win_end_h     unsigned, default = 1919 ; //
//Bit   15:13,   reserved
//Bit   12:0,    enc_win_bgn_h     unsigned, default = 0    ; //
#define DI_AFBCE1_PIXEL_IN_VER_SCOPE               0x20c7
//Bit   31:29,   reserved
//Bit   28:16,   enc_win_end_v     unsigned, default = 1079 ; //
//Bit   15:13,   reserved
//Bit   12:0,    enc_win_bgn_v     unsigned, default = 0    ; //
#define DI_AFBCE1_CONV_CTRL                        0x20c8
//Bit   31:29,   reserved
//Bit   28:16,   fmt_ybuf_depth    unsigned, default = 2048
//Bit   15:12,   reserved
//Bit   11: 0,   lbuf_depth
//unsigned, default = 256, unit=16 pixel need to set = 2^n
#define DI_AFBCE1_MIF_HOR_SCOPE                    0x20c9
//Bit   31:26,   reserved
//Bit   25:16,   blk_end_h         unsigned, default = 0    ; //
//Bit   15:10,   reserved
//Bit   9:0,     blk_bgn_h         unsigned, default = 59    ; //
#define DI_AFBCE1_MIF_VER_SCOPE                    0x20ca
//Bit   31:28,   reserved
//Bit   27:16,   blk_end_v         unsigned, default = 0    ; //
//Bit   15:12,   reserved
//Bit   11:0,    blk_bgn_v         unsigned, default = 269    ; //
#define DI_AFBCE1_STAT1                            0x20cb
//Bit   31,     ro_frm_end_pulse1   unsigned, RO,default = 0  ;frame end status
//Bit   30:0,   ro_dbg_top_info1    unsigned, RO,default = 0  ;
#define DI_AFBCE1_STAT2                            0x20cc
//Bit   31,     reserved
//Bit   30:0,   ro_dbg_top_info2    unsigned, RO,default = 0  ;
#define DI_AFBCE1_FORMAT                           0x20cd
//Bit 31:12        reserved
//Bit 11:10        reserved
//Bit  9: 8        reg_format_mode
// unsigned ,    RW, default = 2
//data format;0 : YUV444, 1:YUV422, 2:YUV420, 3:RGB
//Bit  7: 4        reg_compbits_c
// unsigned ,    RW, default = 10  chroma bitwidth
//Bit  3: 0        reg_compbits_y
// unsigned ,    RW, default = 10  luma bitwidth
#define DI_AFBCE1_MODE_EN                          0x20ce
//Bit 31:28        reserved
//Bit 27:26        reserved
//Bit 25           reg_adpt_interleave_ymode
// unsigned ,    RW, default = 0  force 0 to disable it
//: no  HW implementation
//Bit 24           reg_adpt_interleave_cmode
// unsigned ,    RW, default = 0  force 0 to disable it
//: not HW implementation
//Bit 23           reg_adpt_yinterleave_luma_ride
// unsigned ,    RW, default = 1
//vertical interleave piece luma reorder ride;
//0: no reorder ride; 1: w/4 as ride
//Bit 22           reg_adpt_yinterleave_chrm_ride
// unsigned ,    RW, default = 1
//vertical interleave piece chroma reorder ride;
//0: no reorder ride; 1: w/2 as ride
//Bit 21           reg_adpt_xinterleave_luma_ride
// unsigned ,    RW, default = 1
//vertical interleave piece luma reorder ride;
//0: no reorder ride; 1: w/4 as ride
//Bit 20           reg_adpt_xinterleave_chrm_ride
// unsigned ,    RW, default = 1
//vertical interleave piece chroma reorder ride;
//0: no reorder ride; 1: w/2 as ride
//Bit 19            reserved
//Bit 18           reg_disable_order_mode_i_6
// unsigned ,    RW, default = 0
//disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 17           reg_disable_order_mode_i_5
// unsigned ,    RW, default = 0
//disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 16           reg_disable_order_mode_i_4
// unsigned ,    RW, default = 0
//disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 15           reg_disable_order_mode_i_3
// unsigned ,    RW, default = 0
//disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 14           reg_disable_order_mode_i_2
// unsigned ,    RW, default = 0
//disable order mode0~6: each mode with one
//disable bit: 0: no disable, 1: disable
//Bit 13           reg_disable_order_mode_i_1
// unsigned ,    RW, default = 0
//disable order mode0~6:
//each mode with one  disable bit: 0: no disable, 1: disable
//Bit 12           reg_disable_order_mode_i_0
// unsigned ,    RW, default = 0
//disable order mode0~6:
//each mode with one  disable bit: 0: no disable, 1: disable
//Bit 11            reserved
//Bit 10           reg_minval_yenc_en
// unsigned ,    RW, default = 0
//force disable, final decision to remove this ws 1% performance loss
//Bit  9           reg_16x4block_enable
// unsigned ,    RW, default = 0
//block as mission, but permit 16x4 block
//Bit  8           reg_uncompress_split_mode
// unsigned ,    RW, default = 0  0: no split; 1: split
//Bit  7: 6        reserved
//Bit  5           reg_input_padding_uv128
// unsigned ,    RW, default = 0
//input picture 32x4 block gap mode: 0:  pad uv=0; 1: pad uv=128
//Bit  4           reg_dwds_padding_uv128
// unsigned ,    RW, default = 0
//downsampled image for double write 32x gap mode: 0:  pad uv=0; 1: pad uv=128
//Bit  3: 1        reg_force_order_mode_value
// unsigned ,    RW, default = 0  force order mode 0~7
//Bit  0           reg_force_order_mode_en
// unsigned ,    RW, default = 0
// force order mode enable: 0: no force; 1: forced to force_value
#define DI_AFBCE1_DWSCALAR                         0x20cf
//Bit 31: 8        reserved
//Bit  7: 6        reg_dwscalar_w0
// unsigned ,    RW, default = 3  horizontal 1st step scalar mode:
// 0: 1:1 no scalar; 1: 2:1 data drop (0,2,4, 6) pixel kept;
// 2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg
//Bit  5: 4        reg_dwscalar_w1
// unsigned ,    RW, default = 0  horizontal 2nd step scalar mode:
// 0: 1:1 no scalar; 1: 2:1 data drop (0,2,4, 6) pixel kept;
// 2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg
//Bit  3: 2        reg_dwscalar_h0
// unsigned ,    RW, default = 2
//vertical 1st step scalar mode: 0: 1:1 no scalar;
// 1: 2:1 data drop (0,2,4, 6) pixel kept;
// 2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg
//Bit  1: 0        reg_dwscalar_h1
// unsigned ,    RW, default = 3
// vertical 2nd step scalar mode: 0: 1:1 no scalar;
// 1: 2:1 data drop (0,2,4, 6) pixel kept;
// 2: 2:1 data drop (1, 3, 5,7..) pixels kept; 3: avg
#define DI_AFBCE1_DEFCOLOR_1                       0x20d0
//Bit 31:24        reserved
//Bit 23:12        reg_enc_default_color_3
// unsigned ,    RW, default = 4095
//Picture wise default color value in [Y Cb Cr]
//Bit 11: 0        reg_enc_default_color_0
// unsigned ,    RW, default = 4095
//Picture wise default color value in [Y Cb Cr]
#define DI_AFBCE1_DEFCOLOR_2                       0x20d1
//Bit 31:24        reserved
//Bit 23:12        reg_enc_default_color_2
// unsigned ,    RW, default = 2048  wise default color value in [Y Cb Cr]
//Bit 11: 0        reg_enc_default_color_1
// unsigned ,    RW, default = 2048  wise default color value in [Y Cb Cr]
#define DI_AFBCE1_QUANT_ENABLE                     0x20d2
//Bit 31:12        reserved
//Bit 11           reg_quant_expand_en_1
// unsigned ,    RW, default = 0  enable for quantization value expansion
//Bit 10           reg_quant_expand_en_0
// unsigned ,    RW, default = 0  enable for quantization value expansion
//Bit  9: 8        reg_bcleav_ofst
//signed , RW, default = 0
//bcleave ofset to get lower range,
//especially under lossy, for v1/v2, x=0 is equivalent, default = -1;
//Bit  7: 5        reserved
//Bit  4           reg_quant_enable_1
// unsigned ,    RW, default = 0  enable for quant to get some lossy
//Bit  3: 1        reserved
//Bit  0           reg_quant_enable_0
// unsigned ,    RW, default = 0  enable for quant to get some lossy
#define DI_AFBCE1_IQUANT_LUT_1                     0x20d3
//Bit 31            reserved
//Bit 30:28        reg_iquant_yclut_0_11
// unsigned ,    RW, default = 0
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27            reserved
//Bit 26:24        reg_iquant_yclut_0_10
// unsigned ,    RW, default = 1
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23            reserved
//Bit 22:20        reg_iquant_yclut_0_9
// unsigned ,    RW, default = 2
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19            reserved
//Bit 18:16        reg_iquant_yclut_0_8
// unsigned ,    RW, default = 3
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15            reserved
//Bit 14:12        reg_iquant_yclut_0_7
// unsigned ,    RW, default = 4
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11            reserved
//Bit 10: 8        reg_iquant_yclut_0_6
// unsigned ,    RW, default = 5
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7            reserved
//Bit  6: 4        reg_iquant_yclut_0_5
// unsigned ,    RW, default = 5
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3            reserved
//Bit  2: 0        reg_iquant_yclut_0_4
// unsigned ,    RW, default = 4
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define DI_AFBCE1_IQUANT_LUT_2                     0x20d4
//Bit 31:16        reserved
//Bit 15            reserved
//Bit 14:12        reg_iquant_yclut_0_3
// unsigned ,    RW, default = 3
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11            reserved
//Bit 10: 8        reg_iquant_yclut_0_2
// unsigned ,    RW, default = 2
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7            reserved
//Bit  6: 4        reg_iquant_yclut_0_1
// unsigned ,    RW, default = 1
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3            reserved
//Bit  2: 0        reg_iquant_yclut_0_0
// unsigned ,    RW, default = 0
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define DI_AFBCE1_IQUANT_LUT_3                     0x20d5
//Bit 31            reserved
//Bit 30:28        reg_iquant_yclut_1_11
// unsigned ,    RW, default = 0
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27            reserved
//Bit 26:24        reg_iquant_yclut_1_10
// unsigned ,    RW, default = 1
// quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23            reserved
//Bit 22:20        reg_iquant_yclut_1_9
// unsigned ,    RW, default = 2
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19            reserved
//Bit 18:16        reg_iquant_yclut_1_8
// unsigned ,    RW, default = 3
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15            reserved
//Bit 14:12        reg_iquant_yclut_1_7
// unsigned ,    RW, default = 4
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11            reserved
//Bit 10: 8        reg_iquant_yclut_1_6
// unsigned ,    RW, default = 5
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7            reserved
//Bit  6: 4        reg_iquant_yclut_1_5
// unsigned ,    RW, default = 5
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3            reserved
//Bit  2: 0        reg_iquant_yclut_1_4
// unsigned ,    RW, default = 4
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define DI_AFBCE1_IQUANT_LUT_4                     0x20d6
//Bit 31:16        reserved
//Bit 15            reserved
//Bit 14:12        reg_iquant_yclut_1_3
// unsigned ,    RW, default = 3
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11            reserved
//Bit 10: 8        reg_iquant_yclut_1_2
// unsigned ,    RW, default = 2
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7            reserved
//Bit  6: 4        reg_iquant_yclut_1_1
// unsigned ,    RW, default = 1
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3            reserved
//Bit  2: 0        reg_iquant_yclut_1_0
// unsigned ,    RW, default = 0
//quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define DI_AFBCE1_RQUANT_LUT_1                     0x20d7
//Bit 31            reserved
//Bit 30:28        reg_rquant_yclut_0_11     // unsigned ,    RW, default = 5
//quantization lut for bctree leavs,
//quant=2^lut(bc_leav_r+1), can be calculated from iquant_yclut(fw_setting)
//Bit 27            reserved
//Bit 26:24        reg_rquant_yclut_0_10     // unsigned ,    RW, default = 5
//Bit 23            reserved
//Bit 22:20        reg_rquant_yclut_0_9      // unsigned ,    RW, default = 4
//Bit 19            reserved
//Bit 18:16        reg_rquant_yclut_0_8      // unsigned ,    RW, default = 4
//Bit 15            reserved
//Bit 14:12        reg_rquant_yclut_0_7      // unsigned ,    RW, default = 3
//Bit 11            reserved
//Bit 10: 8        reg_rquant_yclut_0_6      // unsigned ,    RW, default = 3
//Bit  7            reserved
//Bit  6: 4        reg_rquant_yclut_0_5      // unsigned ,    RW, default = 2
//Bit  3            reserved
//Bit  2: 0        reg_rquant_yclut_0_4      // unsigned ,    RW, default = 2
#define DI_AFBCE1_RQUANT_LUT_2                     0x20d8
//Bit 31:16        reserved
//Bit 15            reserved
//Bit 14:12        reg_rquant_yclut_0_3      // unsigned ,    RW, default = 1
//Bit 11            reserved
//Bit 10: 8        reg_rquant_yclut_0_2      // unsigned ,    RW, default = 1
//Bit  7            reserved
//Bit  6: 4        reg_rquant_yclut_0_1      // unsigned ,    RW, default = 0
//Bit  3            reserved
//Bit  2: 0        reg_rquant_yclut_0_0      // unsigned ,    RW, default = 0
#define DI_AFBCE1_RQUANT_LUT_3                     0x20d9
//Bit 31            reserved
//Bit 30:28        reg_rquant_yclut_1_11     // unsigned ,    RW, default = 5
//quantization lut for bctree leavs, quant=2^lut(bc_leav_r+1),
// can be calculated from iquant_yclut(fw_setting)
//Bit 27            reserved
//Bit 26:24        reg_rquant_yclut_1_10     // unsigned ,    RW, default = 5
//Bit 23            reserved
//Bit 22:20        reg_rquant_yclut_1_9      // unsigned ,    RW, default = 4
//Bit 19            reserved
//Bit 18:16        reg_rquant_yclut_1_8      // unsigned ,    RW, default = 4
//Bit 15            reserved
//Bit 14:12        reg_rquant_yclut_1_7      // unsigned ,    RW, default = 3
//Bit 11            reserved
//Bit 10: 8        reg_rquant_yclut_1_6      // unsigned ,    RW, default = 3
//Bit  7            reserved
//Bit  6: 4        reg_rquant_yclut_1_5      // unsigned ,    RW, default = 2
//Bit  3            reserved
//Bit  2: 0        reg_rquant_yclut_1_4      // unsigned ,    RW, default = 2
#define DI_AFBCE1_RQUANT_LUT_4                     0x20da
//Bit 31:16        reserved
//Bit 15            reserved
//Bit 14:12        reg_rquant_yclut_1_3      // unsigned ,    RW, default = 1
//Bit 11            reserved
//Bit 10: 8        reg_rquant_yclut_1_2      // unsigned ,    RW, default = 1
//Bit  7            reserved
//Bit  6: 4        reg_rquant_yclut_1_1      // unsigned ,    RW, default = 0
//Bit  3            reserved
//Bit  2: 0        reg_rquant_yclut_1_0      // unsigned ,    RW, default = 0
#define DI_AFBCE1_YUV_FORMAT_CONV_MODE             0x20db
//Bit 31: 8        reserved
//Bit  7           reserved
//Bit  6: 4        reg_444to422_mode         // unsigned ,    RW, default = 0
//Bit  3           reserved
//Bit  2: 0        reg_422to420_mode         // unsigned ,    RW, default = 0
#define DI_AFBCE1_DUMMY_DATA                       0x20dc
//Bit 31:30        reserved
//Bit 29: 0        reg_dummy_data           // unsigned ,  default = 0  ;
#define DI_AFBCE1_CLR_FLAG                         0x20dd
//Bit 31:0         reg_di_AFBCE1_clr_flag           // unsigned, default = 0  ;
#define DI_AFBCE1_STA_FLAGT                        0x20de
//Bit 31:0         ro_di_AFBCE1__sta_flag        // unsigned, RO,default = 0  ;
#define DI_AFBCE1_MMU_NUM                          0x20df
//Bit 31:16        reserved
//Bit 15: 0        ro_frm_mmu_num           // unsigned, RO,default = 0  ;
#define DI_AFBCE1_MMU_RMIF_CTRL1                   0x20e0
//Bit 31:26 reserved
//Bit 25:24 reg_sync_sel
// unsigned , default = 0, axi canvas id sync with frm rst
//Bit 23:16 reg_canvas_id
// unsigned , default = 0, axi canvas id num
//Bit 15    reserved
//Bit 14:12 reg_cmd_intr_len
// unsigned , default = 1, interrupt send cmd when how many series axi cmd,
// 0=12 1=16 2=24 3=32 4=40 5=48 6=56 7=64
//Bit 11:10 reg_cmd_req_size
// unsigned , default = 1,
//how many room fifo have, then axi send series req, 0=16 1=32 2=24 3=64
//Bit 9:8   reg_burst_len
// unsigned , default = 2, burst type: 0-single 1-bst2 2-bst4
//Bit 7     reg_swap_64bit
// unsigned , default = 0, 64bits of 128bit swap enable
//Bit 6     reg_little_endian
// unsigned , default = 0, big endian enable
//Bit 5     reg_y_rev
// unsigned , default = 0, vertical reverse enable
//Bit 4     reg_x_rev
// unsigned , default = 0, horizontal reverse enable
//Bit 3     reserved
//Bit 2:0   reg_pack_mode
// unsigned , default = 3, 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit
#define DI_AFBCE1_MMU_RMIF_CTRL2                   0x20e1
//Bit 31:30 reg_sw_rst        // unsigned , default = 0,
//Bit 29:24 reserved
//Bit 23:18 reg_gclk_ctrl
//Bit 17    reserved
//Bit 16:0  reg_urgent_ctrl
// unsigned , default = 0, urgent control reg :
//  16  reg_ugt_init  :  urgent initial value
//  15  reg_ugt_en    :  urgent enable
//  14  reg_ugt_type  :  1= wrmif 0=rdmif
// 7:4  reg_ugt_top_th:  urgent top threshold
// 3:0  reg_ugt_bot_th:  urgent bottom threshold
#define DI_AFBCE1_MMU_RMIF_CTRL3                   0x20e2
//Bit 31:17 reserved
//Bit 16    reg_acc_mode      // unsigned , default = 1,
//Bit 15:13 reserved
//Bit 12:0  reg_stride        // unsigned , default = 4096,
#define DI_AFBCE1_MMU_RMIF_CTRL4                   0x20e3
//Bit 31:0  reg_baddr        // unsigned , default = 0,
#define DI_AFBCE1_MMU_RMIF_SCOPE_X                 0x20e4
//Bit 31:29 reserved

//Bit 28:16 reg_x_end
// unsigned , default = 4095, the canvas hor end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_x_start
// unsigned , default = 0, the canvas hor start pixel position
#define DI_AFBCE1_MMU_RMIF_SCOPE_Y                 0x20e5
//Bit 31:29 reserved
//Bit 28:16 reg_y_end
// unsigned , default = 0, the canvas ver end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_y_start
// unsigned , default = 0, the canvas ver start pixel position

#define DI_AFBCE1_MMU_RMIF_RO_STAT                 0x20e6
//Bit 15:0  reg_status        // unsigned ,
#define DI_AFBCE1_PIP_CTRL                         0x20ea
//Bit   31:3      reserved
//Bit   2         reg_enc_align_en     //unsigned  , RW,default = 1,
//Bit   1         reg_pip_ini_ctrl     //unsigned  , RW,default = 0,
//Bit   0         reg_pip_mode         //unsigned  , RW,default = 0,
#define DI_AFBCE1_ROT_CTRL                         0x20eb
//Bit   31:5      reserved

//Bit   4         reg_rot_en
//unsigned  , RW,default = 0, rotation enable
//Bit   3:0       reg_vstep
//unsigned  , RW,default = 8,
//rotation vstep ,setting acorrding rotation shrink mode
#define DI_AFBCE1_DIMM_CTRL                        0x20ec
//Bit   31        reg_dimm_layer_en
//unsigned  , RW,default = 0,dimm_layer enable signal
//Bit   30        reserved
//Bit   29:0      reg_dimm_data
//unsigned  , RW,default = 29'h00080200,dimm_layer data
#define DI_AFBCE1_BND_DEC_MISC                     0x20ed
//Bit 31:28  reserved
//Bit 27:26  bnd_dec_rev_mode
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 25:24  bnd_dec_mif_urgent
//unsigned , RW,default = 3
//only pip mode use those bits,usually don't need configure
//Bit 23:22  bnd_dec_burst_len
//unsigned , RW,default = 2
//only pip mode use those bits,usually don't need configure
//Bit 21:20  bnd_dec_ddr_blk_size
//unsigned , RW,default = 1
//only pip mode use those bits,usually don't need configure
//Bit 19     reserved
//Bit 18:16  bnd_dec_cmd_blk_size
//unsigned , RW,default = 3
//only pip mode use those bits,usually don't need configure
//Bit 15     reserved
//Bit 14     bnd_dec_blk_mem_mode
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 13     bnd_dec_addr_link_en
//unsigned , RW,default = 1
//only pip mode use those bits,usually don't need configure
//Bit 12     bnd_dec_always_body_rden
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 11:0   bnd_dec_mif_lbuf_depth
//unsigned , RW,default = 128
//only pip mode use those bits,usually don't need configure
#define DI_AFBCE1_RD_ARB_MISC                      (0x20ee)
//Bit 31:13  reserved
//Bit 12     reg_arb_sw_rst
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 11:10  reserved
//Bit 9      reg_arb_arblk_last1
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 8      reg_arb_arblk_last0
//unsigned , RW,default = 0
//only pip mode use those bits,usually don't need configure
//Bit 7:4    reg_arb_weight_ch1
//unsigned , RW,default = 4
//only pip mode use those bits,usually don't need configure
//Bit 3:0    reg_arb_weight_ch0
//unsigned , RW,default = 10
//only pip mode use those bits,usually don't need configure

#define DI_CRC_CHK0		((0x17c3)) /* << 2) + 0xd0100000) */
#define DI_RO_CRC_NRWR		((0x17c0)) /* << 2) + 0xd0100000) */
#define DI_RO_CRC_DEINT		((0x17c1)) /* << 2) + 0xd0100000) */
#define DI_RO_CRC_MTNWR		((0x17c2)) /* << 2) + 0xd0100000) */

#define DI_PD_RO_SUM_P		((0x17f9))
#define DI_PD_RO_SUM_N		((0x17fa))
#define DI_PD_RO_CNT_P		((0x17fb))
#define DI_PD_RO_CNT_N		((0x17fc))

#define DI_PRE_SEC_IN				0x2010
#define DI_POST_SEC_IN				0x2011
#define DI_VIU_DATA_SEC				0x1A50
#define DI_VIUB_SECURE_REG			0x200F
#define DI_VPU_SECURE_REG			0x270F
//for s5 from vlsi feijun HF secure Polarity

//t7
#define CONTRD_BADDR                               0x3729
#define CONT2RD_BADDR                              0x372a
#define MTNRD_BADDR                                0x372b
#define MCVECRD_BADDR                              0x372c
#define MCINFRD_BADDR                              0x372d
#define CONTWR_BADDR                               0x3734
#define CONTWR_STRIDE                              0x3735
#define MTNWR_BADDR                                0x3736
#define MTNWR_STRIDE                               0x3737
#define MCVECWR_BADDR                              0x372e
#define MCVECWR_STRIDE                             0x372f
#define MCINFWR_BADDR                              0x37ce
#define MCINFWR_STRIDE                             0x37cf
#define NRDSWR_BADDR                               0x37fd
#define NRDSWR_STRIDE                              0x37fe

/********************************************************/
/*from T3*/
// reg_base = 0x20_xx
#define NN_LRHF0                                   0x2014
//Bit 31           reg_lrhf_en
//Bit 29:26        reg_gclk_ctrl[3:0]
//Bit 25:16        reg_lrhf_hf_gain_neg
//Bit 9:0          reg_lrhf_hf_gain_pos
#define NN_LRHF1                                   0x2015
//Bit 31:28        reg_gclk_ctrl[7:4]
//Bit 26:16        reg_lrhf_lpf_coeff01
//Bit 10:0         reg_lrhf_lpf_coeff00
#define NN_LRHF2                                   0x2016
//Bit 31:28        reg_gclk_ctrl[11:8]
//Bit 26:16        reg_lrhf_lpf_coeff10
//Bit 10:0         reg_lrhf_lpf_coeff02
#define NN_LRHF3                                   0x2017
//Bit 26:16        reg_lrhf_lpf_coeff12
//Bit 10:0         reg_lrhf_lpf_coeff11
#define NN_LRHF4                                   0x2018
//Bit 26:16        reg_lrhf_lpf_coeff21
//Bit 10:0         reg_lrhf_lpf_coeff20
#define NN_LRHF5                                   0x2019
//Bit 31:24        reg_hblank_num
//Bit 23:16        reg_vblank_num
//Bit 10:0         eg_lrhf_lpf_coeff22
#define NN_LRHF6                                   0x201a
//Bit 31:24        reg_lrhf_pad_val
//Bit 22:12        reg_lrhf_pad_cols
//Bit 11           reg_dummy_en
//Bit 10:0         reg_lrhf_pad_rows
#define AISR_PRE_WRMIF_BADDR                       0x2020
//`define AISR_PRE_WMIF_CTRL1    AISR_PRE_WRMIF_BADDR+8'h00;  8'h20
#define AISR_PRE_WMIF_CTRL2                        0x2021
#define AISR_PRE_WMIF_CTRL3                        0x2022
#define AISR_PRE_WMIF_CTRL4                        0x2023
#define AISR_PRE_WMIF_SCOPE_X                      0x2024
#define AISR_PRE_WMIF_SCOPE_Y                      0x2025
#define AISR_PRE_WMIF_RO_STAT                      0x2026
/********************************************************/

/********************************************************/
/*from T3X*/
#define DI_T3X_AFBCE_ENABLE                            0x7f24
#define DI_T3X_AFBCE_MODE                              0x7f25
#define DI_T3X_AFBCE_SIZE_IN                           0x7f26
#define DI_T3X_AFBCE_BLK_SIZE_IN                       0x7f27
#define DI_T3X_AFBCE_HEAD_BADDR                        0x7f28
#define DI_T3X_AFBCE_MIF_SIZE                          0x7f29
#define DI_T3X_AFBCE_PIXEL_IN_HOR_SCOPE                0x7f2a
#define DI_T3X_AFBCE_PIXEL_IN_VER_SCOPE                0x7f2b
#define DI_T3X_AFBCE_CONV_CTRL                         0x7f2c
#define DI_T3X_AFBCE_MIF_HOR_SCOPE                     0x7f2d
#define DI_T3X_AFBCE_MIF_VER_SCOPE                     0x7f2e
#define DI_T3X_AFBCE_STAT1                             0x7f2f
#define DI_T3X_AFBCE_STAT2                             0x7f30
#define DI_T3X_AFBCE_FORMAT                            0x7f00
#define DI_T3X_AFBCE_MODE_EN                           0x7f01
#define DI_T3X_AFBCE_DWSCALAR                          0x7f02
#define DI_T3X_AFBCE_DEFCOLOR_1                        0x7f03
#define DI_T3X_AFBCE_DEFCOLOR_2                        0x7f04
#define DI_T3X_AFBCE_QUANT_ENABLE                      0x7f05
#define DI_T3X_AFBCE_IQUANT_LUT_1                      0x7f06
#define DI_T3X_AFBCE_IQUANT_LUT_2                      0x7f07
#define DI_T3X_AFBCE_IQUANT_LUT_3                      0x7f08
#define DI_T3X_AFBCE_IQUANT_LUT_4                      0x7f09
#define DI_T3X_AFBCE_RQUANT_LUT_1                      0x7f0a
#define DI_T3X_AFBCE_RQUANT_LUT_2                      0x7f0b
#define DI_T3X_AFBCE_RQUANT_LUT_3                      0x7f0c
#define DI_T3X_AFBCE_RQUANT_LUT_4                      0x7f0d
#define DI_T3X_AFBCE_YUV_FORMAT_CONV_MODE              0x7f0e
#define DI_T3X_AFBCE_DUMMY_DATA                        0x7f31
#define DI_T3X_AFBCE_CLR_FLAG                          0x7f32
#define DI_T3X_AFBCE_STA_FLAGT                         0x7f33
#define DI_T3X_AFBCE_MMU_NUM                           0x7f34
#define DI_T3X_AFBCE_MMU_RMIF_CTRL1                    0x7f3a
#define DI_T3X_AFBCE_MMU_RMIF_CTRL2                    0x7f3b
#define DI_T3X_AFBCE_MMU_RMIF_CTRL3                    0x7f3c
#define DI_T3X_AFBCE_MMU_RMIF_CTRL4                    0x7f3d
#define DI_T3X_AFBCE_MMU_RMIF_SCOPE_X                  0x7f3e
#define DI_T3X_AFBCE_MMU_RMIF_SCOPE_Y                  0x7f3f
#define DI_T3X_AFBCE_MMU_RMIF_RO_STAT                  0x7f40
#define DI_T3X_AFBCE_PIP_CTRL                          0x7f35
#define DI_T3X_AFBCE_ROT_CTRL                          0x7f36
#define DI_T3X_AFBCE_DIMM_CTRL                         0x7f37
#define DI_T3X_AFBCE_BND_DEC_MISC                      0x7f38
#define DI_T3X_AFBCE_RD_ARB_MISC                       0x7f39
/*from T3X,add new loss */
#define DI_T3X_AFBCE_LOSS_CTRL                         0x7f0f
//Bit 31         reg_fix_cr_en
// unsigned ,    RW, default = 1  enable of fix CR
//Bit 30         reg_rc_en
// unsigned ,    RW, default = 0  enable of rc
//Bit 29         reg_write_qlevel_mode
// unsigned ,    RW, default = 1  qlevel write mode
//Bit 28         reg_debug_qlevel_en
// unsigned ,    RW, default = 0  debug en
//Bit 27:20      reserved
//Bit 19:18      reserved
//Bit 17:16      reg_cal_bit_early_mode
// unsigned ,    RW, default = 2  early mode, RTL fix 2
//Bit 15:12      reg_quant_diff_root_leave
// unsigned ,    RW, default = 2  diff value of bctroot and bcleave
//Bit 11: 8      reg_debug_qlevel_value
// unsigned ,    RW, default = 0  debug qlevel value
//Bit  7: 4      reg_debug_qlevel_max_0
// unsigned ,    RW, default = 10  debug qlevel value
//Bit  3: 0      reg_debug_qlevel_max_1
// unsigned ,    RW, default = 10  debug qlevel value
#define DI_T3X_AFBCE_LOSS_BURST_NUM                    0x7f10
//Bit 31:29      reserved
//Bit 28:24      reg_block_burst_num_0
// unsigned ,    RW, default = 5  the num of burst of block
//Bit 23:21      reserved
//Bit 20:16      reg_block_burst_num_1
// unsigned ,    RW, default = 5  the num of burst of block
//Bit 15:13      reserved
//Bit 12: 8      reg_block_burst_num_2
// unsigned ,    RW, default = 5  the num of burst of block
//Bit  7: 5      reserved
//Bit  4: 0      reg_block_burst_num_3
// unsigned ,    RW, default = 5  the num of burst of block
#define DI_T3X_AFBCE_LOSS_RC                           0x7f11
//Bit 31:29      reserved
//Bit 28         reg_rc_fifo_margin_mode
// unsigned ,    RW, default = 1  enable of fifo margin mode
//Bit 27:24      reg_rc_1stbl_xbdgt
// unsigned ,    RW,default = 0extra bit budget for the first block in each line
//Bit 23:20      reg_rc_1stln_xbdgt
// unsigned ,    RW, default = 0  extra bit budget for the first line
//Bit 19:18      reserved
//Bit 17:16      reg_rc_qlevel_chk_mode
// unsigned ,    RW, default = 0
//0: max qlevel 1: avg qleve 2:max luma qlevel 3 : avg luma qlevel
//Bit 15: 8      reg_fix_cr_subb_bit_adj_0
// signed ,    RW, default = 0  luma each sub block add bits normal >=0
//Bit  7: 0      reg_fix_cr_subb_bit_adj_1
// signed ,    RW, default = 0  chroma each sub block minus bits normal <= 0
#define DI_T3X_AFBCE_LOSS_RC_FIFO_THD                  0x7f12
//Bit 31:28      reserved
//Bit 27:16      reg_rc_fifo_margin_thd_0
// unsigned ,    RW, default = 8
//threshold of fifo level to guard the rc loop by adding delta to burst
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_fifo_margin_thd_1
// unsigned ,    RW, default = 16
#define DI_T3X_AFBCE_LOSS_RC_FIFO_BUGET                0x7f13
//Bit 31:28      reserved
//Bit 27:16      reg_rc_fifo_margin_thd_2
// unsigned ,    RW, default = 32
//Bit 15:12      reg_rc_fifo_margin_buget_0
// unsigned ,    RW, default = 2  delta of fifo level to guard the rc loop
//Bit 11: 8      reg_rc_fifo_margin_buget_1
// unsigned ,    RW, default = 1
//Bit  7: 4      reg_rc_fifo_margin_buget_2
// unsigned ,    RW, default = 0
//Bit  3: 0      reg_rc_max_add_buget
// unsigned ,    RW, default = 2  the max add burst num
#define DI_T3X_AFBCE_LOSS_RC_ACCUM_THD_0               0x7f14
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_2_2
// unsigned ,    RW, default = 8
//threshold of accum to guard the rc loop by adding delta to burst,
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_2_1
// unsigned ,    RW, default = 8
//threshold of accum to guard the rc loop by adding delta to burst,
#define DI_T3X_AFBCE_LOSS_RC_ACCUM_THD_1               0x7f15
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_2_0
// unsigned ,    RW, default = 8
//threshold of accum to guard the rc loop by adding delta to burst,
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_1_2
// unsigned ,    RW, default = 128
//threshold of accum to guard the rc loop by adding delta to burst,
#define DI_T3X_AFBCE_LOSS_RC_ACCUM_THD_2               0x7f16
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_1_1
// unsigned ,    RW, default = 64
//threshold of accum to guard the rc loop by adding delta to burst,
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_1_0
// unsigned ,    RW, default = 4
//threshold of accum to guard the rc loop by adding delta to burst,
#define DI_T3X_AFBCE_LOSS_RC_ACCUM_THD_3               0x7f17
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_0_2
// unsigned ,    RW, default = 256
//threshold of accum to guard the rc loop by adding delta to burst,
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_0_1
// unsigned ,    RW, default = 128
//threshold of accum to guard the rc loop by adding delta to burst,
#define DI_T3X_AFBCE_LOSS_RC_ACCUM_BUGET_0             0x7f18
//Bit 31:20      reg_rc_accum_margin_thd_0_0
// unsigned ,    RW, default = 0
//threshold of accum to guard the rc loop by adding delta to burst,
//Bit 19:16      reserved
//Bit 15:12      reg_rc_accum_margin_buget_2_2
// unsigned ,    RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 11: 8      reg_rc_accum_margin_buget_2_1
// unsigned ,    RW, default = 0
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit  7: 4      reg_rc_accum_margin_buget_2_0
// unsigned ,    RW, default = 0
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit  3: 0      reg_rc_accum_margin_buget_1_2
// unsigned ,    RW, default = 2
//delta of accum to guard the rc loop by adding delta to burst, default=[]
#define DI_T3X_AFBCE_LOSS_RC_ACCUM_BUGET_1             0x7f19
//Bit 31:28      reg_rc_accum_margin_buget_1_1
// unsigned ,    RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 27:24      reg_rc_accum_margin_buget_1_0
// unsigned ,    RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 23:20      reg_rc_accum_margin_buget_0_2
// unsigned ,    RW, default = 2
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 19:16      reg_rc_accum_margin_buget_0_1
// unsigned ,    RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 15:12      reg_rc_accum_margin_buget_0_0
// unsigned ,    RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 11: 8      reserved
//Bit  7: 4      reg_rc_qlevel_chk_th_0
// unsigned ,    RW, default = 5
//threshold of qlevel for adjust burst based on accum
//Bit  3: 0      reg_rc_qlevel_chk_th_1
// unsigned ,    RW, default = 4
//threshold of qlevel for adjust burst based on accum
#define DI_T3X_AFBCE_LOSS_RO_ERROR_L_0                 0x7f1a
//Bit 31: 0      ro_error_acc_mode0_l_0
// unsigned ,    RO, default = 0  square
#define DI_T3X_AFBCE_LOSS_RO_COUNT_0                   0x7f1b
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_0
// unsigned ,    RO, default = 0  mode num contour
#define DI_T3X_AFBCE_LOSS_RO_ERROR_L_1                 0x7f1c
//Bit 31: 0      ro_error_acc_mode0_l_1
// unsigned ,    RO, default = 0  square
#define DI_T3X_AFBCE_LOSS_RO_COUNT_1                   0x7f1
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_1
// unsigned ,    RO, default = 0  mode num contour
#define DI_T3X_AFBCE_LOSS_RO_ERROR_L_2                 0x7f1
//Bit 31: 0      ro_error_acc_mode0_l_2
// unsigned ,    RO, default = 0  square
#define DI_T3X_AFBCE_LOSS_RO_COUNT_2                   0x7f1f
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_2
// unsigned ,    RO, default = 0  mode num contour
#define DI_T3X_AFBCE_LOSS_RO_ERROR_H_0                 0x7f20
//Bit 31:16      ro_error_acc_mode0_h_2
// unsigned ,    RO, default = 0  square
//Bit 15: 0      ro_error_acc_mode0_h_1
// unsigned ,    RO, default = 0  square
#define DI_T3X_AFBCE_LOSS_RO_ERROR_H_1                 0x7f21
//Bit 31:16      reserved
//Bit 15: 0      ro_error_acc_mode0_h_0
// unsigned ,    RO, default = 0  square
#define DI_T3X_AFBCE_LOSS_RO_MAX_ERROR_0               0x7f22
//Bit 31:28      reserved
//Bit 27:16      ro_max_error_mode0_2
// unsigned ,    RO, default = 0  error
//Bit 15:12      reserved
//Bit 11: 0      ro_max_error_mode0_1
// unsigned ,    RO, default = 0  error
#define DI_T3X_AFBCE_LOSS_RO_MAX_ERROR_1               0x7f23
//Bit 31:12      reserved
//Bit 11: 0      ro_max_error_mode0_0
// unsigned ,    RO, default = 0  error

#define DI_T3X_AFBCE1_ENABLE                           0x7fa4
#define DI_T3X_AFBCE1_MODE                             0x7fa5
#define DI_T3X_AFBCE1_SIZE_IN                          0x7fa6
#define DI_T3X_AFBCE1_BLK_SIZE_IN                      0x7fa7
#define DI_T3X_AFBCE1_HEAD_BADDR                       0x7fa8
#define DI_T3X_AFBCE1_MIF_SIZE                         0x7fa9
#define DI_T3X_AFBCE1_PIXEL_IN_HOR_SCOPE               0x7faa
#define DI_T3X_AFBCE1_PIXEL_IN_VER_SCOPE               0x7fab
#define DI_T3X_AFBCE1_CONV_CTRL                        0x7fac
#define DI_T3X_AFBCE1_MIF_HOR_SCOPE                    0x7fad
#define DI_T3X_AFBCE1_MIF_VER_SCOPE                    0x7fae
#define DI_T3X_AFBCE1_STAT1                            0x7faf
#define DI_T3X_AFBCE1_STAT2                            0x7fb0
#define DI_T3X_AFBCE1_FORMAT                           0x7f80
#define DI_T3X_AFBCE1_MODE_EN                          0x7f81
#define DI_T3X_AFBCE1_DWSCALAR                         0x7f82
#define DI_T3X_AFBCE1_DEFCOLOR_1                       0x7f83
#define DI_T3X_AFBCE1_DEFCOLOR_2                       0x7f84
#define DI_T3X_AFBCE1_QUANT_ENABLE                     0x7f85
#define DI_T3X_AFBCE1_IQUANT_LUT_1                     0x7f86
#define DI_T3X_AFBCE1_IQUANT_LUT_2                     0x7f87
#define DI_T3X_AFBCE1_IQUANT_LUT_3                     0x7f88
#define DI_T3X_AFBCE1_IQUANT_LUT_4                     0x7f89
#define DI_T3X_AFBCE1_RQUANT_LUT_1                     0x7f8a
#define DI_T3X_AFBCE1_RQUANT_LUT_2                     0x7f8b
#define DI_T3X_AFBCE1_RQUANT_LUT_3                     0x7f8c
#define DI_T3X_AFBCE1_RQUANT_LUT_4                     0x7f8d
#define DI_T3X_AFBCE1_YUV_FORMAT_CONV_MODE             0x7f8e
#define DI_T3X_AFBCE1_DUMMY_DATA                       0x7fb1
#define DI_T3X_AFBCE1_CLR_FLAG                         0x7fb2
#define DI_T3X_AFBCE1_STA_FLAGT                        0x7fb3
#define DI_T3X_AFBCE1_MMU_NUM                          0x7fb4
#define DI_T3X_AFBCE1_MMU_RMIF_CTRL1                   0x7fba
#define DI_T3X_AFBCE1_MMU_RMIF_CTRL2                   0x7fbb
#define DI_T3X_AFBCE1_MMU_RMIF_CTRL3                   0x7fbc
#define DI_T3X_AFBCE1_MMU_RMIF_CTRL4                   0x7fbd
#define DI_T3X_AFBCE1_MMU_RMIF_SCOPE_X                 0x7fbe
#define DI_T3X_AFBCE1_MMU_RMIF_SCOPE_Y                 0x7fbf
#define DI_T3X_AFBCE1_MMU_RMIF_RO_STAT                 0x7fc0
#define DI_T3X_AFBCE1_PIP_CTRL                         0x7fb5
#define DI_T3X_AFBCE1_ROT_CTRL                         0x7fb6
#define DI_T3X_AFBCE1_DIMM_CTRL                        0x7fb7
#define DI_T3X_AFBCE1_BND_DEC_MISC                     0x7fb8
#define DI_T3X_AFBCE1_RD_ARB_MISC                      0x7fb9
#define DI_T3X_AFBCE1_LOSS_CTRL                        0x7f8f
//Bit 31         reg_fix_cr_en
// unsigned ,    RW, default = 1  enable of fix CR
//Bit 30         reg_rc_en
// unsigned ,    RW, default = 0  enable of rc
//Bit 29         reg_write_qlevel_mode
// unsigned ,    RW, default = 1  qlevel write mode
//Bit 28         reg_debug_qlevel_en
// unsigned ,    RW, default = 0  debug en
//Bit 27:20      reserved
//Bit 19:18      reserved
//Bit 17:16      reg_cal_bit_early_mode
// unsigned ,    RW, default = 2  early mode, RTL fix 2
//Bit 15:12      reg_quant_diff_root_leave
// unsigned ,    RW, default = 2  diff value of bctroot and bcleave
//Bit 11: 8      reg_debug_qlevel_value
// unsigned ,    RW, default = 0  debug qlevel value
//Bit  7: 4      reg_debug_qlevel_max_0
// unsigned ,    RW, default = 10  debug qlevel value
//Bit  3: 0      reg_debug_qlevel_max_1
// unsigned ,    RW, default = 10  debug qlevel value
#define DI_T3X_AFBCE1_LOSS_BURST_NUM                   0x7f90
//Bit 31:29      reserved
//Bit 28:24      reg_block_burst_num_0
// unsigned ,    RW, default = 5  the num of burst of block
//Bit 23:21      reserved
//Bit 20:16      reg_block_burst_num_1
// unsigned ,    RW, default = 5  the num of burst of block
//Bit 15:13      reserved
//Bit 12: 8      reg_block_burst_num_2
// unsigned ,    RW, default = 5  the num of burst of block
//Bit  7: 5      reserved
//Bit  4: 0      reg_block_burst_num_3
// unsigned ,    RW, default = 5  the num of burst of block
#define DI_T3X_AFBCE1_LOSS_RC                          0x7f91
//Bit 31:29      reserved
//Bit 28         reg_rc_fifo_margin_mode
// unsigned ,    RW, default = 1  enable of fifo margin mode
//Bit 27:24      reg_rc_1stbl_xbdgt
// unsigned ,    RW, default = 0
//extra bit budget for the first block in each line
//Bit 23:20      reg_rc_1stln_xbdgt
// unsigned ,    RW, default = 0  extra bit budget for the first line
//Bit 19:18      reserved
//Bit 17:16      reg_rc_qlevel_chk_mode
// unsigned ,    RW,
//default = 0  0: max qlevel 1: avg qleve 2:max luma qlevel 3 : avg luma qlevel
//Bit 15: 8      reg_fix_cr_subb_bit_adj_0
// signed ,    RW, default = 0  luma each sub block add bits normal >=0
//Bit  7: 0      reg_fix_cr_subb_bit_adj_1
// signed ,    RW, default = 0  chroma each sub block minus bits normal <= 0
#define DI_T3X_AFBCE1_LOSS_RC_FIFO_THD                 0x7f92
//Bit 31:28      reserved
//Bit 27:16      reg_rc_fifo_margin_thd_0
// unsigned ,    RW,
//default=8threshold of fifo level to guard the rc loop by adding delta to burst
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_fifo_margin_thd_1
// unsigned ,    RW, default = 16
#define DI_T3X_AFBCE1_LOSS_RC_FIFO_BUGET               0x7f93
//Bit 31:28      reserved
//Bit 27:16      reg_rc_fifo_margin_thd_2
// unsigned ,    RW, default = 32
//Bit 15:12      reg_rc_fifo_margin_buget_0
// unsigned ,    RW, default = 2  delta of fifo level to guard the rc loop
//Bit 11: 8      reg_rc_fifo_margin_buget_1
// unsigned ,    RW, default = 1
//Bit  7: 4      reg_rc_fifo_margin_buget_2
// unsigned ,    RW, default = 0
//Bit  3: 0      reg_rc_max_add_buget
// unsigned ,    RW, default = 2  the max add burst num
#define DI_T3X_AFBCE1_LOSS_RC_ACCUM_THD_0              0x7f94
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_2_2
// unsigned ,    RW, default = 8
//threshold of accum to guard the rc loop by adding delta to burst,
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_2_1
// unsigned ,    RW, default = 8
//threshold of accum to guard the rc loop by adding delta to burst,
#define DI_T3X_AFBCE1_LOSS_RC_ACCUM_THD_1              0x7f95
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_2_0
// unsigned ,    RW, default = 8
//threshold of accum to guard the rc loop by adding delta to burst,
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_1_2
// unsigned ,    RW, default = 128
//threshold of accum to guard the rc loop by adding delta to burst,
#define DI_T3X_AFBCE1_LOSS_RC_ACCUM_THD_2              0x7f96
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_1_1
// unsigned ,    RW, default = 64
//threshold of accum to guard the rc loop by adding delta to burst,
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_1_0
// unsigned ,    RW, default = 4
//threshold of accum to guard the rc loop by adding delta to burst,
#define DI_T3X_AFBCE1_LOSS_RC_ACCUM_THD_3              0x7f97
//Bit 31:28      reserved
//Bit 27:16      reg_rc_accum_margin_thd_0_2
// unsigned ,    RW, default = 256
//threshold of accum to guard the rc loop by adding delta to burst,
//Bit 15:12      reserved
//Bit 11: 0      reg_rc_accum_margin_thd_0_1
// unsigned ,    RW, default = 128
//threshold of accum to guard the rc loop by adding delta to burst,
#define DI_T3X_AFBCE1_LOSS_RC_ACCUM_BUGET_0            0x7f98
//Bit 31:20      reg_rc_accum_margin_thd_0_0
// unsigned ,    RW, default = 0
//threshold of accum to guard the rc loop by adding delta to burst,
//Bit 19:16      reserved
//Bit 15:12      reg_rc_accum_margin_buget_2_2
// unsigned ,    RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 11: 8      reg_rc_accum_margin_buget_2_1
// unsigned ,    RW, default = 0
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit  7: 4      reg_rc_accum_margin_buget_2_0
// unsigned ,    RW, default = 0
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit  3: 0      reg_rc_accum_margin_buget_1_2
// unsigned ,    RW, default = 2
//delta of accum to guard the rc loop by adding delta to burst, default=[]
#define DI_T3X_AFBCE1_LOSS_RC_ACCUM_BUGET_1            0x7f99
//Bit 31:28      reg_rc_accum_margin_buget_1_1
// unsigned ,    RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 27:24      reg_rc_accum_margin_buget_1_0
// unsigned ,    RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 23:20      reg_rc_accum_margin_buget_0_2
// unsigned ,    RW, default = 2
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 19:16      reg_rc_accum_margin_buget_0_1
// unsigned ,    RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 15:12      reg_rc_accum_margin_buget_0_0
//unsigned,RW, default = 1
//delta of accum to guard the rc loop by adding delta to burst, default=[]
//Bit 11: 8      reserved
//Bit  7: 4      reg_rc_qlevel_chk_th_0
// nsigned,RW, default = 5  threshold of qlevel for adjust burst based on accum
//Bit  3: 0      reg_rc_qlevel_chk_th_1
// unsigned,RW, default = 4  threshold of qlevel for adjust burst based on accum
#define DI_T3X_AFBCE1_LOSS_RO_ERROR_L_0                0x7f9a
//Bit 31: 0      ro_error_acc_mode0_l_0
//unsigned ,    RO, default = 0  square
#define DI_T3X_AFBCE1_LOSS_RO_COUNT_0                  0x7f9b
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_0
//unsigned ,    RO, default = 0  mode num contour
#define DI_T3X_AFBCE1_LOSS_RO_ERROR_L_1                0x7f9c
//Bit 31: 0      ro_error_acc_mode0_l_1
//unsigned ,    RO, default = 0  square
#define DI_T3X_AFBCE1_LOSS_RO_COUNT_1                  0x7f9d
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_1
//unsigned ,    RO, default = 0  mode num contour
#define DI_T3X_AFBCE1_LOSS_RO_ERROR_L_2                0x7f9e
//Bit 31: 0      ro_error_acc_mode0_l_2
//unsigned ,    RO, default = 0  square
#define DI_T3X_AFBCE1_LOSS_RO_COUNT_2                  0x7f9f
//Bit 31:24      reserved
//Bit 23: 0      ro_enter_mode0_num_2
//unsigned ,    RO, default = 0  mode num contour
#define DI_T3X_AFBCE1_LOSS_RO_ERROR_H_0                0x7fa0
//Bit 31:16      ro_error_acc_mode0_h_2
//unsigned ,    RO, default = 0  square
//Bit 15: 0      ro_error_acc_mode0_h_1
//unsigned ,    RO, default = 0  square
#define DI_T3X_AFBCE1_LOSS_RO_ERROR_H_1                0x7fa1
//Bit 31:16      reserved
//Bit 15: 0      ro_error_acc_mode0_h_0
//unsigned ,    RO, default = 0  square
#define DI_T3X_AFBCE1_LOSS_RO_MAX_ERROR_0              0x7fa2
//Bit 31:28      reserved
//Bit 27:16      ro_max_error_mode0_2
//unsigned ,    RO, default = 0  error
//Bit 15:12      reserved
//Bit 11: 0      ro_max_error_mode0_1
//unsigned ,    RO, default = 0  error
#define DI_T3X_AFBCE1_LOSS_RO_MAX_ERROR_1              0x7fa3
//Bit 31:12      reserved
//Bit 11: 0      ro_max_error_mode0_0
//unsigned ,    RO, default = 0  error

//pre
#define T3X_NRWR_DBG_AXI_CMD_CNT                       0x7f60
#define T3X_NRWR_DBG_AXI_DAT_CNT                       0x7f61
#define DI_T3X_NRWR_CANVAS                             0x7f62
#define DI_T3X_NRWR_URGENT                             0x7f63
#define DI_T3X_NRWR_X                                  0x7f64
#define DI_T3X_NRWR_Y                                  0x7f65
#define DI_T3X_NRWR_CTRL                               0x7f66
#define DI_T3X_NRWR_SHRK_CTRL                          0x7f67
#define DI_T3X_NRWR_SHRK_SIZE                          0x7f68
#define DI_T3X_NRWR_CROP_CTRL                          0x7f6a
#define DI_T3X_NRWR_CROP_DIMM_CTRL                     0x7f6b
#define DI_T3X_NRWR_CROP_SIZE_IN                       0x7f6c
#define DI_T3X_NRWR_CROP_HSCOPE                        0x7f6d
#define DI_T3X_NRWR_CROP_VSCOPE                        0x7f6e
#define DI_T3X_NRWR_BADDR0                             0x7f70
#define DI_T3X_NRWR_STRIDE0                            0x7f71
#define DI_T3X_NRWR_BADDR1                             0x7f72
#define DI_T3X_NRWR_STRIDE1                            0x7f73
#define DI_T3X_NRWR_CTRL3                              0x7f74
//Bit 31:0        wmif_ctrl3            unsigned, default=0;

//post
#define T3X_DIWR_DBG_AXI_CMD_CNT                       0x7fe0
#define T3X_DIWR_DBG_AXI_DAT_CNT                       0x7fe1
#define DI_T3X_DIWR_CANVAS                             0x7fe2
#define DI_T3X_DIWR_URGENT                             0x7fe3
#define DI_T3X_DIWR_X                                  0x7fe4
#define DI_T3X_DIWR_Y                                  0x7fe5
#define DI_T3X_DIWR_CTRL                               0x7fe6
#define DI_T3X_DIWR_SHRK_CTRL                          0x7fe7
#define DI_T3X_DIWR_SHRK_SIZE                          0x7fe8
#define DI_T3X_DIWR_CROP_CTRL                          0x7fea
#define DI_T3X_DIWR_CROP_DIMM_CTRL                     0x7feb
#define DI_T3X_DIWR_CROP_SIZE_IN                       0x7fec
#define DI_T3X_DIWR_CROP_HSCOPE                        0x7fed
#define DI_T3X_DIWR_CROP_VSCOPE                        0x7fee
#define DI_T3X_DIWR_BADDR0                             0x7ff8
#define DI_T3X_DIWR_STRIDE0                            0x7ff9
#define DI_T3X_DIWR_BADDR1                             0x7ffa
#define DI_T3X_DIWR_STRIDE1                            0x7ffb
#define DI_T3X_DIWR_CTRL3                              0x7ffc
//Bit 31:0        wmif_ctrl3            unsigned, default=0;

#endif	/*__DI_REG_V3_H__*/

