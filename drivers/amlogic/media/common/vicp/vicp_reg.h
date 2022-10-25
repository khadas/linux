/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_processor/common/vicp/vicp_reg.h
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

#ifndef _VICP_REG_H_
#define _VICP_REG_H_

#include <linux/types.h>

//==========================================================================
// DMA
//==========================================================================
#define VID_CMPR_DMA_ENABLE                        0x0000
#define VID_CMPR_DMA_CBUF_READY                    0x0001
#define VID_CMPR_DMA_START                         0x0002
#define VID_CMPR_DMA_RESET                         0x0003
#define VID_CMPR_DMA_LBUF_CTRL                     0x0004
#define VID_CMPR_DMA_INN_CMD_CTRL                  0x0005
#define VID_CMPR_DMA_SYNC                          0x0006
#define VID_CMPR_DMA_CLK_CTRL                      0x0007
#define VID_CMPR_DMA_CBUF_BADDR_LSB                0x0008
#define VID_CMPR_DMA_CBUF_BADDR_MSB                0x0009
#define VID_CMPR_DMA_CBUF_LENGTH                   0x000a
#define VID_CMPR_DMA_LBUF_BADDR_LSB                0x000b
#define VID_CMPR_DMA_LBUF_BADDR_MSB                0x000c
#define VID_CMPR_DMA_LBUF_LENGTH                   0x000d
#define VID_CMPR_DMA_CBUF_MAX_NUM                  0x000e
#define VID_CMPR_DMA_JUMP_CTRL                     0x000f
#define VID_CMPR_DMA_MODE                          0x0010
#define VID_CMPR_DMA_ERR_MASK                      0x0011
#define VID_CMPR_DMA_ERR_CLR                       0x0012
#define VID_CMPR_DMA_ERR_FLAG                      0x0013
#define VID_CMPR_DMA_CBUF_STATUS0                  0x0015
#define VID_CMPR_DMA_CBUF_STATUS1                  0x0016
#define VID_CMPR_DMA_CBUF_STATUS2                  0x0017
#define VID_CMPR_DMA_CMD_DATA_LSB                  0x0018
#define VID_CMPR_DMA_CMD_DATA_MSB                  0x0019
#define VID_CMPR_CBUF_FETH_INDEX                   0x001a
#define VID_CMPR_CBUF_FETH_BADDR                   0x001b
#define VID_CMPR_CBUF_FETH_LENGTH                  0x001c
#define VID_CMPR_LBUF_FETH_BADDR                   0x001d
#define VID_CMPR_LBUF_FETH_LENGTH                  0x001e
#define VID_CMPR_DMA_INI_CMD0                      0x0020
#define VID_CMPR_DMA_INI_CMD1                      0x0021
#define VID_CMPR_DMA_INI_CMD2                      0x0022
#define VID_CMPR_DMA_INI_CMD3                      0x0023
#define VID_CMPR_DMA_INI_CMD4                      0x0024
#define VID_CMPR_DMA_INI_CMD5                      0x0025
#define VID_CMPR_DMA_INI_CMD6                      0x0026
#define VID_CMPR_DMA_INI_CMD7                      0x0027
#define VID_CMPR_DMA_INI_CMD8                      0x0028
#define VID_CMPR_DMA_INI_CMD9                      0x0029
#define VID_CMPR_DMA_INI_CMD10                     0x002a
#define VID_CMPR_DMA_INI_CMD11                     0x002b
#define VID_CMPR_DMA_INI_CMD12                     0x002c
#define VID_CMPR_DMA_INI_CMD13                     0x002d
#define VID_CMPR_DMA_INI_CMD14                     0x002e
#define VID_CMPR_DMA_INI_CMD15                     0x002f
#define VID_CMPR_DMA_CMD_DBG_IDX                   0x0040
#define VID_CMPR_DMA_RDMIF_CTRL0                   0x0060
#define VID_CMPR_DMA_RDMIF_CTRL1                   0x0061
#define VID_CMPR_DMA_RDMIF_STATUS                  0x0062
#define VID_CMPR_DMA_WRMIF_CTRL0                   0x0070
#define VID_CMPR_DMA_WRMIF_CTRL1                   0x0071
#define VID_CMPR_DMA_WRMIF_STATUS                  0x0072
#define VID_CMPR_DMA_ALIGN_MODE                    0x007f
#define VID_CMPR_DMA_WT_TIME                       0x0090
#define VID_CMPR_DMA_LD_TIME                       0x0091

