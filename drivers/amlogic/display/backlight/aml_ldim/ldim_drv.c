/*
 * drivers/amlogic/display/backlight/aml_ldim/ldim_drv.c
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

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/amports/vframe.h>
#include <linux/spinlock.h>
#include <linux/amlogic/iomap.h>
#include "ldim_drv.h"
#include "ldim_func.h"
#include "ldim_reg.h"
#include <linux/workqueue.h>
#include <linux/amlogic/vout/aml_ldim.h>
#include <linux/amlogic/vout/aml_bl.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include "../../../amports/vdec_reg.h"

#define AML_LDIM_DEV_NAME            "aml_ldim"
const char ldim_dev_id[] = "ldim-dev";
#define VIU_VSYNC_INT    35
#define RDMA_LDIM_INT    175

static int ldim_on_flag;
static unsigned int ldim_func_en;
static unsigned int ldim_func_bypass;

static unsigned int ldim_test_en;
module_param(ldim_test_en, uint, 0664);
MODULE_PARM_DESC(ldim_test_en, "ldim_test_en_flag");
static unsigned int ldim_bypass;
module_param(ldim_bypass, uint, 0664);
MODULE_PARM_DESC(ldim_bypass, "ldim_bypass");
static unsigned int ldim_level_update;
module_param(ldim_level_update, uint, 0664);
MODULE_PARM_DESC(ldim_level_update, "ldim_level_update");

struct LDReg nPRM;
struct FW_DAT FDat;

unsigned int hist_matrix[LD_BLKREGNUM*16] = {0};
unsigned int max_rgb[LD_BLKREGNUM] = {0};
unsigned int global_ldim_max[LD_BLKREGNUM] = {0};

static unsigned long val_1[16] = {512, 512, 512, 512, 546, 546,
	585, 630, 745, 819, 910, 1170, 512, 512, 512, 512};
static unsigned long bin_1[16] = {4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4};/* 0~32 */
static unsigned long val_2[16] = {3712, 3712, 3712, 3712, 3823, 3823,
	3647, 3455, 3135, 3007, 2879, 2623, 3750, 3800, 4000, 4055};/*0~4095*/
static unsigned long bin_2[16] = {29, 29, 29, 29, 29, 29,
	25, 22, 17, 15, 13, 28, 28, 27, 26, 25};

unsigned int invalid_val_cnt = 0;
module_param(invalid_val_cnt, uint, 0664);
MODULE_PARM_DESC(invalid_val_cnt, "invalid_val_cnt");

static unsigned int ldim_irq;
static spinlock_t  ldim_isr_lock;
static spinlock_t  rdma_ldim_isr_lock;

static struct tasklet_struct   ldim_tasklet;
static struct workqueue_struct *ldim_read_queue;
static struct work_struct   ldim_read_work;

#if 1
#define FRM_NUM_DBG 5
static unsigned long fw_LD_ThSF_l = 1600;
static unsigned long fw_LD_ThTF_l = 256;

static unsigned long cal_cur_en;  /* [1~128~256] */

static unsigned long avg_gain_sf_l = 128;  /* [1~128~256] */
unsigned long dif_gain_sf_l = 0;  /* [0~128] */

unsigned long Debug = 0;
static unsigned long LPF = 1;  /* [0~128] */
/* static unsigned long slp_gain = 28;*/
static unsigned long  transmit_gain = 1;
static unsigned long  lpf_gain = 128;  /* [0~128~256], norm 128 as 1*/
static unsigned long  lpf_res = 41;    /* 1024/9 = 113*/
unsigned long  rgb_base = 127;
unsigned long boost_gain = 380; /*256;*/
unsigned long Dbprint_lv = 0;
/* static unsigned long bl_remap_curve[16] = {272, 317, 392, 497, 632,
	797, 991, 1216, 1471, 1756, 2071, 2416, 2791, 3195, 3630, 4095};*/
static unsigned int bl_remap_curve[16] = {
				436, 479, 551, 651, 780, 938, 1125, 1340,
				1584, 1856, 2158, 2488, 2847, 3234, 3650, 4095
				};/*BL_matrix remap curve*/
static unsigned long Sf_bypass, Boost_light_bypass;
static unsigned long Lpf_bypass, Ld_remap_bypass, Tf_bypass;
static unsigned int  Tf_luma_avg_frm[FRM_NUM_DBG] = {0, 0, 0, 0, 0};
static unsigned int  Tf_blkLuma_avg[FRM_NUM_DBG][8] = {
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0} };
static unsigned int  Tf_bl_matrix[FRM_NUM_DBG][8] = {
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0} };
static unsigned int  Map_bl_matrix[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static unsigned int  Tf_bl_matrix_map[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static unsigned int  Tf_diff_frm_luma[FRM_NUM_DBG] = {0, 0, 0, 0, 0};
static unsigned int  Tf_diff_blk_luma[FRM_NUM_DBG][8] = {
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0},
				{0, 0, 0, 0, 0, 0, 0, 0} };
static unsigned int  Map_bl_matrix_Compensate;
static unsigned int  Map_bl_matrix_AVG;
unsigned int  db_cnt = 0;
#endif

unsigned long ldim_frm_time = 0;
unsigned long ldim_stts_start_time = 0;
unsigned long ldim_stts_end_time = 0;
long ldim_stts_time = 0;

unsigned long ld_on_vs_start_time = 0;
unsigned long ld_on_vs_end_time = 0;
long ld_on_vs_time = 0;

unsigned long ld_fw_alg_frm_start_time = 0;
unsigned long ld_fw_alg_frm_end_time = 0;
long ld_fw_alg_frm_time = 0;

#define LD_DATA_MIN    10
static unsigned int ldim_data_min;
static unsigned int ldim_brightness_level;
unsigned long litgain = LD_DATA_DEPTH; /* 0xfff */
unsigned long avg_gain = LD_DATA_DEPTH; /* 0xfff */
/*unsigned long Backlit_coeff_l = 4096;*/

#ifndef MAX
#define MAX(a, b)   ((a > b) ? a:b)
#endif
#ifndef MIN
#define MIN(a, b)   ((a < b) ? a:b)
#endif

#ifndef ABS
#define ABS(a)   ((a < 0) ? (-a):a)
#endif

unsigned int ldim_debug_print;
module_param(ldim_debug_print, uint, 0664);
MODULE_PARM_DESC(ldim_debug_print, "ldim_debug_print");

static unsigned int ldim_irq_cnt;
module_param(ldim_irq_cnt, uint, 0664);
MODULE_PARM_DESC(ldim_irq_cnt, "ldim_irq_cnt");

static unsigned int rdma_ldim_irq_cnt;
module_param(rdma_ldim_irq_cnt, uint, 0664);
MODULE_PARM_DESC(rdma_ldim_irq_cnt, "rdma_ldim_irq_cnt");

static unsigned int ldim_hist_en;
module_param(ldim_hist_en, uint, 0664);
MODULE_PARM_DESC(ldim_hist_en, "ldim_hist_en");

static unsigned int ldim_hist_row = 1;
module_param(ldim_hist_row, uint, 0664);
MODULE_PARM_DESC(ldim_hist_row, "ldim_hist_row");

static unsigned int ldim_hist_col = 8;
module_param(ldim_hist_col, uint, 0664);
MODULE_PARM_DESC(ldim_hist_col, "ldim_hist_col");

static unsigned int ldim_blk_row = 1;
module_param(ldim_blk_row, uint, 0664);
MODULE_PARM_DESC(ldim_blk_row, "ldim_blk_row");

static unsigned int ldim_blk_col = 8;
module_param(ldim_blk_col, uint, 0664);
MODULE_PARM_DESC(ldim_blk_col, "ldim_blk_col");

static unsigned int ldim_avg_update_en;
module_param(ldim_avg_update_en, uint, 0664);
MODULE_PARM_DESC(ldim_avg_update_en, "ldim_avg_update_en");

static unsigned int ldim_matrix_update_en;
module_param(ldim_matrix_update_en, uint, 0664);
MODULE_PARM_DESC(ldim_matrix_update_en, "ldim_matrix_update_en");

static unsigned int ldim_alg_en;
module_param(ldim_alg_en, uint, 0664);
MODULE_PARM_DESC(ldim_alg_en, "ldim_alg_en");

static unsigned int ldim_top_en;
module_param(ldim_top_en, uint, 0664);
MODULE_PARM_DESC(ldim_top_en, "ldim_top_en");

static unsigned int slp_gain = 28;
module_param(slp_gain, uint, 0664);
MODULE_PARM_DESC(slp_gain, "slp_gain");

static unsigned int incr_con_en;
module_param(incr_con_en, uint, 0664);
MODULE_PARM_DESC(incr_con_en, "incr_con_en");

static unsigned int ov_gain = 16;
module_param(ov_gain, uint, 0664);
MODULE_PARM_DESC(ov_gain, "ov_gain");

static unsigned int incr_dif_gain = 16;
module_param(incr_dif_gain, uint, 0664);
MODULE_PARM_DESC(incr_dif_gain, "incr_dif_gain");


static unsigned int logo_en;
module_param(logo_en, uint, 0664);
MODULE_PARM_DESC(logo_en, "logo_en");

static unsigned int test_static_pic;
module_param(test_static_pic, uint, 0664);
MODULE_PARM_DESC(test_static_pic, "test_static_pic");


static unsigned long  vs_time_record;

static struct aml_ldim_driver_s ldim_driver;
static void ldim_on_vs_arithmetic(void);
static void ldim_update_setting(void);
static void ldim_get_matrix_info_6(void);
static void ldim_print_debug(void);
static void ldim_bl_remap_curve(int slop_gain);
static void ldim_bl_remap_curve_print(void);
static struct ldim_config_s ldim_config = {
	.hsize = 3840,
	.vsize = 2160,
	.bl_mode = 1,
};

static void ldim_stts_read_region(struct work_struct *work)
{
	if (test_static_pic == 0)
		ldim_read_region(ldim_hist_row, ldim_hist_col);
	ldim_on_vs_arithmetic();
	return;
}

void LDIM_WR_32Bits(unsigned int addr, unsigned int data)
{
	Wr(LDIM_BL_ADDR_PORT, addr);
	Wr(LDIM_BL_DATA_PORT, data);
}

unsigned int LDIM_RD_32Bits(unsigned int addr)
{
	Wr(LDIM_BL_ADDR_PORT, addr);
	return Rd(LDIM_BL_DATA_PORT);
}

void LDIM_wr_reg_bits(unsigned int addr, unsigned int val,
				unsigned int start, unsigned int len)
{
	unsigned int data;
	data = LDIM_RD_32Bits(addr);
	data = (data & (~((1 << len) - 1)<<start))  |
		((val & ((1 << len) - 1)) << start);
	LDIM_WR_32Bits(addr, data);
}

void LDIM_WR_BASE_LUT(unsigned int base, unsigned int *pData,
				unsigned int size_t, unsigned int len)
{
	unsigned int i;
	unsigned int addr, data;
	unsigned int mask, subCnt;
	unsigned int cnt;

	addr   = base;/* (base<<4); */
	mask   = (1<<size_t)-1;
	subCnt = 32/size_t;
	cnt  = 0;
	data = 0;

	Wr(LDIM_BL_ADDR_PORT, addr);

	for (i = 0; i < len; i++) {
		/* data = (data<<size_t)|(pData[i]&mask); */
		data = (data)|((pData[i]&mask)<<(size_t *cnt));
		cnt++;
		if (cnt == subCnt) {
			Wr(LDIM_BL_DATA_PORT, data);
			data = 0;
			cnt = 0;
			addr++;
		}
	}
	if (cnt != 0)
		Wr(LDIM_BL_DATA_PORT, data);
}
void LDIM_RD_BASE_LUT(unsigned int base, unsigned int *pData,
				unsigned int size_t, unsigned int len)
{
	unsigned int i;
	unsigned int addr, data;
	unsigned int mask, subCnt;
	unsigned int cnt;

	addr   = base;/* (base<<4); */
	mask   = (1<<size_t)-1;
	subCnt = 32/size_t;
	cnt  = 0;
	data = 0;

	Wr(LDIM_BL_ADDR_PORT, addr);

	for (i = 0; i < len; i++) {
		/* data = (data<<size_t)|(pData[i]&mask); */
		/* data = (data)|((pData[i]&mask)<<(size_t*cnt)); */
		cnt++;
		if (cnt == subCnt) {
			data = Rd(LDIM_BL_DATA_PORT);
			pData[i-1] = data&mask;
			pData[i] = (data>>size_t)&mask;
			data = 0;
			cnt = 0;
			addr++;
		}
	}
	if (cnt != 0)
		data = Rd(LDIM_BL_DATA_PORT);
}
void LDIM_RD_BASE_LUT_2(unsigned int base, unsigned int *pData,
				unsigned int size_t, unsigned int len)
{
	unsigned int i;
	unsigned int addr, data;
	unsigned int mask, subCnt;
	unsigned int cnt;

	addr   = base;/* (base<<4); */
	mask   = (1<<size_t)-1;
	subCnt = 2;
	cnt  = 0;
	data = 0;

	Wr(LDIM_BL_ADDR_PORT, addr);

	for (i = 0; i < len; i++) {
		cnt++;
		if (cnt == subCnt) {
			data = Rd(LDIM_BL_DATA_PORT);
			pData[i-1] = data&mask;
			pData[i] = (data>>size_t)&mask;
			data = 0;
			cnt = 0;
			addr++;
		}
	}
	if (cnt != 0)
		data = Rd(LDIM_BL_DATA_PORT);
}


static void ldim_bl_remap_curve(int slop_gain)
{
	int i = 0 , ii = 0;
	int t_bl_remap_curve[16] = {0};
	int bl_max = 0, mid_value = 0;
	LDIMPR("%s:\n", __func__);
	for (i = 0; i < 16; i++) {
		ii = (i+1) * 256;
		mid_value = ii*ii>>17;
		t_bl_remap_curve[i] = mid_value*slop_gain + 512;
	}
	bl_max = t_bl_remap_curve[15];

	for (i = 0; i < 16; i++)
		bl_remap_curve[i] = t_bl_remap_curve[i]*4095/bl_max;
}

void ldim_bl_remap_curve_print(void)
{
	int i = 0;
	LDIMPR("%s:\n", __func__);
	pr_info("bl_remap_curve:");
	for (i = 0; i < 16; i++)
		pr_info("[%4d]\n", bl_remap_curve[i]);
}


static void ld_fw_alg_frm(struct LDReg *nPRM, struct FW_DAT *FDat,
	unsigned int *max_matrix, unsigned int *hist_matrix)
{
	/* Notes, nPRM will be set here in SW algorithm too */
	int dif, blkRow, blkCol, k, m, n, i;
	unsigned long sum;
	unsigned int avg, adpt_alp, dif_RGB, alpha, Bmin, Bmax;
	unsigned int bl_value, bl_valuex128;
	unsigned int Vnum  = (nPRM->reg_LD_BLK_Vnum);
	unsigned int Hnum  = (nPRM->reg_LD_BLK_Hnum);
	/* unsigned int Bsize = Vnum*Hnum; */
	unsigned int *tBL_matrix;
	unsigned int BLmax = 4096;   /* maximum BL value */
	unsigned int a = 0;
	/*int stride = (nPRM->reg_LD_STA_Hnum);*/
	int stride = ldim_hist_col;
	int RGBmax, Histmx, maxNB, curNB = 0;
	unsigned int fw_blk_num  = Vnum*Hnum;
	unsigned int fw_LD_Thist = ((fw_blk_num*5)>>2);
	unsigned int fw_LD_Whist[16] = {32, 64, 96, 128, 160, 192, 224, 256,
				288, 320, 352, 384, 416, 448, 480, 512};
	unsigned int fw_pic_size =
			(nPRM->reg_LD_pic_RowMax)*(nPRM->reg_LD_pic_ColMax);
	int fw_LD_ThSF = 1600;
	unsigned int fw_LD_ThTF = 32;

	unsigned int fw_LD_BLEst_ACmode = 1;
	int num = 0, mm = 0, nn = 0;
	/* u2: 0: est on BLmatrix; 1: est on (BL-DC);
		2: est on (BL-MIN); 3: est on (BL-MAX) */
	unsigned int fw_hist_mx;
	unsigned int SF_sum = 0, TF_sum = 0, dif_sum = 0;
    /* alocate the memory for the matrix */
    /* tBL_matrix = (unsigned int *)
    kmalloc(Bsize*sizeof(unsigned int),GFP_KERNEL); */
	int int_x, rmd_x, norm;
	int left,  right, bl_value_map;
	int Luma_avg = 0, blkLuma_avg = 0, lmh_avg = 0, blk_sum = 0;
	int diff_frm_luma = 0, diff_blk_luma = 0;
	int diff_blk_matrix = 0, remap_value = 0;
	int dif_r = 0, dif_g = 0, dif_b = 0;
	int tvalue = 0, min_diff = 0;
	int Bmax_lpf = 0;
	int frm_rgbmax = 0, black_blk_num = 0;
	#if 1
	int SF_avg = 0;
	unsigned int SF_dif = 0;

	fw_LD_ThSF = fw_LD_ThSF_l;
	fw_LD_ThTF = fw_LD_ThTF_l;
#endif
	diff_frm_luma = 0;
	diff_blk_luma = 0;
	diff_blk_matrix = 0;
	remap_value = 0;

	tBL_matrix = FDat->TF_BL_matrix_2;
	/* Luma_avg transmit */
	for (i = 0; i < (FRM_NUM_DBG - 1); i++) {
		for (blkRow = 0; blkRow < Vnum; blkRow++) {
			for (blkCol = 0; blkCol < Hnum; blkCol++) {
				Tf_blkLuma_avg[i][blkRow*Hnum + blkCol]   =
				Tf_blkLuma_avg[i+1][blkRow*Hnum + blkCol];
				Tf_bl_matrix[i][blkRow*Hnum + blkCol]     =
				Tf_bl_matrix[i+1][blkRow*Hnum + blkCol];
				Tf_diff_blk_luma[i][blkRow*Hnum + blkCol] =
				Tf_diff_blk_luma[i+1][blkRow*Hnum + blkCol];
			}
		}
		Tf_luma_avg_frm[i] = Tf_luma_avg_frm[i+1];
		Tf_diff_frm_luma[i] = Tf_diff_frm_luma[i+1];
	}

	frm_rgbmax = 0; black_blk_num = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			RGBmax =
			MAX(MAX(max_matrix[blkRow*3*stride + blkCol*3 + 0],
				max_matrix[blkRow*3*stride + blkCol*3 + 1]),
				max_matrix[blkRow*3*stride + blkCol*3 + 2]);
			if ((RGBmax == 0) &&
			(hist_matrix[blkRow*LD_STA_BIN_NUM*stride +
			blkCol*LD_STA_BIN_NUM + 0] >= 1030800))
				black_blk_num++;
			frm_rgbmax = MAX(frm_rgbmax, RGBmax);
		}
	}

	/* calculate the current frame */
	sum = 0; Luma_avg = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			RGBmax =
			MAX(MAX(max_matrix[blkRow*3*stride + blkCol*3 + 0],
				max_matrix[blkRow*3*stride + blkCol*3 + 1]),
				max_matrix[blkRow*3*stride + blkCol*3 + 2]);
			if (frm_rgbmax != 0) {
				if (RGBmax < rgb_base)
					RGBmax = rgb_base;
			}
			/* Consider the sitrogram */
			Histmx = 0;
			blkLuma_avg = 0;
			blk_sum = 0;
			for (k = 0; k < 16; k++) {
				Histmx +=
				(hist_matrix[blkRow*LD_STA_BIN_NUM*stride
				+ blkCol*LD_STA_BIN_NUM + k] * fw_LD_Whist[k]);
				blk_sum +=
				hist_matrix[blkRow*LD_STA_BIN_NUM*stride
				+ blkCol*LD_STA_BIN_NUM + k];
				blkLuma_avg +=
				hist_matrix[blkRow*LD_STA_BIN_NUM*stride
				+ blkCol*LD_STA_BIN_NUM + k] * k * 32;
				/*16-->512grads*/
				a = 0;
			}
			/* sum += blk_sum;
			Luma_avg += blkLuma_avg; */
			Tf_blkLuma_avg[FRM_NUM_DBG - 1][blkRow*Hnum + blkCol] =
			((blkLuma_avg + (blk_sum >> 1)) / (blk_sum + 1));
			Luma_avg +=
			Tf_blkLuma_avg[FRM_NUM_DBG - 1][blkRow*Hnum + blkCol];
			fw_hist_mx =
				((Histmx>>8)*fw_LD_Thist*2/(fw_pic_size>>8));
			/* further debug */
			tBL_matrix[blkRow*Hnum + blkCol] =
				((BLmax*MIN(fw_hist_mx, RGBmax))>>10);

			a = 0;
		}
	}

	/*To solve black/white woman flicker*/
	lmh_avg = (Luma_avg  / (Vnum * Hnum));
	Tf_luma_avg_frm[FRM_NUM_DBG - 1] = lmh_avg;

	/* Spatial Filter the BackLits */
	sum = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			maxNB = 0;
			for (m =  -1; m < 2; m++) {
				for (n =  -1; n < 2; n++) {
					if ((m == 0) && (n == 0)) {
						curNB =
					tBL_matrix[blkRow*Hnum + blkCol];
					} else if (((blkRow+m) >= 0) &&
						((blkRow+m) < Vnum) &&
						((blkCol + n) >= 0) &&
						((blkCol+n) < Hnum)) {
						maxNB =
					MAX(maxNB, tBL_matrix[(blkRow+m)*Hnum
					+ blkCol + n]);
					}
				}
			}
			/* SF matrix */
			if (Sf_bypass == 1) {
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol] =
				tBL_matrix[blkRow*Hnum + blkCol];
			} else {
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol] =
				MAX(curNB, (maxNB-fw_LD_ThSF));
			}

			sum += FDat->SF_BL_matrix[blkRow*Hnum + blkCol];
			/* for SF_BL_matrix average calculation */
		}
	}

	/* boost the bright region lights a little bit. */
	avg = ((sum*7/fw_blk_num)>>3);
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			dif = (FDat->SF_BL_matrix[blkRow*Hnum + blkCol] - avg);

			#if 1
			FDat->SF_BL_matrix[blkRow*Hnum + blkCol] += (0*dif);
			if (Boost_light_bypass == 1) {
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol]  =
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol];
			} else {
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol]  =
	(FDat->SF_BL_matrix[blkRow*Hnum + blkCol] * boost_gain + 64)>>7;
			}

			#endif

			if (FDat->SF_BL_matrix[blkRow*Hnum + blkCol] > 4095)
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol] =
				4095;
		}
	}

