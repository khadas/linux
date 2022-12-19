/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
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

#ifndef __AML_P1_ADAPTER_HW_H__
#define __AML_P1_ADAPTER_HW_H__

#ifdef T7C_CHIP
#define ADAPT_BASE                      0xff3b0000

#define FRONTEND_BASE                   0x0
#define FRONTEND1_BASE                  0x400
#define FRONTEND2_BASE                  0x800
#define FRONTEND3_BASE                  0xc00
#define RD_BASE                         0x1000
#define PIXEL_BASE                      0x1000
#define ALIGN_BASE                      0x1000
#define MISC_BASE                       0x1000

#define MIPI_TOP_CSI2_CTRL0             (0x00c0 << 2)

#define ISP_TOP_OFFSET                  0x3000
#define PROC_BASE                       ISP_TOP_OFFSET
#else
#define ADAPT_BASE                      0xff018000

#define FRONTEND_BASE                   0x3000
#define RD_BASE                         0x5000
#define PIXEL_BASE                      0x5000
#define ALIGN_BASE                      0x5000
#define MISC_BASE                       0x5000
#define PROC_BASE                       0x0
#endif

#define CSI2_CLK_RESET                  (0x00<<2)
#define CSI2_GEN_CTRL0                  (0x01<<2)
#define CSI2_GEN_CTRL1                  (0x02<<2)
#define CSI2_X_START_END_ISP            (0x03<<2)
#define CSI2_Y_START_END_ISP            (0x04<<2)
#define CSI2_X_START_END_MEM            (0x05<<2)
#define CSI2_Y_START_END_MEM            (0x06<<2)
#define CSI2_VC_MODE                    (0x07<<2)
#define CSI2_VC_MODE2_MATCH_MASK_A_L    (0x08<<2)
#define CSI2_VC_MODE2_MATCH_MASK_A_H    (0x09<<2)
#define CSI2_VC_MODE2_MATCH_A_L         (0x0a<<2)
#define CSI2_VC_MODE2_MATCH_A_H         (0x0b<<2)
#define CSI2_VC_MODE2_MATCH_B_L         (0x0c<<2)
#define CSI2_VC_MODE2_MATCH_B_H         (0x0d<<2)
#define CSI2_DDR_START_PIX              (0x0e<<2)
#define CSI2_DDR_START_PIX_ALT          (0x0f<<2)
#define CSI2_DDR_STRIDE_PIX             (0x10<<2)
#define CSI2_DDR_START_OTHER            (0x11<<2)
#define CSI2_DDR_START_OTHER_ALT        (0x12<<2)
#define CSI2_DDR_MAX_BYTES_OTHER        (0x13<<2)
#define CSI2_INTERRUPT_CTRL_STAT        (0x14<<2)
#define CSI2_VC_MODE2_MATCH_MASK_B_L    (0X15<<2)
#define CSI2_VC_MODE2_MATCH_MASK_B_H    (0X16<<2)
#define CSI2_DDR_LOOP_LINES_PIX         (0X17<<2)

#define CSI2_GEN_STAT0                  (0x20<<2)
#define CSI2_ERR_STAT0                  (0x21<<2)
#define CSI2_PIC_SIZE_STAT              (0x22<<2)
#define CSI2_DDR_WPTR_STAT_PIX          (0x23<<2)
#define CSI2_DDR_WPTR_STAT_OTHER        (0x24<<2)
#define CSI2_STAT_MEM_0                 (0x25<<2)
#define CSI2_STAT_MEM_1                 (0x26<<2)
#define CSI2_STAT_MEM_2                 (0x27<<2)
#define CSI2_STAT_GEN_SHORT_08          (0x28<<2)
#define CSI2_STAT_GEN_SHORT_09          (0x29<<2)
#define CSI2_STAT_GEN_SHORT_0A          (0x2a<<2)
#define CSI2_STAT_GEN_SHORT_0B          (0x2b<<2)
#define CSI2_STAT_GEN_SHORT_0C          (0x2c<<2)
#define CSI2_STAT_GEN_SHORT_0D          (0x2d<<2)
#define CSI2_STAT_GEN_SHORT_0E          (0x2e<<2)
#define CSI2_STAT_GEN_SHORT_0F          (0x2f<<2)
#define CSI2_STAT_TYPE_RCVD_L           (0x30<<2)
#define CSI2_STAT_TYPE_RCVD_H           (0x31<<2)
#define CSI2_DDR_START_PIX_B            (0x32<<2)
#define CSI2_DDR_START_PIX_B_ALT        (0x33<<2)
#define CSI2_DDR_STRIDE_PIX_B           (0x34<<2)
#define CSI2_DDR_WPTR_STAT_PIX_B        (0x35<<2)
#define CSI2_STAT_MEM_3                 (0x36<<2)
#define CSI2_AXI_QOS_CNTL0              (0x37<<2)
#define CSI2_AXI_QOS_CNTL1              (0x38<<2)
#define CSI2_AXI_UGT_CNTL0              (0x39<<2)
#define CSI2_AXI_QOS_ST                 (0x3a<<2)
#define CSI2_LINE_SUP_CNTL0             (0x3b<<2)
#define CSI2_LINE_SUP_ST                (0x3c<<2)

