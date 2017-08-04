/*
 * drivers/amlogic/vpu/vpu.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"
#include "vpu_clk.h"
#include "vpu_module.h"

/* v03: add vpu clk gate control */
/* v04: add txlx support & clktree support */
#define VPU_VERION        "v04"

enum vpu_chip_e vpu_chip_type = VPU_CHIP_MAX;
int vpu_debug_print_flag;
static spinlock_t vpu_lock;
static DEFINE_MUTEX(vpu_mutex);

static unsigned int clk_vmod[VPU_MAX];

static struct vpu_conf_s vpu_conf = {
	.mem_pd0 = 0,
	.mem_pd1 = 0,
	.clk_level_dft = 0,
	.clk_level_max = 1,
	.clk_level = 0,
	.fclk_type = 0,
	/* clocktree */
	.clk_vpu_clk0 = NULL,
	.clk_vpu_clk1 = NULL,
	.clk_vpu_clk = NULL,
};

int vpu_chip_valid_check(void)
{
	int ret = 0;

	if (vpu_chip_type == VPU_CHIP_MAX) {
		VPUERR("invalid VPU in current chip\n");
		ret = -1;
	}
	return ret;
}

static void vpu_chip_detect(void)
{
	unsigned int cpu_type;

	cpu_type = get_cpu_type();
	switch (cpu_type) {
	case MESON_CPU_MAJOR_ID_M8:
		vpu_chip_type = VPU_CHIP_M8;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_M8;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_M8;
		vpu_conf.fclk_type = FCLK_TYPE_M8;
		break;
	case MESON_CPU_MAJOR_ID_M8B:
		vpu_chip_type = VPU_CHIP_M8B;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_M8B;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_M8B;
		vpu_conf.fclk_type = FCLK_TYPE_M8B;
		break;
	case MESON_CPU_MAJOR_ID_M8M2:
		vpu_chip_type = VPU_CHIP_M8M2;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_M8M2;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_M8M2;
		vpu_conf.fclk_type = FCLK_TYPE_M8M2;
		break;
	case MESON_CPU_MAJOR_ID_MG9TV:
		vpu_chip_type = VPU_CHIP_G9TV;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_G9TV;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_G9TV;
		vpu_conf.fclk_type = FCLK_TYPE_G9TV;
		break;
	/* case MESON_CPU_MAJOR_ID_MG9BB:
		vpu_chip_type = VPU_CHIP_G9BB;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_G9BB;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_G9BB;
		vpu_conf.fclk_type = FCLK_TYPE_G9BB;
		break; */
	case MESON_CPU_MAJOR_ID_GXBB:
		vpu_chip_type = VPU_CHIP_GXBB;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_GXBB;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_GXBB;
		vpu_conf.fclk_type = FCLK_TYPE_GXBB;
		break;
	case MESON_CPU_MAJOR_ID_GXTVBB:
		vpu_chip_type = VPU_CHIP_GXTVBB;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_GXTVBB;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_GXTVBB;
		vpu_conf.fclk_type = FCLK_TYPE_GXTVBB;
		break;
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:
	case MESON_CPU_MAJOR_ID_GXLX:
		vpu_chip_type = VPU_CHIP_GXL;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_GXL;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_GXL;
		vpu_conf.fclk_type = FCLK_TYPE_GXL;
		break;
	case MESON_CPU_MAJOR_ID_TXL:
		vpu_chip_type = VPU_CHIP_TXL;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_TXL;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_TXL;
		vpu_conf.fclk_type = FCLK_TYPE_TXL;
		break;
	case MESON_CPU_MAJOR_ID_TXLX:
		vpu_chip_type = VPU_CHIP_TXLX;
		vpu_conf.clk_level_dft = CLK_LEVEL_DFT_TXLX;
		vpu_conf.clk_level_max = CLK_LEVEL_MAX_TXLX;
		vpu_conf.fclk_type = FCLK_TYPE_TXLX;
		break;
	default:
		vpu_chip_type = VPU_CHIP_MAX;
		vpu_conf.clk_level_dft = 0;
		vpu_conf.clk_level_max = 1;
	}

	if (vpu_debug_print_flag)
		VPUPR("detect chip type: %d\n", vpu_chip_type);
}

static unsigned int get_vpu_clk_level_max_vmod(void)
{
	unsigned int max_level = 0;
	int i;

	for (i = 0; i < VPU_MAX; i++) {
		if (clk_vmod[i] > max_level)
			max_level = clk_vmod[i];
	}

	return max_level;
}

