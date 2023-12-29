// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/deinterlace/deinterlace_mtn.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/uaccess.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include "register.h"
#include "deinterlace_mtn.h"
#include "deinterlace.h"
#include <linux/amlogic/media/di/di.h>

#include "di_pqa.h"
#define MAX_NUM_DI_REG 32
#define GXTVBB_REG_START 12
static unsigned int combing_setting_registers[MAX_NUM_DI_REG] = {
	DI_MTN_1_CTRL1,
	DI_MTN_1_CTRL2,
	DI_MTN_1_CTRL3,
	DI_MTN_1_CTRL4,
	DI_MTN_1_CTRL5,
	DI_MTN_1_CTRL6,
	DI_MTN_1_CTRL7,
	DI_MTN_1_CTRL8,
	DI_MTN_1_CTRL9,
	DI_MTN_1_CTRL10,
	DI_MTN_1_CTRL11,
	DI_MTN_1_CTRL12,
	/* below reg are gxtvbb only, offset defined in GXTVBB_REG_START */
	MCDI_REL_DET_PD22_CHK,
	MCDI_PD22_CHK_THD,
	MCDI_PD22_CHK_THD_RT,
	NR2_MATNR_DEGHOST,
	0
};

/* decide the levels based on glb_mot[0:4]
 * or with patches like Hist, smith trig logic
 * level:    0->1  1->2   2->3   3->4
 * from low motion to high motion level
 * take 720x480 as example
 */
static unsigned int combing_glb_mot_thr_LH[4] = {1440, 2880, 4760, 9520};

/* >> 4 */
static unsigned int combing_glbmot_radprat[4] = {32, 48, 64, 80};
static unsigned int num_glbmot_radprat = 4;
module_param_array(combing_glbmot_radprat, uint, &num_glbmot_radprat, 0664);

/* level:    0<-1  1<-2   2<-3   3<-4
 * from high motion to low motion level
 */
static unsigned int combing_glb_mot_thr_HL[4] = {720, 1440, 2880, 5760};

static unsigned int num_glb_mot_thr_LH = 4;
module_param_array(combing_glb_mot_thr_LH, uint, &num_glb_mot_thr_LH, 0664);
static unsigned int num_glb_mot_thr_HL = 4;
module_param_array(combing_glb_mot_thr_HL, uint, &num_glb_mot_thr_HL, 0664);

static int last_lev = -1;
static int force_lev = 0xff;
module_param_named(combing_force_lev, force_lev, int, 0664);
static int dejaggy_flag = -1;
module_param_named(combing_dejaggy_flag, dejaggy_flag, int, 0664);
int dejaggy_enable = 1;
module_param_named(combing_dejaggy_enable, dejaggy_enable, int, 0664);
static uint num_dejaggy_setting = 5;
/* 0:off 1:1-14-1 2:1-6-1 3:3-10-3 4:100% */
/* current setting dejaggy always on when interlace source */
static int combing_dejaggy_setting[6] = {1, 1, 1, 2, 3, 3};
module_param_array(combing_dejaggy_setting, uint,
	&num_dejaggy_setting, 0664);
static struct combing_param_s cmb_param;
/*from T3 /t5db adaptive_combing_new from vlsi yanling*/
#ifdef DI_NEW_PQ_V1
static int combing_debug_print;
module_param_named(combing_debug_print, combing_debug_print, int, 0664);
static int roku_th_test = 10000;
module_param_named(roku_th_test, roku_th_test, int, 0664);
static int th = 200;
module_param_named(th, th, int, 0664);
static int th_cont = 10;
module_param_named(th_cont, th_cont, int, 0664);
static int change_th = 26;
module_param_named(change_th, change_th, int, 0664);
#endif
/*end*/

static unsigned int combing_setting_masks[MAX_NUM_DI_REG] = {
	0x0fffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffff9f,
	0xffffffff,
	0x0003ff1f,
	0x01ff3fff,
	0x07ffff1f,
	0x000001ff,
	0
};

static unsigned int combing_pure_still_setting[MAX_NUM_DI_REG] = {
	0x00141410,
	0x1A1A3A62,
	0x15200A0A,
	0x01800880,
	0x74200D0D,
	0x0D5A1520,
	0x0A800480,
	0x1A1A2662,
	0x0D200302,
	0x02020202,
	0x06090748,
	0x40020A04,
	0x0001FF0C,
	0x00400204,
	0x00016404,
	0x00000199
};

static unsigned int combing_bias_static_setting[MAX_NUM_DI_REG] = {
	0x00141410,
	0x1A1A3A62,
	0x15200A0A,
	0x01800880,
	0x74200D0D,
	0x0D5A1520,
	0x0A800480,
	0x1A1A2662,
	0x0D200302,
	0x02020202,
	0x06090748,
	0x40020A04,
	0x0001FF0C,
	0x00400204,
	0x00016404,
	0x00000166
};

static unsigned int combing_normal_setting[MAX_NUM_DI_REG] = {
	0x00202015,
	0x1A1A3A62,
	0x15200a0a,
	0x01000880,
	0x74200D0D,
	0x0D5A1520,
	0x0A0A0201,
	0x1A1A2662,
	0x0D200302,
	0x02020606,
	0x05080344,
	0x40020a04,
	0x0001FF0C,
	0x00400204,
	0x00016404,
	0x00000153
};

