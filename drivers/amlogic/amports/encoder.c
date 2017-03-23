/*
 * drivers/amlogic/amports/encoder.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>
#include <linux/amlogic/amports/ptsserv.h>
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/canvas/canvas_mgr.h>
#include <linux/amlogic/codec_mm/codec_mm.h>

#include "vdec_reg.h"
#include "vdec.h"
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/dma-contiguous.h>
#include <linux/kthread.h>
#include <linux/sched/rt.h>
#include "amports_config.h"
#include "encoder.h"
#include "amvdec.h"
#include "amlog.h"
#include "amports_priv.h"
#include <linux/of_reserved_mem.h>
#ifdef CONFIG_AM_JPEG_ENCODER
#include "jpegenc.h"
#endif

#define ENCODE_NAME "encoder"
#define AMVENC_CANVAS_INDEX 0xE4
#define AMVENC_CANVAS_MAX_INDEX 0xEF

#define MIN_SIZE amvenc_buffspec[0].min_buffsize
#define DUMP_INFO_BYTES_PER_MB 80

#define ADJUSTED_QP_FLAG 64

static s32 avc_device_major;
static struct device *amvenc_avc_dev;
#define DRIVER_NAME "amvenc_avc"
#define CLASS_NAME "amvenc_avc"
#define DEVICE_NAME "amvenc_avc"

static struct encode_manager_s encode_manager;

#define MULTI_SLICE_MC
#define H264_ENC_CBR
/* #define MORE_MODULE_PARAM */

#define ENC_CANVAS_OFFSET  AMVENC_CANVAS_INDEX

#define UCODE_MODE_FULL 0

/* #define ENABLE_IGNORE_FUNCTION */

static u32 ie_me_mb_type;
static u32 ie_me_mode;
static u32 ie_pippeline_block = 3;
static u32 ie_cur_ref_sel;
/* static u32 avc_endian = 6; */
static u32 clock_level = 5;

static u32 encode_print_level = LOG_DEBUG;
static u32 no_timeout;
static int nr_mode = -1;
static u32 qp_table_debug;

static u32 me_mv_merge_ctl =
	(0x1 << 31)  |  /* [31] me_merge_mv_en_16 */
	(0x1 << 30)  |  /* [30] me_merge_small_mv_en_16 */
	(0x1 << 29)  |  /* [29] me_merge_flex_en_16 */
	(0x1 << 28)  |  /* [28] me_merge_sad_en_16 */
	(0x1 << 27)  |  /* [27] me_merge_mv_en_8 */
	(0x1 << 26)  |  /* [26] me_merge_small_mv_en_8 */
	(0x1 << 25)  |  /* [25] me_merge_flex_en_8 */
	(0x1 << 24)  |  /* [24] me_merge_sad_en_8 */
	/* [23:18] me_merge_mv_diff_16 - MV diff <= n pixel can be merged */
	(0x12 << 18) |
	/* [17:12] me_merge_mv_diff_8 - MV diff <= n pixel can be merged */
	(0x2b << 12) |
	/* [11:0] me_merge_min_sad - SAD >= 0x180 can be merged with other MV */
	(0x80 << 0);
	/* ( 0x4 << 18)  |
	// [23:18] me_merge_mv_diff_16 - MV diff <= n pixel can be merged */
	/* ( 0x3f << 12)  |
	// [17:12] me_merge_mv_diff_8 - MV diff <= n pixel can be merged */
	/* ( 0xc0 << 0);
	// [11:0] me_merge_min_sad - SAD >= 0x180 can be merged with other MV */

static u32 me_mv_weight_01 = (0x40 << 24) | (0x30 << 16) | (0x20 << 8) | 0x30;
static u32 me_mv_weight_23 = (0x40 << 8) | 0x30;
static u32 me_sad_range_inc = 0x03030303;
static u32 me_step0_close_mv = 0x003ffc21;
static u32 me_f_skip_sad;
static u32 me_f_skip_weight;
static u32 me_sad_enough_01;/* 0x00018010; */
static u32 me_sad_enough_23;/* 0x00000020; */

/* [31:0] NUM_ROWS_PER_SLICE_P */
/* [15:0] NUM_ROWS_PER_SLICE_I */
static u32 fixed_slice_cfg;

/* y tnr */
static unsigned int y_tnr_mc_en = 1;
static unsigned int y_tnr_txt_mode;
static unsigned int y_tnr_mot_sad_margin = 1;
static unsigned int y_tnr_mot_cortxt_rate = 1;
static unsigned int y_tnr_mot_distxt_ofst = 5;
static unsigned int y_tnr_mot_distxt_rate = 4;
static unsigned int y_tnr_mot_dismot_ofst = 4;
static unsigned int y_tnr_mot_frcsad_lock = 8;
static unsigned int y_tnr_mot2alp_frc_gain = 10;
static unsigned int y_tnr_mot2alp_nrm_gain = 216;
static unsigned int y_tnr_mot2alp_dis_gain = 128;
static unsigned int y_tnr_mot2alp_dis_ofst = 32;
static unsigned int y_tnr_alpha_min = 32;
static unsigned int y_tnr_alpha_max = 63;
static unsigned int y_tnr_deghost_os;
/* c tnr */
static unsigned int c_tnr_mc_en = 1;
static unsigned int c_tnr_txt_mode;
static unsigned int c_tnr_mot_sad_margin = 1;
static unsigned int c_tnr_mot_cortxt_rate = 1;
static unsigned int c_tnr_mot_distxt_ofst = 5;
static unsigned int c_tnr_mot_distxt_rate = 4;
static unsigned int c_tnr_mot_dismot_ofst = 4;
static unsigned int c_tnr_mot_frcsad_lock = 8;
static unsigned int c_tnr_mot2alp_frc_gain = 10;
static unsigned int c_tnr_mot2alp_nrm_gain = 216;
static unsigned int c_tnr_mot2alp_dis_gain = 128;
static unsigned int c_tnr_mot2alp_dis_ofst = 32;
static unsigned int c_tnr_alpha_min = 32;
static unsigned int c_tnr_alpha_max = 63;
static unsigned int c_tnr_deghost_os;
/* y snr */
static unsigned int y_snr_err_norm = 1;
static unsigned int y_snr_gau_bld_core = 1;
static int y_snr_gau_bld_ofst = -1;
static unsigned int y_snr_gau_bld_rate = 48;
static unsigned int y_snr_gau_alp0_min;
static unsigned int y_snr_gau_alp0_max = 63;
static unsigned int y_bld_beta2alp_rate = 16;
static unsigned int y_bld_beta_min;
static unsigned int y_bld_beta_max = 63;
/* c snr */
static unsigned int c_snr_err_norm = 1;
static unsigned int c_snr_gau_bld_core = 1;
static int c_snr_gau_bld_ofst = -1;
static unsigned int c_snr_gau_bld_rate = 48;
static unsigned int c_snr_gau_alp0_min;
static unsigned int c_snr_gau_alp0_max = 63;
static unsigned int c_bld_beta2alp_rate = 16;
static unsigned int c_bld_beta_min;
static unsigned int c_bld_beta_max = 63;

static DEFINE_SPINLOCK(lock);

#define ADV_MV_LARGE_16x8 1
#define ADV_MV_LARGE_8x16 1
#define ADV_MV_LARGE_16x16 1

/* me weight offset should not very small, it used by v1 me module. */
/* the min real sad for me is 16 by hardware. */
#define ME_WEIGHT_OFFSET 0x520
#define I4MB_WEIGHT_OFFSET 0x655
#define I16MB_WEIGHT_OFFSET 0x560

#define ADV_MV_16x16_WEIGHT 0x080
#define ADV_MV_16_8_WEIGHT 0x0e0
#define ADV_MV_8x8_WEIGHT 0x240
#define ADV_MV_4x4x4_WEIGHT 0x3000

#define IE_SAD_SHIFT_I16 0x001
#define IE_SAD_SHIFT_I4 0x001
#define ME_SAD_SHIFT_INTER 0x001

#define STEP_2_SKIP_SAD 0
#define STEP_1_SKIP_SAD 0
#define STEP_0_SKIP_SAD 0
#define STEP_2_SKIP_WEIGHT 0
#define STEP_1_SKIP_WEIGHT 0
#define STEP_0_SKIP_WEIGHT 0

#define ME_SAD_RANGE_0 0x1 /* 0x0 */
#define ME_SAD_RANGE_1 0x0
#define ME_SAD_RANGE_2 0x0
#define ME_SAD_RANGE_3 0x0

/* use 0 for v3, 0x18 for v2 */
#define ME_MV_PRE_WEIGHT_0 0x18
/* use 0 for v3, 0x18 for v2 */
#define ME_MV_PRE_WEIGHT_1 0x18
#define ME_MV_PRE_WEIGHT_2 0x0
#define ME_MV_PRE_WEIGHT_3 0x0

/* use 0 for v3, 0x18 for v2 */
#define ME_MV_STEP_WEIGHT_0 0x18
/* use 0 for v3, 0x18 for v2 */
#define ME_MV_STEP_WEIGHT_1 0x18
#define ME_MV_STEP_WEIGHT_2 0x0
#define ME_MV_STEP_WEIGHT_3 0x0

#define ME_SAD_ENOUGH_0_DATA 0x00
#define ME_SAD_ENOUGH_1_DATA 0x04
#define ME_SAD_ENOUGH_2_DATA 0x11
#define ADV_MV_8x8_ENOUGH_DATA 0x20

/* V4_COLOR_BLOCK_FIX */
#define V3_FORCE_SKIP_SAD_0 0x10
/* 4 Blocks */
#define V3_FORCE_SKIP_SAD_1 0x60
/* 16 Blocks + V3_SKIP_WEIGHT_2 */
#define V3_FORCE_SKIP_SAD_2 0x250
/* almost disable it -- use t_lac_coeff_2 output to F_ZERO is better */
#define V3_ME_F_ZERO_SAD (ME_WEIGHT_OFFSET + 0x10)

#define V3_IE_F_ZERO_SAD_I16 (I16MB_WEIGHT_OFFSET + 0x10)
#define V3_IE_F_ZERO_SAD_I4 (I4MB_WEIGHT_OFFSET + 0x20)

#define V3_SKIP_WEIGHT_0 0x10
/* 4 Blocks  8 seperate search sad can be very low */
#define V3_SKIP_WEIGHT_1 0x8 /* (4 * ME_MV_STEP_WEIGHT_1 + 0x100) */
#define V3_SKIP_WEIGHT_2 0x3

#define V3_LEVEL_1_F_SKIP_MAX_SAD 0x0
#define V3_LEVEL_1_SKIP_MAX_SAD 0x6

#define I4_ipred_weight_most   0x18
#define I4_ipred_weight_else   0x28

#define C_ipred_weight_V       0x04
#define C_ipred_weight_H       0x08
#define C_ipred_weight_DC      0x0c

#define I16_ipred_weight_V       0x04
#define I16_ipred_weight_H       0x08
#define I16_ipred_weight_DC      0x0c

/* 0x00 same as disable */
#define v3_left_small_max_ie_sad 0x00
#define v3_left_small_max_me_sad 0x40

#define v5_use_small_diff_cnt 0
#define v5_simple_mb_inter_all_en 1
#define v5_simple_mb_inter_8x8_en 1
#define v5_simple_mb_inter_16_8_en 1
#define v5_simple_mb_inter_16x16_en 1
#define v5_simple_mb_intra_en 1
#define v5_simple_mb_C_en 0
#define v5_simple_mb_Y_en 1
#define v5_small_diff_Y 0x10
#define v5_small_diff_C 0x18
/* shift 8-bits, 2, 1, 0, -1, -2, -3, -4 */
#define v5_simple_dq_setting 0x43210fed
#define v5_simple_me_weight_setting 0

#ifdef H264_ENC_CBR
#define CBR_TABLE_SIZE  0x800
#define CBR_SHORT_SHIFT 12 /* same as disable */
#define CBR_LONG_MB_NUM 2
#define START_TABLE_ID 8
#define CBR_LONG_THRESH 4
#endif

static u32 v3_mv_sad[64] = {
	/* For step0 */
	0x00000004,
	0x00010008,
	0x00020010,
	0x00030018,
	0x00040020,
	0x00050028,
	0x00060038,
	0x00070048,
	0x00080058,
	0x00090068,
	0x000a0080,
	0x000b0098,
	0x000c00b0,
	0x000d00c8,
	0x000e00e8,
	0x000f0110,
	/* For step1 */
	0x00100002,
	0x00110004,
	0x00120008,
	0x0013000c,
	0x00140010,
	0x00150014,
	0x0016001c,
	0x00170024,
	0x0018002c,
	0x00190034,
	0x001a0044,
	0x001b0054,
	0x001c0064,
	0x001d0074,
	0x001e0094,
	0x001f00b4,
	/* For step2 */
	0x00200006,
	0x0021000c,
	0x0022000c,
	0x00230018,
	0x00240018,
	0x00250018,
	0x00260018,
	0x00270030,
	0x00280030,
	0x00290030,
	0x002a0030,
	0x002b0030,
	0x002c0030,
	0x002d0030,
	0x002e0030,
	0x002f0050,
	/* For step2 4x4-8x8 */
	0x00300001,
	0x00310002,
	0x00320002,
	0x00330004,
	0x00340004,
	0x00350004,
	0x00360004,
	0x00370006,
	0x00380006,
	0x00390006,
	0x003a0006,
	0x003b0006,
	0x003c0006,
	0x003d0006,
	0x003e0006,
	0x003f0006
};

static struct BuffInfo_s amvenc_buffspec[] = {
	{
		.lev_id = 0,
		.max_width = 1920,
		.max_height = 1088,
		.min_buffsize = 0x1400000,
		.dct = {
			.buf_start = 0,
			.buf_size = 0x800000, /* 1920x1088x4 */
		},
		.dec0_y = {
			.buf_start = 0x800000,
			.buf_size = 0x300000,
		},
		.dec1_y = {
			.buf_start = 0xb00000,
			.buf_size = 0x300000,
		},
		.assit = {
			.buf_start = 0xe10000,
			.buf_size = 0xc0000,
		},
		.bitstream = {
			.buf_start = 0xf00000,
			.buf_size = 0x100000,
		},
		.scale_buff = {
			.buf_start = 0x1000000,
			.buf_size = 0x300000,
		},
		.dump_info = {
			.buf_start = 0x1300000,
			.buf_size = 0xa0000, /* (1920x1088/256)x80 */
		},
		.cbr_info = {
			.buf_start = 0x13b0000,
			.buf_size = 0x2000,
		}
	}
};

enum ucode_type_e {
	UCODE_GX,
	UCODE_GXTV,
	UCODE_GXL,
	UCODE_TXL,
	UCODE_MAX
};

const char *ucode_name[] = {
	"h264_enc_mc_gx",
	"h264_enc_mc_gxtv",
	"h264_enc_mc_gxl",
	"h264_enc_mc_txl",
};

static void dma_flush(u32 buf_start, u32 buf_size);
static void cache_flush(u32 buf_start , u32 buf_size);

static const char *select_ucode(u32 ucode_index)
{
	enum ucode_type_e ucode = UCODE_GX;
	switch (ucode_index) {
	case UCODE_MODE_FULL:
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)
			ucode = UCODE_TXL;
		else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
			ucode = UCODE_GXL;
		else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB)
			ucode = UCODE_GXTV;
		else /* if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) */
			ucode = UCODE_GX;
		break;
		break;
	default:
		break;
	}
	return (const char *)ucode_name[ucode];
}

static void hcodec_prog_qtbl(struct encode_wq_s *wq)
{
	WRITE_HREG(HCODEC_Q_QUANT_CONTROL,
		(0 << 23) |  /* quant_table_addr */
		(1 << 22));  /* quant_table_addr_update */

	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i4[0]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i4[1]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i4[2]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i4[3]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i4[4]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i4[5]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i4[6]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i4[7]);

	WRITE_HREG(HCODEC_Q_QUANT_CONTROL,
		(8 << 23) |  /* quant_table_addr */
		(1 << 22));  /* quant_table_addr_update */

	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i16[0]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i16[1]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i16[2]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i16[3]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i16[4]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i16[5]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i16[6]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_i16[7]);

	WRITE_HREG(HCODEC_Q_QUANT_CONTROL,
		(16 << 23) | /* quant_table_addr */
		(1 << 22));  /* quant_table_addr_update */

	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_me[0]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_me[1]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_me[2]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_me[3]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_me[4]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_me[5]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_me[6]);
	WRITE_HREG(HCODEC_QUANT_TABLE_DATA,
		wq->quant_tbl_me[7]);
	return;
}

static void InitEncodeWeight(void)
{
	me_mv_merge_ctl =
		(0x1 << 31)  |  /* [31] me_merge_mv_en_16 */
		(0x1 << 30)  |  /* [30] me_merge_small_mv_en_16 */
		(0x1 << 29)  |  /* [29] me_merge_flex_en_16 */
		(0x1 << 28)  |  /* [28] me_merge_sad_en_16 */
		(0x1 << 27)  |  /* [27] me_merge_mv_en_8 */
		(0x1 << 26)  |  /* [26] me_merge_small_mv_en_8 */
		(0x1 << 25)  |  /* [25] me_merge_flex_en_8 */
		(0x1 << 24)  |  /* [24] me_merge_sad_en_8 */
		(0x12 << 18) |
		/* [23:18] me_merge_mv_diff_16 - MV diff
			<= n pixel can be merged */
		(0x2b << 12) |
		/* [17:12] me_merge_mv_diff_8 - MV diff
			<= n pixel can be merged */
		(0x80 << 0);
		/* [11:0] me_merge_min_sad - SAD
			>= 0x180 can be merged with other MV */

	me_mv_weight_01 = (ME_MV_STEP_WEIGHT_1 << 24) |
		(ME_MV_PRE_WEIGHT_1 << 16) |
		(ME_MV_STEP_WEIGHT_0 << 8) |
		(ME_MV_PRE_WEIGHT_0 << 0);

	me_mv_weight_23 = (ME_MV_STEP_WEIGHT_3 << 24) |
		(ME_MV_PRE_WEIGHT_3  << 16) |
		(ME_MV_STEP_WEIGHT_2 << 8) |
		(ME_MV_PRE_WEIGHT_2  << 0);

	me_sad_range_inc = (ME_SAD_RANGE_3 << 24) |
		(ME_SAD_RANGE_2 << 16) |
		(ME_SAD_RANGE_1 << 8) |
		(ME_SAD_RANGE_0 << 0);

	me_step0_close_mv = (0x100 << 10) |
		/* me_step0_big_sad -- two MV sad
		  diff bigger will use use 1 */
		(2 << 5) | /* me_step0_close_mv_y */
		(2 << 0); /* me_step0_close_mv_x */

	me_f_skip_sad = (0x00 << 24) | /* force_skip_sad_3 */
		(STEP_2_SKIP_SAD << 16) | /* force_skip_sad_2 */
		(STEP_1_SKIP_SAD << 8) | /* force_skip_sad_1 */
		(STEP_0_SKIP_SAD << 0); /* force_skip_sad_0 */

	me_f_skip_weight = (0x00 << 24) | /* force_skip_weight_3 */
		/* force_skip_weight_2 */
		(STEP_2_SKIP_WEIGHT << 16) |
		/* force_skip_weight_1 */
		(STEP_1_SKIP_WEIGHT << 8) |
		/* force_skip_weight_0 */
		(STEP_0_SKIP_WEIGHT << 0);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
		me_f_skip_sad = 0;
		me_f_skip_weight = 0;
		me_mv_weight_01 = 0;
		me_mv_weight_23 = 0;
	}

	me_sad_enough_01 = (ME_SAD_ENOUGH_1_DATA << 12) |
		/* me_sad_enough_1 */
		(ME_SAD_ENOUGH_0_DATA << 0) |
		/* me_sad_enough_0 */
		(0 << 12) | /* me_sad_enough_1 */
		(0 << 0); /* me_sad_enough_0 */

	me_sad_enough_23 = (ADV_MV_8x8_ENOUGH_DATA << 12) |
		/* adv_mv_8x8_enough */
		(ME_SAD_ENOUGH_2_DATA << 0) |
		/* me_sad_enough_2 */
		(0 << 12) | /* me_sad_enough_3 */
		(0 << 0); /* me_sad_enough_2 */
}

