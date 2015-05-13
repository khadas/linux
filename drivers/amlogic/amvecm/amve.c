/*
 * Video Enhancement
 *
 * Author: Lin Xu <lin.xu@amlogic.com>
 *         Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/amvecm/ve.h>
/* #include <linux/amlogic/aml_common.h> */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amvecm/amvecm.h>
#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "amve.h"
#include "amve_gamma_table.h"

#define pr_amve_dbg(fmt, args...)\
	do {\
		if (dnlp_debug)\
			printk("AMVECM: " fmt, ## args);\
	} while (0)\
/* #define pr_amve_error(fmt, args...) */
/* printk(KERN_##(KERN_INFO) "AMVECM: " fmt, ## args) */

#define GAMMA_RETRY        1000

/* 0: Invalid */
/* 1: Valid */
/* 2: Updated in 2D mode */
/* 3: Updated in 3D mode */
unsigned long flags;

/* #if (MESON_CPU_TYPE>=MESON_CPU_TYPE_MESONG9TV) */
#define NEW_DNLP_IN_SHARPNESS 2
#define NEW_DNLP_IN_VPP 1

unsigned int dnlp_sel = NEW_DNLP_IN_VPP;
module_param(dnlp_sel, int, 0664);
MODULE_PARM_DESC(dnlp_sel, "dnlp_sel");
/* #endif */

struct ve_hist_s video_ve_hist;

static unsigned char ve_dnlp_tgt[64];
bool ve_en;
unsigned int ve_dnlp_white_factor;
unsigned int ve_dnlp_rt = 0;
unsigned int ve_dnlp_rl;
unsigned int ve_dnlp_black;
unsigned int ve_dnlp_white;
unsigned int ve_dnlp_luma_sum;
static ulong ve_dnlp_lpf[64], ve_dnlp_reg[16];
unsigned int ve_size;

static bool frame_lock_nosm = 1;
static int ve_dnlp_waist_h = 128;
static int ve_dnlp_waist_l = 128;
static int ve_dnlp_ankle = 16;
static int ve_dnlp_strength = 255;
unsigned int vpp_log[128][10];
struct ve_dnlp_s am_ve_dnlp;
struct ve_dnlp_table_s am_ve_new_dnlp;
static unsigned int lock_range_50hz_fast =  7; /* <= 14 */
static unsigned int lock_range_50hz_slow =  7; /* <= 14 */
static unsigned int lock_range_60hz_fast =  5; /* <=  4 */
static unsigned int lock_range_60hz_slow =  2; /* <= 10 */
struct tcon_gamma_table_s video_gamma_table_r;
struct tcon_gamma_table_s video_gamma_table_g;
struct tcon_gamma_table_s video_gamma_table_b;
struct tcon_gamma_table_s video_gamma_table_r_adj;
struct tcon_gamma_table_s video_gamma_table_g_adj;
struct tcon_gamma_table_s video_gamma_table_b_adj;
struct tcon_rgb_ogo_s     video_rgb_ogo;

#define FLAG_LVDS_FREQ_SW1       (1 <<  6)

module_param(ve_dnlp_waist_h, int, 0664);
MODULE_PARM_DESC(ve_dnlp_waist_h, "ve_dnlp_waist_h");
module_param(ve_dnlp_waist_l, int, 0664);
MODULE_PARM_DESC(ve_dnlp_waist_l, "ve_dnlp_waist_l");
module_param(ve_dnlp_ankle, int, 0664);
MODULE_PARM_DESC(ve_dnlp_ankle, "ve_dnlp_ankle");
module_param(ve_dnlp_strength, int, 0664);
MODULE_PARM_DESC(ve_dnlp_strength, "ve_dnlp_strength");

static int dnlp_respond;
module_param(dnlp_respond, int, 0664);
MODULE_PARM_DESC(dnlp_respond, "dnlp_respond");

static int dnlp_debug;
module_param(dnlp_debug, int, 0664);
MODULE_PARM_DESC(dnlp_debug, "dnlp_debug");

static int ve_dnlp_method;
module_param(ve_dnlp_method, int, 0664);
MODULE_PARM_DESC(ve_dnlp_method, "ve_dnlp_method");

static int ve_dnlp_cliprate = 6;
module_param(ve_dnlp_cliprate, int, 0664);
MODULE_PARM_DESC(ve_dnlp_cliprate, "ve_dnlp_cliprate");

static int ve_dnlp_lowrange = 18;
module_param(ve_dnlp_lowrange, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lowrange, "ve_dnlp_lowrange");

static int ve_dnlp_hghrange = 18;
module_param(ve_dnlp_hghrange, int, 0664);
MODULE_PARM_DESC(ve_dnlp_hghrange, "ve_dnlp_hghrange");

static int ve_dnlp_lowalpha = 24;
module_param(ve_dnlp_lowalpha, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lowalpha, "ve_dnlp_lowalpha");

static int ve_dnlp_midalpha = 24;
module_param(ve_dnlp_midalpha, int, 0664);
MODULE_PARM_DESC(ve_dnlp_midalpha, "ve_dnlp_midalpha");

static int ve_dnlp_hghalpha = 24;
module_param(ve_dnlp_hghalpha, int, 0664);
MODULE_PARM_DESC(ve_dnlp_hghalpha, "ve_dnlp_hghalpha");

module_param(frame_lock_nosm, bool, 0664);
MODULE_PARM_DESC(frame_lock_nosm, "frame_lock_nosm");

static int dnlp_adj_level = 6;
module_param(dnlp_adj_level, int, 0664);
MODULE_PARM_DESC(dnlp_adj_level, "dnlp_adj_level");

static int dnlp_en;/* 0:disabel;1:enable */
module_param(dnlp_en, int, 0664);
MODULE_PARM_DESC(dnlp_en, "\n enable or disable dnlp\n");
static int dnlp_status = 1;/* 0:done;1:todo */

static int frame_lock_freq;
module_param(frame_lock_freq, int, 0664);
MODULE_PARM_DESC(frame_lock_freq, "frame_lock_50");

static int video_rgb_ogo_mode_sw;
module_param(video_rgb_ogo_mode_sw, int, 0664);
MODULE_PARM_DESC(video_rgb_ogo_mode_sw,
		"enable/disable video_rgb_ogo_mode_sw");

static unsigned int assist_cnt;/* ASSIST_SPARE8_REG1; */
static unsigned int assist_cnt2;/* ASSIST_SPARE8_REG2; */

/* *********************************************************************** */
/* *** VPP_FIQ-oriented functions **************************************** */
/* *********************************************************************** */
static void ve_hist_gamma_tgt(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;
	video_ve_hist.sum    = p->hist.luma_sum;
	video_ve_hist.width  = p->hist.width;
	video_ve_hist.height = p->hist.height;
}

static void ve_dnlp_calculate_tgt_ext(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;
	static unsigned int sum_b = 0, sum_c;
	ulong i = 0, j = 0, sum = 0, ave = 0, ankle = 0,
			waist = 0, peak = 0, start = 0;
	ulong qty_h = 0, qty_l = 0, ratio_h = 0, ratio_l = 0;
	ulong div1  = 0, div2  = 0, step_h  = 0, step_l  = 0;
	ulong data[55];
	bool  flag[55], previous_state_high = false;
	/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	unsigned int cnt = assist_cnt;
	/* old historic luma sum */
	sum_b = sum_c;
	sum_c = ve_dnlp_luma_sum;
	/* new historic luma sum */
	ve_dnlp_luma_sum = p->hist.luma_sum;
	pr_amve_dbg("ve_dnlp_luma_sum=%x,sum_b=%x,sum_c=%x\n",
			ve_dnlp_luma_sum, sum_b, sum_c);
	/* picture mode: freeze dnlp curve */
	if (dnlp_respond) {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if (!ve_dnlp_luma_sum)
			return;
	} else {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if ((!ve_dnlp_luma_sum) ||
			/* new luma sum is closed to old one (1 +/- 1/64),
			 * picture mode, freeze curve */
			((ve_dnlp_luma_sum <
					sum_b + (sum_b >> dnlp_adj_level)) &&
			(ve_dnlp_luma_sum >
					sum_b - (sum_b >> dnlp_adj_level))))
			return;
	}
	/* calculate ave (55 times of ave & data[] for accuracy) */
	/* ave    22-bit */
	/* data[] 22-bit */
	ave = 0;
	for (i = 0; i < 55; i++) {
		data[i]  = (ulong)p->hist.gamma[i + 4];
		ave     += data[i];
		data[i] *= 55;
		flag[i]  = false;
	}
	/* calculate ankle */
	/* ankle 22-bit */
	/* waist 22-bit */
	/* qty_h 6-bit */
	ankle = (ave * ve_dnlp_ankle + 128) >> 8;
	/* scan data[] to find out waist pulses */
	qty_h = 0;
	previous_state_high = false;
	for (i = 0; i < 55; i++) {
		if (data[i] >= ankle) {
			/* ankle pulses start */
			if (!previous_state_high) {
				previous_state_high = true;
				start = i;
				peak = 0;
			}
			/* normal maintenance */
			if (peak < data[i])
				peak = data[i];
		} else {
			/* ankle pulses end + 1 */
			if (previous_state_high) {
				previous_state_high = false;
				/* calculate waist of high area pulses */
				if (peak >= ave)
					waist =	((peak - ankle) *
						ve_dnlp_waist_h + 128) >> 8;
				/* calculate waist of high area pulses */
				else
					waist =	((peak - ankle) *
						ve_dnlp_waist_l + 128) >> 8;
				/* find out waist pulses */
				for (j = start; j < i; j++) {
					if (data[j] >= waist) {
						flag[j] = true;
						qty_h++;
					}
				}
			}
		}
	}

	/* calculate ratio_h and ratio_l */
	/* (div2 = 512*H*L times of value for accuracy) */
	/* averaged duty > 1/3 */
	/* qty_l 6-bit */
	/* div1 20-bit */
	/* div2 21-bit */
	/* ratio_h 22-bit */
	/* ratio_l 21-bit */
	qty_l =  55 - qty_h;
	if ((!qty_h) || (!qty_l)) {
		for (i = 5; i <= 58; i++)
			ve_dnlp_tgt[i] = i << 2;
	} else {
		div1  = 256 * qty_h * qty_l;
		div2  = 512 * qty_h * qty_l;
		if (qty_h > 18) {
			/* [1.0 ~ 2.0) */
			ratio_h = div2 + ve_dnlp_strength * qty_l * qty_l;
			 /* [0.5 ~ 1.0] */
			ratio_l = div2 - ve_dnlp_strength * qty_h * qty_l;
		}
		/* averaged duty < 1/3 */
		/* [1.0 ~ 2.0] */
		ratio_h = div2 + (ve_dnlp_strength << 1) * qty_h * qty_l;
		/* (0.5 ~ 1.0] */
		ratio_l = div2 - (ve_dnlp_strength << 1) * qty_h * qty_h;
		/* distribute ratio_h & ratio_l to */
		/* ve_dnlp_tgt[5] ~ ve_dnlp_tgt[58] */
		/* sum 29-bit */
		/* step_h 24-bit */
		/* step_l 23-bit */
		sum = div2 << 4; /* start from 16 */
		step_h = ratio_h << 2;
		step_l = ratio_l << 2;
		for (i = 5; i <= 58; i++) {
			/* high phase */
			if (flag[i - 5])
				sum += step_h;
			/* low  phase */
			else
				sum += step_l;
			ve_dnlp_tgt[i] = (sum + div1) / div2;
		}
		if (cnt) {
			for (i = 0; i < 64; i++)
				pr_amve_dbg(" ve_dnlp_tgte[%ld]=%d\n",
						i, ve_dnlp_tgt[i]);
			/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, 0); */
			assist_cnt = 0;
		}
	}
}