static unsigned int combing_bias_motion_setting[MAX_NUM_DI_REG] = {
	0x00202015,	/* 0 */
	0x1A1A3A62,	/* 1 */
	0x15200101,	/* 2 */
	0x01200440,	/* 3 */
	0x74200D0D,	/* 4 */
	0x0D5A1520,	/* 5 */
	0x0A0A0201,	/* 6 */
	0x1A1A2662,	/* 7 */
	0x0D200302,	/* 8 */
	0x02020606,	/* 9 */
	0x05080344,	/* 10 */
	0x40020a04,	/* 11 */
	/*idea from mingliang.dong & vlsi zheng.bao begin*/
	0x0001FF12, /* 0x0001ff0c */
	0x00200204, /* 0x00400204 */
	/* change 0x00012002 to 0x00016404(idea from VLSI yanling.liu)*/
	0x00016404, /*22-12-09*/
	/*idea from mingliang.dong & vlsi zheng.bao end*/
	0x00000142
};

static unsigned int combing_very_motion_setting[MAX_NUM_DI_REG] = {
	0x00202015,	/* 0 */
	0x1A1A3A62,	/* 1 */
	0x15200101,	/* 2 */
	0x01200440,	/* 3 */
	0x74200D0D,	/* 4 */
	0x0D5A1520,	/* 5 */
	0x0A0A0201,	/* 6 */
	0x1A1A2662,	/* 7 */
	0x0D200302,	/* 8 */
	0x02020606,	/* 9 */
	0x05080344,	/* 10 */
	/*idea from mingliang.dong & vlsi zheng.bao begin*/
	0x60000404, /* 0x40020a04*/
	0x0001FF12, /* 0x0001ff0c */
	0x00200204, /* 0x00400204 */
	/* change 0x00012002 to 0x00016404(idea from VLSI yanling.liu)*/
	0x00016404, /*22-12-09*/
	/*idea from mingliang.dong & vlsi zheng.bao end*/
	0x00000131
};

/**************************************************
 *
 **************************************************/
/*same as: combing_bias_motion_setting*/
static const unsigned int combing_bias_p_ori[] = {
	/**/
	0x00202015,	/* 0 */
	0x1A1A3A62,	/* 1 */
	0x15200101,	/* 2 */
	0x01200440,	/* 3 */
	0x74200D0D,	/* 4 */
	0x0D5A1520,	/* 5 */
	0x0A0A0201,	/* 6 */
	0x1A1A2662,	/* 7 */
	0x0D200302,	/* 8 */
	0x02020606,	/* 9 */
	0x05080344,	/* 10 */
	0x40020a04,	/* 11 */
	/*idea from mingliang.dong & vlsi zheng.bao begin*/
	0x0001FF12, /* 0x0001ff0c */
	0x00200204, /* 0x00400204 */
	/* change 0x00012002 to 0x00016404(idea from VLSI yanling.liu)*/
	0x00016404, /*22-12-09*/
	/*idea from mingliang.dong & vlsi zheng.bao end*/
	0x00000142
};

static const unsigned int combing_very_p_ori[] = {
	0x00202015,	/* 0 */
	0x1A1A3A62,	/* 1 */
	0x15200101,	/* 2 */
	0x01200440,	/* 3 */
	0x74200D0D,	/* 4 */
	0x0D5A1520,	/* 5 */
	0x0A0A0201,	/* 6 */
	0x1A1A2662,	/* 7 */
	0x0D200302,	/* 8 */
	0x02020606,	/* 9 */
	0x05080344,	/* 10 */
	/*idea from mingliang.dong & vlsi zheng.bao begin*/
	0x60000404, /* 0x40020a04*/
	0x0001FF12, /* 0x0001ff0c */
	0x00200204, /* 0x00400204 */
	/* change 0x00012002 to 0x00016404(idea from VLSI yanling.liu)*/
	0x00016404, /*22-12-09*/
	/*idea from mingliang.dong & vlsi zheng.bao end*/
	0x00000131
};

static const unsigned int combing_bias_p_1080i[] = {
	/**/
	0x00202015,	/* 0 */
	0x1A1A3A62,	/* 1 */
	0x15200101,	/* 2 */
	0x01200440,	/* 3 */
	0x74200D0D,	/* 4 */
	0x0D5A1520,	/* 5 */
	0x0A0A0201,	/* 6 */
	0x1A1A2662,	/* 7 */
	0x0D200302,	/* 8 */
	0x02020606,	/* 9 */
	0x05080344,	/* 10 */
	0x40020a04,	/* 11 */
	/*idea from mingliang.dong & vlsi zheng.bao begin*/
	0x0001ff0c,
	0x00400204,
	0x00016404,
	/*idea from mingliang.dong & vlsi zheng.bao end*/
	0x00000142
};

static const unsigned int combing_very_p_1080i[] = {
	0x00202015,	/* 0 */
	0x1A1A3A62,	/* 1 */
	0x15200101,	/* 2 */
	0x01200440,	/* 3 */
	0x74200D0D,	/* 4 */
	0x0D5A1520,	/* 5 */
	0x0A0A0201,	/* 6 */
	0x1A1A2662,	/* 7 */
	0x0D200302,	/* 8 */
	0x02020606,	/* 9 */
	0x05080344,	/* 10 */
	/*idea from mingliang.dong & vlsi zheng.bao begin*/
	0x40020a04,
	0x0001ff0c,
	0x00400204,
	0x00016404,
	/*idea from mingliang.dong & vlsi zheng.bao end*/
	0x00000131
};

