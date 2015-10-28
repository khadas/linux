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
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/vpu.h>
#include <linux/amlogic/vout/vinfo.h>
#include "vpu_reg.h"
#include "vpu.h"

#define VPU_VERION        "v01"

static int vpu_debug_print_flag;
static spinlock_t vpu_lock;
static spinlock_t vpu_mem_lock;
static DEFINE_MUTEX(vpu_mutex);

static unsigned int clk_vmod[50];

static struct VPU_Conf_t vpu_conf = {
	.mem_pd0 = 0,
	.mem_pd1 = 0,
	/* .chip_type = VPU_CHIP_MAX, */
	.clk_level_dft = 0,
	.clk_level_max = 1,
	.clk_level = 0,
	.fclk_type = 0,
};

static enum vpu_mod_t get_vpu_mod(unsigned int vmod)
{
	unsigned int vpu_mod = VPU_MAX;
	if (vmod < VMODE_MAX) {
		switch (vmod) {
		case VMODE_480I:
		case VMODE_480I_RPT:
		case VMODE_576I:
		case VMODE_576I_RPT:
		case VMODE_480CVBS:
		case VMODE_576CVBS:
			vpu_mod = VPU_VENCI;
			break;
		case VMODE_LCD:
		case VMODE_LVDS_1080P:
		case VMODE_LVDS_1080P_50HZ:
			vpu_mod = VPU_VENCL;
			break;
		default:
			vpu_mod = VPU_VENCP;
			break;
		}
	} else if ((vmod >= VPU_MOD_START) && (vmod < VPU_MAX)) {
		vpu_mod = vmod;
	} else {
		vpu_mod = VPU_MAX;
	}
	return vpu_mod;
}

#ifdef CONFIG_VPU_DYNAMIC_ADJ
static unsigned int get_vpu_clk_level_max_vmod(void)
{
	unsigned int max_level;
	int i;

	max_level = 0;
	for (i = VPU_MOD_START; i < VPU_MAX; i++) {
		if (clk_vmod[i-VPU_MOD_START] > max_level)
			max_level = clk_vmod[i-VPU_MOD_START];
	}

	return max_level;
}
#endif

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
	unsigned int reg;
	unsigned int clk_freq;
	unsigned int fclk, clk_source;
	unsigned int mux, div;

	if (vpu_chip_type == VPU_CHIP_GXBB)
		reg = HHI_VPU_CLK_CNTL_GX;
	else
		reg = HHI_VPU_CLK_CNTL;

	fclk = fclk_table[vpu_conf.fclk_type] * 100; /* 0.01M resolution */
	mux = vpu_hiu_getb(reg, 9, 3);
	switch (mux) {
	case 0: /* fclk_div4 */
	case 1: /* fclk_div3 */
	case 2: /* fclk_div5 */
	case 3: /* fclk_div7, m8m2 gp_pll = fclk_div7 */
		clk_source = fclk / fclk_div_table[mux];
		break;
	case 7:
		if (vpu_chip_type == VPU_CHIP_G9TV)
			clk_source = 696 * 100; /* 0.01MHz */
		else
			clk_source = 0;
		break;
	default:
		clk_source = 0;
		break;
	}

	div = vpu_hiu_getb(reg, 0, 7) + 1;
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
		vpu_cbus_write(HHI_GP_PLL_CNTL, 0x206b6);
		vpu_cbus_setb(HHI_GP_PLL_CNTL, 1, 30, 1); /* enable */
		do {
			udelay(10);
			vpu_cbus_setb(HHI_GP_PLL_CNTL, 1, 29, 1); /* reset */
			udelay(50);
			/* release reset */
			vpu_cbus_setb(HHI_GP_PLL_CNTL, 0, 29, 1);
			udelay(50);
			cnt--;
			if (cnt == 0)
				break;
		} while (vpu_cbus_getb(HHI_GP_PLL_CNTL, 31, 1) == 0);
		if (cnt == 0) {
			ret = 1;
			vpu_cbus_setb(HHI_GP_PLL_CNTL, 0, 30, 1);
			VPUERR("GPLL lock failed, can't use the clk source\n");
		}
	} else { /* disable gp_pll */
		vpu_cbus_setb(HHI_GP_PLL_CNTL, 0, 30, 1);
	}

	return ret;
}

static int switch_gp1_pll_g9tv(int flag)
{
	int cnt = 100;
	int ret = 0;

	if (flag) { /* enable gp1_pll */
		/* GP1 DPLL 696MHz output*/
		vpu_cbus_write(HHI_GP1_PLL_CNTL, 0x6a01023a);
		vpu_cbus_write(HHI_GP1_PLL_CNTL2, 0x69c80000);
		vpu_cbus_write(HHI_GP1_PLL_CNTL3, 0x0a5590c4);
		vpu_cbus_write(HHI_GP1_PLL_CNTL4, 0x0000500d);
		vpu_cbus_write(HHI_GP1_PLL_CNTL, 0x4a01023a);
		do {
			udelay(10);
			vpu_cbus_setb(HHI_GP1_PLL_CNTL, 1, 29, 1); /* reset */
			udelay(50);
			/* release reset */
			vpu_cbus_setb(HHI_GP1_PLL_CNTL, 0, 29, 1);
			udelay(50);
			cnt--;
			if (cnt == 0)
				break;
		} while (vpu_cbus_getb(HHI_GP1_PLL_CNTL, 31, 1) == 0);
		if (cnt == 0) {
			ret = 1;
			vpu_cbus_setb(HHI_GP1_PLL_CNTL, 0, 30, 1);
			VPUERR("GPLL lock failed, can't use the clk source\n");
		}
	} else { /* disable gp1_pll */
		vpu_cbus_setb(HHI_GP1_PLL_CNTL, 0, 30, 1);
	}

	return ret;
}