/*output stream buffer setting*/
static void avc_init_output_buffer(struct encode_wq_s *wq)
{
	WRITE_HREG(HCODEC_VLC_VB_MEM_CTL,
		((1 << 31) | (0x3f << 24) |
		(0x20 << 16) | (2 << 0)));
	WRITE_HREG(HCODEC_VLC_VB_START_PTR,
		wq->mem.BitstreamStart);
	WRITE_HREG(HCODEC_VLC_VB_WR_PTR,
		wq->mem.BitstreamStart);
	WRITE_HREG(HCODEC_VLC_VB_SW_RD_PTR,
		wq->mem.BitstreamStart);
	WRITE_HREG(HCODEC_VLC_VB_END_PTR,
		wq->mem.BitstreamEnd);
	WRITE_HREG(HCODEC_VLC_VB_CONTROL, 1);
	WRITE_HREG(HCODEC_VLC_VB_CONTROL,
		((0 << 14) | (7 << 3) |
		(1 << 1) | (0 << 0)));
}

/*input dct buffer setting*/
static void avc_init_input_buffer(struct encode_wq_s *wq)
{
	WRITE_HREG(HCODEC_QDCT_MB_START_PTR,
		wq->mem.dct_buff_start_addr);
	WRITE_HREG(HCODEC_QDCT_MB_END_PTR,
		wq->mem.dct_buff_end_addr);
	WRITE_HREG(HCODEC_QDCT_MB_WR_PTR,
		wq->mem.dct_buff_start_addr);
	WRITE_HREG(HCODEC_QDCT_MB_RD_PTR,
		wq->mem.dct_buff_start_addr);
	WRITE_HREG(HCODEC_QDCT_MB_BUFF, 0);
}

/*input reference buffer setting*/
static void avc_init_reference_buffer(s32 canvas)
{
	WRITE_HREG(HCODEC_ANC0_CANVAS_ADDR, canvas);
	WRITE_HREG(HCODEC_VLC_HCMD_CONFIG, 0);
}

static void avc_init_assit_buffer(struct encode_wq_s *wq)
{
	WRITE_HREG(MEM_OFFSET_REG, wq->mem.assit_buffer_offset);
}

/*deblock buffer setting, same as INI_CANVAS*/
static void avc_init_dblk_buffer(s32 canvas)
{
	WRITE_HREG(HCODEC_REC_CANVAS_ADDR, canvas);
	WRITE_HREG(HCODEC_DBKR_CANVAS_ADDR, canvas);
	WRITE_HREG(HCODEC_DBKW_CANVAS_ADDR, canvas);
}

static void avc_init_encoder(struct encode_wq_s *wq, bool idr)
{
	WRITE_HREG(HCODEC_VLC_TOTAL_BYTES, 0);
	WRITE_HREG(HCODEC_VLC_CONFIG, 0x07);
	WRITE_HREG(HCODEC_VLC_INT_CONTROL, 0);

	WRITE_HREG(HCODEC_ASSIST_AMR1_INT0, 0x15);
	WRITE_HREG(HCODEC_ASSIST_AMR1_INT1, 0x8);
	WRITE_HREG(HCODEC_ASSIST_AMR1_INT3, 0x14);

	WRITE_HREG(IDR_PIC_ID, wq->pic.idr_pic_id);
	WRITE_HREG(FRAME_NUMBER,
		(idr == true) ? 0 : wq->pic.frame_number);
	WRITE_HREG(PIC_ORDER_CNT_LSB,
		(idr == true) ? 0 : wq->pic.pic_order_cnt_lsb);

	WRITE_HREG(LOG2_MAX_PIC_ORDER_CNT_LSB,
		wq->pic.log2_max_pic_order_cnt_lsb);
	WRITE_HREG(LOG2_MAX_FRAME_NUM,
		wq->pic.log2_max_frame_num);
	WRITE_HREG(ANC0_BUFFER_ID, 0);
	WRITE_HREG(QPPICTURE, wq->pic.init_qppicture);
}

