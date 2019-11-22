/*
 * drivers/amlogic/drm/vpu-hw/meson_vpu_postblend.c
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

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"
#include "meson_vpu_util.h"
#include "meson_vpu_postblend.h"

static struct postblend_reg_s postblend_reg = {
	VPP_OSD1_BLD_H_SCOPE,
	VPP_OSD1_BLD_V_SCOPE,
	VPP_OSD2_BLD_H_SCOPE,
	VPP_OSD2_BLD_V_SCOPE,
	VD1_BLEND_SRC_CTRL,
	VD2_BLEND_SRC_CTRL,
	OSD1_BLEND_SRC_CTRL,
	OSD2_BLEND_SRC_CTRL,
	VPP_OSD1_IN_SIZE,
};

/*vpp post&post blend for osd1 premult flag config as 0 default*/
static void osd1_blend_premult_set(struct postblend_reg_s *reg)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->osd1_blend_src_ctrl, 0, 4, 1);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->osd1_blend_src_ctrl, 0, 16, 1);
}

/*vpp pre&post blend for osd2 premult flag config as 0 default*/
static void osd2_blend_premult_set(struct postblend_reg_s *reg)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->osd2_blend_src_ctrl, 0, 4, 1);
	VSYNCOSD_WR_MPEG_REG_BITS(reg->osd2_blend_src_ctrl, 0, 16, 1);
}

/*vpp osd1 blend sel*/
static void osd1_blend_switch_set(struct postblend_reg_s *reg,
	enum vpp_blend_e blend_sel)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->osd1_blend_src_ctrl, blend_sel, 20, 1);
}

/*vpp osd2 blend sel*/
static void osd2_blend_switch_set(struct postblend_reg_s *reg,
	enum vpp_blend_e blend_sel)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->osd2_blend_src_ctrl, blend_sel, 20, 1);
}

/*vpp osd1 preblend mux sel*/
static void vpp_osd1_preblend_mux_set(struct postblend_reg_s *reg,
	enum vpp_blend_src_e src_sel)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->osd1_blend_src_ctrl, src_sel, 0, 4);
}

/*vpp osd2 preblend mux sel*/
static void vpp_osd2_preblend_mux_set(struct postblend_reg_s *reg,
	enum vpp_blend_src_e src_sel)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->osd2_blend_src_ctrl, src_sel, 0, 4);
}

/*vpp osd1 postblend mux sel*/
static void vpp_osd1_postblend_mux_set(struct postblend_reg_s *reg,
	enum vpp_blend_src_e src_sel)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->osd1_blend_src_ctrl, src_sel, 8, 4);
}
/*vpp osd2 postblend mux sel*/
static void vpp_osd2_postblend_mux_set(struct postblend_reg_s *reg,
	enum vpp_blend_src_e src_sel)
{
	VSYNCOSD_WR_MPEG_REG_BITS(reg->osd2_blend_src_ctrl, src_sel, 8, 4);
}
/*vpp osd1 blend scope set*/
static void vpp_osd1_blend_scope_set(struct postblend_reg_s *reg,
	struct osd_scope_s scope)
{
	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd1_bld_h_scope,
		(scope.h_start << 16) | scope.h_end);
	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd1_bld_v_scope,
		(scope.v_start << 16) | scope.v_end);
}
/*vpp osd2 blend scope set*/
static void vpp_osd2_blend_scope_set(struct postblend_reg_s *reg,
	struct osd_scope_s scope)
{
	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd2_bld_h_scope,
		(scope.h_start << 16) | scope.h_end);
	VSYNCOSD_WR_MPEG_REG(reg->vpp_osd2_bld_v_scope,
		(scope.v_start << 16) | scope.v_end);
}

static int postblend_check_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	if (state->checked)
		return 0;

	state->checked = true;

	DRM_DEBUG("%s check_state called.\n", postblend->base.name);
	return 0;
}

static void postblend_set_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
	struct drm_crtc *crtc;
	struct am_meson_crtc *amc;
	struct meson_vpu_pipeline_state *mvps;

	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct osd_scope_s scope = {0, 1919, 0, 1079};
	struct meson_vpu_pipeline *pipeline = postblend->base.pipeline;
	struct postblend_reg_s *reg = postblend->reg;

	crtc = vblk->pipeline->crtc;
	amc = to_am_meson_crtc(crtc);

	DRM_DEBUG("%s set_state called.\n", postblend->base.name);
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	scope.h_start = 0;
	scope.h_end = mvps->scaler_param[0].output_width - 1;
	scope.v_start = 0;
	scope.v_end = mvps->scaler_param[0].output_height - 1;
	vpp_osd1_blend_scope_set(reg, scope);
	if (0)
		vpp_osd2_blend_scope_set(reg, scope);

	if (amc->blank_enable)
		vpp_osd1_postblend_mux_set(postblend->reg, VPP_NULL);
	else
		vpp_osd1_postblend_mux_set(postblend->reg, VPP_OSD1);

	osd1_blend_premult_set(reg);
	osd2_blend_premult_set(reg);
	DRM_DEBUG("scope h/v start/end [%d,%d,%d,%d].\n",
		scope.h_start, scope.h_end, scope.v_start, scope.v_end);
}

static void postblend_hw_enable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	DRM_DEBUG("%s enable called.\n", postblend->base.name);
}

static void postblend_hw_disable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	DRM_DEBUG("%s disable called.\n", postblend->base.name);
}

static void postblend_dump_register(struct meson_vpu_block *vblk,
				struct seq_file *seq)
{
	u32 value;
	struct meson_vpu_postblend *postblend;
	struct postblend_reg_s *reg;

	postblend = to_postblend_block(vblk);
	reg = postblend->reg;

	value = meson_drm_read_reg(reg->osd1_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD1_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->osd2_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD2_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vd1_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VD1_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vd2_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VD2_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_in_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_IN_SIZE:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_bld_h_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_BLD_H_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_bld_v_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_BLD_V_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd2_bld_h_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD2_BLD_H_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd2_bld_v_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD2_BLD_V_SCOPE:", value);
}

static void postblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	postblend->reg = &postblend_reg;
	/*dout switch config*/
	osd1_blend_switch_set(postblend->reg, VPP_POSTBLEND);
	osd2_blend_switch_set(postblend->reg, VPP_POSTBLEND);
	/*vpp input config*/
	vpp_osd1_preblend_mux_set(postblend->reg, VPP_NULL);
	vpp_osd2_preblend_mux_set(postblend->reg, VPP_NULL);
	vpp_osd1_postblend_mux_set(postblend->reg, VPP_OSD1);
	vpp_osd2_postblend_mux_set(postblend->reg, VPP_NULL);
	DRM_DEBUG("%s hw_init called.\n", postblend->base.name);
}

struct meson_vpu_block_ops postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = postblend_hw_init,
};
