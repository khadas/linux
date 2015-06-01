/*
 * drivers/amlogic/amports/arch/clkm8.c
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
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/amlogic/amports/gp_pll.h>
#include "../vdec_reg.h"
#include "../amports_config.h"
#include "../vdec.h"
#include "register.h"


#include "register_ops.h"

/*
HHI_VDEC_CLK_CNTL
0x1078[11:9] (fclk = 2000MHz)
    0: fclk_div4
    1: fclk_div3
    2: fclk_div5
    3: fclk_div7
    4: mpll_clk_out1
    5: mpll_clk_out2
0x1078[6:0]
    divider
0x1078[8]
    enable
*/

/* 182.8M <-- (2000/7)/2 */
#define VDEC1_142M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (3 << 9) | (1), 0, 16)
#define VDEC2_142M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (3 << 9) | (1))

/* 166.6M <-- (2000/3)/4 */
#define VDEC1_166M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (1 << 9) | (3), 0, 16)
#define VDEC2_166M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (1 << 9) | (3))

/* 200.00M <-- (2000/5)/2 */
#define VDEC1_200M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (2 << 9) | (1), 0, 16)
#define VDEC2_200M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (2 << 9) | (1))
#define HCODEC_200M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL, (2 << 9) | (1), 16, 16)
#define HEVC_200M()  \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (2 << 9) | (1), 16, 16)

/* 222M <-- (2000/3)/3 */
#define VDEC1_283M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (1 << 9) | (2), 0, 16)
#define VDEC2_283M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (1 << 9) | (2));

/* 250M <-- (2000/4)/2 */
#define VDEC1_250M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (0 << 9) | (1), 0, 16)
#define VDEC2_250M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (0 << 9) | (1))

/* 285M <-- (2000/7)/1 -- over limit, do not use */
#define VDEC1_285M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (3 << 9) | (0), 0, 16)
#define VDEC2_285M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (3 << 9) | (0))

/* 333.00M <-- (2000/3)/2 */
#define VDEC1_333M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (1 << 9) | (2), 0, 16)

/* 400.00M <-- (2000/5)/1 */
#define VDEC1_400M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (2 << 9) | (0), 0, 16)
#define HEVC_400M()  \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (2 << 9) | (0), 16, 16)

/* 500M <-- (2000/4)/1 */
#define VDEC1_500M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (0 << 9) | (0), 0, 16)
#define HEVC_500M()  \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (0 << 9) | (0), 16, 16)

/* 648M <-- (1296/2) */
#define VDEC1_648M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (6 << 9) | (1), 0, 16)
#define HEVC_648M() \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (6 << 9) | (1), 16, 16)

#define VDEC1_WITH_GP_PLL() \
	((READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0xe00) == 0xc00)
#define HEVC_WITH_GP_PLL() \
	((READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0xe000000) == 0xc000000)

/* 666M <-- (2000/3)/1 */
#define VDEC1_666M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (1 << 9) | (0), 0, 16)
#define HEVC_666M()  \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (1 << 9) | (0), 16, 16)


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
		WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 0, 24, 1); \
		WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 0, 31, 1); \
		WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 1, 24, 1); \
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
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, \
		(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) >> 16) & 0x7f, 16, 7); \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 1, 24, 1); \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 1, 31, 1);\
	} while (0)

#define HEVC_CLOCK_OFF()   WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, 0, 24, 1)

static int clock_level[VDEC_MAX + 1];
static struct gp_pll_user_handle_s *gp_pll_user_vdec, *gp_pll_user_hevc;

static int gp_pll_user_cb_vdec(struct gp_pll_user_handle_s *user,
			int event)
{
	if (event == GP_PLL_USER_EVENT_GRANT) {
		struct clk *clk = clk_get(NULL, "gp0_pll");
		if (!IS_ERR(clk)) {
			clk_set_rate(clk, 1296000000UL);
			VDEC1_648M();
		}
	}

	return 0;
}

