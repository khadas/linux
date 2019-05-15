/*
 * drivers/amlogic/media/common/arch/clk/clkgx.c
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
#define DEBUG
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/amlogic/media/clk/gp_pll.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include "../../../frame_provider/decoder/utils/vdec.h"
#include <linux/amlogic/media/registers/register.h>
#include "clk_priv.h"
#include <linux/amlogic/media/utils/log.h>

#include <linux/amlogic/media/registers/register_ops.h>
#include "../switch/amports_gate.h"
#include "../../chips/decoder_cpu_ver_info.h"

#define MHz (1000000)
#define debug_print pr_info
#define TL1_HEVC_MAX_CLK  (800)

//#define  NO_CLKTREE

/* set gp0 648M vdec use gp0 clk*/
#define VDEC1_648M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (6 << 9) | (0), 0, 16)

#define HEVC_648M() \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (6 << 9) | (0), 16, 16)

/*set gp0 1296M vdec use gp0 clk div2*/
#define VDEC1_648M_DIV() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (6 << 9) | (1), 0, 16)

#define HEVC_648M_DIV() \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (6 << 9) | (1), 16, 16)

#define VDEC1_WITH_GP_PLL() \
	((READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0xe00) == 0xc00)
#define HEVC_WITH_GP_PLL() \
	((READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0xe000000) == 0xc000000)

#define VDEC1_CLOCK_ON()  \
	do { if (is_meson_m8_cpu()) { \
		WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL, 1, 8, 1); \
		WRITE_VREG_BITS(DOS_GCLK_EN0, 0x3ff, 0, 10); \
		} else { \
		WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL, 1, 8, 1); \
		WRITE_HHI_REG_BITS(HHI_VDEC3_CLK_CNTL, 0, 15, 1); \
		WRITE_HHI_REG_BITS(HHI_VDEC3_CLK_CNTL, 0, 8, 1); \
		WRITE_VREG_BITS(DOS_GCLK_EN0, 0x3ff, 0, 10); \
		} \
	} while (0)

#define VDEC2_CLOCK_ON()   do {\
		WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 1, 8, 1); \
		WRITE_VREG(DOS_GCLK_EN1, 0x3ff);\
	} while (0)

#define HCODEC_CLOCK_ON()  do {\
		WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL, 1, 24, 1); \
		WRITE_VREG_BITS(DOS_GCLK_EN0, 0x7fff, 12, 15);\
	} while (0)
#define HEVC_CLOCK_ON()    do {\
		WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 1, 24, 1); \
		WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 1, 8, 1); \
		WRITE_HHI_REG_BITS(HHI_VDEC4_CLK_CNTL, 0, 31, 1); \
		WRITE_HHI_REG_BITS(HHI_VDEC4_CLK_CNTL, 0, 15, 1); \
		WRITE_HHI_REG_BITS(HHI_VDEC4_CLK_CNTL, 0, 24, 1); \
		WRITE_VREG(DOS_GCLK_EN3, 0xffffffff);\
	} while (0)
#define VDEC1_SAFE_CLOCK() do {\
	WRITE_HHI_REG_BITS(HHI_VDEC3_CLK_CNTL, \
		READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x7f, 0, 7); \
	WRITE_HHI_REG_BITS(HHI_VDEC3_CLK_CNTL, 1, 8, 1); \
	WRITE_HHI_REG_BITS(HHI_VDEC3_CLK_CNTL, 1, 15, 1);\
	} while (0)

#define VDEC1_CLOCK_OFF()  \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  0, 8, 1)
#define VDEC2_CLOCK_OFF()  \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 0, 8, 1)
#define HCODEC_CLOCK_OFF() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL, 0, 24, 1)
#define HEVC_SAFE_CLOCK()  do { \
	WRITE_HHI_REG_BITS(HHI_VDEC4_CLK_CNTL, \
		(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) >> 16) & 0x7f, 16, 7);\
	WRITE_HHI_REG_BITS(HHI_VDEC4_CLK_CNTL, \
		(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) >> 25) & 0x7f, 25, 7);\
	WRITE_HHI_REG_BITS(HHI_VDEC4_CLK_CNTL, 1, 24, 1); \
	WRITE_HHI_REG_BITS(HHI_VDEC4_CLK_CNTL, 1, 31, 1);\
	WRITE_HHI_REG_BITS(HHI_VDEC4_CLK_CNTL, 1, 15, 1);\
	} while (0)

#define HEVC_CLOCK_OFF()  do {\
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 0, 24, 1);\
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 0, 8, 1);\
}while(0)