#define CSI2_EMB_DEC_CTRL0              (0x40<<2)
#define CSI2_EMB_DEC_CTRL1              (0x41<<2)
#define CSI2_EMB_DEC_CTRL2              (0x42<<2)
#define CSI2_EMB_DEC_CTRL3              (0x43<<2)
#define CSI2_EMB_DEC_CTRL4              (0x44<<2)
#define CSI2_EMB_DEC_STAT0              (0x48<<2)
#define CSI2_EMB_DEC_STAT1              (0x49<<2)
#define CSI2_EMB_DEC_STAT2              (0x4a<<2)

#define MIPI_ADAPT_DDR_RD0_CNTL0        (0x00<<2)
#define MIPI_ADAPT_DDR_RD0_CNTL1        (0x01<<2)
#define MIPI_ADAPT_DDR_RD0_CNTL2        (0x02<<2)
#define MIPI_ADAPT_DDR_RD0_CNTL3        (0x03<<2)
#define MIPI_ADAPT_DDR_RD0_CNTL4        (0x04<<2)
#define MIPI_ADAPT_DDR_RD0_CNTL5        (0x05<<2)
#define MIPI_ADAPT_DDR_RD0_CNTL6        (0x06<<2)
#define MIPI_ADAPT_LBUF_0_UGT_CNTL0     (0x07<<2)
#define MIPI_ADAPT_LBUF_0_UGT_CNTL1     (0x08<<2)
#define MIPI_ADAPT_DDR_RD0_ST0          (0x0a<<2)
#define MIPI_ADAPT_DDR_RD0_ST1          (0x0b<<2)
#define MIPI_ADAPT_DDR_RD0_ST2          (0x0c<<2)
#define MIPI_ADAPT_LBUF_0_UGT_ST        (0x0d<<2)
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

#define MIPI_ADAPT_PIXEL0_CNTL0         (0x20<<2)
#define MIPI_ADAPT_PIXEL0_CNTL1         (0x21<<2)
#define MIPI_ADAPT_PIXEL0_CNTL2         (0x22<<2)
#define MIPI_ADAPT_PIXEL0_CNTL3         (0x23<<2)
#define MIPI_ADAPT_PIXEL0_ST0           (0x2a<<2)
#define MIPI_ADAPT_PIXEL0_ST1           (0x2b<<2)
#define MIPI_ADAPT_PIXEL1_CNTL0              (0x0030  << 2)
#define MIPI_ADAPT_PIXEL1_CNTL1              (0x0031  << 2)
#define MIPI_ADAPT_PIXEL1_CNTL2              (0x0032  << 2)
#define MIPI_ADAPT_PIXEL1_CNTL3              (0x0033  << 2)
#define MIPI_ADAPT_PIXEL1_ST0                (0x003a  << 2)
#define MIPI_ADAPT_PIXEL1_ST1                (0x003b  << 2)