void GetWgtLst(ulong *iHst, ulong tAvg, ulong nLen, ulong alpha)
{
	ulong iMax = 0;
	ulong iMin = 0;
	ulong iPxl = 0;
	ulong iT = 0;
	for (iT = 0; iT < nLen; iT++) {
		iPxl = iHst[iT];
		if (iPxl > tAvg) {
			iMax = iPxl;
			iMin = tAvg;
		} else {
			iMax = tAvg;
			iMin = iPxl;
		}
		if (alpha < 16) {
			iPxl = ((16-alpha)*iMin+8)>>4;
			iPxl += alpha*iMin;
		} else if (alpha < 32) {
			iPxl = (32-alpha)*iMin;
			iPxl += (alpha-16)*iMax;
		} else {
			iPxl = (48-alpha)+4*(alpha-32);
			iPxl *= iMax;
		}
		iPxl = (iPxl+8)>>4;
		iHst[iT] = iPxl < 1 ? 1 : iPxl;
	}
}

static void ve_dnlp_calculate_tgtx(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;
	ulong iHst[64];
	ulong oHst[64];
	static unsigned int sum_b = 0, sum_c;
	ulong i = 0, j = 0, sum = 0, max = 0;
	ulong cLmt = 0, nStp = 0, stp = 0, uLmt = 0;
	long nExc = 0;
	unsigned int cnt = assist_cnt;/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	unsigned int cnt2 = assist_cnt2;/* READ_VPP_REG(ASSIST_SPARE8_REG2); */
	unsigned int clip_rate = ve_dnlp_cliprate; /* 8bit */
	unsigned int low_range = ve_dnlp_lowrange;/* 18; //6bit [0-54] */
	unsigned int hgh_range = ve_dnlp_hghrange;/* 18; //6bit [0-54] */
	unsigned int low_alpha = ve_dnlp_lowalpha;/* 24; //6bit [0--48] */
	unsigned int mid_alpha = ve_dnlp_midalpha;/* 24; //6bit [0--48] */
	unsigned int hgh_alpha = ve_dnlp_hghalpha;/* 24; //6bit [0--48] */
	/* ------------------------------------------------- */
	ulong tAvg = 0;
	ulong nPnt = 0;
	ulong mRng = 0;
	/* old historic luma sum */
	sum_b = sum_c;
	sum_c = ve_dnlp_luma_sum;
	/* new historic luma sum */
	ve_dnlp_luma_sum = p->hist.luma_sum;
	pr_amve_dbg("ve_dnlp_luma_sum=%x,sum_b=%x,sum_c=%x\n",
			ve_dnlp_luma_sum, sum_b, sum_c);
	/* picture mode: freeze dnlp curve */
	if (dnlp_respond) {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if (!ve_dnlp_luma_sum)
			return;
	} else {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if ((!ve_dnlp_luma_sum) ||
			/* new luma sum is closed to old one (1 +/- 1/64), */
			/* picture mode, freeze curve */
			((ve_dnlp_luma_sum <
					sum_b + (sum_b >> dnlp_adj_level)) &&
			(ve_dnlp_luma_sum >
					sum_b - (sum_b >> dnlp_adj_level))))
			return;
	}
	/* 64 bins, max, ave */
	for (i = 0; i < 64; i++) {
		iHst[i] = (ulong)p->hist.gamma[i];
		if (i >= 4 && i <= 58) {
			oHst[i] = iHst[i];
			if (max < iHst[i])
				max = iHst[i];
			sum += iHst[i];
		} else {
			oHst[i] = 0;
		}
	}
	cLmt = (clip_rate*sum)>>8;
	tAvg = sum/55;
	/* invalid histgram: freeze dnlp curve */
	if (max <= 55)
		return;
	/* get 1st 4 points */
	for (i = 4; i <= 58; i++) {
		if (iHst[i] > cLmt)
			nExc += (iHst[i]-cLmt);
	}
	nStp = (nExc+28)/55;
	uLmt = cLmt-nStp;
	if (cnt2) {
		pr_amve_dbg(" ve_dnlp_tgtx:cLmt=%ld,nStp=%ld,uLmt=%ld\n",
				cLmt, nStp, uLmt);
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG2, 0); */
		assist_cnt2 = 0;
	}
	if (clip_rate <= 4 || tAvg <= 2) {
		cLmt = (sum+28)/55;
		sum = cLmt*55;
		for (i = 4; i <= 58; i++)
			oHst[i] = cLmt;
	} else if (nStp != 0) {
		for (i = 4; i <= 58; i++) {
			if (iHst[i] >= cLmt)
				oHst[i] = cLmt;
			else {
				if (iHst[i] > uLmt) {
					oHst[i] = cLmt;
					nExc -= cLmt-iHst[i];
				} else {
					oHst[i] = iHst[i]+nStp;
					nExc -= nStp;
				}
				if (nExc < 0)
					nExc = 0;
			}
		}
		j = 4;
		while (nExc > 0) {
			if (nExc >= 55) {
				nStp = 1;
				stp = nExc/55;
			} else {
				nStp = 55/nExc;
				stp = 1;
			}
			for (i = j; i <= 58; i += nStp) {
				if (oHst[i] < cLmt) {
					oHst[i] += stp;
					nExc -= stp;
				}
				if (nExc <= 0)
					break;
			}
			j += 1;
			if (j > 58)
				break;
		}
	}
	if (low_range == 0 && hgh_range == 0)
		nPnt = 0;
	else {
		if (low_range == 0 || hgh_range == 0) {
			nPnt = 1;
			mRng = (hgh_range > low_range ? hgh_range : low_range);
		} else if (low_range+hgh_range >= 54) {
			nPnt = 1;
			mRng = (hgh_range < low_range ? hgh_range : low_range);
		} else
			nPnt = 2;
	}
	if (nPnt == 0 && low_alpha >= 16 && low_alpha <= 32) {
		sum = 0;
		for (i = 5; i <= 59; i++) {
			j = oHst[i]*(32-low_alpha)+tAvg*(low_alpha-16);
			j = (j+8)>>4;
			oHst[i] = j;
			sum += j;
		}
	} else if (nPnt == 1) {
		GetWgtLst(oHst+4, tAvg, mRng, low_alpha);
		GetWgtLst(oHst+4+mRng, tAvg, 54-mRng, hgh_alpha);
	} else if (nPnt == 2) {
		mRng = 55-(low_range+hgh_range);
		GetWgtLst(oHst+4, tAvg, low_range, low_alpha);
		GetWgtLst(oHst+4+low_range, tAvg, mRng, mid_alpha);
		GetWgtLst(oHst+4+mRng+low_range, tAvg, hgh_range, hgh_alpha);
	}
	sum = 0;
	for (i = 4; i <= 58; i++) {
		if (oHst[i] > cLmt)
			oHst[i] = cLmt;
		sum += oHst[i];
	}
	nStp = 0;
	/* sum -= oHst[4]; */
	for (i = 5; i <= 59; i++) {
		nStp += oHst[i-1];
		/* nStp += oHst[i]; */
		j = (236-16)*nStp;
		j += (sum>>1);
		j /= sum;
		ve_dnlp_tgt[i] = j + 16;
	}
	if (cnt) {
		for (i = 0; i < 64; i++)
			pr_amve_dbg(" ve_dnlp_tgtx[%ld]=%d\n",
					i, ve_dnlp_tgt[i]);
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, 0); */
		assist_cnt = 0;
	}
	return;
}

