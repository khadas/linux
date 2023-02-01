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

unsigned int ai_clr_dbg;
module_param(ai_clr_dbg, uint, 0664);
MODULE_PARM_DESC(ai_clr_dbg, "\n ai color dbg\n");

#define pr_ai_clr(fmt, args...)\
	do {\
		if (ai_clr_dbg)\
			pr_info("ai_color: " fmt, ## args);\
	} while (0)\

#define AI_COLOR_VER "ai_color ver: 2022-08-31\n"
unsigned int slice_reg_ofst[4] = {
	0x0, 0x100, 0x900, 0xa00
};

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
	.reg_sat_s_gain_en = 0,
	.reg_sat_l_gain_en = 0,
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

	s_gain_en = sa_adj_param->reg_sat_s_gain_en;
	l_gain_en = sa_adj_param->reg_sat_l_gain_en;

	en = s_gain_en | l_gain_en;
	VSYNC_WRITE_VPP_REG_BITS(SA_CTRL, en, 0, 1);
	if (!en)
		return;
	VSYNC_WRITE_VPP_REG_BITS(SA_ADJ, (s_gain_en << 1) | l_gain_en, 27, 2);

	s_gain = sa_adj_param->reg_s_gain_lut;
	l_gain = sa_adj_param->reg_l_gain_lut;

	if (s_gain_en) {
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_0, (s_gain[0] << 16) | s_gain[1]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_1, (s_gain[2] << 16) | s_gain[3]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_2, (s_gain[4] << 16) | s_gain[5]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_3, (s_gain[6] << 16) | s_gain[7]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_4, (s_gain[8] << 16) | s_gain[9]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_5, (s_gain[10] << 16) | s_gain[11]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_6, (s_gain[12] << 16) | s_gain[13]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_7, (s_gain[14] << 16) | s_gain[15]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_8, (s_gain[16] << 16) | s_gain[17]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_9, (s_gain[18] << 16) | s_gain[19]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_10, (s_gain[20] << 16) | s_gain[21]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_11, (s_gain[22] << 16) | s_gain[23]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_12, (s_gain[24] << 16) | s_gain[25]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_13, (s_gain[26] << 16) | s_gain[27]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_14, (s_gain[28] << 16) | s_gain[29]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_15, (s_gain[30] << 16) | s_gain[31]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_16, (s_gain[32] << 16) | s_gain[33]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_17, (s_gain[34] << 16) | s_gain[35]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_18, (s_gain[36] << 16) | s_gain[37]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_19, (s_gain[38] << 16) | s_gain[39]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_20, (s_gain[40] << 16) | s_gain[41]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_21, (s_gain[42] << 16) | s_gain[43]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_22, (s_gain[44] << 16) | s_gain[45]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_23, (s_gain[46] << 16) | s_gain[47]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_24, (s_gain[48] << 16) | s_gain[49]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_25, (s_gain[50] << 16) | s_gain[51]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_26, (s_gain[52] << 16) | s_gain[53]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_27, (s_gain[54] << 16) | s_gain[55]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_28, (s_gain[56] << 16) | s_gain[57]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_29, (s_gain[58] << 16) | s_gain[59]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_30, (s_gain[60] << 16) | s_gain[61]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_31, (s_gain[62] << 16) | s_gain[63]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_32, (s_gain[64] << 16) | s_gain[65]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_33, (s_gain[66] << 16) | s_gain[67]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_34, (s_gain[68] << 16) | s_gain[69]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_35, (s_gain[70] << 16) | s_gain[71]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_36, (s_gain[72] << 16) | s_gain[73]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_37, (s_gain[74] << 16) | s_gain[75]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_38, (s_gain[76] << 16) | s_gain[77]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_39, (s_gain[78] << 16) | s_gain[79]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_40, (s_gain[80] << 16) | s_gain[81]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_41, (s_gain[82] << 16) | s_gain[83]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_42, (s_gain[84] << 16) | s_gain[85]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_43, (s_gain[86] << 16) | s_gain[87]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_44, (s_gain[88] << 16) | s_gain[89]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_45, (s_gain[90] << 16) | s_gain[91]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_46, (s_gain[92] << 16) | s_gain[93]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_47, (s_gain[94] << 16) | s_gain[95]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_48, (s_gain[96] << 16) | s_gain[97]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_49, (s_gain[98] << 16) | s_gain[99]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_50, (s_gain[100] << 16) | s_gain[101]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_51, (s_gain[102] << 16) | s_gain[103]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_52, (s_gain[104] << 16) | s_gain[105]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_53, (s_gain[106] << 16) | s_gain[107]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_54, (s_gain[108] << 16) | s_gain[109]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_55, (s_gain[110] << 16) | s_gain[111]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_56, (s_gain[112] << 16) | s_gain[113]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_57, (s_gain[114] << 16) | s_gain[115]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_58, (s_gain[116] << 16) | s_gain[117]);
		VSYNC_WRITE_VPP_REG(SA_S_GAIN_59, (s_gain[118] << 16) | s_gain[119]);
	}

	if (l_gain_en) {
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_0, (l_gain[0] << 16 | l_gain[1]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_1, (l_gain[2] << 16 | l_gain[3]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_2, (l_gain[4] << 16 | l_gain[5]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_3, (l_gain[6] << 16 | l_gain[7]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_4, (l_gain[8] << 16 | l_gain[9]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_5, (l_gain[10] << 16 | l_gain[11]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_6, (l_gain[12] << 16 | l_gain[13]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_7, (l_gain[14] << 16 | l_gain[15]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_8, (l_gain[16] << 16 | l_gain[17]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_9, (l_gain[18] << 16 | l_gain[19]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_10, (l_gain[20] << 16 | l_gain[21]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_11, (l_gain[22] << 16 | l_gain[23]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_12, (l_gain[24] << 16 | l_gain[25]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_13, (l_gain[26] << 16 | l_gain[27]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_14, (l_gain[28] << 16 | l_gain[29]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_15, (l_gain[30] << 16 | l_gain[31]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_16, (l_gain[32] << 16 | l_gain[33]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_17, (l_gain[34] << 16 | l_gain[35]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_18, (l_gain[36] << 16 | l_gain[37]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_19, (l_gain[38] << 16 | l_gain[39]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_20, (l_gain[40] << 16 | l_gain[41]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_21, (l_gain[42] << 16 | l_gain[43]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_22, (l_gain[44] << 16 | l_gain[45]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_23, (l_gain[46] << 16 | l_gain[47]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_24, (l_gain[48] << 16 | l_gain[49]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_25, (l_gain[50] << 16 | l_gain[51]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_26, (l_gain[52] << 16  | l_gain[53]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_27, (l_gain[54] << 16 | l_gain[55]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_28, (l_gain[56] << 16 | l_gain[57]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_29, (l_gain[58] << 16 | l_gain[59]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_30, (l_gain[60] << 16 | l_gain[61]));
		VSYNC_WRITE_VPP_REG(SA_L_GAIN_31, (l_gain[62] << 16 | l_gain[63]));
	}
	pr_ai_clr("%s: done\n", __func__);
}

void ai_color_proc(struct vframe_s *vf)
{
	if (sa_adj_parm.reg_sat_s_gain_en == 0 &&
		sa_adj_parm.reg_sat_l_gain_en == 0)
		return;

	SLut_gen(&sa_adj_parm, &sa_fw_parm);
	ai_color_cfg(&sa_adj_parm);

	if (ai_clr_dbg > 0)
		ai_clr_dbg--;
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
	}

	return 0;
}