static int change_vpu_clk(void *vconf1)
{
	unsigned int clk_level, temp;
	struct VPU_Conf_t *vconf = (struct VPU_Conf_t *)vconf1;

	clk_level = vconf->clk_level;
	temp = (vpu_clk_table[vpu_conf.fclk_type][clk_level][1] << 9) |
		(vpu_clk_table[vpu_conf.fclk_type][clk_level][2] << 0);
	vpu_cbus_write(HHI_VPU_CLK_CNTL, ((1 << 8) | temp));

	return 0;
}

static void switch_vpu_clk_m8_g9(void)
{
	unsigned int mux, div;

	/* switch to second vpu clk patch */
	mux = vpu_clk_table[vpu_conf.fclk_type][0][1];
	vpu_cbus_setb(HHI_VPU_CLK_CNTL, mux, 25, 3);
	div = vpu_clk_table[vpu_conf.fclk_type][0][2];
	vpu_cbus_setb(HHI_VPU_CLK_CNTL, div, 16, 7);
	vpu_cbus_setb(HHI_VPU_CLK_CNTL, 1, 24, 1);
	vpu_cbus_setb(HHI_VPU_CLK_CNTL, 1, 31, 1);
	udelay(10);
	/* adjust first vpu clk frequency */
	vpu_cbus_setb(HHI_VPU_CLK_CNTL, 0, 8, 1);
	mux = vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][1];
	vpu_cbus_setb(HHI_VPU_CLK_CNTL, mux, 9, 3);
	div = vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][2];
	vpu_cbus_setb(HHI_VPU_CLK_CNTL, div, 0, 7);
	vpu_cbus_setb(HHI_VPU_CLK_CNTL, 1, 8, 1);
	udelay(20);
	/* switch back to first vpu clk patch */
	vpu_cbus_setb(HHI_VPU_CLK_CNTL, 0, 31, 1);
	vpu_cbus_setb(HHI_VPU_CLK_CNTL, 0, 24, 1);

	VPUPR("set vpu clk: %uHz, readback: %uHz(0x%x)\n",
		vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][0],
		get_vpu_clk(), (vpu_cbus_read(HHI_VPU_CLK_CNTL)));
}

/* unit: MHz */
#define VPU_CLKB_MAX    350
static void switch_vpu_clk_gx(void)
{
	unsigned int mux, div;
	unsigned int vpu_clk;

	/* switch to second vpu clk patch */
	mux = vpu_clk_table[vpu_conf.fclk_type][0][1];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, mux, 25, 3);
	div = vpu_clk_table[vpu_conf.fclk_type][0][2];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, div, 16, 7);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, 1, 24, 1);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, 1, 31, 1);
	udelay(10);
	/* adjust first vpu clk frequency */
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, 0, 8, 1);
	mux = vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][1];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, mux, 9, 3);
	div = vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][2];
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, div, 0, 7);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, 1, 8, 1);
	udelay(20);
	/* switch back to first vpu clk patch */
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, 0, 31, 1);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, 0, 24, 1);

	vpu_clk = vpu_clk_table[vpu_conf.fclk_type][vpu_conf.clk_level][0];
	if (vpu_clk >= (VPU_CLKB_MAX * 1000000))
		div = 2;
	else
		div = 1;
	vpu_hiu_setb(HHI_VPU_CLKB_CNTL_GX, (div - 1), 0, 8);
#if 0
	vpu_hiu_write(HHI_VAPBCLK_CNTL_GX,
		(1 << 30)  |  /* turn on ge2d clock */
		(0 << 9)   |  /* clk_sel    //250Mhz */
		(1 << 0));    /* clk_div */
	vpu_hiu_setb(HHI_VAPBCLK_CNTL_GX, 1, 8, 1);
