/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2018 Amlogic or its affiliates
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

#ifndef __SYSTEM_AM_ADAP_H__
#define __SYSTEM_AM_ADAP_H__

#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include "acamera_firmware_config.h"

//#define MIPI_CSI2_ADPT_FT0_BASE_ADDR       0xFE3B0000
//#define MIPI_CSI2_ADPT_FT1_BASE_ADDR       0xFE3B0400
//#define MIPI_CSI2_ADPT_FT2_BASE_ADDR       0xFE3B0800
//#define MIPI_CSI2_ADPT_FT3_BASE_ADDR       0xFE3B0C00
#define FRONTEND0_BASE                       0x0
#define FRONTEND1_BASE                       0x400
#define FRONTEND2_BASE                       0x800
#define FRONTEND3_BASE                       0xC00

#define FTE1_OFFSET                          0x400
#define RD_BASE                              0x1000
#define PIXEL_BASE                           0x1000
#define ALIGN_BASE                           0x1000
#define MISC_BASE                            0x1000
#define CMPR_CNTL_BASE                       0x3000
#define CMPR_BASE                            0x4000
#define MIPI_CSI2_ADPT_CFG_BASE_ADDR         0xFE3B1000

#define CSI2_CLK_RESET                       (0x00<<2)
#define CSI2_GEN_CTRL0                       (0x01<<2)
#define CSI2_GEN_CTRL1                       (0x02<<2)
#define CSI2_X_START_END_ISP                 (0x03<<2)
#define CSI2_Y_START_END_ISP                 (0x04<<2)
#define CSI2_X_START_END_MEM                 (0x05<<2)
#define CSI2_Y_START_END_MEM                 (0x06<<2)
#define CSI2_VC_MODE                         (0x07<<2)
#define CSI2_VC_MODE2_MATCH_MASK_A_L         (0x08<<2)
#define CSI2_VC_MODE2_MATCH_MASK_A_H         (0x09<<2)
#define CSI2_VC_MODE2_MATCH_A_L              (0x0a<<2)
#define CSI2_VC_MODE2_MATCH_A_H              (0x0b<<2)
#define CSI2_VC_MODE2_MATCH_B_L              (0x0c<<2)
#define CSI2_VC_MODE2_MATCH_B_H              (0x0d<<2)
#define CSI2_DDR_START_PIX                   (0x0e<<2)
#define CSI2_DDR_START_PIX_ALT               (0x0f<<2)
#define CSI2_DDR_STRIDE_PIX                  (0x10<<2)
#define CSI2_DDR_START_OTHER                 (0x11<<2)
#define CSI2_DDR_START_OTHER_ALT             (0x12<<2)
#define CSI2_DDR_MAX_BYTES_OTHER             (0x13<<2)
#define CSI2_INTERRUPT_CTRL_STAT             (0x14<<2)
#define CSI2_VC_MODE2_MATCH_MASK_B_L         (0x15<<2)
#define CSI2_VC_MODE2_MATCH_MASK_B_H         (0x16<<2)
#define CSI2_GEN_STAT0                       (0x20<<2)
#define CSI2_ERR_STAT0                       (0x21<<2)
#define CSI2_PIC_SIZE_STAT                   (0x22<<2)
#define CSI2_DDR_WPTR_STAT_PIX               (0x23<<2)
#define CSI2_DDR_WPTR_STAT_OTHER             (0x24<<2)
#define CSI2_STAT_MEM_0                      (0x25<<2)
#define CSI2_STAT_MEM_1                      (0x26<<2)
#define CSI2_STAT_MEM_2                      (0x27<<2)
#define CSI2_STAT_GEN_SHORT_08               (0x28<<2)
#define CSI2_STAT_GEN_SHORT_09               (0x29<<2)
#define CSI2_STAT_GEN_SHORT_0A               (0x2a<<2)
#define CSI2_STAT_GEN_SHORT_0B               (0x2b<<2)
#define CSI2_STAT_GEN_SHORT_0C               (0x2c<<2)
#define CSI2_STAT_GEN_SHORT_0D               (0x2d<<2)
#define CSI2_STAT_GEN_SHORT_0E               (0x2e<<2)
#define CSI2_STAT_GEN_SHORT_0F               (0x2f<<2)
#define CSI2_STAT_TYPE_RCVD_L                (0x30<<2)
#define CSI2_STAT_TYPE_RCVD_H                (0x31<<2)
#define CSI2_DDR_START_PIX_B                 (0x32<<2)
#define CSI2_DDR_START_PIX_B_ALT             (0x33<<2)
#define CSI2_DDR_STRIDE_PIX_B                (0x34<<2)
#define CSI2_DDR_WPTR_STAT_PIX_B             (0x35<<2)
#define CSI2_STAT_MEM_3                      (0x36<<2)
#define CSI2_AXI_QOS_CNTL0                   (0x37<<2)
#define CSI2_AXI_QOS_CNTL1                   (0x38<<2)
#define CSI2_AXI_UGT_CNTL0                   (0x39<<2)
#define CSI2_AXI_QOS_ST                      (0x3a<<2)
#define CSI2_LINE_SUP_CNTL0                  (0x3b<<2)
#define CSI2_LINE_SUP_ST                     (0x3c<<2)
#define MIPI_ADAPT_DDR_RD0_CNTL0             (0x0000  << 2)
#define MIPI_ADAPT_DDR_RD0_CNTL1             (0x0001  << 2)
#define MIPI_ADAPT_DDR_RD0_CNTL2             (0x0002  << 2)
#define MIPI_ADAPT_DDR_RD0_CNTL3             (0x0003  << 2)
#define MIPI_ADAPT_DDR_RD0_CNTL4             (0x0004  << 2)
#define MIPI_ADAPT_DDR_RD0_CNTL5             (0x0005  << 2)
#define MIPI_ADAPT_DDR_RD0_CNTL6             (0x0006  << 2)
#define MIPI_ADAPT_DDR_RD0_UGT_CNTL0         (0x0007  << 2)
#define MIPI_ADAPT_DDR_RD0_UGT_CNTL1         (0x0008  << 2)
#define MIPI_ADAPT_DDR_RD0_ST0               (0x000a  << 2)
#define MIPI_ADAPT_DDR_RD0_ST1               (0x000b  << 2)
#define MIPI_ADAPT_DDR_RD0_ST2               (0x000c  << 2)
#define MIPI_ADAPT_DDR_RD0_UGT_ST            (0x000d  << 2)
#define MIPI_ADAPT_DDR_RD1_CNTL0             (0x0010  << 2)
#define MIPI_ADAPT_DDR_RD1_CNTL1             (0x0011  << 2)
#define MIPI_ADAPT_DDR_RD1_CNTL2             (0x0012  << 2)
#define MIPI_ADAPT_DDR_RD1_CNTL3             (0x0013  << 2)
#define MIPI_ADAPT_DDR_RD1_CNTL4             (0x0014  << 2)
#define MIPI_ADAPT_DDR_RD1_CNTL5             (0x0015  << 2)
#define MIPI_ADAPT_DDR_RD1_CNTL6             (0x0016  << 2)
#define MIPI_ADAPT_DDR_RD1_UGT_CNTL0         (0x0017  << 2)
#define MIPI_ADAPT_DDR_RD1_UGT_CNTL1         (0x0018  << 2)
#define MIPI_ADAPT_DDR_RD1_ST0               (0x001a  << 2)
#define MIPI_ADAPT_DDR_RD1_ST1               (0x001b  << 2)
#define MIPI_ADAPT_DDR_RD1_ST2               (0x001c  << 2)
#define MIPI_ADAPT_DDR_RD1_UGT_ST            (0x001d  << 2)
#define MIPI_ADAPT_PIXEL0_CNTL0              (0x0020  << 2)
#define MIPI_ADAPT_PIXEL0_CNTL1              (0x0021  << 2)
#define MIPI_ADAPT_PIXEL0_CNTL2              (0x0022  << 2)
#define MIPI_ADAPT_PIXEL0_CNTL3              (0x0023  << 2)
#define MIPI_ADAPT_PIXEL0_ST0                (0x002a  << 2)
#define MIPI_ADAPT_PIXEL0_ST1                (0x002b  << 2)
#define MIPI_ADAPT_PIXEL1_CNTL0              (0x0030  << 2)
#define MIPI_ADAPT_PIXEL1_CNTL1              (0x0031  << 2)
#define MIPI_ADAPT_PIXEL1_CNTL2              (0x0032  << 2)
#define MIPI_ADAPT_PIXEL1_CNTL3              (0x0033  << 2)
#define MIPI_ADAPT_PIXEL1_ST0                (0x003a  << 2)
#define MIPI_ADAPT_PIXEL1_ST1                (0x003b  << 2)
#define MIPI_ADAPT_ALIG_CNTL0                (0x0040  << 2)
#define MIPI_ADAPT_ALIG_CNTL1                (0x0041  << 2)
#define MIPI_ADAPT_ALIG_CNTL2                (0x0042  << 2)
#define MIPI_ADAPT_ALIG_CNTL3                (0x0043  << 2)
#define MIPI_ADAPT_ALIG_CNTL4                (0x0044  << 2)
#define MIPI_ADAPT_ALIG_CNTL5                (0x0045  << 2)
#define MIPI_ADAPT_ALIG_CNTL6                (0x0046  << 2)
#define MIPI_ADAPT_ALIG_CNTL7                (0x0047  << 2)
#define MIPI_ADAPT_ALIG_CNTL8                (0x0048  << 2)
#define MIPI_ADAPT_ALIG_CNTL9                (0x0049  << 2)
#define MIPI_ADAPT_ALIG_CNTL10               (0x004a  << 2)
#define MIPI_ADAPT_ALIG_CNTL11               (0x004b  << 2)
#define MIPI_ADAPT_ALIG_CNTL12               (0x004c  << 2)
#define MIPI_ADAPT_ALIG_CNTL13               (0x004d  << 2)
#define MIPI_ADAPT_ALIG_CNTL14               (0x004e  << 2)
#define MIPI_ADAPT_ALIG_CNTL15               (0x004f  << 2)
#define MIPI_ADAPT_ALIG_CNTL16               (0x0050  << 2)
#define MIPI_ADAPT_ALIG_CNTL17               (0x0051  << 2)
#define MIPI_ADAPT_ALIG_CNTL18               (0x0052  << 2)
#define MIPI_ADAPT_ALIG_CNTL19               (0x0053  << 2)
#define MIPI_ADAPT_ALIG_ST0                  (0x005a  << 2)
#define MIPI_ADAPT_ALIG_ST1                  (0x005b  << 2)
#define MIPI_ADAPT_ALIG_ST2                  (0x005c  << 2)
#define MIPI_ADAPT_OTHER_CNTL0               (0x0060  << 2)
#define MIPI_ADAPT_FE_MUX_CTL0               (0x0090  << 2)
#define MIPI_ADAPT_FE_MUX_CTL1               (0x0091  << 2)
#define MIPI_ADAPT_FE_MUX_CTL2               (0x00be  << 2)
#define MIPI_ADAPT_FE_MUX_CTL3               (0x00bf  << 2)
#define MIPI_ADAPT_FE_MUX_CTL4               (0x00c0  << 2)