#define MIPI_ADAPT_ALIG_CNTL0           (0x40<<2)
#define MIPI_ADAPT_ALIG_CNTL1           (0x41<<2)
#define MIPI_ADAPT_ALIG_CNTL2           (0x42<<2)
#define MIPI_ADAPT_ALIG_CNTL3           (0x43<<2)
#define MIPI_ADAPT_ALIG_CNTL4           (0x44<<2)
#define MIPI_ADAPT_ALIG_CNTL5           (0x45<<2)
#define MIPI_ADAPT_ALIG_CNTL6           (0x46<<2)
#define MIPI_ADAPT_ALIG_CNTL7           (0x47<<2)
#define MIPI_ADAPT_ALIG_CNTL8           (0x48<<2)
#define MIPI_ADAPT_ALIG_CNTL9           (0x49<<2)
#define MIPI_ADAPT_ALIG_CNTL10          (0x4a<<2)
#define MIPI_ADAPT_ALIG_CNTL11          (0x4b<<2)
#define MIPI_ADAPT_ALIG_CNTL12          (0x4c<<2)
#define MIPI_ADAPT_ALIG_CNTL13          (0x4d<<2)
#define MIPI_ADAPT_ALIG_CNTL14          (0x4e<<2)
#define MIPI_ADAPT_ALIG_ST0             (0x5a<<2)
#define MIPI_ADAPT_ALIG_ST1             (0x5b<<2)
#define MIPI_ADAPT_ALIG_ST2             (0x5c<<2)

#define MIPI_ADAPT_TOP_CNTL0            (0x60<<2)
#define MIPI_ADAPT_TOP_CNTL1            (0x61<<2)
#define MIPI_ADAPT_AXI_CTRL0            (0x62<<2)
#define MIPI_ADAPT_AXI_PROT_STAT0       (0x68<<2)
#define MIPI_ADAPT_AXI_PROT_STAT1       (0x69<<2)
#define DDR_RD0_LBUF_STATUS             (0x7a<<2)

#define MIPI_ADAPT_FE_MUX_CTL0          (0x90<<2)
#define MIPI_ADAPT_FE_MUX_CTL1          (0x91<<2)
#define MIPI_ADAPT_FE_MUX_CTL2          (0x92<<2)
#define MIPI_ADAPT_FE_MUX_CTL3          (0xbf<<2)
#define MIPI_ADAPT_FE_MUX_CTL9          (0xc5<<2)
#define MIPI_ADAPT_FE_MUX_CTL10         (0xc6<<2)

#define MIPI_ADAPT_FE_MUX0_DLY_STAT0    (0x93<<2)
#define MIPI_ADAPT_FE_MUX1_DLY_CNTL0    (0x94<<2)
#define MIPI_ADAPT_FE_MUX1_DLY_STAT0    (0x95<<2)
#define MIPI_ADAPT_FE_MUX2_DLY_CNTL0    (0x96<<2)
#define MIPI_ADAPT_FE_MUX2_DLY_STAT0    (0x97<<2)
#define MIPI_ADAPT_FE_MUX3_DLY_CNTL0    (0x98<<2)
#define MIPI_ADAPT_FE_MUX3_DLY_STAT0    (0x99<<2)
#define MIPI_ADAPT_FE_MUX4_DLY_CNTL0    (0x9b<<2)
#define MIPI_ADAPT_FE_MUX4_DLY_STAT0    (0x9a<<2)
#define MIPI_ADAPT_CSI_HSYNC_0_DLY_CNTL (0xa0<<2)
#define MIPI_ADAPT_CSI_HSYNC_0_DLY_STAT (0xa1<<2)
#define MIPI_ADAPT_CSI_VSYNC_0_DLY_CNTL (0xa8<<2)
#define MIPI_ADAPT_CSI_VSYNC_0_DLY_STAT (0xa9<<2)
#define MIPI_ADAPT_CSI_VSYNC_1_DLY_CNTL (0xaa<<2)
#define MIPI_ADAPT_CSI_VSYNC_1_DLY_STAT (0xab<<2)
#define MIPI_ADAPT_CSI_VSYNC_2_DLY_CNTL (0xac<<2)
#define MIPI_ADAPT_CSI_VSYNC_2_DLY_STAT (0xad<<2)

#define MIPI_PROC_TOP_CTRL0             (0x00<<2)
#define MIPI_PROC_TOP_CTRL1             (0x01<<2)