/*same as combing_bias_motion_setting ex:*/
/*	3nd : from 0x15200101 to 0x15200301*/
static unsigned int combing_bias_p_480i[] = {
	0x00202015,	/* 0 */
	0x1A1A3A62,	/* 1 */
	0x15200301,	/* 2 */
	0x01200440,	/* 3 */
	0x74200D0D,	/* 4 */
	0x0D5A1520,	/* 5 */
	0x0A0A0201,	/* 6 */
	0x1A1A2662,	/* 7 */
	0x0D200302,	/* 8 */
	0x02020606,	/* 9 */
	0x05080344,	/* 10 */
	0x40020a04,	/* 11 */
	/*idea from mingliang.dong & vlsi zheng.bao begin*/
	0x0001FF12, /* 0x0001ff0c */
	0x00200204, /* 0x00400204 */
	0x00012002, /* 0x00016404 */
	/*idea from mingliang.dong & vlsi zheng.bao end*/
	0x00000142
};

/*same as combing_very_motion_setting ex:*/
/*	3nd : from 0x15200101 to 0x15200301*/
static unsigned int combing_very_p_480i[] = {
	0x00202015,	/* 0 */
	0x1A1A3A62,	/* 1 */
	0x15200301,	/* 2 */
	0x01200440,	/* 3 */
	0x74200D0D,	/* 4 */
	0x0D5A1520,	/* 5 */
	0x0A0A0201,	/* 6 */
	0x1A1A2662,	/* 7 */
	0x0D200302,	/* 8 */
	0x04040606,	/* 9 */
	0x05080344,	/* 10 */
	/*idea from mingliang.dong & vlsi zheng.bao begin*/
	0x60000404, /* 0x40020a04*/
	0x0001FF12, /* 0x0001ff0c */
	0x00200204, /* 0x00400204 */
	0x00012002, /* 0x00016404 */
	/*idea from mingliang.dong & vlsi zheng.bao end*/
	0x00000131
};

static unsigned int di_mtn_p_mode;

void com_patch_pre_sw_set(unsigned int mode)
{
	unsigned int *p1, *p2;
	/* mode is 0: ori */
	/* mode is 1: 1080i */
	/* mode is 2: 480i */
	if (mode == di_mtn_p_mode)
		return;

	p1 = &combing_bias_motion_setting[0];
	p2 = &combing_very_motion_setting[0];
	if (mode == 0) {
		memcpy(p1, &combing_bias_p_ori[0],
		       sizeof(combing_bias_p_ori));
		memcpy(p2, &combing_very_p_ori[0],
		       sizeof(combing_very_p_ori));
		di_mtn_p_mode = 0;
	} else if (mode == 1) {
		memcpy(p1, &combing_bias_p_1080i[0],
		       sizeof(combing_bias_p_1080i));
		memcpy(p2, &combing_very_p_1080i[0],
		       sizeof(combing_very_p_1080i));
		di_mtn_p_mode = 1;
	} else if (mode == 2) {
		memcpy(p1, &combing_bias_p_480i[0],
		       sizeof(combing_bias_p_480i));
		memcpy(p2, &combing_very_p_480i[0],
		       sizeof(combing_very_p_480i));
		di_mtn_p_mode = 2;
	}
}

/*special for resolution test file*/
static unsigned int combing_resolution_setting[MAX_NUM_DI_REG] = {
	0x00202015,
	0x141a3a62,
	0x15200a0a,
	0x01800880,
	0x74200d0d,
	0x0d5a1520,
	0x0a800480,
	0x1a1a2662,
	0x0d200302,
	0x01010101,
	0x06090748,
	0x40020a04,
	0x0001ff0c,
	0x00400204,
	0x00016404,
	0x00000131
};

static unsigned int num_combing_setting_registers = MAX_NUM_DI_REG;
module_param_array(combing_setting_registers, uint,
	&num_combing_setting_registers, 0664);
static unsigned int num_combing_setting_masks = MAX_NUM_DI_REG;
module_param_array(combing_setting_masks, uint,
	&num_combing_setting_masks, 0664);
static unsigned int num_combing_pure_still_setting = MAX_NUM_DI_REG;
module_param_array(combing_pure_still_setting, uint,
	&num_combing_pure_still_setting, 0664);
static unsigned int num_combing_bias_static_setting = MAX_NUM_DI_REG;
module_param_array(combing_bias_static_setting, uint,
	&num_combing_bias_static_setting, 0664);
static unsigned int num_combing_normal_setting = MAX_NUM_DI_REG;
module_param_array(combing_normal_setting, uint, &num_combing_normal_setting,
	0664);
static unsigned int num_combing_bias_motion_setting = MAX_NUM_DI_REG;
module_param_array(combing_bias_motion_setting, uint,
	&num_combing_bias_motion_setting, 0664);
static unsigned int num_combing_very_motion_setting = MAX_NUM_DI_REG;
module_param_array(combing_very_motion_setting, uint,
	&num_combing_very_motion_setting, 0664);
