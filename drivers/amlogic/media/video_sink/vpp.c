// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_sink/vpp.c
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/video_sink/vpp.h>

#include "videolog.h"
/* #define CONFIG_VIDEO_LOG */
#ifdef CONFIG_VIDEO_LOG
#define AMLOG
#endif
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>

#include "video_priv.h"

#define MAX_NONLINEAR_FACTOR    0x40
#define MAX_NONLINEAR_T_FACTOR    100

/* vpp filter coefficients */
#define COEF_BICUBIC         0
#define COEF_3POINT_TRIANGLE 1
#define COEF_4POINT_TRIANGLE 2
#define COEF_BILINEAR        3
#define COEF_2POINT_BILINEAR 4
#define COEF_BICUBIC_SHARP   5
#define COEF_3POINT_TRIANGLE_SHARP   6
#define COEF_3POINT_BSPLINE  7
#define COEF_4POINT_BSPLINE  8
#define COEF_3D_FILTER       9
#define COEF_NULL            0xff
#define TOTAL_FILTERS        10

#define COEF_BICUBIC_8TAP             0
#define COEF_LANCZOS_8TAP_A2          1
#define COEF_LANCZOS_8TAP_A3          2
#define COEF_LANCZOS_8TAP_A4          3
#define COEF_MITCHELL_NETRAVALI_8TAP  4
#define COEF_BSPLINE_8TAP             5
#define COEF_BILINEAR_8TAP            6

#define VPP_SPEED_FACTOR 0x110ULL
#define SUPER_SCALER_V_FACTOR  100
#define PPS_FRAC_BITS 24
#define PPS_INT_BITS 4

#define DATA_ALIGNED_4(x)  (((x) + 3) & ~3)
#define DATA_ALIGNED_DOWN_4(x)  ((x) & ~3)

struct filter_info_s {
	u32 cur_vert_filter;
	u32 cur_horz_filter;
	u32 last_vert_filter;
	u32 last_horz_filter;
	u32 scaler_filter_cnt;
};

#define PPS_COEF_NUM       (66 + 2)
#define PPS_COEF_8TAP_NUM  (132 + 2)
static unsigned int pps_coef = PPS_COEF_NUM;
static unsigned int pps_coef_8tap = PPS_COEF_8TAP_NUM;
static uint test_pps_h_coef[PPS_COEF_NUM] = {2, 33};
static uint test_pps_h_coef_8tap[PPS_COEF_8TAP_NUM] = {8, 33};
static uint test_pps_v_coef[PPS_COEF_NUM] = {2, 33};
static uint test_pps_v_coef_8tap[PPS_COEF_8TAP_NUM] = {8, 33};
static uint force_pps_hcoef_update;
static uint force_pps_vcoef_update;
uint load_pps_coef;
MODULE_PARM_DESC(force_pps_hcoef_update, "\n force_pps_hcoef_update\n");
module_param(force_pps_hcoef_update, uint, 0664);
MODULE_PARM_DESC(force_pps_vcoef_update, "\n force_pps_vcoef_update\n");
module_param(force_pps_vcoef_update, uint, 0664);
module_param_array(test_pps_h_coef, uint, &pps_coef, 0664);
MODULE_PARM_DESC(test_pps_h_coef, "\n test_pps_h_coef\n");
module_param_array(test_pps_v_coef, uint, &pps_coef, 0664);
MODULE_PARM_DESC(test_pps_v_coef, "\n test_pps_v_coef\n");
module_param_array(test_pps_h_coef_8tap, uint, &pps_coef_8tap, 0664);
MODULE_PARM_DESC(test_pps_h_coef_8tap, "\n test_pps_h_coef_8tap\n");
module_param_array(test_pps_v_coef_8tap, uint, &pps_coef_8tap, 0664);
MODULE_PARM_DESC(test_pps_v_coef_8tap, "\n test_pps_v_coef_8tap\n");

#define MAX_VD_LAYER 3
static struct filter_info_s gfilter[MAX_VD_LAYER];
const u32 vpp_filter_coefs_bicubic_sharp[] = {
	3,
	33,
	/* 0x01f80090, 0x01f80100, 0xff7f0200, 0xfe7f0300, */
	0x00800000, 0xff800100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8
};

const u32 vpp_filter_coefs_bicubic[] = {
	4,
	33,
	0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8
};

const u32 vpp_filter_coefs_bicubic_high_freq[] = {
	4,
	33,
	0x20402000, 0x007f0100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8
};

const u32 vpp_filter_coefs_bilinear[] = {
	4,
	33,
	0x00800000, 0x007e0200, 0x007c0400, 0x007a0600,
	0x00780800, 0x00760a00, 0x00740c00, 0x00720e00,
	0x00701000, 0x006e1200, 0x006c1400, 0x006a1600,
	0x00681800, 0x00661a00, 0x00641c00, 0x00621e00,
	0x00602000, 0x005e2200, 0x005c2400, 0x005a2600,
	0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,
	0x00503000, 0x004e3200, 0x004c3400, 0x004a3600,
	0x00483800, 0x00463a00, 0x00443c00, 0x00423e00,
	0x00404000
};

const u32 vpp_3d_filter_coefs_bilinear[] = {
	2,
	33,
	0x80000000, 0x7e020000, 0x7c040000, 0x7a060000,
	0x78080000, 0x760a0000, 0x740c0000, 0x720e0000,
	0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
	0x68180000, 0x661a0000, 0x641c0000, 0x621e0000,
	0x60200000, 0x5e220000, 0x5c240000, 0x5a260000,
	0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
	0x50300000, 0x4e320000, 0x4c340000, 0x4a360000,
	0x48380000, 0x463a0000, 0x443c0000, 0x423e0000,
	0x40400000
};

const u32 vpp_filter_coefs_3point_triangle[] = {
	3,
	33,
	0x40400000, 0x3f400100, 0x3d410200, 0x3c410300,
	0x3a420400, 0x39420500, 0x37430600, 0x36430700,
	0x35430800, 0x33450800, 0x32450900, 0x31450a00,
	0x30450b00, 0x2e460c00, 0x2d460d00, 0x2c470d00,
	0x2b470e00, 0x29480f00, 0x28481000, 0x27481100,
	0x26491100, 0x25491200, 0x24491300, 0x234a1300,
	0x224a1400, 0x214a1500, 0x204a1600, 0x1f4b1600,
	0x1e4b1700, 0x1d4b1800, 0x1c4c1800, 0x1b4c1900,
	0x1a4c1a00
};

/* point_num =4, filt_len =4, group_num = 64, [1 2 1] */
const u32 vpp_filter_coefs_4point_triangle[] = {
	4,
	33,
	0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
	0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
	0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
	0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
	0x18382808, 0x18382808, 0x17372909, 0x17372909,
	0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
	0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
	0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
	0x10303010
};

/*
 *4th order (cubic) b-spline
 *filt_cubic point_num =4, filt_len =4, group_num = 64, [1 5 1]
 */
const u32 vpp_filter_coefs_4point_bspline[] = {
	4,
	33,
	0x15561500, 0x14561600, 0x13561700, 0x12561800,
	0x11551a00, 0x11541b00, 0x10541c00, 0x0f541d00,
	0x0f531e00, 0x0e531f00, 0x0d522100, 0x0c522200,
	0x0b522300, 0x0b512400, 0x0a502600, 0x0a4f2700,
	0x094e2900, 0x084e2a00, 0x084d2b00, 0x074c2c01,
	0x074b2d01, 0x064a2f01, 0x06493001, 0x05483201,
	0x05473301, 0x05463401, 0x04453601, 0x04433702,
	0x04423802, 0x03413a02, 0x03403b02, 0x033f3c02,
	0x033d3d03
};

/*3rd order (quadratic) b-spline*/
/*filt_quadratic, point_num =3, filt_len =3, group_num = 64, [1 6 1] */
const u32 vpp_filter_coefs_3point_bspline[] = {
	3,
	33,
	0x40400000, 0x3e420000, 0x3c440000, 0x3a460000,
	0x38480000, 0x364a0000, 0x344b0100, 0x334c0100,
	0x314e0100, 0x304f0100, 0x2e500200, 0x2c520200,
	0x2a540200, 0x29540300, 0x27560300, 0x26570300,
	0x24580400, 0x23590400, 0x215a0500, 0x205b0500,
	0x1e5c0600, 0x1d5c0700, 0x1c5d0700, 0x1a5e0800,
	0x195e0900, 0x185e0a00, 0x175f0a00, 0x15600b00,
	0x14600c00, 0x13600d00, 0x12600e00, 0x11600f00,
	0x10601000
};

/*filt_triangle, point_num =3, filt_len =2.6, group_num = 64, [1 7 1] */
const u32 vpp_filter_coefs_3point_triangle_sharp[] = {
	3,
	33,
	0x40400000, 0x3e420000, 0x3d430000, 0x3b450000,
	0x3a460000, 0x38480000, 0x37490000, 0x354b0000,
	0x344c0000, 0x324e0000, 0x314f0000, 0x2f510000,
	0x2e520000, 0x2c540000, 0x2b550000, 0x29570000,
	0x28580000, 0x265a0000, 0x245c0000, 0x235d0000,
	0x215f0000, 0x20600000, 0x1e620000, 0x1d620100,
	0x1b620300, 0x19630400, 0x17630600, 0x15640700,
	0x14640800, 0x12640a00, 0x11640b00, 0x0f650c00,
	0x0d660d00
};

const u32 vpp_filter_coefs_2point_binilear[] = {
	2,
	33,
	0x80000000, 0x7e020000, 0x7c040000, 0x7a060000,
	0x78080000, 0x760a0000, 0x740c0000, 0x720e0000,
	0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
	0x68180000, 0x661a0000, 0x641c0000, 0x621e0000,
	0x60200000, 0x5e220000, 0x5c240000, 0x5a260000,
	0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
	0x50300000, 0x4e320000, 0x4c340000, 0x4a360000,
	0x48380000, 0x463a0000, 0x443c0000, 0x423e0000,
	0x40400000
};

static const u32 *filter_table[] = {
	vpp_filter_coefs_bicubic,
	vpp_filter_coefs_3point_triangle,
	vpp_filter_coefs_4point_triangle,
	vpp_filter_coefs_bilinear,
	vpp_filter_coefs_2point_binilear,
	vpp_filter_coefs_bicubic_sharp,
	vpp_filter_coefs_3point_triangle_sharp,
	vpp_filter_coefs_3point_bspline,
	vpp_filter_coefs_4point_bspline,
	vpp_3d_filter_coefs_bilinear
};

static int chroma_filter_table[] = {
	COEF_BICUBIC, /* bicubic */
	COEF_3POINT_TRIANGLE,
	COEF_4POINT_TRIANGLE,
	COEF_4POINT_TRIANGLE, /* bilinear */
	COEF_2POINT_BILINEAR,
	COEF_3POINT_TRIANGLE, /* bicubic_sharp */
	COEF_3POINT_TRIANGLE, /* 3point_triangle_sharp */
	COEF_3POINT_TRIANGLE, /* 3point_bspline */
	COEF_4POINT_TRIANGLE, /* 4point_bspline */
	COEF_3D_FILTER		  /* can not change */
};

const u32 vpp_filter_coeff_bicubic_8tap[] = {
	8,
	33,
	0x80, 0xff80, 0xfe80, 0xfd7f,
	0xfc7f, 0xfc7e, 0xfb7e, 0xfa7d,
	0xfa7b, 0xf97a, 0xf978, 0xf878,
	0xf876, 0xf874, 0xf773, 0xf771,
	0xf76f, 0xf76d, 0xf76b, 0xf768,
	0xf766, 0xf764, 0xf762, 0xf75f,
	0xf75d, 0xf75a, 0xf758, 0xf756,
	0xf753, 0xf750, 0xf84d, 0xf84b,
	0xf848,
	0x0, 0x1000000, 0x2000000, 0x4000000,
	0x5000000, 0x6000000, 0x8ff0000, 0xaff0000,
	0xcff0000, 0xeff0000, 0x10ff0000, 0x12fe0000,
	0x14fe0000, 0x16fe0000, 0x18fe0000, 0x1bfd0000,
	0x1dfd0000, 0x1ffd0000, 0x22fc0000, 0x25fc0000,
	0x27fc0000, 0x2afb0000, 0x2cfb0000, 0x2ffb0000,
	0x32fa0000, 0x35fa0000, 0x37fa0000, 0x3af90000,
	0x3df90000, 0x40f90000, 0x42f90000, 0x45f80000,
	0x48f80000
};

const u32 vpp_filter_coeff_lanczos_8tap_a2[] = {
	8,
	33,
	0x80, 0xff80, 0xfe80, 0xfd7f,
	0xfc7f, 0xfb7e, 0xfa7e, 0xf97d,
	0xf87c, 0xf87b, 0xf77a, 0xf778,
	0xf677, 0xf675, 0xf674, 0xf572,
	0xf570, 0xf56e, 0xf56c, 0xf46a,
	0xf568, 0xf466, 0xf364, 0xf561,
	0xf45f, 0xf45c, 0xf45a, 0xf557,
	0xf654, 0xf552, 0xf54f, 0xf64c,
	0xf749,
	0x0, 0x1000000, 0x3ff0000, 0x4000000,
	0x6ff0000, 0x7000000, 0x9ff0000, 0xbff0000,
	0xdff0000, 0xffe0000, 0x11fe0000, 0x13fe0000,
	0x15fe0000, 0x17fe0000, 0x19fd0000, 0x1cfd0000,
	0x1efd0000, 0x21fc0000, 0x23fc0000, 0x26fc0000,
	0x28fb0000, 0x2bfb0000, 0x2efb0000, 0x30fa0000,
	0x33fa0000, 0x36fa0000, 0x39f90000, 0x3bf90000,
	0x3ef80000, 0x41f80000, 0x44f80000, 0x47f70000,
	0x49f70000
};

const u32 vpp_filter_coeff_lanczos_8tap_a3[] = {
	8,
	33,
	0x80, 0xfe80, 0x1fd80, 0x1fb7f,
	0x1fa7f, 0x2f97f, 0x2f77e, 0x2f67d,
	0x3f57c, 0x3f47b, 0x3f37a, 0x4f279,
	0x3f178, 0x4f177, 0x4f075, 0x4f074,
	0x4ef72, 0x4ef70, 0x4ee6e, 0x5ee6c,
	0x4ee6a, 0x4ed68, 0x4ed66, 0x4ed64,
	0x4ed62, 0x4ed5f, 0x4ed5d, 0x4ed5b,
	0x4ee58, 0x3ee56, 0x4ee53, 0x4ee50,
	0x3ef4e,
	0x0, 0x2000000, 0x3ff0000, 0x5ff0100,
	0x7fe0100, 0x9feff00, 0xbfd0100, 0xdfd0100,
	0xffc0100, 0x12fc0000, 0x14fb0100, 0x16fa0100,
	0x19fa0100, 0x1bf90000, 0x1ef90000, 0x20f80000,
	0x23f70100, 0x25f70100, 0x28f60200, 0x2af50200,
	0x2df50200, 0x30f40300, 0x33f40200, 0x35f30300,
	0x38f30200, 0x3bf20300, 0x3ef10300, 0x40f10300,
	0x43f00300, 0x46f00300, 0x48ef0400, 0x4bef0400,
	0x4eef0300
};

const u32 vpp_filter_coeff_lanczos_8tap_a4[] = {
	8,
	33,
	0x80, 0x1fe80, 0x1fd80, 0xff02fb80,
	0xff02f97f, 0xff03f87f, 0xff03f67e, 0xff04f57d,
	0xff04f47d, 0xfe05f37c, 0xfe05f27b, 0xfd06f17a,
	0xfe06f078, 0xfe06ef77, 0xfe07ee76, 0xfe07ed74,
	0xfe07ec73, 0xfe07ec71, 0xfe08eb6f, 0xfe08eb6d,
	0xfe08eb6b, 0xfe08ea69, 0xfe08ea67, 0xfe08ea65,
	0xfe08ea63, 0xfe08ea61, 0xfe08ea5e, 0xfe08ea5c,
	0xfe08ea5a, 0xfe08ea57, 0xfe08ea55, 0xfe08ea52,
	0xfe08eb4f,
	0x0, 0x2ff0000, 0x4ff00ff, 0x6fe01ff,
	0x8fd0101, 0xafd01ff, 0xcfc0101, 0xefb0200,
	0x10fa0200, 0x13fa02ff, 0x15f90200, 0x18f803ff,
	0x1af70300, 0x1df703ff, 0x1ff603ff, 0x22f504ff,
	0x24f40400, 0x27f30400, 0x2af305fe, 0x2cf205ff,
	0x2ff105ff, 0x32f105ff, 0x34f006ff, 0x37ef06ff,
	0x3aef06fe, 0x3dee06fe, 0x3fed07ff, 0x42ed07fe,
	0x45ec07fe, 0x47ec07ff, 0x4aeb07ff, 0x4deb08fe,
	0x4feb08fe
};

const u32 vpp_filter_coeff_mitchell_netravali_8tap[] = {
	8,
	33,
	0x772, 0x672, 0x572, 0x471,
	0x470, 0x370, 0x26f, 0x16f,
	0x16e, 0x6e, 0x6c, 0xff6b,
	0xff69, 0xfe68, 0xfe67, 0xfd66,
	0xfd64, 0xfd62, 0xfc61, 0xfc5f,
	0xfc5e, 0xfc5b, 0xfc59, 0xfc57,
	0xfb56, 0xfb54, 0xfb52, 0xfb50,
	0xfb4e, 0xfb4b, 0xfb49, 0xfb47,
	0xfc44,
	0x7000000, 0x8000000, 0x9000000, 0xb000000,
	0xc000000, 0xd000000, 0xf000000, 0x10000000,
	0x12ff0000, 0x13ff0000, 0x15ff0000, 0x17ff0000,
	0x19ff0000, 0x1bff0000, 0x1dfe0000, 0x1ffe0000,
	0x21fe0000, 0x23fe0000, 0x25fe0000, 0x27fe0000,
	0x29fd0000, 0x2cfd0000, 0x2efd0000, 0x30fd0000,
	0x32fd0000, 0x35fc0000, 0x37fc0000, 0x39fc0000,
	0x3bfc0000, 0x3efc0000, 0x40fc0000, 0x42fc0000,
	0x44fc0000
};

const u32 vpp_filter_coeff_bspline_8tap[] = {
	8,
	33,
	0x1556, 0x1456, 0x1356, 0x1256,
	0x1254, 0x1154, 0x1054, 0xf54,
	0xe54, 0xe53, 0xd52, 0xc52,
	0xb52, 0xb51, 0xa50, 0xa4f,
	0x94f, 0x84e, 0x84d, 0x74c,
	0x74a, 0x64a, 0x649, 0x647,
	0x547, 0x546, 0x445, 0x443,
	0x442, 0x341, 0x340, 0x33f,
	0x33d,
	0x15000000, 0x16000000, 0x17000000, 0x18000000,
	0x1a000000, 0x1b000000, 0x1c000000, 0x1d000000,
	0x1e000000, 0x1f000000, 0x21000000, 0x22000000,
	0x23000000, 0x24000000, 0x26000000, 0x27000000,
	0x28000000, 0x2a000000, 0x2b000000, 0x2c010000,
	0x2e010000, 0x2f010000, 0x30010000, 0x32010000,
	0x33010000, 0x34010000, 0x36010000, 0x37020000,
	0x38020000, 0x3a020000, 0x3b020000, 0x3c020000,
	0x3d030000
};

const u32 vpp_filter_coeff_bilinear_8tap[] = {
	8,
	33,
	0x80, 0x7e, 0x7c, 0x7a,
	0x78, 0x76, 0x74, 0x72,
	0x70, 0x6e, 0x6c, 0x6a,
	0x68, 0x66, 0x64, 0x62,
	0x60, 0x5e, 0x5c, 0x5a,
	0x58, 0x56, 0x54, 0x52,
	0x50, 0x4e, 0x4c, 0x4a,
	0x48, 0x46, 0x44, 0x42,
	0x40,
	0x0, 0x2000000, 0x4000000, 0x6000000,
	0x8000000, 0xa000000, 0xc000000, 0xe000000,
	0x10000000, 0x12000000, 0x14000000, 0x16000000,
	0x18000000, 0x1a000000, 0x1c000000, 0x1e000000,
	0x20000000, 0x22000000, 0x24000000, 0x26000000,
	0x28000000, 0x2a000000, 0x2c000000, 0x2e000000,
	0x30000000, 0x32000000, 0x34000000, 0x36000000,
	0x38000000, 0x3a000000, 0x3c000000, 0x3e000000,
	0x40000000
};
static const u32 *hscaler_8tap_filter_table[] = {
	vpp_filter_coeff_bicubic_8tap,
	vpp_filter_coeff_lanczos_8tap_a2,
	vpp_filter_coeff_lanczos_8tap_a3,
	vpp_filter_coeff_lanczos_8tap_a4,
	vpp_filter_coeff_mitchell_netravali_8tap,
	vpp_filter_coeff_bspline_8tap,
	vpp_filter_coeff_bilinear_8tap,
};

static unsigned int sharpness1_sr2_ctrl_32d7 = 0x00181008;
MODULE_PARM_DESC(sharpness1_sr2_ctrl_32d7, "sharpness1_sr2_ctrl_32d7");
module_param(sharpness1_sr2_ctrl_32d7, uint, 0664);
/*0x3280 default val: 1920x1080*/
static unsigned int sharpness1_sr2_ctrl_3280 = 0xffffffff;
MODULE_PARM_DESC(sharpness1_sr2_ctrl_3280, "sharpness1_sr2_ctrl_3280");
module_param(sharpness1_sr2_ctrl_3280, uint, 0664);

static unsigned int vpp_filter_fix;
MODULE_PARM_DESC(vpp_filter_fix, "vpp_filter_fix");
module_param(vpp_filter_fix, uint, 0664);

#define MAX_COEFF_LEVEL 5
#define MAX_COEFF_LEVEL_SC2 7
static uint num_coeff_level = MAX_COEFF_LEVEL;
static uint vert_coeff_settings[MAX_COEFF_LEVEL] = {
	/* in:out */
	COEF_BICUBIC,
	/* ratio < 1 */
	COEF_BICUBIC_SHARP,
	/* ratio = 1 and phase = 0, */
	/* use for MBX without sharpness HW module, */
	/* TV use COEF_BICUBIC in function coeff */
	COEF_BICUBIC,
	/* ratio in (1~0.5) */
	COEF_3POINT_BSPLINE,
	/* ratio in [0.5~0.333) with pre-scaler on */
	/* this setting is sharpness/smooth balanced */
	/* if need more smooth(less sharp, could use */
	/* COEF_4POINT_BSPLINE or COEF_4POINT_TRIANGLE */
	COEF_4POINT_TRIANGLE,
	/* ratio <= 0.333 with pre-scaler on */
	/* this setting is most smooth */
};

static uint horz_coeff_settings[MAX_COEFF_LEVEL] = {
	/* in:out */
	COEF_BICUBIC,
	/* ratio < 1 */
	COEF_BICUBIC_SHARP,
	/* ratio = 1 and phase = 0, */
	/* use for MBX without sharpness HW module, */
	/* TV use COEF_BICUBIC in function coeff */
	COEF_BICUBIC,
	/* ratio in (1~0.5) */
	COEF_3POINT_BSPLINE,
	/* ratio in [0.5~0.333) with pre-scaler on */
	/* this setting is sharpness/smooth balanced */
	/* if need more smooth(less sharp, could use */
	/* COEF_4POINT_BSPLINE or COEF_4POINT_TRIANGLE */
	COEF_4POINT_TRIANGLE,
	/* ratio <= 0.333 with pre-scaler on */
	/* this setting is most smooth */
};

static uint hert_coeff_settings_sc2[MAX_COEFF_LEVEL_SC2] = {
	/* in:out */
	COEF_BICUBIC_8TAP,
	COEF_LANCZOS_8TAP_A2,
	COEF_LANCZOS_8TAP_A3,
	COEF_LANCZOS_8TAP_A4,
	COEF_MITCHELL_NETRAVALI_8TAP,
	/* reserved */
	COEF_BSPLINE_8TAP,
	COEF_BILINEAR_8TAP,
	/* reserved */
};

static uint coeff(uint *settings, uint ratio, uint phase,
		  bool interlace, int combing_lev)
{
	uint coeff_select = 0;
	uint coeff_type = 0;

	if (ratio >= (3 << 24)) {
		coeff_select = 4;
	} else if (ratio >= (2 << 24)) {
		coeff_select = 3;
	} else if (ratio > (1 << 24)) {
		coeff_select = 2;
	} else if (ratio == (1 << 24)) {
		if (phase == 0)
			coeff_select = 1;
		else
			coeff_select = 2;
	}
	coeff_type = settings[coeff_select];
	/* TODO: add future TV chips */
	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu() ||
	    is_meson_txlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (coeff_type == COEF_BICUBIC_SHARP)
			coeff_type = COEF_BICUBIC;
#endif
	} else {
		/*
		 *gxtvbb use dejaggy in SR0 to reduce intelace combing
		 *other chip no dejaggy, need switch to more blur filter
		 */
		if (interlace && coeff_select < 3 && vpp_filter_fix)
			coeff_type = COEF_4POINT_BSPLINE;
		/* use bicubic for static scene */
		if (combing_lev == 0)
			coeff_type = COEF_BICUBIC;
	}
	return coeff_type;
}