static int clock_real_clk[VDEC_MAX + 1];

static unsigned int set_frq_enable, vdec_frq, hevc_frq, hevcb_frq;

#ifdef NO_CLKTREE
static struct gp_pll_user_handle_s *gp_pll_user_vdec, *gp_pll_user_hevc;
static bool is_gp0_div2 = true;

static int gp_pll_user_cb_vdec(struct gp_pll_user_handle_s *user,
			int event)
{
	debug_print("gp_pll_user_cb_vdec call\n");
	if (event == GP_PLL_USER_EVENT_GRANT) {
		struct clk *clk = clk_get(NULL, "gp0_pll");
		if (!IS_ERR(clk)) {
			if (is_gp0_div2)
				clk_set_rate(clk, 1296000000UL);
			else
				clk_set_rate(clk, 648000000UL);
			VDEC1_SAFE_CLOCK();
			VDEC1_CLOCK_OFF();
			if (is_gp0_div2)
				VDEC1_648M_DIV();
			else
				VDEC1_648M();

			VDEC1_CLOCK_ON();
			debug_print("gp_pll_user_cb_vdec call set\n");
		}
	}
	return 0;
}

static int gp_pll_user_cb_hevc(struct gp_pll_user_handle_s *user,
			int event)
{
	debug_print("gp_pll_user_cb_hevc callback\n");
	if (event == GP_PLL_USER_EVENT_GRANT) {
		struct clk *clk = clk_get(NULL, "gp0_pll");
		if (!IS_ERR(clk)) {
			if (is_gp0_div2)
				clk_set_rate(clk, 1296000000UL);
			else
				clk_set_rate(clk, 648000000UL);
//			HEVC_SAFE_CLOCK();
			HEVC_CLOCK_OFF();
			if (is_gp0_div2)
				HEVC_648M_DIV();
			else
				HEVC_648M();
			HEVC_CLOCK_ON();
			debug_print("gp_pll_user_cb_hevc callback2\n");
		}
	}

	return 0;
}


#endif

struct clk_mux_s {
	struct gate_switch_node *vdec_mux_node;
	struct gate_switch_node *hcodec_mux_node;
	struct gate_switch_node *hevc_mux_node;
	struct gate_switch_node *hevc_back_mux_node;
};

struct clk_mux_s gclk;

void vdec1_set_clk(int source, int div)
{
	pr_debug("vdec1_set_clk %d, %d\n", source, div);
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL, (source << 9) | (div - 1), 0, 16);
}
EXPORT_SYMBOL(vdec1_set_clk);

void hcodec_set_clk(int source, int div)
{
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,
		(source << 9) | (div - 1), 16, 16);
}
EXPORT_SYMBOL(hcodec_set_clk);

void vdec2_set_clk(int source, int div)
{
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL,
		(source << 9) | (div - 1), 0, 16);
}
EXPORT_SYMBOL(vdec2_set_clk);

//extern uint force_hevc_clock_cntl;
uint force_hevc_clock_cntl = 0;
void hevc_set_clk(int source, int div)
{
	if (force_hevc_clock_cntl) {
		pr_info("%s, write force clock cntl %x\n", __func__, force_hevc_clock_cntl);
		WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, force_hevc_clock_cntl);
	} else {
		pr_debug("hevc_set_clk %d, %d\n", source, div);
		WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL,
			(source << 9) | (div - 1), 16, 16);
		WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (source << 9) | (div - 1), 0, 16);
	}
}
EXPORT_SYMBOL(hevc_set_clk);

void vdec_get_clk_source(int clk, int *source, int *div, int *rclk)
{
#define source_div4 (0)
#define source_div3 (1)
#define source_div5 (2)
#define source_div7 (3)
	if (clk > 500) {
		*source = source_div3;
		*div = 1;
		*rclk = 667;
	} else if (clk >= 500) {
		*source = source_div4;
		*div = 1;
		*rclk = 500;
	} else if (clk >= 400) {
		*source = source_div5;
		*div = 1;
		*rclk = 400;
	} else if (clk >= 333) {
		*source = source_div3;
		*div = 2;
		*rclk = 333;
	} else if (clk >= 200) {
		*source = source_div5;
		*div = 2;
		*rclk = 200;
	} else if (clk >= 166) {
		*source = source_div4;
		*div = 3;
		*rclk = 166;
	} else if (clk >= 133) {
		*source = source_div5;
		*div = 3;
		*rclk = 133;
	} else if (clk >= 100) {
		*source = source_div5;
		*div = 4;
		*rclk = 100;
	} else if (clk >= 50) {
		*source = source_div5;
		*div = 8;
		*rclk = 50;
	} else {
		*source = source_div5;
		*div = 20;
		*rclk = 10;
	}
}
EXPORT_SYMBOL(vdec_get_clk_source);