static void ve_dnlp_calculate_tgt(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;
	ulong data[5];
	static unsigned int sum_b = 0, sum_c;
	ulong i = 0, j = 0, ave = 0, max = 0, div = 0;
	unsigned int cnt = assist_cnt;/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	/* old historic luma sum */
	sum_b = sum_c;
	sum_c = ve_dnlp_luma_sum;
	/* new historic luma sum */
	ve_dnlp_luma_sum = p->hist.luma_sum;
	pr_amve_dbg("ve_dnlp_luma_sum=%x,sum_b=%x,sum_c=%x\n",
			ve_dnlp_luma_sum, sum_b, sum_c);
	/* picture mode: freeze dnlp curve */
	if (dnlp_respond) {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if (!ve_dnlp_luma_sum)
			return;
	} else {
		/* new luma sum is 0, something is wrong, freeze dnlp curve */
		if ((!ve_dnlp_luma_sum) ||
			/* new luma sum is closed to old one (1 +/- 1/64), */
			/* picture mode, freeze curve *//* 5 *//* 5 */
			((ve_dnlp_luma_sum <
					sum_b + (sum_b >> dnlp_adj_level)) &&
			(ve_dnlp_luma_sum >
					sum_b - (sum_b >> dnlp_adj_level))))
			return;
	}
	/* get 5 regions */
	for (i = 0; i < 5; i++) {
		j = 4 + 11 * i;
		data[i] = (ulong)p->hist.gamma[j] +
		(ulong)p->hist.gamma[j +  1] +
		(ulong)p->hist.gamma[j +  2] +
		(ulong)p->hist.gamma[j +  3] +
		(ulong)p->hist.gamma[j +  4] +
		(ulong)p->hist.gamma[j +  5] +
		(ulong)p->hist.gamma[j +  6] +
		(ulong)p->hist.gamma[j +  7] +
		(ulong)p->hist.gamma[j +  8] +
		(ulong)p->hist.gamma[j +  9] +
		(ulong)p->hist.gamma[j + 10];
	}
	/* get max, ave, div */
	for (i = 0; i < 5; i++) {
		if (max < data[i])
			max = data[i];
		ave += data[i];
		data[i] *= 5;
	}
	max *= 5;
	div = (max - ave > ave) ? max - ave : ave;
	/* invalid histgram: freeze dnlp curve */
	if (!max)
		return;
	/* get 1st 4 points */
	for (i = 0; i < 4; i++) {
		if (data[i] > ave)
			data[i] = 64 + (((data[i] - ave) << 1) + div) *
			ve_dnlp_rl / (div << 1);
		else if (data[i] < ave)
			data[i] = 64 - (((ave - data[i]) << 1) + div) *
			ve_dnlp_rl / (div << 1);
		else
			data[i] = 64;
		ve_dnlp_tgt[4 + 11 * (i + 1)] = ve_dnlp_tgt[4 + 11 * i] +
			((44 * data[i] + 32) >> 6);
	}
	/* fill in region 0 with black extension */
	data[0] = ve_dnlp_black;
	if (data[0] > 16)
		data[0] = 16;
	data[0] =
		(ve_dnlp_tgt[15] - ve_dnlp_tgt[4]) * (16 - data[0]);
	for (j = 1; j <= 6; j++)
		ve_dnlp_tgt[4 + j] =
			ve_dnlp_tgt[4] + (data[0] * j + 88) / 176;
	data[0] = (ve_dnlp_tgt[15] - ve_dnlp_tgt[10]) << 1;
	for (j = 1; j <= 4; j++)
		ve_dnlp_tgt[10 + j] =
			ve_dnlp_tgt[10] + (data[0] * j + 5) / 10;
	/* fill in regions 1~3 */
	for (i = 1; i <= 3; i++) {
		data[i] =
		(ve_dnlp_tgt[11 * i + 15] - ve_dnlp_tgt[11 * i + 4]) << 1;
		for (j = 1; j <= 10; j++)
			ve_dnlp_tgt[11 * i + 4 + j] = ve_dnlp_tgt[11 * i + 4] +
			(data[i] * j + 11) / 22;
	}
	/* fill in region 4 with white extension */
	data[4] /= 20;
	data[4] = (ve_dnlp_white *
			((ave << 4) - data[4] * ve_dnlp_white_factor)
			+ (ave << 3)) / (ave << 4);
	if (data[4] > 16)
		data[4] = 16;
	data[4] = (ve_dnlp_tgt[59] - ve_dnlp_tgt[48]) * (16 - data[4]);
	for (j = 1; j <= 6; j++)
		ve_dnlp_tgt[59 - j] = ve_dnlp_tgt[59] -
		(data[4] * j + 88) / 176;
	data[4] = (ve_dnlp_tgt[53] - ve_dnlp_tgt[48]) << 1;
	for (j = 1; j <= 4; j++)
		ve_dnlp_tgt[53 - j] = ve_dnlp_tgt[53] - (data[4] * j + 5) / 10;
	if (cnt) {
		for (i = 0; i < 64; i++)
			pr_amve_dbg(" ve_dnlp_tgt[%ld]=%d\n",
					i, ve_dnlp_tgt[i]);
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, 0); */
		assist_cnt = 0;
	}
}
 /* lpf[0] is always 0 & no need calculation */
