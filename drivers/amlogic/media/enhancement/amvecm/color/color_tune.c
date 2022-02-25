// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/color/color_tune.c
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
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/amvecm/color_tune.h>
#include "../amve.h"

#define CT_DRV_VER "color tune drv ver: 2022-02-24-v1"
static int (*plut)[3];
static unsigned int (*plut_out)[3];

static struct color_param_s ct_parm = {
	.rgain_r = 614,
	.rgain_g = -102,
	.rgain_b = 512,

	.ggain_r = 0,
	.ggain_g = 512,
	.ggain_b = 0,

	.bgain_r = -102,
	.bgain_g = 0,
	.bgain_b = 819,

	.cgain_r = -614,
	.cgain_g = 0,
	.cgain_b = 512,

	.mgain_r = 1024,
	.mgain_g = -1024,
	.mgain_b = 0,

	.ygain_r = 205,
	.ygain_g = 819,
	.ygain_b = -307
};

struct ct_func_s ct_func = {
	.cl_par = &ct_parm,
	.ct = NULL,
};

struct ct_func_s *get_ct_func(void)
{
	return &ct_func;
}
EXPORT_SYMBOL(get_ct_func);

void color_lut_init(unsigned int en)
{
	int i, j, k;

	if (!en)
		return;

	plut = kmalloc(4913 * sizeof(int) * 3, GFP_KERNEL);
	plut_out = kmalloc(4913 * sizeof(unsigned int) * 3, GFP_KERNEL);

	if (!plut || !plut_out) {
		pr_info("plut for rgbcmy 3dlut kmalloc fail\n");
		return;
	}

	for (i = 0; i < 17; i++)
		for (j = 0; j < 17; j++)
			for (k = 0; k < 17; k++) {
				plut[17 * 17 * i + 17 * j + k][0] = i * 64;
				plut[17 * 17 * i + 17 * j + k][1] = j * 64;
				plut[17 * 17 * i + 17 * j + k][2] = k * 64;

				if (plut[17 * 17 * i + 17 * j + k][0] > 1023)
					plut[17 * 17 * i + 17 * j + k][0] = 1023;
				if (plut[17 * 17 * i + 17 * j + k][1] > 1023)
					plut[17 * 17 * i + 17 * j + k][1] = 1023;
				if (plut[17 * 17 * i + 17 * j + k][2] > 1023)
					plut[17 * 17 * i + 17 * j + k][2] = 1023;
			}
}

void lut_release(void)
{
	kfree(plut);
	kfree(plut_out);
	plut = NULL;
	plut_out = NULL;
}

void ct_process(void)
{
	struct ct_func_s *ct_func;

	ct_func = get_ct_func();
	ct_func->ct(ct_func->cl_par, plut, plut_out);
	vpp_set_lut3d(0, 0, plut_out, 0);
}