/*
 *enum vformat_e {
 *	VFORMAT_MPEG12 = 0,
 *	VFORMAT_MPEG4,
 *	VFORMAT_H264,
 *	VFORMAT_MJPEG,
 *	VFORMAT_REAL,
 *	VFORMAT_JPEG,
 *	VFORMAT_VC1,
 *	VFORMAT_AVS,
 *	VFORMAT_YUV,
 *	VFORMAT_H264MVC,
 *	VFORMAT_H264_4K2K,
 *	VFORMAT_HEVC,
 *	VFORMAT_H264_ENC,
 *	VFORMAT_JPEG_ENC,
 *	VFORMAT_VP9,
 *	VFORMAT_MAX
 *};
 *sample:
 *{{1280*720*30, 100}, {1920*1080*30, 166}, {1920*1080*60, 333},
 *	{4096*2048*30, 600}, {4096*2048*60, 600}, {INT_MAX, 600},}
 *mean:
 *width * height * fps
 *<720p30fps						clk=100MHZ
 *>=720p30fps & < 1080p30fps		clk=166MHZ
 *>=1080p 30fps & < 1080p60fps	clk=333MHZ
 */
static struct clk_set_setting clks_for_formats[] = {
	{			/*[VFORMAT_MPEG12] */
			{{1280 * 720 * 30, 100}, {1920 * 1080 * 30, 166},
				{1920 * 1080 * 60, 333},
				{4096 * 2048 * 30, 600}, {4096 * 2048 * 60,
						600}, {INT_MAX, 600},
				}
		},
	{			/*[VFORMAT_MPEG4] */
			{{1280 * 720 * 30, 100}, {1920 * 1080 * 30, 166},
				{1920 * 1080 * 60, 333},
				{4096 * 2048 * 30, 600}, {4096 * 2048 * 60,
						600}, {INT_MAX, 600},
				}
		},
	{			/*[VFORMAT_H264] */
			{{1280 * 720 * 30, 100}, {1920 * 1080 * 21, 166},
				{1920 * 1080 * 30, 333},
				{1920 * 1080 * 60, 600}, {4096 * 2048 * 60,
						600}, {INT_MAX, 600},
				}
		},
	{			/*[VFORMAT_MJPEG] */
			{{1280 * 720 * 30, 200}, {1920 * 1080 * 30, 200},
				{1920 * 1080 * 60, 333},
				{4096 * 2048 * 30, 600}, {4096 * 2048 * 60,
						600}, {INT_MAX, 600},
				}
		},
	{			/*[VFORMAT_REAL] */
			{{1280 * 720 * 20, 200}, {1920 * 1080 * 30, 500},
				{1920 * 1080 * 60, 500},
				{4096 * 2048 * 30, 600}, {4096 * 2048 * 60,
						600}, {INT_MAX, 600},
				}
		},
	{			/*[VFORMAT_JPEG] */
			{{1280 * 720 * 30, 100}, {1920 * 1080 * 30, 166},
				{1920 * 1080 * 60, 333},
				{4096 * 2048 * 30, 600}, {4096 * 2048 * 60,
						600}, {INT_MAX, 600},
				}
		},
	{			/*[VFORMAT_VC1] */
			{{1280 * 720 * 30, 100}, {1920 * 1080 * 30, 166},
				{1920 * 1080 * 60, 333},
				{4096 * 2048 * 30, 600}, {4096 * 2048 * 60,
						600}, {INT_MAX, 600},
				}
		},
	{			/*[VFORMAT_AVS] */
			{{1280 * 720 * 30, 100}, {1920 * 1080 * 30, 166},
				{1920 * 1080 * 60, 333},
				{4096 * 2048 * 30, 600}, {4096 * 2048 * 60,
						600}, {INT_MAX, 600},
				}
		},
	{			/*[VFORMAT_YUV] */
			{{1280 * 720 * 30, 100}, {INT_MAX, 100},
				{0, 0}, {0, 0}, {0, 0}, {0, 0},
				}
		},
	{			/*VFORMAT_H264MVC */
			{{1280 * 720 * 30, 333}, {1920 * 1080 * 30, 333},
				{4096 * 2048 * 60, 600},
				{INT_MAX, 630}, {0, 0}, {0, 0},
				}
		},
	{			/*VFORMAT_H264_4K2K */
			{{1280 * 720 * 30, 600}, {4096 * 2048 * 60, 630},
				{INT_MAX, 630},
				{0, 0}, {0, 0}, {0, 0},
				}
		},
	{			/*VFORMAT_HEVC */
			{{1280 * 720 * 30, 100}, {1920 * 1080 * 60, 600},
				{4096 * 2048 * 25, 630},
				{4096 * 2048 * 30, 630}, {4096 * 2048 * 60,
						630}, {INT_MAX, 630},
				}
		},
	{			/*VFORMAT_H264_ENC */
			{{1280 * 720 * 30, 0}, {INT_MAX, 0},
				{0, 0}, {0, 0}, {0, 0}, {0, 0},
				}
		},
	{			/*VFORMAT_JPEG_ENC */
			{{1280 * 720 * 30, 0}, {INT_MAX, 0},
				{0, 0}, {0, 0}, {0, 0}, {0, 0},
				}
		},
	{			/*VFORMAT_VP9 */
			{{1280 * 720 * 30, 100}, {1920 * 1080 * 30, 100},
				{1920 * 1080 * 60, 166},
				{4096 * 2048 * 30, 333}, {4096 * 2048 * 60,
						630}, {INT_MAX, 630},
				}
		},
	{/*VFORMAT_AVS2*/
		{{1280*720*30, 100}, {1920*1080*30, 100},
		{1920*1080*60, 166}, {4096*2048*30, 333},
		{4096*2048*60, 630}, {INT_MAX, 630},}
	},

};

