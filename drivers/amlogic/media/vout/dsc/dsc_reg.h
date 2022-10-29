/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DSC_REG_H__
#define __DSC_REG_H__

#define ENCP_VIDEO_SYNC_MODE			0x1b81
#define VPU_HDMI_SETTING			0x271b

#define DSC_COMP_CTRL			0x0
#define NATIVE_422			9
#define NATIVE_422_WID			1
#define NATIVE_420			8
#define NATIVE_420_WID			1
#define CONVERT_RGB			7
#define CONVERT_RGB_WID			1
#define BLOCK_PRED_ENABLE		6
#define BLOCK_PRED_ENABLE_WID		1
#define VBR_ENABLE			5
#define VBR_ENABLE_WID			1
#define FULL_ICH_ERR_PRECISION		4
#define FULL_ICH_ERR_PRECISION_WID	1
#define DSC_VERSION_MINOR		0
#define DSC_VERSION_MINOR_WID		4

#define DSC_COMP_PIC_SIZE                      0x1
#define PCI_WIDTH			16
#define PCI_WIDTH_WID			16
#define PCI_HEIGHT			0
#define PCI_HEIGHT_WID			16

#define DSC_COMP_SLICE_SIZE                    0x2
#define SLICE_WIDTH			16
#define SLICE_WIDTH_WID			16
#define SLICE_HEIGHT			0
#define SLICE_HEIGHT_WID		16

#define DSC_COMP_BIT_DEPTH                     0x3
#define LINE_BUF_DEPTH			24
#define LINE_BUF_DEPTH_WID		5
#define BITS_PER_COMPONENT		16
#define BITS_PER_COMPONENT_WID		5
#define BITS_PER_PIXEL			0
#define BITS_PER_PIXEL_WID		10

#define DSC_COMP_RC_BITS_SIZE                  0x4
#define RCB_BITS			16
#define RCB_BITS_WID			16
#define RC_MODEL_SIZE			0
#define RC_MODEL_SIZE_WID		16

#define DSC_COMP_RC_TGT_QUANT                  0x5
#define RC_TGT_OFFSET_HI		28
#define RC_TGT_OFFSET_HI_WID		4
#define RC_TGT_OFFSET_LO		24
#define RC_TGT_OFFSET_LO_WID		4
#define RC_EDGE_FACTOR			16
#define RC_EDGE_FACTOR_WID		5
#define RC_QUANT_INCR_LIMIT1		8
#define RC_QUANT_INCR_LIMIT1_WID	5
#define RC_QUANT_INCR_LIMIT0		0
#define RC_QUANT_INCR_LIMIT0_WID	5

#define DSC_COMP_INITIAL_DELAY                 0x6
#define INITIAL_XMIT_DELAY		16
#define INITIAL_XMIT_DELAY_WID		10
#define INITIAL_DEC_DELAY		0
#define INITIAL_DEC_DELAY_WID		16

#define DSC_COMP_INITIAL_OFFSET_SCALE          0x7
#define INITIAL_SCALE_VALUE		16
#define INITIAL_SCALE_VALUE_WID		6
#define INITIAL_OFFSET			0
#define INITIAL_OFFSET_WID		16

#define DSC_COMP_SEC_OFS_ADJ                   0x8
#define SECOND_LINE_OFS_ADJ		0
#define SECOND_LINE_OFS_ADJ_WID		16

#define DSC_COMP_FS_BPG_OFS                    0x9
#define FIRST_LINE_BPG_OFS	8
#define FIRST_LINE_BPG_OFS_WID	5
#define SECOND_LINE_BPG_OFS	0
#define SECOND_LINE_BPG_OFS_WID	5

#define DSC_COMP_NFS_BPG_OFFSET                0xa
#define NFL_BPG_OFFSET			16
#define NFL_BPG_OFFSET_WID		16
#define NSL_BPG_OFFSET			0
#define NSL_BPG_OFFSET_WID		16

#define DSC_COMP_RC_BUF_THRESH_0               0xb
#define RC_BUF_THRESH_0			0
#define RC_BUF_THRESH_0_WID		16

#define DSC_COMP_RC_BUF_THRESH_1               0xc
#define RC_BUF_THRESH_1			0
#define RC_BUF_THRESH_2_WID		16

#define DSC_COMP_RC_BUF_THRESH_2               0xd
#define RC_BUF_THRESH_2			0
#define RC_BUF_THRESH_2_WID		16

