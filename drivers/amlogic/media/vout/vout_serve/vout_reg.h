/*
 * drivers/amlogic/media/vout/vout_serve/vout_reg.h
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

#ifndef _VOUT_REG_H_
#define _VOUT_REG_H_
#include <linux/amlogic/iomap.h>

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

/* HIU */
#define HHI_VIID_CLK_DIV                           0x4a
#define DAC0_CLK_SEL           28
#define DAC1_CLK_SEL           24
#define DAC2_CLK_SEL           20
#define VCLK2_XD_RST           17
#define VCLK2_XD_EN            16
#define ENCL_CLK_SEL           12
#define VCLK2_XD                0

#define HHI_VID_CLK_DIV                            0x59
#define ENCI_CLK_SEL           28
#define ENCP_CLK_SEL           24
#define ENCT_CLK_SEL           20
#define VCLK_XD_RST            17
#define VCLK_XD_EN             16
#define ENCL_CLK_SEL           12
#define VCLK_XD1                8
#define VCLK_XD0                0

#define HHI_VID_CLK_CNTL2                          0x65
#define HDMI_TX_PIXEL_GATE_VCLK  5
#define VDAC_GATE_VCLK           4
#define ENCL_GATE_VCLK           3
#define ENCP_GATE_VCLK           2
#define ENCT_GATE_VCLK           1
#define ENCI_GATE_VCLK           0

static inline unsigned int vout_vcbus_read(unsigned int _reg)
{
	return aml_read_vcbus(_reg);
};

static inline void vout_vcbus_write(unsigned int _reg, unsigned int _value)
{
	aml_write_vcbus(_reg, _value);
};

static inline void vout_vcbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vout_vcbus_write(_reg, ((vout_vcbus_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

static inline unsigned int vout_vcbus_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return (vout_vcbus_read(reg) >> _start) & ((1L << _len) - 1);
}

static inline unsigned int vout_hiu_read(unsigned int _reg)
{
	return aml_read_hiubus(_reg);
};

static inline void vout_hiu_write(unsigned int _reg, unsigned int _value)
{
	aml_write_hiubus(_reg, _value);
};

static inline void vout_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	vout_hiu_write(_reg, ((vout_hiu_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

static inline unsigned int vout_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (vout_hiu_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

#endif