void set_clock_gate(struct gate_switch_node *nodes, int num)
{
	struct gate_switch_node *node = NULL;

	do {
		node = &nodes[num - 1];
		if (IS_ERR_OR_NULL(node))
			pr_info("get mux clk err.\n");

		if (!strcmp(node->name, "clk_vdec_mux"))
			gclk.vdec_mux_node = node;
		else if (!strcmp(node->name, "clk_hcodec_mux"))
			gclk.hcodec_mux_node = node;
		else if (!strcmp(node->name, "clk_hevc_mux"))
			gclk.hevc_mux_node = node;
		else if (!strcmp(node->name, "clk_hevcb_mux"))
			gclk.hevc_back_mux_node = node;
	} while(--num);
}
EXPORT_SYMBOL(set_clock_gate);
#ifdef NO_CLKTREE
int vdec_set_clk(int dec, int source, int div)
{

	if (dec == VDEC_1)
		vdec1_set_clk(source, div);
	else if (dec == VDEC_2)
		vdec2_set_clk(source, div);
	else if (dec == VDEC_HEVC)
		hevc_set_clk(source, div);
	else if (dec == VDEC_HCODEC)
		hcodec_set_clk(source, div);
	return 0;
}

#else
static int vdec_set_clk(int dec, int rate)
{
	struct clk *clk = NULL;

	switch (dec) {
	case VDEC_1:
		clk = gclk.vdec_mux_node->clk;
		WRITE_VREG_BITS(DOS_GCLK_EN0, 0x3ff, 0, 10);
		break;

	case VDEC_HCODEC:
		clk = gclk.hcodec_mux_node->clk;
		WRITE_VREG_BITS(DOS_GCLK_EN0, 0x7fff, 12, 15);
		break;

	case VDEC_2:
		clk = gclk.vdec_mux_node->clk;
		WRITE_VREG(DOS_GCLK_EN1, 0x3ff);
		break;

	case VDEC_HEVC:
		clk = gclk.hevc_mux_node->clk;
		WRITE_VREG(DOS_GCLK_EN3, 0xffffffff);
		break;

	case VDEC_HEVCB:
		clk = gclk.hevc_back_mux_node->clk;
		WRITE_VREG(DOS_GCLK_EN3, 0xffffffff);
		break;

	case VDEC_MAX:
		break;

	default:
		pr_info("invaild vdec type.\n");
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_info("the mux clk err.\n");
		return -1;
	}

	clk_set_rate(clk, rate);

	return 0;
}

static int vdec_clock_init(void)
{
	return 0;
}

#endif
#ifdef NO_CLKTREE
static int vdec_clock_init(void)
{
	gp_pll_user_vdec = gp_pll_user_register("vdec", 0,
		gp_pll_user_cb_vdec);
	if (get_cpu_major_id() >= MESON_CPU_MAJOR_ID_GXL)
		is_gp0_div2 = false;
	else
		is_gp0_div2 = true;

	if (get_cpu_major_id() >= MESON_CPU_MAJOR_ID_GXL) {
		pr_info("used fix clk for vdec clk source!\n");
		//update_vdec_clk_config_settings(1);
	}
	return (gp_pll_user_vdec) ? 0 : -ENOMEM;
}



