/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef AI_COLOR_H
#define AI_COLOR_H

struct sa_adj_param_s {
	int *reg_s_gain_lut;// s12bit (-1276, 768) = (-2.5, 1.5)  2*768/1023 = 1.5
	int *reg_l_gain_lut;// 10bit (0, 1)
	int reg_sat_s_gain_en;// u1
	int reg_sat_l_gain_en;// u1
	int reg_sat_adj_a;// 10bit
	int reg_sat_prt;// 10bit
	int reg_sat_prt_p;// u10.10 20bit (2048~2^20)
	int reg_sat_prt_th;// 10bit
};

struct sa_fw_param_s {
	int reg_zero;
	int reg_sat_adj;//8bit
	int reg_sat_shift;//10bit 0.1 = 0.1 * 1023 / 2   (-512 ~ 512  = -1 ~ 1)

	int reg_skin_th;
	int reg_skin_shift;//10bit 0.1 = 0.1 * 1023 / 2   (-256 ~ 255  = -0.5 ~ 0.5)
	int reg_skin_adj;//8bit 7bit = 1

	int reg_hue_left1;
	int reg_hue_right1;
	int reg_hue_adj1;
	int reg_hue_shift1;

	int reg_hue_left2;
	int reg_hue_right2;
	int reg_hue_adj2;
	int reg_hue_shift2;

	int reg_hue_left3;
	int reg_hue_right3;
	int reg_hue_adj3;
	int reg_hue_shift3;
};

struct sa_param_s    {
	struct sa_adj_param_s *sa_adj_param;
	struct sa_fw_param_s *sa_fw_param;
};

void ai_color_proc(struct vframe_s *vf);
int ai_color_debug_store(char **parm);
void ai_clr_config(int enable);
#endif