static uint coeff_sc2(uint *settings, uint ratio)
{
	uint coeff_select = 0;
	uint coeff_type = 0;

	if (ratio < (8 << 20))
		/* zoom out > 2 */
		coeff_select = COEF_LANCZOS_8TAP_A4;
	else if ((ratio <= (1 << 24)) && (ratio >= (8 << 20)))
		/* ration in >= 1/2 */
		coeff_select = COEF_LANCZOS_8TAP_A4;
	else if (ratio <= (2 << 24))
		/* zoom in >= 1/2*/
		coeff_select = COEF_LANCZOS_8TAP_A2;
	else if (ratio > (2 << 24))
		/* ratio in < 1/2*/
		coeff_select = COEF_LANCZOS_8TAP_A2;

	coeff_type = settings[coeff_select];
	return coeff_type;
}

/* vertical and horizontal coeff settings */
module_param_array(vert_coeff_settings, uint, &num_coeff_level, 0664);
MODULE_PARM_DESC(vert_coeff_settings, "\n vert_coeff_settings\n");

module_param_array(horz_coeff_settings, uint, &num_coeff_level, 0664);
MODULE_PARM_DESC(horz_coeff_settings, "\n horz_coeff_settings\n");

static bool vert_chroma_filter_en = true;
module_param(vert_chroma_filter_en, bool, 0664);
MODULE_PARM_DESC(vert_chroma_filter_en, "enable vertical chromafilter\n");

static bool vert_chroma_filter_force_en;
module_param(vert_chroma_filter_force_en, bool, 0664);
MODULE_PARM_DESC(vert_chroma_filter_force_en,
		 "force enable vertical chromafilter\n");

static uint vert_chroma_filter_limit = 480;
module_param(vert_chroma_filter_limit, uint, 0664);
MODULE_PARM_DESC(vert_chroma_filter_limit, "vertical chromafilter limit\n");

static uint num_chroma_filter = TOTAL_FILTERS;
module_param_array(chroma_filter_table, uint, &num_chroma_filter, 0664);
MODULE_PARM_DESC(chroma_filter_table, "\n chroma_filter_table\n");

static uint cur_vert_chroma_filter;
MODULE_PARM_DESC(cur_vert_chroma_filter, "cur_vert_chroma_filter");
module_param(cur_vert_chroma_filter, int, 0444);

static uint cur_vert_filter;
MODULE_PARM_DESC(cur_vert_filter, "cur_vert_filter");
module_param(cur_vert_filter, int, 0444);

static uint cur_horz_filter;
MODULE_PARM_DESC(cur_horz_filter, "cur_horz_filter");
module_param(cur_horz_filter, int, 0444);

static uint cur_skip_line;
MODULE_PARM_DESC(cur_skip_line, "cur_skip_line");
module_param(cur_skip_line, int, 0444);

static unsigned int super_scaler_v_ratio = 133;
MODULE_PARM_DESC(super_scaler_v_ratio, "super_scaler_v_ratio");
module_param(super_scaler_v_ratio, uint, 0664);

static u32 skip_policy = 0x81;
module_param(skip_policy, uint, 0664);
MODULE_PARM_DESC(skip_policy, "\n skip_policy\n");

static unsigned int scaler_filter_cnt_limit = 10;
MODULE_PARM_DESC(scaler_filter_cnt_limit, "scaler_filter_cnt_limit");
module_param(scaler_filter_cnt_limit, uint, 0664);

#ifdef TV_3D_FUNCTION_OPEN
static int force_filter_mode = 1;
MODULE_PARM_DESC(force_filter_mode, "force_filter_mode");
module_param(force_filter_mode, int, 0664);
#endif
/*temp disable sr for power test*/
bool super_scaler = true;
struct sr_info_s sr_info;
static unsigned int super_debug;
module_param(super_debug, uint, 0664);
MODULE_PARM_DESC(super_debug, "super_debug");

static unsigned int scaler_path_sel = SCALER_PATH_MAX;
module_param(scaler_path_sel, uint, 0664);
MODULE_PARM_DESC(scaler_path_sel, "scaler_path_sel");

static bool bypass_spscl0;
module_param(bypass_spscl0, bool, 0664);
MODULE_PARM_DESC(bypass_spscl0, "bypass_spscl0");

static bool bypass_spscl1;
module_param(bypass_spscl1, bool, 0664);
MODULE_PARM_DESC(bypass_spscl1, "bypass_spscl1");

static unsigned int vert_scaler_filter = 0xff;
module_param(vert_scaler_filter, uint, 0664);
MODULE_PARM_DESC(vert_scaler_filter, "vert_scaler_filter");

static unsigned int vert_chroma_scaler_filter = 0xff;
module_param(vert_chroma_scaler_filter, uint, 0664);
MODULE_PARM_DESC(vert_chroma_scaler_filter, "vert_chroma_scaler_filter");

static unsigned int horz_scaler_filter = 0xff;
module_param(horz_scaler_filter, uint, 0664);
MODULE_PARM_DESC(horz_scaler_filter, "horz_scaler_filter");

static unsigned int horz_scaler_filter_8tap = 0xff;
module_param(horz_scaler_filter_8tap, uint, 0664);
MODULE_PARM_DESC(horz_scaler_filter_8tap, "horz_scaler_filter_8tap");
/*need check this value,*/
static unsigned int bypass_ratio = 205;
module_param(bypass_ratio, uint, 0664);
MODULE_PARM_DESC(bypass_ratio, "bypass_ratio");

static unsigned int sr0_sr1_refresh = 1;
module_param(sr0_sr1_refresh, uint, 0664);
MODULE_PARM_DESC(sr0_sr1_refresh, "sr0_sr1_refresh");

static bool pre_scaler_en = true;
module_param(pre_scaler_en, bool, 0664);
MODULE_PARM_DESC(pre_scaler_en, "pre_scaler_en");

static unsigned int force_vskip_cnt;
MODULE_PARM_DESC(force_vskip_cnt, "force_vskip_cnt");
module_param(force_vskip_cnt, uint, 0664);

static unsigned int disable_adapted;
MODULE_PARM_DESC(disable_adapted, "disable_adapted");
module_param(disable_adapted, uint, 0664);

static u32 cur_nnhf_input_w;
static u32 cur_nnhf_input_h;

#define ZOOM_BITS       18
#define PHASE_BITS      8
/*
 *   when ratio for Y is 1:1
 *   Line #   In(P)   In(I)       Out(P)      Out(I)            Out(P)  Out(I)
 *   0        P_Y     IT_Y        P_Y          IT_Y
 *   1                                                          P_Y     IT_Y
 *   2                IB_Y                     IB_Y
 *   3                                                                  IB_Y
 *   4        P_Y     IT_Y        P_Y          IT_Y
 *   5                                                          P_Y     IT_Y
 *   6                IB_Y                     IB_Y
 *   7                                                                  IB_Y
 *   8        P_Y     IT_Y        P_Y          IT_Y
 *   9                                                          P_Y     IT_Y
 *  10                IB_Y                     IB_Y
 *  11                                                                  IB_Y
 *  12        P_Y     IT_Y        P_Y          IT_Y
 *                                                              P_Y     IT_Y
 */
/*
 *The table data sequence here is arranged according to
 * enum f2v_vphase_type_e enum,
 *  IT2IT, IB2IB, T2IB, IB2IT, P2IT, P2IB, IT2P, IB2P, P2P
 */
static const u8 f2v_420_in_pos[F2V_TYPE_MAX] = { 0, 2, 0, 2, 0, 0, 0, 2, 0 };
static const u8 f2v_420_out_pos1[F2V_TYPE_MAX] = { 0, 2, 2, 0, 0, 2, 0, 0, 0 };
static const u8 f2v_420_out_pos2[F2V_TYPE_MAX] = { 1, 3, 3, 1, 1, 3, 1, 1, 1 };

static void f2v_get_vertical_phase(u32 zoom_ratio,
				   u32 phase_adj,
				   struct f2v_vphase_s vphase[F2V_TYPE_MAX],
				   u32 interlace)
{
	enum f2v_vphase_type_e type;
	s32 offset_in, offset_out;
	s32 phase;
	const u8 *f2v_420_out_pos;

	if (interlace == 0 && (zoom_ratio > (1 << ZOOM_BITS)))
		f2v_420_out_pos = f2v_420_out_pos2;
	else
		f2v_420_out_pos = f2v_420_out_pos1;

	for (type = F2V_IT2IT; type < F2V_TYPE_MAX; type++) {
		offset_in = f2v_420_in_pos[type] << PHASE_BITS;
		offset_out =
			(f2v_420_out_pos[type] * zoom_ratio) >> (ZOOM_BITS -
					PHASE_BITS);

		if (offset_in > offset_out) {
			vphase[type].repeat_skip = -1;	/* repeat line */
			vphase[type].phase =
			((4 << PHASE_BITS) + offset_out - offset_in) >> 2;

		} else {
			vphase[type].repeat_skip = 0;	/* skip line */

			while ((offset_in + (4 << PHASE_BITS)) <=
				offset_out) {
				vphase[type].repeat_skip++;
				offset_in += 4 << PHASE_BITS;
			}

			vphase[type].phase = (offset_out - offset_in) >> 2;
		}

		phase = vphase[type].phase + phase_adj;

		if (phase > 0x100)
			vphase[type].repeat_skip++;

		vphase[type].phase = phase & 0xff;

		if (vphase[type].repeat_skip > 5)
			vphase[type].repeat_skip = 5;
	}
}

/* Trapezoid non-linear mode */
/* nonlinear_factor: 0x40: 1 : 1 */
static void calculate_non_linear_ratio_T
	(unsigned int nonlinear_t_factor,
	unsigned int width_in,
	unsigned int width_out,
	struct vpp_frame_par_s *next_frame_par)
{
	u32 ratio, r2_ratio, r1_ratio;
	u32 region2_hsize_B_max;
	u32 region2_hsize_A, region2_hsize_B;
	u32 region1_hsize;
	u32 phase_step, phase_slope;
	struct vppfilter_mode_s *vpp_filter =
		&next_frame_par->vpp_filter;

	ratio = (width_in << 18) / width_out;
	r2_ratio = (ratio * MAX_NONLINEAR_T_FACTOR /
		nonlinear_t_factor) << 6;
	if (r2_ratio < ratio) {
		pr_info("nonlinear_t_factor(%d) is too large!\n",
		nonlinear_t_factor);
		return;
	}
	region2_hsize_B_max = (((2 * width_in << 18) / r2_ratio) << 6) - width_out;
	region2_hsize_B = region2_hsize_B_max;
	region2_hsize_A = (region2_hsize_B * (r2_ratio >> 6)) >> 18;
	region1_hsize = (width_out - region2_hsize_B) / 2;
	r1_ratio = (((width_in - region2_hsize_A) << 18) /
		(width_out - region2_hsize_B)) << 6;
	phase_step = 2 * r1_ratio - r2_ratio;
	phase_slope = (r2_ratio - phase_step) / region1_hsize;
	vpp_filter->vpp_hf_start_phase_step = phase_step;
	vpp_filter->vpp_hf_start_phase_slope = phase_slope;
	vpp_filter->vpp_hf_end_phase_slope =
	vpp_filter->vpp_hf_start_phase_slope | 0x1000000;
	next_frame_par->VPP_hsc_linear_startp = region1_hsize - 1;
	next_frame_par->VPP_hsc_linear_endp =
		region1_hsize + region2_hsize_B - 1;
}

/*
 * V-shape non-linear mode
 */
static void calculate_non_linear_ratio_V
	(unsigned int nonlinear_factor,
	unsigned int middle_ratio,
	unsigned int width_out,
	struct vpp_frame_par_s *next_frame_par)
{
	unsigned int diff_ratio;
	struct vppfilter_mode_s *vpp_filter =
		&next_frame_par->vpp_filter;

	diff_ratio = middle_ratio * nonlinear_factor;
	vpp_filter->vpp_hf_start_phase_step = (middle_ratio << 6) - diff_ratio;
	vpp_filter->vpp_hf_start_phase_slope = diff_ratio * 4 / width_out;
	vpp_filter->vpp_hf_end_phase_slope =
		vpp_filter->vpp_hf_start_phase_slope | 0x1000000;
}
/*
 *We find that the minimum line the video can be scaled
 * down without skip line for different modes as below:
 * 4k2k mode:
 * video source		minimus line		ratio(height_in/height_out)
 * 3840 * 2160		1690			1.278
 * 1920 * 1080		860			1.256
 * 1280 * 720		390			1.846
 * 720 * 480		160			3.000
 * 1080p mode:
 * video source		minimus line		ratio(height_in/height_out)
 * 3840 * 2160		840			2.571
 * 1920 * 1080		430			2.511
 * 1280 * 720		200			3.600
 * 720 * 480		80			6.000
 * So the safe scal ratio is 1.25 for 4K2K mode and 2.5
 * (1.25 * 3840 / 1920) for 1080p mode.
 */
#define MIN_RATIO_1000	1250
static unsigned int min_skip_ratio = MIN_RATIO_1000;
MODULE_PARM_DESC(min_skip_ratio, "min_skip_ratio");
module_param(min_skip_ratio, uint, 0664);
static unsigned int max_proc_height = 2160;
MODULE_PARM_DESC(max_proc_height, "max_proc_height");
module_param(max_proc_height, uint, 0664);
static unsigned int cur_proc_height;
MODULE_PARM_DESC(cur_proc_height, "cur_proc_height");
module_param(cur_proc_height, uint, 0444);
static unsigned int cur_skip_ratio;
MODULE_PARM_DESC(cur_skip_ratio, "cur_skip_ratio");
module_param(cur_skip_ratio, uint, 0444);
static unsigned int cur_vf_type;
MODULE_PARM_DESC(cur_vf_type, "cur_vf_type");
module_param(cur_vf_type, uint, 0444);
static unsigned int cur_freq_ratio;
MODULE_PARM_DESC(cur_freq_ratio, "cur_freq_ratio");
module_param(cur_freq_ratio, uint, 0444);

static unsigned int custom_ar;
MODULE_PARM_DESC(custom_ar, "custom_ar");
module_param(custom_ar, uint, 0664);

static unsigned int force_use_ext_ar;
MODULE_PARM_DESC(force_use_ext_ar, "force_use_ext_ar");
module_param(force_use_ext_ar, uint, 0664);

static unsigned int force_no_compress;
MODULE_PARM_DESC(force_no_compress, "force_no_compress");
module_param(force_no_compress, uint, 0664);

static unsigned int hscaler_input_h_threshold = 60;
MODULE_PARM_DESC(hscaler_input_h_threshold, "hscaler_input_height_threshold");
module_param(hscaler_input_h_threshold, uint, 0664);

static int force_reshape_vskip_cnt;
MODULE_PARM_DESC(force_reshape_vskip_cnt, "\n force_reshape_vskip_cnt\n");
module_param(force_reshape_vskip_cnt, uint, 0664);
static unsigned int screen_ar_threshold = 3;

static unsigned int aisr_debug_flag;
MODULE_PARM_DESC(aisr_debug_flag, "aisr_debug_flag");
module_param(aisr_debug_flag, uint, 0664);
/*
 *test on txlx:
 *Time_out = (V_out/V_screen_total)/FPS_out;
 *if di bypas:
 *Time_in = (H_in * V_in)/Clk_vpu;
 *if di work; for di clk is less than vpu usually;
 *Time_in = (H_in * V_in)/Clk_di;
 *if Time_in < Time_out,need do vskip;
 *but in effect,test result may have some error.
 *ratio1:V_out test result may larger than calc result;
 *--after test is large ratio is 1.09;
 *--so wo should choose the largest ratio_v_out = 110/100;
 *ratio2:use clk_di or clk_vpu;
 *--txlx di clk is 250M or 500M;
 *--before txlx di clk is 333M;
 *So need adjust bypass_ratio;
 */

static int vpp_process_speed_check
	(u32 layer_id,
	s32 width_in,
	s32 height_in,
	s32 height_out,
	s32 height_screen,
	u32 video_speed_check_width,
	u32 video_speed_check_height,
	struct vpp_frame_par_s *next_frame_par,
	const struct vinfo_s *vinfo, struct vframe_s *vf,
	u32 vpp_flags)
{
	u32 cur_ratio, bpp = 1;
	int min_ratio_1000 = 0;
	int freq_ratio = 1;
	u32 sync_duration_den = 1;
	u32 vtotal, htotal = 0, clk_in_pps = 0, clk_vpu = 0, clk_temp;
	u32 input_time_us = 0, display_time_us = 0, dummy_time_us = 0;
	u32 width_out = 0;
	u32 vpu_clk = 0, max_height = 2160; /* 4k mode */

	if (!vf)
		return SPEED_CHECK_DONE;

	/* store the debug info for legacy */
	if (layer_id == 0 && (vpp_flags & VPP_FLAG_FROM_TOGGLE_FRAME))
		cur_vf_type = vf->type;

	if (force_vskip_cnt == 0xff)/*for debug*/
		return SPEED_CHECK_DONE;

	if (next_frame_par->vscale_skip_count < force_vskip_cnt)
		return SPEED_CHECK_VSKIP;

	if (vinfo->sync_duration_den >  0)
		sync_duration_den = vinfo->sync_duration_den;

	if (IS_DI_POST(vf->type)) {
		if (is_meson_txlx_cpu())
			clk_in_pps = 250000000;
		else
			clk_in_pps = 333000000;
	} else {
		clk_in_pps = vpu_clk_get();
	}

	next_frame_par->clk_in_pps = clk_in_pps;
	vpu_clk = vpu_clk_get();
	/* the output is only up to 1080p */
	if (vpu_clk <= 250000000) {
		/* ((3840 * 2160) / 1920) *  (vpu_clk / 1000000) / 666 */
		max_height =  4320 *  (vpu_clk / 1000000) / 666;
	}

	if (layer_id == 0 && (vpp_flags & VPP_FLAG_FROM_TOGGLE_FRAME)) {
		if (max_proc_height < max_height)
			max_height = max_proc_height;
		cur_proc_height = max_height;
	}

	if (width_in > 720)
		min_ratio_1000 =  min_skip_ratio;
	else
		min_ratio_1000 = 1750;

	if (vinfo->field_height < vinfo->height)
		vtotal = vinfo->vtotal / 2;
	else
		vtotal = vinfo->vtotal;

	/*according vlsi suggest,
	 *if di work need check mif input and vpu process speed
	 */
	if (IS_DI_POST(vf->type)) {
		htotal = vinfo->htotal;
		clk_vpu = vpu_clk_get();
		clk_temp = clk_in_pps / 1000000;
		if (clk_temp)
			input_time_us = height_in * width_in / clk_temp;
		clk_temp = clk_vpu / 1000000;
		width_out = next_frame_par->VPP_hsc_endp -
			next_frame_par->VPP_hsc_startp + 1;
		if (clk_temp)
			dummy_time_us = (vtotal * htotal -
			height_out * width_out) / clk_temp;
		display_time_us = 1000000 * sync_duration_den /
			vinfo->sync_duration_num;
		if (display_time_us > dummy_time_us)
			display_time_us = display_time_us - dummy_time_us;
		if (input_time_us > display_time_us)
			return SPEED_CHECK_VSKIP;
	}

	if ((vinfo->sync_duration_num / sync_duration_den) > 60)
		freq_ratio = vinfo->sync_duration_num /
			sync_duration_den / 60;

	if (freq_ratio < 1)
		freq_ratio = 1;
	if (layer_id == 0 && (vpp_flags & VPP_FLAG_FROM_TOGGLE_FRAME))
		cur_freq_ratio = freq_ratio;

	/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8) */
	if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) && !is_meson_mtvd_cpu()) {
		if (width_in <= 0 || height_in <= 0 ||
		    height_out <= 0 || height_screen <= 0)
			return SPEED_CHECK_DONE;

		if (next_frame_par->vscale_skip_count > 0 &&
		    ((vf->type & VIDTYPE_VIU_444) ||
		     (vf->type & VIDTYPE_RGB_444)))
			bpp = 2;
		if (height_in * bpp > height_out) {
			/*
			 *don't need do skip for under 5% scaler down
			 *reason:for 1080p input,4k output, if di clk is 250M,
			 *the clac height is 1119;which is bigger than 1080!
			 */
			if (height_in > height_out &&
			    ((height_in - height_out) < height_in / 20))
				return SPEED_CHECK_DONE;
			if (get_cpu_type() >=
				MESON_CPU_MAJOR_ID_GXBB) {
				cur_ratio = div_u64((u64)height_in *
					(u64)vinfo->height *
					1000 * freq_ratio,
					height_out * max_height);
				/* di process first, need more a bit of ratio */
				if (IS_DI_POST(vf->type))
					cur_ratio = (cur_ratio * 105) / 100;
				if (next_frame_par->vscale_skip_count > 0 &&
				    ((vf->type & VIDTYPE_VIU_444) ||
				     (vf->type & VIDTYPE_RGB_444)))
					cur_ratio = cur_ratio * 2;

				/* store the debug info for legacy */
				if (layer_id == 0 &&
				    (vpp_flags & VPP_FLAG_FROM_TOGGLE_FRAME))
					cur_skip_ratio = cur_ratio;

				if (cur_ratio > min_ratio_1000 &&
				    vf->source_type !=
				    VFRAME_SOURCE_TYPE_TUNER &&
				    vf->source_type !=
				    VFRAME_SOURCE_TYPE_CVBS)
					return SPEED_CHECK_VSKIP;
			}
			if (vf->type & VIDTYPE_VIU_422) {
				/*TODO vpu */
				if (height_out == 0 ||
				    div_u64((u64)VPP_SPEED_FACTOR *
					    (u64)width_in *
					    (u64)height_in *
					    (u64)vinfo->sync_duration_num *
					    (u64)vtotal,
					    height_out *
					    sync_duration_den *
					    bypass_ratio) > clk_in_pps)
					return SPEED_CHECK_VSKIP;
				else
					return SPEED_CHECK_DONE;
			} else {
				/*TODO vpu */
				if (height_out == 0 ||
				    div_u64((u64)VPP_SPEED_FACTOR *
					    (u64)width_in *
					    (u64)height_in *
					    (u64)vinfo->sync_duration_num *
					    (u64)vtotal,
					    height_out *
					    sync_duration_den * 256)
					    > clk_in_pps)
					return SPEED_CHECK_VSKIP;
				/* 4K down scaling to non 4K > 30hz,*/
				   /*skip lines for memory bandwidth */
				else if ((((vf->type &
					  VIDTYPE_COMPRESS) == 0) ||
					  (next_frame_par->nocomp)) &&
					(height_in > 2048) &&
					(height_out < 2048) &&
					(vinfo->sync_duration_num >
					(30 * sync_duration_den)) &&
					(get_cpu_type() !=
					MESON_CPU_MAJOR_ID_GXTVBB) &&
					(get_cpu_type() !=
					MESON_CPU_MAJOR_ID_GXM))
					return SPEED_CHECK_VSKIP;
				else
					return SPEED_CHECK_DONE;
			}
		} else if (next_frame_par->hscale_skip_count == 0) {
			/*TODO vpu */
			if (div_u64(VPP_SPEED_FACTOR * width_in *
				vinfo->sync_duration_num * height_screen,
				sync_duration_den * 256)
				> vpu_clk_get())
				return SPEED_CHECK_HSKIP;
			else
				return SPEED_CHECK_DONE;
		}
		return SPEED_CHECK_DONE;
	}
	/* #else */
	/* return okay if vpp preblend enabled */

	if ((READ_VCBUS_REG(VPP_MISC) & VPP_PREBLEND_EN) &&
	    (READ_VCBUS_REG(VPP_MISC) & VPP_OSD1_PREBLEND))
		return SPEED_CHECK_DONE;

	/* #if (MESON_CPU_TYPE > MESON_CPU_TYPE_MESON6) */
	if (get_cpu_type() > MESON_CPU_MAJOR_ID_M6) {
		if ((height_out + 1) > height_in)
			return SPEED_CHECK_DONE;
	} else {
		/* #else */
		if (video_speed_check_width * video_speed_check_height *
			height_out > height_screen * width_in * height_in)
			return SPEED_CHECK_DONE;
	}
	/* #endif */
	amlog_mask(LOG_MASK_VPP, "%s failed\n", __func__);
	return SPEED_CHECK_VSKIP;
}

static void horz_coef_print(u32 layer_id, struct vppfilter_mode_s *filter)
{
	int i;
	int bit9_mode = filter->vpp_horz_coeff[1] & 0x8000;

	if (bit9_mode) {
		if (hscaler_8tap_enable[layer_id]) {
			for (i = 0; i < (filter->vpp_horz_coeff[1]
				& 0xff); i++) {
				pr_info("horz 8tap 9bit coef[%d] = %x\n",
					i,
					filter->vpp_horz_coeff[i + 2]);
				}
			for (i = 0; i < (filter->vpp_horz_coeff[1]
				& 0xff); i++) {
				pr_info("horz 8tap 9bit coef[%d] = %x\n",
					i,
					filter->vpp_horz_coeff[i + 2 + 33]);
			}
			for (i = 0; i < (filter->vpp_horz_coeff[1]
				& 0xff); i++) {
				pr_info("horz 8tap 9bit coef[%d] = %x\n",
					i,
					filter->vpp_horz_coeff[i + 2 + 33 * 2]);
			}
			for (i = 0; i < (filter->vpp_horz_coeff[1]
				& 0xff); i++) {
				pr_info("horz 8tap 9bit coef[%d] = %x\n",
					i,
					filter->vpp_horz_coeff[i + 2 + 33 * 3]);
			}
		} else {
			for (i = 0; i < (filter->vpp_horz_coeff[1]
				& 0xff); i++) {
				pr_info("horz 9bit coef[%d] = %x\n",
					i,
					filter->vpp_horz_coeff[i + 2]);
				}
			for (i = 0; i < (filter->vpp_horz_coeff[1]
				& 0xff); i++) {
				pr_info("horz 9bit coef[%d] = %x\n",
					i,
					filter->vpp_horz_coeff[i + 2 + 33]);
			}
		}
	} else {
		if (hscaler_8tap_enable[layer_id]) {
			for (i = 0; i < (filter->vpp_horz_coeff[1]
				& 0xff); i++) {
				pr_info("horz 8tap coef[%d] = %x\n",
					i,
					filter->vpp_horz_coeff[i + 2]);
				}
			for (i = 0; i < (filter->vpp_horz_coeff[1]
				& 0xff); i++) {
				pr_info("horz 8tap coef[%d] = %x\n",
					i,
					filter->vpp_horz_coeff[i + 2 + 33]);
				}
		} else {
			for (i = 0; i < (filter->vpp_horz_coeff[1]
				& 0xff); i++) {
				pr_info("horz coef[%d] = %x\n",
					i,
					filter->vpp_horz_coeff[i + 2]);
			}
		}
	}
}