static void update_clk_with_clk_configs(
	int clk, int *source, int *div, int *rclk)
{
	unsigned int config = 0;//get_vdec_clk_config_settings();

	if (!config)
		return;
	if (config >= 10) {
		int wantclk;
		wantclk = config;
		vdec_get_clk_source(wantclk, source, div, rclk);
	}
	return;
}
#define NO_GP0_PLL 0//(get_vdec_clk_config_settings() == 1)
#define ALWAYS_GP0_PLL 0//(get_vdec_clk_config_settings() == 2)

#define NO_GP0_PLL 0//(get_vdec_clk_config_settings() == 1)
#define ALWAYS_GP0_PLL 0//(get_vdec_clk_config_settings() == 2)

static int vdec_clock_set(int clk)
{
	int use_gpll = 0;
	int source, div, rclk;
	int clk_seted = 0;
	int gp_pll_wait = 0;
	if (clk == 1)
		clk = 200;
	else if (clk == 2) {
		if (clock_real_clk[VDEC_1] != 648)
			clk = 500;
		else
			clk = 648;
	} else if (clk == 0) {
		/*used for release gp pull.
		   if used, release it.
		   if not used gp pll
		   do nothing.
		 */
		if (clock_real_clk[VDEC_1] == 667 ||
		    (clock_real_clk[VDEC_1] == 648) ||
			clock_real_clk[VDEC_1] <= 0)
			clk = 200;
		else
			clk = clock_real_clk[VDEC_1];
	}
	vdec_get_clk_source(clk, &source, &div, &rclk);
	update_clk_with_clk_configs(clk, &source, &div, &rclk);

	if (clock_real_clk[VDEC_1] == rclk)
		return rclk;
	if (NO_GP0_PLL) {
		use_gpll = 0;
		clk_seted = 0;
	} else if ((rclk > 500 && clk != 667) || ALWAYS_GP0_PLL) {
		if (clock_real_clk[VDEC_1] == 648)
			return 648;
		use_gpll = 1;
		gp_pll_request(gp_pll_user_vdec);
		while (!VDEC1_WITH_GP_PLL() && gp_pll_wait++ < 1000000)
			udelay(1);
		if (VDEC1_WITH_GP_PLL()) {
			clk_seted = 1;
			rclk = 648;
		} else {
			use_gpll = 0;
			rclk = 667;
			/*gp_pull request failed,used default 500Mhz*/
			pr_info("get gp pll failed used fix pull\n");
		}
	}
	if (!clk_seted) {/*if 648 not set,*/
		VDEC1_SAFE_CLOCK();
		VDEC1_CLOCK_OFF();
		vdec_set_clk(VDEC_1, source, div);
		VDEC1_CLOCK_ON();
	}

	if (!use_gpll)
		gp_pll_release(gp_pll_user_vdec);
	clock_real_clk[VDEC_1] = rclk;
	debug_print("vdec_clock_set 2 to %d\n", rclk);
	return rclk;
}
static int hevc_clock_init(void)
{
	gp_pll_user_hevc = gp_pll_user_register("hevc", 0,
		gp_pll_user_cb_hevc);

	return (gp_pll_user_hevc) ? 0 : -ENOMEM;
}
static int hevc_back_clock_init(void)
{
	return 0;
}

static int hevc_back_clock_set(int clk)
{
	return 0;
}

