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
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "amve.h"
#include "amve_gamma_table.h"
#include "amvecm_vlock_regmap.h"

#define pr_amve_dbg(fmt, args...)\
	do {\
		if (dnlp_debug)\
			pr_info("AMVE: " fmt, ## args);\
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
static ulong ve_dnlp_reg_def[16] = {
	0x0b070400,	0x1915120e,	0x2723201c,	0x35312e2a,
	0x47423d38,	0x5b56514c,	0x6f6a6560,	0x837e7974,
	0x97928d88,	0xaba6a19c,	0xbfbab5b0,	0xcfccc9c4,
	0xdad7d5d2,	0xe6e3e0dd,	0xf2efece9,	0xfdfaf7f4
};

/*static bool frame_lock_nosm = 1;*/
static int ve_dnlp_waist_h = 128;
static int ve_dnlp_waist_l = 128;
static int ve_dnlp_ankle = 16;
static int ve_dnlp_strength = 255;
unsigned int vpp_log[128][10];
struct ve_dnlp_s am_ve_dnlp;
struct ve_dnlp_table_s am_ve_new_dnlp;
#if 0
static unsigned int lock_range_50hz_fast =  7; /* <= 14 */
static unsigned int lock_range_50hz_slow =  7; /* <= 14 */
static unsigned int lock_range_60hz_fast =  5; /* <=  4 */
static unsigned int lock_range_60hz_slow =  2; /* <= 10 */
#endif
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
/*
module_param(frame_lock_nosm, bool, 0664);
MODULE_PARM_DESC(frame_lock_nosm, "frame_lock_nosm");
*/
static int dnlp_adj_level = 6;
module_param(dnlp_adj_level, int, 0664);
MODULE_PARM_DESC(dnlp_adj_level, "dnlp_adj_level");

int dnlp_en;/* 0:disabel;1:enable */
module_param(dnlp_en, int, 0664);
MODULE_PARM_DESC(dnlp_en, "\n enable or disable dnlp\n");
static int dnlp_status = 1;/* 0:done;1:todo */

int dnlp_en_2;/* 0:disabel;1:enable */
module_param(dnlp_en_2, int, 0664);
MODULE_PARM_DESC(dnlp_en_2, "\n enable or disable dnlp\n");

static int frame_lock_freq;
module_param(frame_lock_freq, int, 0664);
MODULE_PARM_DESC(frame_lock_freq, "frame_lock_50");

static int video_rgb_ogo_mode_sw;
module_param(video_rgb_ogo_mode_sw, int, 0664);
MODULE_PARM_DESC(video_rgb_ogo_mode_sw,
		"enable/disable video_rgb_ogo_mode_sw");

static int ve_dnlp_adj_level = 6;
module_param(ve_dnlp_adj_level, int, 0664);
MODULE_PARM_DESC(ve_dnlp_adj_level,
		"ve_dnlp_adj_level");

/* new dnlp start */
static int ve_dnlp_mvreflsh = 4;
module_param(ve_dnlp_mvreflsh, int, 0664);
MODULE_PARM_DESC(ve_dnlp_mvreflsh,
		"ve_dnlp_mvreflsh");

static int ve_dnlp_gmma_rate = 60;
module_param(ve_dnlp_gmma_rate, int, 0664);
MODULE_PARM_DESC(ve_dnlp_gmma_rate,
		"ve_dnlp_gmma_rate");

static int ve_dnlp_lowalpha_new = 20;
module_param(ve_dnlp_lowalpha_new, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lowalpha_new,
		"ve_dnlp_lowalpha_new");

static int ve_dnlp_hghalpha_new = 28;
module_param(ve_dnlp_hghalpha_new, int, 0664);
MODULE_PARM_DESC(ve_dnlp_hghalpha_new,
		"ve_dnlp_hghalpha_new");

static int ve_dnlp_cliprate_new = 6;
module_param(ve_dnlp_cliprate_new, int, 0664);
MODULE_PARM_DESC(ve_dnlp_cliprate_new,
		"ve_dnlp_cliprate_new");

int ve_dnlp_sbgnbnd = 0;
module_param(ve_dnlp_sbgnbnd, int, 0664);
MODULE_PARM_DESC(ve_dnlp_sbgnbnd, "ve_dnlp_sbgnbnd");

static int ve_dnlp_sendbnd = 5;
module_param(ve_dnlp_sendbnd, int, 0664);
MODULE_PARM_DESC(ve_dnlp_sendbnd, "ve_dnlp_sendbnd");

int ve_dnlp_clashBgn = 0;
module_param(ve_dnlp_clashBgn, int, 0664);
MODULE_PARM_DESC(ve_dnlp_clashBgn, "ve_dnlp_clashBgn");

static int ve_dnlp_clashEnd = 10;
module_param(ve_dnlp_clashEnd, int, 0664);
MODULE_PARM_DESC(ve_dnlp_clashEnd, "ve_dnlp_clashEnd");

static int ve_mtdbld_rate = 32;
module_param(ve_mtdbld_rate, int, 0664);
MODULE_PARM_DESC(ve_mtdbld_rate, "ve_mtdbld_rate");

static int ve_blkgma_rate = 4;
module_param(ve_blkgma_rate, int, 0664);
MODULE_PARM_DESC(ve_blkgma_rate, "ve_blkgma_rate");

/*dnlp method = 3, use this flag or no use*/
bool ve_dnlp_respond_flag = 0;
module_param(ve_dnlp_respond_flag, bool, 0664);
MODULE_PARM_DESC(ve_dnlp_respond_flag,
		"ve_dnlp_respond_flag");

/*concentration*/
static int ve_dnlp_blk_cctr = 8;
module_param(ve_dnlp_blk_cctr, int, 0664);
MODULE_PARM_DESC(ve_dnlp_blk_cctr, "dnlp low luma concenration");

/*the center to be brighter*/
static int ve_dnlp_brgt_ctrl = 48;
module_param(ve_dnlp_brgt_ctrl, int, 0664);
MODULE_PARM_DESC(ve_dnlp_brgt_ctrl, "dnlp center to be brighter");

/*brighter range*/
static int ve_dnlp_brgt_range = 16;
module_param(ve_dnlp_brgt_range, int, 0664);
MODULE_PARM_DESC(ve_dnlp_brgt_range, "dnlp brighter range");

/*yout=yin+ve_dnlp_brght_add*/
static int ve_dnlp_brght_add = 1;
module_param(ve_dnlp_brght_add, int, 0664);
MODULE_PARM_DESC(ve_dnlp_brght_add, "dnlp brightness up absolute");

/*yout=yin+ve_dnlp_brght_add + ve_dnlp_brght_max*rate*/
static int ve_dnlp_brght_max = 16;
module_param(ve_dnlp_brght_max, int, 0664);
MODULE_PARM_DESC(ve_dnlp_brght_max, "dnlp brightness up maximum");

bool dnlp_prt_hst = 0;
module_param(dnlp_prt_hst, bool, 0664);
MODULE_PARM_DESC(dnlp_prt_hst, "dnlp print histogram");

bool dnlp_prt_curve = 0;
module_param(dnlp_prt_curve, bool, 0664);
MODULE_PARM_DESC(dnlp_prt_curve, "dnlp print mapping curve");

/*the maximum bins > x/256*/
static int ve_dnlp_lgst_bin = 100;
module_param(ve_dnlp_lgst_bin, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lgst_bin, "dnlp: define it maximum bin");

/*two maximum bins' distance*/
static int ve_dnlp_lgst_dst = 30;
module_param(ve_dnlp_lgst_dst, int, 0664);
MODULE_PARM_DESC(ve_dnlp_lgst_dst, "dnlp: two maximum bins' distance");

bool dnlp_printk = 0;
module_param(dnlp_printk, bool, 0664);
MODULE_PARM_DESC(dnlp_printk, "dnlp_printk");
/*new dnlp end */

static bool hist_sel = 1; /*1->vpp , 0->vdin*/
module_param(hist_sel, bool, 0664);
MODULE_PARM_DESC(hist_sel, "hist_sel");


static unsigned int assist_cnt;/* ASSIST_SPARE8_REG1; */
static unsigned int assist_cnt2;/* ASSIST_SPARE8_REG2; */

/* video lock */
#define VLOCK_MODE_ENC          0
#define VLOCK_MODE_PLL		1

unsigned int vlock_mode = VLOCK_MODE_PLL;/* 0:enc;1:pll */
module_param(vlock_mode, uint, 0664);
MODULE_PARM_DESC(vlock_mode, "\n vlock_mode\n");

unsigned int vlock_en = 0;
module_param(vlock_en, uint, 0664);
MODULE_PARM_DESC(vlock_en, "\n vlock_en\n");

/*
0:only support 50->50;60->60;24->24;30->30;
1:support 24/30/50/60/100/120 mix,such as 50->60;
*/
unsigned int vlock_adapt = 0;
module_param(vlock_adapt, uint, 0664);
MODULE_PARM_DESC(vlock_adapt, "\n vlock_adapt\n");

unsigned int vlock_dis_cnt_limit = 2;
module_param(vlock_dis_cnt_limit, uint, 0664);
MODULE_PARM_DESC(vlock_dis_cnt_limit, "\n vlock_dis_cnt_limit\n");

static unsigned int vlock_sync_limit_flag;
static enum vmode_e pre_vmode = VMODE_1080P;
static enum vframe_source_type_e pre_source_type =
		VFRAME_SOURCE_TYPE_OTHERS;
static enum vframe_source_mode_e pre_source_mode =
		VFRAME_SOURCE_MODE_OTHERS;
static unsigned int pre_input_freq;
static unsigned int pre_output_freq;
static unsigned int vlock_dis_cnt;

/* 3d sync parts begin */
unsigned int sync_3d_h_start = 0;
module_param(sync_3d_h_start, uint, 0664);
MODULE_PARM_DESC(sync_3d_h_start, "\n sync_3d_h_start\n");

unsigned int sync_3d_h_end = 0;
module_param(sync_3d_h_end, uint, 0664);
MODULE_PARM_DESC(sync_3d_h_end, "\n sync_3d_h_end\n");

unsigned int sync_3d_v_start = 10;
module_param(sync_3d_v_start, uint, 0664);
MODULE_PARM_DESC(sync_3d_v_start, "\n sync_3d_v_start\n");

unsigned int sync_3d_v_end = 20;
module_param(sync_3d_v_end, uint, 0664);
MODULE_PARM_DESC(sync_3d_v_end, "\n sync_3d_v_end\n");

unsigned int sync_3d_polarity = 0;
module_param(sync_3d_polarity, uint, 0664);
MODULE_PARM_DESC(sync_3d_polarity, "\n sync_3d_polarity\n");

unsigned int sync_3d_out_inv = 0;
module_param(sync_3d_out_inv, uint, 0664);
MODULE_PARM_DESC(sync_3d_out_inv, "\n sync_3d_out_inv\n");

unsigned int sync_3d_black_color = 0x008080;/* yuv black */
module_param(sync_3d_black_color, uint, 0664);
MODULE_PARM_DESC(sync_3d_black_color, "\n sync_3d_black_color\n");

/* 3d sync to v by one enable/disable */
unsigned int sync_3d_sync_to_vbo = 0;
module_param(sync_3d_sync_to_vbo, uint, 0664);
MODULE_PARM_DESC(sync_3d_sync_to_vbo, "\n sync_3d_sync_to_vbo\n");
/* #endif */
/* 3d sync parts end */


/* *********************************************************************** */
/* *** VPP_FIQ-oriented functions **************************************** */
/* *********************************************************************** */
static void ve_hist_gamma_tgt(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;
	video_ve_hist.sum    = p->hist.vpp_luma_sum;
	video_ve_hist.width  = p->hist.vpp_width;
	video_ve_hist.height = p->hist.vpp_height;
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

static unsigned int pre_2_gamma[65];
static unsigned int pre_1_gamma[65];

static unsigned int pst_2_gamma[65];
static unsigned int pst_1_gamma[65];
static unsigned int pst_0_gamma[65];

unsigned int pst_curve_1[65];

/*rGmIn[0:64]   ==>0:4:256, gamma*/
/*rGmOt[0:pwdth]==>0-0, pwdth-64-256*/
void GetSubCurve(unsigned int *rGmOt,
		unsigned int *rGmIn, unsigned int pwdth)
{
	int nT0 = 0;
	unsigned int BASE = 64;

	unsigned int plft = 0;
	unsigned int prto = 0;
	unsigned int rst = 0;

	unsigned int idx1 = 0;
	unsigned int idx2 = 0;

	if (pwdth == 0)
		pwdth = 1;

	for (nT0 = 0; nT0 <= pwdth; nT0++) {
		plft = nT0*64/pwdth;
		prto = (BASE*(nT0*BASE-plft*pwdth) + pwdth/2)/pwdth;

		idx1 = plft;
		idx2 = plft+1;
		if (idx1 > 64)
			idx1 = 64;
		if (idx2 > 64)
			idx2 = 64;

		rst = rGmIn[idx1]*(BASE-prto) + rGmIn[idx2]*prto;
		rst = (rst + BASE/2)*4*pwdth/BASE;
		rst = ((rst + 128)>>8);

		if (rst > 4*pwdth)
			rst = 4*pwdth;

		rGmOt[nT0] = rst;
	}
}

/*rGmOt[0:64]*/
/*rGmIn[0:64]*/
void GetGmCurves(unsigned int *rGmOt, unsigned int *rGmIn,
		unsigned int pval, unsigned int BgnBnd, unsigned EndBnd)
{
	int nT0 = 0;
	/*unsigned int rst=0;*/
	unsigned int pwdth = 0;
	unsigned int pLst[65];

	if (pval <= 2) {
		for (nT0 = 0; nT0 < 65; nT0++)
			rGmOt[nT0] = rGmIn[nT0];
		return;
	} else if (pval >= 63) {
		for (nT0 = 0; nT0 < 65; nT0++)
			rGmOt[64-nT0] = 255-rGmIn[nT0];
		return;
	}

	if (BgnBnd > 4)
		BgnBnd = 4;
	if (EndBnd > 4)
		EndBnd = 4;

	for (nT0 = 0; nT0 < 65; nT0++)
		rGmOt[nT0] = (nT0<<2);

	if (pval > BgnBnd) {
		pwdth = pval - BgnBnd;
		GetSubCurve(pLst, rGmIn, pwdth);
		for (nT0 = BgnBnd; nT0 <= pval; nT0++)
			rGmOt[nT0] = pLst[nT0 - BgnBnd] + (BgnBnd<<2);
	}

	if (64 > pval + EndBnd) {
		pwdth = 64 - pval - EndBnd;
		GetSubCurve(pLst, rGmIn, pwdth);
		for (nT0 = pval; nT0 <= 64 - EndBnd; nT0++)
			rGmOt[nT0] = 256 - (EndBnd<<2)
				- pLst[pwdth - (nT0 - pval)];
	}
}

unsigned int AdjHistAvg(unsigned int pval, unsigned int ihstEnd)
{
	unsigned int pEXT = 224;
	unsigned int pMid = 128;
	unsigned int pMAX = 236;
	if (ihstEnd > 59)
		pMAX = ihstEnd << 2;

	if (pval > pMid) {
		pval = pMid + (pMAX - pMid)*(pval - pMid)/(pEXT - pMid);
		if (pval > pMAX)
			pval = pMAX;
	}

	return pval;
}


/*iHst[0:63]: [0,4)->iHst[0], [252,256)->iHst[63]*/
/*oMap[0:64]:0:4:256*/
void clash(unsigned int *oMap, unsigned int *iHst,
		unsigned int clip_rate, unsigned int hstBgn,
		unsigned int hstEnd)
{
	unsigned int i = 0, j = 0;
	unsigned int tmax = 0;
	unsigned int tsum = 0;
	unsigned int oHst[65];
	unsigned int cLmt = 0;
	unsigned int tLen = (hstEnd - hstBgn);
	unsigned int tAvg = 0;
	unsigned int nExc = 0;
	unsigned int nStp = 0;
	unsigned int uLmt = 0;
	unsigned int stp = 0;

	if (hstBgn > 16)
		hstBgn = 16;

	if (hstEnd > 64)
		hstEnd = 64;
	else if (hstEnd < 48)
		hstEnd = 48;

	oMap[64] = 256;
	/*64 bins, max, ave*/
	for (i = 0; i < 64; i++) {
		oHst[i] = iHst[i];
		oMap[i] = 4*i;

		if (i >= hstBgn && i <= hstEnd-1) {
			if (tmax < iHst[i])
				tmax = iHst[i];
			tsum += iHst[i];
		} else {
			oHst[i] = 0;
		}
	}

	if (hstEnd <= hstBgn)
		return;

	cLmt = (clip_rate*tsum)>>8;
	tAvg = (tsum + tLen/2)/tLen;
	/*invalid histgram: freeze dnlp curve*/
	if (tmax <= (tLen<<4))
		return;

	nExc = 0;
	/*[bgn, end-1]*/
	for (i = hstBgn; i < hstEnd; i++) {
		if (iHst[i] > cLmt)
			nExc += (iHst[i] - cLmt);
	}
	nStp = (nExc + tLen/2)/tLen;
	uLmt = cLmt - nStp;

	if (clip_rate <= 4 || tAvg <= 2) {
		cLmt = (tsum + tLen/2)/tLen;
		tsum = cLmt*tLen;
		for (i = hstBgn; i < hstEnd; i++)
			oHst[i] = cLmt;
	} else if (nStp != 0) {
		for (i = hstBgn; i < hstEnd; i++) {
			if (iHst[i] >= cLmt)
				oHst[i] = cLmt;
			else {
				if (iHst[i] > uLmt) {
					oHst[i] = cLmt;
					nExc -= cLmt - iHst[i];
				} else {
					oHst[i] = iHst[i]+nStp;
					nExc -= nStp;
				}
				if (nExc < 0)
					nExc = 0;
			}
		}
		j = hstBgn;
		while (nExc > 0) {
			if (nExc >= tLen) {
				nStp = 1;
				stp = nExc/tLen;
			} else {
				nStp = tLen/nExc;
				stp = 1;
			}
			for (i = j; i < hstEnd; i += nStp) {
				if (oHst[i] < cLmt) {
					oHst[i] += stp;
					nExc -= stp;
				}
				if (nExc <= 1)
					break;
			}
			j += 1;
			if (j > hstEnd - 1)
				break;
		}
	}

	/*hstBgn:hstEnd-1*/
	tsum = 0;
	for (i = hstBgn; i < hstEnd; i++) {
		if (oHst[i] > cLmt)
			oHst[i] = cLmt;
		tsum += oHst[i];
	}

	nStp = 0;
	/*sum -= oHst[4];*/
	for (i = hstBgn; i < hstEnd; i++) {
		nStp += oHst[i];

		j = 4*(hstEnd - hstBgn)*nStp;
		j += (tsum>>1);
		j /= tsum;
		oMap[i+1] = j + 4*hstBgn;
	}
}

/*xhu*/
int old_dnlp_mvreflsh;
int old_dnlp_gmma_rate;
int old_dnlp_lowalpha_new;
int old_dnlp_hghalpha_new;
int old_dnlp_sbgnbnd;
int old_dnlp_sendbnd;
int old_dnlp_cliprate_new;
int old_dnlp_clashBgn;
int old_dnlp_clashEnd;
int old_mtdbld_rate;
int old_blkgma_rate;
int old_dnlp_blk_cctr;
int old_dnlp_brgt_ctrl;
int old_dnlp_brgt_range;
int old_dnlp_brght_add;
int old_dnlp_brght_max;
int old_dnlp_lgst_bin;
int old_dnlp_lgst_dst;

static int cal_brght_plus(int luma_avg4, int low_lavg4)
{
	int avg_dif = 0;
	int dif_rat = 0;

	int low_rng = 0;
	int low_rat = 0;

	int dnlp_brightness = 0;

	if (luma_avg4 > low_lavg4)
		avg_dif = luma_avg4 - low_lavg4;

	if (avg_dif < ve_dnlp_blk_cctr)
		dif_rat = ve_dnlp_blk_cctr - avg_dif;

	if (luma_avg4 > ve_dnlp_brgt_ctrl)
		low_rng = luma_avg4 - ve_dnlp_brgt_ctrl;
	else
		low_rng = ve_dnlp_brgt_ctrl - luma_avg4;

	if (low_rng < ve_dnlp_brgt_range)
		low_rat = ve_dnlp_brgt_range - low_rng;

	dnlp_brightness  = (ve_dnlp_brght_max*dif_rat*low_rat + 64)>>7;
	dnlp_brightness += ve_dnlp_brght_add;

	return dnlp_brightness;
}

/*xhu*/
static void ve_dnlp_calculate_tgtx_new(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;

	static unsigned int iHst[65];
	static unsigned int tSumDif[10];
	static unsigned int tDifHst[10];

	unsigned int oHst[65];
	unsigned int tLumDif[9];
	unsigned int clash_curve[65];

	static unsigned int nTstCnt;

	static unsigned int sum_b, sum_c;
	unsigned int i = 0, sum = 0, max = 0;
	unsigned int nTmp = 0;
	unsigned int nT0 = 0, nT1 = 0;
	unsigned int lSby = 0;

	int dnlp_brightness = 0;
	unsigned int mMaxLst[4];
	unsigned int mMaxIdx[4];

	/*unsigned int adj_level = (unsigned int)ve_dnlp_adj_level;*/

	/*u4[0-8] smooth moving,reflesh the curve,0-refresh one frame*/
	unsigned int mvreflsh = (unsigned int) ve_dnlp_mvreflsh;
	/*u8larger-->near to gamma1.8, smaller->gamma1.2 [0-256]dft60*/
	unsigned int gmma_rate = (unsigned int) ve_dnlp_gmma_rate;
	/* u6[0-64]dft20*/
	unsigned int low_alpha = (unsigned int) ve_dnlp_lowalpha_new;

	/* u6[0-64]dft28*/
	unsigned int hgh_alpha = (unsigned int) ve_dnlp_hghalpha_new;

	/*u4s-curve begin band [0-16]dft0*/
	unsigned int sBgnBnd = (unsigned int) ve_dnlp_sbgnbnd;

	/*u4s-curve begin band [0-16]dft5*/
	unsigned int sEndBnd = (unsigned int) ve_dnlp_sendbnd;

	/*u6clash max slope[4-64]	 dft6*/
	unsigned int clip_rate = (unsigned int) ve_dnlp_cliprate_new;

	/*u4clash hist begin point [0-16] dft0*/
	unsigned int clashBgn = (unsigned int) ve_dnlp_clashBgn;

	/*u4 clash hist end point [0~15] dft10*/
	unsigned int clashEnd = (unsigned int) ve_dnlp_clashEnd+49;

	/*please add the parameters*/
	/*u6method blending rate (0~64) dft32*/
	unsigned int mtdbld_rate = (unsigned int) ve_mtdbld_rate;

	/*u6 dft32*/
	unsigned int blkgma_rate = (unsigned int) ve_blkgma_rate;
	/*unsigned int chst_bgn = 4; //histogram beginning */
	/*unsigned int chst_end = 59;//histogram ending,(bgn<=x<end)*/

	/*-------------------------------------------------*/
	unsigned int tAvg = 0;
	/*unsigned int nPnt=0;*/
	/*unsigned int mRng=0;*/
	unsigned int luma_avg = 0;
	static unsigned int pre_luma_avg4;
	unsigned int luma_avg4 = 0;
	unsigned int low_lavg4 = 0; /*low luma average*/

	unsigned int ihstBgn = 0;
	unsigned int ihstEnd = 0;

	unsigned int rGm1p2[] = {0, 2, 4, 7, 9, 12, 15,
					18, 21, 24, 28, 31, 34,
					38, 41, 45, 49, 52, 56,
					60, 63, 67, 71, 75, 79,
					83, 87, 91, 95, 99, 103,
					107, 111, 116, 120, 124,
					128, 133, 137, 141, 146,
					150, 154, 159, 163, 168,
					172, 177, 181, 186, 190,
					195, 200, 204, 209, 213,
					218, 223, 227, 232, 237,
					242, 246, 251, 255};

	unsigned int rGm1p8[] = {0, 0, 1, 1, 2, 3, 4, 5,
					6, 7, 9, 11, 13, 15, 17,
					19, 21, 24, 26, 29, 32,
					34, 37, 41, 44, 47, 51,
					54, 58, 62, 65, 69, 74, 78,
					82, 86, 91, 95, 100, 105,
					110, 115, 120, 125, 130,
					136, 141, 147, 153, 158,
					164, 170, 176, 182, 189,
					195, 201, 208, 214, 221,
					228, 235, 242, 249, 255};

	if (dnlp_respond) {
		if ((old_dnlp_mvreflsh != ve_dnlp_mvreflsh) ||
			(old_dnlp_gmma_rate != ve_dnlp_gmma_rate) ||
			(old_dnlp_lowalpha_new != ve_dnlp_lowalpha_new) ||
			(old_dnlp_hghalpha_new != ve_dnlp_hghalpha_new) ||
			(old_dnlp_sbgnbnd != ve_dnlp_sbgnbnd) ||
			(old_dnlp_sendbnd != ve_dnlp_sendbnd) ||
			(old_dnlp_cliprate_new != ve_dnlp_cliprate_new) ||
			(old_dnlp_clashBgn != ve_dnlp_clashBgn) ||
			(old_dnlp_clashEnd != ve_dnlp_clashEnd) ||
			(old_mtdbld_rate != ve_mtdbld_rate) ||
			(old_blkgma_rate != ve_blkgma_rate) ||
			(old_dnlp_blk_cctr != ve_dnlp_blk_cctr) ||
			(old_dnlp_brgt_ctrl != ve_dnlp_brgt_ctrl) ||
			(old_dnlp_brgt_range != ve_dnlp_brgt_range) ||
			(old_dnlp_brght_add != ve_dnlp_brght_add) ||
			(old_dnlp_brght_max != ve_dnlp_brght_max) ||
			(old_dnlp_lgst_bin != ve_dnlp_lgst_bin) ||
			(old_dnlp_lgst_dst != ve_dnlp_lgst_dst))
			ve_dnlp_respond_flag = 1;
		else
			ve_dnlp_respond_flag = 0;
	}

	old_dnlp_mvreflsh = ve_dnlp_mvreflsh;
	old_dnlp_gmma_rate = ve_dnlp_gmma_rate;
	old_dnlp_lowalpha_new = ve_dnlp_lowalpha_new;
	old_dnlp_hghalpha_new = ve_dnlp_hghalpha_new;
	old_dnlp_sbgnbnd = ve_dnlp_sbgnbnd;
	old_dnlp_sendbnd = ve_dnlp_sendbnd;
	old_dnlp_cliprate_new = ve_dnlp_cliprate_new;
	old_dnlp_clashBgn = ve_dnlp_clashBgn;
	old_dnlp_clashEnd = ve_dnlp_clashEnd;
	old_mtdbld_rate = ve_mtdbld_rate;
	old_blkgma_rate = ve_blkgma_rate;
	old_dnlp_blk_cctr = ve_dnlp_blk_cctr;
	old_dnlp_brgt_ctrl = ve_dnlp_brgt_ctrl;
	old_dnlp_brgt_range = ve_dnlp_brgt_range;
	old_dnlp_brght_add = ve_dnlp_brght_add;
	old_dnlp_brght_max = ve_dnlp_brght_max;
	old_dnlp_lgst_bin = ve_dnlp_lgst_bin;
	old_dnlp_lgst_dst = ve_dnlp_lgst_dst;

	if (low_alpha > 64)
		low_alpha = 64;
	if (hgh_alpha > 64)
		hgh_alpha = 64;
	if (clashBgn > 16)
		clashBgn = 16;
	if (clashEnd > 64)
		clashEnd = 64;
	if (clashEnd < 49)
		clashEnd = 49;
	/* old historic luma sum*/
	sum_b = sum_c;
	/* new historic luma sum*/
	if (hist_sel)
		ve_dnlp_luma_sum = p->hist.vpp_luma_sum;
	else
		ve_dnlp_luma_sum = p->hist.luma_sum;
	sum_c = ve_dnlp_luma_sum;

	if (dnlp_respond) {
		/*new luma sum is 0,something is wrong,freeze dnlp curve*/
		if (!ve_dnlp_luma_sum)
			return;
	}

	for (i = 0; i < 64; i++) {
		pre_2_gamma[i] = pre_1_gamma[i];
		pre_1_gamma[i] = iHst[i];
		if (hist_sel)
			iHst[i]        = (unsigned int)p->hist.vpp_gamma[i];
		else
			iHst[i]        = (unsigned int)p->hist.gamma[i];

		pst_2_gamma[i] = pst_1_gamma[i];
		pst_1_gamma[i] = pst_0_gamma[i];
		pst_0_gamma[i] = ve_dnlp_tgt[i];
	}

	if (dnlp_prt_hst) {
		for (i = 0; i < 64; i++)
			pr_amve_dbg("[%03d,%03d): %05d\n",
			4*i, 4*(i+1), iHst[i]);
	}

	for (i = 0; i < 64; i++) {
		if (iHst[i] != 0) {
			if (ihstBgn == 0)
				ihstBgn = i;

			if (ihstEnd != 64)
				ihstEnd = i+1;
		}
		clash_curve[i] = (i<<2);
	}
	clash_curve[64] = 256;

	/* new historic luma sum*/
	pr_amve_dbg("ve_dnlp_luma_sum=%x,sum_b=%x,sum_c=%x\n",
				ve_dnlp_luma_sum, sum_b, sum_c);
	/* picture mode: freeze dnlp curve*/
	sum = 0;
	max = 0;
	luma_avg = 0;

	/*Get the maximum4*/
	mMaxLst[0] = 0;
	mMaxLst[1] = 0;
	mMaxLst[2] = 0;
	mMaxLst[3] = 0;
	mMaxIdx[0] = 0;
	mMaxIdx[1] = 0;
	mMaxIdx[2] = 0;
	mMaxIdx[3] = 0;
	nT0 = 0;
	for (i = 0; i < 64; i++) {
		nTmp = iHst[i];

		oHst[i] = nTmp;

		sum += nTmp;

		if (max < nTmp)
			max = nTmp;

		/*lower extension [0-63]*/
		luma_avg += nTmp*i;

		if (i == 31)
			low_lavg4 = luma_avg; /*low luma average*/

		if (dnlp_printk && (nTstCnt == 1))
			pr_amve_dbg("0.0 %u => %u\n", nTmp, sum);

		/*Get the maximum4*/
		for (nT0 = 0; nT0 < 4; nT0++) {
			if (nTmp >= mMaxLst[nT0]) {
				for (nT1 = 3; nT1 >= nT0+1; nT1--) {
					mMaxLst[nT1] = mMaxLst[nT1-1];
					mMaxIdx[nT1] = mMaxIdx[nT1-1];
				}

				mMaxLst[nT0] = nTmp;
				mMaxIdx[nT0] = i;
				break;
			}
		}
	}

	if (dnlp_prt_hst) {
		pr_amve_dbg("Max: %04d(%d) > %04d(%d) > %04d(%d) > %04d(%d)\n",
		mMaxLst[0], mMaxIdx[0], mMaxLst[1], mMaxIdx[1],
		mMaxLst[2], mMaxIdx[2], mMaxLst[3], mMaxIdx[3]);
		dnlp_prt_hst = 0;
	}

	/*invalid histgram: freeze dnlp curve*/
	if (max <= 55 || sum == 0)
		return;

	/*tAvg = sum/64; (64bins)*/
	tAvg = sum>>6;
	luma_avg4 = 4*luma_avg/sum;
	low_lavg4 = 4*low_lavg4/sum;

	dnlp_brightness = cal_brght_plus(luma_avg4, low_lavg4);

	/*150918 for 32-step luma pattern*/
	luma_avg4 = AdjHistAvg(luma_avg4, ihstEnd);
	luma_avg = (luma_avg4>>2);

	nTstCnt++;
	if (nTstCnt > 240)
		nTstCnt = 0;

	for (i = 0; i < 9; i++)
		tDifHst[i] = tDifHst[i+1];

	tDifHst[9] = ve_dnlp_luma_sum;

	for (i = 0; i < 9; i++) {
		tLumDif[i] = (tDifHst[i+1] > tDifHst[0]) ?
			(tDifHst[i+1] - tDifHst[0]) :
			(tDifHst[0] - tDifHst[i+1]);
	}

	lSby = 0;
	for (i = 0; i < 8; i++) {
		if (tLumDif[i+1] > tLumDif[i])
			lSby++;
	}

	for (i = 0; i < 9; i++)
		tSumDif[i] = tSumDif[i+1];

	tSumDif[9] = ((sum_b > ve_dnlp_luma_sum) ?
			(sum_b - ve_dnlp_luma_sum) :
			(ve_dnlp_luma_sum - sum_b));

	nTmp = 0;
	for (i = 5; i <= 9; i++)
		nTmp = nTmp + tSumDif[i];

	nT0 = ve_dnlp_adj_level;

	if (nT0 + luma_avg <= 8)
		nT0 = 0;
	else if (luma_avg <= 8)
		nT0 = nT0 - (8 - luma_avg);

	/*new luma sum is closed to old one
		(1 +/- 1/64),picture mode,freeze curve*/
	if ((nTmp < (sum_b >> nT0)) &&
			(lSby < mvreflsh) && (!ve_dnlp_respond_flag)) {
		for (i = 0; i < 64; i++) {
			ve_dnlp_tgt[i] = pst_1_gamma[i];
			pst_0_gamma[i] = ve_dnlp_tgt[i];
		}
		return;
	}

	if (dnlp_printk) {
		pr_amve_dbg("Rflsh: %03u\n", nTstCnt);
		pr_amve_dbg("0.0 sum(%u~%u) =%u\n",
					ihstBgn, ihstEnd, sum);
		pr_amve_dbg("1.0 CalAvg (%u, %u)\n", max, luma_avg4);
		pr_amve_dbg("1.1 GetGmCurves (%u, %u, %u)\n",
				luma_avg, sBgnBnd, sEndBnd);
	}
	GetGmCurves(pst_0_gamma, rGm1p2, luma_avg, sBgnBnd, sEndBnd);
	GetGmCurves(pst_curve_1, rGm1p8, luma_avg, sBgnBnd, sEndBnd);

	nTmp = (luma_avg > 31) ? luma_avg-31 : 31-luma_avg;
	nTmp = (32 - nTmp + 2) >> 2;

	gmma_rate = gmma_rate + nTmp;
	if (gmma_rate > 255)
		gmma_rate = 255;

	if (luma_avg4 <= 32)
		low_alpha = low_alpha + (32 - luma_avg4);

	if (luma_avg4 >= 224) {
		if (low_alpha < (luma_avg4 - 224))
			low_alpha = 0;
		else
			low_alpha = low_alpha - (luma_avg4 - 224);
	}

	clash(clash_curve, iHst, clip_rate, clashBgn, clashEnd);

	/*patch for black+white stripe*/
	if ((mMaxLst[1] > (ve_dnlp_lgst_bin*sum>>8)) &&
		((mMaxIdx[1] > (mMaxIdx[0]+ve_dnlp_lgst_dst)) ||
		(mMaxIdx[0] > (mMaxIdx[1]+ve_dnlp_lgst_dst)))) {
		gmma_rate = 255;
		low_alpha = 0;
		hgh_alpha = 0;
		mtdbld_rate = 64;
	}
	/*=========================================================*/

	for (i = 0; i < 64; i++) {
		nTmp = (((256 - gmma_rate)*pst_0_gamma[i] +
					pst_curve_1[i]*gmma_rate + 128)>>8);

		if (i <= luma_avg)
			nTmp = (nTmp*(64 - low_alpha) + low_alpha*4*i + 32)>>6;
		else
			nTmp = (nTmp*(64 - hgh_alpha) + hgh_alpha*4*i + 32)>>6;

		nTmp = nTmp*mtdbld_rate + clash_curve[i]*(64 - mtdbld_rate);
		nTmp = (nTmp + 32)>>6;
		nTmp = rGm1p8[i]*blkgma_rate + nTmp*(64 - blkgma_rate);
		nTmp = (nTmp+32)>>6;

		nTmp += dnlp_brightness;

		if (nTmp > 255)
			nTmp = 255;
		/*else if (nTmp < 0)*/
		/*    nTmp=0; //unsigned*/

		pst_0_gamma[i] = nTmp;
		ve_dnlp_tgt[i] = nTmp;
	}

	if (dnlp_prt_curve) {
		pr_amve_dbg("low_avg=%03d all_avg=%03d\n",
						low_lavg4, luma_avg4);

	    for (i = 0; i < 64; i++)
			pr_amve_dbg("%02d: %03d=>%03d\n",
					i, 4*i, ve_dnlp_tgt[i]);
		dnlp_prt_curve = 0;
	}

	if (dnlp_printk) {
		pr_amve_dbg("3.1Pars %d,%d,%d,%d,%d,%d\n", mvreflsh,
				gmma_rate, low_alpha, hgh_alpha,
				sBgnBnd, sEndBnd);
		pr_amve_dbg("3.2Pars %d,%d,%d,%d\n\n",
				clip_rate, clashBgn,
				clashEnd, mtdbld_rate);
	}

	pre_luma_avg4 = luma_avg4;

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

static void ve_dnlp_load_def_reg(void)
{
	WRITE_VPP_REG(VPP_DNLP_CTRL_00, ve_dnlp_reg_def[0]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_01, ve_dnlp_reg_def[1]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_02, ve_dnlp_reg_def[2]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_03, ve_dnlp_reg_def[3]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_04, ve_dnlp_reg_def[4]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_05, ve_dnlp_reg_def[5]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_06, ve_dnlp_reg_def[6]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_07, ve_dnlp_reg_def[7]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_08, ve_dnlp_reg_def[8]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_09, ve_dnlp_reg_def[9]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_10, ve_dnlp_reg_def[10]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_11, ve_dnlp_reg_def[11]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_12, ve_dnlp_reg_def[12]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_13, ve_dnlp_reg_def[13]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_14, ve_dnlp_reg_def[14]);
	WRITE_VPP_REG(VPP_DNLP_CTRL_15, ve_dnlp_reg_def[15]);
}

void ve_on_vs(struct vframe_s *vf)
{

	if (dnlp_en_2 || ve_en) {
		/* calculate dnlp target data */
		if (ve_dnlp_method == 0)
			ve_dnlp_calculate_tgt(vf);
		else if (ve_dnlp_method == 1)
			ve_dnlp_calculate_tgtx(vf);
		else if (ve_dnlp_method == 2)
			ve_dnlp_calculate_tgt_ext(vf);
		else if (ve_dnlp_method == 3)
			ve_dnlp_calculate_tgtx_new(vf);
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

	/* vlock processs */
	if (is_meson_g9tv_cpu() || is_meson_gxtvbb_cpu())
		amve_vlock_process(vf);

	/* sharpness process */
	sharpness_process(vf);

	/* */
#if 0
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
#endif
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
	if (is_meson_gxbb_cpu()) {
		ve_dnlp_method		 = 1;
		ve_dnlp_cliprate	 = 6;
		ve_dnlp_hghrange	 = 14;
		ve_dnlp_lowrange	 = 18;
		ve_dnlp_hghalpha	 = 26;
		ve_dnlp_midalpha	 = 28;
		ve_dnlp_lowalpha	 = 18;
	}
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
	/*ve_dnlp_load_reg();*/
	ve_dnlp_load_def_reg();
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

	ve_dnlp_mvreflsh  = p->new_mvreflsh;
	ve_dnlp_gmma_rate = p->new_gmma_rate;
	ve_dnlp_lowalpha_new  = p->new_lowalpha;
	ve_dnlp_hghalpha_new  = p->new_hghalpha;

	ve_dnlp_sbgnbnd   = p->new_sbgnbnd;
	ve_dnlp_sendbnd   = p->new_sendbnd;

	ve_dnlp_cliprate_new  = p->new_cliprate;
	ve_dnlp_clashBgn  = p->new_clashBgn;
	ve_dnlp_clashEnd  = p->new_clashEnd;

	ve_mtdbld_rate    = p->new_mtdbld_rate;
	ve_blkgma_rate    = p->new_blkgma_rate;

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

void ve_frame_size_patch(unsigned int width, unsigned int height)
{
	unsigned int vpp_size = height|(width << 16);
	if (READ_VPP_REG(VPP_VE_H_V_SIZE) != vpp_size)
		WRITE_VPP_REG(VPP_VE_H_V_SIZE, vpp_size);
}

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

/*video lock begin*/
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
static unsigned int amvecm_vlock_check_input_hz(struct vframe_s *vf)
{
	unsigned int ret_hz = 0;
	unsigned int duration = vf->duration;

	if ((vf->source_type != VFRAME_SOURCE_TYPE_TUNER) &&
		(vf->source_type != VFRAME_SOURCE_TYPE_CVBS) &&
		(vf->source_type != VFRAME_SOURCE_TYPE_HDMI))
		ret_hz = 0;
	else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		if (duration != 0)
			ret_hz = 96000/duration;
	} else if ((vf->source_type == VFRAME_SOURCE_TYPE_TUNER) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_CVBS)) {
		if (vf->source_mode == VFRAME_SOURCE_MODE_NTSC)
			ret_hz = 60;
		else if ((vf->source_mode == VFRAME_SOURCE_MODE_PAL) ||
			(vf->source_mode == VFRAME_SOURCE_MODE_SECAM))
			ret_hz = 50;
		else
			ret_hz = 0;
	}
	return ret_hz;
}

static unsigned int
amvecm_vlock_check_output_hz(unsigned int sync_duration_num)
{
	unsigned int ret_hz = 0;
	switch (sync_duration_num) {
	case 24:
		ret_hz = 24;
		break;
	case 30:
		ret_hz = 30;
		break;
	case 50:
		ret_hz = 50;
		break;
	case 60:
		ret_hz = 60;
		break;
	case 100:
		ret_hz = 100;
		break;
	case 120:
		ret_hz = 120;
		break;
	default:
		ret_hz = 0;
		break;
	}
	return ret_hz;
}
static void amvecm_vlock_setting(struct vframe_s *vf,
		unsigned int input_hz, unsigned int output_hz)
{
	unsigned int freq_hz = 0;
	if (((input_hz != output_hz) && (vlock_adapt == 0)) ||
			(input_hz == 0) || (output_hz == 0)) {
		/* VLOCK_CNTL_EN disable */
		/* WRITE_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL6,0,20,1); */
		aml_cbus_update_bits(HHI_HDMI_PLL_CNTL6, 1<<20, 0);
		vlock_dis_cnt = vlock_dis_cnt_limit;
		pr_info("[%s]auto disable vlock module for no support case!!!\n",
				__func__);
		return;
	}
	aml_write_cbus(HHI_VID_LOCK_CLK_CNTL, 0x80);
	if (vlock_mode == VLOCK_MODE_ENC) {
		am_set_regmap(&vlock_enc_lcd720x480);
		/* VLOCK_CNTL_EN disable */
		/* WRITE_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL6,0,20,1); */
		aml_cbus_update_bits(HHI_HDMI_PLL_CNTL6, 1<<20, 0);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 29, 1);
		/* CFG_VID_LOCK_ADJ_EN enable */
		WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT, 1, 13, 1);
		/* enable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 30, 1);
	}
	if (vlock_mode == VLOCK_MODE_PLL) {
		/* av pal in,1080p60 hdmi out as default */
		am_set_regmap(&vlock_pll_in50hz_out60hz);
		/*
		set input & output freq
		bit0~7:input freq
		bit8~15:output freq
		*/
		freq_hz = input_hz | (output_hz << 8);
		WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, freq_hz, 0, 16);
		/*
		Ifrm_cnt_mod:0x3001(bit23~16);
		(output_freq/input_freq)*Ifrm_cnt_mod must be integer
		*/
		if (vlock_adapt == 0)
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL,
				1, 16, 8);
		else
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL,
				input_hz, 16, 8);
		/*set PLL M_INT;PLL M_frac*/
		/* WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096, */
		/* READ_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL,0,9),12,9); */
		WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096,
			(aml_read_cbus(HHI_HDMI_PLL_CNTL) & 0x3ff), 12, 9);
		/* WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096, */
		/* READ_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL,0,12),0,12); */
		WRITE_VPP_REG_BITS(VPU_VLOCK_MX4096,
			(aml_read_cbus(HHI_HDMI_PLL_CNTL) & 0x1fff), 0, 12);
		/* vlock module output goes to which module */
		switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
		case 0:/* ENCL */
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, 0, 26, 2);
			break;
		case 1:/* ENCI */
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, 2, 26, 2);
			break;
		case 2: /* ENCP */
			WRITE_VPP_REG_BITS(VPU_VLOCK_MISC_CTRL, 1, 26, 2);
			break;
		default:
			break;
		}
		/*enable vlock to adj pll*/
		/* CFG_VID_LOCK_ADJ_EN disable */
		WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT, 0, 13, 1);
		/* disable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 30, 1);
		/* VLOCK_CNTL_EN enable */
		/* WRITE_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL6,1,20,1); */
		aml_cbus_update_bits(HHI_HDMI_PLL_CNTL6, 1<<20, 1);
		/* enable to adjust pll */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 29, 1);
	}
	if ((vf->source_type == VFRAME_SOURCE_TYPE_TUNER) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_CVBS))
		/* Input Vsync source select from tv-decoder */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 16, 3);
	else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI)
		/* Input Vsync source select from hdmi-rx */
		WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 2, 16, 3);
	WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 1, 31, 1);
}
/* won't change this function internal seqence,
 * if really need change,please be carefull */
