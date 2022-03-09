/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/vout/vdac/vdac_dev.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef _VDAC_DEV_H_
#define _VDAC_DEV_H_

#define HHI_VDAC_CNTL0        0xbd
#define HHI_VDAC_CNTL1        0xbe
#define HHI_VDAC_CNTL0_G12A   0xbb
#define HHI_VDAC_CNTL1_G12A   0xbc

#define ANACTRL_VDAC_CTRL0    0xb0
#define ANACTRL_VDAC_CTRL1    0xb1
#define HHI_VIID_CLK_DIV      0x4a
#define HHI_VIID_CLK_CNTL     0x4b
#define HHI_VIID_DIVIDER_CNTL 0x4c
#define HHI_VID_CLK_CNTL2     0x65
#define HHI_VID_DIVIDER_CNTL  0x66

#define CLKCTRL_VID_CLK_CTRL2 0x31
#define CLKCTRL_VIID_CLK_DIV  0x33
#define VENC_VDAC_DACSEL0     0x1b78

/* T5W vdac clk register */
#define HHI_VIID_CLK0_DIV     0xa0
#define HHI_VID_CLK0_CTRL2    0xa4

#define VDAC_CTRL_MAX         10

/* 20220419:adjust cvbsout clk delay */
#define VDIN_VER "20220419:adjust cvbsout clk delay"

enum vdac_cpu_type {
	VDAC_CPU_G12AB = 0,
	VDAC_CPU_TL1 = 1,
	VDAC_CPU_SM1 = 2,
	VDAC_CPU_TM2 = 3,
	VDAC_CPU_SC2 = 4,
	VDAC_CPU_T5  = 5,
	VDAC_CPU_T5D  = 6,
	VDAC_CPU_S4   = 7,
	VDAC_CPU_T3   = 8,
	VDAC_CPU_S4D   = 9,
	VDAC_CPU_T5W   = 10,
	VDAC_CPU_MAX,
};

#define VDAC_REG_OFFSET(reg)          ((reg) << 2)
#define VDAC_REG_MAX    0xffff

struct meson_vdac_data {
	enum vdac_cpu_type cpu_id;
	const char *name;

	unsigned int reg_cntl0;
	unsigned int reg_cntl1;
	unsigned int reg_vid_clk_ctrl2;
	unsigned int reg_vid2_clk_div;
	struct meson_vdac_ctrl_s *ctrl_table;
	unsigned int cdac_disable;
};

struct meson_vdac_ctrl_s {
	unsigned int reg;
	unsigned int val;
	unsigned int bit;
	unsigned int len;
};

extern const struct of_device_id meson_vdac_dt_match[];
struct meson_vdac_data *aml_vdac_config_probe(struct platform_device *pdev);

#endif