#endif
	VPUPR("set vpu clk: %uHz, readback: %uHz(0x%x)\n",
		vpu_clk, get_vpu_clk(), (vpu_hiu_read(HHI_VPU_CLK_CNTL_GX)));
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
	case VPU_CHIP_M8:
		change_vpu_clk(&vpu_conf);
		break;
	case VPU_CHIP_M8M2:
		if (clk_level == (CLK_LEVEL_MAX_M8M2 - 1)) {
			ret = switch_gp_pll_m8m2(1);
			if (ret) {
				clk_level = CLK_LEVEL_MAX_M8M2 - 2;
				vpu_conf.clk_level = clk_level;
			}
		} else {
			ret = switch_gp_pll_m8m2(0);
		}
		switch_vpu_clk_m8_g9();
		break;
	case VPU_CHIP_G9TV:
		if (clk_level == (CLK_LEVEL_MAX_G9TV - 1)) {
			ret = switch_gp1_pll_g9tv(1);
			if (ret) {
				clk_level = CLK_LEVEL_MAX_G9TV - 2;
				vpu_conf.clk_level = clk_level;
			}
		} else {
			ret = switch_gp1_pll_g9tv(0);
		}
		switch_vpu_clk_m8_g9();
		break;
	case VPU_CHIP_M8B:
	case VPU_CHIP_G9BB:
		switch_vpu_clk_m8_g9();
		break;
	case VPU_CHIP_GXBB:
		switch_vpu_clk_gx();
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
	unsigned int reg;
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

	if (vpu_chip_type == VPU_CHIP_GXBB)
		reg = HHI_VPU_CLK_CNTL_GX;
	else
		reg = HHI_VPU_CLK_CNTL;

	mux = vpu_hiu_getb(reg, 9, 3);
	div = vpu_hiu_getb(reg, 0, 7);
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
 *      vmod - unsigned int, must be one of the following constants:
 *                 VMODE, VMODE is supported by VOUT
 *                 VPU_MOD, supported by vpu_mod_t
 *
 *  Returns:
 *      unsigned int, vpu clk frequency unit in Hz
 *
 *      Example:
 *      video_clk = get_vpu_clk_vmod(VMODE_720P);
 *      video_clk = get_vpu_clk_vmod(VPU_VIU_OSD1);
 *
