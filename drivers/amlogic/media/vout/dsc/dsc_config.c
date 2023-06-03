// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vout/dsc/dsc.h>
#include "dsc_config.h"
#include "dsc_hw.h"
#include "dsc_drv.h"
#include "dsc_debug.h"
#include "rc_tables.h"

/* VPP-->VPU VENC(video)-->VPU VENC bist(DE/DVI, about 2 pixel delay
 * than venc video)-->DSC->HDMI
 * the DE timing of DSC is later than the vpu bist timing.
 * for 8k it delays 2 lines, for 4k it delay 2~4 lines.
 * checked on board, it's 3 lines for 8k30/50/60hz.
 * while for 8k24/25hz it's 1 or 2 lines, and should not be 3 lines
 * otherwise it will got a snow board screen
 */
/* bpp_value is multiplied by 10000 */
unsigned int dsc_timing[][DSC_TIMING_VALUE_MAX] = {
//dsc_encode_mode bpp_value, havon_begin,vavon_bline,vavon_eline,hso_begin,hs_end
//vs_begin,vs_end,vs_bline,vs_eline,hc_vtotal_m1,hc_htotal_m1, hc_active, hc_blank
//hc_htotal_m1 = (hc_active + hc_blank) / 2 -1
/* {3840, 70625, 52, 94, 4, 16, 24, 22, 22, 12, 22, 2249, 616}, //4k 422 bpp7.0625 */
/* {3840, 120000, 140, 94, 4, 44,  66, 22, 22, 12, 22, 2249, 1099}, //4k60/120hz y444,420, bpp12 */

/* for 4k60hz y444/rgb/y422/y420, bpp = 12, dsc vertical timing = encp bist timing + 3 */
{DSC_RGB_3840X2160_60HZ, 120000, 140, 86, 2246, 44, 66, 22, 22, 5, 15, 2249, 1099, 1920, 280},
{DSC_YUV444_3840X2160_60HZ, 120000, 140, 86, 2246, 44, 66, 22, 22, 5, 15, 2249, 1099, 1920, 280},
{DSC_YUV422_3840X2160_60HZ, 120000, 140, 86, 2246, 44, 66, 22, 22, 5, 15, 2249, 1099, 1920, 280},
{DSC_YUV420_3840X2160_60HZ, 120000, 140, 86, 2246, 44, 66, 22, 22, 5, 15, 2249, 1099, 1920, 280},
/* for 4k120hz y444/rgb bpp = 12, dsc vertical timing = encp bist timing + 3 */
{DSC_RGB_3840X2160_120HZ, 120000, 140, 86, 2246, 44, 66, 22, 22, 5, 15, 2249, 1099, 1920, 280},
{DSC_YUV444_3840X2160_120HZ, 120000, 140, 86, 2246, 44, 66, 22, 22, 5, 15, 2249, 1099, 1920, 280},

/* for 4k120hz y422/420 bpp = 7.0625, dsc vertical timing = encp bist timing + 3 */
{DSC_YUV422_3840X2160_120HZ, 70625, 52, 86, 2246, 16, 24, 22, 22, 5, 15, 2249, 616, 1130, 104},
{DSC_YUV420_3840X2160_120HZ, 70625, 52, 86, 2246, 16, 24, 22, 22, 5, 15, 2249, 616, 1130, 104},

/* for 4k50hz y444/rgb/y422/y420(the same), bpp = 12, dsc vertical timing = encp bist timing + 3 */
{DSC_RGB_3840X2160_50HZ, 120000, 360, 86, 2246, 264, 286, 22, 22, 5, 15, 2249, 1319, 1920, 720},
{DSC_YUV444_3840X2160_50HZ, 120000, 360, 86, 2246, 264, 286, 22, 22, 5, 15, 2249, 1319, 1920, 720},
{DSC_YUV422_3840X2160_50HZ, 120000, 360, 86, 2246, 264, 286, 22, 22, 5, 15, 2249, 1319, 1920, 720},
{DSC_YUV420_3840X2160_50HZ, 120000, 360, 86, 2246, 264, 286, 22, 22, 5, 15, 2249, 1319, 1920, 720},
/* for 4k100hz y444/rgb bpp = 12, dsc vertical timing = encp bist timing + 3 */
{DSC_RGB_3840X2160_100HZ, 120000, 360, 86, 2246, 264, 286, 22, 22, 5, 15, 2249, 1319, 1920, 720},
{DSC_YUV444_3840X2160_100HZ, 120000, 360, 86, 2246, 264, 286, 22, 22, 5, 15, 2249, 1319, 1920, 720},

/* for 4k100hz y422/420 bpp = 8.5625, dsc vertical timing = encp bist timing + 3 */
{DSC_YUV422_3840X2160_100HZ, 85625, 52, 86, 2246, 38, 41, 22, 22, 5, 15, 2249, 736, 1370, 104},
{DSC_YUV420_3840X2160_100HZ, 85625, 52, 86, 2246, 38, 41, 22, 22, 5, 15, 2249, 736, 1370, 104},

/* for 8k30hz y444/rgb, bpp = 12,
 * and 8k30hz y422/y420(force bpp = 12, not spec recommended,
 * now not use this one for y422/420)
 */
/* below timing is from test case (v timing is from 8k60hz) */
/* {DSC_RGB_7680X4320_30HZ, 120000, 330, 83, 3, 138, 182, 22, 22, 19, 39, 4399, 2249}, */
/* {DSC_YUV444_7680X4320_30HZ, 120000, 330, 83, 3, 138, 182, 22, 22, 19, 39, 4399, 2249}, */
/* below is based on vpu bist vertical timing + 3 */
{DSC_RGB_7680X4320_30HZ, 120000, 330, 68, 4388, 138, 182, 22, 22, 5, 25, 4399, 2249, 3840, 660},
{DSC_YUV444_7680X4320_30HZ, 120000, 330, 68, 4388, 138, 182, 22, 22, 5, 25, 4399, 2249, 3840, 660},

/* for 8k30hz y422/y420(spec recommended bpp = 7.375) */
/* below timing is from test case (v timing is from 8k60hz) */
/* 72 = 144 / 2,
 * 30 = 552 * (144 / 2) / 1320
 * 39 = (552 + 176) * (144 / 2) / 1320
 * (2360 + 144) / 2 - 1 = 1251
 */
/* {DSC_YUV422_7680X4320_30HZ, 73750, 72, 83, 3, 30, 39, 22, 22, 19, 39, 4399, 1251}, */
/* {DSC_YUV420_7680X4320_30HZ, 73750, 72, 83, 3, 30, 39, 22, 22, 19, 39, 4399, 1251}, */
/* below is based on vpu bist vertical timing + 3 */
{DSC_YUV422_7680X4320_30HZ, 73750, 72, 68, 4388, 30, 39, 22, 22, 5, 25, 4399, 1251, 2360, 144},
{DSC_YUV420_7680X4320_30HZ, 73750, 72, 68, 4388, 30, 39, 22, 22, 5, 25, 4399, 1251, 2360, 144},

/* for 8k60hz y444/rgb, bpp = 9.9375 */
/* below timing is from test case */
/* {DSC_RGB_7680X4320_60HZ, 99375, 70, 83, 3, 29, 38, 22, 22, 19, 39, 4399, 1660}, */
/* {DSC_YUV444_7680X4320_60HZ, 99375, 70, 83, 3, 29, 38, 22, 22, 19, 39, 4399, 1660}, */
/* below is based on vpu bist vertical timing + 3 */
{DSC_RGB_7680X4320_60HZ, 99375, 70, 68, 4388, 29, 38, 22, 22, 5, 25, 4399, 1660, 3182, 140},
{DSC_YUV444_7680X4320_60HZ, 99375, 70, 68, 4388, 29, 38, 22, 22, 5, 25, 4399, 1660, 3182, 140},

/* for 8k60h y422/420, bpp = 7.4375 */
/* below timing is from test case */
/* {DSC_YUV422_7680X4320_60HZ, 74375, 58, 83, 3, 24, 32, 22, 22, 19, 39, 4399, 1247}, */
/* {DSC_YUV420_7680X4320_60HZ, 74375, 58, 83, 3, 24, 32, 22, 22, 19, 39, 4399, 1247}, */
/* below is based on vpu bist vertical timing + 3 */
{DSC_YUV422_7680X4320_60HZ, 74375, 58, 68, 4388, 24, 32, 22, 22, 5, 25, 4399, 1247, 2380, 116},
{DSC_YUV420_7680X4320_60HZ, 74375, 58, 68, 4388, 24, 32, 22, 22, 5, 25, 4399, 1247, 2380, 116},

/* for 8k50hz rgb/y444, bpp = 9.8125 */
/* below 8k24/25 is based on vpu bist vertical timing + 2,
 * 8k50 is based on vpu bist vertical timing + 3
 */
/* below V timing is from test case for 8k60hz */
/* echo manual_dsc_tmg 590 83 3 444 478 22 22 19 39 4399 2160 > debug */
/* {DSC_RGB_7680X4320_50HZ, 98125, 590, 83, 3, 444, 478, 22, 22, 19, 39, 4399, 2160}, */
/* {DSC_YUV444_7680X4320_50HZ, 98125, 590, 83, 3, 444, 478, 22, 22, 19, 39, 4399, 2160}, */
{DSC_RGB_7680X4320_50HZ, 98125, 590, 68, 4388, 444, 478, 22, 22, 5, 25, 4399, 2160, 3142, 1180},
{DSC_YUV444_7680X4320_50HZ, 98125, 590, 68, 4388, 444, 478, 22, 22, 5, 25, 4399, 2160, 3142, 1180},

/* for 8k50hz y422/420, bpp = 7.6875 */
/* below V timing is from test case for 8k60hz */
/* {DSC_YUV422_7680X4320_50HZ, 76875, 366, 83, 3, 275, 296, 22, 22, 19, 39, 4399, 1595}, */
/* {DSC_YUV420_7680X4320_50HZ, 76875, 366, 83, 3, 275, 296, 22, 22, 19, 39, 4399, 1595}, */
{DSC_YUV422_7680X4320_50HZ, 76875, 366, 68, 4388, 275, 296, 22, 22, 5, 25, 4399, 1595, 2460, 732},
{DSC_YUV420_7680X4320_50HZ, 76875, 366, 68, 4388, 275, 296, 22, 22, 5, 25, 4399, 1595, 2460, 732},

/* for 8k25hz y422/420,  bpp = 7.6875 */
{DSC_YUV422_7680X4320_25HZ, 76875, 366, 68, 4388, 275, 296, 22, 22, 5, 25, 4399, 1595, 2460, 732},
{DSC_YUV420_7680X4320_25HZ, 76875, 366, 68, 4388, 275, 296, 22, 22, 5, 25, 4399, 1595, 2460, 732},

/* for 8k25hz y444/rgb: bpp = 12, (3840 + 1560) / 2 - 1 = 2699 */
{DSC_RGB_7680X4320_25HZ, 120000, 780, 67, 4387, 588, 632, 22, 22, 4, 24, 4399, 2699, 3840, 1560},
{DSC_YUV444_7680X4320_25HZ, 120000, 780, 67, 4387, 588, 632, 22, 22, 4, 24, 4399, 2699, 3840, 1560},

/* for 8k24hz y444/rgb: bpp = 12
 * below is the example how these parameters come from
 * havon_begin = 830 = hc_blank / 2 = 1660 / 2
 * vavon_bline = 167 = de_v_begin(venc bist timing) + 2 = 0xa5 + 2(see note)
 * vavon_eline = 4487 = de_v_end(venc bist timing) + 2 = 0x1185 + 2(see note)
 * hso_begin = 638 = h_front * (hc_blank / 2) / h_blank = 2552 * 830 / 3320
 * hso_end = 682 = (h_front + h_sync) * (hc_blank / 2) / h_blank = 2728 * 830 / 3320
 * vs_begin = vs_begin = 22 = 44 / 2 (44: 7.7.3.2 Vertical Timing for Compressed Video Transport)
 * vs_bline = 4 = vso_bline(venc bist timing) + 2 = 0x2 + 2(see note)
 * vs_eline = 24 = vso_eline(venc bist timing) + 2 = 0x16 + 2(see note)
 * hc_vtotal_m1 = v_total - 1 = 4499
 * hc_htotal_m1 = (hc_active + hc_blank) / 2 -1 =(3840 + 1660) / 2 - 1 = 2749
 * note: for 8k24/25 y444/rgb, dsc vertical timing = encp bist timing + 2
 * (should not + 3 like 8k60/30/50hz), it's checked on board
 */
{DSC_RGB_7680X4320_24HZ, 120000, 830, 167, 4487, 638, 682, 22, 22, 4, 24, 4499, 2749, 3840, 1660},
{DSC_YUV444_7680X4320_24HZ, 120000, 830, 167, 4487, 638, 682, 22, 22, 4, 24, 4499, 2749,
	3840, 1660},
/* for 8k24hz y422/420, bpp = 7.6875 */
{DSC_YUV422_7680X4320_24HZ, 76875, 408, 168, 4488, 313, 335, 22, 22, 5, 25, 4499, 1637, 2460, 816},
{DSC_YUV420_7680X4320_24HZ, 76875, 408, 168, 4488, 313, 335, 22, 22, 5, 25, 4499, 1637, 2460, 816},

{DSC_ENCODE_MAX},
};

