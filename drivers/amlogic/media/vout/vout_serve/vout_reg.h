/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _VOUT_REG_H_
#define _VOUT_REG_H_
#include <linux/amlogic/iomap.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

#define VOUT_REG_OFFSET(reg)          ((reg) << 2)

/* [3: 2] cntl_viu2_sel_venc:
 *         0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 * [1: 0] cntl_viu1_sel_venc:
 *         0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 */
#define VPU_VIU_VENC_MUX_CTRL                      0x271a
/* [2] Enci_afifo_clk: 0: cts_vpu_clk_tm 1: cts_vpu_clkc_tm
 * [1] Encl_afifo_clk: 0: cts_vpu_clk_tm 1: cts_vpu_clkc_tm
 * [0] Encp_afifo_clk: 0: cts_vpu_clk_tm 1: cts_vpu_clkc_tm
 */
#define VPU_VENCX_CLK_CTRL                         0x2785

#define VPP_POSTBLEND_H_SIZE                       0x1d21
#define VPP2_POSTBLEND_H_SIZE                      0x1921
#define VPP_WRBAK_CTRL                             0x1df9

#define ENCI_VIDEO_MODE                            0x1b00
#define ENCI_VIDEO_MODE_ADV                        0x1b01
#define ENCI_VIDEO_FSC_ADJ                         0x1b02
#define ENCI_VIDEO_BRIGHT                          0x1b03
#define ENCI_VIDEO_CONT                            0x1b04
#define ENCI_VIDEO_SAT                             0x1b05
#define ENCI_VIDEO_HUE                             0x1b06
#define ENCI_VIDEO_SCH                             0x1b07
#define ENCI_SYNC_MODE                             0x1b08
#define ENCI_SYNC_CTRL                             0x1b09
#define ENCI_SYNC_HSO_BEGIN                        0x1b0a
#define ENCI_SYNC_HSO_END                          0x1b0b
#define ENCI_SYNC_VSO_EVN                          5
#define ENCI_SYNC_VSO_ODD                          0x1b0d
#define ENCI_SYNC_VSO_EVNLN                        0x1b0e
#define ENCI_SYNC_VSO_ODDLN                        0x1b0f
#define ENCI_SYNC_HOFFST                           0x1b10
#define ENCI_SYNC_VOFFST                           0x1b11
#define ENCI_SYNC_ADJ                              0x1b12
#define ENCI_RGB_SETTING                           0x1b13

#define ENCI_DE_H_BEGIN                            0x1b16
#define ENCI_DE_H_END                              0x1b17
#define ENCI_DE_V_BEGIN_EVEN                       0x1b18
#define ENCI_DE_V_END_EVEN                         0x1b19
#define ENCI_DE_V_BEGIN_ODD                        0x1b1a
#define ENCI_DE_V_END_ODD                          0x1b1b

#define ENCI_DBG_PX_RST                            0x1b48
#define ENCI_DBG_FLDLN_RST                         0x1b49
#define ENCI_DBG_PX_INT                            0x1b4a
#define ENCI_DBG_FLDLN_INT                         0x1b4b
#define ENCI_DBG_MAXPX                             0x1b4c
#define ENCI_DBG_MAXLN                             0x1b4d
#define ENCI_MACV_MAX_AMP                          0x1b50
#define ENCI_MACV_PULSE_LO                         0x1b51
#define ENCI_MACV_PULSE_HI                         0x1b52
#define ENCI_MACV_BKP_MAX                          0x1b53
#define ENCI_CFILT_CTRL                            0x1b54
#define ENCI_CFILT7                                0x1b55
#define ENCI_YC_DELAY                              0x1b56
#define ENCI_VIDEO_EN                              0x1b57

#define VENC_SYNC_ROUTE                            0x1b60
#define VENC_UPSAMPLE_CTRL0                        0x1b64
#define VENC_UPSAMPLE_CTRL1                        0x1b65
#define VENC_UPSAMPLE_CTRL2                        0x1b66
#define VENC_VIDEO_PROG_MODE                       0x1b68
#define VENC_INTCTRL                               0x1b6e

#define ENCI_DVI_HSO_BEGIN                         0x1c00
#define ENCI_DVI_HSO_END                           0x1c01
#define ENCI_DVI_VSO_BLINE_EVN                     0x1c02
#define ENCI_DVI_VSO_BLINE_ODD                     0x1c03
#define ENCI_DVI_VSO_ELINE_EVN                     0x1c04
#define ENCI_DVI_VSO_ELINE_ODD                     0x1c05
#define ENCI_DVI_VSO_BEGIN_EVN                     0x1c06
#define ENCI_DVI_VSO_BEGIN_ODD                     0x1c07
#define ENCI_DVI_VSO_END_EVN                       0x1c08
#define ENCI_DVI_VSO_END_ODD                       0x1c09