void amve_vlock_process(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo;
	unsigned int input_hz, output_hz, input_vs_cnt;

	if (vecm_latch_flag & FLAG_VLOCK_DIS) {
		/* VLOCK_CNTL_EN disable */
		/* WRITE_CBUS_REG_BITS(HHI_HDMI_PLL_CNTL6,0,20,1); */
		aml_cbus_update_bits(HHI_HDMI_PLL_CNTL6, 1<<20, 0);
		vlock_dis_cnt = vlock_dis_cnt_limit;
		vlock_en = 0;
		vecm_latch_flag &= ~FLAG_VLOCK_DIS;
		return;
	}
	if (vlock_en == 1) {
		vinfo = get_current_vinfo();
		input_hz = amvecm_vlock_check_input_hz(vf);
		if (input_hz == 0)
			return;
		output_hz =
			amvecm_vlock_check_output_hz(vinfo->sync_duration_num);
		if ((vinfo->mode != pre_vmode) ||
			(vf->source_type != pre_source_type) ||
			(vf->source_mode != pre_source_mode) ||
			(input_hz != pre_input_freq) ||
			(output_hz != pre_output_freq)) {
			amvecm_vlock_setting(vf, input_hz, output_hz);
			pr_amve_dbg("%s:vmode/source_type/source_mode/",
					__func__);
			pr_amve_dbg("input_freq/output_freq change:");
			pr_amve_dbg("%d/%d/%d/%d/%d=>%d/%d/%d/%d/%d\n",
				pre_vmode, pre_source_type, pre_source_mode,
				pre_input_freq, pre_output_freq, vinfo->mode,
				vf->source_type, vf->source_mode,
				input_hz, output_hz);
			pre_vmode = vinfo->mode;
			pre_source_type = vf->source_type;
			pre_source_mode = vf->source_mode;
			pre_input_freq = input_hz;
			pre_output_freq = output_hz;
			vlock_sync_limit_flag = 0;
		}
		if (vlock_sync_limit_flag < 5) {
			vlock_sync_limit_flag++;
			if (vlock_sync_limit_flag == 5) {
				input_vs_cnt =
				READ_VPP_REG_BITS(VPU_VLOCK_RO_VS_I_DIST,
					0, 28);
				WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MAX,
						input_vs_cnt*103/100);
				WRITE_VPP_REG(VPU_VLOCK_LOOP1_IMISSYNC_MIN,
						input_vs_cnt*97/100);
			}
		}
		return;
	}
	if (vlock_dis_cnt > 0) {
		vlock_dis_cnt--;
		if (vlock_dis_cnt == 0) {
			/* disable to adjust pll */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 29, 1);
			/* CFG_VID_LOCK_ADJ_EN disable */
			WRITE_VPP_REG_BITS(ENCL_MAX_LINE_SWITCH_POINT,
					0, 13, 1);
			/* disable to adjust pll */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 30, 1);
			/* disable vid_lock_en */
			WRITE_VPP_REG_BITS(VPU_VLOCK_CTRL, 0, 31, 1);
		}
	}
	if (vecm_latch_flag & FLAG_VLOCK_EN) {
		vinfo = get_current_vinfo();
		/* pr_info("[%s]vinfo->name:%s\n",__func__,vinfo->name); */
		input_hz = amvecm_vlock_check_input_hz(vf);
		output_hz =
			amvecm_vlock_check_output_hz(vinfo->sync_duration_num);
		amvecm_vlock_setting(vf, input_hz, output_hz);
		pr_amve_dbg("%s:current vmode/source_type/source_mode/",
				__func__);
		pr_amve_dbg("input_freq/output_freq/sig_fmt is:");
		pr_amve_dbg("%d/%d/%d/%d/%d/0x%x\n",
				vinfo->mode, vf->source_type,
				vf->source_mode, input_hz,
				output_hz, vf->sig_fmt);
		vinfo = get_current_vinfo();
		pre_vmode = vinfo->mode;
		pre_source_type = vf->source_type;
		pre_source_mode = vf->source_mode;
		pre_input_freq = input_hz;
		pre_output_freq = output_hz;
		vlock_en = 1;
		vlock_sync_limit_flag = 0;
		vecm_latch_flag &= ~FLAG_VLOCK_EN;
	}
}
/*video lock end*/