static int hevc_clock_set(int clk)
{
	int use_gpll = 0;
	int source, div, rclk;
	int gp_pll_wait = 0;
	int clk_seted = 0;

	debug_print("hevc_clock_set 1 to clk %d\n", clk);
	if (clk == 1)
		clk = 200;
	else if (clk == 2) {
		if (clock_real_clk[VDEC_HEVC] != 648)
			clk = 500;
		else
			clk = 648;
	} else if (clk == 0) {
		/*used for release gp pull.
		   if used, release it.
		   if not used gp pll
		   do nothing.
		 */
		if ((clock_real_clk[VDEC_HEVC] == 667) ||
			(clock_real_clk[VDEC_HEVC] == 648) ||
			(clock_real_clk[VDEC_HEVC] <= 0))
			clk = 200;
		else
			clk = clock_real_clk[VDEC_HEVC];
	}
	vdec_get_clk_source(clk, &source, &div, &rclk);
	update_clk_with_clk_configs(clk, &source, &div, &rclk);

	if (rclk == clock_real_clk[VDEC_HEVC])
		return rclk;/*clk not changed,*/
	if (NO_GP0_PLL) {
		use_gpll = 0;
		clk_seted = 0;
	} else if ((rclk > 500 && clk != 667) || ALWAYS_GP0_PLL) {
		if (clock_real_clk[VDEC_HEVC] == 648)
			return 648;
		use_gpll = 1;
		gp_pll_request(gp_pll_user_hevc);
		while (!HEVC_WITH_GP_PLL() && gp_pll_wait++ < 1000000)
			udelay(1);
		if (HEVC_WITH_GP_PLL()) {
			clk_seted = 1;
			rclk = 648;
		} else {
			rclk = 667;
			/*gp_pull request failed,used default 500Mhz*/
			pr_info("get gp pll failed used fix pull\n");
		}
	}
	if (!clk_seted) {/*if 648 not set,*/
//		HEVC_SAFE_CLOCK();
		HEVC_CLOCK_OFF();
		vdec_set_clk(VDEC_HEVC, source, div);
		HEVC_CLOCK_ON();
	}
	if (!use_gpll)
		gp_pll_release(gp_pll_user_hevc);
	clock_real_clk[VDEC_HEVC] = rclk;
	/*debug_print("hevc_clock_set 2 to rclk=%d, configs=%d\n",
		rclk,
		get_vdec_clk_config_settings());*/ //DEBUG_TMP
	return rclk;
}

static int hcodec_clock_set(int clk)
{
	int source, div, rclk;
	HCODEC_CLOCK_OFF();
	vdec_get_clk_source(200, &source, &div, &rclk);
	vdec_set_clk(VDEC_HCODEC, source, div);
	HCODEC_CLOCK_ON();
	clock_real_clk[VDEC_HCODEC] = rclk;
	return rclk;
}


#else
static int vdec_clock_set(int clk)
{
	if (clk == 1)
		clk = 200;
	else if (clk == 2) {
		if (clock_real_clk[VDEC_1] != 648)
			clk = 500;
		else
			clk = 648;
	} else if (clk == 0) {
		if (clock_real_clk[VDEC_1] == 667 ||
			(clock_real_clk[VDEC_1] == 648) ||
			clock_real_clk[VDEC_1] <= 0)
			clk = 200;
		else
			clk = clock_real_clk[VDEC_1];
	}

	if ((clk > 500 && clk != 667)) {
		if (clock_real_clk[VDEC_1] == 648)
			return 648;
		clk = 667;
	}

	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SM1)
		clk = 800;

	if (set_frq_enable && vdec_frq) {
		pr_info("Set the vdec frq is %u MHz\n", vdec_frq);
		clk = vdec_frq;
	}

	vdec_set_clk(VDEC_1, clk * MHz);

	clock_real_clk[VDEC_1] = clk;

	pr_debug("vdec mux clock is %lu Hz\n",
		clk_get_rate(gclk.vdec_mux_node->clk));

	return clk;
}

static int hevc_clock_init(void)
{
	return 0;
}

static int hevc_back_clock_init(void)
{
	return 0;
}

static int hevc_back_clock_set(int clk)
{
	if (clk == 1)
		clk = 200;
	else if (clk == 2) {
		if (clock_real_clk[VDEC_HEVCB] != 648)
			clk = 500;
		else
			clk = 648;
	} else if (clk == 0) {
		if (clock_real_clk[VDEC_HEVCB] == 667 ||
			(clock_real_clk[VDEC_HEVCB] == 648) ||
			clock_real_clk[VDEC_HEVCB] <= 0)
			clk = 200;
		else
			clk = clock_real_clk[VDEC_HEVCB];
	}

	if ((clk > 500 && clk != 667)) {
		if (clock_real_clk[VDEC_HEVCB] == 648)
		return 648;
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
			clk = TL1_HEVC_MAX_CLK;
		else
			clk = 667;
	}

	if (set_frq_enable && hevcb_frq) {
		pr_info("Set the hevcb frq is %u MHz\n", hevcb_frq);
		clk = hevcb_frq;
	}

	if (get_cpu_major_id() >= MESON_CPU_MAJOR_ID_TXLX) {
		if ((READ_EFUSE_REG(EFUSE_LIC1) >> 28 & 0x1) && clk > 333) {
			pr_info("The hevcb clock limit to 333MHz.\n");
			clk = 333;
		}
	}

	vdec_set_clk(VDEC_HEVCB, clk * MHz);

	clock_real_clk[VDEC_HEVCB] = clk;
	pr_debug("hevc back mux clock is %lu Hz\n",
		clk_get_rate(gclk.hevc_back_mux_node->clk));

	return clk;
}