static void ve_dnlp_calculate_lpf(void)
{
	ulong i = 0;
	for (i = 0; i < 64; i++)
		ve_dnlp_lpf[i] = ve_dnlp_lpf[i] -
		(ve_dnlp_lpf[i] >> ve_dnlp_rt) + ve_dnlp_tgt[i];
}

static void ve_dnlp_calculate_reg(void)
{
	ulong i = 0, j = 0, cur = 0, data = 0,
			offset = ve_dnlp_rt ? (1 << (ve_dnlp_rt - 1)) : 0;
	for (i = 0; i < 16; i++) {
		ve_dnlp_reg[i] = 0;
		cur = i << 2;
		for (j = 0; j < 4; j++) {
			data = (ve_dnlp_lpf[cur + j] + offset) >> ve_dnlp_rt;
			if (data > 255)
				data = 255;
			ve_dnlp_reg[i] |= data << (j << 3);
		}
	}
}

static void ve_dnlp_load_reg(void)
{
/* #ifdef NEW_DNLP_IN_SHARPNESS */
	/* if(dnlp_sel == NEW_DNLP_IN_SHARPNESS){ */
	if (is_meson_g9tv_cpu() &&
		(dnlp_sel == NEW_DNLP_IN_SHARPNESS)) {
		WRITE_VPP_REG(DNLP_00, ve_dnlp_reg[0]);
		WRITE_VPP_REG(DNLP_01, ve_dnlp_reg[1]);
		WRITE_VPP_REG(DNLP_02, ve_dnlp_reg[2]);
		WRITE_VPP_REG(DNLP_03, ve_dnlp_reg[3]);
		WRITE_VPP_REG(DNLP_04, ve_dnlp_reg[4]);
		WRITE_VPP_REG(DNLP_05, ve_dnlp_reg[5]);
		WRITE_VPP_REG(DNLP_06, ve_dnlp_reg[6]);
		WRITE_VPP_REG(DNLP_07, ve_dnlp_reg[7]);
		WRITE_VPP_REG(DNLP_08, ve_dnlp_reg[8]);
		WRITE_VPP_REG(DNLP_09, ve_dnlp_reg[9]);
		WRITE_VPP_REG(DNLP_10, ve_dnlp_reg[10]);
		WRITE_VPP_REG(DNLP_11, ve_dnlp_reg[11]);
		WRITE_VPP_REG(DNLP_12, ve_dnlp_reg[12]);
		WRITE_VPP_REG(DNLP_13, ve_dnlp_reg[13]);
		WRITE_VPP_REG(DNLP_14, ve_dnlp_reg[14]);
		WRITE_VPP_REG(DNLP_15, ve_dnlp_reg[15]);
	} else {
		/* #endif */
		WRITE_VPP_REG(VPP_DNLP_CTRL_00, ve_dnlp_reg[0]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_01, ve_dnlp_reg[1]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_02, ve_dnlp_reg[2]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_03, ve_dnlp_reg[3]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_04, ve_dnlp_reg[4]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_05, ve_dnlp_reg[5]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_06, ve_dnlp_reg[6]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_07, ve_dnlp_reg[7]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_08, ve_dnlp_reg[8]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_09, ve_dnlp_reg[9]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_10, ve_dnlp_reg[10]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_11, ve_dnlp_reg[11]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_12, ve_dnlp_reg[12]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_13, ve_dnlp_reg[13]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_14, ve_dnlp_reg[14]);
		WRITE_VPP_REG(VPP_DNLP_CTRL_15, ve_dnlp_reg[15]);
	}
}

