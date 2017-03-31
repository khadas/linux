/*
 * TVAFE cvd2 device driver.
 *
 * Copyright (c) 2010 Frank zhao <frank.zhao@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/******************************Includes************************************/
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <asm/div64.h>
#include <linux/module.h>

/*#include <mach/am_regs.h>*/

#include <linux/amlogic/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "tvafe_regs.h"
#include "tvafe_adc.h"
#include "tvafe_cvd.h"
#include "../vdin/vdin_regs.h"
/***************************Local defines**********************************/
/* cvd2 memory size defines */
#define DECODER_MOTION_BUFFER_ADDR_OFFSET   0x70000
#define DECODER_MOTION_BUFFER_4F_LENGTH     0x15a60
#define DECODER_VBI_ADDR_OFFSET             0x86000
#define DECODER_VBI_VBI_SIZE                0x1000
#define DECODER_VBI_START_ADDR              0x0

/* cvd2 mode detection defines */
#define TVAFE_CVD2_CORDIC_IN_MAX            224
/* CVD cordic range of right fmt */
#define TVAFE_CVD2_CORDIC_IN_MIN            85  /* 0x20 */

#define TVAFE_CVD2_CORDIC_OUT_MAX           192
/* CVD cordic range of error fmt */
#define TVAFE_CVD2_CORDIC_OUT_MIN           96  /* 0x40 */

#define TVAFE_CVD2_STATE_CNT                15
/* cnt*10ms,delay between fmt check funtion */
#define TVAFE_CVD2_SHIFT_CNT                6
/* cnt*10ms,delay for fmt shift counter */

#define TVAFE_CVD2_NONSTD_DGAIN_MAX         0x500
/* CVD max digital gain for non-std signal */
#define TVAFE_CVD2_NONSTD_CNT_MAX           0x0A
/* CVD max cnt non-std signal */

#define TVAFE_CVD2_FMT_LOOP_CNT             1
/* n*loop, max fmt check loop counter */

/* cvd2 function in vsync */
#define CVD_3D_COMB_CHECK_MAX_CNT           100
/* cnt*vs_interval,3D comb error check delay */

#define TVAFE_CVD2_CDTO_ADJ_TH              0x4CEDB3
/* CVD cdto adjust threshold */
#define TVAFE_CVD2_CDTO_ADJ_STEP            3
/* CVD cdto adjust step cdto/2^n */

#define TVAFE_CVD2_PGA_MIN                  0
/* CVD max cnt non-std signal */
#define TVAFE_CVD2_PGA_MAX                  255
/* CVD max cnt non-std signal */

#define PAL_BURST_MAG_UPPER_LIMIT           0x1C80
#define PAL_BURST_MAG_LOWER_LIMIT           0x1980

#define NTSC_BURST_MAG_UPPER_LIMIT          0x1580
#define NTSC_BURST_MAG_LOWER_LIMIT          0x1280

#define SYNC_HEIGHT_LOWER_LIMIT             0x60
#define SYNC_HEIGHT_UPPER_LIMIT             0xFF

#define SYNC_HEIGHT_ADJ_CNT                 0x2

/* cvd2 function in 10ms timer */
#define TVAFE_CVD2_NOSIG_REG_CHECK_CNT      50   /* n*10ms */
#define TVAFE_CVD2_NORMAL_REG_CHECK_CNT     100  /* n*10ms */

/* zhuangwei, nonstd flag */
#define CVD2_NONSTD_CNT_INC_STEP 1
#define CVD2_NONSTD_CNT_DEC_STEP 1
#define CVD2_NONSTD_CNT_INC_LIMIT 50
#define CVD2_NONSTD_CNT_DEC_LIMIT 1
/* should be large than CVD2_NONSTD_CNT_DEC_STEP */
#define CVD2_NONSTD_FLAG_ON_TH 10
#define CVD2_NONSTD_FLAG_OFF_TH 2

#define SCENE_COLORFUL_TH 0x80000
/********Local variables*********************************/
static const unsigned int cvd_mem_4f_length[TVIN_SIG_FMT_CVBS_SECAM-
			TVIN_SIG_FMT_CVBS_NTSC_M+1] = {

	0x0000e946, /* TVIN_SIG_FMT_CVBS_NTSC_M, */
	0x0000e946, /* TVIN_SIG_FMT_CVBS_NTSC_443, */
	0x00015a60, /* TVIN_SIG_FMT_CVBS_PAL_I, */
	0x0000e905, /* TVIN_SIG_FMT_CVBS_PAL_M, */
	0x00015a60, /* TVIN_SIG_FMT_CVBS_PAL_60, */
	0x000117d9, /* TVIN_SIG_FMT_CVBS_PAL_CN, */
	0x00015a60, /* TVIN_SIG_FMT_CVBS_SECAM, */
};

/* debug value */
#ifdef CONFIG_AM_SI2176
static int cordic_tv_in_min = 133;
static int cordic_tv_in_max = 224;
static int cordic_tv_out_min = 134;
static int cordic_tv_out_max = 192;
#else
static int cordic_tv_in_min = TVAFE_CVD2_CORDIC_IN_MIN;
static int cordic_tv_in_max = TVAFE_CVD2_CORDIC_IN_MAX;
static int cordic_tv_out_min = TVAFE_CVD2_CORDIC_OUT_MIN;
static int cordic_tv_out_max = TVAFE_CVD2_CORDIC_OUT_MAX;
#endif

/* static char version_s[] = "cvd2-2013-11-5a"; */
static int cordic_av_in_min = TVAFE_CVD2_CORDIC_IN_MIN;
static int cordic_av_in_max = TVAFE_CVD2_CORDIC_IN_MAX;
static int cordic_av_out_min = TVAFE_CVD2_CORDIC_OUT_MIN;
static int cordic_av_out_max = TVAFE_CVD2_CORDIC_OUT_MAX;
static int cnt_dbg_en;
static int force_fmt_flag;
static unsigned int scene_colorful = 1;
static int scene_colorful_old;
static int auto_de_en = 1;
static int lock_cnt;
static unsigned int cvd_reg8a = 0xa;

module_param(auto_de_en, int, 0664);
MODULE_PARM_DESC(auto_de_en, "auto_de_en\n");

module_param(cnt_dbg_en, int, 0664);
MODULE_PARM_DESC(cnt_dbg_en, "cnt_dbg_en\n");

module_param(cordic_av_in_min, int, 0664);
MODULE_PARM_DESC(cordic_av_in_min, "cordic_av_in_min");

module_param(cordic_av_in_max, int, 0644);
MODULE_PARM_DESC(cordic_av_in_max, "cordic_av_in_max");

module_param(cordic_av_out_min, int, 0664);
MODULE_PARM_DESC(cordic_av_out_min, "cordic_av_out_min");

module_param(cordic_av_out_max, int, 0644);
MODULE_PARM_DESC(cordic_av_out_max, "cordic_av_out_max");

module_param(cordic_tv_in_min, int, 0664);
MODULE_PARM_DESC(cordic_tv_in_min, "cordic_tv_in_min");

module_param(cordic_tv_in_max, int, 0644);
MODULE_PARM_DESC(cordic_tv_in_max, "cordic_tv_in_max");

module_param(cordic_tv_out_min, int, 0664);
MODULE_PARM_DESC(cordic_tv_out_min, "cordic_tv_out_min");

module_param(cordic_tv_out_max, int, 0644);
MODULE_PARM_DESC(cordic_tv_out_max, "cordic_tv_out_max");

static int cdto_adj_th = TVAFE_CVD2_CDTO_ADJ_TH;
module_param(cdto_adj_th, int, 0664);
MODULE_PARM_DESC(cdto_adj_th, "cvd2_adj_diff_threshold");

static int cdto_adj_step = TVAFE_CVD2_CDTO_ADJ_STEP;
module_param(cdto_adj_step, int, 0664);
MODULE_PARM_DESC(cdto_adj_step, "cvd2_adj_step");

static bool cvd_dbg_en;
module_param(cvd_dbg_en, bool, 0664);
MODULE_PARM_DESC(cvd_dbg_en, "cvd2 debug enable");

static bool cvd_nonstd_dbg_en;
module_param(cvd_nonstd_dbg_en, bool, 0664);
MODULE_PARM_DESC(cvd_nonstd_dbg_en, "cvd2 nonstd debug enable");


static int ntsc_sw_maxcnt = 20;
module_param(ntsc_sw_maxcnt, int, 0664);
MODULE_PARM_DESC(ntsc_sw_maxcnt, "ntsc_sw_maxcnt");

static int ntsc_sw_midcnt = 40;
module_param(ntsc_sw_midcnt, int, 0664);
MODULE_PARM_DESC(ntsc_sw_midcnt, "ntsc_sw_midcnt");

static int fmt_try_maxcnt = 5;
module_param(fmt_try_maxcnt, int, 0664);
MODULE_PARM_DESC(fmt_try_maxcnt, "fmt_try_maxcnt");

static int fmt_wait_cnt = 15;
module_param(fmt_wait_cnt, int, 0664);
MODULE_PARM_DESC(fmt_wait_cnt, "fmt_wait_cnt");

static int cvd2_shift_cnt = TVAFE_CVD2_SHIFT_CNT;
module_param(cvd2_shift_cnt, int, 0664);
MODULE_PARM_DESC(cvd2_shift_cnt, "cvd2_shift_cnt");

/*force the fmt for chrome off,for example ntsc pal_i 12*/
static unsigned int config_force_fmt;
module_param(config_force_fmt, uint, 0664);
MODULE_PARM_DESC(config_force_fmt,
		"after try try_format_cnt times ,we will force one fmt");

static int cvd_reg07_pal = 0x03;
module_param(cvd_reg07_pal, int, 0664);
MODULE_PARM_DESC(cvd_reg07_pal, "cvd_reg07_pal");

static unsigned short cnt_vld_th = 0x30;
module_param(cnt_vld_th, ushort, 0664);
MODULE_PARM_DESC(cnt_vld_th, "threshold for 4xx or 3xx vaild.");
/*0:normal 1:force nonstandard configure
2:force don't nonstandard configure*/
static unsigned int  force_nostd = 2;
module_param(force_nostd, uint, 0644);
MODULE_PARM_DESC(force_nostd,
			"fixed nosig problem by removing the nostd config.\n");

/*0x001:enable cdto adj 0x010:enable 3d adj 0x100:enable pga;
0x1000:enable hs adj,which can instead cdto*/
static unsigned int  cvd_isr_en = 0x1110;
module_param(cvd_isr_en, uint, 0644);
MODULE_PARM_DESC(cvd_isr_en, "cvd_isr_en\n");

static bool sync_sensitivity = true;
module_param(sync_sensitivity, bool, 0644);
MODULE_PARM_DESC(sync_sensitivity, "sync_sensitivity\n");

#ifdef TVAFE_SET_CVBS_CDTO_EN
static uint cdto_clamp = HS_CNT_STANDARD;
module_param(cdto_clamp, uint, 0644);
MODULE_PARM_DESC(cdto_clamp, "cdto_clamp\n");

static int  cdto_filter_factor = 1;
module_param(cdto_filter_factor, int, 0644);
MODULE_PARM_DESC(cdto_filter_factor, "cdto_filter_factor\n");
#endif

static int noise_judge;
module_param(noise_judge, int, 0644);
MODULE_PARM_DESC(noise_judge, "cdto_filter_factor\n");

static int ignore_pal_nt;
module_param(ignore_pal_nt, int, 0644);
MODULE_PARM_DESC(ignore_pal_nt, "ignore_pal_nt\n");


static int ignore_443_358;
module_param(ignore_443_358, int, 0644);
MODULE_PARM_DESC(ignore_443_358, "ignore_443_358\n");

static int pga_default_vale = 0x10;
module_param(pga_default_vale, int, 0644);
MODULE_PARM_DESC(pga_default_vale, "pga_default_vale\n");

static int pga_delta_val = 0x10;
module_param(pga_delta_val, int, 0644);
MODULE_PARM_DESC(pga_delta_val, "pga delta val\n");
static int dg_ave_last = 0x200;

/*for force fmt*/
/* static enum tvin_sig_fmt_e line525fmt = TVIN_SIG_FMT_NULL; */
/* static enum tvin_sig_fmt_e line625fmt = TVIN_SIG_FMT_NULL; */
static unsigned int try_format_max = 5;
module_param(try_format_max, uint, 0664);
MODULE_PARM_DESC(try_format_max, "try_format_max");

static unsigned int acd_h_config = 0x8e035e;
module_param(acd_h_config, uint, 0664);
MODULE_PARM_DESC(acd_h_config, "acd_h_config");

static unsigned int acd_h = 0X880358;
module_param(acd_h, uint, 0664);
MODULE_PARM_DESC(acd_h, "acd_h");

/*0:NORMAL  1:a little sharper 2:sharper 3:even sharper*/
static unsigned int cvd2_filter_config_level;
module_param(cvd2_filter_config_level, uint, 0664);
MODULE_PARM_DESC(cvd2_filter_config_level, "cvd2_filter_config_level");

static unsigned int hs_adj_th_level0 = 0x260;
module_param(hs_adj_th_level0, uint, 0664);
MODULE_PARM_DESC(hs_adj_th_level0, "hs_adj_th_level0");

static unsigned int hs_adj_th_level1 = 0x4f0;
module_param(hs_adj_th_level1, uint, 0664);
MODULE_PARM_DESC(hs_adj_th_level1, "hs_adj_th_level1");

static unsigned int hs_adj_th_level2 = 0x770;
module_param(hs_adj_th_level2, uint, 0664);
MODULE_PARM_DESC(hs_adj_th_level2, "hs_adj_th_level2");

static unsigned int hs_adj_th_level3 = 0x9e0;
module_param(hs_adj_th_level3, uint, 0664);
MODULE_PARM_DESC(hs_adj_th_level3, "hs_adj_th_level3");