#if 1
	SF_sum = 0;
	TF_sum = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			SF_sum += FDat->SF_BL_matrix[blkRow*Hnum + blkCol];
			TF_sum += FDat->TF_BL_matrix[blkRow*Hnum + blkCol];
		}
	}
#endif

	/*20160408 optimize */
#if 1
	SF_avg = SF_sum/fw_blk_num;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			if (Debug == 1) {
				SF_dif = FDat->SF_BL_matrix[blkRow*Hnum
					+ blkCol] - SF_avg;
				if (FDat->SF_BL_matrix[blkRow*Hnum
					+ blkCol] <= SF_avg) {
					FDat->SF_BL_matrix[blkRow*Hnum + blkCol]
						= ((SF_avg  + 64)
						>> 7);
					dif_gain_sf_l = 0;
					avg_gain_sf_l = 0;
				} else {
					FDat->SF_BL_matrix[blkRow*Hnum
						+ blkCol] = ((SF_avg
						 + 64) >> 7)
						+ SF_dif; /* need optimize */
				}
				if (FDat->SF_BL_matrix[blkRow*Hnum
					+ blkCol] > 4095)
					FDat->SF_BL_matrix[blkRow*Hnum
					+ blkCol] = 4095;
			}
		}
	}
#endif

	/* LPF  Only for Xiaomi 8 Led ,here Vnum = 1;*/
	Bmin = 4096; Bmax_lpf = 0;
	if (LPF == 1) {
		for (blkRow = 0; blkRow < Vnum; blkRow++) {
			for (blkCol = 0; blkCol < Hnum; blkCol++) {
				sum = 0;
				for (m = -2; m < 3; m++) {
					for (n = -2; n < 3; n++) {
						/* be careful here */
						mm = MIN(MAX(blkRow+m, 0),
							Vnum-1);
						nn = MIN(MAX(blkCol+n, 0),
							Hnum-1);
						num = MIN(MAX((mm*Hnum + nn),
							0), (Vnum * Hnum - 1));
						sum += FDat->SF_BL_matrix[num];
					}
				}
				if (Lpf_bypass == 1) {
					FDat->SF_BL_matrix[blkRow*Hnum +
					blkCol] =
	FDat->SF_BL_matrix[blkRow*Hnum + blkCol]; /*1024/9 = 113*/
				} else {
					FDat->SF_BL_matrix[blkRow*Hnum +
					blkCol] =
		(((sum * lpf_res >> 10) * lpf_gain) >> 7); /*1024/9 = 113*/
				}

				tvalue =
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol];
				tvalue = (tvalue * ov_gain + 16) >> 4;
				/*ov_gain[16, 32, 64, 128, 256]norm 16 as "1"*/
				min_diff =
			tvalue - FDat->SF_BL_matrix[blkRow*Hnum + blkCol];
				Bmin = MIN(Bmin, min_diff);

				Bmax_lpf =
				MAX(Bmax_lpf,
			Tf_bl_matrix[FRM_NUM_DBG-2][blkRow*Hnum + blkCol]);

				if (FDat->SF_BL_matrix[blkRow*Hnum + blkCol] >
									4095) {
					FDat->SF_BL_matrix[blkRow*Hnum +
							blkCol] = 4095;
				}
			}
		}
	}


	if (incr_con_en == 1) {
		for (blkRow = 0; blkRow < Vnum; blkRow++) {
			for (blkCol = 0; blkCol < Hnum; blkCol++) {
				tvalue =
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol];
				tvalue = (tvalue * ov_gain + 16) >> 4;
				/*ov_gain[16, 32, 64, 128, 256]norm 16 as "1"*/
				tvalue =
				tvalue - ((Bmin * incr_dif_gain + 8) >> 4);
				/* incr_dif_gain [1~16]*/

				FDat->SF_BL_matrix[blkRow*Hnum +
					blkCol] = tvalue;

				if (FDat->SF_BL_matrix[blkRow*Hnum +
							blkCol] > 4095)
					FDat->SF_BL_matrix[blkRow*Hnum +
							blkCol] = 4095;

			}
		}
	}



	if (logo_en == 1) {
		/*
		for (blkRow = 0; blkRow < Vnum; blkRow++) {
			for (blkCol = 0; blkCol < Hnum; blkCol++) {
				if (Dbprint_lv == 1) {
					if ((db_cnt%4) == 0) {
						pr_info(
				"0_FDat->SF_BL_matrix[%4d] = [%4d]\n",
				(blkRow*Hnum + blkCol),
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol]);
					}
				}

				maxNB = 0;
				for (m =  -1; m < 2; m++) {
					for (n =  -1; n < 2; n++) {
						if ((m == 0) && (n == 0)) {
							curNB =
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol];
						} else if (((blkRow+m) >= 0)
						&& ((blkRow+m) < Vnum)
						&& ((blkCol+n) >= 0)
						&& ((blkCol+n) < Hnum)) {
							maxNB =
		MAX(maxNB, FDat->SF_BL_matrix[(blkRow+m)*Hnum + blkCol+n]);
						}
					}
				}
				dif = maxNB - curNB;
				tvalue = ((curNB + 1) > 1);
				if (dif > 500)
					FDat->SF_BL_matrix[blkRow*Hnum +
					blkCol] = ((maxNB * 3 + 2) >> 2);

				if (Dbprint_lv == 1) {
					if ((db_cnt%4) == 0) {
						pr_info(
				"1_FDat->SF_BL_matrix[%4d] = [%4d]\n",
				(blkRow*Hnum + blkCol),
				FDat->SF_BL_matrix[blkRow*Hnum + blkCol]);
					}
				}
			}
		}
		*/
	}

	/* Temperary filter */
	sum = 0; Bmin = 4096; Bmax = 0;
	for (blkRow = 0; blkRow < Vnum; blkRow++) {
		for (blkCol = 0; blkCol < Hnum; blkCol++) {
			/* Optimization needed here */
			/* dif_RGB =
MAX(MAX(ABS(FDat->last_STA1_MaxRGB[blkRow*3*stride + blkCol*3 + 0] -
max_matrix[blkRow*3*stride + blkCol*3 + 0]),
ABS(FDat->last_STA1_MaxRGB[blkRow*3*stride + blkCol*3 + 1] -
max_matrix[blkRow*3*stride + blkCol*3 + 1])),
ABS(FDat->last_STA1_MaxRGB[blkRow*3*stride + blkCol*3 + 2] -
max_matrix[blkRow*3*stride + blkCol*3 + 2]));*/
			dif_r =
			FDat->last_STA1_MaxRGB[blkRow*3*stride + blkCol*3 + 0] -
				max_matrix[blkRow*3*stride + blkCol*3 + 0];
			dif_r = ABS(dif_r);
			dif_g =
			FDat->last_STA1_MaxRGB[blkRow*3*stride + blkCol*3 + 1] -
				max_matrix[blkRow*3*stride + blkCol*3 + 1];
			dif_g = ABS(dif_g);
			dif_b =
			FDat->last_STA1_MaxRGB[blkRow*3*stride + blkCol*3 + 2] -
				max_matrix[blkRow*3*stride + blkCol*3 + 2];
			dif_b = ABS(dif_b);
			dif_RGB = MAX(MAX(dif_r, dif_g), dif_b);
			adpt_alp =
			(FDat->SF_BL_matrix[blkRow*Hnum + blkCol]) -
				(FDat->TF_BL_matrix[blkRow*Hnum + blkCol]);
			adpt_alp = ABS(adpt_alp);

			#if 0
			alpha = MIN(256, fw_LD_ThTF + (MAX(adpt_alp, dif_RGB)));
			SF_sum = 0;
			TF_sum = 0;
			dif_sum = 0;
			bl_valuex128 = 0;
			#endif

			#if 1
			dif_sum = SF_sum - TF_sum;
			dif_sum = ABS(dif_sum);
			if (dif_sum > 32760)
				alpha = 256;
			else
				alpha = MIN(256, fw_LD_ThTF);
			bl_valuex128 = 0;
			#endif

			FDat->TF_BL_alpha[blkRow*Hnum + blkCol] = alpha;
			/* 256 normalized as "1" */

			/* get the temporary filtered BL_value */
			 bl_value =
			(((256-alpha) *
			(FDat->TF_BL_matrix[blkRow*Hnum + blkCol]) +
			alpha *
			(FDat->SF_BL_matrix[blkRow*Hnum + blkCol]) + 128) >> 8);
			/* bl_valuex128 =
			(FDat->TF_BL_matrix[blkRow*Hnum +blkCol]) +
			(alpha*
			((FDat->SF_BL_matrix[blkRow*Hnum + blkCol]<<7) -
			FDat->TF_BL_matrix[blkRow*Hnum + blkCol] + 128)>>8); */
			/* 7 more bits precison kept */
			/* bl_value = (bl_valuex128 + 64)>>7;*//* u12 */
			if (bl_value > 4095)
				bl_value = 4095;

			/*ld_curve map*/
			int_x = bl_value>>8;
			rmd_x = bl_value%256;
			norm  = 256;
			if (int_x == 0)
				left = 0;
			else
				left = bl_remap_curve[int_x - 1];

			right = bl_remap_curve[int_x];

			if (Ld_remap_bypass == 1) {
				bl_value_map = bl_value;
			} else {
				bl_value_map = left +
				(((right-left)*rmd_x + 128)>>8);
			}
			bl_value_map = (bl_value_map > 4095) ? 4095 :
				bl_value_map;
				/*clip to u12: debug 20150728  output u12*/

			Tf_bl_matrix_map[blkRow*Hnum + blkCol] = bl_value_map;

			diff_frm_luma = Tf_luma_avg_frm[FRM_NUM_DBG - 1] -
					Tf_luma_avg_frm[FRM_NUM_DBG - 2];
			diff_frm_luma = ABS(diff_frm_luma);
			Tf_diff_frm_luma[FRM_NUM_DBG - 1] = diff_frm_luma;


			 /* if (Tf_luma_avg_frm[FRM_NUM_DBG - 1] == 0)
				bl_value_map =
				Tf_bl_matrix[FRM_NUM_DBG -
				3][blkRow*Hnum + blkCol];

			if (diff_frm_luma > 20 &&
				(Tf_luma_avg_frm[FRM_NUM_DBG - 1] == 0))
				bl_value_map =
				Tf_bl_matrix[FRM_NUM_DBG -
				3][blkRow*Hnum + blkCol];
			*/

			if (Tf_luma_avg_frm[FRM_NUM_DBG - 1] <= 1) {
				Tf_bl_matrix[FRM_NUM_DBG - 2][blkRow*Hnum +
							blkCol] = Bmax_lpf>>1;
			}


			if (diff_frm_luma < 10) {/*same video scene*/
				diff_blk_luma =
			Tf_blkLuma_avg[FRM_NUM_DBG - 1][blkRow*Hnum + blkCol] -
			Tf_blkLuma_avg[FRM_NUM_DBG - 2][blkRow*Hnum + blkCol];

				diff_blk_matrix =
				bl_value_map -
			Tf_bl_matrix[FRM_NUM_DBG-2][blkRow*Hnum + blkCol];
				diff_blk_matrix = ABS(diff_blk_matrix);
				Tf_diff_blk_luma[FRM_NUM_DBG -
				1][blkRow*Hnum + blkCol] = diff_blk_matrix;

				/*Debug print local value*/
				if (Dbprint_lv == 1) {
					if ((db_cnt%4) == 0) {
						/*4 frames print once*/
						pr_info(
"diff_blk_matrix[%4d], bl_value_map[%4d], Tf_bl_matrix[%4d], frm_luma[%4d]\n",
			diff_blk_matrix,
			bl_value_map,
			Tf_bl_matrix[FRM_NUM_DBG-2][blkRow*Hnum + blkCol],
			Tf_luma_avg_frm[FRM_NUM_DBG - 1]);
					}
				}
				if ((diff_blk_matrix > 50)) {
					/*
					if (Dbprint_lv == 1) {
						if ((db_cnt%4) == 0) {
							pr_info(
			"if(diff_blk_matrix > 20){...}\n");
						}
					}
					*/
					if (bl_value_map >=
			Tf_bl_matrix[FRM_NUM_DBG-2][blkRow*Hnum + blkCol]) {
						bl_value_map =
			Tf_bl_matrix[FRM_NUM_DBG-2][blkRow*Hnum + blkCol] +
			((diff_blk_matrix + 16) >> 5); /*1/32*/

						/*if (Dbprint_lv == 1) {
							if ((db_cnt%4) == 0) {
								pr_info(
		"bl_value_map[%4d]((diff_blk_matrix + 16) >> 5)[%4d]\n",
		bl_value_map, ((diff_blk_matrix + 16) >> 5));
								}
							}*/

					} else {
							bl_value_map =
			Tf_bl_matrix[FRM_NUM_DBG-2][blkRow*Hnum + blkCol] -
						((diff_blk_matrix + 16) >> 5);
					}
				} else {
					bl_value_map = bl_value_map;
				}
				 if (bl_value_map > 4095)
					bl_value_map = 4095;
				 if (bl_value_map <= 0)
					bl_value_map = 0;
			}

			/*Debug print local value*/
			if (Dbprint_lv == 1) {
				if ((db_cnt%4) == 0) {  /*4 frames print once*/
					pr_info("Aftert bl_value_map[%4d]\n",
						bl_value_map);
				}
			}

			/* if (Tf_luma_avg_frm[FRM_NUM_DBG - 1] == 0)
					bl_value_map = 0;
			if (bl_value_map <= lit_base)
				bl_value_map = lit_base; *//*default 127*/

			remap_value = bl_value_map;
			if (remap_value > 4095)
				remap_value = 4095;
			if (nPRM->reg_LD_BackLit_mode == 1) {
				nPRM->BL_matrix[blkCol*Vnum + blkRow]  =
								bl_value_map;
			    Map_bl_matrix[blkCol*Vnum + blkRow] = remap_value;
			} else {
				nPRM->BL_matrix[blkRow*Hnum + blkCol]  =
								bl_value_map;
			    Map_bl_matrix[blkRow*Hnum + blkCol] = remap_value;
			}


			/* Get the TF_BL_matrix */
			FDat->TF_BL_matrix[blkRow*Hnum + blkCol] = bl_value_map;
			Tf_bl_matrix[FRM_NUM_DBG-1][blkRow*Hnum + blkCol] =
								bl_value_map;

			/* leave the Delayed version for next frame */
			for (k = 0; k < 3; k++) {
				FDat->last_STA1_MaxRGB[blkRow*3*stride +
					blkCol*3 + k] =
					max_matrix[blkRow*3*stride +
						blkCol*3 + k];
			}
			/* get the sum/min/max */
			sum += bl_value_map;
			Bmin = MIN(Bmin, bl_value_map);
			Bmax = MAX(Bmax, bl_value_map);
		}
	}
	/*Debug print local value*/
	if (Dbprint_lv == 1) {
		if ((db_cnt%4) == 0) {  /*5 frames print once*/
			for (i = 0; i < 8; i++)
				pr_info("Tf_bl_matrix[blk_%d][%4d]\n", i,
					Tf_bl_matrix[FRM_NUM_DBG-1][i]);
				ldim_print_debug();
		}
	}
	db_cnt++;
	if (db_cnt > 4095)
		db_cnt = 0;

	/* set the DC reduction for the BL_modeling */
	if (fw_LD_BLEst_ACmode == 0)
		nPRM->reg_BL_matrix_AVG = 0;
	else if (fw_LD_BLEst_ACmode == 1) {
		/*nPRM->reg_BL_matrix_AVG = (sum/fw_blk_num);*/
		nPRM->reg_BL_matrix_AVG = ((sum/fw_blk_num) *
			avg_gain + 2048)>>12;
		Map_bl_matrix_AVG = ((sum/fw_blk_num) *
			avg_gain + 2048)>>12;
		Map_bl_matrix_Compensate = ((sum/fw_blk_num) *
			avg_gain + 2048)>>12;
	}
	else if (fw_LD_BLEst_ACmode == 2)
		nPRM->reg_BL_matrix_AVG = Bmin;
	else if (fw_LD_BLEst_ACmode == 3)
		nPRM->reg_BL_matrix_AVG = Bmax;
	else if (fw_LD_BLEst_ACmode == 4)
		nPRM->reg_BL_matrix_AVG = 2048;
	else
		nPRM->reg_BL_matrix_AVG = 1024;

	nPRM->reg_BL_matrix_Compensate = nPRM->reg_BL_matrix_AVG;
	/* printk("BL_AVG=%d; BL_COMP=%d\n", nPRM->reg_BL_matrix_AVG,
		nPRM->reg_BL_matrix_Compensate);
		LOG the prm to prm.txt
		FPRINTF_DBG(fid_prm,"\n ====================
		=\n nFrm= %d\n =====================\n",nFrm);
		kfree(tBL_matrix); */
}

