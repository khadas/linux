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


#include "register_ops.h"

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
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL,  (3 << 9) | (1), 0, 16)
#define VDEC2_182M() \
	codec_hhibus_write(HHI_VDEC2_CLK_CNTL, (3 << 9) | (1))

/* 212.50M <-- (2550/3)/4 */
#define VDEC1_212M() \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL,  (1 << 9) | (3), 0, 16)
#define VDEC2_212M() \
	codec_hhibus_write(HHI_VDEC2_CLK_CNTL, (1 << 9) | (3))

/* 255.00M <-- (2550/5)/2 */
#define VDEC1_255M() \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL,  (2 << 9) | (1), 0, 16)
#define VDEC2_255M() \
	codec_hhibus_write(HHI_VDEC2_CLK_CNTL, (2 << 9) | (1))
#define HCODEC_255M() \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL, (2 << 9) | (1), 16, 16)
#define HEVC_255M()  \
	codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, (2 << 9) | (1), 16, 16)

/* 283.33M <-- (2550/3)/3 */
#define VDEC1_283M() \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL,  (1 << 9) | (2), 0, 16)
#define VDEC2_283M() \
	codec_hhibus_write(HHI_VDEC2_CLK_CNTL, (1 << 9) | (2));

/* 318.75M <-- (2550/4)/2 */
#define VDEC1_319M() \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL,  (0 << 9) | (1), 0, 16)
#define VDEC2_319M() \
	codec_hhibus_write(HHI_VDEC2_CLK_CNTL, (0 << 9) | (1))

/* 364.29M <-- (2550/7)/1 -- over limit, do not use */
#define VDEC1_364M() \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL,  (3 << 9) | (0), 0, 16)
#define VDEC2_364M() \
	codec_hhibus_write(HHI_VDEC2_CLK_CNTL, (3 << 9) | (0))

/* 425.00M <-- (2550/3)/2 */
#define VDEC1_425M() \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL,  (1 << 9) | (2), 0, 16)

/* 510.00M <-- (2550/5)/1 */
#define VDEC1_510M() \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL,  (2 << 9) | (0), 0, 16)
#define HEVC_510M()  \
	codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, (2 << 9) | (0), 16, 16)

/* 637.50M <-- (2550/4)/1 */
#define VDEC1_638M() \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL,  (0 << 9) | (0), 0, 16)
#define HEVC_638M()  \
	codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, (0 << 9) | (0), 16, 16)

#define VDEC1_CLOCK_ON()  \
	do { if (is_meson_m8_cpu()) { \
		codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL, 1, 8, 1); \
		codec_set_dosbus_bits(DOS_GCLK_EN0, 0x3ff, 0, 10); \
		} else { \
		codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL, 1, 8, 1); \
		codec_set_hhibus_bits(HHI_VDEC3_CLK_CNTL, 0, 15, 1); \
		codec_set_hhibus_bits(HHI_VDEC3_CLK_CNTL, 0, 8, 1); \
		codec_set_dosbus_bits(DOS_GCLK_EN0, 0x3ff, 0, 10); \
		} \
	} while (0)

#define VDEC2_CLOCK_ON()   do {\
		codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, 1, 8, 1); \
		codec_dosbus_write(DOS_GCLK_EN1, 0x3ff);\
	} while (0)

#define HCODEC_CLOCK_ON()  do {\
		codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL, 1, 24, 1); \
		codec_set_dosbus_bits(DOS_GCLK_EN0, 0x7fff, 12, 15);\
	} while (0)
#define HEVC_CLOCK_ON()    do {\
		codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, 0, 24, 1); \
		codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, 0, 31, 1); \
		codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, 1, 24, 1); \
		codec_dosbus_write(DOS_GCLK_EN3, 0xffffffff);\
	} while (0)
#define VDEC1_SAFE_CLOCK() do {\
	codec_set_hhibus_bits(HHI_VDEC3_CLK_CNTL, \
		codec_hhibus_read(HHI_VDEC_CLK_CNTL) & 0x7f, 0, 7); \
	codec_set_hhibus_bits(HHI_VDEC3_CLK_CNTL, 1, 8, 1); \
	codec_set_hhibus_bits(HHI_VDEC3_CLK_CNTL, 1, 15, 1);\
	} while (0)

#define VDEC1_CLOCK_OFF()  \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL,  0, 8, 1)
#define VDEC2_CLOCK_OFF()  \
	codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, 0, 8, 1)
#define HCODEC_CLOCK_OFF() \
	codec_set_hhibus_bits(HHI_VDEC_CLK_CNTL, 0, 24, 1)
#define HEVC_SAFE_CLOCK()  do { \
	codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, \
		(codec_hhibus_read(HHI_VDEC2_CLK_CNTL) >> 16) & 0x7f, 16, 7); \
	codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, 1, 24, 1); \
	codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, 1, 31, 1);\
	} while (0)

#define HEVC_CLOCK_OFF()   codec_set_hhibus_bits(HHI_VDEC2_CLK_CNTL, 0, 24, 1)

static int clock_level[VDEC_MAX + 1];

static void vdec_clock_enable(void)
{
	VDEC1_CLOCK_OFF();
	VDEC1_255M();
	VDEC1_CLOCK_ON();
	clock_level[VDEC_1] = 0;
}

static void vdec_clock_hi_enable(void)
{
	VDEC1_CLOCK_OFF();
	if (is_meson_m8_cpu())
		VDEC1_364M();
	else
		VDEC1_638M();
	VDEC1_CLOCK_ON();
	clock_level[VDEC_1] = 1;
}

static void vdec_clock_on(void)
{
	VDEC1_CLOCK_ON();
}

static void vdec_clock_off(void)
{
	VDEC1_CLOCK_OFF();
}

static void vdec2_clock_enable(void)
{
	VDEC2_CLOCK_OFF();
	VDEC2_255M();
	VDEC2_CLOCK_ON();
	clock_level[VDEC_2] = 0;
}

static void vdec2_clock_hi_enable(void)
{
	VDEC2_CLOCK_OFF();
	VDEC2_364M();
	VDEC2_CLOCK_ON();
	clock_level[VDEC_2] = 1;
}

static void vdec2_clock_on(void)
{
	VDEC2_CLOCK_ON();
}

static void vdec2_clock_off(void)
{
	VDEC2_CLOCK_OFF();
}

static void hcodec_clock_enable(void)
{
	HCODEC_CLOCK_OFF();
	HCODEC_255M();
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

static void hevc_clock_enable(void)
{
	HEVC_CLOCK_OFF();
	/* HEVC_255M(); */
	HEVC_638M();
	HEVC_CLOCK_ON();
}

static void hevc_clock_hi_enable(void)
{
	HEVC_CLOCK_OFF();
	HEVC_638M();
	HEVC_CLOCK_ON();
	clock_level[VDEC_HEVC] = 1;
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
#define VDEC_HAS_VDEC2
#define VDEC_HAS_HEVC
#define VDEC_HAS_VDEC_HCODEC
#define CLK_FOR_CPU {\
			MESON_CPU_MAJOR_ID_M8,\
			MESON_CPU_MAJOR_ID_M8M2,\
			MESON_CPU_MAJOR_ID_GXBB,\
			0}
#include "clk.h"
ARCH_VDEC_CLK_INIT();