*/
unsigned int get_vpu_clk_vmod(unsigned int vmod)
{
	unsigned int vpu_mod;
	unsigned int vpu_clk;
	mutex_lock(&vpu_mutex);

	vpu_mod = get_vpu_mod(vmod);
	if ((vpu_mod >= VPU_MOD_START) && (vpu_mod < VPU_MAX)) {
		vpu_clk = clk_vmod[vpu_mod - VPU_MOD_START];
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
 *      vmod - unsigned int, must be one of the following constants:
 *                 VMODE, VMODE is supported by VOUT
 *                 VPU_MOD, supported by vpu_mod_t
 *
 *  Returns:
 *      int, 0 for success, 1 for failed
 *
 *  Example:
 *      ret = request_vpu_clk_vmod(100000000, VMODE_720P);
 *      ret = request_vpu_clk_vmod(300000000, VPU_VIU_OSD1);
 *
*/
int request_vpu_clk_vmod(unsigned int vclk, unsigned int vmod)
{
	int ret = 0;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	unsigned clk_level;
	unsigned vpu_mod;

	mutex_lock(&vpu_mutex);

	if (vclk >= 100) { /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	} else { /* regard as clk_level */
		clk_level = vclk;
	}

	if (clk_level >= vpu_conf.clk_level_max) {
		ret = 1;
		VPUERR("set vpu clk out of supported range\n");
		goto request_vpu_clk_limit;
	}

	vpu_mod = get_vpu_mod(vmod);
	if (vpu_mod == VPU_MAX) {
		ret = 2;
		VPUERR("unsupport vmod\n");
		goto request_vpu_clk_limit;
	}

	clk_vmod[vpu_mod - VPU_MOD_START] = clk_level;
	if (vpu_debug_print_flag) {
		VPUPR("request vpu clk: %s %uHz\n",
			vpu_mod_table[vpu_mod - VPU_MOD_START],
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
 *      vmod - unsigned int, must be one of the following constants:
 *                 VMODE, VMODE is supported by VOUT
 *                 VPU_MOD, supported by vpu_mod_t
 *
 *  Returns:
 *      int, 0 for success, 1 for failed
 *
 *  Example:
 *      ret = release_vpu_clk_vmod(VMODE_720P);
 *      ret = release_vpu_clk_vmod(VPU_VIU_OSD1);
 *
*/
int release_vpu_clk_vmod(unsigned int vmod)
{
	int ret = 0;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	unsigned clk_level;
	unsigned vpu_mod;

	mutex_lock(&vpu_mutex);

	clk_level = 0;
	vpu_mod = get_vpu_mod(vmod);
	if (vpu_mod == VPU_MAX) {
		ret = 2;
		VPUERR("unsupport vmod\n");
		goto release_vpu_clk_limit;
	}

	clk_vmod[vpu_mod - VPU_MOD_START] = clk_level;
	if (vpu_debug_print_flag) {
		VPUPR("release vpu clk: %s\n",
			vpu_mod_table[vpu_mod - VPU_MOD_START]);
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
/* VPU_MEM_PD control */
/* *********************************************** */
/*
 *  Function: switch_vpu_mem_pd_vmod
 *      switch vpu memory power down by specified vmod
 *
 *  Parameters:
 *      vmod - unsigned int, must be one of the following constants:
 *                 VMODE, VMODE is supported by VOUT
 *                 VPU_MOD, supported by vpu_mod_t
 *      flag - int, on/off switch flag, must be one of the following constants:
 *                 VPU_MEM_POWER_ON
 *                 VPU_MEM_POWER_DOWN
 *
 *  Example:
 *      switch_vpu_mem_pd_vmod(VMODE_720P, VPU_MEM_POWER_ON);
 *      switch_vpu_mem_pd_vmod(VPU_VIU_OSD1, VPU_MEM_POWER_DOWN);
 *
*/
void switch_vpu_mem_pd_vmod(unsigned int vmod, int flag)
{
	unsigned vpu_mod;
	unsigned long flags = 0;
	unsigned int _reg0;
	unsigned int _reg1;
	unsigned int val;

	if (vpu_chip_type == VPU_CHIP_MAX) {
		VPUPR("invalid VPU in current CPU type\n");
		return;
	}

	spin_lock_irqsave(&vpu_mem_lock, flags);

	val = (flag == VPU_MEM_POWER_ON) ? 0 : 3;
	if (vpu_chip_type == VPU_CHIP_GXBB) {
		_reg0 = HHI_VPU_MEM_PD_REG0_GX;
		_reg1 = HHI_VPU_MEM_PD_REG1_GX;
	} else {
		_reg0 = HHI_VPU_MEM_PD_REG0;
		_reg1 = HHI_VPU_MEM_PD_REG1;
	}

	vpu_mod = get_vpu_mod(vmod);
	switch (vpu_mod) {
	case VPU_VIU_OSD1:
		vpu_hiu_setb(_reg0, val, 0, 2);
		break;
	case VPU_VIU_OSD2:
		vpu_hiu_setb(_reg0, val, 2, 2);
		break;
	case VPU_VIU_VD1:
		vpu_hiu_setb(_reg0, val, 4, 2);
		break;
	case VPU_VIU_VD2:
		vpu_hiu_setb(_reg0, val, 6, 2);
		break;
	case VPU_VIU_CHROMA:
		vpu_hiu_setb(_reg0, val, 8, 2);
		break;
	case VPU_VIU_OFIFO:
		vpu_hiu_setb(_reg0, val, 10, 2);
		break;
	case VPU_VIU_SCALE:
		vpu_hiu_setb(_reg0, val, 12, 2);
		break;
	case VPU_VIU_OSD_SCALE:
		vpu_hiu_setb(_reg0, val, 14, 2);
		break;
	case VPU_VIU_VDIN0:
		vpu_hiu_setb(_reg0, val, 16, 2);
		break;
	case VPU_VIU_VDIN1:
		vpu_hiu_setb(_reg0, val, 18, 2);
		break;
	case VPU_PIC_ROT1:
	case VPU_VIU_SRSCL: /* G9TV only */
		vpu_hiu_setb(_reg0, val, 20, 2);
		break;
	case VPU_PIC_ROT2:
	case VPU_VIU_OSDSR: /* G9TV only */
		vpu_hiu_setb(_reg0, val, 22, 2);
		break;
	case VPU_PIC_ROT3:
	case VPU_REV: /* G9TV only */
		vpu_hiu_setb(_reg0, val, 24, 2);
		break;
	case VPU_DI_PRE:
		vpu_hiu_setb(_reg0, val, 26, 2);
		break;
	case VPU_DI_POST:
		vpu_hiu_setb(_reg0, val, 28, 2);
		break;
	case VPU_SHARP: /* G9TV & G9BB only */
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_G9BB)) {
			vpu_hiu_setb(_reg0, val, 30, 2);
		}
		break;
	case VPU_VIU2_OSD1:
		vpu_hiu_setb(_reg1, val, 0, 2);
		break;
	case VPU_VIU2_OSD2:
		vpu_hiu_setb(_reg1, val, 2, 2);
		break;
	case VPU_D2D3: /* G9TV only */
		if (vpu_chip_type == VPU_CHIP_G9TV)
			vpu_hiu_setb(_reg1, ((val << 2) | val), 0, 4);
		break;
	case VPU_VIU2_VD1:
		vpu_hiu_setb(_reg1, val, 4, 2);
		break;
	case VPU_VIU2_CHROMA:
		vpu_hiu_setb(_reg1, val, 6, 2);
		break;
	case VPU_VIU2_OFIFO:
		vpu_hiu_setb(_reg1, val, 8, 2);
		break;
	case VPU_VIU2_SCALE:
		vpu_hiu_setb(_reg1, val, 10, 2);
		break;
	case VPU_VIU2_OSD_SCALE:
		vpu_hiu_setb(_reg1, val, 12, 2);
		break;
	case VPU_VDIN_AM_ASYNC: /* G9TV only */
	case VPU_VPU_ARB: /* GXBB only */
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_GXBB)) {
			vpu_hiu_setb(_reg1, val, 14, 2);
		}
		break;
	case VPU_VDISP_AM_ASYNC: /* G9TV only */
	case VPU_AFBC_DEC: /* GXBB only */
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_GXBB)) {
			vpu_hiu_setb(_reg1, val, 16, 2);
		}
		break;
	case VPU_VPUARB2_AM_ASYNC: /* G9TV only */
		if (vpu_chip_type == VPU_CHIP_G9TV)
			vpu_hiu_setb(_reg1, val, 18, 2);
		break;
	case VPU_VENCP:
		vpu_hiu_setb(_reg1, val, 20, 2);
		break;
	case VPU_VENCL:
		vpu_hiu_setb(_reg1, val, 22, 2);
		break;
	case VPU_VENCI:
		vpu_hiu_setb(_reg1, val, 24, 2);
		break;
	case VPU_ISP:
		vpu_hiu_setb(_reg1, val, 26, 2);
		break;
	case VPU_CVD2: /* G9TV & G9BB only */
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_G9BB)) {
			vpu_hiu_setb(_reg1, val, 28, 2);
		}
		break;
	case VPU_ATV_DMD: /* G9TV & G9BB only */
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_G9BB)) {
			vpu_hiu_setb(_reg1, val, 30, 2);
		}
		break;
	default:
		VPUPR("switch_vpu_mem_pd: unsupport vpu mod\n");
		break;
	}

	if (vpu_debug_print_flag) {
		VPUPR("switch_vpu_mem_pd: %s %s\n",
			vpu_mod_table[vpu_mod - VPU_MOD_START],
			((flag > 0) ? "OFF" : "ON"));
		dump_stack();
	}
	spin_unlock_irqrestore(&vpu_mem_lock, flags);
}
/* *********************************************** */