static void vert_coef_print(u32 layer_id, struct vppfilter_mode_s *filter)
{
	int i;
	int bit9_mode = filter->vpp_vert_coeff[1] & 0x8000;

	if (bit9_mode) {
		for (i = 0; i < (filter->vpp_vert_coeff[1] & 0xff); i++) {
			pr_info("vert 9bit coef[%d] = %x\n",
				i,
				filter->vpp_vert_coeff[i + 2]);
		}
		for (i = 0; i < (filter->vpp_vert_coeff[1] & 0xff); i++) {
			pr_info("vert 9bit coef[%d] = %x\n",
				i,
				filter->vpp_vert_coeff[i + 2 + 33]);
		}
	} else {
		for (i = 0; i < (filter->vpp_vert_coeff[1] & 0xff); i++) {
			pr_info("vert coef[%d] = %x\n",
				i,
				filter->vpp_vert_coeff[i + 2]);
		}
	}
}

static int vpp_set_filters_internal
	(struct disp_info_s *input,
	u32 width_in,
	u32 height_in,
	u32 wid_out,
	u32 hei_out,
	const struct vinfo_s *vinfo,
	u32 vpp_flags,
	struct vpp_frame_par_s *next_frame_par, struct vframe_s *vf)
{
	u32 screen_width = 0, screen_height = 0;
	s32 start, end;
	s32 video_top, video_left, temp;
	u32 video_width, video_height;
	u32 ratio_x = 0;
	u32 ratio_y = 0;
	u32 tmp_ratio_y = 0;
	int temp_width;
	int temp_height;
	struct vppfilter_mode_s *filter = &next_frame_par->vpp_filter;
	u32 wide_mode;
	s32 height_shift = 0;
	u32 height_after_ratio = 0;
	u32 aspect_factor;
	s32 ini_vphase;
	u32 w_in = width_in;
	u32 h_in = height_in;
	bool h_crop_enable = false, v_crop_enable = false;
	u32 width_out = wid_out;	/* vinfo->width; */
	u32 height_out = hei_out;	/* vinfo->height; */
	u32 aspect_ratio_out =
		(vinfo->aspect_ratio_den << 10) / vinfo->aspect_ratio_num;
	bool fill_match = true;
	u32 orig_aspect = 0;
	u32 screen_aspect = 0;
	bool skip_policy_check = true;
	u32 vskip_step;
	s32 video_layer_global_offset_x, video_layer_global_offset_y;
	u32 video_source_crop_top, video_source_crop_left;
	u32 video_source_crop_bottom, video_source_crop_right;
	u32 vpp_zoom_ratio, nonlinear_factor, nonlinear_t_factor;
	u32 speed_check_width, speed_check_height;
	s32 video_layer_top, video_layer_left;
	s32 video_layer_width, video_layer_height;
	u32 cur_custom_ar;
#ifdef TV_REVERSE
	bool reverse = false;
#endif
	int ret = vppfilter_success;
	u32 vert_chroma_filter;
	struct filter_info_s *cur_filter;
	s32 vpp_zoom_center_x, vpp_zoom_center_y;
	u32 crop_ratio = 1;
	u32 crop_left, crop_right, crop_top, crop_bottom;
	u32 sar_width = 0, sar_height = 0;
	bool ext_sar = false;
	bool no_compress = false;
	u32 min_aspect_ratio_out, max_aspect_ratio_out;
	u32 cur_super_debug = 0;
	int is_larger_4k50hz = 0;
	u32 src_width_max, src_height_max;
	bool afbc_support;
	bool crop_adjust = false;
	bool hskip_adjust = false;

	if (!input)
		return vppfilter_fail;

	if (vpp_flags & VPP_FLAG_MORE_LOG)
		cur_super_debug = super_debug;

	/* min = 0.95 x 1024 * height / width */
	min_aspect_ratio_out =
		((100 - screen_ar_threshold) << 10) / 100;
	min_aspect_ratio_out =
		(vinfo->height * min_aspect_ratio_out) / vinfo->width;
	/* max = 1.05 x 1024 * height / width */
	max_aspect_ratio_out =
		((100 + screen_ar_threshold) << 10) / 100;
	max_aspect_ratio_out =
		(vinfo->height * max_aspect_ratio_out) / vinfo->width;

	if (aspect_ratio_out <= max_aspect_ratio_out &&
	    aspect_ratio_out >= min_aspect_ratio_out)
		aspect_ratio_out = (vinfo->height << 10) / vinfo->width;

	cur_filter = &gfilter[input->layer_id];
	cur_custom_ar = input->custom_ar;
	vpp_zoom_ratio = input->zoom_ratio;
	vpp_zoom_center_x = input->zoom_center_x;
	vpp_zoom_center_y = input->zoom_center_y;
	speed_check_width = input->speed_check_width;
	speed_check_height = input->speed_check_height;
	nonlinear_factor = input->nonlinear_factor;
	nonlinear_t_factor = input->nonlinear_t_factor;
	video_layer_global_offset_x = input->global_offset_x;
	video_layer_global_offset_y = input->global_offset_y;

	video_layer_top = input->layer_top;
	video_layer_left = input->layer_left;
	video_layer_width = input->layer_width;
	video_layer_height = input->layer_height;
	src_width_max = input->src_width_max;
	src_height_max = input->src_height_max;
#ifdef TV_REVERSE
	reverse = input->reverse;
#endif

	if ((vf->type & VIDTYPE_MVC) ||
	    (input->proc_3d_type & MODE_3D_ENABLE)) {
		video_source_crop_left = 0;
		video_source_crop_right = 0;
		video_source_crop_top = 0;
		video_source_crop_bottom = 0;
	} else {
		video_source_crop_left = input->crop_left;
		video_source_crop_right = input->crop_right;
		video_source_crop_top = input->crop_top;
		video_source_crop_bottom = input->crop_bottom;
	}
	next_frame_par->crop_top = 0;
	next_frame_par->crop_bottom = 0;
	next_frame_par->crop_top_adjust = 0;
	next_frame_par->crop_bottom_adjust = 0;

#ifndef TV_3D_FUNCTION_OPEN
	next_frame_par->vscale_skip_count = 0;
	next_frame_par->hscale_skip_count = 0;
#endif
	next_frame_par->nocomp = false;
	if (vpp_flags & VPP_FLAG_INTERLACE_IN)
		next_frame_par->vscale_skip_count++;
	if (vpp_flags & VPP_FLAG_INTERLACE_OUT)
		height_shift++;

	if (vpp_flags & VPP_FLAG_INTERLACE_IN)
		vskip_step = 2;
#ifdef TV_3D_FUNCTION_OPEN
	else if ((next_frame_par->vpp_3d_mode
		== VPP_3D_MODE_LA) &&
		(input->proc_3d_type & MODE_3D_ENABLE))
		vskip_step = 2;
#endif
	else
		vskip_step = 1;

	if (cur_super_debug)
		pr_info("sar_width=%d, sar_height = %d, %d\n",
			vf->sar_width, vf->sar_height,
			force_use_ext_ar);

	/* width_in > src_width_max, hskip = 1 */
	/* hskip = 1, crop left must 4 aligned */
	if (width_in > src_width_max) {
		video_source_crop_left =
			(video_source_crop_left + 3) & ~0x03;
		video_source_crop_right =
			(video_source_crop_right + 3) & ~0x03;
	}

	if (is_bandwidth_policy_hit(input->layer_id))
		next_frame_par->vscale_skip_count++;

RESTART_ALL:
	crop_left = video_source_crop_left / crop_ratio;
	crop_right = video_source_crop_right / crop_ratio;
	crop_top = video_source_crop_top / crop_ratio;
	crop_bottom = video_source_crop_bottom / crop_ratio;

	/* fix both h/w crop odd issue */
	if (crop_adjust) {
		crop_left &= ~1;
		crop_top &= ~1;
		crop_right &= ~1;
		crop_bottom &= ~1;
		w_in = width_in;
		h_in = height_in;
		crop_adjust = false;
	}

	if (likely(w_in >
		(crop_left + crop_right)) &&
		(crop_left > 0 || crop_right > 0)) {
		w_in -= crop_left;
		w_in -= crop_right;
		h_crop_enable = true;
	}

	if (likely(h_in >
		(crop_top + crop_bottom)) &&
		(crop_top > 0 || crop_bottom > 0)) {
		h_in -= crop_top;
		h_in -= crop_bottom;
		v_crop_enable = true;
	}
	if (width_in > src_width_max)
		next_frame_par->hscale_skip_count++;
	if (height_in > src_height_max)
		next_frame_par->vscale_skip_count++;

RESTART:
	if (next_frame_par->hscale_skip_count && !hskip_adjust) {
		w_in  = DATA_ALIGNED_DOWN_4(w_in);
		crop_left = DATA_ALIGNED_4(crop_left);
		hskip_adjust = true;
	}

	aspect_factor = (vpp_flags & VPP_FLAG_AR_MASK) >> VPP_FLAG_AR_BITS;
	/* don't use input->wide_mode */
	wide_mode = (vpp_flags & VPP_FLAG_WIDEMODE_MASK) >> VPP_WIDEMODE_BITS;

	if ((vpp_flags & VPP_FLAG_AR_MASK) == VPP_FLAG_AR_MASK) {
		ext_sar = true;
		sar_width = vf->sar_width;
		sar_height = vf->sar_height;
	} else if (force_use_ext_ar) {
		ext_sar = true;
		sar_width = 1;
		sar_height = 1;
	}

	if (ext_sar && sar_width && sar_height && width_in) {
		aspect_factor =
			div_u64((u64)256ULL *
			(u64)sar_height *
			(u64)height_in,
			(u32)(sar_width * width_in));
	} else {
		ext_sar = false;
	}

	/* speical mode did not use ext sar mode */
	if (wide_mode == VIDEO_WIDEOPTION_NONLINEAR ||
	    wide_mode == VIDEO_WIDEOPTION_NORMAL_NOSCALEUP ||
	    wide_mode == VIDEO_WIDEOPTION_NONLINEAR_T)
		ext_sar = false;

	/* keep 8 bits resolution for aspect conversion */
	if (wide_mode == VIDEO_WIDEOPTION_4_3) {
		if (vpp_flags & VPP_FLAG_PORTRAIT_MODE)
			aspect_factor = 0x155;
		else
			aspect_factor = 0xc0;
		if (!(vpp_flags & VPP_FLAG_PORTRAIT_MODE) &&
		    (aspect_factor * video_layer_width == video_layer_height << 8))
			wide_mode = VIDEO_WIDEOPTION_FULL_STRETCH;
		else
			wide_mode = VIDEO_WIDEOPTION_NORMAL;
		ext_sar = false;
	} else if (wide_mode == VIDEO_WIDEOPTION_16_9) {
		if (vpp_flags & VPP_FLAG_PORTRAIT_MODE)
			aspect_factor = 0x1c7;
		else
			aspect_factor = 0x90;
		if (!(vpp_flags & VPP_FLAG_PORTRAIT_MODE) &&
		    (aspect_factor * video_layer_width == video_layer_height << 8))
			wide_mode = VIDEO_WIDEOPTION_FULL_STRETCH;
		else
			wide_mode = VIDEO_WIDEOPTION_NORMAL;
		ext_sar = false;
	} else if ((wide_mode >= VIDEO_WIDEOPTION_4_3_IGNORE) &&
		   (wide_mode <= VIDEO_WIDEOPTION_4_3_COMBINED)) {
		if (aspect_factor != 0xc0)
			fill_match = false;

		orig_aspect = aspect_factor;
		screen_aspect = 0xc0;
		ext_sar = false;
	} else if ((wide_mode >= VIDEO_WIDEOPTION_16_9_IGNORE) &&
		(wide_mode <= VIDEO_WIDEOPTION_16_9_COMBINED)) {
		if (aspect_factor != 0x90)
			fill_match = false;

		orig_aspect = aspect_factor;
		screen_aspect = 0x90;
		ext_sar = false;
	} else if (wide_mode == VIDEO_WIDEOPTION_CUSTOM) {
		if (cur_super_debug)
			pr_info("layer%d: wide_mode=%d, aspect_factor=%d, cur_custom_ar=%d\n",
				input->layer_id, wide_mode,
				aspect_factor, cur_custom_ar);
		if (cur_custom_ar != 0) {
			aspect_factor = cur_custom_ar & 0x3ff;
			ext_sar = false;
		}
		wide_mode = VIDEO_WIDEOPTION_NORMAL;
	} else if (wide_mode == VIDEO_WIDEOPTION_AFD) {
		if (aspect_factor == 0x90) {
			wide_mode = VIDEO_WIDEOPTION_FULL_STRETCH;
			ext_sar = false;
		} else {
			wide_mode = VIDEO_WIDEOPTION_NORMAL;
		}
	}
	/* if use the mode ar, will disable ext ar */

	if (cur_super_debug)
		pr_info("aspect_factor=%d,%d,%d,%d,%d,%d\n",
			aspect_factor, w_in, height_out,
			width_out, h_in, aspect_ratio_out >> 2);

	if (aspect_factor == 0 ||
	    wide_mode == VIDEO_WIDEOPTION_FULL_STRETCH ||
	    wide_mode == VIDEO_WIDEOPTION_NONLINEAR ||
	    wide_mode == VIDEO_WIDEOPTION_NONLINEAR_T) {
		aspect_factor = 0x100;
		height_after_ratio = h_in;
	} else if (ext_sar) {
		/* avoid the bit length overflow */
		u64 tmp = (u64)((u64)(width_out * width_in) * aspect_ratio_out);

		tmp = tmp >> 2;
		if (tmp != 0)
			height_after_ratio =
				div_u64((u64)256ULL *
					(u64)w_in *
					(u64)height_out *
					(u64)sar_height *
					(u64)height_in,
					(u32)tmp);
		height_after_ratio /= sar_width;
		aspect_factor = (height_after_ratio << 8) / h_in;
		if (cur_super_debug)
			pr_info("ext_sar: aspect_factor=%d, %d,%d,%d,%d,%d\n",
				aspect_factor, w_in, h_in,
				height_after_ratio,
				sar_width, sar_height);
	} else {
		/* avoid the bit length overflow */
		u64 tmp = (u64)((u64)(width_out * h_in) * aspect_ratio_out);

		tmp = tmp >> 2;
		if (tmp != 0)
			aspect_factor =
				div_u64((unsigned long long)w_in * height_out *
					(aspect_factor << 8),
					(u32)tmp);
		height_after_ratio = (h_in * aspect_factor) >> 8;
	}

	/*
	 *if we have ever set a cropped display area for video layer
	 * (by checking video_layer_width/video_height), then
	 * it will override the input width_out/height_out for
	 * ratio calculations, a.k.a we have a window for video content
	 */
	video_top = video_layer_top;
	video_left = video_layer_left;
	video_width = video_layer_width;
	video_height = video_layer_height;
	if (video_top == 0 &&
	    video_left == 0 &&
	    video_width <= 1 &&
	    video_height <= 1) {
		/* special case to do full screen display */
		video_width = width_out;
		video_height = height_out;
	} else {
		if (video_layer_width < 16 &&
		    video_layer_height < 16) {
			/*
			 *sanity check to move
			 *video out when the target size is too small
			 */
			video_width = width_out;
			video_height = height_out;
			video_left = width_out * 2;
		}
		video_top += video_layer_global_offset_y;
		video_left += video_layer_global_offset_x;
	}

	/*aspect ratio match */
	if (wide_mode >= VIDEO_WIDEOPTION_4_3_IGNORE &&
	    wide_mode <= VIDEO_WIDEOPTION_16_9_COMBINED &&
		orig_aspect) {
		if (!fill_match) {
			u32 screen_ratio_x, screen_ratio_y;

			screen_ratio_x = 1 << 18;
			screen_ratio_y = (orig_aspect << 18) / screen_aspect;

			switch (wide_mode) {
			case VIDEO_WIDEOPTION_4_3_LETTER_BOX:
			case VIDEO_WIDEOPTION_16_9_LETTER_BOX:
				screen_ratio_x =
					max(screen_ratio_x, screen_ratio_y);
				screen_ratio_y = screen_ratio_x;
				break;
			case VIDEO_WIDEOPTION_4_3_PAN_SCAN:
			case VIDEO_WIDEOPTION_16_9_PAN_SCAN:
				screen_ratio_x =
				min(screen_ratio_x, screen_ratio_y);
				screen_ratio_y = screen_ratio_x;
				break;
			case VIDEO_WIDEOPTION_4_3_COMBINED:
			case VIDEO_WIDEOPTION_16_9_COMBINED:
				screen_ratio_x =
				((screen_ratio_x + screen_ratio_y) >> 1);
				screen_ratio_y = screen_ratio_x;
				break;
			default:
				break;
			}

			ratio_x = screen_ratio_x * w_in / video_width;
			ratio_y =	screen_ratio_y * h_in / orig_aspect *
				screen_aspect / video_height;
		} else {
			screen_width = video_width * vpp_zoom_ratio / 100;
			screen_height = video_height * vpp_zoom_ratio / 100;

			ratio_x = (w_in << 18) / screen_width;
			ratio_y = (h_in << 18) / screen_height;
		}
	} else {
		screen_width = video_width * vpp_zoom_ratio / 100;
		screen_height = video_height * vpp_zoom_ratio / 100;

		ratio_x = (w_in << 18) / screen_width;
		/* check if overflow 0.5 pixel */
		if (screen_width * 2  < (w_in << 19) / ratio_x)
			ratio_x++;

		ratio_y = (height_after_ratio << 18) / screen_height;
		if (cur_super_debug)
			pr_info("layer%d: height_after_ratio=%d,%d,%d,%d,%d\n",
				input->layer_id,
				height_after_ratio, ratio_x, ratio_y,
				aspect_factor, wide_mode);

		if (wide_mode == VIDEO_WIDEOPTION_NORMAL) {
			ratio_x = max(ratio_x, ratio_y);
			ratio_y = ratio_x;
			if (ext_sar)
				ratio_y = (ratio_y * h_in) / height_after_ratio;
			else
				ratio_y = (ratio_y << 8) / aspect_factor;
		} else if (wide_mode == VIDEO_WIDEOPTION_NORMAL_NOSCALEUP) {
			u32 r1, r2;

			r1 = max(ratio_x, ratio_y);
			r2 = (r1 << 8) / aspect_factor;

			if ((r1 < (1 << 18)) || (r2 < (1 << 18))) {
				if (r1 < r2) {
					ratio_x = 1 << 18;
					ratio_y =
					(ratio_x << 8) / aspect_factor;
				} else {
					ratio_y = 1 << 18;
					ratio_x = aspect_factor << 10;
				}
			} else {
				ratio_x = r1;
				ratio_y = r2;
			}
		}
	}

	/* vertical */
	ini_vphase = vpp_zoom_center_y & 0xff;

	/* screen position for source */
	start = video_top + (video_height + 1) / 2 -
		((h_in << 17) +
		(vpp_zoom_center_y << 10) +
		(ratio_y >> 1)) / ratio_y;
	end = ((h_in << 18) + (ratio_y >> 1)) / ratio_y + start - 1;
	if (cur_super_debug)
		pr_info("layer%d: top:start =%d,%d,%d,%d  %d,%d,%d\n",
			input->layer_id,
			start, end, video_top,
			video_height, h_in, ratio_y, vpp_zoom_center_y);

#ifdef TV_REVERSE
	if (reverse) {
		/* calculate source vertical clip */
		if (video_top < 0) {
			if (start < 0) {
				temp = (-start * ratio_y) >> 18;
				next_frame_par->VPP_vd_end_lines_ =
					h_in - 1 - temp;
				next_frame_par->crop_bottom_adjust = temp;
			} else {
				next_frame_par->VPP_vd_end_lines_ = h_in - 1;
			}
		} else {
			if (start < video_top) {
				temp = ((video_top - start) * ratio_y) >> 18;
				next_frame_par->VPP_vd_end_lines_ =
					h_in - 1 - temp;
				next_frame_par->crop_bottom_adjust = temp;
			} else {
				next_frame_par->VPP_vd_end_lines_ = h_in - 1;
			}
		}
		temp = next_frame_par->VPP_vd_end_lines_ -
			(video_height * ratio_y >> 18);
		next_frame_par->VPP_vd_start_lines_ = (temp >= 0) ? temp : 0;
	} else {
#endif
		if (video_top < 0) {
			if (start < 0) {
				temp = (-start * ratio_y) >> 18;
				next_frame_par->VPP_vd_start_lines_ = temp;
			} else {
				next_frame_par->VPP_vd_start_lines_ = 0;
			}
			temp_height = min((video_top + video_height - 1),
					  (vinfo->height - 1));
		} else {
			if (start < video_top) {
				temp = ((video_top - start) * ratio_y) >> 18;
				next_frame_par->VPP_vd_start_lines_ = temp;
			} else {
				next_frame_par->VPP_vd_start_lines_ = 0;
			}
			temp_height = min((video_top + video_height - 1),
					  (vinfo->height - 1)) - video_top + 1;
		}
		temp = next_frame_par->VPP_vd_start_lines_ +
			(temp_height * ratio_y >> 18);
		next_frame_par->VPP_vd_end_lines_ =
			(temp <= (h_in - 1)) ? temp : (h_in - 1);
#ifdef TV_REVERSE
	}
#endif
	if (v_crop_enable) {
		next_frame_par->VPP_vd_start_lines_ += crop_top;
		next_frame_par->VPP_vd_end_lines_ += crop_top;
		next_frame_par->crop_top = crop_top;
		next_frame_par->crop_bottom = crop_bottom;
	}

	if (vpp_flags & VPP_FLAG_INTERLACE_IN)
		next_frame_par->VPP_vd_start_lines_ &= ~1;

	next_frame_par->VPP_pic_in_height_ =
		next_frame_par->VPP_vd_end_lines_ -
		next_frame_par->VPP_vd_start_lines_ + 1;
	next_frame_par->VPP_pic_in_height_ =
		next_frame_par->VPP_pic_in_height_ /
		(next_frame_par->vscale_skip_count + 1);
	/* DI POST link, need make pps input size is even */
	if ((next_frame_par->VPP_pic_in_height_ & 1) &&
	    IS_DI_POST(vf->type)) {
		next_frame_par->VPP_pic_in_height_ &= ~1;
		next_frame_par->VPP_vd_end_lines_ =
			next_frame_par->VPP_pic_in_height_ *
			(next_frame_par->vscale_skip_count + 1) +
			next_frame_par->VPP_vd_start_lines_;
		if (next_frame_par->VPP_vd_end_lines_ > 0)
			next_frame_par->VPP_vd_end_lines_--;
	}
	/*
	 *find overlapped region between
	 *[start, end], [0, height_out-1],
	 *[video_top, video_top+video_height-1]
	 */
	start = max(start, max(0, video_top));
	end = min(end, min((s32)(vinfo->height - 1),
			   (s32)(video_top + video_height - 1)));

	if (start >= end) {
		/* nothing to display */
		next_frame_par->VPP_vsc_startp = 0;
		next_frame_par->VPP_vsc_endp = 0;
	} else {
		next_frame_par->VPP_vsc_startp =
			(vpp_flags & VPP_FLAG_INTERLACE_OUT) ?
			(start >> 1) : start;
		next_frame_par->VPP_vsc_endp =
			(vpp_flags & VPP_FLAG_INTERLACE_OUT) ?
			(end >> 1) : end;
	}

	/* set filter co-efficients */
	tmp_ratio_y = ratio_y;
	ratio_y <<= height_shift;
	ratio_y = ratio_y / (next_frame_par->vscale_skip_count + 1);

	filter->vpp_vsc_start_phase_step = ratio_y << 6;

	f2v_get_vertical_phase(ratio_y, ini_vphase,
			       next_frame_par->VPP_vf_ini_phase_,
			       vpp_flags & VPP_FLAG_INTERLACE_OUT);

	/* horizontal */
	filter->vpp_hf_start_phase_slope = 0;
	filter->vpp_hf_end_phase_slope = 0;
	filter->vpp_hf_start_phase_step = ratio_x << 6;

	next_frame_par->VPP_hsc_linear_startp = next_frame_par->VPP_hsc_startp;
	next_frame_par->VPP_hsc_linear_endp = next_frame_par->VPP_hsc_endp;

	filter->vpp_hsc_start_phase_step = ratio_x << 6;
	next_frame_par->VPP_hf_ini_phase_ = vpp_zoom_center_x & 0xff;