static void avc_canvas_init(struct encode_wq_s *wq)
{
	u32 canvas_width, canvas_height;
	u32 start_addr = wq->mem.buf_start;

	canvas_width = ((wq->pic.encoder_width + 31) >> 5) << 5;
	canvas_height = ((wq->pic.encoder_height + 15) >> 4) << 4;

	canvas_config(ENC_CANVAS_OFFSET,
	      start_addr + wq->mem.bufspec.dec0_y.buf_start,
	      canvas_width, canvas_height,
	      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	canvas_config(1 + ENC_CANVAS_OFFSET,
	      start_addr + wq->mem.bufspec.dec0_uv.buf_start,
	      canvas_width, canvas_height / 2,
	      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	/*here the third plane use the same address as the second plane*/
	canvas_config(2 + ENC_CANVAS_OFFSET,
	      start_addr + wq->mem.bufspec.dec0_uv.buf_start,
	      canvas_width, canvas_height / 2,
	      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);

	canvas_config(3 + ENC_CANVAS_OFFSET,
	      start_addr + wq->mem.bufspec.dec1_y.buf_start,
	      canvas_width, canvas_height,
	      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	canvas_config(4 + ENC_CANVAS_OFFSET,
	      start_addr + wq->mem.bufspec.dec1_uv.buf_start,
	      canvas_width, canvas_height / 2,
	      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	/*here the third plane use the same address as the second plane*/
	canvas_config(5 + ENC_CANVAS_OFFSET,
	      start_addr + wq->mem.bufspec.dec1_uv.buf_start,
	      canvas_width, canvas_height / 2,
	      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
}

static void avc_buffspec_init(struct encode_wq_s *wq)
{
	u32 canvas_width, canvas_height;
	u32 start_addr = wq->mem.buf_start;
	u32 mb_w = (wq->pic.encoder_width + 15) >> 4;
	u32 mb_h = (wq->pic.encoder_height + 15) >> 4;
	u32 mbs = mb_w * mb_h;

	canvas_width = ((wq->pic.encoder_width + 31) >> 5) << 5;
	canvas_height = ((wq->pic.encoder_height + 15) >> 4) << 4;

	wq->mem.dct_buff_start_addr = start_addr +
		wq->mem.bufspec.dct.buf_start;
	wq->mem.dct_buff_end_addr =
		wq->mem.dct_buff_start_addr +
		wq->mem.bufspec.dct.buf_size - 1;
	enc_pr(LOG_INFO, "dct_buff_start_addr is 0x%x, wq:%p.\n",
		wq->mem.dct_buff_start_addr, (void *)wq);

	wq->mem.bufspec.dec0_uv.buf_start =
		wq->mem.bufspec.dec0_y.buf_start +
		canvas_width * canvas_height;
	wq->mem.bufspec.dec0_uv.buf_size = canvas_width * canvas_height / 2;
	wq->mem.bufspec.dec1_uv.buf_start =
		wq->mem.bufspec.dec1_y.buf_start +
		canvas_width * canvas_height;
	wq->mem.bufspec.dec1_uv.buf_size = canvas_width * canvas_height / 2;
	wq->mem.assit_buffer_offset = start_addr +
		wq->mem.bufspec.assit.buf_start;
	enc_pr(LOG_INFO, "assit_buffer_offset is 0x%x, wq: %p.\n",
		wq->mem.assit_buffer_offset, (void *)wq);
	/*output stream buffer config*/
	wq->mem.BitstreamStart = start_addr +
		wq->mem.bufspec.bitstream.buf_start;
	wq->mem.BitstreamEnd =
		wq->mem.BitstreamStart +
		wq->mem.bufspec.bitstream.buf_size - 1;
	enc_pr(LOG_INFO, "BitstreamStart is 0x%x, wq: %p.\n",
		wq->mem.BitstreamStart, (void *)wq);

	wq->mem.scaler_buff_start_addr =
		wq->mem.buf_start + wq->mem.bufspec.scale_buff.buf_start;
	wq->mem.dump_info_ddr_start_addr =
		wq->mem.buf_start + wq->mem.bufspec.dump_info.buf_start;
	wq->mem.dump_info_ddr_size =
		DUMP_INFO_BYTES_PER_MB * mbs;
	wq->mem.dump_info_ddr_size =
		(wq->mem.dump_info_ddr_size + PAGE_SIZE - 1)
		& ~(PAGE_SIZE - 1);
	wq->mem.cbr_info_ddr_start_addr =
		wq->mem.buf_start + wq->mem.bufspec.cbr_info.buf_start;
	wq->mem.cbr_info_ddr_size =
		wq->mem.bufspec.cbr_info.buf_size;

	wq->mem.dblk_buf_canvas =
		((ENC_CANVAS_OFFSET + 2) << 16) |
		((ENC_CANVAS_OFFSET + 1) << 8) |
		(ENC_CANVAS_OFFSET);
	wq->mem.ref_buf_canvas =
		((ENC_CANVAS_OFFSET + 5) << 16) |
		((ENC_CANVAS_OFFSET + 4) << 8) |
		(ENC_CANVAS_OFFSET + 3);
}

static void avc_init_ie_me_parameter(struct encode_wq_s *wq, u32 quant)
{
	ie_cur_ref_sel = 0;
	ie_pippeline_block = 12;
	/* currently disable half and sub pixel */
	ie_me_mode =
		(ie_pippeline_block & IE_PIPPELINE_BLOCK_MASK) <<
	      IE_PIPPELINE_BLOCK_SHIFT;

	WRITE_HREG(IE_ME_MODE, ie_me_mode);
	WRITE_HREG(IE_REF_SEL, ie_cur_ref_sel);
	WRITE_HREG(IE_ME_MB_TYPE, ie_me_mb_type);
#ifdef MULTI_SLICE_MC
	if (fixed_slice_cfg)
		WRITE_HREG(FIXED_SLICE_CFG, fixed_slice_cfg);
	else if (wq->pic.rows_per_slice !=
			(wq->pic.encoder_height + 15) >> 4) {
		u32 mb_per_slice = (wq->pic.encoder_height + 15) >> 4;
		mb_per_slice = mb_per_slice * wq->pic.rows_per_slice;
		WRITE_HREG(FIXED_SLICE_CFG, mb_per_slice);
	} else
		WRITE_HREG(FIXED_SLICE_CFG, 0);
#else
	WRITE_HREG(FIXED_SLICE_CFG, 0);
#endif
}

/* for temp */
#define HCODEC_MFDIN_REGC_MBLP		(HCODEC_MFDIN_REGB_AMPC + 0x1)
#define HCODEC_MFDIN_REG0D			(HCODEC_MFDIN_REGB_AMPC + 0x2)
#define HCODEC_MFDIN_REG0E			(HCODEC_MFDIN_REGB_AMPC + 0x3)
#define HCODEC_MFDIN_REG0F			(HCODEC_MFDIN_REGB_AMPC + 0x4)
#define HCODEC_MFDIN_REG10			(HCODEC_MFDIN_REGB_AMPC + 0x5)
#define HCODEC_MFDIN_REG11			(HCODEC_MFDIN_REGB_AMPC + 0x6)
#define HCODEC_MFDIN_REG12			(HCODEC_MFDIN_REGB_AMPC + 0x7)
#define HCODEC_MFDIN_REG13			(HCODEC_MFDIN_REGB_AMPC + 0x8)
#define HCODEC_MFDIN_REG14			(HCODEC_MFDIN_REGB_AMPC + 0x9)
#define HCODEC_MFDIN_REG15			(HCODEC_MFDIN_REGB_AMPC + 0xa)
#define HCODEC_MFDIN_REG16			(HCODEC_MFDIN_REGB_AMPC + 0xb)

static void mfdin_basic(u32 input, u8 iformat,
	u8 oformat, u32 picsize_x, u32 picsize_y,
	u8 r2y_en, u8 nr, u8 ifmt_extra)
{
	u8 dsample_en; /* Downsample Enable */
	u8 interp_en;  /* Interpolation Enable */
	u8 y_size;     /* 0:16 Pixels for y direction pickup; 1:8 pixels */
	u8 r2y_mode;   /* RGB2YUV Mode, range(0~3) */
	/* mfdin_reg3_canv[25:24];
	  // bytes per pixel in x direction for index0, 0:half 1:1 2:2 3:3 */
	u8 canv_idx0_bppx;
	/* mfdin_reg3_canv[27:26];
	  // bytes per pixel in x direction for index1-2, 0:half 1:1 2:2 3:3 */
	u8 canv_idx1_bppx;
	/* mfdin_reg3_canv[29:28];
	  // bytes per pixel in y direction for index0, 0:half 1:1 2:2 3:3 */
	u8 canv_idx0_bppy;
	/* mfdin_reg3_canv[31:30];
	  // bytes per pixel in y direction for index1-2, 0:half 1:1 2:2 3:3 */
	u8 canv_idx1_bppy;
	u8 ifmt444, ifmt422, ifmt420, linear_bytes4p;
	u8 nr_enable;
	u8 cfg_y_snr_en;
	u8 cfg_y_tnr_en;
	u8 cfg_c_snr_en;
	u8 cfg_c_tnr_en;
	u32 linear_bytesperline;
	s32 reg_offset;
	bool linear_enable = false;
	bool format_err = false;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL) {
		if ((iformat == 7) && (ifmt_extra > 2))
			format_err = true;
	} else if (iformat == 7)
		format_err = true;

	if (format_err) {
		enc_pr(LOG_ERROR,
			"mfdin format err, iformat:%d, ifmt_extra:%d\n",
			iformat, ifmt_extra);
		return;
	}
	if (iformat != 7)
		ifmt_extra = 0;

	ifmt444 = ((iformat == 1) || (iformat == 5) || (iformat == 8) ||
		(iformat == 9) || (iformat == 12)) ? 1 : 0;
	if (iformat == 7 && ifmt_extra == 1)
		ifmt444 = 1;
	ifmt422 = ((iformat == 0) || (iformat == 10)) ? 1 : 0;
	if (iformat == 7 && ifmt_extra != 1)
		ifmt422 = 1;
	ifmt420 = ((iformat == 2) || (iformat == 3) || (iformat == 4) ||
		(iformat == 11)) ? 1 : 0;
	dsample_en = ((ifmt444 && (oformat != 2)) ||
		(ifmt422 && (oformat == 0))) ? 1 : 0;
	interp_en = ((ifmt422 && (oformat == 2)) ||
		(ifmt420 && (oformat != 0))) ? 1 : 0;
	y_size = (oformat != 0) ? 1 : 0;
	if (iformat == 12)
		y_size = 0;
	r2y_mode = (r2y_en == 1) ? 1 : 0; /* Fixed to 1 (TODO) */
	canv_idx0_bppx = (iformat == 1) ? 3 : (iformat == 0) ? 2 : 1;
	canv_idx1_bppx = (iformat == 4) ? 0 : 1;
	canv_idx0_bppy = 1;
	canv_idx1_bppy = (iformat == 5) ? 1 : 0;

	if ((iformat == 8) || (iformat == 9) || (iformat == 12))
		linear_bytes4p = 3;
	else if (iformat == 10)
		linear_bytes4p = 2;
	else if (iformat == 11)
		linear_bytes4p = 1;
	else
		linear_bytes4p = 0;
	if (iformat == 12)
		linear_bytesperline = picsize_x * 4;
	else
		linear_bytesperline = picsize_x * linear_bytes4p;

	if (iformat < 8)
		linear_enable = false;
	else
		linear_enable = true;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		reg_offset = -8;
		/* nr_mode: 0:Disabled 1:SNR Only 2:TNR Only 3:3DNR */
		nr_enable = (nr) ? 1 : 0;
		cfg_y_snr_en = ((nr == 1) || (nr == 3)) ? 1 : 0;
		cfg_y_tnr_en = ((nr == 2) || (nr == 3)) ? 1 : 0;
		cfg_c_snr_en = cfg_y_snr_en;
		/* cfg_c_tnr_en = cfg_y_tnr_en; */
		cfg_c_tnr_en = 0;

		/* NR For Y */
		WRITE_HREG((HCODEC_MFDIN_REG0D + reg_offset),
			((cfg_y_snr_en << 0) |
			(y_snr_err_norm << 1) |
			(y_snr_gau_bld_core << 2) |
			(((y_snr_gau_bld_ofst) & 0xff) << 6) |
			(y_snr_gau_bld_rate << 14) |
			(y_snr_gau_alp0_min << 20) |
			(y_snr_gau_alp0_max << 26)));
		WRITE_HREG((HCODEC_MFDIN_REG0E + reg_offset),
			((cfg_y_tnr_en << 0) |
			(y_tnr_mc_en << 1) |
			(y_tnr_txt_mode << 2) |
			(y_tnr_mot_sad_margin << 3) |
			(y_tnr_alpha_min << 7) |
			(y_tnr_alpha_max << 13) |
			(y_tnr_deghost_os << 19)));
		WRITE_HREG((HCODEC_MFDIN_REG0F + reg_offset),
			((y_tnr_mot_cortxt_rate << 0) |
			(y_tnr_mot_distxt_ofst << 8) |
			(y_tnr_mot_distxt_rate << 4) |
			(y_tnr_mot_dismot_ofst << 16) |
			(y_tnr_mot_frcsad_lock << 24)));
		WRITE_HREG((HCODEC_MFDIN_REG10 + reg_offset),
			((y_tnr_mot2alp_frc_gain << 0) |
			(y_tnr_mot2alp_nrm_gain << 8) |
			(y_tnr_mot2alp_dis_gain << 16) |
			(y_tnr_mot2alp_dis_ofst << 24)));
		WRITE_HREG((HCODEC_MFDIN_REG11 + reg_offset),
			((y_bld_beta2alp_rate << 0) |
			(y_bld_beta_min << 8) |
			(y_bld_beta_max << 14)));

		/* NR For C */
		WRITE_HREG((HCODEC_MFDIN_REG12 + reg_offset),
			((cfg_y_snr_en << 0) |
			(c_snr_err_norm << 1) |
			(c_snr_gau_bld_core << 2) |
			(((c_snr_gau_bld_ofst) & 0xff) << 6) |
			(c_snr_gau_bld_rate << 14) |
			(c_snr_gau_alp0_min << 20) |
			(c_snr_gau_alp0_max << 26)));

		WRITE_HREG((HCODEC_MFDIN_REG13 + reg_offset),
			((cfg_c_tnr_en << 0) |
			(c_tnr_mc_en << 1) |
			(c_tnr_txt_mode << 2) |
			(c_tnr_mot_sad_margin << 3) |
			(c_tnr_alpha_min << 7) |
			(c_tnr_alpha_max << 13) |
			(c_tnr_deghost_os << 19)));
		WRITE_HREG((HCODEC_MFDIN_REG14 + reg_offset),
			((c_tnr_mot_cortxt_rate << 0) |
			(c_tnr_mot_distxt_ofst << 8) |
			(c_tnr_mot_distxt_rate << 4) |
			(c_tnr_mot_dismot_ofst << 16) |
			(c_tnr_mot_frcsad_lock << 24)));
		WRITE_HREG((HCODEC_MFDIN_REG15 + reg_offset),
			((c_tnr_mot2alp_frc_gain << 0) |
			(c_tnr_mot2alp_nrm_gain << 8) |
			(c_tnr_mot2alp_dis_gain << 16) |
			(c_tnr_mot2alp_dis_ofst << 24)));

		WRITE_HREG((HCODEC_MFDIN_REG16 + reg_offset),
			((c_bld_beta2alp_rate << 0) |
			(c_bld_beta_min << 8) |
			(c_bld_beta_max << 14)));

		WRITE_HREG((HCODEC_MFDIN_REG1_CTRL + reg_offset),
			(iformat << 0) | (oformat << 4) |
			(dsample_en << 6) | (y_size << 8) |
			(interp_en << 9) | (r2y_en << 12) |
			(r2y_mode << 13) | (ifmt_extra << 16) |
			(nr_enable << 19));
		WRITE_HREG((HCODEC_MFDIN_REG8_DMBL + reg_offset),
			(picsize_x << 14) | (picsize_y << 0));
	} else {
		reg_offset = 0;
		WRITE_HREG((HCODEC_MFDIN_REG1_CTRL + reg_offset),
			(iformat << 0) | (oformat << 4) |
			(dsample_en << 6) | (y_size << 8) |
			(interp_en << 9) | (r2y_en << 12) |
			(r2y_mode << 13));
		WRITE_HREG((HCODEC_MFDIN_REG8_DMBL + reg_offset),
			(picsize_x << 12) | (picsize_y << 0));
	}

	if (linear_enable == false) {
		WRITE_HREG((HCODEC_MFDIN_REG3_CANV + reg_offset),
			(input & 0xffffff) |
			(canv_idx1_bppy << 30) |
			(canv_idx0_bppy << 28) |
			(canv_idx1_bppx << 26) |
			(canv_idx0_bppx << 24));
		WRITE_HREG((HCODEC_MFDIN_REG4_LNR0 + reg_offset),
			(0 << 16) | (0 << 0));
		WRITE_HREG((HCODEC_MFDIN_REG5_LNR1 + reg_offset), 0);
	} else {
		WRITE_HREG((HCODEC_MFDIN_REG3_CANV + reg_offset),
			(canv_idx1_bppy << 30) |
			(canv_idx0_bppy << 28) |
			(canv_idx1_bppx << 26) |
			(canv_idx0_bppx << 24));
		WRITE_HREG((HCODEC_MFDIN_REG4_LNR0 + reg_offset),
			(linear_bytes4p << 16) | (linear_bytesperline << 0));
		WRITE_HREG((HCODEC_MFDIN_REG5_LNR1 + reg_offset), input);
	}

	if (iformat == 12)
		WRITE_HREG((HCODEC_MFDIN_REG9_ENDN + reg_offset),
			(2 << 0) | (1 << 3) | (0 << 6) |
			(3 << 9) | (6 << 12) | (5 << 15) |
			(4 << 18) | (7 << 21));
	else
		WRITE_HREG((HCODEC_MFDIN_REG9_ENDN + reg_offset),
			(7 << 0) | (6 << 3) | (5 << 6) |
			(4 << 9) | (3 << 12) | (2 << 15) |
			(1 << 18) | (0 << 21));
}

#ifdef CONFIG_AM_GE2D
static int scale_frame(struct encode_wq_s *wq,
	struct encode_request_s *request,
	struct config_para_ex_s *ge2d_config,
	u32 src_addr, bool canvas)
{
	struct ge2d_context_s *context = encode_manager.context;
	int src_top, src_left, src_width, src_height;
	struct canvas_s cs0, cs1, cs2, cd;
	u32 src_canvas, dst_canvas;
	u32 src_canvas_w, dst_canvas_w;
	u32 src_h = request->src_h;
	u32 dst_w = ((wq->pic.encoder_width + 15) >> 4) << 4;
	u32 dst_h = ((wq->pic.encoder_height + 15) >> 4) << 4;
	int input_format = GE2D_FORMAT_M24_NV21;
	src_top = request->crop_top;
	src_left = request->crop_left;
	src_width = request->src_w - src_left - request->crop_right;
	src_height = request->src_h - src_top - request->crop_bottom;
	if (canvas) {
		if ((request->fmt == FMT_NV21)
			|| (request->fmt == FMT_NV12)) {
			src_canvas = src_addr & 0xffff;
			input_format = GE2D_FORMAT_M24_NV21;
		} else {
			src_canvas = src_addr & 0xffffff;
			input_format = GE2D_FORMAT_M24_YUV420;
		}
	} else {
		if ((request->fmt == FMT_NV21)
			|| (request->fmt == FMT_NV12)) {
			src_canvas_w =
				((request->src_w + 31) >> 5) << 5;
			canvas_config(ENC_CANVAS_OFFSET + 9,
				src_addr,
				src_canvas_w, src_h,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 10,
				src_addr + src_canvas_w * src_h,
				src_canvas_w, src_h / 2,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			src_canvas =
				((ENC_CANVAS_OFFSET + 10) << 8)
				| (ENC_CANVAS_OFFSET + 9);
			input_format = GE2D_FORMAT_M24_NV21;
		} else {
			src_canvas_w =
				((request->src_w + 63) >> 6) << 6;
			canvas_config(ENC_CANVAS_OFFSET + 9,
				src_addr,
				src_canvas_w, src_h,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 10,
				src_addr + src_canvas_w * src_h,
				src_canvas_w / 2, src_h / 2,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 11,
				src_addr + src_canvas_w * src_h * 5 / 4,
				src_canvas_w / 2, src_h / 2,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			src_canvas =
				((ENC_CANVAS_OFFSET + 11) << 16) |
				((ENC_CANVAS_OFFSET + 10) << 8) |
				(ENC_CANVAS_OFFSET + 9);
			input_format = GE2D_FORMAT_M24_YUV420;
		}
	}
	dst_canvas_w =  ((dst_w + 31) >> 5) << 5;
	canvas_config(ENC_CANVAS_OFFSET + 6,
		wq->mem.scaler_buff_start_addr,
		dst_canvas_w, dst_h,
		CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	canvas_config(ENC_CANVAS_OFFSET + 7,
		wq->mem.scaler_buff_start_addr + dst_canvas_w * dst_h,
		dst_canvas_w, dst_h / 2,
		CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	dst_canvas = ((ENC_CANVAS_OFFSET + 7) << 8) |
		(ENC_CANVAS_OFFSET + 6);
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en  = 0;
	ge2d_config->src1_gb_alpha = 0;
	ge2d_config->dst_xy_swap = 0;
	canvas_read(src_canvas & 0xff, &cs0);
	canvas_read((src_canvas >> 8) & 0xff, &cs1);
	canvas_read((src_canvas >> 16) & 0xff, &cs2);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	ge2d_config->src_planes[1].addr = cs1.addr;
	ge2d_config->src_planes[1].w = cs1.width;
	ge2d_config->src_planes[1].h = cs1.height;
	ge2d_config->src_planes[2].addr = cs2.addr;
	ge2d_config->src_planes[2].w = cs2.width;
	ge2d_config->src_planes[2].h = cs2.height;
	canvas_read(dst_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = src_canvas;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = input_format | GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = request->src_w;
	ge2d_config->src_para.height = request->src_h;
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = dst_canvas;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format =
		GE2D_FORMAT_M24_NV21 | GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = dst_w;
	ge2d_config->dst_para.height = dst_h;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, src_left, src_top, src_width, src_height,
		0, 0, wq->pic.encoder_width, wq->pic.encoder_height);
	return dst_canvas_w*dst_h * 3 / 2;
}
#endif

static s32 set_input_format(struct encode_wq_s *wq,
			    struct encode_request_s *request)
{
	s32 ret = 0;
	u8 iformat = MAX_FRAME_FMT, oformat = MAX_FRAME_FMT, r2y_en = 0;
	u32 picsize_x, picsize_y, src_addr;
	u32 canvas_w = 0;
	u32 input = request->src;
	u8 ifmt_extra = 0;

	if ((request->fmt == FMT_RGB565) || (request->fmt >= MAX_FRAME_FMT))
		return -1;

	picsize_x = ((wq->pic.encoder_width + 15) >> 4) << 4;
	picsize_y = ((wq->pic.encoder_height + 15) >> 4) << 4;
	oformat = 0;
	if ((request->type == LOCAL_BUFF)
		|| (request->type == PHYSICAL_BUFF)) {
		if ((request->type == LOCAL_BUFF) &&
			(request->flush_flag & AMVENC_FLUSH_FLAG_INPUT))
			dma_flush(wq->mem.dct_buff_start_addr,
				request->framesize);
		if (request->type == LOCAL_BUFF) {
			input = wq->mem.dct_buff_start_addr;
			src_addr =
				wq->mem.dct_buff_start_addr;
		} else {
			src_addr = input;
			picsize_y = wq->pic.encoder_height;
		}
		if (request->scale_enable) {
#ifdef CONFIG_AM_GE2D
			struct config_para_ex_s ge2d_config;
			memset(&ge2d_config, 0,
				sizeof(struct config_para_ex_s));
			scale_frame(
				wq, request,
				&ge2d_config,
				src_addr,
				false);
			iformat = 2;
			r2y_en = 0;
			input = ((ENC_CANVAS_OFFSET + 7) << 8) |
				(ENC_CANVAS_OFFSET + 6);
			ret = 0;
			goto MFDIN;
#else
			enc_pr(LOG_ERROR,
				"Warning: need enable ge2d for scale frame!\n");
			return -1;
#endif
		}
		if ((request->fmt <= FMT_YUV444_PLANE) ||
			(request->fmt >= FMT_YUV422_12BIT))
			r2y_en = 0;
		else
			r2y_en = 1;

		if (request->fmt >= FMT_YUV422_12BIT) {
			iformat = 7;
			ifmt_extra = request->fmt - FMT_YUV422_12BIT;
			if (request->fmt == FMT_YUV422_12BIT)
				canvas_w = picsize_x * 24 / 8;
			else if (request->fmt == FMT_YUV444_10BIT)
				canvas_w = picsize_x * 32 / 8;
			else
				canvas_w = (picsize_x * 20 + 7) / 8;
			canvas_w = ((canvas_w + 31) >> 5) << 5;
			canvas_config(ENC_CANVAS_OFFSET + 6,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ENC_CANVAS_OFFSET + 6;
			input = input & 0xff;
		} else if (request->fmt == FMT_YUV422_SINGLE)
			iformat = 10;
		else if ((request->fmt == FMT_YUV444_SINGLE)
			|| (request->fmt == FMT_RGB888)) {
			iformat = 1;
			if (request->fmt == FMT_RGB888)
				r2y_en = 1;
			canvas_w =  picsize_x * 3;
			canvas_w = ((canvas_w + 31) >> 5) << 5;
			canvas_config(ENC_CANVAS_OFFSET + 6,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ENC_CANVAS_OFFSET + 6;
		} else if ((request->fmt == FMT_NV21)
			|| (request->fmt == FMT_NV12)) {
			canvas_w = ((wq->pic.encoder_width + 31) >> 5) << 5;
			iformat = (request->fmt == FMT_NV21) ? 2 : 3;
			canvas_config(ENC_CANVAS_OFFSET + 6,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 7,
				input + canvas_w * picsize_y,
				canvas_w, picsize_y / 2,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ((ENC_CANVAS_OFFSET + 7) << 8) |
				(ENC_CANVAS_OFFSET + 6);
		} else if (request->fmt == FMT_YUV420) {
			iformat = 4;
			canvas_w = ((wq->pic.encoder_width + 63) >> 6) << 6;
			canvas_config(ENC_CANVAS_OFFSET + 6,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 7,
				input + canvas_w * picsize_y,
				canvas_w / 2, picsize_y / 2,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 8,
				input + canvas_w * picsize_y * 5 / 4,
				canvas_w / 2, picsize_y / 2,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ((ENC_CANVAS_OFFSET + 8) << 16) |
				((ENC_CANVAS_OFFSET + 7) << 8) |
				(ENC_CANVAS_OFFSET + 6);
		} else if ((request->fmt == FMT_YUV444_PLANE)
			|| (request->fmt == FMT_RGB888_PLANE)) {
			if (request->fmt == FMT_RGB888_PLANE)
				r2y_en = 1;
			iformat = 5;
			canvas_w = ((wq->pic.encoder_width + 31) >> 5) << 5;
			canvas_config(ENC_CANVAS_OFFSET + 6,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 7,
				input + canvas_w * picsize_y,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 8,
				input + canvas_w * picsize_y * 2,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ((ENC_CANVAS_OFFSET + 8) << 16) |
				((ENC_CANVAS_OFFSET + 7) << 8) |
				(ENC_CANVAS_OFFSET + 6);
		} else if (request->fmt == FMT_RGBA8888) {
			r2y_en = 1;
			iformat = 12;
		}
		ret = 0;
	} else if (request->type == CANVAS_BUFF) {
		r2y_en = 0;
		if (request->scale_enable) {
#ifdef CONFIG_AM_GE2D
			struct config_para_ex_s ge2d_config;
			memset(&ge2d_config, 0,
				sizeof(struct config_para_ex_s));
			scale_frame(
				wq, request,
				&ge2d_config,
				input, true);
			iformat = 2;
			r2y_en = 0;
			input = ((ENC_CANVAS_OFFSET + 7) << 8) |
				(ENC_CANVAS_OFFSET + 6);
			ret = 0;
			goto MFDIN;
#else
			enc_pr(LOG_ERROR,
				"Warning: need enable ge2d for scale frame!\n");
			return -1;
#endif
		}
		if (request->fmt == FMT_YUV422_SINGLE) {
			iformat = 0;
			input = input & 0xff;
		} else if (request->fmt == FMT_YUV444_SINGLE) {
			iformat = 1;
			input = input & 0xff;
		} else if ((request->fmt == FMT_NV21)
			|| (request->fmt == FMT_NV12)) {
			iformat = (request->fmt == FMT_NV21) ? 2 : 3;
			input = input & 0xffff;
		} else if (request->fmt == FMT_YUV420) {
			iformat = 4;
			input = input & 0xffffff;
		} else if ((request->fmt == FMT_YUV444_PLANE)
			|| (request->fmt == FMT_RGB888_PLANE)) {
			if (request->fmt == FMT_RGB888_PLANE)
				r2y_en = 1;
			iformat = 5;
			input = input & 0xffffff;
		} else if ((request->fmt == FMT_YUV422_12BIT)
			|| (request->fmt == FMT_YUV444_10BIT)
			|| (request->fmt == FMT_YUV422_10BIT)) {
			iformat = 7;
			ifmt_extra = request->fmt - FMT_YUV422_12BIT;
			input = input & 0xff;
		} else
			ret = -1;
	}
MFDIN:
	if (ret == 0)
		mfdin_basic(input, iformat, oformat,
			picsize_x, picsize_y, r2y_en,
			request->nr_mode, ifmt_extra);
	return ret;
}

#ifdef H264_ENC_CBR
static void ConvertTable2Risc(void *table, u32 len)
{
	u32 i, j;
	u16 temp;
	u16 *tbl = (u16 *)table;
	if ((len < 8) || (len % 8) || (!table)) {
		enc_pr(LOG_ERROR, "ConvertTable2Risc tbl %p, len %d error\n",
			table, len);
		return;
	}
	for (i = 0; i < len / 8; i++) {
		j = i << 2;
		temp = tbl[j];
		tbl[j] = tbl[j + 3];
		tbl[j + 3] = temp;

		temp = tbl[j + 1];
		tbl[j + 1] = tbl[j + 2];
		tbl[j + 2] = temp;
	}

}
#endif

static void avc_prot_init(struct encode_wq_s *wq,
	struct encode_request_s *request, u32 quant, bool IDR)
{
	u32 data32;
	u32 pic_width, pic_height;
	u32 pic_mb_nr;
	u32 pic_mbx, pic_mby;
	u32 i_pic_qp, p_pic_qp;
	u32 i_pic_qp_c, p_pic_qp_c;
	u32 pic_width_in_mb;
	u32 slice_qp;
	pic_width  = wq->pic.encoder_width;
	pic_height = wq->pic.encoder_height;
	pic_mb_nr  = 0;
	pic_mbx    = 0;
	pic_mby    = 0;
	i_pic_qp   = quant;
	p_pic_qp   = quant;

	pic_width_in_mb = (pic_width + 15) / 16;
	WRITE_HREG(HCODEC_HDEC_MC_OMEM_AUTO,
		(1 << 31) | /* use_omem_mb_xy */
		((pic_width_in_mb - 1) << 16)); /* omem_max_mb_x */

	WRITE_HREG(HCODEC_VLC_ADV_CONFIG,
		/* early_mix_mc_hcmd -- will enable in P Picture */
		(0 << 10) |
		(1 << 9) | /* update_top_left_mix */
		(1 << 8) | /* p_top_left_mix */
		/* mv_cal_mixed_type -- will enable in P Picture */
		(0 << 7) |
		/* mc_hcmd_mixed_type -- will enable in P Picture */
		(0 << 6) |
		(1 << 5) | /* use_seperate_int_control */
		(1 << 4) | /* hcmd_intra_use_q_info */
		(1 << 3) | /* hcmd_left_use_prev_info */
		(1 << 2) | /* hcmd_use_q_info */
		(1 << 1) | /* use_q_delta_quant */
		/* detect_I16_from_I4 use qdct detected mb_type */
		(0 << 0));

	WRITE_HREG(HCODEC_QDCT_ADV_CONFIG,
		(1 << 29) | /* mb_info_latch_no_I16_pred_mode */
		(1 << 28) | /* ie_dma_mbxy_use_i_pred */
		(1 << 27) | /* ie_dma_read_write_use_ip_idx */
		(1 << 26) | /* ie_start_use_top_dma_count */
		(1 << 25) | /* i_pred_top_dma_rd_mbbot */
		(1 << 24) | /* i_pred_top_dma_wr_disable */
		/* i_pred_mix -- will enable in P Picture */
		(0 << 23) |
		(1 << 22) | /* me_ab_rd_when_intra_in_p */
		(1 << 21) | /* force_mb_skip_run_when_intra */
		/* mc_out_mixed_type -- will enable in P Picture */
		(0 << 20) |
		(1 << 19) | /* ie_start_when_quant_not_full */
		(1 << 18) | /* mb_info_state_mix */
		/* mb_type_use_mix_result -- will enable in P Picture */
		(0 << 17) |
		/* me_cb_ie_read_enable -- will enable in P Picture */
		(0 << 16) |
		/* ie_cur_data_from_me -- will enable in P Picture */
		(0 << 15) |
		(1 << 14) | /* rem_per_use_table */
		(0 << 13) | /* q_latch_int_enable */
		(1 << 12) | /* q_use_table */
		(0 << 11) | /* q_start_wait */
		(1 << 10) | /* LUMA_16_LEFT_use_cur */
		(1 << 9) | /* DC_16_LEFT_SUM_use_cur */
		(1 << 8) | /* c_ref_ie_sel_cur */
		(0 << 7) | /* c_ipred_perfect_mode */
		(1 << 6) | /* ref_ie_ul_sel */
		(1 << 5) | /* mb_type_use_ie_result */
		(1 << 4) | /* detect_I16_from_I4 */
		(1 << 3) | /* ie_not_wait_ref_busy */
		(1 << 2) | /* ie_I16_enable */
		(3 << 0)); /* ie_done_sel  // fastest when waiting */

	if (request != NULL) {
		WRITE_HREG(HCODEC_IE_WEIGHT,
			(request->i16_weight << 16) |
			(request->i4_weight << 0));
		WRITE_HREG(HCODEC_ME_WEIGHT,
			(request->me_weight << 0));
		WRITE_HREG(HCODEC_SAD_CONTROL_0,
			/* ie_sad_offset_I16 */
			(request->i16_weight << 16) |
			/* ie_sad_offset_I4 */
			(request->i4_weight << 0));
		WRITE_HREG(HCODEC_SAD_CONTROL_1,
			/* ie_sad_shift_I16 */
			(IE_SAD_SHIFT_I16 << 24) |
			/* ie_sad_shift_I4 */
			(IE_SAD_SHIFT_I4 << 20) |
			/* me_sad_shift_INTER */
			(ME_SAD_SHIFT_INTER << 16) |
			/* me_sad_offset_INTER */
			(request->me_weight << 0));
		wq->me_weight = request->me_weight;
		wq->i4_weight = request->i4_weight;
		wq->i16_weight = request->i16_weight;
	} else {
		WRITE_HREG(HCODEC_IE_WEIGHT,
			(I16MB_WEIGHT_OFFSET << 16) |
			(I4MB_WEIGHT_OFFSET << 0));
		WRITE_HREG(HCODEC_ME_WEIGHT,
			(ME_WEIGHT_OFFSET << 0));
		WRITE_HREG(HCODEC_SAD_CONTROL_0,
			/* ie_sad_offset_I16 */
			(I16MB_WEIGHT_OFFSET << 16) |
			/* ie_sad_offset_I4 */
			(I4MB_WEIGHT_OFFSET << 0));
		WRITE_HREG(HCODEC_SAD_CONTROL_1,
			/* ie_sad_shift_I16 */
			(IE_SAD_SHIFT_I16 << 24) |
			/* ie_sad_shift_I4 */
			(IE_SAD_SHIFT_I4 << 20) |
			/* me_sad_shift_INTER */
			(ME_SAD_SHIFT_INTER << 16) |
			/* me_sad_offset_INTER */
			(ME_WEIGHT_OFFSET << 0));
	}

	WRITE_HREG(HCODEC_ADV_MV_CTL0,
		(ADV_MV_LARGE_16x8 << 31) |
		(ADV_MV_LARGE_8x16 << 30) |
		(ADV_MV_8x8_WEIGHT << 16) |   /* adv_mv_8x8_weight */
		/* adv_mv_4x4x4_weight should be set bigger */
		(ADV_MV_4x4x4_WEIGHT << 0));
	WRITE_HREG(HCODEC_ADV_MV_CTL1,
		/* adv_mv_16x16_weight */
		(ADV_MV_16x16_WEIGHT << 16) |
		(ADV_MV_LARGE_16x16 << 15) |
		(ADV_MV_16_8_WEIGHT << 0));  /* adv_mv_16_8_weight */

	hcodec_prog_qtbl(wq);
	if (IDR) {
		i_pic_qp =
			wq->quant_tbl_i4[0] & 0xff;
		i_pic_qp +=
			wq->quant_tbl_i16[0] & 0xff;
		i_pic_qp /= 2;
		p_pic_qp = i_pic_qp;
	} else {
		i_pic_qp =
			wq->quant_tbl_i4[0] & 0xff;
		i_pic_qp +=
			wq->quant_tbl_i16[0] & 0xff;
		p_pic_qp = wq->quant_tbl_me[0] & 0xff;
		slice_qp = (i_pic_qp + p_pic_qp) / 3;
		i_pic_qp = slice_qp;
		p_pic_qp = i_pic_qp;
	}
#ifdef H264_ENC_CBR
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
		data32 = READ_HREG(HCODEC_SAD_CONTROL_1);
		data32 = data32 & 0xffff; /* remove sad shift */
		WRITE_HREG(HCODEC_SAD_CONTROL_1, data32);
		WRITE_HREG(H264_ENC_CBR_TABLE_ADDR,
			wq->mem.cbr_info_ddr_start_addr);
		WRITE_HREG(H264_ENC_CBR_MB_SIZE_ADDR,
			wq->mem.cbr_info_ddr_start_addr
			+ CBR_TABLE_SIZE);
		WRITE_HREG(H264_ENC_CBR_CTL,
			(wq->cbr_info.start_tbl_id << 28) |
			(wq->cbr_info.short_shift << 24) |
			(wq->cbr_info.long_mb_num << 16) |
			(wq->cbr_info.long_th << 0));
		WRITE_HREG(H264_ENC_CBR_REGION_SIZE,
			(wq->cbr_info.block_w << 16) |
			(wq->cbr_info.block_h << 0));
	}
#endif

	WRITE_HREG(HCODEC_QDCT_VLC_QUANT_CTL_0,
		(0 << 19) | /* vlc_delta_quant_1 */
		(i_pic_qp << 13) | /* vlc_quant_1 */
		(0 << 6) | /* vlc_delta_quant_0 */
		(i_pic_qp << 0)); /* vlc_quant_0 */
	WRITE_HREG(HCODEC_QDCT_VLC_QUANT_CTL_1,
		(14 << 6) | /* vlc_max_delta_q_neg */
		(13 << 0)); /* vlc_max_delta_q_pos */
	WRITE_HREG(HCODEC_VLC_PIC_SIZE,
		pic_width | (pic_height << 16));
	WRITE_HREG(HCODEC_VLC_PIC_POSITION,
		(pic_mb_nr << 16) |
		(pic_mby << 8) |
		(pic_mbx << 0));

	/* synopsys parallel_case full_case */
	switch (i_pic_qp) {
	case 0:
		i_pic_qp_c = 0;
		break;
	case 1:
		i_pic_qp_c = 1;
		break;
	case 2:
		i_pic_qp_c = 2;
		break;
	case 3:
		i_pic_qp_c = 3;
		break;
	case 4:
		i_pic_qp_c = 4;
		break;
	case 5:
		i_pic_qp_c = 5;
		break;
	case 6:
		i_pic_qp_c = 6;
		break;
	case 7:
		i_pic_qp_c = 7;
		break;
	case 8:
		i_pic_qp_c = 8;
		break;
	case 9:
		i_pic_qp_c = 9;
		break;
	case 10:
		i_pic_qp_c = 10;
		break;
	case 11:
		i_pic_qp_c = 11;
		break;
	case 12:
		i_pic_qp_c = 12;
		break;
	case 13:
		i_pic_qp_c = 13;
		break;
	case 14:
		i_pic_qp_c = 14;
		break;
	case 15:
		i_pic_qp_c = 15;
		break;
	case 16:
		i_pic_qp_c = 16;
		break;
	case 17:
		i_pic_qp_c = 17;
		break;
	case 18:
		i_pic_qp_c = 18;
		break;
	case 19:
		i_pic_qp_c = 19;
		break;
	case 20:
		i_pic_qp_c = 20;
		break;
	case 21:
		i_pic_qp_c = 21;
		break;
	case 22:
		i_pic_qp_c = 22;
		break;
	case 23:
		i_pic_qp_c = 23;
		break;
	case 24:
		i_pic_qp_c = 24;
		break;
	case 25:
		i_pic_qp_c = 25;
		break;
	case 26:
		i_pic_qp_c = 26;
		break;
	case 27:
		i_pic_qp_c = 27;
		break;
	case 28:
		i_pic_qp_c = 28;
		break;
	case 29:
		i_pic_qp_c = 29;
		break;
	case 30:
		i_pic_qp_c = 29;
		break;
	case 31:
		i_pic_qp_c = 30;
		break;
	case 32:
		i_pic_qp_c = 31;
		break;
	case 33:
		i_pic_qp_c = 32;
		break;
	case 34:
		i_pic_qp_c = 32;
		break;
	case 35:
		i_pic_qp_c = 33;
		break;
	case 36:
		i_pic_qp_c = 34;
		break;
	case 37:
		i_pic_qp_c = 34;
		break;
	case 38:
		i_pic_qp_c = 35;
		break;
	case 39:
		i_pic_qp_c = 35;
		break;
	case 40:
		i_pic_qp_c = 36;
		break;
	case 41:
		i_pic_qp_c = 36;
		break;
	case 42:
		i_pic_qp_c = 37;
		break;
	case 43:
		i_pic_qp_c = 37;
		break;
	case 44:
		i_pic_qp_c = 37;
		break;
	case 45:
		i_pic_qp_c = 38;
		break;
	case 46:
		i_pic_qp_c = 38;
		break;
	case 47:
		i_pic_qp_c = 38;
		break;
	case 48:
		i_pic_qp_c = 39;
		break;
	case 49:
		i_pic_qp_c = 39;
		break;
	case 50:
		i_pic_qp_c = 39;
		break;
	default:
		i_pic_qp_c = 39;
		break;
	}

	/* synopsys parallel_case full_case */
	switch (p_pic_qp) {
	case 0:
		p_pic_qp_c = 0;
		break;
	case 1:
		p_pic_qp_c = 1;
		break;
	case 2:
		p_pic_qp_c = 2;
		break;
	case 3:
		p_pic_qp_c = 3;
		break;
	case 4:
		p_pic_qp_c = 4;
		break;
	case 5:
		p_pic_qp_c = 5;
		break;
	case 6:
		p_pic_qp_c = 6;
		break;
	case 7:
		p_pic_qp_c = 7;
		break;
	case 8:
		p_pic_qp_c = 8;
		break;
	case 9:
		p_pic_qp_c = 9;
		break;
	case 10:
		p_pic_qp_c = 10;
		break;
	case 11:
		p_pic_qp_c = 11;
		break;
	case 12:
		p_pic_qp_c = 12;
		break;
	case 13:
		p_pic_qp_c = 13;
		break;
	case 14:
		p_pic_qp_c = 14;
		break;
	case 15:
		p_pic_qp_c = 15;
		break;
	case 16:
		p_pic_qp_c = 16;
		break;
	case 17:
		p_pic_qp_c = 17;
		break;
	case 18:
		p_pic_qp_c = 18;
		break;
	case 19:
		p_pic_qp_c = 19;
		break;
	case 20:
		p_pic_qp_c = 20;
		break;
	case 21:
		p_pic_qp_c = 21;
		break;
	case 22:
		p_pic_qp_c = 22;
		break;
	case 23:
		p_pic_qp_c = 23;
		break;
	case 24:
		p_pic_qp_c = 24;
		break;
	case 25:
		p_pic_qp_c = 25;
		break;
	case 26:
		p_pic_qp_c = 26;
		break;
	case 27:
		p_pic_qp_c = 27;
		break;
	case 28:
		p_pic_qp_c = 28;
		break;
	case 29:
		p_pic_qp_c = 29;
		break;
	case 30:
		p_pic_qp_c = 29;
		break;
	case 31:
		p_pic_qp_c = 30;
		break;
	case 32:
		p_pic_qp_c = 31;
		break;
	case 33:
		p_pic_qp_c = 32;
		break;
	case 34:
		p_pic_qp_c = 32;
		break;
	case 35:
		p_pic_qp_c = 33;
		break;
	case 36:
		p_pic_qp_c = 34;
		break;
	case 37:
		p_pic_qp_c = 34;
		break;
	case 38:
		p_pic_qp_c = 35;
		break;
	case 39:
		p_pic_qp_c = 35;
		break;
	case 40:
		p_pic_qp_c = 36;
		break;
	case 41:
		p_pic_qp_c = 36;
		break;
	case 42:
		p_pic_qp_c = 37;
		break;
	case 43:
		p_pic_qp_c = 37;
		break;
	case 44:
		p_pic_qp_c = 37;
		break;
	case 45:
		p_pic_qp_c = 38;
		break;
	case 46:
		p_pic_qp_c = 38;
		break;
	case 47:
		p_pic_qp_c = 38;
		break;
	case 48:
		p_pic_qp_c = 39;
		break;
	case 49:
		p_pic_qp_c = 39;
		break;
	case 50:
		p_pic_qp_c = 39;
		break;
	default:
		p_pic_qp_c = 39;
		break;
	}
	WRITE_HREG(HCODEC_QDCT_Q_QUANT_I,
		(i_pic_qp_c << 22) |
		(i_pic_qp << 16) |
		((i_pic_qp_c % 6) << 12) |
		((i_pic_qp_c / 6) << 8) |
		((i_pic_qp % 6) << 4) |
		((i_pic_qp / 6) << 0));

	WRITE_HREG(HCODEC_QDCT_Q_QUANT_P,
		(p_pic_qp_c << 22) |
		(p_pic_qp << 16) |
		((p_pic_qp_c % 6) << 12) |
		((p_pic_qp_c / 6) << 8) |
		((p_pic_qp % 6) << 4) |
		((p_pic_qp / 6) << 0));

#ifdef ENABLE_IGNORE_FUNCTION
	WRITE_HREG(HCODEC_IGNORE_CONFIG,
		(1 << 31) | /* ignore_lac_coeff_en */
		(1 << 26) | /* ignore_lac_coeff_else (<1) */
		(1 << 21) | /* ignore_lac_coeff_2 (<1) */
		(2 << 16) | /* ignore_lac_coeff_1 (<2) */
		(1 << 15) | /* ignore_cac_coeff_en */
		(1 << 10) | /* ignore_cac_coeff_else (<1) */
		(1 << 5)  | /* ignore_cac_coeff_2 (<1) */
		(3 << 0));  /* ignore_cac_coeff_1 (<2) */

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB)
		WRITE_HREG(HCODEC_IGNORE_CONFIG_2,
			(1 << 31) | /* ignore_t_lac_coeff_en */
			(1 << 26) | /* ignore_t_lac_coeff_else (<1) */
			(2 << 21) | /* ignore_t_lac_coeff_2 (<2) */
			(6 << 16) | /* ignore_t_lac_coeff_1 (<6) */
			(1<<15) | /* ignore_cdc_coeff_en */
			(0<<14) | /* ignore_t_lac_coeff_else_le_3 */
			(1<<13) | /* ignore_t_lac_coeff_else_le_4 */
			(1<<12) | /* ignore_cdc_only_when_empty_cac_inter */
			(1<<11) | /* ignore_cdc_only_when_one_empty_inter */
			/* ignore_cdc_range_max_inter 0-0, 1-1, 2-2, 3-3 */
			(2<<9) |
			/* ignore_cdc_abs_max_inter 0-1, 1-2, 2-3, 3-4 */
			(0<<7) |
			/* ignore_cdc_only_when_empty_cac_intra */
			(1<<5) |
			/* ignore_cdc_only_when_one_empty_intra */
			(1<<4) |
			/* ignore_cdc_range_max_intra 0-0, 1-1, 2-2, 3-3 */
			(1<<2) |
			/* ignore_cdc_abs_max_intra 0-1, 1-2, 2-3, 3-4 */
			(0<<0));
	else
		WRITE_HREG(HCODEC_IGNORE_CONFIG_2,
			(1 << 31) | /* ignore_t_lac_coeff_en */
			(1 << 26) | /* ignore_t_lac_coeff_else (<1) */
			(1 << 21) | /* ignore_t_lac_coeff_2 (<1) */
			(5 << 16) | /* ignore_t_lac_coeff_1 (<5) */
			(0 << 0));
#else
	WRITE_HREG(HCODEC_IGNORE_CONFIG, 0);
	WRITE_HREG(HCODEC_IGNORE_CONFIG_2, 0);
#endif

	WRITE_HREG(HCODEC_QDCT_MB_CONTROL,
		(1 << 9) | /* mb_info_soft_reset */
		(1 << 0)); /* mb read buffer soft reset */

	WRITE_HREG(HCODEC_QDCT_MB_CONTROL,
		(1 << 28) | /* ignore_t_p8x8 */
		(0 << 27) | /* zero_mc_out_null_non_skipped_mb */
		(0 << 26) | /* no_mc_out_null_non_skipped_mb */
		(0 << 25) | /* mc_out_even_skipped_mb */
		(0 << 24) | /* mc_out_wait_cbp_ready */
		(0 << 23) | /* mc_out_wait_mb_type_ready */
		(1 << 29) | /* ie_start_int_enable */
		(1 << 19) | /* i_pred_enable */
		(1 << 20) | /* ie_sub_enable */
		(1 << 18) | /* iq_enable */
		(1 << 17) | /* idct_enable */
		(1 << 14) | /* mb_pause_enable */
		(1 << 13) | /* q_enable */
		(1 << 12) | /* dct_enable */
		(1 << 10) | /* mb_info_en */
		(0 << 3) | /* endian */
		(0 << 1) | /* mb_read_en */
		(0 << 0)); /* soft reset */

	WRITE_HREG(HCODEC_SAD_CONTROL,
		(0 << 3) | /* ie_result_buff_enable */
		(1 << 2) | /* ie_result_buff_soft_reset */
		(0 << 1) | /* sad_enable */
		(1 << 0)); /* sad soft reset */
	WRITE_HREG(HCODEC_IE_RESULT_BUFFER, 0);

	WRITE_HREG(HCODEC_SAD_CONTROL,
		(1 << 3) | /* ie_result_buff_enable */
		(0 << 2) | /* ie_result_buff_soft_reset */
		(1 << 1) | /* sad_enable */
		(0 << 0)); /* sad soft reset */

	WRITE_HREG(HCODEC_IE_CONTROL,
		(1 << 30) | /* active_ul_block */
		(0 << 1) | /* ie_enable */
		(1 << 0)); /* ie soft reset */

	WRITE_HREG(HCODEC_IE_CONTROL,
		(1 << 30) | /* active_ul_block */
		(0 << 1) | /* ie_enable */
		(0 << 0)); /* ie soft reset */

	WRITE_HREG(HCODEC_ME_SKIP_LINE,
		(8 << 24) | /* step_3_skip_line */
		(8 << 18) | /* step_2_skip_line */
		(2 << 12) | /* step_1_skip_line */
		(0 << 6) | /* step_0_skip_line */
		(0 << 0));

	WRITE_HREG(HCODEC_ME_MV_MERGE_CTL, me_mv_merge_ctl);
	WRITE_HREG(HCODEC_ME_STEP0_CLOSE_MV, me_step0_close_mv);
	WRITE_HREG(HCODEC_ME_SAD_ENOUGH_01, me_sad_enough_01);
	WRITE_HREG(HCODEC_ME_SAD_ENOUGH_23, me_sad_enough_23);
	WRITE_HREG(HCODEC_ME_F_SKIP_SAD, me_f_skip_sad);
	WRITE_HREG(HCODEC_ME_F_SKIP_WEIGHT, me_f_skip_weight);
	WRITE_HREG(HCODEC_ME_MV_WEIGHT_01, me_mv_weight_01);
	WRITE_HREG(HCODEC_ME_MV_WEIGHT_23, me_mv_weight_23);
	WRITE_HREG(HCODEC_ME_SAD_RANGE_INC, me_sad_range_inc);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL) {
		WRITE_HREG(HCODEC_V5_SIMPLE_MB_CTL, 0);
		WRITE_HREG(HCODEC_V5_SIMPLE_MB_CTL,
			(v5_use_small_diff_cnt << 7) |
			(v5_simple_mb_inter_all_en << 6) |
			(v5_simple_mb_inter_8x8_en << 5) |
			(v5_simple_mb_inter_16_8_en << 4) |
			(v5_simple_mb_inter_16x16_en << 3) |
			(v5_simple_mb_intra_en << 2) |
			(v5_simple_mb_C_en << 1) |
			(v5_simple_mb_Y_en << 0));
		WRITE_HREG(HCODEC_V5_MB_DIFF_SUM, 0);
		WRITE_HREG(HCODEC_V5_SMALL_DIFF_CNT,
			(v5_small_diff_C<<16) |
			(v5_small_diff_Y<<0));
		WRITE_HREG(HCODEC_V5_SIMPLE_MB_DQUANT,
			v5_simple_dq_setting);
		WRITE_HREG(HCODEC_V5_SIMPLE_MB_ME_WEIGHT,
			v5_simple_me_weight_setting);
		/* txlx can remove it */
		WRITE_HREG(HCODEC_QDCT_CONFIG, 1 << 0);
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
		WRITE_HREG(HCODEC_V4_FORCE_SKIP_CFG,
			(i_pic_qp << 26) | /* v4_force_q_r_intra */
			(i_pic_qp << 20) | /* v4_force_q_r_inter */
			(0 << 19) | /* v4_force_q_y_enable */
			(5 << 16) | /* v4_force_qr_y */
			(6 << 12) | /* v4_force_qp_y */
			(0 << 0)); /* v4_force_skip_sad */

		/* V3 Force skip */
		WRITE_HREG(HCODEC_V3_SKIP_CONTROL,
			(1 << 31) | /* v3_skip_enable */
			(0 << 30) | /* v3_step_1_weight_enable */
			(1 << 28) | /* v3_mv_sad_weight_enable */
			(1 << 27) | /* v3_ipred_type_enable */
			(V3_FORCE_SKIP_SAD_1 << 12) |
			(V3_FORCE_SKIP_SAD_0 << 0));
		WRITE_HREG(HCODEC_V3_SKIP_WEIGHT,
			(V3_SKIP_WEIGHT_1 << 16) |
			(V3_SKIP_WEIGHT_0 << 0));
		WRITE_HREG(HCODEC_V3_L1_SKIP_MAX_SAD,
			(V3_LEVEL_1_F_SKIP_MAX_SAD << 16) |
			(V3_LEVEL_1_SKIP_MAX_SAD << 0));
		WRITE_HREG(HCODEC_V3_L2_SKIP_WEIGHT,
			(V3_FORCE_SKIP_SAD_2 << 16) |
			(V3_SKIP_WEIGHT_2 << 0));
		if (request != NULL) {
			unsigned int off1, off2;
			off1 = V3_IE_F_ZERO_SAD_I4 - I4MB_WEIGHT_OFFSET;
			off2 = V3_IE_F_ZERO_SAD_I16
				- I16MB_WEIGHT_OFFSET;
			WRITE_HREG(HCODEC_V3_F_ZERO_CTL_0,
				((request->i16_weight + off2) << 16) |
				((request->i4_weight + off1) << 0));
			off1 = V3_ME_F_ZERO_SAD - ME_WEIGHT_OFFSET;
			WRITE_HREG(HCODEC_V3_F_ZERO_CTL_1,
				(0 << 25) |
				/* v3_no_ver_when_top_zero_en */
				(0 << 24) |
				/* v3_no_hor_when_left_zero_en */
				(3 << 16) |  /* type_hor break */
				((request->me_weight + off1) << 0));
		} else {
			WRITE_HREG(HCODEC_V3_F_ZERO_CTL_0,
				(V3_IE_F_ZERO_SAD_I16 << 16) |
				(V3_IE_F_ZERO_SAD_I4 << 0));
			WRITE_HREG(HCODEC_V3_F_ZERO_CTL_1,
				(0 << 25) |
				/* v3_no_ver_when_top_zero_en */
				(0 << 24) |
				/* v3_no_hor_when_left_zero_en */
				(3 << 16) |  /* type_hor break */
				(V3_ME_F_ZERO_SAD << 0));
		}
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
		/* V3 Force skip */
		WRITE_HREG(HCODEC_V3_SKIP_CONTROL,
			(1 << 31) | /* v3_skip_enable */
			(0 << 30) | /* v3_step_1_weight_enable */
			(1 << 28) | /* v3_mv_sad_weight_enable */
			(1 << 27) | /* v3_ipred_type_enable */
			(0 << 12) | /* V3_FORCE_SKIP_SAD_1 */
			(0 << 0)); /* V3_FORCE_SKIP_SAD_0 */
		WRITE_HREG(HCODEC_V3_SKIP_WEIGHT,
			(V3_SKIP_WEIGHT_1 << 16) |
			(V3_SKIP_WEIGHT_0 << 0));
		WRITE_HREG(HCODEC_V3_L1_SKIP_MAX_SAD,
			(V3_LEVEL_1_F_SKIP_MAX_SAD << 16) |
			(V3_LEVEL_1_SKIP_MAX_SAD << 0));
		WRITE_HREG(HCODEC_V3_L2_SKIP_WEIGHT,
			(0 << 16) | /* V3_FORCE_SKIP_SAD_2 */
			(V3_SKIP_WEIGHT_2 << 0));
		WRITE_HREG(HCODEC_V3_F_ZERO_CTL_0,
			(0 << 16) | /* V3_IE_F_ZERO_SAD_I16 */
			(0 << 0)); /* V3_IE_F_ZERO_SAD_I4 */
		WRITE_HREG(HCODEC_V3_F_ZERO_CTL_1,
			(0 << 25) | /* v3_no_ver_when_top_zero_en */
			(0 << 24) | /* v3_no_hor_when_left_zero_en */
			(3 << 16) |  /* type_hor break */
			(0 << 0)); /* V3_ME_F_ZERO_SAD */
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
		int i;
		/* MV SAD Table */
		for (i = 0; i < 64; i++)
			WRITE_HREG(HCODEC_V3_MV_SAD_TABLE,
				v3_mv_sad[i]);

		/* IE PRED SAD Table*/
		WRITE_HREG(HCODEC_V3_IPRED_TYPE_WEIGHT_0,
			(C_ipred_weight_H << 24) |
			(C_ipred_weight_V << 16) |
			(I4_ipred_weight_else << 8) |
			(I4_ipred_weight_most << 0));
		WRITE_HREG(HCODEC_V3_IPRED_TYPE_WEIGHT_1,
			(I16_ipred_weight_DC << 24) |
			(I16_ipred_weight_H << 16) |
			(I16_ipred_weight_V << 8) |
			(C_ipred_weight_DC << 0));
		WRITE_HREG(HCODEC_V3_LEFT_SMALL_MAX_SAD,
			(v3_left_small_max_me_sad << 16) |
			(v3_left_small_max_ie_sad << 0));
	}
	WRITE_HREG(HCODEC_IE_DATA_FEED_BUFF_INFO, 0);

	WRITE_HREG(HCODEC_CURR_CANVAS_CTRL, 0);
	data32 = READ_HREG(HCODEC_VLC_CONFIG);
	data32 = data32 | (1 << 0); /* set pop_coeff_even_all_zero */
	WRITE_HREG(HCODEC_VLC_CONFIG, data32);

	WRITE_HREG(INFO_DUMP_START_ADDR,
		wq->mem.dump_info_ddr_start_addr);

	/* clear mailbox interrupt */
	WRITE_HREG(HCODEC_IRQ_MBOX_CLR, 1);

	/* enable mailbox interrupt */
	WRITE_HREG(HCODEC_IRQ_MBOX_MASK, 1);
}

void amvenc_reset(void)
{
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	WRITE_VREG(DOS_SW_RESET1,
		(1 << 2) | (1 << 6) |
		(1 << 7) | (1 << 8) |
		(1 << 14) | (1 << 16) |
		(1 << 17));
	WRITE_VREG(DOS_SW_RESET1, 0);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
}

void amvenc_start(void)
{
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	WRITE_VREG(DOS_SW_RESET1,
		(1 << 12) | (1 << 11));
	WRITE_VREG(DOS_SW_RESET1, 0);

	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);

	WRITE_HREG(HCODEC_MPSR, 0x0001);
}

void amvenc_stop(void)
{
	ulong timeout = jiffies + HZ;

	WRITE_HREG(HCODEC_MPSR, 0);
	WRITE_HREG(HCODEC_CPSR, 0);
	while (READ_HREG(HCODEC_IMEM_DMA_CTRL) & 0x8000) {
		if (time_after(jiffies, timeout))
			break;
	}
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);

	WRITE_VREG(DOS_SW_RESET1,
		(1 << 12) | (1 << 11) |
		(1 << 2) | (1 << 6) |
		(1 << 7) | (1 << 8) |
		(1 << 14) | (1 << 16) |
		(1 << 17));

	WRITE_VREG(DOS_SW_RESET1, 0);

	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
}

static void __iomem *mc_addr;
static u32 mc_addr_map;
#define MC_SIZE (4096 * 4)
s32 amvenc_loadmc(const char *p, struct encode_wq_s *wq)
{
	ulong timeout;
	s32 ret = 0;

	/* use static mempry*/
	if (mc_addr == NULL) {
		mc_addr = kmalloc(MC_SIZE, GFP_KERNEL);
		if (!mc_addr) {
			enc_pr(LOG_ERROR, "avc loadmc iomap mc addr error.\n");
			return -ENOMEM;
		}
	}

	enc_pr(LOG_ALL, "avc encode ucode name is %s\n", p);
	ret = get_decoder_firmware_data(VFORMAT_H264_ENC, p,
		(u8 *)mc_addr, MC_SIZE);
	if (ret < 0) {
		enc_pr(LOG_ERROR,
			"avc microcode fail ret=%d, name: %s, wq:%p.\n",
			ret, p, (void *)wq);
	}

	mc_addr_map = dma_map_single(
		&encode_manager.this_pdev->dev,
		mc_addr, MC_SIZE, DMA_TO_DEVICE);

	/* mc_addr_map = wq->mem.assit_buffer_offset; */
	/* mc_addr = ioremap_wc(mc_addr_map, MC_SIZE); */
	/* memcpy(mc_addr, p, MC_SIZE); */
	enc_pr(LOG_ALL, "address 0 is 0x%x\n", *((u32 *)mc_addr));
	enc_pr(LOG_ALL, "address 1 is 0x%x\n", *((u32 *)mc_addr + 1));
	enc_pr(LOG_ALL, "address 2 is 0x%x\n", *((u32 *)mc_addr + 2));
	enc_pr(LOG_ALL, "address 3 is 0x%x\n", *((u32 *)mc_addr + 3));
	WRITE_HREG(HCODEC_MPSR, 0);
	WRITE_HREG(HCODEC_CPSR, 0);

	/* Read CBUS register for timing */
	timeout = READ_HREG(HCODEC_MPSR);
	timeout = READ_HREG(HCODEC_MPSR);

	timeout = jiffies + HZ;

	WRITE_HREG(HCODEC_IMEM_DMA_ADR, mc_addr_map);
	WRITE_HREG(HCODEC_IMEM_DMA_COUNT, 0x1000);
	WRITE_HREG(HCODEC_IMEM_DMA_CTRL, (0x8000 | (7 << 16)));

	while (READ_HREG(HCODEC_IMEM_DMA_CTRL) & 0x8000) {
		if (time_before(jiffies, timeout))
			schedule();
		else {
			enc_pr(LOG_ERROR, "hcodec load mc error\n");
			ret = -EBUSY;
			break;
		}
	}
	dma_unmap_single(
		&encode_manager.this_pdev->dev,
		mc_addr_map, MC_SIZE, DMA_TO_DEVICE);
	return ret;
}

const u32 fix_mc[] __aligned(8) = {
	0x0809c05a, 0x06696000, 0x0c780000, 0x00000000
};


/*
 * DOS top level register access fix.
 * When hcodec is running, a protocol register HCODEC_CCPU_INTR_MSK
 * is set to make hcodec access one CBUS out of DOS domain once
 * to work around a HW bug for 4k2k dual decoder implementation.
 * If hcodec is not running, then a ucode is loaded and executed
 * instead.
 */
void amvenc_dos_top_reg_fix(void)
{
	bool hcodec_on;
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	hcodec_on = vdec_on(VDEC_HCODEC);

	if ((hcodec_on) && (READ_VREG(HCODEC_MPSR) & 1)) {
		WRITE_HREG(HCODEC_CCPU_INTR_MSK, 1);
		spin_unlock_irqrestore(&lock, flags);
		return;
	}

	if (!hcodec_on)
		vdec_poweron(VDEC_HCODEC);

	amhcodec_loadmc(fix_mc);

	amhcodec_start();

	udelay(1000);

	amhcodec_stop();

	if (!hcodec_on)
		vdec_poweroff(VDEC_HCODEC);

	spin_unlock_irqrestore(&lock, flags);
}

bool amvenc_avc_on(void)
{
	bool hcodec_on;
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	hcodec_on = vdec_on(VDEC_HCODEC);
	hcodec_on &= (encode_manager.wq_count > 0);

	spin_unlock_irqrestore(&lock, flags);
	return hcodec_on;
}

static s32 avc_poweron(u32 clock)
{
	ulong flags;
	u32 data32;

	data32 = 0;

	amports_switch_gate("vdec", 1);

	spin_lock_irqsave(&lock, flags);

	WRITE_AOREG(AO_RTI_PWR_CNTL_REG0,
		(READ_AOREG(AO_RTI_PWR_CNTL_REG0) & (~0x18)));
	udelay(10);
	/* Powerup HCODEC */
	/* [1:0] HCODEC */
	WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
		(READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & (~0x3)));
	udelay(10);

	WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
	WRITE_VREG(DOS_SW_RESET1, 0);

	/* Enable Dos internal clock gating */
	hvdec_clock_enable(clock);

	/* Powerup HCODEC memories */
	WRITE_VREG(DOS_MEM_PD_HCODEC, 0x0);

	/* Remove HCODEC ISO */
	WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
		(READ_AOREG(AO_RTI_GEN_PWR_ISO0) & (~0x30)));
	udelay(10);
	/* Disable auto-clock gate */
	WRITE_VREG(DOS_GEN_CTRL0,
		(READ_VREG(DOS_GEN_CTRL0) | 0x1));
	WRITE_VREG(DOS_GEN_CTRL0,
		(READ_VREG(DOS_GEN_CTRL0) & 0xFFFFFFFE));

	spin_unlock_irqrestore(&lock, flags);

	mdelay(10);
	return 0;
}

static s32 avc_poweroff(void)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	/* enable HCODEC isolation */
	WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
		READ_AOREG(AO_RTI_GEN_PWR_ISO0) | 0x30);
	/* power off HCODEC memories */
	WRITE_VREG(DOS_MEM_PD_HCODEC, 0xffffffffUL);

	/* disable HCODEC clock */
	hvdec_clock_disable();

	/* HCODEC power off */
	WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
		READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 0x3);

	spin_unlock_irqrestore(&lock, flags);

	/* release DOS clk81 clock gating */
	amports_switch_gate("vdec", 0);
	return 0;
}

static s32 reload_mc(struct encode_wq_s *wq)
{
	const char *p = select_ucode(encode_manager.ucode_index);

	amvenc_stop();

	WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
	WRITE_VREG(DOS_SW_RESET1, 0);

	udelay(10);

	WRITE_HREG(HCODEC_ASSIST_MMC_CTRL1, 0x32);
	enc_pr(LOG_INFO, "reload microcode\n");

	if (amvenc_loadmc(p, wq) < 0)
		return -EBUSY;
	return 0;
}

static void encode_isr_tasklet(ulong data)
{
	struct encode_manager_s *manager = (struct encode_manager_s *)data;
	enc_pr(LOG_INFO, "encoder is done %d\n", manager->encode_hw_status);
	if (((manager->encode_hw_status == ENCODER_IDR_DONE)
		|| (manager->encode_hw_status == ENCODER_NON_IDR_DONE)
		|| (manager->encode_hw_status == ENCODER_SEQUENCE_DONE)
		|| (manager->encode_hw_status == ENCODER_PICTURE_DONE))
		&& (manager->process_irq)) {
		wake_up_interruptible(&manager->event.hw_complete);
	}
}

/* irq function */
static irqreturn_t enc_isr(s32 irq_number, void *para)
{
	struct encode_manager_s *manager = (struct encode_manager_s *)para;
	WRITE_HREG(HCODEC_IRQ_MBOX_CLR, 1);

	manager->encode_hw_status  = READ_HREG(ENCODER_STATUS);
	if ((manager->encode_hw_status == ENCODER_IDR_DONE)
		|| (manager->encode_hw_status == ENCODER_NON_IDR_DONE)
		|| (manager->encode_hw_status == ENCODER_SEQUENCE_DONE)
		|| (manager->encode_hw_status == ENCODER_PICTURE_DONE)) {
		enc_pr(LOG_ALL, "encoder stage is %d\n",
			manager->encode_hw_status);
	}

	if (((manager->encode_hw_status == ENCODER_IDR_DONE)
		|| (manager->encode_hw_status == ENCODER_NON_IDR_DONE)
		|| (manager->encode_hw_status == ENCODER_SEQUENCE_DONE)
		|| (manager->encode_hw_status == ENCODER_PICTURE_DONE))
		&& (!manager->process_irq)) {
		manager->process_irq = true;
		if (manager->encode_hw_status != ENCODER_SEQUENCE_DONE)
			manager->need_reset = true;
		tasklet_schedule(&manager->encode_tasklet);
	}
	return IRQ_HANDLED;
}

static s32 convert_request(struct encode_wq_s *wq, u32 *cmd_info)
{
	int i = 0;
	u8 *ptr;
	u32 data_offset;
	u32 cmd = cmd_info[0];
	if (!wq)
		return -1;
	memset(&wq->request, 0, sizeof(struct encode_request_s));
	wq->request.me_weight = ME_WEIGHT_OFFSET;
	wq->request.i4_weight = I4MB_WEIGHT_OFFSET;
	wq->request.i16_weight = I16MB_WEIGHT_OFFSET;

	if (cmd == ENCODER_SEQUENCE) {
		wq->request.cmd = cmd;
		wq->request.ucode_mode = cmd_info[1];
		wq->request.quant = cmd_info[2];
		wq->request.flush_flag = cmd_info[3];
		wq->request.timeout = cmd_info[4];
		wq->request.timeout = 5000; /* 5000 ms */
	} else if ((cmd == ENCODER_IDR) || (cmd == ENCODER_NON_IDR)) {
		wq->request.cmd = cmd;
		wq->request.ucode_mode = cmd_info[1];
		wq->request.type = cmd_info[2];
		wq->request.fmt = cmd_info[3];
		wq->request.src = cmd_info[4];
		wq->request.framesize = cmd_info[5];
		wq->request.quant = cmd_info[6];
		wq->request.flush_flag = cmd_info[7];
		wq->request.timeout = cmd_info[8];
		wq->request.crop_top = cmd_info[9];
		wq->request.crop_bottom = cmd_info[10];
		wq->request.crop_left = cmd_info[11];
		wq->request.crop_right = cmd_info[12];
		wq->request.src_w = cmd_info[13];
		wq->request.src_h = cmd_info[14];
		wq->request.scale_enable = cmd_info[15];
		wq->request.nr_mode =
			(nr_mode > 0) ? nr_mode : cmd_info[16];
		if (cmd == ENCODER_IDR)
			wq->request.nr_mode = 0;

		data_offset = 17 +
			(sizeof(wq->quant_tbl_i4)
			+ sizeof(wq->quant_tbl_i16)
			+ sizeof(wq->quant_tbl_me)) / 4;

		if (wq->request.quant == ADJUSTED_QP_FLAG) {
			ptr = (u8 *) &cmd_info[17];
			memcpy(wq->quant_tbl_i4, ptr,
				sizeof(wq->quant_tbl_i4));
			ptr += sizeof(wq->quant_tbl_i4);
			memcpy(wq->quant_tbl_i16, ptr,
				sizeof(wq->quant_tbl_i16));
			ptr += sizeof(wq->quant_tbl_i16);
			memcpy(wq->quant_tbl_me, ptr,
				sizeof(wq->quant_tbl_me));
			wq->request.i4_weight -=
				cmd_info[data_offset++];
			wq->request.i16_weight -=
				cmd_info[data_offset++];
			wq->request.me_weight -=
				cmd_info[data_offset++];
			if (qp_table_debug) {
				u8 *qp_tb = (u8 *)(&wq->quant_tbl_i4[0]);
				for (i = 0; i < 32; i++) {
					enc_pr(LOG_INFO, "%d ", *qp_tb);
					qp_tb++;
				}
				enc_pr(LOG_INFO, "\n");

				qp_tb = (u8 *)(&wq->quant_tbl_i16[0]);
				for (i = 0; i < 32; i++) {
					enc_pr(LOG_INFO, "%d ", *qp_tb);
					qp_tb++;
				}
				enc_pr(LOG_INFO, "\n");

				qp_tb = (u8 *)(&wq->quant_tbl_me[0]);
				for (i = 0; i < 32; i++) {
					enc_pr(LOG_INFO, "%d ", *qp_tb);
					qp_tb++;
				}
				enc_pr(LOG_INFO, "\n");
			}
		} else {
			memset(wq->quant_tbl_me, wq->request.quant,
				sizeof(wq->quant_tbl_me));
			memset(wq->quant_tbl_i4, wq->request.quant,
				sizeof(wq->quant_tbl_i4));
			memset(wq->quant_tbl_i16, wq->request.quant,
				sizeof(wq->quant_tbl_i16));
			data_offset += 3;
		}
#ifdef H264_ENC_CBR
		wq->cbr_info.block_w = cmd_info[data_offset++];
		wq->cbr_info.block_h = cmd_info[data_offset++];
		wq->cbr_info.long_th = cmd_info[data_offset++];
		wq->cbr_info.start_tbl_id = cmd_info[data_offset++];
		wq->cbr_info.short_shift = CBR_SHORT_SHIFT;
		wq->cbr_info.long_mb_num = CBR_LONG_MB_NUM;
#endif
	} else {
		enc_pr(LOG_ERROR, "error cmd = %d, wq: %p.\n",
			cmd, (void *)wq);
		return -1;
	}
	wq->request.parent = wq;
	return 0;
}

void amvenc_avc_start_cmd(struct encode_wq_s *wq,
			  struct encode_request_s *request)
{
	u32 reload_flag = 0;
	if (request->ucode_mode != encode_manager.ucode_index) {
		encode_manager.ucode_index = request->ucode_mode;
		if (reload_mc(wq)) {
			enc_pr(LOG_ERROR,
				"reload mc fail, wq:%p\n", (void *)wq);
			return;
		}
		reload_flag = 1;
		encode_manager.need_reset = true;
	}

	wq->hw_status = 0;
	wq->output_size = 0;
	wq->ucode_index = encode_manager.ucode_index;

	ie_me_mode = (0 & ME_PIXEL_MODE_MASK) << ME_PIXEL_MODE_SHIFT;
	if (encode_manager.need_reset) {
		encode_manager.need_reset = false;
		encode_manager.encode_hw_status = ENCODER_IDLE;
		amvenc_reset();
		avc_canvas_init(wq);
		avc_init_encoder(wq,
			(request->cmd == ENCODER_IDR) ? true : false);
		avc_init_input_buffer(wq);
		avc_init_output_buffer(wq);
		avc_prot_init(wq, request, request->quant,
			(request->cmd == ENCODER_IDR) ? true : false);
		avc_init_assit_buffer(wq);
		enc_pr(LOG_INFO,
			"begin to new frame, request->cmd: %d, ucode mode: %d, wq:%p.\n",
			request->cmd, request->ucode_mode, (void *)wq);
	}
	if ((request->cmd == ENCODER_IDR) ||
		(request->cmd == ENCODER_NON_IDR)) {
		avc_init_dblk_buffer(wq->mem.dblk_buf_canvas);
		avc_init_reference_buffer(wq->mem.ref_buf_canvas);
	}
	if ((request->cmd == ENCODER_IDR) ||
		(request->cmd == ENCODER_NON_IDR))
		set_input_format(wq, request);
	if (request->cmd == ENCODER_IDR)
		ie_me_mb_type = HENC_MB_Type_I4MB;
	else if (request->cmd == ENCODER_NON_IDR)
		ie_me_mb_type =
			(HENC_SKIP_RUN_AUTO << 16) |
			(HENC_MB_Type_AUTO << 4) |
			(HENC_MB_Type_AUTO << 0);
	else
		ie_me_mb_type = 0;
	avc_init_ie_me_parameter(wq, request->quant);

#ifdef MULTI_SLICE_MC
	if (fixed_slice_cfg)
		WRITE_HREG(FIXED_SLICE_CFG, fixed_slice_cfg);
	else if (wq->pic.rows_per_slice !=
		(wq->pic.encoder_height + 15) >> 4) {
		u32 mb_per_slice = (wq->pic.encoder_height + 15) >> 4;
		mb_per_slice = mb_per_slice * wq->pic.rows_per_slice;
		WRITE_HREG(FIXED_SLICE_CFG, mb_per_slice);
	} else
		WRITE_HREG(FIXED_SLICE_CFG, 0);
#else
	WRITE_HREG(FIXED_SLICE_CFG, 0);
#endif

	encode_manager.encode_hw_status = request->cmd;
	wq->hw_status = request->cmd;
	WRITE_HREG(ENCODER_STATUS , request->cmd);
	if ((request->cmd == ENCODER_IDR)
		|| (request->cmd == ENCODER_NON_IDR)
		|| (request->cmd == ENCODER_SEQUENCE)
		|| (request->cmd == ENCODER_PICTURE))
		encode_manager.process_irq = false;

	if (reload_flag)
		amvenc_start();
	enc_pr(LOG_ALL, "amvenc_avc_start cmd, wq:%p.\n", (void *)wq);
}

static void dma_flush(u32 buf_start , u32 buf_size)
{
	if ((buf_start == 0) || (buf_size == 0))
		return;
	dma_sync_single_for_device(
		&encode_manager.this_pdev->dev, buf_start,
		buf_size, DMA_TO_DEVICE);
}

static void cache_flush(u32 buf_start , u32 buf_size)
{
	if ((buf_start == 0) || (buf_size == 0))
		return;
	dma_sync_single_for_cpu(
		&encode_manager.this_pdev->dev, buf_start,
		buf_size, DMA_FROM_DEVICE);
}

static u32 getbuffer(struct encode_wq_s *wq, u32 type)
{
	u32 ret = 0;

	switch (type) {
	case ENCODER_BUFFER_INPUT:
		ret = wq->mem.dct_buff_start_addr;
		break;
	case ENCODER_BUFFER_REF0:
		ret = wq->mem.dct_buff_start_addr +
			wq->mem.bufspec.dec0_y.buf_start;
		break;
	case ENCODER_BUFFER_REF1:
		ret = wq->mem.dct_buff_start_addr +
			wq->mem.bufspec.dec1_y.buf_start;
		break;
	case ENCODER_BUFFER_OUTPUT:
		ret = wq->mem.BitstreamStart;
		break;
	case ENCODER_BUFFER_DUMP:
		ret = wq->mem.dump_info_ddr_start_addr;
		break;
	case ENCODER_BUFFER_CBR:
		ret = wq->mem.cbr_info_ddr_start_addr;
		break;
	default:
		break;
	}
	return ret;
}

s32 amvenc_avc_start(struct encode_wq_s *wq, u32 clock)
{
	const char *p = select_ucode(encode_manager.ucode_index);

	avc_poweron(clock);
	avc_canvas_init(wq);

	WRITE_HREG(HCODEC_ASSIST_MMC_CTRL1, 0x32);

	if (amvenc_loadmc(p, wq) < 0)
		return -EBUSY;

	encode_manager.need_reset = true;
	encode_manager.process_irq = false;
	encode_manager.encode_hw_status = ENCODER_IDLE;
	amvenc_reset();
	avc_init_encoder(wq, true);
	avc_init_input_buffer(wq);  /* dct buffer setting */
	avc_init_output_buffer(wq);  /* output stream buffer */

	ie_me_mode = (0 & ME_PIXEL_MODE_MASK) << ME_PIXEL_MODE_SHIFT;
	avc_prot_init(wq, NULL, wq->pic.init_qppicture, true);
	if (request_irq(encode_manager.irq_num, enc_isr, IRQF_SHARED,
		"enc-irq", (void *)&encode_manager) == 0)
		encode_manager.irq_requested = true;
	else
		encode_manager.irq_requested = false;

	/* decoder buffer , need set before each frame start */
	avc_init_dblk_buffer(wq->mem.dblk_buf_canvas);
	/* reference  buffer , need set before each frame start */
	avc_init_reference_buffer(wq->mem.ref_buf_canvas);
	avc_init_assit_buffer(wq); /* assitant buffer for microcode */
	ie_me_mb_type = 0;
	avc_init_ie_me_parameter(wq, wq->pic.init_qppicture);
	WRITE_HREG(ENCODER_STATUS , ENCODER_IDLE);

#ifdef MULTI_SLICE_MC
	if (fixed_slice_cfg)
		WRITE_HREG(FIXED_SLICE_CFG, fixed_slice_cfg);
	else if (wq->pic.rows_per_slice !=
		(wq->pic.encoder_height + 15) >> 4) {
		u32 mb_per_slice = (wq->pic.encoder_height + 15) >> 4;
		mb_per_slice = mb_per_slice * wq->pic.rows_per_slice;
		WRITE_HREG(FIXED_SLICE_CFG, mb_per_slice);
	} else
		WRITE_HREG(FIXED_SLICE_CFG, 0);
#else
	WRITE_HREG(FIXED_SLICE_CFG, 0);
#endif
	amvenc_start();
	return 0;
}

void amvenc_avc_stop(void)
{
	if ((encode_manager.irq_num >= 0) &&
		(encode_manager.irq_requested == true)) {
		free_irq(encode_manager.irq_num, &encode_manager);
		encode_manager.irq_requested = false;
	}
	amvenc_stop();
	avc_poweroff();
}

static s32 avc_init(struct encode_wq_s *wq)
{
	s32 r = 0;

	encode_manager.ucode_index = wq->ucode_index;
	r = amvenc_avc_start(wq, clock_level);

	enc_pr(LOG_DEBUG,
		"init avc encode. microcode %d, ret=%d, wq:%p.\n",
		encode_manager.ucode_index, r, (void *)wq);
	return 0;
}

static s32 amvenc_avc_light_reset(struct encode_wq_s *wq, u32 value)
{
	s32 r = 0;

	amvenc_avc_stop();

	mdelay(value);

	encode_manager.ucode_index = UCODE_MODE_FULL;
	r = amvenc_avc_start(wq, clock_level);

	enc_pr(LOG_DEBUG,
		"amvenc_avc_light_reset finish, wq:%p. ret=%d\n",
		 (void *)wq, r);
	return r;
}

#ifdef CONFIG_CMA
static u32 checkCMA(void)
{
	u32 ret;
	if (encode_manager.cma_pool_size > 0) {
		ret = encode_manager.cma_pool_size;
		ret = ret / MIN_SIZE;
	} else
		ret = 0;
	return ret;
}
#endif

/* file operation */
static s32 amvenc_avc_open(struct inode *inode, struct file *file)
{
	s32 r = 0;
	struct encode_wq_s *wq = NULL;
	file->private_data = NULL;
	enc_pr(LOG_DEBUG, "avc open\n");
#ifdef CONFIG_AM_JPEG_ENCODER
	if (jpegenc_on() == true) {
		enc_pr(LOG_ERROR,
			"hcodec in use for JPEG Encode now.\n");
		return -EBUSY;
	}
#endif

#ifdef CONFIG_CMA
	if ((encode_manager.use_reserve == false) &&
	    (encode_manager.check_cma == false)) {
		encode_manager.max_instance = checkCMA();
		if (encode_manager.max_instance > 0) {
			enc_pr(LOG_DEBUG,
				"amvenc_avc  check CMA pool sucess, max instance: %d.\n",
				encode_manager.max_instance);
		} else {
			enc_pr(LOG_ERROR,
				"amvenc_avc CMA pool too small.\n");
		}
		encode_manager.check_cma = true;
	}
#endif

	wq = create_encode_work_queue();
	if (wq == NULL) {
		enc_pr(LOG_ERROR, "amvenc_avc create instance fail.\n");
		return -EBUSY;
	}

#ifdef CONFIG_CMA
	if (encode_manager.use_reserve == false) {
		wq->mem.buf_start = codec_mm_alloc_for_dma(ENCODE_NAME,
			MIN_SIZE  >> PAGE_SHIFT, 0,
			CODEC_MM_FLAGS_CPU);
		if (wq->mem.buf_start) {
			wq->mem.buf_size = MIN_SIZE;
			enc_pr(LOG_DEBUG,
				"allocating phys 0x%x, size %dk, wq:%p.\n",
				wq->mem.buf_start,
				wq->mem.buf_size >> 10, (void *)wq);
		} else {
			enc_pr(LOG_ERROR,
				"CMA failed to allocate dma buffer for %s, wq:%p.\n",
				encode_manager.this_pdev->name,
				(void *)wq);
			destroy_encode_work_queue(wq);
			return -ENOMEM;
		}
	}
#endif

	if (wq->mem.buf_start == 0 ||
		wq->mem.buf_size < MIN_SIZE) {
		enc_pr(LOG_ERROR,
			"alloc mem failed, start: 0x%x, size:0x%x, wq:%p.\n",
			wq->mem.buf_start,
			wq->mem.buf_size, (void *)wq);
		destroy_encode_work_queue(wq);
		return -ENOMEM;
	}

	memcpy(&wq->mem.bufspec, &amvenc_buffspec[0],
	       sizeof(struct BuffInfo_s));

	enc_pr(LOG_DEBUG,
		"amvenc_avc  memory config sucess, buff start:0x%x, size is 0x%x, wq:%p.\n",
		wq->mem.buf_start, wq->mem.buf_size, (void *)wq);

	file->private_data = (void *) wq;
	return r;
}

static s32 amvenc_avc_release(struct inode *inode, struct file *file)
{
	struct encode_wq_s *wq = (struct encode_wq_s *)file->private_data;
	if (wq) {
		enc_pr(LOG_DEBUG, "avc release, wq:%p\n", (void *)wq);
		destroy_encode_work_queue(wq);
	}
	return 0;
}

static long amvenc_avc_ioctl(struct file *file, u32 cmd, ulong arg)
{
	long r = 0;
	u32 amrisc_cmd = 0;
	struct encode_wq_s *wq = (struct encode_wq_s *)file->private_data;
#define MAX_ADDR_INFO_SIZE 50
	u32 addr_info[MAX_ADDR_INFO_SIZE + 4];
	ulong argV;
	u32 buf_start;
	s32 canvas = -1;
	struct canvas_s dst;
	switch (cmd) {
	case AMVENC_AVC_IOC_GET_ADDR:
		if ((wq->mem.ref_buf_canvas & 0xff) == (ENC_CANVAS_OFFSET))
			put_user(1, (u32 *)arg);
		else
			put_user(2, (u32 *)arg);
		break;
	case AMVENC_AVC_IOC_INPUT_UPDATE:
		break;
	case AMVENC_AVC_IOC_NEW_CMD:
		if (copy_from_user(addr_info, (void *)arg,
			MAX_ADDR_INFO_SIZE * sizeof(u32))) {
			enc_pr(LOG_ERROR,
				"avc get new cmd error, wq:%p.\n", (void *)wq);
			return -1;
		}
		r = convert_request(wq, addr_info);
		if (r == 0)
			r = encode_wq_add_request(wq);
		if (r) {
			enc_pr(LOG_ERROR,
				"avc add new request error, wq:%p.\n",
				(void *)wq);
		}
		break;
	case AMVENC_AVC_IOC_GET_STAGE:
		put_user(wq->hw_status, (u32 *)arg);
		break;
	case AMVENC_AVC_IOC_GET_OUTPUT_SIZE:
		addr_info[0] = wq->output_size;
		addr_info[1] = wq->me_weight;
		addr_info[2] = wq->i4_weight;
		addr_info[3] = wq->i16_weight;
		r = copy_to_user((u32 *)arg,
			addr_info , 4 * sizeof(u32));
		break;
	case AMVENC_AVC_IOC_CONFIG_INIT:
		if (copy_from_user(addr_info, (void *)arg,
			MAX_ADDR_INFO_SIZE * sizeof(u32))) {
			enc_pr(LOG_ERROR,
				"avc config init error, wq:%p.\n", (void *)wq);
			return -1;
		}
		wq->ucode_index = UCODE_MODE_FULL;
#ifdef MULTI_SLICE_MC
		wq->pic.rows_per_slice = addr_info[1];
		enc_pr(LOG_DEBUG,
			"avc init -- rows_per_slice: %d, wq: %p.\n",
			wq->pic.rows_per_slice, (void *)wq);
#endif
		enc_pr(LOG_DEBUG,
			"avc init as mode %d, wq: %p.\n",
			wq->ucode_index, (void *)wq);

		if (addr_info[2] > wq->mem.bufspec.max_width ||
		    addr_info[3] > wq->mem.bufspec.max_height) {
			enc_pr(LOG_ERROR,
				"avc config init- encode size %dx%d is larger than supported (%dx%d).  wq:%p.\n",
				addr_info[2], addr_info[3],
				wq->mem.bufspec.max_width,
				wq->mem.bufspec.max_height, (void *)wq);
			return -1;
		}
		wq->pic.encoder_width = addr_info[2];
		wq->pic.encoder_height = addr_info[3];
		if (wq->pic.encoder_width *
			wq->pic.encoder_height >= 1280 * 720)
			clock_level = 6;
		 else
			clock_level = 5;
		avc_buffspec_init(wq);
		complete(&encode_manager.event.request_in_com);
		addr_info[1] = wq->mem.bufspec.dct.buf_start;
		addr_info[2] = wq->mem.bufspec.dct.buf_size;
		addr_info[3] = wq->mem.bufspec.bitstream.buf_start;
		addr_info[4] = wq->mem.bufspec.bitstream.buf_size;
		addr_info[5] = wq->mem.bufspec.scale_buff.buf_start;
		addr_info[6] = wq->mem.bufspec.scale_buff.buf_size;
		addr_info[7] = wq->mem.bufspec.dump_info.buf_start;
		addr_info[8] = wq->mem.bufspec.dump_info.buf_size;
		addr_info[9] = wq->mem.bufspec.cbr_info.buf_start;
		addr_info[10] = wq->mem.bufspec.cbr_info.buf_size;
		r = copy_to_user((u32 *)arg, addr_info , 11*sizeof(u32));
		break;
	case AMVENC_AVC_IOC_FLUSH_CACHE:
		if (copy_from_user(addr_info, (void *)arg,
				   MAX_ADDR_INFO_SIZE * sizeof(u32))) {
			enc_pr(LOG_ERROR,
				"avc flush cache error, wq: %p.\n", (void *)wq);
			return -1;
		}
		buf_start = getbuffer(wq, addr_info[0]);
		dma_flush(buf_start + addr_info[1],
			addr_info[2] - addr_info[1]);
		break;
	case AMVENC_AVC_IOC_FLUSH_DMA:
		if (copy_from_user(addr_info, (void *)arg,
				MAX_ADDR_INFO_SIZE * sizeof(u32))) {
			enc_pr(LOG_ERROR,
				"avc flush dma error, wq:%p.\n", (void *)wq);
			return -1;
		}
		buf_start = getbuffer(wq, addr_info[0]);
		cache_flush(buf_start + addr_info[1],
			addr_info[2] - addr_info[1]);
		break;
	case AMVENC_AVC_IOC_GET_BUFFINFO:
		put_user(wq->mem.buf_size, (u32 *)arg);
		break;
	case AMVENC_AVC_IOC_GET_DEVINFO:
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
			/* send the same id as GXTVBB to upper*/
			r = copy_to_user((s8 *)arg, AMVENC_DEVINFO_GXTVBB,
				strlen(AMVENC_DEVINFO_GXTVBB));
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB) {
			r = copy_to_user((s8 *)arg, AMVENC_DEVINFO_GXTVBB,
				strlen(AMVENC_DEVINFO_GXTVBB));
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) {
			r = copy_to_user((s8 *)arg, AMVENC_DEVINFO_GXBB,
				strlen(AMVENC_DEVINFO_GXBB));
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_MG9TV) {
			r = copy_to_user((s8 *)arg, AMVENC_DEVINFO_G9,
				strlen(AMVENC_DEVINFO_G9));
		} else {
			r = copy_to_user((s8 *)arg, AMVENC_DEVINFO_M8,
				strlen(AMVENC_DEVINFO_M8));
		}
		break;
	case AMVENC_AVC_IOC_SUBMIT:
		get_user(amrisc_cmd, ((u32 *)arg));
		if (amrisc_cmd == ENCODER_IDR) {
			wq->pic.idr_pic_id++;
			if (wq->pic.idr_pic_id > 65535)
				wq->pic.idr_pic_id = 0;
			wq->pic.pic_order_cnt_lsb = 2;
			wq->pic.frame_number = 1;
		} else if (amrisc_cmd == ENCODER_NON_IDR) {
			wq->pic.frame_number++;
			wq->pic.pic_order_cnt_lsb += 2;
			if (wq->pic.frame_number > 65535)
				wq->pic.frame_number = 0;
		}
		amrisc_cmd = wq->mem.dblk_buf_canvas;
		wq->mem.dblk_buf_canvas = wq->mem.ref_buf_canvas;
		/* current dblk buffer as next reference buffer */
		wq->mem.ref_buf_canvas = amrisc_cmd;
		break;
	case AMVENC_AVC_IOC_READ_CANVAS:
		get_user(argV, ((u32 *)arg));
		canvas = argV;
		if (canvas & 0xff) {
			canvas_read(canvas & 0xff, &dst);
			addr_info[0] = dst.addr;
			if ((canvas & 0xff00) >> 8)
				canvas_read((canvas & 0xff00) >> 8, &dst);
			if ((canvas & 0xff0000) >> 16)
				canvas_read((canvas & 0xff0000) >> 16, &dst);
			addr_info[1] = dst.addr - addr_info[0] +
				dst.width * dst.height;
		} else {
			addr_info[0] = 0;
			addr_info[1] = 0;
		}
		dma_flush(dst.addr, dst.width * dst.height * 3 / 2);
		r = copy_to_user((u32 *)arg, addr_info , 2 * sizeof(u32));
		break;
	case AMVENC_AVC_IOC_MAX_INSTANCE:
		put_user(encode_manager.max_instance, (u32 *)arg);
		break;
	default:
		r = -1;
		break;
	}
	return r;
}

#ifdef CONFIG_COMPAT
static long amvenc_avc_compat_ioctl(struct file *filp,
	unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = amvenc_avc_ioctl(filp, cmd, args);
	return ret;
}
#endif

static s32 avc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct encode_wq_s *wq = (struct encode_wq_s *)filp->private_data;
	ulong off = vma->vm_pgoff << PAGE_SHIFT;
	ulong vma_size = vma->vm_end - vma->vm_start;

	if (vma_size == 0) {
		enc_pr(LOG_ERROR, "vma_size is 0, wq:%p.\n", (void *)wq);
		return -EAGAIN;
	}
	if (!off)
		off += wq->mem.buf_start;
	enc_pr(LOG_ALL,
		"vma_size is %ld , off is %ld, wq:%p.\n",
		vma_size , off, (void *)wq);
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP | VM_IO;
	/* vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); */
	if (remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		enc_pr(LOG_ERROR,
			"set_cached: failed remap_pfn_range, wq:%p.\n",
			(void *)wq);
		return -EAGAIN;
	}
	return 0;
}