/* 0: pure still; 1: bias static; 2: normal; 3: bias motion, 4: very motion */

static unsigned int (*combing_setting_values[6])[MAX_NUM_DI_REG] = {
	&combing_pure_still_setting,
	&combing_bias_static_setting,
	&combing_normal_setting,
	&combing_bias_motion_setting,
	&combing_very_motion_setting,
	&combing_resolution_setting
};
static struct combing_status_s cmb_status;
struct combing_status_s *adpative_combing_config(unsigned int width,
	unsigned int height,
	enum vframe_source_type_e src_type,
	bool prog, enum tvin_sig_fmt_e fmt)
{
	int i = 0;

	for (i = 0; i < 4; i++) {
		combing_glb_mot_thr_LH[i] =
			((width * combing_glbmot_radprat[i] + 8) >> 4);
		combing_glb_mot_thr_HL[i] =
			combing_glb_mot_thr_LH[i] - width;
	}
	last_lev = -1;
	cmb_param.width = width;
	cmb_param.height = height;
	cmb_param.field_idx = 0;
	cmb_param.src_type = src_type;
	cmb_param.fmt = fmt;
	cmb_param.prog_flag = prog;
	if (dim_config_crc_ic())
		cmb_status.cur_level = 0;//add for crc @2k22-0102
	return &cmb_status;
}

/*from PQ qianyufei,add ic config from sc2*/
void mtn_int_combing_glbmot(void)
{
	if (is_meson_tl1_cpu() || is_meson_tm2_cpu() ||
	    IS_IC(dil_get_cpuver_flag(), T5)	||
	    IS_IC(dil_get_cpuver_flag(), T5D)	||
	    IS_IC(dil_get_cpuver_flag(), T5DB) ||/*from VLSI yanling.liu*/
	    IS_IC_EF(dil_get_cpuver_flag(), SC2)) {
		combing_glbmot_radprat[0] = 30;
	}
	di_mtn_p_mode = 0;
}
void adpative_combing_exit(void)
{
}
static int cmb_adpset_cnt;
unsigned int adp_set_level(unsigned int diff, unsigned int field_diff_num)
{
	unsigned int rst = 0;
	char tlog[] = "LHM";

	if (diff <= combing_glb_mot_thr_LH[0])
		rst = 0;
	else if (diff >= combing_glb_mot_thr_LH[3])
		rst = 1;
	else
		rst = 2;

	if (cmb_adpset_cnt > 0) {
		pr_info("\field-num=%04d frame-num=%04d lvl=%c\n",
			field_diff_num, diff, tlog[rst]);
		cmb_adpset_cnt--;
	}

	return rst;
}

unsigned int adp_set_mtn_ctrl3(unsigned int diff, unsigned int dlvel)
{
	int istp = 0;
	int idats = 0;
	int idatm = 0;
	int idatr = 0;
	unsigned int rst = 0;

	if (dlvel == 0)
		rst = combing_pure_still_setting[2];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[2];
	else {
		rst = 0x1520;
		istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
			(combing_glb_mot_thr_LH[3] -
			combing_glb_mot_thr_LH[0] + 1);

		idats = (combing_pure_still_setting[2] >> 8) & 0xff;
		idatm = (combing_very_motion_setting[2] >> 8) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_pure_still_setting[2]) & 0xff;
		idatm = (combing_very_motion_setting[2]) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);
	}
	/*
	 *if (cmb_adpset_cnt > 0)
	 *pr_info("mtn_ctrl3=%8x\n", rst);
	 */

	return rst;
}

int cmb_num_rat_ctl4 = 64; /* 0~255 */
module_param(cmb_num_rat_ctl4, int, 0644);
MODULE_PARM_DESC(cmb_num_rat_ctl4, "cmb_num_rat_ctl4");

int cmb_rat_ctl4_minthd = 64;
module_param(cmb_rat_ctl4_minthd, int, 0644);
MODULE_PARM_DESC(cmb_rat_ctl4_minthd, "cmb_rat_ctl4_minthd");

unsigned int adp_set_mtn_ctrl4(unsigned int diff, unsigned int dlvel,
	unsigned int height, int cmb_cnt)
{
	int hHeight = height;
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;

	if (dlvel == 0)
		rst = combing_pure_still_setting[3];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[3];
	else {
			rst = 1;
			istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
				(combing_glb_mot_thr_LH[3] -
				combing_glb_mot_thr_LH[0] + 1);

			idats = (combing_pure_still_setting[3] >> 16) & 0xff;
			idatm = (combing_very_motion_setting[3] >> 16) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			idatr = idatr >> 1;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[3] >> 8) & 0xff;
			idatm = (combing_very_motion_setting[3] >> 8) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[3]) & 0xff;
			idatm = (combing_very_motion_setting[3]) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst << 8) | (idatr & 0xff);
	}

		istp = ((cmb_num_rat_ctl4 * hHeight + 128) >> 8);
		if (cmb_adpset_cnt > 0)
			pr_info("mtn_ctrl4=%8x %03d (%03d)\n",
				rst, istp, cmb_cnt);
	if (cmb_cnt > istp) {
		istp = 64 * (hHeight - cmb_cnt) / (hHeight - istp + 1);
		if (istp < 4)
			istp = 4;

		idatm = 1;
		idats = (rst >> 16) & 0xff;
		idatr = ((idats * istp + 32) >> 6);
				idatr = idatr >> 1; /*color*/
		if (idatr < (cmb_rat_ctl4_minthd >> 1))
			idatr = (cmb_rat_ctl4_minthd >> 1);
		idatm = (idatm<<8) | (idatr & 0xff);

		idats = (rst >> 8) & 0xff;
		idatr = ((idats * istp + 32) >> 6);
		if (idatr < 4)
			idatr = 4;
		idatm = (idatm<<8) | (idatr & 0xff);

		idats = rst & 0xff;
		idatr = ((idats * istp + 32) >> 6);
		if (idatr < cmb_rat_ctl4_minthd)
			idatr = cmb_rat_ctl4_minthd;
		idatm = (idatm<<8) | (idatr & 0xff);

		rst = idatm;

		if (cmb_adpset_cnt > 0)
			pr_info("%03d (%03d)=%8x\n",
				cmb_cnt, hHeight, rst);
	}

	return rst;
}