/* e.g. 8k60hz vactive = 0 - 0x50 = 4320, hactive = 0x7c9 - 0x49 = 1920 */
/* the param come from config_tv_enc_calc() */
unsigned int encp_timing_bist[][12] = {
//hactive,fps,hso_begin,hso_end,vso_bline,vso_eline,vso_begin,vso_end,
//de_h_begin,de_h_end,de_v_begin,de_v_end
/* below is test case for 8k 60hz */
/* {7680, 60000, 0x853, 0x87f, 0x10, 0x24, 0x0, 0x0, 0x49, 0x7c9, 0x50, 0x0}, */
/* below is from config_tv_enc_calc */
{7680, 60000, 0x5, 0x31, 0x2, 0x16, 0x5, 0x5, 0xc5, 0x845, 0x41, 0x1121}, //8k 60hz
/* below is test case for 8k 30hz */
/* {7680, 30000, 0x853, 0x87f, 0x10, 0x24, 0x0, 0x0, 0x49, 0x7c9, 0x50, 0x0}, */
/* below is from config_tv_enc_calc */
{7680, 30000, 0x5, 0x31, 0x2, 0x16, 0x5, 0x5, 0xc5, 0x845, 0x41, 0x1121}, //8k 30hz

/* below V timing is from test case for 8k60hz */
/* echo manual_vpu_bist_tmg 5 31 10 24 0 0 c5 845 50 0 a8c 1130 > debug */
/* {7680, 50000, 0x5, 0x31, 0x10, 0x24, 0x0, 0x0, 0xc5, 0x845, 0x50, 0x0}, //8k50hz */

{7680, 50000, 0x5, 0x31, 0x2, 0x16, 0x5, 0x5, 0xc5, 0x845, 0x41, 0x1121}, //8k50hz
/* echo manual_vpu_bist_tmg 5 31 2 16 5 5 c5 845 41 1121 a8c 1130 > debug */
{7680, 25000, 0x5, 0x31, 0x2, 0x16, 0x5, 0x5, 0xc5, 0x845, 0x41, 0x1121}, //8k25hz
/* note for 8k24hz, timing of vertical is different with 8k25/30/50/60hz */
{7680, 24000, 0x5, 0x31, 0x2, 0x16, 0x5, 0x5, 0xc5, 0x845, 0xa5, 0x1185}, //8k24hz
{3840, 120000, 0x5, 0x1b, 0x2, 0xc, 0x5, 0x5, 0x65, 0x425, 0x53, 0x8c3}, //4k120hz
{3840, 60000, 0x5, 0x1b, 0x2, 0xc, 0x5, 0x5, 0x65, 0x425, 0x53, 0x8c3}, //4k60hz
{3840, 100000, 0x5, 0x1b, 0x2, 0xc, 0x5, 0x5, 0x65, 0x425, 0x53, 0x8c3}, //4k100hz
{3840, 50000, 0x5, 0x1b, 0x2, 0xc, 0x5, 0x5, 0x65, 0x425, 0x53, 0x8c3}, //4k50hz
{0},
};

/* e.g. 8k60hz: vactive = 0x112f - 0x50 = 4319, hactive = 0x7c6 - 0x47 = 1919 */
/* the param come from config_tv_enc_calc() */
unsigned int encp_timing_video[][12] = {
//hactive,fps,hso_begin,hso_end,vso_bline,vso_eline,vso_begin,vso_end
//havon_begin,havon_end,vavon_bline,vavon_eline
/* below is test case for 8k60hz */
/* {7680, 60000, 0x851, 0x87d, 0x10, 0x24, 0x0, 0x0, 0x47, 0x7c6, 0x50, 0x112f}, */
/* below is from config_tv_enc_calc */
{7680, 60000, 0x3, 0x2f, 0x1, 0x15, 0x0, 0x0, 0xc3, 0x842, 0x41, 0x1120},//8k 60hz
/* below is test case for 8k30hz */
/* {7680, 30000, 0x851, 0x87d, 0x10, 0x24, 0x0, 0x0, 0x47, 0x7c6, 0x50, 0x112f}, */
/* below is from config_tv_enc_calc */
{7680, 30000, 0x3, 0x2f, 0x1, 0x15, 0x0, 0x0, 0xc3, 0x842, 0x41, 0x1120},//8k 30hz

/* below V timing is from test case for 8k60hz */
/* echo manual_vpu_video_tmg 3 2f 10 24 0 0 c3 842(should not be 843) 50 112f a8c 1130 > debug */
/* {7680, 50000, 0x3, 0x2f, 0x10, 0x24, 0x0, 0x0, 0xc3, 0x842, 0x50, 0x112f}, //8k50hz */

{7680, 50000, 0x3, 0x2f, 0x1, 0x15, 0x0, 0x0, 0xc3, 0x842, 0x41, 0x1120}, //8k50hz
/* echo manual_vpu_video_tmg 3 2f 1 15 0 0 c3 842 41 1120 a8c 1130 > debug */
{7680, 25000, 0x3, 0x2f, 0x1, 0x15, 0x0, 0x0, 0xc3, 0x842, 0x41, 0x1120}, //8k25hz
{7680, 24000, 0x3, 0x2f, 0x1, 0x15, 0x0, 0x0, 0xc3, 0x842, 0xa5, 0x1184}, //8k24hz
{3840, 120000, 0x3, 0x19, 0x1, 0xb, 0x0, 0x0, 0x63, 0x422, 0x53, 0x8c2}, //4k120hz
{3840, 60000, 0x3, 0x19, 0x1, 0xb, 0x0, 0x0, 0x63, 0x422, 0x53, 0x8c2}, //4k60hz
{3840, 100000, 0x3, 0x19, 0x1, 0xb, 0x0, 0x0, 0x63, 0x422, 0x53, 0x8c2}, //4k100hz
{3840, 50000, 0x3, 0x19, 0x1, 0xb, 0x0, 0x0, 0x63, 0x422, 0x53, 0x8c2}, //4k50hz
{0},
};

static inline void range_check(char *s, u32 a, u32 b, u32 c)
{
	if (((a) < (b)) || ((a) > (c)))
		DSC_ERR("%s out of range, needs to be between %d and %d\n", s, b, c);
}

static inline u8 dsc_clamp(u8 x, u8 min, u8 max)
{
	return ((x) > (max) ? (max) : ((x) < (min) ? (min) : (x)));
}

/*!
 ************************************************************************
 * \brief
 *    compute_offset() - Compute offset value at a specific group
 *
 * \param dsc_drv
 *    Pointer to DSC configuration structure
 * \param pixels_per_group
 *    Number of pixels per group
 * \param groups_per_line
 *    Number of groups per line
 * \param grp_cnt
 *    Group to compute offset for
 * \return
 *    Offset value for the group grp_cnt
 ************************************************************************
 */
static int compute_offset(struct aml_dsc_drv_s *dsc_drv, int pixels_per_group,
				int groups_per_line, int grp_cnt)
{
	int offset = 0;
	int grp_cnt_id;
	int bits_per_pixel_multiple = dsc_drv->bits_per_pixel_multiple;

	if (pixels_per_group == 0)
		return offset;

	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("[%s]bits_per_pixel_multiple:%d\n", __func__, bits_per_pixel_multiple);

	grp_cnt_id = DIV_ROUND_UP(dsc_drv->pps_data.initial_xmit_delay, pixels_per_group);
	if (grp_cnt <= grp_cnt_id)
		offset = DIV_ROUND_UP(grp_cnt * pixels_per_group * bits_per_pixel_multiple,
						BPP_FRACTION);
	else
		offset = DIV_ROUND_UP(grp_cnt_id * pixels_per_group * bits_per_pixel_multiple,
				BPP_FRACTION) -
				(((grp_cnt - grp_cnt_id) * dsc_drv->pps_data.slice_bpg_offset) >>
				OFFSET_FRACTIONAL_BITS);
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("[%s]offset:%d\n", __func__, offset);

	if (grp_cnt <= groups_per_line)
		offset += grp_cnt * dsc_drv->pps_data.first_line_bpg_offset;
	else
		offset += groups_per_line * dsc_drv->pps_data.first_line_bpg_offset -
			(((grp_cnt - groups_per_line) * dsc_drv->pps_data.nfl_bpg_offset) >>
			OFFSET_FRACTIONAL_BITS);
	if (dsc_drv->pps_data.native_420) {
		if (grp_cnt <= groups_per_line)
			offset -= (grp_cnt * dsc_drv->pps_data.nsl_bpg_offset) >>
					OFFSET_FRACTIONAL_BITS;
		else if (grp_cnt <= 2 * groups_per_line)
			offset += (grp_cnt - groups_per_line) *
				dsc_drv->pps_data.second_line_bpg_offset -
				((groups_per_line * dsc_drv->pps_data.nsl_bpg_offset) >>
					OFFSET_FRACTIONAL_BITS);
		else
			offset += (grp_cnt - groups_per_line) *
				dsc_drv->pps_data.second_line_bpg_offset -
				(((grp_cnt - groups_per_line) * dsc_drv->pps_data.nsl_bpg_offset) >>
				OFFSET_FRACTIONAL_BITS);
	}
	return offset;
}

/*!
 ************************************************************************
 * \brief
 *    compute_rc_parameters() - Compute rate control parameters
 *
 * \param dsc_drv
 *    Pointer to DSC configuration structure
 * \param pixels_per_group
 *    Number of pixels per group
 * \param groups_per_line
 *    Number of groups per line
 * \param groups per cnt
 *    Group to compute offset for
 * \return
 *    0 = success; 1 = error with configuration
 ************************************************************************
 */