#define ENCI_CFILT_CTRL2                           0x1c0a
#define ENCI_DACSEL_0                              0x1c0b
#define ENCI_DACSEL_1                              0x1c0c
#define ENCP_DACSEL_0                              0x1c0d
#define ENCP_DACSEL_1                              0x1c0e
#define ENCP_MAX_LINE_SWITCH_POINT                 0x1c0f
#define ENCI_TST_EN                                0x1c10
#define ENCI_TST_MDSEL                             0x1c11
#define ENCI_TST_Y                                 0x1c12
#define ENCI_TST_CB                                0x1c13
#define ENCI_TST_CR                                0x1c14
#define ENCI_TST_CLRBAR_STRT                       0x1c15
#define ENCI_TST_CLRBAR_WIDTH                      0x1c16
#define ENCI_TST_VDCNT_STSET                       0x1c17

#define ENCI_VFIFO2VD_CTL                          0x1c18

#define ENCI_VFIFO2VD_PIXEL_START                  0x1c19

#define ENCI_VFIFO2VD_PIXEL_END                    0x1c1a

#define ENCI_VFIFO2VD_LINE_TOP_START               0x1c1b

#define ENCI_VFIFO2VD_LINE_TOP_END                 0x1c1c

#define ENCI_VFIFO2VD_LINE_BOT_START               0x1c1d

#define ENCI_VFIFO2VD_LINE_BOT_END                 0x1c1e
#define ENCI_VFIFO2VD_CTL2                         0x1c1f

/* ENCL registers */
#define ENCL_VIDEO_EN                              0x1ca0
#define ENCL_VIDEO_Y_SCL                           0x1ca1
#define ENCL_VIDEO_PB_SCL                          0x1ca2
#define ENCL_VIDEO_PR_SCL                          0x1ca3
#define ENCL_VIDEO_Y_OFFST                         0x1ca4
#define ENCL_VIDEO_PB_OFFST                        0x1ca5
#define ENCL_VIDEO_PR_OFFST                        0x1ca6
/* ----- Video mode */
#define ENCL_VIDEO_MODE                            0x1ca7
#define ENCL_VIDEO_MODE_ADV                        0x1ca8
/* --------------- Debug pins */
#define ENCL_DBG_PX_RST                            0x1ca9
#define ENCL_DBG_LN_RST                            0x1caa
#define ENCL_DBG_PX_INT                            0x1cab
#define ENCL_DBG_LN_INT                            0x1cac
/* ----------- Video Advanced setting */
#define ENCL_VIDEO_YFP1_HTIME                      0x1cad
#define ENCL_VIDEO_YFP2_HTIME                      0x1cae
#define ENCL_VIDEO_YC_DLY                          0x1caf
#define ENCL_VIDEO_MAX_PXCNT                       0x1cb0
#define ENCL_VIDEO_HAVON_END                       0x1cb1
#define ENCL_VIDEO_HAVON_BEGIN                     0x1cb2
#define ENCL_VIDEO_VAVON_ELINE                     0x1cb3
#define ENCL_VIDEO_VAVON_BLINE                     0x1cb4
#define ENCL_VIDEO_HSO_BEGIN                       0x1cb5
#define ENCL_VIDEO_HSO_END                         0x1cb6
#define ENCL_VIDEO_VSO_BEGIN                       0x1cb7
#define ENCL_VIDEO_VSO_END                         0x1cb8
#define ENCL_VIDEO_VSO_BLINE                       0x1cb9
#define ENCL_VIDEO_VSO_ELINE                       0x1cba
#define ENCL_VIDEO_MAX_LNCNT                       0x1cbb
#define ENCL_VIDEO_BLANKY_VAL                      0x1cbc
#define ENCL_VIDEO_BLANKPB_VAL                     0x1cbd
#define ENCL_VIDEO_BLANKPR_VAL                     0x1cbe
#define ENCL_VIDEO_HOFFST                          0x1cbf
#define ENCL_VIDEO_VOFFST                          0x1cc0
#define ENCL_VIDEO_RGB_CTRL                        0x1cc1
#define ENCL_VIDEO_FILT_CTRL                       0x1cc2
#define ENCL_VIDEO_OFLD_VPEQ_OFST                  0x1cc3
#define ENCL_VIDEO_OFLD_VOAV_OFST                  0x1cc4
#define ENCL_VIDEO_MATRIX_CB                       0x1cc5
#define ENCL_VIDEO_MATRIX_CR                       0x1cc6
#define ENCL_VIDEO_RGBIN_CTRL                      0x1cc7
#define ENCL_MAX_LINE_SWITCH_POINT                 0x1cc8
#define ENCL_DACSEL_0                              0x1cc9
#define ENCL_DACSEL_1                              0x1cca

