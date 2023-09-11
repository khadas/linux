// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/amvecm/hdr2_ext.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/highmem.h>
#include <linux/page-flags.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/sched/clock.h>

#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "vdin_ctl.h"
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"
#include "vdin_afbce.h"
#include "vdin_dv.h"

#define VDIN_V_SHRINK_H_LIMIT 1280
#define TVIN_MAX_PIX_CLK 20000
#define META_RETRY_MAX 10
#define VDIN_MAX_H_ACTIVE 4096	/*the max h active of vdin*/
/*0: 1 word in 1burst, 1: 2 words in 1burst;
 *2: 4 words in 1burst;
 */
#define VDIN_WR_BURST_MODE 2

static bool rgb_info_enable;
static unsigned int rgb_info_x;
static unsigned int rgb_info_y;
static unsigned int rgb_info_r;
static unsigned int rgb_info_g;
static unsigned int rgb_info_b;
static int vdin_det_idle_wait = 100;
static unsigned int delay_line_num;
static bool invert_top_bot;
//static unsigned int vdin0_afbce_debug_force;
static unsigned int vdin_luma_max;

static unsigned int vpu_reg_27af = 0x3;

//tmp
static bool cm_enable = 1;

/***************************Local defines**********************************/
#define BBAR_BLOCK_THR_FACTOR           3
#define BBAR_LINE_THR_FACTOR            7

#define VDIN_MUX_NULL                   0
#define VDIN_MUX_MPEG                   1
#define VDIN_MUX_656                    2
#define VDIN_MUX_TVFE                   3
#define VDIN_MUX_CVD2                   4
#define VDIN_MUX_HDMI                   5
#define VDIN_MUX_DVIN                   6
#define VDIN_MUX_VIU1_WB0		7
#define VDIN_MUX_MIPI                   8
#define VDIN_MUX_ISP			9
/* after g12a new add*/
#define VDIN_MUX_VIU1_WB1		9
#define VDIN_MUX_656_B                  10

#define VDIN_MAP_Y_G                    0
#define VDIN_MAP_BPB                    1
#define VDIN_MAP_RCR                    2

#define MEAS_MUX_NULL                   0
#define MEAS_MUX_656                    1
#define MEAS_MUX_TVFE                   2
#define MEAS_MUX_CVD2                   3
#define MEAS_MUX_HDMI                   4
#define MEAS_MUX_DVIN                   5
#define MEAS_MUX_DTV                    6
#define MEAS_MUX_ISP                    8
#define MEAS_MUX_656_B                  9
#define MEAS_MUX_VIU1                   6
#define MEAS_MUX_VIU2                   8

#define HDMI_DE_REPEAT_DONE_FLAG	0xF0
#define DECIMATION_REAL_RANGE		0x0F
#define VDIN_PIXEL_CLK_4K_30HZ		248832000
#define VDIN_PIXEL_CLK_4K_60HZ		497664000

/*reset reg mif value of vdin0:
 *	VDIN_WR_CTRL \VDIN_COM_CTRL0\ VDIN_MISC_CTRL
 */
static void vdin0_wr_mif_reset_s5(void)
{
	if (vsync_reset_mask & 0x08) {
		W_VCBUS_BIT(VDIN_WR_CTRL, 1, FRAME_SOFT_RST_EN_BIT, FRAME_SOFT_RST_EN_WID);
		W_VCBUS_BIT(VDIN_COM_CTRL0, 1, VDIN_FORCEGOLINE_EN_BIT, 1);
		udelay(1);
		W_VCBUS_BIT(VDIN_WR_CTRL, 0, FRAME_SOFT_RST_EN_BIT, FRAME_SOFT_RST_EN_WID);
	} else {
		W_VCBUS_BIT(VDIN_WR_CTRL, 0,
			VDIN_WR_CTRL_REG_PAUSE_BIT, VDIN_WR_CTRL_REG_PAUSE_WID);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 1, 2, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_MIF_RST_BIT, 1);
		udelay(1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN0_MIF_RST_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 0, 2, 1);
		W_VCBUS_BIT(VDIN_WR_CTRL, 1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		W_VCBUS_BIT(VDIN_WR_CTRL, 1,
			VDIN_WR_CTRL_REG_PAUSE_BIT, VDIN_WR_CTRL_REG_PAUSE_WID);
	}
};

/*reset reg mif value of vdin1:
 *	VDIN_WR_CTRL \VDIN_COM_CTRL1\ VDIN_MISC_CTRL
 */
static void vdin1_wr_mif_reset_s5(void)
{
	if (vsync_reset_mask & 0x08) {
		W_VCBUS_BIT(VDIN1_WR_CTRL, 1, FRAME_SOFT_RST_EN_BIT, 1);
		W_VCBUS_BIT(VDIN_COM_CTRL1, 1, VDIN_FORCEGOLINE_EN_BIT, 1);
		udelay(1);
		W_VCBUS_BIT(VDIN1_WR_CTRL, 0, FRAME_SOFT_RST_EN_BIT, 1);
	} else {
		W_VCBUS_BIT(VDIN1_WR_CTRL, 0, VDIN_WR_CTRL_REG_PAUSE_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN1_MIF_RST_BIT, 1);
		udelay(1);
		W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN1_MIF_RST_BIT, 1);
		W_VCBUS_BIT(VDIN1_WR_CTRL, 1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		W_VCBUS_BIT(VDIN1_WR_CTRL, 1, VDIN_WR_CTRL_REG_PAUSE_BIT, 1);
	}
}

