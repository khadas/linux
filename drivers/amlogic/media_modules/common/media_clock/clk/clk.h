/*
 * drivers/amlogic/media/common/arch/clk/clk.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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
#include "clk_priv.h"
#include <linux/amlogic/media/clk/gp_pll.h>

#ifndef INCLUDE_FROM_ARCH_CLK_MGR
int vdec_clock_init(void);
int vdec_clock_set(int clk);
int vdec2_clock_set(int clk);

int hcodec_clock_set(int clk);
int hevc_clock_init(void);
int hevc_clock_set(int clk);

void vdec_clock_on(void);
void vdec_clock_off(void);
void vdec2_clock_on(void);

void vdec2_clock_off(void);
void hcodec_clock_on(void);
void hcodec_clock_off(void);
void hevc_clock_on(void);
void hevc_clock_off(void);

int hevc_back_clock_init(void);
void hevc_back_clock_on(void);
void hevc_back_clock_off(void);
int hevc_back_clock_set(int clk);
void hevc_back_clock_enable(void);
void hevc_back_clock_hi_enable(void);

int vdec_source_get(enum vdec_type_e core);
int vdec_clk_get(enum vdec_type_e core);

int vdec_source_changed_for_clk_set(int format, int width, int height, int fps);
int get_clk_with_source(int format, int w_x_h_fps);

void vdec_clock_enable(void);
void vdec_clock_hi_enable(void);
void hcodec_clock_enable(void);
void hcodec_clock_hi_enable(void);
void hevc_clock_enable(void);
void hevc_clock_hi_enable(void);
void vdec2_clock_enable(void);
void vdec2_clock_hi_enable(void);
void set_clock_gate(struct gate_switch_node *nodes, int num);

#endif
int register_vdec_clk_mgr(int cputype[],
	enum vdec_type_e vdec_type, struct chip_vdec_clk_s *t_mgr);

int unregister_vdec_clk_mgr(enum vdec_type_e vdec_type);

int register_vdec_clk_setting(int cputype[],
	struct clk_set_setting *p_seting, int size);

int unregister_vdec_clk_setting(void);

#ifdef INCLUDE_FROM_ARCH_CLK_MGR
static struct chip_vdec_clk_s vdec_clk_mgr __initdata = {
	.clock_init = vdec_clock_init,
	.clock_set = vdec_clock_set,
	.clock_on = vdec_clock_on,
	.clock_off = vdec_clock_off,
	.clock_get = vdec_clock_get,
};

#ifdef VDEC_HAS_VDEC2
static struct chip_vdec_clk_s vdec2_clk_mgr __initdata = {
	.clock_set = vdec2_clock_set,
	.clock_on = vdec2_clock_on,
	.clock_off = vdec2_clock_off,
	.clock_get = vdec_clock_get,
};
#endif

#ifdef VDEC_HAS_HEVC
static struct chip_vdec_clk_s vdec_hevc_clk_mgr __initdata = {
	.clock_init = hevc_clock_init,
	.clock_set = hevc_clock_set,
	.clock_on = hevc_clock_on,
	.clock_off = hevc_clock_off,
	.clock_get = vdec_clock_get,
};
static struct chip_vdec_clk_s vdec_hevc_back_clk_mgr __initdata = {
        .clock_init = hevc_back_clock_init,
        .clock_set = hevc_back_clock_set,
        .clock_on = hevc_back_clock_on,
        .clock_off = hevc_back_clock_off,
        .clock_get = vdec_clock_get,
};
#endif

#ifdef VDEC_HAS_VDEC_HCODEC
static struct chip_vdec_clk_s vdec_hcodec_clk_mgr __initdata = {
	.clock_set = hcodec_clock_set,
	.clock_on = hcodec_clock_on,
	.clock_off = hcodec_clock_off,
	.clock_get = vdec_clock_get,
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
	if (get_cpu_major_id() >= MESON_CPU_MAJOR_ID_G12A)
		register_vdec_clk_mgr(cpus, VDEC_HEVCB, &vdec_hevc_back_clk_mgr);
#endif
#ifdef VDEC_HAS_VDEC_HCODEC
	register_vdec_clk_mgr(cpus, VDEC_HCODEC, &vdec_hcodec_clk_mgr);
#endif

#ifdef VDEC_HAS_CLK_SETTINGS
	register_vdec_clk_setting(cpus,
		clks_for_formats, sizeof(clks_for_formats));
#endif
	return 0;
}

static void __exit vdec_clk_exit(void)
{
		unregister_vdec_clk_mgr(VDEC_1);
#ifdef VDEC_HAS_VDEC2
		unregister_vdec_clk_mgr(VDEC_2);
#endif
#ifdef VDEC_HAS_HEVC
		unregister_vdec_clk_mgr(VDEC_HEVC);
#endif
#ifdef VDEC_HAS_VDEC_HCODEC
		unregister_vdec_clk_mgr(VDEC_HCODEC);
#endif
#ifdef VDEC_HAS_CLK_SETTINGS
		unregister_vdec_clk_setting();
#endif
		pr_info("media clock exit.\n");
}

#define ARCH_VDEC_CLK_INIT()\
		module_init(vdec_init_clk)

#define ARCH_VDEC_CLK_EXIT()\
		module_exit(vdec_clk_exit)

#endif
#endif