void ve_on_vs(struct vframe_s *vf)
{

	if (ve_en) {
		/* calculate dnlp target data */
		if (ve_dnlp_method == 0)
			ve_dnlp_calculate_tgt(vf);
		else if (ve_dnlp_method == 1)
			ve_dnlp_calculate_tgtx(vf);
		else if (ve_dnlp_method == 2)
			ve_dnlp_calculate_tgt_ext(vf);
		else
			ve_dnlp_calculate_tgt(vf);
		/* calculate dnlp low-pass-filter data */
		ve_dnlp_calculate_lpf();
		/* calculate dnlp reg data */
		ve_dnlp_calculate_reg();
		/* load dnlp reg data */
		ve_dnlp_load_reg();
	}
	ve_hist_gamma_tgt(vf);
	/* comment for duration algorithm is not based on panel vsync */
	if (vf->prop.meas.vs_cycle && !frame_lock_nosm) {
		if ((vecm_latch_flag & FLAG_LVDS_FREQ_SW1) &&
		(vf->duration >= 1920 - 19) && (vf->duration <= 1920 + 19))
			vpp_phase_lock_on_vs(vf->prop.meas.vs_cycle,
				vf->prop.meas.vs_stamp,
				true,
				lock_range_50hz_fast,
				lock_range_50hz_slow);
		if ((!(vecm_latch_flag & FLAG_LVDS_FREQ_SW1)) &&
		(vf->duration >= 1600 - 5) && (vf->duration <= 1600 + 13))
			vpp_phase_lock_on_vs(vf->prop.meas.vs_cycle,
				vf->prop.meas.vs_stamp,
				false,
				lock_range_60hz_fast,
				lock_range_60hz_slow);
	}
}

/* *********************************************************************** */
/* *** IOCTL-oriented functions ****************************************** */
/* *********************************************************************** */

void vpp_enable_lcd_gamma_table(void)
{
	WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT, 1, GAMMA_EN, 1);
}

void vpp_disable_lcd_gamma_table(void)
{
	WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT, 0, GAMMA_EN, 1);
}

void vpp_set_lcd_gamma_table(u16 *data, u32 rgb_mask)
{
	int i;
	int cnt = 0;

	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	cnt = 0;
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x0 << HADR));
	for (i = 0; i < 256; i++) {
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY)
				break;
		}
		cnt = 0;
		WRITE_VPP_REG(L_GAMMA_DATA_PORT, data[i]);
	}
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));
}