unsigned int adp_set_mtn_ctrl7(unsigned int diff, unsigned int dlvel)
{
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;

	if (dlvel == 0)
		rst = combing_pure_still_setting[6];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[6];
	else {
			rst = 10;
			istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
				(combing_glb_mot_thr_LH[3] -
				combing_glb_mot_thr_LH[0] + 1);

			idats = (combing_pure_still_setting[6] >> 16) & 0xff;
			idatm = (combing_very_motion_setting[6] >> 16) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[6] >> 8) & 0xff;
			idatm = (combing_very_motion_setting[6] >> 8) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);

			idats = (combing_pure_still_setting[6]) & 0xff;
			idatm = (combing_very_motion_setting[6]) & 0xff;

			idatr = ((idats - idatm) * istp >> 6) + idatm;
			rst = (rst<<8) | (idatr & 0xff);
	}
	/*
	 *if (cmb_adpset_cnt > 0) {
	 *	pr_info("mtn_ctrl7=%8x\n", rst);
	 *}
	 */
	return rst;
}
static unsigned int small_local_mtn = 70;
module_param(small_local_mtn, uint, 0644);
MODULE_PARM_DESC(small_local_mtn, "small_local_mtn");

static unsigned int small_local_mtn1 = 30;
module_param(small_local_mtn1, uint, 0644);
MODULE_PARM_DESC(small_local_mtn1, "small_local_mtn1");

unsigned int adp_set_mtn_ctrl10(unsigned int diff, unsigned int dlvel,
	unsigned int frame_diff_avg)
{
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;

	if (
		(frame_diff_avg < small_local_mtn) &&
		(frame_diff_avg > small_local_mtn1))
		rst = combing_very_motion_setting[9];
	else if (dlvel == 0) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
			rst = 0x01010101;
		else
			rst = combing_pure_still_setting[9];
	} else if (dlvel == 1)
		rst = combing_very_motion_setting[9];
	else {
		istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
			(combing_glb_mot_thr_LH[3] -
			combing_glb_mot_thr_LH[0] + 1);

		idats = (combing_very_motion_setting[9] >> 24) & 0xff;
		idatm = (combing_pure_still_setting[9] >> 24) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_very_motion_setting[9] >> 16) & 0xff;
		idatm = (combing_pure_still_setting[9] >> 16) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_very_motion_setting[9] >> 8) & 0xff;
		idatm = (combing_pure_still_setting[9] >> 8) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_very_motion_setting[9]) & 0xff;
		idatm = (combing_pure_still_setting[9]) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);
	}

	if (cmb_adpset_cnt > 0) {
		pr_info("mtn_ctr10=0x%08x (frame_dif_avg=%03d)\n",
			rst, frame_diff_avg);
	}
	return rst;
}

unsigned int adp_set_mtn_ctrl11(unsigned int diff, unsigned int dlvel)
{
	int istp = 0, idats = 0, idatm = 0, idatr = 0;
	unsigned int rst = 0;

	if (dlvel == 0)
		rst = combing_pure_still_setting[10];
	else if (dlvel == 1)
		rst = combing_very_motion_setting[10];
	else {
		istp = 64 * (diff - combing_glb_mot_thr_LH[0]) /
			(combing_glb_mot_thr_LH[3] -
			combing_glb_mot_thr_LH[0] + 1);

		idats = (combing_pure_still_setting[10] >> 24) & 0xff;
		idatm = (combing_very_motion_setting[10] >> 24) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_pure_still_setting[10] >> 16) & 0xff;
		idatm = (combing_very_motion_setting[10] >> 16) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_pure_still_setting[10] >> 8) & 0xff;
		idatm = (combing_very_motion_setting[10] >> 8) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);

		idats = (combing_pure_still_setting[10]) & 0xff;
		idatm = (combing_very_motion_setting[10]) & 0xff;

		idatr = ((idats - idatm) * istp >> 6) + idatm;
		rst = (rst<<8) | (idatr & 0xff);
	}
	/*
	 *if (cmb_adpset_cnt > 0) {
	 *	pr_info("mtn_ctr11=%8x\n", rst);
	 *}
	 */
	return rst;
}

