// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include "../arch/vpp_regs_v2.h"
#include "../reg_helper.h"
#include "ai_color.h"
#include "../amcsc.h"

unsigned int ai_clr_dbg;
module_param(ai_clr_dbg, uint, 0664);
MODULE_PARM_DESC(ai_clr_dbg, "\n ai color dbg\n");

int aice_offset[4] = {
	0x0, 0x100, 0x900, 0xa00
};

#define pr_ai_clr(fmt, args...)\
	do {\
		if (ai_clr_dbg)\
			pr_info("ai_color: " fmt, ## args);\
	} while (0)\

#define AI_COLOR_VER "ai_color ver: 2023-04-07\n"

int s_gain_lut[120] = {0};
int l_gain_lut[64] = {
	112, 112, 110, 108, 106, 105, 103, 101,
	99, 97, 96, 94, 92, 90, 88, 87,
	85, 83, 81, 79, 78, 76, 74, 72,
	70, 69, 67, 65, 63, 61, 60, 58,
	56, 54, 52, 51, 49, 47, 45, 43,
	41, 40, 38, 36, 34, 32, 31, 29,
	27, 25, 23, 22, 20, 18, 16, 14,
	13, 11, 9, 7, 5, 4, 2, 0
};

struct sa_adj_param_s sa_adj_parm = {
	.reg_s_gain_lut = s_gain_lut,
	.reg_l_gain_lut = l_gain_lut,
	.reg_sat_s_gain_en = 1,
	.reg_sat_l_gain_en = 1,
	.reg_sat_adj_a = 818,
	.reg_sat_prt = 1024,
	.reg_sat_prt_p = 900,
	.reg_sat_prt_th = 8456
};

struct sa_fw_param_s sa_fw_parm = {
	.reg_zero = 191,
	.reg_sat_adj = 128,
	.reg_sat_shift = 0,

	.reg_skin_th = 0,
	.reg_skin_shift = 0,
	.reg_skin_adj = 128,

	.reg_hue_left1 = 99,
	.reg_hue_right1 = 1,
	.reg_hue_adj1 = 512,
	.reg_hue_shift1 = 0,

	.reg_hue_left2 = 40,
	.reg_hue_right2 = 45,
	.reg_hue_adj2 = 512,
	.reg_hue_shift2 = 0,

	.reg_hue_left3 = 80,
	.reg_hue_right3 = 85,
	.reg_hue_adj3 = 512,
	.reg_hue_shift3 = 0
};

struct sa_param_s sa_parm = {
	.sa_adj_param = &sa_adj_parm,
	.sa_fw_param = &sa_fw_parm
};

void hue_adj(int *sg_lut, int *hueleft,
	int *hueright, int *hueadj, int *hueshift)
{
	int hleft = *hueleft;
	int hright = *hueright;
	int hadj = *hueadj;
	int hshift = *hueshift;
	int hleft1;
	int hright1;
	int num = 0;
	int k;
	int param;
	int sf = 0;

	if (hleft > hright && hleft > 100 && hright < 20)
		hright = hright + 120;

	num = hright - hleft;

	if (num < 0)
		return;

	if (num == 0) {
		sg_lut[hleft] = hadj * sg_lut[hleft] >> 9;
	} else if (num == 1) {
		hright = hright > 119 ? hright - 120 : hright;
		sg_lut[hleft] = hadj * sg_lut[hleft] >> 9;
		sg_lut[hright] = hadj * sg_lut[hright] >> 9;
	} else if (num == 2) {
		hright = hright > 119 ? hright - 120 : hright;
		hleft1 = hleft + 1;
		hleft1 = hleft1 > 119 ? hleft1 - 120 : hleft1;

		sg_lut[hleft] = hadj * sg_lut[hleft] >> 10;
		sg_lut[hleft1] = hadj * sg_lut[hleft1] >> 9;
		sg_lut[hright] = hadj * sg_lut[hright] >> 10;
	} else {
		hleft1  = hleft + (((num * 1 << 8)  / 3 + 128) >> 8);
		hright1 = hleft + (((num * 2 << 8) / 3 + 128) >> 8);

		for (k = hleft; k < hleft1; k++) {
			param =  (((hadj - 512) * ((k - hleft + 1) << 10) /
				(hleft1 - hleft + 1) + 512) >> 10) + 512;
			sf = k > 119 ? k - 120 : k;
			sg_lut[sf] = sg_lut[sf] + hshift;
			sg_lut[sf] = param * sg_lut[sf] >> 9;
		}

		for (k = hleft1; k < hright1; k++) {
			sf = k > 119 ? k - 120 : k;
			sg_lut[sf] = sg_lut[sf] + hshift;
			sg_lut[sf] = hadj * sg_lut[sf] >> 9;
		}

		for (k = hright1; k <= hright; k++) {
			param = (((hadj - 512) * ((hright + 1 - k) << 10) /
				(hright + 1 - hright1) + 512) >> 10) + 512;
			sf = k > 119 ? k - 120 : k;
			sg_lut[sf] = sg_lut[sf] + hshift;
			sg_lut[sf] = param * sg_lut[sf] >> 9;
		}
	}
	pr_ai_clr("%s: done\n", __func__);
}

void SLut_gen(struct sa_adj_param_s *reg_sat,
	struct sa_fw_param_s *reg_fw_sat)
{
	int k = 0;
	int huesum = 0;
	int ki = 0;
	int kx = 0;
//	int skin = 0;
	int post = 0;
	int maxgain;
	int mingain;
//	int Ratio;
	int midslut[120] = {0};
//	int nhuenum[120] = {0};
	int slope_l;
	int sloper;
	int skl;
	int skr;

	//double against[120] = {0};

	reg_sat->reg_sat_prt = reg_sat->reg_sat_prt > 512 ? reg_sat->reg_sat_prt : 512;
	reg_sat->reg_sat_prt_p = (1 << 20) / (1024 - reg_sat->reg_sat_prt);

	for (k = 0; k < 120; k++) {
		reg_sat->reg_s_gain_lut[k] =
			(reg_sat->reg_s_gain_lut[k] - reg_fw_sat->reg_zero) << 2;
		midslut[k] = reg_sat->reg_s_gain_lut[k];
		midslut[k] = (midslut[k] + reg_fw_sat->reg_sat_shift) *
			reg_fw_sat->reg_sat_adj >> 7;
		if (midslut[k] > 0)
			post++;
	}

	if (post != 0) {
		//hsy 20 - 60   hsl 110 - 140
		for (k = 20; k <= 60; k++) {
			//skin = k > 119 ? k - 120 : k;
			if (midslut[k] > reg_fw_sat->reg_skin_th) {
				midslut[k] = midslut[k] + reg_fw_sat->reg_skin_shift;
				midslut[k] = (midslut[k] * reg_fw_sat->reg_skin_adj) >> 7;
			}
		}
	}

	hue_adj(midslut, &reg_fw_sat->reg_hue_left1, &reg_fw_sat->reg_hue_right1,
		&reg_fw_sat->reg_hue_adj1, &reg_fw_sat->reg_hue_shift1);
	hue_adj(midslut, &reg_fw_sat->reg_hue_left2, &reg_fw_sat->reg_hue_right2,
		&reg_fw_sat->reg_hue_adj2, &reg_fw_sat->reg_hue_shift2);
	hue_adj(midslut, &reg_fw_sat->reg_hue_left3, &reg_fw_sat->reg_hue_right3,
		&reg_fw_sat->reg_hue_adj3, &reg_fw_sat->reg_hue_shift3);

	for (k = 0; k < 120; k++) {
		huesum = 0;

		if (midslut[k] >= 0) {
			maxgain = -512;
			mingain = 512;
			for (ki = 0; ki < 5; ki++) {
				kx = k + ki - 2;
				if (kx < 0)
					kx = kx + 120;

				if (kx > 119)
					kx = kx - 120;

				huesum = huesum + midslut[kx];
			}
			reg_sat->reg_s_gain_lut[k] = (huesum * 205) >> 10;
		} else {
			for (ki = 0; ki < 9; ki++) {
				kx = k + ki - 4;
				if (kx < 0)
					kx = kx + 120;
				if (kx > 119)
					kx = kx - 120;
				huesum = huesum + midslut[kx];
			}

			reg_sat->reg_s_gain_lut[k] = (huesum * 114) >> 10;
		}
	}

	for (k = 0; k < 120; k++) {
		skl = k == 0 ? 119 : k - 1;
		skr = k == 119 ? 0 : k + 1;
		slope_l = reg_sat->reg_s_gain_lut[k] - reg_sat->reg_s_gain_lut[skl];
		sloper = reg_sat->reg_s_gain_lut[skr] - reg_sat->reg_s_gain_lut[k];

		if (reg_sat->reg_s_gain_lut[k] < 0) {
			if ((slope_l >= 0 && sloper < 0) || (slope_l <= 0 && sloper > 0))
				reg_sat->reg_s_gain_lut[k] = reg_sat->reg_s_gain_lut[skl];
		}
		reg_sat->reg_s_gain_lut[k] =
			reg_sat->reg_s_gain_lut[k] > 2047 ? 2047 : reg_sat->reg_s_gain_lut[k];
		reg_sat->reg_s_gain_lut[k] =
			reg_sat->reg_s_gain_lut[k] < -2048 ? -2048 : reg_sat->reg_s_gain_lut[k];
	}
	pr_ai_clr("%s: done\n", __func__);
}

void ai_color_cfg(struct sa_adj_param_s *sa_adj_param)
{
	int s_gain_en;
	int l_gain_en;
	int en;
	int *s_gain;
	int *l_gain;
	int i;

	s_gain_en = sa_adj_param->reg_sat_s_gain_en;
	l_gain_en = sa_adj_param->reg_sat_l_gain_en;

	en = s_gain_en | l_gain_en;
	VSYNC_WR_MPEG_REG_BITS(SA_CTRL, en, 0, 1);
	if (!en)
		return;
	VSYNC_WR_MPEG_REG_BITS(SA_ADJ, (s_gain_en << 1) | l_gain_en, 27, 2);

	s_gain = sa_adj_param->reg_s_gain_lut;
	l_gain = sa_adj_param->reg_l_gain_lut;

	if (s_gain_en) {
		for (i = 0; i < 60; i++)
			VSYNC_WR_MPEG_REG(SA_S_GAIN_0 + i,
				((s_gain[2 * i] & 0xfff) << 16) | (s_gain[2 * i + 1] & 0xfff));
	}

	if (l_gain_en) {
		for (i = 0; i < 32; i++)
			VSYNC_WR_MPEG_REG(SA_L_GAIN_0 + i,
				((l_gain[2 * i] & 0x3ff) << 16) | (l_gain[2 * i + 1] & 0x3ff));
	}
	pr_ai_clr("%s: done\n", __func__);
}

void ai_color_proc(struct vframe_s *vf)
{
	int i;

	if (!vf || !vf->vc_private)
		return;

	if (sa_adj_parm.reg_sat_s_gain_en == 0 &&
		sa_adj_parm.reg_sat_l_gain_en == 0)
		return;

	if (!vf->vc_private->aicolor_info) {
		if (ai_clr_dbg) {
			pr_info("no aicolor_info\n");
			ai_clr_dbg--;
		}
		return;
	}

	for (i = 0; i < 120; i++)
		sa_adj_parm.reg_s_gain_lut[i] =
			(int)vf->vc_private->aicolor_info->color_value[i];

	if (ai_clr_dbg > 0) {
		for (i = 0; i < 120; i++)
			pr_info("input: reg_s_gain_lut[%d] = %d\n", i,
				sa_adj_parm.reg_s_gain_lut[i]);
	}

	SLut_gen(&sa_adj_parm, &sa_fw_parm);
	ai_color_cfg(&sa_adj_parm);

	if (ai_clr_dbg > 0) {
		for (i = 0; i < 120; i++)
			pr_info("output-> reg_s_gain_lut[%d] = %d\n", i,
				sa_adj_parm.reg_s_gain_lut[i]);
		ai_clr_dbg--;
	}
}

void ai_clr_config(int enable)
{
	int i;
	int s5_slice_mode = get_s5_silce_mode();

	if (s5_slice_mode < 1 || s5_slice_mode > 4)
		return;

	for (i = 0; i < s5_slice_mode; i++)
		WRITE_VPP_REG_BITS_S5(SA_CTRL + aice_offset[i], enable, 0, 1);
}

int ai_color_debug_store(char **parm)
{
	long val;

	if (!strcmp(parm[0], "ai_color_ver")) {
		pr_info(AI_COLOR_VER);
	} else if (!strcmp(parm[0], "sat_s_gain_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_adj_parm.reg_sat_s_gain_en = (uint)val;
		pr_info("sat_s_gain_en = %d\n", sa_adj_parm.reg_sat_s_gain_en);
	} else if (!strcmp(parm[0], "sat_l_gain_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_adj_parm.reg_sat_l_gain_en = (uint)val;
		pr_info("sat_l_gain_en = %d\n", sa_adj_parm.reg_sat_l_gain_en);
	} else if (!strcmp(parm[0], "reg_sat_adj_a")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_adj_parm.reg_sat_adj_a = (uint)val;
		pr_info("reg_sat_adj_a = %d\n", sa_adj_parm.reg_sat_adj_a);
	} else if (!strcmp(parm[0], "reg_sat_prt")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_adj_parm.reg_sat_prt = (uint)val;
		pr_info("reg_sat_prt = %d\n", sa_adj_parm.reg_sat_prt);
	} else if (!strcmp(parm[0], "reg_sat_prt_p")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_adj_parm.reg_sat_prt_p = (uint)val;
		pr_info("reg_sat_prt_p = %d\n", sa_adj_parm.reg_sat_prt_p);
	} else if (!strcmp(parm[0], "reg_sat_prt_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_adj_parm.reg_sat_prt_th = (uint)val;
		pr_info("reg_sat_prt_th = %d\n", sa_adj_parm.reg_sat_prt_th);
	} else if (!strcmp(parm[0], "reg_zero")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_zero = (uint)val;
		pr_info("reg_zero = %d\n", sa_fw_parm.reg_zero);
	} else if (!strcmp(parm[0], "reg_sat_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_sat_adj = (uint)val;
		pr_info("reg_sat_adj = %d\n", sa_fw_parm.reg_sat_adj);
	} else if (!strcmp(parm[0], "reg_sat_shift")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_sat_shift = (uint)val;
		pr_info("reg_sat_shift = %d\n", sa_fw_parm.reg_sat_shift);
	} else if (!strcmp(parm[0], "reg_skin_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_skin_th = (uint)val;
		pr_info("reg_skin_th = %d\n", sa_fw_parm.reg_skin_th);
	} else if (!strcmp(parm[0], "reg_skin_shift")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_skin_shift = (uint)val;
		pr_info("reg_skin_shift = %d\n", sa_fw_parm.reg_skin_shift);
	} else if (!strcmp(parm[0], "reg_skin_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_skin_adj = (uint)val;
		pr_info("reg_skin_adj = %d\n", sa_fw_parm.reg_skin_adj);
	} else if (!strcmp(parm[0], "reg_hue_left1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_left1 = (uint)val;
		pr_info("reg_hue_left1 = %d\n", sa_fw_parm.reg_hue_left1);
	} else if (!strcmp(parm[0], "reg_hue_right1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_right1 = (uint)val;
		pr_info("reg_hue_right1 = %d\n", sa_fw_parm.reg_hue_right1);
	} else if (!strcmp(parm[0], "reg_hue_adj1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_adj1 = (uint)val;
		pr_info("reg_hue_adj1 = %d\n", sa_fw_parm.reg_hue_adj1);
	} else if (!strcmp(parm[0], "reg_hue_shift1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_shift1 = (uint)val;
		pr_info("reg_hue_shift1 = %d\n", sa_fw_parm.reg_hue_shift1);
	} else if (!strcmp(parm[0], "reg_hue_left2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_left2 = (uint)val;
		pr_info("reg_hue_left2 = %d\n", sa_fw_parm.reg_hue_left2);
	} else if (!strcmp(parm[0], "reg_hue_right2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_right2 = (uint)val;
		pr_info("reg_hue_right2 = %d\n", sa_fw_parm.reg_hue_right2);
	} else if (!strcmp(parm[0], "reg_hue_adj2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_adj2 = (uint)val;
		pr_info("reg_hue_adj2 = %d\n", sa_fw_parm.reg_hue_adj2);
	} else if (!strcmp(parm[0], "reg_hue_shift2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_shift2 = (uint)val;
		pr_info("reg_hue_shift2 = %d\n", sa_fw_parm.reg_hue_shift2);
	} else if (!strcmp(parm[0], "reg_hue_left3")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_left3 = (uint)val;
		pr_info("reg_hue_left3 = %d\n", sa_fw_parm.reg_hue_left3);
	} else if (!strcmp(parm[0], "reg_hue_right3")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_right3 = (uint)val;
		pr_info("reg_hue_right3 = %d\n", sa_fw_parm.reg_hue_right3);
	} else if (!strcmp(parm[0], "reg_hue_adj3")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_adj3 = (uint)val;
		pr_info("reg_hue_adj3 = %d\n", sa_fw_parm.reg_hue_adj3);
	} else if (!strcmp(parm[0], "reg_hue_shift3")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_fw_parm.reg_hue_shift3 = (uint)val;
		pr_info("reg_hue_shift3 = %d\n", sa_fw_parm.reg_hue_shift3);
	} else if (!strcmp(parm[0], "read_all")) {
		pr_info("sat_s_gain_en = %d\n", sa_adj_parm.reg_sat_s_gain_en);
		pr_info("sat_l_gain_en = %d\n", sa_adj_parm.reg_sat_l_gain_en);
		pr_info("reg_sat_adj_a = %d\n", sa_adj_parm.reg_sat_adj_a);
		pr_info("reg_sat_prt = %d\n", sa_adj_parm.reg_sat_prt);
		pr_info("reg_sat_prt_p = %d\n", sa_adj_parm.reg_sat_prt_p);
		pr_info("reg_sat_prt_th = %d\n", sa_adj_parm.reg_sat_prt_th);
		pr_info("\n");
		pr_info("reg_zero = %d\n", sa_fw_parm.reg_zero);
		pr_info("reg_sat_adj = %d\n", sa_fw_parm.reg_sat_adj);
		pr_info("reg_sat_shift = %d\n", sa_fw_parm.reg_sat_shift);
		pr_info("reg_skin_th = %d\n", sa_fw_parm.reg_skin_th);
		pr_info("reg_skin_shift = %d\n", sa_fw_parm.reg_skin_shift);
		pr_info("reg_skin_adj = %d\n", sa_fw_parm.reg_skin_adj);
		pr_info("reg_hue_left1 = %d\n", sa_fw_parm.reg_hue_left1);
		pr_info("reg_hue_right1 = %d\n", sa_fw_parm.reg_hue_right1);
		pr_info("reg_hue_adj1 = %d\n", sa_fw_parm.reg_hue_adj1);
		pr_info("reg_hue_shift1 = %d\n", sa_fw_parm.reg_hue_shift1);
		pr_info("reg_hue_left2 = %d\n", sa_fw_parm.reg_hue_left2);
		pr_info("reg_hue_right2 = %d\n", sa_fw_parm.reg_hue_right2);
		pr_info("reg_hue_adj2 = %d\n", sa_fw_parm.reg_hue_adj2);
		pr_info("reg_hue_shift2 = %d\n", sa_fw_parm.reg_hue_shift2);
		pr_info("reg_hue_left3 = %d\n", sa_fw_parm.reg_hue_left3);
		pr_info("reg_hue_right3 = %d\n", sa_fw_parm.reg_hue_right3);
		pr_info("reg_hue_adj3 = %d\n", sa_fw_parm.reg_hue_adj3);
		pr_info("reg_hue_shift3 = %d\n", sa_fw_parm.reg_hue_shift3);
	}

	return 0;
}