static u32 amvenc_avc_poll(struct file *file, poll_table *wait_table)
{
	struct encode_wq_s *wq = (struct encode_wq_s *)file->private_data;
	poll_wait(file, &wq->request_complete, wait_table);

	if (atomic_read(&wq->request_ready)) {
		atomic_dec(&wq->request_ready);
		return POLLIN | POLLRDNORM;
	}
	return 0;
}

static const struct file_operations amvenc_avc_fops = {
	.owner = THIS_MODULE,
	.open = amvenc_avc_open,
	.mmap = avc_mmap,
	.release = amvenc_avc_release,
	.unlocked_ioctl = amvenc_avc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amvenc_avc_compat_ioctl,
#endif
	.poll = amvenc_avc_poll,
};

/* work queue function */
static s32 encode_process_request(struct encode_manager_s *manager,
	struct encode_queue_item_s *pitem)
{
	s32 ret = 0;
	struct encode_wq_s *wq = pitem->request.parent;
	struct encode_request_s *request = &pitem->request;
	u32 timeout = (request->timeout == 0) ?
		1 : msecs_to_jiffies(request->timeout);
	u32 buf_start = 0;
	u32 size = 0;
	u32 flush_size = ((wq->pic.encoder_width + 31) >> 5 << 5) *
		((wq->pic.encoder_height + 15) >> 4 << 4) * 3 / 2;

#ifdef H264_ENC_CBR
	if (request->cmd == ENCODER_IDR || request->cmd == ENCODER_NON_IDR) {
		if (request->flush_flag & AMVENC_FLUSH_FLAG_CBR
			&& get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
			void *vaddr =
				phys_to_virt(wq->mem.cbr_info_ddr_start_addr);
			ConvertTable2Risc(vaddr, 0xa00);
			buf_start = getbuffer(wq, ENCODER_BUFFER_CBR);
			dma_flush(buf_start, wq->mem.cbr_info_ddr_size);
		}
	}
#endif

Again:
	amvenc_avc_start_cmd(wq, request);