	/* screen position for source */
	start = video_left + (video_width + 1) / 2 - ((w_in << 17) +
		(vpp_zoom_center_x << 10) +
		(ratio_x >> 1)) / ratio_x;
	end = ((w_in << 18) + (ratio_x >> 1)) / ratio_x + start - 1;
	if (cur_super_debug)
		pr_info("layer%d: left:start =%d,%d,%d,%d  %d,%d,%d\n",
			input->layer_id,
			start, end, video_left,
			video_width, w_in, ratio_x, vpp_zoom_center_x);

	/* calculate source horizontal clip */
#ifdef TV_REVERSE
	if (reverse) {
		if (video_left < 0) {
			if (start < 0) {
				temp = (-start * ratio_x) >> 18;
				next_frame_par->VPP_hd_end_lines_ =
					w_in - 1 - temp;
			} else {
				next_frame_par->VPP_hd_end_lines_ = w_in - 1;
			}
		} else {
			if (start < video_left) {
				temp = ((video_left - start) * ratio_x) >> 18;
				next_frame_par->VPP_hd_end_lines_ =
					w_in - 1 - temp;
			} else {
				next_frame_par->VPP_hd_end_lines_ = w_in - 1;
			}
		}
		temp = next_frame_par->VPP_hd_end_lines_ -
			(video_width * ratio_x >> 18);
		next_frame_par->VPP_hd_start_lines_ = (temp >= 0) ? temp : 0;
	} else {
#endif
		if (video_left < 0) {
			if (start < 0) {
				temp = (-start * ratio_x) >> 18;
				next_frame_par->VPP_hd_start_lines_ = temp;
			} else {
				next_frame_par->VPP_hd_start_lines_ = 0;
			}
			temp_width = min((video_left + video_width - 1),
					 (vinfo->width - 1));
		} else {
			if (start < video_left) {
				temp = ((video_left - start) * ratio_x) >> 18;
				next_frame_par->VPP_hd_start_lines_ = temp;
			} else {
				next_frame_par->VPP_hd_start_lines_ = 0;
			}
			temp_width = min((video_left + video_width - 1),
					 (vinfo->width - 1)) - video_left + 1;
		}
		temp = next_frame_par->VPP_hd_start_lines_ +
			(temp_width * ratio_x >> 18);
		next_frame_par->VPP_hd_end_lines_ =
			(temp <= (w_in - 1)) ? temp : (w_in - 1);
#ifdef TV_REVERSE
	}
#endif
	if (h_crop_enable) {
		next_frame_par->VPP_hd_start_lines_ += crop_left;
		next_frame_par->VPP_hd_end_lines_ += crop_left;
	}

	next_frame_par->VPP_line_in_length_ =
		next_frame_par->VPP_hd_end_lines_ -
		next_frame_par->VPP_hd_start_lines_ + 1;
	/*
	 *find overlapped region between
	 * [start, end], [0, width_out-1],
	 * [video_left, video_left+video_width-1]
	 */
	start = max(start, max(0, video_left));
	end = min(end,
		  min((s32)(vinfo->width - 1),
		      (s32)(video_left + video_width - 1)));

	if (start >= end) {
		/* nothing to display */
		next_frame_par->VPP_hsc_startp = 0;
		next_frame_par->VPP_hsc_endp = 0;
		/* avoid mif set wrong or di out size overflow */
		next_frame_par->VPP_hd_start_lines_ = 0;
		next_frame_par->VPP_hd_end_lines_ = 0;
		next_frame_par->VPP_line_in_length_ = 0;
	} else {
		next_frame_par->VPP_hsc_startp = start;

		next_frame_par->VPP_hsc_endp = end;
	}
	if (wide_mode == VIDEO_WIDEOPTION_NONLINEAR &&
	    end > start) {
		calculate_non_linear_ratio_V
			(nonlinear_factor,
			ratio_x, end - start,
			next_frame_par);
		next_frame_par->VPP_hsc_linear_startp =
		next_frame_par->VPP_hsc_linear_endp = (start + end) / 2;
	}
	if (wide_mode == VIDEO_WIDEOPTION_NONLINEAR_T &&
	    end > start) {
		calculate_non_linear_ratio_T(nonlinear_t_factor,
			w_in, end - start + 1,
			next_frame_par);
	}
	/* speical mode did not use aisr */
	/* 3d not use aisr */
	if (wide_mode == VIDEO_WIDEOPTION_NONLINEAR ||
	    wide_mode == VIDEO_WIDEOPTION_NORMAL_NOSCALEUP ||
	    wide_mode == VIDEO_WIDEOPTION_NONLINEAR_T ||
	    process_3d_type) {
		cur_dev->aisr_enable = 0;
		cur_dev->pps_auto_calc = 0;
	}
	/*
	 *check the painful bandwidth limitation and see
	 * if we need skip half resolution on source side for progressive
	 * frames.
	 */
	/* check vskip and hskip */
	if ((next_frame_par->vscale_skip_count < MAX_VSKIP_COUNT ||
	     !next_frame_par->hscale_skip_count) &&
	    (!(vpp_flags & VPP_FLAG_VSCALE_DISABLE))) {
		int skip = vpp_process_speed_check
			(input->layer_id,
			(next_frame_par->VPP_hd_end_lines_ -
			 next_frame_par->VPP_hd_start_lines_ + 1) /
			(next_frame_par->hscale_skip_count + 1),
			(next_frame_par->VPP_vd_end_lines_ -
			next_frame_par->VPP_vd_start_lines_ + 1) /
			(next_frame_par->vscale_skip_count + 1),
			(next_frame_par->VPP_vsc_endp -
			next_frame_par->VPP_vsc_startp + 1),
			vinfo->height >>
			((vpp_flags & VPP_FLAG_INTERLACE_OUT) ? 1 : 0),
			speed_check_width,
			speed_check_height,
			next_frame_par,
			vinfo, vf, vpp_flags);

		if (skip == SPEED_CHECK_VSKIP) {
			u32 next_vskip =
				next_frame_par->vscale_skip_count + vskip_step;

			if (next_vskip <= MAX_VSKIP_COUNT) {
				next_frame_par->vscale_skip_count = next_vskip;
				goto RESTART;
			} else {
				next_frame_par->hscale_skip_count = 1;
			}
		} else if (skip == SPEED_CHECK_HSKIP) {
			next_frame_par->hscale_skip_count = 1;
		}
	}

	if ((vf->type & VIDTYPE_COMPRESS) &&
	    !(vf->type & VIDTYPE_NO_DW) &&
	    !next_frame_par->nocomp &&
	    vf->canvas0Addr != 0 &&
	    (!IS_DI_POSTWRTIE(vf->type) ||
	     vf->flag & VFRAME_FLAG_DI_DW)) {
		if (vd1_vd2_mux)
			afbc_support = 0;
		else
			afbc_support = input->afbc_support;
		if ((vpp_flags & VPP_FLAG_FORCE_NO_COMPRESS) ||
		    next_frame_par->vscale_skip_count > 1 ||
		    !afbc_support ||
		    force_no_compress)
			no_compress = true;
	} else {
		no_compress = false;
	}

	if (no_compress) {
		if ((vpp_flags & VPP_FLAG_MORE_LOG) &&
		    input->afbc_support)
			pr_info
			("layer%d: Try DW buffer for compress frame.\n",
			input->layer_id);

		/* for VIDTYPE_COMPRESS, check if we can use double write
		 * buffer when primary frame can not be scaled.
		 */
		next_frame_par->nocomp = true;
		if (input->proc_3d_type & MODE_3D_ENABLE) {
			w_in = (width_in * vf->width) / vf->compWidth;
			width_in = w_in;
			h_in = (height_in * vf->height) / vf->compHeight;
			height_in = h_in;
		} else {
			w_in = vf->width;
			width_in = vf->width;
			h_in = vf->height;
			height_in = vf->height;
		}
		next_frame_par->hscale_skip_count = 0;
		next_frame_par->vscale_skip_count = 0;
		if (vf->width && vf->compWidth)
			crop_ratio = vf->compWidth / vf->width;
		goto RESTART_ALL;
	}

	if ((skip_policy & 0xf0) && skip_policy_check) {
		skip_policy_check = false;
		if (skip_policy & 0x40) {
			next_frame_par->vscale_skip_count = skip_policy & 0xf;
			goto RESTART;
		} else if (skip_policy & 0x80) {
			if (((vf->width >= 4096 &&
			      (!(vf->type & VIDTYPE_COMPRESS))) ||
			     (vf->flag & VFRAME_FLAG_HIGH_BANDWIDTH)) &&
			    next_frame_par->vscale_skip_count == 0) {
				next_frame_par->vscale_skip_count =
				skip_policy & 0xf;
				goto RESTART;
			}
		}
	}

	next_frame_par->video_input_h = next_frame_par->VPP_vd_end_lines_ -
		next_frame_par->VPP_vd_start_lines_ + 1;
	next_frame_par->video_input_h = next_frame_par->video_input_h /
		(next_frame_par->vscale_skip_count + 1);
	next_frame_par->video_input_w = next_frame_par->VPP_hd_end_lines_ -
		next_frame_par->VPP_hd_start_lines_ + 1;
	next_frame_par->video_input_w = next_frame_par->video_input_w /
		(next_frame_par->hscale_skip_count + 1);

	filter->vpp_hsc_start_phase_step = ratio_x << 6;

	/* coeff selection before skip and apply pre_scaler */
	filter->vpp_vert_filter =
		coeff(vert_coeff_settings,
		      filter->vpp_vsc_start_phase_step *
		      (next_frame_par->vscale_skip_count + 1),
		      1,
		      ((vf->type_original & VIDTYPE_TYPEMASK)
			!= VIDTYPE_PROGRESSIVE),
		      vf->combing_cur_lev);

	filter->vpp_vert_coeff =
		filter_table[filter->vpp_vert_filter];
	if (filter->vpp_vert_filter == COEF_BICUBIC &&
		input->ver_coef_adjust)
		filter->vpp_vert_coeff = vpp_filter_coefs_bicubic_high_freq;
	/* when local interlace or AV or ATV */
	/* TODO: add 420 check for local */
	if (vert_chroma_filter_force_en ||
	    (vert_chroma_filter_en &&
	    ((vf->source_type == VFRAME_SOURCE_TYPE_OTHERS &&
	     (((vf->type_original & VIDTYPE_TYPEMASK) !=
		VIDTYPE_PROGRESSIVE) ||
		vf->height < vert_chroma_filter_limit)) ||
	      vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
	      vf->source_type == VFRAME_SOURCE_TYPE_TUNER))) {
		vert_chroma_filter =
			chroma_filter_table[filter->vpp_vert_filter];
		filter->vpp_vert_chroma_coeff =
			filter_table[vert_chroma_filter];
		filter->vpp_vert_chroma_filter_en = true;
	} else {
		vert_chroma_filter = COEF_NULL;
		filter->vpp_vert_chroma_filter_en = false;
	}

	/* avoid hscaler fitler adjustion affect on picture shift*/
	if (hscaler_8tap_enable[input->layer_id])
		filter->vpp_horz_filter =
			coeff_sc2(hert_coeff_settings_sc2,
				  filter->vpp_hf_start_phase_step);
	else
		filter->vpp_horz_filter =
			coeff(horz_coeff_settings,
			      filter->vpp_hf_start_phase_step,
			      next_frame_par->VPP_hf_ini_phase_,
			      ((vf->type_original & VIDTYPE_TYPEMASK)
			       != VIDTYPE_PROGRESSIVE),
			      vf->combing_cur_lev);
	/*for gxl cvbs out index*/
	if (vinfo->mode == VMODE_CVBS &&
	    (filter->vpp_hf_start_phase_step == (1 << 24))) {
		if (hscaler_8tap_enable[input->layer_id])
			filter->vpp_horz_filter =
				COEF_BICUBIC_8TAP;
		else
			filter->vpp_horz_filter =
				COEF_BICUBIC_SHARP;
	}
	if (hscaler_8tap_enable[input->layer_id]) {
		/* hscaler 8 tap */
		filter->vpp_horz_coeff =
			hscaler_8tap_filter_table
			[filter->vpp_horz_filter];
	} else {
		filter->vpp_horz_coeff =
			filter_table[filter->vpp_horz_filter];
	}
	/* apply line skip */
	if (next_frame_par->hscale_skip_count) {
		filter->vpp_hf_start_phase_step >>= 1;
		filter->vpp_hsc_start_phase_step >>= 1;
		next_frame_par->VPP_line_in_length_ >>= 1;
	}

	/*pre hsc&vsc in pps for scaler down*/
	if ((filter->vpp_hf_start_phase_step >= 0x2000000 &&
	    filter->vpp_vsc_start_phase_step >= 0x2000000 &&
	    filter->vpp_hsc_start_phase_step == filter->vpp_hf_start_phase_step &&
	    pre_scaler_en) ||
	    pre_scaler[input->layer_id].force_pre_scaler) {
		filter->vpp_pre_vsc_en = 1;
		filter->vpp_vsc_start_phase_step >>=
			pre_scaler[input->layer_id].pre_vscaler_rate;
		ratio_y >>= pre_scaler[input->layer_id].pre_vscaler_rate;
		f2v_get_vertical_phase(ratio_y, ini_vphase,
				       next_frame_par->VPP_vf_ini_phase_,
				       vpp_flags & VPP_FLAG_INTERLACE_OUT);
	} else {
		filter->vpp_pre_vsc_en = 0;
	}
	if ((filter->vpp_hf_start_phase_step >= 0x2000000 &&
	    filter->vpp_hsc_start_phase_step == filter->vpp_hf_start_phase_step &&
	    pre_scaler_en) ||
	    pre_scaler[input->layer_id].force_pre_scaler) {
		filter->vpp_pre_hsc_en = 1;
		filter->vpp_hf_start_phase_step >>=
			pre_scaler[input->layer_id].pre_hscaler_rate;
		filter->vpp_hsc_start_phase_step >>=
			pre_scaler[input->layer_id].pre_hscaler_rate;
	} else {
		filter->vpp_pre_hsc_en = 0;

	if (filter->vpp_hsc_start_phase_step == 0x1000000 &&
	   filter->vpp_vsc_start_phase_step == 0x1000000 &&
	   h_crop_enable && v_crop_enable) {
		int w, h;

		w = next_frame_par->VPP_hd_end_lines_ -
			next_frame_par->VPP_hd_start_lines_ + 1;
		h = next_frame_par->VPP_vd_end_lines_ -
			next_frame_par->VPP_vd_start_lines_ + 1;
		if (cur_super_debug) {
			pr_info("%s:crop info=%d,%d,%d,%d\n",
				__func__, crop_left, crop_top, crop_right, crop_bottom);
			pr_info("%s:w_in=%d, h_in=%d, w=%x, h=%x\n",
				__func__, w_in, h_in, w, h);
		}
		if ((w & 1) && (h & 1)) {
			crop_adjust = true;
			h_crop_enable = false;
			v_crop_enable = false;
			goto RESTART_ALL;
		}
	}

	/* vscaler enable
	 * vout 4k 50hz
	 * video src heiht >= 2160*60%
	 * 4tap pre-hscaler bandwidth issue, need used old pre hscaler
	 */
	}
	if (vinfo->sync_duration_den) {
		if (vinfo->width >= 3840 &&
		    vinfo->height >= 2160 &&
		    (vinfo->sync_duration_num /
		    vinfo->sync_duration_den >= 50))
			is_larger_4k50hz = 1;
	}
	if (pre_scaler[input->layer_id].pre_hscaler_ntap_set == 0xff) {
		if (filter->vpp_pre_hsc_en &&
		    is_larger_4k50hz &&
		    (height_in >= 2160 * hscaler_input_h_threshold / 100) &&
		    filter->vpp_vsc_start_phase_step != 0x1000000)
			pre_scaler[input->layer_id].pre_hscaler_ntap_enable = 0;
		else
			pre_scaler[input->layer_id].pre_hscaler_ntap_enable = 1;
	} else {
		pre_scaler[input->layer_id].pre_hscaler_ntap_enable =
			pre_scaler[input->layer_id].pre_hscaler_ntap_set;
	}
	next_frame_par->VPP_hf_ini_phase_ = vpp_zoom_center_x & 0xff;

	/* overwrite filter setting for interlace output*/
	/* TODO: not reasonable when 4K input to 480i output */
	if (vpp_flags & VPP_FLAG_INTERLACE_OUT) {
		filter->vpp_vert_coeff = filter_table[COEF_BILINEAR];
		filter->vpp_vert_filter = COEF_BILINEAR;
	}

	/* force overwrite filter setting */
	if (vert_scaler_filter <= COEF_3D_FILTER) {
		filter->vpp_vert_coeff = filter_table[vert_scaler_filter];
		filter->vpp_vert_filter = vert_scaler_filter;
	}
	if (vert_chroma_filter_force_en &&
	    vert_chroma_scaler_filter <= COEF_3D_FILTER) {
		vert_chroma_filter = vert_chroma_scaler_filter;
		filter->vpp_vert_chroma_coeff =
			filter_table[vert_chroma_filter];
		filter->vpp_vert_chroma_filter_en = true;
	} else {
		vert_chroma_filter = COEF_NULL;
		filter->vpp_vert_chroma_filter_en = false;
	}

	if (hscaler_8tap_enable[input->layer_id] &&
	    horz_scaler_filter_8tap <= COEF_BSPLINE_8TAP) {
		/* hscaler 8 tap */
		filter->vpp_horz_coeff =
			hscaler_8tap_filter_table
			[horz_scaler_filter_8tap];
		filter->vpp_horz_filter =
			horz_scaler_filter_8tap;
	} else if (horz_scaler_filter <= COEF_3D_FILTER) {
		filter->vpp_horz_coeff =
			filter_table[horz_scaler_filter];
		filter->vpp_horz_filter = horz_scaler_filter;
	}

#ifdef TV_3D_FUNCTION_OPEN
	/* final stage for 3D filter overwrite */
	if (next_frame_par->vpp_3d_scale && force_filter_mode) {
		filter->vpp_vert_coeff = filter_table[COEF_3D_FILTER];
		filter->vpp_vert_filter = COEF_3D_FILTER;
	}
#endif

	if (force_pps_hcoef_update) {
		if (hscaler_8tap_enable[input->layer_id])
			filter->vpp_horz_coeff = test_pps_h_coef_8tap;
		else
			filter->vpp_horz_coeff = test_pps_h_coef;
		if (load_pps_coef)
			horz_coef_print(input->layer_id, filter);
	}
	if (force_pps_vcoef_update) {
		filter->vpp_vert_coeff = test_pps_v_coef;
		if (load_pps_coef)
			vert_coef_print(input->layer_id, filter);
	}

	if (cur_filter->last_vert_filter != filter->vpp_vert_filter ||
	   cur_filter->last_horz_filter != filter->vpp_horz_filter) {
		cur_filter->last_vert_filter = filter->vpp_vert_filter;
		cur_filter->last_horz_filter = filter->vpp_horz_filter;
		cur_filter->scaler_filter_cnt = 0;
	} else {
		cur_filter->scaler_filter_cnt++;
	}
	if (cur_filter->scaler_filter_cnt >=
		scaler_filter_cnt_limit &&
		(cur_filter->cur_vert_filter !=
		filter->vpp_vert_filter ||
		cur_filter->cur_horz_filter !=
		filter->vpp_horz_filter)) {
		cur_filter->cur_vert_filter = filter->vpp_vert_filter;
		cur_filter->cur_horz_filter = filter->vpp_horz_filter;
		cur_filter->scaler_filter_cnt = scaler_filter_cnt_limit;
		ret = vppfilter_success_and_changed;
	}
	if (load_pps_coef &&
	    (force_pps_hcoef_update ||
	    force_pps_vcoef_update)) {
		load_pps_coef = 0;
		ret = vppfilter_success_and_changed;
	}

	/* store the debug info for legacy */
	if (input->layer_id == 0) {
		cur_vert_filter = cur_filter->cur_vert_filter;
		cur_horz_filter = cur_filter->cur_horz_filter;
		cur_vert_chroma_filter = vert_chroma_filter;
		cur_skip_line =
			next_frame_par->vscale_skip_count;
	}

	if (next_frame_par->vscale_skip_count > 1 &&
	    (vf->type & VIDTYPE_COMPRESS) &&
	    (vf->type & VIDTYPE_NO_DW))
		ret = vppfilter_changed_but_hold;

	if (vf->vf_ext &&
	    !(vpp_flags & VPP_FLAG_FORCE_NOT_SWITCH_VF)) {
		if ((next_frame_par->vscale_skip_count > 1 &&
		     (vf->type & VIDTYPE_COMPRESS) &&
		     !next_frame_par->nocomp &&
		     IS_DI_POSTWRTIE(vf->type)) ||
		    (vpp_flags & VPP_FLAG_FORCE_SWITCH_VF))
			ret = vppfilter_changed_but_switch;
		if ((vpp_flags & VPP_FLAG_MORE_LOG) &&
		    ret == vppfilter_changed_but_switch)
			pr_debug
				("layer%d: switch the display to vf_ext %p->%p\n",
				input->layer_id, vf, vf->vf_ext);
	}
	return ret;
}

static s64 s3_to_s64(s64 data)
{
	s64 conver_data = -1;

	if (data & 0x4)
		conver_data = (conver_data & (~0x3)) | (data & 0x3);
	else
		conver_data = data;
	return conver_data;
}

static void vd_hphase_ctrl_adjust(struct vpp_frame_par_s *frame_par)
{
	if (!cur_dev->aisr_support ||
	    !cur_dev->pps_auto_calc)
		return;
	while (frame_par->h_phase[1] < 0) {
		frame_par->h_phase[1] += (1 << 24);
		frame_par->hsc_rpt_p0_num0++;
	}
	frame_par->VPP_hf_ini_phase_ = (u32)((frame_par->h_phase[1] + 128) >> 8) & 0xffff;
	if (aisr_debug_flag)
		pr_info("frame_par->VPP_hf_ini_phase_=0x%x, hsc_rpt_p0_num0=%x\n",
			frame_par->VPP_hf_ini_phase_,
			frame_par->hsc_rpt_p0_num0);
}

static void vd_vphase_ctrl_adjust(struct vpp_frame_par_s *frame_par)
{
	if (!cur_dev->aisr_support ||
	    !cur_dev->pps_auto_calc)
		return;
	while (frame_par->v_phase[1] < 0) {
		frame_par->v_phase[1] += (1 << 24);
		frame_par->vsc_top_rpt_l0_num++;
	}
	frame_par->VPP_vf_init_phase = (u32)((frame_par->v_phase[1] + 128) >> 8) & 0xffff;
	if (aisr_debug_flag)
		pr_info("frame_par->VPP_vf_init_phase=0x%x, vsc_top_rpt_l0_num=%x\n",
			frame_par->VPP_vf_init_phase,
			frame_par->vsc_top_rpt_l0_num);
}

static void sr_pps_step_phase_id(struct vpp_frame_par_s *next_frame_par)
{
	s64 cascade_h_size[4];
	s64 cascade_v_size[4];

	cascade_h_size[0] = next_frame_par->spsc0_w_in;
	cascade_h_size[1] = cascade_h_size[0] * (next_frame_par->supsc0_hori_ratio + 1);
	cascade_h_size[2] = next_frame_par->spsc1_w_in;
	cascade_h_size[3] = (s64)next_frame_par->spsc1_w_in *
		(next_frame_par->supsc1_hori_ratio + 1);
	cascade_v_size[0] = next_frame_par->spsc0_h_in;
	cascade_v_size[1] = cascade_v_size[0] * (next_frame_par->supsc0_vert_ratio + 1);
	cascade_v_size[2] = next_frame_par->spsc1_h_in;
	cascade_v_size[3] = (s64)next_frame_par->spsc1_h_in *
		(next_frame_par->supsc1_vert_ratio + 1);
	next_frame_par->h_phase[1] = div_s64((cascade_h_size[0] << 25),
					    cascade_h_size[3]);
	next_frame_par->h_phase[1] = (next_frame_par->h_phase[1] >> 1) - (1 << 24);
	next_frame_par->v_phase[1] = div_s64((cascade_v_size[0] << 25),
					    cascade_v_size[3]);
	next_frame_par->v_phase[1] = (next_frame_par->v_phase[1] >> 1) - (1 << 24);
	next_frame_par->h_phase[1] -= next_frame_par->h_phase[0];
	next_frame_par->v_phase[1] -= next_frame_par->v_phase[0];
	next_frame_par->h_phase[1] -= div_s64((s64)next_frame_par->h_phase[2] *
					     cascade_h_size[0],
					     cascade_h_size[2]);
	next_frame_par->v_phase[1] -= div_s64((s64)next_frame_par->v_phase[2] *
					     cascade_v_size[0],
					     cascade_v_size[2]);
	next_frame_par->h_phase[1] = div_s64(next_frame_par->h_phase[1] *
					    cascade_h_size[1],
					    cascade_h_size[0]);
	next_frame_par->h_phase[1] = (next_frame_par->h_phase[1] + 1) >> 1;
	next_frame_par->v_phase[1] = div_s64(next_frame_par->v_phase[1] *
					    cascade_v_size[1],
					    cascade_v_size[0]);
	next_frame_par->v_phase[1] = (next_frame_par->v_phase[1] + 1) >> 1;
	if (aisr_debug_flag) {
		pr_info("cascade h_size[0]=0x%llx, h_size[1]=0x%llx,h_size[2]=0x%llx,h_size[3]=0x%llx\n",
			cascade_h_size[0],
			cascade_h_size[1],
			cascade_h_size[2],
			cascade_h_size[3]);
		pr_info("cascade v_size[0]=0x%llx, v_size[1]=0x%llx,v_size[2]=0x%llx,v_size[3]=0x%llx\n",
			cascade_v_size[0],
			cascade_v_size[1],
			cascade_v_size[2],
			cascade_v_size[3]);
		pr_info("h_phase[1]=0x%llx, v_phase[1]=0x%llx\n",
			next_frame_par->h_phase[1], next_frame_par->v_phase[1]);
	}
}