#define MIPI_TOP_ISP_PENDING_MASK0           ((0xf0 << 2) + 0xfe3b3000)
#define MIPI_TOP_ISP_PENDING_MASK1           ((0xf1 << 2) + 0xfe3b3000)
#define MIPI_TOP_ISP_PENDING_MASK2           ((0xf2 << 2) + 0xfe3b3000)
#define MIPI_TOP_ISP_PENDING0                ((0xf3 << 2) + 0xfe3b3000)
#define MIPI_TOP_ISP_PENDING1                ((0xf4 << 2) + 0xfe3b3000)
#define MIPI_TOP_ISP_PENDING2                ((0xf5 << 2) + 0xfe3b3000)




/*----------differnent with c2-------------*/
#define CSI2_DDR_LOOP_LINES_PIX      0x5c
#define MIPI_OTHER_CNTL0             0x60 << 2
#define MIPI_ADAPT_IRQ_MASK0         0x180
#define MIPI_ADAPT_IRQ_PENDING0      0x184
/*-----------------------------------------*/

#define MODE_MIPI_RAW_SDR_DDR          0
#define MODE_MIPI_RAW_SDR_DIRCT        1
#define MODE_MIPI_RAW_HDR_DDR_DDR      2
#define MODE_MIPI_RAW_HDR_DDR_DIRCT    3
#define MODE_MIPI_YUV_SDR_DDR          4
#define MODE_MIPI_RGB_SDR_DDR          5
#define MODE_MIPI_YUV_SDR_DIRCT        6
#define MODE_MIPI_RGB_SDR_DIRCT        7