static unsigned int get_vpu_clk_level(unsigned int video_clk)
{
	unsigned int video_bw;
	unsigned clk_level;
	int i;

	video_bw = video_clk + 1000000;

	for (i = 0; i < vpu_conf.clk_level_max; i++) {
		if (video_bw <= vpu_clk_table[vpu_conf.fclk_type][i][0])
			break;
	}
	clk_level = i;

	return clk_level;
}

unsigned int get_vpu_clk(void)
{
	unsigned int clk_freq;
	unsigned int fclk, clk_source;
	unsigned int mux, div;

	fclk = fclk_table[vpu_conf.fclk_type] * 100; /* 0.01M resolution */
	mux = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 9, 3);
	switch (mux) {
	case 0: /* fclk_div4 */
	case 1: /* fclk_div3 */
	case 2: /* fclk_div5 */
	case 3: /* fclk_div7, m8m2 gp_pll = fclk_div7 */
		clk_source = fclk / fclk_div_table[mux];
		break;
	case 7:
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_GXTVBB))
			clk_source = 696 * 100; /* 0.01MHz */
		else
			clk_source = 0;
		break;
	default:
		clk_source = 0;
		break;
	}

	div = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 0, 7) + 1;
	clk_freq = (clk_source / div) * 10 * 1000; /* change to Hz */

	return clk_freq;
}

static int switch_gp_pll_m8m2(int flag)
{
	int cnt = 100;
	int ret = 0;

	if (flag) { /* enable gp_pll */
		/* M=182, N=3, OD=2. gp_pll=24*M/N/2^OD=364M */
		/* set gp_pll frequency fixed to 364M */
		vpu_hiu_write(HHI_GP_PLL_CNTL, 0x206b6);
		vpu_hiu_setb(HHI_GP_PLL_CNTL, 1, 30, 1); /* enable */
		do {
			udelay(10);
			vpu_hiu_setb(HHI_GP_PLL_CNTL, 1, 29, 1); /* reset */
			udelay(50);
			/* release reset */
			vpu_hiu_setb(HHI_GP_PLL_CNTL, 0, 29, 1);
			udelay(50);
			cnt--;
			if (cnt == 0)
				break;
		} while (vpu_hiu_getb(HHI_GP_PLL_CNTL, 31, 1) == 0);
		if (cnt == 0) {
			ret = -1;
			vpu_hiu_setb(HHI_GP_PLL_CNTL, 0, 30, 1);
			VPUERR("GPLL lock failed, can't use the clk source\n");
		}
	} else { /* disable gp_pll */
		vpu_hiu_setb(HHI_GP_PLL_CNTL, 0, 30, 1);
	}

	return ret;
}

static int switch_gp1_pll_g9tv(int flag)
{
	int cnt = 100;
	int ret = 0;

	if (flag) { /* enable gp1_pll */
		/* GP1 DPLL 696MHz output*/
		vpu_hiu_write(HHI_GP1_PLL_CNTL, 0x6a01023a);
		vpu_hiu_write(HHI_GP1_PLL_CNTL2, 0x69c80000);
		vpu_hiu_write(HHI_GP1_PLL_CNTL3, 0x0a5590c4);
		vpu_hiu_write(HHI_GP1_PLL_CNTL4, 0x0000500d);
		vpu_hiu_write(HHI_GP1_PLL_CNTL, 0x4a01023a);
		do {
			udelay(10);
			vpu_hiu_setb(HHI_GP1_PLL_CNTL, 1, 29, 1); /* reset */
			udelay(50);
			/* release reset */
			vpu_hiu_setb(HHI_GP1_PLL_CNTL, 0, 29, 1);
			udelay(50);
			cnt--;
			if (cnt == 0)
				break;
		} while (vpu_hiu_getb(HHI_GP1_PLL_CNTL, 31, 1) == 0);
		if (cnt == 0) {
			ret = 1;
			vpu_hiu_setb(HHI_GP1_PLL_CNTL, 0, 30, 1);
			VPUERR("GPLL lock failed, can't use the clk source\n");
		}
	} else { /* disable gp1_pll */
		vpu_hiu_setb(HHI_GP1_PLL_CNTL, 0, 30, 1);
	}

	return ret;
}

static int change_vpu_clk_m8(void *vconf1)
{
	unsigned int clk_level, temp;
	struct vpu_conf_s *vconf = (struct vpu_conf_s *)vconf1;

	clk_level = vconf->clk_level;
	temp = (vpu_clk_table[vpu_conf.fclk_type][clk_level][1] << 9) |
		(vpu_clk_table[vpu_conf.fclk_type][clk_level][2] << 0);
	vpu_hiu_write(HHI_VPU_CLK_CNTL, ((1 << 8) | temp));

	return 0;
}