void LDIM_WR_BASE_LUT_DRT(int base, int *pData, int len)
{
	int i;
	int addr;
	addr  = base;/*(base<<4)*/
	Wr(LDIM_BL_ADDR_PORT, addr);
	for (i = 0; i < len; i++)
		Wr(LDIM_BL_DATA_PORT, pData[i]);
}

static void ldim_stts_initial(unsigned int pic_h, unsigned int pic_v,
		unsigned int BLK_Vnum, unsigned int BLK_Hnum)
{
	unsigned int resolution, resolution_region, blk_height, blk_width;
	unsigned int row_start, col_start;

	LDIMPR("%s: %d %d %d %d\n", __func__, pic_h, pic_v, BLK_Vnum, BLK_Hnum);
	resolution = (((pic_h - 1) & 0xffff) << 16) | ((pic_v - 1) & 0xffff);
	/*Wr(VDIN0_HIST_CTRL, 0x10d);*/
	Wr(LDIM_STTS_CTRL0, 3<<3);/*4 mux to vpp_dout*/
	ldim_set_matrix_ycbcr2rgb();

/*	ldim_set_matrix_rgb2ycbcr(0); */
	ldim_stts_en(resolution, 0, 0, 1, 1, 1, 0);
	resolution_region = 0;
#if 0
	blk_height = pic_v/BLK_Vnum;
	blk_width = pic_h/BLK_Hnum;
	row_start = 0;
	col_start = 0;
#endif

	blk_height = (pic_v-8)/BLK_Vnum;
	blk_width = (pic_h-8)/BLK_Hnum;
	row_start = (pic_v-(blk_height*BLK_Vnum))>>1;
	col_start = (pic_h-(blk_width*BLK_Hnum))>>1;
	ldim_set_region(0, blk_height, blk_width,
		row_start, col_start, BLK_Hnum);
}

#if 0
static void ldim_remap_ctrl(int flag)
{
	unsigned int data;

	if (flag) {
		data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
		data = data | (1<<2);
		LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
	} else {
		data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
		data = data & (~(1<<2));
		LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
	}
}
#endif