static void sr_pps_phase_auto_calculation(struct vpp_frame_par_s *next_frame_par)
{
	struct sr_info_s *sr;
	u32 sr0_sharp_sr2_ctrl;
	u32 sr1_sharp_sr2_ctrl;
	u32 sr0_sharp_sr2_ctrl2;
	u32 sr1_sharp_sr2_ctrl2;
	s64 sr0_bic_init_hphase_y;
	s64 sr0_bic_init_vphase_y;
	s64 sr0_bic_init_hphase_c;
	s64 sr0_bic_init_vphase_c;
	s64 sr1_bic_init_hphase_y;
	s64 sr1_bic_init_vphase_y;
	s64 sr1_bic_init_hphase_c;
	s64 sr1_bic_init_vphase_c;
	u32 sr0_sr2_vert_outphs;
	u32 sr0_sr2_horz_outphs;
	u32 sr1_sr2_vert_outphs;
	u32 sr1_sr2_horz_outphs;

	if (hscaler_8tap_enable[0])
		next_frame_par->hsc_rpt_p0_num0 = 3;
	else
		next_frame_par->hsc_rpt_p0_num0 = 1;
	next_frame_par->vsc_top_rpt_l0_num = 1;
	if (!cur_dev->pps_auto_calc)
		return;
	sr = &sr_info;
	sr0_sharp_sr2_ctrl =
		cur_dev->rdma_func[VPP0].rdma_rd
			(SRSHARP0_SHARP_SR2_CTRL + sr->sr_reg_offt);
	sr0_sr2_vert_outphs = (sr0_sharp_sr2_ctrl & 0x80) >> 7;
	sr0_sr2_horz_outphs = (sr0_sharp_sr2_ctrl & 0x40) >> 6;
	sr1_sharp_sr2_ctrl =
		cur_dev->rdma_func[VPP0].rdma_rd
			(SRSHARP1_SHARP_SR2_CTRL + sr->sr_reg_offt2);
	sr1_sr2_vert_outphs = (sr1_sharp_sr2_ctrl & 0x80) >> 7;
	sr1_sr2_horz_outphs = (sr1_sharp_sr2_ctrl & 0x40) >> 6;
	sr0_sharp_sr2_ctrl2 =
		cur_dev->rdma_func[VPP0].rdma_rd
			(SRSHARP0_SHARP_SR2_CTRL2 + sr->sr_reg_offt);
	sr0_bic_init_hphase_y = s3_to_s64((sr0_sharp_sr2_ctrl2 & 0x7fff) >> 12);
	sr0_bic_init_vphase_y = s3_to_s64((sr0_sharp_sr2_ctrl2 & 0x7ff) >> 8);
	sr0_bic_init_hphase_c = s3_to_s64((sr0_sharp_sr2_ctrl2 & 0x7f) >> 4);
	sr0_bic_init_vphase_c = s3_to_s64(sr0_sharp_sr2_ctrl2 & 0x7);
	sr1_sharp_sr2_ctrl2 =
		cur_dev->rdma_func[VPP0].rdma_rd
			(SRSHARP1_SHARP_SR2_CTRL2 + sr->sr_reg_offt2);
	sr1_bic_init_hphase_y = s3_to_s64((sr1_sharp_sr2_ctrl2 & 0x7fff) >> 12);
	sr1_bic_init_vphase_y = s3_to_s64((sr1_sharp_sr2_ctrl2 & 0x7ff) >> 8);
	sr1_bic_init_hphase_c = s3_to_s64((sr1_sharp_sr2_ctrl2 & 0x7f) >> 4);
	sr1_bic_init_vphase_c = s3_to_s64(sr1_sharp_sr2_ctrl2 & 0x7);
	if (next_frame_par->supsc0_enable) {
		next_frame_par->h_phase[0] =
			(sr0_bic_init_hphase_y +
			sr0_sr2_horz_outphs * 2);
		next_frame_par->v_phase[0] =
			(sr0_bic_init_vphase_y +
			sr0_sr2_vert_outphs * 2);
	} else {
		next_frame_par->h_phase[0] = 0;
		next_frame_par->v_phase[0] = 0;
	}
	if (next_frame_par->supsc1_enable) {
		next_frame_par->h_phase[2] =
			(sr1_bic_init_hphase_y +
			sr1_sr2_horz_outphs * 2);
		next_frame_par->v_phase[2] =
			(sr1_bic_init_vphase_y +
			sr1_sr2_vert_outphs * 2);
	} else {
		next_frame_par->h_phase[2] = 0;
		next_frame_par->v_phase[2] = 0;
	}
	next_frame_par->h_phase[0] <<= (25 - 2);
	next_frame_par->v_phase[0] <<= (25 - 2);

	next_frame_par->h_phase[2] <<= (25 - 2);
	next_frame_par->v_phase[2] <<= (25 - 2);
	if (aisr_debug_flag)
		pr_info("%s:h_phase[0]=0x%llx, v_phase[0]=0x%llx,h_phase[2]=0x%llx,v_phase[2]=0x%llx\n",
			__func__,
			next_frame_par->h_phase[0],
			next_frame_par->v_phase[0],
			next_frame_par->h_phase[2],
			next_frame_par->v_phase[2]);
	sr_pps_step_phase_id(next_frame_par);
	vd_hphase_ctrl_adjust(next_frame_par);
	if (next_frame_par->hsc_rpt_p0_num0 >= 15)
		next_frame_par->hsc_rpt_p0_num0 = 15;
	vd_vphase_ctrl_adjust(next_frame_par);
	if (next_frame_par->vsc_top_rpt_l0_num >= 3)
		next_frame_par->vsc_top_rpt_l0_num = 3;
}

static void dnn_pps_phase_auto_calculation(struct vpp_frame_par_s *aisr_frame_par)
{
	s64 h_pps_ratio = 0;
	s64 v_pps_ratio = 0;
	s64 h_pps_phase = 0;
	s64 v_pps_phase = 0;

	if (!cur_dev->aisr_support ||
	    !cur_dev->pps_auto_calc)
		return;
	h_pps_ratio = div_s64(((s64)aisr_frame_par->nnhf_input_w << 25),
			      (s64)aisr_frame_par->reshape_output_w);
	h_pps_ratio = (h_pps_ratio + 1) >> 1;
	v_pps_ratio = div_s64(((s64)aisr_frame_par->nnhf_input_h << 25),
			      (s64)aisr_frame_par->reshape_output_h);
	v_pps_ratio = (v_pps_ratio + 1) >> 1;
	h_pps_phase = div_s64(((s64)1 << 49), h_pps_ratio * 2);
	h_pps_phase = (h_pps_phase - (1 << 24) + 1) >> 1;
	v_pps_phase = div_s64(((s64)1 << 49), v_pps_ratio * 2);
	v_pps_phase = (v_pps_phase - (1 << 24) + 1) >> 1;
	aisr_frame_par->h_phase[1] = h_pps_phase;
	aisr_frame_par->v_phase[1] = v_pps_phase;
	if (hscaler_8tap_enable[0])
		aisr_frame_par->hsc_rpt_p0_num0 = 3;
	else
		aisr_frame_par->hsc_rpt_p0_num0 = 1;
	aisr_frame_par->vsc_top_rpt_l0_num = 1;
	vd_hphase_ctrl_adjust(aisr_frame_par);
	if (aisr_frame_par->hsc_rpt_p0_num0 >= 15)
		aisr_frame_par->hsc_rpt_p0_num0 = 15;
	vd_vphase_ctrl_adjust(aisr_frame_par);
	if (aisr_frame_par->vsc_top_rpt_l0_num >= 3)
		aisr_frame_par->vsc_top_rpt_l0_num = 3;
}

static int check_reshape_speed(s32 width_in,
					s32 height_in,
					s32 height_out,
					struct vpp_frame_par_s *aisr_frame_par,
					const struct vinfo_s *vinfo,
					u32 vpp_flags)
{
	u32 vtotal, clk_in_pps = 0;
	u32 sync_duration_den = 1;
	u64 calc_clk = 0;
	u32 cur_super_debug = 0;

	if (vpp_flags & VPP_FLAG_MORE_LOG)
		cur_super_debug = super_debug;
	if (force_reshape_vskip_cnt == 0xff)/*for debug*/
		return SPEED_CHECK_DONE;
	if (aisr_frame_par->vscale_skip_count < force_reshape_vskip_cnt)
		return SPEED_CHECK_VSKIP;
	if (vinfo->sync_duration_den >  0)
		sync_duration_den = vinfo->sync_duration_den;
	if (vinfo->field_height < vinfo->height)
		vtotal = vinfo->vtotal / 2;
	else
		vtotal = vinfo->vtotal;
	clk_in_pps = vpu_clk_get();
	calc_clk = div_u64((u64)width_in *
		(u64)height_in *
		(u64)vinfo->sync_duration_num *
		(u64)vtotal,
		height_out *
		sync_duration_den);
	if (cur_super_debug)
		pr_info("%s, calc_clk=%lld, clk_in_pps=%d\n",
			__func__,
			calc_clk, clk_in_pps);
	if (calc_clk > clk_in_pps)
		return SPEED_CHECK_VSKIP;
	return SPEED_CHECK_DONE;
}

void aisr_set_filters(struct disp_info_s *input,
			struct vpp_frame_par_s *next_frame_par,
			struct vframe_s *vf,
			const struct vinfo_s *vinfo,
			u32 vpp_flags)
{
	s32 start, end;
	s32 ini_vphase, skip, vskip_step = 1;
	u32 layer_id = 0;
	u32 ratio_x = 0;
	u32 ratio_y = 0;
	u32 src_w, src_h, dst_w, dst_h;
	u32 w_in, h_in;
	u32 cur_super_debug = 0;
	u32 wide_mode;
	struct vppfilter_mode_s *filter = NULL;
	struct vpp_frame_par_s *aisr_frame_par;

	if (vpp_flags & VPP_FLAG_MORE_LOG)
		cur_super_debug = super_debug;
	/* don't use input->wide_mode */
	wide_mode = (vpp_flags & VPP_FLAG_WIDEMODE_MASK) >> VPP_WIDEMODE_BITS;
	aisr_frame_par = &cur_dev->aisr_frame_parms;
	aisr_frame_par->vscale_skip_count = 0;
	filter = &aisr_frame_par->vpp_filter;

	/* speical mode did not use ext sar mode */
	/* 3d not use aisr */
	if (wide_mode == VIDEO_WIDEOPTION_NONLINEAR ||
	    wide_mode == VIDEO_WIDEOPTION_NORMAL_NOSCALEUP ||
	    wide_mode == VIDEO_WIDEOPTION_NONLINEAR_T ||
	    process_3d_type) {
		aisr_frame_par->aisr_enable = 0;
		return;
	}
	if (!cur_dev->aisr_enable)
		return;
	w_in = next_frame_par->VPP_hd_end_lines_ -
		next_frame_par->VPP_hd_start_lines_ + 1;
	h_in = next_frame_par->VPP_vd_end_lines_ -
		next_frame_par->VPP_vd_start_lines_ + 1;

	aisr_frame_par->crop_top = next_frame_par->crop_top +
		next_frame_par->crop_top_adjust;
	aisr_frame_par->crop_bottom = next_frame_par->crop_bottom +
		next_frame_par->crop_bottom_adjust;

	aisr_frame_par->video_input_w = w_in;
	aisr_frame_par->video_input_h = h_in;
	aisr_frame_par->reshape_output_w = w_in * aisr_frame_par->reshape_scaler_w;
	aisr_frame_par->reshape_output_h = h_in * aisr_frame_par->reshape_scaler_h;
	if (cur_super_debug)
		pr_info("%s:after crop video_input_w=%d, video_input_h=%d, reshape_output_w=%d, reshape_output_h=%d\n",
			__func__,
			w_in,
			h_in,
			aisr_frame_par->reshape_output_w,
			aisr_frame_par->reshape_output_h);

	src_w = aisr_frame_par->reshape_output_w;
	src_h = aisr_frame_par->reshape_output_h;
	dst_w = next_frame_par->nnhf_input_w;
	dst_h = next_frame_par->nnhf_input_h;
RESTART:
	skip = check_reshape_speed(src_w,
				  src_h,
				  dst_h,
				  aisr_frame_par,
				  vinfo,
				  vpp_flags);

	aisr_frame_par->aisr_enable = 1;

	if (skip == SPEED_CHECK_VSKIP) {
		u32 next_vskip =
			aisr_frame_par->vscale_skip_count + vskip_step;
		if (next_vskip < aisr_frame_par->reshape_scaler_h) {
			aisr_frame_par->vscale_skip_count = next_vskip;
			src_h = aisr_frame_par->video_input_h *
				(aisr_frame_par->reshape_scaler_h -
				aisr_frame_par->vscale_skip_count);
			goto RESTART;
		} else {
			pr_info("close aisr: aisr_enable=%d\n",
				aisr_frame_par->aisr_enable);
			aisr_frame_par->aisr_enable = 0;
		}
	}
	src_h = aisr_frame_par->video_input_h *
		(aisr_frame_par->reshape_scaler_h -
		aisr_frame_par->vscale_skip_count);
	aisr_frame_par->reshape_output_h = src_h;
	aisr_frame_par->reshape_output_w = src_w;
	aisr_frame_par->nnhf_input_w = dst_w;
	aisr_frame_par->nnhf_input_h = dst_h;
	ratio_x = (src_w << 18) / dst_w;
	ratio_y = (src_h << 18) / dst_h;
	if (cur_super_debug)
		pr_info("after adjust: vscale_skip_count=%d, src_w=%d,src_h=%d,dst_w=%d, dst_h=%d\n",
			aisr_frame_par->vscale_skip_count,
			src_w,
			src_h,
			dst_w,
			dst_h);
	/* vertical */
	ini_vphase = 0; /* vpp_zoom_center_y & 0xff; */

	start = 0;
	end = dst_h - 1;
	aisr_frame_par->VPP_vsc_startp = start;
	aisr_frame_par->VPP_vsc_endp = end;

	/* set filter co-efficients */
	filter->vpp_vsc_start_phase_step = ratio_y << 6;

	/* horizontal */
	aisr_frame_par->VPP_hsc_startp = 0;
	aisr_frame_par->VPP_hsc_endp = dst_w - 1;
	filter->vpp_hf_start_phase_slope = 0;
	filter->vpp_hf_end_phase_slope = 0;
	filter->vpp_hf_start_phase_step = ratio_x << 6;

	aisr_frame_par->VPP_hsc_linear_startp = aisr_frame_par->VPP_hsc_startp;
	aisr_frame_par->VPP_hsc_linear_endp = aisr_frame_par->VPP_hsc_endp;

	filter->vpp_hsc_start_phase_step = ratio_x << 6;

	aisr_frame_par->VPP_hf_ini_phase_ = ini_vphase;

	/* coeff selection before skip and apply pre_scaler */
	if (aisr_frame_par->video_input_w >= 960 &&
	   aisr_frame_par->video_input_h >= 540) {
		filter->vpp_vert_filter = COEF_BILINEAR;
		/* avoid hscaler fitler adjustion affect on picture shift*/
		if (hscaler_8tap_enable[layer_id])
			filter->vpp_horz_filter = COEF_BILINEAR_8TAP;
		else
			filter->vpp_horz_filter = COEF_BILINEAR;
	} else {
		filter->vpp_vert_filter = coeff(vert_coeff_settings,
		      filter->vpp_vsc_start_phase_step,
		      1,
		      0,
		      vf->combing_cur_lev);
		if (hscaler_8tap_enable[layer_id])
			filter->vpp_horz_filter =
				coeff_sc2(hert_coeff_settings_sc2,
					  filter->vpp_hf_start_phase_step);
		else
			filter->vpp_horz_filter =
				coeff(horz_coeff_settings,
				      filter->vpp_hf_start_phase_step,
				      aisr_frame_par->VPP_hf_ini_phase_,
				      0,
				      vf->combing_cur_lev);
	}

	filter->vpp_vert_coeff =
		filter_table[filter->vpp_vert_filter];

	if (hscaler_8tap_enable[layer_id]) {
		/* hscaler 8 tap */
		filter->vpp_horz_coeff =
			hscaler_8tap_filter_table
			[filter->vpp_horz_filter];
	} else {
		filter->vpp_horz_coeff =
			filter_table[filter->vpp_horz_filter];
	}

	/* for aisr vpp_pre_vsc_en/vpp_pre_hsc_en disable */
	filter->vpp_pre_vsc_en = 0;
	filter->vpp_pre_hsc_en = 0;
	dnn_pps_phase_auto_calculation(aisr_frame_par);
}

void aisr_sr1_nn_enable(u32 enable)
{
	struct sr_info_s *sr;
	u32 sr_reg_offt2;

	if (!cur_dev->aisr_support)
		return;
	sr = &sr_info;
	sr_reg_offt2 = sr->sr_reg_offt2;
	if (enable)
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(SRSHARP1_NN_POST_TOP + sr_reg_offt2,
			0x3, 13, 2);
	else
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(SRSHARP1_NN_POST_TOP + sr_reg_offt2,
			0x0, 13, 2);
}

void aisr_reshape_output(u32 enable)
{
	struct sr_info_s *sr;
	u32 sr_reg_offt2;

	if (!cur_dev->aisr_support)
		return;
	sr = &sr_info;
	sr_reg_offt2 = sr->sr_reg_offt2;
	if (enable) {
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(SRSHARP1_NN_POST_TOP + sr_reg_offt2,
			0x5, 9, 4);
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(SRSHARP1_DEMO_MODE_WINDOW_CTRL0 + sr_reg_offt2,
			0xa, 12, 4);
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(SRSHARP1_DEMO_MODE_WINDOW_CTRL0 + sr_reg_offt2,
			0x2, 28, 2);
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(SRSHARP1_SHARP_SR2_CTRL + sr_reg_offt2,
			0x0, 1, 1);
	} else {
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(SRSHARP1_NN_POST_TOP + sr_reg_offt2,
			0x0, 9, 4);
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(SRSHARP1_DEMO_MODE_WINDOW_CTRL0 + sr_reg_offt2,
			0x4, 12, 4);
		cur_dev->rdma_func[VPP0].rdma_wr_bits
			(SRSHARP1_DEMO_MODE_WINDOW_CTRL0 + sr_reg_offt2,
			0x0, 28, 2);
		}
}

/*
 *VPP_SRSHARP0_CTRL:0x1d91
 *[0]srsharp0 enable for sharpness module reg r/w
 *[1]if sharpness is enable or vscaler is enable,must set to 1,
 *sharpness1;reg can only to be w on gxtvbb;which is fix after txl
 */
int vpp_set_super_scaler_regs(int scaler_path_sel,
			      int reg_srscl0_enable,
			      int reg_srscl0_hsize,
			      int reg_srscl0_vsize,
			      int reg_srscl0_hori_ratio,
			      int reg_srscl0_vert_ratio,
			      int reg_srscl1_enable,
			      int reg_srscl1_hsize,
			      int reg_srscl1_vsize,
			      int reg_srscl1_hori_ratio,
			      int reg_srscl1_vert_ratio,
			      int vpp_postblend_out_width,
			      int vpp_postblend_out_height)
{
	int tmp_data = 0;
	int tmp_data2 = 0;
	unsigned int data_path_chose;
	int sr_core0_max_width;
	struct sr_info_s *sr;
	u32 sr_reg_offt;
	u32 sr_reg_offt2;
	u32 sr_support;

	sr = &sr_info;
	sr_support = sr->sr_support;
	if (super_scaler == 0) {
		sr_support &= ~SUPER_CORE0_SUPPORT;
		sr_support &= ~SUPER_CORE1_SUPPORT;
	}
	sr_reg_offt = sr->sr_reg_offt;
	sr_reg_offt2 = sr->sr_reg_offt2;
	/* just work around for g12a not to disable sr core2 bit2 */
	if (reg_srscl0_vert_ratio == 0)
		sr_core0_max_width = sr->core0_v_disable_width_max;
	else
		sr_core0_max_width = sr->core0_v_enable_width_max;

	/* top config */
	tmp_data = VSYNC_RD_MPEG_REG(VPP_SRSHARP0_CTRL);
	if (sr0_sr1_refresh) {
		if (reg_srscl0_hsize > sr_core0_max_width) {
			if (((tmp_data >> 1) & 0x1) != 0)
				VSYNC_WR_MPEG_REG_BITS(VPP_SRSHARP0_CTRL,
						       0, 1, 1);
			if ((tmp_data & 0x1) != 0)
				VSYNC_WR_MPEG_REG_BITS(VPP_SRSHARP0_CTRL,
						       0, 0, 1);
			vpu_module_clk_disable(VPP0, SR0, 0);
		} else {
			if (((tmp_data >> 1) & 0x1) != 1)
				VSYNC_WR_MPEG_REG_BITS(VPP_SRSHARP0_CTRL,
						       1, 1, 1);
			if ((tmp_data & 0x1) != 1)
				vpu_module_clk_enable(VPP0, SR0, 0);
			VSYNC_WR_MPEG_REG_BITS(VPP_SRSHARP0_CTRL, 1, 0, 1);
		}
	}
	tmp_data = VSYNC_RD_MPEG_REG(VPP_SRSHARP1_CTRL);
	if (sr0_sr1_refresh) {
		if (((tmp_data >> 1) & 0x1) != 1)
			VSYNC_WR_MPEG_REG_BITS(VPP_SRSHARP1_CTRL, 1, 1, 1);
		if ((tmp_data & 0x1) != 1)
			VSYNC_WR_MPEG_REG_BITS(VPP_SRSHARP1_CTRL, 1, 0, 1);
		if (cur_dev->aisr_support)
			VSYNC_WR_MPEG_REG_BITS(VPP_SRSHARP1_CTRL, 3, 8, 2);
	}
	/* core0 config */
	tmp_data = VSYNC_RD_MPEG_REG
	(SRSHARP0_SHARP_SR2_CTRL + sr_reg_offt);
	if (sr0_sr1_refresh) {
		if (((tmp_data >> 5) & 0x1) !=
			(reg_srscl0_vert_ratio & 0x1))
			VSYNC_WR_MPEG_REG_BITS
			(SRSHARP0_SHARP_SR2_CTRL + sr_reg_offt,
		     reg_srscl0_vert_ratio & 0x1, 5, 1);
		if (((tmp_data >> 4) & 0x1) !=
			(reg_srscl0_hori_ratio & 0x1))
			VSYNC_WR_MPEG_REG_BITS
			(SRSHARP0_SHARP_SR2_CTRL + sr_reg_offt,
			reg_srscl0_hori_ratio & 0x1, 4, 1);

		if (reg_srscl0_hsize > sr_core0_max_width) {
			if (((tmp_data >> 2) & 0x1) != 0)
				VSYNC_WR_MPEG_REG_BITS
					(SRSHARP0_SHARP_SR2_CTRL + sr_reg_offt,
					0, 2, 1);
		} else {
			if (((tmp_data >> 2) & 0x1) != 1) {
				if (is_meson_txlx_cpu())
					WRITE_VCBUS_REG_BITS
					(SRSHARP0_SHARP_SR2_CTRL
					+ sr_reg_offt,
					1, 2, 1);
				VSYNC_WR_MPEG_REG_BITS
					(SRSHARP0_SHARP_SR2_CTRL + sr_reg_offt,
					1, 2, 1);
			}
		}

		if ((tmp_data & 0x1) == (reg_srscl0_hori_ratio & 0x1))
			VSYNC_WR_MPEG_REG_BITS
			(SRSHARP0_SHARP_SR2_CTRL + sr_reg_offt,
			((~(reg_srscl0_hori_ratio & 0x1)) & 0x1), 0, 1);
	}
	/* core1 config */
	if (sr_support & SUPER_CORE1_SUPPORT) {
		if (is_meson_gxtvbb_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			tmp_data = sharpness1_sr2_ctrl_32d7;
#endif
		} else {
			tmp_data = VSYNC_RD_MPEG_REG
			(SRSHARP1_SHARP_SR2_CTRL
			+ sr_reg_offt2); /*0xc80*/
		}
		if (is_meson_gxtvbb_cpu() ||
		    (((tmp_data >> 5) & 0x1) !=
		     (reg_srscl1_vert_ratio & 0x1)) ||
		    (((tmp_data >> 4) & 0x1) !=
		     (reg_srscl1_hori_ratio & 0x1)) ||
		    ((tmp_data & 0x1) ==
		     (reg_srscl1_hori_ratio & 0x1)) ||
		    (((tmp_data >> 2) & 0x1) != 1)) {
			tmp_data = tmp_data & (~(1 << 5));
			tmp_data = tmp_data & (~(1 << 4));
			tmp_data = tmp_data & (~(1 << 2));
			tmp_data = tmp_data & (~(1 << 0));
			tmp_data |= ((reg_srscl1_vert_ratio & 0x1) << 5);
			tmp_data |= ((reg_srscl1_hori_ratio & 0x1) << 4);
			tmp_data |= (1 << 2);
			tmp_data |=
				(((~(reg_srscl1_hori_ratio & 0x1)) & 0x1) << 0);
			if (sr0_sr1_refresh) {
				VSYNC_WR_MPEG_REG
					(SRSHARP1_SHARP_SR2_CTRL
					+ sr_reg_offt2, /*0xc80*/
					tmp_data);
				sharpness1_sr2_ctrl_32d7 = tmp_data;
			}
		}
	}

	/* size config */
	tmp_data = ((reg_srscl0_hsize & 0x1fff) << 16) |
		(reg_srscl0_vsize & 0x1fff);
	tmp_data2 = VSYNC_RD_MPEG_REG
		(SRSHARP0_SHARP_HVSIZE + sr_reg_offt);
	if (tmp_data != tmp_data2)
		VSYNC_WR_MPEG_REG
		(SRSHARP0_SHARP_HVSIZE + sr_reg_offt,
		tmp_data);
	if (cur_dev->sr_in_size) {
		tmp_data2 = VSYNC_RD_MPEG_REG(VPP_SR0_IN_SIZE);
		if (tmp_data != tmp_data2)
			VSYNC_WR_MPEG_REG
			(VPP_SR0_IN_SIZE, tmp_data);
	}
	tmp_data = ((reg_srscl1_hsize & 0x1fff) << 16) |
		(reg_srscl1_vsize & 0x1fff);

	if (sr_support & SUPER_CORE1_SUPPORT) {
		if (get_cpu_type() != MESON_CPU_MAJOR_ID_GXTVBB)
			tmp_data2 = VSYNC_RD_MPEG_REG
			(SRSHARP1_SHARP_HVSIZE + sr_reg_offt2);
		if (is_meson_gxtvbb_cpu() || tmp_data != tmp_data2) {
			VSYNC_WR_MPEG_REG
			(SRSHARP1_SHARP_HVSIZE + sr_reg_offt2, tmp_data);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			if (is_meson_gxtvbb_cpu())
				sharpness1_sr2_ctrl_3280 = tmp_data;
#endif
		}
	}
	if (cur_dev->sr_in_size) {
		tmp_data2 = VSYNC_RD_MPEG_REG(VPP_SR1_IN_SIZE);
		if (tmp_data != tmp_data2)
			VSYNC_WR_MPEG_REG
			(VPP_SR1_IN_SIZE, tmp_data);
	}

	/*ve input size setting*/
	if (sr->core_support == ONLY_CORE0)
		tmp_data = ((reg_srscl0_hsize & 0x1fff) << 16) |
			(reg_srscl0_vsize & 0x1fff);
	else if ((sr->core_support == NEW_CORE0_CORE1) &&
		 ((scaler_path_sel == PPS_CORE0_CORE1) ||
		 (scaler_path_sel == PPS_CORE0_POSTBLEND_CORE1)))
		tmp_data = ((reg_srscl0_hsize & 0x1fff) << 16) |
			(reg_srscl0_vsize & 0x1fff);
	else
		tmp_data = ((reg_srscl1_hsize & 0x1fff) << 16) |
			(reg_srscl1_vsize & 0x1fff);
	tmp_data2 = VSYNC_RD_MPEG_REG(VPP_VE_H_V_SIZE);
	if (tmp_data != tmp_data2)
		VSYNC_WR_MPEG_REG(VPP_VE_H_V_SIZE, tmp_data);
	/*chroma blue stretch size setting*/
	if (is_meson_txlx_cpu() || is_meson_txhd_cpu() ||
	    (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))) {
		tmp_data = (((vpp_postblend_out_width & 0x1fff) << 16) |
			(vpp_postblend_out_height & 0x1fff));
		VSYNC_WR_MPEG_REG(VPP_OUT_H_V_SIZE, tmp_data);
	} else {
		if (scaler_path_sel == CORE0_PPS_CORE1) {
			tmp_data = (((reg_srscl1_hsize & 0x1fff) <<
				reg_srscl1_hori_ratio) << 16) |
				((reg_srscl1_vsize & 0x1fff) <<
				reg_srscl1_vert_ratio);
			tmp_data2 = VSYNC_RD_MPEG_REG(VPP_PSR_H_V_SIZE);
			if (tmp_data != tmp_data2)
				VSYNC_WR_MPEG_REG(VPP_PSR_H_V_SIZE, tmp_data);
		} else if ((scaler_path_sel == CORE0_CORE1_PPS) ||
			(scaler_path_sel == CORE1_BEFORE_PPS) ||
			(scaler_path_sel == CORE1_AFTER_PPS)) {
			tmp_data = ((reg_srscl1_hsize & 0x1fff) << 16) |
					   (reg_srscl1_vsize & 0x1fff);
			tmp_data2 = VSYNC_RD_MPEG_REG(VPP_PSR_H_V_SIZE);
			if (tmp_data != tmp_data2)
				VSYNC_WR_MPEG_REG(VPP_PSR_H_V_SIZE, tmp_data);
		}
	}

	/* path config */
	if (is_meson_txhd_cpu())
		data_path_chose = 6;
	else
		data_path_chose = 5;
	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_TXHD) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (scaler_path_sel == CORE0_PPS_CORE1 ||
			scaler_path_sel == CORE1_BEFORE_PPS ||
			scaler_path_sel == CORE0_BEFORE_PPS) {
			VSYNC_WR_MPEG_REG_BITS(VPP_VE_ENABLE_CTRL,
					       0, data_path_chose, 1);
		} else {
			VSYNC_WR_MPEG_REG_BITS(VPP_VE_ENABLE_CTRL,
					       1, data_path_chose, 1);
		}
