/*
 * drivers/amlogic/amports/arch/clk.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include <linux/amlogic/amports/vformat.h>
#include <linux/amlogic/cpu_version.h>
#include "../amports_priv.h"
#include "../vdec.h"
#include "chips.h"

#define p_vdec() (get_current_vdec_chip()->clk_mgr[VDEC_1])
#define p_vdec2() (get_current_vdec_chip()->clk_mgr[VDEC_2])
#define p_vdec_hcodec() (get_current_vdec_chip()->clk_mgr[VDEC_HCODEC])
#define p_vdec_hevc() (get_current_vdec_chip()->clk_mgr[VDEC_HEVC])

#define IF_HAVE_RUN(p, fn)\
	do {\
		if (p && p->fn)\
			p->fn();\
	} while (0)

void vdec_clock_enable(void)
{
	IF_HAVE_RUN(p_vdec(), clock_enable);
}

void vdec_clock_hi_enable(void)
{
	IF_HAVE_RUN(p_vdec(), clock_hi_enable);
}

void vdec_clock_on(void)
{
	IF_HAVE_RUN(p_vdec(), clock_on);
}

void vdec_clock_off(void)
{
	IF_HAVE_RUN(p_vdec(), clock_off);
}

void vdec2_clock_enable(void)
{
	IF_HAVE_RUN(p_vdec2(), clock_enable);
}

void vdec2_clock_hi_enable(void)
{
	IF_HAVE_RUN(p_vdec2(), clock_hi_enable);
}

void vdec2_clock_on(void)
{
	IF_HAVE_RUN(p_vdec2(), clock_on);
}

void vdec2_clock_off(void)
{
	IF_HAVE_RUN(p_vdec2(), clock_off);
}

void hcodec_clock_enable(void)
{
	IF_HAVE_RUN(p_vdec_hcodec(), clock_enable);
}

void hcodec_clock_on(void)
{
	IF_HAVE_RUN(p_vdec_hcodec(), clock_on);
}

void hcodec_clock_off(void)
{
	IF_HAVE_RUN(p_vdec_hcodec(), clock_off);
}

void hevc_clock_enable(void)
{
	IF_HAVE_RUN(p_vdec_hevc(), clock_enable);
}

void hevc_clock_hi_enable(void)
{
	IF_HAVE_RUN(p_vdec_hevc(), clock_hi_enable);
}

void hevc_clock_on(void)
{
	IF_HAVE_RUN(p_vdec_hevc(), clock_on);
}

void hevc_clock_off(void)
{
	IF_HAVE_RUN(p_vdec_hevc(), clock_off);
}

void vdec_clock_prepare_switch(void)
{
	IF_HAVE_RUN(p_vdec(), clock_prepare_switch);
}

void hevc_clock_prepare_switch(void)
{
	IF_HAVE_RUN(p_vdec_hevc(), clock_prepare_switch);
}

int vdec_clock_level(enum vdec_type_e core)
{
	return get_current_vdec_chip()->clk_mgr[core]->clock_level(core);
}

static int register_vdec_clk_mgr_per_cpu(int cputype,
		enum vdec_type_e vdec_type, struct chip_vdec_clk_s *t_mgr)
{

	struct chip_vdec_clk_s *mgr;
	if (cputype != get_cpu_type() || vdec_type >= VDEC_MAX) {
		/*
		pr_info("ignore vdec clk mgr for vdec[%d] cpu=%d\n",
			vdec_type, cputype);
		*/
		return 0;	/* ignore don't needed firmare. */
	}
	mgr = kmalloc(sizeof(struct chip_vdec_clk_s), GFP_KERNEL);
	if (!mgr)
		return -ENOMEM;
	*mgr = *t_mgr;
	/*
	pr_info("register vdec clk mgr for vdec[%d]\n", vdec_type);
	*/
	get_current_vdec_chip()->clk_mgr[vdec_type] = mgr;
	return 0;
}

int register_vdec_clk_mgr(int cputype[], enum vdec_type_e vdec_type,
						  struct chip_vdec_clk_s *t_mgr)
{
	int i = 0;
	while (cputype[i] > 0) {
		register_vdec_clk_mgr_per_cpu(cputype[i], vdec_type, t_mgr);
		i++;
	}
	return 0;
}