static void switch_vpu_clk_m8_g9(void)
{
	unsigned int mux, div;
	int ret;

	switch (vpu_chip_type) {
	case VPU_CHIP_M8M2:
		if (vpu_conf.clk_level == (CLK_LEVEL_MAX_M8M2 - 1)) {
			ret = switch_gp_pll_m8m2(1);
			if (ret)
				vpu_conf.clk_level = CLK_LEVEL_MAX_M8M2 - 2;
		} else {
			ret = switch_gp_pll_m8m2(0);
		}
		break;
	case VPU_CHIP_G9TV:
		if (vpu_conf.clk_level == (CLK_LEVEL_MAX_M8M2 - 1)) {
			ret = switch_gp1_pll_g9tv(1);
			if (ret)
				vpu_conf.clk_level = CLK_LEVEL_MAX_M8M2 - 2;
		} else {
			ret = switch_gp1_pll_g9tv(0);
		}
		break;
	default:
		break;
	}

	/* step 1: switch to 2nd vpu clk patch */
	mux = vpu_clk_table[vpu_conf.fclk_type][0][1];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, mux, 25, 3);
	div = vpu_clk_table[vpu_conf.fclk_type][0][2];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, div, 16, 7);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 1, 24, 1);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 1, 31, 1);
	udelay(10);
	/* step 2:  adjust 1st vpu clk frequency */
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 8, 1);
	mux = vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][1];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, mux, 9, 3);
	div = vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][2];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, div, 0, 7);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 1, 8, 1);
	udelay(20);
	/* step 3:  switch back to 1st vpu clk patch */
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 31, 1);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 24, 1);

	VPUPR("set vpu clk: %uHz, readback: %uHz(0x%x)\n",
		vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][0],
		get_vpu_clk(), (vpu_hiu_read(HHI_VPU_CLK_CNTL)));
}

static int switch_gp1_pll_gxtvbb(int flag)
{
	int cnt = 100;
	int ret = 0;

	if (flag) { /* enable gp1_pll */
		/* GP1 DPLL 696MHz output*/
		vpu_hiu_write(HHI_GP1_PLL_CNTL2, 0x69c80000);
		vpu_hiu_write(HHI_GP1_PLL_CNTL3, 0x0a5590c4);
		vpu_hiu_write(HHI_GP1_PLL_CNTL4, 0x0000500d);
		vpu_hiu_write(HHI_GP1_PLL_CNTL, 0x6a01023a);
		vpu_hiu_write(HHI_GP1_PLL_CNTL, 0x4a01023a);
		do {
			udelay(10);
			vpu_hiu_setb(HHI_GP1_PLL_CNTL, 1, 29, 1); /* reset */
			udelay(50);
			/* release reset */
			vpu_hiu_setb(HHI_GP1_PLL_CNTL, 0, 29, 1);
			udelay(50);
			cnt--;
			if (cnt == 0)
				break;
		} while (vpu_hiu_getb(HHI_GP1_PLL_CNTL, 31, 1) == 0);
		if (vpu_hiu_getb(HHI_GP1_PLL_CNTL, 31, 1) == 0) {
			ret = -1;
			vpu_hiu_setb(HHI_GP1_PLL_CNTL, 0, 30, 1);
			VPUERR("GP1_PLL lock failed\n");
		}
	} else { /* disable gp1_pll */
		vpu_hiu_setb(HHI_GP1_PLL_CNTL, 0, 30, 1);
	}

	return ret;
}

/* unit: MHz */
static void switch_vpu_clk_gx(void)
{
	unsigned int mux, div;
	int ret;

	if (vpu_chip_type == VPU_CHIP_GXTVBB) {
		if (vpu_conf.clk_level == 8) {
			ret = switch_gp1_pll_gxtvbb(1);
			if (ret)
				vpu_conf.clk_level = 7;
		}
	}

	/* step 1:  switch to 2nd vpu clk patch */
	mux = vpu_clk_table[vpu_conf.fclk_type][0][1];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, mux, 25, 3);
	div = vpu_clk_table[vpu_conf.fclk_type][0][2];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, div, 16, 7);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 1, 24, 1);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 1, 31, 1);
	udelay(10);
	/* step 2:  adjust 1st vpu clk frequency */
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 8, 1);
	mux = vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][1];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, mux, 9, 3);
	div = vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][2];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, div, 0, 7);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 1, 8, 1);
	udelay(20);
	/* step 3:  switch back to 1st vpu clk patch */
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 31, 1);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 24, 1);

	VPUPR("set vpu clk: %uHz, readback: %uHz(0x%x)\n",
		vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][0],
		get_vpu_clk(), (vpu_hiu_read(HHI_VPU_CLK_CNTL)));
}