static unsigned int hs_adj_th_level4 = 0xc50;
module_param(hs_adj_th_level4, uint, 0664);
MODULE_PARM_DESC(hs_adj_th_level4, "hs_adj_th_level4");

static unsigned int try_format_cnt;

static bool cvd_pr_flag;
static bool cvd_pr1_chroma_flag;
static bool cvd_pr2_chroma_flag;

/* zhuangwei, nonstd experiment */
static short nonstd_cnt;
static short nonstd_flag;
static unsigned int chroma_sum_pre1;
static unsigned int chroma_sum_pre2;
static unsigned int chroma_sum_pre3;
/* fanghui,noise det to juge some reg setting */
static unsigned int noise1;
static unsigned int noise2;
static unsigned int noise3;
/* test */
static short print_cnt;

/*****************the  version of changing log**********************/
static  const char last_version_s[] = "2015-07-06a";
static  const char version_s[] = "2017-01-19a";
/**********************************************************/

void get_cvd_version(const char **ver, const char **last_ver)
{
	*ver = version_s;
	*last_ver = last_version_s;
	return;
}

/*
 * tvafe cvd2 memory setting for 3D comb/motion detection/vbi function
 */
static void tvafe_cvd2_memory_init(struct tvafe_cvd2_mem_s *mem,
				enum tvin_sig_fmt_e fmt)
{
	unsigned int cvd2_addr = mem->start >> 4;/*gxtvbb >>4;g9tv >>3*/
	unsigned int motion_offset = DECODER_MOTION_BUFFER_ADDR_OFFSET;
	unsigned int vbi_offset = DECODER_VBI_ADDR_OFFSET;
	unsigned int vbi_size = DECODER_VBI_VBI_SIZE;

	if ((mem->start == 0) || (mem->size == 0)) {

		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: cvd2 memory size error!!!\n",
			__func__);
		return;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		cvd2_addr = mem->start >> 4;
		motion_offset = motion_offset >> 4;
		vbi_offset = vbi_offset >> 4;
		vbi_size = vbi_size >> 4;
	} else {
		cvd2_addr = mem->start >> 3;
		motion_offset = motion_offset >> 3;
		vbi_offset = vbi_offset >> 3;
		vbi_size = vbi_size >> 3;
	}
	/* CVD2 mem addr is based on 64bit, system mem is based on 8bit*/
	W_APB_REG(CVD2_REG_96, cvd2_addr);
	W_APB_REG(ACD_REG_30, (cvd2_addr + motion_offset));
	/* 4frame mode memory setting */
	W_APB_BIT(ACD_REG_2A ,
		cvd_mem_4f_length[fmt - TVIN_SIG_FMT_CVBS_NTSC_M],
		REG_4F_MOTION_LENGTH_BIT, REG_4F_MOTION_LENGTH_WID);

	/* vbi memory setting */
	W_APB_REG(ACD_REG_2F, (cvd2_addr + vbi_offset));
	W_APB_BIT(ACD_REG_21, vbi_size,
		AML_VBI_SIZE_BIT, AML_VBI_SIZE_WID);
	W_APB_BIT(ACD_REG_21, DECODER_VBI_START_ADDR,
		AML_VBI_START_ADDR_BIT, AML_VBI_START_ADDR_WID);
	/*open front lpf for av ring*/
	W_APB_BIT(ACD_REG_26, 1, 8, 1);

	return;
}
/*
txl new add adj filter:
normal filter: 0x3194 = 0x40100160, 0x3195 = 0x50, 0x3196=0x0.
sharper filter: 0x3194 = 0x40123282, 0x3195 = 0x19dd03a6, 0x96=0x0.
even sharper filter: 0x3194 = 0x401012da, 0x3195 = 0x2023036c,
0x3196 = 0xf20815d4
*/
static void tvafe_cvd2_filter_config(void)
{
	switch (cvd2_filter_config_level) {
	case 0:
		W_APB_REG(ACD_REG_94, 0x40100160);
		W_APB_REG(ACD_REG_95, 0x50);
		W_APB_REG(ACD_REG_96, 0x0);
		break;
	case 1:
		W_APB_REG(ACD_REG_94, 0x40126266);
		W_APB_REG(ACD_REG_95, 0x3ad303a1);
		W_APB_REG(ACD_REG_96, 0x080ee9fa);
		break;
	case 2:
		W_APB_REG(ACD_REG_94, 0x40114288);
		W_APB_REG(ACD_REG_95, 0x38f30388);
		W_APB_REG(ACD_REG_96, 0x0416f7e4);
		break;
	case 3:
		W_APB_REG(ACD_REG_94, 0x40123282);
		W_APB_REG(ACD_REG_95, 0x19dd03a6);
		W_APB_REG(ACD_REG_96, 0x0);
		break;
	default:
		W_APB_REG(ACD_REG_94, 0x40100160);
		W_APB_REG(ACD_REG_95, 0x50);
		W_APB_REG(ACD_REG_96, 0x0);
		break;
	}
	if (cvd_dbg_en)
		pr_info("[tvafe..]%s cvd2 filter config level %d.\n",
				__func__, cvd2_filter_config_level);

}

/*
 * tvafe cvd2 load Reg talbe
 */
static void tvafe_cvd2_write_mode_reg(struct tvafe_cvd2_s *cvd2,
		struct tvafe_cvd2_mem_s *mem)
{
	unsigned int i = 0;

	/* reset CVD2 */
	W_APB_BIT(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);

	/* for rf&cvbs source acd table */
	if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) {

		for (i = 0; i < (ACD_REG_NUM+1); i++) {

			W_APB_REG(((ACD_BASE_ADD+i)<<2),
					(rf_acd_table[cvd2->config_fmt-
					TVIN_SIG_FMT_CVBS_NTSC_M][i]));
		}


	} else{

		for (i = 0; i < (ACD_REG_NUM+1); i++) {


			W_APB_REG(((ACD_BASE_ADD+i)<<2),
					(cvbs_acd_table[cvd2->config_fmt-
					TVIN_SIG_FMT_CVBS_NTSC_M][i]));
		}

	}
	/*cvd filter config*/
	tvafe_cvd2_filter_config();

	/* load CVD2 reg 0x00~3f (char) */
	for (i = 0; i < CVD_PART1_REG_NUM; i++) {

		W_APB_REG(((CVD_BASE_ADD+CVD_PART1_REG_MIN+i)<<2),
				(cvd_part1_table[cvd2->config_fmt-
				TVIN_SIG_FMT_CVBS_NTSC_M][i]));
	}

	/* load CVD2 reg 0x70~ff (char) */
	for (i = 0; i < CVD_PART2_REG_NUM; i++) {

		W_APB_REG(((CVD_BASE_ADD+CVD_PART2_REG_MIN+i)<<2),
				(cvd_part2_table[cvd2->config_fmt-
				TVIN_SIG_FMT_CVBS_NTSC_M][i]));
	}

	/* reload CVD2 reg 0x87, 0x93, 0x94, 0x95, 0x96, 0xe6, 0xfa (int) */
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_0)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][0]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_1)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][1]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_2)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][2]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_3)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][3]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_4)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][4]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_5)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][5]);
	W_APB_REG(((CVD_BASE_ADD+CVD_PART3_REG_6)<<2),
		cvd_part3_table[cvd2->config_fmt-TVIN_SIG_FMT_CVBS_NTSC_M][6]);

	/* s-video setting => reload reg 0x00~03 & 0x18~1f */
	if ((cvd2->vd_port >= TVIN_PORT_SVIDEO0) &&
		(cvd2->vd_port <= TVIN_PORT_SVIDEO7)) {

		for (i = 0; i < 4; i++) {

			W_APB_REG(((CVD_BASE_ADD+i)<<2),
					(cvd_yc_reg_0x00_0x03[cvd2->config_fmt-
					TVIN_SIG_FMT_CVBS_NTSC_M][i]));
		}
		for (i = 0; i < 8; i++) {

			W_APB_REG(((CVD_BASE_ADD+i+0x18)<<2),
					(cvd_yc_reg_0x18_0x1f[cvd2->config_fmt-
					TVIN_SIG_FMT_CVBS_NTSC_M][i]));
		}
	}

	/* for tuner picture quality */

	if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) {

		W_APB_REG(CVD2_REG_B0, 0xf0);
		W_APB_BIT(CVD2_REG_B2 , 0,
			ADAPTIVE_CHROMA_MODE_BIT, ADAPTIVE_CHROMA_MODE_WID);
		W_APB_BIT(CVD2_CONTROL1, 0, CHROMA_BW_LO_BIT, CHROMA_BW_LO_WID);

	} else{
		W_APB_REG(CVD2_VSYNC_NO_SIGNAL_THRESHOLD, 0xf0);
		if (cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)
				/*add for chroma state adjust dynamicly*/
			W_APB_REG(CVD2_CHROMA_LOOPFILTER_STATE, cvd_reg8a);
	}
#ifdef TVAFE_CVD2_CC_ENABLE
	W_APB_REG(CVD2_VBI_DATA_TYPE_LINE21, 0x00000011);
	W_APB_REG(CVD2_VSYNC_VBI_LOCKOUT_START, 0x00000000);
	W_APB_REG(CVD2_VSYNC_VBI_LOCKOUT_END, 0x00000025);
	W_APB_REG(CVD2_VSYNC_TIME_CONSTANT, 0x0000004a);
	W_APB_REG(CVD2_VBI_CC_START, 0x00000054);
	W_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x00000015);
	W_APB_REG(ACD_REG_22, 0x82080000); /* manuel reset vbi */
	W_APB_REG(ACD_REG_22, 0x04080000);
	/* vbi reset release, vbi agent enable */
#endif

#if defined(CONFIG_TVIN_TUNER_SI2176)
	if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) {

		W_APB_BIT(CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 3,
				HNON_STD_TH_BIT, HNON_STD_TH_WID);
	}
#elif defined(CONFIG_TVIN_TUNER_HTM9AW125)
	if (((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) &&
		(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_NTSC_M)) {

		W_APB_BIT(ACD_REG_1B , 0xc, YCSEP_TEST6F_BIT, YCSEP_TEST6F_WID);
	}
#endif

	/* add for board e04&e08  */
	if (((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) &&
		(cvd_reg07_pal != 0x03)
		&& (cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I))
		W_APB_REG(CVD2_OUTPUT_CONTROL, cvd_reg07_pal);

	/*disable vbi*/
	W_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x14);

	/* 3D comb filter buffer assignment */
	tvafe_cvd2_memory_init(mem, cvd2->config_fmt);

#if 1/* TVAFE_CVD2_WSS_ENABLE */
	/* config data type */
	/*line17 for PAL M*/
	W_APB_REG(CVD2_VBI_DATA_TYPE_LINE17, 0xcc);
	/*line23 for PAL B,D,G,H,I,N,CN*/
	W_APB_REG(CVD2_VBI_DATA_TYPE_LINE23, 0xcc);
	/* config wss dto */
	W_APB_REG(CVD2_VBI_WSS_DTO_MSB, 0x20);
	W_APB_REG(CVD2_VBI_WSS_DTO_LSB, 0x66);

	/* config vbi start line */
	W_APB_REG(CVD2_VBI_WSS_START, 0x54);
	W_APB_BIT(CVD2_VBI_CONTROL, 1, 0, 1);
	W_APB_REG(CVD2_VSYNC_VBI_LOCKOUT_START, 0x00000000);
	W_APB_REG(CVD2_VSYNC_VBI_LOCKOUT_END, 0x00000025);
	/* be care the polarity bellow!!! */
	W_APB_BIT(CVD2_VSYNC_TIME_CONSTANT, 0, 7, 1);
	/*enable vbi*/
	W_APB_REG(CVD2_VBI_FRAME_CODE_CTL, 0x15);
	/* manuel reset vbi */
	W_APB_REG(ACD_REG_22, 0x82080000);
	W_APB_REG(ACD_REG_22, 0x04080000);
#endif

	/*set for wipe off vertical stripes*/
	if ((cvd2->vd_port > TVIN_PORT_CVBS0) &&
		(cvd2->vd_port <= TVIN_PORT_CVBS7) &&
		(cvd2->vd_port != TVIN_PORT_CVBS3) &&
		(get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL))
		W_APB_REG(ACD_REG_25, 0x00e941a8);

	/* enable CVD2 */
	W_APB_BIT(CVD2_RESET_REGISTER, 0, SOFT_RST_BIT, SOFT_RST_WID);

	return;
}

/*
 * tvafe cvd2 configure Reg for non-standard signal
 */