/***************************Local Structures**********************************/
static struct vdin_matrix_lup_s vdin_matrix_lup[] = {
	{0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x0400200, 0x00000200,},
	/* VDIN_MATRIX_RGB_YUV601 */
	/* 0     0.257  0.504  0.098     16 */
	/* 0    -0.148 -0.291  0.439    128 */
	/* 0     0.439 -0.368 -0.071    128 */
	{0x00000000, 0x00000000, 0x01070204, 0x00641f68, 0x1ed601c2, 0x01c21e87,
		0x00001fb7, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_GBR_YUV601 */
	/* 0	    0.504  0.098  0.257     16 */
	/* 0      -0.291  0.439 -0.148    128 */
	/* 0	   -0.368 -0.071  0.439    128 */
	{0x00000000, 0x00000000, 0x02040064, 0x01071ed6, 0x01c21f68, 0x1e871fb7,
		0x000001c2, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_BRG_YUV601 */
	/* 0	    0.098  0.257  0.504     16 */
	/* 0       0.439 -0.148 -0.291    128 */
	/* 0      -0.071  0.439 -0.368    128 */
	{0x00000000, 0x00000000, 0x00640107, 0x020401c2, 0x1f681ed6, 0x1fb701c2,
		0x00001e87, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_RGB */
	/* -16     1.164  0      1.596      0 */
	/* -128     1.164 -0.391 -0.813      0 */
	/* -128     1.164  2.018  0          0 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x066204a8, 0x1e701cbf, 0x04a80812,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV601_GBR */
	/* -16     1.164 -0.391 -0.813      0 */
	/* -128     1.164  2.018  0	     0 */
	/* -128     1.164  0	  1.596      0 */
	{0x07c00600, 0x00000600, 0x04a81e70, 0x1cbf04a8, 0x08120000, 0x04a80000,
		0x00000662, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV601_BRG */
	/* -16     1.164  2.018  0          0 */
	/* -128     1.164  0      1.596      0 */
	/* -128     1.164 -0.391 -0.813      0 */
	{0x07c00600, 0x00000600, 0x04a80812, 0x000004a8, 0x00000662, 0x04a81e70,
		0x00001cbf, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGB_YUV601F */
	/* 0     0.299  0.587  0.114      0 */
	/* 0    -0.169 -0.331  0.5      128 */
	/* 0     0.5   -0.419 -0.081    128 */
	{0x00000000, 0x00000000, 0x01320259, 0x00751f53, 0x1ead0200, 0x02001e53,
		0x00001fad, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_RGB */
	/* 0     1      0      1.402      0 */
	/* -128     1     -0.344 -0.714      0 */
	/* -128     1      1.772  0          0 */
	{0x00000600, 0x00000600, 0x04000000, 0x059c0400, 0x1ea01d25, 0x04000717,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGBS_YUV601 */
	/* -16     0.299  0.587  0.114     16 */
	/* -16    -0.173 -0.339  0.511    128 */
	/* -16     0.511 -0.429 -0.083    128 */
	{0x07c007c0, 0x000007c0, 0x01320259, 0x00751f4f, 0x1ea5020b, 0x020b1e49,
		0x00001fab, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_RGBS */
	/* -16     1      0      1.371     16 */
	/* -128     1     -0.336 -0.698     16 */
	/* -128     1      1.733  0         16 */
	{0x07c00600, 0x00000600, 0x04000000, 0x057c0400, 0x1ea81d35, 0x040006ef,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_RGBS_YUV601F */
	/* -16     0.348  0.683  0.133      0 */
	/* -16    -0.197 -0.385  0.582    128 */
	/* -16     0.582 -0.488 -0.094    128 */
	{0x07c007c0, 0x000007c0, 0x016402bb, 0x00881f36, 0x1e760254, 0x02541e0c,
		0x00001fa0, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_RGBS */
	/* 0     0.859  0      1.204     16 */
	/* -128     0.859 -0.295 -0.613     16 */
	/* -128     0.859  1.522  0         16 */
	{0x00000600, 0x00000600, 0x03700000, 0x04d10370, 0x1ed21d8c, 0x03700617,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_YUV601F_YUV601 */
	/* 0     0.859  0      0         16 */
	/* -128     0      0.878  0        128 */
	/* -128     0      0      0.878    128 */
	{0x00000600, 0x00000600, 0x03700000, 0x00000000, 0x03830000, 0x00000000,
		0x00000383, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_YUV601F */
	/* -16     1.164  0      0          0 */
	/* -128     0      1.138  0        128 */
	/* -128     0      0      1.138    128 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x00000000, 0x048d0000, 0x00000000,
		0x0000048d, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_RGB_YUV709 */
	/* 0     0.183  0.614  0.062     16 */
	/* 0    -0.101 -0.338  0.439    128 */
	/* 0     0.439 -0.399 -0.04     128 */
	{0x00000000, 0x00000000, 0x00bb0275, 0x003f1f99, 0x1ea601c2, 0x01c21e67,
		0x00001fd7, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_RGB */
	/* -16     1.164  0      1.793      0 */
	/* -128     1.164 -0.213 -0.534      0 */
	/* -128     1.164  2.115  0          0 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x072c04a8, 0x1f261ddd, 0x04a80876,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV709_GBR */
	/* -16	1.164 -0.213 -0.534	 0 */
	/* -128	1.164  2.115  0	 0 */
	/* -128	1.164  0      1.793	 0 */
	{0x07c00600, 0x00000600, 0x04a81f26, 0x1ddd04a8, 0x08760000, 0x04a80000,
		0x0000072c, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV709_BRG */
	/* -16	1.164  2.115  0	 0 */
	/* -128	1.164  0      1.793	 0 */
	/* -128	1.164 -0.213 -0.534	 0 */
	{0x07c00600, 0x00000600, 0x04a80876, 0x000004a8, 0x0000072c, 0x04a81f26,
		0x00001ddd, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGB_YUV709F */
	/* 0     0.213  0.715  0.072      0 */
	/* 0    -0.115 -0.385  0.5      128 */
	/* 0     0.5   -0.454 -0.046    128 */
	{0x00000000, 0x00000000, 0x00da02dc, 0x004a1f8a, 0x1e760200, 0x02001e2f,
		0x00001fd1, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_RGB */
	/* 0     1      0      1.575      0 */
	/* -128     1     -0.187 -0.468      0 */
	/* -128     1      1.856  0          0 */
	{0x00000600, 0x00000600, 0x04000000, 0x064d0400, 0x1f411e21, 0x0400076d,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGBS_YUV709 */
	/* -16     0.213  0.715  0.072     16 */
	/* -16    -0.118 -0.394  0.511    128 */
	/* -16     0.511 -0.464 -0.047    128 */
	{0x07c007c0, 0x000007c0, 0x00da02dc, 0x004a1f87, 0x1e6d020b, 0x020b1e25,
		0x00001fd0, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_RGBS */
	/* -16     1      0      1.54      16 */
	/* -128     1     -0.183 -0.459     16 */
	/* -128     1      1.816  0         16 */
	{0x07c00600, 0x00000600, 0x04000000, 0x06290400, 0x1f451e2a, 0x04000744,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_RGBS_YUV709F */
	/* -16     0.248  0.833  0.084      0 */
	/* -16    -0.134 -0.448  0.582    128 */
	/* -16     0.582 -0.529 -0.054    128 */
	{0x07c007c0, 0x000007c0, 0x00fe0355, 0x00561f77, 0x1e350254, 0x02541de2,
		0x00001fc9, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_RGBS */
	/* 0     0.859  0      1.353     16 */
	/* -128     0.859 -0.161 -0.402     16 */
	/* -128     0.859  1.594  0         16 */
	{0x00000600, 0x00000600, 0x03700000, 0x05690370, 0x1f5b1e64, 0x03700660,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_YUV709F_YUV709 */
	/* 0     0.859  0      0         16 */
	/* -128     0      0.878  0        128 */
	/* -128     0      0      0.878    128 */
	{0x00000600, 0x00000600, 0x03700000, 0x00000000, 0x03830000, 0x00000000,
		0x00000383, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_YUV709F */
	/* -16     1.164  0      0          0 */
	/* -128     0      1.138  0        128 */
	/* -128     0      0      1.138    128 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x00000000, 0x048d0000, 0x00000000,
		0x0000048d, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_YUV709 */
	/* -16     1     -0.115 -0.207     16 */
	/* -128     0      1.018  0.114    128 */
	/* -128     0      0.075  1.025    128 */
	{0x07c00600, 0x00000600, 0x04001f8a, 0x1f2c0000, 0x04120075, 0x0000004d,
		0x0000041a, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_YUV601 */
	/* -16     1      0.100  0.192     16 */
	/* -128     0      0.990 -0.110    128 */
	/* -128     0     -0.072  0.984    128 */
	{0x07c00600, 0x00000600, 0x04000066, 0x00c50000, 0x03f61f8f, 0x00001fb6,
		0x000003f0, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_YUV709F */
	/* -16     1.164 -0.134 -0.241      0 */
	/* -128     0      1.160  0.129    128 */
	/* -128     0      0.085  1.167    128 */
	{0x07c00600, 0x00000600, 0x04a81f77, 0x1f090000, 0x04a40084, 0x00000057,
		0x000004ab, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_YUV601 */
	/* 0     0.859  0.088  0.169     16 */
	/* -128     0      0.869 -0.097    128 */
	/* -128     0     -0.063  0.864    128 */
	{0x00000600, 0x00000600, 0x0370005a, 0x00ad0000, 0x037a1f9d, 0x00001fbf,
		0x00000375, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_YUV709 */
	/* 0     0.859 -0.101 -0.182     16 */
	/* -128     0      0.894  0.100    128 */
	/* -128     0      0.066  0.900    128 */
	{0x00000600, 0x00000600, 0x03701f99, 0x1f460000, 0x03930066, 0x00000044,
		0x0000039a, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_YUV601F */
	/* -16     1.164  0.116  0.223      0 */
	/* -128     0      1.128 -0.126    128 */
	/* -128     0     -0.082  1.120    128 */
	{0x07c00600, 0x00000600, 0x04a80077, 0x00e40000, 0x04831f7f, 0x00001fac,
		0x0000047b, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_YUV709F */
	/* 0     1     -0.118 -0.212     16 */
	/* -128     0      1.018  0.114    128 */
	/* -128     0      0.075  1.025    128 */
	{0x00000600, 0x00000600, 0x04001f87, 0x1f270000, 0x04120075, 0x0000004d,
		0x0000041a, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_YUV601F */
	/* 0     1      0.102  0.196      0 */
	/* -128     0      0.990 -0.111    128 */
	/* -128     0     -0.072  0.984    128 */
	{0x00000600, 0x00000600, 0x04000068, 0x00c90000, 0x03f61f8e, 0x00001fb6,
		0x000003f0, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_RGBS_RGB */
	/* -16     1.164  0      0          0 */
	/* -16     0      1.164  0          0 */
	/* -16     0      0      1.164      0 */
	{0x07c007c0, 0x000007c0, 0x04a80000, 0x00000000, 0x04a80000, 0x00000000,
		0x000004a8, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGB_RGBS */
	/* 0     0.859  0      0         16 */
	/* 0     0      0.859  0         16 */
	/* 0     0      0      0.859     16 */
	{0x00000000, 0x00000000, 0x03700000, 0x00000000, 0x03700000, 0x00000000,
		0x00000370, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_2020RGB_YUV2020 */
	/* 0	 0.224732	0.580008  0.050729	 16 */
	/* 0	-0.122176 -0.315324  0.437500	128 */
	/* 0	 0.437500 -0.402312 -0.035188	128 */
	{0x00000000, 0x00000000, 0x00e60252, 0x00341f84, 0x1ebe01c0, 0x01c01e65,
		0x00001fdd, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV2020F_YUV2020 */
	/* 0 0.859 0 0 16 */
	/* -128 0 0.878 0 128 */
	/* -128 0 0 0.878 128 */
	{0x00000600, 0x00000600, 0x03700000, 0x00000000, 0x03830000, 0x00000000,
		0x00000383, 0x00400200, 0x00000200,},
};

/***************************Local function**********************************/

/*set csc range idx
 * parameters:
 *	a.devp
 * return
 *	0: limit
 *	1: full
 */
u32 vdin_matrix_range_chk_s5(struct vdin_dev_s *devp)
{
	enum tvin_color_fmt_range_e fmt_range;
	enum vdin_format_convert_e format_convert;

	fmt_range = devp->prop.color_fmt_range;
	format_convert = devp->format_convert;
	switch (devp->csc_idx) {
	case VDIN_MATRIX_YUV601F_RGB:
	case VDIN_MATRIX_RGBS_YUV601F:
	case VDIN_MATRIX_YUV601_YUV601F:
	case VDIN_MATRIX_YUV709_RGB:
	case VDIN_MATRIX_YUV709_GBR:
	case VDIN_MATRIX_YUV709_BRG:
	case VDIN_MATRIX_RGB_YUV709F:
	case VDIN_MATRIX_YUV709F_RGB:
	case VDIN_MATRIX_RGBS_YUV709F:
	case VDIN_MATRIX_YUV709_YUV709F:
	case VDIN_MATRIX_YUV601_YUV709F:
	case VDIN_MATRIX_YUV601F_YUV709F:
	case VDIN_MATRIX_YUV709F_YUV601F:
	case VDIN_MATRIX_YUV709_YUV601F:
	case VDIN_MATRIX_RGBS_RGB:
		return 1;

	case VDIN_MATRIX_NULL:
		if (format_convert == VDIN_FORMAT_CONVERT_RGB_RGB)
			if (fmt_range == TVIN_FMT_RANGE_NULL ||
			    fmt_range == TVIN_RGB_FULL)
				return 1;
			else
				return 0;
		else
			if (fmt_range == TVIN_RGB_FULL ||
			    fmt_range == TVIN_YUV_FULL)
				return 1;
			else
				return 0;
	case VDIN_MATRIX_XXX_YUV601_BLACK:
	case VDIN_MATRIX_RGB_YUV601:
	case VDIN_MATRIX_GBR_YUV601:
	case VDIN_MATRIX_BRG_YUV601:
	case VDIN_MATRIX_RGBS_YUV601:
	case VDIN_MATRIX_YUV601_RGBS:
	case VDIN_MATRIX_YUV601F_RGBS:
	case VDIN_MATRIX_YUV601F_YUV601:
	case VDIN_MATRIX_RGB_YUV709:
	case VDIN_MATRIX_RGBS_YUV709:
	case VDIN_MATRIX_YUV709_RGBS:
	case VDIN_MATRIX_YUV709F_RGBS:
	case VDIN_MATRIX_YUV709F_YUV709:
	case VDIN_MATRIX_YUV601_YUV709:
	case VDIN_MATRIX_YUV709_YUV601:
	case VDIN_MATRIX_YUV709F_YUV601:
	case VDIN_MATRIX_YUV601F_YUV709:
	case VDIN_MATRIX_RGB_RGBS:
	case VDIN_MATRIX_RGB2020_YUV2020:
	default:
		return 0;
	}
}

/* set format_convert
 * base on parameters:
 *	a.color_format
 *	b.dest_cfmt
 */
void vdin_get_format_convert_s5(struct vdin_dev_s *devp)
{
	enum vdin_format_convert_e	format_convert;
	unsigned int port = devp->parm.port;
	unsigned int scan_mod = devp->fmt_info_p->scan_mode;
	unsigned int manual_md = (devp->flags & VDIN_FLAG_MANUAL_CONVERSION);

	if (IS_HDMI_SRC(port) && !manual_md)
		devp->prop.dest_cfmt = TVIN_COLOR_FMT_MAX;

#ifdef VDIN_BRINGUP_BYPASS_COLOR_CNVT
	devp->prop.dest_cfmt = devp->prop.color_format;
#endif

	if (devp->prop.color_format == devp->prop.dest_cfmt) {
		switch (devp->prop.color_format) {
		case TVIN_YUV422:
		case TVIN_YUYV422:
		case TVIN_YVYU422:
		case TVIN_UYVY422:
		case TVIN_VYUY422:
			format_convert = VDIN_FORMAT_CONVERT_YUV_YUV422;
			break;
		case TVIN_YUV444:
			format_convert = VDIN_FORMAT_CONVERT_YUV_YUV444;
			break;
		case TVIN_RGB444:
			format_convert = VDIN_FORMAT_CONVERT_RGB_RGB;
			break;
		default:
			format_convert = VDIN_FORMAT_CONVERT_MAX;
			break;
		}
	} else {
		switch (devp->prop.color_format) {
		case TVIN_YUV422:
		case TVIN_YUYV422:
		case TVIN_YVYU422:
		case TVIN_UYVY422:
		case TVIN_VYUY422:
			if (devp->prop.dest_cfmt == TVIN_NV21) {
				format_convert = VDIN_FORMAT_CONVERT_YUV_NV21;
			} else if (devp->prop.dest_cfmt == TVIN_NV12) {
				format_convert = VDIN_FORMAT_CONVERT_YUV_NV12;
			} else {
				if (manual_md &&
				    devp->prop.dest_cfmt == TVIN_RGB444)
					format_convert =
						VDIN_FORMAT_CONVERT_YUV_RGB;
				else if (manual_md &&
					 devp->prop.dest_cfmt == TVIN_YUV444)
					format_convert =
						VDIN_FORMAT_CONVERT_YUV_YUV444;
				else
					format_convert =
						VDIN_FORMAT_CONVERT_YUV_YUV422;
			}
			break;
		case TVIN_YUV444:
			if (IS_HDMI_SRC(port) &&
			    scan_mod == TVIN_SCAN_MODE_PROGRESSIVE && !manual_md) {
				if (devp->vdin_pc_mode)
					format_convert =
						VDIN_FORMAT_CONVERT_YUV_YUV444;
				else
					format_convert =
						VDIN_FORMAT_CONVERT_YUV_YUV422;
			} else if (devp->prop.dest_cfmt == TVIN_NV21) {
				format_convert = VDIN_FORMAT_CONVERT_YUV_NV21;
			} else if (devp->prop.dest_cfmt == TVIN_NV12) {
				format_convert = VDIN_FORMAT_CONVERT_YUV_NV12;
			} else {
				if (manual_md &&
				    devp->prop.dest_cfmt == TVIN_RGB444)
					format_convert =
						VDIN_FORMAT_CONVERT_YUV_RGB;
				else if (manual_md &&
					 devp->prop.dest_cfmt == TVIN_YUV422)
					format_convert =
						VDIN_FORMAT_CONVERT_YUV_YUV422;
				else if (manual_md &&
					 devp->prop.dest_cfmt == TVIN_YUV444)
					format_convert =
						VDIN_FORMAT_CONVERT_YUV_YUV444;
				else
					format_convert =
						VDIN_FORMAT_CONVERT_YUV_YUV422;
			}
			break;
		case TVIN_RGB444:
			if (IS_HDMI_SRC(port) &&
			    scan_mod == TVIN_SCAN_MODE_PROGRESSIVE && !manual_md) {
				if (devp->vdin_pc_mode)
					format_convert =
						VDIN_FORMAT_CONVERT_RGB_RGB;
				else
					format_convert =
						VDIN_FORMAT_CONVERT_RGB_YUV422;
			} else if (devp->prop.dest_cfmt == TVIN_NV21) {
				format_convert = VDIN_FORMAT_CONVERT_RGB_NV21;
			} else if (devp->prop.dest_cfmt == TVIN_NV12) {
				format_convert = VDIN_FORMAT_CONVERT_RGB_NV12;
			} else {
				if (manual_md &&
				    devp->prop.dest_cfmt == TVIN_RGB444)
					format_convert =
						VDIN_FORMAT_CONVERT_RGB_RGB;
				else if (manual_md &&
					 devp->prop.dest_cfmt == TVIN_YUV422)
					format_convert =
						VDIN_FORMAT_CONVERT_RGB_YUV422;
				else if (manual_md &&
					 devp->prop.dest_cfmt == TVIN_YUV444)
					format_convert =
						VDIN_FORMAT_CONVERT_RGB_YUV444;
				else
					format_convert =
						VDIN_FORMAT_CONVERT_RGB_YUV422;
			}
			break;
		default:
			format_convert = VDIN_FORMAT_CONVERT_MAX;
		break;
		}
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (vdin_is_dolby_signal_in(devp)) {
		/* Is dolby vision signal input */
		/*
		 *if (devp->dv.low_latency) {
		 *	if (devp->prop.color_format == TVIN_YUV422) {
		 *		format_convert = VDIN_FORMAT_CONVERT_YUV_YUV444;
		 *	} else if (devp->prop.color_format == TVIN_RGB444) {
		 *		format_convert = VDIN_FORMAT_CONVERT_RGB_RGB;
		 *	}
		 *} else {
		 *	format_convert = VDIN_FORMAT_CONVERT_YUV_YUV444;
		 *}
		 */
		if (devp->prop.color_format == TVIN_YUV422) {
			/*rx will tunneled to 444*/
			format_convert = VDIN_FORMAT_CONVERT_YUV_YUV444;
		} else if (devp->prop.color_format == TVIN_RGB444) {
			format_convert = VDIN_FORMAT_CONVERT_RGB_RGB;
		} else {
			format_convert = VDIN_FORMAT_CONVERT_YUV_YUV444;
		}
	}
#endif
	/*hw test:vdin de-tunnel to 422 12bit*/
	//if (devp->dtdata->ipt444_to_422_12bit && vdin_cfg_444_to_422_wmif_en)
	//	format_convert = VDIN_FORMAT_CONVERT_YUV_YUV422;

	/* vdin v4l2 mode */
	if (devp->work_mode == VDIN_WORK_MD_V4L) {
		pr_info("%s vdin v4l2 mode,color_format:%#x,pixelformat:%#x\n", __func__,
			devp->prop.color_format, devp->v4l2_fmt.fmt.pix_mp.pixelformat);
		if (devp->prop.color_format == TVIN_RGB444) {
			if (devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12 ||
				devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12M)
				format_convert = VDIN_FORMAT_CONVERT_RGB_NV12;
			else if (devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21 ||
				devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21M)
				format_convert = VDIN_FORMAT_CONVERT_RGB_NV21;
			else
				format_convert = VDIN_FORMAT_CONVERT_RGB_YUV422;
		} else {
			if (devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12 ||
				devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12M)
				format_convert = VDIN_FORMAT_CONVERT_YUV_NV12;
			else if (devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21 ||
				devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21M)
				format_convert = VDIN_FORMAT_CONVERT_YUV_NV21;
			else
				format_convert = VDIN_FORMAT_CONVERT_YUV_YUV422;
		}
	}

	devp->format_convert = format_convert;
	if (vdin_is_convert_to_422(format_convert))
		devp->mif_fmt = MIF_FMT_YUV422;
	else if (vdin_is_convert_to_444(format_convert))
		devp->mif_fmt = MIF_FMT_YUV444;
	else if (vdin_is_convert_to_nv21(format_convert))
		devp->mif_fmt = MIF_FMT_NV12_21;
	else
		devp->mif_fmt = MIF_FMT_YUV422;
	pr_info("%s pc mode:%d(%d), man_md:0x%x, cfmt:%d, dst cfmt:%d convert md:%d mif_fmt:%d\n",
		__func__, vdin_pc_mode, devp->vdin_pc_mode, manual_md,
		devp->prop.color_format, devp->prop.dest_cfmt, format_convert,
		devp->mif_fmt);
}

/*function:
 *	format_convert
 *	based on dest_cfmt
 */
enum vdin_format_convert_e
vdin_get_format_convert_matrix0_s5(struct vdin_dev_s *devp)
{
	enum vdin_format_convert_e format_convert = VDIN_FORMAT_CONVERT_MAX;

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		format_convert = VDIN_FORMAT_CONVERT_RGB_NV21;
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		format_convert = VDIN_FORMAT_CONVERT_RGB_NV12;
		break;
	case VDIN_FORMAT_CONVERT_YUV_RGB:
	case VDIN_FORMAT_CONVERT_RGB_RGB:
		format_convert = VDIN_FORMAT_CONVERT_RGB_RGB;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		format_convert = VDIN_FORMAT_CONVERT_RGB_YUV444;
		break;
	default:
		format_convert = VDIN_FORMAT_CONVERT_RGB_YUV422;
		break;
	}

	return format_convert;
}

/*function:
 *	format_convert
 *	based on color_cfmt
 */
enum vdin_format_convert_e
vdin_get_format_convert_matrix1_s5(struct vdin_dev_s *devp)
{
	enum vdin_format_convert_e format_convert = VDIN_FORMAT_CONVERT_MAX;

	switch (devp->prop.color_format) {
	case TVIN_YUV422:
	case TVIN_YUYV422:
	case TVIN_YVYU422:
	case TVIN_UYVY422:
	case TVIN_VYUY422:
	case TVIN_YUV444:
		format_convert = VDIN_FORMAT_CONVERT_YUV_RGB;
	break;
	case TVIN_RGB444:
		format_convert = VDIN_FORMAT_CONVERT_RGB_RGB;
		break;
	default:
		format_convert = VDIN_FORMAT_CONVERT_MAX;
	break;
	}
	return format_convert;
}

/*get prob of r/g/b
 *	r 9:0
 *	g 19:10
 *	b 29:20
 */
void vdin_prob_get_rgb_s5(unsigned int offset,
		       unsigned int *r, unsigned int *g, unsigned int *b)
{
	*b = rgb_info_b = rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
				  COMPONENT2_PROBE_COLOR_BIT,
				  COMPONENT0_PROBE_COLOR_WID);
	*g = rgb_info_g = rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
				  COMPONENT1_PROBE_COLOR_BIT,
				  COMPONENT1_PROBE_COLOR_WID);
	*r = rgb_info_r = rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
				  COMPONENT0_PROBE_COLOR_BIT,
				  COMPONENT0_PROBE_COLOR_WID);
}

void vdin_prob_get_yuv_s5(unsigned int offset,
		       unsigned int *rgb_yuv0,
		       unsigned int *rgb_yuv1,
		       unsigned int *rgb_yuv2)
{
	*rgb_yuv0 = ((rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
			      COMPONENT2_PROBE_COLOR_BIT,
			      COMPONENT2_PROBE_COLOR_WID) << 8) >> 10);
	*rgb_yuv1 = ((rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
			      COMPONENT1_PROBE_COLOR_BIT,
			      COMPONENT1_PROBE_COLOR_WID) << 8) >> 10);
	*rgb_yuv2 = ((rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
			      COMPONENT0_PROBE_COLOR_BIT,
			      COMPONENT0_PROBE_COLOR_WID) << 8) >> 10);
}

void vdin_prob_set_before_or_after_mat_s5(unsigned int offset,
				       unsigned int x, struct vdin_dev_s *devp)
{
	if (x != 0 && x != 1)
		return;
	/* 1:probe pixel data after matrix */
	wr_bits(offset, VDIN_MATRIX_CTRL, x,
		VDIN_PROBE_POST_BIT, VDIN_PROBE_POST_WID);
}

void vdin_prob_matrix_sel_s5(unsigned int offset,
			  unsigned int sel, struct vdin_dev_s *devp)
{
	unsigned int x;

	x = sel & 0x03;
	/* 1:select matrix 1 */
	wr_bits(offset, VDIN_MAT0_CTRL, x, 1, 1);
}

/* this function set flowing parameters:
 *a.rgb_info_x	b.rgb_info_y
 *debug usage:
 *echo rgb_xy x y > /sys/class/vdin/vdinx/attr
 */
void vdin_prob_set_xy_s5(unsigned int offset,
		      unsigned int x, unsigned int y, struct vdin_dev_s *devp)
{
	/* set position */
	rgb_info_x = x;
	if (devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_INTERLACED)
		rgb_info_y = y / 2;
	else
		rgb_info_y = y;

	/* #if defined(VDIN_V1) */
	wr_bits(offset, VDIN_MAT0_PROBE_POS, rgb_info_y,
		PROBE_POX_Y_BIT, PROBE_POX_Y_WID);
	wr_bits(offset, VDIN_MAT0_PROBE_POS, rgb_info_x,
		PROBE_POS_X_BIT, PROBE_POS_X_WID);
}

/*function:
 *	1.set meas mux based on port_:
 *		0x01: /mpeg/			0x10: /CVBS/
 *		0x02: /bt656/			0x20: /SVIDEO/
 *		0x04: /VGA/				0x40: /hdmi/
 *		0x08: /COMPONENT/		0x80: /dvin/
 *		0xc0:/viu/				0x100:/dtv mipi/
 *		0x200:/isp/
 *	2.set VDIN_MEAS in accumulation mode
 *	3.set VPP_VDO_MEAS in accumulation mode
 *	4.set VPP_MEAS in latch-on-falling-edge mode
 *	5.set VDIN_MEAS mux
 *	6.manual reset VDIN_MEAS & VPP_VDO_MEAS at the same time
 */
static void vdin_set_meas_mux_s5(unsigned int offset, enum tvin_port_e port_,
			      enum bt_path_e bt_path)
{
	/* unsigned int offset = devp->addr_offset; */
	unsigned int meas_mux = MEAS_MUX_NULL;

	if (is_meson_s5_cpu()) {
		return;
	}
	switch ((port_) >> 8) {
	case 0x01: /* mpeg */
		meas_mux = MEAS_MUX_NULL;
		break;
	case 0x02: /* bt656 , txl and txlx do not support bt656 */
		if ((is_meson_gxbb_cpu() || is_meson_gxtvbb_cpu()) &&
		    bt_path == BT_PATH_GPIO_B) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			meas_mux = MEAS_MUX_656_B;
#endif
		} else if ((is_meson_gxl_cpu() || is_meson_gxm_cpu() ||
			cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) &&
			(bt_path == BT_PATH_GPIO)) {
			meas_mux = MEAS_MUX_656;
		} else {
			pr_info("cpu not define or do not support  bt656");
		}
		break;
	case 0x04: /* VGA */
		meas_mux = MEAS_MUX_TVFE;
		break;
	case 0x08: /* COMPONENT */
		meas_mux = MEAS_MUX_TVFE;
		break;
	case 0x10: /* CVBS */
		meas_mux = MEAS_MUX_CVD2;
		break;
	case 0x20: /* SVIDEO */
		meas_mux = MEAS_MUX_CVD2;
		break;
	case 0x40: /* hdmi */
		meas_mux = MEAS_MUX_HDMI;
		break;
	case 0x80: /* dvin */
		meas_mux = MEAS_MUX_DVIN;
		break;
	case 0xa0:/* viu */
		meas_mux = MEAS_MUX_VIU1;
		break;
	case 0xc0:/* viu */
		meas_mux = MEAS_MUX_VIU2;
		break;
	case 0x100:/* dtv mipi */
		meas_mux = MEAS_MUX_DTV;
		break;
	case 0x200:/* isp */
		meas_mux = MEAS_MUX_ISP;
		break;
	default:
		meas_mux = MEAS_MUX_NULL;
		break;
	}
	/* set VDIN_MEAS in accumulation mode */
	wr_bits(offset, VDIN_MEAS_CTRL0, 1,
		MEAS_VS_TOTAL_CNT_EN_BIT, MEAS_VS_TOTAL_CNT_EN_WID);
	/* set VDIN_MEAS mux */
	wr_bits(offset, VDIN_MEAS_CTRL0, meas_mux,
		MEAS_HS_VS_SEL_BIT, MEAS_HS_VS_SEL_WID);
	/* manual reset VDIN_MEAS,
	 * rst = 1 & 0
	 */
	wr_bits(offset, VDIN_MEAS_CTRL0, 1, MEAS_RST_BIT, MEAS_RST_WID);
	wr_bits(offset, VDIN_MEAS_CTRL0, 0, MEAS_RST_BIT, MEAS_RST_WID);
}

/*function:set VDIN_COM_CTRL0
 *Bit 3:0 vdin selection,
 *	1: mpeg_in from dram, 2: bt656 input,3: component input
 *	4: tv decoder input, 5: hdmi rx input,6: digital video input,
 *	7: loopback from Viu1, 8: MIPI.
 */
/* Bit 7:6, component0 output switch,
 *	00: select component0 in,01: select component1 in,
 *	10: select component2 in
 */
/* Bit 9:8, component1 output switch,
 *	00: select component0 in,
 *	01: select component1 in, 10: select component2 in
 */
/* Bit 11:10, component2 output switch,
 *	00: select component0 in, 01: select component1 in,
 *	10: select component2 in
 */

/* attention:new add for bt656
 * 0x02: /bt656/
	a.BT_PATH_GPIO:	gxl & gxm & g12a
	b.BT_PATH_GPIO_B:gxtvbb & gxbb
	c.txl and txlx don't support bt656
 */
void vdin_set_top_s5(struct vdin_dev_s *devp, enum tvin_port_e port,
		  enum tvin_color_fmt_e input_cfmt, enum bt_path_e bt_path)
{
	unsigned int offset = devp->addr_offset;
	unsigned int vdin_mux = VDIN_MUX_NULL;
	unsigned int vdin_data_bus_0 = VDIN_MAP_Y_G;
	unsigned int vdin_data_bus_1 = VDIN_MAP_BPB;
	unsigned int vdin_data_bus_2 = VDIN_MAP_RCR;

	pr_info("%s %d:port:%d,input_cfmt:%d\n",
		__func__, __LINE__, port, input_cfmt);

	wr(offset, VDIN_TOP_SECURE1_MAX_SIZE, (3840 << 16) | (2160 << 0));
	wr(offset, VDIN_IF_TOP_SIZE, (devp->h_active_org << 16) | (devp->v_active_org << 0));
	switch (port >> 8) {
	case 0x01: /* mpeg */
		vdin_mux = VDIN_VDI0_MPEG;
		wr(offset, VDIN_IF_TOP_MPEG_CTRL, 0xe4);
		break;
	case 0x02: /* first bt656 */
		vdin_mux = VDIN_VDI1_BT656;
		wr(offset, VDIN_IF_TOP_VDI1_CTRL, 0xe4);
		break;
	case 0x04: /* reserved */
		vdin_mux = VDIN_VDI2_RESERVED;
		wr(offset, VDIN_IF_TOP_VDI2_CTRL, 0xe4);
		break;
	case 0x08: /* tv decode */
		vdin_mux = VDIN_VDI3_TV_DECODE_IN;
		wr(offset, VDIN_IF_TOP_VDI3_CTRL, 0xe4);
		break;
	case 0x10:
		break;
	case 0x20: /* digital video */
		vdin_mux = VDIN_VDI5_DVI;
		wr(offset, VDIN_IF_TOP_VDI5_CTRL, 0xe4);
		break;
	case 0x40: /* hdmi rx *//* first internal loopback */
		vdin_mux = VDIN_VDI4_HDMIRX;
		wr(offset, VDIN_IF_TOP_VDI4_CTRL, 0xe4);
		break;
	case 0x80: /* mipi csi2 */
		vdin_mux = VDIN_VDI7_MIPI_CSI2;
		wr(offset, VDIN_IF_TOP_VDI7_CTRL, 0xe4);
		break;
	case 0xa0:/* viu1 */
		if (port >= TVIN_PORT_VIU1_WB0_VD1 && port <= TVIN_PORT_VIU1_WB0_POST_BLEND)
			vdin_mux = VDIN_VDI6_LOOPBACK_1;
		else if ((port >= TVIN_PORT_VIU1_WB1_VD1) &&
			 (port <= TVIN_PORT_VIU1_WB1_POST_BLEND))
			vdin_mux = VDIN_VDI8_LOOPBACK_2;
		wr(offset, VDIN_IF_TOP_VDI6_CTRL, 0xe4);
		//wr_bits(offset, VDIN_IF_TOP_BUF_CTRL, 0xe4, 0, 8);
		break;
	case 0xc0: /* viu2 */
		//vdin_mux = VDIN_MUX_VIU1_WB1;
		//wr(offset, VDIN_IF_TOP_VDI9_CTRL, 0xe4);
		break;
	case 0xe0: /* venc0 */
		vdin_mux = VDIN_VDI6_LOOPBACK_1;
		wr(offset, VDIN_IF_TOP_VDI6_CTRL, 0xf4);
		break;
	default:
		vdin_mux = VDIN_MUX_NULL;
		break;
	}

	switch (input_cfmt) {
	case TVIN_YVYU422:
	//case TVIN_YUV444:
		vdin_data_bus_1 = VDIN_MAP_RCR;
		vdin_data_bus_2 = VDIN_MAP_BPB;
		break;
	case TVIN_UYVY422:
		vdin_data_bus_0 = VDIN_MAP_BPB;
		vdin_data_bus_1 = VDIN_MAP_RCR;
		vdin_data_bus_2 = VDIN_MAP_Y_G;
		break;
	case TVIN_VYUY422:
		vdin_data_bus_0 = VDIN_MAP_BPB;
		vdin_data_bus_1 = VDIN_MAP_RCR;
		vdin_data_bus_2 = VDIN_MAP_Y_G;
		break;
	case TVIN_RGB444:
		/*RGB mapping*/
		if (devp->set_canvas_manual == 1) {
			vdin_data_bus_0 = VDIN_MAP_RCR;
			vdin_data_bus_1 = VDIN_MAP_BPB;
			vdin_data_bus_2 = VDIN_MAP_Y_G;
		}
		break;
	default:
		break;
	}
	if (vdin_dv_is_need_tunnel(devp)) {
		vdin_data_bus_0 = VDIN_MAP_BPB;
		vdin_data_bus_1 = VDIN_MAP_Y_G;
		vdin_data_bus_2 = VDIN_MAP_RCR;
	}
	pr_info("%s %d:vdin_mux:%d\n", __func__, __LINE__, vdin_mux);

	wr_bits(offset, VDIN_PP_TOP_CTRL, vdin_data_bus_0,
		PP_COMP0_SWAP_BIT, PP_COMP0_SWAP_WID);
	wr_bits(offset, VDIN_PP_TOP_CTRL, vdin_data_bus_1,
		PP_COMP1_SWAP_BIT, PP_COMP1_SWAP_WID);
	wr_bits(offset, VDIN_PP_TOP_CTRL, vdin_data_bus_2,
		PP_COMP2_SWAP_BIT, PP_COMP2_SWAP_WID);

	wr(offset, VDIN_PP_TOP_IN_SIZE, devp->h_active_org << 16 | devp->v_active_org);
	wr(offset, VDIN_PP_TOP_OUT_SIZE, devp->h_active << 16 | devp->v_active);

	wr_bits(offset, VDIN_IF_TOP_VDIN1_SECURE_CTRL, vdin_mux,
		IF_VDIN1_SEL_BIT, IF_VDIN1_SEL_WID);
	//wr_bits(offset, VDIN_IF_TOP_VDIN1_SECURE_CTRL, 1, IF_VDIN1_EN_BIT, IF_VDIN1_EN_WID);
	wr_bits(offset, VDIN_IF_TOP_VDIN1_CTRL, 1, 0, 1); /* reg_vdin1_enable */
}

/*this function will set the bellow parameters of devp:
 *1.h_active
 *2.v_active
 */
void vdin_set_decimation_s5(struct vdin_dev_s *devp)
{
	//unsigned int offset = devp->addr_offset;
	unsigned int new_clk = 0;
	bool decimation_in_frontend = false;

	if (devp->prop.decimation_ratio & HDMI_DE_REPEAT_DONE_FLAG) {
		decimation_in_frontend = true;
		if (vdin_ctl_dbg)
			pr_info("decimation_in_frontend\n");
	}
	devp->prop.decimation_ratio = devp->prop.decimation_ratio &
			DECIMATION_REAL_RANGE;

	new_clk = devp->fmt_info_p->pixel_clk /
			(devp->prop.decimation_ratio + 1);
	devp->h_active = devp->fmt_info_p->h_active /
			(devp->prop.decimation_ratio + 1);
	devp->v_active = devp->fmt_info_p->v_active;
	if (vdin_ctl_dbg)
		pr_info("%s decimation_ratio=%u,new_clk=%u.h:%d,v:%d\n",
			__func__, devp->prop.decimation_ratio, new_clk,
			devp->h_active, devp->v_active);

//	if (devp->prop.decimation_ratio && !decimation_in_frontend)	{
//		/* ratio */
//		wr_bits(offset, VDIN_ASFIFO_CTRL2,
//			devp->prop.decimation_ratio,
//			ASFIFO_DECIMATION_NUM_BIT, ASFIFO_DECIMATION_NUM_WID);
//		/* en */
//		wr_bits(offset, VDIN_ASFIFO_CTRL2, 1,
//			ASFIFO_DECIMATION_DE_EN_BIT,
//			ASFIFO_DECIMATION_DE_EN_WID);
//		/* manual reset, rst = 1 & 0 */
//		wr_bits(offset, VDIN_ASFIFO_CTRL2, 1,
//			ASFIFO_DECIMATION_SYNC_WITH_DE_BIT,
//			ASFIFO_DECIMATION_SYNC_WITH_DE_WID);
//		wr_bits(offset, VDIN_ASFIFO_CTRL2, 0,
//			ASFIFO_DECIMATION_SYNC_WITH_DE_BIT,
//			ASFIFO_DECIMATION_SYNC_WITH_DE_WID);
//	}

	/* output_width_m1 */
	//wr_bits(offset, VDIN_INTF_WIDTHM1, (devp->h_active - 1),
	//	VDIN_INTF_WIDTHM1_BIT, VDIN_INTF_WIDTHM1_WID);
	if (vdin_ctl_dbg)
		pr_info("%s: h_active=%u, v_active=%u\n",
			__func__, devp->h_active, devp->v_active);
}

void vdin_fix_nonstd_vsync_s5(struct vdin_dev_s *devp)
{
	wr_bits(devp->addr_offset, VDIN_WRMIF_URGENT_CTRL, 1,
		0, 16);
}

/* this function will set the bellow parameters of devp:
 * 1.h_active
 * 2.v_active
 *	set VDIN_WIN_H_START_END
 *		Bit 28:16 input window H start
 *		Bit 12:0  input window H end
 *	set VDIN_WIN_V_START_END
 *		Bit 28:16 input window V start
 *		Bit 12:0  input window V start
 */
void vdin_set_cutwin_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	unsigned int he = 0, ve = 0;

	if ((devp->prop.hs || devp->prop.he ||
	     devp->prop.vs || devp->prop.ve) &&
	    devp->h_active > (devp->prop.hs + devp->prop.he) &&
	    devp->v_active > (devp->prop.vs + devp->prop.ve)) {
		devp->h_active -= (devp->prop.he + devp->prop.hs);
		devp->v_active -= (devp->prop.ve + devp->prop.vs);
		he = devp->prop.hs + devp->h_active - 1;
		ve = devp->prop.vs + devp->v_active - 1;
		wr(offset, VDIN_PP_TOP_H_WIN,
		   (devp->prop.hs << INPUT_WIN_H_START_BIT) |
		   (he << INPUT_WIN_H_END_BIT));
		wr(offset, VDIN_PP_TOP_V_WIN,
		   (devp->prop.vs << INPUT_WIN_V_START_BIT) |
		   (ve << INPUT_WIN_V_END_BIT));
		wr_bits(offset, VDIN_PP_TOP_CTRL, 1,
			PP_WIN_EN_BIT, PP_WIN_EN_WID);
		if (vdin_ctl_dbg)
			pr_info("%s enable cutwin hs=%d, he=%d,  vs=%d, ve=%d\n",
				__func__,
			devp->prop.hs, devp->prop.he,
			devp->prop.vs, devp->prop.ve);
	} else {
		wr_bits(offset, VDIN_PP_TOP_CTRL, 0,
			PP_WIN_EN_BIT, PP_WIN_EN_WID);
		wr(offset, VDIN_PP_TOP_H_WIN, 0x1fff);
		wr(offset, VDIN_PP_TOP_V_WIN, 0x1fff);
		if (vdin_ctl_dbg)
			pr_info("%s disable cutwin!!! hs=%d, he=%d,  vs=%d, ve=%d\n",
				__func__,
			devp->prop.hs, devp->prop.he,
			devp->prop.vs, devp->prop.ve);
	}
	if (vdin_ctl_dbg)
		pr_info("%s: h_active=%d, v_active=%d, hs:%u, he:%u, vs:%u, ve:%u\n",
			__func__, devp->h_active, devp->v_active,
			devp->prop.hs, devp->prop.he,
			devp->prop.vs, devp->prop.ve);
}

void vdin_set_config_s5(struct vdin_dev_s *devp)
{
	if (is_meson_gxbb_cpu() || is_meson_gxm_cpu() || is_meson_gxl_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		/* max pixel clk of vdin for gxbb/gxm/gxl */
		devp->vdin_max_pixel_clk =
			VDIN_PIXEL_CLK_4K_30HZ; /* 2160p30hz*/
#endif
	} else {
		devp->vdin_max_pixel_clk =
			VDIN_PIXEL_CLK_4K_60HZ; /* 2160p60hz*/
	}
}

void vdin_change_matrix0_s5(u32 offset, u32 matrix_csc)
{
	struct vdin_matrix_lup_s *matrix_tbl;

	if (matrix_csc == VDIN_MATRIX_NULL)	{
		wr_bits(offset, VDIN_PP_TOP_CTRL, 0,
			PP_MAT0_EN_BIT, PP_MAT0_EN_WID);
	} else {
		matrix_tbl = &vdin_matrix_lup[matrix_csc - 1];

		/*coefficient index select matrix0*/
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX_COEF_INDEX_BIT, VDIN_MATRIX_COEF_INDEX_WID);

		wr(offset, VDIN_MAT0_PRE_OFFSET0_1, matrix_tbl->pre_offset0_1);
		wr(offset, VDIN_MAT0_PRE_OFFSET2, matrix_tbl->pre_offset2);
		wr(offset, VDIN_MAT0_COEF00_01, matrix_tbl->coef00_01);
		wr(offset, VDIN_MAT0_COEF02_10, matrix_tbl->coef02_10);
		wr(offset, VDIN_MAT0_COEF11_12, matrix_tbl->coef11_12);
		wr(offset, VDIN_MAT0_COEF20_21, matrix_tbl->coef20_21);
		wr(offset, VDIN_MAT0_COEF22,    matrix_tbl->coef22);
		wr(offset, VDIN_MAT0_OFFSET0_1, matrix_tbl->post_offset0_1);
		wr(offset, VDIN_MAT0_OFFSET2,   matrix_tbl->post_offset2);
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX0_BYPASS_BIT, VDIN_MATRIX0_BYPASS_WID);
		wr_bits(offset, VDIN_PP_TOP_CTRL, 1,
			PP_MAT0_EN_BIT, PP_MAT0_EN_WID);
	}

	pr_info("%s id:%d\n", __func__, matrix_csc);
}

/* hdr2 used as only matrix,not hdr matrix
 * should use hdr matrix related registers
 * VDIN_MAT1_XX have no effect
 */
void vdin_change_matrix1_s5(u32 offset, u32 matrix_csc)
{
	struct vdin_matrix_lup_s *matrix_tbl;

	if (matrix_csc == VDIN_MATRIX_NULL)	{
		wr_bits(offset, VDIN_PP_TOP_CTRL, 0,
			PP_MAT1_EN_BIT, PP_MAT1_EN_WID);
	} else {
		matrix_tbl = &vdin_matrix_lup[matrix_csc - 1];

		/*coefficient index select matrix0*/
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX_COEF_INDEX_BIT, VDIN_MATRIX_COEF_INDEX_WID);

		wr(offset, VDIN_HDR2_MATRIXI_PRE_OFFSET0_1, matrix_tbl->pre_offset0_1);
		wr(offset, VDIN_HDR2_MATRIXI_PRE_OFFSET2, matrix_tbl->pre_offset2);
		wr(offset, VDIN_HDR2_MATRIXI_COEF00_01, matrix_tbl->coef00_01);
		wr(offset, VDIN_HDR2_MATRIXI_COEF02_10, matrix_tbl->coef02_10);
		wr(offset, VDIN_HDR2_MATRIXI_COEF11_12, matrix_tbl->coef11_12);
		wr(offset, VDIN_HDR2_MATRIXI_COEF20_21, matrix_tbl->coef20_21);
		wr(offset, VDIN_HDR2_MATRIXI_COEF22, matrix_tbl->coef22);
		wr(offset, VDIN_HDR2_MATRIXI_OFFSET0_1, matrix_tbl->post_offset0_1);
		wr(offset, VDIN_HDR2_MATRIXI_OFFSET2, matrix_tbl->post_offset2);
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX0_BYPASS_BIT, VDIN_MATRIX0_BYPASS_WID);
		wr(offset, VDIN_PP_HDR2_MATRIXI_EN_CTRL, 1);/* rgb2yuv or yuv2rgb */
		wr_bits(offset, VDIN_PP_HDR2_CTRL, 1, 16, 1);// hdr2 used as matrix
		wr_bits(offset, VDIN_PP_TOP_CTRL, 1,
			PP_MAT1_EN_BIT, PP_MAT1_EN_WID);
	}

	pr_info("%s id:%d\n", __func__, matrix_csc);
}

/*
 * after, equal g12a have this matrix
 */
void vdin_change_matrix_hdr_s5(u32 offset, u32 matrix_csc)
{
	struct vdin_matrix_lup_s *matrix_tbl;

	if (matrix_csc == VDIN_MATRIX_NULL)	{
		wr_bits(offset, VDIN_PP_TOP_CTRL, 0,
			PP_MAT0_EN_BIT, PP_MAT0_EN_WID);
		wr_bits(offset, VDIN_PP_HDR2_MATRIXI_EN_CTRL, 0, 0, 8);
	} else {
		matrix_tbl = &vdin_matrix_lup[matrix_csc - 1];

		/*coefficient index select matrix0*/
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX_COEF_INDEX_BIT, VDIN_MATRIX_COEF_INDEX_WID);

		wr(offset, VDIN_PP_HDR2_MATRIXI_PRE_OFFSET0_1, matrix_tbl->pre_offset0_1);
		wr(offset, VDIN_PP_HDR2_MATRIXI_PRE_OFFSET2, matrix_tbl->pre_offset2);
		wr(offset, VDIN_PP_HDR2_MATRIXI_COEF00_01, matrix_tbl->coef00_01);
		wr(offset, VDIN_PP_HDR2_MATRIXI_COEF02_10, matrix_tbl->coef02_10);
		wr(offset, VDIN_PP_HDR2_MATRIXI_COEF11_12, matrix_tbl->coef11_12);
		wr(offset, VDIN_PP_HDR2_MATRIXI_COEF20_21, matrix_tbl->coef20_21);
		wr(offset, VDIN_PP_HDR2_MATRIXI_COEF22,    matrix_tbl->coef22);
		wr(offset, VDIN_PP_HDR2_MATRIXI_OFFSET0_1, matrix_tbl->post_offset0_1);
		wr(offset, VDIN_PP_HDR2_MATRIXI_OFFSET2,   matrix_tbl->post_offset2);
		wr_bits(offset, VDIN_PP_HDR2_MATRIXI_EN_CTRL, 1, 0, 8);
		//wr_bits(offset, VDIN_PP_HDR2_CTRL, 1, 16, 1);
		//wr_bits(offset, VDIN_PP_HDR2_CTRL, 0, 13, 1);//VDIN0_HDR2_top_en
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX0_BYPASS_BIT, VDIN_MATRIX0_BYPASS_WID);
		wr_bits(offset, VDIN_PP_HDR2_CTRL, 1,
			PP_MATRIXI_TOP_EN_BIT, PP_MATRIXI_TOP_EN_WID);
		wr_bits(offset, VDIN_PP_HDR2_CTRL, 0,//refer to above
			PP_HDR2_TOP_EN_BIT, PP_HDR2_TOP_EN_WID);
	}

	pr_info("%s id:%d\n", __func__, matrix_csc);
}

static enum vdin_matrix_csc_e
vdin_set_color_matrix_s5(enum vdin_matrix_sel_e matrix_sel, unsigned int offset,
		       struct tvin_format_s *tvin_fmt_p,
		       enum vdin_format_convert_e format_convert,
		       enum tvin_port_e port,
		       enum tvin_color_fmt_range_e color_fmt_range,
		       unsigned int vdin_hdr_flag,
		       unsigned int color_range_mode)
{
	enum vdin_matrix_csc_e    matrix_csc = VDIN_MATRIX_NULL;
	/*struct vdin_matrix_lup_s *matrix_tbl;*/
	struct tvin_format_s *fmt_info = tvin_fmt_p;

	pr_info("%s tvin_fmt_p:%p %p,format_convert:%d\n",
		__func__, fmt_info, tvin_fmt_p, format_convert);

	if (!fmt_info) {
		pr_info("error %s tvin_fmt_p:%p\n", __func__, tvin_fmt_p);
		return VDIN_MATRIX_NULL;
	}
	switch (format_convert)	{
	case VDIN_MATRIX_XXX_YUV_BLACK:
		matrix_csc = VDIN_MATRIX_XXX_YUV601_BLACK;
		break;
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		if (port >= TVIN_PORT_HDMI0 && port <= TVIN_PORT_HDMI7) {
			if (color_range_mode == 1) {
				if (color_fmt_range == TVIN_RGB_FULL) {
					matrix_csc = VDIN_MATRIX_RGB_YUV709F;
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGB_YUV709;
				} else {
					matrix_csc =
						VDIN_MATRIX_RGBS_YUV709F;
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGBS_YUV709;
				}
			} else {
				if (color_fmt_range == TVIN_RGB_FULL) {
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGB2020_YUV2020;
					else
						matrix_csc =
						VDIN_MATRIX_RGB_YUV709;
				} else {
					matrix_csc = VDIN_MATRIX_RGBS_YUV709;
				}
			}
		} else {
			if (color_range_mode == 1)
				matrix_csc = VDIN_MATRIX_RGB_YUV709F;
			else
				matrix_csc = VDIN_MATRIX_RGB_YUV709;
		}
		break;
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
		matrix_csc = VDIN_MATRIX_GBR_YUV601;
		break;
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		matrix_csc = VDIN_MATRIX_BRG_YUV601;
		break;
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		if (port >= TVIN_PORT_HDMI0 && port <= TVIN_PORT_HDMI7) {
			if (color_range_mode == 1) {
				if (color_fmt_range == TVIN_RGB_FULL) {
					matrix_csc = VDIN_MATRIX_RGB_YUV709F;
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGB_YUV709;
				} else {
					matrix_csc = VDIN_MATRIX_RGBS_YUV709F;
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGBS_YUV709;
				}
			} else {
				if (color_fmt_range == TVIN_RGB_FULL) {
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGB2020_YUV2020;
					else
						matrix_csc =
						VDIN_MATRIX_RGB_YUV709F;
				} else {
					matrix_csc = VDIN_MATRIX_RGBS_YUV709;
				}
			}
		} else {
			if (color_range_mode == 1)
				matrix_csc = VDIN_MATRIX_RGB_YUV709F;
			else
				matrix_csc = VDIN_MATRIX_RGB_YUV709;
		}
		break;
	case VDIN_FORMAT_CONVERT_YUV_RGB:
		if ((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE &&
		     fmt_info->v_active >= 720) || /* 720p & above */
		    (fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED &&
		     fmt_info->v_active >= 540))  /* 1080i & above */
			matrix_csc = VDIN_MATRIX_YUV709_RGB;
		else
			matrix_csc = VDIN_MATRIX_YUV601_RGB;
		break;
	case VDIN_FORMAT_CONVERT_YUV_GBR:
		if ((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE &&
		     fmt_info->v_active >= 720) || /* 720p & above */
		    (fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED &&
		     fmt_info->v_active >= 540))    /* 1080i & above */
			matrix_csc = VDIN_MATRIX_YUV709_GBR;
		else
			matrix_csc = VDIN_MATRIX_YUV601_GBR;
		break;
	case VDIN_FORMAT_CONVERT_YUV_BRG:
		if ((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE &&
		     fmt_info->v_active >= 720) || /* 720p & above */
		    (fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED &&
		     fmt_info->v_active >= 540))    /* 1080i & above */
			matrix_csc = VDIN_MATRIX_YUV709_BRG;
		else
			matrix_csc = VDIN_MATRIX_YUV601_BRG;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
		if ((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE &&
		     fmt_info->v_active >= 720) || /* 720p & above */
		    (fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED &&
		     fmt_info->v_active >= 540)) { /* 1080i & above */

			if (color_range_mode == 1 &&
			    color_fmt_range != TVIN_YUV_FULL)
				matrix_csc = VDIN_MATRIX_YUV709_YUV709F;
			else if (color_range_mode == 0 &&
				 color_fmt_range == TVIN_YUV_FULL)
				matrix_csc = VDIN_MATRIX_YUV709F_YUV709;
		} else {
			if (color_range_mode == 1) {
				if (color_fmt_range == TVIN_YUV_FULL)
					matrix_csc =
						VDIN_MATRIX_YUV601F_YUV709F;
				else
					matrix_csc = VDIN_MATRIX_YUV601_YUV709F;
			} else {
				if (color_fmt_range == TVIN_YUV_FULL)
					matrix_csc = VDIN_MATRIX_YUV601F_YUV709;
				else
					matrix_csc = VDIN_MATRIX_YUV601_YUV709;
			}
		}
		if (vdin_hdr_flag == 1) {
			if (color_fmt_range == TVIN_YUV_FULL)
				matrix_csc = VDIN_MATRIX_YUV2020F_YUV2020;
			else
				matrix_csc = VDIN_MATRIX_NULL;
		}
		break;
	default:
		matrix_csc = VDIN_MATRIX_NULL;
		break;
	}

	//vdin_manual_matrix_csc_s5(&matrix_csc);

	if (matrix_sel == VDIN_SEL_MATRIX0)
		vdin_change_matrix0_s5(offset, matrix_csc);
	else if (matrix_sel == VDIN_SEL_MATRIX1)
		vdin_change_matrix1_s5(offset, matrix_csc);
	else if (matrix_sel == VDIN_SEL_MATRIX_HDR)
		vdin_change_matrix_hdr_s5(offset, matrix_csc);
	else
		matrix_csc = VDIN_MATRIX_NULL;

	return matrix_csc;
}

void vdin_set_hdr_s5(struct vdin_dev_s *devp)
{
	enum vd_format_e video_format;

	if (devp->index == 0 || !devp->dtdata->vdin1_set_hdr ||
		devp->debug.vdin1_set_hdr_bypass)
		return;

	switch (devp->parm.port) {
	case TVIN_PORT_VIU1_VIDEO:
	case TVIN_PORT_VIU1_WB0_VD1:
	case TVIN_PORT_VIU1_WB0_VD2:
	case TVIN_PORT_VIU1_WB0_POST_BLEND:
	case TVIN_PORT_VIU1_WB1_POST_BLEND:
	case TVIN_PORT_VIU1_WB1_VD1:
	case TVIN_PORT_VIU1_WB1_VD2:
	case TVIN_PORT_VIU1_WB0_OSD1:
	case TVIN_PORT_VIU1_WB0_OSD2:
	case TVIN_PORT_VIU1_WB1_OSD1:
	case TVIN_PORT_VIU1_WB1_OSD2:
		video_format = devp->vd1_fmt;
		break;

	case TVIN_PORT_VIU1_WB0_VPP:
	case TVIN_PORT_VIU1_WB1_VPP:
	case TVIN_PORT_VIU2_ENCL:
	case TVIN_PORT_VIU2_ENCI:
	case TVIN_PORT_VIU2_ENCP:
		video_format = devp->tx_fmt;
		break;

	default:
		video_format = devp->tx_fmt;
		break;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (for_amdv_certification()) {
		/*not modify data*/
		return;
	}
#endif

#ifndef VDIN_BRINGUP_NO_AMLVECM
	pr_info("%s fmt:%d\n", __func__, video_format);
	switch (video_format) {
	case SIGNAL_HDR10:
	case SIGNAL_HDR10PLUS:
		/* parameters:
		 * 1st, module sel: 6=VDIN0_HDR, 7=VDIN1_HDR
		 * 2nd, process sel: bit1=HDR_SDR
		 * bit11=HDR10P_SDR
		 */
		hdr_set(VDIN1_HDR, HDR_SDR, VPP_TOP0);
		break;

	case SIGNAL_SDR:
		/* HDR_BYPASS(bit0) | HLG_BYPASS(bit3) */
		hdr_set(VDIN1_HDR, HDR_BYPASS | HLG_BYPASS, VPP_TOP0);
		break;

	case SIGNAL_HLG:
		/* HLG_SDR(bit4) */
		hdr_set(VDIN1_HDR, HLG_SDR, VPP_TOP0);
		break;

	/* VDIN DON'T support dv loopback currently */
	case SIGNAL_DOVI:
		pr_err("err:  don't support dv signal loopback");
		break;

	default:
		break;
	}
#endif
	/* hdr set uses rdma, will delay 1 frame to take effect */
	devp->frame_drop_num = 1;
}

/*set matrix based on rgb_info_enable:
 * 0:set matrix0, disable matrix1
 * 1:set matrix1, set matrix0
 * after equal g12a: matrix1, matrix_hdr2
 */
void vdin_set_matrix_s5(struct vdin_dev_s *devp)
{
//	enum vdin_format_convert_e format_convert_matrix0;
//	enum vdin_format_convert_e format_convert_matrix1;
//	unsigned int offset = devp->addr_offset;
	enum vdin_matrix_sel_e matrix_sel;

	if (rgb_info_enable == 0) {
		/* matrix1 disable */
//	wr_bits(offset, VDIN_MATRIX_CTRL, 0,
//		VDIN_MATRIX1_EN_BIT, VDIN_MATRIX1_EN_WID);
//		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
//			matrix_sel = VDIN_SEL_MATRIX1;/*VDIN_SEL_MATRIX_HDR*/
//		} else {
//			matrix_sel = VDIN_SEL_MATRIX0;
//		}

		if (devp->debug.dbg_sel_mat & 0x10)
			matrix_sel = devp->debug.dbg_sel_mat & 0x10;
		else
			matrix_sel = VDIN_SEL_MATRIX0;/*VDIN_SEL_MATRIX_HDR*/
		pr_info("%s %d:%p,conv:%d,port:%d,fmt_range:%d,color_range:%d\n",
			__func__, __LINE__,
			devp->fmt_info_p, devp->format_convert,
			devp->parm.port, devp->prop.color_fmt_range,
			devp->color_range_mode);

		if (!devp->dv.dv_flag) {
			devp->csc_idx = vdin_set_color_matrix_s5(matrix_sel,
				devp->addr_offset,
				devp->fmt_info_p,
				devp->format_convert,
				devp->parm.port,
				devp->prop.color_fmt_range,
				devp->prop.vdin_hdr_flag | devp->dv.dv_flag,
				devp->color_range_mode);
		} else {
			devp->csc_idx = VDIN_MATRIX_NULL;
		}

		vdin_set_hdr_s5(devp);

//		#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
//		if (vdin_is_dolby_signal_in(devp) ||
//		    devp->parm.info.fmt == TVIN_SIG_FMT_CVBS_SECAM)
//			wr_bits(offset, VDIN_MATRIX_CTRL, 0,
//				VDIN_MATRIX_EN_BIT, VDIN_MATRIX_EN_WID);
//		#endif
//		wr_bits(offset, VDIN_MATRIX_CTRL, 3,
//			VDIN_PROBE_SEL_BIT, VDIN_PROBE_SEL_WID);
	}

	if (devp->matrix_pattern_mode)
		vdin_set_matrix_color_s5(devp);
}

void vdin_select_matrix_s5(struct vdin_dev_s *devp, unsigned char id,
		      enum vdin_format_convert_e csc)
{
	pr_info("%s %d:id=%d, csc=%d\n", __func__, __LINE__, id, csc);
	switch (id) {
	case 0:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			devp->csc_idx =
				vdin_set_color_matrix_s5(VDIN_SEL_MATRIX1,
				devp->addr_offset,
				devp->fmt_info_p,
				devp->format_convert,
				devp->parm.port,
				devp->prop.color_fmt_range,
				devp->prop.vdin_hdr_flag | devp->dv.dv_flag,
				devp->color_range_mode);
		else
			devp->csc_idx =
				vdin_set_color_matrix_s5(VDIN_SEL_MATRIX0,
				devp->addr_offset,
				devp->fmt_info_p, csc,
				devp->parm.port,
				devp->prop.color_fmt_range,
				devp->prop.vdin_hdr_flag | devp->dv.dv_flag,
				devp->color_range_mode);
		break;
	case 1:
		devp->csc_idx = vdin_set_color_matrix_s5(VDIN_SEL_MATRIX1,
				devp->addr_offset,
				devp->fmt_info_p, csc,
				devp->parm.port,
				devp->prop.color_fmt_range,
				devp->prop.vdin_hdr_flag | devp->dv.dv_flag,
				devp->color_range_mode);
		break;
	default:
		break;
	}
}

/*set block bar
 *base on flowing parameters:
 *a.h_active	b.v_active
 */
static inline void vdin_set_bbar_s5(unsigned int offset, unsigned int v,
				 unsigned int h)
{
	unsigned int region_width = 1, block_thr = 0, line_thr = 0;

	while ((region_width << 1) < h)
		region_width <<= 1;

	block_thr = (region_width >> 1) * v;
	/* bblk=(bpix>thr) */
	block_thr = block_thr - (block_thr >> BBAR_BLOCK_THR_FACTOR);
	 /* bln=!(wpix>=thr) */
	line_thr  = h >> BBAR_LINE_THR_FACTOR;

	/* region_width */
	wr_bits(offset, VDIN_BLKBAR_CTRL0,
		region_width, BLKBAR_H_WIDTH_BIT, BLKBAR_H_WIDTH_WID);
	/* win_he */
	wr_bits(offset, VDIN_BLKBAR_H_START_END,
		(h - 1), BLKBAR_HEND_BIT, BLKBAR_HEND_WID);
	/* win_ve */
	wr_bits(offset, VDIN_BLKBAR_V_START_END,
		(v - 1), BLKBAR_VEND_BIT, BLKBAR_VEND_WID);
	/* bblk_thr_on_bpix */
	wr_bits(offset, VDIN_BLKBAR_CNT_THRESHOLD,
		block_thr, BLKBAR_CNT_TH_BIT, BLKBAR_CNT_TH_WID);
	/* blnt_thr_on_wpix */
	wr_bits(offset, VDIN_BLKBAR_ROW_TH1_TH2,
		line_thr, BLKBAR_ROW_TH1_BIT, BLKBAR_ROW_TH1_WID);
	/* blnb_thr_on_wpix */
	wr_bits(offset, VDIN_BLKBAR_ROW_TH1_TH2,
		line_thr, BLKBAR_ROW_TH2_BIT, BLKBAR_ROW_TH2_WID);
	/* en */
	wr_bits(offset, VDIN_BLKBAR_CTRL0,
		1, BLKBAR_DET_TOP_EN_BIT, BLKBAR_DET_TOP_EN_WID);
	/* manual reset, rst = 0 & 1, raising edge mode */
	wr_bits(offset, VDIN_BLKBAR_CTRL0, 0,
		BLKBAR_DET_SOFT_RST_N_BIT, BLKBAR_DET_SOFT_RST_N_WID);
	wr_bits(offset, VDIN_BLKBAR_CTRL0, 1,
		BLKBAR_DET_SOFT_RST_N_BIT, BLKBAR_DET_SOFT_RST_N_WID);
}

/*et histogram window
 * pow\h_start\h_end\v_start\v_end
 */
static inline void vdin_set_histogram_s5(unsigned int offset, unsigned int hs,
				      unsigned int he, unsigned int vs,
				      unsigned int ve)
{
	//unsigned int pixel_sum = 0, record_len = 0, hist_pow = 0;
	//pr_info("%s %d:hs=%d,he=%d,vs=%d,ve=%d\n",
	//	__func__, __LINE__, hs, he, vs, ve);
	if (hs < he && vs < ve) {
//		pixel_sum = (he - hs + 1) * (ve - vs + 1);
//		record_len = 0xffff << 3;
//		while (pixel_sum > record_len && hist_pow < 3) {
//			hist_pow++;
//			record_len <<= 1;
//		}
//		/* #ifdef CONFIG_MESON2_CHIP */
//		/* pow */
//		wr_bits(offset, VDIN_HIST_CTRL, hist_pow,
//			HIST_POW_BIT, HIST_POW_WID);
		/* win_hs */
		wr_bits(offset, VDIN_LUMA_HIST_H_START_END, hs,
			HIST_HSTART_BIT, HIST_HSTART_WID);
		/* win_he */
		wr_bits(offset, VDIN_LUMA_HIST_H_START_END, he,
			HIST_HEND_BIT, HIST_HEND_WID);
		/* win_vs */
		wr_bits(offset, VDIN_LUMA_HIST_V_START_END, vs,
			HIST_VSTART_BIT, HIST_VSTART_WID);
		/* win_ve */
		wr_bits(offset, VDIN_LUMA_HIST_V_START_END, ve,
			HIST_VEND_BIT, HIST_VEND_WID);
	}
}

/* set hist mux
 * hist_spl_sel, 0: win; 1: mat0; 2: vsc; 3: hdr2_mat1
 */
static inline void vdin_set_hist_mux_s5(struct vdin_dev_s *devp)
{
	enum tvin_port_e port = TVIN_PORT_NULL;

	port = devp->parm.port;
	/*if ((port < TVIN_PORT_HDMI0) || (port > TVIN_PORT_HDMI7))*/
	/*	return;*/
	/* For AV input no correct data, from vlsi fei.jun
	 * AV, HDMI all set 3
	 */
	/* use 11: form matrix1 din */
//	wr_bits(devp->addr_offset, VDIN_HIST_CTRL, 3,
//		HIST_HIST_DIN_SEL_BIT, HIST_HIST_DIN_SEL_WID);
	wr_bits(devp->addr_offset, VDIN_LUMA_HIST_CTRL, 0x7f, 24, 8);//reg_luma_hist_pix_white_th
	wr_bits(devp->addr_offset, VDIN_LUMA_HIST_CTRL, 0x7f, 16, 8);//reg_luma_hist_pix_black_th
	wr_bits(devp->addr_offset, VDIN_PP_TOP_CTRL, 0, 12, 2);//reg_hist_spl_sel
	wr_bits(devp->addr_offset, VDIN_PP_TOP_CTRL, 1, 7, 1);//reg_hist_spl_en

	/*for project get vdin1 hist*/
	//if (devp->index == 1)
	// wr_bits(devp->addr_offset, VDIN_WR_CTRL2, 1, 8, 1);
}

/* urgent ctr config */
/* if vdin fifo over up_th,will trigger increase
 *  urgent responds to vdin write,
 * if vdin fifo lower dn_th,will trigger decrease
 *  urgent responds to vdin write
 */
static void vdin_urgent_patch_s5(unsigned int offset, unsigned int v,
			      unsigned int h)
{
	if (h >= 1920 && v >= 1080) {
		wr_bits(offset, VDIN_LFIFO_URG_CTRL, 1,
			VDIN_LFIFO_URG_CTRL_EN_BIT, VDIN_LFIFO_URG_CTRL_EN_WID);
		wr_bits(offset, VDIN_LFIFO_URG_CTRL, 1,
			VDIN_LFIFO_URG_WR_EN_BIT, VDIN_LFIFO_URG_WR_EN_WID);
		wr_bits(offset, VDIN_LFIFO_URG_CTRL, 20,
			VDIN_LFIFO_URG_UP_TH_BIT, VDIN_LFIFO_URG_UP_TH_WID);
		wr_bits(offset, VDIN_LFIFO_URG_CTRL, 8,
			VDIN_LFIFO_URG_DN_TH_BIT, VDIN_LFIFO_URG_DN_TH_WID);
		/*vlsi guys suggest setting:*/
		W_VCBUS_BIT(VPU_ARB_URG_CTRL, 1,
			    VDIN_LFF_URG_CTRL_BIT, VDIN_LFF_URG_CTRL_WID);
		W_VCBUS_BIT(VPU_ARB_URG_CTRL, 1,
			    VPP_OFF_URG_CTRL_BIT, VPP_OFF_URG_CTRL_WID);
	} else {
		wr(offset, VDIN_LFIFO_URG_CTRL, 0);
		aml_write_vcbus(VPU_ARB_URG_CTRL, 0);
	}
}

void vdin_urgent_patch_resume_s5(unsigned int offset)
{
	/* urgent ctr config */
	wr(offset, VDIN_LFIFO_URG_CTRL, 0);
	aml_write_vcbus(VPU_ARB_URG_CTRL, 0);
}

static unsigned int vdin_is_support_10bit_for_dw_s5(struct vdin_dev_s *devp)
{
	if (devp->double_wr) {
		if (devp->double_wr_10bit_sup)
			return 1;
		else
			return 0;
	} else {
		return 1;
	}
}

/*static unsigned int vdin_wr_mode = 0xff;*/
/*module_param(vdin_wr_mode, uint, 0644);*/
/*MODULE_PARM_DESC(vdin_wr_mode, "vdin_wr_mode");*/

/* set write ctrl regs:
 * VDIN_WR_H_START_END
 * VDIN_WR_V_START_END
 * VDIN_WR_CTRL
 * VDIN_LFIFO_URG_CTRL
 */
static inline void vdin_set_wr_ctrl_s5(struct vdin_dev_s *devp,
				    unsigned int offset, unsigned int v,
				    unsigned int h,
				    enum vdin_format_convert_e format_convert,
				    unsigned int full_pack,
				    unsigned int source_bitdepth)
{
	enum vdin_mif_fmt write_fmt = MIF_FMT_YUV422;
	unsigned int swap_cbcr = 0;
	unsigned int hconv_mode = 2;

	if (devp->vf_mem_size_small) {
		h = devp->h_shrink_out;
		v = devp->v_shrink_out;
	}

	pr_info("%s,%d.h=%d,v=%d\n", __func__, __LINE__, h, v);
	switch (format_convert)	{
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		write_fmt = MIF_FMT_YUV422;
		break;

	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		write_fmt = MIF_FMT_NV12_21;
		swap_cbcr = 1;
		break;

	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		write_fmt = MIF_FMT_NV12_21;
		swap_cbcr = 0;
		break;

	default:
		write_fmt = MIF_FMT_YUV444;
		break;
	}

	/* yuv422 full pack mode for 10bit
	 * only support 8bit at vpp side when double write
	 */
	if ((format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422 ||
	     format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422 ||
	     format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422 ||
	     format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422) &&
	    full_pack == VDIN_422_FULL_PK_EN &&
	    source_bitdepth > VDIN_COLOR_DEEPS_8BIT &&
	    vdin_is_support_10bit_for_dw_s5(devp)) {
		write_fmt = MIF_FMT_YUV422_FULL_PACK;

		/* IC bug, fixed at tm2 revB */
		if (devp->dtdata->hw_ver == VDIN_HW_ORG)
			hconv_mode = 0;
	}

	/* win_he */
	if ((h % 2) && devp->source_bitdepth > VDIN_COLOR_DEEPS_8BIT &&
	    devp->full_pack == VDIN_422_FULL_PK_EN &&
	    (devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422 ||
	     devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422 ||
	     devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422 ||
	     devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422))
		h += 1;

	wr(offset, VDIN_DW_TOP_IN_SIZE,
		devp->s5_data.h_scale_out << 16 | devp->s5_data.v_scale_out << 0);
	wr(offset, VDIN_DW_TOP_OUT_SIZE, devp->h_shrink_out << 16 | devp->v_shrink_out << 0);
	wr_bits(offset, VDIN_WRMIF_H_START_END, (devp->h_shrink_out - 1), WR_HEND_BIT, WR_HEND_WID);
	/* win_ve */
	wr_bits(offset, VDIN_WRMIF_V_START_END, (devp->v_shrink_out - 1), WR_VEND_BIT, WR_VEND_WID);
	/* hconv_mode */
	wr_bits(offset, VDIN_WRMIF_CTRL, hconv_mode, 26, 2);
	/* vconv_mode */
	/* output even lines's cbcr */
	wr_bits(offset, VDIN_WRMIF_CTRL, 0, 28, 2);

	if (write_fmt == MIF_FMT_NV12_21) {
		/* swap_cbcr */
		wr_bits(offset, VDIN_WRMIF_CTRL, swap_cbcr, 30, 1);
	} else if (write_fmt == MIF_FMT_YUV444) {
		/* output all cbcr */
		wr_bits(offset, VDIN_WRMIF_CTRL, 3, 28, 2);
	} else {
		/* swap_cbcr */
		wr_bits(offset, VDIN_WRMIF_CTRL, 0, 30, 1);
		/* chroma canvas */
		/* wr_bits(offset, VDIN_WR_CTRL2, 0,
		 *  WRITE_CHROMA_CANVAS_ADDR_BIT,
		 */
		/* WRITE_CHROMA_CANVAS_ADDR_WID); */
	}

	/* format444 */
	wr_bits(offset, VDIN_WRMIF_CTRL, write_fmt, WR_FMT_BIT, WR_FMT_WID);
	/* req_urgent */
	wr_bits(offset, VDIN_WRMIF_CTRL, 1, WR_REQ_URGENT_BIT, WR_REQ_URGENT_WID);
	/* req_en */
	wr_bits(offset, VDIN_WRMIF_CTRL, 0, WR_REQ_EN_BIT, WR_REQ_EN_WID);

	/*only for vdin0*/
	if (devp->dts_config.urgent_en && devp->index == 0)
		vdin_urgent_patch_s5(offset, v, h);
	/* dis ctrl reg w pulse */
	/*if (is_meson_g9tv_cpu() || is_meson_m8_cpu() ||
	 *	is_meson_m8m2_cpu() || is_meson_gxbb_cpu() ||
	 *	is_meson_m8b_cpu())
	 */
	//wr_bits(offset, VDIN_WR_CTRL, 1,
	//	VDIN_WR_CTRL_REG_PAUSE_BIT, VDIN_WR_CTRL_REG_PAUSE_WID);
	/*  swap the 2 64bits word in 128 words */
	/*if (is_meson_gxbb_cpu())*/
	if (devp->set_canvas_manual == 1 || devp->cfg_dma_buf ||
		devp->work_mode == VDIN_WORK_MD_V4L || devp->dbg_no_swap_en) {
		/*not swap 2 64bits words in 128 words */
		wr_bits(offset, VDIN_WRMIF_CTRL, 0, 31, 1);
		/*little endian*/
		wr_bits(offset, VDIN_WRMIF_CTRL2, 1, 25, 1);
	} else {
		wr_bits(offset, VDIN_WRMIF_CTRL, 1, 31, 1);
	}
	wr_bits(offset, VDIN_WRMIF_CTRL, 1, 19, 1);
}

void vdin_set_wr_ctrl_vsync_s5(struct vdin_dev_s *devp,
			    unsigned int offset,
			    enum vdin_format_convert_e format_convert,
			    unsigned int full_pack,
			    unsigned int source_bitdepth,
			    unsigned int rdma_enable)
{
	enum vdin_mif_fmt write_fmt = MIF_FMT_YUV422;
	unsigned int swap_cbcr = 0;
	unsigned int hconv_mode = 2, vconv_mode;

	switch (format_convert)	{
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		write_fmt = MIF_FMT_YUV422;
		break;

	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		write_fmt = MIF_FMT_NV12_21;
		swap_cbcr = 1;

		break;
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		write_fmt = MIF_FMT_NV12_21;
		swap_cbcr = 0;
		break;

	default:
		write_fmt = MIF_FMT_YUV444;
		break;
	}

	/* yuv422 full pack mode for 10bit
	 * only support 8bit at vpp side when double write
	 */
	if ((format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422 ||
	     format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422 ||
	     format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422 ||
	     format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422) &&
	    full_pack == VDIN_422_FULL_PK_EN && source_bitdepth > 8 &&
	    vdin_is_support_10bit_for_dw_s5(devp)) {
		write_fmt = MIF_FMT_YUV422_FULL_PACK;

		/* IC bug, fixed at tm2 revB */
		if (devp->dtdata->hw_ver == VDIN_HW_ORG)
			hconv_mode = 0;
	}

	/* vconv_mode */
	vconv_mode = 0;

	if (write_fmt == MIF_FMT_NV12_21)
		vconv_mode = 0;
	else if (write_fmt == MIF_FMT_YUV444)
		vconv_mode = 3;
	else
		swap_cbcr = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable) {
		rdma_write_reg_bits(devp->rdma_handle,
				    VDIN_WR_CTRL + devp->addr_offset,
				    hconv_mode, HCONV_MODE_BIT, HCONV_MODE_WID);
		rdma_write_reg_bits(devp->rdma_handle,
				    VDIN_WR_CTRL + devp->addr_offset,
				    vconv_mode, VCONV_MODE_BIT, VCONV_MODE_WID);
		rdma_write_reg_bits(devp->rdma_handle,
				    VDIN_WR_CTRL + devp->addr_offset,
				    swap_cbcr, SWAP_CBCR_BIT, SWAP_CBCR_WID);
		rdma_write_reg_bits(devp->rdma_handle,
				    VDIN_WR_CTRL + devp->addr_offset,
				    write_fmt, WR_FMT_BIT, WR_FMT_WID);
	} else {
#endif
		wr_bits(offset, VDIN_WR_CTRL, hconv_mode,
			HCONV_MODE_BIT, HCONV_MODE_WID);
		wr_bits(offset, VDIN_WR_CTRL, vconv_mode,
			VCONV_MODE_BIT, VCONV_MODE_WID);
		wr_bits(offset, VDIN_WR_CTRL, swap_cbcr,
			SWAP_CBCR_BIT, SWAP_CBCR_WID);
		wr_bits(offset, VDIN_WR_CTRL, write_fmt,
			WR_FMT_BIT, WR_FMT_WID);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	}
#endif
}

/* set vdin_wr_mif for video only */
void vdin_set_wr_mif_s5(struct vdin_dev_s *devp)
{
//	int height, width;
//	static unsigned int temp_height;
//	static unsigned int temp_width;
//
//	height = ((rd(0, VPP_POSTBLEND_VD1_V_START_END) & 0xfff) -
//		((rd(0, VPP_POSTBLEND_VD1_V_START_END) >> 16) & 0xfff) + 1);
//	width = ((rd(0, VPP_POSTBLEND_VD1_H_START_END) & 0xfff) -
//		((rd(0, VPP_POSTBLEND_VD1_H_START_END) >> 16) & 0xfff) + 1);
//	if (devp->parm.port == TVIN_PORT_VIU1_VIDEO &&
//	    devp->index == 1 &&
//	    height != temp_height &&
//	    width != temp_width) {
//		if ((width % 2) &&
//		    devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH &&
//		    (devp->full_pack == VDIN_422_FULL_PK_EN) &&
//		    (devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422 ||
//		    devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422 ||
//		    devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422 ||
//		    devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422))
//			width += 1;
//
//		wr_bits(devp->addr_offset, VDIN_WR_H_START_END,
//			(width - 1), WR_HEND_BIT, WR_HEND_WID);
//		wr_bits(devp->addr_offset, VDIN_WR_V_START_END,
//			(height - 1), WR_VEND_BIT, WR_VEND_WID);
//		temp_height = height;
//		temp_width = width;
//	}
}

void vdin_set_mif_on_off_s5(struct vdin_dev_s *devp, unsigned int rdma_enable)
{
	unsigned int offset = devp->addr_offset;

	if (devp->vframe_wr_en_pre == devp->vframe_wr_en)
		return;

	#if CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable) {
		rdma_write_reg_bits(devp->rdma_handle, VDIN_PP_TOP_CTRL + offset,
			devp->vframe_wr_en ? 0 : 1, 21, 1);
		rdma_write_reg_bits(devp->rdma_handle, VDIN_WRMIF_CTRL + offset,
				    devp->vframe_wr_en, WR_REQ_EN_BIT,
				    WR_REQ_EN_WID);
	}
	#endif
	devp->vframe_wr_en_pre = devp->vframe_wr_en;
}

/***************************global function**********************************/
unsigned int vdin_get_meas_hcnt64_s5(unsigned int offset)
{
	return rd_bits(offset, VDIN_MEAS_HS_COUNT,
		       MEAS_HS_CNT_BIT, MEAS_HS_CNT_WID);
}

unsigned int vdin_get_meas_vstamp_s5(unsigned int offset)
{
	return rd(offset, VDIN_MEAS_VS_COUNT_LO);
}

unsigned int vdin_get_active_h_s5(unsigned int offset)
{
	return rd_bits(offset, VDIN_IF_TOP_VDIN1_ACTIVE_MAX_PIX_CNT_STATUS,
		       ACTIVE_MAX_PIX_CNT_SDW_BIT, ACTIVE_MAX_PIX_CNT_SDW_WID);
}

unsigned int vdin_get_active_v_s5(unsigned int offset)
{
	return rd_bits(offset, VDIN_IF_TOP_VDIN1_LINE_CNT_SHADOW_STATUS,
		       ACTIVE_LN_CNT_SDW_BIT, ACTIVE_LN_CNT_SDW_WID);
}

unsigned int vdin_get_total_v_s5(unsigned int offset)
{
	return rd_bits(offset, VDIN_IF_TOP_VDIN1_LINE_CNT_SHADOW_STATUS,
		       GO_LN_CNT_SDW_BIT, GO_LN_CNT_SDW_WID);
}

void vdin_set_frame_mif_write_addr_s5(struct vdin_dev_s *devp,
			unsigned int rdma_enable,
			struct vf_entry *vfe)
{
	u32 stride_luma, stride_chroma = 0;
	u32 hsize;
	u32 phy_addr_luma = 0, phy_addr_chroma = 0;

	if (devp->vf_mem_size_small)
		hsize = devp->h_shrink_out;
	else
		hsize = devp->h_active;

	stride_luma = devp->canvas_w >> 4;
	if (vfe->vf.plane_num == 2)
		stride_chroma = devp->canvas_w >> 4;

	/* one region mode only have one buffer RGB,YUV */
	phy_addr_luma = vfe->vf.canvas0_config[0].phy_addr;
	/* two region mode have Y and uv two buffer(NV12, NV21) */
	if (vfe->vf.plane_num == 2)
		phy_addr_chroma = vfe->vf.canvas0_config[1].phy_addr;

	if (vdin_isr_monitor & VDIN_ISR_MONITOR_BUFFER) {
		pr_info("vdin%d,phy addr luma:0x%x chroma:0x%x\n", devp->index,
			phy_addr_luma, phy_addr_chroma);
		pr_info("vf[%d],stride luma:0x%x, chroma:0x%x\n", vfe->vf.index,
			stride_luma, stride_chroma);
	}
	/*if (vdin_ctl_dbg) {*/
	/*	pr_info("mif fmt:0x%x (0:422,1:444,2:NV21) bit:0x%x h:%d\n",*/
	/*		devp->mif_fmt, devp->source_bitdepth, hsize);*/
	/*	pr_info("phy addr luma:0x%x chroma:0x%x\n",*/
	/*		phy_addr_luma, phy_addr_chroma);*/
	/*	pr_info("stride luma:0x%x, chroma:0x%x\n",*/
	/*		stride_luma, stride_chroma);*/
	/*}*/

	if (rdma_enable) {
		rdma_write_reg(devp->rdma_handle,
			       VDIN_WRMIF_BADDR_LUMA + devp->addr_offset,
			       phy_addr_luma >> 4);
		rdma_write_reg(devp->rdma_handle,
			       VDIN_WRMIF_STRIDE_LUMA + devp->addr_offset,
			       stride_luma);

		if (vfe->vf.plane_num == 2) {
			rdma_write_reg(devp->rdma_handle,
				       VDIN_WRMIF_BADDR_CHROMA + devp->addr_offset,
				       phy_addr_chroma >> 4);
			rdma_write_reg(devp->rdma_handle,
				       VDIN_WRMIF_STRIDE_CHROMA + devp->addr_offset,
				       stride_chroma);
		}
		if (devp->pause_dec || devp->msct_top.sct_pause_dec)
			rdma_write_reg_bits(devp->rdma_handle,
				VDIN_WRMIF_CTRL + devp->addr_offset,
				0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		else
			rdma_write_reg_bits(devp->rdma_handle,
				VDIN_WRMIF_CTRL + devp->addr_offset,
				1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
	} else {
		wr(devp->addr_offset, VDIN_WRMIF_BADDR_LUMA, phy_addr_luma >> 4);
		wr(devp->addr_offset, VDIN_WRMIF_STRIDE_LUMA, stride_luma);

		if (vfe->vf.plane_num == 2) {
			wr(devp->addr_offset, VDIN_WRMIF_BADDR_CHROMA,
			   phy_addr_chroma >> 4);
			wr(devp->addr_offset, VDIN_WRMIF_STRIDE_CHROMA,
			   stride_chroma);
		}
		if (devp->pause_dec || devp->msct_top.sct_pause_dec)
			wr_bits(devp->addr_offset, VDIN_WRMIF_CTRL,
				0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		else
			wr_bits(devp->addr_offset, VDIN_WRMIF_CTRL,
				1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
	}
}

void vdin_set_chroma_canvas_id_s5(struct vdin_dev_s *devp, unsigned int rdma_enable,
		struct vf_entry *vfe)
{
	u32 canvas_id;

	if (!vfe)
		return;

	if (vfe->vf.canvas0Addr != (u32)-1)
		canvas_id = vfe->vf.canvas0Addr;
	else
		canvas_id =
		devp->vf_canvas_id[vfe->vf.index];
	canvas_id = (canvas_id >> 8);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable)
		rdma_write_reg_bits(devp->rdma_handle,
				    VDIN_WRMIF_CTRL2 + devp->addr_offset,
				    canvas_id, WRITE_CHROMA_CANVAS_ADDR_BIT,
				    WRITE_CHROMA_CANVAS_ADDR_WID);
	else
#endif
		wr_bits(devp->addr_offset, VDIN_WRMIF_CTRL2, canvas_id,
			WRITE_CHROMA_CANVAS_ADDR_BIT,
			WRITE_CHROMA_CANVAS_ADDR_WID);
}

void vdin_set_canvas_id_s5(struct vdin_dev_s *devp, unsigned int rdma_enable,
			struct vf_entry *vfe)
{
	u32 canvas_id, vf_idx = -1;

	if (vfe) {
		if (vfe->vf.canvas0Addr != (u32)-1)
			canvas_id = vfe->vf.canvas0Addr;
		else
			canvas_id = devp->vf_canvas_id[vfe->vf.index];
		vf_idx = vfe->vf.index;
	} else {
		canvas_id = vdin_canvas_ids[devp->index][irq_max_count];
	}
	canvas_id = (canvas_id & 0xff);
	if (vdin_isr_monitor & VDIN_ISR_MONITOR_BUFFER) {
		pr_info("vdin%d,vf[%d],canvas_id:%d\n", devp->index,
			vf_idx, canvas_id);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable) {
		rdma_write_reg_bits(devp->rdma_handle,
				    VDIN_WRMIF_CTRL + devp->addr_offset,
				    canvas_id, WR_CANVAS_BIT, WR_CANVAS_WID);

		if (devp->pause_dec || devp->msct_top.sct_pause_dec)
			rdma_write_reg_bits(devp->rdma_handle, VDIN_WRMIF_CTRL + devp->addr_offset,
					    0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		else
			rdma_write_reg_bits(devp->rdma_handle, VDIN_WRMIF_CTRL + devp->addr_offset,
					    1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
	} else {
#endif
		wr_bits(devp->addr_offset, VDIN_WRMIF_CTRL, canvas_id,
			WR_CANVAS_BIT, WR_CANVAS_WID);

		if (devp->pause_dec || devp->msct_top.sct_pause_dec)
			wr_bits(devp->addr_offset, VDIN_WRMIF_CTRL, 0,
				WR_REQ_EN_BIT, WR_REQ_EN_WID);
		else
			wr_bits(devp->addr_offset, VDIN_WRMIF_CTRL, 1,
				WR_REQ_EN_BIT, WR_REQ_EN_WID);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	}
#endif
}

void vdin_pause_mif_write_s5(struct vdin_dev_s *devp, unsigned int rdma_enable)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable)
		rdma_write_reg_bits(devp->rdma_handle, VDIN_WR_CTRL + devp->addr_offset,
				    0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
	else
#endif
		wr_bits(devp->addr_offset, VDIN_WR_CTRL, 0,
			WR_REQ_EN_BIT, WR_REQ_EN_WID);
}

void vdin_set_crc_pulse_s5(struct vdin_dev_s *devp)
{
//#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
//	if (devp->flags & VDIN_FLAG_RDMA_ENABLE) {
//		rdma_write_reg(devp->rdma_handle,
//			       VDIN_CRC_CHK + devp->addr_offset, 1);
//	} else {
//#endif
//		wr(devp->addr_offset, VDIN_CRC_CHK, 1);
//#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
//	}
//#endif
}

/*set local dimming*/
#ifdef CONFIG_AML_LOCAL_DIMMING

#define VDIN_LDIM_PIC_ROWMAX 1080
#define VDIN_LDIM_PIC_COLMAX 1920
#define VDIN_LDIM_MAX_HIDX 4095
#define VDIN_LDIM_BLK_VNUM 2
#define VDIN_LDIM_BLK_HNUM 8

static void vdin_set_ldim_max_init_s5(unsigned int offset,
				   int pic_h, int pic_v, int blk_vnum,
				   int blk_hnum)
{
	int k;
	struct ldim_max_s ldim_max;
	int ldim_pic_rowmax = VDIN_LDIM_PIC_ROWMAX;
	int ldim_pic_colmax = VDIN_LDIM_PIC_COLMAX;
	int ldim_blk_vnum = VDIN_LDIM_BLK_VNUM;
	int ldim_blk_hnum = VDIN_LDIM_BLK_HNUM;

	ldim_pic_rowmax = pic_v;
	ldim_pic_colmax = pic_h;
	ldim_blk_vnum = blk_vnum; /* 8; */
	ldim_blk_hnum = blk_hnum; /* 2; */
	ldim_max.ld_stamax_hidx[0] = 0;

	/* check ic type */
	if (!is_meson_gxtvbb_cpu())
		return;
	if (vdin_ctl_dbg)
		pr_info("\n****************%s:hidx start********\n", __func__);
	for (k = 1; k < 11; k++) {
		ldim_max.ld_stamax_hidx[k] =
			((ldim_pic_colmax + ldim_blk_hnum - 1) /
			ldim_blk_hnum) * k;
		if (ldim_max.ld_stamax_hidx[k] > VDIN_LDIM_MAX_HIDX)
			/* clip U12 */
			ldim_max.ld_stamax_hidx[k] = VDIN_LDIM_MAX_HIDX;
		if (ldim_max.ld_stamax_hidx[k] == ldim_pic_colmax)
			ldim_max.ld_stamax_hidx[k] = ldim_pic_colmax - 1;
		if (vdin_ctl_dbg)
			pr_info("%d\t", ldim_max.ld_stamax_hidx[k]);
	}
	if (vdin_ctl_dbg)
		pr_info("\n****************%s:hidx end*********\n", __func__);
	ldim_max.ld_stamax_vidx[0] = 0;
	if (vdin_ctl_dbg)
		pr_info("\n***********%s:vidx start************\n", __func__);
	for (k = 1; k < 11; k++) {
		ldim_max.ld_stamax_vidx[k] = ((ldim_pic_rowmax +
					      ldim_blk_vnum - 1) /
					     ldim_blk_vnum) * k;
		if (ldim_max.ld_stamax_vidx[k] > VDIN_LDIM_MAX_HIDX)
			/* clip to U12 */
			ldim_max.ld_stamax_vidx[k] = VDIN_LDIM_MAX_HIDX;
		if (ldim_max.ld_stamax_vidx[k] == ldim_pic_rowmax)
			ldim_max.ld_stamax_vidx[k] = ldim_pic_rowmax - 1;
		if (vdin_ctl_dbg)
			pr_info("%d\t", ldim_max.ld_stamax_vidx[k]);
	}
	if (vdin_ctl_dbg)
		pr_info("\n*******%s:vidx end*******\n", __func__);
	wr(offset, VDIN_LDIM_STTS_HIST_REGION_IDX,
	   (1 << LOCAL_DIM_STATISTIC_EN_BIT)  |
	   (0 << EOL_EN_BIT)                  |
	   (2 << VLINE_OVERLAP_NUMBER_BIT)    |
	   (1 << HLINE_OVERLAP_NUMBER_BIT)    |
	   (1 << LPF_BEFORE_STATISTIC_EN_BIT) |
	   (1 << REGION_RD_INDEX_INC_BIT));
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 0,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_vidx[0] << 16 | ldim_max.ld_stamax_hidx[0]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 1,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_hidx[2] << 16 | ldim_max.ld_stamax_hidx[1]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 2,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_vidx[2] << 16 | ldim_max.ld_stamax_vidx[1]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 3,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_hidx[4] << 16 | ldim_max.ld_stamax_hidx[3]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 4,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_vidx[4] << 16 | ldim_max.ld_stamax_vidx[3]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 5,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_hidx[6] << 16 | ldim_max.ld_stamax_hidx[5]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 6,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_vidx[6] << 16 | ldim_max.ld_stamax_vidx[5]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 7,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_hidx[8] << 16 | ldim_max.ld_stamax_hidx[7]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 8,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_vidx[8] << 16 | ldim_max.ld_stamax_vidx[7]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 9,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_hidx[10] << 16 | ldim_max.ld_stamax_hidx[9]);
	wr_bits(offset, VDIN_LDIM_STTS_HIST_REGION_IDX, 10,
		BLK_HV_POS_IDX_BIT, BLK_HV_POS_IDX_WID);
	wr(offset, VDIN_LDIM_STTS_HIST_SET_REGION,
	   ldim_max.ld_stamax_vidx[10] << 16 | ldim_max.ld_stamax_vidx[9]);
	wr_bits(offset, VDIN_HIST_CTRL, 3, 9, 2);
	wr_bits(offset, VDIN_HIST_CTRL, 1, 8, 1);
}

#endif

//static unsigned int vdin_luma_max;

void vdin_set_vframe_prop_info_s5(struct vframe_s *vf,
			       struct vdin_dev_s *devp)
{
	int i;
	unsigned int offset = devp->addr_offset;
	u64 divid;
	struct vframe_bbar_s bbar = {0};
//
//#ifdef CONFIG_AML_LOCAL_DIMMING
//	/*int i;*/
//#endif
	/* fetch hist info */
	/* vf->prop.hist.luma_sum   = READ_CBUS_REG_BITS(VDIN_HIST_SPL_VAL,
	 * HIST_LUMA_SUM_BIT,    HIST_LUMA_SUM_WID   );
	 */
//	vf->prop.hist.hist_pow   = rd_bits(offset, VDIN_HIST_CTRL,
//					   HIST_POW_BIT, HIST_POW_WID);
	vf->prop.hist.luma_sum   = rd(offset, VDIN_LUMA_HIST_SPL_VAL);
	/* vf->prop.hist.chroma_sum = READ_CBUS_REG_BITS(VDIN_HIST_CHROMA_SUM,
	 * HIST_CHROMA_SUM_BIT,  HIST_CHROMA_SUM_WID );
	 */
	vf->prop.hist.chroma_sum = rd(offset, VDIN_LUMA_HIST_CHROMA_SUM);
	vf->prop.hist.pixel_sum  = rd(offset, VDIN_LUMA_HIST_SPL_CNT) & 0xffffff;
	vf->prop.hist.height     =
		rd_bits(offset, VDIN_LUMA_HIST_V_START_END, HIST_VEND_BIT, HIST_VEND_WID) -
		rd_bits(offset, VDIN_LUMA_HIST_V_START_END, HIST_VSTART_BIT, HIST_VSTART_WID) + 1;
	vf->prop.hist.width      =
		rd_bits(offset, VDIN_LUMA_HIST_H_START_END, HIST_HEND_BIT, HIST_HEND_WID) -
		rd_bits(offset, VDIN_LUMA_HIST_H_START_END, HIST_HSTART_BIT, HIST_HSTART_WID) + 1;
	vf->prop.hist.luma_max   = rd_bits(offset, VDIN_LUMA_HIST_MAX_MIN,
								HIST_MAX_BIT, HIST_MAX_WID);
	vf->prop.hist.luma_min   = rd_bits(offset, VDIN_LUMA_HIST_MAX_MIN,
								HIST_MIN_BIT, HIST_MIN_WID);
	for (i = 0; i < 64; i++) {
		wr(offset, VDIN_LUMA_HIST_DNLP_IDX, i);
		vf->prop.hist.gamma[i]	 = rd(offset, VDIN_LUMA_HIST_DNLP_GRP);
	}

	/* fetch bbar info */
	bbar.top = rd_bits(offset, VDIN_PP_BLKBAR_STATUS0,
			   BLKBAR_TOP_POS_BIT, BLKBAR_TOP_POS_WID);
	bbar.bottom = rd_bits(offset, VDIN_PP_BLKBAR_STATUS0,
			      BLKBAR_BTM_POS_BIT,   BLKBAR_BTM_POS_WID);
	bbar.left = rd_bits(offset, VDIN_PP_BLKBAR_STATUS1,
			    BLKBAR_LEFT_POS_BIT, BLKBAR_LEFT_POS_WID);
	bbar.right = rd_bits(offset, VDIN_PP_BLKBAR_STATUS1,
			     BLKBAR_RIGHT_POS_BIT, BLKBAR_RIGHT_POS_WID);
	if (bbar.top > bbar.bottom) {
		bbar.top = 0;
		bbar.bottom = vf->height - 1;
	}
	if (bbar.left > bbar.right) {
		bbar.left = 0;
		bbar.right = vf->width - 1;
	}

	/* Update Histgram windown with detected BlackBar window */
	if (devp->hist_bar_enable)
		vdin_set_histogram_s5(offset, 0, vf->width - 1, 0, vf->height - 1);
	else
		vdin_set_histogram_s5(offset, bbar.left, vf->width - 1 - bbar.right,
				   bbar.top, vf->height - 1 - bbar.bottom);

	if (devp->black_bar_enable) {
		vf->prop.bbar.top        = bbar.top;
		vf->prop.bbar.bottom     = bbar.bottom;
		vf->prop.bbar.left       = bbar.left;
		vf->prop.bbar.right      = bbar.right;
	} else {
		memset(&vf->prop.bbar, 0, sizeof(struct vframe_bbar_s));
	}

	/* fetch meas info - For M2 or further chips only, not for M1 chip */
	vf->prop.meas.vs_stamp = devp->stamp;
	vf->prop.meas.vs_cycle = devp->cycle;
	if ((vdin_ctl_dbg & CTL_DEBUG_LUMA_MAX) &&
	    vdin_luma_max != vf->prop.hist.luma_max) {
		vf->ready_clock_hist[0] = sched_clock();
		divid = vf->ready_clock_hist[0];
		do_div(divid, 1000);
		pr_info("vdin write done %lld us. lum_max(0x%x-->0x%x)\n",
			divid, vdin_luma_max, vf->prop.hist.luma_max);
		vdin_luma_max = vf->prop.hist.luma_max;
	}
}

void vdin_get_crc_val_s5(struct vframe_s *vf, struct vdin_dev_s *devp)
{
	/* fetch CRC value of the previous frame */
	vf->crc = rd(devp->addr_offset, VDIN_TOP_CRC1_OUT);
}

static inline ulong vdin_reg_limit_s5(ulong val, ulong wid)
{
	if (val < (1 << wid))
		return val;
	else
		return (1 << wid) - 1;
}

void vdin_set_all_regs_s5(struct vdin_dev_s *devp)
{
	/* matrix sub-module */
	vdin_set_matrix_s5(devp);

	/* bbar sub-module */
	//vdin_set_bbar_s5(devp->addr_offset, devp->v_active, devp->h_active);
#ifdef CONFIG_AML_LOCAL_DIMMING
	/* ldim sub-module */
	/* vdin_set_ldim_max_init(devp->addr_offset, 1920, 1080, 8, 2); */
	vdin_set_ldim_max_init_s5(devp->addr_offset, devp->h_active,
			       devp->v_active,
			       VDIN_LDIM_BLK_HNUM, VDIN_LDIM_BLK_VNUM);
#endif

	/* hist sub-module */
	//vdin_set_histogram_s5(devp->addr_offset, 0,
	//		   devp->h_active - 1, 0, devp->v_active - 1);
	/* hist mux select */
	vdin_set_hist_mux_s5(devp);
	/* write sub-module */
	//vdin_set_wr_ctrl_s5(devp, devp->addr_offset, devp->v_active,
	//		 devp->h_active, devp->format_convert,
	//		 devp->full_pack, devp->source_bitdepth);
	vdin_set_wr_ctrl_s5(devp, devp->addr_offset, devp->v_active,
			devp->h_active, devp->format_convert,
			devp->full_pack, devp->source_bitdepth);
	/* top sub-module */
	vdin_set_top_s5(devp, devp->parm.port,
		     devp->prop.color_format,
		     devp->bt_path);

	vdin_set_meas_mux_s5(devp->addr_offset, devp->parm.port,
			  devp->bt_path);

//	/* for t7 vdin2 write meta data */
//	if (devp->dtdata->hw_ver == VDIN_HW_T7) {
//		vdin_wrmif2_initial(devp);
//		vdin_wrmif2_addr_update(devp);
//		vdin_wrmif2_enable(devp, 0);
//	}
}

void vdin_set_dv_tunnel_s5(struct vdin_dev_s *devp)
{
}

static void vdin_delay_line_s5(struct vdin_dev_s *devp, unsigned short num)
{
	unsigned int offset;

	offset = devp->addr_offset;
	wr_bits(offset, VDIN_IF_TOP_VDIN1_SYNC_CTRL0, num,
		DLY_GO_FIELD_LINES_BIT, DLY_GO_FIELD_LINES_WID);
	if (num)
		wr_bits(offset, VDIN_IF_TOP_VDIN1_SYNC_CTRL0, 1,
			DLY_GO_FIELD_EN_BIT, DLY_GO_FIELD_EN_WID);
	else
		wr_bits(offset, VDIN_IF_TOP_VDIN1_SYNC_CTRL0, 0,
			DLY_GO_FIELD_EN_BIT, DLY_GO_FIELD_EN_WID);
}

void vdin_set_double_write_regs_s5(struct vdin_dev_s *devp)
{
//	if (devp->double_wr) {
//		if (devp->index == 0) {
//			/* vdin0 normal->afbce, small->mif0 */
//			wr_bits(0, VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN0_NOR,
//				AFBCE_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
//			wr_bits(0, VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN0_SML,
//				MIF0_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
//			/* enable both channel for double wr */
//			/*wr(0, VDIN_PP_LFIFO_CTRL, 0xc0060f00);*/
//			wr_bits(0, VDIN_PP_LFIFO_CTRL, 0x1, CH0_OUT_EN_BIT, 1);
//			wr_bits(0, VDIN_PP_LFIFO_CTRL, 0x1, CH1_OUT_EN_BIT, 1);
//			wr_bits(0, VDIN_PP_LFIFO_CTRL, 0x3, 30, 2);
//		}
//	}
}

void vdin_if_default_s5(struct vdin_dev_s *devp)
{
	unsigned int offset;

	offset = devp->addr_offset;

	wr(offset, VDIN_IF_TOP_MPEG_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI1_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI2_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI3_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI4_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI5_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI6_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI7_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI8_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI9_CTRL, 0x0);

	wr(offset, VDIN_IF_TOP_SIZE, 0x0);
	wr(offset, VDIN_IF_TOP_DECIMATE_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_BUF_CTRL, 0x0);

	wr(offset, VDIN_IF_TOP_MEAS_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_MEAS_FIRST_RANGE, 0x0);
	wr(offset, VDIN_IF_TOP_MEAS_SECOND_RANGE, 0x0);
	wr(offset, VDIN_IF_TOP_MEAS_THIRD_RANGE, 0x0);
	wr(offset, VDIN_IF_TOP_MEAS_FOURTH_RANGE, 0x0);
	wr(offset, VDIN_IF_TOP_PTGEN_CTRL, 0x0);

	if (devp->index == 0) {
		wr(offset, VDIN_IF_TOP_VDIN0_SECURE_CTRL, 0x0);
		wr(offset, VDIN_IF_TOP_VDIN0_CTRL, 0x0);
		wr(offset, VDIN_IF_TOP_VDIN0_SYNC_CTRL0, 0x1);
		wr(offset, VDIN_IF_TOP_VDIN0_SYNC_CTRL1, 0x4040);
		wr(offset, VDIN_IF_TOP_VDIN0_LINE_INT_CTRL, 0x0);
	} else {
		//wr(offset, VDIN_IF_TOP_VDIN1_SECURE_CTRL, 0x0);
		wr(offset, VDIN_IF_TOP_VDIN1_CTRL, 0x0);
		wr(offset, VDIN_IF_TOP_VDIN1_SYNC_CTRL0, 0x1);
		wr(offset, VDIN_IF_TOP_VDIN1_SYNC_CTRL1, 0x4040);
		wr(offset, VDIN_IF_TOP_VDIN1_LINE_INT_CTRL, 0x0);
	}
}

void vdin_pp_default_s5(struct vdin_dev_s *devp)
{
	unsigned int offset;
	unsigned int lfifo_buf_size;

	offset = devp->addr_offset;
	if (devp->index)
		lfifo_buf_size = devp->dtdata->vdin1_line_buff_size;
	else
		lfifo_buf_size = devp->dtdata->vdin0_line_buff_size;
	//wr(offset, VDIN_PP_TOP_GCLK_CTRL,	0x0);
	wr(offset, VDIN_PP_TOP_CTRL,		0x90000000);
	wr(offset, VDIN_PP_TOP_IN_SIZE,		0x0);
	wr(offset, VDIN_PP_TOP_OUT_SIZE,	0x0);
	wr(offset, VDIN_PP_TOP_H_WIN,		0x1fff);
	wr(offset, VDIN_PP_TOP_V_WIN,		0x1fff);
	wr(offset, VDIN_PP_LFIFO_CTRL,
		(0 << PP_LFIFO_GCLK_CTRL_BIT) |
		(1 << PP_LFIFO_SOFT_RST_BIT) |
		(lfifo_buf_size << PP_LFIFO_BUF_SIZE_BIT) |
		(0 << PP_LFIFO_URG_CTRL_BIT));
	//udelay(5);
	//wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0, PP_LFIFO_SOFT_RST_BIT, 1);
	//wrmif
	//wr_bits(offset, VDIN_WRMIF_STRIDE_LUMA, 1, 31, 1);
}

void vdin_dw_default_s5(struct vdin_dev_s *devp)
{
	unsigned int offset;

	offset = devp->addr_offset;
	//wr(offset, VDIN_DW_TOP_GCLK_CTRL,	0x0);
	wr(offset, VDIN_DW_TOP_CTRL,		0x0);
	wr(offset, VDIN_DW_TOP_IN_SIZE,		0x0);
	wr(offset, VDIN_DW_TOP_OUT_SIZE,	0x0);
}

void vdin_set_default_regmap_s5(struct vdin_dev_s *devp)
{
	vdin_if_default_s5(devp);
	vdin_pp_default_s5(devp);
	vdin_dw_default_s5(devp);

	vdin_delay_line_s5(devp, delay_line_num);
}

void vdin_hw_enable_s5(struct vdin_dev_s *devp)
{
	//unsigned int offset = devp->addr_offset;

	/* enable video data input */
	/* [    4]  top.datapath_en  = 1 */
	//wr_bits(offset, VDIN_COM_CTRL0, 1,
	//	VDIN_COMMON_INPUT_EN_BIT, VDIN_COMMON_INPUT_EN_WID);

	vdin_clk_on_off_s5(devp, true);
	/* wr(offset, VDIN_COM_GCLK_CTRL, 0x0); */
}

/*
 * 1) disable cm2
 * 2) disable video input and set mux input to null
 * 3) set delay line number
 * 4) disable vdin clk
 */
void vdin_hw_disable_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	//unsigned int def_canvas;
	//int temp;

	pr_info("%s: vdin[%d] VDIN_PP_TOP_CTRL:%x, offset:%d\n",
		__func__, devp->index, VDIN_PP_TOP_CTRL, offset);
	//rdma write may working now! clear req_en will not take effect
	/* req_en */
	wr_bits(offset, VDIN_WRMIF_CTRL, 0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
	/* req_en */
	wr_bits(offset, VDIN_IF_TOP_VDIN1_CTRL, 0, 0, 1); /* reg_vdin1_enable */
	//def_canvas = offset ? vdin_canvas_ids[1][0] : vdin_canvas_ids[0][0];
	/* disable cm2 */
	//wr(offset, VDIN_PP_TOP_CTRL, 0);
	//wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 0, CM_TOP_EN_BIT, CM_TOP_EN_WID);
	/* disable video data input */
	/* [    4]  top.datapath_en  = 0 */
	//wr_bits(offset, VDIN_COM_CTRL0, 0,
	//	VDIN_COMMON_INPUT_EN_BIT, VDIN_COMMON_INPUT_EN_WID);

	/* mux null input */
	/* [ 3: 0]  top.mux  = 0/(null, mpeg, 656, tvfe, cvd2, hdmi, dvin) */
	//wr_bits(offset, VDIN_COM_CTRL0, 0, VDIN_SEL_BIT, VDIN_SEL_WID);
	//wr(offset, VDIN_COM_CTRL0, 0x00000910);
	//vdin_delay_line_s5(devp, delay_line_num);
	//if (enable_reset)
	//	wr(offset, VDIN_WR_CTRL, 0x0b401000 | def_canvas);
	//else
	//	wr(offset, VDIN_WR_CTRL, 0x0bc01000 | def_canvas);

	//vdin_wrmif2_enable(devp, 0);

	/* disable clock of blackbar, histogram, histogram, line fifo1, matrix,
	 * hscaler, pre hscaler, clock0
	 */
	/* [15:14]  Disable blackbar clock      = 01/(auto, off, on, on) */
	/* [13:12]  Disable histogram clock     = 01/(auto, off, on, on) */
	/* [11:10]  Disable line fifo1 clock    = 01/(auto, off, on, on) */
	/* [ 9: 8]  Disable matrix clock        = 01/(auto, off, on, on) */
	/* [ 7: 6]  Disable hscaler clock       = 01/(auto, off, on, on) */
	/* [ 5: 4]  Disable pre hscaler clock   = 01/(auto, off, on, on) */
	/* [ 3: 2]  Disable clock0              = 01/(auto, off, on, on) */
	/* [    0]  Enable register clock       = 00/(auto, off!!!!!!!!) */
	/*switch_vpu_clk_gate_vmod(offset == 0 ? VPU_VIU_VDIN0:VPU_VIU_VDIN1,
	 *VPU_CLK_GATE_OFF);
	 */
	vdin_clk_on_off_s5(devp, false);
	/* wr(offset, VDIN_COM_GCLK_CTRL, 0x5554); */

	/*if (devp->dtdata->de_tunnel_tunnel) */{
		//todo vdin_dolby_desc_to_4448bit(devp, 0);
		//todo vdin_dolby_de_tunnel_to_44410bit(devp, 0);
	}
}

/* get current vsync field type 0:top 1 bottom */
unsigned int vdin_get_field_type_s5(unsigned int offset)
{
	return rd_bits(offset, VDIN_COM_STATUS0, 0, 1);
}

bool vdin_check_vdi6_afifo_overflow_s5(unsigned int offset)
{
	return rd_bits(offset, VDIN_COM_STATUS2, 15, 1);
}

void vdin_clear_vdi6_afifo_overflow_flg_s5(unsigned int offset)
{
	wr_bits(offset, VDIN_ASFIFO_CTRL3, 0x1, 1, 1);
	wr_bits(offset, VDIN_ASFIFO_CTRL3, 0x0, 1, 1);
}

static unsigned int vdin_reset_flag;
inline int vdin_vsync_reset_mif_s5(int index)
{
	int i;
	int start_line = aml_read_vcbus(VDIN_LCNT_STATUS) & 0xfff;

	if (!enable_reset || vdin_reset_flag || start_line > 0)
		return 0;

	vdin_reset_flag = 1;
	if (index == 0) {
		/* vdin->vdin mif wr en */
		W_VCBUS_BIT(VDIN_WR_CTRL, 0, VCP_WR_EN_BIT, VCP_WR_EN_WID);
		W_VCBUS_BIT(VDIN_WR_CTRL, 1, NO_CLOCK_GATE_BIT,	NO_CLOCK_GATE_WID);
		/* wr req en */
		W_VCBUS_BIT(VDIN_WR_CTRL, 0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		aml_write_vcbus(VPU_WRARB_REQEN_SLV_L1C2, vpu_reg_27af &
						(~(1 << VDIN0_REQ_EN_BIT)));
		vpu_reg_27af &= (~VDIN0_REQ_EN_BIT);
		if (R_VCBUS_BIT(VPU_ARB_DBG_STAT_L1C2, VDIN_DET_IDLE_BIT,
				VDIN_DET_IDLE_WIDTH) & VDIN0_IDLE_MASK) {
			vdin0_wr_mif_reset_s5();
		} else {
			for (i = 0; i < vdin_det_idle_wait; i++) {
				if (R_VCBUS_BIT(VPU_ARB_DBG_STAT_L1C2,
						VDIN_DET_IDLE_BIT,
						VDIN_DET_IDLE_WIDTH) &
				    VDIN0_IDLE_MASK) {
					vdin0_wr_mif_reset_s5();
					break;
				}
			}
			if (i >= vdin_det_idle_wait && vdin_ctl_dbg)
				pr_info("============== !!! idle wait timeout\n");
		}

		aml_write_vcbus(VPU_WRARB_REQEN_SLV_L1C2,
				vpu_reg_27af | (1 << VDIN0_REQ_EN_BIT));

		W_VCBUS_BIT(VDIN_WR_CTRL, 1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		W_VCBUS_BIT(VDIN_WR_CTRL, 0, NO_CLOCK_GATE_BIT,	NO_CLOCK_GATE_WID);
		W_VCBUS_BIT(VDIN_WR_CTRL, 1, VCP_WR_EN_BIT, VCP_WR_EN_WID);

		vpu_reg_27af |= VDIN0_REQ_EN_BIT;
	} else if (index == 1) {
		W_VCBUS_BIT(VDIN1_WR_CTRL2, 1, VDIN1_VCP_WR_EN_BIT,
			    VDIN1_VCP_WR_EN_WID); /* vdin->vdin mif wr en */
		W_VCBUS_BIT(VDIN1_WR_CTRL, 1, VDIN1_DISABLE_CLOCK_GATE_BIT,
			    VDIN1_DISABLE_CLOCK_GATE_WID); /* clock gate */
		/* wr req en */
		W_VCBUS_BIT(VDIN1_WR_CTRL, 0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		aml_write_vcbus(VPU_WRARB_REQEN_SLV_L1C2,
				vpu_reg_27af & (~(1 << VDIN1_REQ_EN_BIT)));
		vpu_reg_27af &= (~(1 << VDIN1_REQ_EN_BIT));
		if (R_VCBUS_BIT(VPU_ARB_DBG_STAT_L1C2, VDIN_DET_IDLE_BIT,
				VDIN_DET_IDLE_WIDTH) & VDIN1_IDLE_MASK) {
			vdin1_wr_mif_reset_s5();
		} else {
			for (i = 0; i < vdin_det_idle_wait; i++) {
				if (R_VCBUS_BIT(VPU_ARB_DBG_STAT_L1C2,
						VDIN_DET_IDLE_BIT,
						VDIN_DET_IDLE_WIDTH) &
				    VDIN1_IDLE_MASK) {
					vdin1_wr_mif_reset_s5();
					break;
				}
			}
		}
		aml_write_vcbus(VPU_WRARB_REQEN_SLV_L1C2,
				vpu_reg_27af | (1 << VDIN1_REQ_EN_BIT));
		vpu_reg_27af |= (1 << VDIN1_REQ_EN_BIT);
		W_VCBUS_BIT(VDIN1_WR_CTRL, 1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		W_VCBUS_BIT(VDIN1_WR_CTRL, 0, VDIN1_DISABLE_CLOCK_GATE_BIT,
			    VDIN1_DISABLE_CLOCK_GATE_WID);
		W_VCBUS_BIT(VDIN1_WR_CTRL2, 0,
			    VDIN1_VCP_WR_EN_BIT, VDIN1_VCP_WR_EN_WID);
	}
	vdin_reset_flag = 0;

	return vsync_reset_mask & 0x08;
}

void vdin_enable_module_s5(struct vdin_dev_s *devp, bool enable)
{
	//unsigned int offset = devp->addr_offset;

	//vdin_dmc_ctrl(devp, 1);
	if (enable)	{
		/* set VDIN_MEAS_CLK_CNTL, select XTAL clock */
		/* if (is_meson_gxbb_cpu()) */
		/* ; */
		/* else */
		/* aml_write_cbus(HHI_VDIN_MEAS_CLK_CNTL, 0x00000100); */
		/* vdin_hw_enable_s5(devp); */
		/* todo: check them */
	} else {
		/* set VDIN_MEAS_CLK_CNTL, select XTAL clock */
		/* if (is_meson_gxbb_cpu()) */
		/* ; */
		/* else */
		/* aml_write_cbus(HHI_VDIN_MEAS_CLK_CNTL, 0x00000000); */
		vdin_hw_disable_s5(devp);
		//todo vdin_dobly_mdata_write_en(offset, false);
	}
}

bool vdin_write_done_check_s5(unsigned int offset, struct vdin_dev_s *devp)
{
	if (vdin_isr_monitor & VDIN_ISR_MONITOR_WRITE_DONE) {
		pr_info("vdin%d,[%#x]:%#x,[%#x]:%#x,[%#x]:%#x\n", devp->index,
			VDIN_LFIFO_BUF_COUNT, rd(0, VDIN_LFIFO_BUF_COUNT),
			VDIN_IF_TOP_VDI_INT_STATUS1, rd(0, VDIN_IF_TOP_VDI_INT_STATUS1),
			VDIN_WRMIF_RO_STATUS, rd(0, VDIN_WRMIF_RO_STATUS));
	}
	/*clear int status,reg_field_done_clr_bit */
	wr_bits(offset, VDIN_WRMIF_CTRL, 1, 18, 1);

//	/*clear int status*/
//	wr_bits(offset, VDIN_WR_CTRL, 1,
//			DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
//	wr_bits(offset, VDIN_WR_CTRL, 0,
//			DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
//
//	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
//		if (rd_bits(offset, VDIN_RO_WRMIF_STATUS,
//			    WRITE_DONE_BIT, WRITE_DONE_WID))
//			return true;
//
//	} else {
//		if (rd_bits(offset, VDIN_COM_STATUS0,
//			    DIRECT_DONE_STATUS_BIT, DIRECT_DONE_STATUS_WID))
//			return true;
//	}
//	devp->wr_done_abnormal_cnt++;
//	return false;
	return true;
}

void vdin_get_duration_by_fps_s5(struct vdin_dev_s *devp)
{
	if (devp->parm.info.fps && IS_HDMI_SRC(devp->parm.port))
		devp->duration = 96000 / (devp->parm.info.fps);
	else
		devp->duration = devp->fmt_info_p->duration;
}

/*
 * cycle = delta_stamp = ((1/fps)/(1/msr_clk))*(vsync_span + 1)
 * msr_clk/fps unit is HZ
 * vsync_span(0x125a[11:4]) usually should be 0;
 */
bool vdin_check_cycle_s5(struct vdin_dev_s *devp)
{
	bool ret = false;
	unsigned int stamp, cycle;

	stamp = vdin_get_meas_v_stamp(devp->addr_offset);

	if (stamp < devp->stamp)
		cycle = 0xffffffff - devp->stamp + stamp + 1;
	else
		cycle = stamp - devp->stamp;

	if (cycle <= (devp->msr_clk_val / 1000)) {
		ret = true;
	} else {
		devp->stamp = stamp;
		devp->cycle  = cycle;
		ret = false;
	}

	return ret;
}

/*function:calculate curr_wr_vf->duration
 * based on parm.port\ msr_clk_val\ parm.flag and
 * curr_field_type
 */
void vdin_calculate_duration_s5(struct vdin_dev_s *devp)
{
	unsigned int last_field_type, cycle_phase;
	struct vframe_s *curr_wr_vf = NULL;
	const struct tvin_format_s *fmt_info = devp->fmt_info_p;
	enum tvin_port_e port = devp->parm.port;
	unsigned int fps;

	curr_wr_vf = &devp->curr_wr_vfe->vf;
	last_field_type = devp->curr_field_type;
	cycle_phase = devp->msr_clk_val / 96000;
	if (cycle_phase == 0)
		cycle_phase = 250;

	/* dynamic duration update */
	if (devp->dtdata->hw_ver >= VDIN_HW_T7) {
		/* 59.94 for 1601 so need special handling */
		/* value is 834170 add 56 and minus 56 */
		if (devp->cycle > 834114 && devp->cycle < 834226) {
			devp->duration = 1601;
		} else if (devp->cycle) {
			fps = devp->cycle * 96;
			devp->duration = DIV_ROUND_CLOSEST(fps, (devp->msr_clk_val / 1000));
		} else {
			/* null branch */
		}
		curr_wr_vf->duration = devp->duration;
	} else {
#ifdef VDIN_DYNAMIC_DURATION
		devp->curr_wr_vf->duration =
			(devp->cycle + cycle_phase / 2) / cycle_phase;
#else
		if (devp->use_frame_rate == 1 &&
		    (port >= TVIN_PORT_HDMI0 && port <= TVIN_PORT_HDMI7)) {
			curr_wr_vf->duration =
				(devp->cycle + cycle_phase / 2) / cycle_phase;
		} else {
			curr_wr_vf->duration = devp->duration;
		}
#endif
		/* for 2D->3D mode & interlaced format, double top field
		 * duration to match software frame lock
		 */
#ifdef VDIN_DYNAMIC_DURATION
		if ((devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
		    (last_field_type & VIDTYPE_INTERLACE))
			curr_wr_vf->duration <<= 1;
#else
		if ((devp->parm.flag & TVIN_PARM_FLAG_2D_TO_3D) &&
		    (last_field_type & VIDTYPE_INTERLACE)) {
			if (!fmt_info->duration)
				curr_wr_vf->duration = ((devp->cycle << 1) +
							cycle_phase / 2) / cycle_phase;
			else
				curr_wr_vf->duration = fmt_info->duration << 1;
		}
#endif
	}
}

/* just for horizontal down scale src_w is origin width,
 * dst_w is width after scale down
 */
static void vdin_set_hscale_s5(struct vdin_dev_s *devp, unsigned int dst_w)
{
	unsigned int offset = devp->addr_offset;
	unsigned int src_w = devp->h_active;
	//unsigned int tmp;
	int horz_phase_step, i;

	u32 coef_lut[3][33] = {
		{ 0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300,
		  0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
		  0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
		  0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
		  0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
		  0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
		  0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
		  0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	      0xf84848f8 }, // bicubic
		{ 0x00800000, 0x007e0200, 0x007c0400, 0x007a0600,
		  0x00780800, 0x00760a00, 0x00740c00, 0x00720e00,
		  0x00701000, 0x006e1200, 0x006c1400, 0x006a1600,
		  0x00681800, 0x00661a00, 0x00641c00, 0x00621e00,
		  0x00602000, 0x005e2200, 0x005c2400, 0x005a2600,
		  0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,
		  0x00503000, 0x004e3200, 0x004c3400, 0x004a3600,
		  0x00483800, 0x00463a00, 0x00443c00, 0x00423e00,
		  0x00404000 }, // 2 point bilinear
		{ 0x80000000, 0x7e020000, 0x7c040000, 0x7a060000,
		  0x78080000, 0x760a0000, 0x740c0000, 0x720e0000,
		  0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
		  0x68180000, 0x661a0000, 0x641c0000, 0x621e0000,
		  0x60200000, 0x5e220000, 0x5c240000, 0x5a260000,
		  0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
		  0x50300000, 0x4e320000, 0x4c340000, 0x4a360000,
		  0x48380000, 0x463a0000, 0x443c0000, 0x423e0000,
		  0x40400000 }}; // 2 point bilinear, bank_length == 2

	if (!dst_w) {
		pr_err("[vdin..]%s parameter dst_w error.\n", __func__);
		return;
	}
	/* enable reg_hsc_gclk_ctrl */
	wr_bits(offset, VDIN_PP_TOP_GCLK_CTRL, 2, PP_HSC_GCLK_CTRL_BIT, PP_HSC_GCLK_CTRL_WID);
	/* disable hscale */
	wr_bits(offset, VDIN_PP_TOP_CTRL, 0, PP_HSC_EN_BIT, PP_HSC_EN_WID);
	/* write horz filter coefs */
	//wr_bits(offset, VDIN_HSC_COEF_IDX, 0x0100, 0, 7);
	wr(offset, VDIN_HSC_COEF_IDX, 0x0100);
	for (i = 0; i < 33; i++)
		wr(offset, VDIN_HSC_COEF, coef_lut[0][i]); /* bicubic */

	if (src_w >= 2048) {/* for src_w >= 4096, avoid data overflow. */
		horz_phase_step = ((src_w << 18) / dst_w) << 2;
		//horz_phase_step = (horz_phase_step << 6);
	} else {
		horz_phase_step = (src_w << 20) / dst_w;
		//horz_phase_step = (horz_phase_step << 4);
	}
	horz_phase_step = (horz_phase_step << 4);

	//wr(offset, VDIN_WIDTHM1I_WIDTHM1O, ((src_w - 1) << WIDTHM1I_BIT) |
	//		(dst_w  - 1));

	wr(offset, VDIN_PP_HSC_PHASE_STEP, horz_phase_step);
	wr(offset, VDIN_HSC_INI_PHASE, 0);
	wr(offset, VDIN_HSC_MISC,
		(1 << HSC_RPT_P0_NUM_BIT)  |
		(4 << HSC_INI_RCV_NUM_BIT));
	/* hsc_p0_num */
	//wr(offset, VDIN_HSC_INI_CTRL, (1 << HSCL_RPT_P0_NUM_BIT) |
	//   (4 << HSCL_INI_RCV_NUM_BIT) | /* hsc_ini_rcv_num */
	//   (0 << HSCL_INI_PHASE_BIT) /* hsc_ini_phase */
	//   );

	wr(offset, VDIN_HSC_CTRL,
	   (2 << PP_HSC_GCLK_CTL_BIT)           | /* gclk_ctrl */
	   (1 << PP_HSC_NEAREST_EN_BIT)         | /* nearest_en */
	   (1 << PP_HSC_SHORT_LINE_O_EN_BIT)    | /* short_line_o_en */
	   (0 << PP_HSC_SP422_MODE_BIT)         | /* sp422_mode */
	   (0 << PP_HSC_PHASE0_ALWASY_EN_BIT)   | /* phase0_always_en */
	   (4 << PP_HSC_BANK_LEN_BIT));           /* hsc_bank_length */
	/* enable hscale */
	wr_bits(offset, VDIN_PP_TOP_CTRL, 1, PP_HSC_EN_BIT, PP_HSC_EN_WID);

	devp->h_active = dst_w;
}

/*
 *just for vertical scale src_w is origin height,
 *just dst_h is the height after scale
 */
static void vdin_set_vscale_s5(struct vdin_dev_s *devp)
{
	int ver_phase_step, tmp;
	unsigned int offset = devp->addr_offset;
	unsigned int src_h = devp->v_active;
	unsigned int dst_h = devp->prop.scaling4h;

	if (!dst_h) {
		pr_err("[vdin..]%s parameter dst_h error.\n", __func__);
		return;
	}
	/* enable reg_hsc_gclk_ctrl */
	wr_bits(offset, VDIN_PP_TOP_GCLK_CTRL, 2, PP_VSC_GCLK_CTRL_BIT, PP_VSC_GCLK_CTRL_WID);
	/* disable vscale */
	wr_bits(offset, VDIN_PP_TOP_CTRL, 0, PP_VSC_EN_BIT, PP_VSC_EN_WID);

	if (src_h >= 2048)
		ver_phase_step = ((src_h << 18) / dst_h) << 2;
	else
		ver_phase_step = (src_h << 20) / dst_h;

	tmp = ver_phase_step >> 25;
	if (tmp) {
		pr_err("[vdin..]%s error. cannot be divided more than 31.9999.\n",
		       __func__);
		return;
	}
	wr(offset, VDIN_PP_VSC_PHASE_STEP, ver_phase_step);
	if (!(ver_phase_step >> 20)) {/* scale up the bit should be 1 */
		wr_bits(offset, VDIN_VSC_INI_CTRL, 1,
			VSC_PHASE0_ALWAYS_EN_BIT, VSC_PHASE0_ALWAYS_EN_WID);
		/* scale phase is 0 */
		wr_bits(offset, VDIN_VSC_INI_CTRL, 0,
			VSCALER_INI_PHASE_BIT, VSCALER_INI_PHASE_WID);
	} else {
		wr_bits(offset, VDIN_VSC_CTRL, 0,
			PP_VSC_PHASE0_ALWAS_EN_BIT, PP_VSC_PHASE0_ALWAS_EN_WID);
		wr_bits(offset, VDIN_VSC_CTRL, 1, 0, 1);/* reg_vsc_rpt_last_ln_mode */
		/* scale phase is 0x0 */
		wr_bits(offset, VDIN_VSC_INI_PHASE, 0x0,
			VSCALER_INI_PHASE_BIT, VSCALER_INI_PHASE_WID);
	}
	/* skip 0 line in the beginning */
	wr_bits(offset, VDIN_VSC_CTRL, 0,
		PP_VSC_SKIP_LINE_NUM_BIT, PP_VSC_SKIP_LINE_NUM_WID);

	//wr_bits(offset, VDIN_SCIN_HEIGHTM1, src_h - 1,
	//	SCALER_INPUT_HEIGHT_BIT, SCALER_INPUT_HEIGHT_WID);
	wr(offset, VDIN_VSC_DUMMY_DATA, 0x8080);
	/* enable vscale */
	wr_bits(offset, VDIN_PP_TOP_CTRL, 1, PP_VSC_EN_BIT, PP_VSC_EN_WID);
	devp->v_active = dst_h;
}

/* new add pre_hscale module
 * do hscaler down when scaling4w is smaller than half of h_active
 * which is closed by default
 */
static void vdin_set_pre_h_scale_s5(struct vdin_dev_s *devp)
{
	wr_bits(devp->addr_offset, VDIN_PHSC_CTRL, 0, PP_PHSC_MODE_BIT,
		PP_PHSC_MODE_WID);
	wr_bits(devp->addr_offset, VDIN_PP_TOP_CTRL, 1, PP_PHSC_EN_BIT,
		PP_PHSC_EN_WID);
	devp->h_active >>= 1;
	pr_info("set_prehsc done!h_active:%d\n", devp->h_active);
}

/*tm2 new add*/
static void vdin_set_h_shrink_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	unsigned int src_w = devp->h_active;
	unsigned int dst_w = devp->h_shrink_out;
	unsigned int hshrk_mode = 0;
	unsigned int i = 0;
	unsigned int coef = 0;

	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		pr_err("vdin.%d don't supports h_shrink before TM2\n",
		       devp->index);
		return;
	}

	hshrk_mode = src_w / dst_w;

	/*check maximum value*/
	if (hshrk_mode < 2 || hshrk_mode > 64) {
		pr_err("vdin.%d h_shrink: size is out of range\n", devp->index);
		return;
	}
	pr_err("%s vdin.%d h:%d,v:%d,src:%d %d;mode:%d\n", __func__,
		   devp->index, devp->h_active, devp->v_active,
		   src_w, dst_w, hshrk_mode);
	/*check legality, valid is 2,4,8,16,32,64*/
	switch (hshrk_mode) {
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
		break;

	default:
		pr_err("vdin.%d invalid h_shrink mode : %d\n", devp->index,
		       hshrk_mode);
		return;
	}
//int hsk_mode = (int)(ceil((float)(vdin->vdin_dw_top.in_hsize) /
//(float)(vdin->vdin_dw_top.out_hsize)));
	coef = 64 / hshrk_mode;
	coef = (coef << 24) | (coef << 16) | (coef << 8) | coef;

	/*hshrk_mode: 2->1/2, 4->1/4... 64->1/64(max)*/
	wr_bits(offset, VDIN_DW_HSK_CTRL, hshrk_mode, DW_HSK_MODE_BIT, DW_HSK_MODE_WID);
	//wr_bits(offset, VDIN_DW_HSK_CTRL, src_w, HSK_HSIZE_IN_BIT,
	//	HSK_HSIZE_IN_WID);

	/*h shrink coef*/
	for (i = VDIN_HSK_COEF_0; i <= VDIN_HSK_COEF_15; i++)
		wr(offset, i, coef);

	/*h shrink enable*/
	//wr_bits(offset, VDIN_DW_HSK_CTRL, 1, DW_HSK_EN_BIT, DW_HSK_EN_WID);
	wr_bits(offset, VDIN_DW_TOP_CTRL, 1, DW_HSK_EN_BIT, DW_HSK_EN_WID);

	devp->h_active = devp->h_shrink_out;
	pr_info("vdin.%d set_h_shrink done! h shrink mode = %d\n", devp->index,
		hshrk_mode);
}

/* new add v_shrink module
 * do vertical scale down,when scaling4h is smaller than half of v_active
 * which is closed by default
 * vshrk_mode:0-->1:2; 1-->1:4; 2-->1:8
 * chip <= TL1, only vdin1 has this module.
 * chip >= TM2 && != T5, both vdin0 and vdin1 are supported.
 * chip == T5, only vdin0 support
 */
static void vdin_set_v_shrink_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	unsigned int src_w = devp->h_shrink_out;
	unsigned int src_h = devp->v_active;
	unsigned int vshrk_mode = 0;
	int lpf_mode = 1;

	if (src_w > VDIN_V_SHRINK_H_LIMIT) {
		pr_err("vdin.%d v_shrink: only support input width <= %d\n",
		       devp->index, VDIN_V_SHRINK_H_LIMIT);
		return;
	}

	vshrk_mode = src_h / devp->v_shrink_out;

	pr_err("%s vdin.%d h:%d,v:%d,src:%d %d;mode:%d\n", __func__,
		   devp->index, devp->h_active, devp->v_active,
		   src_h, devp->v_shrink_out, vshrk_mode);

	if (vshrk_mode < 2) {
		pr_info("vdin.%d v shrink: out > in/2,return\n", devp->index);
		return;
	}

	if (vshrk_mode >= 8)
		vshrk_mode = 2;
	else if (vshrk_mode >= 4)
		vshrk_mode = 1;
	else//if (vshrk_mode >= 2)
		vshrk_mode = 0;
	// mode: 0 for 2:1, 1 for 4:1, 2 for 8:1

	pr_info("vdin.%d vshrk mode = %d\n", devp->index, vshrk_mode);

	wr(offset, VDIN_VSK_CTRL, (vshrk_mode << 2) | (lpf_mode << 1));
	wr(offset, VDIN_VSK_PTN_DATA0, 0x8080);
	wr(offset, VDIN_VSK_PTN_DATA1, 0x8080);
	wr(offset, VDIN_VSK_PTN_DATA2, 0x8080);
	wr(offset, VDIN_VSK_PTN_DATA3, 0x8080);
	wr_bits(offset, VDIN_DW_TOP_CTRL, 1, DW_VSK_EN_BIT, DW_VSK_EN_WID);
	devp->v_active = devp->v_shrink_out;
	pr_info("vdin.%d set_v_shrink done!\n", devp->index);
}

/*function:set horizontal and vertical scale
 *vdin scale down path:
 *	vdin0:pre hsc-->horizontal scaling-->v scaling;
 *	vdin1:pre hsc-->horizontal scaling-->v_shrink-->v scaling
 *for tm2, scaling path, both vdin are same:
 *	prehsc-->h scaling-->v scaling-->optional h/v shrink;
 */
void vdin_set_hv_scale_s5(struct vdin_dev_s *devp)
{
	/*backup current h v size*/
	devp->h_active_org = devp->h_active;
	devp->v_active_org = devp->v_active;
	devp->s5_data.h_scale_out = devp->h_active;
	devp->s5_data.v_scale_out = devp->v_active;

	if (K_FORCE_HV_SHRINK)
		goto set_hv_shrink;

	pr_info("vdin%d %s h_active:%u,v_active:%u.scaling:%dx%d\n", devp->index,
		__func__, devp->h_active, devp->v_active,
		devp->prop.scaling4w, devp->prop.scaling4h);

	if (devp->prop.scaling4w < devp->h_active &&
	    devp->prop.scaling4w > 0) {
		if (devp->pre_h_scale_en && (devp->prop.scaling4w <=
		    (devp->h_active >> 1)))
			vdin_set_pre_h_scale_s5(devp);

		if (devp->prop.scaling4w < devp->h_active)
			vdin_set_hscale_s5(devp, devp->prop.scaling4w);
	} else if (devp->h_active > VDIN_MAX_H_ACTIVE) {
		vdin_set_hscale_s5(devp, VDIN_MAX_H_ACTIVE);
	}

	if (devp->prop.scaling4h < devp->v_active &&
	    devp->prop.scaling4h > 0) {
		if (devp->v_shrink_en && !cpu_after_eq(MESON_CPU_MAJOR_ID_TM2) &&
		    (devp->prop.scaling4h <= (devp->v_active >> 1))) {
			vdin_set_v_shrink_s5(devp);
		}

		if (devp->prop.scaling4h < devp->v_active)
			vdin_set_vscale_s5(devp);
	}

set_hv_shrink:

	if ((devp->double_wr || K_FORCE_HV_SHRINK) &&
	    devp->h_active > 1920 && devp->v_active > 1080) {
		devp->h_shrink_times = H_SHRINK_TIMES_4k;
		devp->v_shrink_times = V_SHRINK_TIMES_4k;
	} else if (devp->double_wr && devp->h_active > 1280 &&
		   devp->v_active > 720) {
		devp->h_shrink_times = H_SHRINK_TIMES_1080;
		devp->v_shrink_times = V_SHRINK_TIMES_1080;
	} else {
		devp->h_shrink_times = 1;
		devp->v_shrink_times = 1;
	}
	if (devp->debug.dbg_force_shrink_en) {
		devp->h_shrink_times = H_SHRINK_TIMES_1080;
		devp->v_shrink_times = V_SHRINK_TIMES_1080;
	}
	devp->h_shrink_out = devp->h_active / devp->h_shrink_times;
	devp->v_shrink_out = devp->v_active / devp->v_shrink_times;

	if (devp->double_wr || K_FORCE_HV_SHRINK || devp->debug.dbg_force_shrink_en) {
		if (devp->h_shrink_out < devp->h_active)
			vdin_set_h_shrink_s5(devp);

		if (devp->v_shrink_out < devp->v_active)
			vdin_set_v_shrink_s5(devp);
	}

	if (vdin_dbg_en) {
		pr_info("[vdin.%d] %s h_active:%u,v_active:%u.\n", devp->index,
			__func__, devp->h_active, devp->v_active);
		pr_info("[vdin.%d] %s shrink out h:%d,v:%d\n", devp->index,
			__func__, devp->h_shrink_out, devp->v_shrink_out);
	}
}

/*set source_bitdepth
 *	base on color_depth_config:
 *		10, 8, 0, other
 */
void vdin_set_bitdepth_s5(struct vdin_dev_s *devp)
{
	//unsigned int offset = devp->addr_offset;
	unsigned int set_width = 0;
	unsigned int port;
	enum vdin_color_deeps_e bit_dep;
	unsigned int convert_fmt, offset;

	offset = devp->addr_offset;
	/* yuv 422 full pack check */
	if (devp->color_depth_support &
	    VDIN_WR_COLOR_DEPTH_10BIT_FULL_PACK_MODE)
		devp->full_pack = VDIN_422_FULL_PK_EN;
	else
		devp->full_pack = VDIN_422_FULL_PK_DIS;

	/*hw verify:de-tunnel 444 to 422 12bit*/
	//if (devp->dtdata->ipt444_to_422_12bit && vdin_cfg_444_to_422_wmif_en)
	//	devp->full_pack = VDIN_422_FULL_PK_DIS;

	if (devp->output_color_depth &&
	    (devp->prop.fps == 50 || devp->prop.fps == 60) &&
	    (devp->parm.info.fmt == TVIN_SIG_FMT_HDMI_3840_2160_00HZ ||
	     devp->parm.info.fmt == TVIN_SIG_FMT_HDMI_4096_2160_00HZ) &&
	    devp->prop.colordepth == VDIN_COLOR_DEEPS_10BIT) {
		set_width = devp->output_color_depth;
		pr_info("set output color depth %d bit from dts\n", set_width);
	}

	switch (devp->color_depth_config & 0xff) {
	case COLOR_DEEPS_8BIT:
		bit_dep = VDIN_COLOR_DEEPS_8BIT;
		break;
	case COLOR_DEEPS_10BIT:
		bit_dep = VDIN_COLOR_DEEPS_10BIT;
		break;
	/*
	 * vdin not support 12bit now, when rx submit is 12bit,
	 * vdin config it as 10bit , 12 to 10
	 */
	case COLOR_DEEPS_12BIT:
		bit_dep = VDIN_COLOR_DEEPS_10BIT;
		break;
	case COLOR_DEEPS_AUTO:
		/* vdin_bitdepth is set to 0 by default, in this case,
		 * devp->source_bitdepth is controlled by colordepth
		 * change default to 10bit for 8in8out detail maybe lost
		 */
		if (vdin_is_convert_to_444(devp->format_convert) &&
		    vdin_is_4k(devp)) {
			bit_dep = VDIN_COLOR_DEEPS_8BIT;
		} else if (devp->prop.colordepth == VDIN_COLOR_DEEPS_8BIT) {
			/* hdmi YUV422, 8 or 10 bit valid is unknown*/
			/* so need vdin 10bit to frame buffer*/
			port = devp->parm.port;
			if (port >= TVIN_PORT_HDMI0 && port <= TVIN_PORT_HDMI7) {
				convert_fmt = devp->format_convert;
				if ((vdin_is_convert_to_422(convert_fmt) &&
				     (devp->color_depth_support &
				      VDIN_WR_COLOR_DEPTH_10BIT)) ||
				    devp->prop.vdin_hdr_flag)
					bit_dep = VDIN_COLOR_DEEPS_10BIT;
				else
					bit_dep = VDIN_COLOR_DEEPS_8BIT;

				if (vdin_is_dolby_tunnel_444_input(devp))
					bit_dep = VDIN_COLOR_DEEPS_8BIT;
			} else {
				bit_dep = VDIN_COLOR_DEEPS_8BIT;
			}
		} else if ((devp->color_depth_support & VDIN_WR_COLOR_DEPTH_10BIT) &&
			   ((devp->prop.colordepth == VDIN_COLOR_DEEPS_10BIT) ||
			   (devp->prop.colordepth == VDIN_COLOR_DEEPS_12BIT))) {
			if (set_width == VDIN_COLOR_DEEPS_8BIT)
				bit_dep = VDIN_COLOR_DEEPS_8BIT;
			else
				bit_dep = VDIN_COLOR_DEEPS_10BIT;
		} else {
			bit_dep = VDIN_COLOR_DEEPS_8BIT;
		}

		break;
	default:
		bit_dep = VDIN_COLOR_DEEPS_8BIT;
		break;
	}

	if (devp->work_mode == VDIN_WORK_MD_V4L)
		bit_dep = VDIN_COLOR_DEEPS_8BIT;
	devp->source_bitdepth = bit_dep;
#ifdef VDIN_BRINGUP_BYPASS_COLOR_CNVT
	devp->source_bitdepth = devp->prop.colordepth;
#endif
	if (devp->source_bitdepth == VDIN_COLOR_DEEPS_8BIT)
		wr_bits(offset, VDIN_WRMIF_CTRL2, 0,
			VDIN_WR_10BIT_MODE_BIT, VDIN_WR_10BIT_MODE_WID);
	else if (devp->source_bitdepth == VDIN_COLOR_DEEPS_10BIT)
		wr_bits(offset, VDIN_WRMIF_CTRL2, 1,
			VDIN_WR_10BIT_MODE_BIT, VDIN_WR_10BIT_MODE_WID);

	/* only support 8bit mode at vpp side when double wr */
	if (!vdin_is_support_10bit_for_dw_s5(devp))
		wr_bits(offset, VDIN_WRMIF_CTRL2, MIF_8BIT,
			VDIN_WR_10BIT_MODE_BIT,	VDIN_WR_10BIT_MODE_WID);
	if (vdin_dbg_en)
		pr_info("%s %d cfg:0x%x prop.dep:%x\n", __func__, devp->source_bitdepth,
			devp->color_depth_config, devp->prop.colordepth);
}

static void vdin_get_video_format_s5(struct vdin_dev_s *devp)
{
	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		devp->vdin2vpp_info.cfmt = TVIN_YUV444;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
		devp->vdin2vpp_info.cfmt = TVIN_YUV422;
		break;
	case VDIN_FORMAT_CONVERT_YUV_RGB:
	case VDIN_FORMAT_CONVERT_RGB_RGB:
		devp->vdin2vpp_info.cfmt = TVIN_RGB444;
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		devp->vdin2vpp_info.cfmt = TVIN_NV21;
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		devp->vdin2vpp_info.cfmt = TVIN_NV12;
		break;
	default:
		break;
	}
}

void vdin_set_to_vpp_parm_s5(struct vdin_dev_s *devp)
{
	if (devp->index)
		return;

	devp->vdin2vpp_info.is_dv = devp->dv.dv_flag;
	devp->vdin2vpp_info.scan_mode = devp->fmt_info_p->scan_mode;
	devp->vdin2vpp_info.fps = devp->parm.info.fps;
	devp->vdin2vpp_info.width = devp->h_active;
	devp->vdin2vpp_info.height = devp->v_active;
	vdin_get_video_format_s5(devp);

	vdin_start_notify_vpp(&devp->vdin2vpp_info);
}

/* do horizontal reverse and vertical reverse
 * h reverse:
 * VDIN_WR_H_START_END
 * Bit29:	1.reverse	0.do not reverse
 *
 * v reverse:
 * VDIN_WR_V_START_END
 * Bit29:	1.reverse	0.do not reverse
 */
void vdin_wr_reverse_s5(unsigned int offset, bool h_reverse, bool v_reverse)
{
	if (h_reverse)
		wr_bits(offset, VDIN_WRMIF_CTRL2, 1, 21, 1);
	else
		wr_bits(offset, VDIN_WRMIF_CTRL2, 0, 21, 1);
	if (v_reverse)
		wr_bits(offset, VDIN_WRMIF_CTRL2, 1, 20, 1);
	else
		wr_bits(offset, VDIN_WRMIF_CTRL2, 0, 20, 1);
}

void vdin_bypass_isp_s5(unsigned int offset)
{
	wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 0, CM_TOP_EN_BIT, CM_TOP_EN_WID);
	wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		VDIN_MATRIX_EN_BIT, VDIN_MATRIX_EN_WID);
	wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		VDIN_MATRIX1_EN_BIT, VDIN_MATRIX1_EN_WID);
}

void vdin_set_cm2_s5(unsigned int offset, unsigned int w,
		  unsigned int h, unsigned int *cm2)
{
	unsigned int i = 0, j = 0, start_addr = 0x100;

	if (!cm_enable)
		return;
	pr_info("%s %d,w=%d,h=%d\n", __func__, __LINE__, w, h);

	//wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 0, CM_TOP_EN_BIT, CM_TOP_EN_WID);
	wr_bits(offset, VDIN_PP_TOP_CTRL, 0, PP_CM_EN_BIT, PP_CM_EN_WID);
	for (i = 0; i < 160; i++) {
		j = i / 5;
		wr(offset, VDIN_PP_CHROMA_ADDR_PORT, start_addr + (j << 3) + (i % 5));
		wr(offset, VDIN_PP_CHROMA_DATA_PORT, cm2[i]);
	}
	for (i = 0; i < 28; i++) {
		wr(offset, VDIN_PP_CHROMA_ADDR_PORT, 0x200 + i);
		wr(offset, VDIN_PP_CHROMA_DATA_PORT, cm2[160 + i]);
	}
	/*config cm2 frame size*/
	wr(offset, VDIN_PP_CHROMA_ADDR_PORT, 0x205);
	wr(offset, VDIN_PP_CHROMA_DATA_PORT, h << 16 | w);
	//wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 1, CM_TOP_EN_BIT, CM_TOP_EN_WID);
	wr_bits(offset, VDIN_PP_TOP_CTRL, 1, PP_CM_EN_BIT, PP_CM_EN_WID);
}

/*vdin0 output ctrl
 * Bit 26 vcp_nr_en. Only used in VDIN0. NOT used in VDIN1.
 * Bit 25 vcp_wr_en. Only used in VDIN0. NOT used in VDIN1.
 * Bit 24 vcp_in_en. Only used in VDIN0. NOT used in VDIN1.
 */
static void vdin_output_ctl_s5(unsigned int offset, unsigned int output_nr_flag)
{
	wr_bits(offset, VDIN_WR_CTRL, 1, VCP_IN_EN_BIT, VCP_IN_EN_WID);
	if (output_nr_flag) {
		wr_bits(offset, VDIN_WR_CTRL, 0, VCP_WR_EN_BIT, VCP_WR_EN_WID);
		wr_bits(offset, VDIN_WR_CTRL, 1, VCP_NR_EN_BIT, VCP_NR_EN_WID);
	} else {
		wr_bits(offset, VDIN_WR_CTRL, 1, VCP_WR_EN_BIT, VCP_WR_EN_WID);
		wr_bits(offset, VDIN_WR_CTRL, 0, VCP_NR_EN_BIT, VCP_NR_EN_WID);
	}
}

/* set mpegin
 * VDIN_COM_CTRL0
 * Bit 31,   mpeg_to_vdin_sel,
 * 0: mpeg source to NR directly,
 * 1: mpeg source pass through here
 */
void vdin_set_mpegin_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;

	/* set VDIN_MEAS_CLK_CNTL, select XTAL clock */
	/* if (is_meson_gxbb_cpu()) */
	/* ; */
	/* else */
	/* aml_write_cbus(HHI_VDIN_MEAS_CLK_CNTL, 0x00000100); */

	wr(offset, VDIN_COM_CTRL0, 0x80000911);
	vdin_delay_line_s5(devp, delay_line_num);
	wr(offset, VDIN_COM_GCLK_CTRL, 0x0);

	wr(offset, VDIN_INTF_WIDTHM1, devp->h_active - 1);
	wr(offset, VDIN_WR_CTRL2, 0x0);

	wr(offset, VDIN_HIST_CTRL, 0x3);
	wr(offset, VDIN_HIST_H_START_END, devp->h_active - 1);
	wr(offset, VDIN_HIST_V_START_END, devp->v_active - 1);
	vdin_output_ctl_s5(offset, 1);
}

void vdin_force_gofiled_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;

	wr_bits(offset, VDIN_COM_CTRL0, 1, 28, 1);
	wr_bits(offset, VDIN_COM_CTRL0, 0, 28, 1);
}

bool vdin_is_convert_to_444_s5(u32 format_convert)
{
	if (format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444 ||
	    format_convert == VDIN_FORMAT_CONVERT_YUV_RGB ||
	    format_convert == VDIN_FORMAT_CONVERT_YUV_GBR ||
	    format_convert == VDIN_FORMAT_CONVERT_YUV_BRG ||
	    format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444 ||
	    format_convert == VDIN_FORMAT_CONVERT_RGB_RGB)
		return true;
	else
		return false;
}

bool vdin_is_convert_to_422_s5(u32 format_convert)
{
	if (format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422 ||
	    format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422 ||
	    format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422 ||
	    format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422)
		return true;
	else
		return false;
}

bool vdin_is_convert_to_nv21_s5(u32 format_convert)
{
	if (format_convert == VDIN_FORMAT_CONVERT_YUV_NV12 ||
	    format_convert == VDIN_FORMAT_CONVERT_YUV_NV21 ||
	    format_convert == VDIN_FORMAT_CONVERT_RGB_NV12 ||
	    format_convert == VDIN_FORMAT_CONVERT_RGB_NV21)
		return true;
	else
		return false;
}

bool vdin_is_4k_s5(struct vdin_dev_s *devp)
{
	if (devp->h_active >= 3800 && devp->v_active >= 2100)
		return true;
	else
		return false;
}

bool vdin_is_hdr_signal_in_s5(struct vdin_dev_s *devp)
{
	if (devp->prop.hdr_info.hdr_state == HDR_STATE_GET)
		return true;
	else
		return false;
}

bool vdin_is_dolby_signal_in_s5(struct vdin_dev_s *devp)
{
	if (K_FORCE_DV_ON)
		return 1;
	else
		return ((devp->dv.dolby_input & (1 << devp->index)) ||
			(devp->dv.dv_flag));
}

void vdin_dolby_addr_alloc_s5(struct vdin_dev_s *devp, unsigned int size)
{
	unsigned int index;
	/*int highmem_flag;*/
	unsigned int one_buf_size = dolby_size_byte;

	dolby_size_byte = one_buf_size;

	if (devp->dtdata->hw_ver == VDIN_HW_T7)
		devp->dv.dv_dma_size = one_buf_size * size + K_DV_META_RAW_BUFF0;
	else
		devp->dv.dv_dma_size = one_buf_size * size;
	devp->dv.dv_dma_vaddr = dma_alloc_coherent(&devp->this_pdev->dev,
						   devp->dv.dv_dma_size,
						   &devp->dv.dv_dma_paddr,
						   GFP_KERNEL);
	if (!devp->dv.dv_dma_vaddr) {
		pr_info("%s:dma alloc_coherent fail!!\n", __func__);
		return;
	}
	memset(devp->dv.dv_dma_vaddr, 0, devp->dv.dv_dma_size);
	/*if (devp->cma_config_flag & 0x100)*/
	/*	highmem_flag = PageHighMem(phys_to_page(devp->vfmem_start[0]));*/
	/*else*/
	/*	highmem_flag = PageHighMem(phys_to_page(devp->mem_start));*/

	for (index = 0; index < size; index++) {
		devp->vfp->dv_buf_mem[index] = devp->dv.dv_dma_paddr +
			one_buf_size * index;
		devp->vfp->dv_buf_vmem[index] = devp->dv.dv_dma_vaddr +
			one_buf_size * index;
		/*
		 *if (highmem_flag == 0) {
		 *	if ((devp->cma_config_flag & 0x100) &&
		 *	    devp->cma_config_en)
		 *		devp->vfp->dv_buf_ori[index] =
		 *			phys_to_virt(devp->vfmem_start[index] +
		 *				     devp->vfmem_size -
		 *				     one_buf_size);
		 *	else
		 *		devp->vfp->dv_buf_ori[index] =
		 *			phys_to_virt(devp->mem_start +
		 *				     devp->mem_size -
		 *				     one_buf_size *
		 *				     (devp->canvas_max_num
		 *				      - index));
		 *} else {
		 *	if ((devp->cma_config_flag & 0x100) &&
		 *	    devp->cma_config_en)
		 *		devp->vfp->dv_buf_ori[index] =
		 *			vdin_vmap(devp->vfmem_start[index] +
		 *				  devp->vfmem_size -
		 *				  one_buf_size,
		 *				  one_buf_size);
		 *	else
		 *		devp->vfp->dv_buf_ori[index] =
		 *			vdin_vmap(devp->mem_start +
		 *			devp->mem_size -
		 *			one_buf_size *
		 *			(devp->canvas_max_num - index),
		 *			one_buf_size);
		 *}
		 */
		/*pr_info("%s:dv_buf[%d]=(0x%x,0x%p)\n", __func__, index,*/
			/*devp->vfp->dv_buf_ori[index],*/
		/*	devp->vfp->dv_buf_mem[index],*/
		/*	devp->vfp->dv_buf_vmem[index]);*/
	}
	devp->dv.dv_mem_allocated = 1;
	pr_info("%s:dv_dma_vaddr=0x%p,dv_dma_paddr=0x%lx\n", __func__,
		devp->dv.dv_dma_vaddr, (ulong)devp->dv.dv_dma_paddr);

	devp->dv.temp_meta_data = kmalloc(K_DV_META_TEMP_BUFF_SIZE, GFP_KERNEL);
	if (IS_ERR_OR_NULL(devp->dv.temp_meta_data)) {
		pr_info("malloc dv temp buffer fail\n");
		devp->dv.temp_meta_data = NULL;
	} else {
		pr_info("malloc dv temp 2k buffer\n");
	}
}

void vdin_dolby_addr_release_s5(struct vdin_dev_s *devp, unsigned int size)
{
	/*int highmem_flag;*/
	/*int index;*/

	if (devp->dv.dv_mem_allocated == 0)
		return;

	if (devp->dv.dv_dma_vaddr)
		dma_free_coherent(&devp->this_pdev->dev, devp->dv.dv_dma_size,
				  devp->dv.dv_dma_vaddr, devp->dv.dv_dma_paddr);
	devp->dv.dv_dma_vaddr = NULL;
	devp->dv.dv_dma_size = 0;
	/*
	 *if (devp->cma_config_flag & 0x100)
	 *	highmem_flag = PageHighMem(phys_to_page(devp->vfmem_start[0]));
	 *else
	 *	highmem_flag = PageHighMem(phys_to_page(devp->mem_start));
	 *
	 *if (highmem_flag) {
	 *	for (index = 0; index < size; index++) {
	 *		if (devp->vfp->dv_buf_ori[index]) {
	 *			vdin_unmap_phyaddr(devp->vfp->dv_buf_ori
	 *					   [index]);
	 *			devp->vfp->dv_buf_ori[index] = NULL;
	 *		}
	 *	}
	 *}
	 */
	devp->dv.dv_mem_allocated = 0;

	if (!IS_ERR_OR_NULL(devp->dv.temp_meta_data)) {
		kfree(devp->dv.temp_meta_data);
		devp->dv.temp_meta_data = NULL;
		pr_info("release dv temp buffer\n");
	}

	if (!IS_ERR_OR_NULL(devp->dv.meta_data_raw_buffer1)) {
		kfree(devp->dv.meta_data_raw_buffer1);
		devp->dv.meta_data_raw_buffer1 = NULL;
		pr_info("release dv raw_buff1\n");
	}
}

static void vdin_dolby_metadata_swap_s5(struct vdin_dev_s *devp, char *buf)
{
	char ext;
	unsigned int i, j;

	for (i = 0; (i * 16) < dolby_size_byte; i++) {
		for (j = 0; j < 8; j++) {
			ext = buf[i * 16 + j];
			buf[i * 16 + j] = buf[i * 16 + 15 - j];
			buf[i * 16 + 15 - j] = ext;
		}
	}

	//vdin_dma_flush(devp, buf, dolby_size_byte, DMA_TO_DEVICE);
}

static u32 crc32_lut[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static u32 crc32_s5(u32 crc, const void *buf, size_t size)
{
	const u8 *p = (const uint8_t *)buf;

	crc = ~crc;
	while (size) {
		crc = (crc << 8) ^ crc32_lut[((crc >> 24) ^ *p) & 0xff];
		p++;
		size--;
	}
	return crc;
}

bool vdin_is_dolby_tunnel_444_input_s5(struct vdin_dev_s *devp)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (vdin_is_dolby_signal_in(devp) &&
	    devp->prop.color_format == TVIN_YUV422)
		return true;
#endif
	return false;
}

void vdin_dv_pr_meta_data_s5(void *addr, unsigned int size, unsigned int index)
{
	unsigned int i;
	char *c = addr;

	for (i = 0; i < size; i += 16) {
		pr_info("meta0:%d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			index, c[i], c[i + 1], c[i + 2], c[i + 3],
			c[i + 4], c[i + 5], c[i + 6], c[i + 7],
			c[i + 8], c[i + 9], c[i + 10], c[i + 11],
			c[i + 12], c[i + 13], c[i + 14], c[i + 15]);
	}
}

static void vdin_dv_pr_event_meta_data_s5(void *addr, unsigned int size, unsigned int index)
{
	unsigned int i;
	char *c = addr;

	for (i = 0; i < size; i += 16) {
		pr_info("meta1:%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			c[i], c[i + 1], c[i + 2], c[i + 3],
			c[i + 4], c[i + 5], c[i + 6], c[i + 7],
			c[i + 8], c[i + 9], c[i + 10], c[i + 11],
			c[i + 12], c[i + 13], c[i + 14], c[i + 15]);
	}
}

void vdin_pr_vsif_data_s5(struct vdin_dev_s *devp, struct vframe_s *vf)
{
	if (dv_dbg_log & DV_DEBUG_RAW_DATA)
		pr_info("vsif:index:%d size:%x pk:%02x ver:%02x,len:%02x PB:0x%02x %02x %02x %02x\n",
			vf->index, vf->vsif.size, devp->dv.dv_vsif_raw.pkttype,
			devp->dv.dv_vsif_raw.version, devp->dv.dv_vsif_raw.length,
			devp->dv.dv_vsif_raw.PB[0], devp->dv.dv_vsif_raw.PB[1],
			devp->dv.dv_vsif_raw.PB[2], devp->dv.dv_vsif_raw.PB[3]);
}

static void vdin_pr_hdr_rawdata_s5(struct vdin_dev_s *devp, struct vframe_s *vf)
{
	if (vdin_isr_monitor & VDIN_ISR_MONITOR_HDR_RAW_DATA)
		pr_info("hdr_rawdata idx:%d size:%x data:0x%02x 0x%02x 0x%02x 0x%02x\n",
			vf->index, vf->drm_if.size,
			devp->prop.hdr_info.hdr_data.rawdata[0],
			devp->prop.hdr_info.hdr_data.rawdata[1],
			devp->prop.hdr_info.hdr_data.rawdata[2],
			devp->prop.hdr_info.hdr_data.rawdata[3]);
}

void vdin_pr_emp_data_s5(struct vdin_dev_s *devp, struct vframe_s *vf)
{
	if (vdin_isr_monitor & VDIN_ISR_MONITOR_HDR_EMP_DATA)
		pr_info("emp idx:%d emp size:%02x id:0x02%x data:0x%02x 0x%02x 0x%02x 0x%02x\n",
			vf->index, devp->prop.emp_data.size,
			devp->prop.emp_data.tag_id,
			devp->prop.emp_data.empbuf[0],
			devp->prop.emp_data.empbuf[1],
			devp->prop.emp_data.empbuf[2],
			devp->prop.emp_data.empbuf[3]);
}

static void vdin_pr_sei_hdr_data_s5(struct vdin_dev_s *devp)
{
	if (vdin_isr_monitor & VDIN_ISR_MONITOR_AFBCE)
		pr_info("sei_data idx:%d et:%d sta:%x id:%02x len:%02x data:%x %x %x %x %x %x\n",
			devp->curr_wr_vfe->vf.index,
			devp->prop.hdr_info.hdr_data.eotf,
			devp->prop.hdr_info.hdr_state,
			devp->prop.hdr_info.hdr_data.metadata_id,
			devp->prop.hdr_info.hdr_data.length,
			devp->prop.hdr_info.hdr_data.primaries[0].x,
			devp->prop.hdr_info.hdr_data.primaries[0].y,
			devp->prop.hdr_info.hdr_data.white_points.x,
			devp->prop.hdr_info.hdr_data.white_points.y,
			devp->prop.hdr_info.hdr_data.master_lum.x,
			devp->prop.hdr_info.hdr_data.master_lum.y);
}

void vdin_pr_vrr_data_s5(struct vdin_dev_s *devp, struct vframe_s *vf)
{
	if (vdin_isr_monitor & VDIN_ISR_MONITOR_VRR_DATA)
		pr_info("index:%d en:%d mc:%x ffm:%02x pkt:%02x ver:%02x %02x %02x %02x %02x %02x\n",
			vf->index, devp->prop.vtem_data.vrr_en,
			devp->prop.vtem_data.m_const,
			devp->prop.vtem_data.fva_factor_m1,
			devp->prop.spd_data.pkttype,
			devp->prop.spd_data.version,
			devp->prop.spd_data.length,
			devp->prop.spd_data.data[0],
			devp->prop.spd_data.data[1],
			devp->prop.spd_data.data[2],
			devp->prop.spd_data.data[5]);
}

void vdin_dolby_buffer_update_s5(struct vdin_dev_s *devp, unsigned int index)
{
	u32 *p;
	char *c;
	char *cp;
	u32 meta32, meta_size;
	int i, j;
	int count;
	unsigned int offset = devp->addr_offset;
	u32 crc_r = 0, crc = 1;
	bool multimeta_flag = 0;
	bool multimetatail_flag = 0;
	u32 crc1_r = 0;
	u32 crc1 = 0;
	u32 max_pkt = 15;
	static u32 cnt;
	u32 rpt_cnt = 0, tail_cnt = 0, rev_cnt = 0, end_flag = 0;
	u8 pkt_type;
	struct dv_meta_pkt *pkt_p;
	u32 cp_size, cp_offset, cp_sum = 0;
	u32 *src_meta = devp->dv.meta_data_raw_buffer1;

	devp->dv.dv_crc_check = true;
	max_pkt = 15;
	if (index >= devp->canvas_max_num) {
		if (dv_dbg_log & DV_DEBUG_CANVAS_NUM)
			pr_info("%s er: %d %d\n", __func__,
				index, devp->canvas_max_num);
		devp->dv.dv_crc_check = false;
		return;
	}

	if (devp->dtdata->hw_ver == VDIN_HW_T7 &&
	    IS_ERR_OR_NULL(src_meta)) {
		devp->dv.dv_crc_check = false;
		pr_info("err: null meta buffer\n");
		return;
	}

	cp = devp->dv.temp_meta_data;
	/*c = devp->vfp->dv_buf_ori[index];*/
	c = devp->vfp->dv_buf_vmem[index];
	p = (u32 *)c;
	for (count = 0; count < META_RETRY_MAX; count++) {
		if (dv_dbg_mask & DV_READ_MODE_AXI) {
			memcpy(p, devp->vfp->dv_buf_vmem[index], 128 * max_pkt);
			//vdin_dma_flush(devp, p, 128 * max_pkt, DMA_TO_DEVICE);
			vdin_dolby_metadata_swap_s5(devp, c);
			/*vdin_dma_flush(devp, p, 128*max_pkt, DMA_TO_DEVICE);*/
		} else {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
				wr(offset, VDIN_DOLBY_DSC_CTRL2, 0xe800c0d5);
			else
				wr(offset, VDIN_DOLBY_DSC_CTRL2, 0xd180c0d5);
			rpt_cnt = 0;
			tail_cnt = 0;
			rev_cnt = 0;
			for (i = 0; i < (32 * max_pkt); i++) {
				if (devp->dtdata->hw_ver == VDIN_HW_T7) {
					p[i] = src_meta[i];
				} else {
					meta32 = rd(offset, VDIN_DOLBY_DSC_STATUS1);
					p[i] = swap32(meta32);
				}

				if (((i % 32) == 0) && !end_flag) {
					rev_cnt++;
					pkt_type = c[i * 4] & 0xc0;
					if (pkt_type == META_PKY_TYPE_START)
						multimeta_flag = 1;

					if (pkt_type == META_PKY_TYPE_START)
						rpt_cnt++;

					if (pkt_type == META_PKY_TYPE_TAIL) {
						multimetatail_flag = 1;
						tail_cnt++;
					}

					if (tail_cnt >= rpt_cnt) {
						end_flag = 1;
						max_pkt = rev_cnt;
					}
					if (((cnt % dv_dbg_log_du) == 0) &&
					    (dv_dbg_log & DV_DEBUG_NORMAL))
						pr_info("pkt_type %d:0x%x\n", i,
							pkt_type);
				}

				if (i == 31 && multimeta_flag == 0)
					break;
			}
		}

		meta_size = (c[3] << 8) | c[4];
		if (meta_size > 1024 || rev_cnt > 20 ||
		    rpt_cnt != tail_cnt ||
		    (meta_size > 128 && rev_cnt == 1)) {
			if (((cnt % dv_dbg_log_du) == 0) && (dv_dbg_log & DV_DEBUG_NORMAL))
				pr_info("err size:%d rp_cnt:%d, tal_cnt:%d, rv_cnt:%d max_pkt:%d\n",
					meta_size, rpt_cnt, tail_cnt, rev_cnt, max_pkt);

			devp->dv.dv_crc_check = false;
			return;
		}
		crc = p[31];/*first pkt chk sum*/
		crc_r = crc32_s5(0, p, 124);
		crc_r = swap32(crc_r);
		if (meta_size > DV_META_SINGLE_PKT_SIZE || multimeta_flag) {
			rev_cnt = min(rev_cnt, max_pkt);
			for (j = 0; j < (128 * rev_cnt); j += (128 * rpt_cnt)) {
				pkt_type = c[j] & 0xc0;
				cp_size = DV_META_SINGLE_PKT_SIZE;
				if (!(pkt_type == META_PKY_TYPE_START)) {
					cp_size += 2;
					cp_offset = 3;
				} else {
					cp_offset = 5;
				}
				for (i = 0; i < rpt_cnt; i++) {
					pkt_p = (struct dv_meta_pkt *)
						&c[j + 128 * i];
					crc1 = pkt_p->crc;
					crc1_r = crc32_s5(0, (char *)pkt_p, 124);
					crc1_r = swap32(crc1_r);
					if (((cnt % dv_dbg_log_du) == 0) &&
					    (dv_dbg_log & DV_DEBUG_NORMAL))
						pr_info("crc:0x%x, 0x%x\n",
							crc1, crc1_r);
					if (crc1 == crc1_r) {
						cp_offset += j + 128 * i;
						memcpy(cp,
						       &c[cp_offset], cp_size);
						cp += cp_size;
						cp_sum += cp_size;
						break;
					}
				}
				if (rpt_cnt == 0)
					break;
				if (((cnt % dv_dbg_log_du) == 0) &&
				    (dv_dbg_log & DV_DEBUG_NORMAL))
					pr_info("data idx %d:0x%x\n",
						j, c[j] & 0xc0);
			}
		} else {
			rev_cnt = 1;
			rpt_cnt = 0;
			tail_cnt = 0;
		}

		if (((cnt % dv_dbg_log_du) == 0) && (dv_dbg_log & DV_DEBUG_NORMAL)) {
			pr_info("mult_flag=%d tail_flag=%d meta_size=%d\n",
				multimeta_flag, multimetatail_flag, meta_size);
			pr_info("rpt_cnt:%d tail_cnt:%d rev_cnt:%d cp_sum:%d\n",
				rpt_cnt, tail_cnt, rev_cnt, cp_sum);
		}

		if (crc == crc_r) {
			if (multimeta_flag == 1 &&
			    multimetatail_flag == 1) {
				if (crc1 == crc1_r)
					break;
			} else {
				break;
			}
		}
	}

	/*meta data pkt data*/
	if (((cnt % dv_dbg_log_du) == 0) && (dv_dbg_log & DV_DEBUG_META_PKT_DATA))
		vdin_dv_pr_meta_data(c, 128 * rev_cnt, index);

	if (crc != crc_r ||
	    (multimeta_flag == 1 && multimetatail_flag == 1 &&
	    crc1 != crc1_r)) {
		/* set small size to make control path return -1
		 *	to use previous setting
		 */
		devp->vfp->dv_buf[index] = &c[5];
		devp->vfp->dv_buf_size[index] = 4;
		if (((cnt % dv_dbg_log_du) == 0) && (dv_dbg_log & DV_DEBUG_ERR_META_DATA)) {
			pr_err("%s:hdmi dovi meta crc error:%08x!=%08x\n",
			       __func__, crc, crc_r);
			pr_info("%s:index:%d dma:%x vaddr:%p size:%d\n",
				__func__, index,
				devp->vfp->dv_buf_mem[index],
				devp->vfp->dv_buf_vmem[index],
				meta_size);
			vdin_dv_pr_meta_data(p, 128, index);
		}
		devp->dv.dv_crc_check = false;
	} else {
		devp->dv.dv_crc_check = true;
		devp->vfp->dv_buf[index] = &c[5];
		devp->vfp->dv_buf_size[index] = meta_size;
		if (multimeta_flag == 1 && multimetatail_flag == 1) {
			devp->vfp->dv_buf[index] = c;
			cp = devp->dv.temp_meta_data;
			memcpy(c, cp, meta_size);
			if (((cnt % dv_dbg_log_du) == 0) && (dv_dbg_log & DV_DEBUG_NORMAL))
				pr_info("cp %d meta to buff\n", meta_size);
		} else if (meta_size > DV_META_SINGLE_PKT_SIZE) {
			devp->dv.dv_crc_check = false;
		}

		if (((cnt % dv_dbg_log_du) == 0) && (dv_dbg_log & DV_DEBUG_NORMAL))
			pr_info("%s:index:%d dma:%x vaddr:%p size:%d crc:%x crc1:%x\n",
				__func__, index,
				devp->vfp->dv_buf_mem[index],
				devp->vfp->dv_buf_vmem[index],
				meta_size, crc, crc1);
		/*meta data raw data*/
		if (((cnt % dv_dbg_log_du) == 0) && (dv_dbg_log & DV_DEBUG_OK_META_DATA))
			vdin_dv_pr_meta_data(c, meta_size, index);
	}

	cnt++;
}

void vdin_dolby_addr_update_s5(struct vdin_dev_s *devp, unsigned int index)
{
	unsigned int offset = devp->addr_offset;
	u32 *p;

	if (index >= devp->canvas_max_num)
		return;
	devp->vfp->dv_buf[index] = NULL;
	devp->vfp->dv_buf_size[index] = 0;
	p = (u32 *)devp->vfp->dv_buf_vmem[index];
	p[0] = 0;
	p[1] = 0;
	if (dv_dbg_mask & DV_READ_MODE_AXI) {
		wr(offset, VDIN_DOLBY_AXI_CTRL1,
		   devp->vfp->dv_buf_mem[index]);
		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 1, 4, 1);
		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 0, 4, 1);
		if (dv_dbg_log & DV_DEBUG_NORMAL)
			pr_info("%s:index:%d dma:%x\n", __func__,
				index, devp->vfp->dv_buf_mem[index]);
	} else {
		wr(offset, VDIN_DOLBY_DSC_CTRL2, 0x5180c0d5);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			wr(offset, VDIN_DOLBY_DSC_CTRL2, 0x6800c0d5);
		else
			wr(offset, VDIN_DOLBY_DSC_CTRL2, 0x5180c0d5);
		wr(offset, VDIN_DOLBY_DSC_CTRL3, 0);
	}
}

void vdin_dolby_config_s5(struct vdin_dev_s *devp)
{
//	unsigned int offset = devp->addr_offset;
//
//	devp->dv.dv_config = 1;
//	devp->dv.dv_path_idx = devp->index;
//	devp->vfp->low_latency = devp->dv.low_latency;
//	memcpy(&devp->vfp->dv_vsif,
//	       &devp->dv.dv_vsif, sizeof(struct tvin_dv_vsif_s));
//	wr_bits(offset, VDIN_DOLBY_DSC_CTRL0, 1, 30, 1);
//	wr_bits(offset, VDIN_DOLBY_DSC_CTRL0, 1, 26, 1);
//	wr_bits(offset, VDIN_DOLBY_DSC_CTRL0, 0, 26, 1);
//	if (dv_dbg_mask & DV_READ_MODE_AXI) {
//		wr(offset, VDIN_DOLBY_AXI_CTRL1, devp->vfp->dv_buf_mem[0]);
//		/* hold line = 0 */
//		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 0, 8, 8);
//		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 1, 4, 1);
//		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 1, 5, 1);
//		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 0, 5, 1);
//		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 0, 4, 1);
//		/*enable wr memory*/
//		vdin_dobly_mdata_write_en(offset, 1);
//	} else {
//		/*disable wr memory*/
//		vdin_dobly_mdata_write_en(offset, 0);
//		wr(offset, VDIN_DOLBY_DSC_CTRL2, 0x5180c0d5);
//		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
//			wr(offset, VDIN_DOLBY_DSC_CTRL2, 0x6800c0d5);
//		else
//			wr(offset, VDIN_DOLBY_DSC_CTRL2, 0x5180c0d5);
//		wr(offset, VDIN_DOLBY_DSC_CTRL3, 0x0);
//	}
//
//	if (vdin_dbg_en)
//		pr_info("%s\n", __func__);
}

void vdin_dolby_mdata_write_en_s5(unsigned int offset, unsigned int en)
{
	/*printk("=========>> wr memory %d\n", en);*/
	if (en) {
		/*enable write metadata to memory*/
		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 1, 30, 1);
		/*vdin0 dolby meta write enable*/
		/*W_VCBUS_BIT(0x27af, 1, 2, 1);*/
	} else {
		/*disable write metadata to memory*/
		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 0, 30, 1);
		/*vdin0 dolby meta write disable*/
		/*W_VCBUS_BIT(0x27af, 0, 2, 1);*/
	}
}

/*
 * set dv de-scramble or scramble, and this function
 * need call after vdin_set_hv_scale if vdin scaling down
 * enable
 * parm: devp
 * on:1 off:0
 */
void vdin_dv_desc_to_4448bit_s5(struct vdin_dev_s *devp,
			       unsigned int on_off)
{
	unsigned int offset = devp->addr_offset;

	/* only support vdin0
	 * don't support in T5
	 */
	if (devp->index == 1 ||
	    (devp->dtdata->hw_ver == VDIN_HW_T5 ||
	     devp->dtdata->hw_ver == VDIN_HW_T5D))
		return;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		if (on_off) {
			if (!devp->double_wr) {
				/* enable channel 1 for scramble */
				wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0,
					CH0_OUT_EN_BIT, CH_OUT_EN_WID);
				wr_bits(offset, VDIN_PP_LFIFO_CTRL, 1,
					CH1_OUT_EN_BIT, CH_OUT_EN_WID);
				/* switch vdin 0 wr mif to small */
				wr_bits(0, VDIN_TOP_DOUBLE_CTRL,
					WR_SEL_VDIN0_SML,
					MIF0_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
			}
			wr(offset, VDIN_DSC_TUNNEL_SEL, 0x3d11);/*re-order*/

			/*small path size in*/
			wr_bits(offset, VDIN_SCB_CTRL1, devp->v_shrink_out, 0, 13);
			wr_bits(offset, VDIN_SCB_CTRL1, devp->h_shrink_out, 16, 13);
			/*de-scramble enable*/
			wr_bits(offset, VDIN_VSHRK_CTRL, 0x1, 28, 1);
			/*small path scramble enable*/
			wr_bits(offset, VDIN_VSHRK_CTRL, 0x1, 29, 1);
			if (vdin_dbg_en)
				pr_info("vdin SC small in:(%d, %d)\n",
					devp->h_shrink_out, devp->v_shrink_out);

			/* normal path size in */
			wr_bits(offset, VDIN_DSC2_HSIZE, devp->v_active, 0, 13);
			wr_bits(offset, VDIN_DSC2_HSIZE, devp->h_active, 16, 13);
			/* normal patch scramble enable */
			wr_bits(offset, VDIN_SCB_CTRL0, 0x1, 28, 1);

			if (vdin_dbg_en)
				pr_info("vdin SC normal in:(%d, %d)\n",
					devp->h_active, devp->v_active);

		} else {
			/* disable descramble & scramble */
			/*small patch disable*/
			/*de-scramble disable*/
			wr_bits(offset, VDIN_VSHRK_CTRL, 0x0, 28, 1);
			/*small path scramble enable*/
			wr_bits(offset, VDIN_VSHRK_CTRL, 0x0, 29, 1);
			/* normal path disable */
			wr_bits(offset, VDIN_SCB_CTRL0, 0x0, 28, 1);

			if (!devp->double_wr) {
				/* enable channel 0 for normal TV path */
				wr_bits(offset, VDIN_PP_LFIFO_CTRL, 1,
					CH0_OUT_EN_BIT, CH_OUT_EN_WID);
				wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0,
					CH1_OUT_EN_BIT, CH_OUT_EN_WID);
			}
		}
	}
}

/*
 * yuv format, DV descramble 422 12bit TO 444 10bit
 */
void vdin_dv_de_tunnel_to_44410bit_s5(struct vdin_dev_s *devp,
				   unsigned int on_off)
{
	u32 data32;
	unsigned int offset = devp->addr_offset;

	data32 = (3 << 18 |
		  2 << 15 |
		  0 << 12 |
		  5 << 9 |
		  4 << 6 |
		  1 << 3);/*1 << 2 0:off 1:on*/

	if (on_off)
		wr(offset, VDIN_RO_WRMIF_STATUS, data32 | (1 << 2));
	else
		wr(offset, VDIN_RO_WRMIF_STATUS, data32);

	wr(offset, VDIN_DSC_DETUNNEL_SEL, 0x2c2d0);
	/*de-scramble h size in*/
	wr_bits(offset, VDIN_CFMT_W, devp->h_active_org / 2,
		0, 13);
	wr_bits(offset, VDIN_CFMT_W, devp->h_active_org,
		16, 13);
	/**/
	wr_bits(offset, VDIN_DSC_HSIZE, devp->h_active_org,
		0, 13);
	wr_bits(offset, VDIN_DSC_HSIZE, devp->h_active_org,
		16, 13);
	if (vdin_dbg_en)
		pr_info("vdin DSC in:(%d, %d)\n",
			devp->h_active_org, devp->v_active_org);
}

void vdin_dv_tunnel_set_s5(struct vdin_dev_s *devp)
{
//	if (!vdin_is_dolby_signal_in(devp))
//		return;
//
//	if (!devp->dtdata->de_tunnel_tunnel)
//		return;
//
//	if (vdin_dbg_en) {
//		pr_info("vdin dv tunel_set shrink:(%d, %d)\n",
//			devp->h_active, devp->h_shrink_out);
//	}
//
//	/* h shrink on*/
//	if (devp->h_shrink_out < devp->h_active) {
//		/*hw verify:de-tunnel 444 to 422 12bit*/
//		vdin_dolby_de_tunnel_to_44410bit(devp, true);
//		/*vdin de tunnel and tunnel for vdin scaling*/
//		vdin_dolby_desc_to_4448bit(devp, true);
//	} else {
//		vdin_dolby_de_tunnel_to_44410bit(devp, false);
//		vdin_dolby_desc_to_4448bit(devp, false);
//	}
}

static int vdin_get_max_buf_s5(struct vdin_dev_s *devp)
{
	if (!devp) {
		pr_err("devp is NULL\n");
		return -1;
	}

	return devp->frame_buff_num;
}

int vdin_event_cb_s5(int type, void *data, void *op_arg)
{
	unsigned long flags;
	struct vf_pool *p;
	struct vdin_dev_s *devp = vdin_get_dev(0);

	if (!op_arg) {
		if (vdin_ctl_dbg & CTL_DEBUG_EVENT_OP_ARG)
			pr_info("%s:op_arg is NULL!\n", __func__);
		return -1;
	}
	if (!data) {
		if (vdin_ctl_dbg & CTL_DEBUG_EVENT_DATA)
			pr_info("%s:data is NULL!\n", __func__);
		return -1;
	}
	p = (struct vf_pool *)op_arg;
	if (type & VFRAME_EVENT_RECEIVER_GET_AUX_DATA) {
		struct provider_aux_req_s *req =
			(struct provider_aux_req_s *)data;
		unsigned char index;

		if (!req->vf) {
			pr_info("%s:req->vf is NULL!\n", __func__);
			return -1;
		}
		spin_lock_irqsave(&p->dv_lock, flags);
		index = req->vf->index & 0xff;
		req->aux_buf = NULL;
		req->aux_size = 0;
		req->dv_enhance_exist = 0;
		/* TODO: need change the low latency flag when LL mode */
		req->low_latency = p->low_latency;
		memcpy(&req->dv_vsif,
			&p->dv_vsif, sizeof(struct tvin_dv_vsif_s));
		if (req->bot_flag)
			index = (req->vf->index >> 8) & 0xff;
		if (index != 0xff && index < p->size && p->dv_buf[index]) {
			req->aux_buf = p->dv_buf[index];
			req->aux_size = p->dv_buf_size[index];
		}
		spin_unlock_irqrestore(&p->dv_lock, flags);

		if (dv_dbg_log & DV_DEBUG_EVENT_META_DATA) {
			pr_info("%s(type 0x%x vf index 0x%x)=>size 0x%x\n",
				__func__, type, index, req->aux_size);
			vdin_dv_pr_event_meta_data_s5(req->aux_buf,
					req->aux_size, index);
		}
	} else if (type & VFRAME_EVENT_RECEIVER_DISP_MODE) {
		struct provider_disp_mode_req_s *req =
			(struct provider_disp_mode_req_s *)data;
		unsigned int index_disp;

		if (!req->vf) {
			pr_info("%s:req->vf is NULL!\n", __func__);
			return -1;
		}
		index_disp = req->vf->index_disp & 0xff;
		if (index_disp >= VFRAME_DISP_MAX_NUM) {
			if (vdin_ctl_dbg & CTL_DEBUG_EVENT_INDEX)
				pr_info("%s:req->vf->index_disp(%d) is overflow!\n",
					__func__, index_disp);
			return -1;
		}
		if (devp->game_mode || devp->skip_disp_md_check)
			req->disp_mode = VFRAME_DISP_MODE_NULL;
		else
			req->disp_mode = p->disp_mode[index_disp];
		if (req->req_mode == 1 && p->skip_vf_num)
			p->disp_mode[index_disp] = VFRAME_DISP_MODE_UNKNOWN;
		if (vdin_ctl_dbg & CTL_DEBUG_EVENT_DISP_MODE)
			pr_info("%s(type 0x%x vf index 0x%x)=>disp_mode %d,req_mode:%d;[%d]=%d,[%d]=%d\n",
				__func__, type, index_disp, req->disp_mode, req->req_mode,
				p->disp_index[0], p->disp_mode[p->disp_index[0]],
				p->disp_index[1], p->disp_mode[p->disp_index[1]]);
	} else if (type & VFRAME_EVENT_RECEIVER_NEED_NO_COMP) {
		unsigned int *cnt;

		/* use for debug */
//		if (vdin0_afbce_debug_force)
//			return 0;

		cnt = (unsigned int *)data;
		if (*cnt) {
			devp->afbce_mode = 0;
		} else {
			if (devp->afbce_valid)
				devp->afbce_mode = 1;
		}

		if (vdin_ctl_dbg & CTL_DEBUG_EVENT_NO_COMP)
			pr_info("%s(type 0x%x vdin%d) afbce_mode: %d, vpp_cnt: %d\n",
				__func__, type, devp->index,
				devp->afbce_mode, *cnt);
	} else if (type & VFRAME_EVENT_RECEIVER_BUF_COUNT) {
		*(int *)data = vdin_get_max_buf_s5(devp);
	} else if (type & VFRAME_EVENT_RECEIVER_REQ_STATE) {
		struct provider_state_req_s *req =
			(struct provider_state_req_s *)data;

		#ifdef CONFIG_AMLOGIC_TEE
		if (devp->secure_en && devp->mem_protected)
			req->req_result[0] = 1;
		else
			req->req_result[0] = 0;
		#else
			req->req_result[0] = 0;
		#endif
	}

	return 0;
}

void set_invert_top_bot_s5(bool invert_flag)
{
	invert_top_bot = invert_flag;
}

int vdin_hdr_sei_error_check_s5(struct vdin_dev_s *devp)
{
	int primary_data[3][2];
	int i;

	vdin_pr_sei_hdr_data_s5(devp);
	/*GBR compare with standard 709 primary*/
	for (i = 0; i < 3; i++) {
		primary_data[i][0] =
			devp->prop.hdr_info.hdr_data.primaries[i].x;
		primary_data[i][1] =
			devp->prop.hdr_info.hdr_data.primaries[i].y;
	}
	if (((primary_data[0][0] + 250) / 500 == 30) &&
	    ((primary_data[0][1] + 250) / 500 == 60) &&
	    ((primary_data[1][0] + 250) / 500 == 15) &&
	    ((primary_data[1][1] + 250) / 500 == 6) &&
	    ((primary_data[2][0] + 250) / 500 == 64) &&
	    ((primary_data[2][1] + 250) / 500 == 33))
		return 1;

	/*RGB compare with standard 709 primary*/
	for (i = 0; i < 3; i++) {
		primary_data[(i + 2) % 3][0] =
			devp->prop.hdr_info.hdr_data.primaries[i].x;
		primary_data[(i + 2) % 3][1] =
			devp->prop.hdr_info.hdr_data.primaries[i].y;
	}

	if (((primary_data[0][0] + 250) / 500 == 30) &&
	    ((primary_data[0][1] + 250) / 500 == 60) &&
	    ((primary_data[1][0] + 250) / 500 == 15) &&
	    ((primary_data[1][1] + 250) / 500 == 6) &&
	    ((primary_data[2][0] + 250) / 500 == 64) &&
	    ((primary_data[2][1] + 250) / 500 == 33))
		return 1;

	/*GxBxRxGyByRy compare with standard 709 primary*/
	primary_data[0][0] = devp->prop.hdr_info.hdr_data.primaries[0].x;
	primary_data[0][1] = devp->prop.hdr_info.hdr_data.primaries[1].y;
	primary_data[1][0] = devp->prop.hdr_info.hdr_data.primaries[0].y;
	primary_data[1][1] = devp->prop.hdr_info.hdr_data.primaries[2].x;
	primary_data[2][0] = devp->prop.hdr_info.hdr_data.primaries[1].x;
	primary_data[2][1] = devp->prop.hdr_info.hdr_data.primaries[2].y;

	if (((primary_data[0][0] + 250) / 500 == 30) &&
	    ((primary_data[0][1] + 250) / 500 == 60) &&
	    ((primary_data[1][0] + 250) / 500 == 15) &&
	    ((primary_data[1][1] + 250) / 500 == 6) &&
	    ((primary_data[2][0] + 250) / 500 == 64) &&
	    ((primary_data[2][1] + 250) / 500 == 33))
		return 1;
	else
		return 0;
}

void vdin_hdr10plus_check_s5(struct vdin_dev_s *devp,
			  struct vframe_s *vf)
{
	if (devp->prop.hdr10p_info.hdr10p_on) {
		/*devp->prop.hdr10p_info.hdr10p_on = false;*/

		vf->signal_type |= (1 << 29);/*present_flag*/
		vf->signal_type |= (0 << 25);/*0:limited*/
		/*color_primaries*/
		vf->signal_type = ((9 << 16) |
			(vf->signal_type & (~0xFF0000)));
		/*transfer_characteristic*/
		vf->signal_type = ((0x30 << 8) |
			(vf->signal_type & (~0xFF00)));
		/*matrix_coefficient*/
		vf->signal_type = ((9 << 0) |
			(vf->signal_type & (~0xFF)));
		memcpy(&vf->prop.hdr10p_data,
		       &devp->prop.hdr10p_info.hdr10p_data,
		       sizeof(struct tvin_hdr10p_data_s));
	}
}

void vdin_set_drm_data_s5(struct vdin_dev_s *devp,
		       struct vframe_s *vf)
{
	struct vframe_master_display_colour_s *vf_dp =
		&vf->prop.master_display_colour;
	u32 val = 0;

	if (devp->prop.hdr_info.hdr_state != HDR_STATE_NULL) {
		if (vdin_hdr_sei_error_check(devp) == 1) {
			vf_dp->present_flag = false;
			vf->signal_type &= ~(1 << 29);
			vf->signal_type &= ~(1 << 25);
			/*todo;default is bt709,if change need sync*/
			vf->signal_type = ((1 << 16) |
				(vf->signal_type & (~0xFF0000)));
			vf->signal_type = ((1 << 8) |
				(vf->signal_type & (~0xFF00)));
		} else {
			memcpy(vf_dp->primaries,
			       devp->prop.hdr_info.hdr_data.primaries,
			       sizeof(u32) * 6);
			memcpy(vf_dp->white_point,
			       &devp->prop.hdr_info.hdr_data.white_points,
			       sizeof(u32) * 2);
			memcpy(vf_dp->luminance,
			       &devp->prop.hdr_info.hdr_data.master_lum,
			       sizeof(u32) * 2);
			/* content_light_level */
			vf_dp->content_light_level.max_content =
				devp->prop.hdr_info.hdr_data.mcll;
			vf_dp->content_light_level.max_pic_average =
				devp->prop.hdr_info.hdr_data.mfall;

			vf_dp->present_flag = true;

			if (devp->prop.hdr_info.hdr_data.mcll != 0 &&
			    devp->prop.hdr_info.hdr_data.mfall != 0)
				vf_dp->content_light_level.present_flag = 1;
			else
				vf_dp->content_light_level.present_flag = 0;

			if (devp->prop.hdr_info.hdr_data.eotf ==
			    EOTF_SMPTE_ST_2048 ||
			    devp->prop.hdr_info.hdr_data.eotf ==
			    EOTF_HDR) {
				vf->signal_type |= (1 << 29);
				vf->signal_type |= (0 << 25);/*0:limit*/
				vf->signal_type = ((9 << 16) |
					(vf->signal_type & (~0xFF0000)));
				vf->signal_type = ((16 << 8) |
					(vf->signal_type & (~0xFF00)));
				vf->signal_type = ((9 << 0) |
					(vf->signal_type & (~0xFF)));
			} else if (devp->prop.hdr_info.hdr_data.eotf ==
					EOTF_HLG) {
				vf->signal_type |= (1 << 29);
				vf->signal_type |= (0 << 25);/*0:limit*/
				vf->signal_type = ((9 << 16) |
					(vf->signal_type & (~0xFF0000)));
				vf->signal_type = ((14 << 8) |
					(vf->signal_type & (~0xFF00)));
				vf->signal_type = ((9 << 0) |
					(vf->signal_type & (~0xFF)));
			} else {
				vf->signal_type &= ~(1 << 29);
				vf->signal_type &= ~(1 << 25);
				/*todo;default is bt709,if change need sync*/
				vf->signal_type = ((1 << 16) |
					(vf->signal_type & (~0xFF0000)));
				vf->signal_type = ((1 << 8) |
					(vf->signal_type & (~0xFF00)));
			}
		}
	} else if (devp->prop.hdr_info.hdr_state == HDR_STATE_NULL) {
		vf_dp->present_flag = false;
		vf->signal_type &= ~(1 << 29);
		vf->signal_type &= ~(1 << 25);
		val = vdin_matrix_range_chk_s5(devp);
		vf->signal_type |= (val << 25);
		/*todo;default is bt709,if change need sync*/
		vf->signal_type = ((1 << 16) |
			(vf->signal_type & (~0xFF0000)));
		vf->signal_type = ((1 << 8) |
			(vf->signal_type & (~0xFF00)));
	}

	/* hdr10+ check */
	vdin_hdr10plus_check_s5(devp, vf);

	memcpy(&devp->dv.dv_vsif_raw, &devp->prop.dv_vsif_raw,
		sizeof(struct tvin_dv_vsif_raw_s));
	vf->vsif.addr = &devp->dv.dv_vsif_raw;
	if (devp->dv.dv_flag) {
		vf->vsif.size = sizeof(struct tvin_dv_vsif_raw_s);
		vdin_pr_vsif_data_s5(devp, vf);
	} else {
		vf->vsif.size = 0;
	}

	vf->drm_if.addr = &devp->prop.hdr_info.hdr_data.rawdata;
	if (devp->prop.vdin_hdr_flag) {
		vf->drm_if.size = sizeof(devp->prop.hdr_info.hdr_data.rawdata);
		vdin_pr_hdr_rawdata_s5(devp, vf);
	} else {
		vf->drm_if.size = 0;
	}

	vf->emp.addr = &devp->prop.emp_data.empbuf;
	vf->emp.size = devp->prop.emp_data.size;
	vdin_pr_emp_data_s5(devp, vf);
}

bool vdin_check_is_spd_data_s5(struct vdin_dev_s *devp)
{
	if (!devp)
		return false;

	if (devp->prop.spd_data.data[0] == 0x1A &&
	    devp->prop.spd_data.data[1] == 0x00 &&
	    devp->prop.spd_data.data[2] == 0x00)
		return true;
	else
		return false;
}

bool vdin_check_spd_data_chg_s5(struct vdin_dev_s *devp)
{
	if (!devp)
		return false;

	if ((devp->pre_prop.spd_data.data[0] == 0x1a &&
	     devp->pre_prop.spd_data.data[1] == 0x00 &&
	     devp->pre_prop.spd_data.data[2] == 0x00) ||
	    (devp->prop.spd_data.data[0] == 0x1a &&
	     devp->prop.spd_data.data[1] == 0x00 &&
	     devp->prop.spd_data.data[2] == 0x00)) {
		if ((devp->pre_prop.spd_data.data[5] >> 2 & 0x3) !=
		     (devp->prop.spd_data.data[5] >> 2 & 0x3))
			return true;
	} else {
		return false;
	}

	return false;
}

void vdin_set_freesync_data_s5(struct vdin_dev_s *devp, struct vframe_s *vf)
{
	if (devp->prop.vtem_data.vrr_en) {
		memcpy(&devp->vrr_data.vtem_data, &devp->prop.vtem_data,
			sizeof(struct tvin_vtem_data_s));
		vf->vtem.addr = &devp->vrr_data.vtem_data;
		vf->vtem.size = sizeof(devp->prop.vtem_data);
	} else if (vdin_check_is_spd_data(devp) &&
		(devp->prop.spd_data.data[5] >> 2 & 0x3)) {
		memcpy(&devp->vrr_data.spd_data, &devp->prop.spd_data,
			sizeof(struct tvin_spd_data_s));
		vf->spd.addr = &devp->vrr_data.spd_data;
		vf->spd.size = sizeof(devp->prop.spd_data);
	} else {
		vf->vtem.size = 0;
		vf->spd.size = 0;
	}
	vdin_pr_vrr_data_s5(devp, vf);
}

void vdin_vs_proc_monitor_s5(struct vdin_dev_s *devp)
{
	if (IS_HDMI_SRC(devp->parm.port)) {
		if (devp->prop.dolby_vision != devp->dv.dv_flag ||
		    (devp->prop.low_latency != devp->dv.low_latency &&
		     devp->dv.dv_flag))
			devp->dv.chg_cnt++;
		else
			devp->dv.chg_cnt = 0;

		if (devp->prop.vdin_hdr_flag != devp->pre_prop.vdin_hdr_flag)
			devp->prop.hdr_info.hdr_check_cnt++;
		else
			devp->prop.hdr_info.hdr_check_cnt = 0;

		if (devp->prop.latency.allm_mode != devp->pre_prop.latency.allm_mode ||
		    devp->prop.latency.it_content != devp->pre_prop.latency.it_content ||
		    devp->prop.latency.cn_type != devp->pre_prop.latency.cn_type)
			devp->dv.allm_chg_cnt++;
		else
			devp->dv.allm_chg_cnt = 0;

		/*hdmi source afd check*/
		if (devp->pre_prop.aspect_ratio !=
		    devp->prop.aspect_ratio)
			devp->sg_chg_afd_cnt++;
		else
			devp->sg_chg_afd_cnt = 0;

		if (devp->vrr_data.vdin_vrr_en_flag != devp->prop.vtem_data.vrr_en ||
			vdin_check_spd_data_chg(devp))
			devp->vrr_data.vrr_chg_cnt++;
		else
			devp->vrr_data.vrr_chg_cnt = 0;

		if (vdin_isr_monitor & VDIN_ISR_MONITOR_FLAG)
			pr_info("dv:%d, LL:%d hdr st:%d eotf:%d fg:%d allm:%d sty:0x%x spd:0x%x 0x%x\n",
				devp->prop.dolby_vision,
				devp->prop.low_latency,
				devp->prop.hdr_info.hdr_state,
				devp->prop.hdr_info.hdr_data.eotf,
				devp->prop.vdin_hdr_flag,
				devp->prop.latency.allm_mode,
				devp->parm.info.signal_type,
				devp->prop.spd_data.data[0],
				devp->prop.spd_data.data[5]);
		if (vdin_isr_monitor & VDIN_ISR_MONITOR_EMP)
			pr_info("emp size:%d, data:0x%02x 0x%02x 0x%02x 0x%02x\n",
				devp->prop.emp_data.size,
				devp->prop.emp_data.empbuf[0],
				devp->prop.emp_data.empbuf[1],
				devp->prop.emp_data.empbuf[2],
				devp->prop.emp_data.empbuf[3]);
		if (vdin_isr_monitor & VDIN_ISR_MONITOR_RATIO)
			pr_info("aspect_ratio=0x%x,vrr_en:%d,fr:%d\n",
				devp->prop.aspect_ratio, devp->prop.vtem_data.vrr_en,
				devp->prop.vtem_data.base_framerate);
	}

	if (color_range_force)
		devp->prop.color_fmt_range =
		tvin_get_force_fmt_range(devp->prop.color_format);
}

void vdin_check_hdmi_hdr_s5(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	enum tvin_port_e port = TVIN_PORT_NULL;
	struct tvin_sig_property_s *prop;

	if (!devp)
		return;

	port = devp->parm.port;

	if (port < TVIN_PORT_HDMI0 || port > TVIN_PORT_HDMI7)
		return;

	prop = &devp->prop;
	sm_ops = devp->frontend->sm_ops;
	if (sm_ops->get_sig_property) {
		/* don't need get again, already got from start dec */
		/* sm_ops->get_sig_property(devp->frontend, prop); */
		pr_info("vdin%d hdmi hdr eotf:0x%x, state:%d\n",
			devp->index,
			devp->prop.hdr_info.hdr_data.eotf,
			devp->prop.hdr_info.hdr_state);
		/*devp->prop.vdin_hdr_flag = vdin_is_hdr_signal_in(devp);*/
	}
}

u32 vdin_get_curr_field_type_s5(struct vdin_dev_s *devp)
{
	u32 field_status;
	u32 type = VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD;

	/* struct tvin_parm_s *parm = &devp->parm; */
	const struct tvin_format_s *fmt_info = devp->fmt_info_p;

	if (fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE) {
		type |= VIDTYPE_PROGRESSIVE;
	} else {
		field_status = vdin_get_field_type(devp->addr_offset);
		/*tvafe FIELD POLARITY 0 TOP,vdin must invert for correct*/

		if (invert_top_bot || IS_TVAFE_SRC(devp->parm.port))
			type |=	field_status ?
			VIDTYPE_INTERLACE_TOP :	VIDTYPE_INTERLACE_BOTTOM;
		else
			type |= field_status ?
			VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;

		/*md2 get pre field sts, need invert for cur*/
		if ((devp->game_mode & VDIN_GAME_MODE_2) == VDIN_GAME_MODE_2) {
			if ((type & VIDTYPE_INTERLACE_BOTTOM) ==
				VIDTYPE_INTERLACE_TOP) {
				type |= VIDTYPE_INTERLACE_BOTTOM;
			} else if ((type & VIDTYPE_INTERLACE_BOTTOM) ==
				VIDTYPE_INTERLACE_BOTTOM) {
				type &= ~VIDTYPE_INTERLACE_BOTTOM;
				type |= VIDTYPE_INTERLACE_TOP;
			}
		}
	}

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		type |= VIDTYPE_VIU_444;
		/*if (devp->afbce_mode_pre) */
		/*	type |= VIDTYPE_COMB_MODE; */
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
		type |= VIDTYPE_VIU_422;
		break;
	case VDIN_FORMAT_CONVERT_YUV_RGB:
	case VDIN_FORMAT_CONVERT_RGB_RGB:
		type |= VIDTYPE_RGB_444;
		if (devp->afbce_mode_pre && vdin_chk_is_comb_mode(devp))
			type |= VIDTYPE_COMB_MODE;
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		type |= VIDTYPE_VIU_NV21;
		type &= (~VIDTYPE_VIU_SINGLE_PLANE);
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		/* type |= VIDTYPE_VIU_NV12; */
		type &= (~VIDTYPE_VIU_SINGLE_PLANE);
		break;
	default:
		break;
	}

	if (devp->afbce_valid)
		type |= VIDTYPE_SUPPORT_COMPRESS;

	if (devp->double_wr) {
		type |= VIDTYPE_COMPRESS;
		type &= ~VIDTYPE_NO_DW;
		type |= VIDTYPE_SCATTER;

		/* add only for TL1 switch mif/afbce dynamically */
		type &= ~VIDTYPE_SUPPORT_COMPRESS;

		if (devp->afbce_flag & VDIN_AFBCE_EN_LOSSY)
			type |= VIDTYPE_COMPRESS_LOSS;
		if (vdin_chk_is_comb_mode(devp))
			type |= VIDTYPE_COMB_MODE;
	} else if (devp->afbce_mode_pre) {
		type |= VIDTYPE_COMPRESS;
		type |= VIDTYPE_NO_DW;
		type |= VIDTYPE_SCATTER;
		if (devp->afbce_flag & VDIN_AFBCE_EN_LOSSY)
			type |= VIDTYPE_COMPRESS_LOSS;
		if (vdin_chk_is_comb_mode(devp))
			type |= VIDTYPE_COMB_MODE;
	}

#ifdef CONFIG_AMLOGIC_TEE
	if (devp->secure_en)
		type |= VIDTYPE_SEC_MD;
	else
		type &= ~VIDTYPE_SEC_MD;
#endif

	return type;
}

/*	function: set vf source type
 *	source type list:
 *	a.VFRAME_SOURCE_TYPE_TUNER	(TVIN_PORT CVBS3)
 *	b.VFRAME_SOURCE_TYPE_CVBS	(TVIN_PORT CVBS0/CVBS1/
 *		CVBS2/CVBS4/CVBS5/CVBS6/CVBS7)
 *	c.VFRAME_SOURCE_TYPE_COMP	(TVIN_PORT COMP0~COMP7)
 *	d.VFRAME_SOURCE_TYPE_HDMI	(TVIN_PORT HDMI0~HDMI7/DVIN0)
 *	e.others
 */
inline void vdin_set_source_type_s5(struct vdin_dev_s *devp,
				 struct vframe_s *vf)
{
	switch (devp->parm.port) {
	case TVIN_PORT_CVBS3:
		vf->source_type = VFRAME_SOURCE_TYPE_TUNER;
		break;
	case TVIN_PORT_CVBS0:
	case TVIN_PORT_CVBS1:
	case TVIN_PORT_CVBS2:
		vf->source_type = VFRAME_SOURCE_TYPE_CVBS;
		break;
	case TVIN_PORT_HDMI0:
	case TVIN_PORT_HDMI1:
	case TVIN_PORT_HDMI2:
	case TVIN_PORT_HDMI3:
	case TVIN_PORT_HDMI4:
	case TVIN_PORT_HDMI5:
	case TVIN_PORT_HDMI6:
	case TVIN_PORT_HDMI7:
	case TVIN_PORT_DVIN0:/* external hdmiin used default */
		vf->source_type = VFRAME_SOURCE_TYPE_HDMI;
		break;
	default:
		vf->source_type = VFRAME_SOURCE_TYPE_OTHERS;
		break;
	}
}

/* function:set
 * main source modes:
 * a.NTSC	b.PAL
 * c.SECAM	d.others
 */
inline void vdin_set_source_mode_s5(struct vdin_dev_s *devp,
				 struct vframe_s *vf)
{
	switch (devp->parm.info.fmt) {
	case TVIN_SIG_FMT_CVBS_NTSC_M:
	case TVIN_SIG_FMT_CVBS_NTSC_443:
		vf->source_mode = VFRAME_SOURCE_MODE_NTSC;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_I:
	case TVIN_SIG_FMT_CVBS_PAL_M:
	case TVIN_SIG_FMT_CVBS_PAL_60:
	case TVIN_SIG_FMT_CVBS_PAL_CN:
		vf->source_mode = VFRAME_SOURCE_MODE_PAL;
		break;
	case TVIN_SIG_FMT_CVBS_SECAM:
		vf->source_mode = VFRAME_SOURCE_MODE_SECAM;
		break;
	default:
		vf->source_mode = VFRAME_SOURCE_MODE_OTHERS;
		break;
	}
}

/*
 * 480p/i = 9:8
 * 576p/i = 16:15
 * 720p and 1080p/i = 1:1
 * All VGA format = 1:1
 */
void vdin_set_pixel_aspect_ratio_s5(struct vdin_dev_s *devp,
				 struct vframe_s *vf)
{
	switch (devp->parm.info.fmt) {
	/* 480P */
	case TVIN_SIG_FMT_HDMI_640X480P_60HZ:
	case TVIN_SIG_FMT_HDMI_640X480P_72HZ:
	case TVIN_SIG_FMT_HDMI_640X480P_75HZ:
	case TVIN_SIG_FMT_HDMI_720X480P_60HZ:
	case TVIN_SIG_FMT_HDMI_1440X480P_60HZ:
	case TVIN_SIG_FMT_HDMI_2880X480P_60HZ:
	case TVIN_SIG_FMT_HDMI_720X480P_120HZ:
	case TVIN_SIG_FMT_HDMI_720X480P_240HZ:
	case TVIN_SIG_FMT_HDMI_720X480P_60HZ_FRAME_PACKING:
	case TVIN_SIG_FMT_CAMERA_640X480P_30HZ:
		/* 480I */
	case TVIN_SIG_FMT_CVBS_NTSC_M:
	case TVIN_SIG_FMT_CVBS_NTSC_443:
	case TVIN_SIG_FMT_CVBS_PAL_M:
	case TVIN_SIG_FMT_CVBS_PAL_60:
	case TVIN_SIG_FMT_HDMI_720X480I_60HZ:
	case TVIN_SIG_FMT_HDMI_1440X480I_60HZ:
	case TVIN_SIG_FMT_HDMI_2880X480I_60HZ:
	case TVIN_SIG_FMT_HDMI_1440X480I_120HZ:
	case TVIN_SIG_FMT_HDMI_1440X480I_240HZ:
	case TVIN_SIG_FMT_BT656IN_480I_60HZ:
	case TVIN_SIG_FMT_BT601IN_480I_60HZ:
		vf->pixel_ratio = PIXEL_ASPECT_RATIO_8_9;
		break;
		/* 576P */
	case TVIN_SIG_FMT_HDMI_720X576P_50HZ:
	case TVIN_SIG_FMT_HDMI_1440X576P_50HZ:
	case TVIN_SIG_FMT_HDMI_2880X576P_50HZ:
	case TVIN_SIG_FMT_HDMI_720X576P_100HZ:
	case TVIN_SIG_FMT_HDMI_720X576P_200HZ:
	case TVIN_SIG_FMT_HDMI_720X576P_50HZ_FRAME_PACKING:
		/* 576I */
	case TVIN_SIG_FMT_CVBS_PAL_I:
	case TVIN_SIG_FMT_CVBS_PAL_CN:
	case TVIN_SIG_FMT_CVBS_SECAM:
	case TVIN_SIG_FMT_HDMI_720X576I_50HZ:
	case TVIN_SIG_FMT_HDMI_1440X576I_50HZ:
	case TVIN_SIG_FMT_HDMI_2880X576I_50HZ:
	case TVIN_SIG_FMT_HDMI_1440X576I_100HZ:
	case TVIN_SIG_FMT_HDMI_1440X576I_200HZ:
	case TVIN_SIG_FMT_BT656IN_576I_50HZ:
	case TVIN_SIG_FMT_BT601IN_576I_50HZ:
		vf->pixel_ratio = PIXEL_ASPECT_RATIO_16_15;
		break;
	default:
		vf->pixel_ratio = PIXEL_ASPECT_RATIO_1_1;
		break;
	}
}

/*
 * based on the bellow parameters:
 * 1.h_active
 * 2.v_active
 */
void vdin_set_display_ratio_s5(struct vdin_dev_s *devp,
			    struct vframe_s *vf)
{
	unsigned int re = 0;
	enum tvin_aspect_ratio_e aspect_ratio = devp->prop.aspect_ratio;

	if (vf->width == 0 || vf->height == 0)
		return;
	re = (vf->width * 36) / vf->height;
	if ((re > 36 && re <= 56) || re == 90 || re == 108)
		vf->ratio_control = 0xc0 << DISP_RATIO_ASPECT_RATIO_BIT;
	else if (re > 56)
		vf->ratio_control = 0x90 << DISP_RATIO_ASPECT_RATIO_BIT;
	else
		vf->ratio_control = 0x0 << DISP_RATIO_ASPECT_RATIO_BIT;

	switch (aspect_ratio) {
	case TVIN_ASPECT_4x3_FULL:
		vf->pic_mode.screen_mode = VIDEO_WIDEOPTION_CUSTOM;
		vf->pic_mode.provider = PIC_MODE_PROVIDER_WSS;
		vf->pic_mode.hs = 0;
		vf->pic_mode.he = 0;
		vf->pic_mode.vs = 0;
		vf->pic_mode.ve = 0;
		/* 3*256/4=0xc0 */
		vf->pic_mode.custom_ar = 0xc0;
		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
		break;
	case TVIN_ASPECT_14x9_FULL:
		vf->pic_mode.screen_mode = VIDEO_WIDEOPTION_CUSTOM;
		vf->pic_mode.provider = PIC_MODE_PROVIDER_WSS;
		vf->pic_mode.hs = 0;
		vf->pic_mode.he = 0;
		vf->pic_mode.vs = 0;
		vf->pic_mode.ve = 0;
		/* 9*256/14=0xc0 */
		vf->pic_mode.custom_ar = 0xa4;
		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
		break;
	case TVIN_ASPECT_16x9_FULL:
		vf->pic_mode.screen_mode = VIDEO_WIDEOPTION_CUSTOM;
		vf->pic_mode.provider = PIC_MODE_PROVIDER_WSS;
		vf->pic_mode.hs = 0;
		vf->pic_mode.he = 0;
		vf->pic_mode.vs = 0;
		vf->pic_mode.ve = 0;
		/* 9*256/16=0xc0 */
		vf->pic_mode.custom_ar = 0x90;
		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
		break;
	case TVIN_ASPECT_14x9_LB_CENTER:
		/**720/462=14/9;(576-462)/2=57;57/2=28**/
		/**need cut more**/
		vf->pic_mode.screen_mode = VIDEO_WIDEOPTION_CUSTOM;
		vf->pic_mode.provider = PIC_MODE_PROVIDER_WSS;
		vf->pic_mode.hs = 0;
		vf->pic_mode.he = 0;
		vf->pic_mode.vs = 36;
		vf->pic_mode.ve = 36;
		vf->pic_mode.custom_ar = 0xa4;
		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
		break;
	case TVIN_ASPECT_14x9_LB_TOP:
		/**720/462=14/9;(576-462)/2=57**/
		vf->pic_mode.screen_mode = VIDEO_WIDEOPTION_CUSTOM;
		vf->pic_mode.provider = PIC_MODE_PROVIDER_WSS;
		vf->pic_mode.hs = 0;
		vf->pic_mode.he = 0;
		vf->pic_mode.vs = 0;
		vf->pic_mode.ve = 57;
		vf->pic_mode.custom_ar = 0xa4;
		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
		break;
	case TVIN_ASPECT_16x9_LB_CENTER:
		/**720/405=16/9;(576-405)/2=85;85/2=42**/
		/**need cut more**/
		vf->pic_mode.screen_mode = VIDEO_WIDEOPTION_CUSTOM;
		vf->pic_mode.provider = PIC_MODE_PROVIDER_WSS;
		vf->pic_mode.hs = 0;
		vf->pic_mode.he = 0;
		vf->pic_mode.vs = 70;
		vf->pic_mode.ve = 70;
		vf->pic_mode.custom_ar = 0x90;
		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
		break;
	case TVIN_ASPECT_16x9_LB_TOP:
		/**720/405=16/9;(576-405)/2=85**/
		vf->pic_mode.screen_mode = VIDEO_WIDEOPTION_CUSTOM;
		vf->pic_mode.provider = PIC_MODE_PROVIDER_WSS;
		vf->pic_mode.hs = 0;
		vf->pic_mode.he = 0;
		vf->pic_mode.vs = 0;
		vf->pic_mode.ve = 85;
		vf->pic_mode.custom_ar = 0x90;
		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
		break;
	default:
		break;
	}
}

/*function:set source bitdepth
 *	based on parameter:
 *	devp->source_bitdepth:	8/9/10
 */
inline void vdin_set_source_bitdepth_s5(struct vdin_dev_s *devp,
				     struct vframe_s *vf)
{
	switch (devp->source_bitdepth) {
	case VDIN_COLOR_DEEPS_10BIT:
		vf->bitdepth = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10;
		break;
	case VDIN_COLOR_DEEPS_9BIT:
		vf->bitdepth = BITDEPTH_Y9 | BITDEPTH_U9 | BITDEPTH_V9;
		break;
	case VDIN_COLOR_DEEPS_8BIT:
		vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		break;
	default:
		vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		break;
	}
	if (devp->full_pack == VDIN_422_FULL_PK_EN &&
	    devp->source_bitdepth > 8 &&
	    (devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422 ||
	     devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422 ||
	     devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422 ||
	     devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422))
		vf->bitdepth |= FULL_PACK_422_MODE;
}

/*@20170905new add for support dynamic adj dest_format yuv422/yuv444,
 *not support nv21 dynamic adj!!!
 */
void vdin_source_bitdepth_reinit_s5(struct vdin_dev_s *devp)
{
	int i = 0;
	struct vf_entry *master;
	struct vframe_s *vf;
	struct vf_pool *p = devp->vfp;

	for (i = 0; i < p->size; ++i) {
		master = vf_get_master(p, i);
		vf = &master->vf;
		vdin_set_source_bitdepth(devp, vf);
	}
}

void vdin_clk_on_off_s5(struct vdin_dev_s *devp, bool on_off)
{
	unsigned int offset = devp->addr_offset;

	pr_info("%s %d:on_off:%d\n", __func__, __LINE__, on_off);

	/* gclk_ctrl,00: auto, 01: off, 1x: on */
	if (on_off) { /* on */
		/* 8k top */
		wr(offset, VDIN_TOP_CTRL,
			(0x0 << TOP_GCLK_CTRL_BIT)        | /* 8k top_gclk */
			(0x0 << LOCAL_ARB_GCLK_CTRL_BIT)   | /* local_arb_gclk  */
			(0x0 << TIMING_MEAS_POL_CTRL_BIT) | /* timing_meas_pol */
			(0x0 << TOP_RESET_BIT));             /* reset */
		/* hist */
		wr(offset, VDIN_LUMA_HIST_GCLK_CTRL, 0x0); /* luma_hist_gclk */
		/* lfifo */
		wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0x0, 30, 2);/* gclk */
		wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0x1, 29, 1);/* soft_rst_en */
		/* vsc */
		wr_bits(offset, VDIN_VSC_CTRL, 0x0, 28, 4);/* gclk */
		/* hsc */
		wr_bits(offset, VDIN_HSC_CTRL, 0x0, 30, 2);/* gclk */
		/* phsc */
		wr_bits(offset, VDIN_PHSC_CTRL, 0x0, 30, 2);/* gclk */
		/* mat1 */
		wr_bits(offset, VDIN_MAT1_CTRL, 0x0, 30, 2);/* gclk */
		/* mat0 */
		wr_bits(offset, VDIN_MAT0_CTRL, 0x0, 30, 2);/* gclk */
		/* hist */
		wr_bits(offset, VDIN_LUMA_HIST_GCLK_CTRL, 0x0, 30, 2);/* gclk */
		/* pp top */
		wr(offset, VDIN_PP_TOP_GCLK_CTRL, (0x0 << 0)); /* pp top_gclk */
		/* vsk */
		wr_bits(offset, VDIN_VSK_CTRL, 0x0, 28, 4);/* gclk */
		/* hsk */
		wr_bits(offset, VDIN_HSK_CTRL, 0x0, 30, 2);/* gclk */
		/* dw top */
		wr(offset, VDIN_DW_TOP_GCLK_CTRL, (0x0 << 0)); /* dw top_gclk */
		/* hdr2 */
		wr(offset, VDIN_PP_HDR2_CLK_GATE, 0x0);/* hdr2 gclk */
		/* if top buf */
		wr_bits(offset, VDIN_IF_TOP_BUF_CTRL, 0x0, 30, 2); /* buf_gclk */
		/* local arb */
		wr_bits(offset, VDIN_LOCAL_ARB_CTRL, 0x0, 30, 2);   /* local_arb_gclk */
		wr_bits(offset, VDIN_LOCAL_ARB_WR_MODE, 0x0, 30, 2);/* wr_gate_clk */
	} else { /* off */
		/* local arb */
		wr_bits(offset, VDIN_LOCAL_ARB_CTRL, 0x1, 30, 2);   /* local_arb_gclk */
		wr_bits(offset, VDIN_LOCAL_ARB_WR_MODE, 0x1, 30, 2);/* wr_gate_clk */
		/* hist */
		wr(offset, VDIN_LUMA_HIST_GCLK_CTRL, 0x1); /* luma_hist_gclk */
		/* lfifo */
		wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0x1, 30, 2);/* gclk */
		wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0x1, 29, 1);/* soft_rst_en */
		/* vsc */
		wr_bits(offset, VDIN_VSC_CTRL, 0x5, 28, 4);/* gclk */
		/* hsc */
		wr_bits(offset, VDIN_HSC_CTRL, 0x1, 30, 2);/* gclk */
		/* phsc */
		wr_bits(offset, VDIN_PHSC_CTRL, 0x1, 30, 2);/* gclk */
		/* mat1 */
		wr_bits(offset, VDIN_MAT1_CTRL, 0x1, 30, 2);/* gclk */
		/* mat0 */
		wr_bits(offset, VDIN_MAT0_CTRL, 0x1, 30, 2);/* gclk */
		/* hist */
		wr_bits(offset, VDIN_LUMA_HIST_GCLK_CTRL, 0x1, 30, 2);/* gclk */
		/* vsk */
		wr_bits(offset, VDIN_VSK_CTRL, 0x5, 28, 4);/* gclk */
		/* hsk */
		wr_bits(offset, VDIN_HSK_CTRL, 0x1, 30, 2);/* gclk */
		/* hdr2 */
		wr(offset, VDIN_PP_HDR2_CLK_GATE, 0x55555555);/* hdr2 gclk */
		/* dw top */
		wr(offset, VDIN_DW_TOP_GCLK_CTRL,
			(0x55555555 << 0)); /* dw top_gclk */
		/* pp top */
		wr(offset, VDIN_PP_TOP_GCLK_CTRL,
			(0x55555555 << 0)); /* pp top_gclk */
		/* if top buf */
		wr_bits(offset, VDIN_IF_TOP_BUF_CTRL, 0x1, 30, 2); /* buf_gclk */
	}
}

void vdin_set_matrix_color_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	unsigned int mode = devp->matrix_pattern_mode;

	/*vdin bist mode RGB:black*/
	wr(offset, VDIN_MATRIX_CTRL, 0x4);
	wr(offset, VDIN_MATRIX_COEF00_01, 0x0);
	wr(offset, VDIN_MATRIX_COEF02_10, 0x0);
	wr(offset, VDIN_MATRIX_COEF11_12, 0x0);
	wr(offset, VDIN_MATRIX_COEF20_21, 0x0);
	wr(offset, VDIN_MATRIX_COEF22, 0x0);
	wr(offset, VDIN_MATRIX_PRE_OFFSET0_1, 0x0);
	wr(offset, VDIN_MATRIX_PRE_OFFSET2, 0x0);
	if (mode == 1) {
		wr(offset, VDIN_MATRIX_OFFSET0_1, 0x10f010f);
		wr(offset, VDIN_MATRIX_OFFSET2, 0x2ff);
	} else if (mode == 2) {
		wr(offset, VDIN_MATRIX_OFFSET0_1, 0x10f010f);
		wr(offset, VDIN_MATRIX_OFFSET2, 0x1ff);
	} else if (mode == 3) {
		wr(offset, VDIN_MATRIX_OFFSET0_1, 0x00003ff);
		wr(offset, VDIN_MATRIX_OFFSET2, 0x0);
	} else if (mode == 4) {
		wr(offset, VDIN_MATRIX_OFFSET0_1, 0x1ff01ff);
		wr(offset, VDIN_MATRIX_OFFSET2, 0x1ff);
	} else {
		wr(offset, VDIN_MATRIX_OFFSET0_1, 0x1ff010f);
		wr(offset, VDIN_MATRIX_OFFSET2, 0x2ff);
	}

	if (mode)
		wr(offset, VDIN_MATRIX_CTRL, 0x6);
	else
		wr(offset, VDIN_MATRIX_CTRL, 0x0);
	if (vdin_dbg_en)
		pr_info("%s offset:%d, md:%d\n", __func__, offset, mode);
}

/* only active under vdi6 loopback case */
void vdin_set_bist_pattern_s5(struct vdin_dev_s *devp, unsigned int on_off, unsigned int pat)
{
//	unsigned int offset = devp->addr_offset;
//	unsigned int de_start = 0x7;
//	unsigned int v_blank = 0x3f;
//	unsigned int h_blank = 0xff;

//	if (on_off) {
//		wr_bits(offset, VDIN_ASFIFO_CTRL0, de_start,
//			VDI6_BIST_DE_START_BIT, VDI6_BIST_DE_START_WID);
//		wr_bits(offset, VDIN_ASFIFO_CTRL0, v_blank,
//			VDI6_BIST_VBLANK_BIT, VDI6_BIST_VBLANK_WID);
//		wr_bits(offset, VDIN_ASFIFO_CTRL0, h_blank,
//			VDI6_BIST_HBLANK_BIT, VDI6_BIST_HBLANK_WID);
//
//		/* 0:horizontal gray scale, 1:vertical gray scale, 2,3:random data */
//		wr_bits(offset, VDIN_ASFIFO_CTRL0, pat,
//			VDI6_BIST_SEL_BIT, VDI6_BIST_SEL_WID);
//		wr_bits(offset, VDIN_ASFIFO_CTRL0, 1,
//			VDI6_BIST_EN_BIT, VDI6_BIST_EN_WID);
//	} else {
//		wr_bits(offset, VDIN_ASFIFO_CTRL0, 0,
//			VDI6_BIST_EN_BIT, VDI6_BIST_EN_WID);
//	}
}

void vdin_dmc_ctrl_s5(struct vdin_dev_s *devp, bool on_off)
{
	unsigned int reg;

	if (on_off) {
		/* for 4k nr, dmc vdin write band width not good
		 * dmc write 1 set to supper urgent
		 */
		if (devp->dtdata->hw_ver == VDIN_HW_T5 ||
		    devp->dtdata->hw_ver == VDIN_HW_T5D ||
		    devp->dtdata->hw_ver == VDIN_HW_T5W) {
			reg = READ_DMCREG(0x6c) & (~(1 << 17));
			if (!(reg & (1 << 18)))
				WRITE_DMCREG(0x6c, reg | (1 << 18));
		}
	} else {
		/* for 4k nr, dmc vdin write band width not good
		 * dmc write 1 set to urgent
		 */
		if (devp->dtdata->hw_ver == VDIN_HW_T5 ||
		    devp->dtdata->hw_ver == VDIN_HW_T5D ||
		    devp->dtdata->hw_ver == VDIN_HW_T5W) {
			reg = READ_DMCREG(0x6c) & (~(1 << 18));
			WRITE_DMCREG(0x6c, reg | (1 << 17));
		}
	}
}

void vdin_sw_reset_s5(struct vdin_dev_s *devp)
{
	//reg_reset[3:0]
	wr_bits(devp->addr_offset, VDIN_TOP_CTRL, 0xf, 0, 4);
	wr_bits(devp->addr_offset, VDIN_TOP_CTRL, 0x0, 0, 4);
}

/*
 *[0]:enable/disable;[1~31]:pattern mode 0/1/2/...
 */
void vdin_bist_s5(struct vdin_dev_s *devp, unsigned int mode)
{
	unsigned int bist_en = 0, test_pat = 0;
	unsigned int hblank = 0, vblank = 0, vde_start = 0;

	if (!(mode & 0x1)) {
		bist_en = 0;
	} else {
		bist_en = 1;
		test_pat = (mode >> 1);
		if (devp->h_active == 320) {
			//320x240
			hblank = 11;
			vblank = 8;
			vde_start = 2;
		} else {
			//1920x1080
			hblank = 128;//280;
			vblank = 128;//45;
			vde_start = 2;
		}
	}
	wr(devp->addr_offset, VDIN_IF_TOP_PTGEN_CTRL,
		(bist_en << 0) | (test_pat << 1) | (hblank << 4) |
		(vblank << 12) | (vde_start << 20));
}