static void LDIM_Initial(unsigned int pic_h, unsigned int pic_v,
		unsigned int BLK_Vnum, unsigned int BLK_Hnum,
		unsigned int BackLit_mode, unsigned int ldim_bl_en,
		unsigned int ldim_hvcnt_bypass)
{
	unsigned int i, j, k;
	unsigned int data;
	unsigned int *arrayTmp;

	LDIMPR("%s: %d %d %d %d %d %d %d\n",
		__func__, pic_h, pic_v, BLK_Vnum, BLK_Hnum,
		BackLit_mode, ldim_bl_en, ldim_hvcnt_bypass);

	ldim_matrix_update_en = ldim_bl_en;

	arrayTmp = kmalloc(1536*sizeof(unsigned int), GFP_KERNEL);
	LD_ConLDReg(&nPRM);
	/* config params begin */
	/* configuration of the panel parameters */
	nPRM.reg_LD_pic_RowMax = pic_v;
	nPRM.reg_LD_pic_ColMax = pic_h;
	/* Maximum to BLKVMAX  , Maximum to BLKHMAX */
	nPRM.reg_LD_BLK_Vnum     = BLK_Vnum;
	nPRM.reg_LD_BLK_Hnum     = BLK_Hnum;
	nPRM.reg_LD_BackLit_mode = BackLit_mode;
	/*config params end */
	ld_fw_cfg_once(&nPRM);
	/*stimulus_print("[LDIM] Config LDIM beginning.....\n");*/
	/*enable the CBUS configure the RAM*/
	/*REG_LD_MISC_CTRL0  {ram_clk_gate_en,2'h0,ldlut_ram_sel,
	ram_clk_sel,reg_hvcnt_bypass,reg_ldim_bl_en,soft_bl_start,
	reg_soft_rst)*/
	data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
	data = data & (~(3<<4));
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
	/*change here: gBLK_Hidx_LUT: s14*19 */
	LDIM_WR_BASE_LUT(REG_LD_BLK_HIDX_BASE,
			nPRM.reg_LD_BLK_Hidx, 16, LD_BLK_LEN_H);
	/*stimulus_display("reg_LD_BLK_Hidx[0]=%x\n",nPRM.reg_LD_BLK_Hidx[0]);*/
	/* change here: gBLK_Vidx_LUT: s14*19 */
	LDIM_WR_BASE_LUT(REG_LD_BLK_VIDX_BASE,
			nPRM.reg_LD_BLK_Vidx, 16, LD_BLK_LEN_V);
	/*stimulus_display("reg_LD_BLK_Vidx[0]=%x\n",nPRM.reg_LD_BLK_Vidx[0]);*/
	/* change here: gHDG_LUT: u10*32  */
	LDIM_WR_BASE_LUT(REG_LD_LUT_HDG_BASE,
			nPRM.reg_LD_LUT_Hdg, 16, LD_LUT_LEN);
	/* stimulus_display("reg_LD_LUT_Hdg[0]=%x\n",nPRM.reg_LD_LUT_Hdg[0]); */
	/* change here: gVDG_LUT: u10*32 */
	LDIM_WR_BASE_LUT(REG_LD_LUT_VDG_BASE,
			nPRM.reg_LD_LUT_Vdg, 16, LD_LUT_LEN);
	/* stimulus_display("reg_LD_LUT_Vdg[0]=%x\n",nPRM.reg_LD_LUT_Vdg[0]); */
	/* change here: gVHk_LUT: u10*32  */
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_BASE,
			nPRM.reg_LD_LUT_VHk, 16, LD_LUT_LEN);
	/* stimulus_display("reg_LD_LUT_VHk[0]=%x\n",nPRM.reg_LD_LUT_VHk[0]); */
	/* reg_LD_LUT_VHk_pos[32]/reg_LD_LUT_VHk_neg[32]: u8 */
	for (i = 0; i < 32; i++)
		arrayTmp[i]    =  nPRM.reg_LD_LUT_VHk_pos[i];
	for (i = 0; i < 32; i++)
		arrayTmp[32+i] =  nPRM.reg_LD_LUT_VHk_neg[i];
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_NEGPOS_BASE, arrayTmp, 8, 64);
	/* stimulus_display("reg_LD_LUT_VHk_pos[0]=%x\n",
			nPRM.reg_LD_LUT_VHk_pos[0]); */
	/* stimulus_display("reg_LD_LUT_VHk_neg[0]=%x\n",
			nPRM.reg_LD_LUT_VHk_neg[0]); */
	/* reg_LD_LUT_VHo_pos[32]/reg_LD_LUT_VHo_neg[32]: s8 */
	for (i = 0; i < 32; i++)
		arrayTmp[i]    =  nPRM.reg_LD_LUT_VHo_pos[i];
	for (i = 0; i < 32; i++)
		arrayTmp[32+i] =  nPRM.reg_LD_LUT_VHo_neg[i];
	LDIM_WR_BASE_LUT(REG_LD_LUT_VHO_NEGPOS_BASE, arrayTmp, 8, 64);
	/* stimulus_display("reg_LD_LUT_VHo_pos[0]=%x\n",
			nPRM.reg_LD_LUT_VHo_pos[0]); */
	/* stimulus_display("reg_LD_LUT_VHo_neg[0]=%x\n",
			nPRM.reg_LD_LUT_VHo_neg[0]); */
	/* reg_LD_LUT_HHk[32]:u8 */
	LDIM_WR_BASE_LUT(REG_LD_LUT_HHK_BASE, nPRM.reg_LD_LUT_HHk, 8, 32);
	/*stimulus_display("reg_LD_LUT_HHk[0]=%x\n",
			nPRM.reg_LD_LUT_HHk[0]); */
	/*gLD_REFLECT_DGR_LUT: u6 * (20+20+4) */
	for (i = 0; i < 20; i++)
		arrayTmp[i] = nPRM.reg_LD_Reflect_Hdgr[i];
	for (i = 0; i < 20; i++)
		arrayTmp[20+i] = nPRM.reg_LD_Reflect_Vdgr[i];
	for (i = 0; i < 4; i++)
		arrayTmp[40+i] = nPRM.reg_LD_Reflect_Xdgr[i];
	LDIM_WR_BASE_LUT(REG_LD_REFLECT_DGR_BASE, arrayTmp, 8, 44);
	/* stimulus_display("reg_LD_Reflect_Hdgr[0]=%x\n",
			nPRM.reg_LD_Reflect_Hdgr[0]); */
	/* stimulus_display("reg_LD_Reflect_Vdgr[0]=%x\n",
			nPRM.reg_LD_Reflect_Vdgr[0]); */
	/* stimulus_display("reg_LD_Reflect_Xdgr[0]=%x\n",
			nPRM.reg_LD_Reflect_Xdgr[0]); */
	/* X_lut: 12 * 3*16*32 */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 16; j++)
			for (k = 0; k < 32; k++)
				arrayTmp[16*32*i+32*j+k] = nPRM.X_lut[i][j][k];
	LDIM_WR_BASE_LUT(REG_LD_RGB_LUT_BASE, arrayTmp, 16, 32*16*3);
	/* stimulus_display("reg_X_lut[0]=%x\n",nPRM.X_lut[0][0][0]); */
	/* X_nrm: 4 * 16 */
	LDIM_WR_BASE_LUT(REG_LD_RGB_NRMW_BASE, nPRM.X_nrm[0], 4, 16);
	/*stimulus_display("reg_X_nrm[0]=%x\n",nPRM.X_nrm[0][0]); */
	/*  X_idx: 12*16  */
	/*LDIM_WR_BASE_LUT(REG_LD_RGB_IDX_BASE, nPRM.X_idx[0], 12, 16);*/
	LDIM_WR_BASE_LUT(REG_LD_RGB_IDX_BASE, nPRM.X_idx[0], 16, 16);
	/* stimulus_display("reg_X_idx[0]=%x\n",nPRM.X_idx[0][0]); */
	/*  gMatrix_LUT: u12*LD_BLKREGNUM  */
	LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE, nPRM.BL_matrix, LD_BLKREGNUM);
	/* stimulus_display("BL_matrix[0]=%x\n",nPRM.BL_matrix[0]); */
	/*  LD_FRM_SIZE  */
	data = ((nPRM.reg_LD_pic_RowMax&0xfff)<<16) |
					(nPRM.reg_LD_pic_ColMax&0xfff);
	LDIM_WR_32Bits(REG_LD_FRM_SIZE, data);
	/* LD_RGB_MOD */
	data = ((nPRM.reg_LD_RGBmapping_demo & 0x1) << 19) |
			((nPRM.reg_LD_X_LUT_interp_mode[2] & 0x1) << 18) |
			((nPRM.reg_LD_X_LUT_interp_mode[1] & 0x1) << 17) |
			((nPRM.reg_LD_X_LUT_interp_mode[0] & 0x1) << 16) |
			((nPRM.reg_LD_BkLit_LPFmod & 0x7) << 12) |
			((nPRM.reg_LD_Litshft  & 0x7) << 8) |
			((nPRM.reg_LD_BackLit_Xtlk & 0x1) << 7) |
			((nPRM.reg_LD_BkLit_Intmod & 0x1) << 6) |
			((nPRM.reg_LD_BkLUT_Intmod & 0x1) << 5) |
			((nPRM.reg_LD_BkLit_curmod & 0x1) << 4) |
			((nPRM.reg_LD_BackLit_mode & 0x3));
	LDIM_WR_32Bits(REG_LD_RGB_MOD, data);
	/* LD_BLK_HVNUM  */
	data = ((nPRM.reg_LD_Reflect_Vnum & 0x7) << 20) |
			((nPRM.reg_LD_Reflect_Hnum & 0x7) << 16) |
			((nPRM.reg_LD_BLK_Vnum & 0x3f) << 8) |
			((nPRM.reg_LD_BLK_Hnum & 0x3f));
	LDIM_WR_32Bits(REG_LD_BLK_HVNUM, data);
	/* REG_LD_FRM_HBLAN_VHOLS  */
	data = ((nPRM.reg_LD_LUT_VHo_LS & 0x7) << 16) |
					((6 & 0x1fff)) ;  /*frm_hblank_num */
	LDIM_WR_32Bits(REG_LD_FRM_HBLAN_VHOLS, data);
	/* LD_HVGAIN */
	data = ((nPRM.reg_LD_Vgain & 0xfff) << 16) |
					(nPRM.reg_LD_Hgain & 0xfff);
	LDIM_WR_32Bits(REG_LD_HVGAIN, data);
	/*  LD_LIT_GAIN_COMP */
	data = ((nPRM.reg_LD_Litgain & 0xfff) << 16) |
					(nPRM.reg_BL_matrix_Compensate & 0xfff);
	LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	/*  LD_BKLIT_VLD  */
	data = 0;
	for (i = 0; i < 32; i++)
		if (nPRM.reg_LD_BkLit_valid[i])
			data = data | (1<<i);
	LDIM_WR_32Bits(REG_LD_BKLIT_VLD, data);
	/* LD_BKLIT_PARAM */
	data = ((nPRM.reg_LD_BkLit_Celnum & 0xff) << 16) |
				(nPRM.reg_BL_matrix_AVG & 0xfff);
	LDIM_WR_32Bits(REG_LD_BKLIT_PARAM, data);
	/* REG_LD_LUT_XDG_LEXT */
	data = ((nPRM.reg_LD_LUT_Vdg_LEXT & 0x3ff) << 20) |
				((nPRM.reg_LD_LUT_VHk_LEXT & 0x3ff) << 10) |
				(nPRM.reg_LD_LUT_Hdg_LEXT & 0x3ff);
	LDIM_WR_32Bits(REG_LD_LUT_XDG_LEXT, data);

	/* LD_FRM_RST_POS */
	data = (16<<16) | (3); /* h=16,v=3 :ldim_param_frm_rst_pos */
	LDIM_WR_32Bits(REG_LD_FRM_RST_POS, data);
	/* LD_FRM_BL_START_POS */
	data = (16<<16) | (4); /* ldim_param_frm_bl_start_pos; */
	LDIM_WR_32Bits(REG_LD_FRM_BL_START_POS, data);

	/* REG_LD_XLUT_DEMO_ROI_XPOS */
	data = ((nPRM.reg_LD_xlut_demo_roi_xend & 0x1fff) << 16) |
			(nPRM.reg_LD_xlut_demo_roi_xstart & 0x1fff);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_XPOS, data);

	/* REG_LD_XLUT_DEMO_ROI_YPOS */
	data = ((nPRM.reg_LD_xlut_demo_roi_yend & 0x1fff) << 16) |
			(nPRM.reg_LD_xlut_demo_roi_ystart & 0x1fff);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_YPOS, data);

	/* REG_LD_XLUT_DEMO_ROI_CTRL */
	data = ((nPRM.reg_LD_xlut_oroi_enable & 0x1) << 1) |
			(nPRM.reg_LD_xlut_iroi_enable & 0x1);
	LDIM_WR_32Bits(REG_LD_XLUT_DEMO_ROI_CTRL, data);

	/* REG_LD_MISC_CTRL0 {ram_clk_gate_en,2'h0,ldlut_ram_sel,ram_clk_sel,
	reg_hvcnt_bypass,reg_ldim_bl_en,soft_bl_start,reg_soft_rst) */
	data = (0 << 1) | (ldim_bl_en << 2) |
			(ldim_hvcnt_bypass << 3) | (3 << 4) | (1 << 8);
	/* ldim_param_misc_ctrl0; */
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
	kfree(arrayTmp);

}

static int aml_ldim_open(struct inode *inode, struct file *file)
{
    /* @todo */
	return 0;
}

static int aml_ldim_release(struct inode *inode, struct file *file)
{
    /* @todo */
	return 0;
}

static long aml_ldim_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
    /* @todo */
	return 0;
}

static const struct file_operations aml_ldim_fops = {
	.owner          = THIS_MODULE,
	.open           = aml_ldim_open,
	.release        = aml_ldim_release,
	.unlocked_ioctl = aml_ldim_ioctl,
};

static dev_t aml_ldim_devno;
static struct class *aml_ldim_clsp;
static struct cdev *aml_ldim_cdevp;

static void ldim_dump_histgram(void)
{
	unsigned int i, j, k;
	unsigned int *p = NULL;

	p = kmalloc(2048*sizeof(unsigned int), GFP_KERNEL);
	if (p == NULL) {
		pr_err("malloc momery err!!!\n");
		return;
	}
	memcpy(p, &hist_matrix[0],
		ldim_hist_row*ldim_hist_col*16*sizeof(unsigned int));
	for (i = 0; i < ldim_hist_row; i++) {
		for (j = 0; j < ldim_hist_col; j++) {
			for (k = 0; k < 16; k++) {
				pr_info("0x%x\t",
					*(p+i*16*ldim_hist_col+j*16+k));
			}
			pr_info("\n");
			udelay(10000);
		}
		pr_info("\n");
	}
	kfree(p);
}

static irqreturn_t rdma_ldim_intr(unsigned int irq, void *dev_id)
{
	ulong flags;
	/*LDIMPR("*********rdma_ldim_intr start*********\n");*/
	spin_lock_irqsave(&rdma_ldim_isr_lock, flags);

	if (ldim_hist_en) {
		schedule_work(&ldim_read_work);
		/*ldim_read_region(ldim_hist_row, ldim_hist_col);*/
	}
	rdma_ldim_irq_cnt++;
	if (rdma_ldim_irq_cnt > 0xfffffff)
		rdma_ldim_irq_cnt = 0;
	spin_unlock_irqrestore(&rdma_ldim_isr_lock, flags);
	/*LDIMPR("*********rdma_ldim_intr end*********\n");*/
	return IRQ_HANDLED;
}
#if 1
/*
 * vsync fiq handler
 */
static irqreturn_t ldim_vsync_isr(unsigned int irq, void *dev_id)
{
	ulong flags;

	if (ldim_on_flag == 0)
		return IRQ_HANDLED;
	/*LDIMPR("*********ldim_vsync_isr start*********\n");*/
	spin_lock_irqsave(&ldim_isr_lock, flags);

	if (ldim_avg_update_en)
		ldim_update_setting();
	tasklet_schedule(&ldim_tasklet);

	ldim_irq_cnt++;
	if (ldim_irq_cnt > 0xfffffff)
		ldim_irq_cnt = 0;
	spin_unlock_irqrestore(&ldim_isr_lock, flags);
/*	LDIMPR("*********ldim_vsync_isr end*********\n");*/

	return IRQ_HANDLED;
}
#endif

#if 0
/*
 * vsync fiq handler
 */
static irqreturn_t ldim_vsync_isr(unsigned int irq, void *dev_id)
{
	ulong flags;
	/* unsigned int ldim_read_region_i = 0; */
	ld_on_vs_start_time = jiffies_to_usecs(jiffies);

	if (vs_time_record == 1)
		LDIMPR("*********ldim_vsync_isr start*********\n");

	spin_lock_irqsave(&ldim_isr_lock, flags);
	if (ldim_avg_update_en)
		ldim_update_setting();
	if (vs_time_record == 1)
		LDIMPR("*********queue_delayed_work start *********\n");
	if (ldim_hist_en) {
		ldim_read_region(ldim_hist_row, ldim_hist_col);
		if (vs_time_record == 1)
			LDIMPR("*********ldim_read_region  end *********\n");
	}
	if (vs_time_record == 1)
		LDIMPR("*********ldim_on_vs start *********\n");
	ldim_on_vs();
	if (vs_time_record == 1)
		LDIMPR("*********ldim_on_vs end *********\n");
	ldim_irq_cnt++;
	if (ldim_irq_cnt > 0xfffffff)
		ldim_irq_cnt = 0;
	spin_unlock_irqrestore(&ldim_isr_lock, flags);

	ld_on_vs_end_time = jiffies_to_usecs(jiffies);
	ld_on_vs_time = ld_on_vs_end_time - ld_on_vs_start_time;

	if (vs_time_record == 1)
		LDIMPR("*********ldim_vsync_isr end *********\n");

	return IRQ_HANDLED;
}
#endif

static void ldim_update_setting(void)
{
	unsigned int data, i;
	/* enable the CBUS configure the RAM */
	/* REG_LD_MISC_CTRL0  {ram_clk_gate_en,2'h0,ldlut_ram_sel,ram_clk_sel,
	reg_hvcnt_bypass,reg_ldim_bl_en,soft_bl_start,reg_soft_rst) */

	if (ldim_avg_update_en) {
		/* pr_info("_1BL_AVG=%x;1BL_COMP=%x\n",nPRM.reg_BL_matrix_AVG,
		nPRM.reg_BL_matrix_Compensate); */

		/* LD_BKLIT_PARAM */
		data = LDIM_RD_32Bits(REG_LD_BKLIT_PARAM);
		data = (data&(~0xfff)) | (nPRM.reg_BL_matrix_AVG&0xfff);
		/* pr_info("_2BL_AVG=%x;2BL_COMP=%x\n",nPRM.reg_BL_matrix_AVG,
		nPRM.reg_BL_matrix_Compensate);*/
		/* printk("data=%x\n",data); */
		/*LDIM_WR_32Bits(REG_LD_BKLIT_PARAM, data);*/
		VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT, REG_LD_BKLIT_PARAM);
		VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, data);

		/* compensate */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		/* data = data|(nPRM.reg_BL_matrix_Compensate&0xfff); */
		data = (data&(~0xfff)) |
			(nPRM.reg_BL_matrix_Compensate & 0xfff);
		/*LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);*/
		VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT, REG_LD_LIT_GAIN_COMP);
		VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, data);
	}
	if (ldim_matrix_update_en) {
		data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
		data = data & (~(3<<4));
		data = data | (1<<2);
		/*LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);*/
		VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT, REG_LD_MISC_CTRL0);
		VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, data);

		/* gMatrix_LUT: s12*100 ==> max to 8*8 enum ##r/w ram method*/
		/*LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&(nPRM.BL_matrix[0]), ldim_blk_row*ldim_blk_col);*/