static void tvafe_cvd2_non_std_config(struct tvafe_cvd2_s *cvd2)
{
	static unsigned int time_non_count = 50;
	unsigned int noise_read = 0;
	unsigned int noise_strenth = 0;

	noise_read = R_APB_REG(CVD2_SYNC_NOISE_STATUS);
	noise3 = noise2;
	noise2 = noise1;
	noise1 = noise_read;
	noise_strenth = (noise1+(noise2<<1)+noise3)>>2;

	if (time_non_count) {
		time_non_count--;
		return;
	}
	time_non_count = 200;
	if (force_nostd == 3)
		return;
	if ((cvd2->info.non_std_config == cvd2->info.non_std_enable) &&
		(force_nostd&0x2))
		return;
	cvd2->info.non_std_config = cvd2->info.non_std_enable;
	if (cvd2->info.non_std_config && (!(force_nostd&0x1))) {

		if (cvd_nonstd_dbg_en) {
			pr_info("[tvafe..] %s: config non-std signal reg.\n",
				__func__);
			pr_info("[tvafe..] %s: noise_strenth=%d.\n",
						__func__, noise_strenth);
		}

#ifdef CONFIG_AM_SI2176
		if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
			(cvd2->vd_port == TVIN_PORT_CVBS0)) {

			W_APB_BIT(CVD2_NON_STANDARD_SIGNAL_THRESHOLD, 3,
					HNON_STD_TH_BIT, HNON_STD_TH_WID);
			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 1,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x04);
			if (scene_colorful)
				W_APB_REG(CVD2_VSYNC_CNTL, 0x02);
			if (noise_strenth > 48 && noise_judge)
				W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 4,
					HSTATE_MAX_BIT, HSTATE_MAX_WID);
			else
				W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 5,
					HSTATE_MAX_BIT, HSTATE_MAX_WID);
			W_APB_BIT(CVD2_ACTIVE_VIDEO_VSTART, 0x14,
					VACTIVE_START_BIT, VACTIVE_START_WID);
			W_APB_BIT(CVD2_ACTIVE_VIDEO_VHEIGHT, 0xe0,
					VACTIVE_HEIGHT_BIT, VACTIVE_HEIGHT_WID);

		} else{

			/* W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 1,
			VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID); */
			W_APB_REG(CVD2_HSYNC_RISING_EDGE_START, 0x25);
			W_APB_REG(TVFE_CLAMP_INTF, 0x8661);
			if (noise_strenth > 48 && noise_judge)
				W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 4,
					HSTATE_MAX_BIT, HSTATE_MAX_WID);
			else
				W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 5,
					HSTATE_MAX_BIT, HSTATE_MAX_WID);
			if (scene_colorful)
				W_APB_REG(CVD2_VSYNC_CNTL, 0x02);
			W_APB_REG(CVD2_VSYNC_SIGNAL_THRESHOLD, 0x10);
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x08);
		}
#else
		W_APB_REG(CVD2_HSYNC_RISING_EDGE_START, 0x25);
		W_APB_REG(TVFE_CLAMP_INTF, 0x8000);
		W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 4,
			HSTATE_MAX_BIT, HSTATE_MAX_WID);
		if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
			(cvd2->vd_port == TVIN_PORT_CVBS0)) {

#if ((defined CONFIG_AM_R840) || (defined CONFIG_AM_MXL661))
			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 0,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
#else
			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 1,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
#endif
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x00);

		} else{

			if (scene_colorful)
				W_APB_REG(CVD2_VSYNC_CNTL, 0x02);
			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 1,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
			W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 1,
				DISABLE_HFINE_BIT, DISABLE_HFINE_WID);
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x08);
		}
#endif

	} else{

	if (cvd_nonstd_dbg_en)
		pr_info("[tvafe..] %s: out of non-std signal.\n", __func__);
		W_APB_REG(CVD2_HSYNC_RISING_EDGE_START, 0x6d);
		W_APB_REG(TVFE_CLAMP_INTF, 0x8666);
		W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 5,
			HSTATE_MAX_BIT, HSTATE_MAX_WID);
		if ((cvd2->vd_port == TVIN_PORT_CVBS3) ||
			(cvd2->vd_port == TVIN_PORT_CVBS0)) {


#ifdef CVD_SI2176_RSSI
			if (cvd_get_rf_strength() < 187 &&
				cvd_get_rf_strength() > 100 &&
				sync_sensitivity) {

				W_APB_REG(CVD2_VSYNC_SIGNAL_THRESHOLD, 0xf0);
				W_APB_REG(CVD2_VSYNC_CNTL, 0x2);
				if (cvd_nonstd_dbg_en)
					pr_info("[tvafe..] %s: out of non-std signal.rssi=%d\n",
					__func__, cvd_get_rf_strength());
			}
#else

			if (R_APB_REG(CVD2_SYNC_NOISE_STATUS) > 48 &&
				sync_sensitivity) {
				W_APB_REG(CVD2_VSYNC_SIGNAL_THRESHOLD, 0xf0);
				W_APB_REG(CVD2_VSYNC_CNTL, 0x2);
				if (cvd_nonstd_dbg_en)
					pr_info("[tvafe..] %s: use the cvd register to judge the rssi.rssi=%u\n",
				__func__, R_APB_REG(CVD2_SYNC_NOISE_STATUS));
			}
#endif
			else{
				W_APB_REG(CVD2_VSYNC_CNTL, 0x01);
				W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 0,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
			}
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x32);
			W_APB_BIT(CVD2_ACTIVE_VIDEO_VSTART, 0x2a,
					VACTIVE_START_BIT, VACTIVE_START_WID);
			W_APB_BIT(CVD2_ACTIVE_VIDEO_VHEIGHT, 0xc0,
					VACTIVE_HEIGHT_BIT, VACTIVE_HEIGHT_WID);

		} else{

			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 0,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
			W_APB_REG(CVD2_VSYNC_CNTL, 0x01);
			W_APB_BIT(CVD2_VSYNC_SIGNAL_THRESHOLD, 0,
				VS_SIGNAL_AUTO_TH_BIT, VS_SIGNAL_AUTO_TH_WID);
			W_APB_BIT(CVD2_H_LOOP_MAXSTATE, 0,
				DISABLE_HFINE_BIT, DISABLE_HFINE_WID);
			W_APB_REG(CVD2_NOISE_THRESHOLD, 0x32);
		}
	}

}

/*
 * tvafe cvd2 reset pga setting
 */
inline void tvafe_cvd2_reset_pga(void)
{
	/* reset pga value */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB)
		W_APB_BIT(TVFE_VAFE_CTRL1, pga_default_vale,
			VAFE_PGA_GAIN_BIT, VAFE_PGA_GAIN_WID);
	else {
		if ((R_APB_BIT(ADC_REG_05, PGAGAIN_BIT, PGAGAIN_WID) !=
		pga_default_vale) ||
		(R_APB_BIT(ADC_REG_06 , PGAMODE_BIT, PGAMODE_WID) != 0)) {

			W_APB_BIT(ADC_REG_05 , pga_default_vale,
				PGAGAIN_BIT, PGAGAIN_WID);
			W_APB_BIT(ADC_REG_06, 0, PGAMODE_BIT, PGAMODE_WID);
			if (cvd_dbg_en)
				pr_info("[tvafe..] %s: reset pga value\n",
				__func__);
		}
	}
}

#ifdef TVAFE_SET_CVBS_CDTO_EN
/*
 * tvafe cvd2 read cdto setting from Reg
 */
static unsigned int tvafe_cvd2_get_cdto(void)
{
	unsigned int cdto = 0;

	cdto = (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_29_24,
		CDTO_INC_29_24_BIT, CDTO_INC_29_24_WID) & 0x0000003f)<<24;
	cdto += (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_23_16,
		CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID) & 0x000000ff)<<16;
	cdto += (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_15_8,
		CDTO_INC_15_8_BIT, CDTO_INC_15_8_WID) & 0x000000ff)<<8;
	cdto += (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_7_0,
		CDTO_INC_7_0_BIT, CDTO_INC_7_0_WID) & 0x000000ff);
	return cdto;
}

/*
 * tvafe cvd2 write cdto value to Reg
 */
static void tvafe_cvd2_set_cdto(unsigned int cdto)
{
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_29_24, (cdto >> 24) & 0x0000003f);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_23_16, (cdto >> 16) & 0x000000ff);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_15_8,  (cdto >>  8) & 0x000000ff);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_7_0,   (cdto >>  0) & 0x000000ff);
}

/*
   set default cdto
 */
void tvafe_cvd2_set_default_cdto(struct tvafe_cvd2_s *cvd2)
{
	if (!cvd2) {

		pr_info("[tvafe..]%s cvd2 null error.\n", __func__);
		return;
	}
		switch (cvd2->config_fmt) {

		case TVIN_SIG_FMT_CVBS_NTSC_M:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_NTSC_M)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_NTSC_M);
			break;
		case TVIN_SIG_FMT_CVBS_NTSC_443:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_NTSC_443)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_NTSC_443);
			break;
		case TVIN_SIG_FMT_CVBS_PAL_I:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_PAL_I)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_I);
			break;
		case TVIN_SIG_FMT_CVBS_PAL_M:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_PAL_M)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_M);
			break;
		case TVIN_SIG_FMT_CVBS_PAL_60:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_PAL_60)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_60);
			break;
		case TVIN_SIG_FMT_CVBS_PAL_CN:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_PAL_CN)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_CN);
			break;
		case TVIN_SIG_FMT_CVBS_SECAM:
			if (tvafe_cvd2_get_cdto() != CVD2_CHROMA_DTO_SECAM)
				tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_SECAM);
			break;
		default:
			break;
	}
	if (cvd_dbg_en)
		pr_info("[tvafe..]%s set cdto to default fmt %s.\n",
				__func__, tvin_sig_fmt_str(cvd2->config_fmt));
}
#endif

/*
 * tvafe cvd2 write Reg table by different format
 */
inline void tvafe_cvd2_try_format(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem, enum tvin_sig_fmt_e fmt)
{
	/* check format validation */
	if ((fmt < TVIN_SIG_FMT_CVBS_NTSC_M) ||
		(fmt > TVIN_SIG_FMT_CVBS_SECAM)) {

		if (cvd_dbg_en)
			pr_err("[tvafe..] %s: cvd2 try format error!!!\n",
			__func__);
		return;
	}

	if (fmt != cvd2->config_fmt) {
		lock_cnt = 0;
		pr_info("[tvafe..] %s: try new fmt:%s\n",
				__func__, tvin_sig_fmt_str(fmt));
		cvd2->config_fmt = fmt;
		tvafe_cvd2_write_mode_reg(cvd2, mem);
		/* init variable */
		memset(&cvd2->info, 0, sizeof(struct tvafe_cvd2_info_s));
	}
}

/*
 * tvafe cvd2 get signal status from Reg
 */