#define DSC_COMP_RC_BUF_THRESH_3               0xe
#define RC_BUF_THRESH_3			0
#define RC_BUF_THRESH_3_WID		16

#define DSC_COMP_RC_BUF_THRESH_4               0xf
#define RC_BUF_THRESH_4			0
#define RC_BUF_THRESH_4_WID		16

#define DSC_COMP_RC_BUF_THRESH_5              0x10
#define RC_BUF_THRESH_5			0
#define RC_BUF_THRESH_6_WID		16

#define DSC_COMP_RC_BUF_THRESH_6              0x11
#define RC_BUF_THRESH_6			0
#define RC_BUF_THRESH_6_WID		16

#define DSC_COMP_RC_BUF_THRESH_7              0x12
#define RC_BUF_THRESH_7			0
#define RC_BUF_THRESH_7_WID		16

#define DSC_COMP_RC_BUF_THRESH_8              0x13
#define RC_BUF_THRESH_8			0
#define RC_BUF_THRESH_8_WID		16

#define DSC_COMP_RC_BUF_THRESH_9              0x14
#define RC_BUF_THRESH_9			0
#define RC_BUF_THRESH_9_WID		16

#define DSC_COMP_RC_BUF_THRESH_10             0x15
#define RC_BUF_THRESH_10			0
#define RC_BUF_THRESH_10_WID		16

#define DSC_COMP_RC_BUF_THRESH_11             0x16
#define RC_BUF_THRESH_11			0
#define RC_BUF_THRESH_11_WID		16

#define DSC_COMP_RC_BUF_THRESH_12             0x17
#define RC_BUF_THRESH_12		0
#define RC_BUF_THRESH_12_WID		16

#define DSC_COMP_RC_BUF_THRESH_13             0x18
#define RC_BUF_THRESH_13		0
#define RC_BUF_THRESH_13_WID		16

#define DSC_COMP_RC_RANGE_0                   0x19
#define RANGE_BPG_OFFSET_0		16
#define RANGE_BPG_OFFSET_0_WID		6
#define RANGE_MAX_QP_0			8
#define RANGE_MAX_QP_0_WID		5
#define RANGE_MIN_QP_0			0
#define RANGE_MIN_QP_0_WID		5

#define DSC_COMP_RC_RANGE_1                   0x1a
#define RANGE_BPG_OFFSET_1		16
#define RANGE_BPG_OFFSET_1_WID		6
#define RANGE_MAX_QP_1			8
#define RANGE_MAX_QP_1_WID		5
#define RANGE_MIN_QP_1			0
#define RANGE_MIN_QP_1_WID		5

#define DSC_COMP_RC_RANGE_2                   0x1b
#define RANGE_BPG_OFFSET_2		16
#define RANGE_BPG_OFFSET_2_WID		6
#define RANGE_MAX_QP_2			8
#define RANGE_MAX_QP_2_WID		5
#define RANGE_MIN_QP_2			0
#define RANGE_MIN_QP_2_WID		5

#define DSC_COMP_RC_RANGE_3                   0x1c
#define RANGE_BPG_OFFSET_3		16
#define RANGE_BPG_OFFSET_3_WID		6
#define RANGE_MAX_QP_3			8
#define RANGE_MAX_QP_3_WID		5
#define RANGE_MIN_QP_3			0
#define RANGE_MIN_QP_3_WID		5

#define DSC_COMP_RC_RANGE_4                   0x1d
#define RANGE_BPG_OFFSET_4		16
#define RANGE_BPG_OFFSET_4_WID		6
#define RANGE_MAX_QP_4			8
#define RANGE_MAX_QP_4_WID		5
#define RANGE_MIN_QP_4			0
#define RANGE_MIN_QP_4_WID		5

#define DSC_COMP_RC_RANGE_5                   0x1e
#define RANGE_BPG_OFFSET_5		16
#define RANGE_BPG_OFFSET_5_WID		6
#define RANGE_MAX_QP_5			8
#define RANGE_MAX_QP_5_WID		5
#define RANGE_MIN_QP_5			0
#define RANGE_MIN_QP_5_WID		5

#define DSC_COMP_RC_RANGE_6                   0x1f
#define RANGE_BPG_OFFSET_6		16
#define RANGE_BPG_OFFSET_6_WID		6
#define RANGE_MAX_QP_6			8
#define RANGE_MAX_QP_6_WID		5
#define RANGE_MIN_QP_6			0
#define RANGE_MIN_QP_6_WID		5