static int compute_rc_parameters(struct aml_dsc_drv_s *dsc_drv, int pixels_per_group, int num_s_sps)
{
	int groups_per_line;
	int num_extra_mux_bits;
	int slice_bits;
	int final_value;
	int final_scale;
	int invalid = 0;
	int groups_total;
	int max_offset;
	int rbs_min;
	int hrd_delay;
	int slice_w;
	int uncompressed_bpg_rate;
	int initial_xmit_multi_bpp;
	int bits_per_pixel_multiple = dsc_drv->bits_per_pixel_multiple;
	int rc_model_size = dsc_drv->pps_data.rc_parameter_set.rc_model_size;

	slice_w = dsc_drv->pps_data.slice_width >> (dsc_drv->pps_data.native_420 ||
					dsc_drv->pps_data.native_422);
	if (dsc_drv->pps_data.native_422)
		uncompressed_bpg_rate = 3 * dsc_drv->pps_data.bits_per_component * 4;
	else
		uncompressed_bpg_rate = (3 * dsc_drv->pps_data.bits_per_component +
					(!dsc_drv->pps_data.convert_rgb ? 0 : 2)) * 3;

	if (dsc_drv->pps_data.slice_height >= 8)
		dsc_drv->pps_data.first_line_bpg_offset = 12 +
			(9 * BPP_FRACTION / 100 * MIN(34, dsc_drv->pps_data.slice_height - 8)) /
									BPP_FRACTION;
	else
		dsc_drv->pps_data.first_line_bpg_offset = 2 * (dsc_drv->pps_data.slice_height - 1);
	dsc_drv->pps_data.first_line_bpg_offset = dsc_clamp(dsc_drv->pps_data.first_line_bpg_offset,
			0, ((uncompressed_bpg_rate * BPP_FRACTION - 3 * bits_per_pixel_multiple) /
									BPP_FRACTION));
	range_check("first_line_bpg_offset", dsc_drv->pps_data.first_line_bpg_offset, 0, 31);
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("first_line_bpg_offset:%d\n", dsc_drv->pps_data.first_line_bpg_offset);

	dsc_drv->pps_data.second_line_bpg_offset = dsc_drv->pps_data.native_420 ? 12 : 0;
	dsc_drv->pps_data.second_line_bpg_offset =
				dsc_clamp(dsc_drv->pps_data.second_line_bpg_offset,
					0, ((uncompressed_bpg_rate * BPP_FRACTION - 3 *
					bits_per_pixel_multiple) / BPP_FRACTION));
	range_check("second_line_bpg_offset", dsc_drv->pps_data.second_line_bpg_offset, 0, 31);
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("second_line_bpg_offset:%d\n", dsc_drv->pps_data.second_line_bpg_offset);

	groups_per_line = (slice_w + pixels_per_group - 1) / pixels_per_group;
	dsc_drv->pps_data.chunk_size = (int)DIV_ROUND_UP(slice_w * bits_per_pixel_multiple,
									(8 * BPP_FRACTION));
	range_check("chunk_size", dsc_drv->pps_data.chunk_size, 0, 65535);
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("chunk_size:%d\n", dsc_drv->pps_data.chunk_size);

	if (dsc_drv->pps_data.convert_rgb)
		num_extra_mux_bits = (num_s_sps * (dsc_drv->mux_word_size +
			(4 * dsc_drv->pps_data.bits_per_component + 4) - 2));
	else if (!dsc_drv->pps_data.native_422) // YCbCr
		num_extra_mux_bits = (num_s_sps * dsc_drv->mux_word_size +
			(4 * dsc_drv->pps_data.bits_per_component + 4) +
			2 * (4 * dsc_drv->pps_data.bits_per_component) - 2);
	else
		num_extra_mux_bits = (num_s_sps * dsc_drv->mux_word_size +
			(4 * dsc_drv->pps_data.bits_per_component + 4) + 3 *
			(4 * dsc_drv->pps_data.bits_per_component) - 2);
	slice_bits = 8 * dsc_drv->pps_data.chunk_size * dsc_drv->pps_data.slice_height;
	while ((num_extra_mux_bits > 0) &&
	       ((slice_bits - num_extra_mux_bits) % dsc_drv->mux_word_size))
		num_extra_mux_bits--;

	dsc_drv->pps_data.initial_scale_value = 8 * rc_model_size /
				(rc_model_size - dsc_drv->pps_data.initial_offset);
	if (groups_per_line < dsc_drv->pps_data.initial_scale_value - 8)
		dsc_drv->pps_data.initial_scale_value = groups_per_line + 8;
	range_check("initial_scale_value", dsc_drv->pps_data.initial_scale_value, 0, 63);

	if (dsc_drv->pps_data.initial_scale_value > 8)
		dsc_drv->pps_data.scale_decrement_interval = groups_per_line /
						(dsc_drv->pps_data.initial_scale_value - 8);
	else
		dsc_drv->pps_data.scale_decrement_interval = 4095;
	range_check("scale_decrement_interval",
		dsc_drv->pps_data.scale_decrement_interval, 0, 4095);

	final_value = rc_model_size -
		((dsc_drv->pps_data.initial_xmit_delay *
			dsc_drv->pps_data.bits_per_pixel + 8) >> 4) + num_extra_mux_bits;
	dsc_drv->pps_data.final_offset = final_value;
	range_check("final_offset", dsc_drv->pps_data.final_offset, 0, 65535);

	if (final_value >= rc_model_size)
		DSC_ERR("final_offset must be less than rc_model_size add initial_xmit_delay.\n");
	final_scale = 8 * rc_model_size /
			(rc_model_size - final_value);
	if (final_scale > 63)
		DSC_ERR("WARNING: final scale value > than 63/8 increasing initial_xmit_delay.\n");
	if (dsc_drv->pps_data.slice_height > 1)
		dsc_drv->pps_data.nfl_bpg_offset =
			(int)DIV_ROUND_UP(dsc_drv->pps_data.first_line_bpg_offset <<
				OFFSET_FRACTIONAL_BITS, (dsc_drv->pps_data.slice_height - 1));
	else
		dsc_drv->pps_data.nfl_bpg_offset = 0;

	if (dsc_drv->pps_data.nfl_bpg_offset > 65535) {
		DSC_ERR("nfl_bpg_offset is too large for this slice height\n");
		invalid = 1;
	}
	if (dsc_drv->pps_data.slice_height > 2)
		dsc_drv->pps_data.nsl_bpg_offset =
			(int)DIV_ROUND_UP(dsc_drv->pps_data.second_line_bpg_offset <<
					OFFSET_FRACTIONAL_BITS,
					(dsc_drv->pps_data.slice_height - 1));
	else
		dsc_drv->pps_data.nsl_bpg_offset = 0;

	if (dsc_drv->pps_data.nsl_bpg_offset > 65535) {
		DSC_ERR("nsl_bpg_offset is too large for this slice height\n");
		invalid = 1;
	}
	groups_total = groups_per_line * dsc_drv->pps_data.slice_height;
	dsc_drv->pps_data.slice_bpg_offset = (int)DIV_ROUND_UP((1 << OFFSET_FRACTIONAL_BITS) *
				(rc_model_size -
				dsc_drv->pps_data.initial_offset + num_extra_mux_bits),
				groups_total);
	range_check("slice_bpg_offset", dsc_drv->pps_data.slice_bpg_offset, 0, 65535);
	if (dsc_drv->pps_data.slice_height == 1) {
		if (dsc_drv->pps_data.first_line_bpg_offset > 0)
			DSC_ERR("For slice_height == 1, the FIRST_LINE_BPG_OFFSET must be 0\n");
	}

	// BEGIN scale_increment_interval fix
	if (final_scale > 9) {
		dsc_drv->pps_data.scale_increment_interval = (int)((1 << OFFSET_FRACTIONAL_BITS) *
					dsc_drv->pps_data.final_offset /
					((final_scale - 9) * (dsc_drv->pps_data.nfl_bpg_offset +
					 dsc_drv->pps_data.slice_bpg_offset +
					 dsc_drv->pps_data.nsl_bpg_offset)));
		if (dsc_drv->pps_data.scale_increment_interval > 65535) {
			DSC_ERR("scale_increment_interval is too large for this slice height.\n");
			invalid = 1;
		}
	} else {
		dsc_drv->pps_data.scale_increment_interval = 0;
	}
	// END scale_increment_interval fix

	if (dsc_drv->pps_data.dsc_version_minor == 2 &&
	   (dsc_drv->pps_data.native_420 || dsc_drv->pps_data.native_422)) {
		max_offset = compute_offset(dsc_drv, pixels_per_group, groups_per_line,
			DIV_ROUND_UP(dsc_drv->pps_data.initial_xmit_delay, pixels_per_group));
		max_offset = MAX(max_offset, compute_offset(dsc_drv, pixels_per_group,
						groups_per_line, groups_per_line));
		max_offset = MAX(max_offset, compute_offset(dsc_drv, pixels_per_group,
						groups_per_line, 2 * groups_per_line));
		rbs_min = rc_model_size - dsc_drv->pps_data.initial_offset + max_offset;
	} else {  // DSC 1.1 method
		initial_xmit_multi_bpp =
			DIV_ROUND_UP(dsc_drv->pps_data.initial_xmit_delay * bits_per_pixel_multiple,
									BPP_FRACTION);
		//DSC_ERR("initial_xmit_multi_bpp:%d\n", initial_xmit_multi_bpp);
		rbs_min = (int)(rc_model_size - dsc_drv->pps_data.initial_offset +
			initial_xmit_multi_bpp +
			groups_per_line * dsc_drv->pps_data.first_line_bpg_offset);
		if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
			DSC_PR("initial_xmit_multi_bpp:%d rbs_min:%d\n",
				initial_xmit_multi_bpp, rbs_min);
	}

	hrd_delay = DIV_ROUND_UP(rbs_min * BPP_FRACTION, bits_per_pixel_multiple);
	dsc_drv->rcb_bits = DIV_ROUND_UP(hrd_delay * bits_per_pixel_multiple, BPP_FRACTION);
	dsc_drv->pps_data.initial_dec_delay = hrd_delay - dsc_drv->pps_data.initial_xmit_delay;
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("hrd_delay:%d rcb_bits:%d\n", hrd_delay, dsc_drv->rcb_bits);

	return invalid;
}

int qlevel_luma_8bpc[] = {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5, 6, 7};
int qlevel_chroma_8bpc[] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8};
int qlevel_luma_10bpc[] = {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7, 8, 9};
int qlevel_chroma_10bpc[] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 10, 10, 10};
int qlevel_luma_12bpc[] = {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9,
				10, 11};
int qlevel_chroma_12bpc[] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
				12, 12, 12};
int qlevel_luma_14bpc[] =   {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,  9,  9,
				10, 10, 11, 11, 11, 12, 13};
int qlevel_chroma_14bpc[] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
				11, 12, 12, 13, 14, 14, 14};
int qlevel_luma_16bpc[] =   {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,  9,  9,
				10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 15};
int qlevel_chroma_16bpc[] = {0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
				11, 12, 12, 13, 13, 14, 14, 15, 16, 16, 16};

static int Qp2Qlevel(struct aml_dsc_drv_s *dsc_drv, int qp, int cpnt)
{
	int qlevel = 0;

	if (((cpnt % 3) == 0) || (dsc_drv->pps_data.native_420 && cpnt == 1)) {
		switch (dsc_drv->pps_data.bits_per_component) {
		case 8:
			qlevel = qlevel_luma_8bpc[qp];
			break;
		case 10:
			qlevel = qlevel_luma_10bpc[qp];
			break;
		case 12:
			qlevel = qlevel_luma_12bpc[qp];
			break;
		case 14:
			qlevel = qlevel_luma_14bpc[qp];
			break;
		case 16:
			qlevel = qlevel_luma_16bpc[qp];
			break;
		}
	} else {
		switch (dsc_drv->pps_data.bits_per_component) {
		case 8:
			qlevel = qlevel_chroma_8bpc[qp];
			break;
		case 10:
			qlevel = qlevel_chroma_10bpc[qp];
			break;
		case 12:
			qlevel = qlevel_chroma_12bpc[qp];
			break;
		case 14:
			qlevel = qlevel_chroma_14bpc[qp];
			break;
		case 16:
			qlevel = qlevel_chroma_16bpc[qp];
			break;
		}
		if (dsc_drv->pps_data.dsc_version_minor == 2 && !dsc_drv->pps_data.convert_rgb)
			qlevel = MAX(0, qlevel - 1);
	}

	return (qlevel);
}

/*!
 ************************************************************************
 * \brief
 *    check_qp_for_overflow() - Ensure max QP's are programmed to avoid overflow
 *
 * \param dsc_cfg
 *    Pointer to DSC configuration structure
 * \param pixel_per_group
 *    Number of pixels per group
 * \return
 *    0 = success; 1 = error with configuration
 ************************************************************************
 */
void check_qp_for_overflow(struct aml_dsc_drv_s *dsc_drv, int pixel_per_group)
{
	int p_mode_bits, max_bits;
	int cpnt, max_res_size;
	int cpntBitDepth[NUM_COMPONENTS];
	struct dsc_rc_parameter_set *rc_parameter_set;

	rc_parameter_set = &dsc_drv->pps_data.rc_parameter_set;
	for (cpnt = 0; cpnt < NUM_COMPONENTS; ++cpnt)
		cpntBitDepth[cpnt] = (dsc_drv->pps_data.convert_rgb && (cpnt == 1 || cpnt == 2)) ?
			(dsc_drv->pps_data.bits_per_component + 1) :
			dsc_drv->pps_data.bits_per_component;

	// MPP group when predicted size is 0
	p_mode_bits = 1;  // extra bit for luma prefix/ICH switch
	for (cpnt = 0; cpnt < (dsc_drv->pps_data.native_422 ? 4 : 3); ++cpnt) {
		max_res_size = cpntBitDepth[cpnt] -
			Qp2Qlevel(dsc_drv, rc_parameter_set->rc_range_parameters[14].range_max_qp,
				  cpnt);
		p_mode_bits += max_res_size * 4;  // prefix + residuals
	}

	// Followed by predicted group (where predicted size is max size-1)
	max_bits = p_mode_bits;
	p_mode_bits = 1;
	for (cpnt = 0; cpnt < (dsc_drv->pps_data.native_422 ? 4 : 3); ++cpnt) {
		max_res_size = cpntBitDepth[cpnt] -
		Qp2Qlevel(dsc_drv, rc_parameter_set->rc_range_parameters[14].range_max_qp,
										cpnt) - 1;
		p_mode_bits += 1 + max_res_size * 3;  // prefix (1bit) + residuals
	}
	max_bits += p_mode_bits;
}