static int hevc_clock_set(int clk)
{
	if (clk == 1)
		clk = 200;
	else if (clk == 2) {
		if (clock_real_clk[VDEC_HEVC] != 648)
			clk = 500;
		else
			clk = 648;
	} else if (clk == 0) {
		if (clock_real_clk[VDEC_HEVC] == 667 ||
			(clock_real_clk[VDEC_HEVC] == 648) ||
			clock_real_clk[VDEC_HEVC] <= 0)
			clk = 200;
		else
			clk = clock_real_clk[VDEC_HEVC];
	}

	if ((clk > 500 && clk != 667)) {
		if (clock_real_clk[VDEC_HEVC] == 648)
			return 648;
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
			clk = TL1_HEVC_MAX_CLK;
		else
			clk = 667;
	}

	if (set_frq_enable && hevc_frq) {
		pr_info("Set the hevc frq is %u MHz\n", hevc_frq);
		clk = hevc_frq;
	}

	vdec_set_clk(VDEC_HEVC, clk * MHz);

	clock_real_clk[VDEC_HEVC] = clk;

	pr_debug("hevc mux clock is %lu Hz\n",
		clk_get_rate(gclk.hevc_mux_node->clk));

	return clk;
}

static int hcodec_clock_set(int clk)
{
	if (clk == 1)
		clk = 200;
	else if (clk == 2) {
		if (clock_real_clk[VDEC_HCODEC] != 648)
			clk = 500;
		else
			clk = 648;
	} else if (clk == 0) {
		if (clock_real_clk[VDEC_HCODEC] == 667 ||
			(clock_real_clk[VDEC_HCODEC] == 648) ||
			clock_real_clk[VDEC_HCODEC] <= 0)
			clk = 200;
		else
			clk = clock_real_clk[VDEC_HCODEC];
	}

	if ((clk > 500 && clk != 667)) {
		if (clock_real_clk[VDEC_HCODEC] == 648)
			return 648;
		clk = 667;
	}

	vdec_set_clk(VDEC_HCODEC, clk * MHz);

	clock_real_clk[VDEC_HCODEC] = clk;

	pr_debug("hcodec mux clock is %lu Hz\n",
		clk_get_rate(gclk.hcodec_mux_node->clk));

	return clk;
}
#endif

static void vdec_clock_on(void)
{
	mutex_lock(&gclk.vdec_mux_node->mutex);
	if (!gclk.vdec_mux_node->ref_count)
		clk_prepare_enable(gclk.vdec_mux_node->clk);

	gclk.vdec_mux_node->ref_count++;
	mutex_unlock(&gclk.vdec_mux_node->mutex);

	pr_debug("the %-15s clock on, ref cnt: %d\n",
		gclk.vdec_mux_node->name,
		gclk.vdec_mux_node->ref_count);
}

static void vdec_clock_off(void)
{
	mutex_lock(&gclk.vdec_mux_node->mutex);
	gclk.vdec_mux_node->ref_count--;
	if (!gclk.vdec_mux_node->ref_count)
		clk_disable_unprepare(gclk.vdec_mux_node->clk);

	clock_real_clk[VDEC_1] = 0;
	mutex_unlock(&gclk.vdec_mux_node->mutex);

	pr_debug("the %-15s clock off, ref cnt: %d\n",
		gclk.vdec_mux_node->name,
		gclk.vdec_mux_node->ref_count);
}

static void hcodec_clock_on(void)
{
	mutex_lock(&gclk.hcodec_mux_node->mutex);
	if (!gclk.hcodec_mux_node->ref_count)
		clk_prepare_enable(gclk.hcodec_mux_node->clk);

	gclk.hcodec_mux_node->ref_count++;
	mutex_unlock(&gclk.hcodec_mux_node->mutex);

	pr_debug("the %-15s clock on, ref cnt: %d\n",
		gclk.hcodec_mux_node->name,
		gclk.hcodec_mux_node->ref_count);
}

static void hcodec_clock_off(void)
{
	mutex_lock(&gclk.hcodec_mux_node->mutex);
	gclk.hcodec_mux_node->ref_count--;
	if (!gclk.hcodec_mux_node->ref_count)
		clk_disable_unprepare(gclk.hcodec_mux_node->clk);

	mutex_unlock(&gclk.hcodec_mux_node->mutex);

	pr_debug("the %-15s clock off, ref cnt: %d\n",
		gclk.hcodec_mux_node->name,
		gclk.hcodec_mux_node->ref_count);
}

