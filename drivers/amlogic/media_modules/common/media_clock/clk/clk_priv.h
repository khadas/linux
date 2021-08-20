/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 * drivers/amlogic/media/common/arch/clk/clk_priv.h
 */
#ifndef AMPORTS_CLK_PRIV_HEADER
#define AMPORTS_CLK_PRIV_HEADER

struct clk_set {
	u32 wh_X_fps;		/* [x*y*fps */
	u32 clk_Mhz;		/*min MHZ */
};
#define MAX_CLK_SET 6
struct clk_set_setting {
	struct clk_set set[MAX_CLK_SET];
};

struct chip_vdec_clk_s {
	int (*clock_get)(enum vdec_type_e core);
	int (*clock_init)(void);
	int (*clock_set)(int clk);
	void (*clock_on)(void);
	void (*clock_off)(void);
	void (*clock_prepare_switch)(void);
};
#endif