static inline unsigned int dsc_do_div(unsigned long long num, unsigned int den)
{
	unsigned long long val = num;

	do_div(val, den);

	return (unsigned int)val;
}

/*!
 * \brief
 *    generate_rc_parameters() - Generate RC parameters
 *
 * \param dsc_drv
 *    PPS data structure
 ************************************************************************
 */
static void generate_rc_parameters(struct aml_dsc_drv_s *dsc_drv)
{
	struct dsc_rc_parameter_set *rc_parameter_set;
	int qp_bpc_modifier;
	int i;
	int padding_pixels;
	int sw;
	int bits_per_pixel_int = dsc_drv->bits_per_pixel_int;
	unsigned int bits_per_pixel_multiple = dsc_drv->bits_per_pixel_multiple;
	int slices;
	u8 bits_per_component;

	rc_parameter_set = &dsc_drv->pps_data.rc_parameter_set;
	bits_per_component = dsc_drv->pps_data.bits_per_component;
	make_qp_tables();

	qp_bpc_modifier = (bits_per_component - 8) * 2 -
		(!dsc_drv->pps_data.convert_rgb && (dsc_drv->pps_data.dsc_version_minor == 1));
	rc_parameter_set->rc_quant_incr_limit0 = 11 + qp_bpc_modifier;
	rc_parameter_set->rc_quant_incr_limit1 = 11 + qp_bpc_modifier;

	if (dsc_drv->pps_data.native_422) {
		if (bits_per_pixel_int >= 16)
			dsc_drv->pps_data.initial_offset = 2048;
		else if (bits_per_pixel_int >= 14)
			dsc_drv->pps_data.initial_offset = 5632 -
				(int)(((bits_per_pixel_multiple - 14 * BPP_FRACTION) * 1792 +
				5 * BPP_FRACTION / 10) / BPP_FRACTION);
		else if (bits_per_pixel_int >= 12)
			dsc_drv->pps_data.initial_offset = 5632;
		else
			DSC_PR("No auto-generated parameters bitsPerPixel < 6 in 4:2:2 mode\n");
		if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
			DSC_PR("initial_offset:%d\n", dsc_drv->pps_data.initial_offset);
	} else {// 4:4:4 or simple 4:2:2 or native 4:2:0
		if (bits_per_pixel_int >= 12)
			dsc_drv->pps_data.initial_offset = 2048;
		else if (bits_per_pixel_int >= 10)
			dsc_drv->pps_data.initial_offset = 5632 -
					(int)(((bits_per_pixel_multiple - 10 * BPP_FRACTION) *
						1792 + 5 * BPP_FRACTION / 10) / BPP_FRACTION);
		else if (bits_per_pixel_int >= 8)
			dsc_drv->pps_data.initial_offset = 6144 -
					(int)(((bits_per_pixel_multiple - 8 * BPP_FRACTION) * 256 +
						5 * BPP_FRACTION / 10) / BPP_FRACTION);
		else if (bits_per_pixel_int >= 6)
			dsc_drv->pps_data.initial_offset = 6144;
		else
			DSC_PR("No auto-generated parameters bitsPerPixel < 6 in 4:4:4 mode\n");
		if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
			DSC_PR("initial_offset:%d\n", dsc_drv->pps_data.initial_offset);
	}
	/* 409600000000 = 4096*BPP_FRACTION*BPP_FRACTION */
	dsc_drv->pps_data.initial_xmit_delay =
		dsc_do_div(dsc_do_div(409600000000, bits_per_pixel_multiple) +
			5 * BPP_FRACTION / 10, BPP_FRACTION);
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("initial_xmit_delay:%d\n", dsc_drv->pps_data.initial_xmit_delay);

	sw = ((dsc_drv->pps_data.native_422 || dsc_drv->pps_data.native_420) ?
		(dsc_drv->pps_data.slice_width / 2) : dsc_drv->pps_data.slice_width);
	padding_pixels = ((sw % 3) ? (3 - (sw % 3)) : 0) *
			(dsc_drv->pps_data.initial_xmit_delay / sw);
	if (3 * bits_per_pixel_multiple >=
	    (((dsc_drv->pps_data.initial_xmit_delay + 2) / 3) *
	     (dsc_drv->pps_data.native_422 ? 4 : 3) * BPP_FRACTION) &&
	     (((dsc_drv->pps_data.initial_xmit_delay + padding_pixels) % 3) == 1))
		dsc_drv->pps_data.initial_xmit_delay++;
	if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
		DSC_PR("initial_xmit_delay:%d\n", dsc_drv->pps_data.initial_xmit_delay);

	dsc_drv->pps_data.flatness_min_qp = 3 + qp_bpc_modifier;
	dsc_drv->pps_data.flatness_max_qp = 12 + qp_bpc_modifier;
	dsc_drv->flatness_det_thresh = 2 << (bits_per_component - 8);
	// The following two lines were added in 1.57e
	if (dsc_drv->pps_data.native_420)
		dsc_drv->pps_data.second_line_offset_adj = 512;
	else
		dsc_drv->pps_data.second_line_offset_adj = 0;

	for (i = 0; i < 15; ++i) {
		int idx;

		if (dsc_drv->pps_data.native_420) {
			int ofs_und4[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12,
					   -12, -12 };
			int ofs_und5[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12,
					   -12, -12 };
			int ofs_und6[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12,
					   -12, -12 };
			int ofs_und8[] = { 10, 8, 6, 4, 2, 0, -2, -4, -6, -8, -10, -10, -12,
					   -12, -12 };

			idx = (int)((bits_per_pixel_multiple - 8 * BPP_FRACTION) / BPP_FRACTION);
			if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
				DSC_PR("idx:%d\n", idx);
			rc_parameter_set->rc_range_parameters[i].range_min_qp =
					min_qp_420[(bits_per_component - 8) / 2][i][idx];
			rc_parameter_set->rc_range_parameters[i].range_max_qp =
					max_qp_420[(bits_per_component - 8) / 2][i][idx];
			if (bits_per_pixel_multiple <= 8 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und4[i];
			else if (bits_per_pixel_multiple <= 10 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und4[i] +
					(int)(((5 * (bits_per_pixel_multiple - 8 * BPP_FRACTION) /
					10) * (ofs_und5[i] - ofs_und4[i]) +
					5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else if (bits_per_pixel_multiple <= 12 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und5[i] +
					(int)(((5 * (bits_per_pixel_multiple - 10 * BPP_FRACTION) /
					10) * (ofs_und6[i] - ofs_und5[i]) +
					5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else if (bits_per_pixel_multiple <= 16 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und6[i] +
					(int)(((25 * (bits_per_pixel_multiple - 12 * BPP_FRACTION) /
					100) * (ofs_und8[i] - ofs_und6[i]) +
					5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und8[i];
			if (dsc_drv->dsc_print_en & DSC_PPS_QP_VALUE)
				DSC_PR("min_pq_420:%d max_pq_420:%d range_bgp_offset:%d\n",
					min_qp_420[(bits_per_component - 8) / 2][i][idx],
					max_qp_420[(bits_per_component - 8) / 2][i][idx],
					rc_parameter_set->rc_range_parameters[i].range_bpg_offset);
		} else if (dsc_drv->pps_data.native_422) {
			int ofs_und6[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12,
					   -12, -12 };
			int ofs_und7[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12,
					   -12, -12 };
			int ofs_und10[] = { 10, 8, 6, 4, 2, 0, -2, -4, -6, -8, -10, -10, -12,
					    -12, -12 };
			idx = (int)((bits_per_pixel_multiple - 12 * BPP_FRACTION) / BPP_FRACTION);
			if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
				DSC_PR("idx:%d\n", idx);
			rc_parameter_set->rc_range_parameters[i].range_min_qp =
					min_qp_422[(bits_per_component - 8) / 2][i][idx];
			rc_parameter_set->rc_range_parameters[i].range_max_qp =
					max_qp_422[(bits_per_component - 8) / 2][i][idx];
			if (bits_per_pixel_multiple <= 12 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und6[i];
			else if (bits_per_pixel_multiple <= 14 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und6[i] +
					(int)(((bits_per_pixel_multiple - 12 * BPP_FRACTION) *
					(ofs_und7[i] - ofs_und6[i]) / 2 +
					5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else if (bits_per_pixel_multiple <= 16 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und7[i];
			else if (bits_per_pixel_multiple <= 20 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und7[i] +
					(int)(((bits_per_pixel_multiple - 16 * BPP_FRACTION) *
					(ofs_und10[i] - ofs_und7[i]) / 4 +
					5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und10[i];
			if (dsc_drv->dsc_print_en & DSC_PPS_QP_VALUE)
				DSC_PR("min_pq_422:%d max_pq_422:%d range_bgp_offset:%d\n",
					min_qp_422[(bits_per_component - 8) / 2][i][idx],
					max_qp_422[(bits_per_component - 8) / 2][i][idx],
					rc_parameter_set->rc_range_parameters[i].range_bpg_offset);
		} else {
			int ofs_und6[] = { 0, -2, -2, -4, -6, -6, -8, -8, -8, -10, -10, -12, -12,
					   -12, -12 };
			int ofs_und8[] = { 2, 0, 0, -2, -4,	-6,	-8,	-8, -8, -10, -10,
					   -10, -12, -12, -12 };
			int ofs_und12[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12,
					    -12, -12 };
			int ofs_und15[] = { 10, 8, 6, 4, 2, 0, -2, -4, -6, -8, -10, -10, -12,
					    -12, -12 };
			idx = (int)(2 * (bits_per_pixel_multiple - 6 * BPP_FRACTION) /
					BPP_FRACTION);
			if (dsc_drv->dsc_print_en & DSC_PPS_PARA_EN)
				DSC_PR("idx:%d\n", idx);
			rc_parameter_set->rc_range_parameters[i].range_min_qp =
				MAX(0, min_qp_444[(bits_per_component - 8) / 2][i][idx] -
					(!dsc_drv->pps_data.convert_rgb &&
					 (dsc_drv->pps_data.dsc_version_minor == 1)));
			rc_parameter_set->rc_range_parameters[i].range_max_qp =
				MAX(0, max_qp_444[(bits_per_component - 8) / 2][i][idx] -
					(!dsc_drv->pps_data.convert_rgb &&
					 (dsc_drv->pps_data.dsc_version_minor == 1)));
			if (bits_per_pixel_multiple <= 6 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und6[i];
			else if (bits_per_pixel_multiple <= 8 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
					ofs_und6[i] +
					(int)(((5 * (bits_per_pixel_multiple - 6 * BPP_FRACTION) /
						10) * (ofs_und8[i] - ofs_und6[i]) +
						5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else if (bits_per_pixel_multiple <= 12 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und8[i];
			else if (bits_per_pixel_multiple <= 15 * BPP_FRACTION)
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und12[i] +
					(int)(((bits_per_pixel_multiple - 12 * BPP_FRACTION) *
						(ofs_und15[i] - ofs_und12[i]) / 3 +
						5 * BPP_FRACTION / 10) / BPP_FRACTION);
			else
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset =
										ofs_und15[i];
			if (dsc_drv->dsc_print_en & DSC_PPS_QP_VALUE)
				DSC_PR("min_pq_444:%d max_pq_444:%d range_bgp_offset:%d\n",
				min_qp_444[(bits_per_component - 8) / 2][i][idx],
				max_qp_444[(bits_per_component - 8) / 2][i][idx],
				rc_parameter_set->rc_range_parameters[i].range_bpg_offset);
		}
		// The following code was added in 1.57g (parameter sanity check)
		range_check("range_max_qp",
			rc_parameter_set->rc_range_parameters[i].range_max_qp, 0,
				15 + 2 * (bits_per_component - 8));
		if (dsc_drv->pps_data.dsc_version_minor == 1 && !dsc_drv->pps_data.convert_rgb &&
		    rc_parameter_set->rc_range_parameters[i].range_max_qp > 12 +
			2 * (bits_per_component - 8))
			DSC_PR("ERROR:DSC 1.1 mode with YCbCr max QP for range %d less than %d\n",
				i, 12 + 2 * (bits_per_component - 8));
	}
	destroy_qp_tables();

	if (dsc_drv->color_format == HDMI_COLORSPACE_YUV420 ||
	    dsc_drv->color_format == HDMI_COLORSPACE_YUV422)
		bits_per_pixel_multiple = bits_per_pixel_multiple / 2;
	slices = dsc_drv->pps_data.pic_width / dsc_drv->pps_data.slice_width;
	dsc_drv->pps_data.hc_active_bytes = slices * DIV_ROUND_UP(dsc_drv->pps_data.slice_width *
						bits_per_pixel_multiple, 8 * BPP_FRACTION);
}

static void populate_pps(struct aml_dsc_drv_s *dsc_drv)
{
	int i;
	int pixels_per_group, num_s_sps;
	struct dsc_pps_data_s *pps_data = &dsc_drv->pps_data;
	/* where's the defination of default value? */
	int default_threshold[] = {896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744,
					7872, 8000, 8064};

	for (i = 0; i < RC_BUF_THRESH_NUM; i++)
		pps_data->rc_parameter_set.rc_buf_thresh[i] = default_threshold[i] >> 6;

	if (pps_data->native_422 && pps_data->native_420)
		DSC_ERR("NATIVE_420 and NATIVE_422 cannot both be enabled at the same time\n");

	if (pps_data->bits_per_component != 8 && pps_data->bits_per_component != 10 &&
	    pps_data->bits_per_component != 12 && pps_data->bits_per_component != 14 &&
	    pps_data->bits_per_component != 16)
		DSC_ERR("bits_per_component must be either 8, 10, 12, 14, or 16\n");

	// Removed from PPS in v1.44:
	dsc_drv->very_flat_qp = 1 + (2 * (pps_data->bits_per_component - 8));
	dsc_drv->somewhat_flat_qp_delta = 4;
	dsc_drv->somewhat_flat_qp_thresh = 7 + (2 * (pps_data->bits_per_component - 8));
	if (pps_data->bits_per_component <= 10)
		dsc_drv->mux_word_size = 48;
	else
		dsc_drv->mux_word_size = 64;

	// The following line was added in v1.57g:

	if (pps_data->rc_parameter_set.rc_model_size <= pps_data->initial_offset)
		DSC_PR("INITIAL_OFFSET must be less than RC_MODEL_SIZE\n");
	pps_data->initial_scale_value =
		8 * pps_data->rc_parameter_set.rc_model_size /
			(pps_data->rc_parameter_set.rc_model_size - pps_data->initial_offset);
	range_check("initial_scale_value", pps_data->initial_scale_value, 0, 63);

	// Compute rate buffer size for auto mode
	if (pps_data->native_422) {
		num_s_sps = 4;
		pixels_per_group = 3;
	} else {
		num_s_sps = 3;
		pixels_per_group = 3;
	}

	generate_rc_parameters(dsc_drv);

	if (compute_rc_parameters(dsc_drv, pixels_per_group, num_s_sps))
		DSC_PR("One or more PPS parameters exceeded their allowed bit depth.");
	check_qp_for_overflow(dsc_drv, pixels_per_group);
}

static void dsc_config_timing_value(struct aml_dsc_drv_s *dsc_drv)
{
	int i;
	unsigned int dsc_timing_mode = 0;
	unsigned int venc_timing_mode = 0;

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_DSC_TMG_CTRL)) {
		for (i = 0; dsc_timing[i][0] != DSC_ENCODE_MAX; i++) {
			if (dsc_timing[i][0] == dsc_drv->dsc_mode &&
				dsc_timing[i][1] == dsc_drv->bits_per_pixel_really_value) {
				dsc_timing_mode = i;
				break;
			}
		}
		if (dsc_timing[i][0] == DSC_ENCODE_MAX) {
			DSC_ERR("%s didn't find dsc timing!!!\n", __func__);
			return;
		}
		//dsc timing set
		dsc_drv->tmg_ctrl.tmg_havon_begin	= dsc_timing[dsc_timing_mode][2];
		dsc_drv->tmg_ctrl.tmg_vavon_bline	= dsc_timing[dsc_timing_mode][3];
		dsc_drv->tmg_ctrl.tmg_vavon_eline	= dsc_timing[dsc_timing_mode][4];
		dsc_drv->tmg_ctrl.tmg_hso_begin		= dsc_timing[dsc_timing_mode][5];
		dsc_drv->tmg_ctrl.tmg_hso_end		= dsc_timing[dsc_timing_mode][6];
		dsc_drv->tmg_ctrl.tmg_vso_begin		= dsc_timing[dsc_timing_mode][7];
		dsc_drv->tmg_ctrl.tmg_vso_end		= dsc_timing[dsc_timing_mode][8];
		dsc_drv->tmg_ctrl.tmg_vso_bline		= dsc_timing[dsc_timing_mode][9];
		dsc_drv->tmg_ctrl.tmg_vso_eline		= dsc_timing[dsc_timing_mode][10];
		dsc_drv->hc_vtotal_m1			= dsc_timing[dsc_timing_mode][11];
		dsc_drv->hc_htotal_m1			= dsc_timing[dsc_timing_mode][12];
	}

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_VPU_BIST_TMG_CTRL)) {
		for (i = 0; encp_timing_bist[i][0] != 0; i++) {
			if (encp_timing_bist[i][0] == dsc_drv->pps_data.pic_width &&
				encp_timing_bist[i][1] == dsc_drv->fps) {
				venc_timing_mode = i;
				break;
			}
		}
		if (encp_timing_bist[i][0] == 0) {
			DSC_ERR("%s didn't find venc bist timing!!!\n", __func__);
			return;
		}
		//encp bist timing set
		dsc_drv->encp_timing_ctrl.encp_dvi_hso_begin =
			encp_timing_bist[venc_timing_mode][2];
		dsc_drv->encp_timing_ctrl.encp_dvi_hso_end = encp_timing_bist[venc_timing_mode][3];
		dsc_drv->encp_timing_ctrl.encp_vso_bline = encp_timing_bist[venc_timing_mode][4];
		dsc_drv->encp_timing_ctrl.encp_vso_eline = encp_timing_bist[venc_timing_mode][5];
		dsc_drv->encp_timing_ctrl.encp_vso_begin = encp_timing_bist[venc_timing_mode][6];
		dsc_drv->encp_timing_ctrl.encp_vso_end = encp_timing_bist[venc_timing_mode][7];
		dsc_drv->encp_timing_ctrl.encp_de_h_begin = encp_timing_bist[venc_timing_mode][8];
		dsc_drv->encp_timing_ctrl.encp_de_h_end = encp_timing_bist[venc_timing_mode][9];
		dsc_drv->encp_timing_ctrl.encp_de_v_begin = encp_timing_bist[venc_timing_mode][10];
		dsc_drv->encp_timing_ctrl.encp_de_v_end = encp_timing_bist[venc_timing_mode][11];
	}

	if (!(dsc_drv->dsc_debug.manual_set_select & MANUAL_VPU_VIDEO_TMG_CTRL)) {
		for (i = 0; encp_timing_video[i][0] != 0; i++) {
			if (encp_timing_video[i][0] == dsc_drv->pps_data.pic_width &&
				encp_timing_video[i][1] == dsc_drv->fps) {
				venc_timing_mode = i;
				break;
			}
		}
		if (encp_timing_video[i][0] == 0) {
			DSC_ERR("%s didn't find venc video timing!!!\n", __func__);
			return;
		}
		//encp video timing set
		dsc_drv->encp_timing_ctrl.encp_hso_begin = encp_timing_video[venc_timing_mode][2];
		dsc_drv->encp_timing_ctrl.encp_hso_end = encp_timing_video[venc_timing_mode][3];
		dsc_drv->encp_timing_ctrl.encp_video_vso_bline =
			encp_timing_video[venc_timing_mode][4];
		dsc_drv->encp_timing_ctrl.encp_video_vso_eline =
			encp_timing_video[venc_timing_mode][5];
		dsc_drv->encp_timing_ctrl.encp_video_vso_begin =
			encp_timing_video[venc_timing_mode][6];
		dsc_drv->encp_timing_ctrl.encp_video_vso_end =
			encp_timing_video[venc_timing_mode][7];
		dsc_drv->encp_timing_ctrl.encp_havon_begin =
			encp_timing_video[venc_timing_mode][8];
		dsc_drv->encp_timing_ctrl.encp_havon_end =
			encp_timing_video[venc_timing_mode][9];
		dsc_drv->encp_timing_ctrl.encp_vavon_bline =
			encp_timing_video[venc_timing_mode][10];
		dsc_drv->encp_timing_ctrl.encp_vavon_eline =
			encp_timing_video[venc_timing_mode][11];
	}
	DSC_PR("[%s] dsc_timing_mode:%d venc_timing_mode:%d\n",
		__func__, dsc_timing_mode, venc_timing_mode);
}

static void dsc_enc_confirm_clk_en(struct aml_dsc_drv_s *dsc_drv)
{
	switch (dsc_drv->dsc_mode) {
	case DSC_RGB_3840X2160_120HZ:
	case DSC_YUV444_3840X2160_120HZ:
	case DSC_RGB_3840X2160_100HZ:
	case DSC_YUV444_3840X2160_100HZ:
		/* 4 slice */
		dsc_drv->c3_clk_en = 1;
		dsc_drv->c2_clk_en = 1;
		dsc_drv->c1_clk_en = 1;
		dsc_drv->c0_clk_en = 1;
		break;

	case DSC_RGB_3840X2160_60HZ:
	case DSC_YUV444_3840X2160_60HZ:
	case DSC_YUV420_3840X2160_60HZ:
	case DSC_RGB_3840X2160_50HZ:
	case DSC_YUV444_3840X2160_50HZ:
	case DSC_YUV420_3840X2160_50HZ:
		/* for 2 slice(slice width = 1920) */
		dsc_drv->c3_clk_en = 0;
		dsc_drv->c2_clk_en = 0;
		dsc_drv->c1_clk_en = 1;
		dsc_drv->c0_clk_en = 1;
		break;
	case DSC_YUV422_3840X2160_100HZ:
	case DSC_YUV420_3840X2160_100HZ:
	case DSC_YUV422_3840X2160_120HZ:
	case DSC_YUV420_3840X2160_120HZ:
	case DSC_YUV422_3840X2160_60HZ:
	case DSC_YUV422_3840X2160_50HZ:
		/* for 2 slice(slice width = 1920) */
		dsc_drv->c3_clk_en = 0;
		dsc_drv->c2_clk_en = 0;
		dsc_drv->c1_clk_en = 1;
		dsc_drv->c0_clk_en = 1;
		break;
	/* 8k60hz */
	case DSC_RGB_7680X4320_60HZ:
	case DSC_YUV444_7680X4320_60HZ:
	case DSC_YUV422_7680X4320_60HZ:
	case DSC_YUV420_7680X4320_60HZ:
	/* 8k50hz */
	case DSC_RGB_7680X4320_50HZ:
	case DSC_YUV444_7680X4320_50HZ:
	case DSC_YUV422_7680X4320_50HZ:
	case DSC_YUV420_7680X4320_50HZ:
	/* 8K30Hz */
	case DSC_RGB_7680X4320_30HZ:
	case DSC_YUV444_7680X4320_30HZ:
	case DSC_YUV422_7680X4320_30HZ:
	case DSC_YUV420_7680X4320_30HZ:
	/* 8K25Hz */
	case DSC_RGB_7680X4320_25HZ:
	case DSC_YUV444_7680X4320_25HZ:
	case DSC_YUV422_7680X4320_25HZ:
	case DSC_YUV420_7680X4320_25HZ:
	/* 8K24Hz */
	case DSC_RGB_7680X4320_24HZ:
	case DSC_YUV444_7680X4320_24HZ:
	case DSC_YUV422_7680X4320_24HZ:
	case DSC_YUV420_7680X4320_24HZ:
		dsc_drv->c3_clk_en = 1;
		dsc_drv->c2_clk_en = 1;
		dsc_drv->c1_clk_en = 1;
		dsc_drv->c0_clk_en = 1;
		break;
	default:
		DSC_ERR("dsc_mode(%d) not config clk_en\n", dsc_drv->dsc_mode);
		break;
	}
}

static void calculate_asic_data(struct aml_dsc_drv_s *dsc_drv)
{
	int slice_num;

	slice_num = dsc_drv->pps_data.pic_width / dsc_drv->pps_data.slice_width;
	dsc_drv->slice_num_m1 = slice_num - 1;
	dsc_drv->dsc_enc_frm_latch_en		= 0;
	dsc_drv->pix_per_clk			= 2;
	dsc_drv->inbuf_rd_dly0			= 8;
	dsc_drv->inbuf_rd_dly1			= 44;
	dsc_enc_confirm_clk_en(dsc_drv);
	if (slice_num == 8)
		dsc_drv->slices_in_core = 1;
	else
		dsc_drv->slices_in_core = 0;

	if (dsc_drv->pps_data.convert_rgb ||
	    (!dsc_drv->pps_data.native_422 && !dsc_drv->pps_data.native_420))
		dsc_drv->slice_group_number =
			dsc_drv->pps_data.slice_width / 3 * dsc_drv->pps_data.slice_height;
	else if (dsc_drv->pps_data.native_422 || dsc_drv->pps_data.native_420)
		dsc_drv->slice_group_number =
			dsc_drv->pps_data.slice_width / 6 * dsc_drv->pps_data.slice_height;

	dsc_drv->partial_group_pix_num		= 3;
	dsc_drv->chunk_6byte_num_m1 = DIV_ROUND_UP(dsc_drv->pps_data.chunk_size, 6) - 1;

	dsc_config_timing_value(dsc_drv);

	dsc_drv->pix_in_swap0			= 0x67534201;
	dsc_drv->pix_in_swap1			= 0xb9a8;
	dsc_drv->last_slice_active_m1		= dsc_drv->pps_data.slice_width - 1;
	dsc_drv->gclk_ctrl			= 0;
	dsc_drv->c0s1_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c0s0_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c1s1_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c1s0_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c2s1_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c2s0_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c3s1_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->c3s0_cb_ovfl_th		= 8 / slice_num * 350;
	dsc_drv->out_swap			= 0x543210;
	dsc_drv->intr_maskn			= 0;

	dsc_drv->hs_odd_no_tail			= 0;
	dsc_drv->hs_odd_no_head			= 0;
	dsc_drv->hs_even_no_tail		= 0;
	dsc_drv->hs_even_no_head		= 0;
	dsc_drv->vs_odd_no_tail			= 0;
	dsc_drv->vs_odd_no_head			= 0;
	dsc_drv->vs_even_no_tail		= 0;
	dsc_drv->vs_even_no_head		= 0;
	dsc_drv->hc_active_odd_mode		= 0;
	dsc_drv->hc_htotal_offs_oddline		= 0;
	dsc_drv->hc_htotal_offs_evenline		= 0;
}

inline enum dsc_encode_mode dsc_enc_confirm_mode(unsigned int pic_width, unsigned int pic_height,
	unsigned int fps, enum hdmi_colorspace color_format)
{
	enum dsc_encode_mode dsc_mode = DSC_ENCODE_MAX;
	struct aml_dsc_drv_s *dsc_drv = dsc_drv_get();

	if (!dsc_drv || !dsc_drv->data || dsc_drv->data->chip_type != DSC_CHIP_S5)
		return DSC_ENCODE_MAX;

	/* 4k60hz */
	if (pic_width == 3840 && pic_height == 2160 &&
		fps == 60000 && color_format == HDMI_COLORSPACE_RGB)
		dsc_mode = DSC_RGB_3840X2160_60HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 60000 && color_format == HDMI_COLORSPACE_YUV444)
		dsc_mode = DSC_YUV444_3840X2160_60HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 60000 && color_format == HDMI_COLORSPACE_YUV422)
		dsc_mode = DSC_YUV422_3840X2160_60HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 60000 && color_format == HDMI_COLORSPACE_YUV420)
		dsc_mode = DSC_YUV420_3840X2160_60HZ;
	/* 4k50hz */
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 50000 && color_format == HDMI_COLORSPACE_RGB)
		dsc_mode = DSC_RGB_3840X2160_50HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 50000 && color_format == HDMI_COLORSPACE_YUV444)
		dsc_mode = DSC_YUV444_3840X2160_50HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 50000 && color_format == HDMI_COLORSPACE_YUV422)
		dsc_mode = DSC_YUV422_3840X2160_50HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 50000 && color_format == HDMI_COLORSPACE_YUV420)
		dsc_mode = DSC_YUV420_3840X2160_50HZ;
	/* 4k120hz */
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 120000 && color_format == HDMI_COLORSPACE_RGB)
		dsc_mode = DSC_RGB_3840X2160_120HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		  fps == 120000 && color_format == HDMI_COLORSPACE_YUV444)
		dsc_mode = DSC_YUV444_3840X2160_120HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 120000 && color_format == HDMI_COLORSPACE_YUV422)
		dsc_mode = DSC_YUV422_3840X2160_120HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 120000 && color_format == HDMI_COLORSPACE_YUV420)
		dsc_mode = DSC_YUV420_3840X2160_120HZ;
	/* 4k100hz */
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 100000 && color_format == HDMI_COLORSPACE_RGB)
		dsc_mode = DSC_RGB_3840X2160_100HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		  fps == 100000 && color_format == HDMI_COLORSPACE_YUV444)
		dsc_mode = DSC_YUV444_3840X2160_100HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 100000 && color_format == HDMI_COLORSPACE_YUV422)
		dsc_mode = DSC_YUV422_3840X2160_100HZ;
	else if (pic_width == 3840 && pic_height == 2160 &&
		fps == 100000 && color_format == HDMI_COLORSPACE_YUV420)
		dsc_mode = DSC_YUV420_3840X2160_100HZ;
	/* 8K60Hz */
	else if (pic_width == 7680 && pic_height == 4320 &&
		fps == 60000 && color_format == HDMI_COLORSPACE_RGB)
		dsc_mode = DSC_RGB_7680X4320_60HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		fps == 60000 && color_format == HDMI_COLORSPACE_YUV444)
		dsc_mode = DSC_YUV444_7680X4320_60HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		fps == 60000 && color_format == HDMI_COLORSPACE_YUV422)
		dsc_mode = DSC_YUV422_7680X4320_60HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		fps == 60000 && color_format == HDMI_COLORSPACE_YUV420)
		dsc_mode = DSC_YUV420_7680X4320_60HZ;
	/* 8k50hz */
	else if (pic_width == 7680 && pic_height == 4320 &&
		fps == 50000 && color_format == HDMI_COLORSPACE_RGB)
		dsc_mode = DSC_RGB_7680X4320_50HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 50000 && color_format == HDMI_COLORSPACE_YUV444)
		dsc_mode = DSC_YUV444_7680X4320_50HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 50000 && color_format == HDMI_COLORSPACE_YUV422)
		dsc_mode = DSC_YUV422_7680X4320_50HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 50000 && color_format == HDMI_COLORSPACE_YUV420)
		dsc_mode = DSC_YUV420_7680X4320_50HZ;
	/* 8K24Hz */
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 24000 && color_format == HDMI_COLORSPACE_RGB)
		dsc_mode = DSC_RGB_7680X4320_24HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 24000 && color_format == HDMI_COLORSPACE_YUV444)
		dsc_mode = DSC_YUV444_7680X4320_24HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 24000 && color_format == HDMI_COLORSPACE_YUV422)
		dsc_mode = DSC_YUV422_7680X4320_24HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 24000 && color_format == HDMI_COLORSPACE_YUV420)
		dsc_mode = DSC_YUV420_7680X4320_24HZ;
	/* 8k25hz */
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 25000 && color_format == HDMI_COLORSPACE_RGB)
		dsc_mode = DSC_RGB_7680X4320_25HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 25000 && color_format == HDMI_COLORSPACE_YUV444)
		dsc_mode = DSC_YUV444_7680X4320_25HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 25000 && color_format == HDMI_COLORSPACE_YUV422)
		dsc_mode = DSC_YUV422_7680X4320_25HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 25000 && color_format == HDMI_COLORSPACE_YUV420)
		dsc_mode = DSC_YUV420_7680X4320_25HZ;
	/* 8k30hz */
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 30000 && color_format == HDMI_COLORSPACE_RGB)
		dsc_mode = DSC_RGB_7680X4320_30HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 30000 && color_format == HDMI_COLORSPACE_YUV444)
		dsc_mode = DSC_YUV444_7680X4320_30HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 30000 && color_format == HDMI_COLORSPACE_YUV422)
		dsc_mode = DSC_YUV422_7680X4320_30HZ;
	else if (pic_width == 7680 && pic_height == 4320 &&
		 fps == 30000 && color_format == HDMI_COLORSPACE_YUV420)
		dsc_mode = DSC_YUV420_7680X4320_30HZ;
	else
		dsc_mode = DSC_ENCODE_MAX;

	return dsc_mode;
}
EXPORT_SYMBOL(dsc_enc_confirm_mode);