static int adjust_vpu_clk(unsigned int clk_level)
{
	unsigned long flags = 0;
	int ret = 0;

	if (vpu_chip_type == VPU_CHIP_MAX) {
		VPUPR("invalid VPU in current CPU type\n");
		return 0;
	}

	spin_lock_irqsave(&vpu_lock, flags);

	vpu_conf.clk_level = clk_level;
	switch (vpu_chip_type) {
	case VPU_CHIP_GXBB:
	case VPU_CHIP_GXTVBB:
	case VPU_CHIP_GXL:
	case VPU_CHIP_GXM:
	case VPU_CHIP_TXL:
	case VPU_CHIP_TXLX:
	case VPU_CHIP_GXLX:
		switch_vpu_clk_gx();
		break;
	case VPU_CHIP_M8M2:
	case VPU_CHIP_G9TV:
	case VPU_CHIP_M8B:
	case VPU_CHIP_G9BB:
		switch_vpu_clk_m8_g9();
		break;
	case VPU_CHIP_M8:
		change_vpu_clk_m8(&vpu_conf);
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&vpu_lock, flags);
	return ret;
}

static int set_vpu_clk(unsigned int vclk)
{
	int ret = 0;
	unsigned int clk_level, mux, div;
	mutex_lock(&vpu_mutex);

	if (vclk >= 100) { /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	} else { /* regard as clk_level */
		clk_level = vclk;
	}

	if (clk_level >= vpu_conf.clk_level_max) {
		ret = 1;
		VPUERR("set vpu clk out of supported range\n");
		goto set_vpu_clk_limit;
	}
#ifdef LIMIT_VPU_CLK_LOW
	else if (clk_level < vpu_conf.clk_level_dft) {
		ret = 3;
		VPUERR("set vpu clk less than system default\n");
		goto set_vpu_clk_limit;
	}
#endif

	mux = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 9, 3);
	div = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 0, 7);
	if ((mux != vpu_clk_table[vpu_conf.fclk_type][clk_level][1])
		|| (div != vpu_clk_table[vpu_conf.fclk_type][clk_level][2])) {
		ret = adjust_vpu_clk(clk_level);
	}

set_vpu_clk_limit:
	mutex_unlock(&vpu_mutex);
	return ret;
}

/* *********************************************** */
/* VPU_CLK control */
/* *********************************************** */
/*
 *  Function: get_vpu_clk_vmod
 *      Get vpu clk holding frequency with specified vmod
 *
 *      Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *
 *  Returns:
 *      unsigned int, vpu clk frequency unit in Hz
 *
 *      Example:
 *      video_clk = get_vpu_clk_vmod(VPU_VENCP);
 *      video_clk = get_vpu_clk_vmod(VPU_VIU_OSD1);
 *
*/
unsigned int get_vpu_clk_vmod(unsigned int vmod)
{
	unsigned int vpu_clk;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return 0;

	mutex_lock(&vpu_mutex);

	if (vmod < VPU_MAX) {
		vpu_clk = clk_vmod[vmod];
		vpu_clk = vpu_clk_table[vpu_conf.fclk_type][vpu_clk][0];
	} else {
		vpu_clk = 0;
		VPUERR("unsupport vmod\n");
	}

	mutex_unlock(&vpu_mutex);
	return vpu_clk;
}

/*
 *  Function: request_vpu_clk_vmod
 *      Request a new vpu clk holding frequency with specified vmod
 *      Will change vpu clk if the max level in all vmod vpu clk holdings
 *      is unequal to current vpu clk level
 *
 *  Parameters:
 *      vclk - unsigned int, vpu clk frequency unit in Hz
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *
 *  Returns:
 *      int, 0 for success, 1 for failed
 *
 *  Example:
 *      ret = request_vpu_clk_vmod(100000000, VPU_VENCP);
 *      ret = request_vpu_clk_vmod(300000000, VPU_VIU_OSD1);
 *
*/
int request_vpu_clk_vmod(unsigned int vclk, unsigned int vmod)
{
	int ret = 0;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	unsigned clk_level;

	ret = vpu_chip_valid_check();
	if (ret)
		return 1;

	mutex_lock(&vpu_mutex);

	if (vclk >= 100) /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	else /* regard as clk_level */
		clk_level = vclk;

	if (clk_level >= vpu_conf.clk_level_max) {
		ret = 1;
		VPUERR("set vpu clk out of supported range\n");
		goto request_vpu_clk_limit;
	}

	if (vmod == VPU_MAX) {
		ret = 1;
		VPUERR("unsupport vmod\n");
		goto request_vpu_clk_limit;
	}

	clk_vmod[vmod] = clk_level;
	if (vpu_debug_print_flag) {
		VPUPR("request vpu clk: %s %uHz\n",
			vpu_mod_table[vmod],
			vpu_clk_table[vpu_conf.fclk_type][clk_level][0]);
		dump_stack();
	}

	clk_level = get_vpu_clk_level_max_vmod();
	if (clk_level != vpu_conf.clk_level)
		adjust_vpu_clk(clk_level);

request_vpu_clk_limit:
	mutex_unlock(&vpu_mutex);
#endif
	return ret;
}