#define VID_CMPR_CTRL                              0x0100
#define VID_CMPR_SW_RESETS                         0x0101
#define VID_CMPR_START                             0x0102
#define VID_CMPR_AUTO_START_MODE                   0x0103
#define VID_CMPR_AFLG_CLR                          0x0104
#define VID_CMPR_INT_CTRL                          0x0105
#define VID_CMPR_SEC_CTRL                          0x0106
#define VID_CMPR_HOLD_LINE                         0x0107
#define VID_CMPR_FIELD_FLAG                        0x0109
#define VID_CMPR_GCLK_CTRL                         0x010a
#define VID_CMPR_IN_SIZE                           0x0110
#define VID_CMPR_OUT_SIZE                          0x0111
#define VID_CMPR_DUMMY_DATA                        0x0114
#define VID_CMPR_WR_PATH_CTRL                      0x0118
#define VID_CMPR_PATH_HOLD_CTRL                    0x0119
#define VID_CMPR_SCO_BUF_COUNT                     0x011a
#define VID_CMPR_WRMIF_STATUS                      0x011b
#define VID_CMPR_DUMMY_LINE_MAX                    0x011f
#define VID_CMPR_APB_PROT_CTRL                     0x0125
#define VID_CMPR_APB_PROT_RST                      0x0126
#define VID_CMPR_APB_CRASH_NUM                     0x0127
#define VID_CMPR_CBUS_ARB_CTRL                     0x0128
#define VID_CMPR_SCAN_REG                          0x0130
#define VID_CMPR_AXIRD_QLEVEL                      0x0131
#define VID_CMPR_AXIWR_QLEVEL                      0x0132
#define VID_CMPR_ARCACHE                           0x0133
#define VID_CMPR_AWCACHE                           0x0134
#define VID_CMPR_RD_ASYNC_CTRL                     0x0135
#define VID_CMPR_WR_ASYNC_CTRL                     0x0136
#define VID_CMPR_SECURE_DAT                        0x0137
#define VID_CMPR_RAXI_PROT_CTRL                    0x0140
#define VID_CMPR_WAXI_PROT_CTRL                    0x0141
#define VID_CMPR_RAXI_PROT_STAT                    0x0145
#define VID_CMPR_WAXI_PROT_STAT                    0x0146
#define VID_CMPR_CRC_CTRL                          0x0150
#define VID_CMPR_CRC0_OUT                          0x0152
#define VID_CMPR_CRC1_0_OUT                        0x0153
#define VID_CMPR_CRC1_1_OUT                        0x0154
#define VID_CMPR_AXIRD_ARBX4_BADDR                 0x01b0
#define VID_CMPR_AXIWR_ARBX4_BADDR                 0x01c0

