// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Amlogic Headers */
#include <linux/amlogic/media/ge2d/ge2d.h>
#include "ge2d_log.h"

static void _bitblt(struct ge2d_context_s *wq,
		    int src_x, int src_y, int w, int h,
		    int dst_x, int dst_y, int block, int enqueue)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	if (ge2d_log_level & GE2D_LOG_DUMP_STACK)
		dump_stack();

	if (src_x < 0 || src_y < 0 || w < 0 || h < 0 ||
	   dst_x < 0 || dst_y < 0) {
		ge2d_log_err("%s wrong params, %d %d %d %d-> %d %d\n",
			     __func__,
			     src_x, src_y, w, h,
			     dst_x, dst_y);
		return;
	}

	ge2d_cmd_cfg->src1_x_start = src_x;
	ge2d_cmd_cfg->src1_x_end   = src_x + w - 1;
	ge2d_cmd_cfg->src1_y_start = src_y;
	ge2d_cmd_cfg->src1_y_end   = src_y + h - 1;

	ge2d_cmd_cfg->dst_x_start = dst_x;
	ge2d_cmd_cfg->dst_x_end   = dst_x + w - 1;
	ge2d_cmd_cfg->dst_y_start = dst_y;
	ge2d_cmd_cfg->dst_y_end   = dst_y + h - 1;

	ge2d_cmd_cfg->sc_hsc_en = 0;
	ge2d_cmd_cfg->sc_vsc_en = 0;
	ge2d_cmd_cfg->hsc_rpt_p0_num = 0;
	ge2d_cmd_cfg->vsc_rpt_l0_num = 0;
	ge2d_cmd_cfg->hsc_div_en = 0;
	ge2d_cmd_cfg->hsc_adv_num = 0;

	ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->color_logic_op   = LOGIC_OPERATION_COPY;
	ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->alpha_logic_op   = LOGIC_OPERATION_COPY;
	ge2d_cmd_cfg->wait_done_flag   = block;
	ge2d_cmd_cfg->cmd_op           = IS_BLIT;

	ge2d_wq_add_work(wq, enqueue);
}

static void _bitblt_noalpha(struct ge2d_context_s *wq,
			    int src_x, int src_y, int w, int h,
			    int dst_x, int dst_y, int block, int enqueue)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);
	struct ge2d_dp_gen_s *dp_gen_cfg = ge2d_wq_get_dp_gen(wq);

	if (ge2d_log_level & GE2D_LOG_DUMP_STACK)
		dump_stack();

	if (src_x < 0 || src_y < 0 || w < 0 || h < 0 ||
	   dst_x < 0 || dst_y < 0) {
		ge2d_log_err("%s wrong params, %d %d %d %d-> %d %d\n",
			     __func__,
			     src_x, src_y, w, h,
			     dst_x, dst_y);
		return;
	}

	if (dp_gen_cfg->alu_const_color != 0xff) {
		dp_gen_cfg->alu_const_color = 0xff;
		wq->config.update_flag |= UPDATE_DP_GEN;
	}
	ge2d_cmd_cfg->src1_x_start = src_x;
	ge2d_cmd_cfg->src1_x_end   = src_x + w - 1;
	ge2d_cmd_cfg->src1_y_start = src_y;
	ge2d_cmd_cfg->src1_y_end   = src_y + h - 1;

	ge2d_cmd_cfg->dst_x_start  = dst_x;
	ge2d_cmd_cfg->dst_x_end    = dst_x + w - 1;
	ge2d_cmd_cfg->dst_y_start  = dst_y;
	ge2d_cmd_cfg->dst_y_end    = dst_y + h - 1;

	ge2d_cmd_cfg->sc_hsc_en = 0;
	ge2d_cmd_cfg->sc_vsc_en = 0;
	ge2d_cmd_cfg->hsc_rpt_p0_num = 0;
	ge2d_cmd_cfg->vsc_rpt_l0_num = 0;
	ge2d_cmd_cfg->hsc_div_en = 0;
	ge2d_cmd_cfg->hsc_adv_num = 0;

	ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->color_logic_op   = LOGIC_OPERATION_COPY;
	ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->alpha_logic_op   = LOGIC_OPERATION_SET;
	ge2d_cmd_cfg->wait_done_flag   = block;
	ge2d_cmd_cfg->cmd_op           = IS_BLIT;

	ge2d_wq_add_work(wq, enqueue);
}

void bitblt(struct ge2d_context_s *wq,
	    int src_x, int src_y, int w, int h,
	    int dst_x, int dst_y)
{
	_bitblt(wq, src_x, src_y, w, h, dst_x, dst_y, 1, 0);
}
EXPORT_SYMBOL(bitblt);

void bitblt_noblk(struct ge2d_context_s *wq,
		  int src_x, int src_y, int w, int h,
		  int dst_x, int dst_y)
{
	_bitblt(wq, src_x, src_y, w, h, dst_x, dst_y, 0, 0);
}
EXPORT_SYMBOL(bitblt_noblk);

void bitblt_noalpha(struct ge2d_context_s *wq,
		    int src_x, int src_y, int w, int h,
		    int dst_x, int dst_y)
{
	_bitblt_noalpha(wq, src_x, src_y, w, h, dst_x, dst_y, 1, 0);
}
EXPORT_SYMBOL(bitblt_noalpha);

void bitblt_noalpha_noblk(struct ge2d_context_s *wq,
			  int src_x, int src_y, int w, int h,
			  int dst_x, int dst_y)
{
	_bitblt_noalpha(wq, src_x, src_y, w, h, dst_x, dst_y, 0, 0);
}
EXPORT_SYMBOL(bitblt_noalpha_noblk);

void bitblt_enqueue(struct ge2d_context_s *wq,
		    int src_x, int src_y, int w, int h,
		    int dst_x, int dst_y)
{
	_bitblt(wq, src_x, src_y, w, h, dst_x, dst_y, 0, 1);
}
EXPORT_SYMBOL(bitblt_enqueue);

void bitblt_noalpha_enqueue(struct ge2d_context_s *wq,
			    int src_x, int src_y, int w, int h,
			    int dst_x, int dst_y)
{
	_bitblt_noalpha(wq, src_x, src_y, w, h, dst_x, dst_y, 0, 1);
}
EXPORT_SYMBOL(bitblt_noalpha_enqueue);