#endif
	}
	if (super_scaler == 0) {
		VSYNC_WR_MPEG_REG(VPP_SRSHARP0_CTRL, 0);
		VSYNC_WR_MPEG_REG(VPP_SRSHARP1_CTRL, 0);
	}

	return 0;
}

static void vpp_set_super_scaler
	(u32 vpp_wide_mode,
	const struct vinfo_s *vinfo,
	struct vpp_frame_par_s *next_frame_par,
	bool bypass_sr0, bool bypass_sr1, u32 vpp_flags)
{
	unsigned int hor_sc_multiple_num, ver_sc_multiple_num, temp;
	u32 width_out = next_frame_par->VPP_hsc_endp -
		next_frame_par->VPP_hsc_startp + 1;
	u32 height_out = next_frame_par->VPP_vsc_endp -
		next_frame_par->VPP_vsc_startp + 1;
	u32 src_width = next_frame_par->video_input_w;
	u32 src_height = next_frame_par->video_input_h;
	u32 sr_path;
	struct sr_info_s *sr;
	u32 sr_support;

	sr = &sr_info;
	sr_support = sr->sr_support;
	/*for sr adjust*/
	if (super_scaler == 0) {
		sr_support &= ~SUPER_CORE0_SUPPORT;
		sr_support &= ~SUPER_CORE1_SUPPORT;
	}
	next_frame_par->sr_core_support = sr->sr_support;

	hor_sc_multiple_num = (1 << PPS_FRAC_BITS) /
		next_frame_par->vpp_filter.vpp_hsc_start_phase_step;
	ver_sc_multiple_num = SUPER_SCALER_V_FACTOR *
		(1 << PPS_FRAC_BITS) /
		next_frame_par->vpp_filter.vpp_vsc_start_phase_step;

	/* just calcuate the enable sclaer module */
	/*
	 *note:if first check h may cause v can't do scaling;
	 * for example: 1920x1080(input),3840x2160(output);
	 * todo:if you have better idea,you can improve it
	 */
	/* step1: judge core0&core1 vertical enable or disable*/
	if (ver_sc_multiple_num >= 2 * SUPER_SCALER_V_FACTOR) {
		next_frame_par->supsc0_vert_ratio =
			((src_width < sr->core0_v_enable_width_max) &&
			(sr_support & SUPER_CORE0_SUPPORT)) ? 1 : 0;
		next_frame_par->supsc1_vert_ratio =
			((width_out < sr->core1_v_disable_width_max) &&
			(src_width < sr->core1_v_enable_width_max) &&
			(sr_support & SUPER_CORE1_SUPPORT)) ? 1 : 0;
		if (next_frame_par->supsc0_vert_ratio &&
		    (ver_sc_multiple_num < 4 * SUPER_SCALER_V_FACTOR))
			next_frame_par->supsc1_vert_ratio = 0;
		next_frame_par->supsc0_enable =
			next_frame_par->supsc0_vert_ratio ? 1 : 0;
		next_frame_par->supsc1_enable =
			next_frame_par->supsc1_vert_ratio ? 1 : 0;
	} else {
		next_frame_par->supsc0_enable = 0;
		next_frame_par->supsc0_vert_ratio = 0;
		next_frame_par->supsc1_enable = 0;
		next_frame_par->supsc1_vert_ratio = 0;
	}
	/* step2: judge core0&core1 horizontal enable or disable*/
	if (hor_sc_multiple_num >= 2 &&
	    ((vpp_wide_mode != VIDEO_WIDEOPTION_NONLINEAR) &&
	    (vpp_wide_mode != VIDEO_WIDEOPTION_NONLINEAR_T))) {
		if (src_width > sr->core0_v_disable_width_max ||
		    (src_width > sr->core0_v_enable_width_max &&
		     next_frame_par->supsc0_vert_ratio) ||
		    (((src_width << 1) > sr->core1_v_enable_width_max) &&
		     next_frame_par->supsc1_vert_ratio))
			next_frame_par->supsc0_hori_ratio = 0;
		else if (sr_support & SUPER_CORE0_SUPPORT)
			next_frame_par->supsc0_hori_ratio = 1;
		if (((width_out >> 1) > sr->core1_v_disable_width_max) ||
		    (((width_out >> 1) > sr->core1_v_enable_width_max) &&
		     next_frame_par->supsc1_vert_ratio) ||
		    (next_frame_par->supsc0_hori_ratio &&
		    hor_sc_multiple_num < 4))
			next_frame_par->supsc1_hori_ratio = 0;
		else if (sr_support & SUPER_CORE1_SUPPORT)
			next_frame_par->supsc1_hori_ratio = 1;
		next_frame_par->supsc0_enable =
			(next_frame_par->supsc0_hori_ratio ||
			next_frame_par->supsc0_enable) ? 1 : 0;
		next_frame_par->supsc1_enable =
			(next_frame_par->supsc1_hori_ratio ||
			next_frame_par->supsc1_enable) ? 1 : 0;
	} else {
		next_frame_par->supsc0_enable =
			next_frame_par->supsc0_vert_ratio ? 1 : 0;
		next_frame_par->supsc0_hori_ratio = 0;
		next_frame_par->supsc1_enable =
			next_frame_par->supsc1_vert_ratio ? 1 : 0;
		next_frame_par->supsc1_hori_ratio = 0;
	}

	/*double check core0 input width for core0_vert_ratio!!!*/
	if (next_frame_par->supsc0_vert_ratio &&
	    (src_width > sr->core0_v_enable_width_max)) {
		next_frame_par->supsc0_vert_ratio = 0;
		if (next_frame_par->supsc0_hori_ratio == 0)
			next_frame_par->supsc0_enable = 0;
	}

	/*double check core1 input width for core1_vert_ratio!!!*/
	if (next_frame_par->supsc1_vert_ratio &&
	    ((width_out >> next_frame_par->supsc1_hori_ratio) >
	     sr->core1_v_enable_width_max)) {
		next_frame_par->supsc1_vert_ratio = 0;
		if (next_frame_par->supsc1_hori_ratio == 0)
			next_frame_par->supsc1_enable = 0;
	}

	/* option add patch */
	if (ver_sc_multiple_num <= super_scaler_v_ratio &&
	    src_height >= sr->core0_v_enable_width_max &&
	    src_height <= 1088 &&
	    ver_sc_multiple_num > SUPER_SCALER_V_FACTOR &&
	    vinfo->height >= 2000) {
		next_frame_par->supsc0_enable = 0;
		next_frame_par->supsc1_enable = 1;
		next_frame_par->supsc0_hori_ratio = 0;
		next_frame_par->supsc1_hori_ratio = 1;
		next_frame_par->supsc0_vert_ratio = 0;
		next_frame_par->supsc1_vert_ratio = 1;
	}
	if (bypass_sr0 || !(sr_support & SUPER_CORE0_SUPPORT)) {
		next_frame_par->supsc0_enable = 0;
		next_frame_par->supsc0_hori_ratio = 0;
		next_frame_par->supsc0_vert_ratio = 0;
	}

	/* much vskip case, no need super scale up */
	if (next_frame_par->vscale_skip_count >= 2) {
		next_frame_par->supsc0_enable = 0;
		next_frame_par->supsc0_hori_ratio = 0;
		next_frame_par->supsc0_vert_ratio = 0;
	}
	if (cur_dev->aisr_enable) {
		next_frame_par->supsc1_enable = 1;
		next_frame_par->supsc1_hori_ratio = 1;
		next_frame_par->supsc1_vert_ratio = 1;
		if (ver_sc_multiple_num >= 2 * SUPER_SCALER_V_FACTOR &&
		   ver_sc_multiple_num < 4 * SUPER_SCALER_V_FACTOR)
			next_frame_par->supsc0_vert_ratio = 0;
		if (hor_sc_multiple_num >= 2 &&
		    hor_sc_multiple_num < 4)
			next_frame_par->supsc0_hori_ratio = 0;
		if (next_frame_par->supsc0_vert_ratio == 0 &&
		   next_frame_par->supsc0_hori_ratio == 0)
			next_frame_par->supsc0_enable = 0;
	}

	if (bypass_sr1 || !(sr_support & SUPER_CORE1_SUPPORT)) {
		next_frame_par->supsc1_enable = 0;
		next_frame_par->supsc1_hori_ratio = 0;
		next_frame_par->supsc1_vert_ratio = 0;
	}
	/* new add according to pq test @20170808 on gxlx*/
	if (scaler_path_sel >= SCALER_PATH_MAX) {
		if (sr->supscl_path == 0xff) {
			if (is_meson_gxlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
				if (next_frame_par->supsc1_hori_ratio &&
				    next_frame_par->supsc1_vert_ratio)
					next_frame_par->supscl_path =
						CORE1_BEFORE_PPS;
				else
					next_frame_par->supscl_path =
						CORE1_AFTER_PPS;
#endif
			} else if (is_meson_txhd_cpu() ||
				is_meson_g12a_cpu() ||
				is_meson_g12b_cpu() ||
				is_meson_sm1_cpu()) {
				if (next_frame_par->supsc0_hori_ratio &&
				    next_frame_par->supsc0_vert_ratio)
					next_frame_par->supscl_path =
						CORE0_BEFORE_PPS;
				else
					next_frame_par->supscl_path =
						CORE0_AFTER_PPS;
			} else {
				next_frame_par->supscl_path = CORE0_PPS_CORE1;
			}
		} else {
			next_frame_par->supscl_path = sr->supscl_path;
		}
	} else {
		next_frame_par->supscl_path = scaler_path_sel;
	}

	/*double check core0 input width for core0_vert_ratio */
	/*when CORE0_AFTER_PPS!!!*/
	if (next_frame_par->supsc0_vert_ratio) {
		if (next_frame_par->supscl_path == CORE0_AFTER_PPS &&
			next_frame_par->supsc0_hori_ratio == 0) {
			src_width = width_out;
			if (src_width > sr->core0_v_enable_width_max) {
				next_frame_par->supsc0_vert_ratio = 0;
				next_frame_par->supsc0_enable = 0;
			}
		}
	}

	/*patch for width align 2*/
	if (super_scaler && (width_out % 2) &&
	    (((next_frame_par->supscl_path == CORE0_AFTER_PPS ||
	       next_frame_par->supscl_path == PPS_CORE0_CORE1 ||
	       next_frame_par->supscl_path == PPS_CORE0_POSTBLEND_CORE1) &&
	      next_frame_par->supsc0_hori_ratio) ||
	      ((next_frame_par->supscl_path == CORE0_PPS_CORE1 ||
		next_frame_par->supscl_path == CORE0_CORE1_PPS ||
		next_frame_par->supscl_path == CORE1_AFTER_PPS ||
		next_frame_par->supscl_path == CORE0_BEFORE_PPS ||
		next_frame_par->supscl_path == PPS_CORE0_CORE1 ||
		next_frame_par->supscl_path == PPS_CORE0_POSTBLEND_CORE1 ||
		(next_frame_par->supscl_path == PPS_POSTBLEND_CORE1) ||
		(next_frame_par->supscl_path == PPS_CORE1_CM)) &&
	       next_frame_par->supsc1_hori_ratio))) {
		temp = next_frame_par->VPP_hsc_endp;
		if (++temp >= vinfo->width) {
			if (next_frame_par->VPP_hsc_startp > 0 &&
			    next_frame_par->VPP_hsc_startp <
			    next_frame_par->VPP_hsc_endp)
				next_frame_par->VPP_hsc_startp--;
			else if (((next_frame_par->supscl_path ==
				PPS_CORE0_CORE1) ||
				(next_frame_par->supscl_path ==
				PPS_CORE0_POSTBLEND_CORE1)) &&
				next_frame_par->supsc0_hori_ratio &&
				next_frame_par->supsc1_hori_ratio) {
				next_frame_par->supsc1_enable = 0;
				next_frame_par->supsc1_hori_ratio = 0;
				next_frame_par->supsc1_vert_ratio = 0;
				next_frame_par->VPP_hsc_endp++;
			} else if ((next_frame_par->supscl_path ==
				CORE0_AFTER_PPS) ||
				(((next_frame_par->supscl_path ==
				PPS_CORE0_CORE1) ||
				(next_frame_par->supscl_path ==
				PPS_CORE0_POSTBLEND_CORE1)) &&
				next_frame_par->supsc0_hori_ratio)) {
				next_frame_par->supsc0_enable = 0;
				next_frame_par->supsc0_hori_ratio = 0;
				next_frame_par->supsc0_vert_ratio = 0;
			} else if ((next_frame_par->supscl_path ==
				CORE1_AFTER_PPS) ||
				(next_frame_par->supscl_path ==
				CORE0_PPS_CORE1) ||
				(((next_frame_par->supscl_path ==
				PPS_CORE0_CORE1) ||
				(next_frame_par->supscl_path ==
				PPS_CORE0_POSTBLEND_CORE1) ||
				(next_frame_par->supscl_path ==
				PPS_POSTBLEND_CORE1) ||
				(next_frame_par->supscl_path ==
				PPS_CORE1_CM)) &&
				next_frame_par->supsc1_hori_ratio)) {
				next_frame_par->supsc1_enable = 0;
				next_frame_par->supsc1_hori_ratio = 0;
				next_frame_par->supsc1_vert_ratio = 0;
			}
		} else {
			next_frame_par->VPP_hsc_endp++;
		}
		width_out++;
	}

	/*patch for height align 2*/
	if (super_scaler && (height_out % 2) &&
	    (((next_frame_par->supscl_path == CORE0_AFTER_PPS ||
	       next_frame_par->supscl_path == PPS_CORE0_CORE1 ||
	       next_frame_par->supscl_path == PPS_CORE0_POSTBLEND_CORE1) &&
	      next_frame_par->supsc0_vert_ratio) ||
	      ((next_frame_par->supscl_path == CORE0_PPS_CORE1 ||
		next_frame_par->supscl_path == CORE0_CORE1_PPS ||
		next_frame_par->supscl_path == CORE1_AFTER_PPS ||
		next_frame_par->supscl_path == CORE0_BEFORE_PPS ||
		next_frame_par->supscl_path == PPS_CORE0_CORE1 ||
		next_frame_par->supscl_path == PPS_CORE0_POSTBLEND_CORE1 ||
		(next_frame_par->supscl_path == PPS_POSTBLEND_CORE1) ||
		(next_frame_par->supscl_path == PPS_CORE1_CM)) &&
	       next_frame_par->supsc1_vert_ratio))) {
		temp = next_frame_par->VPP_vsc_endp;
		if (++temp >= vinfo->height) {
			if (next_frame_par->VPP_vsc_startp > 0 &&
			    next_frame_par->VPP_vsc_startp <
			    next_frame_par->VPP_vsc_endp)
				next_frame_par->VPP_vsc_startp--;
			else if (((next_frame_par->supscl_path ==
				PPS_CORE0_CORE1) ||
				(next_frame_par->supscl_path ==
				PPS_CORE0_POSTBLEND_CORE1)) &&
				next_frame_par->supsc0_vert_ratio &&
				next_frame_par->supsc1_vert_ratio) {
				next_frame_par->supsc0_enable = 0;
				next_frame_par->supsc0_hori_ratio = 0;
				next_frame_par->supsc0_vert_ratio = 0;
				next_frame_par->VPP_vsc_endp++;
			} else if ((next_frame_par->supscl_path ==
				CORE0_AFTER_PPS) ||
				(((next_frame_par->supscl_path ==
				PPS_CORE0_CORE1) ||
				(next_frame_par->supscl_path ==
				PPS_CORE0_POSTBLEND_CORE1)) &&
				next_frame_par->supsc0_vert_ratio)) {
				next_frame_par->supsc0_enable = 0;
				next_frame_par->supsc0_hori_ratio = 0;
				next_frame_par->supsc0_vert_ratio = 0;
			} else if ((next_frame_par->supscl_path ==
				CORE1_AFTER_PPS) ||
				(next_frame_par->supscl_path ==
				CORE0_PPS_CORE1) ||
				(((next_frame_par->supscl_path ==
				PPS_CORE0_CORE1) ||
				(next_frame_par->supscl_path ==
				PPS_CORE0_POSTBLEND_CORE1) ||
				(next_frame_par->supscl_path ==
				PPS_POSTBLEND_CORE1) ||
				(next_frame_par->supscl_path ==
				PPS_CORE1_CM)) &&
				next_frame_par->supsc1_vert_ratio)) {
				next_frame_par->supsc1_enable = 0;
				next_frame_par->supsc1_hori_ratio = 0;
				next_frame_par->supsc1_vert_ratio = 0;
			}
		} else {
			next_frame_par->VPP_vsc_endp++;
		}
		height_out++;
	}

	/* select the scaler path:[core0 =>>
	 *ppscaler =>> core1]  or
	 *[core0	=>> ppscaler =>> postblender =>> core1]
	 *gxlx only have core1,so the path:[core1 ==> pps ==> postblend]
	 *or pps ==> core1 ==> postblend
	 *txhd only have core0,so the path:[core0 ==> pps ==> postblend]
	 *or pps ==> core0 ==> postblend
	 */
	if (next_frame_par->supscl_path == CORE0_AFTER_PPS) {
		next_frame_par->spsc0_h_in =
			height_out >> next_frame_par->supsc0_vert_ratio;
		next_frame_par->spsc0_w_in =
			width_out >> next_frame_par->supsc0_hori_ratio;
	} else if (next_frame_par->supscl_path == PPS_CORE0_CORE1 ||
		next_frame_par->supscl_path ==
			PPS_CORE0_POSTBLEND_CORE1) { /*tl1*/
		next_frame_par->spsc0_h_in =
			(height_out >> next_frame_par->supsc0_vert_ratio) >>
				next_frame_par->supsc1_vert_ratio;
		next_frame_par->spsc0_w_in =
			(width_out >> next_frame_par->supsc0_hori_ratio) >>
			next_frame_par->supsc1_hori_ratio;
	} else {
		next_frame_par->spsc0_h_in = src_height;
		next_frame_par->spsc0_w_in = src_width;
	}
	if (next_frame_par->supscl_path == CORE0_PPS_CORE1 ||
	    next_frame_par->supscl_path == CORE0_CORE1_PPS ||
	    next_frame_par->supscl_path == CORE1_AFTER_PPS ||
	    next_frame_par->supscl_path == CORE0_BEFORE_PPS ||
	    next_frame_par->supscl_path == PPS_CORE0_CORE1 ||
	    next_frame_par->supscl_path == PPS_CORE0_POSTBLEND_CORE1 ||
	    next_frame_par->supscl_path == PPS_POSTBLEND_CORE1 ||
	    next_frame_par->supscl_path == PPS_CORE1_CM) {
		next_frame_par->spsc1_h_in =
			(height_out >> next_frame_par->supsc1_vert_ratio);
		next_frame_par->spsc1_w_in =
			(width_out >> next_frame_par->supsc1_hori_ratio);
	} else if (next_frame_par->supscl_path == CORE1_BEFORE_PPS) {
		next_frame_par->spsc1_h_in = src_height;
		next_frame_par->spsc1_w_in = src_width;
	} else if (next_frame_par->supscl_path == CORE0_AFTER_PPS) {
		next_frame_par->spsc1_h_in =
			(height_out >> next_frame_par->supsc0_vert_ratio);
		next_frame_par->spsc1_w_in =
			(width_out >> next_frame_par->supsc0_hori_ratio);
	} else {
		next_frame_par->spsc1_h_in = height_out;
		next_frame_par->spsc1_w_in = width_out;
	}
	/*recalc phase step and pps input&output size param*/
	/*phase*/
	if (next_frame_par->supsc0_hori_ratio) {
		next_frame_par->vpp_filter.vpp_hsc_start_phase_step <<= 1;
		next_frame_par->vpp_filter.vpp_hf_start_phase_step <<= 1;
	}
	if (next_frame_par->supsc1_hori_ratio) {
		next_frame_par->vpp_filter.vpp_hsc_start_phase_step <<= 1;
		next_frame_par->vpp_filter.vpp_hf_start_phase_step <<= 1;
	}
	if (next_frame_par->supsc0_vert_ratio)
		next_frame_par->vpp_filter.vpp_vsc_start_phase_step <<= 1;
	if (next_frame_par->supsc1_vert_ratio)
		next_frame_par->vpp_filter.vpp_vsc_start_phase_step <<= 1;
	/*pps input size*/
	if (next_frame_par->supscl_path == CORE0_PPS_CORE1 ||
	    next_frame_par->supscl_path == CORE0_CORE1_PPS ||
	    next_frame_par->supscl_path == CORE0_BEFORE_PPS) {
		next_frame_par->VPP_line_in_length_ <<=
			next_frame_par->supsc0_hori_ratio;
		next_frame_par->VPP_pic_in_height_ <<=
			next_frame_par->supsc0_vert_ratio;
	}
	if (next_frame_par->supscl_path == CORE0_CORE1_PPS ||
	    next_frame_par->supscl_path == CORE1_BEFORE_PPS) {
		next_frame_par->VPP_line_in_length_ <<=
			next_frame_par->supsc1_hori_ratio;
		next_frame_par->VPP_pic_in_height_ <<=
			next_frame_par->supsc1_vert_ratio;
	}

	sr_path = next_frame_par->supscl_path;
	/* path config */
	if (sr->core_support == NEW_CORE0_CORE1) {
		if (sr_path == CORE0_PPS_CORE1) {
			next_frame_par->sr0_position = 1;
			next_frame_par->sr1_position = 1;
		} else if (sr_path == PPS_CORE0_CORE1) {
			next_frame_par->sr0_position = 0;
			next_frame_par->sr1_position = 1;
		} else if (sr_path ==
			PPS_CORE0_POSTBLEND_CORE1) {
			next_frame_par->sr0_position = 0;
			next_frame_par->sr1_position = 0;
		} else if (sr_path ==
			CORE0_PPS_POSTBLEND_CORE1) {
			next_frame_par->sr0_position = 1;
			next_frame_par->sr1_position = 0;
		} else {
			next_frame_par->sr0_position = 1;
			next_frame_par->sr1_position = 1;
		}
		if (next_frame_par->sr1_position) {
			/* sr core 1 output */
			next_frame_par->cm_input_w =
				next_frame_par->spsc1_w_in <<
				next_frame_par->supsc1_hori_ratio;
			next_frame_par->cm_input_h =
				next_frame_par->spsc1_h_in <<
				next_frame_par->supsc1_vert_ratio;
		} else if (!next_frame_par->sr0_position) {
			/* sr core 0 output */
			next_frame_par->cm_input_w =
				next_frame_par->spsc0_w_in <<
				next_frame_par->supsc0_hori_ratio;
			next_frame_par->cm_input_h =
				next_frame_par->spsc0_h_in <<
				next_frame_par->supsc0_vert_ratio;
		} else {
			/* pps output */
			next_frame_par->cm_input_w =
				next_frame_par->VPP_hsc_endp -
				next_frame_par->VPP_hsc_startp + 1;
			next_frame_par->cm_input_h =
				next_frame_par->VPP_vsc_endp -
				next_frame_par->VPP_vsc_startp + 1;
		}
		if (cur_nnhf_input_w != next_frame_par->cm_input_w ||
		    cur_nnhf_input_h != next_frame_par->cm_input_h)
			video_info_change_status |= VIDEO_SIZE_CHANGE_EVENT;
		next_frame_par->nnhf_input_w = next_frame_par->cm_input_w;
		next_frame_par->nnhf_input_h = next_frame_par->cm_input_h;
	} else if (sr->core_support == ONLY_CORE0) {
		if (sr_path == CORE0_BEFORE_PPS)
			next_frame_par->sr0_position = 1;
		else if (sr_path == CORE0_AFTER_PPS)
			next_frame_par->sr0_position = 0;
		else
			next_frame_par->sr0_position = 1;
		next_frame_par->sr1_position = 0;
		if (!next_frame_par->sr0_position) {
			/* sr core 0 output */
			next_frame_par->cm_input_w =
				next_frame_par->spsc0_w_in <<
				next_frame_par->supsc0_hori_ratio;
			next_frame_par->cm_input_h =
				next_frame_par->spsc0_h_in <<
				next_frame_par->supsc0_vert_ratio;
		} else {
			/* pps output */
			next_frame_par->cm_input_w =
				next_frame_par->VPP_hsc_endp -
				next_frame_par->VPP_hsc_startp + 1;
			next_frame_par->cm_input_h =
				next_frame_par->VPP_vsc_endp -
				next_frame_par->VPP_vsc_startp + 1;
		}
	} else if (sr->core_support == ONLY_CORE1) {
		if (sr_path == CORE1_BEFORE_PPS ||
		    sr_path == PPS_CORE1_CM)
			next_frame_par->sr1_position = 1;
		else if (sr_path == CORE1_AFTER_PPS  ||
			 sr_path == PPS_POSTBLEND_CORE1)
			next_frame_par->sr1_position = 0;
		else
			next_frame_par->sr1_position = 1;
		next_frame_par->sr0_position = 0;
		if (!next_frame_par->sr1_position) {
			/* sr core 1 input */
			if (sr_path == PPS_POSTBLEND_CORE1) {
				next_frame_par->cm_input_w =
					next_frame_par->spsc1_w_in;
				next_frame_par->cm_input_h =
					next_frame_par->spsc1_h_in;
			} else {
				/* sr core 1 output */
				next_frame_par->cm_input_w =
					next_frame_par->spsc1_w_in <<
					next_frame_par->supsc1_hori_ratio;
				next_frame_par->cm_input_h =
					next_frame_par->spsc1_h_in <<
					next_frame_par->supsc1_vert_ratio;
				}
		} else {
			/* pps output */
			next_frame_par->cm_input_w =
				next_frame_par->VPP_hsc_endp -
				next_frame_par->VPP_hsc_startp + 1;
			next_frame_par->cm_input_h =
				next_frame_par->VPP_vsc_endp -
				next_frame_par->VPP_vsc_startp + 1;
		}
	} else if (sr->core_support == OLD_CORE0_CORE1) {
		if (sr_path == CORE0_PPS_CORE1) {
			next_frame_par->sr0_position = 1;
			next_frame_par->sr1_position = 1;
		} else if (sr_path ==
			CORE0_PPS_POSTBLEND_CORE1) {
			next_frame_par->sr0_position = 1;
			next_frame_par->sr1_position = 0;
		} else {
			next_frame_par->sr0_position = 1;
			next_frame_par->sr1_position = 1;
		}
		if (next_frame_par->sr1_position) {
			/* sr core 1 output */
			next_frame_par->cm_input_w =
				next_frame_par->spsc1_w_in <<
				next_frame_par->supsc1_hori_ratio;
			next_frame_par->cm_input_h =
				next_frame_par->spsc1_h_in <<
				next_frame_par->supsc1_vert_ratio;
		} else {
			/* pps output */
			next_frame_par->cm_input_w =
				next_frame_par->VPP_hsc_endp -
				next_frame_par->VPP_hsc_startp + 1;
			next_frame_par->cm_input_h =
				next_frame_par->VPP_vsc_endp -
				next_frame_par->VPP_vsc_startp + 1;
		}
	}
	cur_nnhf_input_w = next_frame_par->nnhf_input_w;
	cur_nnhf_input_h = next_frame_par->nnhf_input_h;
	if (super_debug && (vpp_flags & VPP_FLAG_MORE_LOG)) {
		pr_info("layer0: spsc0_w_in=%u, spsc0_h_in=%u, spsc1_w_in=%u, spsc1_h_in=%u.\n",
			next_frame_par->spsc0_w_in, next_frame_par->spsc0_h_in,
			next_frame_par->spsc1_w_in, next_frame_par->spsc1_h_in);
		pr_info("layer0: supsc0_hori=%d,supsc1_hori=%d,supsc0_v=%d,supsc1_v=%d\n",
			next_frame_par->supsc0_hori_ratio,
			next_frame_par->supsc1_hori_ratio,
			next_frame_par->supsc0_vert_ratio,
			next_frame_par->supsc1_vert_ratio);
		pr_info("layer0: VPP_hd_start_lines= %d,%d,%d,%d, %d,%d,%d,%d, %d,%d\n",
			next_frame_par->VPP_hd_start_lines_,
			next_frame_par->VPP_hd_end_lines_,
			next_frame_par->VPP_vd_start_lines_,
			next_frame_par->VPP_vd_end_lines_,
			next_frame_par->VPP_hsc_startp,
			next_frame_par->VPP_hsc_endp,
			next_frame_par->VPP_hsc_linear_startp,
			next_frame_par->VPP_hsc_linear_endp,
			next_frame_par->VPP_vsc_startp,
			next_frame_par->VPP_vsc_endp);
		pr_info("layer0: cm_input_w=%u, cm_input_h=%u, sr0_position=%u, sr1_position=%u.\n",
			next_frame_par->cm_input_w,
			next_frame_par->cm_input_h,
			next_frame_par->sr0_position,
			next_frame_par->sr1_position);
		pr_info("layer0: nnhf_input_w=%u, nnhf_input_h=%u\n",
			next_frame_par->nnhf_input_w,
			next_frame_par->nnhf_input_h);
	}
}