#define MIPI_OTHER_CNTL0                0x100
#define MIPI_ADAPT_IRQ_MASK0            0x180
#define MIPI_ADAPT_IRQ_PENDING0         0x184

//TOF ISP OFFSET 0xff980000
#define ISP_TOFWR_TOP_INPUT_SIZE        (0x00<<2)
#define ISP_TOFWR_TOP_HOLD_SIZE         (0x01<<2)
#define ISP_TOFWR_TOP_CTRL0             (0x02<<2)
#define ISP_TOFWR_TOP_PATH_EN           (0x03<<2)
#define ISP_TOFWR_TOP_MEAS              (0x04<<2)
#define ISP_TOFWR_TOP_SYN_CTRL          (0x05<<2)
#define ISP_TOFWR_TOP_TIMGEN            (0x06<<2)
#define ISP_TOFWR_TOP_TIMGEN_REF        (0x07<<2)
#define ISP_TOFWR_TOP_TIMEGEN_DBG       (0x08<<2)
#define ISP_TOFWR_TOP_IRQ_EN            (0x09<<2)
#define ISP_TOFWR_TOP_IRQ_CLR           (0x0a<<2)
#define ISP_TOFWR_TOP_IRQ_LINE_THRD     (0x0b<<2)
#define ISP_TOFWR_TOP_UNDONE_CLR        (0x0c<<2)
#define ISP_TOFWR_TOP_GCLK_CTRL         (0x0d<<2)
#define ISP_TOFWR_TOP_RGB2Y_MTRX01      (0x0e<<2)
#define ISP_TOFWR_TOP_RGB2Y_MTRX23      (0x0f<<2)
#define ISP_TOFWR_TOP_RGB2Y_PRE_OFST23  (0x10<<2)
#define ISP_TOFWR_TOP_RGB2Y_PRE_OFST01  (0x11<<2)
#define ISP_TOFWR_TOP_RGB2Y_MTRX_RS     (0x12<<2)
#define ISP_TOFWR_TOP_RO_DBG_STAT0      (0x13<<2)
#define ISP_TOFWR_TOP_RO_DBG_STAT1      (0x14<<2)
#define ISP_TOFWR_TOP_RO_IRQ_STAT       (0x15<<2)
#define ISP_TOFWR_TOP_AXI_WR_STAT       (0x16<<2)
#define ISP_TOFWR_WMIF_CTRL1            (0x20<<2)
#define ISP_TOFWR_WMIF_CTRL2            (0x21<<2)
#define ISP_TOFWR_WMIF_CTRL3            (0x22<<2)
#define ISP_TOFWR_WMIF_CTRL4            (0x23<<2)
#define ISP_TOFWR_WMIF_SCOPE_X          (0x24<<2)
#define ISP_TOFWR_WMIF_SCOPE_Y          (0x25<<2)
#define ISP_TOFWR_WMIF_RO_STAT          (0x26<<2)
#define ISP_TOFWR_WMIF_CTRL5            (0x27<<2)
#define ISP_TOFWR_WMIF_CTRL6            (0x28<<2)
#define ISP_TOFWR_WMIF_CTRL7            (0x29<<2)

#define MIPI_TOP_ISP_PENDING_MASK0           (0xf0 << 2)
#define MIPI_TOP_ISP_PENDING_MASK1           (0xf1 << 2)
#define MIPI_TOP_ISP_PENDING0                (0xf3 << 2)
#define MIPI_TOP_ISP_PENDING1                (0xf4 << 2)

enum {
	FRONTEND_MD = 0,
	FRONTEND1_MD,
	FRONTEND2_MD,
	FRONTEND3_MD,
	READER_MD,
	PIXEL_MD,
	ALIGN_MD,
	MISC_MD,
	TEMP_MD,
	PROC_MD,
	WRMIF_MD,
};

struct adap_regval {
	u32 reg;
	u32 val;
};

#define FRONT0_WR_DONE 28
#define FRONT1_WR_DONE 2
#define FRONT2_WR_DONE 8
#define FRONT3_WR_DONE 14
#define ALIGN_FRAME_END 9
#define READ0_RD_DONE 21
#define READ1_RD_DONE 15

#endif /* __AML_P1_ADAPTER_HW_H__ */