#define VPU_MEM_PD_ERR        0xffff
int get_vpu_mem_pd_vmod(unsigned int vmod)
{
	unsigned int vpu_mod;
	unsigned int _reg0;
	unsigned int _reg1;
	unsigned int val;

	if (vpu_chip_type == VPU_CHIP_MAX) {
		VPUPR("invalid VPU in current CPU type\n");
		return 0;
	}

	if (vpu_chip_type == VPU_CHIP_GXBB) {
		_reg0 = HHI_VPU_MEM_PD_REG0_GX;
		_reg1 = HHI_VPU_MEM_PD_REG1_GX;
	} else {
		_reg0 = HHI_VPU_MEM_PD_REG0;
		_reg1 = HHI_VPU_MEM_PD_REG1;
	}

	vpu_mod = get_vpu_mod(vmod);
	switch (vpu_mod) {
	case VPU_VIU_OSD1:
		val = vpu_hiu_getb(_reg0, 0, 2);
		break;
	case VPU_VIU_OSD2:
		val = vpu_hiu_getb(_reg0, 2, 2);
		break;
	case VPU_VIU_VD1:
		val = vpu_hiu_getb(_reg0, 4, 2);
		break;
	case VPU_VIU_VD2:
		val = vpu_hiu_getb(_reg0, 6, 2);
		break;
	case VPU_VIU_CHROMA:
		val = vpu_hiu_getb(_reg0, 8, 2);
		break;
	case VPU_VIU_OFIFO:
		val = vpu_hiu_getb(_reg0, 10, 2);
		break;
	case VPU_VIU_SCALE:
		val = vpu_hiu_getb(_reg0, 12, 2);
		break;
	case VPU_VIU_OSD_SCALE:
		val = vpu_hiu_getb(_reg0, 14, 2);
		break;
	case VPU_VIU_VDIN0:
		val = vpu_hiu_getb(_reg0, 16, 2);
		break;
	case VPU_VIU_VDIN1:
		val = vpu_hiu_getb(_reg0, 18, 2);
		break;
	case VPU_PIC_ROT1:
	case VPU_VIU_SRSCL: /* G9TV only */
		val = vpu_hiu_getb(_reg0, 20, 2);
		break;
	case VPU_PIC_ROT2:
	case VPU_VIU_OSDSR: /* G9TV only */
		val = vpu_hiu_getb(_reg0, 22, 2);
		break;
	case VPU_PIC_ROT3:
	case VPU_REV: /* G9TV only */
		val = vpu_hiu_getb(_reg0, 24, 2);
		break;
	case VPU_DI_PRE:
		val = vpu_hiu_getb(_reg0, 26, 2);
		break;
	case VPU_DI_POST:
		val = vpu_hiu_getb(_reg0, 28, 2);
		break;
	case VPU_SHARP: /* G9TV & G9BB only */
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_G9BB)) {
			val = vpu_hiu_getb(_reg0, 30, 2);
		} else {
			val = VPU_MEM_PD_ERR;
		}
		break;
	case VPU_VIU2_OSD1:
		val = vpu_hiu_getb(_reg1, 0, 2);
		break;
	case VPU_VIU2_OSD2:
		val = vpu_hiu_getb(_reg1, 2, 2);
		break;
	case VPU_D2D3: /* G9TV only */
		if (vpu_chip_type == VPU_CHIP_G9TV)
			val = vpu_hiu_getb(_reg1, 0, 4);
		else
			val = VPU_MEM_PD_ERR;
		break;
	case VPU_VIU2_VD1:
		val = vpu_hiu_getb(_reg1, 4, 2);
		break;
	case VPU_VIU2_CHROMA:
		val = vpu_hiu_getb(_reg1, 6, 2);
		break;
	case VPU_VIU2_OFIFO:
		val = vpu_hiu_getb(_reg1, 8, 2);
		break;
	case VPU_VIU2_SCALE:
		val = vpu_hiu_getb(_reg1, 10, 2);
		break;
	case VPU_VIU2_OSD_SCALE:
		val = vpu_hiu_getb(_reg1, 12, 2);
		break;
	case VPU_VDIN_AM_ASYNC: /* G9TV only */
	case VPU_VPU_ARB: /* GXBB only */
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_GXBB)) {
			val = vpu_hiu_getb(_reg1, 14, 2);
		} else {
			val = VPU_MEM_PD_ERR;
		}
		break;
	case VPU_VDISP_AM_ASYNC: /* G9TV only */
	case VPU_AFBC_DEC: /* GXBB only */
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_GXBB)) {
			val = vpu_hiu_getb(_reg1, 16, 2);
		} else {
			val = VPU_MEM_PD_ERR;
		}
		break;
	case VPU_VPUARB2_AM_ASYNC: /* G9TV only */
		if (vpu_chip_type == VPU_CHIP_G9TV)
			val = vpu_hiu_getb(_reg0, 18, 2);
		else
			val = VPU_MEM_PD_ERR;
		break;
	case VPU_VENCP:
		val = vpu_hiu_getb(_reg1, 20, 2);
		break;
	case VPU_VENCL:
		val = vpu_hiu_getb(_reg1, 22, 2);
		break;
	case VPU_VENCI:
		val = vpu_hiu_getb(_reg1, 24, 2);
		break;
	case VPU_ISP:
		val = vpu_hiu_getb(_reg1, 26, 2);
		break;
	case VPU_CVD2: /* G9TV & G9BB only */
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_G9BB)) {
			val = vpu_hiu_getb(_reg1, 28, 2);
		} else {
			val = VPU_MEM_PD_ERR;
		}
		break;
	case VPU_ATV_DMD: /* G9TV & G9BB only */
		if ((vpu_chip_type == VPU_CHIP_G9TV) ||
			(vpu_chip_type == VPU_CHIP_G9BB)) {
			val = vpu_hiu_getb(_reg1, 30, 2);
		} else {
			val = VPU_MEM_PD_ERR;
		}
		break;
	default:
		val = VPU_MEM_PD_ERR;
		break;
	}

	if (val == 0)
		return VPU_MEM_POWER_ON;
	else if ((val == 0x3) || (val == 0xf))
		return VPU_MEM_POWER_DOWN;
	else
		return -1;
}