void vpp_set_rgb_ogo(struct tcon_rgb_ogo_s *p)
{
	/* write to registers */
	WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
			((p->en << 31) & 0x80000000) |
			((p->r_gain << 16) & 0x07ff0000) |
			((p->g_gain <<  0) & 0x000007ff));
	WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
			((p->b_gain << 16) & 0x07ff0000) |
			((p->r_post_offset <<  0) & 0x000007ff));
	WRITE_VPP_REG(VPP_GAINOFF_CTRL2,
			((p->g_post_offset << 16) & 0x07ff0000) |
			((p->b_post_offset <<  0) & 0x000007ff));
	WRITE_VPP_REG(VPP_GAINOFF_CTRL3,
			((p->r_pre_offset  << 16) & 0x07ff0000) |
			((p->g_pre_offset  <<  0) & 0x000007ff));
	WRITE_VPP_REG(VPP_GAINOFF_CTRL4,
			((p->b_pre_offset  <<  0) & 0x000007ff));
}

void ve_enable_dnlp(void)
{
	ve_en = 1;
/* #ifdef NEW_DNLP_IN_SHARPNESS */
/* if(dnlp_sel == NEW_DNLP_IN_SHARPNESS){ */
	if (is_meson_g9tv_cpu() &&
		(dnlp_sel == NEW_DNLP_IN_SHARPNESS))
		WRITE_VPP_REG_BITS(DNLP_EN, 1, 0, 1);
	else
		/* #endif */
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				1, DNLP_EN_BIT, DNLP_EN_WID);
}

void ve_disable_dnlp(void)
{
	ve_en = 0;
/* #ifdef NEW_DNLP_IN_SHARPNESS */
/* if(dnlp_sel == NEW_DNLP_IN_SHARPNESS){ */
	if (is_meson_g9tv_cpu() &&
		(dnlp_sel == NEW_DNLP_IN_SHARPNESS))
		WRITE_VPP_REG_BITS(DNLP_EN, 0, 0, 1);
	else
/* #endif */
/* { */
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				0, DNLP_EN_BIT, DNLP_EN_WID);
/* } */
}

void ve_set_dnlp(struct ve_dnlp_s *p)
{
	ulong i = 0;
	/* get command parameters */
	ve_en                = p->en;
	ve_dnlp_white_factor = (p->rt >> 4) & 0xf;
	ve_dnlp_rt           = p->rt & 0xf;
	ve_dnlp_rl           = p->rl;
	ve_dnlp_black        = p->black;
	ve_dnlp_white        = p->white;
	if (ve_en) {
		/* clear historic luma sum */
		ve_dnlp_luma_sum = 0;
		/* init tgt & lpf */
		for (i = 0; i < 64; i++) {
			ve_dnlp_tgt[i] = i << 2;
			ve_dnlp_lpf[i] = ve_dnlp_tgt[i] << ve_dnlp_rt;
		}
		/* calculate dnlp reg data */
		ve_dnlp_calculate_reg();
		/* load dnlp reg data */
		ve_dnlp_load_reg();
		/* enable dnlp */
		ve_enable_dnlp();
	} else {
		/* disable dnlp */
		ve_disable_dnlp();
	}
}

void ve_set_dnlp_2(void)
{
	ulong i = 0;
	/* get command parameters */
	ve_dnlp_method       = 1;
	ve_dnlp_cliprate     = 6;
	ve_dnlp_hghrange     = 14;
	ve_dnlp_lowrange     = 18;
	ve_dnlp_hghalpha     = 26;
	ve_dnlp_midalpha     = 28;
	ve_dnlp_lowalpha     = 18;
	/* clear historic luma sum */
	ve_dnlp_luma_sum = 0;
	/* init tgt & lpf */
	for (i = 0; i < 64; i++) {
		ve_dnlp_tgt[i] = i << 2;
		ve_dnlp_lpf[i] = ve_dnlp_tgt[i] << ve_dnlp_rt;
	}
	/* calculate dnlp reg data */
	ve_dnlp_calculate_reg();
	/* load dnlp reg data */
	ve_dnlp_load_reg();
}

void ve_set_new_dnlp(struct ve_dnlp_table_s *p)
{
	ulong i = 0;
	/* get command parameters */
	ve_en                = p->en;
	ve_dnlp_method       = p->method;
	ve_dnlp_cliprate     = p->cliprate;
	ve_dnlp_hghrange     = p->hghrange;
	ve_dnlp_lowrange     = p->lowrange;
	ve_dnlp_hghalpha     = p->hghalpha;
	ve_dnlp_midalpha     = p->midalpha;
	ve_dnlp_lowalpha     = p->lowalpha;
	if (ve_en) {
		/* clear historic luma sum */
		ve_dnlp_luma_sum = 0;
		/* init tgt & lpf */
		for (i = 0; i < 64; i++) {
			ve_dnlp_tgt[i] = i << 2;
			ve_dnlp_lpf[i] = ve_dnlp_tgt[i] << ve_dnlp_rt;
		}
		/* calculate dnlp reg data */
		ve_dnlp_calculate_reg();
		/* load dnlp reg data */
		ve_dnlp_load_reg();
		/* enable dnlp */
		ve_enable_dnlp();
	} else {
		/* disable dnlp */
		ve_disable_dnlp();
	}
}

unsigned int ve_get_vs_cnt(void)
{
	return READ_VPP_REG(VPP_VDO_MEAS_VS_COUNT_LO);
}

