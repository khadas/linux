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
#include "clk_priv.h"
#include "log.h"

#define p_vdec() (get_current_vdec_chip()->clk_mgr[VDEC_1])
#define p_vdec2() (get_current_vdec_chip()->clk_mgr[VDEC_2])
#define p_vdec_hcodec() (get_current_vdec_chip()->clk_mgr[VDEC_HCODEC])
#define p_vdec_hevc() (get_current_vdec_chip()->clk_mgr[VDEC_HEVC])

static int clock_source_wxhxfps_saved[VDEC_MAX + 1];

#define IF_HAVE_RUN(p, fn)\
	do {\
		if (p && p->fn)\
			p->fn();\
	} while (0)

#define IF_HAVE_RUN_P1_RET(p, fn, p1)\
			do {\
				pr_debug("%s-----%d\n", __func__, clk);\
				if (p && p->fn)\
					return p->fn(p1);\
				else\
					return -1;\
			} while (0)


#define IF_HAVE_RUN_RET(p, fn)\
	do {\
		if (p && p->fn)\
			return p->fn();\
		else\
			return 0;\
	} while (0)

int vdec_clock_init(void)
{
	IF_HAVE_RUN_RET(p_vdec(), clock_init);
}
/*
clk ==0 :
	to be release.
	released shared clk,
clk ==1 :default low clk
clk ==2 :default high clk
*/
int vdec_clock_set(int clk)
{

	IF_HAVE_RUN_P1_RET(p_vdec(), clock_set, clk);
}
void vdec_clock_enable(void)
{
	vdec_clock_set(1);
}
void vdec_clock_hi_enable(void)
{
	vdec_clock_set(2);
}


void vdec_clock_on(void)
{
	IF_HAVE_RUN(p_vdec(), clock_on);
}

void vdec_clock_off(void)
{
	IF_HAVE_RUN(p_vdec(), clock_off);
	clock_source_wxhxfps_saved[VDEC_1] = 0;
}

int vdec2_clock_set(int clk)
{
	IF_HAVE_RUN_P1_RET(p_vdec2(), clock_set, clk);
}
void vdec2_clock_enable(void)
{
	vdec2_clock_set(1);
}
void vdec2_clock_hi_enable(void)
{
	vdec2_clock_set(2);
}

void vdec2_clock_on(void)
{
	IF_HAVE_RUN(p_vdec2(), clock_on);
}

void vdec2_clock_off(void)
{
	IF_HAVE_RUN(p_vdec2(), clock_off);
	clock_source_wxhxfps_saved[VDEC_2] = 0;
}

int hcodec_clock_set(int clk)
{
	IF_HAVE_RUN_P1_RET(p_vdec_hcodec(), clock_set, clk);
}
void hcodec_clock_enable(void)
{
	hcodec_clock_set(1);
}
void hcodec_clock_hi_enable(void)
{
	hcodec_clock_set(2);
}

void hcodec_clock_on(void)
{
	IF_HAVE_RUN(p_vdec_hcodec(), clock_on);
}

void hcodec_clock_off(void)
{
	IF_HAVE_RUN(p_vdec_hcodec(), clock_off);
	clock_source_wxhxfps_saved[VDEC_HCODEC] = 0;
}

int hevc_clock_init(void)
{
	IF_HAVE_RUN_RET(p_vdec_hevc(), clock_init);
}

int hevc_clock_set(int clk)
{
	IF_HAVE_RUN_P1_RET(p_vdec_hevc(), clock_set, clk);
}
void hevc_clock_enable(void)
{
	hevc_clock_set(1);
}
void hevc_clock_hi_enable(void)
{
	hevc_clock_set(2);
}
void hevc_clock_on(void)
{
	IF_HAVE_RUN(p_vdec_hevc(), clock_on);
}