#define DSC_COMP_RC_RANGE_7                   0x20
#define RANGE_BPG_OFFSET_7		16
#define RANGE_BPG_OFFSET_7_WID		6
#define RANGE_MAX_QP_7			8
#define RANGE_MAX_QP_7_WID		5
#define RANGE_MIN_QP_7			0
#define RANGE_MIN_QP_7_WID		5

#define DSC_COMP_RC_RANGE_8                   0x21
#define RANGE_BPG_OFFSET_8		16
#define RANGE_BPG_OFFSET_8_WID		6
#define RANGE_MAX_QP_8			8
#define RANGE_MAX_QP_8_WID		5
#define RANGE_MIN_QP_8			0
#define RANGE_MIN_QP_8_WID		5

#define DSC_COMP_RC_RANGE_9                   0x22
#define RANGE_BPG_OFFSET_9		16
#define RANGE_BPG_OFFSET_9_WID		6
#define RANGE_MAX_QP_9			8
#define RANGE_MAX_QP_9_WID		5
#define RANGE_MIN_QP_9			0
#define RANGE_MIN_QP_9_WID		5

#define DSC_COMP_RC_RANGE_10                  0x23
#define RANGE_BPG_OFFSET_10		16
#define RANGE_BPG_OFFSET_10_WID		6
#define RANGE_MAX_QP_10			8
#define RANGE_MAX_QP_10_WID		5
#define RANGE_MIN_QP_10			0
#define RANGE_MIN_QP_10_WID		5

#define DSC_COMP_RC_RANGE_11                  0x24
#define RANGE_BPG_OFFSET_11		16
#define RANGE_BPG_OFFSET_11_WID		6
#define RANGE_MAX_QP_11			8
#define RANGE_MAX_QP_11_WID		5
#define RANGE_MIN_QP_11			0
#define RANGE_MIN_QP_11_WID		5

#define DSC_COMP_RC_RANGE_12                  0x25
#define RANGE_BPG_OFFSET_12		16
#define RANGE_BPG_OFFSET_12_WID		6
#define RANGE_MAX_QP_12			8
#define RANGE_MAX_QP_12_WID		5
#define RANGE_MIN_QP_12			0
#define RANGE_MIN_QP_12_WID		5

#define DSC_COMP_RC_RANGE_13                  0x26
#define RANGE_BPG_OFFSET_13		16
#define RANGE_BPG_OFFSET_13_WID		6
#define RANGE_MAX_QP_13			8
#define RANGE_MAX_QP_13_WID		5
#define RANGE_MIN_QP_13			0
#define RANGE_MIN_QP_13_WID		5

#define DSC_COMP_RC_RANGE_14                  0x27
#define RANGE_BPG_OFFSET_14		16
#define RANGE_BPG_OFFSET_14_WID		6
#define RANGE_MAX_QP_14			8
#define RANGE_MAX_QP_14_WID		5
#define RANGE_MIN_QP_14			0
#define RANGE_MIN_QP_14_WID		5

#define DSC_COMP_FLATNESS                     0x28
#define FLATNESS_MIN_QP			24
#define FLATNESS_MIN_QP_WID		5
#define FLATNESS_MAX_QP			16
#define FLATNESS_MAX_QP_WID		5
#define FLATNESS_DET_THRESH		0
#define FLATNESS_DET_THRESH_WID		16

#define DSC_COMP_SCALE                        0x29
#define SCALE_DECREMENT_INTERVAL	20
#define SCALE_DECREMENT_INTERVAL_WID	12
#define SCALE_INCREMENT_INTERVAL	4
#define SCALE_INCREMENT_INTERVAL_WID	16

#define DSC_COMP_SLICE_FINAL_OFFSET           0x2a
#define SLICE_BPG_OFFSET		16
#define SLICE_BPG_OFFSET_WID		16
#define FINAL_OFFSET			0
#define FINAL_OFFSET_WID		16

#define DSC_COMP_CHUNK_WORD_SIZE              0x2b
#define MUX_WORD_SIZE			16
#define MUX_WORD_SIZE_WID		7
#define CHUNK_SIZE			0
#define CHUNK_SIZE_WID			16