/* ENCP registers */
#define ENCP_VIDEO_EN                              0x1b80
#define ENCP_VIDEO_SYNC_MODE                       0x1b81
#define ENCP_MACV_EN                               0x1b82
#define ENCP_VIDEO_Y_SCL                           0x1b83
#define ENCP_VIDEO_PB_SCL                          0x1b84
#define ENCP_VIDEO_PR_SCL                          0x1b85
#define ENCP_VIDEO_SYNC_SCL                        0x1b86
#define ENCP_VIDEO_MACV_SCL                        0x1b87
#define ENCP_VIDEO_Y_OFFST                         0x1b88
#define ENCP_VIDEO_PB_OFFST                        0x1b89
#define ENCP_VIDEO_PR_OFFST                        0x1b8a
#define ENCP_VIDEO_SYNC_OFFST                      0x1b8b
#define ENCP_VIDEO_MACV_OFFST                      0x1b8c
//----- Video mode
#define ENCP_VIDEO_MODE                            0x1b8d
#define ENCP_VIDEO_MODE_ADV                        0x1b8e
//--------------- Debug pins
#define ENCP_DBG_PX_RST                            0x1b90
#define ENCP_DBG_LN_RST                            0x1b91
#define ENCP_DBG_PX_INT                            0x1b92
#define ENCP_DBG_LN_INT                            0x1b93
//----------- Video Advanced setting
#define ENCP_VIDEO_YFP1_HTIME                      0x1b94
#define ENCP_VIDEO_YFP2_HTIME                      0x1b95
#define ENCP_VIDEO_YC_DLY                          0x1b96
#define ENCP_VIDEO_MAX_PXCNT                       0x1b97
#define ENCP_VIDEO_HSPULS_BEGIN                    0x1b98
#define ENCP_VIDEO_HSPULS_END                      0x1b99
#define ENCP_VIDEO_HSPULS_SWITCH                   0x1b9a
#define ENCP_VIDEO_VSPULS_BEGIN                    0x1b9b
#define ENCP_VIDEO_VSPULS_END                      0x1b9c
#define ENCP_VIDEO_VSPULS_BLINE                    0x1b9d
#define ENCP_VIDEO_VSPULS_ELINE                    0x1b9e
#define ENCP_VIDEO_EQPULS_BEGIN                    0x1b9f
#define ENCP_VIDEO_EQPULS_END                      0x1ba0
#define ENCP_VIDEO_EQPULS_BLINE                    0x1ba1
#define ENCP_VIDEO_EQPULS_ELINE                    0x1ba2
#define ENCP_VIDEO_HAVON_END                       0x1ba3
#define ENCP_VIDEO_HAVON_BEGIN                     0x1ba4
#define ENCP_VIDEO_VAVON_ELINE                     0x1baf
#define ENCP_VIDEO_VAVON_BLINE                     0x1ba6
#define ENCP_VIDEO_HSO_BEGIN                       0x1ba7
#define ENCP_VIDEO_HSO_END                         0x1ba8
#define ENCP_VIDEO_VSO_BEGIN                       0x1ba9
#define ENCP_VIDEO_VSO_END                         0x1baa
#define ENCP_VIDEO_VSO_BLINE                       0x1bab
#define ENCP_VIDEO_VSO_ELINE                       0x1bac
#define ENCP_VIDEO_SYNC_WAVE_CURVE                 0x1bad
#define ENCP_VIDEO_MAX_LNCNT                       0x1bae
#define ENCP_VIDEO_SY_VAL                          0x1bb0
#define ENCP_VIDEO_SY2_VAL                         0x1bb1
#define ENCP_VIDEO_BLANKY_VAL                      0x1bb2
#define ENCP_VIDEO_BLANKPB_VAL                     0x1bb3
#define ENCP_VIDEO_BLANKPR_VAL                     0x1bb4
#define ENCP_VIDEO_HOFFST                          0x1bb5
#define ENCP_VIDEO_VOFFST                          0x1bb6
#define ENCP_VIDEO_RGB_CTRL                        0x1bb7
#define ENCP_VIDEO_FILT_CTRL                       0x1bb8
#define ENCP_VIDEO_OFLD_VPEQ_OFST                  0x1bb9
#define ENCP_VIDEO_OFLD_VOAV_OFST                  0x1bba
#define ENCP_VIDEO_MATRIX_CB                       0x1bbb
#define ENCP_VIDEO_MATRIX_CR                       0x1bbc
#define ENCP_VIDEO_RGBIN_CTRL                      0x1bbd

#define VPU_VENC_CTRL                              0x1cef