#if 0
		VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT, REG_LD_MATRIX_BASE);
		for (i = 0; i < (ldim_blk_row*ldim_blk_col); i++)
			VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, nPRM.BL_matrix[i]);
#else
		for (i = 0; i < (ldim_blk_row*ldim_blk_col); i++) {
			VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT,
				REG_LD_MATRIX_BASE+i);
			VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, nPRM.BL_matrix[i]);
		}
#endif

		/*data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);*/
		data = data | (3<<4);
		/*LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);*/
		VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT, REG_LD_MISC_CTRL0);
		VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, data);
	} else {
		data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
		data = data & (~(1<<2));
		/*LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);*/
		VSYNC_WR_MPEG_REG(LDIM_BL_ADDR_PORT, REG_LD_MISC_CTRL0);
		VSYNC_WR_MPEG_REG(LDIM_BL_DATA_PORT, data);
	}
	/* disable the CBUS configure the RAM */
}

static void ldim_update_matrix(unsigned int mode)
{
	unsigned int data;
	int bl_matrix[8] = {0};
	unsigned int reg_BL_matrix_Compensate = 0x0;
	int bl_matrix_1[8] = {0xfff, 0xfff, 0xfff, 0xfff, 0xfff, 0xfff,
					0xfff, 0xfff};
	unsigned int reg_BL_matrix_Compensate_1 = 0xfff;
	int bl_matrix_2[8] = {0xfff, 0xfff, 0xfff, 0x000, 0x000, 0xfff,
					0xfff, 0xfff};
	unsigned int reg_BL_matrix_Compensate_2 = 0xbff;
	int bl_matrix_3[8] = {0, 0, 0, 0xfff, 0, 0, 0, 0};
	unsigned int reg_BL_matrix_Compensate_3 = 0x1ff;
	int bl_matrix_4[8] = {0xfff, 0xfff, 0xfff, 0, 0xfff, 0xfff,
					0xfff, 0xfff};
	unsigned int reg_BL_matrix_Compensate_4 = 0xdff;
	/* enable the CBUS configure the RAM */
	/* REG_LD_MISC_CTRL0  {ram_clk_gate_en,2'h0,ldlut_ram_sel,ram_clk_sel,
	reg_hvcnt_bypass,reg_ldim_bl_en,soft_bl_start,reg_soft_rst) */
	data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
	data = data & (~(3<<4));
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);

	/* gMatrix_LUT: s12*100 ==> max to 8*8 enum ##r/w ram method*/
	if (mode == 0) {
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&bl_matrix[0], 8);
		/*  compensate  */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = data|(reg_BL_matrix_Compensate&0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	} else if (mode == 1) {
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&bl_matrix_1[0], 8);
	/*  compensate  */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = data | (reg_BL_matrix_Compensate_1 & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	} else if (mode == 2) {
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&bl_matrix_2[0], 8);
	/*  compensate  */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = data|(reg_BL_matrix_Compensate_2 & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	} else if (mode == 3) {
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&bl_matrix_3[0], 8);
	/* compensate */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = data | (reg_BL_matrix_Compensate_3 & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	} else if (mode == 4) {
		LDIM_WR_BASE_LUT_DRT(REG_LD_MATRIX_BASE,
			&bl_matrix_4[0], 8);
	/* compensate */
		data = LDIM_RD_32Bits(REG_LD_LIT_GAIN_COMP);
		data = data | (reg_BL_matrix_Compensate_4 & 0xfff);
		LDIM_WR_32Bits(REG_LD_LIT_GAIN_COMP, data);
	}
	/* disable the CBUS configure the RAM */
	data = LDIM_RD_32Bits(REG_LD_MISC_CTRL0);
	data = data | (3<<4);
	LDIM_WR_32Bits(REG_LD_MISC_CTRL0, data);
}

static unsigned short ldim_test_matrix[LD_BLKREGNUM];
static unsigned short local_ldim_matrix[LD_BLKREGNUM] = {0};

static void ldim_on_vs_spi(unsigned long data)
{
	unsigned int size;
	unsigned short *mapping;
	unsigned int i;
	int ret;

	if (ldim_on_flag == 0)
		return;
	if (ldim_bypass)
		return;

	size = ldim_blk_row * ldim_blk_col;
	mapping = &ldim_config.bl_mapping[0];

	if (ldim_func_en) {
		if (ldim_func_bypass)
			return;
		if (ldim_test_en) {
			for (i = 0; i < size; i++) {
				local_ldim_matrix[i] =
					(unsigned short)
					nPRM.BL_matrix[mapping[i]];
				ldim_driver.ldim_matrix_buf[i] =
					ldim_test_matrix[mapping[i]];
			}
		} else {
			for (i = 0; i < size; i++) {
				local_ldim_matrix[i] =
					(unsigned short)
					nPRM.BL_matrix[mapping[i]];
				ldim_driver.ldim_matrix_buf[i] =
					(unsigned short)
					(((nPRM.BL_matrix[mapping[i]] * litgain)
					+ (LD_DATA_MAX / 2)) >> LD_DATA_DEPTH);
			}
		}
	} else {
		if (ldim_level_update) {
			ldim_level_update = 0;
			if (ldim_debug_print) {
				LDIMPR("%s: level update: 0x%lx\n",
					__func__, litgain);
			}
			for (i = 0; i < size; i++) {
				local_ldim_matrix[i] =
					(unsigned short)
					nPRM.BL_matrix[mapping[i]];
				ldim_driver.ldim_matrix_buf[i] =
					(unsigned short)(litgain);
			}
		} else {
			if (ldim_driver.device_bri_check) {
				ret = ldim_driver.device_bri_check();
				if (ret) {
					if (ldim_debug_print) {
						LDIMERR(
						"%s: device_bri_check error\n",
						__func__);
					}
					ldim_level_update = 1;
				}
			}
			return;
		}
	}

	/* set_bri_for_channels(ldim_driver.ldim_matrix_buf); */
	if (ldim_driver.device_bri_update) {
		ldim_driver.device_bri_update(ldim_driver.ldim_matrix_buf,
			size);
		if (ldim_driver.static_pic_flag == 1) {
			ldim_get_matrix_info_6();
			ldim_dump_histgram();
		}
	} else {
		LDIMERR("%s: device_bri_update is null\n", __func__);
	}
}

static void ldim_on_vs_arithmetic(void)
{
	unsigned int *local_ldim_hist = NULL;
	unsigned int *local_ldim_max = NULL;
	unsigned int *local_ldim_max_rgb = NULL;
	unsigned int i;
	/* unsigned short Backlit_coeff;
	unsigned int time_start,time_end;
	static unsigned short local_ldim_matrix_add_coeff[16]={0};
	Backlit_coeff = Backlit_coeff_l;*/

	if (ldim_top_en == 0)
		return;
	local_ldim_hist = kmalloc(ldim_hist_row*ldim_hist_col*16*
			sizeof(unsigned int), GFP_KERNEL);
	if (local_ldim_hist == NULL) {
		pr_err("local_ldim_hist malloc momery err!!!\n");
		return;
	}
	local_ldim_max = kmalloc(ldim_hist_row*ldim_hist_col*
			sizeof(unsigned int), GFP_KERNEL);
	if (local_ldim_max == NULL) {
		pr_err("local_ldim_max malloc momery err!!!\n");
		kfree(local_ldim_hist);
		return;
	}
	local_ldim_max_rgb = kmalloc(ldim_hist_row*ldim_hist_col*3*
			sizeof(unsigned int), GFP_KERNEL);
	if (local_ldim_max_rgb == NULL) {
		pr_err("local_ldim_max_rgb malloc momery err!!!\n");
		kfree(local_ldim_hist);
		kfree(local_ldim_max);
		return;
	}
	/* spin_lock_irqsave(&ldim_isr_lock, flags); */
	memcpy(local_ldim_hist, &hist_matrix[0],
		ldim_hist_row*ldim_hist_col*16*sizeof(unsigned int));
	memcpy(local_ldim_max, &max_rgb[0],
		ldim_hist_row*ldim_hist_col*sizeof(unsigned int));
	memcpy(&global_ldim_max[0], &max_rgb[0],
		ldim_hist_row*ldim_hist_col*sizeof(unsigned int));
	for (i = 0; i < ldim_hist_row*ldim_hist_col; i++) {
		(*(local_ldim_max_rgb+i*3)) =
			(*(local_ldim_max+i))&0x3ff;
		(*(local_ldim_max_rgb+i*3+1)) =
			(*(local_ldim_max+i))>>10&0x3ff;
		(*(local_ldim_max_rgb+i*3+2)) =
			(*(local_ldim_max+i))>>20&0x3ff;
	}
	if (ldim_alg_en) {
		/*printk("ld_fw_alg_frm_start\n");*/
		ld_fw_alg_frm_start_time = jiffies_to_usecs(jiffies);
		ld_fw_alg_frm(&nPRM, &FDat, local_ldim_max_rgb,
				local_ldim_hist);
		ld_fw_alg_frm_end_time = jiffies_to_usecs(jiffies);
		ld_fw_alg_frm_time = ld_fw_alg_frm_end_time -
				ld_fw_alg_frm_start_time;

		kfree(local_ldim_hist);
		kfree(local_ldim_max);
		kfree(local_ldim_max_rgb);
	}
}

#if 0
static void ldim_on_vs(void)
{
	unsigned int *local_ldim_hist = NULL;
	unsigned int *local_ldim_max = NULL;
	unsigned int *local_ldim_max_rgb = NULL;
	unsigned int i;
/*	unsigned short Backlit_coeff;
	unsigned int time_start,time_end;
	static unsigned short local_ldim_matrix_add_coeff[16]={0};
	Backlit_coeff = Backlit_coeff_l;*/

	if (ldim_top_en == 0)
		return;
	local_ldim_hist = kmalloc(ldim_hist_row*ldim_hist_col*16*
			sizeof(unsigned int), GFP_KERNEL);
	if (local_ldim_hist == NULL) {
		pr_err("local_ldim_hist malloc momery err!!!\n");
		return;
	}
	local_ldim_max = kmalloc(ldim_hist_row*ldim_hist_col*
			sizeof(unsigned int), GFP_KERNEL);
	if (local_ldim_max == NULL) {
		pr_err("local_ldim_max malloc momery err!!!\n");
		kfree(local_ldim_hist);
		return;
	}
	local_ldim_max_rgb = kmalloc(ldim_hist_row*ldim_hist_col*3*
			sizeof(unsigned int), GFP_KERNEL);
	if (local_ldim_max_rgb == NULL) {
		pr_err("local_ldim_max_rgb malloc momery err!!!\n");
		kfree(local_ldim_hist);
		kfree(local_ldim_max);
		return;
	}
	/* spin_lock_irqsave(&ldim_isr_lock, flags); */
	memcpy(local_ldim_hist, &hist_matrix[0],
		ldim_hist_row*ldim_hist_col*16*sizeof(unsigned int));
	memcpy(local_ldim_max, &max_rgb[0],
		ldim_hist_row*ldim_hist_col*sizeof(unsigned int));
	memcpy(&global_ldim_max[0], &max_rgb[0],
		ldim_hist_row*ldim_hist_col*sizeof(unsigned int));
	for (i = 0; i < ldim_hist_row*ldim_hist_col; i++) {
		(*(local_ldim_max_rgb+i*3)) =
			(*(local_ldim_max+i))&0x3ff;
		(*(local_ldim_max_rgb+i*3+1)) =
			(*(local_ldim_max+i))>>10&0x3ff;
		(*(local_ldim_max_rgb+i*3+2)) =
			(*(local_ldim_max+i))>>20&0x3ff;
	}
	if (ldim_alg_en) {
		/*printk("ld_fw_alg_frm_start\n");*/
		ld_fw_alg_frm_start_time = jiffies_to_usecs(jiffies);
		ld_fw_alg_frm(&nPRM, &FDat, local_ldim_max_rgb,
				local_ldim_hist);
		ld_fw_alg_frm_end_time = jiffies_to_usecs(jiffies);
		ld_fw_alg_frm_time = ld_fw_alg_frm_end_time -
				ld_fw_alg_frm_start_time;
		/*printk("ld_fw_alg_frm_end\n");*/
		/*for iw7019*/
#if 1
		/*memcpy(&local_ldim_matrix[0], &FDat.TF_BL_matrix[0],
		ldim_hist_col*ldim_hist_row*sizeof(unsigned int));*/
		/*memcpy(&local_ldim_matrix[0], &nPRM.BL_matrix[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned int));*/
/*
		for (i = 0; i < ldim_blk_col*ldim_blk_row; i++) {
			local_ldim_matrix[i] = nPRM.BL_matrix[i];
			local_ldim_matrix_2_spi[i] = (unsigned short)
				(((nPRM.BL_matrix[i] * litgain)+2048)>>12);
		}
*/

		for (i = 0; i < (ldim_blk_row * ldim_blk_col); i++) {
			local_ldim_matrix[i] = nPRM.BL_matrix[
				ldim_config.bl_mapping[i]];
			ldim_driver.ldim_matrix_buf[i] = (unsigned short)
				(((nPRM.BL_matrix[ldim_config.bl_mapping[i]] *
				litgain) + 2048) >> 12);
		}

		/* set_bri_for_channels(local_ldim_matrix_2_spi); */
		if (ldim_driver.device_bri_update) {
			ldim_driver.device_bri_update(
				ldim_driver.ldim_matrix_buf,
				(ldim_blk_row * ldim_blk_col));
		} else {
			LDIMPR("%s: device_bri_update is null\n", __func__);
		}

#endif
	}
	/*spin_unlock_irqrestore(&ldim_isr_lock, flags);*/
	kfree(local_ldim_hist);
	kfree(local_ldim_max);
	kfree(local_ldim_max_rgb);
}
#endif