//==========================================================================
// READ MIF
//==========================================================================
#define VID_CMPR_RDMIFXN_GEN_REG                   0x0200
#define VID_CMPR_RDMIFXN_CANVAS0                   0x0201
#define VID_CMPR_RDMIFXN_CANVAS1                   0x0202
#define VID_CMPR_RDMIFXN_LUMA_X0                   0x0203
#define VID_CMPR_RDMIFXN_LUMA_Y0                   0x0204
#define VID_CMPR_RDMIFXN_CHROMA_X0                 0x0205
#define VID_CMPR_RDMIFXN_CHROMA_Y0                 0x0206
#define VID_CMPR_RDMIFXN_LUMA_X1                   0x0207
#define VID_CMPR_RDMIFXN_LUMA_Y1                   0x0208
#define VID_CMPR_RDMIFXN_CHROMA_X1                 0x0209
#define VID_CMPR_RDMIFXN_CHROMA_Y1                 0x020a
#define VID_CMPR_RDMIFXN_RPT_LOOP                  0x020b
#define VID_CMPR_RDMIFXN_LUMA0_RPT_PAT             0x020c
#define VID_CMPR_RDMIFXN_CHROMA0_RPT_PAT           0x020d
#define VID_CMPR_RDMIFXN_LUMA1_RPT_PAT             0x020e
#define VID_CMPR_RDMIFXN_CHROMA1_RPT_PAT           0x020f
#define VID_CMPR_RDMIFXN_LUMA_PSEL                 0x0210
#define VID_CMPR_RDMIFXN_CHROMA_PSEL               0x0211
#define VID_CMPR_RDMIFXN_DUMMY_PIXEL               0x0212
#define VID_CMPR_RDMIFXN_LUMA_FIFO_SIZE            0x0213
#define VID_CMPR_RDMIFXN_AXI_CMD_CNT               0x0214
#define VID_CMPR_RDMIFXN_AXI_RDAT_CNT              0x0215
#define VID_CMPR_RDMIFXN_RANGE_MAP_Y               0x0216
#define VID_CMPR_RDMIFXN_RANGE_MAP_CB              0x0217
#define VID_CMPR_RDMIFXN_RANGE_MAP_CR              0x0218
#define VID_CMPR_RDMIFXN_GEN_REG2                  0x0219
#define VID_CMPR_RDMIFXN_PROT                      0x021a
#define VID_CMPR_RDMIFXN_URGENT_CTRL               0x021b
#define VID_CMPR_RDMIFXN_GEN_REG3                  0x021c
#define VID_CMPR_RDMIFXN_CFMT_CTRL                 0x021d
#define VID_CMPR_RDMIFXN_CFMT_W                    0x021e
#define VID_CMPR_RDMIFXN_BADDR_Y                   0x0220
#define VID_CMPR_RDMIFXN_BADDR_CB                  0x0221
#define VID_CMPR_RDMIFXN_BADDR_CR                  0x0222
#define VID_CMPR_RDMIFXN_STRIDE_0                  0x0223
#define VID_CMPR_RDMIFXN_STRIDE_1                  0x0224
#define VID_CMPR_RDMIFXN_BADDR_Y_F1                0x0225
#define VID_CMPR_RDMIFXN_BADDR_CB_F1               0x0226
#define VID_CMPR_RDMIFXN_BADDR_CR_F1               0x0227
#define VID_CMPR_RDMIFXN_STRIDE_0_F1               0x0228
#define VID_CMPR_RDMIFXN_STRIDE_1_F1               0x0229