static inline void dsc_enc_confirm_format_h_v_total(struct aml_dsc_drv_s *dsc_drv)
{
	switch (dsc_drv->dsc_mode) {
	case DSC_RGB_7680X4320_60HZ:
	case DSC_YUV444_7680X4320_60HZ:
	case DSC_YUV422_7680X4320_60HZ:
	case DSC_YUV420_7680X4320_60HZ:
	case DSC_RGB_7680X4320_30HZ:
	case DSC_YUV444_7680X4320_30HZ:
	case DSC_YUV422_7680X4320_30HZ:
	case DSC_YUV420_7680X4320_30HZ:
		dsc_drv->h_total = 9000;
		dsc_drv->v_total = 4400;
		break;
	case DSC_RGB_7680X4320_50HZ:
	case DSC_YUV444_7680X4320_50HZ:
	case DSC_YUV422_7680X4320_50HZ:
	case DSC_YUV420_7680X4320_50HZ:
	case DSC_RGB_7680X4320_25HZ:
	case DSC_YUV444_7680X4320_25HZ:
	case DSC_YUV422_7680X4320_25HZ:
	case DSC_YUV420_7680X4320_25HZ:
		dsc_drv->h_total = 10800;
		dsc_drv->v_total = 4400;
		break;
	case DSC_RGB_7680X4320_24HZ:
	case DSC_YUV444_7680X4320_24HZ:
	case DSC_YUV422_7680X4320_24HZ:
	case DSC_YUV420_7680X4320_24HZ:
		dsc_drv->h_total = 11000;
		dsc_drv->v_total = 4500;
		break;
	case DSC_RGB_3840X2160_60HZ:
	case DSC_YUV444_3840X2160_60HZ:
	case DSC_YUV422_3840X2160_60HZ:
	case DSC_YUV420_3840X2160_60HZ:
	case DSC_RGB_3840X2160_120HZ:
	case DSC_YUV444_3840X2160_120HZ:
	case DSC_YUV422_3840X2160_120HZ:
	case DSC_YUV420_3840X2160_120HZ:
		dsc_drv->h_total = 4400;
		dsc_drv->v_total = 2250;
		break;

	case DSC_RGB_3840X2160_50HZ:
	case DSC_YUV444_3840X2160_50HZ:
	case DSC_YUV422_3840X2160_50HZ:
	case DSC_YUV420_3840X2160_50HZ:
	case DSC_RGB_3840X2160_100HZ:
	case DSC_YUV444_3840X2160_100HZ:
	case DSC_YUV422_3840X2160_100HZ:
	case DSC_YUV420_3840X2160_100HZ:
		dsc_drv->h_total = 5280;
		dsc_drv->v_total = 2250;
		break;
	default:
		DSC_ERR("dsc_mode(%d) not get h or v total\n", dsc_drv->dsc_mode);
		break;
	}
}

