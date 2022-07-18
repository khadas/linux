// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/types.h>

/* Amlogic Headers */
#include <linux/amlogic/media/ge2d/ge2d.h>
#include "ge2d_reg.h"
#include "ge2d_log.h"

void blend(struct ge2d_context_s *wq,
	   int src_x, int src_y, int src_w, int src_h,
	   int src2_x, int src2_y, int src2_w, int src2_h,
	   int dst_x, int dst_y, int dst_w, int dst_h,
	   int op)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	if (ge2d_log_level & GE2D_LOG_DUMP_STACK)
		dump_stack();

	if (src_x < 0 || src_y < 0 || src_w < 0 || src_h < 0 ||
	   src2_x < 0 || src2_y < 0 || src2_w < 0 || src2_h < 0 ||
	   dst_x < 0 || dst_y < 0 || dst_w < 0 || dst_h < 0) {
		ge2d_log_err("%s wrong params, %d %d %d %d + %d %d %d %d -> %d %d %d %d\n",
			     __func__,
			     src_x, src_y, src_w, src_h,
			     src2_x, src2_y, src2_w, src2_h,
			     dst_x, dst_y, dst_w, dst_h);
		return;
	}

	ge2d_cmd_cfg->src1_x_start = src_x;
	ge2d_cmd_cfg->src1_x_end   = src_x + src_w - 1;
	ge2d_cmd_cfg->src1_y_start = src_y;
	ge2d_cmd_cfg->src1_y_end   = src_y + src_h - 1;

	ge2d_cmd_cfg->src2_x_start = src2_x;
	ge2d_cmd_cfg->src2_x_end   = src2_x + src2_w - 1;
	ge2d_cmd_cfg->src2_y_start = src2_y;
	ge2d_cmd_cfg->src2_y_end   = src2_y + src2_h - 1;

	ge2d_cmd_cfg->dst_x_start  = dst_x;
	ge2d_cmd_cfg->dst_x_end    = dst_x + dst_w - 1;
	ge2d_cmd_cfg->dst_y_start  = dst_y;
	ge2d_cmd_cfg->dst_y_end    = dst_y + dst_h - 1;

	/* if ((dst_w != src_w) || (dst_h != src_h)) { */
	ge2d_cmd_cfg->sc_hsc_en = 1;
	ge2d_cmd_cfg->sc_vsc_en = 1;
	ge2d_cmd_cfg->hsc_rpt_p0_num = 1;
	ge2d_cmd_cfg->vsc_rpt_l0_num = 1;
	ge2d_cmd_cfg->hsc_div_en = 1;
#ifdef CONFIG_GE2D_ADV_NUM
	ge2d_cmd_cfg->hsc_adv_num =
		((dst_w - 1) < 1024) ? (dst_w - 1) : 0;
#else
	ge2d_cmd_cfg->hsc_adv_num = 0;
#endif

	ge2d_cmd_cfg->color_blend_mode = (op >> 24) & 0xff;
	ge2d_cmd_cfg->color_src_blend_factor = (op >> 20) & 0xf;
	ge2d_cmd_cfg->color_dst_blend_factor = (op >> 16) & 0xf;
	ge2d_cmd_cfg->alpha_src_blend_factor = (op >>  4) & 0xf;
	ge2d_cmd_cfg->alpha_dst_blend_factor = (op >>  0) & 0xf;

	if (ge2d_meson_dev.chip_type != MESON_CPU_MAJOR_ID_AXG) {
		if (ge2d_cmd_cfg->color_blend_mode >= BLENDOP_LOGIC) {
			ge2d_cmd_cfg->color_logic_op =
				ge2d_cmd_cfg->color_blend_mode - BLENDOP_LOGIC;
			ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
		}
	}
	ge2d_cmd_cfg->alpha_blend_mode = (op >> 8) & 0xff;
	if (ge2d_cmd_cfg->alpha_blend_mode >= BLENDOP_LOGIC) {
		ge2d_cmd_cfg->alpha_logic_op =
			ge2d_cmd_cfg->alpha_blend_mode - BLENDOP_LOGIC;
		ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	}

	ge2d_cmd_cfg->wait_done_flag   = 1;
	ge2d_cmd_cfg->cmd_op           = IS_BLEND;

	ge2d_wq_add_work(wq, 0);
}
EXPORT_SYMBOL(blend);

