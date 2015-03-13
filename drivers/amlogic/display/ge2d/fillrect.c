/*
 * Amlogic GE2D driver
 *
 * Copyright (C) 2015 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author: Platform-BJ <platform.bj@amlogic.com>
 *
 */


/* Amlogic Headers */
#include <linux/amlogic/ge2d/ge2d.h>

/* Local Headers */
#include "ge2dgen.h"


static void _fillrect(struct ge2d_context_s *wq,
			     int x, int y, int w, int h,
			     unsigned color, int blk)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	ge2dgen_src_color(wq, color);

	ge2d_cmd_cfg->src1_x_start = x;
	ge2d_cmd_cfg->src1_x_end   = x + w - 1;
	ge2d_cmd_cfg->src1_y_start = y;
	ge2d_cmd_cfg->src1_y_end   = y + h - 1;

	ge2d_cmd_cfg->dst_x_start  = x;
	ge2d_cmd_cfg->dst_x_end    = x + w - 1;
	ge2d_cmd_cfg->dst_y_start  = y;
	ge2d_cmd_cfg->dst_y_end    = y + h - 1;

	ge2d_cmd_cfg->sc_hsc_en = 0;
	ge2d_cmd_cfg->sc_vsc_en = 0;
	ge2d_cmd_cfg->hsc_rpt_p0_num = 0;
	ge2d_cmd_cfg->vsc_rpt_l0_num = 0;
	ge2d_cmd_cfg->hsc_div_en = 0;

	ge2d_cmd_cfg->src1_fill_color_en = 1;

	ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->color_logic_op   = LOGIC_OPERATION_COPY;
	ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->alpha_logic_op   = LOGIC_OPERATION_COPY;
	ge2d_cmd_cfg->wait_done_flag   = 1;

	ge2d_wq_add_work(wq);
}

void fillrect(struct ge2d_context_s *wq,
	      int x, int y, int w, int h, unsigned color)
{
	_fillrect(wq, x, y, w, h, color, 1);
}
EXPORT_SYMBOL(fillrect);

void fillrect_noblk(struct ge2d_context_s *wq,
		    int x, int y, int w, int h, unsigned color)
{
	_fillrect(wq, x, y, w, h, color, 0);
}
EXPORT_SYMBOL(fillrect_noblk);