static void tvafe_cvd2_get_signal_status(struct tvafe_cvd2_s *cvd2)
{
	int data = 0;

	data = R_APB_REG(CVD2_STATUS_REGISTER1);
	cvd2->hw_data[cvd2->hw_data_cur].no_sig =
				(bool)((data & 0x01) >> NO_SIGNAL_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].h_lock =
				(bool)((data & 0x02) >> HLOCK_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].v_lock =
				(bool)((data & 0x04) >> VLOCK_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].chroma_lock =
				(bool)((data & 0x08) >> CHROMALOCK_BIT);

	data = R_APB_REG(CVD2_STATUS_REGISTER2);
	cvd2->hw_data[cvd2->hw_data_cur].h_nonstd =
				(bool)((data & 0x02) >> HNON_STD_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].v_nonstd =
				(bool)((data & 0x04) >> VNON_STD_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].no_color_burst =
				(bool)((data & 0x08) >> BKNWT_DETECTED_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].comb3d_off =
				(bool)((data & 0x10) >> STATUS_COMB3D_OFF_BIT);

	data = R_APB_REG(CVD2_STATUS_REGISTER3);
	cvd2->hw_data[cvd2->hw_data_cur].pal =
				(bool)((data & 0x01) >> PAL_DETECTED_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].secam =
				(bool)((data & 0x02) >> SECAM_DETECTED_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].line625 =
				(bool)((data & 0x04) >> LINES625_DETECTED_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].noisy =
				(bool)((data & 0x08) >> NOISY_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].vcr =
				(bool)((data & 0x10) >> VCR_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].vcrtrick =
				(bool)((data & 0x20) >> VCR_TRICK_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].vcrff =
				(bool)((data & 0x40) >> VCR_FF_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].vcrrew =
				(bool)((data & 0x80) >> VCR_REW_BIT);

	cvd2->hw_data[cvd2->hw_data_cur].cordic =
			R_APB_BIT(CVD2_CORDIC_FREQUENCY_STATUS,
			STATUS_CORDIQ_FRERQ_BIT, STATUS_CORDIQ_FRERQ_WID);

	/* need the average of 3 fields ? */
	data = R_APB_REG(ACD_REG_83);
	cvd2->hw_data[cvd2->hw_data_cur].acc4xx_cnt =
			(unsigned char)(data >> RO_BD_ACC4XX_CNT_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].acc425_cnt =
			(unsigned char)(data >> RO_BD_ACC425_CNT_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].acc3xx_cnt =
			(unsigned char)(data >> RO_BD_ACC3XX_CNT_BIT);
	cvd2->hw_data[cvd2->hw_data_cur].acc358_cnt =
			(unsigned char)(data >> RO_BD_ACC358_CNT_BIT);
	data = cvd2->hw_data[0].acc4xx_cnt + cvd2->hw_data[1].acc4xx_cnt +
			cvd2->hw_data[2].acc4xx_cnt;
	cvd2->hw.acc4xx_cnt = data / 3;
	data = cvd2->hw_data[0].acc425_cnt + cvd2->hw_data[1].acc425_cnt +
			cvd2->hw_data[2].acc425_cnt;
	cvd2->hw.acc425_cnt = data / 3;
	data = cvd2->hw_data[0].acc3xx_cnt + cvd2->hw_data[1].acc3xx_cnt +
			cvd2->hw_data[2].acc3xx_cnt;
	cvd2->hw.acc3xx_cnt = data / 3;
	data = cvd2->hw_data[0].acc358_cnt + cvd2->hw_data[1].acc358_cnt +
			cvd2->hw_data[2].acc358_cnt;
	cvd2->hw.acc358_cnt = data / 3;
	data = R_APB_REG(ACD_REG_84);
	cvd2->hw_data[cvd2->hw_data_cur].secam_detected =
			(bool)((data & 0x1) >> RO_BD_SECAM_DETECTED_BIT);
	cvd2->hw.secam_phase = (bool)((data & 0x2) >> RO_DBDR_PHASE_BIT);
	if (cvd2->hw_data[0].secam_detected &&
		cvd2->hw_data[1].secam_detected &&
		cvd2->hw_data[2].secam_detected)
		cvd2->hw.secam_detected = true;
	if (!cvd2->hw_data[0].secam_detected &&
		!cvd2->hw_data[1].secam_detected &&
		!cvd2->hw_data[2].secam_detected)
		cvd2->hw.secam_detected = false;

	if (cnt_dbg_en) {

		pr_info("[%d]:cvd2->hw.acc3xx_cnt =%d,cvd2->hw.acc4xx_cnt=%d,acc425_cnt=%d\n",
			__LINE__ ,
		cvd2->hw.acc3xx_cnt, cvd2->hw.acc4xx_cnt, cvd2->hw.acc425_cnt);
		pr_info("[%d]:cvd2->hw.fsc_358=%d,cvd2->hw.fsc_425=%d,cvd2->hw.fsc_443 =%d\n",
			__LINE__,
		cvd2->hw.fsc_358, cvd2->hw.fsc_425, cvd2->hw.fsc_443);
		}
	if (cvd2->hw.acc3xx_cnt > cnt_vld_th) {

		if (cvd2->hw.acc358_cnt >
			(cvd2->hw.acc3xx_cnt - (cvd2->hw.acc3xx_cnt>>2))) {

			cvd2->hw.fsc_358 = true;
			cvd2->hw.fsc_425 = false;
			cvd2->hw.fsc_443 = false;

		} else if (cvd2->hw.acc358_cnt < (cvd2->hw.acc3xx_cnt<<1)/5) {

			cvd2->hw.fsc_358 = false;
			if (cvd2->hw.acc4xx_cnt > cnt_vld_th) {

				/*
				if(cvd2->hw.acc425_cnt >
				(cvd2->hw.acc4xx_cnt -
				(cvd2->hw.acc4xx_cnt>>2)))
				{
				if (cvd_dbg_en)
					printk("[%d]:cvd2->hw.fsc_358=%d,
					cvd2->hw.fsc_425=%d,
					cvd2->hw.fsc_443 =%d\n",
					__LINE__,cvd2->hw.fsc_358,
					cvd2->hw.fsc_425 ,cvd2->hw.fsc_443);


					cvd2->hw.fsc_425 = true;
					cvd2->hw.fsc_443 = false;
				}
				else if (cvd2->hw.acc425_cnt <
				((cvd2->hw.acc4xx_cnt)*3)/5)
				{*/
					cvd2->hw.fsc_425 = false;
					cvd2->hw.fsc_443 = true;

			} else if (cvd2->hw.acc4xx_cnt  < 40) {

				cvd2->hw.fsc_425 = false;
				cvd2->hw.fsc_443 = false;
			}
		}

	} else if (cvd2->hw.acc3xx_cnt < 40) {

		cvd2->hw.fsc_358 = false;
		cvd2->hw.fsc_425 = false;
		cvd2->hw.fsc_443 = false;
		cvd2->hw.no_color_burst = true;
	}
	if (++ cvd2->hw_data_cur >= 3)
		cvd2->hw_data_cur = 0;
	if (cnt_dbg_en)
		pr_info("[%d]:cvd2->hw.fsc_358=%d,cvd2->hw.fsc_425=%d,cvd2->hw.fsc_443 =%d\n",
		__LINE__, cvd2->hw.fsc_358,
		cvd2->hw.fsc_425 , cvd2->hw.fsc_443);

#ifdef TVAFE_CVD2_NOT_TRUST_NOSIG
#else
	if (cvd2->hw_data[0].no_sig && cvd2->hw_data[1].no_sig &&
		cvd2->hw_data[2].no_sig)
		cvd2->hw.no_sig = true;
	if (!cvd2->hw_data[0].no_sig && !cvd2->hw_data[1].no_sig &&
		!cvd2->hw_data[2].no_sig)
		cvd2->hw.no_sig = false;
#endif
	if ((cvd2->hw_data[0].h_lock || cvd2->hw_data[1].h_lock) &&
		cvd2->hw_data[2].h_lock)
		cvd2->hw.h_lock = true;
	else if (cvd2->hw_data[0].h_lock &&
		(cvd2->hw_data[1].h_lock || cvd2->hw_data[2].h_lock))
		cvd2->hw.h_lock = true;
	if (!cvd2->hw_data[0].h_lock && !cvd2->hw_data[1].h_lock &&
		!cvd2->hw_data[2].h_lock)
		cvd2->hw.h_lock = false;

	if (cvd2->hw_data[0].v_lock && cvd2->hw_data[1].v_lock &&
		cvd2->hw_data[2].v_lock)
		cvd2->hw.v_lock = true;
	if (!cvd2->hw_data[0].v_lock && !cvd2->hw_data[1].v_lock &&
		!cvd2->hw_data[2].v_lock)
		cvd2->hw.v_lock = false;

	if (cvd2->hw_data[0].h_nonstd && cvd2->hw_data[1].h_nonstd &&
		cvd2->hw_data[2].h_nonstd)
		cvd2->hw.h_nonstd = true;
	if (!cvd2->hw_data[0].h_nonstd && !cvd2->hw_data[1].h_nonstd &&
		!cvd2->hw_data[2].h_nonstd)
		cvd2->hw.h_nonstd = false;

	if (cvd2->hw_data[0].v_nonstd && cvd2->hw_data[1].v_nonstd &&
		cvd2->hw_data[2].v_nonstd)
		cvd2->hw.v_nonstd = true;
	else if (cvd2->hw_data[0].v_nonstd ||
		(cvd2->hw_data[1].v_nonstd && cvd2->hw_data[2].v_nonstd))
		cvd2->hw.v_nonstd = true;
	if (!cvd2->hw_data[0].v_nonstd && !cvd2->hw_data[1].v_nonstd &&
		!cvd2->hw_data[2].v_nonstd)
		cvd2->hw.v_nonstd = false;

	if (cvd2->hw_data[0].no_color_burst &&
		cvd2->hw_data[1].no_color_burst &&
		cvd2->hw_data[2].no_color_burst)
		cvd2->hw.no_color_burst = true;
	if (!cvd2->hw_data[0].no_color_burst &&
		!cvd2->hw_data[1].no_color_burst &&
		!cvd2->hw_data[2].no_color_burst)
		cvd2->hw.no_color_burst = false;

	if (cvd2->hw_data[0].comb3d_off && cvd2->hw_data[1].comb3d_off &&
		cvd2->hw_data[2].comb3d_off)
		cvd2->hw.comb3d_off = true;
	if (!cvd2->hw_data[0].comb3d_off && !cvd2->hw_data[1].comb3d_off &&
		!cvd2->hw_data[2].comb3d_off)
		cvd2->hw.comb3d_off = false;

	if (cvd2->hw_data[0].chroma_lock &&
		(cvd2->hw_data[1].chroma_lock || cvd2->hw_data[2].chroma_lock))
		cvd2->hw.chroma_lock = true;
	else if ((cvd2->hw_data[0].chroma_lock ||
		cvd2->hw_data[1].chroma_lock) &&
		cvd2->hw_data[2].chroma_lock)
		cvd2->hw.chroma_lock = true;
	if (!cvd2->hw_data[0].chroma_lock && !cvd2->hw_data[1].chroma_lock &&
		!cvd2->hw_data[2].chroma_lock)
		cvd2->hw.chroma_lock = false;

	if ((cvd2->hw_data[0].pal || cvd2->hw_data[1].pal) &&
		cvd2->hw_data[2].pal)
		cvd2->hw.pal = true;
	else if (cvd2->hw_data[0].pal &&
		(cvd2->hw_data[1].pal || cvd2->hw_data[2].pal))
		cvd2->hw.pal = true;
	else if ((cvd2->hw_data[0].pal || cvd2->hw_data[2].pal) &&
		cvd2->hw_data[1].pal)
		cvd2->hw.pal = true;
	if (!cvd2->hw_data[0].pal && !cvd2->hw_data[1].pal &&
		!cvd2->hw_data[2].pal)
		cvd2->hw.pal = false;

	if (cvd2->hw_data[0].secam && cvd2->hw_data[1].secam &&
		cvd2->hw_data[2].secam)
		cvd2->hw.secam = true;
	if (!cvd2->hw_data[0].secam && !cvd2->hw_data[1].secam &&
		!cvd2->hw_data[2].secam)
		cvd2->hw.secam = false;

	if (cvd2->hw_data[0].line625 && cvd2->hw_data[1].line625 &&
		cvd2->hw_data[2].line625)
		cvd2->hw.line625 = true;
	if (!cvd2->hw_data[0].line625 && !cvd2->hw_data[1].line625 &&
		!cvd2->hw_data[2].line625)
		cvd2->hw.line625 = false;

	if (cvd2->hw_data[0].noisy && cvd2->hw_data[1].noisy &&
		cvd2->hw_data[2].noisy)
		cvd2->hw.noisy = true;
	if (!cvd2->hw_data[0].noisy && !cvd2->hw_data[1].noisy &&
		!cvd2->hw_data[2].noisy)
		cvd2->hw.noisy = false;

	if (cvd2->hw_data[0].vcr && cvd2->hw_data[1].vcr &&
		cvd2->hw_data[2].vcr)
		cvd2->hw.vcr = true;
	if (!cvd2->hw_data[0].vcr && !cvd2->hw_data[1].vcr &&
		!cvd2->hw_data[2].vcr)
		cvd2->hw.vcr = false;

	if (cvd2->hw_data[0].vcrtrick && cvd2->hw_data[1].vcrtrick &&
		cvd2->hw_data[2].vcrtrick)
		cvd2->hw.vcrtrick = true;
	if (!cvd2->hw_data[0].vcrtrick && !cvd2->hw_data[1].vcrtrick &&
		!cvd2->hw_data[2].vcrtrick)
		cvd2->hw.vcrtrick = false;

	if (cvd2->hw_data[0].vcrff && cvd2->hw_data[1].vcrff &&
		cvd2->hw_data[2].vcrff)
		cvd2->hw.vcrff = true;
	if (!cvd2->hw_data[0].vcrff && !cvd2->hw_data[1].vcrff &&
		!cvd2->hw_data[2].vcrff)
		cvd2->hw.vcrff = false;

	if (cvd2->hw_data[0].vcrrew && cvd2->hw_data[1].vcrrew &&
		cvd2->hw_data[2].vcrrew)
		cvd2->hw.vcrrew = true;
	if (!cvd2->hw_data[0].vcrrew && !cvd2->hw_data[1].vcrrew &&
		!cvd2->hw_data[2].vcrrew)
		cvd2->hw.vcrrew = false;
#ifdef TVAFE_CVD2_NOT_TRUST_NOSIG
/* while tv channel switch, avoid black screen */
	if (!cvd2->hw.h_lock)
		cvd2->hw.no_sig = true;
	if (cvd2->hw.h_lock)
		cvd2->hw.no_sig = false;
#endif

	data  = 0;
	data += (int)cvd2->hw_data[0].cordic;
	data += (int)cvd2->hw_data[1].cordic;
	data += (int)cvd2->hw_data[2].cordic;
	if (cvd2->hw_data[0].cordic & 0x80)

		data -= 256;

	if (cvd2->hw_data[1].cordic & 0x80)

		data -= 256;

	if (cvd2->hw_data[2].cordic & 0x80)

		data -= 256;

	data /= 3;
	cvd2->hw.cordic  = (unsigned char)(data & 0xff);

	return;
}

/*
 * tvafe cvd2 get cvd2 signal lock status
 */
enum tvafe_cvbs_video_e tvafe_cvd2_get_lock_status(
					struct tvafe_cvd2_s *cvd2)
{
	enum tvafe_cvbs_video_e cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_UNLOCKED;

	if (!cvd2->hw.h_lock && !cvd2->hw.v_lock)
		cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_UNLOCKED;
	else if (cvd2->hw.h_lock && cvd2->hw.v_lock) {
		cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_LOCKED;
		lock_cnt++;
	} else if (cvd2->hw.h_lock) {
		cvbs_lock_status = TVAFE_CVBS_VIDEO_H_LOCKED;
		lock_cnt++;
	} else if (cvd2->hw.v_lock) {
		cvbs_lock_status = TVAFE_CVBS_VIDEO_V_LOCKED;
		lock_cnt++;
	}
	if (lock_cnt >= 1)
		cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_LOCKED;
	return cvbs_lock_status;
}

/*
 * tvafe cvd2 get cvd2 tv format
 */

int tvafe_cvd2_get_atv_format(void)
{
	int format;
	format = R_APB_REG(CVD2_STATUS_REGISTER3)&0x7;
	return format;
}
EXPORT_SYMBOL(tvafe_cvd2_get_atv_format);
int tvafe_cvd2_get_hv_lock(void)
{
	int lock_status;
	lock_status = R_APB_REG(CVD2_STATUS_REGISTER1)&0x6;
	return lock_status;
}
EXPORT_SYMBOL(tvafe_cvd2_get_hv_lock);


/*
 * tvafe cvd2 check current cordic match status
 */
/*
static bool tvafe_cvd2_cordic_match(struct tvafe_cvd2_s *cvd2)
{
	int in_min = 0;
	int in_max = 0;

	if (cvd2->vd_port == TVIN_PORT_CVBS0)
	{
		in_min = cordic_tv_in_min;
		in_max = cordic_tv_in_max;
	}
	else
	{
		in_min = cordic_av_in_min;
		in_max = cordic_av_in_max;
	}

	if ((cvd2->hw.cordic <= in_min) ||
			(cvd2->hw.cordic >= in_max))
	{
		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: codic:%d match:<%d or >%d\n",
			__func__,
					cvd2->hw.cordic, in_min, in_max);
		return true;

	}
	else
	{
		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: codic:%d dismatch:%d~%d\n",
			__func__,
					cvd2->hw.cordic, in_min, in_max);
		return false;
	}

}
*/
/*
 * tvafe cvd2 non-standard signal detection
 */
static void tvafe_cvd2_non_std_signal_det(
				struct tvafe_cvd2_s *cvd2)
{
	unsigned short dgain = 0;
	unsigned long chroma_sum_filt_tmp = 0;
	unsigned long chroma_sum_filt = 0;
	unsigned long chroma_sum_in = 0;