void blend_noblk(struct ge2d_context_s *wq,
		 int src_x, int src_y, int src_w, int src_h,
		 int src2_x, int src2_y, int src2_w, int src2_h,
		 int dst_x, int dst_y, int dst_w, int dst_h,
		 int op, int enqueue)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	if (ge2d_log_level & GE2D_LOG_DUMP_STACK)
		dump_stack();

	if (src_x < 0 || src_y < 0 || src_w < 0 || src_h < 0 ||
	   src2_x < 0 || src2_y < 0 || src2_w < 0 || src2_h < 0 ||
	   dst_x < 0 || dst_y < 0 || dst_w < 0 || dst_h < 0) {
		ge2d_log_err("%s wrong params, %d %d %d %d + %d %d %d %d -> %d %d %d %d\n",
			     __func__,
			     src_x, src_y, src_w, src_h,
			     src2_x, src2_y, src2_w, src2_h,
			     dst_x, dst_y, dst_w, dst_h);
		return;
	}

	ge2d_cmd_cfg->src1_x_start = src_x;
	ge2d_cmd_cfg->src1_x_end   = src_x + src_w - 1;
	ge2d_cmd_cfg->src1_y_start = src_y;
	ge2d_cmd_cfg->src1_y_end   = src_y + src_h - 1;

	ge2d_cmd_cfg->src2_x_start = src2_x;
	ge2d_cmd_cfg->src2_x_end   = src2_x + src2_w - 1;
	ge2d_cmd_cfg->src2_y_start = src2_y;
	ge2d_cmd_cfg->src2_y_end   = src2_y + src2_h - 1;

	ge2d_cmd_cfg->dst_x_start  = dst_x;
	ge2d_cmd_cfg->dst_x_end    = dst_x + dst_w - 1;
	ge2d_cmd_cfg->dst_y_start  = dst_y;
	ge2d_cmd_cfg->dst_y_end    = dst_y + dst_h - 1;

	/* if ((dst_w != src_w) || (dst_h != src_h)) { */
	if (1) {
		ge2d_cmd_cfg->sc_hsc_en = 1;
		ge2d_cmd_cfg->sc_vsc_en = 1;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 1;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 1;
		ge2d_cmd_cfg->hsc_div_en = 1;
#ifdef CONFIG_GE2D_ADV_NUM
		ge2d_cmd_cfg->hsc_adv_num =
			((dst_w - 1) < 1024) ? (dst_w - 1) : 0;
#else
		ge2d_cmd_cfg->hsc_adv_num = 0;
#endif
	} else {
		ge2d_cmd_cfg->sc_hsc_en = 0;
		ge2d_cmd_cfg->sc_vsc_en = 0;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 0;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 0;
		ge2d_cmd_cfg->hsc_div_en = 0;
		ge2d_cmd_cfg->hsc_adv_num = 0;
	}

	ge2d_cmd_cfg->color_blend_mode = (op >> 24) & 0xff;
	ge2d_cmd_cfg->color_src_blend_factor = (op >> 20) & 0xf;
	ge2d_cmd_cfg->color_dst_blend_factor = (op >> 16) & 0xf;
	ge2d_cmd_cfg->alpha_src_blend_factor = (op >>  4) & 0xf;
	ge2d_cmd_cfg->alpha_dst_blend_factor = (op >>  0) & 0xf;
	if (ge2d_meson_dev.chip_type != MESON_CPU_MAJOR_ID_AXG) {
		if (ge2d_cmd_cfg->color_blend_mode >= BLENDOP_LOGIC) {
			ge2d_cmd_cfg->color_logic_op =
				ge2d_cmd_cfg->color_blend_mode - BLENDOP_LOGIC;
			ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
		}
	}
	ge2d_cmd_cfg->alpha_blend_mode = (op >> 8) & 0xff;
	if (ge2d_cmd_cfg->alpha_blend_mode >= BLENDOP_LOGIC) {
		ge2d_cmd_cfg->alpha_logic_op =
			ge2d_cmd_cfg->alpha_blend_mode - BLENDOP_LOGIC;
		ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	}

	ge2d_cmd_cfg->wait_done_flag = 0;
	ge2d_cmd_cfg->cmd_op         = IS_BLEND;

	ge2d_wq_add_work(wq, enqueue);
}
EXPORT_SYMBOL(blend_noblk);