static int vdec_clock_init(void)
{
	gp_pll_user_vdec = gp_pll_user_register("vdec", 0,
		gp_pll_user_cb_vdec);

	return (gp_pll_user_vdec) ? 0 : -ENOMEM;
}

static void vdec_clock_enable(void)
{
	VDEC1_CLOCK_OFF();
	VDEC1_250M();
	VDEC1_CLOCK_ON();
	gp_pll_release(gp_pll_user_vdec);
	clock_level[VDEC_1] = 0;
}

static void vdec_clock_hi_enable(void)
{
	VDEC1_CLOCK_OFF();
	VDEC1_666M();
	VDEC1_CLOCK_ON();
	gp_pll_release(gp_pll_user_vdec);
	clock_level[VDEC_1] = 1;
}

static void vdec_clock_superhi_enable(void)
{
	VDEC1_CLOCK_OFF();
	gp_pll_request(gp_pll_user_vdec);
	while (!VDEC1_WITH_GP_PLL())
		;
	VDEC1_CLOCK_ON();
	clock_level[VDEC_1] = 2;
}

static void vdec_clock_on(void)
{
	VDEC1_CLOCK_ON();
}

static void vdec_clock_off(void)
{
	VDEC1_CLOCK_OFF();
}
static void hcodec_clock_enable(void)
{
	HCODEC_CLOCK_OFF();
	HCODEC_200M();
	HCODEC_CLOCK_ON();
}

static void hcodec_clock_on(void)
{
	HCODEC_CLOCK_ON();
}

static void hcodec_clock_off(void)
{
	HCODEC_CLOCK_OFF();
}

static int gp_pll_user_cb_hevc(struct gp_pll_user_handle_s *user,
			int event)
{
	if (event == GP_PLL_USER_EVENT_GRANT) {
		struct clk *clk = clk_get(NULL, "gp0_pll");
		if (!IS_ERR(clk)) {
			clk_set_rate(clk, 1296000000UL);
			HEVC_648M();
		}
	}

	return 0;
}

static int hevc_clock_init(void)
{
	gp_pll_user_hevc = gp_pll_user_register("hevc", 0,
		gp_pll_user_cb_hevc);

	return (gp_pll_user_hevc) ? 0 : -ENOMEM;
}

static void hevc_clock_enable(void)
{
	HEVC_CLOCK_OFF();
	/* HEVC_255M(); */
	HEVC_400M();
	HEVC_CLOCK_ON();
	clock_level[VDEC_HEVC] = 0;
	gp_pll_release(gp_pll_user_hevc);
}

static void hevc_clock_hi_enable(void)
{
	HEVC_CLOCK_OFF();
	HEVC_666M();
	HEVC_CLOCK_ON();
	gp_pll_release(gp_pll_user_hevc);
	clock_level[VDEC_HEVC] = 1;
}

static void hevc_clock_superhi_enable(void)
{
	HEVC_CLOCK_OFF();
	gp_pll_request(gp_pll_user_hevc);
	while (!HEVC_WITH_GP_PLL())
		;
	HEVC_CLOCK_ON();
	clock_level[VDEC_HEVC] = 2;
}

static void hevc_clock_on(void)
{
	HEVC_CLOCK_ON();
}

static void hevc_clock_off(void)
{
	HEVC_CLOCK_OFF();
}

static void vdec_clock_prepare_switch(void)
{
	VDEC1_SAFE_CLOCK();
}

static void hevc_clock_prepare_switch(void)
{
	HEVC_SAFE_CLOCK();
}

static int vdec_clock_level(enum vdec_type_e core)
{
	if (core >= VDEC_MAX)
		return 0;

	return clock_level[core];
}

#define INCLUDE_FROM_ARCH_CLK_MGR
/*#define VDEC_HAS_VDEC2*/
#define VDEC_HAS_HEVC
#define VDEC_HAS_VDEC_HCODEC
#define CLK_FOR_CPU {\
			MESON_CPU_MAJOR_ID_GXBB,\
			0}
#include "clk.h"
ARCH_VDEC_CLK_INIT();