#define VPP_VDO_MEAS_CTRL                          0x1da8
#define VPP_VDO_MEAS_VS_COUNT_HI                   0x1da9
#define VPP_VDO_MEAS_VS_COUNT_LO                   0x1daa
/* S5 */
#define VPU_VENC_RO_MEAS0                          0x1cfe
#define VPU_VENC_RO_MEAS1                          0x1cff

/* HIU */
#define HHI_VIID_CLK_DIV                           0x4a
#define DAC0_CLK_SEL           28
#define DAC1_CLK_SEL           24
#define DAC2_CLK_SEL           20
#define VCLK2_XD_RST           17
#define VCLK2_XD_EN            16
#define ENCL_CLK_SEL           12
#define VCLK2_XD                0

#define HHI_VIID_CLK_CNTL                          0x4b
#define VCLK2_EN               19
#define VCLK2_CLK_IN_SEL       16
#define VCLK2_SOFT_RST         15
#define VCLK2_DIV12_EN          4
#define VCLK2_DIV6_EN           3
#define VCLK2_DIV4_EN           2
#define VCLK2_DIV2_EN           1
#define VCLK2_DIV1_EN           0

#define HHI_VID_CLK_DIV                            0x59
#define ENCI_CLK_SEL           28
#define ENCP_CLK_SEL           24
#define ENCT_CLK_SEL           20
#define VCLK_XD_RST            17
#define VCLK_XD_EN             16
#define ENCL_CLK_SEL           12
#define VCLK_XD1                8
#define VCLK_XD0                0

#define HHI_VID_CLK_CNTL                           0x5f
#define VCLK_EN1               20
#define VCLK_EN0               19
#define VCLK_CLK_IN_SEL        16
#define VCLK_SOFT_RST          15
#define VCLK_DIV12_EN           4
#define VCLK_DIV6_EN            3
#define VCLK_DIV4_EN            2
#define VCLK_DIV2_EN            1
#define VCLK_DIV1_EN            0

#define HHI_VID_CLK_CNTL2                          0x65
#define HDMI_TX_PIXEL_GATE_VCLK  5
#define VDAC_GATE_VCLK           4
#define ENCL_GATE_VCLK           3
#define ENCP_GATE_VCLK           2
#define ENCT_GATE_VCLK           1
#define ENCI_GATE_VCLK           0

#define CLKCTRL_VID_CLK_CTRL                       0x0030
#define CLKCTRL_VID_CLK_CTRL2                      0x0031
#define CLKCTRL_VID_CLK_DIV                        0x0032
#define CLKCTRL_VIID_CLK_DIV                       0x0033
#define CLKCTRL_VIID_CLK_CTRL                      0x0034

/* ENC0 */
#define CLKCTRL_VID_CLK0_CTRL                      0x0030
#define CLKCTRL_VID_CLK0_CTRL2                     0x0031
#define CLKCTRL_VID_CLK0_DIV                       0x0032
#define CLKCTRL_VIID_CLK0_DIV                      0x0033
#define CLKCTRL_VIID_CLK0_CTRL                     0x0034
/* ENC1 */
#define CLKCTRL_VID_CLK1_CTRL                      0x0073
#define CLKCTRL_VID_CLK1_CTRL2                     0x0074
#define CLKCTRL_VID_CLK1_DIV                       0x0075
#define CLKCTRL_VIID_CLK1_DIV                      0x0076
#define CLKCTRL_VIID_CLK1_CTRL                     0x0077
/* ENC2 */
#define CLKCTRL_VID_CLK2_CTRL                      0x0078
#define CLKCTRL_VID_CLK2_CTRL2                     0x0079
#define CLKCTRL_VID_CLK2_DIV                       0x007a
#define CLKCTRL_VIID_CLK2_DIV                      0x007b
#define CLKCTRL_VIID_CLK2_CTRL                     0x007c

/* t5w */
#define HHI_VIID_CLK0_DIV                          0x0a0
#define HHI_VIID_CLK0_CTRL                         0x0a1
#define HHI_VID_CLK0_DIV                           0x0a2
#define HHI_VID_CLK0_CTRL                          0x0a3
#define HHI_VID_CLK0_CTRL2                         0x0a4

unsigned int vout_clk_read(unsigned int _reg);
void vout_clk_write(unsigned int _reg, unsigned int _value);
void vout_clk_setb(unsigned int _reg, unsigned int _value,
		   unsigned int _start, unsigned int _len);
unsigned int vout_clk_getb(unsigned int reg,
			   unsigned int _start, unsigned int _len);

unsigned int vout_vcbus_read(unsigned int _reg);
void vout_vcbus_write(unsigned int _reg, unsigned int _value);
void vout_vcbus_setb(unsigned int _reg, unsigned int _value,
		     unsigned int _start, unsigned int _len);
unsigned int vout_vcbus_getb(unsigned int reg,
			     unsigned int _start, unsigned int _len);

#endif