int ct_dbg(char **parm)
{
	long val;

	if (!strcmp(parm[0], "ct_en"))  {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		ct_en = (int)val;
		pr_info("ct_en = %d\n", ct_en);
	} else if (!strcmp(parm[0], "read_parm")) {
		pr_info("ct_en = %d\n", ct_en);

		pr_info("rgain_r = %d\n", ct_parm.rgain_r);
		pr_info("rgain_g = %d\n", ct_parm.rgain_g);
		pr_info("rgain_b = %d\n", ct_parm.rgain_b);

		pr_info("ggain_r = %d\n", ct_parm.ggain_r);
		pr_info("ggain_g = %d\n", ct_parm.ggain_g);
		pr_info("ggain_b = %d\n", ct_parm.ggain_b);

		pr_info("bgain_r = %d\n", ct_parm.bgain_r);
		pr_info("bgain_g = %d\n", ct_parm.bgain_g);
		pr_info("bgain_b = %d\n", ct_parm.bgain_b);

		pr_info("cgain_r = %d\n", ct_parm.cgain_r);
		pr_info("cgain_g = %d\n", ct_parm.cgain_g);
		pr_info("cgain_b = %d\n", ct_parm.cgain_b);

		pr_info("mgain_r = %d\n", ct_parm.mgain_r);
		pr_info("mgain_g = %d\n", ct_parm.mgain_g);
		pr_info("mgain_b = %d\n", ct_parm.mgain_b);

		pr_info("ygain_r = %d\n", ct_parm.ygain_r);
		pr_info("ygain_g = %d\n", ct_parm.ygain_g);
		pr_info("ygain_b = %d\n", ct_parm.ygain_b);
	} else if (!strcmp(parm[0], "rgain_r")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.rgain_r = (int)val;
		pr_info("rgain_r = %d\n", ct_parm.rgain_r);
	} else if (!strcmp(parm[0], "rgain_g")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.rgain_g = (int)val;
		pr_info("rgain_g = %d\n", ct_parm.rgain_g);
	} else if (!strcmp(parm[0], "rgain_b")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.rgain_b = (int)val;
		pr_info("rgain_b = %d\n", ct_parm.rgain_b);
	} else if (!strcmp(parm[0], "ggain_r")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.ggain_r = (int)val;
		pr_info("ggain_r = %d\n", ct_parm.ggain_r);
	} else if (!strcmp(parm[0], "ggain_g")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.ggain_g = (int)val;
		pr_info("ggain_g = %d\n", ct_parm.ggain_g);
	} else if (!strcmp(parm[0], "ggain_b")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.ggain_b = (int)val;
		pr_info("ggain_b = %d\n", ct_parm.ggain_b);
	} else if (!strcmp(parm[0], "bgain_r")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.bgain_r = (int)val;
		pr_info("bgain_r = %d\n", ct_parm.bgain_r);
	} else if (!strcmp(parm[0], "bgain_g")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.bgain_g = (int)val;
		pr_info("bgain_g = %d\n", ct_parm.bgain_g);
	} else if (!strcmp(parm[0], "bgain_b")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.bgain_b = (int)val;
		pr_info("bgain_b = %d\n", ct_parm.bgain_b);
	} else if (!strcmp(parm[0], "cgain_r")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.cgain_r = (int)val;
		pr_info("cgain_r = %d\n", ct_parm.cgain_r);
	} else if (!strcmp(parm[0], "cgain_g")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.cgain_g = (int)val;
		pr_info("cgain_g = %d\n", ct_parm.cgain_g);
	} else if (!strcmp(parm[0], "cgain_b")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.cgain_b = (int)val;
		pr_info("cgain_b = %d\n", ct_parm.cgain_b);
	} else if (!strcmp(parm[0], "mgain_r")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.mgain_r = (int)val;
		pr_info("mgain_r = %d\n", ct_parm.mgain_r);
	} else if (!strcmp(parm[0], "mgain_g")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.mgain_g = (int)val;
		pr_info("mgain_g = %d\n", ct_parm.mgain_g);
	} else if (!strcmp(parm[0], "mgain_b")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.mgain_b = (int)val;
		pr_info("mgain_b = %d\n", ct_parm.mgain_b);
	} else if (!strcmp(parm[0], "ygain_r")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.ygain_r = (int)val;
		pr_info("ygain_r = %d\n", ct_parm.ygain_r);
	} else if (!strcmp(parm[0], "ygain_g")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.ygain_g = (int)val;
		pr_info("ygain_g = %d\n", ct_parm.ygain_g);
	} else if (!strcmp(parm[0], "ygain_b")) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto error;
		ct_parm.ygain_b = (int)val;
		pr_info("ygain_b = %d\n", ct_parm.ygain_b);
	} else if (!strcmp(parm[0], "tune")) {
		ct_process();
		pr_info("color tune\n");
	} else if (!strcmp(parm[0], "ct_ver")) {
		pr_info("%s\n", CT_DRV_VER);
	}

	return 0;

error:
	return -1;
}

