/*
 * drivers/amlogic/amports/arch/clk.h
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

#ifndef VDEC_CHIP_CLK_HEADER
#define VDEC_CHIP_CLK_HEADER
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>

struct chip_vdec_clk_s {
	int (*clock_level)(enum vdec_type_e core);
	int (*clock_init)(void);
	void (*clock_enable)(void);
	void (*clock_hi_enable)(void);
	void (*clock_superhi_enable)(void);
	void (*clock_on)(void);
	void (*clock_off)(void);
	void (*clock_prepare_switch)(void);
};

#ifndef INCLUDE_FROM_ARCH_CLK_MGR
int vdec_clock_init(void);
void vdec_clock_enable(void);
void vdec_clock_hi_enable(void);
void vdec_clock_superhi_enable(void);
void vdec2_clock_enable(void);
void vdec2_clock_hi_enable(void);
void hcodec_clock_enable(void);
int hevc_clock_init(void);
void hevc_clock_enable(void);
void hevc_clock_hi_enable(void);
void hevc_clock_superhi_enable(void);

void vdec_clock_on(void);
void vdec_clock_off(void);
void vdec2_clock_on(void);

void vdec2_clock_off(void);
void hcodec_clock_on(void);
void hcodec_clock_off(void);
void hevc_clock_on(void);
void hevc_clock_off(void);
int vdec_clock_level(enum vdec_type_e core);
void vdec_clock_prepare_switch(void);
void hevc_clock_prepare_switch(void);
#endif
int register_vdec_clk_mgr(int cputype[],
			enum vdec_type_e vdec_type,
			struct chip_vdec_clk_s *t_mgr);


#ifdef INCLUDE_FROM_ARCH_CLK_MGR
static struct chip_vdec_clk_s vdec_clk_mgr __initdata = {
	.clock_init = vdec_clock_init,
	.clock_enable = vdec_clock_enable,
	.clock_hi_enable = vdec_clock_hi_enable,
	.clock_superhi_enable = vdec_clock_superhi_enable,
	.clock_on = vdec_clock_on,
	.clock_off = vdec_clock_off,
	.clock_prepare_switch = vdec_clock_prepare_switch,
	.clock_level = vdec_clock_level,
};


#ifdef VDEC_HAS_VDEC2
static struct chip_vdec_clk_s vdec2_clk_mgr __initdata = {
	.clock_enable = vdec2_clock_enable,
	.clock_hi_enable = vdec2_clock_hi_enable,
	.clock_on = vdec2_clock_on,
	.clock_off = vdec2_clock_off,
	.clock_prepare_switch = NULL,
	.clock_level = vdec_clock_level,
};
#endif

#ifdef VDEC_HAS_HEVC
static struct chip_vdec_clk_s vdec_hevc_clk_mgr __initdata = {
	.clock_init = hevc_clock_init,
	.clock_enable = hevc_clock_enable,
	.clock_hi_enable = hevc_clock_hi_enable,
	.clock_superhi_enable = hevc_clock_superhi_enable,
	.clock_on = hevc_clock_on,
	.clock_off = hevc_clock_off,
	.clock_prepare_switch = hevc_clock_prepare_switch,
	.clock_level = vdec_clock_level,
};
#endif

#ifdef VDEC_HAS_VDEC_HCODEC
static struct chip_vdec_clk_s vdec_hcodec_clk_mgr __initdata = {
	.clock_enable = hcodec_clock_enable,
	.clock_hi_enable = NULL,
	.clock_on = hcodec_clock_on,
	.clock_off = hcodec_clock_off,
	.clock_prepare_switch = NULL,
	.clock_level = vdec_clock_level,
};
#endif

static int __init vdec_init_clk(void)
{
	int cpus[] = CLK_FOR_CPU;
	register_vdec_clk_mgr(cpus, VDEC_1, &vdec_clk_mgr);
#ifdef VDEC_HAS_VDEC2
	register_vdec_clk_mgr(cpus, VDEC_2, &vdec2_clk_mgr);
#endif
#ifdef VDEC_HAS_HEVC
	register_vdec_clk_mgr(cpus, VDEC_HEVC, &vdec_hevc_clk_mgr);
#endif
#ifdef VDEC_HAS_VDEC_HCODEC
	register_vdec_clk_mgr(cpus, VDEC_HCODEC, &vdec_hcodec_clk_mgr);
#endif
	return 0;
}

#define ARCH_VDEC_CLK_INIT()\
		module_init(vdec_init_clk);
#endif
#endif