static inline void dsc_enc_confirm_pps_color_format(struct aml_dsc_drv_s *dsc_drv)
{
	struct dsc_pps_data_s *pps_data = &dsc_drv->pps_data;

	switch (dsc_drv->color_format) {
	case HDMI_COLORSPACE_RGB:
		/* 1.Sources shall clear (=0) if the input to the
		 * VESA DSC 1.2a Encoder is YCbCr pixel data.
		 * 2.Sources shall set (=1) if the input to the
		 * VESA DSC 1.2a Encoder is RGB pixel data.
		 * note vpp should output RGB to DSC encoder if
		 * select RGB output
		 */
		pps_data->convert_rgb = 1;
		pps_data->simple_422 = 0;
		pps_data->native_422 = 0;
		pps_data->native_420 = 0;
		break;
	case HDMI_COLORSPACE_YUV444:
		pps_data->convert_rgb = 0;
		pps_data->simple_422 = 0;
		pps_data->native_422 = 0;
		pps_data->native_420 = 0;
		break;
	case HDMI_COLORSPACE_YUV422:
		pps_data->convert_rgb = 0;
		pps_data->simple_422 = 0;
		pps_data->native_422 = 1;
		pps_data->native_420 = 0;
		break;
	case HDMI_COLORSPACE_YUV420:
		pps_data->convert_rgb = 0;
		pps_data->simple_422 = 0;
		pps_data->native_422 = 0;
		pps_data->native_420 = 1;
		break;
	default:
		DSC_ERR("color_format is invalid(%d)\n", dsc_drv->color_format);
		break;
	}
}

/* note the slice with is derived from hdmi2.1 spec
 * Table7-29: Determination of slice_with, and then
 * the slice height is automatically from C-model
 */