static void ldim_print_debug(void)
{
	unsigned int k, i, j;
	unsigned int local_ldim_matrix_t[LD_BLKREGNUM] = {0};

	LDIMPR("%s:\n", __func__);
	pr_info("512step:[luma_avg_frm][Diff_avg_frm:]\n");
	for (i = 0; i < FRM_NUM_DBG; i++)
		pr_info("Tf_luma_avg_frm[%4d] =[%4d][%4d]\n", i,
			Tf_luma_avg_frm[i], Tf_diff_frm_luma[i]);

	memcpy(&local_ldim_matrix_t[0], &FDat.TF_BL_matrix_2[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned int));

	pr_info("512step: ");
	for (k = 0; k < FRM_NUM_DBG; k++) {
		pr_info("Frame[%d][Luma_avg][Diff_blk_luma][bl_matrix]:\n", k);
		for (i = 0; i < ldim_blk_row; i++) {
			for (j = 0; j < ldim_blk_col; j++) {
				pr_info("[%4d][%4d][%4d]\n",
					Tf_blkLuma_avg[k][ldim_blk_col*i+j],
					Tf_diff_blk_luma[k][ldim_blk_col*i+j],
					Tf_bl_matrix[k][ldim_blk_col*i+j]);
			}
			pr_info("\n");
			udelay(10000);
		}
	}
	/* for (k = 0; k < FRM_NUM_DBG; k++) {
		pr_info("Frame[%d][bl_matrix_map][bl_matrix]:\n", k);
		for (i = 0; i < ldim_blk_row; i++) {
			for (j = 0; j < ldim_blk_col; j++) {
				pr_info("[%4d][%4d]\n",
					Tf_bl_matrix_map[k][ldim_blk_col*i+j],
					Tf_bl_matrix[k][ldim_blk_col*i+j]);
			 }
			pr_info("\n");
			udelay(10000);
		 }
	 }*/
}
static void ldim_get_matrix_info_2(void)
{
	unsigned int i, j;
	unsigned int local_ldim_matrix_t[LD_BLKREGNUM] = {0};

	LDIMPR("%s:\n", __func__);
	memcpy(&local_ldim_matrix_t[0], &FDat.TF_BL_matrix[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned int));

	for (i = 0; i < ldim_blk_row; i++) {
		for (j = 0; j < ldim_blk_col; j++) {
			pr_info("0x%x\t",
				local_ldim_matrix_t[ldim_blk_col*i+j]);
		}
		pr_info("\n");
		udelay(10000);
	}
}
static void ldim_get_matrix_info_3(void)
{
	unsigned int i, j;
	unsigned int local_ldim_matrix_t[LD_BLKREGNUM] = {0};

	LDIMPR("%s:\n", __func__);
	memcpy(&local_ldim_matrix_t[0], &FDat.SF_BL_matrix[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned int));

	for (i = 0; i < ldim_blk_row; i++) {
		for (j = 0; j < ldim_blk_col; j++) {
			pr_info("0x%x\t",
				local_ldim_matrix_t[ldim_blk_col*i+j]);
		}
		pr_info("\n");
		udelay(10000);
	}
}
static void ldim_get_matrix_info_4(void)
{
	unsigned int i, j, k;
	unsigned int *local_ldim_matrix_t = NULL;

	LDIMPR("%s:\n", __func__);
	local_ldim_matrix_t = kmalloc(ldim_hist_row*
		ldim_hist_col*16*sizeof(unsigned int), GFP_KERNEL);
	memcpy(local_ldim_matrix_t, &FDat.last_STA1_MaxRGB[0],
		ldim_blk_col*ldim_blk_row*3*sizeof(unsigned int));

	for (i = 0; i < ldim_blk_row; i++) {
		for (j = 0; j < ldim_blk_col; j++) {
			for (k = 0; k < 3; k++) {
				pr_info("0x%x\t",
				local_ldim_matrix_t[3*ldim_blk_col*i+j*3+k]);
			}
		}
		pr_info("\n");
		udelay(10000);
	}
	kfree(local_ldim_matrix_t);
}
static void ldim_get_matrix_info_5(void)
{
	unsigned int i, j;
	unsigned int local_ldim_matrix_t[LD_BLKREGNUM] = {0};

	LDIMPR("%s:\n", __func__);
	memcpy(&local_ldim_matrix_t[0], &FDat.TF_BL_alpha[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned int));

	for (i = 0; i < ldim_blk_row; i++) {
		for (j = 0; j < ldim_blk_col; j++) {
			pr_info("0x%x\t",
				local_ldim_matrix_t[ldim_blk_col*i+j]);
		}
		pr_info("\n");
		udelay(10000);
	}
}
static void ldim_get_matrix_info_6(void)
{
	unsigned int i, j;
	unsigned int local_ldim_max[LD_BLKREGNUM] = {0};

	LDIMPR("%s:\n", __func__);
	memcpy(&local_ldim_max[0], &global_ldim_max[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned int));

	for (i = 0; i < ldim_blk_row; i++) {
		for (j = 0; j < ldim_blk_col; j++) {
			pr_info("(%d,%d,%d)\t", local_ldim_max[j +
				i*ldim_blk_col]&0x3ff,
				(local_ldim_max[j + i*10]>>10)&0x3ff,
				(local_ldim_max[j + i*10]>>20)&0x3ff);
			if ((j+1)%8 == 0) {
				pr_info("\n");
				udelay(10000);
			}
		}
	}
}

static void ldim_get_matrix(unsigned int *data, unsigned int reg_sel)
{
	/* gMatrix_LUT: s12*100 */
	if (reg_sel == 0)
		LDIM_RD_BASE_LUT(REG_LD_BLK_VIDX_BASE, data , 16, 32);
	else if (reg_sel == 2)
		ldim_print_debug();
	else if (reg_sel == 3)
		ldim_get_matrix_info_2();
	else if (reg_sel == 4)
		ldim_get_matrix_info_3();
	else if (reg_sel == 5)
		ldim_get_matrix_info_4();
	else if (reg_sel == 6)
		ldim_get_matrix_info_5();
	else if (reg_sel == 7)
		ldim_get_matrix_info_6();
	else if (reg_sel == REG_LD_LUT_HDG_BASE)
		LDIM_RD_BASE_LUT_2(REG_LD_LUT_HDG_BASE, data , 10, 32);
	else if (reg_sel == REG_LD_LUT_VHK_BASE)
		LDIM_RD_BASE_LUT_2(REG_LD_LUT_VHK_BASE, data , 10, 32);
	else if (reg_sel == REG_LD_LUT_VDG_BASE)
		LDIM_RD_BASE_LUT_2(REG_LD_LUT_VDG_BASE, data , 10, 32);
}

static void ldim_get_matrix_info(void)
{
	unsigned int i, j;
	unsigned short local_ldim_matrix_t[LD_BLKREGNUM] = {0};
	unsigned short local_ldim_matrix_spi_t[LD_BLKREGNUM] = {0};

	memcpy(&local_ldim_matrix_t[0], &local_ldim_matrix[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned short));
	memcpy(&local_ldim_matrix_spi_t[0],
		&ldim_driver.ldim_matrix_buf[0],
		ldim_blk_col*ldim_blk_row*sizeof(unsigned short));
	/*printk("%s and spi info:\n", __func__);*/
	LDIMPR("%s and spi info:\n", __func__);
	for (i = 0; i < ldim_blk_row; i++) {
		for (j = 0; j < ldim_blk_col; j++) {
			pr_info("0x%x\t", local_ldim_matrix_t
				[ldim_blk_col*i+j]);
		}
		pr_info("\n");
		udelay(10000);
	}
	LDIMPR("%s: transfer_matrix:\n", __func__);
	for (i = 0; i < ldim_blk_row; i++) {
		for (j = 0; j < ldim_blk_col; j++) {
			pr_info("0x%x\t", local_ldim_matrix_spi_t
				[ldim_blk_col*i+j]);
		}
		pr_info("\n");
		udelay(10000);
	}
	pr_info("\n");

	/*printk("ldim_stts_start_time = %d, ldim_stts_end_time = %d, :\n",);*/
	/*pr_info("ldim_stts_start_time = %ld,ldim_stts_end_time = %ld,
		ldim_stts_time= %ld \n", ldim_stts_start_time,
		ldim_stts_end_time, ldim_stts_time);
	pr_info("ld_on_vs_start_time = %ld,ld_on_vs_end_time = %ld,
		ld_on_vs_time= %ld \n", ld_on_vs_start_time,
		ld_on_vs_end_time, ld_on_vs_time);
	pr_info("ld_fw_alg_frm_start_time = %ld,ld_fw_alg_frm_end_time = %ld,
		ld_fw_alg_frm_time= %ld \n", ld_fw_alg_frm_start_time,
		ld_fw_alg_frm_end_time, ld_fw_alg_frm_time);*/
}

static void ldim_set_matrix(unsigned int *data, unsigned int reg_sel,
		unsigned int cnt)
{
	/* gMatrix_LUT: s12*100 */
	if (reg_sel == 0)
		LDIM_WR_BASE_LUT(REG_LD_BLK_VIDX_BASE, data , 16, 32);
	else if (reg_sel == 2)
		LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_NEGPOS_BASE, data , 16, 32);
	else if (reg_sel == 3)
		LDIM_WR_BASE_LUT(REG_LD_LUT_VHO_NEGPOS_BASE, data , 16, 4);
	else if (reg_sel == REG_LD_LUT_HDG_BASE)
		LDIM_WR_BASE_LUT(REG_LD_LUT_HDG_BASE, data , 16, cnt);
	else if (reg_sel == REG_LD_LUT_VHK_BASE)
		LDIM_WR_BASE_LUT(REG_LD_LUT_VHK_BASE, data , 16, cnt);
	else if (reg_sel == REG_LD_LUT_VDG_BASE)
		LDIM_WR_BASE_LUT(REG_LD_LUT_VDG_BASE, data , 16, cnt);
}

static void ldim_func_ctrl(int status)
{
	if (status) {
		/*ldim_remap_ctrl(ldim_matrix_update_en);*/
		/* enable other flag */
		ldim_top_en = 1;
		ldim_hist_en = 1;
		ldim_alg_en = 1;
		/* enable update */
		ldim_avg_update_en = 1;
		/*ldim_matrix_update_en = 1;*/
	} else {
		/* disable update */
		ldim_avg_update_en = 0;
		/*ldim_matrix_update_en = 0;*/
		/* disable other flag */
		ldim_top_en = 0;
		ldim_hist_en = 0;
		ldim_alg_en = 0;
		/* disable remap */
		/*ldim_remap_ctrl(0);*/

		/* refresh system brightness */
		ldim_level_update = 1;
	}
}

static int ldim_on_init(void)
{
	int ret = 0;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	LDIMPR("%s\n", __func__);

	/* init ldim */
	ldim_stts_initial(ldim_config.hsize, ldim_config.vsize,
		ldim_blk_row, ldim_blk_col);
	LDIM_Initial(ldim_config.hsize, ldim_config.vsize,
		ldim_blk_row, ldim_blk_col, ldim_config.bl_mode, 1, 0);

	ldim_func_ctrl(0); /* default disable ldim function */

	if (ldim_driver.pinmux_ctrl)
		ldim_driver.pinmux_ctrl(ldim_drv->ldev_conf->pinmux_name, 1);
	ldim_on_flag = 1;
	ldim_level_update = 1;

	return ret;
}

static int ldim_power_on(void)
{
	int ret = 0;

	LDIMPR("%s\n", __func__);

	ldim_func_ctrl(ldim_func_en);

	if (ldim_driver.device_power_on)
		ldim_driver.device_power_on();
	ldim_on_flag = 1;
	ldim_level_update = 1;

	return ret;
}
static int ldim_power_off(void)
{
	int ret = 0;

	LDIMPR("%s\n", __func__);

	ldim_on_flag = 0;
	if (ldim_driver.device_power_off)
		ldim_driver.device_power_off();

	ldim_func_ctrl(0);

	return ret;
}

static int ldim_set_level(unsigned int level)
{
	int ret = 0;
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();
	unsigned int level_max, level_min;

	ldim_brightness_level = level;
	level_max = bl_drv->bconf->level_max;
	level_min = bl_drv->bconf->level_min;

	level = ((level - level_min) * (LD_DATA_MAX - ldim_data_min)) /
		(level_max - level_min) + ldim_data_min;
	level &= 0xfff;
	litgain = (unsigned long)level;
	ldim_level_update = 1;

	return ret;
}

static void ldim_test_ctrl(int flag)
{
	if (flag) /* when enable lcd bist pattern, bypass ldim function */
		ldim_func_bypass = 1;
	else
		ldim_func_bypass = 0;
	LDIMPR("%s: ldim_func_bypass = %d\n", __func__, ldim_func_bypass);
}

static struct aml_ldim_driver_s ldim_driver = {
	.valid_flag = 0, /* default invalid, active when bl_ctrl_method=ldim */
	.dev_index = 0,
	.static_pic_flag = 0,
	.ldev_conf = NULL,
	.ldim_matrix_buf = NULL,
	.init = ldim_on_init,
	.power_on = ldim_power_on,
	.power_off = ldim_power_off,
	.set_level = ldim_set_level,
	.test_ctrl = ldim_test_ctrl,
	.config_print = NULL,
	.pinmux_ctrl = NULL,
	.pwm_vs_update = NULL,
	.device_power_on = NULL,
	.device_power_off = NULL,
	.device_bri_update = NULL,
	.device_bri_check = NULL,
};

struct aml_ldim_driver_s *aml_ldim_get_driver(void)
{
	return &ldim_driver;
}

static ssize_t ldim_attr_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
	len += sprintf(buf+len,
	"\necho histgram_ldim > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo ldim_init 1920 1080 8 2 0 1 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo ldim_matrix_get 0/1/2/3 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo ldim_enable > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo ldim_disable > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo ldim_info > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo test_mode 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo test_set 0 0xfff > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo test_set_all 0xfff > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo test_get > /sys/class/aml_ldim/attr\n");
#if 0
	len += sprintf(buf+len,
	"echo set_threshold 1600 32 > /sys/class/aml_ldim/attr\n");
#endif
#if 1
	len += sprintf(buf+len,
	"echo fw_LD_ThSF_l 1600 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo fw_LD_ThTF_l 32 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo cal_cur_en 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo transmit_gain 1 > /sys/class/aml_ldim/attr\n");
	/* len += sprintf(buf+len,
	"echo slp_gain 28 > /sys/class/aml_ldim/attr\n");*/

	len += sprintf(buf+len,
	"echo rgb_base 128 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo Dbprint_lv 128 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo avg_gain_sf_l 128 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo dif_gain_sf_l 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo Debug 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo LPF 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo lpf_gain 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo lpf_res 0 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo Sf_bypass 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo Boost_light_bypass 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo Lpf_bypass 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo Tf_bypass 0 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo Ld_remap_bypass 0 > /sys/class/aml_ldim/attr\n");


#endif
	len += sprintf(buf+len,
	"echo litgain 4096 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo boost_gain 128 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo avg_gain 2048 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo curve_0 512 4 3712 29 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_1 512 4 3712 29 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_2 512 4 3712 29 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_3 512 4 3712 29 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo curve_4 546 4 3823 29 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_5 546 4 3823 29 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_6 585 4 3647 25 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_7 630 4 3455 22 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo curve_8 745 4 3135 17 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_9 819 4 3007 15 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_10 910 4 2879 13 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_11 1170 4 2623 28 > /sys/class/aml_ldim/attr\n");

	len += sprintf(buf+len,
	"echo curve_12 512 4 3750 28 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_13 512 4 3800 27 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_14 512 4 4000 26 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo curve_15 512 4 4055 25 > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf+len,
	"echo info > /sys/class/aml_ldim/attr\n");

	return len;
}
static ssize_t ldim_attr_store(struct class *cla,
	struct class_attribute *attr, const char *buf, size_t len)
{
	unsigned int n = 0;
   /* unsigned char ret=0; */
	char *buf_orig, *ps, *token;
	char *parm[47] = {NULL};
	char str[3] = {' ', '\n', '\0'};
	unsigned int size;
	int i;