#define MIPI_CSI_YUV422_8BIT           0x1e
#define MIPI_CSI_YUV422_10BIT          0x1f
#define MIPI_CSI_RGB444                0x20
#define MIPI_CSI_RGB555                0x21
#define MIPI_CSI_RGB565                0x22
#define MIPI_CSI_RGB666                0x23
#define MIPI_CSI_RGB888                0x24
#define MIPI_CSI_RAW6                  0x28
#define MIPI_CSI_RAW7                  0x29
#define MIPI_CSI_RAW8                  0x2a
#define MIPI_CSI_RAW10                 0x2b
#define MIPI_CSI_RAW12                 0x2c
#define MIPI_CSI_RAW14                 0x2d

/****************for adapt cmpr*******************/
#define ISP_HAS_CMPR_ADAPT         0    //enable

//MIF PATH
#define MIPI_ADAPT_ENC_WMIF_PATH   0
#define MIPI_ADAPT_DEC_RMIF_PATH   1

#define MIPI2CMPR_ADAPT_SPLIT_PATH 0
#define MIPI2CMPR_ADAPT_PACK_PATH  1

//for adapt
#define ISP_ADAPT_CMPR_EN          1
#define ADAPT_CMPR_LOSSLESS        0
#define ADAPT_CMPR_WR_FRMRST_MODE  3
#define ADAPT_CMPR_RD_FRMRST_MODE  2
#define ADAPT_CMPR_REG_RDMA        0
#define ADAPT_DOL_MODE             1

#define CMPR_OFS(a)      ((a)-ADAPT_CMPR_SUB0_ENC_BASE)
#define CMPR2MIF_OFS(a)  ((a)-MIPI_TNR_WSUB0_META_BASE)
#define MIPI2CMPR_OFS(a) ((a)-MIPI_TNR_CMPR_SPLIT_BASE)

#define CMPR_CNTL_ADDR                             0xfe3b3000
#define CMPR_BASE_ADDR                             0xfe3b4000
#define MIPI_TOP_CSI2_CTRL0                        ((0x00c0  << 2) + 0xfe3b3000)
#define MIPI_TOP_ADAPT_DE_CTRL0                    ((0x00c1  << 2) + 0xfe3b3000)
#define MIPI_TOP_ADAPT_DE_CTRL1                    ((0x00c2  << 2) + 0xfe3b3000)
#define MIPI_TOP_ADAPT_DE_CTRL2                    ((0x00c3  << 2) + 0xfe3b3000)
#define MIPI_TOP_ADAPT_DE_CTRL3                    ((0x00c4  << 2) + 0xfe3b3000)
#define MIPI_TOP_TNR_DE_CTRL0                      ((0x00c5  << 2) + 0xfe3b3000)

#define MIPI_ADAPT_CMPR_SPLIT_CTRL                 ((0x0000  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_SPLIT_SIZE                 ((0x0001  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_SPLIT_FRMRST_CTRL          ((0x0002  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_SPLIT_FRMRST_DLY0          ((0x0003  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_SPLIT_FRMRST_DLY1          ((0x0004  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_SPLIT_FRMHOLD              ((0x0005  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_RO_SPLIT_STATS             ((0x0007  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_WSUB0_CTRL                 ((0x0008  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_WSUB0_IADDRS               ((0x0009  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_WSUB0_IADDRE               ((0x000a  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_PACK_CTRL                  ((0x0010  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_PACK_SIZE                  ((0x0011  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_PACK_FRMRST_CTRL           ((0x0012  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_PACK_FRMRST_DLY0           ((0x0013  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_PACK_FRMRST_DLY1           ((0x0014  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_PACK_FRMHOLD               ((0x0015  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_RO_PACK_STATS              ((0x0017  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_RSUB0_CTRL                 ((0x0018  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_RSUB0_IADDRS               ((0x0019  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_RSUB0_IADDRE               ((0x001a  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_INT_EN                     ((0x0020  << 2) + 0xfe3b4000)
#define MIPI_ADAPT_CMPR_RO_INT_STATUS              ((0x0021  << 2) + 0xfe3b4000)