	chroma_sum_in = rd_bits(0, VDIN_HIST_CHROMA_SUM,
				HIST_CHROMA_SUM_BIT,  HIST_CHROMA_SUM_WID);
	chroma_sum_pre3 = chroma_sum_pre2;
	chroma_sum_pre2 = chroma_sum_pre1;
	chroma_sum_pre1 = chroma_sum_in;

	chroma_sum_filt_tmp = (chroma_sum_pre3 +
				(chroma_sum_pre2 << 1) + chroma_sum_pre1) >> 2;
	chroma_sum_filt = chroma_sum_filt_tmp;

	if (chroma_sum_filt >= SCENE_COLORFUL_TH)
		scene_colorful = 1;
	else
		scene_colorful = 0;

	if (print_cnt == 0x50)
		print_cnt = 0;
	else
		print_cnt = print_cnt + 1;

	if (print_cnt == 0x28) {
		if (cvd_nonstd_dbg_en)
			pr_info("[tvafe..] %s: scene_colorful = %d, chroma_sum_filt = %ld\n",
			__func__, scene_colorful, chroma_sum_filt);
	}

	if ((cvd2->hw.h_nonstd | (cvd2->hw.v_nonstd && scene_colorful)) &&
		(nonstd_cnt < CVD2_NONSTD_CNT_INC_LIMIT)) {

		nonstd_cnt = nonstd_cnt + CVD2_NONSTD_CNT_INC_STEP;

	} else if ((cvd2->hw.h_nonstd |
		(cvd2->hw.v_nonstd && scene_colorful)) &&
			(nonstd_cnt >= CVD2_NONSTD_CNT_INC_LIMIT)) {

		nonstd_cnt = nonstd_cnt;

	} else if ((!cvd2->hw.h_nonstd) && (!cvd2->hw.v_nonstd) &&
			(nonstd_cnt >= CVD2_NONSTD_CNT_DEC_LIMIT)) {

		nonstd_cnt = nonstd_cnt - CVD2_NONSTD_CNT_DEC_STEP;

	} else if ((!cvd2->hw.h_nonstd) && (!cvd2->hw.v_nonstd) &&
			(nonstd_cnt < CVD2_NONSTD_CNT_DEC_LIMIT)) {

		nonstd_cnt = nonstd_cnt;
	}

	if (nonstd_cnt <= CVD2_NONSTD_FLAG_OFF_TH)
		nonstd_flag = 0;
	else if (nonstd_cnt >= CVD2_NONSTD_FLAG_ON_TH)
		nonstd_flag = 1;

	if ((cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I) && cvd2->hw.line625) {

		dgain = R_APB_BIT(CVD2_AGC_GAIN_STATUS_7_0,
				AGC_GAIN_7_0_BIT, AGC_GAIN_7_0_WID);
		dgain |= R_APB_BIT(CVD2_AGC_GAIN_STATUS_11_8,
				AGC_GAIN_11_8_BIT, AGC_GAIN_11_8_WID)<<8;
		if ((dgain >= TVAFE_CVD2_NONSTD_DGAIN_MAX) ||
				cvd2->hw.h_nonstd || nonstd_flag){

			cvd2->info.non_std_enable = 1;

		} else{

			cvd2->info.non_std_enable = 0;
		}
	}
}

/*
 * tvafe cvd2 signal unstable
 */
static bool tvafe_cvd2_sig_unstable(struct tvafe_cvd2_s *cvd2)
{
	bool ret = false;
#if 1
	if (cvd2->hw.no_sig)
		ret = true;
#else
	if (cvd2->vd_port == TVIN_PORT_CVBS0) {

		if (cvd2->hw.no_sig)/* || !cvd2->hw.h_lock) */
			ret = true;

	} else{

		if (cvd2->hw.no_sig || !cvd2->hw.h_lock || !cvd2->hw.v_lock)
			ret = true;
	}
#endif
	return ret;
}

/*
 * tvafe cvd2 checkt current format match condition
 */
static bool tvafe_cvd2_condition_shift(struct tvafe_cvd2_s *cvd2)
{
	bool ret = false;

	/* check non standard signal, ignore SECAM/525 mode */
	if (!tvafe_cvd2_sig_unstable(cvd2))
		tvafe_cvd2_non_std_signal_det(cvd2);

	if (cvd2->manual_fmt)
		return false;

	if (tvafe_cvd2_sig_unstable(cvd2)) {

		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: sig unstable, nosig:%d,h-lock:%d,v-lock:%d\n",
			__func__,
			cvd2->hw.no_sig, cvd2->hw.h_lock, cvd2->hw.v_lock);
		return true;
	}

	/* check line flag */
		switch (cvd2->config_fmt) {

		case TVIN_SIG_FMT_CVBS_PAL_I:
		case TVIN_SIG_FMT_CVBS_PAL_CN:
		case TVIN_SIG_FMT_CVBS_SECAM:
			if (!cvd2->hw.line625) {

				ret = true;
				cvd2->fmt_loop_cnt = 0;
				if (cvd_dbg_en)
					pr_info("[tvafe..] %s: reset fmt try cnt 525 line\n",
					__func__);
			}
			break;
		case TVIN_SIG_FMT_CVBS_PAL_M:
		case TVIN_SIG_FMT_CVBS_NTSC_443:
		case TVIN_SIG_FMT_CVBS_PAL_60:
		case TVIN_SIG_FMT_CVBS_NTSC_M:
			if (cvd2->hw.line625) {

				ret = true;
				cvd2->fmt_loop_cnt = 0;
				if (cvd_dbg_en)
					pr_info("[tvafe..] %s: reset fmt try cnt 625 line\n",
					__func__);
			}
			break;
		default:
			break;
	}
	if (ret) {

		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: line625 error!!!!\n", __func__);
		return true;
	}
	if (cvd2->hw.no_color_burst) {

		/* for SECAM format, set PAL_I */
		if (cvd2->config_fmt != TVIN_SIG_FMT_CVBS_SECAM) {
			/* set default fmt */

			if (!cvd_pr_flag && cvd_dbg_en) {

				cvd_pr_flag = true;
				pr_info("[tvafe..] %s: no-color-burst, do not change mode.\n",
					__func__);
			}
			return false;
		}
	}
	/* ignore pal flag because of cdto adjustment */
	if ((cvd2->info.non_std_worst || cvd2->hw.h_nonstd) &&
			(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)) {

		if (!cvd_pr_flag && cvd_dbg_en) {

			cvd_pr_flag = true;
			pr_info("[tvafe..] %s: if adj cdto or h-nonstd, ignore mode change.\n",
				__func__);
		}
		return false;
	}

	/* for ntsc-m pal-m switch bug */
	if ((!cvd2->hw.chroma_lock) &&
		(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_NTSC_M)) {

		if (cvd2->info.ntsc_switch_cnt++ >= ntsc_sw_maxcnt)

			cvd2->info.ntsc_switch_cnt = 0;


		if (cvd2->info.ntsc_switch_cnt <= ntsc_sw_midcnt) {

			if (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_23_16,
			CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID) != 0x2e){

				W_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_23_16, 0x2e,
					CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID);
				W_APB_BIT(CVD2_PAL_DETECTION_THRESHOLD, 0x40,
					PAL_DET_TH_BIT, PAL_DET_TH_WID);
				W_APB_BIT(CVD2_CONTROL0, 0x00,
					COLOUR_MODE_BIT, COLOUR_MODE_WID);
				W_APB_BIT(CVD2_COMB_FILTER_CONFIG, 0x2,
					PALSW_LVL_BIT, PALSW_LVL_WID);
				W_APB_BIT(CVD2_COMB_LOCK_CONFIG, 0x7,
			LOSE_CHROMALOCK_LVL_BIT, LOSE_CHROMALOCK_LVL_WID);
				W_APB_BIT(CVD2_PHASE_OFFSE_RANGE, 0x20,
			PHASE_OFFSET_RANGE_BIT, PHASE_OFFSET_RANGE_WID);
			}
			if (!cvd_pr1_chroma_flag && cvd_dbg_en) {

				cvd_pr1_chroma_flag = true;
				cvd_pr2_chroma_flag = false;
				pr_info("[tvafe..] %s: change cdto to ntsc-m\n",
					__func__);
			}

		} else{

			if (R_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_23_16,
				CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID) !=
				0x23){

				W_APB_BIT(CVD2_CHROMA_DTO_INCREMENT_23_16, 0x23,
					CDTO_INC_23_16_BIT, CDTO_INC_23_16_WID);
				W_APB_BIT(CVD2_PAL_DETECTION_THRESHOLD, 0x1f,
					PAL_DET_TH_BIT, PAL_DET_TH_WID);
				W_APB_BIT(CVD2_CONTROL0, 0x02,
					COLOUR_MODE_BIT, COLOUR_MODE_WID);
				W_APB_BIT(CVD2_COMB_FILTER_CONFIG, 3,
					PALSW_LVL_BIT, PALSW_LVL_WID);
				W_APB_BIT(CVD2_COMB_LOCK_CONFIG, 2,
					LOSE_CHROMALOCK_LVL_BIT,
					LOSE_CHROMALOCK_LVL_WID);
				W_APB_BIT(CVD2_PHASE_OFFSE_RANGE, 0x15,
					PHASE_OFFSET_RANGE_BIT,
					PHASE_OFFSET_RANGE_WID);
			}
			if (!cvd_pr2_chroma_flag && cvd_dbg_en) {

				cvd_pr2_chroma_flag = true;
				cvd_pr1_chroma_flag = false;
				pr_info("[tvafe..] %s: change cdto to pal-m\n",
					__func__);
			}
		}
	}
	if (((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) &&
		force_fmt_flag) {
		if (cvd_dbg_en)
			pr_info("[%s]:ignore the pal/358/443 flag and return\n",
			__func__);
		return false;
	}
	if (ignore_pal_nt)

		return false;


	/* check pal/secam flag */
		switch (cvd2->config_fmt) {

		case TVIN_SIG_FMT_CVBS_PAL_I:
		case TVIN_SIG_FMT_CVBS_PAL_CN:
		case TVIN_SIG_FMT_CVBS_PAL_60:
		case TVIN_SIG_FMT_CVBS_PAL_M:
			if (!cvd2->hw.pal)
				ret = true;
			break;
		case TVIN_SIG_FMT_CVBS_SECAM:
			if (!cvd2->hw.secam || !cvd2->hw.secam_detected)
				ret = true;
			break;
		case TVIN_SIG_FMT_CVBS_NTSC_443:
		case TVIN_SIG_FMT_CVBS_NTSC_M:
			if (cvd2->hw.pal)
				ret = true;
			break;
		default:
			break;
	}
	if (ignore_443_358) {

		if (ret)
			return true;
		else
			return false;
	}
	/*check 358/443*/
		switch (cvd2->config_fmt) {

		case TVIN_SIG_FMT_CVBS_PAL_CN:
		case TVIN_SIG_FMT_CVBS_PAL_M:
		case TVIN_SIG_FMT_CVBS_NTSC_M:
			if (!cvd2->hw.fsc_358 && cvd2->hw.fsc_443)
				ret = true;
			break;
		case TVIN_SIG_FMT_CVBS_PAL_I:
		case TVIN_SIG_FMT_CVBS_PAL_60:
		case TVIN_SIG_FMT_CVBS_NTSC_443:
			if (cvd2->hw.fsc_358 && !cvd2->hw.fsc_443)
				ret = true;
			break;
		default:
			break;
	}
	if (ret) {

		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: pal is %d,secam flag is %d, changed.\n",
			__func__, cvd2->hw.pal, cvd2->hw.secam);
		return true;

	} else
		return false;
}
/*
due to some cvd falg is invalid,we must force fmt after reach to max-cnt
*/
static void cvd_force_config_fmt(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem, int config_force_fmt)
{

	/* force to secam */
	if ((cvd2->hw.line625 && (cvd2->hw.secam_detected || cvd2->hw.secam)) ||
		config_force_fmt == 1) {
		tvafe_cvd2_try_format(cvd2, mem, TVIN_SIG_FMT_CVBS_SECAM);
		pr_info("[%s]:force the fmt to TVIN_SIG_FMT_CVBS_SECAM\n",
			__func__);
	}
	/* force to ntscm */
	else if ((!cvd2->hw.line625 && !cvd2->hw.pal) ||
		config_force_fmt == 2) {
		tvafe_cvd2_try_format(cvd2, mem, TVIN_SIG_FMT_CVBS_NTSC_M);
		pr_info("[%s]:force the fmt to TVIN_SIG_FMT_CVBS_NTSC_M\n",
			__func__);
	}
	/* force to palm */
	else if ((!cvd2->hw.line625 && cvd2->hw.pal) || config_force_fmt == 3) {
		tvafe_cvd2_try_format(cvd2, mem, TVIN_SIG_FMT_CVBS_PAL_M);
		pr_info("[%s]:force the fmt to TVIN_SIG_FMT_CVBS_PAL_M\n",
			__func__);
	}
	/* force to pali */
	else if ((cvd2->hw.line625) || config_force_fmt == 4) {
		tvafe_cvd2_try_format(cvd2, mem, TVIN_SIG_FMT_CVBS_PAL_I);
		pr_info("[%s]:force the fmt to TVIN_SIG_FMT_CVBS_PAL_I\n",
			__func__);
	}
	return;

}



/*
 * tvafe cvd2 search video format function
 */

