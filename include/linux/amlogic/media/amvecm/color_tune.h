/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef COLOR_TUNE_H
#define COLOR_TUNE_H

struct color_param_s {
	int rgain_r;
	int rgain_g;
	int rgain_b;

	int ggain_r;
	int ggain_g;
	int ggain_b;

	int bgain_r;
	int bgain_g;
	int bgain_b;

	int cgain_r;
	int cgain_g;
	int cgain_b;

	int mgain_r;
	int mgain_g;
	int mgain_b;

	int ygain_r;
	int ygain_g;
	int ygain_b;
};

struct ct_func_s {
	struct color_param_s *cl_par;
	void (*ct)(struct color_param_s *parm, int (*lut3di)[3], unsigned int (*lut3do)[3]);
};

struct ct_func_s *get_ct_func(void);
void color_lut_init(unsigned int en);
int ct_dbg(char **param);
void lut_release(void);
#endif