void blend_noalpha(struct ge2d_context_s *wq,
		   int src_x, int src_y, int src_w, int src_h,
		   int src2_x, int src2_y, int src2_w, int src2_h,
		   int dst_x, int dst_y, int dst_w, int dst_h,
		   int op)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	if (ge2d_log_level & GE2D_LOG_DUMP_STACK)
		dump_stack();

	if (src_x < 0 || src_y < 0 || src_w < 0 || src_h < 0 ||
	   src2_x < 0 || src2_y < 0 || src2_w < 0 || src2_h < 0 ||
	   dst_x < 0 || dst_y < 0 || dst_w < 0 || dst_h < 0) {
		ge2d_log_err("%s wrong params, %d %d %d %d + %d %d %d %d -> %d %d %d %d\n",
			     __func__,
			     src_x, src_y, src_w, src_h,
			     src2_x, src2_y, src2_w, src2_h,
			     dst_x, dst_y, dst_w, dst_h);
		return;
	}

	ge2d_cmd_cfg->src1_x_start = src_x;
	ge2d_cmd_cfg->src1_x_end   = src_x + src_w - 1;
	ge2d_cmd_cfg->src1_y_start = src_y;
	ge2d_cmd_cfg->src1_y_end   = src_y + src_h - 1;

	ge2d_cmd_cfg->src2_x_start = src2_x;
	ge2d_cmd_cfg->src2_x_end   = src2_x + src2_w - 1;
	ge2d_cmd_cfg->src2_y_start = src2_y;
	ge2d_cmd_cfg->src2_y_end   = src2_y + src2_h - 1;

	ge2d_cmd_cfg->dst_x_start  = dst_x;
	ge2d_cmd_cfg->dst_x_end    = dst_x + dst_w - 1;
	ge2d_cmd_cfg->dst_y_start  = dst_y;
	ge2d_cmd_cfg->dst_y_end    = dst_y + dst_h - 1;

	/* if ((dst_w != src_w) || (dst_h != src_h)) { */
	if (1) {
		ge2d_cmd_cfg->sc_hsc_en = 1;
		ge2d_cmd_cfg->sc_vsc_en = 1;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 1;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 1;
		ge2d_cmd_cfg->hsc_div_en = 1;
#ifdef CONFIG_GE2D_ADV_NUM
		ge2d_cmd_cfg->hsc_adv_num =
			((dst_w - 1) < 1024) ? (dst_w - 1) : 0;
#else
		ge2d_cmd_cfg->hsc_adv_num = 0;
#endif
	} else {
		ge2d_cmd_cfg->sc_hsc_en = 0;
		ge2d_cmd_cfg->sc_vsc_en = 0;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 0;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 0;
		ge2d_cmd_cfg->hsc_div_en = 0;
		ge2d_cmd_cfg->hsc_adv_num = 0;
	}

	ge2d_cmd_cfg->color_blend_mode = (op >> 24) & 0xff;
	ge2d_cmd_cfg->color_src_blend_factor = (op >> 20) & 0xf;
	ge2d_cmd_cfg->color_dst_blend_factor = (op >> 16) & 0xf;
	ge2d_cmd_cfg->alpha_src_blend_factor = (op >>  4) & 0xf;
	ge2d_cmd_cfg->alpha_dst_blend_factor = (op >>  0) & 0xf;

	if (ge2d_meson_dev.chip_type != MESON_CPU_MAJOR_ID_AXG) {
		if (ge2d_cmd_cfg->color_blend_mode >= BLENDOP_LOGIC) {
			ge2d_cmd_cfg->color_logic_op =
				ge2d_cmd_cfg->color_blend_mode - BLENDOP_LOGIC;
			ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
		}
	}
	ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->alpha_logic_op   = LOGIC_OPERATION_SET;

	ge2d_cmd_cfg->wait_done_flag   = 1;
	ge2d_cmd_cfg->cmd_op           = IS_BLEND;

	ge2d_wq_add_work(wq, 0);
}
EXPORT_SYMBOL(blend_noalpha);