	if (no_timeout) {
		wait_event_interruptible(manager->event.hw_complete,
			(manager->encode_hw_status == ENCODER_IDR_DONE
			|| manager->encode_hw_status == ENCODER_NON_IDR_DONE
			|| manager->encode_hw_status == ENCODER_SEQUENCE_DONE
			|| manager->encode_hw_status == ENCODER_PICTURE_DONE));
	} else {
		wait_event_interruptible_timeout(manager->event.hw_complete,
			((manager->encode_hw_status == ENCODER_IDR_DONE)
			|| (manager->encode_hw_status == ENCODER_NON_IDR_DONE)
			|| (manager->encode_hw_status == ENCODER_SEQUENCE_DONE)
			|| (manager->encode_hw_status == ENCODER_PICTURE_DONE)),
			timeout);
	}

	if ((request->cmd == ENCODER_SEQUENCE) &&
	    (manager->encode_hw_status == ENCODER_SEQUENCE_DONE)) {
		wq->sps_size = READ_HREG(HCODEC_VLC_TOTAL_BYTES);
		wq->hw_status = manager->encode_hw_status;
		request->cmd = ENCODER_PICTURE;
		goto Again;
	} else if ((request->cmd == ENCODER_PICTURE) &&
		   (manager->encode_hw_status == ENCODER_PICTURE_DONE)) {
		wq->pps_size =
			READ_HREG(HCODEC_VLC_TOTAL_BYTES) - wq->sps_size;
		wq->hw_status = manager->encode_hw_status;
		if (request->flush_flag & AMVENC_FLUSH_FLAG_OUTPUT) {
			buf_start = getbuffer(wq, ENCODER_BUFFER_OUTPUT);
			cache_flush(buf_start,
				wq->sps_size + wq->pps_size);
		}
		wq->output_size = (wq->sps_size << 16) | wq->pps_size;
	} else {
		wq->hw_status = manager->encode_hw_status;
		if ((manager->encode_hw_status == ENCODER_IDR_DONE) ||
		    (manager->encode_hw_status == ENCODER_NON_IDR_DONE)) {
			wq->output_size = READ_HREG(HCODEC_VLC_TOTAL_BYTES);
			if (request->flush_flag & AMVENC_FLUSH_FLAG_OUTPUT) {
				buf_start = getbuffer(wq,
					ENCODER_BUFFER_OUTPUT);
				cache_flush(buf_start, wq->output_size);
			}
			if (request->flush_flag &
				AMVENC_FLUSH_FLAG_DUMP) {
				buf_start = getbuffer(wq,
					ENCODER_BUFFER_DUMP);
				size = wq->mem.dump_info_ddr_size;
				cache_flush(buf_start, size);
			}
			if (request->flush_flag &
				AMVENC_FLUSH_FLAG_REFERENCE) {
				u32 ref_id = ENCODER_BUFFER_REF0;
				if ((wq->mem.ref_buf_canvas & 0xff) ==
					(ENC_CANVAS_OFFSET))
					ref_id = ENCODER_BUFFER_REF0;
				else
					ref_id = ENCODER_BUFFER_REF1;
				buf_start = getbuffer(wq, ref_id);
				cache_flush(buf_start, flush_size);
			}
		} else {
			manager->encode_hw_status = ENCODER_ERROR;
			enc_pr(LOG_DEBUG, "avc encode light reset --- ");
			enc_pr(LOG_DEBUG,
				"frame type: %s, size: %dx%d, wq: %p\n",
				(request->cmd == ENCODER_IDR) ? "IDR" : "P",
				wq->pic.encoder_width,
				wq->pic.encoder_height, (void *)wq);
			enc_pr(LOG_DEBUG,
				"mb info: 0x%x, encode status: 0x%x, dct status: 0x%x ",
				READ_HREG(HCODEC_VLC_MB_INFO),
				READ_HREG(ENCODER_STATUS),
				READ_HREG(HCODEC_QDCT_STATUS_CTRL));
			enc_pr(LOG_DEBUG,
				"vlc status: 0x%x, me status: 0x%x, risc pc:0x%x, debug:0x%x\n",
				READ_HREG(HCODEC_VLC_STATUS_CTRL),
				READ_HREG(HCODEC_ME_STATUS),
				READ_HREG(HCODEC_MPC_E),
				READ_HREG(DEBUG_REG));
			amvenc_avc_light_reset(wq, 30);
		}
	}
	atomic_inc(&wq->request_ready);
	wake_up_interruptible(&wq->request_complete);
	return ret;
}