static void set_combing_regs(int lvl, int bit_mode)
{
	int i;
	unsigned int ndat = 0;

	for (i = 0; i < MAX_NUM_DI_REG; i++) {
		if ((combing_setting_registers[i] == 0)
		|| (combing_setting_masks[i] == 0))
			break;
		if (combing_setting_registers[i] == DI_MTN_1_CTRL1)
			DI_Wr_reg_bits(DI_MTN_1_CTRL1,
				((*combing_setting_values[lvl])[0] &
				combing_setting_masks[i]), 0, 24);
			/*working on db, driver don't handle this
			 *confirmed with vlsi-baozheng, G12a/G12B/SM1
			 *is same as TL1, Change the condition to cpu after G12a
			 */
		if ((bit_mode != 10 ||
		     cpu_after_eq(MESON_CPU_MAJOR_ID_G12A) ||
		     is_meson_txlx_cpu()) &&
		    combing_setting_registers[i] == NR2_MATNR_DEGHOST)
			break;
		else if (i < GXTVBB_REG_START) {
			/* TODO: need change to check if
			 * register only in GCTVBB
			 */
			ndat = (*combing_setting_values[lvl])[i];
			DI_Wr(combing_setting_registers[i], ndat);
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB))
			DI_Wr(combing_setting_registers[i],
				((*combing_setting_values[lvl])[i] &
				combing_setting_masks[i]) |
				(Rd(
					combing_setting_registers[i])
					& ~combing_setting_masks[i]));
	}
}

static int di_debug_readreg;
module_param(di_debug_readreg, int, 0644);
MODULE_PARM_DESC(di_debug_readreg, "di_debug_readreg");

/*from VLSI yanling.liu, the patch fix TL1 1080I in some dark */
/*scenes and roller coasters have small sawtooth, when turn off*/
/*combing_fix_en, set the registers*/
static void fix_tl1_1080i_sawtooth_patch(void)
{
	DI_Wr(0x1741, 0x0A0A1A22);
	DI_Wr(0x1742, 0x0a100101);
	DI_Wr(0x1743, 0x01020420);
	DI_Wr(0x1744, 0x32210404);
	DI_Wr(0x17a9, 0x0a100505);
	DI_Wr(0x17aa, 0x04040101);
	DI_Wr(0x17ab, 0x0a0a0a0a);
	DI_Wr(0x17ac, 0x0f100101);
	DI_Wr(0x17ad, 0x04040606);
	DI_Wr(0x17ae, 0x02030202);
	DI_Wr(0x17af, 0x60020a60);
}

static void fix_tl1_1080i_patch2(void)
{
	DI_Wr(0x1741, 0x010a010a);
	DI_Wr(0x1742, 0x01010101);
	DI_Wr(0x1743, 0x00000101);
	DI_Wr(0x1744, 0x00000101);
	DI_Wr(0x17a9, 0x01010102);
	DI_Wr(0x17aa, 0x02020101);
	DI_Wr(0x17ab, 0x010a010a);
	DI_Wr(0x17ac, 0x010a0102);
	DI_Wr(0x17ad, 0x08080808);
	DI_Wr(0x17ae, 0x01010101);
	DI_Wr(0x17af, 0xff00031f);
}

static void fix_s1a_576i_patch(void)
{
	DI_Wr(0x1741, 0x1a1a2a62);
	DI_Wr(0x1742, 0x10200909);
	DI_Wr(0x1743, 0x01200440);
	DI_Wr(0x1744, 0x74200d0d);
	DI_Wr(0x17a9, 0x0d5a1520);
	DI_Wr(0x17aa, 0x0a7a037a);
	DI_Wr(0x17ab, 0x1a1a2662);
	DI_Wr(0x17ac, 0x0d200302);
	DI_Wr(0x17ad, 0x04040606);
	DI_Wr(0x17ae, 0x05080647);
	DI_Wr(0x17af, 0x40020a04);
}

void fix_tl1_1080i_patch_sel(unsigned int mode)
{
	switch (mode) {
	case 0:/*not set*/
		break;
	case 1:
		fix_tl1_1080i_sawtooth_patch();
		break;
	case 2:
		fix_tl1_1080i_patch2();
		break;
	}
}

void fix_s1a_576i_patch_sel(void)
{
	fix_s1a_576i_patch();
}

static int combing_cnt;
int combing_diff_min = 2000;
int combing_diff_max = 2000;
int combing_cnt_thd = 10;