/* sharpness process begin */
void sharpness_process(struct vframe_s *vf)
{
	return;
}
/* sharpness process end */

/*for gxbbtv contrast adj in vadj1*/
void vpp_vd_adj1_contrast(signed int cont_val)
{
	unsigned int vd1_contrast;
	if ((cont_val > 1023) || (cont_val < -1024))
		return;
	cont_val = ((cont_val + 1024) >> 3);

	vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y) & 0xff00) |
					(cont_val << 0);
	WRITE_VPP_REG(VPP_VADJ1_Y, vd1_contrast);
}

void vpp_vd_adj1_brightness(signed int bri_val, struct vframe_s *vf)
{
	signed int vd1_brightness;

	signed int ao0 =  -64;
	signed int ao1 = -512;
	signed int ao2 = -512;
	unsigned int a01 =    0, a_2 =    0;
	/* enable vd0_csc */

	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;

	if (bri_val > 1023 || bri_val < -1024)
		return;

	if ((vf->source_type == VFRAME_SOURCE_TYPE_TUNER) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_CVBS) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_COMP) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_HDMI))
		vd1_brightness = bri_val;
	else {
		bri_val += ao0;
		if (bri_val < -1024)
			bri_val = -1024;
		vd1_brightness = bri_val;
	}

	a01 = ((vd1_brightness << 16) & 0x0fff0000) |
			((ao1 <<  0) & 0x00000fff);
	a_2 = ((ao2 <<	0) & 0x00000fff);
	/*p01 = ((po0 << 16) & 0x0fff0000) |*/
			/*((po1 <<  0) & 0x00000fff);*/
	/*p_2 = ((po2 <<	0) & 0x00000fff);*/

	WRITE_VPP_REG(VPP_MATRIX_CTRL         , ctl);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2  , a_2);
	WRITE_VPP_REG(VPP_MATRIX_CTRL         , ori);
}