s32 encode_wq_add_request(struct encode_wq_s *wq)
{
	struct encode_queue_item_s *pitem = NULL;
	struct list_head *head = NULL;
	struct encode_wq_s *tmp = NULL;
	bool find = false;

	spin_lock(&encode_manager.event.sem_lock);

	head =  &encode_manager.wq;
	list_for_each_entry(tmp, head, list) {
		if ((wq == tmp) && (wq != NULL)) {
			find = true;
			break;
		}
	}

	if (find == false) {
		enc_pr(LOG_ERROR, "current wq (%p) doesn't register.\n",
			(void *)wq);
		goto error;
	}

	if (list_empty(&encode_manager.free_queue)) {
		enc_pr(LOG_ERROR, "work queue no space, wq:%p.\n",
			(void *)wq);
		goto error;
	}

	pitem = list_entry(encode_manager.free_queue.next,
		struct encode_queue_item_s, list);
	if (IS_ERR(pitem))
		goto error;

	memcpy(&pitem->request, &wq->request, sizeof(struct encode_request_s));
	memset(&wq->request, 0, sizeof(struct encode_request_s));
	wq->hw_status = 0;
	wq->output_size = 0;
	pitem->request.parent = wq;
	list_move_tail(&pitem->list, &encode_manager.process_queue);
	spin_unlock(&encode_manager.event.sem_lock);

	enc_pr(LOG_INFO,
		"add new work ok, cmd:%d, ucode mode: %d, wq:%p.\n",
		pitem->request.cmd, pitem->request.ucode_mode,
		(void *)wq);
	complete(&encode_manager.event.request_in_com);/* new cmd come in */
	return 0;
error:
	spin_unlock(&encode_manager.event.sem_lock);
	return -1;
}