/* *********************************************** */
/* VPU sysfs function */
/* *********************************************** */
static const char *vpu_usage_str = {
"Usage:\n"
"	echo r > mem ; read vpu memory power down status\n"
"	echo w <vmod> <mpd> > mem ; write vpu memory power down\n"
"	<mpd>: 0=power up, 1=power down\n"
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
	int i;
	unsigned int tmp[2], temp;
	unsigned int fclk_type;

	fclk_type = vpu_conf.fclk_type;
	switch (buf[0]) {
	case 'g': /* get */
		pr_info("get current vpu clk: %uHz\n", get_vpu_clk());
		break;
	case 's': /* set */
		tmp[0] = 4;
		ret = sscanf(buf, "set %u", &tmp[0]);
		if (tmp[0] > 100)
			pr_info("set vpu clk frequency: %uHz\n", tmp[0]);
		else
			pr_info("set vpu clk level: %u\n", tmp[0]);
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
		tmp[1] = get_vpu_mod(tmp[0]);
		pr_info("vpu clk holdings:\n");
		if (tmp[1] == VPU_MAX) {
			for (i = VPU_MOD_START; i < VPU_MAX; i++) {
				temp = i - VPU_MOD_START;
				pr_info("%s:  %uHz(%u)\n", vpu_mod_table[temp],
				vpu_clk_table[fclk_type][clk_vmod[temp]][0],
				clk_vmod[temp]);
			}
		} else {
			temp = tmp[1] - VPU_MOD_START;
			pr_info("%s:  %uHz(%u)\n", vpu_mod_table[temp],
			vpu_clk_table[fclk_type][clk_vmod[temp]][0],
			clk_vmod[temp]);
		}
		break;
	default:
		pr_info("wrong format of vpu debug command.\n");
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
	unsigned int tmp[2], temp;
	unsigned int _reg0, _reg1;

	if (vpu_chip_type == VPU_CHIP_GXBB) {
		_reg0 = HHI_VPU_MEM_PD_REG0_GX;
		_reg1 = HHI_VPU_MEM_PD_REG1_GX;
	} else {
		_reg0 = HHI_VPU_MEM_PD_REG0;
		_reg1 = HHI_VPU_MEM_PD_REG1;
	}
	switch (buf[0]) {
	case 'r':
		pr_info("vpu mem_pd0: 0x%08x\n", vpu_hiu_read(_reg0));
		pr_info("vpu mem_pd1: 0x%08x\n", vpu_hiu_read(_reg1));
		break;
	case 'w':
		ret = sscanf(buf, "w %u %u", &tmp[0], &tmp[1]);
		temp = (tmp[1] > 0) ? VPU_MEM_POWER_DOWN : VPU_MEM_POWER_ON;
		switch_vpu_mem_pd_vmod(tmp[0], temp);
		break;
	default:
		pr_info("wrong format of vpu mem command\n");
		break;
	}

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

static struct class_attribute vpu_debug_class_attrs[] = {
	__ATTR(clk, S_IRUGO | S_IWUSR, vpu_debug_help, vpu_clk_debug),
	__ATTR(mem, S_IRUGO | S_IWUSR, vpu_debug_help, vpu_mem_debug),
	__ATTR(print, S_IRUGO | S_IWUSR, vpu_debug_help, vpu_print_debug),
	__ATTR(help, S_IRUGO | S_IWUSR, vpu_debug_help, NULL),
	__ATTR_NULL
};

static struct class aml_vpu_debug_class = {
	.name = "vpu",
	.class_attrs = vpu_debug_class_attrs,
};
/* ********************************************************* */
#if 0
static void vpu_power_on_m8_g9(void)
{
	unsigned int i;

	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 8, 1); /* [8] power on */
	/* power up memories */
	for (i = 0; i < 32; i += 2) {
		vpu_hiu_setb(HHI_VPU_MEM_PD_REG0, 0, i, 2);
		udelay(2);
	}
	for (i = 0; i < 32; i += 2) {
		vpu_hiu_setb(HHI_VPU_MEM_PD_REG1, 0, i, 2);
		udelay(2);
	}
	for (i = 8; i < 16; i++) {
		vpu_hiu_setb(HHI_MEM_PD_REG0, 0, i, 1);
		udelay(2);
	}
	udelay(2);

	/* Reset VIU + VENC */
	/* Reset VENCI + VENCP + VADC + VENCL */
	/* Reset HDMI-APB + HDMI-SYS + HDMI-TX + HDMI-CEC */
	vpu_cbus_clr_mask(RESET0_MASK, ((1<<5) | (1<<10)));
	vpu_cbus_clr_mask(RESET4_MASK, ((1<<6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_cbus_clr_mask(RESET2_MASK, ((1<<2) | (1<<3) | (1<<11) | (1<<15)));
	vpu_cbus_write(RESET2_REGISTER, ((1<<2) | (1<<3) | (1<<11) | (1<<15)));
	/* reset this will cause VBUS reg to 0 */
	vpu_cbus_write(RESET4_REGISTER, ((1<<6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_cbus_write(RESET0_REGISTER, ((1<<5) | (1<<10)));
	vpu_cbus_write(RESET4_REGISTER, ((1<<6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_cbus_write(RESET2_REGISTER, ((1<<2) | (1<<3) | (1<<11) | (1<<15)));
	vpu_cbus_set_mask(RESET0_MASK, ((1<<5) | (1<<10)));
	vpu_cbus_set_mask(RESET4_MASK, ((1<<6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_cbus_set_mask(RESET2_MASK, ((1<<2) | (1<<3) | (1<<11) | (1<<15)));

	/* Remove VPU_HDMI ISO */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 9, 1); /* [9] VPU_HDMI */
}

static void vpu_power_on_gx(void)
{
	unsigned int i;

	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 8, 1); /* [8] power on */
	/* power up memories */
	for (i = 0; i < 32; i += 2) {
		vpu_hiu_setb(HHI_VPU_MEM_PD_REG0_GX, 0, i, 2);
		udelay(2);
	}
	for (i = 0; i < 32; i += 2) {
		vpu_hiu_setb(HHI_VPU_MEM_PD_REG1_GX, 0, i, 2);
		udelay(2);
	}
	for (i = 8; i < 16; i++) {
		vpu_hiu_setb(HHI_MEM_PD_REG0_GX, 0, i, 1);
		udelay(2);
	}
	udelay(2);

	/* Reset VIU + VENC */
	/* Reset VENCI + VENCP + VADC + VENCL */
	/* Reset HDMI-APB + HDMI-SYS + HDMI-TX + HDMI-CEC */
	vpu_cbus_clr_mask(RESET0_MASK, ((1<<5) | (1<<10)));
	vpu_cbus_clr_mask(RESET4_MASK, ((1<<6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_cbus_clr_mask(RESET2_MASK, ((1<<2) | (1<<3) | (1<<11) | (1<<15)));
	vpu_cbus_write(RESET2_REGISTER, ((1<<2) | (1<<3) | (1<<11) | (1<<15)));
	/* reset this will cause VBUS reg to 0 */
	vpu_cbus_write(RESET4_REGISTER, ((1<<6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_cbus_write(RESET0_REGISTER, ((1 << 5) | (1<<10)));
	vpu_cbus_write(RESET4_REGISTER, ((1<<6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_cbus_write(RESET2_REGISTER, ((1<<2) | (1<<3) | (1<<11) | (1<<15)));
	vpu_cbus_set_mask(RESET0_MASK, ((1 << 5) | (1<<10)));
	vpu_cbus_set_mask(RESET4_MASK, ((1<<6) | (1<<7) | (1<<9) | (1<<13)));
	vpu_cbus_set_mask(RESET2_MASK, ((1<<2) | (1<<3) | (1<<11) | (1<<15)));

	/* Remove VPU_HDMI ISO */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 0, 9, 1); /* [9] VPU_HDMI */
}

static void vpu_power_off_m8_g9(void)
{
	unsigned int i;

	/* Power down VPU_HDMI */
	/* Enable Isolation */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 1, 9, 1); /* ISO */
	/* power down memories */
	for (i = 0; i < 32; i += 2) {
		vpu_hiu_setb(HHI_VPU_MEM_PD_REG0, 0x3, i, 2);
		udelay(2);
	}
	for (i = 0; i < 32; i += 2) {
		vpu_hiu_setb(HHI_VPU_MEM_PD_REG1, 0x3, i, 2);
		udelay(2);
	}
	for (i = 8; i < 16; i++) {
		vpu_hiu_setb(HHI_MEM_PD_REG0, 0x1, i, 1);
		udelay(2);
	}
	udelay(2);

	/* Power down VPU domain */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 1, 8, 1); /* PDN */

	vpu_hiu_setb(HHI_VPU_CLK_CNTL, 0, 8, 1);
}

static void vpu_power_off_gx(void)
{
	unsigned int i;

	/* Power down VPU_HDMI */
	/* Enable Isolation */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 1, 9, 1); /* ISO */
	/* power down memories */
	for (i = 0; i < 32; i += 2) {
		vpu_hiu_setb(HHI_VPU_MEM_PD_REG0_GX, 0x3, i, 2);
		udelay(2);
	}
	for (i = 0; i < 32; i += 2) {
		vpu_hiu_setb(HHI_VPU_MEM_PD_REG1_GX, 0x3, i, 2);
		udelay(2);
	}
	for (i = 8; i < 16; i++) {
		vpu_hiu_setb(HHI_MEM_PD_REG0_GX, 0x1, i, 1);
		udelay(2);
	}
	udelay(2);

	/* Power down VPU domain */
	vpu_ao_setb(AO_RTI_GEN_PWR_SLEEP0, 1, 8, 1); /* PDN */

	vpu_hiu_setb(HHI_VAPBCLK_CNTL_GX, 0, 8, 1);
	vpu_hiu_setb(HHI_VPU_CLKB_CNTL_GX, 0, 8, 1);
	vpu_hiu_setb(HHI_VPU_CLK_CNTL_GX, 0, 8, 1);
}

static void vpu_power_on(void)
{
	if (vpu_chip_type == VPU_CHIP_GXBB)
		vpu_power_on_gx();
	else
		vpu_power_on_m8_g9();
}

static void vpu_power_off(void)
{
	if (vpu_chip_type == VPU_CHIP_GXBB)
		vpu_power_off_gx();
	else
		vpu_power_off_m8_g9();
}
#endif

#ifdef CONFIG_PM
static int vpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* vpu_driver_disable(); */
	return 0;
}

static int vpu_resume(struct platform_device *pdev)
{
	/* vpu_driver_init(); */
	return 0;
}
#endif

static void detect_vpu_chip(void)
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
	default:
		vpu_chip_type = VPU_CHIP_MAX;
		vpu_conf.clk_level_dft = 0;
		vpu_conf.clk_level_max = 1;
	}

	if (vpu_debug_print_flag) {
		VPUPR("vpu driver detect cpu type: %s\n",
			vpu_chip_name[vpu_chip_type]);
	}
}

static int get_vpu_config(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int val;
	struct device_node *vpu_np;

	vpu_np = pdev->dev.of_node;
	if (!vpu_np) {
		VPUERR("don't find match vpu node\n");
		return -1;
	}

	ret = of_property_read_u32(vpu_np, "clk_level", &val);
	if (ret) {
		VPUPR("don't find to match clk_level, use default setting\n");
	} else {
		if (val >= vpu_conf.clk_level_max) {
			VPUERR("clk_level in dts is out of support range\n");
			val = vpu_conf.clk_level_dft;
		}
		vpu_conf.clk_level = val;
		VPUPR("load vpu_clk: %uHz(%u)\n",
			vpu_clk_table[vpu_conf.fclk_type][val][0],
			vpu_conf.clk_level);
	}

	return ret;
}

static struct of_device_id vpu_of_table[] = {
	{
		.compatible = "amlogic, vpu",
	},
	{},
};

static int vpu_probe(struct platform_device *pdev)
{
	int ret;

#ifdef VPU_DEBUG_PRINT
	vpu_debug_print_flag = 1;
#else
	vpu_debug_print_flag = 0;
#endif
	spin_lock_init(&vpu_lock);
	spin_lock_init(&vpu_mem_lock);

	VPUPR("VPU driver version: %s\n", VPU_VERION);
	memset(clk_vmod, 0, sizeof(clk_vmod));
	detect_vpu_chip();
	if (vpu_chip_type == VPU_CHIP_MAX) {
		VPUPR("invalid VPU in current CPU type\n");
		return 0;
	}
	vpu_ioremap();
	get_vpu_config(pdev);
	set_vpu_clk(vpu_conf.clk_level);
#if 0
	vpu_power_on();
#endif

	ret = class_register(&aml_vpu_debug_class);
	if (ret)
		VPUPR("class register aml_vpu_debug_class fail\n");

	VPUPR("%s OK\n", __func__);
	return 0;
}

static int vpu_remove(struct platform_device *pdev)
{
#if 0
	vpu_power_off();
#endif
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