#define AWSUB0_ISP_LOSS_CTRL                       ((0x0000  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FRAME_HOLD                 ((0x0001  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_GCLK_CTRL                  ((0x0002  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RO_CODEC_STATUS            ((0x0003  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_MISC                       ((0x0004  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_BASIS                      ((0x0008  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_PIC_SIZE                   ((0x0009  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_SLICE_SIZE                 ((0x000a  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_SLICE_SIZE_1               ((0x000b  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_SLICE_SIZE_2               ((0x000c  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_SLICE_SIZE_3               ((0x000d  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_PRESL_LAST_BITS            ((0x000e  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_PRESL_FIFO_LEVEL           ((0x000f  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_DEFAULT_VALUE              ((0x0010  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_DEFAULT_VALUE_1            ((0x0011  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_ERROR_COUNT_THD            ((0x0012  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_DEBUG                      ((0x0013  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_GLOBAL_PHASE_LUT           ((0x0014  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_GLOBAL_PHASE_LUT_1         ((0x0015  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_PHASE_LUT                  ((0x0016  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FLATNESS_0                 ((0x0017  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FLATNESS_1                 ((0x0018  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FLATNESS_TH                ((0x0019  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_PRED                       ((0x001a  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_PRED_TH                    ((0x001b  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_PRED_TH_1                  ((0x001c  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN0                ((0x001d  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN0_1              ((0x001e  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN0_2              ((0x001f  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN0_3              ((0x0020  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN0_4              ((0x0021  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN0_5              ((0x0022  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN0_6              ((0x0023  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN0_7              ((0x0024  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN0_8              ((0x0025  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN0_9              ((0x0026  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN1                ((0x0027  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN1_1              ((0x0028  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN1_2              ((0x0029  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN1_3              ((0x002a  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN1_4              ((0x002b  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN1_5              ((0x002c  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN1_6              ((0x002d  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN1_7              ((0x002e  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN1_8              ((0x002f  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN1_9              ((0x0030  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN2                ((0x0031  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN2_1              ((0x0032  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN2_2              ((0x0033  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN2_3              ((0x0034  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN2_4              ((0x0035  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN2_5              ((0x0036  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN2_6              ((0x0037  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN2_7              ((0x0038  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN2_8              ((0x0039  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_QP_MAP_CHN2_9              ((0x003a  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_GROUP_2                 ((0x003b  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_BUDGET_0                ((0x003c  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_BUDGET_1                ((0x003d  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_BUDGET_2                ((0x003e  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_BUDGET_3                ((0x003f  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_BUDGET_4                ((0x0040  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_BUDGET_5                ((0x0041  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_BUDGET_6                ((0x0042  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_QP_MARGIN               ((0x0043  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_QP_MARGIN_1             ((0x0044  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_QP_MARGIN_2             ((0x0045  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_QP_MARGIN_3             ((0x0046  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_QP_MARGIN_4             ((0x0047  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RC_QP_MARGIN_5             ((0x0048  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FLATNESS_ADJ0              ((0x0049  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FLATNESS_ADJ1              ((0x004a  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FIFO_THD_0                 ((0x004b  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FIFO_THD_1                 ((0x004c  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FIFO_THD_2                 ((0x004d  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FIFO_AVG                   ((0x004e  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_FIFO_DLT                   ((0x004f  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_BITSGAP_THD_0              ((0x0050  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_BITSGAP_THD_1              ((0x0051  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_ICH                        ((0x0052  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_ICH_UPTH                   ((0x0053  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_LUMA                       ((0x0054  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_LUMA_TH                    ((0x0055  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_ACCUM_OFSET_0              ((0x0056  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_ACCUM_OFSET_1              ((0x0057  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_ACCUM_OFSET_2              ((0x0058  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_ACCUM_OFSET_3              ((0x0059  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_DERIVA_ADJ                 ((0x005a  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_DERIVA_ADJ_1               ((0x005b  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_DERIVA_ADJ_2               ((0x005c  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_DERIVA_ADJ_3               ((0x005d  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_WDR_LINE_DELAY             ((0x005e  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_STATS_RAM_MODE             ((0x005f  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_STATS_RAM_ADDR             ((0x0060  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_RO_STATS_RAM_DATA          ((0x0061  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_MIF_CTRL                   ((0x0070  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_MIF_QOS                    ((0x0071  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_MIF_URGENT                 ((0x0072  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_MIF_MISC                   ((0x0073  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_MIF_BADDR                  ((0x0074  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_MIF_BADDR1                 ((0x0075  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_MIF_FIFO_CTRL              ((0x0077  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_MIF_FRM_HOLD               ((0x0078  << 2) + 0xfe3b4400)
#define AWSUB0_ISP_LOSS_MIF_RO_STATS               ((0x0079  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_CTRL                       ((0x0080  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FRAME_HOLD                 ((0x0081  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_GCLK_CTRL                  ((0x0082  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RO_CODEC_STATUS            ((0x0083  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_MISC                       ((0x0084  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_BASIS                      ((0x0088  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_PIC_SIZE                   ((0x0089  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_SLICE_SIZE                 ((0x008a  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_SLICE_SIZE_1               ((0x008b  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_SLICE_SIZE_2               ((0x008c  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_SLICE_SIZE_3               ((0x008d  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_PRESL_LAST_BITS            ((0x008e  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_PRESL_FIFO_LEVEL           ((0x008f  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_DEFAULT_VALUE              ((0x0090  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_DEFAULT_VALUE_1            ((0x0091  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_ERROR_COUNT_THD            ((0x0092  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_DEBUG                      ((0x0093  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_GLOBAL_PHASE_LUT           ((0x0094  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_GLOBAL_PHASE_LUT_1         ((0x0095  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_PHASE_LUT                  ((0x0096  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FLATNESS_0                 ((0x0097  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FLATNESS_1                 ((0x0098  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FLATNESS_TH                ((0x0099  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_PRED                       ((0x009a  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_PRED_TH                    ((0x009b  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_PRED_TH_1                  ((0x009c  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN0                ((0x009d  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN0_1              ((0x009e  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN0_2              ((0x009f  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN0_3              ((0x00a0  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN0_4              ((0x00a1  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN0_5              ((0x00a2  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN0_6              ((0x00a3  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN0_7              ((0x00a4  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN0_8              ((0x00a5  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN0_9              ((0x00a6  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN1                ((0x00a7  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN1_1              ((0x00a8  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN1_2              ((0x00a9  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN1_3              ((0x00aa  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN1_4              ((0x00ab  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN1_5              ((0x00ac  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN1_6              ((0x00ad  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN1_7              ((0x00ae  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN1_8              ((0x00af  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN1_9              ((0x00b0  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN2                ((0x00b1  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN2_1              ((0x00b2  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN2_2              ((0x00b3  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN2_3              ((0x00b4  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN2_4              ((0x00b5  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN2_5              ((0x00b6  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN2_6              ((0x00b7  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN2_7              ((0x00b8  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN2_8              ((0x00b9  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_QP_MAP_CHN2_9              ((0x00ba  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_GROUP_2                 ((0x00bb  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_BUDGET_0                ((0x00bc  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_BUDGET_1                ((0x00bd  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_BUDGET_2                ((0x00be  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_BUDGET_3                ((0x00bf  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_BUDGET_4                ((0x00c0  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_BUDGET_5                ((0x00c1  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_BUDGET_6                ((0x00c2  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_QP_MARGIN               ((0x00c3  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_QP_MARGIN_1             ((0x00c4  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_QP_MARGIN_2             ((0x00c5  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_QP_MARGIN_3             ((0x00c6  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_QP_MARGIN_4             ((0x00c7  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RC_QP_MARGIN_5             ((0x00c8  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FLATNESS_ADJ0              ((0x00c9  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FLATNESS_ADJ1              ((0x00ca  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FIFO_THD_0                 ((0x00cb  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FIFO_THD_1                 ((0x00cc  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FIFO_THD_2                 ((0x00cd  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FIFO_AVG                   ((0x00ce  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_FIFO_DLT                   ((0x00cf  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_BITSGAP_THD_0              ((0x00d0  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_BITSGAP_THD_1              ((0x00d1  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_ICH                        ((0x00d2  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_ICH_UPTH                   ((0x00d3  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_LUMA                       ((0x00d4  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_LUMA_TH                    ((0x00d5  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_ACCUM_OFSET_0              ((0x00d6  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_ACCUM_OFSET_1              ((0x00d7  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_ACCUM_OFSET_2              ((0x00d8  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_ACCUM_OFSET_3              ((0x00d9  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_DERIVA_ADJ                 ((0x00da  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_DERIVA_ADJ_1               ((0x00db  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_DERIVA_ADJ_2               ((0x00dc  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_DERIVA_ADJ_3               ((0x00dd  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_WDR_LINE_DELAY             ((0x00de  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_STATS_RAM_MODE             ((0x00df  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_STATS_RAM_ADDR             ((0x00e0  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_RO_STATS_RAM_DATA          ((0x00e1  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_MIF_CTRL                   ((0x00f0  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_MIF_QOS                    ((0x00f1  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_MIF_URGENT                 ((0x00f2  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_MIF_MISC                   ((0x00f3  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_MIF_BADDR                  ((0x00f4  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_MIF_BADDR1                 ((0x00f5  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_MIF_FIFO_CTRL              ((0x00f7  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_MIF_FRM_HOLD               ((0x00f8  << 2) + 0xfe3b4400)
#define ARSUB0_ISP_LOSS_MIF_RO_STATS               ((0x00f9  << 2) + 0xfe3b4400)