struct encode_wq_s *create_encode_work_queue(void)
{
	struct encode_wq_s *encode_work_queue = NULL;
	bool done = false;
	u32 i, max_instance;
	struct Buff_s *reserve_buff;

	encode_work_queue = kzalloc(sizeof(struct encode_wq_s), GFP_KERNEL);
	if (IS_ERR(encode_work_queue)) {
		enc_pr(LOG_ERROR, "can't create work queue\n");
		return NULL;
	}
	max_instance = encode_manager.max_instance;
	encode_work_queue->pic.init_qppicture = 26;
	encode_work_queue->pic.log2_max_frame_num = 4;
	encode_work_queue->pic.log2_max_pic_order_cnt_lsb = 4;
	encode_work_queue->pic.idr_pic_id = 0;
	encode_work_queue->pic.frame_number = 0;
	encode_work_queue->pic.pic_order_cnt_lsb = 0;
	encode_work_queue->ucode_index = UCODE_MODE_FULL;

#ifdef H264_ENC_CBR
	encode_work_queue->cbr_info.block_w = 16;
	encode_work_queue->cbr_info.block_h = 9;
	encode_work_queue->cbr_info.long_th = CBR_LONG_THRESH;
	encode_work_queue->cbr_info.start_tbl_id = START_TABLE_ID;
	encode_work_queue->cbr_info.short_shift = CBR_SHORT_SHIFT;
	encode_work_queue->cbr_info.long_mb_num = CBR_LONG_MB_NUM;
#endif
	init_waitqueue_head(&encode_work_queue->request_complete);
	atomic_set(&encode_work_queue->request_ready, 0);
	spin_lock(&encode_manager.event.sem_lock);
	if (encode_manager.wq_count < encode_manager.max_instance) {
		list_add_tail(&encode_work_queue->list, &encode_manager.wq);
		encode_manager.wq_count++;
		if (encode_manager.use_reserve == true) {
			for (i = 0; i < max_instance; i++) {
				reserve_buff = &encode_manager.reserve_buff[i];
				if (reserve_buff->used == false) {
					encode_work_queue->mem.buf_start =
						reserve_buff->buf_start;
					encode_work_queue->mem.buf_size =
						reserve_buff->buf_size;
					reserve_buff->used = true;
					done = true;
					break;
				}
			}
		} else
			done = true;
	}
	spin_unlock(&encode_manager.event.sem_lock);
	if (done == false) {
		kfree(encode_work_queue);
		encode_work_queue = NULL;
		enc_pr(LOG_ERROR, "too many work queue!\n");
	}
	return encode_work_queue; /* find it */
}

static void _destroy_encode_work_queue(struct encode_manager_s *manager,
	struct encode_wq_s **wq,
	struct encode_wq_s *encode_work_queue,
	bool *find)
{
	struct list_head *head;
	struct encode_wq_s *wp_tmp = NULL;
	u32 i, max_instance;
	struct Buff_s *reserve_buff;
	u32 buf_start = encode_work_queue->mem.buf_start;

	max_instance = manager->max_instance;
	head =  &manager->wq;
	list_for_each_entry_safe((*wq), wp_tmp, head, list) {
		if ((*wq) && (*wq == encode_work_queue)) {
			list_del(&(*wq)->list);
			if (manager->use_reserve == true) {
				for (i = 0; i < max_instance; i++) {
					reserve_buff =
						&manager->reserve_buff[i];
					if (reserve_buff->used	== true &&
						buf_start ==
						reserve_buff->buf_start) {
						reserve_buff->used = false;
						break;
					}
				}
			}
			*find = true;
			manager->wq_count--;
			enc_pr(LOG_DEBUG,
				"remove  encode_work_queue %p sucess, %s line %d.\n",
				(void *)encode_work_queue,
				__func__, __LINE__);
			break;
		}
	}
}

s32 destroy_encode_work_queue(struct encode_wq_s *encode_work_queue)
{
	struct encode_queue_item_s *pitem, *tmp;
	struct encode_wq_s *wq = NULL;
	bool find = false;

	struct list_head *head;
	if (encode_work_queue) {
		spin_lock(&encode_manager.event.sem_lock);
		if (encode_manager.current_wq == encode_work_queue) {
			encode_manager.remove_flag = true;
			spin_unlock(&encode_manager.event.sem_lock);
			enc_pr(LOG_DEBUG,
				"warning--Destory the running queue, should not be here.\n");
			wait_for_completion(
				&encode_manager.event.process_complete);
			spin_lock(&encode_manager.event.sem_lock);
		} /* else we can delete it safely. */

		head =  &encode_manager.process_queue;
		list_for_each_entry_safe(pitem, tmp, head, list) {
			if (pitem && pitem->request.parent ==
				encode_work_queue) {
				pitem->request.parent = NULL;
				enc_pr(LOG_DEBUG,
					"warning--remove not process request, should not be here.\n");
				list_move_tail(&pitem->list,
					&encode_manager.free_queue);
			}
		}

		_destroy_encode_work_queue(&encode_manager, &wq,
			encode_work_queue, &find);
		spin_unlock(&encode_manager.event.sem_lock);
#ifdef CONFIG_CMA
		if (encode_work_queue->mem.buf_start) {
			codec_mm_free_for_dma(
				ENCODE_NAME,
				encode_work_queue->mem.buf_start);
			encode_work_queue->mem.buf_start = 0;

		}
#endif
		kfree(encode_work_queue);
		complete(&encode_manager.event.request_in_com);
	}
	return  0;
}

static s32 encode_monitor_thread(void *data)
{
	struct encode_manager_s *manager = (struct encode_manager_s *)data;
	struct encode_queue_item_s *pitem = NULL;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1 };
	s32 ret = 0;
	enc_pr(LOG_DEBUG, "encode workqueue monitor start.\n");
	sched_setscheduler(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);
	/* setup current_wq here. */
	while (manager->process_queue_state != ENCODE_PROCESS_QUEUE_STOP) {
		if (kthread_should_stop())
			break;

		ret = wait_for_completion_interruptible(
				&manager->event.request_in_com);

		if (ret == -ERESTARTSYS)
			break;

		if (kthread_should_stop())
			break;
		if (manager->inited == false) {
			spin_lock(&manager->event.sem_lock);
			if (!list_empty(&manager->wq)) {
				struct encode_wq_s *first_wq =
					list_entry(manager->wq.next,
					struct encode_wq_s, list);
				manager->current_wq = first_wq;
				spin_unlock(&manager->event.sem_lock);
				if (first_wq) {
#ifdef CONFIG_AM_GE2D
					if (!manager->context)
						manager->context =
						create_ge2d_work_queue();
#endif
					avc_init(first_wq);
					manager->inited = true;
				}
				spin_lock(&manager->event.sem_lock);
				manager->current_wq = NULL;
				spin_unlock(&manager->event.sem_lock);
				if (manager->remove_flag) {
					complete(
						&manager
						->event.process_complete);
					manager->remove_flag = false;
				}
			} else
				spin_unlock(&manager->event.sem_lock);
			continue;
		}

		spin_lock(&manager->event.sem_lock);
		pitem = NULL;
		if (list_empty(&manager->wq)) {
			spin_unlock(&manager->event.sem_lock);
			manager->inited = false;
			amvenc_avc_stop();
#ifdef CONFIG_AM_GE2D
			if (manager->context) {
				destroy_ge2d_work_queue(manager->context);
				manager->context = NULL;
			}
#endif
			enc_pr(LOG_DEBUG, "power off encode.\n");
			continue;
		} else if (!list_empty(&manager->process_queue)) {
			pitem = list_entry(manager->process_queue.next,
				struct encode_queue_item_s, list);
			list_del(&pitem->list);
			manager->current_item = pitem;
			manager->current_wq = pitem->request.parent;
		}
		spin_unlock(&manager->event.sem_lock);

		if (pitem) {
			encode_process_request(manager, pitem);
			spin_lock(&manager->event.sem_lock);
			list_add_tail(&pitem->list, &manager->free_queue);
			manager->current_item = NULL;
			manager->last_wq = manager->current_wq;
			manager->current_wq = NULL;
			spin_unlock(&manager->event.sem_lock);
		}
		if (manager->remove_flag) {
			complete(&manager->event.process_complete);
			manager->remove_flag = false;
		}
	}
	while (!kthread_should_stop())
		msleep(20);

	enc_pr(LOG_DEBUG, "exit encode_monitor_thread.\n");
	return 0;
}

static s32 encode_start_monitor(void)
{
	s32 ret = 0;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
		y_tnr_mot2alp_nrm_gain = 216;
		y_tnr_mot2alp_dis_gain = 144;
		c_tnr_mot2alp_nrm_gain = 216;
		c_tnr_mot2alp_dis_gain = 144;
	} else {
		/* more tnr */
		y_tnr_mot2alp_nrm_gain = 144;
		y_tnr_mot2alp_dis_gain = 96;
		c_tnr_mot2alp_nrm_gain = 144;
		c_tnr_mot2alp_dis_gain = 96;
	}

	enc_pr(LOG_DEBUG, "encode start monitor.\n");
	encode_manager.process_queue_state = ENCODE_PROCESS_QUEUE_START;
	encode_manager.encode_thread = kthread_run(encode_monitor_thread,
		&encode_manager, "encode_monitor");
	if (IS_ERR(encode_manager.encode_thread)) {
		ret = PTR_ERR(encode_manager.encode_thread);
		encode_manager.process_queue_state = ENCODE_PROCESS_QUEUE_STOP;
		enc_pr(LOG_ERROR,
			"encode monitor : failed to start kthread (%d)\n", ret);
	}
	return ret;
}

static s32  encode_stop_monitor(void)
{
	enc_pr(LOG_DEBUG, "stop encode monitor thread\n");
	if (encode_manager.encode_thread) {
		spin_lock(&encode_manager.event.sem_lock);
		if (!list_empty(&encode_manager.wq)) {
			u32 count = encode_manager.wq_count;
			spin_unlock(&encode_manager.event.sem_lock);
			enc_pr(LOG_ERROR,
				"stop encode monitor thread error, active wq (%d) is not 0.\n",
				 count);
			return -1;
		}
		spin_unlock(&encode_manager.event.sem_lock);
		encode_manager.process_queue_state = ENCODE_PROCESS_QUEUE_STOP;
		send_sig(SIGTERM, encode_manager.encode_thread, 1);
		complete(&encode_manager.event.request_in_com);
		kthread_stop(encode_manager.encode_thread);
		encode_manager.encode_thread = NULL;
		kfree(mc_addr);
		mc_addr = NULL;
	}
	return  0;
}

static s32 encode_wq_init(void)
{
	u32 i = 0;
	struct encode_queue_item_s *pitem = NULL;

	enc_pr(LOG_DEBUG, "encode_wq_init.\n");
	encode_manager.irq_requested = false;

	spin_lock_init(&encode_manager.event.sem_lock);
	init_completion(&encode_manager.event.request_in_com);
	init_waitqueue_head(&encode_manager.event.hw_complete);
	init_completion(&encode_manager.event.process_complete);
	INIT_LIST_HEAD(&encode_manager.process_queue);
	INIT_LIST_HEAD(&encode_manager.free_queue);
	INIT_LIST_HEAD(&encode_manager.wq);

	tasklet_init(&encode_manager.encode_tasklet,
		encode_isr_tasklet,
		(ulong)&encode_manager);

	for (i = 0; i < MAX_ENCODE_REQUEST; i++) {
		pitem = kcalloc(1,
			sizeof(struct encode_queue_item_s),
			GFP_KERNEL);
		if (IS_ERR(pitem)) {
			enc_pr(LOG_ERROR, "can't request queue item memory.\n");
			return -1;
		}
		pitem->request.parent = NULL;
		list_add_tail(&pitem->list, &encode_manager.free_queue);
	}
	encode_manager.current_wq = NULL;
	encode_manager.last_wq = NULL;
	encode_manager.encode_thread = NULL;
	encode_manager.current_item = NULL;
	encode_manager.wq_count = 0;
	encode_manager.remove_flag = false;
	InitEncodeWeight();
	if (encode_start_monitor()) {
		enc_pr(LOG_ERROR, "encode create thread error.\n");
		return -1;
	}
	return 0;
}

static s32 encode_wq_uninit(void)
{
	struct encode_queue_item_s *pitem, *tmp;
	struct list_head *head;
	u32 count = 0;
	s32 r = -1;
	enc_pr(LOG_DEBUG, "uninit encode wq.\n");
	if (encode_stop_monitor() == 0) {
		if ((encode_manager.irq_num >= 0) &&
			(encode_manager.irq_requested == true)) {
			free_irq(encode_manager.irq_num, &encode_manager);
			encode_manager.irq_requested = false;
		}
		spin_lock(&encode_manager.event.sem_lock);
		head =  &encode_manager.process_queue;
		list_for_each_entry_safe(pitem, tmp, head, list) {
			if (pitem) {
				list_del(&pitem->list);
				kfree(pitem);
				count++;
			}
		}
		head =  &encode_manager.free_queue;
		list_for_each_entry_safe(pitem, tmp, head, list) {
			if (pitem) {
				list_del(&pitem->list);
				kfree(pitem);
				count++;
			}
		}
		spin_unlock(&encode_manager.event.sem_lock);
		if (count == MAX_ENCODE_REQUEST)
			r = 0;
		else {
			enc_pr(LOG_ERROR, "lost some request item %d.\n",
				MAX_ENCODE_REQUEST - count);
		}
	}
	return  r;
}

static ssize_t encode_status_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	u32 process_count = 0;
	u32 free_count = 0;
	struct encode_queue_item_s *pitem = NULL;
	struct encode_wq_s *current_wq = NULL;
	struct encode_wq_s *last_wq = NULL;
	struct list_head *head = NULL;
	s32 irq_num = 0;
	u32 hw_status = 0;
	u32 process_queue_state = 0;
	u32 wq_count = 0;
	u32 ucode_index;
	bool need_reset;
	bool process_irq;
	bool inited;
	bool use_reserve;
	struct Buff_s reserve_mem;
	u32 max_instance;
#ifdef CONFIG_CMA
	bool check_cma = false;
#endif

	spin_lock(&encode_manager.event.sem_lock);
	head = &encode_manager.free_queue;
	list_for_each_entry(pitem, head , list) {
		free_count++;
		if (free_count > MAX_ENCODE_REQUEST)
			break;
	}

	head = &encode_manager.process_queue;
	list_for_each_entry(pitem, head , list) {
		process_count++;
		if (free_count > MAX_ENCODE_REQUEST)
			break;
	}

	current_wq = encode_manager.current_wq;
	last_wq = encode_manager.last_wq;
	pitem = encode_manager.current_item;
	irq_num = encode_manager.irq_num;
	hw_status = encode_manager.encode_hw_status;
	process_queue_state = encode_manager.process_queue_state;
	wq_count = encode_manager.wq_count;
	ucode_index = encode_manager.ucode_index;
	need_reset = encode_manager.need_reset;
	process_irq = encode_manager.process_irq;
	inited = encode_manager.inited;
	use_reserve = encode_manager.use_reserve;
	reserve_mem.buf_start = encode_manager.reserve_mem.buf_start;
	reserve_mem.buf_size = encode_manager.reserve_mem.buf_size;

	max_instance = encode_manager.max_instance;
#ifdef CONFIG_CMA
	check_cma = encode_manager.check_cma;