/* brightness/contrast adjust process begin */
static void vd1_brightness_contrast(signed int brightness,
		signed int contrast)
{
	signed int ao0 =  -64, g00 = 1024, g01 =    0, g02 =    0, po0 =  64;
	signed int ao1 = -512, g10 =    0, g11 = 1024, g12 =    0, po1 = 512;
	signed int ao2 = -512, g20 =    0, g21 =    0, g22 = 1024, po2 = 512;
	unsigned int gc0 =    0, gc1 =    0, gc2 =    0, gc3 =    0, gc4 = 0;
	unsigned int a01 =    0, a_2 =    0, p01 =    0, p_2 =    0;
	/* enable vd0_csc */
	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;

	po0 += brightness >> 1;
	if (po0 >  1023)
		po0 =  1023;
	if (po0 < -1024)
		po0 = -1024;
	g00  *= contrast + 2048;
	g00 >>= 11;
	if (g00 >  4095)
		g00 =  4095;
	if (g00 < -4096)
		g00 = -4096;
	if (contrast < 0) {
		g11  *= contrast   + 2048;
		g11 >>= 11;
	}
	if (brightness < 0) {
		g11  += brightness >> 1;
		if (g11 >  4095)
			g11 =  4095;
		if (g11 < -4096)
			g11 = -4096;
	}
	if (contrast < 0) {
		g22  *= contrast   + 2048;
		g22 >>= 11;
	}
	if (brightness < 0) {
		g22  += brightness >> 1;
		if (g22 >  4095)
			g22 =  4095;
		if (g22 < -4096)
			g22 = -4096;
	}
	gc0 = ((g00 << 16) & 0x1fff0000) | ((g01 <<  0) & 0x00001fff);
	gc1 = ((g02 << 16) & 0x1fff0000) | ((g10 <<  0) & 0x00001fff);
	gc2 = ((g11 << 16) & 0x1fff0000) | ((g12 <<  0) & 0x00001fff);
	gc3 = ((g20 << 16) & 0x1fff0000) | ((g21 <<  0) & 0x00001fff);
	gc4 = ((g22 <<  0) & 0x00001fff);
	/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_g9tv_cpu() || is_meson_gxtvbb_cpu()) {
		a01 = ((ao0 << 16) & 0x0fff0000) |
				((ao1 <<  0) & 0x00000fff);
		a_2 = ((ao2 <<  0) & 0x00000fff);
		p01 = ((po0 << 16) & 0x0fff0000) |
				((po1 <<  0) & 0x00000fff);
		p_2 = ((po2 <<  0) & 0x00000fff);
	} else {
		/* #else */
		a01 = ((ao0 << 16) & 0x07ff0000) |
				((ao1 <<  0) & 0x000007ff);
		a_2 = ((ao2 <<  0) & 0x000007ff);
		p01 = ((po0 << 16) & 0x07ff0000) |
				((po1 <<  0) & 0x000007ff);
		p_2 = ((po2 <<  0) & 0x000007ff);
	}
	/* #endif */
	WRITE_VPP_REG(VPP_MATRIX_CTRL         , ctl);
	WRITE_VPP_REG(VPP_MATRIX_COEF00_01    , gc0);
	WRITE_VPP_REG(VPP_MATRIX_COEF02_10    , gc1);
	WRITE_VPP_REG(VPP_MATRIX_COEF11_12    , gc2);
	WRITE_VPP_REG(VPP_MATRIX_COEF20_21    , gc3);
	WRITE_VPP_REG(VPP_MATRIX_COEF22       , gc4);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2  , a_2);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1    , p01);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET2      , p_2);
	WRITE_VPP_REG(VPP_MATRIX_CTRL         , ori);
}