void vpp_phase_lock_on_vs(unsigned int cycle,
			  unsigned int stamp,
			  bool         lock50,
			  unsigned int range_fast,
			  unsigned int range_slow)
{
	unsigned int vtotal_ori = READ_VPP_REG(ENCL_VIDEO_MAX_LNCNT);
	unsigned int vtotal     = lock50 ? 1349 : 1124;
	unsigned int stamp_in   = READ_VPP_REG(VDIN_MEAS_VS_COUNT_LO);
	unsigned int stamp_out  = ve_get_vs_cnt();
	unsigned int phase      = 0;
	unsigned int cnt = assist_cnt;/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	int step = 0, i = 0;
	/* get phase */
	if (stamp_out < stamp)
		phase = 0xffffffff - stamp + stamp_out + 1;
	else
		phase = stamp_out - stamp;
	while (phase >= cycle)
		phase -= cycle;
	/* 225~315 degree => tune fast panel output */
	if ((phase > ((cycle * 5) >> 3)) && (phase < ((cycle * 7) >> 3))) {
		vtotal -= range_slow;
		step = 1;
	} else if ((phase > (cycle >> 3)) && (phase < ((cycle * 3) >> 3))) {
		/* 45~135 degree => tune slow panel output */
		vtotal += range_slow;
		step = -1;
	} else if (phase >= ((cycle * 7) >> 3)) {
		/* 315~360 degree => tune fast panel output */
		vtotal -= range_fast;
		step = + 2;
	} else if (phase <= (cycle >> 3)) {
		/* 0~45 degree => tune slow panel output */
		vtotal += range_fast;
		step = -2;
	} else {/* 135~225 degree => keep still */
		vtotal = vtotal_ori;
		step = 0;
	}
	if (vtotal != vtotal_ori)
		WRITE_VPP_REG(ENCL_VIDEO_MAX_LNCNT, vtotal);
	if (cnt) {
		cnt--;
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, cnt); */
		assist_cnt = cnt;
		if (cnt) {
			vpp_log[cnt][0] = stamp;
			vpp_log[cnt][1] = stamp_in;
			vpp_log[cnt][2] = stamp_out;
			vpp_log[cnt][3] = cycle;
			vpp_log[cnt][4] = phase;
			vpp_log[cnt][5] = vtotal;
			vpp_log[cnt][6] = step;
		} else {
			for (i = 127; i > 0; i--) {
				pr_amve_dbg("Ti=%10u Tio=%10u To=%10u CY=%6u ",
					vpp_log[i][0],
					vpp_log[i][1],
					vpp_log[i][2],
					vpp_log[i][3]);
				pr_amve_dbg("PH =%10u Vt=%4u S=%2d\n",
					vpp_log[i][4],
					vpp_log[i][5],
					vpp_log[i][6]);
			}
		}
	}

}

/* #if (MESON_CPU_TYPE>=MESON_CPU_TYPE_MESON6TVD) */
void ve_frame_size_patch(unsigned int width, unsigned int height)
{
	unsigned int vpp_size = height|(width << 16);
	if (ve_size != vpp_size) {
		WRITE_VPP_REG(VPP_VE_H_V_SIZE, vpp_size);
		ve_size = vpp_size;
	}
}
/* #endif */

void ve_dnlp_latch_process(void)
{
	if (vecm_latch_flag & FLAG_VE_DNLP) {
		vecm_latch_flag &= ~FLAG_VE_DNLP;
		ve_set_dnlp(&am_ve_dnlp);
	}
	if (vecm_latch_flag & FLAG_VE_NEW_DNLP) {
		vecm_latch_flag &= ~FLAG_VE_NEW_DNLP;
		ve_set_new_dnlp(&am_ve_new_dnlp);
	}
	if (vecm_latch_flag & FLAG_VE_DNLP_EN) {
		vecm_latch_flag &= ~FLAG_VE_DNLP_EN;
		ve_enable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_enable_dnlp OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_VE_DNLP_DIS) {
		vecm_latch_flag &= ~FLAG_VE_DNLP_DIS;
		ve_disable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_disable_dnlp OK!!!\n");
	}
	if (dnlp_en && dnlp_status) {
		dnlp_status = 0;
		ve_set_dnlp_2();
		ve_enable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_enable_dnlp OK!!!\n");
	} else if (dnlp_en == 0) {
		dnlp_status = 1;
		ve_disable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_disable_dnlp OK!!!\n");
	}
}

void ve_lcd_gamma_process(void)
{
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_EN) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_EN;
		vpp_enable_lcd_gamma_table();
		pr_amve_dbg("\n[amve..] set vpp_enable_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_DIS) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_DIS;
		vpp_disable_lcd_gamma_table();
		pr_amve_dbg("\n[amve..] set vpp_disable_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_R) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_R;
		vpp_set_lcd_gamma_table(video_gamma_table_r.data, H_SEL_R);
		pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_G) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_G;
		vpp_set_lcd_gamma_table(video_gamma_table_g.data, H_SEL_G);
		pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_GAMMA_TABLE_B) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_B;
		vpp_set_lcd_gamma_table(video_gamma_table_b.data, H_SEL_B);
		pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
	}
	if (vecm_latch_flag & FLAG_RGB_OGO) {
		vecm_latch_flag &= ~FLAG_RGB_OGO;
		if (video_rgb_ogo_mode_sw) {
			if (video_rgb_ogo.en) {
				vpp_set_lcd_gamma_table(
						video_gamma_table_r_adj.data,
						H_SEL_R);
				vpp_set_lcd_gamma_table(
						video_gamma_table_g_adj.data,
						H_SEL_G);
				vpp_set_lcd_gamma_table(
						video_gamma_table_b_adj.data,
						H_SEL_B);
			} else {
				vpp_set_lcd_gamma_table(
						video_gamma_table_r.data,
						H_SEL_R);
				vpp_set_lcd_gamma_table(
						video_gamma_table_g.data,
						H_SEL_G);
				vpp_set_lcd_gamma_table(
						video_gamma_table_b.data,
						H_SEL_B);
			}
			pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
		} else {
			vpp_set_rgb_ogo(&video_rgb_ogo);
			pr_amve_dbg("\n[amve..] set vpp_set_rgb_ogo OK!!!\n");
		}
	}
}
void lvds_freq_process(void)
{
/* #if ((MESON_CPU_TYPE==MESON_CPU_TYPE_MESON6TV)|| */
/* (MESON_CPU_TYPE==MESON_CPU_TYPE_MESON6TVD)) */
/*  lvds freq 50Hz/60Hz */
/* if (frame_lock_freq == 1){//50 hz */
/* // panel freq is 60Hz => change back to 50Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) < 1237) */
/* (1124 + 1349 +1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1349); */
/* } */
/* else if (frame_lock_freq == 2){//60 hz */
/* // panel freq is 50Hz => change back to 60Hz */
/* if(READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) >= 1237) */
/* (1124 + 1349 + 1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1124); */
/* } */
/* else if (frame_lock_freq == 0){ */
/*  lvds freq 50Hz/60Hz */
/* if (vecm_latch_flag & FLAG_LVDS_FREQ_SW){  //50 hz */
/* // panel freq is 60Hz => change back to 50Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) < 1237) */
/* (1124 + 1349 +1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1349); */
/* }else{	 //60 hz */
/* // panel freq is 50Hz => change back to 60Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) >= 1237) */
/* (1124 + 1349 + 1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1124); */
/* } */
/* } */
/* #endif */
}