#define MIPI_TNR_CMPR_SPLIT_CTRL                   ((0x0000  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_SPLIT_SIZE                   ((0x0001  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_SPLIT_FRMRST_CTRL            ((0x0002  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_SPLIT_FRMRST_DLY0            ((0x0003  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_SPLIT_FRMRST_DLY1            ((0x0004  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_SPLIT_FRMHOLD                ((0x0005  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_RO_SPLIT_STATS               ((0x0007  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_WSUB0_CTRL                   ((0x0008  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_WSUB0_IADDRS                 ((0x0009  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_WSUB0_IADDRE                 ((0x000a  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_WSUB1_CTRL                   ((0x000b  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_WSUB1_IADDRS                 ((0x000c  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_WSUB1_IADDRE                 ((0x000d  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_PACK_CTRL                    ((0x0010  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_PACK_SIZE                    ((0x0011  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_PACK_FRMRST_CTRL             ((0x0012  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_PACK_FRMRST_DLY0             ((0x0013  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_PACK_FRMRST_DLY1             ((0x0014  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_PACK_FRMHOLD                 ((0x0015  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_RO_PACK_STATS                ((0x0017  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_RSUB0_CTRL                   ((0x0018  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_RSUB0_IADDRS                 ((0x0019  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_RSUB0_IADDRE                 ((0x001a  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_RSUB1_CTRL                   ((0x001b  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_RSUB1_IADDRS                 ((0x001c  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_RSUB1_IADDRE                 ((0x001d  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_INT_EN                       ((0x0020  << 2) + 0xfe3b4800)
#define MIPI_TNR_CMPR_RO_INT_STATUS                ((0x0021  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB0_MIF_CTRL               ((0x0080  << 2) + 0xfe3b4800)
#define ISP_TNR_META_WSUB0_MIF_QOS                 ((0x0081  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB0_MIF_URGENT             ((0x0082  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB0_MIF_MISC               ((0x0083  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB0_MIF_BADDR              ((0x0084  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB0_MIF_BADDR1             ((0x0085  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB0_MIF_SIZE               ((0x0086  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB0_MIF_FIFO               ((0x0087  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB0_MIF_FRMHOLD            ((0x0088  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB0_MIF_RO_STATS           ((0x0089  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB1_MIF_CTRL               ((0x0090  << 2) + 0xfe3b4800)
#define ISP_TNR_META_WSUB1_MIF_QOS                 ((0x0091  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB1_MIF_URGENT             ((0x0092  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB1_MIF_MISC               ((0x0093  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB1_MIF_BADDR              ((0x0094  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB1_MIF_BADDR1             ((0x0095  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB1_MIF_SIZE               ((0x0096  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB1_MIF_FIFO               ((0x0097  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB1_MIF_FRMHOLD            ((0x0098  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_WSUB1_MIF_RO_STATS           ((0x0099  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB0_MIF_CTRL               ((0x00a0  << 2) + 0xfe3b4800)
#define ISP_TNR_META_RSUB0_MIF_QOS                 ((0x00a1  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB0_MIF_URGENT             ((0x00a2  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB0_MIF_MISC               ((0x00a3  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB0_MIF_BADDR              ((0x00a4  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB0_MIF_BADDR1             ((0x00a5  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB0_MIF_SIZE               ((0x00a6  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB0_MIF_FIFO               ((0x00a7  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB0_MIF_FRMHOLD            ((0x00a8  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB0_MIF_RO_STATS           ((0x00a9  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB1_MIF_CTRL               ((0x00b0  << 2) + 0xfe3b4800)
#define ISP_TNR_META_RSUB1_MIF_QOS                 ((0x00b1  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB1_MIF_URGENT             ((0x00b2  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB1_MIF_MISC               ((0x00b3  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB1_MIF_BADDR              ((0x00b4  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB1_MIF_BADDR1             ((0x00b5  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB1_MIF_SIZE               ((0x00b6  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB1_MIF_FIFO               ((0x00b7  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB1_MIF_FRMHOLD            ((0x00b8  << 2) + 0xfe3b4800)
#define MIPI_TNR_META_RSUB1_MIF_RO_STATS           ((0x00b9  << 2) + 0xfe3b4800)
#define TWSUB0_ISP_LOSS_CTRL                       ((0x0000  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FRAME_HOLD                 ((0x0001  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_GCLK_CTRL                  ((0x0002  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RO_CODEC_STATUS            ((0x0003  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_MISC                       ((0x0004  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_BASIS                      ((0x0008  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_PIC_SIZE                   ((0x0009  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_SLICE_SIZE                 ((0x000a  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_SLICE_SIZE_1               ((0x000b  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_SLICE_SIZE_2               ((0x000c  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_SLICE_SIZE_3               ((0x000d  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_PRESL_LAST_BITS            ((0x000e  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_PRESL_FIFO_LEVEL           ((0x000f  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_DEFAULT_VALUE              ((0x0010  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_DEFAULT_VALUE_1            ((0x0011  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_ERROR_COUNT_THD            ((0x0012  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_DEBUG                      ((0x0013  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_GLOBAL_PHASE_LUT           ((0x0014  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_GLOBAL_PHASE_LUT_1         ((0x0015  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_PHASE_LUT                  ((0x0016  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FLATNESS_0                 ((0x0017  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FLATNESS_1                 ((0x0018  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FLATNESS_TH                ((0x0019  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_PRED                       ((0x001a  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_PRED_TH                    ((0x001b  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_PRED_TH_1                  ((0x001c  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN0                ((0x001d  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN0_1              ((0x001e  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN0_2              ((0x001f  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN0_3              ((0x0020  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN0_4              ((0x0021  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN0_5              ((0x0022  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN0_6              ((0x0023  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN0_7              ((0x0024  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN0_8              ((0x0025  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN0_9              ((0x0026  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN1                ((0x0027  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN1_1              ((0x0028  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN1_2              ((0x0029  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN1_3              ((0x002a  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN1_4              ((0x002b  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN1_5              ((0x002c  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN1_6              ((0x002d  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN1_7              ((0x002e  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN1_8              ((0x002f  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN1_9              ((0x0030  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN2                ((0x0031  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN2_1              ((0x0032  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN2_2              ((0x0033  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN2_3              ((0x0034  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN2_4              ((0x0035  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN2_5              ((0x0036  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN2_6              ((0x0037  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN2_7              ((0x0038  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN2_8              ((0x0039  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_QP_MAP_CHN2_9              ((0x003a  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_GROUP_2                 ((0x003b  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_BUDGET_0                ((0x003c  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_BUDGET_1                ((0x003d  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_BUDGET_2                ((0x003e  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_BUDGET_3                ((0x003f  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_BUDGET_4                ((0x0040  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_BUDGET_5                ((0x0041  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_BUDGET_6                ((0x0042  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_QP_MARGIN               ((0x0043  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_QP_MARGIN_1             ((0x0044  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_QP_MARGIN_2             ((0x0045  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_QP_MARGIN_3             ((0x0046  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_QP_MARGIN_4             ((0x0047  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RC_QP_MARGIN_5             ((0x0048  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FLATNESS_ADJ0              ((0x0049  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FLATNESS_ADJ1              ((0x004a  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FIFO_THD_0                 ((0x004b  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FIFO_THD_1                 ((0x004c  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FIFO_THD_2                 ((0x004d  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FIFO_AVG                   ((0x004e  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_FIFO_DLT                   ((0x004f  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_BITSGAP_THD_0              ((0x0050  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_BITSGAP_THD_1              ((0x0051  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_ICH                        ((0x0052  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_ICH_UPTH                   ((0x0053  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_LUMA                       ((0x0054  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_LUMA_TH                    ((0x0055  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_ACCUM_OFSET_0              ((0x0056  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_ACCUM_OFSET_1              ((0x0057  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_ACCUM_OFSET_2              ((0x0058  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_ACCUM_OFSET_3              ((0x0059  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_DERIVA_ADJ                 ((0x005a  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_DERIVA_ADJ_1               ((0x005b  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_DERIVA_ADJ_2               ((0x005c  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_DERIVA_ADJ_3               ((0x005d  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_WDR_LINE_DELAY             ((0x005e  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_STATS_RAM_MODE             ((0x005f  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_STATS_RAM_ADDR             ((0x0060  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_RO_STATS_RAM_DATA          ((0x0061  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_MIF_CTRL                   ((0x0070  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_MIF_QOS                    ((0x0071  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_MIF_URGENT                 ((0x0072  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_MIF_MISC                   ((0x0073  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_MIF_BADDR                  ((0x0074  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_MIF_BADDR1                 ((0x0075  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_MIF_FIFO_CTRL              ((0x0077  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_MIF_FRM_HOLD               ((0x0078  << 2) + 0xfe3b4c00)
#define TWSUB0_ISP_LOSS_MIF_RO_STATS               ((0x0079  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_CTRL                       ((0x0080  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FRAME_HOLD                 ((0x0081  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_GCLK_CTRL                  ((0x0082  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RO_CODEC_STATUS            ((0x0083  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_MISC                       ((0x0084  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_BASIS                      ((0x0088  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_PIC_SIZE                   ((0x0089  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_SLICE_SIZE                 ((0x008a  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_SLICE_SIZE_1               ((0x008b  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_SLICE_SIZE_2               ((0x008c  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_SLICE_SIZE_3               ((0x008d  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_PRESL_LAST_BITS            ((0x008e  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_PRESL_FIFO_LEVEL           ((0x008f  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_DEFAULT_VALUE              ((0x0090  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_DEFAULT_VALUE_1            ((0x0091  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_ERROR_COUNT_THD            ((0x0092  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_DEBUG                      ((0x0093  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_GLOBAL_PHASE_LUT           ((0x0094  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_GLOBAL_PHASE_LUT_1         ((0x0095  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_PHASE_LUT                  ((0x0096  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FLATNESS_0                 ((0x0097  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FLATNESS_1                 ((0x0098  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FLATNESS_TH                ((0x0099  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_PRED                       ((0x009a  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_PRED_TH                    ((0x009b  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_PRED_TH_1                  ((0x009c  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN0                ((0x009d  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN0_1              ((0x009e  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN0_2              ((0x009f  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN0_3              ((0x00a0  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN0_4              ((0x00a1  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN0_5              ((0x00a2  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN0_6              ((0x00a3  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN0_7              ((0x00a4  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN0_8              ((0x00a5  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN0_9              ((0x00a6  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN1                ((0x00a7  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN1_1              ((0x00a8  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN1_2              ((0x00a9  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN1_3              ((0x00aa  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN1_4              ((0x00ab  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN1_5              ((0x00ac  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN1_6              ((0x00ad  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN1_7              ((0x00ae  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN1_8              ((0x00af  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN1_9              ((0x00b0  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN2                ((0x00b1  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN2_1              ((0x00b2  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN2_2              ((0x00b3  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN2_3              ((0x00b4  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN2_4              ((0x00b5  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN2_5              ((0x00b6  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN2_6              ((0x00b7  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN2_7              ((0x00b8  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN2_8              ((0x00b9  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_QP_MAP_CHN2_9              ((0x00ba  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_GROUP_2                 ((0x00bb  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_BUDGET_0                ((0x00bc  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_BUDGET_1                ((0x00bd  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_BUDGET_2                ((0x00be  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_BUDGET_3                ((0x00bf  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_BUDGET_4                ((0x00c0  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_BUDGET_5                ((0x00c1  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_BUDGET_6                ((0x00c2  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_QP_MARGIN               ((0x00c3  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_QP_MARGIN_1             ((0x00c4  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_QP_MARGIN_2             ((0x00c5  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_QP_MARGIN_3             ((0x00c6  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_QP_MARGIN_4             ((0x00c7  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RC_QP_MARGIN_5             ((0x00c8  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FLATNESS_ADJ0              ((0x00c9  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FLATNESS_ADJ1              ((0x00ca  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FIFO_THD_0                 ((0x00cb  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FIFO_THD_1                 ((0x00cc  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FIFO_THD_2                 ((0x00cd  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FIFO_AVG                   ((0x00ce  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_FIFO_DLT                   ((0x00cf  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_BITSGAP_THD_0              ((0x00d0  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_BITSGAP_THD_1              ((0x00d1  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_ICH                        ((0x00d2  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_ICH_UPTH                   ((0x00d3  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_LUMA                       ((0x00d4  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_LUMA_TH                    ((0x00d5  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_ACCUM_OFSET_0              ((0x00d6  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_ACCUM_OFSET_1              ((0x00d7  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_ACCUM_OFSET_2              ((0x00d8  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_ACCUM_OFSET_3              ((0x00d9  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_DERIVA_ADJ                 ((0x00da  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_DERIVA_ADJ_1               ((0x00db  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_DERIVA_ADJ_2               ((0x00dc  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_DERIVA_ADJ_3               ((0x00dd  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_WDR_LINE_DELAY             ((0x00de  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_STATS_RAM_MODE             ((0x00df  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_STATS_RAM_ADDR             ((0x00e0  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_RO_STATS_RAM_DATA          ((0x00e1  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_MIF_CTRL                   ((0x00f0  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_MIF_QOS                    ((0x00f1  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_MIF_URGENT                 ((0x00f2  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_MIF_MISC                   ((0x00f3  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_MIF_BADDR                  ((0x00f4  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_MIF_BADDR1                 ((0x00f5  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_MIF_FIFO_CTRL              ((0x00f7  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_MIF_FRM_HOLD               ((0x00f8  << 2) + 0xfe3b4c00)
#define TRSUB0_ISP_LOSS_MIF_RO_STATS               ((0x00f9  << 2) + 0xfe3b4c00)