int adaptive_combing_fixing(
	struct combing_status_s *cmb_status,
	unsigned int field_diff,
	unsigned int frame_diff,
	int bit_mode)
{
	unsigned int glb_mot_avg2;
	unsigned int glb_mot_avg3;
	unsigned int glb_mot_avg5;

	unsigned int diff = 0;
	unsigned int wt_dat = 0;
	unsigned int dlvl = 0;
	static unsigned int pre_dat[5];
	bool prt_flg = (cmb_adpset_cnt > 0);
	unsigned int i = 0;
	static unsigned int pre_num;
	unsigned int crt_num = field_diff;
	unsigned int drat = 0;
	int tmp = 0;
	static int still_field_count;
	static int glb_mot[5] = {0, 0, 0, 0, 0};

	if (pre_num > crt_num)
		diff = pre_num - crt_num;
	else
		diff = crt_num - pre_num;

	if (diff >= cmb_param.width)
		cmb_status->field_diff_rate = 0;
	else {
		drat = (diff << 8) / (cmb_param.width + 1);
		if (drat > 255)
			cmb_status->field_diff_rate = 0;
		else
			cmb_status->field_diff_rate = 256 - drat;
	}
	pre_num = crt_num;

	if (di_debug_readreg > 1) {
		for (i = 0; i < 12; i++) {
			wt_dat = Rd(combing_setting_registers[i]);
			pr_info("mtn_ctrl%02d = 0x%08x\n",
				i+1, wt_dat);
		}
		pr_info("\n");
		di_debug_readreg--;
	}

	glb_mot[4] = glb_mot[3];
	glb_mot[3] = glb_mot[2];
	glb_mot[2] = glb_mot[1];
	glb_mot[1] = glb_mot[0];
	glb_mot[0] = frame_diff;
	if (glb_mot[0] < combing_diff_min)
		combing_cnt = combing_cnt + 1;
	else
		combing_cnt = 0;
	/*from PQ qianyufei,add ic config from sc2*/
	if (
		is_meson_txhd_cpu() ||
		is_meson_tl1_cpu() ||
		is_meson_tm2_cpu() ||
		IS_IC(dil_get_cpuver_flag(), T5)	||
		IS_IC(dil_get_cpuver_flag(), T5D)	||
		IS_IC(dil_get_cpuver_flag(), T5DB)	||
		IS_IC_EF(dil_get_cpuver_flag(), SC2)) {
		if (
			glb_mot[0] < combing_diff_min &&
			glb_mot[1] > combing_diff_max &&
			combing_cnt <= combing_cnt_thd)
			glb_mot[0] = glb_mot[1];
	}
	glb_mot_avg5 =
		(glb_mot[0] + glb_mot[1] + glb_mot[2] + glb_mot[3] +
		 glb_mot[4]) / 5;
	glb_mot_avg3 = (glb_mot[0] + glb_mot[1] + glb_mot[2]) / 3;
	glb_mot_avg2 = (glb_mot[0] + glb_mot[1]) / 2;

	if (glb_mot[0] > combing_glb_mot_thr_LH[0])
		still_field_count = 0;
	else
	if (still_field_count < 16)
		still_field_count++;
	tmp = cmb_status->cur_level;
	if (glb_mot_avg3 > combing_glb_mot_thr_LH[min(tmp, 3)]) {
		if (cmb_status->cur_level < 4)
			cmb_status->cur_level++;
	} else {
		tmp = cmb_status->cur_level;
		if (glb_mot_avg5 <
		    combing_glb_mot_thr_HL[max((tmp - 1), 0)]) {
			if (cmb_status->cur_level <= 1 && still_field_count > 5)
				cmb_status->cur_level = 0;
			else
				cmb_status->cur_level = max((tmp - 1), 1);
		}
	}
	if ((force_lev >= 0) & (force_lev < 6))
		cmb_status->cur_level = force_lev;
	if (cmb_status->cur_level != last_lev) {
		set_combing_regs(cmb_status->cur_level, bit_mode);
		if (prt_flg)
			pr_info("\t%5d: from %d to %d: mtn_1_ctrl1 = %08x\n",
				cmb_param.field_idx, last_lev,
				cmb_status->cur_level, Rd(DI_MTN_1_CTRL1));
		last_lev = cmb_status->cur_level;
	}

	if ((force_lev > 5) && (glb_mot[1] != glb_mot[0])) {
		dlvl = adp_set_level(glb_mot[0], field_diff);
		diff = glb_mot[0];
		pre_dat[0] = Rd(DI_MTN_1_CTRL3);
		if (prt_flg)
			pr_info("%s dlevel=%d.\n", __func__, dlvl);
		wt_dat = adp_set_mtn_ctrl3(diff, dlvl);
		if (pre_dat[0] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL3, wt_dat);
			pre_dat[0] = wt_dat;
			if (prt_flg)
				pr_info("set mtn03 0x%08x.\n", wt_dat);
		}

		pre_dat[1] = Rd(DI_MTN_1_CTRL4);
		wt_dat = adp_set_mtn_ctrl4(diff, dlvl, cmb_param.height,
				cmb_status->cmb_row_num);
		if (pre_dat[1] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL4, wt_dat);
			if (prt_flg)
				pr_info("set mtn04 %08x -> %08x.\n",
				pre_dat[1], wt_dat);
			pre_dat[1] = wt_dat;
		}

		pre_dat[2] = Rd(DI_MTN_1_CTRL7);
		wt_dat = adp_set_mtn_ctrl7(diff, dlvl);
		if (pre_dat[2] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL7, wt_dat);
			pre_dat[2] = wt_dat;
			if (prt_flg)
				pr_info("set mtn07 0x%08x.\n", wt_dat);
		}

		pre_dat[3] = Rd(DI_MTN_1_CTRL10);
		wt_dat = adp_set_mtn_ctrl10(diff, dlvl,
				cmb_status->frame_diff_avg);
		if (pre_dat[3] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL10, wt_dat);
			pre_dat[3] = wt_dat;
			if (prt_flg)
				pr_info("set mtn10 0x%08x.\n", wt_dat);
		}

		pre_dat[4] = Rd(DI_MTN_1_CTRL11);
		wt_dat = adp_set_mtn_ctrl11(diff, dlvl);
		if (pre_dat[4] != wt_dat) {
			DI_Wr(DI_MTN_1_CTRL11, wt_dat);
			pre_dat[4] = wt_dat;
			if (prt_flg)
				pr_info("set mtn11 0x%08x.\n\n", wt_dat);
		}
	}
	cmb_param.field_idx++;
	return 0;
}