void blend_noalpha_noblk(struct ge2d_context_s *wq,
			 int src_x, int src_y, int src_w, int src_h,
			 int src2_x, int src2_y, int src2_w, int src2_h,
			 int dst_x, int dst_y, int dst_w, int dst_h,
			 int op, int enqueue)
{
	struct ge2d_cmd_s *ge2d_cmd_cfg = ge2d_wq_get_cmd(wq);

	if (ge2d_log_level & GE2D_LOG_DUMP_STACK)
		dump_stack();

	if (src_x < 0 || src_y < 0 || src_w < 0 || src_h < 0 ||
	   src2_x < 0 || src2_y < 0 || src2_w < 0 || src2_h < 0 ||
	   dst_x < 0 || dst_y < 0 || dst_w < 0 || dst_h < 0) {
		ge2d_log_err("%s wrong params, %d %d %d %d + %d %d %d %d -> %d %d %d %d\n",
			     __func__,
			     src_x, src_y, src_w, src_h,
			     src2_x, src2_y, src2_w, src2_h,
			     dst_x, dst_y, dst_w, dst_h);
		return;
	}

	ge2d_cmd_cfg->src1_x_start = src_x;
	ge2d_cmd_cfg->src1_x_end   = src_x + src_w - 1;
	ge2d_cmd_cfg->src1_y_start = src_y;
	ge2d_cmd_cfg->src1_y_end   = src_y + src_h - 1;

	ge2d_cmd_cfg->src2_x_start = src2_x;
	ge2d_cmd_cfg->src2_x_end   = src2_x + src2_w - 1;
	ge2d_cmd_cfg->src2_y_start = src2_y;
	ge2d_cmd_cfg->src2_y_end   = src2_y + src2_h - 1;

	ge2d_cmd_cfg->dst_x_start  = dst_x;
	ge2d_cmd_cfg->dst_x_end    = dst_x + dst_w - 1;
	ge2d_cmd_cfg->dst_y_start  = dst_y;
	ge2d_cmd_cfg->dst_y_end    = dst_y + dst_h - 1;

	/* if ((dst_w != src_w) || (dst_h != src_h)) { */
	if (1) {
		ge2d_cmd_cfg->sc_hsc_en = 1;
		ge2d_cmd_cfg->sc_vsc_en = 1;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 1;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 1;
		ge2d_cmd_cfg->hsc_div_en = 1;
#ifdef CONFIG_GE2D_ADV_NUM
		ge2d_cmd_cfg->hsc_adv_num =
			((dst_w - 1) < 1024) ? (dst_w - 1) : 0;
#else
		ge2d_cmd_cfg->hsc_adv_num = 0;
#endif
	} else {
		ge2d_cmd_cfg->sc_hsc_en = 0;
		ge2d_cmd_cfg->sc_vsc_en = 0;
		ge2d_cmd_cfg->hsc_rpt_p0_num = 0;
		ge2d_cmd_cfg->vsc_rpt_l0_num = 0;
		ge2d_cmd_cfg->hsc_div_en = 0;
		ge2d_cmd_cfg->hsc_adv_num = 0;
	}

	ge2d_cmd_cfg->color_blend_mode = (op >> 24) & 0xff;
	ge2d_cmd_cfg->color_src_blend_factor = (op >> 20) & 0xf;
	ge2d_cmd_cfg->color_dst_blend_factor = (op >> 16) & 0xf;
	ge2d_cmd_cfg->alpha_src_blend_factor = (op >>  4) & 0xf;
	ge2d_cmd_cfg->alpha_dst_blend_factor = (op >>  0) & 0xf;

	if (ge2d_meson_dev.chip_type != MESON_CPU_MAJOR_ID_AXG) {
		if (ge2d_cmd_cfg->color_blend_mode >= BLENDOP_LOGIC) {
			ge2d_cmd_cfg->color_logic_op =
				ge2d_cmd_cfg->color_blend_mode - BLENDOP_LOGIC;
			ge2d_cmd_cfg->color_blend_mode = OPERATION_LOGIC;
		}
	}
	ge2d_cmd_cfg->alpha_blend_mode = OPERATION_LOGIC;
	ge2d_cmd_cfg->alpha_logic_op   = LOGIC_OPERATION_SET;

	ge2d_cmd_cfg->wait_done_flag   = 0;
	ge2d_cmd_cfg->cmd_op           = IS_BLEND;

	ge2d_wq_add_work(wq, enqueue);
}
EXPORT_SYMBOL(blend_noalpha_noblk);

void blend_enqueue(struct ge2d_context_s *wq,
		   int src_x, int src_y, int src_w, int src_h,
		   int src2_x, int src2_y, int src2_w, int src2_h,
		   int dst_x, int dst_y, int dst_w, int dst_h,
		   int op)
{
	blend_noblk(wq, src_x, src_y, src_w, src_h,
	      src2_x,  src2_y, src2_w, src2_h,
	      dst_x,  dst_y, dst_w, dst_h,
	      op, 1);
}
EXPORT_SYMBOL(blend_enqueue);

void blend_noalpha_enqueue(struct ge2d_context_s *wq,
			   int src_x, int src_y, int src_w, int src_h,
			   int src2_x, int src2_y, int src2_w, int src2_h,
			   int dst_x, int dst_y, int dst_w, int dst_h,
			   int op)
{
	blend_noalpha_noblk(wq, src_x, src_y, src_w, src_h,
	      src2_x,  src2_y, src2_w, src2_h,
	      dst_x,  dst_y, dst_w, dst_h,
	      op, 1);
}
EXPORT_SYMBOL(blend_noalpha_enqueue);