//==========================================================================
// AFBC_DEC
//==========================================================================
#define VID_CMPR_AFBCDM_VDTOP_CTRL0                0x0238
#define VID_CMPR_AFBCDM_ENABLE                     0x0240
#define VID_CMPR_AFBCDM_MODE                       0x0241
#define VID_CMPR_AFBCDM_SIZE_IN                    0x0242
#define VID_CMPR_AFBCDM_DEC_DEF_COLOR              0x0243
#define VID_CMPR_AFBCDM_CONV_CTRL                  0x0244
#define VID_CMPR_AFBCDM_LBUF_DEPTH                 0x0245
#define VID_CMPR_AFBCDM_HEAD_BADDR                 0x0246
#define VID_CMPR_AFBCDM_BODY_BADDR                 0x0247
#define VID_CMPR_AFBCDM_SIZE_OUT                   0x0248
#define VID_CMPR_AFBCDM_OUT_YSCOPE                 0x0249
#define VID_CMPR_AFBCDM_STAT                       0x024a
#define VID_CMPR_AFBCDM_VD_CFMT_CTRL               0x024b
#define VID_CMPR_AFBCDM_VD_CFMT_W                  0x024c
#define VID_CMPR_AFBCDM_MIF_HOR_SCOPE              0x024d
#define VID_CMPR_AFBCDM_MIF_VER_SCOPE              0x024e
#define VID_CMPR_AFBCDM_PIXEL_HOR_SCOPE            0x024f
#define VID_CMPR_AFBCDM_PIXEL_VER_SCOPE            0x0250
#define VID_CMPR_AFBCDM_VD_CFMT_H                  0x0251
#define VID_CMPR_AFBCDM_IQUANT_ENABLE              0x0252
#define VID_CMPR_AFBCDM_IQUANT_LUT_1               0x0253
#define VID_CMPR_AFBCDM_IQUANT_LUT_2               0x0254
#define VID_CMPR_AFBCDM_IQUANT_LUT_3               0x0255
#define VID_CMPR_AFBCDM_IQUANT_LUT_4               0x0256
#define VID_CMPR_AFBCDM_ROT_CTRL                   0x0260
#define VID_CMPR_AFBCDM_ROT_SCOPE                  0x0261
#define VID_CMPR_AFBCDM_RPLC_CTRL                  0x0262
#define VID_CMPR_AFBCDM_RPLC_PICEN                 0x0263
#define VID_CMPR_AFBCDM_RPLC_DEFCOL                0x0264
#define VID_CMPR_AFBCDM_RPLC_SCPXN_ADDR            0x0265
#define VID_CMPR_AFBCDM_RPLC_SCPXN_DATA            0x0266
#define VID_CMPR_AFBCDM_ROT_RO_STAT                0x0267
#define VID_CMPR_AFBCDM_FGRAIN_CTRL                0x0270
#define VID_CMPR_AFBCDM_FGRAIN_WIN_H               0x0271
#define VID_CMPR_AFBCDM_FGRAIN_WIN_V               0x0272
#define VID_CMPR_AFBCDM_FGRAIN_SW_Y_RANNGE         0x0273
#define VID_CMPR_AFBCDM_FGRAIN_SW_C_RANNGE         0x0274
#define VID_CMPR_AFBCDM_FGRAIN_GCLK_CTRL_0         0x0275
#define VID_CMPR_AFBCDM_FGRAIN_GCLK_CTRL_1         0x0276
#define VID_CMPR_AFBCDM_FGRAIN_GCLK_CTRL_2         0x0277
#define VID_CMPR_AFBCDM_FGRAIN_PARAM_ADDR          0x0278
#define VID_CMPR_AFBCDM_FGRAIN_PARAM_DATA          0x0279