/*
 *  Function: release_vpu_clk_vmod
 *      Release vpu clk holding frequency to 0 with specified vmod
 *      Will change vpu clk if the max level in all vmod vpu clk holdings
 *      is unequal to current vpu clk level
 *
 *  Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *
 *  Returns:
 *      int, 0 for success, 1 for failed
 *
 *  Example:
 *      ret = release_vpu_clk_vmod(VPU_VENCP);
 *      ret = release_vpu_clk_vmod(VPU_VIU_OSD1);
 *
*/
int release_vpu_clk_vmod(unsigned int vmod)
{
	int ret = 0;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	unsigned clk_level;

	ret = vpu_chip_valid_check();
	if (ret)
		return 1;

	mutex_lock(&vpu_mutex);

	clk_level = 0;
	if (vmod == VPU_MAX) {
		ret = 1;
		VPUERR("unsupport vmod\n");
		goto release_vpu_clk_limit;
	}

	clk_vmod[vmod] = clk_level;
	if (vpu_debug_print_flag) {
		VPUPR("release vpu clk: %s\n",
			vpu_mod_table[vmod]);
		dump_stack();
	}

	clk_level = get_vpu_clk_level_max_vmod();
	if (clk_level != vpu_conf.clk_level)
		adjust_vpu_clk(clk_level);

release_vpu_clk_limit:
	mutex_unlock(&vpu_mutex);
#endif
	return ret;
}

/* *********************************************** */

/* *********************************************** */
/* VPU sysfs function */
/* *********************************************** */
static const char *vpu_usage_str = {
"Usage:\n"
"	echo r > mem ; read vpu memory power down status\n"
"	echo w <vmod> <flag> > mem ; write vpu memory power down\n"
"	<flag>: 0=power up, 1=power down\n"
"\n"
"	echo r > gate ; read vpu clk gate status\n"
"	echo w <vmod> <flag> > gate ; write vpu clk gate\n"
"	<flag>: 0=gate off, 1=gate on\n"
"\n"
"	echo 1 > test ; run vcbus access test\n"
"\n"
"	echo get > clk ; print current vpu clk\n"
"	echo set <vclk> > clk ; force to set vpu clk\n"
"	echo dump [vmod] > clk ; dump vpu clk by vmod, [vmod] is unnecessary\n"
"	echo request <vclk> <vmod> > clk ; request vpu clk holding by vmod\n"
"	echo release <vmod> > clk ; release vpu clk holding by vmod\n"
"\n"
"	request & release will change vpu clk if the max level in all vmod vpu clk holdings is unequal to current vpu clk level.\n"
"	vclk both support level(1~10) and frequency value (unit in Hz).\n"
"	vclk level & frequency:\n"
"		0: 100M        1: 160M        2: 200M\n"
"		3: 250M        4: 350M        5: 400M\n"
"		6: 500M        7: 650M        8: 700M\n"
"\n"
"	echo <0|1> > print ; set debug print flag\n"
};

static ssize_t vpu_debug_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", vpu_usage_str);
}