#if defined(TV_3D_FUNCTION_OPEN) && defined(CONFIG_AMLOGIC_MEDIA_TVIN)
void get_vpp_3d_mode(u32 process_3d_type,
		     u32 trans_fmt,
		     u32 *vpp_3d_mode)
{
	switch (trans_fmt) {
	case TVIN_TFMT_3D_LRH_OLOR:
	case TVIN_TFMT_3D_LRH_OLER:
	case TVIN_TFMT_3D_LRH_ELOR:
	case TVIN_TFMT_3D_LRH_ELER:
	case TVIN_TFMT_3D_DET_LR:
		*vpp_3d_mode = VPP_3D_MODE_LR;
		break;
	case TVIN_TFMT_3D_FP:
	case TVIN_TFMT_3D_TB:
	case TVIN_TFMT_3D_DET_TB:
	case TVIN_TFMT_3D_FA:
		*vpp_3d_mode = VPP_3D_MODE_TB;
		if (process_3d_type & MODE_3D_MVC)
			*vpp_3d_mode = VPP_3D_MODE_FA;
		break;
	case TVIN_TFMT_3D_LA:
	case TVIN_TFMT_3D_DET_INTERLACE:
		*vpp_3d_mode = VPP_3D_MODE_LA;
		break;
	case TVIN_TFMT_3D_DET_CHESSBOARD:
	default:
		*vpp_3d_mode = VPP_3D_MODE_NULL;
		break;
	}
}

static void vpp_get_video_source_size
	(u32 *src_width, u32 *src_height,
	u32 process_3d_type, struct vframe_s *vf,
	struct vpp_frame_par_s *next_frame_par)
{
	int frame_width, frame_height;

	if (vf->type & VIDTYPE_COMPRESS) {
		frame_width = vf->compWidth;
		frame_height = vf->compHeight;
	} else {
		frame_width = vf->width;
		frame_height = vf->height;
	}
	if ((process_3d_type & MODE_3D_AUTO) ||
	    (((process_3d_type & MODE_3D_TO_2D_R) ||
	     (process_3d_type & MODE_3D_TO_2D_L) ||
	     (process_3d_type & MODE_3D_LR_SWITCH) ||
	     (process_3d_type & MODE_FORCE_3D_TO_2D_TB) ||
	     (process_3d_type & MODE_FORCE_3D_TO_2D_LR)) &&
	    (process_3d_type & MODE_3D_ENABLE))) {
		if (vf->trans_fmt) {
			if (process_3d_type & MODE_3D_TO_2D_MASK) {
				*src_height = vf->left_eye.height;
			} else {
				*src_height = vf->left_eye.height << 1;
				next_frame_par->vpp_2pic_mode = 1;
			}
			*src_width = vf->left_eye.width;
		}

		switch (vf->trans_fmt) {
		case TVIN_TFMT_3D_LRH_OLOR:
		case TVIN_TFMT_3D_LRH_OLER:
		case TVIN_TFMT_3D_LRH_ELOR:
		case TVIN_TFMT_3D_LRH_ELER:
		case TVIN_TFMT_3D_DET_LR:
			next_frame_par->vpp_3d_mode = VPP_3D_MODE_LR;
			break;
		case TVIN_TFMT_3D_FP:
		case TVIN_TFMT_3D_TB:
		case TVIN_TFMT_3D_DET_TB:
		case TVIN_TFMT_3D_FA:
			next_frame_par->vpp_3d_mode = VPP_3D_MODE_TB;
			/*just for mvc 3d file */
			if (process_3d_type & MODE_3D_MVC) {
				next_frame_par->vpp_2pic_mode = 2;
				next_frame_par->vpp_3d_mode = VPP_3D_MODE_FA;
			}
			break;
		case TVIN_TFMT_3D_LA:
		case TVIN_TFMT_3D_DET_INTERLACE:
			next_frame_par->vpp_3d_mode = VPP_3D_MODE_LA;
			next_frame_par->vpp_2pic_mode = 0;
			break;
		case TVIN_TFMT_3D_DET_CHESSBOARD:
		default:
			*src_width = frame_width;
			*src_height = frame_height;
			next_frame_par->vpp_3d_mode = VPP_3D_MODE_NULL;
			next_frame_par->vpp_3d_scale = 0;
			next_frame_par->vpp_2pic_mode = 0;
			break;
		}

	} else if ((process_3d_type & MODE_3D_LR) ||
		(process_3d_type & MODE_FORCE_3D_LR)) {
		next_frame_par->vpp_3d_mode = VPP_3D_MODE_LR;
		if (process_3d_type & MODE_3D_TO_2D_MASK) {
			*src_width = frame_width >> 1;
			*src_height = frame_height;
		} else if (process_3d_type & MODE_3D_OUT_LR) {
			*src_width = frame_width;
			*src_height = frame_height;
			next_frame_par->vpp_2pic_mode = 1;
		} else {
			*src_width = frame_width >> 1;
			*src_height = frame_height << 1;
			next_frame_par->vpp_2pic_mode = 1;
		}

	} else if ((process_3d_type & MODE_3D_TB) ||
		(process_3d_type & MODE_FORCE_3D_TB)) {
		next_frame_par->vpp_3d_mode = VPP_3D_MODE_TB;
		if (process_3d_type & MODE_3D_TO_2D_MASK) {
			*src_width = frame_width;
			*src_height = frame_height >> 1;
		} else if (process_3d_type & MODE_3D_OUT_LR) {
			*src_width = frame_width << 1;
			*src_height = frame_height >> 1;
			next_frame_par->vpp_2pic_mode = 1;
		} else {
			*src_width = frame_width;
			*src_height = frame_height;
			next_frame_par->vpp_2pic_mode = 1;
		}
		if (process_3d_type & MODE_3D_MVC) {
			*src_width = frame_width;
			*src_height = frame_height << 1;
			next_frame_par->vpp_2pic_mode = 2;
			next_frame_par->vpp_3d_mode = VPP_3D_MODE_FA;
		}
	} else if (process_3d_type & MODE_3D_LA) {
		next_frame_par->vpp_3d_mode = VPP_3D_MODE_LA;
		*src_height = frame_height - 1;
		*src_width = frame_width;
		next_frame_par->vpp_2pic_mode = 0;
		next_frame_par->vpp_3d_scale = 1;
		if (process_3d_type & MODE_3D_TO_2D_MASK) {
			next_frame_par->vscale_skip_count = 1;
			next_frame_par->vpp_3d_scale = 0;
		} else if (process_3d_type & MODE_3D_OUT_TB) {
			*src_height = frame_height << 1;
			next_frame_par->vscale_skip_count = 1;
			next_frame_par->vpp_3d_scale = 0;
		} else if (process_3d_type & MODE_3D_OUT_LR) {
			*src_width = frame_width << 1;
			next_frame_par->vscale_skip_count = 1;
			next_frame_par->vpp_3d_scale = 0;
		}
	} else if ((process_3d_type & MODE_3D_FA) ||
		(process_3d_type & MODE_FORCE_3D_FA_LR) ||
		(process_3d_type & MODE_FORCE_3D_FA_TB)) {
		next_frame_par->vpp_3d_mode = VPP_3D_MODE_FA;
		if (process_3d_type & MODE_3D_TO_2D_MASK) {
			if (process_3d_type & MODE_FORCE_3D_FA_TB) {
				next_frame_par->vpp_3d_mode = VPP_3D_MODE_TB;
				*src_width = frame_width;
				*src_height = frame_height >> 1;
			}
			if (process_3d_type & MODE_FORCE_3D_FA_LR) {
				next_frame_par->vpp_3d_mode = VPP_3D_MODE_LR;
				*src_width = frame_width >> 1;
				*src_height = frame_height;
			}
			if (process_3d_type & MODE_3D_MVC) {
				*src_width = frame_width;
				*src_height = frame_height;
				next_frame_par->vpp_3d_mode = VPP_3D_MODE_FA;
			}
			if (vf->trans_fmt == TVIN_TFMT_3D_FP) {
				next_frame_par->vpp_3d_mode = VPP_3D_MODE_TB;
				*src_width = frame_width;
				*src_height = vf->left_eye.height;
			}
			next_frame_par->vpp_2pic_mode = 0;
		} else if (process_3d_type & MODE_3D_OUT_LR) {
			*src_width = frame_width << 1;
			*src_height = frame_height;
			next_frame_par->vpp_2pic_mode = 2;
		} else {
			*src_width = frame_width;
			*src_height = frame_height << 1;
			next_frame_par->vpp_2pic_mode = 2;
		}
	} else {
		*src_width = frame_width;
		*src_height = frame_height;
		next_frame_par->vpp_3d_mode = VPP_3D_MODE_NULL;
		next_frame_par->vpp_2pic_mode = 0;
		next_frame_par->vpp_3d_scale = 0;
	}
	/*process 3d->2d or l/r switch case */
	if (next_frame_par->vpp_3d_mode != VPP_3D_MODE_NULL &&
	    next_frame_par->vpp_3d_mode != VPP_3D_MODE_LA &&
	    (process_3d_type & MODE_3D_ENABLE)) {
		if (process_3d_type & MODE_3D_TO_2D_R)
			next_frame_par->vpp_2pic_mode = VPP_SELECT_PIC1;
		else if (process_3d_type & MODE_3D_TO_2D_L)
			next_frame_par->vpp_2pic_mode = VPP_SELECT_PIC0;
		else if (process_3d_type & MODE_3D_LR_SWITCH)
			next_frame_par->vpp_2pic_mode |= VPP_PIC1_FIRST;
		if ((process_3d_type & MODE_FORCE_3D_TO_2D_LR) ||
		    (process_3d_type & MODE_FORCE_3D_TO_2D_TB))
			next_frame_par->vpp_2pic_mode = VPP_SELECT_PIC0;

		/*only display one pic */
		if ((next_frame_par->vpp_2pic_mode & 0x3) == 0)
			next_frame_par->vpp_3d_scale = 0;
		else
			next_frame_par->vpp_3d_scale = 1;
	}
	/*avoid dividing 0 error */
	if (*src_width == 0 || *src_height == 0) {
		*src_width = frame_width;
		*src_height = frame_height;
	}
}
#endif

static int vpp_set_filters_no_scaler_internal
	(struct disp_info_s *input,
	u32 width_in,
	u32 height_in,
	u32 wid_out,
	u32 hei_out,
	const struct vinfo_s *vinfo,
	u32 vpp_flags,
	struct vpp_frame_par_s *next_frame_par, struct vframe_s *vf)
{
	s32 start, end;
	s32 video_top, video_left, temp;
	u32 video_width, video_height;
	u32 ratio_x = 0;
	u32 ratio_y = 0;
	u32 ratio_tmp = 0;
	int temp_width;
	int temp_height;
	struct vppfilter_mode_s *filter = &next_frame_par->vpp_filter;
	u32 wide_mode;
	s32 height_shift = 0;
	u32 w_in = width_in;
	u32 h_in = height_in;
	bool h_crop_enable = false, v_crop_enable = false;
	bool skip_policy_check = true;
	u32 vskip_step;
	s32 video_layer_global_offset_x, video_layer_global_offset_y;
	u32 video_source_crop_top, video_source_crop_left;
	u32 video_source_crop_bottom, video_source_crop_right;
	s32 video_layer_top, video_layer_left;
	s32 video_layer_width, video_layer_height;
#ifdef TV_REVERSE
	bool reverse = false;
#endif
	int ret = vppfilter_success;
	u32 crop_ratio = 1;
	u32 crop_left, crop_right, crop_top, crop_bottom;
	bool no_compress = false;
	u32 cur_super_debug = 0;
	bool afbc_support;
	bool hskip_adjust = false;

	if (!input)
		return vppfilter_fail;
	if (vpp_flags & VPP_FLAG_MORE_LOG)
		cur_super_debug = super_debug;

	video_layer_global_offset_x = input->global_offset_x;
	video_layer_global_offset_y = input->global_offset_y;

	video_layer_top = input->layer_top;
	video_layer_left = input->layer_left;
	video_layer_width = input->layer_width;
	video_layer_height = input->layer_height;
#ifdef TV_REVERSE
	reverse = input->reverse;
#endif

	if ((vf->type & VIDTYPE_MVC) ||
	    (input->proc_3d_type & MODE_3D_ENABLE)) {
		video_source_crop_left = 0;
		video_source_crop_right = 0;
		video_source_crop_top = 0;
		video_source_crop_bottom = 0;
	} else {
		video_source_crop_left = input->crop_left;
		video_source_crop_right = input->crop_right;
		video_source_crop_top = input->crop_top;
		video_source_crop_bottom = input->crop_bottom;
	}

	next_frame_par->vscale_skip_count = 0;
	next_frame_par->hscale_skip_count = 0;
	next_frame_par->nocomp = false;
	if (vpp_flags & VPP_FLAG_INTERLACE_IN)
		next_frame_par->vscale_skip_count++;
	if (vpp_flags & VPP_FLAG_INTERLACE_OUT)
		height_shift++;

	if (vpp_flags & VPP_FLAG_INTERLACE_IN)
		vskip_step = 2;
	else
		vskip_step = 1;

RESTART_ALL:
	crop_left = video_source_crop_left / crop_ratio;
	crop_right = video_source_crop_right / crop_ratio;
	crop_top = video_source_crop_top / crop_ratio;
	crop_bottom = video_source_crop_bottom / crop_ratio;

	if (likely(w_in >
		(crop_left + crop_right)) &&
		(crop_left > 0 || crop_right > 0)) {
		w_in -= crop_left;
		w_in -= crop_right;
		h_crop_enable = true;
	}

	if (likely(h_in >
		(crop_top + crop_bottom)) &&
		(crop_top > 0 || crop_bottom > 0)) {
		h_in -= crop_top;
		h_in -= crop_bottom;
		v_crop_enable = true;
	}

	/* fix both h/w crop odd issue */
	if ((w_in & 1) && (h_in & 1)) {
		if (crop_left & 1) {
			crop_left &= ~1;
			w_in++;
		} else if (crop_right & 1) {
			crop_right &= ~1;
			w_in--;
		}
		if (crop_top & 1) {
			crop_top &= ~1;
			h_in++;
		} else if (crop_bottom & 1) {
			crop_bottom &= ~1;
			h_in--;
		}
		if (cur_super_debug)
			pr_info("%s:crop info=%d,%d,%d,%d, w_in=%d, h_in=%d\n",
				__func__, crop_left, crop_top,
				crop_right, crop_bottom,
				w_in, h_in);
	}

RESTART:
	if (next_frame_par->hscale_skip_count && !hskip_adjust) {
		w_in  = DATA_ALIGNED_DOWN_4(w_in);
		crop_left = DATA_ALIGNED_4(crop_left);
		hskip_adjust = true;
	}

	/* don't use input->wide_mode */
	wide_mode = (vpp_flags & VPP_FLAG_WIDEMODE_MASK) >> VPP_WIDEMODE_BITS;

	/*
	 *if we have ever set a cropped display area for video layer
	 * (by checking video_layer_width/video_height), then
	 * it will override the input width_out/height_out for
	 * ratio calculations, a.k.a we have a window for video content
	 */
	video_top = video_layer_top;
	video_left = video_layer_left;
	video_width = video_layer_width;
	video_height = video_layer_height;
	if (video_top == 0 &&
	    video_left == 0 &&
	    video_width <= 1 &&
	    video_height <= 1) {
		/* special case to do full screen display */
		video_width = video_layer_width;
		video_height = video_layer_height;
	} else {
		video_top += video_layer_global_offset_y;
		video_left += video_layer_global_offset_x;
	}

	ratio_x = (1 << 18);
	ratio_y = (1 << 18);

	/* screen position for source */
	start = video_top + (video_height + 1) / 2 -
		((h_in << 17) +
		(ratio_y >> 1)) / ratio_y;
	end = ((h_in << 18) + (ratio_y >> 1)) / ratio_y + start - 1;
	if (cur_super_debug)
		pr_info("layer%d: top:start =%d,%d,%d,%d  %d,%d\n",
			input->layer_id,
			start, end, video_top,
			video_height, h_in, ratio_y);

#ifdef TV_REVERSE
	if (reverse) {
		/* calculate source vertical clip */
		if (video_top < 0) {
			if (start < 0) {
				temp = (-start * ratio_y) >> 18;
				next_frame_par->VPP_vd_end_lines_ =
					h_in - 1 - temp;
			} else {
				next_frame_par->VPP_vd_end_lines_ = h_in - 1;
			}
		} else {
			if (start < video_top) {
				temp = ((video_top - start) * ratio_y) >> 18;
				next_frame_par->VPP_vd_end_lines_ =
					h_in - 1 - temp;
			} else {
				next_frame_par->VPP_vd_end_lines_ = h_in - 1;
			}
		}
		temp = next_frame_par->VPP_vd_end_lines_ -
			(video_height * ratio_y >> 18);
		next_frame_par->VPP_vd_start_lines_ = (temp >= 0) ? temp : 0;
	} else {
#endif
		if (video_top < 0) {
			if (start < 0) {
				temp = (-start * ratio_y) >> 18;
				next_frame_par->VPP_vd_start_lines_ = temp;
			} else {
				next_frame_par->VPP_vd_start_lines_ = 0;
			}
			temp_height = min((video_top + video_height - 1),
					  (vinfo->height - 1));
		} else {
			if (start < video_top) {
				temp = ((video_top - start) * ratio_y) >> 18;
				next_frame_par->VPP_vd_start_lines_ = temp;
			} else {
				next_frame_par->VPP_vd_start_lines_ = 0;
			}
			temp_height = min((video_top + video_height - 1),
					  (vinfo->height - 1)) - video_top + 1;
		}
		temp = next_frame_par->VPP_vd_start_lines_ +
			(temp_height * ratio_y >> 18);
		next_frame_par->VPP_vd_end_lines_ =
			(temp <= (h_in - 1)) ? temp : (h_in - 1);
#ifdef TV_REVERSE
	}
#endif
	if (v_crop_enable) {
		next_frame_par->VPP_vd_start_lines_ += crop_top;
		next_frame_par->VPP_vd_end_lines_ += crop_top;
	}