#define DSC_COMP_FLAT_QP                      0x2c
#define VERY_FLAT_QP			16
#define VERY_FLAT_QP_WID		5
#define SOMEWHAT_FLAT_QP_DELTA		8
#define SOMEWHAT_FLAT_QP_DELTA_WID	5
#define SOMEWHAT_FLAT_QP_THRESH		0
#define SOMEWHAT_FLAT_QP_THRESH_WID	5

#define DSC_ASIC_CTRL0                        0x2d
#define SLICE_NUM_M1			28
#define SLICE_NUM_M1_WID		4
#define CLR_SSM_FIFO_STS		18
#define CLR_SSM_FIFO_STS_WID		1
#define DSC_ENC_EN			17
#define DSC_ENC_EN_WID			1
#define DSC_ENC_FRM_LATCH_EN		16
#define DSC_ENC_FRM_LATCH_EN_WID	1
#define PIX_PER_CLK			13
#define PIX_PER_CLK_WID			3
#define INBUF_RD_DLY0			8
#define INBUF_RD_DLY0_WID		5
#define INBUF_RD_DLY1			0
#define INBUF_RD_DLY1_WID		8

#define DSC_ASIC_CTRL1                        0x2e
#define C3_CLK_EN			31
#define C3_CLK_EN_WID			1
#define C2_CLK_EN			30
#define C2_CLK_EN_WID			1
#define C1_CLK_EN			29
#define C1_CLK_EN_WID			1
#define C0_CLK_EN			28
#define C0_CLK_EN_WID			1
#define SLICES_IN_CORE			26
#define SLICES_IN_CORE_WID		1
#define SLICES_GROUP_NUMBER		0
#define SLICES_GROUP_NUMBER_WID		26

#define DSC_ASIC_CTRL2                        0x2f
#define PARTIAL_GROUP_PIX_NUM		14
#define PARTIAL_GROUP_PIX_NUM_WID	2

#define DSC_ASIC_CTRL3                        0x30
#define CHUNK_6BYTE_NUM_M1		0
#define CHUNK_6BYTE_NUM_M1_WID		16

#define DSC_ASIC_CTRL4                        0x31
#define TMG_EN				31
#define TMG_EN_WID			1
#define TMG_HAVON_BEGIN			16
#define TMG_HAVON_BEGIN_WID		14

#define DSC_ASIC_CTRL5                        0x32
#define TMG_VAVON_BLINE			16
#define TMG_VAVON_BLINE_WID		14
#define TMG_VAVON_ELINE			0
#define TMG_VAVON_ELINE_WID		14

#define DSC_ASIC_CTRL6                        0x33
#define TMG_HSO_BEGIN			16
#define TMG_HSO_BEGIN_WID		14
#define TMG_HSO_END			0
#define TMG_HSO_END_WID			14

#define DSC_ASIC_CTRL7                        0x34
#define TMG_VSO_BEGIN			16
#define TMG_VSO_BEGIN_WID		14
#define TMG_VSO_END			0
#define TMG_VSO_END_WID			14

#define DSC_ASIC_CTRL8                        0x35
#define TMG_VSO_BLINE			16
#define TMG_VSO_BLINE_WID		14
#define TMG_VSO_ELINE			0
#define TMG_VSO_ELINE_WID		14

#define DSC_ASIC_CTRL9                        0x36
#define PIX_IN_SWAP0			0
#define PIX_IN_SWAP0_WID		32

#define DSC_ASIC_CTRLA                        0x37
#define PIX_IN_SWAP1			0
#define PIX_IN_SWAP1_WID		16

#define DSC_ASIC_CTRLB                        0x38
#define LAST_SLICE_ACTIVE_M1		0
#define LAST_SLICE_ACTIVE_M1_WID	14

#define DSC_ASIC_CTRLC                        0x39
#define GCLK_CTRL			0
#define GCLK_CTRL_WID			32