static ssize_t vpu_clk_debug(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int tmp[2], n;
	unsigned int fclk_type;

	fclk_type = vpu_conf.fclk_type;
	switch (buf[0]) {
	case 'g': /* get */
		VPUPR("get current clk: %uHz\n", get_vpu_clk());
		break;
	case 's': /* set */
		tmp[0] = 4;
		ret = sscanf(buf, "set %u", &tmp[0]);
		if (tmp[0] > 100)
			VPUPR("set clk frequency: %uHz\n", tmp[0]);
		else
			VPUPR("set clk level: %u\n", tmp[0]);
		set_vpu_clk(tmp[0]);
		break;
	case 'r':
		if (buf[2] == 'q') { /* request */
			tmp[0] = 0;
			tmp[1] = VPU_MAX;
			ret = sscanf(buf, "request %u %u", &tmp[0], &tmp[1]);
			request_vpu_clk_vmod(tmp[0], tmp[1]);
		} else if (buf[2] == 'l') { /* release */
			tmp[0] = VPU_MAX;
			ret = sscanf(buf, "release %u", &tmp[0]);
			release_vpu_clk_vmod(tmp[0]);
		}
		break;
	case 'd':
		tmp[0] = VPU_MAX;
		ret = sscanf(buf, "dump %u", &tmp[0]);
		if (tmp[0] == VPU_MAX) {
			n = get_vpu_clk_level_max_vmod();
			VPUPR("clk max holdings: %uHz(%u)\n",
				vpu_clk_table[fclk_type][n][0], n);
		} else {
			VPUPR("clk holdings:\n");
			pr_info("%s:  %uHz(%u)\n", vpu_mod_table[tmp[0]],
			vpu_clk_table[fclk_type][clk_vmod[tmp[0]]][0],
			clk_vmod[tmp[0]]);
		}
		break;
	default:
		VPUERR("wrong debug command\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
}

static ssize_t vpu_mem_debug(struct class *class, struct class_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int tmp[2];
	unsigned int _reg0, _reg1, _reg2;

	_reg0 = HHI_VPU_MEM_PD_REG0;
	_reg1 = HHI_VPU_MEM_PD_REG1;
	_reg2 = HHI_VPU_MEM_PD_REG2;

	switch (buf[0]) {
	case 'r':
		VPUPR("mem_pd0: 0x%08x\n", vpu_hiu_read(_reg0));
		VPUPR("mem_pd1: 0x%08x\n", vpu_hiu_read(_reg1));
		if ((vpu_chip_type == VPU_CHIP_GXL) ||
			(vpu_chip_type == VPU_CHIP_GXM) ||
			(vpu_chip_type == VPU_CHIP_TXL) ||
			(vpu_chip_type == VPU_CHIP_TXLX) ||
			(vpu_chip_type == VPU_CHIP_GXLX)) {
			VPUPR("mem_pd2: 0x%08x\n", vpu_hiu_read(_reg2));
		}
		break;
	case 'w':
		ret = sscanf(buf, "w %u %u", &tmp[0], &tmp[1]);
		tmp[0] = (tmp[0] > VPU_MAX) ? VPU_MAX : tmp[0];
		tmp[1] = (tmp[1] == VPU_MEM_POWER_ON) ? 0 : 1;
		VPUPR("switch_vpu_mem_pd: %s %s\n",
			vpu_mod_table[tmp[0]], (tmp[1] ? "DOWN" : "ON"));
		switch_vpu_mem_pd_vmod(tmp[0], tmp[1]);
		break;
	default:
		VPUERR("wrong mem_pd command\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
}

static ssize_t vpu_clk_gate_debug(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int tmp[2];

	switch (buf[0]) {
	case 'r':
		VPUPR("HHI_GCLK_OTHER: 0x%08x\n", vpu_hiu_read(HHI_GCLK_OTHER));
		VPUPR("VPU_CLK_GATE: 0x%08x\n", vpu_vcbus_read(VPU_CLK_GATE));
		VPUPR("VDIN0_COM_GCLK_CTRL: 0x%08x\n",
			vpu_vcbus_read(VDIN0_COM_GCLK_CTRL));
		VPUPR("VDIN0_COM_GCLK_CTRL2: 0x%08x\n",
			vpu_vcbus_read(VDIN0_COM_GCLK_CTRL2));
		VPUPR("VDIN1_COM_GCLK_CTRL: 0x%08x\n",
			vpu_vcbus_read(VDIN1_COM_GCLK_CTRL));
		VPUPR("VDIN1_COM_GCLK_CTRL2: 0x%08x\n",
			vpu_vcbus_read(VDIN1_COM_GCLK_CTRL2));
		VPUPR("DI_CLKG_CTRL: 0x%08x\n",
			vpu_vcbus_read(DI_CLKG_CTRL));
		VPUPR("VPP_GCLK_CTRL0: 0x%08x\n",
			vpu_vcbus_read(VPP_GCLK_CTRL0));
		VPUPR("VPP_GCLK_CTRL1: 0x%08x\n",
			vpu_vcbus_read(VPP_GCLK_CTRL1));
		VPUPR("VPP_SC_GCLK_CTRL: 0x%08x\n",
			vpu_vcbus_read(VPP_SC_GCLK_CTRL));
		if (vpu_chip_type == VPU_CHIP_G9TV) {
			VPUPR("VPP_SRSCL_GCLK_CTRL: 0x%08x\n",
				vpu_vcbus_read(VPP_SRSCL_GCLK_CTRL));
			VPUPR("VPP_OSDSR_GCLK_CTRL: 0x%08x\n",
				vpu_vcbus_read(VPP_OSDSR_GCLK_CTRL));
		}
		VPUPR("VPP_XVYCC_GCLK_CTRL: 0x%08x\n",
			vpu_vcbus_read(VPP_XVYCC_GCLK_CTRL));
		break;
	case 'w':
		ret = sscanf(buf, "w %u %u", &tmp[0], &tmp[1]);
		tmp[0] = (tmp[0] > VPU_MAX) ? VPU_MAX : tmp[0];
		tmp[1] = (tmp[1] == VPU_CLK_GATE_ON) ? 1 : 0;
		VPUPR("switch_vpu_clk_gate: %s %s\n",
			vpu_mod_table[tmp[0]], (tmp[1] ? "ON" : "OFF"));
		switch_vpu_clk_gate_vmod(tmp[0], tmp[1]);
		break;
	default:
		VPUERR("wrong clk_gate command\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
}

static unsigned int vcbus_reg[] = {
	0x1b7f, /* VENC_VDAC_TST_VAL */
	0x1c30, /* ENCP_DVI_HSO_BEGIN */
	0x1d00, /* VPP_DUMMY_DATA */
	0x2730, /* VPU_VPU_PWM_V0 */
};

static void vcbus_test(void)
{
	unsigned int val;
	unsigned int temp;
	int i, j;

	VPUPR("vcbus test:\n");
	for (i = 0; i < ARRAY_SIZE(vcbus_reg); i++) {
		for (j = 0; j < 24; j++) {
			val = vpu_vcbus_read(vcbus_reg[i]);
			pr_info("%02d read 0x%04x=0x%08x\n",
				j, vcbus_reg[i], val);
		}
		pr_info("\n");
	}
	temp = 0x5a5a5a5a;
	for (i = 0; i < ARRAY_SIZE(vcbus_reg); i++) {
		vpu_vcbus_write(vcbus_reg[i], temp);
		val = vpu_vcbus_read(vcbus_reg[i]);
		pr_info("write 0x%04x=0x%08x, readback: 0x%08x\n",
			vcbus_reg[i], temp, val);
	}
	for (i = 0; i < ARRAY_SIZE(vcbus_reg); i++) {
		for (j = 0; j < 24; j++) {
			val = vpu_vcbus_read(vcbus_reg[i]);
			pr_info("%02d read 0x%04x=0x%08x\n",
				j, vcbus_reg[i], val);
		}
		pr_info("\n");
	}
}

static ssize_t vpu_test_debug(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int ret;

	vcbus_test();

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
}

static ssize_t vpu_print_debug(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int temp;

	temp = 0;
	ret = sscanf(buf, "%u", &temp);
	vpu_debug_print_flag = temp;
	pr_info("set vpu debug_print_flag: %d\n", vpu_debug_print_flag);

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
}

static ssize_t vpu_debug_info(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int level_max;
	ssize_t len = 0;

	level_max = vpu_conf.clk_level_max - 1;
	len = sprintf(buf, "detect chip type: %d\n", vpu_chip_type);
	len += sprintf(buf+len, "clk_level:         %d(%dHz)\n"
		"clk_level default: %d(%dHz)\n"
		"clk_level max:     %d(%dHz)\n"
		"fclk_type:         %d(%dHz)\n",
		vpu_conf.clk_level,
		vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][0],
		vpu_conf.clk_level_dft,
		vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level_dft][0],
		level_max, vpu_clk_table[vpu_conf.fclk_type][level_max][0],
		vpu_conf.fclk_type,
		(fclk_table[vpu_conf.fclk_type] * 1000000));

	return len;
}

static struct class_attribute vpu_debug_class_attrs[] = {
	__ATTR(clk, S_IRUGO | S_IWUSR, vpu_debug_help, vpu_clk_debug),
	__ATTR(mem, S_IRUGO | S_IWUSR, vpu_debug_help, vpu_mem_debug),
	__ATTR(gate, S_IRUGO | S_IWUSR, vpu_debug_help, vpu_clk_gate_debug),
	__ATTR(test, S_IRUGO | S_IWUSR, vpu_debug_help, vpu_test_debug),
	__ATTR(print, S_IRUGO | S_IWUSR, vpu_debug_help, vpu_print_debug),
	__ATTR(info, S_IRUGO | S_IWUSR, vpu_debug_info, NULL),
	__ATTR(help, S_IRUGO | S_IWUSR, vpu_debug_help, NULL),
};

static struct class *vpu_debug_class;
static int creat_vpu_debug_class(void)
{
	int i;

	vpu_debug_class = class_create(THIS_MODULE, "vpu");
	if (IS_ERR(vpu_debug_class)) {
		VPUERR("create vpu_debug_class failed\n");
		return -1;
	}
	for (i = 0; i < ARRAY_SIZE(vpu_debug_class_attrs); i++) {
		if (class_create_file(vpu_debug_class,
			&vpu_debug_class_attrs[i])) {
			VPUERR("create vpu debug attribute %s failed\n",
				vpu_debug_class_attrs[i].attr.name);
		}
	}
	return 0;
}

static int remove_vpu_debug_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vpu_debug_class_attrs); i++)
		class_remove_file(vpu_debug_class, &vpu_debug_class_attrs[i]);

	class_destroy(vpu_debug_class);
	vpu_debug_class = NULL;
	return 0;
}
/* ********************************************************* */

#ifdef CONFIG_PM
static int vpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int vpu_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

static int get_vpu_config(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int val;
	struct device_node *vpu_np;

	vpu_np = pdev->dev.of_node;
	if (!vpu_np) {
		VPUERR("don't find vpu node\n");
		return -1;
	}

	ret = of_property_read_u32(vpu_np, "clk_level", &val);
	if (ret) {
		VPUPR("don't find clk_level, use default setting\n");
	} else {
		if (val >= vpu_conf.clk_level_max) {
			VPUERR("clk_level is out of support, set to default\n");
			val = vpu_conf.clk_level_dft;
		}

		vpu_conf.clk_level = val;
	}
	VPUPR("load vpu_clk: %uHz(%u)\n",
		vpu_clk_table[vpu_conf.fclk_type][val][0], vpu_conf.clk_level);

	return ret;
}

static void vpu_clktree_init(struct device *dev)
{
	static struct clk *clk_vapb;

	/* init & enable vapb_clk */
	clk_vapb = devm_clk_get(dev, "vapb_clk0");
	if (!IS_ERR(clk_vapb))
		clk_prepare_enable(clk_vapb);
	else
		VPUERR("%s: vapb_clk\n", __func__);

	/* init & enable vpu_clk */
	vpu_conf.clk_vpu_clk0 = devm_clk_get(dev, "vpu_clk0");
	vpu_conf.clk_vpu_clk1 = devm_clk_get(dev, "vpu_clk1");
	vpu_conf.clk_vpu_clk = devm_clk_get(dev, "vpu_clk");
	if ((!IS_ERR(vpu_conf.clk_vpu_clk0)) &&
		(!IS_ERR(vpu_conf.clk_vpu_clk0)) &&
		(!IS_ERR(vpu_conf.clk_vpu_clk))) {
		clk_set_parent(vpu_conf.clk_vpu_clk, vpu_conf.clk_vpu_clk0);
		clk_prepare_enable(vpu_conf.clk_vpu_clk);
	} else {
		VPUERR("%s: vpu_clk\n", __func__);
	}

	VPUPR("%s\n", __func__);
}

static struct of_device_id vpu_of_table[] = {
	{
		.compatible = "amlogic, vpu",
	},
	{},
};

static int vpu_probe(struct platform_device *pdev)
{
	int ret = 0;

#ifdef VPU_DEBUG_PRINT
	vpu_debug_print_flag = 1;
#else
	vpu_debug_print_flag = 0;
#endif
	spin_lock_init(&vpu_lock);

	VPUPR("driver version: %s\n", VPU_VERION);
	memset(clk_vmod, 0, sizeof(clk_vmod));

	vpu_chip_detect();
	ret = vpu_chip_valid_check();
	if (ret)
		return 0;

	vpu_ioremap();
	get_vpu_config(pdev);
	vpu_ctrl_probe();

	set_vpu_clk(vpu_conf.clk_level);
	switch (vpu_chip_type) {
	case VPU_CHIP_TXLX:
		vpu_clktree_init(&pdev->dev);
		break;
	default:
		break;
	}

	creat_vpu_debug_class();

	VPUPR("%s OK\n", __func__);
	return 0;
}

static int vpu_remove(struct platform_device *pdev)
{
	remove_vpu_debug_class();
	return 0;
}

static struct platform_driver vpu_driver = {
	.driver = {
		.name = "vpu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vpu_of_table),
	},
	.probe = vpu_probe,
	.remove = vpu_remove,
#ifdef CONFIG_PM
	.suspend    = vpu_suspend,
	.resume     = vpu_resume,
#endif
};

static int __init vpu_init(void)
{
	return platform_driver_register(&vpu_driver);
}

static void __exit vpu_exit(void)
{
	platform_driver_unregister(&vpu_driver);
}

postcore_initcall(vpu_init);
module_exit(vpu_exit);

MODULE_DESCRIPTION("meson vpu driver");
MODULE_LICENSE("GPL v2");