	unsigned long pic_h, pic_v, blk_vnum, blk_hnum, hist_row, hist_col;
	unsigned long backlit_mod, ldim_bl_en, ldim_hvcnt_bypass;
	unsigned long val1, bin1, val2, bin2;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (!strcmp(parm[0], "histgram_ldim")) {
		ldim_dump_histgram();
	} else if (!strcmp(parm[0], "ldim_irq_enable")) {
		enable_irq(ldim_irq);
	} else if (!strcmp(parm[0], "ldim_irq_disable")) {
		disable_irq_nosync(ldim_irq);
	} else if (!strcmp(parm[0], "ldim_init")) {
		if (parm[7] != NULL) {
			if (kstrtoul(parm[1], 10, &pic_h) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &pic_v) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &blk_vnum) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &blk_hnum) < 0)
				return -EINVAL;
			if (kstrtoul(parm[5], 10, &backlit_mod) < 0)
				return -EINVAL;
			if (kstrtoul(parm[6], 10, &ldim_bl_en) < 0)
				return -EINVAL;
			if (kstrtoul(parm[7], 10, &ldim_hvcnt_bypass) < 0)
				return -EINVAL;
		}
		pr_info("****ldim init param:%lu,%lu,%lu,%lu,%lu,%lu,%lu*********\n",
			pic_h, pic_v, blk_vnum, blk_hnum,
			backlit_mod, ldim_bl_en, ldim_hvcnt_bypass);
		ldim_blk_row = blk_vnum;
		ldim_blk_col = blk_hnum;
		ldim_config.bl_mode = (unsigned char)backlit_mod;
		LDIM_Initial(pic_h, pic_v, blk_vnum, blk_hnum,
			backlit_mod, ldim_bl_en, ldim_hvcnt_bypass);
		pr_info("**************ldim init ok*************\n");
	} else if (!strcmp(parm[0], "ldim_stts_init")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &pic_h) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &pic_v) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &hist_row) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &hist_col) < 0)
				return -EINVAL;
		}
		pr_info("****ldim init param:%lu,%lu,%lu,%lu*********\n",
			pic_h, pic_v, hist_row, hist_col);
		ldim_hist_row = hist_row;
		ldim_hist_col = hist_col;
		ldim_stts_initial(pic_h, pic_v, hist_row, hist_col);
		pr_info("**************ldim stts init ok*************\n");
	} else if (!strcmp(parm[0], "remap")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
		}
		ldim_matrix_update_en = (unsigned int)val1;
		/*ldim_remap_ctrl(ldim_matrix_update_en);*/
		LDIMPR("ldim_remap_en: %d\n", ldim_matrix_update_en);
	} else if (!strcmp(parm[0], "remap_get")) {
		LDIMPR("ldim_remap_en: %d\n", ldim_matrix_update_en);
	} else if (!strcmp(parm[0], "ldim_matrix_get")) {
		unsigned int data[32] = {0};
		unsigned int k, g;
		unsigned long reg_sel;
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &reg_sel) < 0)
				return -EINVAL;
		}
		ldim_get_matrix(&data[0], reg_sel);
		if ((reg_sel == 0) || (reg_sel == 1)) {
			pr_info("**************ldim matrix info start*************\n");
			for (k = 0; k < 4; k++) {
				for (g = 0; g < 8; g++)
					/*pr_info("%d\t", data[8*k+g]);*/
					LDIMPR("%d\t", data[8*k+g]);
					pr_info("\n");
			}
			pr_info("**************ldim matrix info end*************\n");
		}
	} else if (!strcmp(parm[0], "ldim_matrix_set")) {
		unsigned int data_set[32] = {0};
		unsigned long reg_sel_1, k1, cnt1;
		unsigned long temp_set[32] = {0};
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &reg_sel_1) < 0)
				return -EINVAL;
		}
		if (parm[2] != NULL) {
			if (kstrtoul(parm[1], 10, &cnt1) < 0)
				return -EINVAL;
		}
		for (k1 = 0; k1 < cnt1; k1++) {
			if (parm[k1+2] != NULL) {
				temp_set[k1] =
					kstrtoul(parm[k1+2], 10, &temp_set[k1]);
				data_set[k1] = (unsigned int)temp_set[k1];
			}
		}
		ldim_set_matrix(&data_set[0], (unsigned int)reg_sel_1, cnt1);
		pr_info("**************ldim matrix set over*************\n");
	} else if (!strcmp(parm[0], "ldim_matrix_info")) {
		ldim_get_matrix_info();
		pr_info("**************ldim matrix info over*************\n");
	} else if (!strcmp(parm[0], "ldim_enable")) {
		ldim_func_en = 1;
		ldim_func_ctrl(1);
		pr_info("**************ldim enable ok*************\n");
	} else if (!strcmp(parm[0], "ldim_disable")) {
		ldim_func_en = 0;
		ldim_func_ctrl(0);
		pr_info("**************ldim disable ok*************\n");
	} else if (!strcmp(parm[0], "data_min")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
		}
		ldim_data_min = (unsigned int)val1;
		ldim_set_level(ldim_brightness_level);
		pr_info("**********ldim brightness data_min update*********\n");
	} else if (!strcmp(parm[0], "ldim_info")) {
		pr_info("ldim_on_flag          = %d\n"
			"ldim_func_en          = %d\n"
			"ldim_test_en          = %d\n"
			"ldim_avg_update_en    = %d\n"
			"ldim_matrix_update_en = %d\n"
			"ldim_alg_en           = %d\n"
			"ldim_top_en           = %d\n"
			"ldim_hist_en          = %d\n"
			"ldim_data_min         = %d\n"
			"ldim_data_max         = %d\n",
			ldim_on_flag, ldim_func_en, ldim_test_en,
			ldim_avg_update_en, ldim_matrix_update_en,
			ldim_alg_en, ldim_top_en, ldim_hist_en,
			ldim_data_min, LD_DATA_MAX);
		pr_info("nPRM.reg_LD_BLK_Hnum   = %d\n"
			"nPRM.reg_LD_BLK_Vnum   = %d\n"
			"nPRM.reg_LD_pic_RowMax = %d\n"
			"nPRM.reg_LD_pic_ColMax = %d\n",
			nPRM.reg_LD_BLK_Hnum, nPRM.reg_LD_BLK_Vnum,
			nPRM.reg_LD_pic_RowMax, nPRM.reg_LD_pic_ColMax);
	} else if (!strcmp(parm[0], "test_mode")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
		}
		ldim_test_en = (unsigned int)val1;
		/*if (ldim_test_en)
			ldim_remap_ctrl(0);
		else
			ldim_remap_ctrl(ldim_matrix_update_en);*/
		LDIMPR("test_mode: %d\n", ldim_test_en);
	} else if (!strcmp(parm[0], "test_set")) {
		if (parm[2] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 16, &val2) < 0)
				return -EINVAL;
		}
		size = ldim_blk_row * ldim_blk_col;
		if (val1 < size) {
			ldim_test_matrix[val1] = (unsigned short)val2;
			LDIMPR("set test_matrix[%d] = 0x%03x\n",
				(unsigned int)val1, (unsigned int)val2);
		} else {
			LDIMERR("invalid index for test_matrix: %d\n",
				(unsigned int)val1);
		}
	} else if (!strcmp(parm[0], "test_set_all")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 16, &val1) < 0)
				return -EINVAL;
		}
		for (i = 0; i < LD_BLKREGNUM; i++)
			ldim_test_matrix[i] = (unsigned short)val1;
		LDIMPR("set all test_matrix to 0x%03x\n", (unsigned int)val1);
	} else if (!strcmp(parm[0], "test_get")) {
		LDIMPR("get test_mode: %d, test_matrix:\n", ldim_test_en);
		size = ldim_blk_row * ldim_blk_col;
		for (i = 0; i < size; i++)
			pr_info("0x%03x\t", ldim_test_matrix[i]);
		pr_info("\n");
	} else if (!strcmp(parm[0], "rs")) {
		unsigned long reg_addr, reg_val;
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 16, &reg_addr) < 0)
				return -EINVAL;
		}
		reg_val = LDIM_RD_32Bits(reg_addr);
		pr_info("reg_addr:0x%x=0x%x\n",
			(unsigned int)reg_addr, (unsigned int)reg_val);
	} else if (!strcmp(parm[0], "ws")) {
		unsigned long reg_addr, reg_val;
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 16, &reg_addr) < 0)
				return -EINVAL;
		}
		if (parm[2] != NULL) {
			if (kstrtoul(parm[2], 16, &reg_val) < 0)
				return -EINVAL;
		}
		LDIM_WR_32Bits(reg_addr, reg_val);
		pr_info("reg_addr:0x%x=0x%x\n",
			(unsigned int)reg_addr, (unsigned int)reg_val);
	} else if (!strcmp(parm[0], "update_matrix")) {
		unsigned long mode;
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &mode) < 0)
				return -EINVAL;
		}
		ldim_update_matrix(mode);
		pr_info("mode:%d\n", (unsigned int)mode);
	}
#if 1
	else if (!strcmp(parm[0], "fw_LD_ThSF_l")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &fw_LD_ThSF_l) < 0)
				return -EINVAL;
		}
		pr_info("set fw_LD_ThSF_l=%ld\n", fw_LD_ThSF_l);
	} else if (!strcmp(parm[0], "fw_LD_ThTF_l")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &fw_LD_ThTF_l) < 0)
				return -EINVAL;
		}
		pr_info("set fw_LD_ThTF_l=%ld\n", fw_LD_ThTF_l);
	} else if (!strcmp(parm[0], "cal_cur_en")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &cal_cur_en) < 0)
				return -EINVAL;
		}
		pr_info("set cal_cur_en=%ld\n", cal_cur_en);
	} else if (!strcmp(parm[0], "transmit_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &transmit_gain) < 0)
				return -EINVAL;
		}
		pr_info("set transmit_gain=%ld\n", transmit_gain);
	} else if (!strcmp(parm[0], "rgb_base")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &rgb_base) < 0)
				return -EINVAL;
		}
		pr_info("set rgb_base=%ld\n", rgb_base);
	} else if (!strcmp(parm[0], "avg_gain_sf_l")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &avg_gain_sf_l) < 0)
				return -EINVAL;
		}
		pr_info("set avg_gain_sf_l=%ld\n", avg_gain_sf_l);
	} else if (!strcmp(parm[0], "dif_gain_sf_l")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &dif_gain_sf_l) < 0)
				return -EINVAL;
		}
		pr_info("set dif_gain_sf_l=%ld\n", dif_gain_sf_l);
	} else if (!strcmp(parm[0], "Debug")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &Debug) < 0)
				return -EINVAL;
		}
		pr_info("set Debug=%ld\n", Debug);
	}	else if (!strcmp(parm[0], "LPF")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &LPF) < 0)
				return -EINVAL;
		}
		pr_info("set LPF=%ld\n", LPF);
	} else if (!strcmp(parm[0], "lpf_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &lpf_gain) < 0)
				return -EINVAL;
		}
		pr_info("set lpf_gain=%ld\n", lpf_gain);
	} else if (!strcmp(parm[0], "lpf_res")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &lpf_res) < 0)
				return -EINVAL;
		}
		pr_info("set lpf_res=%ld\n", lpf_res);
	} else if (!strcmp(parm[0], "Sf_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &Sf_bypass) < 0)
				return -EINVAL;
		}
		pr_info("set Sf_bypass=%ld\n", Sf_bypass);
	} else if (!strcmp(parm[0], "Boost_light_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &Boost_light_bypass) < 0)
				return -EINVAL;
		}
		pr_info("set Boost_light_bypass=%ld\n", Boost_light_bypass);
	} else if (!strcmp(parm[0], "Lpf_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &Lpf_bypass) < 0)
				return -EINVAL;
		}
		pr_info("set Lpf_bypass=%ld\n", Lpf_bypass);
	} else if (!strcmp(parm[0], "Tf_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &Tf_bypass) < 0)
				return -EINVAL;
		}
		pr_info("set Tf_bypass=%ld\n", Tf_bypass);
	} else if (!strcmp(parm[0], "Ld_remap_bypass")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &Ld_remap_bypass) < 0)
				return -EINVAL;
		}
		pr_info("set Ld_remap_bypass=%ld\n", Ld_remap_bypass);
	} else if (!strcmp(parm[0], "slp_gain")) {
		unsigned int slop_gain;
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
		}
		slop_gain = (unsigned int)val1;
		ldim_bl_remap_curve(slop_gain);
		ldim_bl_remap_curve_print();
		pr_info("set slp_gain=%d\n", slop_gain);
	} else if (!strcmp(parm[0], "incr_con_en")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
		}
		incr_con_en = (unsigned int)val1;
		pr_info("set incr_con_en=%d\n", incr_con_en);
	} else if (!strcmp(parm[0], "ov_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
		}
		ov_gain = (unsigned int)val1;
		pr_info("set ov_gain=%d\n", ov_gain);
	} else if (!strcmp(parm[0], "incr_dif_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
		}
		incr_dif_gain = (unsigned int)val1;
		pr_info("set incr_dif_gain=%d\n", incr_dif_gain);
	} else if (!strcmp(parm[0], "logo_en")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
		}
		logo_en = (unsigned int)val1;
		pr_info("set logo_en=%d\n", logo_en);
	} else if (!strcmp(parm[0], "test_static_pic")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
		}
		test_static_pic = (unsigned int)val1;
		pr_info("set test_static_pic=%d\n", test_static_pic);
	}