static void tvafe_cvd2_search_video_mode(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem)
{
	unsigned int shift_cnt = 0;
	/* execute manual mode */
	if ((cvd2->manual_fmt) && (cvd2->config_fmt != cvd2->manual_fmt) &&
		(cvd2->config_fmt != TVIN_SIG_FMT_NULL)) {
		tvafe_cvd2_try_format(cvd2, mem, cvd2->manual_fmt);
	}
	/* state-machine */
	if (cvd2->info.state == TVAFE_CVD2_STATE_INIT) {
		/* wait for signal setup */
		if (tvafe_cvd2_sig_unstable(cvd2)) {
			cvd2->info.state_cnt = 0;
			if (!cvd_pr_flag && cvd_dbg_en) {
				cvd_pr_flag = true;
				pr_info("[tvafe..] %s: sig unstable,nosig:%d,h-lock:%d,v-lock:%d.\n",
				__func__, cvd2->hw.no_sig, cvd2->hw.h_lock,
				cvd2->hw.v_lock);
			}
		}
		/* wait for signal stable */
		else if (++cvd2->info.state_cnt <= fmt_wait_cnt)
			return;
		force_fmt_flag = 0;
		cvd_pr_flag = false;
		cvd2->info.state_cnt = 0;
		/* manual mode =>
		go directly to the manual format */
		if (cvd2->manual_fmt) {
			try_format_cnt = 0;
			cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			if (cvd_dbg_en)
				pr_info("[tvafe..] %s: manual fmt is:%s,do not need try other format!!!\n",
				__func__, tvin_sig_fmt_str(cvd2->manual_fmt));
			return;
		}
		/* auto mode */
		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: switch to fmt:%s,hnon:%d,vnon:%d,c-lk:%d,pal:%d,secam:%d,h-lk:%d,v-lk:%d,fsc358:%d,fsc425:%d,fsc443:%d,secam detected %d,line625:%d\n",
			__func__, tvin_sig_fmt_str(cvd2->config_fmt),
			cvd2->hw.h_nonstd, cvd2->hw.v_nonstd,
			cvd2->hw.chroma_lock, cvd2->hw.pal,
			cvd2->hw.secam, cvd2->hw.h_lock,
			cvd2->hw.v_lock, cvd2->hw.fsc_358,
			cvd2->hw.fsc_425, cvd2->hw.fsc_443,
			cvd2->hw.secam_detected, cvd2->hw.line625);
		if (cvd_dbg_en)
			pr_info("acc4xx_cnt = %d,acc425_cnt = %d,acc3xx_cnt = %d,acc358_cnt = %d secam_detected:%d\n",
			cvd2->hw_data[cvd2->hw_data_cur].acc4xx_cnt ,
			cvd2->hw_data[cvd2->hw_data_cur].acc425_cnt,
			cvd2->hw_data[cvd2->hw_data_cur].acc3xx_cnt,
			cvd2->hw_data[cvd2->hw_data_cur].acc358_cnt,
			cvd2->hw_data[cvd2->hw_data_cur].secam_detected);
			/* force mode:due to some
			signal is hard to check out */
		if (++try_format_cnt == try_format_max) {
			cvd_force_config_fmt(cvd2, mem, config_force_fmt);
			return;
		} else if (try_format_cnt > try_format_max) {
			cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			force_fmt_flag = 1;
			return;
		}
		switch (cvd2->config_fmt) {
		case TVIN_SIG_FMT_CVBS_PAL_I:
			if (cvd2->hw.line625) {
				if (cvd2->hw.secam_detected ||
					cvd2->hw.secam)
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_SECAM);
				else if (cvd2->hw.fsc_443 && cvd2->hw.pal) {
					/* 625 +
					cordic_match =>
					confirm PAL_I */
					cvd2->info.state =
					TVAFE_CVD2_STATE_FIND;
				} else if (cvd2->hw.fsc_358) {
					/* 625 + 358 =>
					try PAL_CN */
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_CN);
				}
			} else {
				/* 525 lines */
				if (cvd_dbg_en)
					pr_info("[tvafe..]%s dismatch pal_i line625 %d!!!and the  fsc358 %d,pal %d,fsc_443:%d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_358, cvd2->hw.pal,
					cvd2->hw.fsc_443);
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_M);
				if (cvd_dbg_en)
					pr_info("[tvafe..]%sdismatch pal_i and after try other format: line625 %d!!!and the  fsc358 %d,pal %d,fsc_443:%d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_358,
					cvd2->hw.pal, cvd2->hw.fsc_443);
			}
			break;
		case TVIN_SIG_FMT_CVBS_PAL_CN:
			if (cvd2->hw.line625 && cvd2->hw.fsc_358 &&
			cvd2->hw.pal){
				/* line625+brust358+pal
				-> pal_cn */
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;

			} else {
				if (cvd_dbg_en)
					pr_info("[tvafe..]%s dismatch pal_cn line625 %d, fsc358 %d,pal %d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_358, cvd2->hw.pal);
				if (cvd2->hw.line625)
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_I);
				else if (!cvd2->hw.line625 &&
				cvd2->hw.fsc_358 &&
				cvd2->hw.pal)
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_M);
				else if (!cvd2->hw.line625 &&
				cvd2->hw.fsc_443 &&
				!cvd2->hw.pal)
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_NTSC_443);
				}
			break;
		case TVIN_SIG_FMT_CVBS_SECAM:
			if (cvd2->hw.line625 && cvd2->hw.secam_detected &&
			cvd2->hw.secam){
				/* 625 + secam =>
				confirm SECAM */
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			} else {
			if (cvd_dbg_en)
				pr_info("[tvafe..]%s dismatch secam line625 %d, secam_detected %d",
				__func__, cvd2->hw.line625,
				cvd2->hw.secam_detected);
			if (cvd2->hw.line625)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_I);
			else if (!cvd2->hw.line625 && cvd2->hw.fsc_358 &&
			cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_M);
			else if (!cvd2->hw.line625 && cvd2->hw.fsc_443 &&
			!cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_NTSC_443);
			}
			break;
		case TVIN_SIG_FMT_CVBS_PAL_M:
			if (!cvd2->hw.line625 && cvd2->hw.fsc_358 &&
			cvd2->hw.pal && cvd2->hw.chroma_lock){
				/* line525 + 358 + pal
				=> confirm PAL_M */
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			} else {
			if (cvd_dbg_en)
				pr_info("[tvafe..]%s dismatch pal m line625 %d, fsc358 %d,pal %d",
				__func__, cvd2->hw.line625,
				cvd2->hw.fsc_358, cvd2->hw.pal);
			if (cvd2->hw.line625)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_I);
			else if (!cvd2->hw.line625 && cvd2->hw.fsc_443 &&
			cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_60);
			else if (!cvd2->hw.line625 && cvd2->hw.fsc_358 &&
				!cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_NTSC_M);
			else if (!cvd2->hw.line625 && cvd2->hw.fsc_443 &&
				!cvd2->hw.pal)
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_NTSC_443);
			}
			break;
		case TVIN_SIG_FMT_CVBS_NTSC_M:
			if (!cvd2->hw.line625 && cvd2->hw.fsc_358 &&
				!cvd2->hw.pal && cvd2->hw.chroma_lock){
				/* line525 + 358 =>
				confirm NTSC_M */
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			} else{
				if (cvd_dbg_en)
					pr_info("[tvafe..]%s dismatch ntsc m line625 %d, fsc358 %d,pal %d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_358, cvd2->hw.pal);
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_I);
			}
			break;
		case TVIN_SIG_FMT_CVBS_PAL_60:
			if (!cvd2->hw.line625 && cvd2->hw.fsc_443 &&
				cvd2->hw.pal)
				/* 525 + 443 + pal =>
				confirm PAL_60 */
				cvd2->info.state = TVAFE_CVD2_STATE_FIND;
			else{
				/* set default to pal i */
				if (cvd_dbg_en)
					pr_info("[tvafe..]%s dismatch pal 60 line625 %d, fsc443 %d,pal %d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_443, cvd2->hw.pal);
				tvafe_cvd2_try_format(cvd2, mem,
				TVIN_SIG_FMT_CVBS_PAL_I);
			}
			break;
		case TVIN_SIG_FMT_CVBS_NTSC_443:
			if (!cvd2->hw.line625 && cvd2->hw.fsc_443 &&
				!cvd2->hw.pal)
				/* 525 + 443 =>
				confirm NTSC_443 */
				cvd2->info.state =
				TVAFE_CVD2_STATE_FIND;
			else{
				/* set default to pal i */
				if (cvd_dbg_en)
					pr_info("[tvafe..]%s dismatch NTSC_443 line625 %d, fsc443 %d,pal %d",
					__func__, cvd2->hw.line625,
					cvd2->hw.fsc_443, cvd2->hw.pal);
				if (!cvd2->hw.line625 &&
				cvd2->hw.fsc_443 && cvd2->hw.pal)
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_60);
				else if (!cvd2->hw.line625) {
					cvd_force_config_fmt(cvd2, mem,
					config_force_fmt);
					try_format_cnt =
						try_format_max+1;
					return;
				} else
					tvafe_cvd2_try_format(cvd2, mem,
					TVIN_SIG_FMT_CVBS_PAL_I);
			}
			break;
		default:
			break;
		}
		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: current fmt is:%s\n",
			__func__, tvin_sig_fmt_str(cvd2->config_fmt));
	} else if (cvd2->info.state == TVAFE_CVD2_STATE_FIND) {
		/* manual mode => go directly to the manual format */
		try_format_cnt = 0;
		if (tvafe_cvd2_condition_shift(cvd2)) {
			shift_cnt = cvd2_shift_cnt;
			if (cvd2->info.non_std_enable)
				shift_cnt = cvd2_shift_cnt*10;
			/* if no color burst,
			pal flag can not be trusted */
			if (cvd2->info.fmt_shift_cnt++ > shift_cnt) {
				tvafe_cvd2_try_format(
				cvd2, mem, TVIN_SIG_FMT_CVBS_PAL_I);
				cvd2->info.state = TVAFE_CVD2_STATE_INIT;
				cvd2->info.ntsc_switch_cnt = 0;
				try_format_cnt = 0;
				cvd_pr_flag = false;
			}
		}
		/* non-standard signal config */
		tvafe_cvd2_non_std_config(cvd2);
	}
}


#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
static void tvafe_cvd2_auto_de(struct tvafe_cvd2_s *cvd2)
{
	struct tvafe_cvd2_lines_s *lines = &cvd2->info.vlines;
	unsigned int i = 0, l_ave = 0, l_max = 0, l_min = 0xff, tmp = 0;
	if (!cvd2->hw.line625 || (cvd2->config_fmt != TVIN_SIG_FMT_CVBS_PAL_I))
		return;
	lines->val[0] = lines->val[1];
	lines->val[1] = lines->val[2];
	lines->val[2] = lines->val[3];
	lines->val[3] = R_APB_REG(CVD2_REG_E6);
	for (i = 0; i < 4; i++) {
		if (l_max < lines->val[i])
			l_max = lines->val[i];
		if (l_min > lines->val[i])
			l_min = lines->val[i];
		l_ave += lines->val[i];
	}
	if (lines->check_cnt++ == TVAFE_CVD2_AUTO_DE_CHECK_CNT) {
		lines->check_cnt = 0;
		/* if (cvd_dbg_en) */
		/* pr_info("%s: check lines every 100*10ms\n", __func__); */
		l_ave = (l_ave - l_max - l_min + 1) >> 1;
		/* get the average value */
		if (l_ave > TVAFE_CVD2_AUTO_DE_TH) {
			tmp = (0xff - l_ave + 1) >> 2;
			/* avoid overflow */
			if (tmp > TVAFE_CVD2_PAL_DE_START)
				tmp = TVAFE_CVD2_PAL_DE_START;
			if (lines->de_offset != tmp || scene_colorful_old) {
				lines->de_offset = tmp;
				tmp = ((TVAFE_CVD2_PAL_DE_START -
					lines->de_offset) << 16) |
					(288 + TVAFE_CVD2_PAL_DE_START -
					lines->de_offset);
				W_APB_REG(ACD_REG_2E, tmp);
				scene_colorful_old = 0;
				if (cvd_dbg_en)
					pr_info("%s: vlines:%d, de_offset:%d tmp:%x\n",
				__func__, l_ave, lines->de_offset, tmp);
			}
		} else {
			if (lines->de_offset > 0) {
				tmp = ((TVAFE_CVD2_PAL_DE_START -
					lines->de_offset) << 16) |
					(288 + TVAFE_CVD2_PAL_DE_START -
					lines->de_offset);
				W_APB_REG(ACD_REG_2E, tmp);
				scene_colorful_old = 0;
				if (cvd_dbg_en)
					pr_info("%s: vlines:%d, de_offset:%d tmp:%x\n",
					__func__, l_ave, lines->de_offset, tmp);
				lines->de_offset--;
			}
		}
	}
}
#endif
/*set default de according to config fmt*/
void tvafe_cvd2_set_default_de(struct tvafe_cvd2_s *cvd2)
{
	if (scene_colorful_old == 1)
		return;
#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
	if (!cvd2) {

		pr_info("[tvafe..]%s error.\n", __func__);
		return;
	}
	/*write default de to register*/
	W_APB_REG(ACD_REG_2E,  (rf_acd_table[cvd2->config_fmt-
				TVIN_SIG_FMT_CVBS_NTSC_M][0x2e]));
	if (cvd_dbg_en)
		pr_info("[tvafe..]%s set default de %s.\n",
				__func__, tvin_sig_fmt_str(cvd2->config_fmt));
	scene_colorful_old = 1;
#endif
}
#ifdef TVAFE_CVD2_ADC_REG_CHECK
/*
 * tvafe cvd2 check adc Reg table for esd test
 */