//==========================================================================
// AFBC_ENC
//==========================================================================
#define VID_CMPR_AFBCE_ENABLE                      0x0290
#define VID_CMPR_AFBCE_MODE                        0x0291
#define VID_CMPR_AFBCE_SIZE_IN                     0x0292
#define VID_CMPR_AFBCE_BLK_SIZE_IN                 0x0293
#define VID_CMPR_AFBCE_HEAD_BADDR                  0x0294
#define VID_CMPR_AFBCE_MIF_SIZE                    0x0295
#define VID_CMPR_AFBCE_PIXEL_IN_HOR_SCOPE          0x0296
#define VID_CMPR_AFBCE_PIXEL_IN_VER_SCOPE          0x0297
#define VID_CMPR_AFBCE_CONV_CTRL                   0x0298
#define VID_CMPR_AFBCE_MIF_HOR_SCOPE               0x0299
#define VID_CMPR_AFBCE_MIF_VER_SCOPE               0x029a
#define VID_CMPR_AFBCE_STAT1                       0x029b
#define VID_CMPR_AFBCE_STAT2                       0x029c
#define VID_CMPR_AFBCE_FORMAT                      0x029d
#define VID_CMPR_AFBCE_MODE_EN                     0x029e
#define VID_CMPR_AFBCE_DWSCALAR                    0x029f
#define VID_CMPR_AFBCE_DEFCOLOR_1                  0x02a0
#define VID_CMPR_AFBCE_DEFCOLOR_2                  0x02a1
#define VID_CMPR_AFBCE_QUANT_ENABLE                0x02a2
#define VID_CMPR_AFBCE_IQUANT_LUT_1                0x02a3
#define VID_CMPR_AFBCE_IQUANT_LUT_2                0x02a4
#define VID_CMPR_AFBCE_IQUANT_LUT_3                0x02a5
#define VID_CMPR_AFBCE_IQUANT_LUT_4                0x02a6
#define VID_CMPR_AFBCE_RQUANT_LUT_1                0x02a7
#define VID_CMPR_AFBCE_RQUANT_LUT_2                0x02a8
#define VID_CMPR_AFBCE_RQUANT_LUT_3                0x02a9
#define VID_CMPR_AFBCE_RQUANT_LUT_4                0x02aa
#define VID_CMPR_AFBCE_YUV_FORMAT_CONV_MODE        0x02ab
#define VID_CMPR_AFBCE_DUMMY_DATA                  0x02ac
#define VID_CMPR_AFBCE_CLR_FLAG                    0x02ad
#define VID_CMPR_AFBCE_STA_FLAGT                   0x02ae
#define VID_CMPR_AFBCE_MMU_NUM                     0x02af
#define VID_CMPR_AFBCE_PIP_CTRL                    0x02b0
#define VID_CMPR_AFBCE_ROT_CTRL                    0x02b1
#define VID_CMPR_AFBCE_DIMM_CTRL                   0x02b2
#define VID_CMPR_AFBCE_BND_DEC_MISC                0x02b3
#define VID_CMPR_AFBCE_RD_ARB_MISC                 0x02b4
#define VID_CMPR_AFBCE_MMU_RMIF_CTRL1              0x02c0
#define VID_CMPR_AFBCE_MMU_RMIF_CTRL2              0x02c1
#define VID_CMPR_AFBCE_MMU_RMIF_CTRL3              0x02c2
#define VID_CMPR_AFBCE_MMU_RMIF_CTRL4              0x02c3
#define VID_CMPR_AFBCE_MMU_RMIF_SCOPE_X            0x02c4
#define VID_CMPR_AFBCE_MMU_RMIF_SCOPE_Y            0x02c5
#define VID_CMPR_AFBCE_MMU_RMIF_RO_STAT            0x02c6

//==========================================================================
//WRITE MIF
//==========================================================================
#define VID_CMPR_WRMIF_BADDR0                      0x02e8
#define VID_CMPR_WRMIF_STRIDE0                     0x02e9
#define VID_CMPR_WRMIF_BADDR1                      0x02ea
#define VID_CMPR_WRMIF_STRIDE1                     0x02eb
#define VID_CMPR_WRMIF_DBG_AXI_CMD_CNT             0x02f0
#define VID_CMPR_WRMIF_DBG_AXI_DAT_CNT             0x02f1
#define VID_CMPR_WRMIF_CANVAS                      0x02f2
#define VID_CMPR_WRMIF_URGENT                      0x02f3
#define VID_CMPR_WRMIF_X                           0x02f4
#define VID_CMPR_WRMIF_Y                           0x02f5
#define VID_CMPR_WRMIF_CTRL                        0x02f6
#define VID_CMPR_WRMIF_SHRK_CTRL                   0x02f7
#define VID_CMPR_WRMIF_SHRK_SIZE                   0x02f8

//==========================================================================
// CROP
//==========================================================================
#define VID_CMPR_CROP_CTRL                         0x02fa
#define VID_CMPR_CROP_DIMM_CTRL                    0x02fb
#define VID_CMPR_CROP_SIZE_IN                      0x02fc
#define VID_CMPR_CROP_HSCOPE                       0x02fd
#define VID_CMPR_CROP_VSCOPE                       0x02fe

