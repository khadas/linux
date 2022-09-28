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

unsigned int aiclr_dbg;
module_param(aiclr_dbg, uint, 0664);
MODULE_PARM_DESC(aiclr_dbg, "\n ai color dbg\n");

#define pr_aiclr(fmt, args...)\
	do {\
		if (aiclr_dbg)\
			pr_info("ai_color: " fmt, ## args);\
	} while (0)\

#define AI_COLOR_VER "aicolor ver: 2022-08-31\n"
unsigned int slice_reg_ofst[4] = {
	0x0, 0x100, 0x900, 0xa00
};

int sgain_lut[120] = {0};
int lgain_lut[64] = {
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
	.reg_sgain_lut = sgain_lut,
	.reg_lgain_lut = lgain_lut,
	.reg_sat_sgain_en = 0,
	.reg_sat_lgain_en = 0,
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

void hue_adj(int *sglut, int *hueleft,
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
		sglut[hleft] = hadj * sglut[hleft] >> 9;
	} else if (num == 1) {
		hright = hright > 119 ? hright - 120 : hright;
		sglut[hleft] = hadj * sglut[hleft] >> 9;
		sglut[hright] = hadj * sglut[hright] >> 9;
	} else if (num == 2) {
		hright = hright > 119 ? hright - 120 : hright;
		hleft1 = hleft + 1;
		hleft1 = hleft1 > 119 ? hleft1 - 120 : hleft1;

		sglut[hleft] = hadj * sglut[hleft] >> 10;
		sglut[hleft1] = hadj * sglut[hleft1] >> 9;
		sglut[hright] = hadj * sglut[hright] >> 10;
	} else {
		hleft1  = hleft + (((num * 1 << 8)  / 3 + 128) >> 8);
		hright1 = hleft + (((num * 2 << 8) / 3 + 128) >> 8);

		for (k = hleft; k < hleft1; k++) {
			param =  (((hadj - 512) * ((k - hleft + 1) << 10) /
				(hleft1 - hleft + 1) + 512) >> 10) + 512;
			sf = k > 119 ? k - 120 : k;
			sglut[sf] = sglut[sf] + hshift;
			sglut[sf] = param * sglut[sf] >> 9;
		}

		for (k = hleft1; k < hright1; k++) {
			sf = k > 119 ? k - 120 : k;
			sglut[sf] = sglut[sf] + hshift;
			sglut[sf] = hadj * sglut[sf] >> 9;
		}

		for (k = hright1; k <= hright; k++) {
			param = (((hadj - 512) * ((hright + 1 - k) << 10) /
				(hright + 1 - hright1) + 512) >> 10) + 512;
			sf = k > 119 ? k - 120 : k;
			sglut[sf] = sglut[sf] + hshift;
			sglut[sf] = param * sglut[sf] >> 9;
		}
	}
	pr_aiclr("%s: done\n", __func__);
}

void SLut_gen(struct sa_adj_param_s *reg_sat,
	struct sa_fw_param_s *reg_fw_sat)
{
	int k = 0;
	int huesum = 0;
	int ki = 0;
	int kx = 0;
	int skinf = 0;
	int posf = 0;
	int maxgain;
	int mingain;
//	int Ratio;
	int midslut[120] = {0};
//	int nhuenum[120] = {0};
	int slopel;
	int sloper;
	int skl;
	int skr;

	//double against[120] = {0};

	reg_sat->reg_sat_prt = reg_sat->reg_sat_prt > 512 ? reg_sat->reg_sat_prt : 512;
	reg_sat->reg_sat_prt_p = (1 << 20) / (1024 - reg_sat->reg_sat_prt);

	for (k = 0; k < 120; k++) {
		reg_sat->reg_sgain_lut[k] =
			(reg_sat->reg_sgain_lut[k] - reg_fw_sat->reg_zero) << 2;
		midslut[k] = reg_sat->reg_sgain_lut[k];
		midslut[k] = (midslut[k] + reg_fw_sat->reg_sat_shift) *
			reg_fw_sat->reg_sat_adj >> 7;
		if (midslut[k] > 0)
			posf++;
	}

	if (posf != 0) {
		//hsy 20 - 60   hsl 110 - 140
		for (k = 20; k <= 60; k++) {
			skinf = k > 119 ? k - 120 : k;
			if (midslut[skinf] > reg_fw_sat->reg_skin_th) {
				midslut[skinf] = midslut[skinf] + reg_fw_sat->reg_skin_shift;
				midslut[skinf] = (midslut[skinf] * reg_fw_sat->reg_skin_adj) >> 7;
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
			reg_sat->reg_sgain_lut[k] = (huesum * 205) >> 10;
		} else {
			for (ki = 0; ki < 9; ki++) {
				kx = k + ki - 4;
				if (kx < 0)
					kx = kx + 120;
				if (kx > 119)
					kx = kx - 120;
				huesum = huesum + midslut[kx];
			}

			reg_sat->reg_sgain_lut[k] = (huesum * 114) >> 10;
		}
	}

	for (k = 0; k < 120; k++) {
		skl = k == 0 ? 119 : k - 1;
		skr = k == 119 ? 0 : k + 1;
		slopel = reg_sat->reg_sgain_lut[k] - reg_sat->reg_sgain_lut[skl];
		sloper = reg_sat->reg_sgain_lut[skr] - reg_sat->reg_sgain_lut[k];

		if (reg_sat->reg_sgain_lut[k] < 0) {
			if ((slopel >= 0 && sloper < 0) || (slopel <= 0 && sloper > 0))
				reg_sat->reg_sgain_lut[k] = reg_sat->reg_sgain_lut[skl];
		}
		reg_sat->reg_sgain_lut[k] =
			reg_sat->reg_sgain_lut[k] > 2047 ? 2047 : reg_sat->reg_sgain_lut[k];
		reg_sat->reg_sgain_lut[k] =
			reg_sat->reg_sgain_lut[k] < -2048 ? -2048 : reg_sat->reg_sgain_lut[k];
	}
	pr_aiclr("%s: done\n", __func__);
}

void ai_color_cfg(struct sa_adj_param_s *sa_adj_param)
{
	int sgain_en;
	int lgain_en;
	int en;
	int *sgain;
	int *lgain;

	sgain_en = sa_adj_param->reg_sat_sgain_en;
	lgain_en = sa_adj_param->reg_sat_lgain_en;

	en = sgain_en | lgain_en;
	VSYNC_WRITE_VPP_REG_BITS(SA_CTRL, en, 0, 1);
	if (!en)
		return;
	VSYNC_WRITE_VPP_REG_BITS(SA_ADJ, (sgain_en << 1) | lgain_en, 27, 2);

	sgain = sa_adj_param->reg_sgain_lut;
	lgain = sa_adj_param->reg_lgain_lut;

	if (sgain_en) {
		VSYNC_WRITE_VPP_REG(SA_SGAIN_0, (sgain[0] << 16) | sgain[1]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_1, (sgain[2] << 16) | sgain[3]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_2, (sgain[4] << 16) | sgain[5]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_3, (sgain[6] << 16) | sgain[7]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_4, (sgain[8] << 16) | sgain[9]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_5, (sgain[10] << 16) | sgain[11]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_6, (sgain[12] << 16) | sgain[13]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_7, (sgain[14] << 16) | sgain[15]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_8, (sgain[16] << 16) | sgain[17]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_9, (sgain[18] << 16) | sgain[19]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_10, (sgain[20] << 16) | sgain[21]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_11, (sgain[22] << 16) | sgain[23]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_12, (sgain[24] << 16) | sgain[25]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_13, (sgain[26] << 16) | sgain[27]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_14, (sgain[28] << 16) | sgain[29]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_15, (sgain[30] << 16) | sgain[31]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_16, (sgain[32] << 16) | sgain[33]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_17, (sgain[34] << 16) | sgain[35]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_18, (sgain[36] << 16) | sgain[37]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_19, (sgain[38] << 16) | sgain[39]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_20, (sgain[40] << 16) | sgain[41]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_21, (sgain[42] << 16) | sgain[43]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_22, (sgain[44] << 16) | sgain[45]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_23, (sgain[46] << 16) | sgain[47]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_24, (sgain[48] << 16) | sgain[49]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_25, (sgain[50] << 16) | sgain[51]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_26, (sgain[52] << 16) | sgain[53]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_27, (sgain[54] << 16) | sgain[55]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_28, (sgain[56] << 16) | sgain[57]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_29, (sgain[58] << 16) | sgain[59]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_30, (sgain[60] << 16) | sgain[61]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_31, (sgain[62] << 16) | sgain[63]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_32, (sgain[64] << 16) | sgain[65]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_33, (sgain[66] << 16) | sgain[67]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_34, (sgain[68] << 16) | sgain[69]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_35, (sgain[70] << 16) | sgain[71]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_36, (sgain[72] << 16) | sgain[73]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_37, (sgain[74] << 16) | sgain[75]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_38, (sgain[76] << 16) | sgain[77]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_39, (sgain[78] << 16) | sgain[79]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_40, (sgain[80] << 16) | sgain[81]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_41, (sgain[82] << 16) | sgain[83]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_42, (sgain[84] << 16) | sgain[85]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_43, (sgain[86] << 16) | sgain[87]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_44, (sgain[88] << 16) | sgain[89]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_45, (sgain[90] << 16) | sgain[91]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_46, (sgain[92] << 16) | sgain[93]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_47, (sgain[94] << 16) | sgain[95]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_48, (sgain[96] << 16) | sgain[97]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_49, (sgain[98] << 16) | sgain[99]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_50, (sgain[100] << 16) | sgain[101]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_51, (sgain[102] << 16) | sgain[103]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_52, (sgain[104] << 16) | sgain[105]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_53, (sgain[106] << 16) | sgain[107]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_54, (sgain[108] << 16) | sgain[109]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_55, (sgain[110] << 16) | sgain[111]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_56, (sgain[112] << 16) | sgain[113]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_57, (sgain[114] << 16) | sgain[115]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_58, (sgain[116] << 16) | sgain[117]);
		VSYNC_WRITE_VPP_REG(SA_SGAIN_59, (sgain[118] << 16) | sgain[119]);
	}

	if (lgain_en) {
		VSYNC_WRITE_VPP_REG(SA_LGAIN_0, (lgain[0] << 16 | lgain[1]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_1, (lgain[2] << 16 | lgain[3]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_2, (lgain[4] << 16 | lgain[5]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_3, (lgain[6] << 16 | lgain[7]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_4, (lgain[8] << 16 | lgain[9]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_5, (lgain[10] << 16 | lgain[11]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_6, (lgain[12] << 16 | lgain[13]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_7, (lgain[14] << 16 | lgain[15]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_8, (lgain[16] << 16 | lgain[17]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_9, (lgain[18] << 16 | lgain[19]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_10, (lgain[20] << 16 | lgain[21]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_11, (lgain[22] << 16 | lgain[23]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_12, (lgain[24] << 16 | lgain[25]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_13, (lgain[26] << 16 | lgain[27]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_14, (lgain[28] << 16 | lgain[29]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_15, (lgain[30] << 16 | lgain[31]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_16, (lgain[32] << 16 | lgain[33]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_17, (lgain[34] << 16 | lgain[35]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_18, (lgain[36] << 16 | lgain[37]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_19, (lgain[38] << 16 | lgain[39]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_20, (lgain[40] << 16 | lgain[41]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_21, (lgain[42] << 16 | lgain[43]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_22, (lgain[44] << 16 | lgain[45]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_23, (lgain[46] << 16 | lgain[47]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_24, (lgain[48] << 16 | lgain[49]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_25, (lgain[50] << 16 | lgain[51]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_26, (lgain[52] << 16  | lgain[53]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_27, (lgain[54] << 16 | lgain[55]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_28, (lgain[56] << 16 | lgain[57]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_29, (lgain[58] << 16 | lgain[59]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_30, (lgain[60] << 16 | lgain[61]));
		VSYNC_WRITE_VPP_REG(SA_LGAIN_31, (lgain[62] << 16 | lgain[63]));
	}
	pr_aiclr("%s: done\n", __func__);
}

void ai_color_proc(struct vframe_s *vf)
{
	if (sa_adj_parm.reg_sat_sgain_en == 0 &&
		sa_adj_parm.reg_sat_lgain_en == 0)
		return;

	SLut_gen(&sa_adj_parm, &sa_fw_parm);
	ai_color_cfg(&sa_adj_parm);

	if (aiclr_dbg > 0)
		aiclr_dbg--;
}

int ai_color_debug_store(char **parm)
{
	long val;

	if (!strcmp(parm[0], "ai_color_ver")) {
		pr_info(AI_COLOR_VER);
	} else if (!strcmp(parm[0], "sat_sgain_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_adj_parm.reg_sat_sgain_en = (uint)val;
		pr_info("sat_sgain_en = %d\n", sa_adj_parm.reg_sat_sgain_en);
	} else if (!strcmp(parm[0], "sat_lgain_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		sa_adj_parm.reg_sat_lgain_en = (uint)val;
		pr_info("sat_lgain_en = %d\n", sa_adj_parm.reg_sat_lgain_en);
	}

	return 0;
}