	if (vpp_flags & VPP_FLAG_INTERLACE_IN)
		next_frame_par->VPP_vd_start_lines_ &= ~1;

	next_frame_par->VPP_pic_in_height_ =
		next_frame_par->VPP_vd_end_lines_ -
		next_frame_par->VPP_vd_start_lines_ + 1;
	next_frame_par->VPP_pic_in_height_ =
		next_frame_par->VPP_pic_in_height_ /
		(next_frame_par->vscale_skip_count + 1);

	/* DI POST link, need make pps input size is even */
	if ((next_frame_par->VPP_pic_in_height_ & 1) &&
	    IS_DI_POST(vf->type)) {
		next_frame_par->VPP_pic_in_height_ &= ~1;
		next_frame_par->VPP_vd_end_lines_ =
			next_frame_par->VPP_pic_in_height_ *
			(next_frame_par->vscale_skip_count + 1) +
			next_frame_par->VPP_vd_start_lines_;
		if (next_frame_par->VPP_vd_end_lines_ > 0)
			next_frame_par->VPP_vd_end_lines_--;
	}
	/*
	 *find overlapped region between
	 *[start, end], [0, height_out-1],
	 *[video_top, video_top+video_height-1]
	 */
	start = max(start, max(0, video_top));
	end = min(end, min((s32)(vinfo->height - 1),
			   (s32)(video_top + video_height - 1)));

	if (start >= end) {
		/* nothing to display */
		next_frame_par->VPP_vsc_startp = 0;
		next_frame_par->VPP_vsc_endp = 0;
	} else {
		next_frame_par->VPP_vsc_startp =
			(vpp_flags & VPP_FLAG_INTERLACE_OUT) ?
			(start >> 1) : start;
		next_frame_par->VPP_vsc_endp =
			(vpp_flags & VPP_FLAG_INTERLACE_OUT) ?
			(end >> 1) : end;
	}

	/* set filter co-efficients */
	ratio_y <<= height_shift;
	ratio_tmp = ratio_y / (next_frame_par->vscale_skip_count + 1);
	ratio_y = ratio_tmp;

	filter->vpp_vsc_start_phase_step = 0x1000000;

	/* horizontal */
	filter->vpp_hf_start_phase_slope = 0;
	filter->vpp_hf_end_phase_slope = 0;
	filter->vpp_hf_start_phase_step = 0x1000000;

	next_frame_par->VPP_hsc_linear_startp = next_frame_par->VPP_hsc_startp;
	next_frame_par->VPP_hsc_linear_endp = next_frame_par->VPP_hsc_endp;

	filter->vpp_hsc_start_phase_step = 0x1000000;
	next_frame_par->VPP_hf_ini_phase_ = 0;

	/* screen position for source */
	start = video_left + (video_width + 1) / 2 -
		((w_in << 17) + (ratio_x >> 1)) / ratio_x;
	end = ((w_in << 18) + (ratio_x >> 1)) / ratio_x + start - 1;
	if (cur_super_debug)
		pr_info("layer%d: left:start =%d,%d,%d,%d  %d,%d\n",
			input->layer_id,
			start, end, video_left,
			video_width, w_in, ratio_x);

	/* calculate source horizontal clip */
#ifdef TV_REVERSE
	if (reverse) {
		if (video_left < 0) {
			if (start < 0) {
				temp = (-start * ratio_x) >> 18;
				next_frame_par->VPP_hd_end_lines_ =
					w_in - 1 - temp;
			} else {
				next_frame_par->VPP_hd_end_lines_ = w_in - 1;
			}
		} else {
			if (start < video_left) {
				temp = ((video_left - start) * ratio_x) >> 18;
				next_frame_par->VPP_hd_end_lines_ =
					w_in - 1 - temp;
			} else {
				next_frame_par->VPP_hd_end_lines_ = w_in - 1;
			}
		}
		temp = next_frame_par->VPP_hd_end_lines_ -
			(video_width * ratio_x >> 18);
		next_frame_par->VPP_hd_start_lines_ = (temp >= 0) ? temp : 0;
	} else {
#endif
		if (video_left < 0) {
			if (start < 0) {
				temp = (-start * ratio_x) >> 18;
				next_frame_par->VPP_hd_start_lines_ = temp;
			} else {
				next_frame_par->VPP_hd_start_lines_ = 0;
			}
			temp_width = min((video_left + video_width - 1),
					 (vinfo->width - 1));
		} else {
			if (start < video_left) {
				temp = ((video_left - start) * ratio_x) >> 18;
				next_frame_par->VPP_hd_start_lines_ = temp;

			} else {
				next_frame_par->VPP_hd_start_lines_ = 0;
			}
			temp_width = min((video_left + video_width - 1),
					 (vinfo->width - 1)) - video_left + 1;
		}
		temp = next_frame_par->VPP_hd_start_lines_ +
			(temp_width * ratio_x >> 18);
		next_frame_par->VPP_hd_end_lines_ =
			(temp <= (w_in - 1)) ? temp : (w_in - 1);
#ifdef TV_REVERSE
	}
#endif
	if (h_crop_enable) {
		next_frame_par->VPP_hd_start_lines_ += crop_left;
		next_frame_par->VPP_hd_end_lines_ += crop_left;
	}

	next_frame_par->VPP_line_in_length_ =
		next_frame_par->VPP_hd_end_lines_ -
		next_frame_par->VPP_hd_start_lines_ + 1;
	/*
	 *find overlapped region between
	 * [start, end], [0, width_out-1],
	 * [video_left, video_left+video_width-1]
	 */
	start = max(start, max(0, video_left));
	end = min(end,
		  min((s32)(vinfo->width - 1),
		      (s32)(video_left + video_width - 1)));

	if (start >= end) {
		/* nothing to display */
		next_frame_par->VPP_hsc_startp = 0;
		next_frame_par->VPP_hsc_endp = 0;
		/* avoid mif set wrong or di out size overflow */
		next_frame_par->VPP_hd_start_lines_ = 0;
		next_frame_par->VPP_hd_end_lines_ = 0;
		next_frame_par->VPP_line_in_length_ = 0;
	} else {
		next_frame_par->VPP_hsc_startp = start;
		next_frame_par->VPP_hsc_endp = end;
	}

	/*
	 *check the painful bandwidth limitation and see
	 * if we need skip half resolution on source side for progressive
	 * frames.
	 */
	/* check vskip and hskip */
	if ((next_frame_par->vscale_skip_count < MAX_VSKIP_COUNT ||
	     !next_frame_par->hscale_skip_count) &&
	    (!(vpp_flags & VPP_FLAG_VSCALE_DISABLE))) {
		int skip = SPEED_CHECK_DONE;

		if (skip == SPEED_CHECK_VSKIP) {
			u32 next_vskip =
				next_frame_par->vscale_skip_count + vskip_step;

			if (next_vskip <= MAX_VSKIP_COUNT) {
				next_frame_par->vscale_skip_count = next_vskip;
				goto RESTART;
			} else {
				next_frame_par->hscale_skip_count = 1;
			}
		} else if (skip == SPEED_CHECK_HSKIP) {
			next_frame_par->hscale_skip_count = 1;
		}
	}

	if ((vf->type & VIDTYPE_COMPRESS) &&
	    !(vf->type & VIDTYPE_NO_DW) &&
	    !next_frame_par->nocomp &&
	    vf->canvas0Addr != 0 &&
	    (!IS_DI_POSTWRTIE(vf->type) ||
	     vf->flag & VFRAME_FLAG_DI_DW)) {
		if (vd1_vd2_mux)
			afbc_support = false;
		else
			afbc_support = input->afbc_support;
		if ((vpp_flags & VPP_FLAG_FORCE_NO_COMPRESS) ||
		    next_frame_par->vscale_skip_count > 1 ||
		    !afbc_support ||
		    force_no_compress)
			no_compress = true;
	} else {
		no_compress = false;
	}
	if (no_compress) {
		if ((vpp_flags & VPP_FLAG_MORE_LOG) &&
		    input->afbc_support)
			pr_info
			("layer%d: Try DW buffer for compress frame.\n",
			input->layer_id);

		/* for VIDTYPE_COMPRESS, check if we can use double write
		 * buffer when primary frame can not be scaled.
		 */
		next_frame_par->nocomp = true;
		if (input->proc_3d_type & MODE_3D_ENABLE) {
			w_in = (width_in * vf->width) / vf->compWidth;
			width_in = w_in;
			h_in = (height_in * vf->height) / vf->compHeight;
			height_in = h_in;
		} else {
			w_in = vf->width;
			width_in = vf->width;
			h_in = vf->height;
			height_in = vf->height;
		}
		next_frame_par->hscale_skip_count = 0;
		next_frame_par->vscale_skip_count = 0;
		if (vf->width && vf->compWidth)
			crop_ratio = vf->compWidth / vf->width;
		goto RESTART_ALL;
	}

	if ((skip_policy & 0xf0) && skip_policy_check) {
		skip_policy_check = false;
		if (skip_policy & 0x40) {
			next_frame_par->vscale_skip_count = skip_policy & 0xf;
			goto RESTART;
		} else if (skip_policy & 0x80) {
			if (((vf->width >= 4096 &&
			      (!(vf->type & VIDTYPE_COMPRESS))) ||
			    (vf->flag & VFRAME_FLAG_HIGH_BANDWIDTH)) &&
			next_frame_par->vscale_skip_count == 0) {
				next_frame_par->vscale_skip_count =
				skip_policy & 0xf;
				goto RESTART;
			}
		}
	}

	next_frame_par->video_input_h = next_frame_par->VPP_vd_end_lines_ -
		next_frame_par->VPP_vd_start_lines_ + 1;
	next_frame_par->video_input_h = next_frame_par->video_input_h /
		(next_frame_par->vscale_skip_count + 1);
	next_frame_par->video_input_w = next_frame_par->VPP_hd_end_lines_ -
		next_frame_par->VPP_hd_start_lines_ + 1;
	next_frame_par->video_input_w = next_frame_par->video_input_w /
		(next_frame_par->hscale_skip_count + 1);

	filter->vpp_hsc_start_phase_step = 0x1000000;

	/* apply line skip */
	if (next_frame_par->hscale_skip_count) {
		filter->vpp_hf_start_phase_step >>= 1;
		filter->vpp_hsc_start_phase_step >>= 1;
		next_frame_par->VPP_line_in_length_ >>= 1;
	}

	if (next_frame_par->vscale_skip_count > 1 &&
	    (vf->type & VIDTYPE_COMPRESS) &&
	    (vf->type & VIDTYPE_NO_DW))
		ret = vppfilter_changed_but_hold;

	if (vf->vf_ext &&
	    !(vpp_flags & VPP_FLAG_FORCE_NOT_SWITCH_VF)) {
		if ((next_frame_par->vscale_skip_count > 1 &&
		     (vf->type & VIDTYPE_COMPRESS) &&
		     !next_frame_par->nocomp &&
		     IS_DI_POSTWRTIE(vf->type)) ||
		    (vpp_flags & VPP_FLAG_FORCE_SWITCH_VF))
			ret = vppfilter_changed_but_switch;
		if ((vpp_flags & VPP_FLAG_MORE_LOG) &&
		    ret == vppfilter_changed_but_switch)
			pr_debug
				("layer%d: switch the display to vf_ext %p->%p\n",
				input->layer_id, vf, vf->vf_ext);
	}
	return ret;
}

int vpp_set_filters(struct disp_info_s *input,
		    struct vframe_s *vf,
		    struct vpp_frame_par_s *next_frame_par,
		    const struct vinfo_s *vinfo,
		    bool bypass_sr, u32 op_flag)
{
	u32 src_width = 0;
	u32 src_height = 0;
	u32 vpp_flags = 0;
	u32 aspect_ratio = 0;
	u32 process_3d_type;
	u32 wide_mode;
	int ret = vppfilter_fail;
	struct disp_info_s local_input;
	bool bypass_sr0 = bypass_sr;
	bool bypass_sr1 = bypass_sr;
	bool retry = false;

	if (!input)
		return ret;

	WARN_ON(!vinfo);

RERTY:
	vpp_flags = 0;
	/* use local var to avoid the input data be overwritten */
	memcpy(&local_input, input, sizeof(struct disp_info_s));

	process_3d_type = local_input.proc_3d_type;
	wide_mode = local_input.wide_mode;

	next_frame_par->VPP_post_blend_vd_v_start_ = 0;
	next_frame_par->VPP_post_blend_vd_h_start_ = 0;

	next_frame_par->VPP_postproc_misc_ = 0x200;
	next_frame_par->vscale_skip_count = 0;
	next_frame_par->hscale_skip_count = 0;

	if (vf->type & VIDTYPE_COMPRESS) {
		src_width = vf->compWidth;
		src_height = vf->compHeight;
	} else {
		src_width = vf->width;
		src_height = vf->height;
	}
	update_vd_src_info(input->layer_id,
		src_width, src_height, vf->compWidth, vf->compHeight);
	if (super_debug)
		pr_info("vf->compWidth=%d, vf->compHeight=%d, vf->width=%d, vf->height=%d\n",
			vf->compWidth, vf->compHeight,
			vf->width, vf->height);
#if defined(TV_3D_FUNCTION_OPEN) && defined(CONFIG_AMLOGIC_MEDIA_TVIN)
	/*
	 *check 3d mode change in display buffer or 3d type
	 *get the source size according to 3d mode
	 */
	if (local_input.layer_id == 0) {
		if (process_3d_type & MODE_3D_ENABLE) {
			vpp_get_video_source_size
				(&src_width, &src_height,
				process_3d_type,
				vf, next_frame_par);
		} else {
			next_frame_par->vpp_3d_mode =
				VPP_3D_MODE_NULL;
			next_frame_par->vpp_2pic_mode = 0;
			next_frame_par->vpp_3d_scale = 0;
		}
		next_frame_par->trans_fmt = vf->trans_fmt;
		get_vpp_3d_mode(process_3d_type,
				next_frame_par->trans_fmt,
				&next_frame_par->vpp_3d_mode);
		if (local_input.vpp_3d_scale)
			next_frame_par->vpp_3d_scale = 1;
	} else {
		next_frame_par->vpp_3d_mode = VPP_3D_MODE_NULL;
		next_frame_par->vpp_2pic_mode = 0;
		next_frame_par->vpp_3d_scale = 0;
		next_frame_par->trans_fmt = vf->trans_fmt;
	}
	amlog_mask(LOG_MASK_VPP, "%s: src_width %u,src_height %u.\n", __func__,
		   src_width, src_height);
#endif
	/* check force ratio change flag in display buffer also
	 * if it exist then it will override the settings in display side
	 */
	if (vf->ratio_control & DISP_RATIO_FORCECONFIG) {
		if ((vf->ratio_control & DISP_RATIO_CTRL_MASK) ==
			DISP_RATIO_KEEPRATIO) {
			if (wide_mode == VIDEO_WIDEOPTION_FULL_STRETCH)
				wide_mode = VIDEO_WIDEOPTION_NORMAL;
		} else {
			if (wide_mode == VIDEO_WIDEOPTION_NORMAL)
				wide_mode = VIDEO_WIDEOPTION_FULL_STRETCH;
		}
		if (vf->ratio_control & DISP_RATIO_FORCE_NORMALWIDE)
			wide_mode = VIDEO_WIDEOPTION_NORMAL;
		else if (vf->ratio_control & DISP_RATIO_FORCE_FULL_STRETCH)
			wide_mode = VIDEO_WIDEOPTION_FULL_STRETCH;
	}

	aspect_ratio = (vf->ratio_control & DISP_RATIO_ASPECT_RATIO_MASK)
				   >> DISP_RATIO_ASPECT_RATIO_BIT;

	if (!aspect_ratio) {
		u32 sar_width, sar_height;

		if (vf->type & VIDTYPE_COMPRESS) {
			sar_width = vf->compWidth;
			sar_height = vf->compHeight;
		} else {
			sar_width = vf->width;
			sar_height = vf->height;
		}
		aspect_ratio = (sar_height << 8) / sar_width;
	}
	/* the height from vdin afbc will be half */
	/* so need no interlace in */
	if ((vf->type & VIDTYPE_INTERLACE) &&
	    !(vf->type & VIDTYPE_COMPRESS))
		vpp_flags = VPP_FLAG_INTERLACE_IN;

	if (vf->ratio_control & DISP_RATIO_PORTRAIT_MODE)
		vpp_flags |= VPP_FLAG_PORTRAIT_MODE;

	if (vf->type & VIDTYPE_VSCALE_DISABLE)
		vpp_flags |= VPP_FLAG_VSCALE_DISABLE;

	if ((vf->ratio_control & DISP_RATIO_ADAPTED_PICMODE) &&
	    !disable_adapted) {
		if (vf->pic_mode.screen_mode != 0xff)
			wide_mode = vf->pic_mode.screen_mode;
		if (vf->pic_mode.AFD_enable &&
		    (vf->ratio_control & DISP_RATIO_INFOFRAME_AVAIL))
			wide_mode = VIDEO_WIDEOPTION_AFD;
		if (wide_mode == VIDEO_WIDEOPTION_CUSTOM) {
			if (custom_ar)
				local_input.custom_ar = custom_ar;
			else
				local_input.custom_ar =
					vf->pic_mode.custom_ar;
		}
		local_input.crop_top = vf->pic_mode.vs;
		local_input.crop_left = vf->pic_mode.hs;
		local_input.crop_bottom = vf->pic_mode.ve;
		local_input.crop_right = vf->pic_mode.he;
	}

	if (!local_input.pps_support)
		wide_mode = VIDEO_WIDEOPTION_NORMAL;

	if ((vf->flag & VFRAME_FLAG_COMPOSER_DONE) &&
		(vf->flag & VFRAME_FLAG_FIX_TUNNEL)) {
		wide_mode = VIDEO_WIDEOPTION_FULL_STRETCH;
		local_input.crop_top = vf->crop[0];
		local_input.crop_left = vf->crop[1];
		local_input.crop_bottom = vf->crop[2];
		local_input.crop_right = vf->crop[3];
	}

	if (local_input.afd_enable && !disable_adapted) {
		wide_mode = VIDEO_WIDEOPTION_FULL_STRETCH;
		local_input.crop_top = local_input.afd_crop.top;
		local_input.crop_left = local_input.afd_crop.left;
		local_input.crop_bottom = local_input.afd_crop.bottom;
		local_input.crop_right = local_input.afd_crop.right;
		local_input.layer_left = local_input.afd_pos.x_start;
		local_input.layer_top = local_input.afd_pos.y_start;
		local_input.layer_width =
			local_input.afd_pos.x_end -
			local_input.afd_pos.x_start + 1;
		local_input.layer_height =
			local_input.afd_pos.y_end -
			local_input.afd_pos.y_start + 1;
		vpp_flags |= VPP_FLAG_FORCE_AFD_ENABLE;
	}

	/* don't restore the wide mode */
	/* input->wide_mode = wide_mode; */
	vpp_flags |= (wide_mode << VPP_WIDEMODE_BITS) |
		(aspect_ratio << VPP_FLAG_AR_BITS);

	if (vinfo->field_height != vinfo->height)
		vpp_flags |= VPP_FLAG_INTERLACE_OUT;

	if (op_flag & OP_VPP_MORE_LOG) {
		vpp_flags |= VPP_FLAG_MORE_LOG;
		vpp_flags |= VPP_FLAG_FROM_TOGGLE_FRAME;
	}
	if (op_flag & OP_FORCE_SWITCH_VF)
		vpp_flags |= VPP_FLAG_FORCE_SWITCH_VF;
	else if (op_flag & OP_FORCE_NOT_SWITCH_VF)
		vpp_flags |= VPP_FLAG_FORCE_NOT_SWITCH_VF;

	if (local_input.need_no_compress)
		vpp_flags |= VPP_FLAG_FORCE_NO_COMPRESS;

	next_frame_par->VPP_post_blend_vd_v_end_ = vinfo->field_height - 1;
	next_frame_par->VPP_post_blend_vd_h_end_ = vinfo->width - 1;
	next_frame_par->VPP_post_blend_h_size_ = vinfo->width;

	if (local_input.pps_support)
		ret = vpp_set_filters_internal
			(&local_input, src_width, src_height,
			vinfo->width, vinfo->height,
			vinfo, vpp_flags, next_frame_par, vf);
	else
		ret = vpp_set_filters_no_scaler_internal
			(&local_input, src_width, src_height,
			vinfo->width, vinfo->height,
			vinfo, vpp_flags, next_frame_par, vf);

	/* bypass sr since the input w/h may be wrong */
	if (ret == vppfilter_changed_but_hold) {
		bypass_sr0 = true;
		bypass_sr1 = true;
	}

	if (ret == vppfilter_changed_but_switch && vf->vf_ext) {
		struct vframe_s *tmp = (struct vframe_s *)vf->vf_ext;

		memcpy(&tmp->pic_mode, &vf->pic_mode,
			sizeof(struct vframe_pic_mode_s));
		vf = tmp;
		retry = true;
		goto RERTY;
	}

	if (retry && ret == vppfilter_success)
		ret = vppfilter_success_and_switched;

	/*config super scaler after set next_frame_par is calc ok for pps*/
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_tl1_cpu()) {
		/* disable sr0 when afbc, width >1920 and crop more than half */
		if ((vf->type & VIDTYPE_COMPRESS) &&
		    !next_frame_par->nocomp &&
		    vf->compWidth > 1920 &&
		    ((next_frame_par->video_input_w <<
		     (next_frame_par->hscale_skip_count + 1)) <=
		    vf->compWidth))
			bypass_sr0 = true;
	}
#endif
	if (local_input.layer_id == 0) {
		vpp_set_super_scaler
			(wide_mode,
			vinfo, next_frame_par,
			(bypass_sr0 | bypass_spscl0),
			(bypass_sr1 | bypass_spscl1),
			vpp_flags);
		/* cm input size will be set in super scaler function */
	} else {
		if (local_input.pps_support) {
			next_frame_par->cm_input_w =
				next_frame_par->VPP_hsc_endp
				- next_frame_par->VPP_hsc_startp + 1;
			next_frame_par->cm_input_h =
				next_frame_par->VPP_vsc_endp
				- next_frame_par->VPP_vsc_startp + 1;
		} else {
			next_frame_par->cm_input_w =
				next_frame_par->video_input_w;
			next_frame_par->cm_input_h =
				next_frame_par->video_input_h;
		}
	}
	aisr_set_filters(&local_input, next_frame_par, vf, vinfo, vpp_flags);
	sr_pps_phase_auto_calculation(next_frame_par);
	return ret;
}

s32 vpp_set_nonlinear_factor(struct disp_info_s *info, u32 f)
{
	if (f < MAX_NONLINEAR_FACTOR) {
		info->nonlinear_factor = f;
		return 0;
	}
	return -1;
}

u32 vpp_get_nonlinear_factor(struct disp_info_s *info)
{
	return info->nonlinear_factor;
}

s32 vpp_set_nonlinear_t_factor(struct disp_info_s *info, u32 f)
{
	if (f < MAX_NONLINEAR_T_FACTOR) {
		info->nonlinear_t_factor = f;
		return 0;
	}
	return -1;
}

u32 vpp_get_nonlinear_t_factor(struct disp_info_s *info)
{
	return info->nonlinear_t_factor;
}

void vpp_disp_info_init(struct disp_info_s *info, u8 id)
{
	if (info) {
		memset(info, 0, sizeof(struct disp_info_s));
		info->nonlinear_factor = MAX_NONLINEAR_FACTOR / 2;
		info->nonlinear_t_factor = MAX_NONLINEAR_T_FACTOR - 10;
		info->zoom_ratio = 100;
		info->speed_check_width = 1800;
		info->speed_check_height = 1400;
		info->layer_id = id;
		info->reverse = reverse;
		memset(&gfilter[id], 0, sizeof(struct filter_info_s));
	}
}

/*for gxlx only have core1 which will affact pip line*/
void vpp_bypass_ratio_config(void)
{
	if (is_meson_gxbb_cpu() || is_meson_gxl_cpu() ||
	    is_meson_gxm_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		bypass_ratio = 125;
#endif
	} else if (is_meson_txlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		/*change from 247 to 210 for bandwidth @20180627*/
		bypass_ratio = 210;
#endif
	} else if (is_meson_txl_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		bypass_ratio = 247;/*0x110 * (100/110)=0xf7*/
#endif
	} else {
		bypass_ratio = 205;
	}
}