//==========================================================================
// HDR
//==========================================================================
#define VID_CMPR_HDR2_CTRL                         0x0300
#define VID_CMPR_HDR2_CLK_GATE                     0x0301
#define VID_CMPR_HDR2_MATRIXI_COEF00_01            0x0302
#define VID_CMPR_HDR2_MATRIXI_COEF02_10            0x0303
#define VID_CMPR_HDR2_MATRIXI_COEF11_12            0x0304
#define VID_CMPR_HDR2_MATRIXI_COEF20_21            0x0305
#define VID_CMPR_HDR2_MATRIXI_COEF22               0x0306
#define VID_CMPR_HDR2_MATRIXI_COEF30_31            0x0307
#define VID_CMPR_HDR2_MATRIXI_COEF32_40            0x0308
#define VID_CMPR_HDR2_MATRIXI_COEF41_42            0x0309
#define VID_CMPR_HDR2_MATRIXI_OFFSET0_1            0x030a
#define VID_CMPR_HDR2_MATRIXI_OFFSET2              0x030b
#define VID_CMPR_HDR2_MATRIXI_PRE_OFFSET0_1        0x030c
#define VID_CMPR_HDR2_MATRIXI_PRE_OFFSET2          0x030d
#define VID_CMPR_HDR2_MATRIXO_COEF00_01            0x030e
#define VID_CMPR_HDR2_MATRIXO_COEF02_10            0x030f
#define VID_CMPR_HDR2_MATRIXO_COEF11_12            0x0310
#define VID_CMPR_HDR2_MATRIXO_COEF20_21            0x0311
#define VID_CMPR_HDR2_MATRIXO_COEF22               0x0312
#define VID_CMPR_HDR2_MATRIXO_COEF30_31            0x0313
#define VID_CMPR_HDR2_MATRIXO_COEF32_40            0x0314
#define VID_CMPR_HDR2_MATRIXO_COEF41_42            0x0315
#define VID_CMPR_HDR2_MATRIXO_OFFSET0_1            0x0316
#define VID_CMPR_HDR2_MATRIXO_OFFSET2              0x0317
#define VID_CMPR_HDR2_MATRIXO_PRE_OFFSET0_1        0x0318
#define VID_CMPR_HDR2_MATRIXO_PRE_OFFSET2          0x0319
#define VID_CMPR_HDR2_MATRIXI_CLIP                 0x031a
#define VID_CMPR_HDR2_MATRIXO_CLIP                 0x031b
#define VID_CMPR_HDR2_CGAIN_OFFT                   0x031c
#define VID_CMPR_HDR2_HIST_RD                      0x031d
#define VID_CMPR_HDR2_EOTF_LUT_ADDR_PORT           0x031e
#define VID_CMPR_HDR2_EOTF_LUT_DATA_PORT           0x031f
#define VID_CMPR_HDR2_OETF_LUT_ADDR_PORT           0x0320
#define VID_CMPR_HDR2_OETF_LUT_DATA_PORT           0x0321
#define VID_CMPR_HDR2_OGAIN_LUT_ADDR_PORT          0x0326
#define VID_CMPR_HDR2_OGAIN_LUT_DATA_PORT          0x0327
#define VID_CMPR_HDR2_CGAIN_LUT_ADDR_PORT          0x0322
#define VID_CMPR_HDR2_CGAIN_LUT_DATA_PORT          0x0323
#define VID_CMPR_HDR2_CGAIN_COEF0                  0x0324
#define VID_CMPR_HDR2_CGAIN_COEF1                  0x0325
#define VID_CMPR_HDR2_ADPS_CTRL                    0x0328
#define VID_CMPR_HDR2_ADPS_ALPHA0                  0x0329
#define VID_CMPR_HDR2_ADPS_ALPHA1                  0x032a
#define VID_CMPR_HDR2_ADPS_BETA0                   0x032b
#define VID_CMPR_HDR2_ADPS_BETA1                   0x032c
#define VID_CMPR_HDR2_ADPS_BETA2                   0x032d
#define VID_CMPR_HDR2_ADPS_COEF0                   0x032e
#define VID_CMPR_HDR2_ADPS_COEF1                   0x032f
#define VID_CMPR_HDR2_GMUT_CTRL                    0x0330
#define VID_CMPR_HDR2_GMUT_COEF0                   0x0331
#define VID_CMPR_HDR2_GMUT_COEF1                   0x0332
#define VID_CMPR_HDR2_GMUT_COEF2                   0x0333
#define VID_CMPR_HDR2_GMUT_COEF3                   0x0334
#define VID_CMPR_HDR2_GMUT_COEF4                   0x0335
#define VID_CMPR_HDR2_PIPE_CTRL1                   0x0336
#define VID_CMPR_HDR2_PIPE_CTRL2                   0x0337
#define VID_CMPR_HDR2_PIPE_CTRL3                   0x0338
#define VID_CMPR_HDR2_PROC_WIN1                    0x0339
#define VID_CMPR_HDR2_PROC_WIN2                    0x033a
#define VID_CMPR_HDR2_MATRIXI_EN_CTRL              0x033b
#define VID_CMPR_HDR2_MATRIXO_EN_CTRL              0x033c
#define VID_CMPR_HDR2_HIST_CTRL                    0x033d
#define VID_CMPR_HDR2_HIST_H_START_END             0x033e
#define VID_CMPR_HDR2_HIST_V_START_END             0x033f