#define DSC_ASIC_CTRLD                        0x3a
#define C3S1_CLR_CB_STS			31
#define C3S1_CLR_CB_STS_WID		1
#define C3S0_CLR_CB_STS			30
#define C3S0_CLR_CB_STS_WID		1
#define C2S1_CLR_CB_STS			29
#define C2S1_CLR_CB_STS_WID		1
#define C2S0_CLR_CB_STS			28
#define C2S0_CLR_CB_STS_WID		1
#define C1S1_CLR_CB_STS			27
#define C1S1_CLR_CB_STS_WID		1
#define C1S0_CLR_CB_STS			26
#define C1S0_CLR_CB_STS_WID		1
#define C0S1_CLR_CB_STS			25
#define C0S1_CLR_CB_STS_WID		1
#define C0S0_CLR_CB_STS			24
#define C0S0_CLR_CB_STS_WID		1
#define C0S1_CB_OVER_FLOW_TH			12
#define C0S1_CB_OVER_FLOW_TH_WID		12
#define C0S0_CB_OVER_FLOW_TH			0
#define C0S0_CB_OVER_FLOW_TH_WID		12

#define DSC_ASIC_CTRLE                        0x3b
#define C1S1_CB_OVER_FLOW_TH			12
#define C1S1_CB_OVER_FLOW_TH_WID		12
#define C1S0_CB_OVER_FLOW_TH			0
#define C1S0_CB_OVER_FLOW_TH_WID		12

#define DSC_ASIC_CTRLF                        0x3c
#define C2S1_CB_OVER_FLOW_TH			12
#define C2S1_CB_OVER_FLOW_TH_WID		12
#define C2S0_CB_OVER_FLOW_TH			0
#define C2S0_CB_OVER_FLOW_TH_WID		12

#define DSC_ASIC_CTRL10                      0x3d
#define C3S1_CB_OVER_FLOW_TH			12
#define C3S1_CB_OVER_FLOW_TH_WID		12
#define C3S0_CB_OVER_FLOW_TH			0
#define C3S0_CB_OVER_FLOW_TH_WID		12

#define DSC_ASIC_CTRL11                      0x3e
#define C3S1_CB_UNDER_FLOW			15
#define C3S1_CB_UNDER_FLOW_WID		1
#define C3S0_CB_UNDER_FLOW			14
#define C3S0_CB_UNDER_FLOW_WID		1
#define C2S1_CB_UNDER_FLOW			13
#define C2S1_CB_UNDER_FLOW_WID		1
#define C2S0_CB_UNDER_FLOW			12
#define C2S0_CB_UNDER_FLOW_WID		1
#define C1S1_CB_UNDER_FLOW			11
#define C1S1_CB_UNDER_FLOW_WID		1
#define C1S0_CB_UNDER_FLOW			10
#define C1S0_CB_UNDER_FLOW_WID		1
#define C0S1_CB_UNDER_FLOW			9
#define C0S1_CB_UNDER_FLOW_WID		1
#define C0S0_CB_UNDER_FLOW			8
#define C0S0_CB_UNDER_FLOW_WID		1
#define C3S1_CB_OVER_FLOW			7
#define C3S1_CB_OVER_FLOW_WID		1
#define C3S0_CB_OVER_FLOW			6
#define C3S0_CB_OVER_FLOW_WID		1
#define C2S1_CB_OVER_FLOW			5
#define C2S1_CB_OVER_FLOW_WID		1
#define C2S0_CB_OVER_FLOW			4
#define C2S0_CB_OVER_FLOW_WID		1
#define C1S1_CB_OVER_FLOW			3
#define C1S1_CB_OVER_FLOW_WID		1
#define C1S0_CB_OVER_FLOW			2
#define C1S0_CB_OVER_FLOW_WID		1
#define C0S1_CB_OVER_FLOW			1
#define C0S1_CB_OVER_FLOW_WID		1
#define C0S0_CB_OVER_FLOW			0
#define C0S0_CB_OVER_FLOW_WID		1

#define DSC_ASIC_CTRL12                      0x3f
#define OUT_SWAP			0
#define OUT_SWAP_WID			24

#define DSC_ASIC_CTRL13                      0x40
#define C0_SSM_FIFO_STS			0
#define C0_SSM_FIFO_STS_WID			32

#define DSC_ASIC_CTRL14                      0x41
#define C1_SSM_FIFO_STS			0
#define C0_SSM_FIFO_STS_WID			32

#define DSC_ASIC_CTRL15                      0x42
#define C2_SSM_FIFO_STS			0
#define C0_SSM_FIFO_STS_WID			32

#define DSC_ASIC_CTRL16                      0x43
#define C3_SSM_FIFO_STS			0
#define C0_SSM_FIFO_STS_WID			32

#define DSC_ASIC_CTRL17                      0x44
#define INTR_STAT			4
#define INTR_STAT_WID			3
#define INTR_MASKN			0
#define INTR_MASKN_WID			3