static inline void dsc_enc_confirm_slice_value(struct aml_dsc_drv_s *dsc_drv)
{
	struct dsc_pps_data_s *pps_data = &dsc_drv->pps_data;

	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_SLICE_W_H) {
		DSC_PR("manual slice slice_width:%d slice_height:%d\n",
			pps_data->slice_width, pps_data->slice_height);
		return;
	}

	switch (dsc_drv->dsc_mode) {
	case DSC_RGB_3840X2160_60HZ:
	case DSC_YUV444_3840X2160_60HZ:
	case DSC_RGB_3840X2160_50HZ:
	case DSC_YUV444_3840X2160_50HZ:
		/* per table7-29, the slice_num should be 2 and slice width = 1920
		 * now we force 4 slice in and 4 slice out to dsc encoder
		 */
		/* pps_data->slice_width = 960; */
		pps_data->slice_width = 1920;
		pps_data->slice_height = 2160;
		break;
	case DSC_YUV420_3840X2160_60HZ:
	case DSC_YUV420_3840X2160_50HZ:
		/* per table7-29 and C-model */
		pps_data->slice_width = 1920;
		pps_data->slice_height = 1080;
		break;

	case DSC_YUV422_3840X2160_60HZ:
	case DSC_YUV422_3840X2160_50HZ:
		/* per table7-29 and C-model */
		pps_data->slice_width = 1920;
		pps_data->slice_height = 2160;
		break;

	case DSC_RGB_3840X2160_120HZ:
	case DSC_YUV444_3840X2160_120HZ:
	case DSC_RGB_3840X2160_100HZ:
	case DSC_YUV444_3840X2160_100HZ:
		/* per table7-29 and C-model */
		pps_data->slice_width = 960;
		pps_data->slice_height = 2160;
		break;
	case DSC_YUV422_3840X2160_120HZ:
	case DSC_YUV422_3840X2160_100HZ:
	case DSC_YUV420_3840X2160_120HZ:
	case DSC_YUV420_3840X2160_100HZ:
		/* per table7-29 and C-model */
		pps_data->slice_width = 1920;
		pps_data->slice_height = 2160;
		break;

	/* 8k60hz */
	case DSC_YUV422_7680X4320_60HZ:
	case DSC_YUV420_7680X4320_60HZ:
		pps_data->slice_width = 1920;
		pps_data->slice_height = 2160;
		break;
	case DSC_RGB_7680X4320_60HZ:
	case DSC_YUV444_7680X4320_60HZ:
		pps_data->slice_width = 960;
		pps_data->slice_height = 2160;
		break;
	/* 8k50hz, similar as 8k60hz */
	case DSC_YUV422_7680X4320_50HZ:
	case DSC_YUV420_7680X4320_50HZ:
		pps_data->slice_width = 1920;
		pps_data->slice_height = 2160;
		break;
	case DSC_RGB_7680X4320_50HZ:
	case DSC_YUV444_7680X4320_50HZ:
		pps_data->slice_width = 960;
		pps_data->slice_height = 2160;
		break;
	/* 8k30hz */
	case DSC_YUV422_7680X4320_30HZ: /* bpp = 7.375 */
	case DSC_YUV444_7680X4320_30HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_30HZ: /* bpp = 12 */
	case DSC_YUV422_7680X4320_25HZ: /* bpp = 7.6875 */
	case DSC_YUV444_7680X4320_25HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_25HZ: /* bpp = 12 */
	case DSC_YUV422_7680X4320_24HZ: /* bpp = 7.6875 */
	case DSC_YUV444_7680X4320_24HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_24HZ: /* bpp = 12 */
		pps_data->slice_width = 1920;
		pps_data->slice_height = 2160;
		break;
	/* case DSC_YUV422_7680X4320_30HZ:*/ /* force bpp = 12, todo removed */
	case DSC_YUV420_7680X4320_30HZ: /* bpp = 7.375 */
	case DSC_YUV420_7680X4320_25HZ: /* bpp = 7.6875 */
	case DSC_YUV420_7680X4320_24HZ: /* bpp = 7.6875 */
		pps_data->slice_width = 1920;
		pps_data->slice_height = 4320;
		break;
	/* case DSC_YUV420_7680X4320_30HZ: */
		/* force bpp = 12, tested pass, todo removed */
		/* pps_data->slice_width = 1920; */
		/* pps_data->slice_height = 1440; */
		/* break; */
	default:
		DSC_ERR("dsc_mode(%d) not get slice width/height\n", dsc_drv->dsc_mode);
		break;
	}

	DSC_PR("%s slice w(%d) & h(%d)\n", __func__, pps_data->slice_width, pps_data->slice_height);
}

/* sync with dsc_enc_confirm_slice_value() */
static u32 dsc_get_slice_width(enum dsc_encode_mode dsc_mode)
{
	u32 slice_width = 0;

	switch (dsc_mode) {
	case DSC_RGB_3840X2160_60HZ:
	case DSC_YUV444_3840X2160_60HZ:
	case DSC_RGB_3840X2160_50HZ:
	case DSC_YUV444_3840X2160_50HZ:
		/* force mode, not spec recommended */
		/* slice_width = 960; */
		slice_width = 1920;
		break;
	case DSC_YUV420_3840X2160_60HZ:
	case DSC_YUV420_3840X2160_50HZ:
	case DSC_YUV422_3840X2160_60HZ:
	case DSC_YUV422_3840X2160_50HZ:
		slice_width = 1920;
		break;

	case DSC_RGB_3840X2160_120HZ:
	case DSC_YUV444_3840X2160_120HZ:
	case DSC_RGB_3840X2160_100HZ:
	case DSC_YUV444_3840X2160_100HZ:
		slice_width = 960;
		break;
	case DSC_YUV420_3840X2160_120HZ:
	case DSC_YUV420_3840X2160_100HZ:
	case DSC_YUV422_3840X2160_120HZ:
	case DSC_YUV422_3840X2160_100HZ:
		slice_width = 1920;
		break;

	case DSC_RGB_7680X4320_60HZ:
	case DSC_YUV444_7680X4320_60HZ:
	case DSC_RGB_7680X4320_50HZ:
	case DSC_YUV444_7680X4320_50HZ:
		slice_width = 960;
		break;

	case DSC_YUV422_7680X4320_60HZ:
	case DSC_YUV420_7680X4320_60HZ:
	case DSC_YUV422_7680X4320_50HZ:
	case DSC_YUV420_7680X4320_50HZ:

	case DSC_YUV422_7680X4320_30HZ: /* bpp = 7.375 */
	case DSC_YUV444_7680X4320_30HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_30HZ: /* bpp = 12 */
	case DSC_YUV422_7680X4320_25HZ: /* bpp = 7.6875 */
	case DSC_YUV444_7680X4320_25HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_25HZ: /* bpp = 12 */
	case DSC_YUV422_7680X4320_24HZ: /* bpp = 7.6875 */
	case DSC_YUV444_7680X4320_24HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_24HZ: /* bpp = 12 */

	case DSC_YUV420_7680X4320_30HZ: /* bpp = 7.375 */
	case DSC_YUV420_7680X4320_25HZ: /* bpp = 7.6875 */
	case DSC_YUV420_7680X4320_24HZ: /* bpp = 7.6875 */
		slice_width = 1920;
		break;

	case DSC_ENCODE_MAX:
	default:
		slice_width = 0;
		break;
	}
	return slice_width;
}

/* use slice_with from dsc_get_slice_width() */
u8 dsc_get_slice_num(enum dsc_encode_mode dsc_mode)
{
	u8 slice_num = 0;
	struct aml_dsc_drv_s *dsc_drv = dsc_drv_get();
	/* dsc_drv->pps_data.pic_width / dsc_drv->pps_data.slice_width; */

	/* default 1 */
	if (!dsc_drv || !dsc_drv->data || dsc_drv->data->chip_type != DSC_CHIP_S5)
		return 1;

	switch (dsc_mode) {
	case DSC_RGB_3840X2160_60HZ:
	case DSC_YUV444_3840X2160_60HZ:
	case DSC_RGB_3840X2160_50HZ:
	case DSC_YUV444_3840X2160_50HZ:
		/* force 960 */
		/* slice_num = 3840 / 960; */
		slice_num = 3840 / 1920;
		break;
	case DSC_YUV420_3840X2160_60HZ:
	case DSC_YUV420_3840X2160_50HZ:
	case DSC_YUV422_3840X2160_60HZ:
	case DSC_YUV422_3840X2160_50HZ:
		slice_num = 3840 / 1920;
		break;

	case DSC_RGB_3840X2160_120HZ:
	case DSC_YUV444_3840X2160_120HZ:
	case DSC_RGB_3840X2160_100HZ:
	case DSC_YUV444_3840X2160_100HZ:
		slice_num = 3840 / 960;
		break;
	case DSC_YUV420_3840X2160_120HZ:
	case DSC_YUV420_3840X2160_100HZ:
	case DSC_YUV422_3840X2160_120HZ:
	case DSC_YUV422_3840X2160_100HZ:
		slice_num = 3840 / 1920;
		break;

	case DSC_RGB_7680X4320_60HZ:
	case DSC_YUV444_7680X4320_60HZ:
	case DSC_RGB_7680X4320_50HZ:
	case DSC_YUV444_7680X4320_50HZ:
		slice_num = 7680 / 960;
		break;

	case DSC_YUV422_7680X4320_60HZ:
	case DSC_YUV420_7680X4320_60HZ:
	case DSC_YUV422_7680X4320_50HZ:
	case DSC_YUV420_7680X4320_50HZ:

	case DSC_YUV422_7680X4320_30HZ: /* bpp = 7.375 */
	case DSC_YUV444_7680X4320_30HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_30HZ: /* bpp = 12 */
	case DSC_YUV422_7680X4320_25HZ: /* bpp = 7.6875 */
	case DSC_YUV444_7680X4320_25HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_25HZ: /* bpp = 12 */
	case DSC_YUV422_7680X4320_24HZ: /* bpp = 7.6875 */
	case DSC_YUV444_7680X4320_24HZ: /* bpp = 12 */
	case DSC_RGB_7680X4320_24HZ: /* bpp = 12 */

	case DSC_YUV420_7680X4320_30HZ: /* bpp = 7.375 */
	case DSC_YUV420_7680X4320_25HZ: /* bpp = 7.6875 */
	case DSC_YUV420_7680X4320_24HZ: /* bpp = 7.6875 */
		slice_num = 7680 / 1920;
		break;

	case DSC_ENCODE_MAX:
	default:
		slice_num = 0;
		break;
	}
	return slice_num;
}
EXPORT_SYMBOL(dsc_get_slice_num);

/* these bpp are from 2.1 spec table 7-37/38 */
static u32 dsc_get_bpp_target(enum dsc_encode_mode dsc_mode)
{
	u8 i = 0;

	for (i = 0; dsc_timing[i][0] != DSC_ENCODE_MAX; i++) {
		if (dsc_timing[i][0] == dsc_mode)
			break;
	}
	if (dsc_timing[i][0] == DSC_ENCODE_MAX) {
		DSC_PR("don't find bpp_target for dsc_mode :%d\n", dsc_mode);
		return 0;
	}

	return dsc_timing[i][1];
}

/* 2.1 spec table 6-56: bytes_target = slice_num * CEILING(bpp_target * slice_width / 8); */
static u32 dsc_get_bytes_target(u32 bpp_target, u8 slice_num, u32 slice_width)
{
	u32 bytes_target = 0;
	u32 bytes_per_slice = 0;
	struct aml_dsc_drv_s *dsc_drv = dsc_drv_get();

	if (!dsc_drv || !dsc_drv->data || dsc_drv->data->chip_type != DSC_CHIP_S5)
		return 0;

	if (bpp_target == 0 || slice_num == 0 || slice_width == 0) {
		DSC_ERR("don't find bytes_target, bpp: %d, slice_num: %d, slice_width: %d\n",
			bpp_target, slice_num, slice_width);
		return 0;
	}

	/* bpp_target is multiplied by BPP_FRACTION previously, below is for CEILING */
	if ((bpp_target * slice_width) % (8 * BPP_FRACTION) == 0)
		bytes_per_slice = (bpp_target * slice_width) / (8 * BPP_FRACTION);
	else
		bytes_per_slice = (bpp_target * slice_width) / (8 * BPP_FRACTION) + 1;

	bytes_target = slice_num * bytes_per_slice;

	return bytes_target;
}