static void hevc_clock_on(void)
{
	mutex_lock(&gclk.hevc_mux_node->mutex);
	if (!gclk.hevc_mux_node->ref_count)
		clk_prepare_enable(gclk.hevc_mux_node->clk);

	gclk.hevc_mux_node->ref_count++;
	WRITE_VREG(DOS_GCLK_EN3, 0xffffffff);
	mutex_unlock(&gclk.hevc_mux_node->mutex);

	pr_debug("the %-15s clock on, ref cnt: %d\n",
		gclk.hevc_mux_node->name,
		gclk.hevc_mux_node->ref_count);
}

static void hevc_clock_off(void)
{
	mutex_lock(&gclk.hevc_mux_node->mutex);
	gclk.hevc_mux_node->ref_count--;
	if (!gclk.hevc_mux_node->ref_count)
		clk_disable_unprepare(gclk.hevc_mux_node->clk);

	clock_real_clk[VDEC_HEVC] = 0;
	mutex_unlock(&gclk.hevc_mux_node->mutex);

	pr_debug("the %-15s clock off, ref cnt: %d\n",
		gclk.hevc_mux_node->name,
		gclk.hevc_mux_node->ref_count);
}

static void hevc_back_clock_on(void)
{
	mutex_lock(&gclk.hevc_back_mux_node->mutex);
	if (!gclk.hevc_back_mux_node->ref_count)
		clk_prepare_enable(gclk.hevc_back_mux_node->clk);

	gclk.hevc_back_mux_node->ref_count++;
	WRITE_VREG(DOS_GCLK_EN3, 0xffffffff);
	mutex_unlock(&gclk.hevc_back_mux_node->mutex);

	pr_debug("the %-15s clock on, ref cnt: %d\n",
		gclk.hevc_back_mux_node->name,
		gclk.hevc_back_mux_node->ref_count);
}

static void hevc_back_clock_off(void)
{
	mutex_lock(&gclk.hevc_back_mux_node->mutex);
	gclk.hevc_back_mux_node->ref_count--;
	if (!gclk.hevc_back_mux_node->ref_count)
		clk_disable_unprepare(gclk.hevc_back_mux_node->clk);

	clock_real_clk[VDEC_HEVC] = 0;
	mutex_unlock(&gclk.hevc_back_mux_node->mutex);

	pr_debug("the %-15s clock off, ref cnt: %d\n",
		gclk.hevc_back_mux_node->name,
		gclk.hevc_back_mux_node->ref_count);
}

static int vdec_clock_get(enum vdec_type_e core)
{
	if (core >= VDEC_MAX)
		return 0;

	return clock_real_clk[core];
}

#define INCLUDE_FROM_ARCH_CLK_MGR

/*#define VDEC_HAS_VDEC2*/
#define VDEC_HAS_HEVC
#define VDEC_HAS_VDEC_HCODEC
#define VDEC_HAS_CLK_SETTINGS
#define CLK_FOR_CPU {\
	AM_MESON_CPU_MAJOR_ID_GXBB,\
	AM_MESON_CPU_MAJOR_ID_GXTVBB,\
	AM_MESON_CPU_MAJOR_ID_GXL,\
	AM_MESON_CPU_MAJOR_ID_GXM,\
	AM_MESON_CPU_MAJOR_ID_TXL,\
	AM_MESON_CPU_MAJOR_ID_TXLX,\
	AM_MESON_CPU_MAJOR_ID_G12A,\
	AM_MESON_CPU_MAJOR_ID_G12B,\
	AM_MESON_CPU_MAJOR_ID_SM1,\
	AM_MESON_CPU_MAJOR_ID_TL1,\
	0}
#include "clk.h"

module_param(set_frq_enable, uint, 0664);
MODULE_PARM_DESC(set_frq_enable, "\n set frequency enable\n");

module_param(vdec_frq, uint, 0664);
MODULE_PARM_DESC(vdec_frq, "\n set vdec frequency\n");

module_param(hevc_frq, uint, 0664);
MODULE_PARM_DESC(hevc_frq, "\n set hevc frequency\n");

module_param(hevcb_frq, uint, 0664);
MODULE_PARM_DESC(hevcb_frq, "\n set hevcb frequency\n");

ARCH_VDEC_CLK_INIT();
ARCH_VDEC_CLK_EXIT();

MODULE_LICENSE("GPL");
