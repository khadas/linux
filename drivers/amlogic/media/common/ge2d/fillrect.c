// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Amlogic Headers */
#include <linux/amlogic/media/ge2d/ge2d.h>

/* Local Headers */
#include "ge2dgen.h"
#include "ge2d_log.h"

static void _fillrect(struct ge2d_context_s *wq,
		      int x, int y, int w, int h,
		      unsigned int color, int blk, int enqueue)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	if (ge2d_log_level & GE2D_LOG_DUMP_STACK)
		dump_stack();

	if (x < 0 || y < 0 || w < 0 || h < 0) {
		ge2d_log_err("%s wrong params, %d %d %d %d\n",
			     __func__, x, y, w, h);
		return;
	}

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
	ge2d_cmd_cfg->hsc_adv_num = 0;

	ge2d_cmd_cfg->src1_fill_color_en = 1;

	ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->color_logic_op   = LOGIC_OPERATION_COPY;
	ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->alpha_logic_op   = LOGIC_OPERATION_COPY;
	ge2d_cmd_cfg->wait_done_flag   = blk;
	ge2d_cmd_cfg->cmd_op           = IS_FILLRECT;

	ge2d_wq_add_work(wq, enqueue);
}

void fillrect(struct ge2d_context_s *wq,
	      int x, int y, int w, int h, unsigned int color)
{
	_fillrect(wq, x, y, w, h, color, 1, 0);
}
EXPORT_SYMBOL(fillrect);

void fillrect_noblk(struct ge2d_context_s *wq,
		    int x, int y, int w, int h, unsigned int color)
{
	_fillrect(wq, x, y, w, h, color, 0, 0);
}
EXPORT_SYMBOL(fillrect_noblk);

void fillrect_enqueue(struct ge2d_context_s *wq,
		      int x, int y, int w, int h, unsigned int color)
{
	_fillrect(wq, x, y, w, h, color, 0, 1);
}
EXPORT_SYMBOL(fillrect_enqueue);