u32 dsc_get_bytes_target_by_mode(enum dsc_encode_mode dsc_mode)
{
	u32 bpp_target = dsc_get_bpp_target(dsc_mode);
	u8 slice_num = dsc_get_slice_num(dsc_mode);
	u32 slice_width = dsc_get_slice_width(dsc_mode);

	return dsc_get_bytes_target(bpp_target, slice_num, slice_width);
}
EXPORT_SYMBOL(dsc_get_bytes_target_by_mode);

u32 dsc_get_hc_active_by_mode(enum dsc_encode_mode dsc_mode)
{
	u8 i = 0;

	for (i = 0; dsc_timing[i][0] != DSC_ENCODE_MAX; i++) {
		if (dsc_timing[i][0] == dsc_mode)
			break;
	}
	if (dsc_timing[i][0] == DSC_ENCODE_MAX) {
		DSC_ERR("don't find bpp_target for dsc_mode :%d\n", dsc_mode);
		return 0;
	}

	return dsc_timing[i][13];
}
EXPORT_SYMBOL(dsc_get_hc_active_by_mode);

/* hdmi2.1 7.7.3.4(7-37) bits_per_pixel = 444: bpp * 16, 422/420: bpp * 32 */
/* note: should double bpp for BITS_PER_PIXEL used in cfg file of C-model */
static inline void dsc_enc_confirm_bpp_value(struct aml_dsc_drv_s *dsc_drv)
{
	struct dsc_pps_data_s *pps_data = &dsc_drv->pps_data;

	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_BITS_PER_PIXEL) {
		DSC_PR("manual bits_per_pixel:%d\n", pps_data->bits_per_pixel);
		return;
	}

	switch (dsc_drv->dsc_mode) {
	case DSC_RGB_3840X2160_60HZ:
	case DSC_YUV444_3840X2160_60HZ:
	case DSC_RGB_3840X2160_120HZ:
	case DSC_YUV444_3840X2160_120HZ:
	case DSC_RGB_3840X2160_50HZ:
	case DSC_YUV444_3840X2160_50HZ:
	case DSC_RGB_3840X2160_100HZ:
	case DSC_YUV444_3840X2160_100HZ:
		/* hdmi2.1 table 7-37 bpp = 12, 12 * 16 = 192 */
		pps_data->bits_per_pixel = 192;
		break;
	case DSC_YUV422_3840X2160_60HZ:
	case DSC_YUV420_3840X2160_60HZ:
	case DSC_YUV422_3840X2160_50HZ:
	case DSC_YUV420_3840X2160_50HZ:
		/* hdmi2.1 table 7-37/38 bpp = 12, 12 * 16 * 2 = 384*/
		pps_data->bits_per_pixel = 384;
		break;
	case DSC_YUV422_3840X2160_120HZ:
	case DSC_YUV420_3840X2160_120HZ:
		/* hdmi2.1 table 7-37/38 7.0625 * 32 = 226 */
		pps_data->bits_per_pixel = 226;

		/* note: test case: bpp = 12, 12 * 32 = 384, todo removed */
		/* pps_data->bits_per_pixel = 384; */
		break;
	case DSC_YUV422_3840X2160_100HZ:
	case DSC_YUV420_3840X2160_100HZ:
		/* hdmi2.1 table 7-37/38 8.5625 * 32 = 274 */
		pps_data->bits_per_pixel = 274;
		break;

	case DSC_RGB_7680X4320_60HZ:
	case DSC_YUV444_7680X4320_60HZ:
		/* hdmi2.1 table 7-37 bpp = 9.9375,  9.9375 * 16 = 159 */
		pps_data->bits_per_pixel = 159;
		break;
	case DSC_YUV422_7680X4320_60HZ:
	case DSC_YUV420_7680X4320_60HZ:
		/* hdmi2.1 table 7-37/38 bpp = 7.4375, 7.4375 * 16 * 2 =238 */
		pps_data->bits_per_pixel = 238;
		break;
	case DSC_RGB_7680X4320_30HZ:
	case DSC_YUV444_7680X4320_30HZ:
	case DSC_RGB_7680X4320_25HZ:
	case DSC_YUV444_7680X4320_25HZ:
	case DSC_RGB_7680X4320_24HZ:
	case DSC_YUV444_7680X4320_24HZ:
		/* hdmi2.1 table 7-37 bpp = 12bp, 12 * 16 = 192 */
		pps_data->bits_per_pixel = 192;
		break;
	case DSC_YUV422_7680X4320_30HZ:
	case DSC_YUV420_7680X4320_30HZ:
		/* note: test case force bpp = 12, 12 * 32 = 384. tested pass, todo removed */
		/* pps_data->bits_per_pixel = 384; */
		/* hdmi2.1 table 7-37/38 bpp = 7.375, 7.375 * 16 * 2 = 236 */
		pps_data->bits_per_pixel = 236;
		break;
	case DSC_RGB_7680X4320_50HZ:
	case DSC_YUV444_7680X4320_50HZ:
		/* hdmi2.1 table 7-37 bpp = 9.8125, 9.8125 * 16 = 157 */
		pps_data->bits_per_pixel = 157;
		break;
	case DSC_YUV422_7680X4320_50HZ:
	case DSC_YUV420_7680X4320_50HZ:
	case DSC_YUV422_7680X4320_25HZ:
	case DSC_YUV420_7680X4320_25HZ:
	case DSC_YUV422_7680X4320_24HZ:
	case DSC_YUV420_7680X4320_24HZ:
		/* hdmi2.1 table 7-37/38 bpp = 7.6875, 7.6875 * 16 * 2 = 246  */
		pps_data->bits_per_pixel = 246;
		break;
	default:
		DSC_ERR("dsc_mode(%d) not get bpp\n", dsc_drv->dsc_mode);
		break;
	}
}

//1. cts_hdmi_tx_fe_clk > cts_hdmi_tx_pixel_clk
//2. enc0_clk = cts_hdmi_tx_fe_clk = htotal*vtotal*fps/4 (4 is vpp_slice_num)
//left side means the time to process one line of slice(4 is 4 slice) before compression,
//right side means the time to process one line of chunk after compression.
//3. 1/cts_hdmi_tx_fe_clk*htotal/4 = 1/cts_hdmi_tx_pixel_clk*hc_htotal (if 420 hc_htotal/2)
//4. hc_htotal:(pic_width*bpp+47)/48 + hc_blank/2
//hdmitx ip receive dsc is 48bit,hdmitx protocol is 24bit, so need divided by 48 and 2
// enc0_clk = cts_hdmi_tx_fe_clk
// cts_hdmi_tx_pnx_clk = cts_hdmi_tx_pixel_clk=cts_htx_tmds_clk=fpll_pixel_clk
static inline void calculate_dsc_enc_need_clk(struct aml_dsc_drv_s *dsc_drv)
{
	unsigned int fps = dsc_drv->fps;
	unsigned int hc_htotal = dsc_drv->hc_htotal_m1 + 1;

	if (dsc_drv->dsc_mode >= DSC_ENCODE_MAX) {
		DSC_ERR("%s:dsc mode error(%d)\n", __func__, dsc_drv->dsc_mode);
		return;
	}

	//cts_hdmi_tx_fe_clk = ecn0_clk
	//cts_hdmi_tx_fe_clk = htotal*vtotal*fps/1000/4 (1000 is fps*1000, 4 is vpp_slice_num)
	dsc_drv->enc0_clk = dsc_drv->h_total * dsc_drv->v_total / 1000 * fps / 4;
	//4 means 4 slice, multiply by 4 means the total clk before cut to slice
	//cts_hdmi_tx_pixel_clk = cts_hdmi_tx_fe_clk*4*hc_htotal/htotal
	dsc_drv->cts_hdmi_tx_pixel_clk = dsc_drv->enc0_clk * 4 / dsc_drv->h_total * hc_htotal;
	/* for LG TV, need to set slice_width = 1920, and double enc0/fe_clk under 4k50/60hz y444 */
	if (dsc_drv->dsc_mode == DSC_YUV444_3840X2160_60HZ ||
		dsc_drv->dsc_mode == DSC_RGB_3840X2160_60HZ ||
		dsc_drv->dsc_mode == DSC_YUV444_3840X2160_50HZ ||
		dsc_drv->dsc_mode == DSC_RGB_3840X2160_50HZ)
		dsc_drv->enc0_clk = dsc_drv->enc0_clk * 2;
}

void init_pps_data(struct aml_dsc_drv_s *dsc_drv)
{
	struct dsc_pps_data_s *pps_data = &dsc_drv->pps_data;

	dsc_drv->pps_data.dsc_version_major = 1;
	dsc_drv->pps_data.dsc_version_minor = 2;

	//confirm dsc_mode
	dsc_drv->dsc_mode = dsc_enc_confirm_mode(dsc_drv->pps_data.pic_width,
		dsc_drv->pps_data.pic_height, dsc_drv->fps, dsc_drv->color_format);

	//get format htotal and vtotal
	dsc_enc_confirm_format_h_v_total(dsc_drv);

	//dsc confirm dsc pps color format
	dsc_enc_confirm_pps_color_format(dsc_drv);

	//set bits_per_pixel
	dsc_enc_confirm_bpp_value(dsc_drv);

	//set slice_width slice_height
	dsc_enc_confirm_slice_value(dsc_drv);

	//confirm bits_per_component
	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_BITS_PER_COMPONENT)
		DSC_PR("manual bits_per_component:%d\n", pps_data->bits_per_component);
	else if (pps_data->bits_per_component == 0)
		pps_data->bits_per_component = 8;

	//set line_buf_depth
	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_LINE_BUF_DEPTH)
		DSC_PR("manual line_buf_depth:%d\n", pps_data->line_buf_depth);
	else
		pps_data->line_buf_depth = 13;

	//set block_pred_enable vbr_enable
	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_PREDICTION_MODE) {
		DSC_PR("manual block_pred_enable:%d vbr_enable:%d\n",
			pps_data->block_pred_enable, pps_data->vbr_enable);
	} else {
		pps_data->block_pred_enable = 1;
		pps_data->vbr_enable = 0;
	}

	dsc_drv->full_ich_err_precision = 1;
	//set rc_model_size rc_edge_factor rc_tgt_offset
	if (dsc_drv->dsc_debug.manual_set_select & MANUAL_SOME_RC_PARAMETER) {
		DSC_PR("manual rc_model_size:%d rc_edge_factor:%d lo:%d hi:%d\n",
			pps_data->rc_parameter_set.rc_model_size,
			pps_data->rc_parameter_set.rc_edge_factor,
			pps_data->rc_parameter_set.rc_tgt_offset_lo,
			pps_data->rc_parameter_set.rc_tgt_offset_hi);
	} else {
		/* where's the defination of default value? */
		pps_data->rc_parameter_set.rc_model_size = 8192;
		pps_data->rc_parameter_set.rc_tgt_offset_hi = 3;
		pps_data->rc_parameter_set.rc_tgt_offset_lo = 3;
		pps_data->rc_parameter_set.rc_edge_factor = 6;
	}

	dsc_drv->bits_per_pixel_int = dsc_drv->pps_data.bits_per_pixel / 16;
	dsc_drv->bits_per_pixel_remain = (dsc_drv->pps_data.bits_per_pixel -
					16 * dsc_drv->bits_per_pixel_int) * BPP_FRACTION / 16;
	dsc_drv->bits_per_pixel_multiple = dsc_drv->bits_per_pixel_int * BPP_FRACTION +
					dsc_drv->bits_per_pixel_remain;

	if (dsc_drv->color_format == HDMI_COLORSPACE_RGB ||
	    dsc_drv->color_format == HDMI_COLORSPACE_YUV444)
		dsc_drv->bits_per_pixel_really_value = dsc_drv->bits_per_pixel_multiple;
	else if (dsc_drv->color_format == HDMI_COLORSPACE_YUV422 ||
		 dsc_drv->color_format == HDMI_COLORSPACE_YUV420)
		dsc_drv->bits_per_pixel_really_value = dsc_drv->bits_per_pixel_multiple / 2;
	else
		DSC_ERR("color_format is error:%d\n", dsc_drv->color_format);
}

void calculate_dsc_enc_data(struct aml_dsc_drv_s *dsc_drv)
{
	if (!dsc_drv) {
		DSC_ERR("dsc_drv is NULL\n");
		return;
	}
	init_pps_data(dsc_drv);
	populate_pps(dsc_drv);
	calculate_asic_data(dsc_drv);
	calculate_dsc_enc_need_clk(dsc_drv);
	/* no need? */
	compute_offset(dsc_drv, 0, 0, 0);
}