#define ADAPT_CMPR_SUB0_BASE      (AWSUB0_ISP_LOSS_CTRL)
#define ADAPT_CMPR_SUB0_ENC_BASE  (ADAPT_CMPR_SUB0_BASE)
#define ADAPT_CMPR_SUB0_WMIF_BASE (ADAPT_CMPR_SUB0_ENC_BASE + 0x70*4)
#define ADAPT_CMPR_SUB0_DEC_BASE  (ADAPT_CMPR_SUB0_ENC_BASE + 0x80*4)
#define ADAPT_CMPR_SUB0_RMIF_BASE (ADAPT_CMPR_SUB0_DEC_BASE + 0x70*4)

#define MIPI_ADAPT_CMPR_SPLIT_BASE  (MIPI_ADAPT_CMPR_SPLIT_CTRL)
#define MIPI_ADAPT_CMPR_PACK_BASE   (MIPI_ADAPT_CMPR_PACK_CTRL )

#define MIPI_TNR_CMPR_SPLIT_BASE    (MIPI_TNR_CMPR_SPLIT_CTRL  )
#define MIPI_TNR_CMPR_PACK_BASE     (MIPI_TNR_CMPR_PACK_CTRL   )
#define MIPI_TNR_WSUB0_META_BASE    (MIPI_TNR_META_WSUB0_MIF_CTRL)
#define MIPI_TNR_WSUB1_META_BASE    (MIPI_TNR_META_WSUB1_MIF_CTRL)
#define MIPI_TNR_RSUB0_META_BASE    (MIPI_TNR_META_RSUB0_MIF_CTRL)
#define MIPI_TNR_RSUB1_META_BASE    (MIPI_TNR_META_RSUB1_MIF_CTRL)