void amvecm_bricon_process(unsigned int bri_val,
		unsigned int cont_val, struct vframe_s *vf)
{
	if (vecm_latch_flag & FLAG_VADJ1_BRI) {
		vecm_latch_flag &= ~FLAG_VADJ1_BRI;
		vpp_vd_adj1_brightness(bri_val, vf);
		pr_amve_dbg("\n[amve..] set vd1_brightness OK!!!\n");
	}

	if (vecm_latch_flag & FLAG_VADJ1_CON) {
		vecm_latch_flag &= ~FLAG_VADJ1_CON;
		vpp_vd_adj1_contrast(cont_val);
		pr_amve_dbg("\n[amve..] set vd1_contrast OK!!!\n");
	}

	if (0) { /* vecm_latch_flag & FLAG_BRI_CON) { */
		vecm_latch_flag &= ~FLAG_BRI_CON;
		vd1_brightness_contrast(bri_val, cont_val);
		pr_amve_dbg("\n[amve..] set vd1_brightness_contrast OK!!!\n");
	}
}
/* brightness/contrast adjust process end */

/* 3d process begin */
void amvecm_3d_black_process(void)
{
	if (vecm_latch_flag & FLAG_3D_BLACK_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 0, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 0, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_BLACK_EN) {
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 1, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 1, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 1, 31, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL,
				sync_3d_black_color&0xffffff, 0, 24);
		if (sync_3d_sync_to_vbo)
			WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_EN;
	}
}
void amvecm_3d_sync_process(void)
{

	if (vecm_latch_flag & FLAG_3D_SYNC_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_SYNC_EN) {
		/*select vpu pwm source clock*/
		switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
		case 0:/* ENCL */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 0, 29, 2);
			break;
		case 1:/* ENCI */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 1, 29, 2);
			break;
		case 2:/* ENCP */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 2, 29, 2);
			break;
		case 3:/* ENCT */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 3, 29, 2);
			break;
		default:
			break;
		}
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_EN;
	}
}
/* 3d process end */