#endif
	else if (!strcmp(parm[0], "litgain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &litgain) < 0)
				return -EINVAL;
		}
		pr_info("set litgain=%ld\n", litgain);
	} else if (!strcmp(parm[0], "boost_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &boost_gain) < 0)
				return -EINVAL;
		}
		pr_info("set boost_gain=%ld\n", boost_gain);
	} else if (!strcmp(parm[0], "Dbprint_lv")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &Dbprint_lv) < 0)
				return -EINVAL;
		}
		pr_info("set Dbprint_lv=%ld\n", Dbprint_lv);
	} else if (!strcmp(parm[0], "avg_gain")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 10, &avg_gain) < 0)
				return -EINVAL;
		}
		pr_info("set avg_gain=%ld\n", avg_gain);
	} else if (!strcmp(parm[0], "vs_time_record")) {
		if (parm[1] != NULL) {
			if (kstrtoul(parm[1], 16, &vs_time_record) < 0)
				return -EINVAL;
		}
	} else if (!strcmp(parm[0], "curve_0")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_0 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[0] = val1;
		nPRM.bin_1[0] = bin1;
		nPRM.val_2[0] = val2;
		nPRM.bin_2[0] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[0] = val1;
		nPRM.bin_1[0] = bin1;
		nPRM.val_2[0] = val2;
		nPRM.bin_2[0] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_0 ok*************\n");
	} else if (!strcmp(parm[0], "curve_1")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_1 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[1] = val1;
		nPRM.bin_1[1] = bin1;
		nPRM.val_2[1] = val2;
		nPRM.bin_2[1] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[1] = val1;
		nPRM.bin_1[1] = bin1;
		nPRM.val_2[1] = val2;
		nPRM.bin_2[1] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_1 ok*************\n");
	} else if (!strcmp(parm[0], "curve_2")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_2 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[2] = val1;
		nPRM.bin_1[2] = bin1;
		nPRM.val_2[2] = val2;
		nPRM.bin_2[2] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[2] = val1;
		nPRM.bin_1[2] = bin1;
		nPRM.val_2[2] = val2;
		nPRM.bin_2[2] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_2 ok*************\n");
	} else if (!strcmp(parm[0], "curve_3")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_3 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[3] = val1;
		nPRM.bin_1[3] = bin1;
		nPRM.val_2[3] = val2;
		nPRM.bin_2[3] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[3] = val1;
		nPRM.bin_1[3] = bin1;
		nPRM.val_2[3] = val2;
		nPRM.bin_2[3] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_3 ok*************\n");
	} else if (!strcmp(parm[0], "curve_4")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_4 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[4] = val1;
		nPRM.bin_1[4] = bin1;
		nPRM.val_2[4] = val2;
		nPRM.bin_2[4] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[4] = val1;
		nPRM.bin_1[4] = bin1;
		nPRM.val_2[4] = val2;
		nPRM.bin_2[4] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_4 ok*************\n");
	} else if (!strcmp(parm[0], "curve_5")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_5 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[5] = val1;
		nPRM.bin_1[5] = bin1;
		nPRM.val_2[5] = val2;
		nPRM.bin_2[5] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[5] = val1;
		nPRM.bin_1[5] = bin1;
		nPRM.val_2[5] = val2;
		nPRM.bin_2[5] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_5 ok*************\n");
	} else if (!strcmp(parm[0], "curve_6")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_6 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[6] = val1;
		nPRM.bin_1[6] = bin1;
		nPRM.val_2[6] = val2;
		nPRM.bin_2[6] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[6] = val1;
		nPRM.bin_1[6] = bin1;
		nPRM.val_2[6] = val2;
		nPRM.bin_2[6] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_6 ok*************\n");
	} else if (!strcmp(parm[0], "curve_7")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_7 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[7] = val1;
		nPRM.bin_1[7] = bin1;
		nPRM.val_2[7] = val2;
		nPRM.bin_2[7] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[7] = val1;
		nPRM.bin_1[7] = bin1;
		nPRM.val_2[7] = val2;
		nPRM.bin_2[7] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_7 ok*************\n");
	} else if (!strcmp(parm[0], "curve_8")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_8 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[8] = val1;
		nPRM.bin_1[8] = bin1;
		nPRM.val_2[8] = val2;
		nPRM.bin_2[8] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[8] = val1;
		nPRM.bin_1[8] = bin1;
		nPRM.val_2[8] = val2;
		nPRM.bin_2[8] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_8 ok*************\n");
	} else if (!strcmp(parm[0], "curve_9")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_9 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[9] = val1;
		nPRM.bin_1[9] = bin1;
		nPRM.val_2[9] = val2;
		nPRM.bin_2[9] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[9] = val1;
		nPRM.bin_1[9] = bin1;
		nPRM.val_2[9] = val2;
		nPRM.bin_2[9] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_9 ok*************\n");
	} else if (!strcmp(parm[0], "curve_10")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_10 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[10] = val1;
		nPRM.bin_1[10] = bin1;
		nPRM.val_2[10] = val2;
		nPRM.bin_2[10] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[10] = val1;
		nPRM.bin_1[10] = bin1;
		nPRM.val_2[10] = val2;
		nPRM.bin_2[10] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_10 ok*************\n");
	} else if (!strcmp(parm[0], "curve_11")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_11 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[11] = val1;
		nPRM.bin_1[11] = bin1;
		nPRM.val_2[11] = val2;
		nPRM.bin_2[11] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[11] = val1;
		nPRM.bin_1[11] = bin1;
		nPRM.val_2[11] = val2;
		nPRM.bin_2[11] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_11 ok*************\n");
	} else if (!strcmp(parm[0], "curve_12")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_12 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[12] = val1;
		nPRM.bin_1[12] = bin1;
		nPRM.val_2[12] = val2;
		nPRM.bin_2[12] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[12] = val1;
		nPRM.bin_1[12] = bin1;
		nPRM.val_2[12] = val2;
		nPRM.bin_2[12] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_12 ok*************\n");
	} else if (!strcmp(parm[0], "curve_13")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_13 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[13] = val1;
		nPRM.bin_1[13] = bin1;
		nPRM.val_2[13] = val2;
		nPRM.bin_2[13] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[13] = val1;
		nPRM.bin_1[13] = bin1;
		nPRM.val_2[13] = val2;
		nPRM.bin_2[13] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_13 ok*************\n");
	} else if (!strcmp(parm[0], "curve_14")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_14 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[14] = val1;
		nPRM.bin_1[14] = bin1;
		nPRM.val_2[14] = val2;
		nPRM.bin_2[14] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[14] = val1;
		nPRM.bin_1[14] = bin1;
		nPRM.val_2[14] = val2;
		nPRM.bin_2[14] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_14 ok*************\n");
	} else if (!strcmp(parm[0], "curve_15")) {
		if (parm[4] != NULL) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[2], 10, &bin1) < 0)
				return -EINVAL;
			if (kstrtoul(parm[3], 10, &val2) < 0)
				return -EINVAL;
			if (kstrtoul(parm[4], 10, &bin2) < 0)
				return -EINVAL;
		}
		pr_info("****ldim curve_15 param:%lu,%lu,%lu,%lu*********\n",
						val1, bin1, val2, bin2);
		nPRM.val_1[15] = val1;
		nPRM.bin_1[15] = bin1;
		nPRM.val_2[15] = val2;
		nPRM.bin_2[15] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		nPRM.val_1[15] = val1;
		nPRM.bin_1[15] = bin1;
		nPRM.val_2[15] = val2;
		nPRM.bin_2[15] = bin2;
		LDIM_Initial(3840, 2160, 16, 24, 2, 1, 0);
		pr_info("**************ldim curve_15 ok*************\n");
	} else if (!strcmp(parm[0], "info")) {
		ldim_driver.config_print();
		pr_info("ldim_on_flag          = %d\n"
			"ldim_func_en          = %d\n"
			"ldim_remap_en         = %d\n"
			"ldim_test_en          = %d\n\n",
			ldim_on_flag, ldim_func_en,
			ldim_matrix_update_en, ldim_test_en);
	} else
		pr_info("no support cmd!!!\n");

	kfree(buf_orig);
	return len;
}

static ssize_t ldim_func_en_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "%d\n", ldim_func_en);

	return ret;
}

static ssize_t ldim_func_en_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;
	int ret = 0;

	ret = sscanf(buf, "%d", &val);
	LDIMPR("local diming function: %s\n", (val ? "enable" : "disable"));
	ldim_func_en = val ? 1 : 0;
	ldim_func_ctrl(ldim_func_en);

	return count;
}

static struct class_attribute aml_ldim_class_attrs[] = {
	__ATTR(attr, S_IWUGO | S_IRUGO, ldim_attr_show, ldim_attr_store),
	__ATTR(func_en, S_IWUGO | S_IRUGO,
		ldim_func_en_show, ldim_func_en_store),
	__ATTR_NULL,
};

static int aml_ldim_get_config(struct ldim_config_s *ldconf,
		struct device *dev)
{
	struct vinfo_s *vinfo = get_current_vinfo();
	struct aml_bl_drv_s *bl_drv = aml_bl_get_driver();
	struct device_node *child;
	char propname[20];
	unsigned int *para;
	int i;
	int ret;

	if (dev->of_node == NULL) {
		LDIMERR("dev of_node is null\n");
		return -1;
	}
	sprintf(propname, "backlight_%d", bl_drv->index);
	child = of_get_child_by_name(dev->of_node, propname);
	if (child == NULL) {
		LDIMERR("failed to get %s\n", propname);
		return -1;
	}

	ldconf->hsize = vinfo->width;
	ldconf->vsize = vinfo->height;
	for (i = 0; i < LD_BLKREGNUM; i++)
		ldconf->bl_mapping[i] = (unsigned short)i;

	/* malloc para */
	para = kzalloc(sizeof(unsigned int) * LD_BLKREGNUM, GFP_KERNEL);
	if (para == NULL) {
		LDIMERR("%s: malloc failed\n", __func__);
		return -1;
	}

	/* get row & col from dts */
	ret = of_property_read_u32_array(child, "bl_ldim_region_row_col",
			para, 2);
	if (ret) {
		LDIMERR("failed to get bl_ldim_region_row_col\n");
	} else {
		if ((para[0] * para[1]) > LD_BLKREGNUM) {
			LDIMERR("region row*col(%d*%d) is out of support\n",
				para[0], para[1]);
		} else {
			ldim_blk_row = para[0];
			ldim_blk_col = para[1];
		}
	}
	if (ldim_debug_print) {
		LDIMPR("get region row = %d, col = %d\n",
			ldim_blk_row, ldim_blk_col);
	}

	/* get bl_mode from dts */
	ret = of_property_read_u32(child, "bl_ldim_mode", &para[0]);
	if (ret)
		LDIMERR("failed to get bl_ldim_mode\n");
	else
		ldconf->bl_mode = (unsigned char)para[0];
	if (ldim_debug_print)
		LDIMPR("get bl_mode = %d\n", ldconf->bl_mode);

	/* get bl_mapping_table from dts */
	ret = of_property_read_u32_array(child, "bl_ldim_mapping", para,
			(ldim_blk_row * ldim_blk_col));
	if (ret) {
		LDIMERR("failed to get bl_ldim_mapping, use default\n");
	} else {
		for (i = 0; i < (ldim_blk_row * ldim_blk_col); i++)
			ldconf->bl_mapping[i] = (unsigned short)para[i];
	}

	/* get ldim_dev_index from dts */
	ret = of_property_read_u32(child, "ldim_dev_index", &para[0]);
	if (ret)
		LDIMERR("failed to get ldim_dev_index\n");
	else
		ldim_driver.dev_index = para[0];
	LDIMPR("get ldim_dev_index = %d\n", ldim_driver.dev_index);

	kfree(para);
	return 0;
}

int aml_ldim_probe(struct platform_device *pdev)
{
	unsigned int ret = 0;
	unsigned int i;

	ldim_brightness_level = 0;
	ldim_data_min = LD_DATA_MIN;
	ldim_on_flag = 0;
	ldim_func_en = 0;
	ldim_func_bypass = 0;
	ldim_test_en = 0;
	ldim_bypass = 0;
	ldim_level_update = 0;
	ldim_avg_update_en = 0;
	ldim_matrix_update_en = 0;
	ldim_alg_en = 0;
	ldim_top_en = 0;
	ldim_hist_en = 0;
	memset(ldim_test_matrix, 0, sizeof(unsigned short)*LD_BLKREGNUM);
	avg_gain = LD_DATA_MAX;

	nPRM.val_1 = &val_1[0];
	nPRM.bin_1 = &bin_1[0];
	nPRM.val_2 = &val_2[0];
	nPRM.bin_2 = &bin_2[0];
	aml_ldim_get_config(&ldim_config, &pdev->dev);

	ldim_driver.ldim_matrix_buf = kzalloc(
		(sizeof(unsigned short) * LD_BLKREGNUM), GFP_KERNEL);
	if (ldim_driver.ldim_matrix_buf == NULL) {
		LDIMERR("ldim_driver ldim_matrix_buf malloc error\n");
		return -1;
	}

	ret = alloc_chrdev_region(&aml_ldim_devno, 0, 1, AML_LDIM_DEVICE_NAME);
	if (ret < 0) {
		pr_err("ldim: faild to alloc major number\n");
		ret = -ENODEV;
		goto err;
	}

	aml_ldim_clsp = class_create(THIS_MODULE, "aml_ldim");
	if (IS_ERR(aml_ldim_clsp)) {
		ret = PTR_ERR(aml_ldim_clsp);
		return ret;
	}
	for (i = 0; aml_ldim_class_attrs[i].attr.name; i++) {
		if (class_create_file(aml_ldim_clsp,
				&aml_ldim_class_attrs[i]) < 0)
			goto err1;
	}

	aml_ldim_cdevp = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!aml_ldim_cdevp) {
		pr_err("aml_ldim: failed to allocate memory\n");
		ret = -ENOMEM;
		goto err2;
	}

	/* connect the file operations with cdev */
	cdev_init(aml_ldim_cdevp, &aml_ldim_fops);
	aml_ldim_cdevp->owner = THIS_MODULE;

	/* connect the major/minor number to the cdev */
	ret = cdev_add(aml_ldim_cdevp, aml_ldim_devno, 1);
	if (ret) {
		pr_err("aml_ldim: failed to add device\n");
		goto err3;
	}

	FDat.SF_BL_matrix = kmalloc
		(LD_BLKREGNUM*sizeof(unsigned int), GFP_KERNEL);
	memset(FDat.SF_BL_matrix, 0, LD_BLKREGNUM*sizeof(unsigned int));
	FDat.last_STA1_MaxRGB = kmalloc
		(LD_BLKREGNUM*3*sizeof(unsigned int), GFP_KERNEL);
	memset(FDat.last_STA1_MaxRGB, 0, LD_BLKREGNUM*3*sizeof(unsigned int));
	FDat.TF_BL_matrix = kmalloc
		(LD_BLKREGNUM*sizeof(unsigned int), GFP_KERNEL);
	memset(FDat.TF_BL_matrix, 0, LD_BLKREGNUM*sizeof(unsigned int));
	FDat.TF_BL_matrix_2 = kmalloc
		(LD_BLKREGNUM*sizeof(unsigned int), GFP_KERNEL);
	memset(FDat.TF_BL_matrix_2, 0, LD_BLKREGNUM*sizeof(unsigned int));
	FDat.TF_BL_alpha = kmalloc
		(LD_BLKREGNUM*sizeof(unsigned int), GFP_KERNEL);
	memset(FDat.TF_BL_alpha, 0, LD_BLKREGNUM*sizeof(unsigned int));

	tasklet_init(&ldim_tasklet, ldim_on_vs_spi, (unsigned long)123);
	ldim_read_queue = create_singlethread_workqueue("ldim read");
	if (!ldim_read_queue) {
		LDIMERR("ldim_read_queue create failed\n");
		goto err;
	}
	INIT_WORK(&ldim_read_work, ldim_stts_read_region);

	spin_lock_init(&ldim_isr_lock);
	spin_lock_init(&rdma_ldim_isr_lock);

	ldim_irq = VIU_VSYNC_INT;
	if (request_irq(VIU_VSYNC_INT, (irq_handler_t)&ldim_vsync_isr,
		IRQF_SHARED, "ldim_vsync", (void *)"ldim_vsync")) {
		LDIMERR("request irq ldim_vsync failed\n");
	}
	if (request_irq(RDMA_LDIM_INT, (irq_handler_t)&rdma_ldim_intr,
		IRQF_SHARED, "rdma_ldim", (void *)"rdma_ldim")) {
		LDIMERR("request irq rdma_ldim failed\n");
	}

	ldim_driver.valid_flag = 1;

	LDIMPR("%s ok\n", __func__);
	return 0;
err3:
	kfree(aml_ldim_cdevp);
err2:
	for (i = 0; aml_ldim_class_attrs[i].attr.name; i++)
		class_remove_file(aml_ldim_clsp, &aml_ldim_class_attrs[i]);
	class_destroy(aml_ldim_clsp);
err1:
	unregister_chrdev_region(aml_ldim_devno, 1);
err:
	return ret;

	return -1;
}

int aml_ldim_remove(void)
{
	unsigned int i;
	kfree(FDat.SF_BL_matrix);
	kfree(FDat.TF_BL_matrix);
	kfree(FDat.TF_BL_matrix_2);
	kfree(FDat.last_STA1_MaxRGB);
	kfree(FDat.TF_BL_alpha);
	kfree(ldim_driver.ldim_matrix_buf);

	free_irq(RDMA_LDIM_INT, (void *)"rdma_ldim");
	free_irq(VIU_VSYNC_INT, (void *)"ldim_vsync");
	tasklet_kill(&ldim_tasklet);
	cdev_del(aml_ldim_cdevp);

	kfree(aml_ldim_cdevp);
	for (i = 0; aml_ldim_class_attrs[i].attr.name; i++) {
		class_remove_file(aml_ldim_clsp,
				&aml_ldim_class_attrs[i]);
	}
	class_destroy(aml_ldim_clsp);
	unregister_chrdev_region(aml_ldim_devno, 1);

	LDIMPR("%s ok\n", __func__);
	return 0;
}