static void tvafe_cvd2_check_adc_reg(struct tvafe_cvd2_s *cvd2)
{
#if 0
	int i = 0, tmp = 0;

	if (cvd2->info.normal_cnt++ == TVAFE_CVD2_NORMAL_REG_CHECK_CNT) {

		cvd2->info.normal_cnt = 0;
		cvd2->info.adc_reload_en = false;
		for (i = 1; i < ADC_REG_NUM; i++) {

			/* 0x20 has output buffer register,
			I have not list it here, should I? */
			/* 0x21 has reset & sleep mode,
			should I watch it here? */
			if ((i == 0x05) || (i == 0x1d) ||
				(i == 0x20) || (i == 0x24) ||
				(i == 0x2b) || (i == 0x35) ||
				(i == 0x3a) || (i == 0x53) ||
				(i == 0x54) || (i == 0x6a) ||
				(i == 0x34) || (i == 0x46) ||
				(i == 0x57) || (i == 0x67) ||
				(i >= 0x4a && i <= 0x55) ||
				(i >= 0x6a && i < ADC_REG_NUM))
				continue;

			tmp = R_APB_REG(((ADC_BASE_ADD + i) << 2));
			if (i == 0x06) {  /* pga enable/disable */

				tmp &= 0x10;
				if (((cvd2->vd_port >= TVIN_PORT_CVBS0) &&
				(cvd2->vd_port <= TVIN_PORT_CVBS7)) &&
				(tmp != 0x10))
					cvd2->info.adc_reload_en = true;

			} else if (i == 0x17) { /* input pga mux */

				tmp &= 0x30;
				if ((((cvd2->vd_port == TVIN_PORT_CVBS0) &&
					(tmp != 0x00)) ||
					((cvd2->vd_port == TVIN_PORT_CVBS1) &&
					(tmp != 0x10)) ||
					((cvd2->vd_port == TVIN_PORT_CVBS2) &&
					(tmp != 0x20)) ||
					((cvd2->vd_port == TVIN_PORT_CVBS3) &&
					(tmp != 0x30))
					) &&
					(get_cpu_type() <
					MESON_CPU_TYPE_MESONG9TV))
					cvd2->info.adc_reload_en = true;

			} else if (i == 0x3d) { /* FILTPLLHSYNC */

				tmp &= 0x80;
				if (tmp != 0x80)
					cvd2->info.adc_reload_en = true;

			} else{

				if (tmp != adc_cvbs_table[i])
					cvd2->info.adc_reload_en = true;
			}
		}

		/* if adc reg readback error reload adc reg table */
		if (cvd2->info.adc_reload_en) {

			/** write 7740 register **/
			tvafe_adc_configure(TVIN_SIG_FMT_CVBS_PAL_I);
			if (cvd_dbg_en)
				pr_info("[tvafe..] %s: Reg error!!!! reload adc table.\n",
						__func__);
		}
	}
#endif
}
#endif

/*
 * tvafe cvd2 init if no signal input
 */
static void tvafe_cvd2_reinit(struct tvafe_cvd2_s *cvd2)
{
	if (cvd2->cvd2_init_en)
		return;


#ifdef TVAFE_SET_CVBS_CDTO_EN
	if ((CVD2_CHROMA_DTO_PAL_I != tvafe_cvd2_get_cdto()) &&
			(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)) {

		tvafe_cvd2_set_cdto(CVD2_CHROMA_DTO_PAL_I);
		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: set default cdto.\n", __func__);

	}
	#endif
	/* reset pga value */
#ifdef TVAFE_SET_CVBS_PGA_EN
	tvafe_cvd2_reset_pga();
#endif
	/* init variable */
	memset(&cvd2->info, 0, sizeof(struct tvafe_cvd2_info_s));
	cvd2->cvd2_init_en = true;

	if (cvd_dbg_en)
		pr_info("[tvafe..] %s: reinit cvd2.\n", __func__);
}

/*
 * tvafe cvd2 signal status
 */
inline bool tvafe_cvd2_no_sig(struct tvafe_cvd2_s *cvd2,
			struct tvafe_cvd2_mem_s *mem)
{
	static bool ret;
	static int time_flag;

	tvafe_cvd2_get_signal_status(cvd2);

	/*TVAFE register status need more time to be stable.
	for double time delay.*/
	time_flag++;
	if (time_flag%2 != 0)
		return ret;

	/* get signal status from HW */

	/* search video mode */
	tvafe_cvd2_search_video_mode(cvd2, mem);

	/* verify adc register for esd test */
#ifdef TVAFE_CVD2_ADC_REG_CHECK
	tvafe_cvd2_check_adc_reg(cvd2);
#endif

	/* init if no signal input */
	if (cvd2->hw.no_sig) {

		ret = true;
		tvafe_cvd2_reinit(cvd2);

	} else{
		ret = false;
		cvd2->cvd2_init_en = false;
#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
	if ((!scene_colorful) && auto_de_en)
		tvafe_cvd2_auto_de(cvd2);
	else
		tvafe_cvd2_set_default_de(cvd2);


#endif
	}
	if (ret&try_format_cnt) {

		try_format_cnt = 0;
		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: initialize try_format_cnt to zero.\n",
					__func__);
	}
	return ret;
}

/*
 * tvafe cvd2 mode change status
 */
inline bool tvafe_cvd2_fmt_chg(struct tvafe_cvd2_s *cvd2)
{
	if (cvd2->info.state == TVAFE_CVD2_STATE_FIND)
		return false;
	else
		return true;
}

/*
 * tvafe cvd2 find the configured format
 */
inline enum tvin_sig_fmt_e tvafe_cvd2_get_format(
		struct tvafe_cvd2_s *cvd2)
{
	if (cvd2->info.state == TVAFE_CVD2_STATE_FIND)
		return cvd2->config_fmt;
	else
		return TVIN_SIG_FMT_NULL;
}

#ifdef TVAFE_SET_CVBS_PGA_EN
/*
 * tvafe cvd2 pag ajustment in vsync interval
 */
inline void tvafe_cvd2_adj_pga(struct tvafe_cvd2_s *cvd2)
{
	unsigned short dg_max = 0, dg_min = 0xffff, dg_ave = 0, i = 0, pga = 0;
	unsigned int tmp = 0;
	unsigned char step = 0;
	unsigned int delta_dg = 0;

	if ((cvd_isr_en & 0x100) == 0)
		return;

	cvd2->info.dgain[0] = cvd2->info.dgain[1];
	cvd2->info.dgain[1] = cvd2->info.dgain[2];
	cvd2->info.dgain[2] = cvd2->info.dgain[3];
	cvd2->info.dgain[3] = R_APB_BIT(CVD2_AGC_GAIN_STATUS_7_0,
			AGC_GAIN_7_0_BIT, AGC_GAIN_7_0_WID);
	cvd2->info.dgain[3] |= R_APB_BIT(CVD2_AGC_GAIN_STATUS_11_8,
			AGC_GAIN_11_8_BIT, AGC_GAIN_11_8_WID)<<8;
	for (i = 0; i < 4; i++) {

		if (dg_max < cvd2->info.dgain[i])
			dg_max = cvd2->info.dgain[i];
		if (dg_min > cvd2->info.dgain[i])
			dg_min = cvd2->info.dgain[i];
		dg_ave += cvd2->info.dgain[i];
	}
	if (++cvd2->info.dgain_cnt >=
		TVAFE_SET_CVBS_PGA_START + TVAFE_SET_CVBS_PGA_STEP){

		cvd2->info.dgain_cnt = TVAFE_SET_CVBS_PGA_START;
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
		if (cvd2->info.dgain_cnt == TVAFE_SET_CVBS_PGA_START) {

			cvd2->info.dgain_cnt = 0;
			dg_ave = (dg_ave - dg_max - dg_min + 1) >> 1;
			pga = R_APB_BIT(TVFE_VAFE_CTRL1, VAFE_PGA_GAIN_BIT,
							VAFE_PGA_GAIN_WID);

			delta_dg = abs((signed short)dg_ave -
				(signed short)dg_ave_last);
			if (((dg_ave >= CVD2_DGAIN_LIMITL) &&
				(dg_ave <= CVD2_DGAIN_LIMITH)) &&
				(delta_dg < CVD2_DGAIN_WINDOW*2)) {
				return;

			} else if ((dg_ave < CVD2_DGAIN_LIMITL) && (pga == 0)) {
				return;

			} else if ((dg_ave > CVD2_DGAIN_LIMITH) &&
				(pga >= (255+97))) {
				return;

			} else{
				if (cvd_dbg_en)
					pr_info("%s: dg_ave_last:0x%x dg_ave:0x%x. pga 0x%x.\n",
					__func__, dg_ave_last, dg_ave, pga);
				dg_ave_last = dg_ave;
			}
			tmp = abs((signed short)dg_ave -
				(signed short)CVD2_DGAIN_MIDDLE);

			if (tmp > CVD2_DGAIN_MIDDLE)
				step = 16;
			else if (tmp > (CVD2_DGAIN_MIDDLE >> 1))
				step = 5;
			else if (tmp > (CVD2_DGAIN_MIDDLE >> 2))
				step = 2;
			else
				step = 1;
			if ((delta_dg > CVD2_DGAIN_MIDDLE) ||
				((delta_dg == 0) &&
				(tmp > (CVD2_DGAIN_MIDDLE >> 2))))
				step = pga_delta_val;
			if (dg_ave > CVD2_DGAIN_LIMITH) {

				pga += step;
				if (pga >= 255)  /* set max value */
					pga = 255;

			} else{

				if (pga < step)  /* set min value */
					pga = 0;
				else
					pga -= step;
			}
			if (pga < 2)
				pga = 2;

			if (pga != R_APB_BIT(TVFE_VAFE_CTRL1,
				VAFE_PGA_GAIN_BIT, VAFE_PGA_GAIN_WID)){

					if (cvd_dbg_en)
						pr_info("%s: set pag:0x%x. current dgain 0x%x.\n",
					__func__, pga, cvd2->info.dgain[3]);
					W_APB_BIT(TVFE_VAFE_CTRL1, pga,
				VAFE_PGA_GAIN_BIT, VAFE_PGA_GAIN_WID);
			}
		}

		return;

	} else{

		if (cvd2->info.dgain_cnt == TVAFE_SET_CVBS_PGA_START) {

			cvd2->info.dgain_cnt = 0;
			dg_ave = (dg_ave - dg_max - dg_min + 1) >> 1;
			pga = R_APB_BIT(ADC_REG_05, PGAGAIN_BIT, PGAGAIN_WID);

			if (R_APB_BIT(ADC_REG_06, PGAMODE_BIT, PGAMODE_WID)) {

				/* pga += 64; */
				pga += 97;
			}
			delta_dg = abs((signed short)dg_ave -
				(signed short)dg_ave_last);
			if (((dg_ave >= CVD2_DGAIN_LIMITL) &&
				(dg_ave <= CVD2_DGAIN_LIMITH)) &&
				(delta_dg < CVD2_DGAIN_WINDOW*2)) {
				return;

			} else if ((dg_ave < CVD2_DGAIN_LIMITL) && (pga == 0)) {
				return;

			} else if ((dg_ave > CVD2_DGAIN_LIMITH) &&
				(pga >= (255+97))) {
				return;

			} else{
				if (cvd_dbg_en)
					pr_info("%s: dg_ave_last:0x%x dg_ave:0x%x. pga 0x%x.\n",
					__func__, dg_ave_last, dg_ave, pga);
				dg_ave_last = dg_ave;
			}
			tmp = abs((signed short)dg_ave -
				(signed short)CVD2_DGAIN_MIDDLE);

			if (tmp > CVD2_DGAIN_MIDDLE)
				step = 16;
			else if (tmp > (CVD2_DGAIN_MIDDLE >> 1))
				step = 5;
			else if (tmp > (CVD2_DGAIN_MIDDLE >> 2))
				step = 2;
			else
				step = 1;
			if ((delta_dg > CVD2_DGAIN_MIDDLE) ||
				((delta_dg == 0) &&
				(tmp > (CVD2_DGAIN_MIDDLE >> 2))))
				step = pga_delta_val;
			if (dg_ave > CVD2_DGAIN_LIMITH) {

				pga += step;
				if (pga >= (255+97))  /* set max value */
					pga = (255+97);

			} else{

				if (pga < step)  /* set min value */
					pga = 0;
				else
					pga -= step;
			}
			if (pga > 255) {

				/* pga -= 64; */
				pga -= 97;
				W_APB_BIT(ADC_REG_06, 1,
					PGAMODE_BIT, PGAMODE_WID);

			} else{

				if (pga < 2)
					pga = 2;
				W_APB_BIT(ADC_REG_06, 0,
					PGAMODE_BIT, PGAMODE_WID);
			}

			if (pga != R_APB_BIT(ADC_REG_05 ,
				PGAGAIN_BIT, PGAGAIN_WID)) {

				if (cvd_dbg_en)
					pr_info("%s: set pag:0x%x. current dgain 0x%x.\n",
					__func__, pga, cvd2->info.dgain[3]);
				W_APB_BIT(ADC_REG_05, pga,
					PGAGAIN_BIT, PGAGAIN_WID);
			}
		}

		return;
	}
}
#endif

#ifdef TVAFE_SET_CVBS_CDTO_EN
/*
 * tvafe cvd2 cdto tune in vsync interval
 */
