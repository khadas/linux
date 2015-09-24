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
#include "../vdec_reg.h"
#include "../amports_config.h"
#include "../vdec.h"
#include "register.h"
#include "clk_priv.h"
#include "register_ops.h"
#include "log.h"

/*
HHI_VDEC_CLK_CNTL
0x1078[11:9] (fclk = 2550MHz)
    0: fclk_div4
    1: fclk_div3
    2: fclk_div5
    3: fclk_div7
    4: mpll_clk_out1
    5: mpll_clk_out2
0x1078[6:0]
    devider
0x1078[8]
    enable
*/

/* 182.14M <-- (2550/7)/2 */
#define VDEC1_182M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (3 << 9) | (1), 0, 16)
#define VDEC2_182M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (3 << 9) | (1))

/* 212.50M <-- (2550/3)/4 */
#define VDEC1_212M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (1 << 9) | (3), 0, 16)
#define VDEC2_212M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (1 << 9) | (3))

/* 255.00M <-- (2550/5)/2 */
#define VDEC1_255M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (2 << 9) | (1), 0, 16)
#define VDEC2_255M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (2 << 9) | (1))
#define HCODEC_255M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL, (2 << 9) | (1), 16, 16)
#define HEVC_255M()  \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (2 << 9) | (1), 16, 16)

/* 283.33M <-- (2550/3)/3 */
#define VDEC1_283M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (1 << 9) | (2), 0, 16)
#define VDEC2_283M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (1 << 9) | (2));

/* 318.75M <-- (2550/4)/2 */
#define VDEC1_319M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (0 << 9) | (1), 0, 16)
#define VDEC2_319M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (0 << 9) | (1))

/* 364.29M <-- (2550/7)/1 -- over limit, do not use */
#define VDEC1_364M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (3 << 9) | (0), 0, 16)
#define VDEC2_364M() \
	WRITE_HHI_REG(HHI_VDEC2_CLK_CNTL, (3 << 9) | (0))

/* 425.00M <-- (2550/3)/2 */
#define VDEC1_425M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (1 << 9) | (2), 0, 16)

/* 510.00M <-- (2550/5)/1 */
#define VDEC1_510M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (2 << 9) | (0), 0, 16)
#define HEVC_510M()  \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (2 << 9) | (0), 16, 16)

/* 637.50M <-- (2550/4)/1 */
#define VDEC1_638M() \
	WRITE_HHI_REG_BITS(HHI_VDEC_CLK_CNTL,  (0 << 9) | (0), 0, 16)
#define HEVC_638M()  \
	WRITE_HHI_REG_BITS(HHI_VDEC2_CLK_CNTL, (0 << 9) | (0), 16, 16)

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


static int vdec_clock_init(void)
{
	return 0;
}

static int vdec_clock_set(int clk)
{
	VDEC1_SAFE_CLOCK();
	VDEC1_CLOCK_OFF();
	VDEC1_255M();
	VDEC1_CLOCK_ON();
	clock_level[VDEC_1] = 255;
	return 255;
}


static void vdec_clock_on(void)
{
	VDEC1_CLOCK_ON();
}

static void vdec_clock_off(void)
{
	VDEC1_CLOCK_OFF();
}

static int vdec2_clock_set(int clk)
{
	VDEC2_CLOCK_OFF();
	VDEC2_255M();
	VDEC2_CLOCK_ON();
	clock_level[VDEC_2] = 255;
	return 255;
}


static void vdec2_clock_on(void)
{
	VDEC2_CLOCK_ON();
}

static void vdec2_clock_off(void)
{
	VDEC2_CLOCK_OFF();
}

static int hcodec_clock_set(int clk)
{
	HCODEC_CLOCK_OFF();
	HCODEC_255M();
	HCODEC_CLOCK_ON();
	clock_level[VDEC_HCODEC] = 255;
	return 255;
}

static void hcodec_clock_on(void)
{
	HCODEC_CLOCK_ON();
}

static void hcodec_clock_off(void)
{
	HCODEC_CLOCK_OFF();
}

static int hevc_clock_init(void)
{
	return 0;
}

static int hevc_clock_set(int clk)
{
	HEVC_SAFE_CLOCK();
	HEVC_CLOCK_OFF();
	/* HEVC_255M(); */
	HEVC_638M();
	HEVC_CLOCK_ON();
	clock_level[VDEC_HEVC] = 638;
	return 638;
}

static void hevc_clock_on(void)
{
	HEVC_CLOCK_ON();
}

static void hevc_clock_off(void)
{
	HEVC_CLOCK_OFF();
}

static int vdec_clock_get(enum vdec_type_e core)
{
	if (core >= VDEC_MAX)
		return 0;

	return clock_level[core];
}

#define INCLUDE_FROM_ARCH_CLK_MGR
#define VDEC_HAS_VDEC2
#define VDEC_HAS_HEVC
#define VDEC_HAS_VDEC_HCODEC
#define CLK_FOR_CPU {\
			MESON_CPU_MAJOR_ID_M8,\
			MESON_CPU_MAJOR_ID_M8M2,\
			0}
#include "clk.h"
ARCH_VDEC_CLK_INIT();