#define DSC_ASIC_CTRL18                      0x45
#define HC_VTOTAL_M1			16
#define HC_VTOTAL_M1_WID			14
#define HC_HTOTAL_M1			0
#define HC_HTOTAL_M1_WID			14

#define DSC_ASIC_CTRL19                      0x46
#define HS_ODD_NO_TAIL			31
#define HS_ODD_NO_TAIL_WID		1
#define HS_ODD_NO_HEAD			30
#define HS_ODD_NO_HEAD_WID		1
#define HS_EVEN_NO_TAIL			29
#define HS_EVEN_NO_TAIL_WID		1
#define HS_EVEN_NO_HEAD			28
#define HS_EVEN_NO_HEAD_WID		1
#define VS_ODD_NO_TAIL			27
#define VS_ODD_NO_TAIL_WID		1
#define VS_ODD_NO_HEAD			26
#define VS_ODD_NO_HEAD_WID		1
#define VS_EVEN_NO_TAIL			25
#define VS_EVEN_NO_TAIL_WID		1
#define VS_EVEN_NO_HEAD			24
#define VS_EVEN_NO_HEAD_WID		1
#define AFF_UNDER_FLOW			17
#define AFF_UNDER_FLOW_WID			1
#define HC_ACTIVE_ODD_MODE		16
#define HC_ACTIVE_ODD_MODE_WID		1
#define HC_HTOTAL_OFFS_ODDLINE		8
#define HC_HTOTAL_OFFS_ODDLINE_WID	8
#define HC_HTOTAL_OFFS_EVENLINE		0
#define HC_HTOTAL_OFFS_EVENLINE_WID	8

#define DSC_ASIC_CTRL20                      0x47
//Bit 31:16       reg_vrr_max_vnum
//Bit 15:0        reg_vrr_min_vnum
#define DSC_ASIC_CTRL21                      0x48
//Bit 31:16       reg_vsp_dly_num
//Bit 7:4         reg_vrr_frm_ths
//Bit 3:0         reg_vsp_rst_num
#define DSC_ASIC_CTRL22                      0x49
//Bit 15:12       ro_vrr_max_err
//Bit 11:8        ro_vrr_vsp_cnt
//Bit 6:6         reg_vrr_mode
//Bit 5:4         reg_vrr_vsp_en
//Bit 3:3         reg_vrr_vsp_sel
//Bit 2:2         reg_vrr_clr
//Bit 1:1         reg_vsp_din
//Bit 0:0         reg_hc_htotal_offs_evenline
#define DSC_ASIC_CTRL23                      0x4a
//Bit 31:31        reg_tmg_shadow_en
//Bit 29:16        reg_hc_vtotal_m1_shadow
//Bit 13:0         reg_hc_htotal_m1_shadow

//encp bist timing register
#define ENCP_DVI_HSO_BEGIN				0x1c30
#define ENCP_DVI_HSO_END				0x1c31
#define ENCP_DVI_VSO_BLINE_EVN				0x1c32
#define ENCP_DVI_VSO_ELINE_EVN				0x1c34
#define ENCP_DVI_VSO_BEGIN_EVN				0x1c36
#define ENCP_DVI_VSO_END_EVN				0x1c38
#define ENCP_DE_H_BEGIN					0x1c3a
#define ENCP_DE_H_END					0x1c3b
#define ENCP_DE_V_BEGIN_EVEN				0x1c3c
#define ENCP_DE_V_END_EVEN				0x1c3d

//encp video timing register
#define ENCP_VIDEO_HSO_BEGIN				0x1ba7
#define ENCP_VIDEO_HSO_END				0x1ba8
#define ENCP_VIDEO_VSO_BLINE				0x1bab
#define ENCP_VIDEO_VSO_ELINE				0x1bac
#define ENCP_VIDEO_VSO_BEGIN				0x1ba9
#define ENCP_VIDEO_VSO_END				0x1baa
#define ENCP_VIDEO_HAVON_END				0x1ba3
#define ENCP_VIDEO_HAVON_BEGIN				0x1ba4
#define ENCP_VIDEO_VAVON_BLINE				0x1ba6
#define ENCP_VIDEO_VAVON_ELINE				0x1baf

#define ENCP_VIDEO_MAX_PXCNT				0x1b97
#define ENCP_VIDEO_MAX_LNCNT				0x1bae
#endif