static void tvafe_cvd2_cdto_tune(unsigned int cur, unsigned int dest)
{
	unsigned int diff = 0, step = 0;

	diff = (unsigned int)abs((signed int)cur - (signed int)dest);

	if (diff == 0)

		return;


	if ((diff > (diff>>1)) && (diff > (0x1<<cdto_adj_step)))
		step = diff >> cdto_adj_step;
	else
		step = 1;

	if (cur > dest)
		cur -= step;
	else
		cur += step;

	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_29_24, (cur >> 24) & 0x0000003f);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_23_16, (cur >> 16) & 0x000000ff);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_15_8,  (cur >>  8) & 0x000000ff);
	W_APB_REG(CVD2_CHROMA_DTO_INCREMENT_7_0,   (cur >>  0) & 0x000000ff);

}
inline void tvafe_cvd2_adj_hs(struct tvafe_cvd2_s *cvd2,
			unsigned int hcnt64)
{
	unsigned int hcnt64_max, hcnt64_min;
	unsigned int diff, hcnt64_ave, i, hcnt64_standard;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
		hcnt64_standard = 0x31380;
	else
		hcnt64_standard = 0x17a00;

	if ((cvd_isr_en & 0x1000) == 0)
		return;

	/* only for pal-i adjusment */
	if (cvd2->config_fmt != TVIN_SIG_FMT_CVBS_PAL_I)
		return;

	cvd2->info.hcnt64[0] = cvd2->info.hcnt64[1];
	cvd2->info.hcnt64[1] = cvd2->info.hcnt64[2];
	cvd2->info.hcnt64[2] = cvd2->info.hcnt64[3];
	cvd2->info.hcnt64[3] = hcnt64;
	hcnt64_ave = 0;
	hcnt64_max = 0;
	hcnt64_min = 0xffffffff;
	for (i = 0; i < 4; i++) {
		if (hcnt64_max < cvd2->info.hcnt64[i])
			hcnt64_max = cvd2->info.hcnt64[i];
		if (hcnt64_min > cvd2->info.hcnt64[i])
			hcnt64_min = cvd2->info.hcnt64[i];
		hcnt64_ave += cvd2->info.hcnt64[i];
	}
	if (++cvd2->info.hcnt64_cnt >= 300)
		cvd2->info.hcnt64_cnt = 300;
	if (cvd2->info.hcnt64_cnt == 300) {
		hcnt64_ave = (hcnt64_ave - hcnt64_max - hcnt64_min + 1) >> 1;
		if (hcnt64_ave == 0)  /* to avoid kernel crash */
			return;
		diff = abs(hcnt64_ave - hcnt64_standard);
		if (diff > hs_adj_th_level0) {
			if (R_APB_REG(CVD2_YC_SEPARATION_CONTROL) != 0x11)
				W_APB_REG(CVD2_YC_SEPARATION_CONTROL, 0x11);
			if (R_APB_REG(CVD2_H_LOOP_MAXSTATE) != 0xc)
				W_APB_REG(CVD2_H_LOOP_MAXSTATE, 0xc);
			if (R_APB_REG(CVD2_REG_87) != 0xc0)
				W_APB_REG(CVD2_REG_87, 0xc0);
			cvd2->info.hs_adj_en = 1;
			if (diff > hs_adj_th_level4)
				cvd2->info.hs_adj_level = 4;
			else if (diff > hs_adj_th_level3)
				cvd2->info.hs_adj_level = 3;
			else if (diff > hs_adj_th_level2)
				cvd2->info.hs_adj_level = 2;
			else if (diff > hs_adj_th_level1)
				cvd2->info.hs_adj_level = 1;
			else
				cvd2->info.hs_adj_level = 0;
			if (hcnt64_ave > hcnt64_standard)
				cvd2->info.hs_adj_dir = 1;
			else
				cvd2->info.hs_adj_dir = 0;
		} else {
			if (R_APB_REG(CVD2_YC_SEPARATION_CONTROL) != 0x12)
				W_APB_REG(CVD2_YC_SEPARATION_CONTROL, 0x12);
			if (R_APB_REG(CVD2_H_LOOP_MAXSTATE) != 0xd)
				W_APB_REG(CVD2_H_LOOP_MAXSTATE, 0xd);
			if (R_APB_REG(CVD2_REG_87) != 0x0)
				W_APB_REG(CVD2_REG_87, 0x0);
			cvd2->info.hs_adj_en = 0;
			cvd2->info.hs_adj_level = 0;
		}
	}
}
/*
 * tvafe cvd2 cdto adjustment in vsync interval
 */
inline void tvafe_cvd2_adj_cdto(struct tvafe_cvd2_s *cvd2,
			unsigned int hcnt64)
{
	unsigned int hcnt64_max = 0, hcnt64_min = 0xffffffff,
				hcnt64_ave = 0, i = 0;
	unsigned int cur_cdto = 0, diff = 0;
	u64 cal_cdto = 0;


	if ((cvd_isr_en & 0x001) == 0)
		return;

	/* only for pal-i adjusment */
	if (cvd2->config_fmt != TVIN_SIG_FMT_CVBS_PAL_I)
		return;

	cvd2->info.hcnt64[0] = cvd2->info.hcnt64[1];
	cvd2->info.hcnt64[1] = cvd2->info.hcnt64[2];
	cvd2->info.hcnt64[2] = cvd2->info.hcnt64[3];
	cvd2->info.hcnt64[3] = hcnt64;
	for (i = 0; i < 4; i++) {
		if (hcnt64_max < cvd2->info.hcnt64[i])
			hcnt64_max = cvd2->info.hcnt64[i];
		if (hcnt64_min > cvd2->info.hcnt64[i])
			hcnt64_min = cvd2->info.hcnt64[i];
		hcnt64_ave += cvd2->info.hcnt64[i];
	}
	if (++cvd2->info.hcnt64_cnt >=
		TVAFE_SET_CVBS_CDTO_START + TVAFE_SET_CVBS_CDTO_STEP){

		cvd2->info.hcnt64_cnt = TVAFE_SET_CVBS_CDTO_START;
	}
	if (cvd2->info.hcnt64_cnt == TVAFE_SET_CVBS_CDTO_START) {

		hcnt64_ave = (hcnt64_ave - hcnt64_max -
					hcnt64_min + 1) >> cdto_filter_factor;
		if (hcnt64_ave == 0)  /* to avoid kernel crash */
			return;
		cal_cdto = CVD2_CHROMA_DTO_PAL_I;
		cal_cdto *= cdto_clamp;
		do_div(cal_cdto, hcnt64_ave);

		cur_cdto = tvafe_cvd2_get_cdto();
		diff = (unsigned int)abs((signed int)cal_cdto -
				(signed int)CVD2_CHROMA_DTO_PAL_I);

		if (diff < cdto_adj_th) {

			/* reset cdto to default value */
			if (cur_cdto != CVD2_CHROMA_DTO_PAL_I)
				tvafe_cvd2_cdto_tune(cur_cdto,
					(unsigned int)CVD2_CHROMA_DTO_PAL_I);
			cvd2->info.non_std_worst = 0;
			return;
		} else
			cvd2->info.non_std_worst = 1;

		if (cvd_dbg_en)
			pr_info("[tvafe..] %s: adj cdto from:0x%x to:0x%x\n",
					__func__, (u32)cur_cdto, (u32)cal_cdto);
		tvafe_cvd2_cdto_tune(cur_cdto, (unsigned int)cal_cdto);
	}

	return;
}
#endif

#ifdef SYNC_HEIGHT_AUTO_TUNING
/*
 * tvafe cvd2 sync height ajustment for picture quality in vsync interval
 */
static inline void tvafe_cvd2_sync_hight_tune(
		struct tvafe_cvd2_s *cvd2)
{
	int burst_mag = 0;
	int burst_mag_16msb = 0, burst_mag_16lsb = 0;
	unsigned int reg_sync_height = 0;
	int burst_mag_upper_limitation = 0;
	int burst_mag_lower_limitation = 0;
	unsigned int std_sync_height = 0xdd;
	unsigned int cur_div_result = 0;
	unsigned int mult_result = 0;
	unsigned int final_contrast = 0;
	unsigned int reg_contrast_default = 0;

	/*if (cvd2->info.non_std_config) {
	} else if (((cvd2->vd_port == TVIN_PORT_CVBS3) ||
		(cvd2->vd_port == TVIN_PORT_CVBS0)) &&
		(is_meson_gxtvbb_cpu())) {
	} else */
	if ((cvd2->config_fmt == TVIN_SIG_FMT_CVBS_NTSC_M) ||
		(cvd2->config_fmt == TVIN_SIG_FMT_CVBS_PAL_I)) {
		/* try to detect AVin NTSCM/PALI */
		if (cvd2->config_fmt ==
			TVIN_SIG_FMT_CVBS_NTSC_M) {

			burst_mag_upper_limitation =
		NTSC_BURST_MAG_UPPER_LIMIT & 0xffff;
			burst_mag_lower_limitation =
		NTSC_BURST_MAG_LOWER_LIMIT & 0xffff;
			reg_contrast_default = 0x7b;

		} else if (cvd2->config_fmt ==
		TVIN_SIG_FMT_CVBS_PAL_I) {

			burst_mag_upper_limitation =
		PAL_BURST_MAG_UPPER_LIMIT & 0xffff;
			burst_mag_lower_limitation =
		PAL_BURST_MAG_LOWER_LIMIT & 0xffff;
			reg_contrast_default = 0x7d;
		}

		burst_mag_16msb = R_APB_REG(CVD2_STATUS_BURST_MAGNITUDE_LSB);
		burst_mag_16lsb = R_APB_REG(CVD2_STATUS_BURST_MAGNITUDE_MSB);
		burst_mag = ((burst_mag_16msb&0xff) << 8) |
				(burst_mag_16lsb&0xff);
		if (burst_mag > burst_mag_upper_limitation) {

			reg_sync_height = R_APB_REG(CVD2_LUMA_AGC_VALUE);
			if (reg_sync_height > SYNC_HEIGHT_LOWER_LIMIT) {

				reg_sync_height = reg_sync_height - 1;
				W_APB_REG(CVD2_LUMA_AGC_VALUE,
					reg_sync_height&0xff);

				cur_div_result = std_sync_height << 16;
				do_div(cur_div_result, reg_sync_height);
				mult_result = cur_div_result *
					(reg_contrast_default&0xff);
				final_contrast = (mult_result + 0x8000) >> 16;
				if (final_contrast > 0xff)
					W_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,
					0xff);
				else if (final_contrast > 0x50)
					W_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,
							final_contrast&0xff);
			}

		} else if (burst_mag < burst_mag_lower_limitation) {

			reg_sync_height = R_APB_REG(CVD2_LUMA_AGC_VALUE);
			if (reg_sync_height < SYNC_HEIGHT_UPPER_LIMIT) {

				reg_sync_height = reg_sync_height + 1;
				W_APB_REG(CVD2_LUMA_AGC_VALUE,
					reg_sync_height&0xff);
				cur_div_result = std_sync_height << 16;
				do_div(cur_div_result, reg_sync_height);
				mult_result = cur_div_result *
					(reg_contrast_default&0xff);
				final_contrast = (mult_result + 0x8000) >> 16;
				if (final_contrast > 0xff)
					W_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,
						0xff);
				else if (final_contrast > 0x50)
					W_APB_REG(CVD2_LUMA_CONTRAST_ADJUSTMENT,
							final_contrast&0xff);
			}
		}
	}
}
#endif

/*
 * tvafe cvd2 3d comb error checking in vsync interval
 */
inline void tvafe_cvd2_check_3d_comb(struct tvafe_cvd2_s *cvd2)
{
	unsigned int cvd2_3d_status = R_APB_REG(CVD2_REG_95);

	if ((cvd_isr_en & 0x010) == 0)
		return;

#ifdef SYNC_HEIGHT_AUTO_TUNING
	tvafe_cvd2_sync_hight_tune(cvd2);
#endif

	if (cvd2->info.comb_check_cnt++ > CVD_3D_COMB_CHECK_MAX_CNT)


		cvd2->info.comb_check_cnt = 0;

	if (cvd2_3d_status & 0x1ffff) {

		W_APB_BIT(CVD2_REG_B2, 1, COMB2D_ONLY_BIT, COMB2D_ONLY_WID);
		W_APB_BIT(CVD2_REG_B2, 0, COMB2D_ONLY_BIT, COMB2D_ONLY_WID);
		/* if (cvd_dbg_en) */
		/* pr_info("%s: reset 3d comb  sts:0x%x\n",
		__func__, cvd2_3d_status); */
	}
}

/*
* tvafe cvd2 set reset to high
*/
inline void tvafe_cvd2_hold_rst(struct tvafe_cvd2_s *cvd2)
{
	W_APB_BIT(CVD2_RESET_REGISTER, 1, SOFT_RST_BIT, SOFT_RST_WID);
}

void tvafe_cvd2_set_reg8a(unsigned int v)
{
	cvd_reg8a = v;
	W_APB_REG(CVD2_CHROMA_LOOPFILTER_STATE, cvd_reg8a);
}
void tvafe_snow_config(unsigned int onoff)
{
	if (tvafe_snow_function_flag == 0)
		return;
	if (onoff) {
		W_APB_BIT(CVD2_OUTPUT_CONTROL, 3, BLUE_MODE_BIT, BLUE_MODE_WID);
	} else {
		W_APB_BIT(CVD2_OUTPUT_CONTROL, 0, BLUE_MODE_BIT, BLUE_MODE_WID);
	}
}

void tvafe_snow_config_clamp(unsigned int onoff)
{
	if (tvafe_snow_function_flag == 0)
		return;
	if (onoff)
		W_APB_BIT(TVFE_ATV_DMD_CLP_CTRL, 0, 20, 1);
	else
		W_APB_BIT(TVFE_ATV_DMD_CLP_CTRL, 1, 20, 1);
}
/*only for pal-i*/
void tvafe_snow_config_acd(void)
{
	if (tvafe_snow_function_flag == 0)
		return;
	/*0x8e035e is debug test result*/
	if (acd_h_config)
		W_APB_REG(ACD_REG_2D, acd_h_config);
}
/*only for pal-i*/
void tvafe_snow_config_acd_resume(void)
{
	if (tvafe_snow_function_flag == 0)
		return;
	/*@todo,0x880358 must be same with cvbs_acd_table/rf_acd_table*/
	if (R_APB_REG(ACD_REG_2D) != acd_h)
		W_APB_REG(ACD_REG_2D, acd_h);
}

enum tvin_aspect_ratio_e tvafe_cvd2_get_wss(void)
{
	unsigned int full_format = 0;
	enum tvin_aspect_ratio_e aspect_ratio = TVIN_ASPECT_NULL;
	full_format = R_APB_BIT(CVD2_VBI_WSS_DATA1, 0, 4);
	if (full_format == 0x8)
		aspect_ratio = TVIN_ASPECT_4x3;
	else if (full_format == 0x7)
		aspect_ratio = TVIN_ASPECT_16x9;
	else
		aspect_ratio = TVIN_ASPECT_NULL;
	return aspect_ratio;
}