void ve_dnlp_param_update(void)
{
	if (am_ve_dnlp.en    >  1)
		am_ve_dnlp.en    =  1;
	if (am_ve_dnlp.black > 16)
		am_ve_dnlp.black = 16;
	if (am_ve_dnlp.white > 16)
		am_ve_dnlp.white = 16;
	vecm_latch_flag |= FLAG_VE_DNLP;
}

void ve_new_dnlp_param_update(void)
{
	if (am_ve_new_dnlp.en > 1)
		am_ve_new_dnlp.en = 1;
	if (am_ve_new_dnlp.cliprate > 256)
		am_ve_new_dnlp.cliprate = 256;
	if (am_ve_new_dnlp.lowrange > 54)
		am_ve_new_dnlp.lowrange = 54;
	if (am_ve_new_dnlp.hghrange > 54)
		am_ve_new_dnlp.hghrange = 54;
	if (am_ve_new_dnlp.lowalpha > 48)
		am_ve_new_dnlp.lowalpha = 48;
	if (am_ve_new_dnlp.midalpha > 48)
		am_ve_new_dnlp.midalpha = 48;
	if (am_ve_new_dnlp.hghalpha > 48)
		am_ve_new_dnlp.hghalpha = 48;
	vecm_latch_flag |= FLAG_VE_NEW_DNLP;
}



static void video_data_limitation(int *val)
{
	if (*val > 1023)
		*val = 1023;
	if (*val < 0)
		*val = 0;
}

static void video_lookup(struct tcon_gamma_table_s *tbl, int *val)
{
	unsigned int idx = (*val) >> 2, mod = (*val) & 3;
	if (idx < 255)
		*val = tbl->data[idx] +
		(((tbl->data[idx + 1] - tbl->data[idx]) * mod + 2) >> 2);
	else
		*val = tbl->data[idx] +
		(((1023 - tbl->data[idx]) * mod + 2) >> 2);
}

static void video_set_rgb_ogo(void)
{
	int i = 0, r = 0, g = 0, b = 0;
	for (i = 0; i < 256; i++) {
		/* Get curve_straight =
		 * input(curve_2d2_inv) * video_curve_2d2 */
		r = video_curve_2d2.data[i];
		g = video_curve_2d2.data[i];
		b = video_curve_2d2.data[i];
		/* Pre_offset */
		r += video_rgb_ogo.r_pre_offset;
		g += video_rgb_ogo.g_pre_offset;
		b += video_rgb_ogo.b_pre_offset;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Gain */
		r  *= video_rgb_ogo.r_gain;
		r >>= 10;
		g  *= video_rgb_ogo.g_gain;
		g >>= 10;
		b  *= video_rgb_ogo.b_gain;
		b >>= 10;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Post_offset */
		r += video_rgb_ogo.r_post_offset;
		g += video_rgb_ogo.g_post_offset;
		b += video_rgb_ogo.b_post_offset;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Get curve_2d2_inv_ogo =
		 * curve_straight_ogo * video_curve_2d2_inv */
		video_lookup(&video_curve_2d2_inv, &r);
		video_lookup(&video_curve_2d2_inv, &g);
		video_lookup(&video_curve_2d2_inv, &b);
		/* Get gamma_ogo = curve_2d2_inv_ogo * gamma */
		video_lookup(&video_gamma_table_r, &r);
		video_lookup(&video_gamma_table_g, &g);
		video_lookup(&video_gamma_table_b, &b);
		/* Save gamma_ogo */
		video_gamma_table_r_adj.data[i] = r;
		video_gamma_table_g_adj.data[i] = g;
		video_gamma_table_b_adj.data[i] = b;
	}
}

void ve_ogo_param_update(void)
{
	if (video_rgb_ogo.en > 1)
		video_rgb_ogo.en = 1;
	if (video_rgb_ogo.r_pre_offset > 1023)
		video_rgb_ogo.r_pre_offset = 1023;
	if (video_rgb_ogo.r_pre_offset < -1024)
		video_rgb_ogo.r_pre_offset = -1024;
	if (video_rgb_ogo.g_pre_offset > 1023)
		video_rgb_ogo.g_pre_offset = 1023;
	if (video_rgb_ogo.g_pre_offset < -1024)
		video_rgb_ogo.g_pre_offset = -1024;
	if (video_rgb_ogo.b_pre_offset > 1023)
		video_rgb_ogo.b_pre_offset = 1023;
	if (video_rgb_ogo.b_pre_offset < -1024)
		video_rgb_ogo.b_pre_offset = -1024;
	if (video_rgb_ogo.r_gain > 2047)
		video_rgb_ogo.r_gain = 2047;
	if (video_rgb_ogo.g_gain > 2047)
		video_rgb_ogo.g_gain = 2047;
	if (video_rgb_ogo.b_gain > 2047)
		video_rgb_ogo.b_gain = 2047;
	if (video_rgb_ogo.r_post_offset > 1023)
		video_rgb_ogo.r_post_offset = 1023;
	if (video_rgb_ogo.r_post_offset < -1024)
		video_rgb_ogo.r_post_offset = -1024;
	if (video_rgb_ogo.g_post_offset > 1023)
		video_rgb_ogo.g_post_offset = 1023;
	if (video_rgb_ogo.g_post_offset < -1024)
		video_rgb_ogo.g_post_offset = -1024;
	if (video_rgb_ogo.b_post_offset > 1023)
		video_rgb_ogo.b_post_offset = 1023;
	if (video_rgb_ogo.b_post_offset < -1024)
		video_rgb_ogo.b_post_offset = -1024;
	if (video_rgb_ogo_mode_sw)
		video_set_rgb_ogo();

	vecm_latch_flag |= FLAG_RGB_OGO;
}