#endif

	spin_unlock(&encode_manager.event.sem_lock);

	enc_pr(LOG_DEBUG,
		"encode process queue count: %d, free queue count: %d.\n",
		process_count, free_count);
	enc_pr(LOG_DEBUG,
		"encode curent wq: %p, last wq: %p, wq count: %d, max_instance: %d.\n",
		current_wq, last_wq, wq_count, max_instance);
	if (current_wq)
		enc_pr(LOG_DEBUG,
			"encode curent wq -- encode width: %d, encode height: %d.\n",
			current_wq->pic.encoder_width,
			current_wq->pic.encoder_height);
	enc_pr(LOG_DEBUG,
		"encode curent pitem: %p, ucode_index: %d, hw_status: %d, need_reset: %s, process_irq: %s.\n",
		pitem, ucode_index, hw_status, need_reset ? "true" : "false",
		process_irq ? "true" : "false");
	enc_pr(LOG_DEBUG,
		"encode irq num: %d,  inited: %s, process_queue_state: %d.\n",
		irq_num, inited ? "true" : "false",  process_queue_state);
	if (use_reserve) {
		enc_pr(LOG_DEBUG,
			"encode use reserve memory, buffer start: 0x%x, size: %d MB.\n",
			 reserve_mem.buf_start,
			 reserve_mem.buf_size / SZ_1M);
	} else {
#ifdef CONFIG_CMA
		enc_pr(LOG_DEBUG, "encode check cma: %s.\n",
			check_cma ? "true" : "false");
#endif
	}
	return snprintf(buf, 40, "encode max instance: %d\n", max_instance);
}

static struct class_attribute amvenc_class_attrs[] = {
	__ATTR(encode_status,
	S_IRUGO | S_IWUSR,
	encode_status_show,
	NULL),
	__ATTR_NULL
};

static struct class amvenc_avc_class = {
	.name = CLASS_NAME,
	.class_attrs = amvenc_class_attrs,
};

s32 init_avc_device(void)
{
	s32  r = 0;
	r = register_chrdev(0, DEVICE_NAME, &amvenc_avc_fops);
	if (r <= 0) {
		enc_pr(LOG_ERROR, "register amvenc_avc device error.\n");
		return  r;
	}
	avc_device_major = r;

	r = class_register(&amvenc_avc_class);
	if (r < 0) {
		enc_pr(LOG_ERROR, "error create amvenc_avc class.\n");
		return r;
	}

	amvenc_avc_dev = device_create(&amvenc_avc_class, NULL,
		MKDEV(avc_device_major, 0), NULL,
		DEVICE_NAME);

	if (IS_ERR(amvenc_avc_dev)) {
		enc_pr(LOG_ERROR, "create amvenc_avc device error.\n");
		class_unregister(&amvenc_avc_class);
		return -1;
	}
	return r;
}

s32 uninit_avc_device(void)
{
	if (amvenc_avc_dev)
		device_destroy(&amvenc_avc_class, MKDEV(avc_device_major, 0));

	class_destroy(&amvenc_avc_class);

	unregister_chrdev(avc_device_major, DEVICE_NAME);
	return 0;
}

static s32 avc_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	s32 r;
	struct resource res;
	if (!rmem) {
		enc_pr(LOG_ERROR,
			"Can not obtain I/O memory, and will allocate avc buffer!\n");
		r = -EFAULT;
		return r;
	}
	res.start = (phys_addr_t)rmem->base;
	res.end = res.start + (phys_addr_t)rmem->size - 1;
	encode_manager.reserve_mem.buf_start = res.start;
	encode_manager.reserve_mem.buf_size = res.end - res.start + 1;

	if (encode_manager.reserve_mem.buf_size >=
		amvenc_buffspec[0].min_buffsize) {
		encode_manager.max_instance =
			encode_manager.reserve_mem.buf_size /
			amvenc_buffspec[0].min_buffsize;
		if (encode_manager.max_instance > MAX_ENCODE_INSTANCE)
			encode_manager.max_instance = MAX_ENCODE_INSTANCE;
		encode_manager.reserve_buff = kzalloc(
			encode_manager.max_instance *
			sizeof(struct Buff_s), GFP_KERNEL);
		if (encode_manager.reserve_buff) {
			u32 i;
			struct Buff_s *reserve_buff;
			u32 max_instance = encode_manager.max_instance;
			for (i = 0; i < max_instance; i++) {
				reserve_buff = &encode_manager.reserve_buff[i];
				reserve_buff->buf_start =
					i *
					amvenc_buffspec[0]
					.min_buffsize +
					encode_manager.reserve_mem.buf_start;
				reserve_buff->buf_size =
					encode_manager.reserve_mem.buf_start;
				reserve_buff->used = false;
			}
			encode_manager.use_reserve = true;
			r = 0;
			enc_pr(LOG_DEBUG,
				"amvenc_avc  use reserve memory, buff start: 0x%x, size: 0x%x, max instance is %d\n",
				encode_manager.reserve_mem.buf_start,
				encode_manager.reserve_mem.buf_size,
				encode_manager.max_instance);
		} else {
			enc_pr(LOG_ERROR,
				"amvenc_avc alloc reserve buffer pointer fail. max instance is %d.\n",
				encode_manager.max_instance);
			encode_manager.max_instance = 0;
			encode_manager.reserve_mem.buf_start = 0;
			encode_manager.reserve_mem.buf_size = 0;
			r = -ENOMEM;
		}
	} else {
		enc_pr(LOG_ERROR,
			"amvenc_avc memory resource too small, size is 0x%x. Need 0x%x bytes at least.\n",
			encode_manager.reserve_mem.buf_size,
			amvenc_buffspec[0]
			.min_buffsize);
		encode_manager.reserve_mem.buf_start = 0;
		encode_manager.reserve_mem.buf_size = 0;
		r = -ENOMEM;
	}
	return r;
}

static s32 amvenc_avc_probe(struct platform_device *pdev)
{
	/* struct resource mem; */
	s32 res_irq;
	s32 idx;
	s32 r;

	enc_pr(LOG_INFO, "amvenc_avc probe start.\n");

	encode_manager.this_pdev = pdev;
#ifdef CONFIG_CMA
	encode_manager.check_cma = false;
#endif
	encode_manager.reserve_mem.buf_start = 0;
	encode_manager.reserve_mem.buf_size = 0;
	encode_manager.use_reserve = false;
	encode_manager.max_instance = 0;
	encode_manager.reserve_buff = NULL;

	idx = of_reserved_mem_device_init(&pdev->dev);
	if (idx != 0) {
		enc_pr(LOG_DEBUG,
			"amvenc_avc_probe -- reserved memory config fail.\n");
	}

	if (encode_manager.use_reserve == false) {
#ifndef CONFIG_CMA
		enc_pr(LOG_ERROR,
			"amvenc_avc memory is invaild, probe fail!\n");
		return -EFAULT;
#else
		encode_manager.cma_pool_size =
			(codec_mm_get_total_size() > (MIN_SIZE * 2)) ?
			(MIN_SIZE * 2) : codec_mm_get_total_size();
		enc_pr(LOG_DEBUG,
			"amvenc_avc - cma memory pool size: %d MB\n",
			(u32)encode_manager.cma_pool_size / SZ_1M);
#endif
	}

	res_irq = platform_get_irq(pdev, 0);
	if (res_irq < 0) {
		enc_pr(LOG_ERROR, "[%s] get irq error!", __func__);
		return -EINVAL;
	}

	encode_manager.irq_num = res_irq;
	if (encode_wq_init()) {
		kfree(encode_manager.reserve_buff);
		encode_manager.reserve_buff = NULL;
		enc_pr(LOG_ERROR, "encode work queue init error.\n");
		return -EFAULT;
	}

	r = init_avc_device();
	enc_pr(LOG_INFO, "amvenc_avc probe end.\n");
	return r;
}

static s32 amvenc_avc_remove(struct platform_device *pdev)
{
	kfree(encode_manager.reserve_buff);
	encode_manager.reserve_buff = NULL;
	if (encode_wq_uninit()) {
		enc_pr(LOG_ERROR, "encode work queue uninit error.\n");
	}
	uninit_avc_device();
	enc_pr(LOG_INFO, "amvenc_avc remove.\n");
	return 0;
}

static const struct of_device_id amlogic_avcenc_dt_match[] = {
	{
		.compatible = "amlogic, amvenc_avc",
	},
	{},
};

static struct platform_driver amvenc_avc_driver = {
	.probe = amvenc_avc_probe,
	.remove = amvenc_avc_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = amlogic_avcenc_dt_match,
	}
};

static struct codec_profile_t amvenc_avc_profile = {
	.name = "avc",
	.profile = ""
};

static s32 __init amvenc_avc_driver_init_module(void)
{
	enc_pr(LOG_INFO, "amvenc_avc module init\n");

	if (platform_driver_register(&amvenc_avc_driver)) {
		enc_pr(LOG_ERROR,
			"failed to register amvenc_avc driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&amvenc_avc_profile);
	return 0;
}

static void __exit amvenc_avc_driver_remove_module(void)
{
	enc_pr(LOG_INFO, "amvenc_avc module remove.\n");

	platform_driver_unregister(&amvenc_avc_driver);
}

static const struct reserved_mem_ops rmem_avc_ops = {
	.device_init = avc_mem_device_init,
};

static s32 __init avc_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_avc_ops;
	enc_pr(LOG_DEBUG, "amvenc_avc reserved mem setup.\n");
	return 0;
}

module_param(fixed_slice_cfg, uint, 0664);
MODULE_PARM_DESC(fixed_slice_cfg, "\n fixed_slice_cfg\n");

module_param(clock_level, uint, 0664);
MODULE_PARM_DESC(clock_level, "\n clock_level\n");

module_param(encode_print_level, uint, 0664);
MODULE_PARM_DESC(encode_print_level, "\n encode_print_level\n");

module_param(no_timeout, uint, 0664);
MODULE_PARM_DESC(no_timeout, "\n no_timeout flag for process request\n");

module_param(nr_mode, int, 0664);
MODULE_PARM_DESC(nr_mode, "\n nr_mode option\n");

module_param(qp_table_debug, uint, 0664);
MODULE_PARM_DESC(qp_table_debug, "\n print qp table\n");

#ifdef MORE_MODULE_PARAM
module_param(me_mv_merge_ctl, uint, 0664);
MODULE_PARM_DESC(me_mv_merge_ctl, "\n me_mv_merge_ctl\n");

module_param(me_step0_close_mv, uint, 0664);
MODULE_PARM_DESC(me_step0_close_mv, "\n me_step0_close_mv\n");

module_param(me_f_skip_sad, uint, 0664);
MODULE_PARM_DESC(me_f_skip_sad, "\n me_f_skip_sad\n");

module_param(me_f_skip_weight, uint, 0664);
MODULE_PARM_DESC(me_f_skip_weight, "\n me_f_skip_weight\n");

module_param(me_mv_weight_01, uint, 0664);
MODULE_PARM_DESC(me_mv_weight_01, "\n me_mv_weight_01\n");

module_param(me_mv_weight_23, uint, 0664);
MODULE_PARM_DESC(me_mv_weight_23, "\n me_mv_weight_23\n");

module_param(me_sad_range_inc, uint, 0664);
MODULE_PARM_DESC(me_sad_range_inc, "\n me_sad_range_inc\n");

module_param(me_sad_enough_01, uint, 0664);
MODULE_PARM_DESC(me_sad_enough_01, "\n me_sad_enough_01\n");

module_param(me_sad_enough_23, uint, 0664);
MODULE_PARM_DESC(me_sad_enough_23, "\n me_sad_enough_23\n");

module_param(y_tnr_mc_en, uint, 0664);
MODULE_PARM_DESC(y_tnr_mc_en, "\n y_tnr_mc_en option\n");
module_param(y_tnr_txt_mode, uint, 0664);
MODULE_PARM_DESC(y_tnr_txt_mode, "\n y_tnr_txt_mode option\n");
module_param(y_tnr_mot_sad_margin, uint, 0664);
MODULE_PARM_DESC(y_tnr_mot_sad_margin, "\n y_tnr_mot_sad_margin option\n");
module_param(y_tnr_mot_cortxt_rate, uint, 0664);
MODULE_PARM_DESC(y_tnr_mot_cortxt_rate, "\n y_tnr_mot_cortxt_rate option\n");
module_param(y_tnr_mot_distxt_ofst, uint, 0664);
MODULE_PARM_DESC(y_tnr_mot_distxt_ofst, "\n y_tnr_mot_distxt_ofst option\n");
module_param(y_tnr_mot_distxt_rate, uint, 0664);
MODULE_PARM_DESC(y_tnr_mot_distxt_rate, "\n y_tnr_mot_distxt_rate option\n");
module_param(y_tnr_mot_dismot_ofst, uint, 0664);
MODULE_PARM_DESC(y_tnr_mot_dismot_ofst, "\n y_tnr_mot_dismot_ofst option\n");
module_param(y_tnr_mot_frcsad_lock, uint, 0664);
MODULE_PARM_DESC(y_tnr_mot_frcsad_lock, "\n y_tnr_mot_frcsad_lock option\n");
module_param(y_tnr_mot2alp_frc_gain, uint, 0664);
MODULE_PARM_DESC(y_tnr_mot2alp_frc_gain, "\n y_tnr_mot2alp_frc_gain option\n");
module_param(y_tnr_mot2alp_nrm_gain, uint, 0664);
MODULE_PARM_DESC(y_tnr_mot2alp_nrm_gain, "\n y_tnr_mot2alp_nrm_gain option\n");
module_param(y_tnr_mot2alp_dis_gain, uint, 0664);
MODULE_PARM_DESC(y_tnr_mot2alp_dis_gain, "\n y_tnr_mot2alp_dis_gain option\n");
module_param(y_tnr_mot2alp_dis_ofst, uint, 0664);
MODULE_PARM_DESC(y_tnr_mot2alp_dis_ofst, "\n y_tnr_mot2alp_dis_ofst option\n");
module_param(y_tnr_alpha_min, uint, 0664);
MODULE_PARM_DESC(y_tnr_alpha_min, "\n y_tnr_alpha_min option\n");
module_param(y_tnr_alpha_max, uint, 0664);
MODULE_PARM_DESC(y_tnr_alpha_max, "\n y_tnr_alpha_max option\n");
module_param(y_tnr_deghost_os, uint, 0664);
MODULE_PARM_DESC(y_tnr_deghost_os, "\n y_tnr_deghost_os option\n");

module_param(c_tnr_mc_en, uint, 0664);
MODULE_PARM_DESC(c_tnr_mc_en, "\n c_tnr_mc_en option\n");
module_param(c_tnr_txt_mode, uint, 0664);
MODULE_PARM_DESC(c_tnr_txt_mode, "\n c_tnr_txt_mode option\n");
module_param(c_tnr_mot_sad_margin, uint, 0664);
MODULE_PARM_DESC(c_tnr_mot_sad_margin, "\n c_tnr_mot_sad_margin option\n");
module_param(c_tnr_mot_cortxt_rate, uint, 0664);
MODULE_PARM_DESC(c_tnr_mot_cortxt_rate, "\n c_tnr_mot_cortxt_rate option\n");
module_param(c_tnr_mot_distxt_ofst, uint, 0664);
MODULE_PARM_DESC(c_tnr_mot_distxt_ofst, "\n c_tnr_mot_distxt_ofst option\n");
module_param(c_tnr_mot_distxt_rate, uint, 0664);
MODULE_PARM_DESC(c_tnr_mot_distxt_rate, "\n c_tnr_mot_distxt_rate option\n");
module_param(c_tnr_mot_dismot_ofst, uint, 0664);
MODULE_PARM_DESC(c_tnr_mot_dismot_ofst, "\n c_tnr_mot_dismot_ofst option\n");
module_param(c_tnr_mot_frcsad_lock, uint, 0664);
MODULE_PARM_DESC(c_tnr_mot_frcsad_lock, "\n c_tnr_mot_frcsad_lock option\n");
module_param(c_tnr_mot2alp_frc_gain, uint, 0664);
MODULE_PARM_DESC(c_tnr_mot2alp_frc_gain, "\n c_tnr_mot2alp_frc_gain option\n");
module_param(c_tnr_mot2alp_nrm_gain, uint, 0664);
MODULE_PARM_DESC(c_tnr_mot2alp_nrm_gain, "\n c_tnr_mot2alp_nrm_gain option\n");
module_param(c_tnr_mot2alp_dis_gain, uint, 0664);
MODULE_PARM_DESC(c_tnr_mot2alp_dis_gain, "\n c_tnr_mot2alp_dis_gain option\n");
module_param(c_tnr_mot2alp_dis_ofst, uint, 0664);
MODULE_PARM_DESC(c_tnr_mot2alp_dis_ofst, "\n c_tnr_mot2alp_dis_ofst option\n");
module_param(c_tnr_alpha_min, uint, 0664);
MODULE_PARM_DESC(c_tnr_alpha_min, "\n c_tnr_alpha_min option\n");
module_param(c_tnr_alpha_max, uint, 0664);
MODULE_PARM_DESC(c_tnr_alpha_max, "\n c_tnr_alpha_max option\n");
module_param(c_tnr_deghost_os, uint, 0664);
MODULE_PARM_DESC(c_tnr_deghost_os, "\n c_tnr_deghost_os option\n");

module_param(y_snr_err_norm, uint, 0664);
MODULE_PARM_DESC(y_snr_err_norm, "\n y_snr_err_norm option\n");
module_param(y_snr_gau_bld_core, uint, 0664);
MODULE_PARM_DESC(y_snr_gau_bld_core, "\n y_snr_gau_bld_core option\n");
module_param(y_snr_gau_bld_ofst, int, 0664);
MODULE_PARM_DESC(y_snr_gau_bld_ofst, "\n y_snr_gau_bld_ofst option\n");
module_param(y_snr_gau_bld_rate, uint, 0664);
MODULE_PARM_DESC(y_snr_gau_bld_rate, "\n y_snr_gau_bld_rate option\n");
module_param(y_snr_gau_alp0_min, uint, 0664);
MODULE_PARM_DESC(y_snr_gau_alp0_min, "\n y_snr_gau_alp0_min option\n");
module_param(y_snr_gau_alp0_max, uint, 0664);
MODULE_PARM_DESC(y_snr_gau_alp0_max, "\n y_snr_gau_alp0_max option\n");
module_param(y_bld_beta2alp_rate, uint, 0664);
MODULE_PARM_DESC(y_bld_beta2alp_rate, "\n y_bld_beta2alp_rate option\n");
module_param(y_bld_beta_min, uint, 0664);
MODULE_PARM_DESC(y_bld_beta_min, "\n y_bld_beta_min option\n");
module_param(y_bld_beta_max, uint, 0664);
MODULE_PARM_DESC(y_bld_beta_max, "\n y_bld_beta_max option\n");

module_param(c_snr_err_norm, uint, 0664);
MODULE_PARM_DESC(c_snr_err_norm, "\n c_snr_err_norm option\n");
module_param(c_snr_gau_bld_core, uint, 0664);
MODULE_PARM_DESC(c_snr_gau_bld_core, "\n c_snr_gau_bld_core option\n");
module_param(c_snr_gau_bld_ofst, int, 0664);
MODULE_PARM_DESC(c_snr_gau_bld_ofst, "\n c_snr_gau_bld_ofst option\n");
module_param(c_snr_gau_bld_rate, uint, 0664);
MODULE_PARM_DESC(c_snr_gau_bld_rate, "\n c_snr_gau_bld_rate option\n");
module_param(c_snr_gau_alp0_min, uint, 0664);
MODULE_PARM_DESC(c_snr_gau_alp0_min, "\n c_snr_gau_alp0_min option\n");
module_param(c_snr_gau_alp0_max, uint, 0664);
MODULE_PARM_DESC(c_snr_gau_alp0_max, "\n c_snr_gau_alp0_max option\n");
module_param(c_bld_beta2alp_rate, uint, 0664);
MODULE_PARM_DESC(c_bld_beta2alp_rate, "\n c_bld_beta2alp_rate option\n");
module_param(c_bld_beta_min, uint, 0664);
MODULE_PARM_DESC(c_bld_beta_min, "\n c_bld_beta_min option\n");
module_param(c_bld_beta_max, uint, 0664);
MODULE_PARM_DESC(c_bld_beta_max, "\n c_bld_beta_max option\n");
#endif

module_init(amvenc_avc_driver_init_module);
module_exit(amvenc_avc_driver_remove_module);
RESERVEDMEM_OF_DECLARE(amvenc_avc, "amlogic, amvenc-memory", avc_mem_setup);

MODULE_DESCRIPTION("AMLOGIC AVC Video Encoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("simon.zheng <simon.zheng@amlogic.com>");