void hevc_clock_off(void)
{
	IF_HAVE_RUN(p_vdec_hevc(), clock_off);
	clock_source_wxhxfps_saved[VDEC_HEVC] = 0;
}


int vdec_source_get(enum vdec_type_e core)
{
	return clock_source_wxhxfps_saved[core];
}

int vdec_clk_get(enum vdec_type_e core)
{
	return get_current_vdec_chip()->clk_mgr[core]->clock_get(core);
}

int get_clk_with_source(int format, int w_x_h_fps)
{
	struct clk_set_setting *p_setting;
	int i;
	int clk = -2;
	p_setting = get_current_vdec_chip()->clk_setting_array;
	if (!p_setting || format < 0 || format > VFORMAT_MAX) {
		pr_info("error on get_clk_with_source ,%p,%d\n",
			p_setting, format);
		return -1;/*no setting found.*/
	}
	p_setting = &p_setting[format];
	for (i = 0; i < MAX_CLK_SET; i++) {
		if (p_setting->set[i].wh_X_fps > w_x_h_fps) {
			clk = p_setting->set[i].clk_Mhz;
			break;
		}
	}
	return clk;
}


int vdec_source_changed_for_clk_set(int format, int width, int height, int fps)
{
	int clk = get_clk_with_source(format, width * height * fps);
	int ret_clk;
	if (clk < 0) {
		pr_info("can't get valid clk for source ,%d,%d,%d\n",
			width, height, fps);
		if (format >= 1920 && width >= 1080 && fps >= 30)
			clk = 2;/*default high clk*/
		else
			clk = 0;/*default clk.*/
	}
	if (width * height * fps == 0)
		clk = 0;
	/*
		clk == 0
		is used for set default clk;
		if used supper clk.
		changed to default  min clk.
	*/

	if (format == VFORMAT_HEVC || format == VFORMAT_VP9) {
		ret_clk = hevc_clock_set(clk);
		clock_source_wxhxfps_saved[VDEC_HEVC] = width * height * fps;
	} else if (format == VFORMAT_H264_ENC && format == VFORMAT_JPEG_ENC) {
		ret_clk = hcodec_clock_set(clk);
		clock_source_wxhxfps_saved[VDEC_HCODEC] = width * height * fps;
	} else if (format == VFORMAT_H264_4K2K &&
			get_cpu_type() == MESON_CPU_MAJOR_ID_M8) {
		ret_clk = vdec2_clock_set(clk);
		clock_source_wxhxfps_saved[VDEC_2] = width * height * fps;
		ret_clk = vdec_clock_set(clk);
		clock_source_wxhxfps_saved[VDEC_1] = width * height * fps;
	} else{
		ret_clk = vdec_clock_set(clk);
		clock_source_wxhxfps_saved[VDEC_1] = width * height * fps;
	}
	return ret_clk;
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
	if (mgr->clock_init) {
		if (mgr->clock_init()) {
			kfree(mgr);
			return -ENOMEM;
		}
	}
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


static int register_vdec_clk_setting_per_cpu(int cputype,
		struct clk_set_setting *setting,
		int size)
{

	struct clk_set_setting *p_setting;
	if (cputype != get_cpu_type()) {
		/*
		pr_info("ignore clk_set_setting for cpu=%d\n",
			cputype);
		*/
		return 0;	/* ignore don't needed this setting . */
	}
	p_setting = kmalloc(size, GFP_KERNEL);
	if (!p_setting)
		return -ENOMEM;
	memcpy(p_setting, setting, size);

	pr_info("register clk_set_setting cpu[%d]\n", cputype);

	get_current_vdec_chip()->clk_setting_array = p_setting;
	return 0;
}

int register_vdec_clk_setting(int cputype[],
		struct clk_set_setting *p_seting,
		int size)
{
	int i = 0;
	while (cputype[i] > 0) {
		register_vdec_clk_setting_per_cpu(cputype[i], p_seting, size);
		i++;
	}
	return 0;
}