typedef struct {
  int reg_pic_xsize;
  int reg_pic_ysize;
  //cmpr
  int reg_src_bit_depth;
  int reg_lossless_en;
  int reg_xphase_ofst;
  int reg_yphase_ofst;
  int reg_wdr_mode_en;
  int reg_wdr_delay_line;
  //calulate by FW
  int reg_ratio_bppx16;
  int reg_error_count_thd;
  int reg_flatness_qp_thresh;
  int reg_flatness_q_err_thresh;
  int reg_default_value[3];
  int reg_flatness_det_thresh;
  int reg_flatness_det_thresh_max;
  int reg_flatness_accum_thresh;
  int reg_pred_err0;
  int reg_pred_flat_thd;
  int reg_pred_drt5flat_thd;
  int reg_pred_drt5_thrd;
  int reg_pixel_luma_adj_th[2];
  int reg_rc_fifo_margin_dlt[6];
  int reg_rc_fifo_qpx2_margin_dlt[4];
  int reg_rc_master_qpx2_max;
  int reg_rc_master_qpx2_min;
  int reg_rc_group_y;
  int reg_slcln_ratio;
  int reg_accum_ofset_en;
  int reg_normalize_idx_ratio;
  int reg_lut_budget2qp[3][40];
} sCMPRCORE_Param;

typedef struct {
  int reg_pic_hsize;
  int reg_pic_vsize;
  int reg_canvas_id;
  int reg_burst_len;
  int reg_dol_mode;            //only for enc/dec mif
  int reg_mif_dbl_baddr_init;
  int reg_mif_dbl_baddr_en;
  int reg_mif_baddr0;
  int reg_mif_baddr1;
} sCMPR2MIF_Param;

typedef struct {
   int reg_pic_hsize;
   int reg_pic_vsize;
   int reg_rdma_trigger_mode;
   int reg_pack_skip_flag_init; //for pack only, skip flag for pack, 1 to skip pack axi cmd for current frame
   int reg_pack_skip_auto_ctrl; //for pack only, auto clear the skip flag
   int reg_pack_keep_axicmd_order; //for TNR pack only
   int reg_tnr_temp3_en;        //only for TNR
   int reg_frmrst_mode;
   int reg_frmrst_dlys4gen;     //only for reg_frmrst_mode=2
   int reg_frmrst_dlys4abort;   //only for reg_frmrst_mode=2
   int reg_raw67_ext;           //0 or 1, only for adapt
   int reg_raw_mode;            //adapt: 3bit-> raw6/7/8/10/12/14;  tnr: 1bit->12/16
   int reg_iaddr_bgn[2];
   int line_stride;
} sMIPI2CMPR_Param;