#ifdef DI_NEW_PQ_V1
void adaptive_combing_new(unsigned int field_diff,
	unsigned int frame_diff)
{
	static int cont_change;
	static int glb_mot_cont;

	static int glb_mot[5] = {0, 0, 0, 0, 0};

	glb_mot[1] = glb_mot[0];
	glb_mot[0] = frame_diff;

	if (glb_mot[1] - glb_mot[0] > glb_mot[0] * 10 &&
		glb_mot[1] > roku_th_test){
		cont_change = 1;
		if (combing_debug_print)
			pr_info("A mot[0]=0x%x,glb_mot[1]=0x%x,change=0x%x\n\n",
				glb_mot[0], glb_mot[1], cont_change);
	}
	if (cont_change > 0) {
		cont_change = cont_change + 1;
		if (combing_debug_print)
			pr_info("B mot[0]=0x%x,glb_mot[1]=0x%x,change=0x%x\n\n",
				glb_mot[0], glb_mot[1], cont_change);
	}
	if (cont_change > change_th) {
		cont_change = 0;
		if (combing_debug_print)
			pr_info("c mot[0]=0x%x,glb_mot[1]=0x%x,change=0x%x\n\n",
				glb_mot[0], glb_mot[1], cont_change);
	}
	if (glb_mot[0] > th) {
		glb_mot_cont  = MIN(20, glb_mot_cont  + 1);
		if (combing_debug_print)
			pr_info("d glb_mot[0]=0x%x,glb_mot_cont=0x%x\n\n",
				glb_mot[0], glb_mot_cont);

	} else {
		if (combing_debug_print)
			pr_info("e glb_mot[0]=0x%x,glb_mot_cont=0x%x\n\n",
				glb_mot[0], glb_mot_cont);
		glb_mot_cont = 0;
	}
	if ((glb_mot_cont < th_cont || glb_mot[0] < th) || cont_change > 0) {
		DI_Wr(0x1741, 0x1A1A3A62);
		DI_Wr(0x1742, 0x15200101);
		DI_Wr(0x1743, 0x01200440);
		DI_Wr(0x1744, 0x74200D0D);
		DI_Wr(0x17ad, 0x02020606);
		DI_Wr(0x17ae, 0x05080304);
		DI_Wr(0x17a9, 0x0D5A1520);
		DI_Wr(0x17aa, 0x0A0A0201);
		DI_Wr(0x17ab, 0x1A1A2662);
		DI_Wr(0x17ac, 0x0D200302);
		DI_Wr(0x17af, 0x40020a04);
		//pr_info("F TEST\n");
	} else {
		DI_Wr(0x1741, 0x0A0A1A22);
		DI_Wr(0x1742, 0x0a100101);
		DI_Wr(0x1743, 0x01020420);
		DI_Wr(0x1744, 0x32210404);
		DI_Wr(0x17ad, 0x04040606);
		DI_Wr(0x17ae, 0x02030202);
		DI_Wr(0x17a9, 0x0a100505);
		DI_Wr(0x17aa, 0x04040101);
		DI_Wr(0x17ab, 0x0a0a0a0a);
		DI_Wr(0x17ac, 0x0f100101);
		DI_Wr(0x17af, 0x60020a60);
		//pr_info("G TEST\n");
	}

	if (combing_debug_print) {
		pr_info("f cont=0x%x,change=0x%x\n\n",
			glb_mot_cont, cont_change);
		pr_info("0x17ad=0x%x,0x17ae=0x%x\n\n",
			Rd(DI_MTN_1_CTRL10), Rd(DI_MTN_1_CTRL11));
	}
}
#endif

#ifdef DEBUG_SUPPORT
module_param_named(cmb_adpset_cnt, cmb_adpset_cnt, int, 0644);
#endif
static const struct mtn_op_s di_ops_mtn = {
	.mtn_int_combing_glbmot		= mtn_int_combing_glbmot,
	.adpative_combing_exit		= adpative_combing_exit,
	.fix_tl1_1080i_patch_sel	= fix_tl1_1080i_patch_sel,
	.fix_s1a_576i_patch_sel		= fix_s1a_576i_patch_sel,
	.adaptive_combing_fixing	= adaptive_combing_fixing,
#ifdef DI_NEW_PQ_V1
	.adaptive_combing_new		= adaptive_combing_new,
#endif
	.adpative_combing_config	= adpative_combing_config,
	.com_patch_pre_sw_set		= com_patch_pre_sw_set,
	/*.module_para			= dim_seq_file_module_para_mtn,*/
};

bool di_attach_ops_mtn(const struct mtn_op_s **ops)
{
	#if 0
	if (!ops)
		return false;

	memcpy(ops, &di_pd_ops, sizeof(struct pulldown_op_s));
	#else
	*ops = &di_ops_mtn;
	#endif

	return true;
}
EXPORT_SYMBOL(di_attach_ops_mtn);