//==========================================================================
// SCALER
//==========================================================================
#define VID_CMPR_SCALE_COEF_IDX                    0x0370
#define VID_CMPR_SCALE_COEF                        0x0371
#define VID_CMPR_VSC_REGION12_STARTP               0x0372
#define VID_CMPR_VSC_REGION34_STARTP               0x0373
#define VID_CMPR_VSC_REGION4_ENDP                  0x0374
#define VID_CMPR_VSC_START_PHASE_STEP              0x0375
#define VID_CMPR_VSC_REGION0_PHASE_SLOPE           0x0376
#define VID_CMPR_VSC_REGION1_PHASE_SLOPE           0x0377
#define VID_CMPR_VSC_REGION3_PHASE_SLOPE           0x0378
#define VID_CMPR_VSC_REGION4_PHASE_SLOPE           0x0379
#define VID_CMPR_VSC_PHASE_CTRL                    0x037a
#define VID_CMPR_VSC_INI_PHASE                     0x037b
#define VID_CMPR_HSC_REGION12_STARTP               0x037c
#define VID_CMPR_HSC_REGION34_STARTP               0x037d
#define VID_CMPR_HSC_REGION4_ENDP                  0x037e
#define VID_CMPR_HSC_START_PHASE_STEP              0x037f
#define VID_CMPR_HSC_REGION0_PHASE_SLOPE           0x0380
#define VID_CMPR_HSC_REGION1_PHASE_SLOPE           0x0381
#define VID_CMPR_HSC_REGION3_PHASE_SLOPE           0x0382
#define VID_CMPR_HSC_REGION4_PHASE_SLOPE           0x0383
#define VID_CMPR_HSC_PHASE_CTRL                    0x0384
#define VID_CMPR_SC_MISC                           0x0385
#define VID_CMPR_SCO_FIFO_CTRL                     0x0386
#define VID_CMPR_HSC_PHASE_CTRL1                   0x0387
#define VID_CMPR_HSC_INI_PAT_CTRL                  0x0388
#define VID_CMPR_SC_GCLK_CTRL                      0x0389
#define VID_CMPR_PREHSC_COEF                       0x038a
#define VID_CMPR_PREVSC_COEF                       0x038b
#define VID_CMPR_PRE_SCALE_CTRL                    0x038c
#define VID_CMPR_PREHSC_COEF1                      0x038d

extern void __iomem *vicp_reg_map;
u32 vicp_reg_read(u32 reg);
void vicp_reg_write(u32 reg, u32 val);
u32 vicp_vcbus_read(u32 reg);
void vicp_vcbus_write(u32 reg, u32 val);
u32 vicp_reg_get_bits(u32 reg, const u32 start, const u32 len);
void vicp_reg_set_bits(u32 reg, const u32 value, const u32 start, const u32 len);
void vicp_reg_write_addr(u64 addr, u64 data);
u64 vicp_reg_read_addr(u64 addr);
#endif