typedef enum {
    FRONTEND0_IO,
    FRONTEND1_IO,
    FRONTEND2_IO,
    FRONTEND3_IO,
    RD_IO,
    PIXEL_IO,
    ALIGN_IO,
    MISC_IO,
    CMPR_CNTL_IO,
    CMPR_IO
} adap_io_type_t;

#define CAM_LAST                   18
#define CAM_CURRENT                26
#define CAM_NEXT                   28
#define CAM_NEXT_NEXT              30

#define FRONT0_WR_DONE 28
#define FRONT1_WR_DONE 2
#define FRONT2_WR_DONE 8
#define FRONT3_WR_DONE 14
#define ALIGN_FRAME_END 9
#define READ0_RD_DONE 21
#define READ1_RD_DONE 15
#define CAMERA_NUM        FIRMWARE_CONTEXT_NUMBER
#define DDR_BUF_SIZE      5
#define CAMERA_QUEUE_NUM  (CAMERA_NUM * DDR_BUF_SIZE)
#define DOL_BUF_SIZE      6

typedef enum {
    FRAME_READY,
    FRAME_NOREADY,
} frame_status_t;

typedef enum {
    DDR_MODE,
    DIR_MODE,
    DCAM_MODE,
    DOL_MODE,
    DCAM_DOL_MODE,
} adap_mode_t;

typedef enum {
    CAM_DIS,
    CAM_EN,
    DUAL_CAM_EN,
    TRIPLE_CAM_EN
} cam_mode_t;

typedef enum {
    ADAP0_PATH,
    ADAP1_PATH,
    ADAP2_PATH,
    ADAP3_PATH,
} adap_chan_t;

typedef enum {
    CAM0_ACT,
    CAM1_ACT,
    CAM2_ACT,
    CAM3_ACT,
    CAMS_MAX,
} cam_num_t;

typedef enum {
    PATH0,
    PATH1,
    PATH2,
    PATH3,
} adap_path_t;

typedef struct {
    uint32_t width;
    uint32_t height;
} adap_img_t;

typedef enum {
    AM_RAW6 = 1,
    AM_RAW7,
    AM_RAW8,
    AM_RAW10,
    AM_RAW12,
    AM_RAW14,
} img_fmt_t;

typedef enum {
    DOL_NON = 0,
    DOL_VC,
    DOL_LINEINFO,
    DOL_YUV,
} dol_type_t;

typedef enum {
    FTE_DONE = 0,
    FTE0_DONE,
    FTE1_DONE,
} dol_state_t;

typedef struct exp_offset {
    int long_offset;
    int short_offset;
    int offset_x;
    int offset_y;
} exp_offset_t;

struct am_adap {
    struct device_node *of_node;
    struct platform_device *p_dev;
    struct resource reg;
    void __iomem *base_addr;
    int f_end_irq;
    int rd_irq;
    unsigned int adap_buf_size[CAMERA_NUM];
    int f_fifo;
    int f_adap;

    uint32_t write_frame_ptr;
    uint32_t read_frame_ptr;

    int frame_state;
    struct task_struct *kadap_stream;
    wait_queue_head_t frame_wq;
    spinlock_t reg_lock;
};

struct am_adap_info {
    adap_path_t path;
    adap_io_type_t frontend;
    adap_mode_t mode;
    adap_img_t img;
    int fmt;
    dol_type_t type;
    exp_offset_t offset;
    uint32_t align_width;
};

struct adaptfe_param {
    int fe_sel;
    int fe_work_mode;
    int fe_mem_x_start;
    int fe_mem_x_end;
    int fe_mem_y_start;
    int fe_mem_y_end;
    int fe_isp_x_start;
    int fe_isp_x_end;
    int fe_isp_y_start;
    int fe_isp_y_end;
    int fe_mem_ping_addr;
    int fe_mem_pong_addr;
    int fe_mem_other_addr;
    int fe_mem_line_stride;
    int fe_mem_line_minbyte;
    int fe_int_mask;
};

struct adaptrd_param {
    int rd_work_mode;
    int rd_mem_ping_addr;
    int rd_mem_pong_addr;
    int rd_mem_line_stride;
    int rd_mem_line_size;
    int rd_mem_line_number;
};

struct adaptpixel_param {
    int pixel_work_mode;
    int pixel_data_type;
    int pixel_isp_x_start;
    int pixel_isp_x_end;
    int pixel_line_size;
    int pixel_pixel_number;
    int pixel_en_0;
    int pixel_en_1;
};

struct adaptalig_param {
    int alig_work_mode;
    int alig_hsize;
    int alig_vsize;
};

//------------
void inline adap_wr_bit(unsigned int adr,
        adap_io_type_t io_type, unsigned int val,
        unsigned int start, unsigned int len);
void inline adap_write(int addr, adap_io_type_t io_type, uint32_t val);
void inline adap_read(int addr, adap_io_type_t io_type, uint32_t *val);

void cmpr_param_fw_calc(sCMPRCORE_Param *prm);
void cmpr_param_fw_lut_init(sCMPRCORE_Param *prm);
void enable_mipi_cmpr_adapt(int enable);
void init_mipi_cmprcore (int addrbase,sCMPRCORE_Param *prm);
void init_mipi_cmpr2mif (int cmpr2mif_path,  int addrbase,sCMPR2MIF_Param *prm);
void init_mipi_mipi2cmpr(int mipi2cmpr_path, int addrbase,sMIPI2CMPR_Param *prm);
void set_mipi_mipi2cmpr_sw_frmbgn(int addrbase);
void set_mipi_cmpr_rdma_mode (int is_adpat,int enable);
int aml_adap_decmpr_init(int adapt_cmpr_en, int w, int h, int fmt);

int am_adap_parse_dt(struct device_node *node);
void am_adap_deinit_parse_dt(void);
int am_adap_init(uint8_t channel);
int am_adap_start(uint8_t channel, uint8_t dcam);
int am_adap_reset(uint8_t channel);
int am_adap_deinit(uint8_t channel);
void am_adap_set_info(struct am_adap_info *info);
int get_fte1_flag(void);
int am_adap_get_depth(uint8_t channel);
void am_adap_camid_update(uint8_t flag, uint8_t channel);
extern int32_t system_timer_usleep( uint32_t usec );
extern int camera_notify( uint notification, void *arg);

void adap_read_ext(int addr, adap_io_type_t io_type, uint32_t *val);
void adapt_set_virtcam(void);
int bypass_init(void);

#endif

